#!/usr/bin/env python3
"""Settings, Saves' Import (lib/ModernGekko/tools/save_import.cpp) without the window: runs the
branded launcher's `--import-save FILE` against fixtures made from real saves and checks each answer.
Fixtures: GCS and MaxDrive SAV wrappers of the card saves, a 59-block card image (files 3 and 1, file 1
saved last), wrong region / game / state version, junk, a cut-short GCI; then replacing (with
.pre-import.bak, then .pre-import.2.bak), the same save under another name, a file where the card folder should be. The
launcher's user directory is WORKDIR/xdg (XDG_DATA_HOME), so the real one is never touched.
usage: tools/save-import-test.py LAUNCHER SAVES_DIR STATE.sav WORKDIR
  LAUNCHER   lib/ModernGekko/build-release-linux/SkiesOfArcadiaLegends (tools/build-release-linux.sh)
  SAVES_DIR  a folder with the game's three card saves, 8P-GEAE-SA_LEGENDS.000/.001/.002.gci
  STATE.sav  a save state made by this build's Dolphin state version (e.g. build/savestates/*.sav)
  WORKDIR    scratch folder, emptied first (refused when not empty and not made by this test)
Prints PASS / FAIL per case and FAILURES: N (0 expected)."""
import os, shutil, struct, subprocess, sys, time

launcher, saves, state, work = sys.argv[1:5]
fx = os.path.join(work, "fixtures"); xdg = os.path.join(work, "xdg")
# WORKDIR is emptied: only one this script made (its marker file) or an empty / new folder
MARK = ".save-import-test"
if os.path.isdir(work) and os.listdir(work) and not os.path.exists(os.path.join(work, MARK)):
    sys.exit(f"{work} isn't empty and wasn't made by this test; give a new folder")
shutil.rmtree(work, ignore_errors=True); os.makedirs(fx); os.makedirs(xdg)
open(os.path.join(work, MARK), "w").close()
user = os.path.join(xdg, "Skies of Arcadia Legends")
card_a = os.path.join(user, "GC", "USA", "Card A")
states = os.path.join(user, "StateSaves")
B = 0x2000

def gci(n): return open(os.path.join(saves, f"8P-GEAE-SA_LEGENDS.00{n}.gci"), "rb").read()
def put(name, data):
    p = os.path.join(fx, name); open(p, "wb").write(data); return p

g0, g1, g2 = gci(0), gci(1), gci(2)
dentry = lambda g: g[:0x40]

# GCS: "GCSAVE", DEntry at 0x110, header 0x150
gcs = bytearray(b"\0" * 0x150); gcs[:6] = b"GCSAVE"; gcs[0x110:0x150] = dentry(g1)
p_gcs = put("file2.gcs", bytes(gcs) + g1[0x40:])
# SAV (MaxDrive): "DATELGC_SAVE", DEntry at 0x80 with some u16 fields byte-swapped, header 0xC0
e = bytearray(dentry(g2))
for o in (0x06, 0x2C, 0x2E, 0x30, 0x32, 0x34, 0x36, 0x38, 0x3A, 0x3C, 0x3E):
    e[o], e[o + 1] = e[o + 1], e[o]
sav = bytearray(b"\0" * 0xC0); sav[:12] = b"DATELGC_SAVE"; sav[0x80:0xC0] = e
p_sav = put("file3.sav", bytes(sav) + g2[0x40:])

def csum(data):
    s = i = 0
    for k in range(0, len(data), 2):
        d = struct.unpack(">H", data[k:k + 2])[0]
        s = (s + d) & 0xFFFF; i = (i + (d ^ 0xFFFF)) & 0xFFFF
    return (0 if s == 0xFFFF else s), (0 if i == 0xFFFF else i)

def card(files):
    """A 59-block card (4 Mbit) holding `files` [(gci bytes, modification time)]."""
    blocks = 64
    hdr = bytearray(b"\xff" * B); hdr[0:0x26] = b"\0" * 0x26
    struct.pack_into(">H", hdr, 0x22, 4)
    struct.pack_into(">H", hdr, 0x1FA, 0)
    c, ci = csum(bytes(hdr[0:0x1FC])); struct.pack_into(">HH", hdr, 0x1FC, c, ci)
    d = bytearray(b"\xff" * B); bat = bytearray(b"\0" * B); data = []
    nxt = 5
    for idx, (g, mtime) in enumerate(files):
        n = (len(g) - 0x40) // B
        ent = bytearray(g[:0x40])
        struct.pack_into(">I", ent, 0x28, mtime)
        struct.pack_into(">HH", ent, 0x36, nxt, n)
        d[idx * 0x40:(idx + 1) * 0x40] = ent
        for k in range(n):
            blk = nxt + k
            struct.pack_into(">H", bat, 0xA + (blk - 5) * 2, 0xFFFF if k == n - 1 else blk + 1)
        data.append(g[0x40:]); nxt += n
    struct.pack_into(">h", d, 0x1FFA, 0)
    c, ci = csum(bytes(d[0:0x1FFC])); struct.pack_into(">HH", d, 0x1FFC, c, ci)
    used = nxt - 5
    struct.pack_into(">hHH", bat, 4, 0, (blocks - 5) - used, nxt - 1)
    c, ci = csum(bytes(bat[4:B])); struct.pack_into(">HH", bat, 0, c, ci)
    body = b"".join(data)
    return bytes(hdr) + bytes(d) + bytes(d) + bytes(bat) + bytes(bat) + body + b"\0" * ((blocks - 5) * B - len(body))

# File 1 saved last on the card, file 3 earlier: Continue should load file 1
p_card = put("MemoryCardA.USA.raw", card([(g2, 1000), (g0, 2000)]))
other = bytearray(g1); other[0:4] = b"GALE"; other[4:6] = b"01"
p_card_other = put("melee.raw", card([(bytes(other), 5)]))
eur = bytearray(g0); eur[0:4] = b"GEAP"
p_eur = put("eur.gci", bytes(eur))
p_other = put("melee.gci", bytes(other))
p_junk = put("junk.bin", os.urandom(5000))
p_short = put("short.gci", g0[:-100])
st = open(state, "rb").read()
p_state = put("state.sav", st)
bad_ver = bytearray(st); struct.pack_into("<I", bad_ver, 24, 0xBAADBABE + 190)
p_state_old = put("old-state.sav", bytes(bad_ver))
bad_game = bytearray(st); bad_game[0:6] = b"GALE01"
p_state_game = put("melee-state.sav", bytes(bad_game))

# Saves the game couldn't load (HANDOFF-review A5): more blocks than a card holds, not 3 blocks, not
# an SA_LEGENDS file, and a GCS whose block count Dolphin would take from a huge file size
def resized(g, blocks):
    e = bytearray(g[:0x40]); struct.pack_into(">H", e, 0x38, blocks)
    body = g[0x40:] + b"\0" * max(0, blocks * B - (len(g) - 0x40))
    return bytes(e) + body[:blocks * B]
p_huge = put("huge.gci", resized(g0, 2100))
p_four = put("four.gci", resized(g0, 4))
renamed = bytearray(g0); renamed[8:0x28] = b"OTHERNAME.000".ljust(0x20, b"\0")
p_renamed = put("renamed.gci", bytes(renamed))
p_big_gcs = os.path.join(fx, "big.gcs")
with open(p_big_gcs, "wb") as f:
    f.write(bytes(gcs)); f.truncate(512 * 1024 * 1024 + 0x150)  # sparse
p_card_four = put("four.raw", card([(resized(g0, 4), 7)]))
# States Dolphin's loader would choke on (A6): a huge unpacked size, a chunk chain cut short, a zero
# chunk length
vlen = struct.unpack_from("<I", st, 28)[0]; ext = 32 + vlen
huge_state = bytearray(st); struct.pack_into("<Q", huge_state, ext + 8, 1 << 60)
p_state_huge = put("huge-state.sav", bytes(huge_state))
p_state_cut = put("cut-state.sav", st[:-1000])
zero_chunk = bytearray(st); struct.pack_into("<i", zero_chunk, ext + 16, 0)
p_state_zero = put("zero-chunk-state.sav", bytes(zero_chunk))

env = dict(os.environ, XDG_DATA_HOME=xdg)
fails = 0
def run(label, path, ok, expect):
    global fails
    r = subprocess.run([launcher, "--import-save", path], env=env, capture_output=True, text=True)
    out = (r.stdout + r.stderr).strip()
    good = (r.returncode == 0) == ok and expect in out
    fails += not good
    print(f"{'PASS' if good else 'FAIL'}  {label}\n      " + out.replace("\n", "\n      "))
def ls():
    for d in (card_a, states):
        if os.path.isdir(d):
            for f in sorted(os.listdir(d), key=lambda f: os.path.getmtime(os.path.join(d, f))):
                print(f"      {os.path.relpath(os.path.join(d, f), user)}")

run("junk refused", p_junk, False, "isn't a save")
run("cut-short GCI refused", p_short, False, "isn't a save")
run("European save refused", p_eur, False, "European version")
run("other game refused", p_other, False, "another game (GALE01)")
run("other game's card refused", p_card_other, False, "no Skies of Arcadia Legends saves")
run("old-version state refused", p_state_old, False, "different version")
run("other game's state refused", p_state_game, False, "another game")
run("GCI bigger than a card refused", p_huge, False, "bigger than a memory card")
run("4-block GCI refused", p_four, False, "saves are 3 blocks")
run("GCI with another file name refused", p_renamed, False, "save file.")
t0 = time.time()
run("512 MB GCS refused before reading", p_big_gcs, False, "bigger than a memory card")
print(f"      took {time.time() - t0:.2f} s")
run("card image with a 4-block save refused", p_card_four, False, "On that memory card")
run("state claiming 2^60 bytes refused", p_state_huge, False, "damaged")
run("state with its chunk cut short refused", p_state_cut, False, "damaged")
run("state with a zero-length chunk refused", p_state_zero, False, "damaged")
print("      nothing written:", not os.path.exists(user) or not any(os.scandir(user)))
run("GCI file 1 into an empty card", os.path.join(saves, "8P-GEAE-SA_LEGENDS.000.gci"), True, "Imported file 1. Continue loads it.")
run("same GCI again: replaced, old kept", os.path.join(saves, "8P-GEAE-SA_LEGENDS.000.gci"), True, "kept as 8P-GEAE-SA_LEGENDS.000.gci.pre-import.bak")
run("third time: a second copy, the first kept (B14)", os.path.join(saves, "8P-GEAE-SA_LEGENDS.000.gci"), True, "kept as 8P-GEAE-SA_LEGENDS.000.gci.pre-import.2.bak")
print("      both copies there:", all(os.path.exists(os.path.join(card_a, f"8P-GEAE-SA_LEGENDS.000.gci.pre-import{n}.bak")) for n in ("", ".2")))
# the same save under another name in the folder
shutil.copy(os.path.join(saves, "8P-GEAE-SA_LEGENDS.001.gci"), os.path.join(card_a, "my copy.gci"))
run("GCS file 2 replaces a renamed copy", p_gcs, True, "my copy.gci.pre-import.bak")
print("      renamed copy gone:", not os.path.exists(os.path.join(card_a, "my copy.gci")))
run("SAV (MaxDrive) file 3", p_sav, True, "Imported file 3")
run("card image: files 3 and 1, last saved first", p_card, True, "Continue loads file 1")
run("save state", p_state, True, "Continue starts from it")
time.sleep(1.1)
run("save state again: new name", p_state, True, "state-skies-import-")
ls()
# imported GCIs are byte-identical to the originals
for n in range(3):
    got = open(os.path.join(card_a, f"8P-GEAE-SA_LEGENDS.00{n}.gci"), "rb").read()
    want = gci(n)
    if n == 0 or n == 2:  # from the card: modification time set by the fixture
        got = got[:0x28] + got[0x2C:]; want = want[:0x28] + want[0x2C:]
        got = got[:0x32] + got[0x38:]; want = want[:0x32] + want[0x38:]  # first block differs per card
    print(f"      file {n + 1} data identical: {got == want}")
# a file where the card folder should be
shutil.rmtree(user); os.makedirs(os.path.dirname(card_a)); open(card_a, "w").write("x")
run("file in the way of the folder", os.path.join(saves, "8P-GEAE-SA_LEGENDS.000.gci"), False, "in the way")
print("FAILURES:", fails)
