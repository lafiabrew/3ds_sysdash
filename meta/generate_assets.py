#!/usr/bin/env python3
"""
Deterministic generator for SysDash CFW (3ds_sysdash) packaging assets:
- meta/icon.png (48x48 RGBA PNG: Emerald/Teal CPU Microchip + Battery Bar + Luma3DS Star)
- meta/banner.png (256x128 RGBA PNG: Circuit board & CFW diagnostic gauge emblem)
- meta/audio.wav & meta/banner.bnr
"""

import math
import os
import struct
import wave
import zlib
from generate_bnr_shared import draw_banner_text_centered, write_cbmd_bnr  # type: ignore


def write_png(path: str, width: int, height: int, rgba_bytes: bytes) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    raw_rows = bytearray()
    stride = width * 4
    for y in range(height):
        raw_rows.append(0)
        raw_rows.extend(rgba_bytes[y * stride : (y + 1) * stride])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    compressed = zlib.compress(bytes(raw_rows), 9)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", compressed))
        f.write(chunk(b"IEND", b""))


def generate_icon(path: str) -> None:
    w, h = 48, 48
    buf = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            idx = (y * w + x) * 4
            # Matrix emerald border
            if x < 2 or x >= w - 2 or y < 2 or y >= h - 2:
                buf[idx : idx + 4] = bytes((48, 225, 150, 255))
                continue
            buf[idx : idx + 4] = bytes((12, 24, 22, 255))

            # Gold IC pins sticking out of the 4 sides of the CPU package (13..34)
            in_pin_band_x = x in (16, 17, 21, 22, 26, 27, 31, 32)
            in_pin_band_y = y in (16, 17, 21, 22, 26, 27, 31, 32)
            if (in_pin_band_x and (8 <= y < 13 or 35 <= y < 40)) or (
                in_pin_band_y and (8 <= x < 13 or 35 <= x < 40)
            ):
                buf[idx : idx + 4] = bytes((245, 205, 85, 255))
                continue

            # CPU package body (13..34, 13..34)
            if 13 <= x <= 34 and 13 <= y <= 34:
                if x in (13, 34) or y in (13, 34):
                    buf[idx : idx + 4] = bytes((70, 240, 170, 255))
                else:
                    buf[idx : idx + 4] = bytes((22, 46, 42, 255))
                    # 3 glowing battery/telemetry bars inside the chip die
                    if 17 <= x <= 30:
                        if 17 <= y <= 19:
                            buf[idx : idx + 4] = bytes((80, 255, 160, 255))
                        elif 22 <= y <= 24:
                            buf[idx : idx + 4] = bytes((95, 220, 255, 255))
                        elif 27 <= y <= 29 and x <= 26:
                            buf[idx : idx + 4] = bytes((255, 215, 90, 255))

    write_png(path, w, h, bytes(buf))


def generate_banner(path: str) -> bytes:
    w, h = 256, 128
    buf = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            idx = (y * w + x) * 4
            r = 10 + (y * 12) // h
            g = 28 + (y * 28) // h
            b = 26 + (y * 24) // h
            buf[idx : idx + 4] = bytes((r, g, b, 255))

            if 96 <= x <= 160 and 16 <= y <= 76:
                if x in (96, 160) or y in (16, 76):
                    buf[idx : idx + 4] = bytes((65, 240, 165, 255))
                elif 106 <= x <= 150 and ((28 <= y <= 36) or (44 <= y <= 52) or (60 <= y <= 68)):
                    buf[idx : idx + 4] = bytes((85, 245, 175, 255))

    draw_banner_text_centered(buf, w, h, 86, "SYSDASH", 2, (95, 255, 175, 255))
    draw_banner_text_centered(buf, w, h, 108, "SYSTEM & MOD MANAGER", 1, (215, 245, 230, 255))
    raw = bytes(buf)
    write_png(path, w, h, raw)
    return raw


def generate_audio(path: str) -> None:
    sample_rate = 22050
    duration_s = 0.8
    n_samples = int(sample_rate * duration_s)
    with wave.open(path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        frames = bytearray()
        for i in range(n_samples):
            t = i / sample_rate
            freq = 440.0 if t < 0.25 else (554.37 if t < 0.5 else 659.25)
            env = max(0.0, 1.0 - (t / duration_s))
            val = int(12000 * env * math.sin(2.0 * math.pi * freq * t))
            frames.extend(struct.pack("<h", val))
        wf.writeframes(bytes(frames))


def main() -> None:
    meta_dir = os.path.dirname(os.path.abspath(__file__))
    generate_icon(os.path.join(meta_dir, "icon.png"))
    banner_rgba = generate_banner(os.path.join(meta_dir, "banner.png"))
    generate_audio(os.path.join(meta_dir, "audio.wav"))
    write_cbmd_bnr(os.path.join(meta_dir, "banner.bnr"), (440.0, 554.37, 659.25), banner_rgba)
    print("Generated SysDash meta/icon.png, meta/banner.png, meta/audio.wav, meta/banner.bnr")


if __name__ == "__main__":
    main()
