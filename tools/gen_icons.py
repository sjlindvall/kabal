#!/usr/bin/env python3
"""Generate 32x32 ICO card assets used by solitaire.rc.

This keeps binary icon generation reproducible and avoids hand-editing binary files.
"""
import argparse
import os
import struct
from pathlib import Path

W = 32
H = 32


def write_ico(path: Path, pixels):
    xor_size = W * H * 4
    and_stride = ((W + 31) // 32) * 4
    and_size = and_stride * H
    bi = struct.pack('<IIIHHIIIIII', 40, W, H * 2, 1, 32, 0, xor_size + and_size, 0, 0, 0, 0)

    data = bytearray()
    for y in range(H - 1, -1, -1):
        for x in range(W):
            r, g, b, a = pixels[y][x]
            data += bytes((b, g, r, a))
    data += bytes(and_size)
    image = bi + data

    header = struct.pack('<HHH', 0, 1, 1)
    entry = struct.pack('<BBBBHHII', W if W < 256 else 0, H if H < 256 else 0, 0, 0, 1, 32, len(image), 6 + 16)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('wb') as f:
        f.write(header + entry + image)


def solid(color, border=(255, 255, 255, 255)):
    px = []
    for y in range(H):
        row = []
        for x in range(W):
            row.append(border if x in (0, W - 1) or y in (0, H - 1) else color)
        px.append(row)
    return px


def main(out_dir: Path):
    write_ico(out_dir / 'card_back.ico', solid((30, 30, 180, 255), (230, 230, 255, 255)))
    write_ico(out_dir / 'card_empty.ico', solid((0, 90, 0, 255), (255, 255, 255, 255)))

    suit_colors = [
        (10, 10, 10, 255),
        (180, 20, 20, 255),
        (200, 30, 30, 255),
        (20, 20, 20, 255),
    ]

    for suit in range(4):
        for rank in range(13):
            px = solid((245, 245, 245, 255), (255, 255, 255, 255))
            color = suit_colors[suit]
            for y in range(2, 6):
                for x in range(2 + suit * 7, 8 + suit * 7):
                    px[y][x] = color

            count = rank + 1
            for n in range(count):
                x = 3 + (n % 6) * 4
                y = 8 + (n // 6) * 8
                for yy in range(y, min(y + 6, H - 2)):
                    for xx in range(x, min(x + 2, W - 2)):
                        px[yy][xx] = color

            idx = suit * 13 + rank
            write_ico(out_dir / f'card_{idx:02d}.ico', px)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', default='icons', help='Output directory for generated .ico files')
    args = parser.parse_args()
    main(Path(args.out))
    print(f'Generated icons in {args.out}')
