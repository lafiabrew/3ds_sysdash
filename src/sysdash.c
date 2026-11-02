#include "sd_sysdash.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#ifdef __3DS__
#include <3ds.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool str_ieq(const char *a, const char *b) {
    if (a == NULL || b == NULL) return false;
    while (*a && *b) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

const char *sd_lookup_title_name(const char *title_id) {
    if (title_id == NULL) return "UNKNOWN 3DS TITLE";
    static const struct {
        const char *tid;
        const char *name;
    } k_titles[] = {
        { "0004000000055D00", "POKEMON X (ETERNAL X / WILTING Y)" },
        { "0004000000055E00", "POKEMON Y (WILTING Y)" },
        { "000400000011C400", "POKEMON OMEGA RUBY (RUTILE RUBY)" },
        { "000400000011C500", "POKEMON ALPHA SAPPHIRE (STAR SAPPHIRE)" },
        { "0004000000164800", "POKEMON SUN (NOVA SUN)" },
        { "0004000000175E00", "POKEMON MOON (UMBRA MOON)" },
        { "00040000001B5000", "POKEMON ULTRA SUN (PHOTONIC SUN)" },
        { "00040000001B5100", "POKEMON ULTRA MOON (PRISMATIC MOON)" },
        { "0004000000030800", "MARIO KART 7 (CTGP-7)" },
        { "00040000000EDF00", "SUPER SMASH BROS 3DS" }
    };
    for (size_t i = 0; i < sizeof(k_titles) / sizeof(k_titles[0]); i++) {
        if (str_ieq(title_id, k_titles[i].tid)) {
            return k_titles[i].name;
        }
    }
    return "CUSTOM 3DS LAYEREDFS MOD";
}

bool sd_parse_log_or_dump(const char *full_path, char *out_summary, size_t summary_cap) {
    if (full_path == NULL || out_summary == NULL || summary_cap < 16) {
        return false;
    }
    FILE *fp = fopen(full_path, "rb");
    if (fp == NULL) {
        return false;
    }
    uint8_t buf[256];
    memset(buf, 0, sizeof(buf));
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    if (n == 0) {
        snprintf(out_summary, summary_cap, "EMPTY LOG FILE");
        return true;
    }

    /* Check Luma3DS exception dump magic ("Luma3DS" or processor header 0xDEADBEEF / ARM11) */
    size_t len = strlen(full_path);
    bool is_dmp = (len >= 4 && strcmp(full_path + len - 4, ".dmp") == 0);
    if (is_dmp && n >= 32) {
        uint32_t magic = read_u32_le(buf + 0x00);
        uint32_t proc  = read_u32_le(buf + 0x08);
        uint32_t exc   = read_u32_le(buf + 0x0C);
        uint32_t pc    = read_u32_le(buf + 0x10);
        uint32_t lr    = read_u32_le(buf + 0x14);
        const char *exc_str = (exc == 0) ? "FIQ" : (exc == 1) ? "UNDEF" :
                              (exc == 2) ? "PREFETCH_ABORT" : "DATA_ABORT";
        snprintf(out_summary, summary_cap,
                 "LUMA DMP (HDR:%08X ARM%u %s PC:0x%08X LR:0x%08X)",
                 (unsigned)magic, (unsigned)(proc == 9 ? 9 : 11),
                 exc_str, (unsigned)pc, (unsigned)lr);
        return true;
    }

    /* Text log / errdisp.txt: extract first non-empty line, sanitized */
    char clean[160];
    size_t ci = 0;
    for (size_t i = 0; i < n && ci + 1 < sizeof(clean); i++) {
        char c = (char)buf[i];
        if (c == '\r' || c == '\n') {
            if (ci > 0) {
                clean[ci++] = ' ';
                clean[ci++] = '|';
                clean[ci++] = ' ';
            }
        } else if (c >= 32 && c <= 126 && c != '"' && c != '\\') {
            clean[ci++] = c;
        }
    }
    clean[ci] = '\0';
    snprintf(out_summary, summary_cap, "%s", ci > 0 ? clean : "BINARY/TEXT LOG");
    return true;
}

static void scan_layeredfs_dir(const char *sd_root, const char *subdir, bool enabled, sd_telemetry_t *out) {
    char dir_path[SD_PATH_MAX];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", sd_root, subdir);
    DIR *d = opendir(dir_path);
    if (d == NULL) {
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && out->mod_count < SD_MAX_MODS) {
        if (ent->d_name[0] == '.') continue;
        char full[SD_PATH_MAX];
        int n = snprintf(full, sizeof(full), "%s/%s", dir_path, ent->d_name);
        if (n < 0 || (size_t)n >= sizeof(full)) continue;
        struct stat st;
        if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        int idx = out->mod_count;
        if (idx < 0 || idx >= SD_MAX_MODS) break;
        sd_layeredfs_mod_t m;
        memset(&m, 0, sizeof(m));
        size_t dlen = strlen(ent->d_name);
        if (dlen >= sizeof(m.title_id)) dlen = sizeof(m.title_id) - 1;
        memcpy(m.title_id, ent->d_name, dlen);
        m.title_id[dlen] = '\0';
        snprintf(m.title_name, sizeof(m.title_name), "%s", sd_lookup_title_name(m.title_id));
        m.enabled = enabled;

        char probe[SD_PATH_MAX + 32];
        snprintf(probe, sizeof(probe), "%s/romfs", full);
        m.has_romfs = (stat(probe, &st) == 0);
        snprintf(probe, sizeof(probe), "%s/code.bin", full);
        m.has_code_bin = (stat(probe, &st) == 0);
        snprintf(probe, sizeof(probe), "%s/exefs", full);
        m.has_exefs = (stat(probe, &st) == 0);
        out->mods[idx] = m;
        out->mod_count = idx + 1;
    }
    closedir(d);
}

static int count_entries_in_dir(const char *dir_path, const char *suffix) {
    DIR *d = opendir(dir_path);
    if (d == NULL) return 0;
    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (suffix == NULL) {
            count++;
        } else {
            size_t nl = strlen(ent->d_name);
            size_t sl = strlen(suffix);
            if (nl >= sl && strcmp(ent->d_name + nl - sl, suffix) == 0) {
                count++;
            }
        }
    }
    closedir(d);
    return count;
}

static void scan_logs_recursive(const char *sd_root, const char *rel_dir, int depth, sd_telemetry_t *out) {
    if (depth > 3 || out->log_count >= SD_MAX_LOGS) return;
    char dir_path[SD_PATH_MAX];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", sd_root, rel_dir);
    DIR *d = opendir(dir_path);
    if (d == NULL) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && out->log_count < SD_MAX_LOGS) {
        if (ent->d_name[0] == '.') continue;
        char rel_child[256];
        int rn = snprintf(rel_child, sizeof(rel_child), "%s/%s", rel_dir, ent->d_name);
        if (rn < 0 || (size_t)rn >= sizeof(rel_child)) continue;

        char full[SD_PATH_MAX];
        int fn = snprintf(full, sizeof(full), "%s/%s", sd_root, rel_child);
        if (fn < 0 || (size_t)fn >= sizeof(full)) continue;

        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            scan_logs_recursive(sd_root, rel_child, depth + 1, out);
        } else if (S_ISREG(st.st_mode)) {
            size_t nl = strlen(ent->d_name);
            bool is_dmp = (nl >= 4 && strcmp(ent->d_name + nl - 4, ".dmp") == 0);
            bool is_log = (nl >= 4 && strcmp(ent->d_name + nl - 4, ".log") == 0) ||
                          strcmp(ent->d_name, "errdisp.txt") == 0 ||
                          strcmp(ent->d_name, "crash.txt") == 0;
            if (is_dmp || is_log) {
                sd_log_entry_t *le = &out->logs[out->log_count++];
                memset(le, 0, sizeof(*le));
                snprintf(le->rel_path, sizeof(le->rel_path), "%s", rel_child);
                le->file_size = (size_t)st.st_size;
                le->is_crash_dump = is_dmp || strcmp(ent->d_name, "errdisp.txt") == 0;
                sd_parse_log_or_dump(full, le->summary, sizeof(le->summary));
            }
        }
    }
    closedir(d);
}

bool sd_scan_sd_card(const char *sd_root, sd_telemetry_t *out) {
    if (sd_root == NULL || out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->battery_percent = 88;
    out->battery_charging = true;
    out->wifi_strength = 3;
    snprintf(out->ip_addr, sizeof(out->ip_addr), "127.0.0.1");
    snprintf(out->console_model, sizeof(out->console_model), "NEW 3DS XL (LUMA3DS CFW)");

#ifdef __3DS__
    u8 bat_lvl = 4;
    if (R_SUCCEEDED(PTMU_GetBatteryLevel(&bat_lvl))) {
        static const uint8_t k_lvl_pct[6] = { 5, 15, 35, 60, 85, 100 };
        out->battery_percent = k_lvl_pct[bat_lvl <= 5 ? bat_lvl : 5];
    }
    u8 chg = 0;
    if (R_SUCCEEDED(PTMU_GetBatteryChargeState(&chg))) {
        out->battery_charging = (chg != 0);
    }
    out->wifi_strength = osGetWifiStrength();
    bool is_n3ds = false;
    if (R_SUCCEEDED(APT_CheckNew3DS(&is_n3ds))) {
        snprintf(out->console_model, sizeof(out->console_model),
                 is_n3ds ? "NEW 3DS (LUMA3DS CFW)" : "OLD 3DS / 2DS (LUMA3DS CFW)");
    }
    uint32_t hid = (uint32_t)gethostid();
    if (hid != 0 && hid != 0x7F000001u && hid != 0x0100007Fu) {
        struct in_addr ia;
        ia.s_addr = hid;
        const char *ips = inet_ntoa(ia);
        if (ips != NULL && ips[0] != '\0') {
            snprintf(out->ip_addr, sizeof(out->ip_addr), "%s", ips);
        }
    } else {
        snprintf(out->ip_addr, sizeof(out->ip_addr), "0.0.0.0 (OFFLINE)");
    }
#endif

    struct statvfs vfs;
    if (statvfs(sd_root, &vfs) == 0) {
        out->sd_free_bytes  = (uint64_t)vfs.f_bavail * (uint64_t)vfs.f_frsize;
        out->sd_total_bytes = (uint64_t)vfs.f_blocks * (uint64_t)vfs.f_frsize;
    } else {
        out->sd_free_bytes  = 16ULL * 1024ULL * 1024ULL * 1024ULL;
        out->sd_total_bytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;
    }

    char path[SD_PATH_MAX];
    snprintf(path, sizeof(path), "%s/3ds/Checkpoint/saves", sd_root);
    out->checkpoint_save_dirs = count_entries_in_dir(path, NULL);

    snprintf(path, sizeof(path), "%s/roms/nds/saves", sd_root);
    out->nds_save_files = count_entries_in_dir(path, ".sav");

    snprintf(path, sizeof(path), "%s/roms/gba", sd_root);
    out->gba_save_files = count_entries_in_dir(path, ".sav");

    snprintf(path, sizeof(path), "%s/3ds", sd_root);
    out->homebrew_3dsx_count = count_entries_in_dir(path, ".3dsx");

    scan_layeredfs_dir(sd_root, "luma/titles", true, out);
    scan_layeredfs_dir(sd_root, "luma/titles_disabled", false, out);

    scan_logs_recursive(sd_root, "luma", 0, out);
    scan_logs_recursive(sd_root, "3ds", 0, out);

    return true;
}

static void ensure_dir(const char *path) {
    char tmp[SD_PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}

bool sd_toggle_layeredfs_mod(const char *sd_root, sd_layeredfs_mod_t *mod) {
    if (sd_root == NULL || mod == NULL || mod->title_id[0] == '\0') {
        return false;
    }
    char active_parent[SD_PATH_MAX];
    char disabled_parent[SD_PATH_MAX];
    char active_dir[SD_PATH_MAX + 32];
    char disabled_dir[SD_PATH_MAX + 32];

    snprintf(active_parent, sizeof(active_parent), "%s/luma/titles", sd_root);
    snprintf(disabled_parent, sizeof(disabled_parent), "%s/luma/titles_disabled", sd_root);
    ensure_dir(active_parent);
    ensure_dir(disabled_parent);

    snprintf(active_dir, sizeof(active_dir), "%s/%s", active_parent, mod->title_id);
    snprintf(disabled_dir, sizeof(disabled_dir), "%s/%s", disabled_parent, mod->title_id);

    if (mod->enabled) {
        if (rename(active_dir, disabled_dir) != 0) {
            return false;
        }
        mod->enabled = false;
    } else {
        if (rename(disabled_dir, active_dir) != 0) {
            return false;
        }
        mod->enabled = true;
    }
    return true;
}

bool sd_export_diagnostic_snapshot(const char *sd_root, const sd_telemetry_t *t, char *out_json_path, size_t cap) {
    if (sd_root == NULL || t == NULL) {
        return false;
    }
    char out_dir[SD_PATH_MAX];
    snprintf(out_dir, sizeof(out_dir), "%s/3ds/sysdash", sd_root);
    ensure_dir(out_dir);

    char json_path[SD_PATH_MAX + 48];
    char tmp_path[SD_PATH_MAX + 64];
    snprintf(json_path, sizeof(json_path), "%s/diagnostic_snapshot.json", out_dir);
    snprintf(tmp_path, sizeof(tmp_path), "%s/diagnostic_snapshot.json.tmp", out_dir);

    FILE *fp = fopen(tmp_path, "wb");
    if (fp == NULL) {
        return false;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"console_model\": \"%s\",\n", t->console_model);
    fprintf(fp, "  \"ip_addr\": \"%s\",\n", t->ip_addr);
    fprintf(fp, "  \"battery_percent\": %u,\n", (unsigned)t->battery_percent);
    fprintf(fp, "  \"battery_charging\": %s,\n", t->battery_charging ? "true" : "false");
    fprintf(fp, "  \"sd_free_bytes\": %llu,\n", (unsigned long long)t->sd_free_bytes);
    fprintf(fp, "  \"sd_total_bytes\": %llu,\n", (unsigned long long)t->sd_total_bytes);
    fprintf(fp, "  \"checkpoint_save_dirs\": %d,\n", t->checkpoint_save_dirs);
    fprintf(fp, "  \"nds_save_files\": %d,\n", t->nds_save_files);
    fprintf(fp, "  \"gba_save_files\": %d,\n", t->gba_save_files);
    fprintf(fp, "  \"layeredfs_mods\": [\n");
    for (int i = 0; i < t->mod_count; i++) {
        const sd_layeredfs_mod_t *m = &t->mods[i];
        fprintf(fp, "    {\"title_id\": \"%s\", \"name\": \"%s\", \"enabled\": %s, \"has_romfs\": %s, \"has_code_bin\": %s}%s\n",
                m->title_id, m->title_name,
                m->enabled ? "true" : "false",
                m->has_romfs ? "true" : "false",
                m->has_code_bin ? "true" : "false",
                (i + 1 < t->mod_count) ? "," : "");
    }
    fprintf(fp, "  ],\n");
    fprintf(fp, "  \"logs\": [\n");
    for (int i = 0; i < t->log_count; i++) {
        const sd_log_entry_t *l = &t->logs[i];
        fprintf(fp, "    {\"path\": \"%s\", \"size\": %zu, \"is_crash_dump\": %s, \"summary\": \"%s\"}%s\n",
                l->rel_path, l->file_size,
                l->is_crash_dump ? "true" : "false",
                l->summary,
                (i + 1 < t->log_count) ? "," : "");
    }
    fprintf(fp, "  ]\n}\n");
    fflush(fp);
    fclose(fp);

    if (rename(tmp_path, json_path) != 0) {
        remove(tmp_path);
        return false;
    }

    if (out_json_path != NULL && cap > 0) {
        snprintf(out_json_path, cap, "%s", json_path);
    }
    return true;
}

int sd_purge_stale_homebrew_titles(void) {
#ifdef __3DS__
    /* Legacy UniqueIds (0x0D3D0..0x0D3DF) before the 32-byte CBMD / 0x105 SMDH fix.
       3DS TitleIDs encode UniqueId in bits 8..27: 0x0004000000000000ULL | ((u64)UniqueId << 8) */
    int removed = 0;
    for (uint32_t uid = 0x0D3D0u; uid <= 0x0D3DFu; uid++) {
        u64 tid = 0x0004000000000000ULL | ((u64)uid << 8);
        if (R_SUCCEEDED(AM_DeleteUserTitle(MEDIATYPE_SD, tid))) {
            removed++;
        }
        AM_DeleteTicket(tid);
        AM_DeletePendingTitle(MEDIATYPE_SD, tid);
    }
    AM_DeleteAllPendingTitles(MEDIATYPE_SD);
    AM_DoCleanup(MEDIATYPE_SD);
    return removed;
#else
    return 4;
#endif
}
