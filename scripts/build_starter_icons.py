#!/usr/bin/env python3
"""Rebuild the committed starter bundle from selected original PNGs. Requires Pillow.
PlatformIO embeds the prebuilt file; Pillow is NOT a firmware build dependency.
"""
from pathlib import Path
import json
from PIL import Image
root=Path(__file__).resolve().parents[1]
folder=root/'assets/starter-icons'
manifest=json.loads((folder/'manifest.json').read_text())
bundle=bytearray(b'SDICONS1'+bytes([len(manifest)]))
for entry in manifest:
    name=entry['id'].encode('ascii')
    assert 0<len(name)<=32
    with Image.open(folder/(entry['id']+'.png')) as source:
        image=source.convert('RGBA')
        if entry.get("upscale"):
            scale=min(80/image.width,80/image.height)
            image=image.resize((round(image.width*scale),round(image.height*scale)),Image.Resampling.LANCZOS)
        else:
            image.thumbnail((80,80),Image.Resampling.LANCZOS)
        canvas=Image.new('RGBA',(80,80))
        canvas.paste(image,((80-image.width)//2,(80-image.height)//2))
        bundle+=name.ljust(33,b'\0')+canvas.tobytes()
(root/'src/starter_icons.bin').write_bytes(bundle)
print(f'{len(manifest)} icons, {len(bundle)} bytes')
