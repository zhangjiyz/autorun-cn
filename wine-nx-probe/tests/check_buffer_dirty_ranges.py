#!/usr/bin/env python3
"""Check the real dirty-range helper preserves the exact bytes to upload."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "dlls/wined3d/buffer.c").read_text()
start = source.index("static void buffer_invalidate_bo_range(")
brace = source.index("{", start)
end, depth = brace + 1, 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1

fixture = r'''
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WARN(...) ((void)0)
#define ERR(...) ((void)0)
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
struct wined3d_range { unsigned int offset, size; };
struct wined3d_buffer {
    struct { unsigned int size; } resource;
    struct wined3d_range *dirty_ranges;
    size_t dirty_ranges_capacity;
    unsigned int dirty_range_count;
};
static int fail_reserve;
static int wined3d_array_reserve(void **data, size_t *capacity, size_t count, size_t size)
{
    void *allocation;
    if (fail_reserve) return 0;
    if (count <= *capacity) return 1;
    allocation = realloc(*data, count * size);
    assert(allocation);
    *data = allocation;
    *capacity = count;
    return 1;
}
'''
tests = r'''
static struct wined3d_buffer buffer(unsigned int size)
{
    struct wined3d_buffer b = {{size}, calloc(1, sizeof(struct wined3d_range)), 1, 0};
    assert(b.dirty_ranges);
    return b;
}
static void check_coverage(const struct wined3d_buffer *b, const unsigned char *expected)
{
    unsigned char actual[256] = {0};
    assert(b->resource.size <= sizeof(actual));
    for (unsigned int i = 0; i < b->dirty_range_count; ++i)
    {
        struct wined3d_range range = b->dirty_ranges[i];
        assert(range.offset <= b->resource.size && range.size <= b->resource.size - range.offset);
        memset(actual + range.offset, 1, range.size);
    }
    assert(!memcmp(actual, expected, b->resource.size));
}
static uint32_t random_state = 42;
static unsigned int next_random(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
int main(void)
{
    struct wined3d_buffer b = buffer(100000);
    for (unsigned int i = 0; i < 10000; ++i) buffer_invalidate_bo_range(&b, i * 4, 4);
    assert(b.dirty_range_count == 1 && b.dirty_ranges[0].offset == 0 && b.dirty_ranges[0].size == 40000);
    free(b.dirty_ranges);
    b = buffer(100000);
    for (unsigned int i = 10000; i; --i) buffer_invalidate_bo_range(&b, (i - 1) * 4, 4);
    assert(b.dirty_range_count == 1 && b.dirty_ranges[0].size == 40000);
    b.dirty_range_count = 0;
    buffer_invalidate_bo_range(&b, 20, 10);
    buffer_invalidate_bo_range(&b, 25, 10);
    buffer_invalidate_bo_range(&b, 26, 2);
    assert(b.dirty_range_count == 1 && b.dirty_ranges[0].offset == 20 && b.dirty_ranges[0].size == 15);
    buffer_invalidate_bo_range(&b, 50, 10);
    assert(b.dirty_range_count == 2); /* Never upload an untouched gap. */
    buffer_invalidate_bo_range(&b, 0, 0);
    for (unsigned int i = 0; i < 10000; ++i) buffer_invalidate_bo_range(&b, i, 1);
    assert(b.dirty_range_count == 1 && b.dirty_ranges[0].size == 100000);
    b.dirty_range_count = 0;
    buffer_invalidate_bo_range(&b, 1, 1);
    fail_reserve = 1;
    buffer_invalidate_bo_range(&b, 10, 1);
    assert(b.dirty_range_count == 1 && b.dirty_ranges[0].size == 100000);
    fail_reserve = 0;
    free(b.dirty_ranges);
    b = buffer(UINT_MAX);
    buffer_invalidate_bo_range(&b, UINT_MAX - 20, 10);
    buffer_invalidate_bo_range(&b, UINT_MAX - 10, 10);
    assert(b.dirty_range_count == 1 && b.dirty_ranges[0].size == 20);
    free(b.dirty_ranges);
    for (unsigned int trial = 0; trial < 500; ++trial)
    {
        unsigned char expected[256] = {0};
        b = buffer(sizeof(expected));
        for (unsigned int step = 0; step < 100; ++step)
        {
            unsigned int offset = next_random() % 256, size = next_random() % (257 - offset);
            if (!(step % 31)) size = 0;
            if (step == 99 && (trial & 1)) offset = UINT_MAX;
            if ((!offset && !size) || offset > b.resource.size || size > b.resource.size - offset)
                memset(expected, 1, sizeof(expected));
            else
                memset(expected + offset, 1, size);
            buffer_invalidate_bo_range(&b, offset, size);
            check_coverage(&b, expected);
        }
        free(b.dirty_ranges);
    }
    puts("Dirty ranges: exact coverage, holes, overlap, full buffer, OOM, overflow and 50000 random updates passed");
    puts("10000 consecutive updates now require 1 upload range");
}
'''
with tempfile.TemporaryDirectory(prefix="wine-nx-buffer-ranges-") as temporary:
    path = Path(temporary)
    (path / "test.c").write_text(fixture + source[start:end] + tests)
    sanitizer = "undefined" if sys.platform == "darwin" else "address,undefined"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=" + sanitizer,
                    str(path / "test.c"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True, timeout=30)
