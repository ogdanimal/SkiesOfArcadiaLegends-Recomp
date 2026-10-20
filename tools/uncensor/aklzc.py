"""AKLZ compressor matching tools/aklz.py's decompressor (LZSS, 4 KiB ring from 0xFEE).

compress(data, size=None): with size, the output is exactly that many bytes (or ValueError when the
data doesn't compress that small). The game's decoder (fn_801C6F1C) stops when it runs out of input,
not output, and a save state keeps the disc's file table in RAM, so a replacement file has to keep
the original's byte count for states made before it to read it at the right place."""
import struct

HEADER = b'AKLZ' + bytes.fromhex('7E3F51643DCCCCCD')


def tokens(data, depth=64):
    """Greedy LZSS parse with one-step lazy matching: ints are literals, (dist, length) pairs are refs."""
    n = len(data); heads = {}; out = []

    def best_at(i):
        best = 0; bestp = 0
        if i + 3 <= n:
            cand = heads.get(data[i:i + 3])
            if cand:
                for p in reversed(cand[-depth:]):
                    if i - p > 4095: break
                    l = 3; m = min(18, n - i)
                    while l < m and data[p + l] == data[i + l]: l += 1
                    if l > best: best, bestp = l, p
                    if l == 18: break
        return best, bestp

    def add(i):
        if i + 3 <= n: heads.setdefault(data[i:i + 3], []).append(i)

    i = 0
    while i < n:
        l, p = best_at(i)
        if l >= 3 and l < 18:
            add(i)
            l2, _ = best_at(i + 1)
            if l2 > l + 1:
                out.append(data[i]); i += 1
                continue
            heads[data[i:i + 3]].pop()
        if l >= 3:
            out.append((i - p, l))
            for k in range(l): add(i + k)
            i += l
        else:
            out.append(data[i]); add(i); i += 1
    return out


def tokens_optimal(data, depth=64):
    """Cheapest parse (literal 9 bits, reference 17) by dynamic programming over each position's
    longest match; any shorter length at the same distance is valid too."""
    n = len(data); heads = {}; mlen = [0] * n; mdist = [0] * n
    for i in range(n):
        if i + 3 <= n:
            key = data[i:i + 3]; cand = heads.get(key); best = 0; bestp = 0
            if cand:
                for p in reversed(cand[-depth:]):
                    if i - p > 4095: break
                    l = 3; m = min(18, n - i)
                    while l < m and data[p + l] == data[i + l]: l += 1
                    if l > best: best, bestp = l, p
                    if l == 18: break
            if best >= 3: mlen[i] = best; mdist[i] = i - bestp
            if cand is None: heads[key] = [i]
            else:
                cand.append(i)
                if len(cand) > 4 * depth: del cand[:len(cand) - depth]
    cost = [0] * (n + 1); choice = [0] * n
    for i in range(n - 1, -1, -1):
        c = 9 + cost[i + 1]; ch = 0
        for l in range(3, mlen[i] + 1):
            v = 17 + cost[i + l]
            if v < c: c, ch = v, l
        cost[i] = c; choice[i] = ch
    out = []; i = 0
    while i < n:
        l = choice[i]
        if l: out.append((mdist[i], l)); i += l
        else: out.append(data[i]); i += 1
    return out


def encoded_size(toks):
    return len(HEADER) + 4 + sum(1 if isinstance(t, int) else 2 for t in toks) + (len(toks) + 7) // 8


def fit(toks, data, target):
    """Grow the stream to exactly target bytes. Two edits keep the output the same:
    A: ref(d, L>=4) at i -> literal data[i] + ref(d, L-1)    (+1 byte, +1 token)
    C: ref(d, L>=6) -> ref(d, 3) + ref(d, L-3)               (+2 bytes, +1 token)"""
    size = encoded_size(toks)
    deficit = target - size
    if deficit < 0:
        raise ValueError(f'compresses to {size} bytes, {-deficit} more than the {target} allowed')
    if deficit == 0:
        return toks
    T = len(toks); fl = lambda t: (t + 7) // 8
    for nops in range(deficit + 1):
        m = deficit - nops - (fl(T + nops) - fl(T))  # C edits; the rest (nops - m) are A edits
        if 0 <= m <= nops:
            break
    else:
        raise ValueError('no exact fit')
    a = nops - m
    out = []; pos = 0
    for t in toks:
        if isinstance(t, tuple):
            d, l = t
            while m and l >= 6:
                out.append((d, 3)); pos += 3; l -= 3; m -= 1
            while a and l >= 4:
                out.append(data[pos]); pos += 1; l -= 1; a -= 1
            out.append((d, l)); pos += l
        else:
            out.append(t); pos += 1
    if a or m:
        raise ValueError('not enough references to grow the stream')
    assert encoded_size(out) == target
    return out


def encode(toks, n):
    out = bytearray(HEADER + struct.pack('>I', n)); pos = 0
    for g in range(0, len(toks), 8):
        flagpos = len(out); out.append(0); flags = 0
        for bit, t in enumerate(toks[g:g + 8]):
            if isinstance(t, int):
                flags |= 1 << bit; out.append(t); pos += 1
            else:
                d, l = t
                off = (pos - d + 0xFEE) & 0xFFF
                out += bytes([off & 0xFF, ((off >> 4) & 0xF0) | (l - 3)]); pos += l
        out[flagpos] = flags
    return bytes(out)


def compress(data, size=None, depth=64):
    toks = tokens(data, depth)
    if size is not None and encoded_size(toks) > size:
        toks = tokens_optimal(data, depth)
    if size is not None and encoded_size(toks) > size:
        toks = tokens_optimal(data, 4096)  # every candidate in the window: optimal for this format
    if size is not None:
        toks = fit(toks, data, size)
    return encode(toks, len(data))
