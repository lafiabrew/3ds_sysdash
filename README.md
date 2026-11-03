# SysDash (`3ds_sysdash`)

<p align="center">
  <img src="docs/icon.png" width="72" height="72" alt="3ds_sysdash icon" />
  &nbsp;&nbsp;&nbsp;
  <img src="docs/banner.png" width="256" height="128" alt="3ds_sysdash 3DS banner" />
</p>

<p align="center">
  <img src="docs/screenshot.png" width="360" alt="3ds_sysdash dual-screen 3DS preview" />
</p>

> **System & Mod Manager (`CTR-P-SYSD`, UniqueId `0x0D3E2`)** — Part of the [**lafiabrew**](https://github.com/orgs/lafiabrew/repositories) Nintendo 3DS Homebrew Suite (companion hub: [`lafiabrew/3ds_hub`](https://github.com/lafiabrew/3ds_hub)).

Nintendo 3DS CFW hardware/battery/IP inspector, 1-button Luma3DS LayeredFS ROM-hack mod toggler, crash dump analyzer, and 128-title scrollable manager with 1-button black-box title purging.

---

## Architecture & Data Flow

```mermaid
flowchart LR
    HW["3DS Services<br/>• ptm:u (Battery/Steps)<br/>• soc:U (LAN IP)<br/>• am:u / am:net (Titles)"]
    SD[("SD Card<br/>• luma/titles/<br/>• luma/titles_disabled/<br/>• luma/dumps/arm11/*.dmp")]
    UI["SysDash 3-Tab UI (8-Row Scroll)<br/>• L:MODS (LayeredFS Toggler)<br/>• LOGS (Crash Dump Decoder)<br/>• TITLES:R (128-Title Cleaner)"]
    SNAP["sdmc:/3ds/sysdash/<br/>diagnostic_snapshot.json"]

    HW & SD --> UI
    UI -->|"X Button"| SNAP
    UI -->|"SELECT Button"| PURGE["Purge [OLD] 0x0D3D*<br/>Black-Box Titles & Tickets"]
```

---

## Controls & Touch Ergonomics (Old 3DS · 2DS · New 3DS XL)

| Button / Input | Action |
|---|---|
| `L` / `R` (or Touch Tabs) | Switch between **`L:MODS`** (LayeredFS), **`LOGS`** (Crash Dumps), and **`TITLES:R`** (Installed SD Titles) |
| `D-Pad Up / Down` | Scroll 1 row up/down through the 8-row sliding viewport (with proportional scrollbar) |
| `D-Pad Left / Right` | Page Jump **8 rows** up/down (fast navigation through 100+ installed titles) |
| `A` (or Double-Tap Row) | Toggle selected Luma3DS LayeredFS mod (`ON`/`OFF`) or delete selected `[OLD]`/`[APP]` SD title + ticket |
| `SELECT` | **1-Button Black-Box Purge**: Deletes all legacy `0x0D3D0..0x0D3DF` test titles, tickets, and pending imports |
| `X` | Export full JSON diagnostic snapshot (`sdmc:/3ds/sysdash/diagnostic_snapshot.json`) |
| `Y` | Rescan SD card mods, logs, and installed titles |
| `START` | Exit to HOME Menu / Homebrew Launcher |

---

## SD Card Paths & Telemetry Logs

- `sdmc:/luma/titles/<TitleID>/` ↔ `sdmc:/luma/titles_disabled/<TitleID>/` — Atomic LayeredFS ROM-hack toggling
- `sdmc:/luma/errdisp.txt` & `sdmc:/luma/dumps/arm11/*.dmp` — Decoded ARM11/ARM9 exception headers
- `sdmc:/3ds/sysdash/diagnostic_snapshot.json` — Exported system + mod + crash log snapshot
- `sdmc:/3ds/logs/sysdash.log` — Structured UTC operational log

---

## Build, Verify & Install

```bash
# 1. Run host C11 ASan/UBSan pass-the-pass & fail-the-fail unit + UI tests
make test

# 2. Build optimized ARM11 (-Os -flto + stripped ELF) .elf, .smdh, .3dsx, and .cia
make 3ds && make verify-3ds

# 3. Serve FBI QR code or push .cia over LAN via serve_3ds.py
make qr
```
