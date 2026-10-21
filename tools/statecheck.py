#!/usr/bin/env python3
"""Where a savestate caught the game: its PC and the return addresses up the stack, and whether any
of them is in game-patch code (the low-memory gap 0x80001900-0x80002F00 or the MetroTRK area
0x802548E8-0x80257AF8). A state caught in patch code returns into whatever a later build put at that
address, so it hangs or crashes there (NOTES *Savestates and moved patch code*).

usage: tools/statecheck.py STATE.sav DOL
  DOL: the main.dol of the build that made the state (used to find main memory in the state)
Reads Dolphin's LZ4 state format (Core/StateFile.cpp); the register block is found by r2 / r13
(0x80350000 / 0x8034E720, the game's small-data bases)."""
import struct, sys

PATCH = ((0x80001900, 0x80002F00), (0x802548E8, 0x80257AF8))


def lz4_block(src, out):
    i, n = 0, len(src)
    while i < n:
        tok = src[i]; i += 1
        lit = tok >> 4
        if lit == 15:
            while True:
                b = src[i]; i += 1; lit += b
                if b != 255:
                    break
        out += src[i:i + lit]; i += lit
        if i >= n:
            break
        off = src[i] | (src[i + 1] << 8); i += 2
        ml = tok & 15
        if ml == 15:
            while True:
                b = src[i]; i += 1; ml += b
                if b != 255:
                    break
        ml += 4
        start = len(out) - off
        if off >= ml:
            out += out[start:start + ml]
        else:
            for k in range(ml):
                out.append(out[start + k])


def load_state(path):
    d = open(path, "rb").read()
    vlen = struct.unpack("<I", d[28:32])[0]
    p = 32 + vlen
    _, compression, payload, size = struct.unpack("<HHIQ", d[p:p + 16])
    p += 16 + payload
    if compression == 0:
        return d[p:]
    out = bytearray()
    while p < len(d) and len(out) < size:
        clen = struct.unpack("<i", d[p:p + 4])[0]; p += 4
        lz4_block(d[p:p + clen], out); p += clen
    return bytes(out)


def in_patch(a):
    return any(lo <= a < hi for lo, hi in PATCH)


def main():
    raw = load_state(sys.argv[1])
    dol = open(sys.argv[2], "rb").read()
    offs = struct.unpack(">18I", dol[0:72]); addrs = struct.unpack(">18I", dol[72:144])
    sig = dol[offs[1] + 0x10000:offs[1] + 0x10040]
    base = raw.find(sig) - (addrs[1] + 0x10000 - 0x80000000)
    mem = raw[base:base + 0x1800000]
    i = raw.find(struct.pack("<I", 0x8034E720))
    while i != -1 and struct.unpack("<I", raw[i - 44:i - 40])[0] != 0x80350000:
        i = raw.find(struct.pack("<I", 0x8034E720), i + 1)
    b = i - 13 * 4
    gpr = struct.unpack("<32I", raw[b:b + 128]); pc = struct.unpack("<I", raw[b + 128:b + 132])[0]
    word = lambda a: struct.unpack(">I", mem[a - 0x80000000:a - 0x7FFFFFFC])[0]
    chain, sp = [], gpr[1]
    for _ in range(24):
        nsp = word(sp)
        if not 0x80000000 <= nsp < 0x81800000:
            break
        chain.append(word(nsp + 4)); sp = nsp
    bad = [a for a in [pc] + chain if in_patch(a)]
    print(f"pc {pc:08X}  r1 {gpr[1]:08X}  chain " + " ".join(f"{a:08X}" for a in chain))
    print("in patch code: " + (" ".join(f"{a:08X}" for a in bad) if bad else "none"))
    sys.exit(1 if bad else 0)


main()
