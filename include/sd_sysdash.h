#ifndef SD_SYSDASH_H
#define SD_SYSDASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SD_MAX_MODS 32
#define SD_MAX_LOGS 32
#define SD_MAX_TITLES 32
#define SD_PATH_MAX 512

typedef struct {
    uint64_t title_id;
    char product_code[16];
    uint32_t size_kb;
    uint16_t version;
    bool is_legacy_blackbox;
} sd_installed_title_t;

typedef struct {
    char title_id[20];
    char title_name[64];
    bool enabled; /* true = in luma/titles/<tid>, false = in luma/titles_disabled/<tid> */
    bool has_romfs;
    bool has_code_bin;
    bool has_exefs;
} sd_layeredfs_mod_t;

typedef struct {
    char rel_path[256];
    char summary[160];
    size_t file_size;
    bool is_crash_dump;
} sd_log_entry_t;

typedef struct {
    uint8_t battery_percent;
    bool battery_charging;
    uint8_t wifi_strength;
    char ip_addr[32];
    char console_model[32];
    uint64_t sd_free_bytes;
    uint64_t sd_total_bytes;
    int checkpoint_save_dirs;
    int nds_save_files;
    int gba_save_files;
    int homebrew_3dsx_count;
    sd_layeredfs_mod_t mods[SD_MAX_MODS];
    int mod_count;
    sd_log_entry_t logs[SD_MAX_LOGS];
    int log_count;
    sd_installed_title_t titles[SD_MAX_TITLES];
    int title_count;
} sd_telemetry_t;

/* Map a 16-hex-char 3DS TitleID to a human-readable Pokémon/3DS title name */
const char *sd_lookup_title_name(const char *title_id);

/* Scan SD card root for Luma3DS LayeredFS mods, Checkpoint/NDS/GBA saves, and crash/homebrew logs */
bool sd_scan_sd_card(const char *sd_root, sd_telemetry_t *out);

/* Atomically toggle a Luma3DS LayeredFS mod between luma/titles/<tid> and luma/titles_disabled/<tid> */
bool sd_toggle_layeredfs_mod(const char *sd_root, sd_layeredfs_mod_t *mod);

/* Parse a Luma3DS ARM9/ARM11 exception .dmp or errdisp.txt file into a human-readable one-line summary */
bool sd_parse_log_or_dump(const char *full_path, char *out_summary, size_t summary_cap);

/* Export full diagnostic snapshot to <sd_root>/3ds/sysdash/diagnostic_snapshot.json and .log */
bool sd_export_diagnostic_snapshot(const char *sd_root, const sd_telemetry_t *t, char *out_json_path, size_t cap);

/* Purge stale/black-square test titles (0x00040000000D3D00..0x00040000000D3DF0) and pending imports via AM */
int sd_purge_stale_homebrew_titles(void);

/* Delete a specific installed SD user title + ticket by 64-bit TitleID */
bool sd_delete_sd_title(uint64_t title_id);

#endif /* SD_SYSDASH_H */
