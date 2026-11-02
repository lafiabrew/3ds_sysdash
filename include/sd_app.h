#ifndef SD_APP_H
#define SD_APP_H

#define PK_DEFAULT_SAVE_DIR_3DS "sdmc:/3ds/sysdash"
#include "pk_platform.h"
#include "sd_sysdash.h"

typedef enum {
    SD_TAB_MODS = 0,
    SD_TAB_LOGS = 1,
    SD_TAB_TITLES = 2
} sd_tab_t;

typedef struct {
    char sd_root[SD_PATH_MAX];
    sd_telemetry_t telemetry;
    sd_tab_t active_tab;
    int selected_mod;
    int selected_log;
    int selected_title;
    char status_msg[128];
} sd_app_t;

void sd_app_init(sd_app_t *app, const char *sd_root);
void sd_app_step(sd_app_t *app, const pk_input_t *input);
void sd_app_render(const sd_app_t *app);

#endif /* SD_APP_H */
