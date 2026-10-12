#!/usr/bin/env python3
"""Print the projection-load trace of a game built with patches/game/trace2d.json: each distinct
call chain (ten return addresses, patch code left out) with its kind (2D = the orthographic
projection, 3D = perspective), hit count and first/last game frame, named from the decomp's
symbols.txt (../SkiesOfArcadiaLegends-Decomp).

usage: AUTO=<automation dir> tools/readtrace.py ADDR [reset] [2d]
  ADDR   the trace table, 0x80319078 (MetroTRK's message buffers, trace2d.c)
  2d     print only orthographic (2D) chains
  reset  clear the table afterwards, to trace only what happens next"""
import bisect, os, re, subprocess, sys

DRIVE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "drive.py")
SYMS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "SkiesOfArcadiaLegends-Decomp",
                    "config", "GEAE8P", "symbols.txt")
DEPTH, ENTRIES = 10, 128  # trace2d.c

funcs = []
for line in open(SYMS):
    m = re.match(r"(\S+) = \.text:0x([0-9A-F]+); // type:function size:0x([0-9A-F]+)", line)
    if m:
        funcs.append((int(m[2], 16), int(m[3], 16), m[1]))
funcs.sort()
starts = [f[0] for f in funcs]


def name(a):
    if a == 0:
        return "-"
    i = bisect.bisect_right(starts, a - 4) - 1
    if i >= 0 and funcs[i][0] <= a - 4 < funcs[i][0] + funcs[i][1]:
        return f"{funcs[i][2]}+{a - funcs[i][0]:X}"
    return f"{a:08X}"


addr = int(sys.argv[1], 16)
only2d = "2d" in sys.argv[2:]
size = 16 + ENTRIES * (DEPTH + 2) * 4
out = subprocess.run([sys.executable, DRIVE, f"peek:0x{addr:X}:{size}"], capture_output=True,
                     text=True, check=True).stdout
data = bytearray()
for line in out.splitlines():
    m = re.match(r"([0-9A-F]{8})\s+((?:[0-9a-f]{2} )+)", line)
    if m:
        data += bytes.fromhex(m[2].replace(" ", ""))
w = [int.from_bytes(data[i:i + 4], "big") for i in range(0, len(data), 4)]
used, dropped = w[1], w[2]
print(f"used={used} dropped={dropped}")
for i in range(min(used, ENTRIES)):
    e = w[4 + i * (DEPTH + 2): 4 + (i + 1) * (DEPTH + 2)]
    kind = "2D" if e[DEPTH] >> 31 else "3D"
    if only2d and kind != "2D":
        continue
    hits = e[DEPTH] & 0x7FFFFFFF
    print(f"{kind} hits={hits:6d} frames={e[DEPTH + 1] >> 16:5d}-{e[DEPTH + 1] & 0xFFFF:5d}  "
          + " <- ".join(name(a) for a in e[:DEPTH] if a))
if "reset" in sys.argv[2:]:
    subprocess.run([sys.executable, DRIVE, f"poke:0x{addr + 4:X}:0000000000000000"], check=True,
                   capture_output=True)
    print("reset")
