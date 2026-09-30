#!/usr/bin/env python3
"""Validate both built environments, including revision guards in both images."""
import hashlib
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[1]
for environment, bounds in (("waveshare_p4_nano", (1, 199)), ("waveshare_p4_nano_rev3", (300, 399))):
    folder = root / ".pio" / "build" / environment
    factory = (folder / "firmware.factory.bin").read_bytes()
    partitions = (folder / "partitions.bin").read_bytes()
    entries = []
    for offset in range(0, len(partitions), 32):
        magic, kind, subtype, start, size, label, flags = struct.unpack_from("<HBBII16sI", partitions, offset)
        if magic != 0x50AA:
            break
        entries.append((kind, subtype, start, size, label.rstrip(b"\0")))
    assert (1, 0x83, 0x210000, 0x200000, b"icons") in entries, (environment, "LittleFS partition")
    assert len(factory) <= 0x210000, (environment, "factory image must not overwrite icons")
    for name, offset in (("bootloader.bin", 0x2000), ("firmware.bin", 0x10000)):
        data = (folder / name).read_bytes()
        assert len(data) > 56 and data[0] == 0xE9, (environment, name, "image magic")
        assert data[3] >> 4 == 4, (environment, name, "16 MB flash header")
        fields = struct.unpack_from("<BBBBHBHHBBBBB", data, 8)
        assert fields[4] == 18, (environment, name, "ESP32-P4 chip ID")
        assert (fields[6], fields[7]) == bounds, (environment, name, "revision bounds", fields[6:8])
        assert fields[-1] == 1 and hashlib.sha256(data[:-32]).digest() == data[-32:], (environment, name, "SHA-256")
        assert factory[offset:offset + len(data)] == data, (environment, name, "factory image mismatch")
        print(f"PASS: {environment}/{name}: 16 MB, revisions {bounds}, SHA-256, factory offset")
