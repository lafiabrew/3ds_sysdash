#include "sd_app.h"

#include <stdio.h>
#include <string.h>

void sd_app_init(sd_app_t *app, const char *sd_root) {
    if (app == NULL) return;
    memset(app, 0, sizeof(*app));
    snprintf(app->sd_root, sizeof(app->sd_root), "%s",
             sd_root != NULL ? sd_root : pk_platform_default_sd_root());
    sd_scan_sd_card(app->sd_root, &app->telemetry);
    snprintf(app->status_msg, sizeof(app->status_msg),
             "SYSDASH: %d LAYEREDFS MODS, %d LOGS FOUND",
             app->telemetry.mod_count, app->telemetry.log_count);
}

void sd_app_step(sd_app_t *app, const pk_input_t *in) {
    if (app == NULL || in == NULL) return;

    if ((in->down & PK_BTN_L) || (in->down & PK_BTN_R)) {
        app->active_tab = (app->active_tab == SD_TAB_MODS) ? SD_TAB_LOGS : SD_TAB_MODS;
    }
    if (in->touch_just_pressed && in->touch_y >= 0 && in->touch_y <= 28) {
        app->active_tab = (in->touch_x < 160) ? SD_TAB_MODS : SD_TAB_LOGS;
    }

    if (app->active_tab == SD_TAB_MODS) {
        if ((in->down & PK_BTN_DUP) && app->selected_mod > 0) {
            app->selected_mod--;
        }
        if ((in->down & PK_BTN_DDOWN) && app->selected_mod + 1 < app->telemetry.mod_count) {
            app->selected_mod++;
        }

        bool toggle = (in->down & PK_BTN_A) != 0;
        if (in->touch_just_pressed && in->touch_y >= 36 && in->touch_y < 196) {
            int row = (in->touch_y - 36) / 20;
            if (row >= 0 && row < app->telemetry.mod_count) {
                app->selected_mod = row;
                toggle = true;
            }
        }
        if (toggle && app->telemetry.mod_count > 0) {
            sd_layeredfs_mod_t *m = &app->telemetry.mods[app->selected_mod];
            if (sd_toggle_layeredfs_mod(app->sd_root, m)) {
                snprintf(app->status_msg, sizeof(app->status_msg),
                         "MOD %s -> %s", m->title_id, m->enabled ? "ENABLED" : "DISABLED");
            } else {
                snprintf(app->status_msg, sizeof(app->status_msg),
                         "ERR TOGGLING %s", m->title_id);
            }
        }
    } else {
        if ((in->down & PK_BTN_DUP) && app->selected_log > 0) {
            app->selected_log--;
        }
        if ((in->down & PK_BTN_DDOWN) && app->selected_log + 1 < app->telemetry.log_count) {
            app->selected_log++;
        }
    }

    if (in->down & PK_BTN_X) {
        char json_out[SD_PATH_MAX];
        if (sd_export_diagnostic_snapshot(app->sd_root, &app->telemetry, json_out, sizeof(json_out))) {
            snprintf(app->status_msg, sizeof(app->status_msg),
                     "SNAPSHOT SAVED: 3DS/SYSDASH/DIAGNOSTIC_SNAPSHOT.JSON");
        } else {
            snprintf(app->status_msg, sizeof(app->status_msg),
                     "ERR EXPORTING DIAGNOSTIC SNAPSHOT");
        }
    }
    if (in->down & PK_BTN_Y) {
        sd_scan_sd_card(app->sd_root, &app->telemetry);
        snprintf(app->status_msg, sizeof(app->status_msg),
                 "RESCANNED: %d MODS, %d LOGS",
                 app->telemetry.mod_count, app->telemetry.log_count);
    }
    if (in->down & PK_BTN_SELECT) {
        int purged = sd_purge_stale_homebrew_titles();
        snprintf(app->status_msg, sizeof(app->status_msg),
                 "PURGED %d STALE/BLACK-BOX TITLES & TICKETS!", purged);
    }
}

void sd_app_render(const sd_app_t *app) {
    if (app == NULL) return;
    pk_render_begin_frame();

    /* Top Screen (400x240) */
    pk_render_begin_target(PK_TARGET_TOP_LEFT, PK_RGBA8(15, 22, 34, 255));
    pk_draw_rect(0, 0, 400, 26, PK_RGBA8(28, 46, 76, 255));
    pk_draw_text(8, 7, 0.55f, PK_RGBA8(115, 235, 255, 255),
                 "3DS SYSDASH - CFW, LAYEREDFS & LOG SNAPSHOTTER");

    const sd_telemetry_t *t = &app->telemetry;
    char line[160];
    snprintf(line, sizeof(line), "MODEL: %s | BAT: %u%% (%s) | IP: %s",
             t->console_model, (unsigned)t->battery_percent,
             t->battery_charging ? "CHG" : "DIS", t->ip_addr);
    pk_draw_text(8, 34, 0.5f, PK_RGBA8(220, 235, 250, 255), line);

    uint64_t free_mb = t->sd_free_bytes / (1024ULL * 1024ULL);
    uint64_t tot_mb  = t->sd_total_bytes / (1024ULL * 1024ULL);
    snprintf(line, sizeof(line), "SD FREE: %llu MB / %llu MB | SAVES: 3DS:%d NDS:%d GBA:%d",
             (unsigned long long)free_mb, (unsigned long long)tot_mb,
             t->checkpoint_save_dirs, t->nds_save_files, t->gba_save_files);
    pk_draw_text(8, 50, 0.5f, PK_RGBA8(170, 235, 185, 255), line);

    pk_draw_rect(6, 68, 388, 140, PK_RGBA8(24, 34, 52, 255));
    pk_draw_rect_outline(6, 68, 388, 140, 1.0f, PK_RGBA8(75, 115, 180, 255));

    if (app->active_tab == SD_TAB_MODS && t->mod_count > 0) {
        const sd_layeredfs_mod_t *m = &t->mods[app->selected_mod];
        snprintf(line, sizeof(line), "MOD [%d/%d]: %s (%s)",
                 app->selected_mod + 1, t->mod_count, m->title_id,
                 m->enabled ? "ACTIVE" : "DISABLED");
        pk_draw_text(14, 78, 0.55f,
                     m->enabled ? PK_RGBA8(120, 250, 160, 255) : PK_RGBA8(255, 165, 110, 255),
                     line);
        pk_draw_text(14, 98, 0.5f, PK_RGBA8(245, 245, 255, 255), m->title_name);
        snprintf(line, sizeof(line), "ROMFS:%s  CODE.BIN:%s  EXEFS:%s",
                 m->has_romfs ? "YES" : "NO",
                 m->has_code_bin ? "YES" : "NO",
                 m->has_exefs ? "YES" : "NO");
        pk_draw_text(14, 118, 0.5f, PK_RGBA8(180, 205, 240, 255), line);
    } else if (app->active_tab == SD_TAB_LOGS && t->log_count > 0) {
        const sd_log_entry_t *l = &t->logs[app->selected_log];
        snprintf(line, sizeof(line), "LOG [%d/%d]: %s (%zu B) %s",
                 app->selected_log + 1, t->log_count, l->rel_path, l->file_size,
                 l->is_crash_dump ? "[CRASH DUMP]" : "[INFO]");
        pk_draw_text(14, 78, 0.5f,
                     l->is_crash_dump ? PK_RGBA8(255, 140, 140, 255) : PK_RGBA8(160, 230, 255, 255),
                     line);
        pk_draw_text(14, 102, 0.5f, PK_RGBA8(235, 235, 245, 255), l->summary);
    }

    pk_draw_text(14, 184, 0.48f, PK_RGBA8(250, 215, 110, 255),
                 "A: TOGGLE MOD | X: SNAPSHOT | SELECT: PURGE BLACK BOXES");

    pk_draw_rect(0, 216, 400, 24, PK_RGBA8(22, 32, 52, 255));
    pk_draw_text(8, 222, 0.5f, PK_RGBA8(120, 245, 170, 255), app->status_msg);

    /* Bottom Screen (320x240) */
    pk_render_begin_target(PK_TARGET_BOTTOM, PK_RGBA8(14, 18, 30, 255));
    pk_draw_rect(2, 2, 156, 24,
                 app->active_tab == SD_TAB_MODS ? PK_RGBA8(55, 100, 175, 255) : PK_RGBA8(28, 38, 60, 255));
    pk_draw_text(10, 8, 0.5f, PK_RGBA8(255, 255, 255, 255), "L: LAYEREDFS MODS");

    pk_draw_rect(162, 2, 156, 24,
                 app->active_tab == SD_TAB_LOGS ? PK_RGBA8(55, 100, 175, 255) : PK_RGBA8(28, 38, 60, 255));
    pk_draw_text(170, 8, 0.5f, PK_RGBA8(255, 255, 255, 255), "R: CRASH & SD LOGS");

    if (app->active_tab == SD_TAB_MODS) {
        for (int i = 0; i < t->mod_count && i < 8; i++) {
            float y = 36.0f + (float)i * 20.0f;
            bool sel = (i == app->selected_mod);
            pk_draw_rect(6.0f, y, 308.0f, 18.0f,
                         sel ? PK_RGBA8(60, 110, 190, 255) : PK_RGBA8(24, 32, 50, 255));
            snprintf(line, sizeof(line), "[%s] %s - %s",
                     t->mods[i].enabled ? "ON " : "OFF",
                     t->mods[i].title_id, t->mods[i].title_name);
            pk_draw_text(10.0f, y + 4.0f, 0.45f, PK_RGBA8(240, 245, 255, 255), line);
        }
    } else {
        for (int i = 0; i < t->log_count && i < 8; i++) {
            float y = 36.0f + (float)i * 20.0f;
            bool sel = (i == app->selected_log);
            pk_draw_rect(6.0f, y, 308.0f, 18.0f,
                         sel ? PK_RGBA8(60, 110, 190, 255) : PK_RGBA8(24, 32, 50, 255));
            pk_draw_text(10.0f, y + 4.0f, 0.45f, PK_RGBA8(240, 245, 255, 255), t->logs[i].rel_path);
        }
    }

    pk_render_end_frame();
}
