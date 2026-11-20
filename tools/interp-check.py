#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# 60 fps in-between check: for each in-between picture I between real pictures A (before) and B (after),
# the per-pixel amount I lies outside [min(A,B), max(A,B)], summed per 16x16 tile. Halfway motion puts some
# pixels out of range at moving edges; a warped or flickering draw puts a whole patch out of range.
#   interp-check.py FRAMES_DIR [TOP]
# Frames: SKIES_DUMP_DIR output (frame_NNNNNN.ppm real, frame_NNNNNN-interp.ppm in-between), converted here to
# 320x240 grey with ImageMagick. Prints the worst tiles: frame, tile x/y, score.
import os, re, subprocess, sys

D = sys.argv[1]
TOP = int(sys.argv[2]) if len(sys.argv) > 2 else 25
W, H, T = 320, 240, 16

def grey(path):
    return subprocess.run(['convert', path, '-resize', f'{W}x{H}!', '-colorspace', 'Gray', '-depth', '8', 'gray:-'],
                          check=True, capture_output=True).stdout

names = os.listdir(D)
real = {int(m.group(1)): n for n in names if (m := re.fullmatch(r'frame_(\d+)\.ppm', n))}
inter = {int(m.group(1)): n for n in names if (m := re.fullmatch(r'frame_(\d+)-interp\.ppm', n))}
cache = {}
def get(n):
    if n not in cache:
        cache[n] = grey(os.path.join(D, n))
    return cache[n]

rows = []
for f in sorted(inter):
    # the in-between shown before frame f's real picture blends frame f-1 and frame f
    if f - 1 not in real or f not in real:
        continue
    a, b, i = get(real[f - 1]), get(real[f]), get(inter[f])
    tiles = [0] * ((W // T) * (H // T))
    for y in range(H):
        row = y * W
        ty = (y // T) * (W // T)
        for x in range(W):
            p = row + x
            av, bv, iv = a[p], b[p], i[p]
            lo, hi = (av, bv) if av < bv else (bv, av)
            d = iv - hi if iv > hi else (lo - iv if iv < lo else 0)
            if d > 8:
                tiles[ty + x // T] += d
    worst = max(range(len(tiles)), key=tiles.__getitem__)
    total = sum(tiles)
    rows.append((tiles[worst], total, f, worst % (W // T), worst // (W // T)))
    cache.pop(real.get(f - 1), None)
    cache.pop(inter[f], None)
rows.sort(reverse=True)
print(f'{len(rows)} in-betweens; worst tiles (score, frame total, frame, tile x, tile y):')
for r in rows[:TOP]:
    print(*r)
