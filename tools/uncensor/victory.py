#!/usr/bin/env python3
"""Gilder's victory pose: the Japanese smoke instead of Willy on his shoulder.

Two files, identical on the three GameCube discs:
- beff/pcwin.mlk, the victory effects archive: entries 1800100 (the smoke puffs) and 6700526 (the
  pistol, with its Japanese motion) are whole small MLDs, converted from BEFF/PCWIN.MLK.
- bchara/ma0050.std, Gilder's battle script: the GameCube records, then the Japanese victory records
  from 979 on (converted from BCHARA/MA0050.STD with each record type's word layout).
The smoke only shows in victory variant 26 (record action word 26), one of Gilder's three.

Manifest ops (applied to the decompressed GameCube file; make_manifest.py emits them):
  {"op": "mlk_entry", "id", "tex_index"}  replace MLK entry `id` with the Dreamcast one, converted
  {"op": "std_records", "records": [[src, first, count]...], "layouts": {"type/size": "HWB.."}}
      rebuild the script from runs of GameCube ("gc") or converted Japanese ("jp") records
  {"op": "pad"}  zero-pad to the original's decompressed size (a smaller pcwin.mlk ran the battle
      loader out of memory)

usage: victory.py   (checks the converters against the US Dreamcast -> GameCube pairs)"""
import difflib, os, struct, sys
from collections import defaultdict
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
from block2gc import block_to_gc
from pvr import decode
import gvr

FILES = {'beff/pcwin.mlk': 'BEFF/PCWIN.MLK', 'bchara/ma0050.std': 'BCHARA/MA0050.STD'}
MLK_IDS = (1800100, 6700526)
TEX_INDEX = 0x10063b10   # the GCIX index every pcwin.mlk texture has
STD_TAIL = 979           # the first Japanese record taken where the files differ


# --- MLK archives: u16 0, 0xffff, count, 0xffff; then (id, offset, size, type) u32s. Dreamcast:
# little-endian, type -1. GameCube: big-endian, AKLZ-compressed, type = the padding Sega added.
def mlk_entries(d, e):
    n = struct.unpack_from(e + 'H', d, 4)[0]
    return [struct.unpack_from(e + 'IIII', d, 8 + 16 * i) for i in range(n)]

def mlk_pack(entries):
    """[(id, data, type)] -> GameCube MLK, entries packed in order"""
    n = len(entries); head = struct.pack('>HHHH', 0, 0xffff, n, 0xffff); off = 8 + 16 * n
    tab = b''; body = b''
    for i, dat, t in entries:
        tab += struct.pack('>IIII', i, off + len(body), len(dat), t); body += dat
    return head + tab + body


# --- a whole Dreamcast MLD (an MLK entry) to the GameCube form. Checked against the 37 pcwin.mlk
# entries the West didn't change: every byte before the texture pixels matches, sizes included.
POF0_CLOBBERED = b'\0OF0'   # a texture name's terminator can overwrite the P of the next POF0 tag

def u(d, p): return struct.unpack_from('<I', d, p)[0]
def sw(o, d, p, n=1):
    for i in range(n):
        q = p + 4 * i; o[q:q + 4] = d[q:q + 4][::-1]

def nmdm_to_gc(blk):
    """NMDM chunk (tag, size, body): every word 32-bit except the (type, inp_fn) u16 pair at body+8."""
    o = bytearray(blk); size = u(blk, 4)
    o[4:8] = struct.pack('>I', size)
    for q in range(8, 8 + size - size % 4, 4):
        o[q:q + 4] = blk[q:q + 4][::-1]
    q = 16; o[q:q + 4] = blk[q:q + 2][::-1] + blk[q + 2:q + 4][::-1]
    return bytes(o)

def motion_block_to_gc(blk):
    """A motion block: NMDM chunk then POF0 (kept, only its size swapped)."""
    o = bytearray(blk); p = 0
    while p + 8 <= len(blk):
        tag = blk[p:p + 4]; size = u(blk, p + 4)
        if tag == b'NMDM': o[p:p + 8 + size] = nmdm_to_gc(blk[p:p + 8 + size])
        elif tag == b'POF0': o[p + 4:p + 8] = struct.pack('>I', size)
        else: break
        p += 8 + size; p = (p + 3) & ~3
    return bytes(o)

def rgb565(img):
    h = len(img); w = len(img[0]); out = bytearray()
    for ty in range(0, max(h, 4), 4):
        for tx in range(0, max(w, 4), 4):
            for y in range(4):
                for x in range(4):
                    r, g, b, a = img[min(ty + y, h - 1)][min(tx + x, w - 1)]
                    out += struct.pack('>H', (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3))
    return bytes(out)

def mld_to_gc(dc, tex_index=TEX_INDEX):
    d = dc; o = bytearray(d)
    cnt, idx, fp, first, tt = struct.unpack_from('<5I', d, 0)
    sw(o, d, 0, 5)
    done = set()
    for i in range(cnt):
        e = idx + 0x68 * i
        # The name is 16 bytes; Sega's converter swapped the words after it too, name overflow included.
        sw(o, d, e, 9); sw(o, d, e + 0x34, 13)
        for f in range(0x08, 0x24, 4):
            p = u(d, e + f)
            if not p or p in done: continue
            done.add(p)
            if f == 0x20:
                # NJS_TEXLIST (textures, count) -> NJS_TEXNAME (filename, attr, texaddr) each
                sw(o, d, p, 2); t, nt = u(d, p), u(d, p + 4)
                if t and t not in done:
                    done.add(t); sw(o, d, t, 3 * nt)
                continue
            n = u(d, p); sw(o, d, p, 1 + n)
            for k in range(n):
                b = u(d, p + 4 + 4 * k)
                if not b or b in done: continue
                done.add(b)
                if f == 0x14:
                    end = b + 16
                    while end + 8 <= len(d) and d[end:end + 4] in (b'NJTL', b'NJCM', b'POF0', POF0_CLOBBERED):
                        end += 8 + u(d, end + 4); end = (end + 3) & ~3
                    o[b:end] = block_to_gc(d[b:end])
                    # block_to_gc steps over the clobbered tag; its size is still swapped
                    q = b + 16
                    while q < end:
                        if d[q:q + 4] == POF0_CLOBBERED: o[q + 4:q + 8] = d[q + 4:q + 8][::-1]
                        q += 8 + u(d, q + 4); q = (q + 3) & ~3
                elif f == 0x1c:
                    end = b
                    while end + 8 <= len(d) and d[end:end + 4] in (b'NMDM', b'POF0'):
                        end += 8 + u(d, end + 4); end = (end + 3) & ~3
                    o[b:end] = motion_block_to_gc(d[b:end])
    if not tt: return bytes(o)
    n = u(d, tt)
    if n == 0:
        out = bytearray(o[:tt + 4]); out[tt:tt + 4] = bytes(4)
        while len(out) % 8: out += b'\0'
        return bytes(out)
    # texture archive: count, 0x2c records (name, 3 words, the last the size), then the blobs
    head = bytearray(o[:tt]); recs = []; pos = tt + 4 + 0x2c * n
    if d[pos:pos + 4] not in (b'GBIX', b'PVRT'): raise ValueError('aligned texture table: not seen in effect files')
    data = pos; blobs = []
    for i in range(n):
        r = tt + 4 + 0x2c * i; size = u(d, r + 0x28); blob = d[data:data + size]; data += size
        p = blob.find(b'PVRT'); img = decode(blob[p:])[0]
        pf, df, w, h = blob[p + 8], blob[p + 9], *struct.unpack_from('<HH', blob, p + 12)
        # As Sega's converter: ARGB4444 -> RGB5A3 (alpha), a plain (non-VQ) RGB565 stays RGB565,
        # the rest (VQ, 1555) -> CMPR
        fmt = 0x05 if pf == 2 else 0x04 if pf == 1 and df in (1, 2) else 0x0E
        lvl = img; px = bytearray()
        mips = df in (2, 4, 0x11, 0x12)
        while lvl is not None:
            px += gvr.tiles(lvl) if fmt == 5 else rgb565(lvl) if fmt == 4 else gvr.cmpr(lvl)
            if not mips: break
            lvl = gvr.half(lvl)
        gvrt = bytes([0, 0, 0x01 if mips else 0, fmt]) + struct.pack('>HH', w, h) + bytes(px)
        g = b'GCIX' + struct.pack('<I', 8) + struct.pack('>II', tex_index, 0) + b'GVRT' + struct.pack('<I', len(gvrt)) + gvrt
        while len(g) % 32: g += b'\0'
        rec = bytearray(d[r:r + 0x2c]); sw(rec, d[r:r + 0x2c], 0x20, 3); struct.pack_into('>I', rec, 0x28, len(g))
        recs.append(bytes(rec)); blobs.append(g)
    # blobs follow the table with no gap (as on the Dreamcast), each a multiple of 32 bytes; the
    # entry's end is padded to 32 (the MLK's type field records that padding)
    out = head + struct.pack('>I', n) + b''.join(recs) + b''.join(blobs)
    while len(out) % 32: out += b'\0'
    return bytes(out)


# --- battle scripts (bchara/*.std): u16 count (incl. end), u16 4, 0, 0, u32 total; then count
# records of (u16 type, u16 3, 0, size, offset from 0x10); type 0xffff ends. Record data 8-aligned.
def std_records(d, e):
    n = struct.unpack_from(e + 'H', d, 0)[0]
    out = []
    for i in range(n):
        t, _, _, sz, off = struct.unpack_from(e + 'HHIII', d, 16 + 16 * i)
        if t != 0xffff: out.append((t, d[16 + off:16 + off + sz]))
    return out

def word_kind(a, b):
    if a == b: return '='
    if a[::-1] == b: return 'W'
    if a[1::-1] + a[3:1:-1] == b: return 'H'
    return '?'

def layout_table(us, gc):
    """(type, size) -> per-word layout: H (16-bit pair), W (32-bit), B (bytes kept), ? (byte-level
    fields, not convertible), learned from the US Dreamcast and GameCube records"""
    L = defaultdict(list)
    for (t, a), (_, b) in zip(std_records(us, '<'), std_records(gc, '>')):
        L[(t, len(a))].append([(word_kind(a[i:i + 4], b[i:i + 4]), a[i:i + 4]) for i in range(0, len(a), 4)])
    def kept(a): return a != a[::-1] and a != a[1::-1] + a[3:1:-1]
    out = {}
    for k, lst in L.items():
        lay = ''
        for w in range(k[1] // 4):
            ks = set(r[w][0] for r in lst) - {'='}
            # unchanged and not symmetric: bytes the converter kept
            if any(r[w][0] == '=' and kept(r[w][1]) for r in lst): ks.add('B')
            # 'W' where both halves are equal reads the same as 'H'; '=' (always 0 or symmetric in
            # the US file) defaults to 32-bit (the Japanese floats there)
            lay += '?' if '?' in ks or ('B' in ks and ks - {'B'}) else 'B' if 'B' in ks else 'H' if 'H' in ks else 'W'
        out[k] = lay
    return out

def convert_record(r, lay):
    o = bytearray(r)
    for w, k in enumerate(lay):
        q = 4 * w
        if k == 'H': o[q:q + 4] = r[q:q + 2][::-1] + r[q + 2:q + 4][::-1]
        elif k == 'W': o[q:q + 4] = r[q:q + 4][::-1]
        elif k != 'B': raise ValueError('byte-level field in a record to convert')
    return bytes(o)

def std_plan(jp, us, gc, tail=STD_TAIL):
    """The std_records op: the output record list as runs of GameCube or Japanese records. Records
    the US Dreamcast file shares with the Japanese one keep the GameCube's own bytes, and so does
    everything before `tail` (US changes outside the victory sequence stay)."""
    J = std_records(jp, '<'); U = std_records(us, '<'); lay = layout_table(us, gc)
    src = [None] * len(J)
    for op, a1, a2, b1, b2 in difflib.SequenceMatcher(None, U, J, autojunk=False).get_opcodes():
        for k in range(b1, b2):
            same = op == 'replace' and b2 - b1 == a2 - a1
            if op == 'equal': src[k] = ('gc', a1 + k - b1); continue
            t, r = J[k]
            if k < tail:
                ut, ur = U[a1 + k - b1]
                if not same or ut != t or len(ur) != len(r): raise ValueError('structural change before the tail')
                src[k] = ('gc', a1 + k - b1); continue
            # Same record but for flag bits (the US build set bit 29 of word 1): keep the GameCube one
            if same:
                ut, ur = U[a1 + k - b1]
                if ut == t and len(ur) == len(r) and ur[:4] == r[:4] and ur[8:] == r[8:]:
                    src[k] = ('gc', a1 + k - b1); continue
            src[k] = ('jp', k)
    runs = []
    for s, i in src:
        if runs and runs[-1][0] == s and runs[-1][1] + runs[-1][2] == i: runs[-1][2] += 1
        else: runs.append([s, i, 1])
    used = sorted({(J[i][0], len(J[i][1])) for s, f, n in runs if s == 'jp' for i in range(f, f + n)})
    return {'op': 'std_records', 'records': runs, 'layouts': {f'{t}/{sz}': lay[(t, sz)] for t, sz in used}}


# --- applying the ops (the reference for DiscIO/SkiesUncensorConvert.cpp)
def apply(ops, gc, jp):
    """gc: the decompressed GameCube file, jp: the Dreamcast file"""
    out = gc
    for op in ops:
        if op['op'] == 'mlk_entry':
            J = {i: jp[o:o + s] for i, o, s, t in mlk_entries(jp, '<')}
            out = mlk_pack([(i, mld_to_gc(J[i], op['tex_index']), 0) if i == op['id'] else (i, out[o:o + s], t)
                            for i, o, s, t in mlk_entries(out, '>')])
        elif op['op'] == 'std_records':
            G = std_records(out, '>'); J = std_records(jp, '<'); recs = []
            for s, f, n in op['records']:
                for i in range(f, f + n):
                    if s == 'gc': recs.append(G[i])
                    else: t, r = J[i]; recs.append((t, convert_record(r, op['layouts'][f'{t}/{len(r)}'])))
            # table: (type, 3) as u16s, 0, size, offset from 0x10; header: (count, 4), 0, 0, total
            n = len(recs) + 1; table = bytearray(); body = bytearray(); off = 16 * n
            for t, r in recs:
                table += struct.pack('>HHIII', t, 3, 0, len(r), off + len(body)); body += r
                while len(body) % 8: body += b'\0'
            table += struct.pack('>HHIII', 0xffff, 0xffff, 0, 0, off + len(body))
            out = struct.pack('>HHIII', n, 4, 0, 0, 16 * n + len(body)) + bytes(table) + bytes(body)
        elif op['op'] == 'pad':
            out = out + bytes(max(0, len(gc) - len(out)))
        else:
            raise ValueError('not a victory op: ' + op['op'])
    return out

def ops_for(gcrel, jp, us, gc):
    if gcrel == 'beff/pcwin.mlk':
        return [{'op': 'mlk_entry', 'id': i, 'tex_index': TEX_INDEX} for i in MLK_IDS] + [{'op': 'pad'}]
    return [std_plan(jp, us, gc), {'op': 'pad'}]


if __name__ == '__main__':
    from inventory import ROOT
    from aklz import decompress
    rd = lambda p: open(p, 'rb').read()
    # the MLD converter against the pcwin.mlk entries the West didn't change
    us = rd(f'{ROOT}/dc/files1/BEFF/PCWIN.MLK'); gc = decompress(rd(f'{ROOT}/disc/files/beff/pcwin.mlk'))
    bad = 0
    for (i, o, s, t), (_, o2, s2, _) in zip(mlk_entries(us, '<'), mlk_entries(gc, '>')):
        a = us[o:o + s]; b = gc[o2:o2 + s2]; c = mld_to_gc(a)
        tt = u(a, 0x10); n = u(a, tt) if tt else 0
        heads = lambda x: [x[k:k + 0x20] for k in range(len(x)) if x[k:k + 4] == b'GCIX']
        if c[:tt + 4 + 0x2c * n] != b[:tt + 4 + 0x2c * n] or len(c) != len(b) or heads(b) != heads(c):
            bad += 1; print('pcwin entry', i, 'differs')
    # the record layouts against the US script
    us = rd(f'{ROOT}/dc/files1/BCHARA/MA0050.STD'); gc = decompress(rd(f'{ROOT}/disc/files/bchara/ma0050.std'))
    lay = layout_table(us, gc); ok = skip = 0
    for (t, r), (_, g) in zip(std_records(us, '<'), std_records(gc, '>')):
        try: c = convert_record(r, lay[(t, len(r))])
        except ValueError: skip += 1; continue
        if c == g: ok += 1
        else: bad += 1; print('ma0050 record differs')
    print(f'pcwin.mlk entries checked; ma0050 records: {ok} exact, {skip} byte-level (not convertible)')
    print('ALL OK' if not bad else f'{bad} bad')
