/* Copyright 2026 Wine-NX contributors. LGPL-2.1-or-later. */
/* Choose once at start-up: open handles and pending writes must never move
 * between backends. CN is the default; upstream remains selectable. */
#include "config_json.h"
#include "sd_cache.h"

int wine_nx_sd_stat_cache, wine_nx_sd_clean_writer_cache;
unsigned int wine_nx_sd_stat_queries, wine_nx_sd_fstat_hits;
unsigned int wine_nx_sd_reads, wine_nx_sd_hits;
unsigned long long wine_nx_sd_read_ns, wine_nx_sd_bytes;
unsigned int wine_nx_sd_writes, wine_nx_sd_writes_held;
unsigned long long wine_nx_sd_write_ns;
unsigned int wine_nx_sd_stats, wine_nx_sd_stat_hits;
unsigned int wine_nx_sd_flush_jump, wine_nx_sd_flush_path, wine_nx_sd_flush_end,
             wine_nx_sd_flush_close, wine_nx_sd_flush_timer;

extern int wine_nx_sd_cache_cn_install(void);
extern unsigned int wine_nx_sd_cache_cn_mb(void);
extern int wine_nx_sd_cache_upstream_install(void);
extern void wine_nx_sd_cache_upstream_flush(void);
extern unsigned int wine_nx_sd_cache_upstream_mb(void);

static int cn_strategy, installed;

int wine_nx_sd_cache_install(void)
{
    struct wine_nx_config config;
    int ret;

    if (installed) return 0;
    /* Read and close this one file with libnx's original device before any
     * wrapper is installed, so the policy applies to all later handles. */
    wine_nx_config_load( &config, "sdmc:/switch/wine/config/settings.json" );
    cn_strategy = wine_nx_config_bool( &config, "sd-cn-strategy", 1 );
    ret = cn_strategy ? wine_nx_sd_cache_cn_install() : wine_nx_sd_cache_upstream_install();
    installed = ret;
    return ret;
}

int wine_nx_sd_cache_is_cn(void)
{
    return cn_strategy;
}

void wine_nx_sd_cache_flush(void)
{
    if (installed && !cn_strategy) wine_nx_sd_cache_upstream_flush();
}

unsigned int wine_nx_sd_cache_mb(void)
{
    return cn_strategy ? wine_nx_sd_cache_cn_mb() : wine_nx_sd_cache_upstream_mb();
}
