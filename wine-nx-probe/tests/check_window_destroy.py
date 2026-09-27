#!/usr/bin/env python3
"""Exercise the real server handler for individual and bulk window destruction."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "dlls/ntdll/unix/horizon.c").read_text()
start = source.index("static int horizon_server_handle_destroy_window(")
brace = source.index("{", start)
end, depth = brace + 1, 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1

fixture = r'''
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
enum { HORIZON_STATUS_SUCCESS = 0, HORIZON_STATUS_INVALID_HANDLE = 6 };
struct horizon_destroy_window_request { unsigned int handle; };
struct horizon_server_connection { int reply_fd; };
struct horizon_window_property { struct horizon_window_property *next; };
struct horizon_user_window {
    unsigned int handle, tid;
    struct horizon_window_property *properties;
    struct horizon_user_window *next;
};
struct horizon_input_shm { unsigned int caret; };
static struct horizon_user_window *horizon_windows;
static struct horizon_input_shm input;
static pthread_mutex_t horizon_server_objects_mutex = PTHREAD_MUTEX_INITIALIZER;
static int horizon_posted_messages, horizon_timers, horizon_clipboard;
static unsigned int drops[4], timer_drops[4], caret_clears, refreshes;
static void horizon_message_queue_drop(int *q, unsigned int tid, unsigned int hwnd)
{ (void)q; assert(tid == 4 && hwnd < 4); drops[hwnd]++; }
static void horizon_win_timers_drop(int *q, unsigned int tid, unsigned int hwnd)
{ (void)q; assert(tid == 4 && hwnd < 4); timer_drops[hwnd]++; }
static int horizon_clip_window_destroyed(int *q, unsigned int hwnd)
{ (void)q; (void)hwnd; return 0; }
static void horizon_server_clipboard_notify_locked(void) {}
static struct horizon_input_shm *horizon_server_input_shared_locked(void) { return &input; }
static void horizon_server_set_caret_window_locked(struct horizon_input_shm *p,
                                                 unsigned int hwnd, int x, int y)
{ assert(!hwnd && !x && !y); p->caret = hwnd; caret_clears++; }
static void horizon_server_flush_input_locked(void) {}
static void horizon_server_refresh_queues_locked(void) { refreshes++; }
static int horizon_server_write_status(int fd, unsigned int status) { (void)fd; return status; }
'''
tests = r'''
static void create(unsigned int hwnd)
{
    struct horizon_user_window *w = calloc(1, sizeof(*w));
    assert(w);
    w->handle = hwnd;
    w->tid = 4;
    w->properties = calloc(1, sizeof(*w->properties));
    assert(w->properties);
    w->next = horizon_windows;
    horizon_windows = w;
}
static int destroy(unsigned int hwnd)
{
    struct horizon_destroy_window_request req = {hwnd};
    struct horizon_server_connection connection = {0};
    return horizon_server_handle_destroy_window(&connection, (const unsigned char *)&req);
}
int main(void)
{
    assert(!destroy(0)); /* already empty */
    create(1);
    input.caret = 1;
    assert(!destroy(0) && !horizon_windows && !input.caret && caret_clears == 1);
    create(1); create(2); create(3);
    input.caret = 2;
    assert(destroy(99) == HORIZON_STATUS_INVALID_HANDLE);
    assert(horizon_windows->handle == 3 && horizon_windows->next->handle == 2);
    assert(!destroy(2)); /* unlink a middle window without dropping its neighbours */
    assert(horizon_windows->handle == 3 && horizon_windows->next->handle == 1);
    assert(!input.caret && caret_clears == 2);
    assert(drops[3] == 0 && drops[1] == 1 && drops[2] == 1);
    assert(!destroy(0) && !horizon_windows); /* bulk destroy must not skip or dereference NULL */
    assert(drops[1] == 2 && drops[2] == 1 && drops[3] == 1);
    create(1); create(2); create(3);
    assert(!destroy(3) && horizon_windows->handle == 2); /* head */
    assert(!destroy(1) && !horizon_windows->next); /* tail */
    assert(!destroy(2) && !horizon_windows);
    for (unsigned int i = 1; i < 4; i++) assert(timer_drops[i] == drops[i]);
    assert(refreshes == 8);
    puts("Horizon window destruction: empty, single, bulk, middle, head, tail and missing handles passed");
}
'''
with tempfile.TemporaryDirectory(prefix="wine-nx-window-destroy-") as temporary:
    path = Path(temporary)
    (path / "test.c").write_text(fixture + source[start:end] + tests)
    # Xcode's ASan runtime can stall during dyld initialization on macOS 26.
    sanitizer = "undefined" if sys.platform == "darwin" else "address,undefined"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pthread",
                    "-fsanitize=" + sanitizer, str(path / "test.c"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True, timeout=30)
