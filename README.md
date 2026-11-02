# SysDash (`3ds_sysdash`)

> **System & Mod Manager** — Nintendo 3DS Homebrew (`CTR-P-SYSD`, UniqueId `0x0D3E2`) by [**lafiabrew**](https://github.com/orgs/lafiabrew/repositories)

Nintendo 3DS CFW hardware/battery/IP inspector, 1-button Luma3DS LayeredFS ROM-hack mod toggler, and crash log snapshotter.

## Build & Verify

```bash
# Run host C11 ASan/UBSan unit & UI simulation tests
make test

# Build ARM11 3DS .elf, .smdh, .3dsx, and .cia (via devkitARM or rootless Podman container)
make 3ds && make verify-3ds

# Serve FBI QR code or push .cia over LAN
make qr
```
