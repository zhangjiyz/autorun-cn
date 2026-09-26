/* Copyright 2026 Wine-NX contributors. LGPL-2.1-or-later. */
#ifndef WINE_NX_SD_CACHE_H
#define WINE_NX_SD_CACHE_H

int wine_nx_sd_cache_install(void);
void wine_nx_sd_cache_flush(void);
unsigned int wine_nx_sd_cache_mb(void);
int wine_nx_sd_cache_is_cn(void);

/* CN per-game options are read only while the CN backend is selected. */
extern int wine_nx_sd_stat_cache, wine_nx_sd_clean_writer_cache;
extern unsigned int wine_nx_sd_stat_queries, wine_nx_sd_fstat_hits;
extern unsigned int wine_nx_sd_reads, wine_nx_sd_hits;
extern unsigned long long wine_nx_sd_read_ns, wine_nx_sd_bytes;
extern unsigned int wine_nx_sd_writes, wine_nx_sd_writes_held;
extern unsigned long long wine_nx_sd_write_ns;
extern unsigned int wine_nx_sd_stats, wine_nx_sd_stat_hits;
extern unsigned int wine_nx_sd_flush_jump, wine_nx_sd_flush_path, wine_nx_sd_flush_end,
                    wine_nx_sd_flush_close, wine_nx_sd_flush_timer;

#endif
