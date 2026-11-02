#include "pk_platform.h"
#include "sd_app.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *sd_root = NULL;
    bool one_shot = false;
    bool export_snapshot = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sd-root") == 0 && i + 1 < argc) {
            sd_root = argv[++i];
        } else if (strcmp(argv[i], "--once") == 0) {
            one_shot = true;
        } else if (strcmp(argv[i], "--export-snapshot") == 0) {
            export_snapshot = true;
        }
    }

    if (!pk_platform_init()) {
        return 1;
    }

    sd_app_t app;
    sd_app_init(&app, sd_root);
    if (export_snapshot) {
        sd_export_diagnostic_snapshot(app.sd_root, &app.telemetry, NULL, 0);
    }

    while (pk_platform_main_loop()) {
        pk_input_t in;
        pk_platform_poll_input(&in);
        if (in.down & PK_BTN_START) {
            break;
        }
        sd_app_step(&app, &in);
        sd_app_render(&app);
        if (one_shot) {
            break;
        }
    }

    pk_platform_shutdown();
    return 0;
}
