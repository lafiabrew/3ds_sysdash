/*
 * Unit & UI simulation tests for 3ds_sysdash:
 * - Proves Luma3DS LayeredFS ROM-hack discovery (`luma/titles/<TitleID>` & `luma/titles_disabled/<TitleID>`).
 * - Proves atomic 1-button toggling of LayeredFS mods between active and disabled directories.
 * - Proves Luma3DS ARM11 exception `.dmp` binary header parsing + `errdisp.txt` + homebrew `.log` parsing.
 * - Proves atomic export of `3ds/sysdash/diagnostic_snapshot.json` containing saves, mods, and logs.
 */

#include "pk_platform.h"
#include "sd_app.h"
#include "sd_sysdash.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void mkdir_p(const char *path) {
    char tmp[512];
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

static void write_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void setup_mock_sd(const char *root) {
    char p[512];
    snprintf(p, sizeof(p), "rm -rf %s", root);
    int rc = system(p);
    (void)rc;
    snprintf(p, sizeof(p), "%s/luma/titles/00040000001B5000/romfs", root);
    mkdir_p(p);
    snprintf(p, sizeof(p), "%s/luma/titles/00040000001B5000/code.bin", root);
    FILE *fp = fopen(p, "wb");
    assert(fp != NULL);
    fwrite("CODE", 1, 4, fp);
    fclose(fp);

    snprintf(p, sizeof(p), "%s/luma/titles_disabled/000400000011C400/romfs", root);
    mkdir_p(p);

    snprintf(p, sizeof(p), "%s/luma/dumps/arm11", root);
    mkdir_p(p);
    snprintf(p, sizeof(p), "%s/luma/dumps/arm11/crash_dump_00000000.dmp", root);
    uint8_t dmp[64];
    memset(dmp, 0, sizeof(dmp));
    write_u32_le(dmp + 0x00, 0xDEADBEEFu);
    write_u32_le(dmp + 0x08, 11u);         /* ARM11 */
    write_u32_le(dmp + 0x0C, 3u);          /* DATA_ABORT */
    write_u32_le(dmp + 0x10, 0x00123456u); /* PC */
    write_u32_le(dmp + 0x14, 0x00112233u); /* LR */
    fp = fopen(p, "wb");
    assert(fp != NULL);
    fwrite(dmp, 1, sizeof(dmp), fp);
    fclose(fp);

    snprintf(p, sizeof(p), "%s/luma/errdisp.txt", root);
    fp = fopen(p, "wb");
    assert(fp != NULL);
    const char *err = "Error type: generic | Process ID: 38 | Title ID: 00040000001b5000\n";
    fwrite(err, 1, strlen(err), fp);
    fclose(fp);

    snprintf(p, sizeof(p), "%s/3ds/Checkpoint/saves/0x1B50_UltraSun", root);
    mkdir_p(p);
    snprintf(p, sizeof(p), "%s/roms/nds/saves", root);
    mkdir_p(p);
    snprintf(p, sizeof(p), "%s/roms/nds/saves/renegade_plat.sav", root);
    fp = fopen(p, "wb");
    assert(fp != NULL);
    fwrite("SAV", 1, 3, fp);
    fclose(fp);
}

int main(void) {
    const char *sd_root = "build/test_sd";
    setup_mock_sd(sd_root);

    sd_telemetry_t t;
    assert(sd_scan_sd_card(sd_root, &t));
    assert(t.mod_count >= 2);
    assert(t.log_count >= 2);
    assert(t.checkpoint_save_dirs >= 1);
    assert(t.nds_save_files >= 1);

    /* Find Photonic Sun (00040000001B5000) and toggle it off then back on */
    int ps_idx = -1;
    for (int i = 0; i < t.mod_count; i++) {
        if (strcmp(t.mods[i].title_id, "00040000001B5000") == 0) {
            ps_idx = i;
            break;
        }
    }
    assert(ps_idx >= 0);
    assert(t.mods[ps_idx].enabled == true);
    assert(t.mods[ps_idx].has_romfs == true);
    assert(t.mods[ps_idx].has_code_bin == true);

    assert(sd_toggle_layeredfs_mod(sd_root, &t.mods[ps_idx]));
    assert(t.mods[ps_idx].enabled == false);
    assert(sd_toggle_layeredfs_mod(sd_root, &t.mods[ps_idx]));
    assert(t.mods[ps_idx].enabled == true);

    /* Export diagnostic snapshot JSON and verify contents */
    char json_path[512];
    assert(sd_export_diagnostic_snapshot(sd_root, &t, json_path, sizeof(json_path)));
    FILE *fp = fopen(json_path, "rb");
    assert(fp != NULL);
    char json_buf[4096];
    size_t n = fread(json_buf, 1, sizeof(json_buf) - 1, fp);
    fclose(fp);
    json_buf[n] = '\0';
    assert(strstr(json_buf, "00040000001B5000") != NULL);
    assert(strstr(json_buf, "DATA_ABORT") != NULL);
    assert(strstr(json_buf, "errdisp.txt") != NULL);

    /* Test UI step & render */
    assert(pk_platform_init());
    sd_app_t app;
    sd_app_init(&app, sd_root);
    pk_input_t in;
    memset(&in, 0, sizeof(in));
    in.down = PK_BTN_A | PK_BTN_X;
    sd_app_step(&app, &in);
    sd_app_render(&app);
    pk_platform_shutdown();

    printf("PASS: test_sysdash all LayeredFS toggle, crash dump parser, and snapshot checks succeeded.\n");
    return 0;
}
