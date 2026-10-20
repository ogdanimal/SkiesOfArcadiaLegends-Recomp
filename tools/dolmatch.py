#!/usr/bin/env python3
"""Match the USA (GEAE8P) main.dol to the European one (GEAP8P) or the Japanese one (GEAJ8P, with
--target GEAJ8P; "EU" in the code and the report means the other DOL): functions, data, and every
address the game patches and the frontend patches use.  Stdlib only; run with `python3 -I tools/dolmatch.py`.

usage: dolmatch.py [--target GEAP8P|GEAJ8P] [--usa DOL] [--eu DOL] [--symbols symbols.txt] [--out-dir build/eu]
The defaults read the DOLs and symbols from the research and decomp checkouts beside this repo
(RESEARCH / DECOMP in the environment point elsewhere).

Writes (out-dir, default build/eu):
  dolmap.json         {"functions": {usa: {eu, size_usa, size_eu, method, name}}, "data": {...}, ...}
  address-map.json    {"0x8xxxxxxx": "0x8yyyyyyy"} for every address the patches use
  address-map.md      the same as a table with meaning, where used, method and notes
  <ID>.addresses.json the addresses the sources use (not those only in a region's table), confident
                      matches only: patches/game/<ID>.addresses.json for gamepatch.py

How it works
  1. Every instruction is masked (bl/b targets, lis/addi/ori/load/store immediates, SDA offsets) and a
     function is looked up by its masked words: a unique window of the EU text is an "exact" match.
  2. Call-graph propagation: matched A/A' are aligned instruction by instruction (difflib); a bl at an
     aligned position pairs the two callees.  Repeated with 3.
  3. Order: the unmatched functions between two matched ones are paired with the EU candidates
     between the corresponding EU functions by instruction-multiset similarity (monotonic DP).
  4. Data: at aligned positions, lis/addi, lis/load, r13/r2 SDA references give USA data address ->
     EU data address votes.  Other addresses go by label-relative offset, a bracket of two consistent
     neighbours, or the label's bytes (pointers wildcarded)."""
import argparse
import collections
import difflib
import json
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# The research and decomp checkouts: beside this repo by default, or RESEARCH / DECOMP in the environment
RESEARCH = os.environ.get("RESEARCH", os.path.join(os.path.dirname(ROOT), "GameCube-Research", "targets",
                                                   "SkiesOfArcadiaLegends"))
DECOMP = os.environ.get("DECOMP", os.path.join(os.path.dirname(ROOT), "SkiesOfArcadiaLegends-Decomp"))
USA_SHA1 = "8c0e278126fa3b0173400fdb632038172743cc13"


# ----------------------------------------------------------------------------- DOL
class Dol:
    def __init__(self, path):
        self.data = open(path, "rb").read()
        w = struct.unpack(">64I", self.data[:0x100])
        self.text = [(w[i], w[18 + i], w[36 + i]) for i in range(7) if w[36 + i]]
        self.dat = [(w[7 + i], w[25 + i], w[43 + i]) for i in range(11) if w[43 + i]]
        self.bss = (w[54], w[55])
        self.entry = w[56]
        self.words = {}          # text address -> word, as one flat array per section
        self.tbase = []          # (base, array('I'-like list))
        for off, base, size in self.text:
            self.tbase.append((base, list(struct.unpack(">%dI" % (size // 4), self.data[off:off + size]))))
        self.text_lo = min(b for b, _ in self.tbase)
        self.text_hi = max(b + 4 * len(a) for b, a in self.tbase)
        # sda bases from __init_registers (0x80003278)
        self.r2 = self.r13 = None
        ws = [self.word(0x80003278 + 4 * i) for i in range(6)]
        hi = lambda w: ((w & 0xFFFF) << 16)
        self.r2 = (hi(ws[2]) | (ws[3] & 0xFFFF)) & 0xFFFFFFFF
        self.r13 = (hi(ws[4]) | (ws[5] & 0xFFFF)) & 0xFFFFFFFF
        self.r1 = (hi(ws[0]) | (ws[1] & 0xFFFF)) & 0xFFFFFFFF

    def section_of(self, addr):
        for off, base, size in self.text:
            if base <= addr < base + size:
                return ("text", off + addr - base)
        for off, base, size in self.dat:
            if base <= addr < base + size:
                return ("data", off + addr - base)
        if self.bss[0] <= addr < self.bss[0] + self.bss[1]:
            return ("bss", None)
        return (None, None)

    def word(self, addr):
        k, o = self.section_of(addr)
        if k in ("text", "data"):
            return struct.unpack(">I", self.data[o:o + 4])[0]
        return None

    def read(self, addr, n):
        k, o = self.section_of(addr)
        if k in ("text", "data"):
            return self.data[o:o + n]
        return None

    def text_array(self, addr, count):
        for base, arr in self.tbase:
            if base <= addr < base + 4 * len(arr):
                i = (addr - base) // 4
                return arr[i:i + count]
        return []

    def in_text(self, a):
        return any(b <= a < b + 4 * len(arr) for b, arr in self.tbase)


# ----------------------------------------------------------------------------- instructions
def sx16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def op_of(w):
    return w >> 26


def is_bl(w):
    return (w >> 26) == 18 and (w & 3) == 1


def is_b(w):
    return (w >> 26) == 18 and (w & 3) == 0


def b_target(w, pc):
    li = w & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    return (pc + li) & 0xFFFFFFFF if not (w & 2) else li & 0xFFFFFFFF


LOADSTORE = set(range(32, 56))
IMM_OPS = {14, 15, 24, 25} | LOADSTORE


def mask(w):
    op = w >> 26
    if op == 18:
        return w & 0xFC000003
    if op in IMM_OPS:
        return w & 0xFFFF0000
    return w


STORE_OPS = {36, 37, 38, 39, 44, 45, 52, 53, 54, 55}


def dis(w, pc=None):
    op = w >> 26
    rd, ra, rb = (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31
    names = {32: "lwz", 33: "lwzu", 34: "lbz", 35: "lbzu", 36: "stw", 37: "stwu", 38: "stb", 39: "stbu",
             40: "lhz", 41: "lhzu", 42: "lha", 43: "lhau", 44: "sth", 45: "sthu", 46: "lmw", 47: "stmw",
             48: "lfs", 49: "lfsu", 50: "lfd", 51: "lfdu", 52: "stfs", 53: "stfsu", 54: "stfd", 55: "stfdu"}
    if op in names:
        return "%s r%d, %d(r%d)" % (names[op], rd, sx16(w), ra)
    if op == 14:
        return "addi r%d, r%d, %d" % (rd, ra, sx16(w)) if ra else "li r%d, %d" % (rd, sx16(w))
    if op == 15:
        return "lis r%d, 0x%X" % (rd, w & 0xFFFF) if ra == 0 else "addis r%d, r%d, 0x%X" % (rd, ra, w & 0xFFFF)
    if op == 24:
        return "ori r%d, r%d, 0x%X" % (ra, rd, w & 0xFFFF)
    if op == 18:
        if pc is not None and not (w & 2):
            return "%s 0x%08X" % ("bl" if w & 1 else "b", b_target(w, pc))
        return "%s" % ("bl" if w & 1 else "b")
    return ".long 0x%08X" % w


def written_regs(w):
    """Registers whose value a (non-store, non-compare) instruction changes, approximately."""
    op = w >> 26
    rd, ra = (w >> 21) & 31, (w >> 16) & 31
    if op in STORE_OPS or op in (10, 11, 16, 18, 19, 47):
        return ()
    if op in (24, 25, 26, 27, 28, 29):
        return (ra,)
    if op == 31:
        return (rd, ra)
    if op in (33, 35, 37, 39, 41, 43, 45, 49, 51, 53, 55):
        return (rd, ra)
    return (rd,)


# ----------------------------------------------------------------------------- symbols
SYM_RE = re.compile(r"^(\S+)\s*=\s*(\.\w+):(0x[0-9A-Fa-f]+);\s*//\s*type:(\w+)(?:\s+size:(0x[0-9A-Fa-f]+))?")


def load_symbols(path):
    funcs, data, byname = [], [], {}
    for line in open(path):
        m = SYM_RE.match(line.strip())
        if not m:
            continue
        name, sec, addr, typ, size = m.groups()
        addr = int(addr, 16)
        size = int(size, 16) if size else 0
        byname[name] = addr
        if typ == "function" and size:
            funcs.append((addr, size, name))
        elif typ in ("object", "label") or typ != "function":
            data.append((addr, size, name, sec))
    funcs.sort()
    data.sort()
    return funcs, data, byname


# ----------------------------------------------------------------------------- matcher
class Matcher:
    def __init__(self, usa, eu, funcs, log=print):
        self.u, self.e, self.funcs = usa, eu, funcs
        self.log = log
        self.fn_by_addr = {a: (s, n) for a, s, n in funcs}
        self.match = {}            # usa addr -> dict(eu, method, size_eu)
        self.used_eu = {}          # eu addr -> usa addr
        self.sig = {a: [mask(w) for w in usa.text_array(a, s // 4)] for a, s, n in funcs}
        self.aligncache = {}

    # -- helpers
    def eu_window(self, ea, n):
        return self.e.text_array(ea, n)

    def add(self, ua, ea, method, force=False):
        if ua in self.match:
            return self.match[ua]["eu"] == ea
        if ea in self.used_eu and not force:
            return False
        self.match[ua] = {"eu": ea, "method": method}
        self.used_eu[ea] = ua
        return True

    def align(self, ua, ea):
        """Instruction alignment of USA fn ua against the EU code at ea: list of (i, j) index pairs
        in equal blocks (identical masked words); also the EU length covered."""
        key = (ua, ea)
        if key in self.aligncache:
            return self.aligncache[key]
        us = self.sig[ua]
        n = len(us)
        ew = self.eu_window(ea, n + n // 2 + 32)
        # do not run past the next matched EU function start
        em = [mask(w) for w in ew]
        if em[:n] == us:
            res = ([(i, i) for i in range(n)], n)
        else:
            sm = difflib.SequenceMatcher(None, us, em, autojunk=False)
            pairs = []
            for a, b, sz in sm.get_matching_blocks():
                for k in range(sz):
                    pairs.append((a + k, b + k))
            res = (pairs, 0)
            if pairs and pairs[-1][0] == n - 1:
                res = (pairs, pairs[-1][1] + 1)
            elif pairs:
                res = (pairs, pairs[-1][1] + 1 + (n - 1 - pairs[-1][0]))
        self.aligncache[key] = res
        return res

    # -- phase 1
    def exact(self):
        K = 6
        eidx = collections.defaultdict(list)
        for base, arr in self.e.tbase:
            m = [mask(w) for w in arr]
            for i in range(len(m) - K + 1):
                eidx[tuple(m[i:i + K])].append((base + 4 * i, base, i))
            self._emask = getattr(self, "_emask", {})
            self._emask[base] = m
        cnt = collections.Counter(tuple(s) for s in self.sig.values())
        n = 0
        for a, s, name in self.funcs:
            sg = self.sig[a]
            if len(sg) < K or cnt[tuple(sg)] != 1:
                continue
            cands = []
            for ea, base, i in eidx.get(tuple(sg[:K]), []):
                if self._emask[base][i:i + len(sg)] == sg:
                    cands.append(ea)
            if len(cands) == 1 and self.add(a, cands[0], "exact"):
                n += 1
        self.eidx = eidx
        self.log("exact unique: %d" % n)

    # -- order sanity: drop matches that break monotonicity (longest increasing subsequence)
    def prune_order(self):
        items = sorted(self.match.items())
        eus = [v["eu"] for _, v in items]
        # LIS (strict) with parents
        import bisect
        tails, idx, par = [], [], [-1] * len(eus)
        for i, e in enumerate(eus):
            p = bisect.bisect_left(tails, e)
            if p == len(tails):
                tails.append(e)
                idx.append(i)
            else:
                tails[p] = e
                idx[p] = i
            par[i] = idx[p - 1] if p else -1
        keep = set()
        k = idx[-1] if idx else -1
        while k >= 0:
            keep.add(k)
            k = par[k]
        dropped = 0
        self.anchors = set(items[i][0] for i in keep)
        for i, (a, v) in enumerate(items):
            if i not in keep and v["method"].startswith("order"):
                del self.match[a]
                self.used_eu.pop(v["eu"], None)
                dropped += 1
        self.log("order prune dropped %d" % dropped)

    # -- phase 2: propagation along calls
    def propagate(self):
        total = 0
        while True:
            added = 0
            for ua in sorted(self.match):
                if ua not in self.sig:
                    continue
                ea = self.match[ua]["eu"]
                pairs, _ = self.align(ua, ea)
                for i, j in pairs:
                    uw = self.u.word(ua + 4 * i)
                    ew = self.e.word(ea + 4 * j)
                    if is_bl(uw) and is_bl(ew):
                        ut, et = b_target(uw, ua + 4 * i), b_target(ew, ea + 4 * j)
                        if ut in self.fn_by_addr and self.e.in_text(et):
                            if ut not in self.match and self.add(ut, et, "call"):
                                added += 1
            total += added
            if not added:
                break
        self.log("call propagation: +%d" % total)
        return total

    # -- phase 3: fill gaps in order
    def eu_candidates(self):
        if hasattr(self, "_cands"):
            return self._cands
        c = set()
        for base, arr in self.e.tbase:
            n = len(arr)
            for i, w in enumerate(arr):
                pc = base + 4 * i
                if is_bl(w):
                    t = b_target(w, pc)
                    if self.e.in_text(t):
                        c.add(t)
                if w == 0x4E800020 and i + 1 < n:
                    j = i + 1
                    while j < n and arr[j] == 0:
                        j += 1
                    if j < n:
                        c.add(base + 4 * j)
                # prologue pattern: mflr r0 ; stw r0, 4(r1)
                if w == 0x7C0802A6 and i + 1 < n and arr[i + 1] == 0x90010004:
                    c.add(pc)
        c |= set(self.used_eu)
        self._cands = sorted(c)
        return self._cands

    def gaps(self):
        import bisect
        cands = self.eu_candidates()
        multis = {}

        def ms_u(a):
            if a not in multis:
                multis[a] = collections.Counter(self.sig[a])
            return multis[a]

        def ms_e(ea, n):
            k = ("e", ea, n)
            if k not in multis:
                multis[k] = collections.Counter(mask(w) for w in self.eu_window(ea, n))
            return multis[k]

        def sim(ua, ea):
            n = len(self.sig[ua])
            cu, ce = ms_u(ua), ms_e(ea, n)
            common = sum((cu & ce).values())
            return 2.0 * common / (n + max(1, sum(ce.values())))

        anchors = sorted((a, v) for a, v in self.match.items() if a in self.anchors)
        ualist = [a for a, s, nm in self.funcs]
        total = 0
        # walk consecutive anchors
        bounds = [(0, 0)] + [(a, v["eu"]) for a, v in anchors] + [(0xFFFFFFFF, 0xFFFFFFFF)]
        for (ua0, ea0), (ua1, ea1) in zip(bounds, bounds[1:]):
            us = [a for a in ualist[bisect.bisect_right(ualist, ua0):bisect.bisect_left(ualist, ua1)]
                  if a not in self.match]
            if not us:
                continue
            lo = ea0 + (4 * len(self.sig[ua0]) if ua0 in self.sig else 0)
            es = [c for c in cands if lo <= c < ea1 and c not in self.used_eu and c > ea0]
            if not es:
                continue
            if len(us) * len(es) > 40000:
                continue
            # monotonic DP maximising similarity
            S = [[sim(u, e) if abs(len(self.sig[u]) - 1) >= 0 else 0 for e in es] for u in us]
            nU, nE = len(us), len(es)
            best = [[0.0] * (nE + 1) for _ in range(nU + 1)]
            ch = [[0] * (nE + 1) for _ in range(nU + 1)]
            for i in range(1, nU + 1):
                for j in range(1, nE + 1):
                    v, c = best[i - 1][j], 1
                    if best[i][j - 1] > v:
                        v, c = best[i][j - 1], 2
                    s = S[i - 1][j - 1]
                    if s >= 0.75 and best[i - 1][j - 1] + s > v:
                        v, c = best[i - 1][j - 1] + s, 3
                    best[i][j], ch[i][j] = v, c
            i, j = nU, nE
            while i > 0 and j > 0:
                c = ch[i][j]
                if c == 3:
                    # tiny functions are only trusted when the neighbouring order agrees
                    if self.add(us[i - 1], es[j - 1], "order(%.2f)" % S[i - 1][j - 1]):
                        total += 1
                    i -= 1
                    j -= 1
                elif c == 1:
                    i -= 1
                else:
                    j -= 1
        self.log("gap fill: +%d" % total)
        return total

    # -- phase 4: pointer tables in data (state tables, vtables): the same slot holds the same function
    def tables(self, cons, symdata):
        added = 0
        for a, size, name, sec in symdata:
            if sec not in (".data", ".rodata", ".sdata", ".sdata2", ".ctors", ".dtors") or size < 8 or size % 4:
                continue
            words = [self.u.word(a + 4 * i) for i in range(size // 4)]
            ptrs = [(i, w) for i, w in enumerate(words) if w in self.fn_by_addr]
            if len(ptrs) < 2 or not any(w not in self.match for _, w in ptrs):
                continue
            eb, method, conf, _ = map_data_addr(self, cons, symdata, a)
            if eb is None or conf not in ("high", "medium"):
                continue
            agree = disagree = 0
            for i, w in ptrs:
                if w in self.match:
                    if self.match[w]["eu"] == self.e.word(eb + 4 * i):
                        agree += 1
                    else:
                        disagree += 1
            if agree < 2 or disagree * 4 > agree:
                continue
            for i, w in ptrs:
                ev = self.e.word(eb + 4 * i)
                if w not in self.match and ev is not None and self.e.in_text(ev) and self.add(w, ev, "table"):
                    added += 1
        self.log("pointer tables: +%d" % added)
        return added

    def run(self):
        self.exact()
        self.prune_order()
        for rnd in range(6):
            a = self.propagate()
            b = self.gaps()
            self.prune_order()
            if not (a or b):
                break
        # EU sizes
        for ua, v in self.match.items():
            if ua in self.sig:
                v["size_eu"] = self.align(ua, v["eu"])[1] * 4

    # -- address inside a function: aligned instruction -> EU address
    def map_in_function(self, addr):
        # find containing USA function
        import bisect
        starts = [a for a, s, n in self.funcs]
        k = bisect.bisect_right(starts, addr) - 1
        if k < 0:
            return None
        fa, fs, fn = self.funcs[k]
        if not (fa <= addr < fa + fs) or fa not in self.match:
            return None
        ea = self.match[fa]["eu"]
        i = (addr - fa) // 4
        pairs, _ = self.align(fa, ea)
        d = dict(pairs)
        if i in d:
            return fa, ea + 4 * d[i], "aligned"
        return fa, None, "unaligned"


# ----------------------------------------------------------------------------- data references
def data_refs(dol, fa, nwords, base_words=None):
    """Yield (index, kind, address) for every resolvable address reference in a function."""
    val = {}
    out = []
    for i in range(nwords):
        w = dol.word(fa + 4 * i)
        op = w >> 26
        rd, ra = (w >> 21) & 31, (w >> 16) & 31
        if op == 15 and ra == 0:
            val[rd] = (w & 0xFFFF) << 16
            continue
        sda = None
        if op in IMM_OPS or op == 14:
            if ra == 13:
                sda = dol.r13 + sx16(w)
            elif ra == 2:
                sda = dol.r2 + sx16(w)
            elif ra in val and op in (14, 24) + tuple(LOADSTORE):
                pass
        if sda is not None and (op in LOADSTORE or op == 14):
            out.append((i, "sda", sda & 0xFFFFFFFF))
            for r in written_regs(w):
                val.pop(r, None)
            if op == 14:
                val[rd] = sda & 0xFFFFFFFF
            continue
        if ra in val and ra not in (13, 2):
            if op == 14:
                a = (val[ra] + sx16(w)) & 0xFFFFFFFF
                out.append((i, "lo", a))
                val[rd] = a
                continue
            if op == 24:
                a = val[ra] | (w & 0xFFFF)
                # ori rA, rS, imm: here rS is bits 21-25 (named rd above), dest is ra
                out.append((i, "ori", a))
                continue
            if op in LOADSTORE:
                a = (val[ra] + sx16(w)) & 0xFFFFFFFF
                out.append((i, "ls", a))
                for r in written_regs(w):
                    val.pop(r, None)
                continue
        if op == 24:
            # ori rA, rS, imm with rS holding a hi value
            if rd in val:
                a = val[rd] | (w & 0xFFFF)
                out.append((i, "ori", a))
                val[ra] = a
                continue
        if op in (18,) and (w & 1):
            for r in (0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12):
                val.pop(r, None)
            continue
        for r in written_regs(w):
            val.pop(r, None)
    return out


# ----------------------------------------------------------------------------- driver
# ----------------------------------------------------------------------------- patch address report
ADDR_RE = re.compile(r"0x(80[0-9A-Fa-f]{6})\b")
NAME_RE = re.compile(r"\b(?:fn|lbl|jumptable|zz)_([0-9A-F]{8})\b")


def strip_comments(text, kind):
    """Return text with comments blanked (same length, newlines kept)."""
    if kind == "json":
        return text
    out, i, n = [], 0, len(text)
    while i < n:
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(c if c == "\n" else " " for c in text[i:j]))
            i = j
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def collect_patch_addresses(patch_dir, byname):
    """-> {addr: {"uses": [(file, line, 'code'|'doc')], "names": set()}}"""
    files = []
    gdir = os.path.join(patch_dir, "game")
    for fn in sorted(os.listdir(gdir)):
        if fn.endswith((".c", ".S", ".json", ".ld")):
            files.append(os.path.join(gdir, fn))
    for fn in ("recompcore-skies-settings.patch", "recompcore-restore-boot-dol.patch"):
        files.append(os.path.join(patch_dir, fn))
    found = {}

    def note(addr, rel, line, kind, name=None):
        d = found.setdefault(addr, {"uses": [], "names": set()})
        if (rel, line, kind) not in d["uses"]:
            d["uses"].append((rel, line, kind))
        if name:
            d["names"].add(name)

    for path in files:
        rel = os.path.relpath(path, os.path.dirname(patch_dir))
        text = open(path, errors="replace").read()
        kind = "json" if path.endswith(".json") else ("ld" if path.endswith(".ld") else "src")
        stripped = strip_comments(text, kind) if kind != "json" else text
        if path.endswith(".patch"):
            # keep only added lines for the code view; comments blanked per line
            stripped = "\n".join(strip_comments(l, "src") if l.startswith("+") else " " * len(l) for l in text.split("\n"))
        # json: "why" and "comment" values are docs
        if kind == "json":
            doc = set()
            for m in re.finditer(r'"(why|comment)"\s*:\s*"((?:[^"\\]|\\.)*)"', text):
                doc.update(range(m.start(2), m.end(2)))
        for m in ADDR_RE.finditer(text):
            addr = int(m.group(1), 16) | 0x0
            line = text.count("\n", 0, m.start()) + 1
            code = stripped[m.start():m.end()] == m.group(0) and not (kind == "json" and m.start() in doc)
            note(0x80000000 | int(m.group(1)[2:], 16) if False else int(m.group(1), 16), rel, line, "code" if code else "doc")
        for m in NAME_RE.finditer(text):
            addr = int(m.group(1), 16)
            line = text.count("\n", 0, m.start()) + 1
            code = stripped[m.start():m.end()] == m.group(0) and not (kind == "json" and m.start() in doc)
            note(addr, rel, line, "code" if code else "doc", m.group(0))
        # symbols named in json / extern declarations / asm branches
        idents = set()
        if kind == "json":
            for m in re.finditer(r'"(?:target|[A-Za-z_0-9]+)"\s*:\s*"([A-Za-z_][A-Za-z_0-9]*)"', text):
                idents.add(m.group(1))
        elif path.endswith(".c"):
            for m in re.finditer(r"extern\s+[^;(]*?\b(\w+)\s*\(", stripped):
                idents.add(m.group(1))
        elif path.endswith(".S"):
            for m in re.finditer(r"^\s*(?:FULLWIDTH|UNWIDE|GATE)\s+\w+\s*,\s*(\w+)|^\s*bl?\s+(\w+)", stripped, re.M):
                idents.add(m.group(1) or m.group(2))
        for ident in idents:
            if ident in byname and not NAME_RE.fullmatch(ident):
                m = re.search(r"\b%s\b" % re.escape(ident), stripped)
                line = stripped.count("\n", 0, m.start()) + 1 if m else 1
                note(byname[ident], rel, line, "code", ident)
    return found


def short_uses(uses, limit=4):
    seen, out = [], []
    for rel, line, kind in uses:
        k = "%s:%d%s" % (os.path.basename(rel), line, "" if kind == "code" else "(doc)")
        if k not in seen:
            seen.append(k)
    code = [k for k in seen if "(doc)" not in k]
    doc = [k for k in seen if "(doc)" in k]
    sel = (code + doc)[:limit]
    return ", ".join(sel) + (" +%d" % (len(seen) - len(sel)) if len(seen) > len(sel) else "")


def report(ctx):
    M, usa, eu = ctx["M"], ctx["usa"], ctx["eu"]
    funcs, symdata, byname, cons, args = ctx["funcs"], ctx["symdata"], ctx["byname"], ctx["cons"], ctx["args"]
    import bisect
    fstarts = [a for a, s, n in funcs]
    names_of = {}
    for a, s, n in funcs:
        names_of.setdefault(a, n)
    dnames = {}
    for a, s, n, sec in symdata:
        dnames.setdefault(a, n)

    def containing_fn(addr):
        k = bisect.bisect_right(fstarts, addr) - 1
        if k >= 0 and funcs[k][0] <= addr < funcs[k][0] + funcs[k][1]:
            return funcs[k]
        return None

    def meaning(addr):
        f = containing_fn(addr)
        if f:
            return f[2] if addr == f[0] else "%s+0x%X" % (f[2], addr - f[0])
        s = region_of(symdata, addr)
        if s:
            return s[2] if addr == s[0] else "%s+0x%X" % (s[2], addr - s[0])
        return ""

    found = collect_patch_addresses(args.patches, byname)
    rows, problems = [], []
    out_map = {}
    PATCH_FN_SIZES = {}

    # redirect sites from the json specs
    site_specs = {}   # site addr -> (target name, to)
    for jf in ("widescreen.json", "shipdebug.json"):
        spec = json.load(open(os.path.join(args.patches, "game", jf)))
        for rs in spec.get("redirect_sites", []):
            for s in ([rs["site"]] if "site" in rs else rs["sites"]):
                site_specs[int(s, 16)] = (rs["target"], rs["to"])
    # frontend hook sites (bl in the unpatched game), by the comments
    site_specs.setdefault(0x801DC498, ("VIGetRetraceCount", None))

    def eu_site_check(ua):
        mp = M.map_in_function(ua)
        if not mp:
            return None, "not in a matched function"
        fa, ea, how = mp
        if ea is None:
            return None, "site is in an unaligned part of %s" % names_of.get(fa)
        uw, ew = usa.word(ua), eu.word(ea)
        note = "%s -> %s" % (dis(uw, ua), dis(ew, ea))
        if is_bl(uw):
            ut, et = b_target(uw, ua), b_target(ew, ea) if is_bl(ew) else None
            if et is None:
                return ea, "EU instruction is NOT a bl: %s" % dis(ew, ea)
            exp = M.match.get(ut, {}).get("eu")
            if exp is None:
                return ea, "bl ok but USA target 0x%08X unmatched (EU target 0x%08X)" % (ut, et)
            if exp != et:
                return ea, "bl TARGET MISMATCH: EU calls 0x%08X, map says 0x%08X" % (et, exp)
            return ea, "bl verified, target 0x%08X" % et
        return ea, "USA instruction is not a bl (%s); EU %s" % (dis(uw, ua), dis(ew, ea))

    for addr_low in sorted(found):
        ua = 0x80000000 | addr_low if addr_low < 0x80000000 else addr_low
        d = found[addr_low]
        ua = addr_low if addr_low >= 0x80000000 else 0x80000000 | addr_low
        uses = d["uses"]
        is_code = any(k == "code" for _, _, k in uses)
        row = {"usa": ua, "meaning": meaning(ua) or "/".join(sorted(d["names"])), "where": short_uses(uses),
               "used_as": "code" if is_code else "doc", "eu": None, "method": "", "conf": "none", "note": "",
               # in a source, spec or linker script (not only listed in a region's address table):
               # gamepatch.py translates comments too, so doc uses count
               "table": any(not rel.endswith(".addresses.json") for rel, _, k in uses)}
        if ua < 0x80003100:
            row.update(eu=ua, method="unchanged (patch cave / low memory)", conf="high",
                       note="outside the DOL; patch-owned or Dolphin low memory")
            if ua in (0x80000000,):
                row["note"] = "start of RAM; constant"
        elif ua >= 0x81000000 or ua == 0x80003000:
            row.update(eu=ua, method="constant", conf="high", note="not a game address")
        elif ua in (0x802548E8, 0x80257074) and False:
            pass
        else:
            f = containing_fn(ua)
            if f and ua == f[0]:
                v = M.match.get(ua)
                if v:
                    cov = len(M.align(ua, v["eu"])[0]) / max(1, len(M.sig[ua]))
                    conf = "high" if (v["method"] == "exact" or cov >= 0.9) else "medium"
                    note = "fn size 0x%X -> 0x%X" % (f[1], v.get("size_eu", 0)) if v.get("size_eu") != f[1] else "same size 0x%X" % f[1]
                    note += ", %d%% of instructions aligned" % round(cov * 100)
                    row.update(eu=v["eu"], method="function/" + v["method"], conf=conf, note=note)
                else:
                    row.update(method="function/UNMATCHED", conf="none", note="no EU match found")
            elif f:
                ea, note = eu_site_check(ua)
                if ea is not None:
                    ok = "verified" in note or "->" in note and "NOT" not in note and "MISMATCH" not in note
                    row.update(eu=ea, method="in-function/aligned", conf="high" if "verified" in note else "medium", note=note)
                    if "NOT" in note or "MISMATCH" in note:
                        row["conf"] = "low"
                else:
                    row.update(method="in-function/UNALIGNED", conf="none", note=note)
            else:
                e, method, conf, note = map_data_addr(M, cons, symdata, ua)
                if e is not None:
                    sym = region_of(symdata, ua)
                    if sym:
                        deltas = collections.Counter(cons[x][0] - x for x in cons if sym[0] <= x < sym[0] + max(sym[1], 1))
                        if len(deltas) == 1 and sum(deltas.values()) > 1:
                            xs = sorted(x for x in cons if sym[0] <= x < sym[0] + max(sym[1], 1))
                            note += "; %d refs in the label (+0x%X..+0x%X) all at delta %+d" % (
                                len(xs), xs[0] - sym[0], xs[-1] - sym[0], list(deltas)[0])
                        if len(deltas) > 1:
                            note += "; LABEL %s has refs with different deltas %s" % (
                                sym[2], ", ".join("%+d x%d" % kv for kv in deltas.most_common(4)))
                            conf = "low"
                    # content comparison for initialised data
                    k, _ = usa.section_of(ua)
                    if k in ("data", "text") and e is not None:
                        size = min(sym[1] if sym else 16, 0x80) or 16
                        ub, eb = usa.read(ua, size), eu.read(e, size)
                        if ub and eb:
                            same = sum(1 for i in range(0, len(ub) - 3, 4)
                                       if ub[i:i + 4] == eb[i:i + 4] or (ub[i] == 0x80 and eb[i] == 0x80))
                            tot = max(1, len(ub) // 4)
                            note += "; content %d/%d words equal (pointers ignored)" % (same, tot)
                    row.update(eu=e, method="data/" + method, conf=conf, note=note)
                else:
                    row.update(method="data/UNMAPPED", conf="none", note="no code reference or neighbour to derive it from")
        rows.append(row)
        if row["eu"] is not None:
            out_map["0x%08X" % ua] = "0x%08X" % row["eu"]
        else:
            problems.append(row)

    # ---------- extra analyses
    extra = []

    # widescreen.c anchor table: function sizes
    wsrc = open(os.path.join(args.patches, "game", "widescreen.c")).read()
    tbl = re.findall(r"\{0x(80[0-9A-F]{6}),\s*0x([0-9A-F]+),", wsrc)
    extra.append("### widescreen.c WS_HUD_ANCHORS: (address, length) entries\n")
    extra.append("| USA fn | USA length in table | USA fn size | EU fn | EU size | note |\n|---|---|---|---|---|---|")
    for a, ln in tbl:
        a, ln = int(a, 16), int(ln, 16)
        f = containing_fn(a)
        v = M.match.get(a)
        usz = f[1] if f and f[0] == a else None
        esz = v.get("size_eu") if v else None
        note = ""
        if usz is not None and ln != usz:
            note = "table length != fn size (%s)" % ("ends mid-function or covers a prefix" if ln < usz else "covers more")
        if esz is not None and usz is not None and esz != usz:
            note += (" " if note else "") + "EU fn size differs by %+d bytes: scale the table length" % (esz - usz)
        extra.append("| 0x%08X %s | 0x%X | %s | %s | %s | %s |" % (a, names_of.get(a, ""), ln, hex(usz) if usz else "?",
                     "0x%08X" % v["eu"] if v else "-", hex(esz) if esz else "?", note))
        if esz is not None and usz is not None and esz != usz:
            problems.append({"usa": a, "meaning": "WS_HUD_ANCHORS length", "note": "EU size %+d" % (esz - usz)})

    # redirect_calls: count the bl sites per target in both DOLs
    spec = json.load(open(os.path.join(args.patches, "game", "widescreen.json")))
    names_ro = []
    for rc in spec.get("redirect_calls", []):
        names_ro.append(rc["target"])
    for rs in spec.get("redirect_sites", []):
        names_ro.append(rs["target"])
    names_ro = sorted(set(names_ro))

    def count_bls(dol, target):
        n = 0
        for base, arr in dol.tbase:
            for i, w in enumerate(arr):
                if (w >> 26) == 18 and (w & 3) == 1 and b_target(w, base + 4 * i) == target:
                    n += 1
        return n

    extra.append("\n### redirect_calls / redirect_sites targets: number of `bl target` in the DOL\n")
    extra.append("Every `bl target` of a redirect_calls entry is rewritten, so the EU count tells how many sites get patched (new EU-only callers will be hooked too, which is normally fine for ws_* wrappers but should be known).\n")
    extra.append("| target | USA addr | USA bl count | EU addr | EU bl count | note |\n|---|---|---|---|---|---|")
    for nm in names_ro:
        ua = byname.get(nm)
        if ua is None:
            continue
        v = M.match.get(ua)
        uc = count_bls(usa, ua)
        ec = count_bls(eu, v["eu"]) if v else None
        extra.append("| %s | 0x%08X | %d | %s | %s | %s |" % (nm, ua, uc, "0x%08X" % v["eu"] if v else "-", ec,
                     "" if ec == uc else "COUNT DIFFERS"))
        if ec is not None and ec != uc:
            problems.append({"usa": ua, "meaning": "bl count of %s" % nm, "note": "USA %d, EU %d" % (uc, ec)})

    # pointer table
    ptab = 0x802DBB10
    ue = usa.word(ptab)
    emap = map_data_addr(M, cons, symdata, ptab)
    extra.append("\n### redirect_pointers: 0x802DBB10 (battle action handlers, entry 0 = fn_8002DC14)\n")
    if emap[0]:
        ew = eu.word(emap[0])
        want = M.match.get(0x8002DC14, {}).get("eu")
        extra.append("- USA entry 0 = 0x%08X; EU table at 0x%08X (%s), entry 0 = 0x%08X; expected EU fn_8002DC14 = %s -> **%s**" % (
            ue, emap[0], emap[1], ew, "0x%08X" % want if want else "?", "OK" if ew == want else "MISMATCH"))
        usz = [usa.word(ptab + 4 * i) for i in range(0x13C // 4)]
        ok = 0
        for i, uw in enumerate(usz):
            ev = eu.word(emap[0] + 4 * i)
            mv = M.match.get(uw, {}).get("eu") if uw else 0
            if uw == 0 and ev == 0 or (mv and mv == ev):
                ok += 1
        extra.append("- all %d table words: %d map to the same EU function (0/zero included)" % (len(usz), ok))

    # frontend HOOK_ORIGINAL words: the unpatched `bl` the frontend compares the site with
    extra.append("\n### Frontend HOOK_ORIGINAL constants (recompcore-skies-settings.patch)\n")
    extra.append("| site (USA) | USA word | = | EU site | EU word (the new HOOK_ORIGINAL) |\n|---|---|---|---|---|")
    for site, const, what in ((0x8022D910, 0x4BFFF8E9, "camera: bl fn_8022D1F8"), (0x800C207C, 0x4819CC49, "qol: bl rand"),
                              (0x801DC350, 0x4804C6D1, "autoload: bl fn_80228A20")):
        mp = M.map_in_function(site)
        ea_ = mp[1] if mp else None
        extra.append("| 0x%08X %s | 0x%08X | %s | %s | %s |" % (
            site, what, usa.word(site), "matches the patch constant" if usa.word(site) == const else "DIFFERS from the patch constant",
            "0x%08X" % ea_ if ea_ else "-", "0x%08X" % eu.word(ea_) if ea_ else "-"))

    # cave / low memory
    extra.append("\n### Patch cave (0x80001900-0x80002F00) and DOL sections\n")
    ol = [(b, s) for _, b, s in eu.text + eu.dat if b < 0x80003100 and b + s > 0x80001800]
    extra.append("- EU DOL sections overlapping 0x80001800-0x80003100: %s" % (ol if ol else "none -> the cave is free"))
    extra.append("- EU text/data sections: " + ", ".join("0x%08X+0x%X" % (b, s) for _, b, s in eu.text + eu.dat))
    extra.append("- EU bss: 0x%08X+0x%X (USA: 0x%08X+0x%X); entry EU 0x%08X, USA 0x%08X" % (
        eu.bss[0], eu.bss[1], usa.bss[0], usa.bss[1], eu.entry, usa.entry))
    extra.append("- patch data sections are added at 0x80002A00.. : no collision with any EU section (checked above)")

    # __start / MetroTRK
    extra.append("\n### __start and MetroTRK\n")
    sw = [(usa.word(0x80003140 + 4 * i), eu.word(0x80003140 + 4 * i)) for i in range(0x138 // 4)]
    diffs = [(0x80003140 + 4 * i, u, e) for i, (u, e) in enumerate(sw) if u != e]
    extra.append("- `__start` (0x80003140, 0x138 bytes): %d of %d words differ between USA and EU%s" % (
        len(diffs), len(sw), "" if not diffs else ": " + "; ".join("0x%08X %s -> %s" % (a, dis(u, a), dis(e, a)) for a, u, e in diffs[:8])))
    def gate_info(dol):
        ws_ = [dol.word(0x80003140 + 4 * i) for i in range(0x138 // 4)]
        g = [0x80003140 + 4 * i for i, w in enumerate(ws_) if (w >> 26) == 14 and sx16(w) == 0x30E8]
        cmps = [(0x80003140 + 4 * i, w & 0xFFFF) for i, w in enumerate(ws_) if (w >> 26) == 10 and (w & 0xFFFF) in (2, 3)]
        return g, cmps
    gu, gc_u = gate_info(usa)
    ge, gc_e = gate_info(eu)
    extra.append("- gate: `addi r7,r7,0x30E8` (reads 0x800030E8) at USA %s, EU %s; cmplwi 2 / 3 at USA %s, EU %s -> %s" % (
        [hex(x) for x in gu], [hex(x) for x in ge], [hex(a) for a, v in gc_u], [hex(a) for a, v in gc_e],
        "same gating" if gu == ge and gc_u == gc_e else "DIFFERENT"))
    w_u = usa.word(0x800031C0), usa.word(0x800031C4)
    w_e = eu.word(0x800031C0), eu.word(0x800031C4)
    itrk_u = ((w_u[0] & 0xFFFF) << 16) + sx16(w_u[1])
    itrk_e = ((w_e[0] & 0xFFFF) << 16) + sx16(w_e[1])
    extra.append("- `lis/addi r6, InitMetroTRK`: USA 0x%08X, EU 0x%08X (map: %s) -> %s" % (
        itrk_u, itrk_e, "0x%08X" % M.match[itrk_u]["eu"] if itrk_u in M.match else "unmatched",
        "OK" if M.match.get(itrk_u, {}).get("eu") == itrk_e else "MISMATCH"))
    bad = []
    for i, (u, e) in enumerate(sw):
        pc = 0x80003140 + 4 * i
        if u != e and is_bl(u) | is_b(u):
            ut, et = b_target(u, pc), b_target(e, pc)
            if M.match.get(ut, {}).get("eu") != et:
                bad.append((hex(pc), hex(ut), hex(et)))
    extra.append("- `__start` branch targets that differ all map to the matched EU function: %s" % ("yes" if not bad else bad))

    TA, TB = 0x802548E8, 0x80257074
    trk_us = [(a, s, n) for a, s, n in funcs if TA <= a < TB]
    unm = [n for a, s, n in trk_us if a not in M.match]
    extra.append("- USA MetroTRK handler range 0x%08X-0x%08X holds %d functions (%s .. %s); matched in EU: %d, unmatched: %s" % (
        TA, TB, len(trk_us), trk_us[0][2], trk_us[-1][2], len(trk_us) - len(unm), unm or "none"))
    if trk_us and trk_us[0][0] in M.match and trk_us[-1][0] in M.match:
        ea0 = M.match[trk_us[0][0]]["eu"]
        eaN = M.match[trk_us[-1][0]]["eu"]
        eend = eaN + M.match[trk_us[-1][0]].get("size_eu", trk_us[-1][1])
        contiguous = all(M.match.get(a, {}).get("eu") == ea0 + (a - TA) for a, s, n in trk_us if a in M.match)
        extra.append("- EU equivalent range: **0x%08X-0x%08X** (size 0x%X, USA 0x%X); all functions at the same relative offsets: %s" % (
            ea0, eend, eend - ea0, TB - TA, contiguous))
        # references from outside the range
        def refs_in(dol, lo_, hi_):
            bl_out = []
            for base, arr in dol.tbase:
                for i, w in enumerate(arr):
                    pc = base + 4 * i
                    if (w >> 26) == 18 and not (w & 2):
                        t = b_target(w, pc)
                        if lo_ <= t < hi_ and not (lo_ <= pc < hi_):
                            bl_out.append((pc, t))
            ptr = []
            for off, base, size in dol.dat:
                for i in range(0, size, 4):
                    v = struct.unpack(">I", dol.data[off + i:off + i + 4])[0]
                    if lo_ <= v < hi_:
                        ptr.append((base + i, v))
            return bl_out, ptr
        for label, dol, lo_, hi_ in (("USA", usa, TA, TB), ("EU", eu, ea0, eend)):
            bo, ptrs = refs_in(dol, lo_, hi_)
            extra.append("  - %s: branches into the range from outside: %s; data pointers into it: %d in 0x%08X..0x%08X" % (
                label, [(hex(a), hex(b)) for a, b in bo[:6]] or "none", len(ptrs),
                min([a for a, _ in ptrs], default=0), max([a for a, _ in ptrs], default=0)))
        out_map["0x%08X" % TA] = "0x%08X" % ea0
        out_map["0x%08X" % TB] = "0x%08X" % eend
        rows.append({"usa": TA, "meaning": "MetroTRK handlers start (TRK_CAVE[0])", "where": "gamepatch.py, cave.ld",
                     "used_as": "code", "eu": ea0, "method": "function/" + M.match[TA]["method"], "conf": "high",
                     "note": "range end below"})
        rows.append({"usa": TB, "meaning": "MetroTRK handlers end (TRK_CAVE[1])", "where": "gamepatch.py, cave.ld",
                     "used_as": "code", "eu": eend, "method": "end of last fn (fn_80256FC4 EU + size)", "conf": "medium",
                     "note": "EU size 0x%X vs USA 0x%X" % (eend - ea0, TB - TA)})
        # dispatch table
        tb = map_data_addr(M, cons, symdata, 0x802FBCF0)
        if tb[0]:
            tot, okc = 0, 0
            for i in range(0x88 // 4):
                uw = usa.word(0x802FBCF0 + 4 * i)
                ev = eu.word(tb[0] + 4 * i)
                if TA <= uw < TB:
                    tot += 1
                    if M.match.get(uw, {}).get("eu") == ev:
                        okc += 1
            extra.append("- dispatch table lbl_802FBCF0 -> EU 0x%08X (%s): %d/%d handler entries point at the matched EU function" % (tb[0], tb[1], okc, tot))
            rows.append({"usa": 0x802FBCF0, "meaning": "lbl_802FBCF0 TRK dispatch table (cave.ld doc)", "where": "cave.ld(doc)",
                         "used_as": "doc", "eu": tb[0], "method": "data/" + tb[1], "conf": tb[2], "note": ""})
            out_map["0x802FBCF0"] = "0x%08X" % tb[0]

    # -- struct fields the patches read or write: same load/store displacement in the aligned EU code?
    LS = tuple(range(32, 56))
    fields = [
        ("battle unit record (lbl_80309DE4[i])", 0x80309DE4, [(0x14, (32, 36), "HP"), (0x18, (32, 36), "max HP"), (0xFE, (34, 38), "MP"), (0xFF, (34, 38), "max MP")]),
        ("ground battle data (lbl_80347390)", 0x80347390, [(0x1, (34, 38), "magic EXP"), (0x6, (40, 42, 44, 43), "spirit max"), (0x8, (40, 42, 44, 43), "spirit"), (0xA, (40, 42, 44, 43), "spirit 2"), (0x30, (32, 36), "EXP"), (0x34, (32, 36), "gold")]),
        ("ship battle data (lbl_8034727C ->)", 0x8034727C, [(0x14, (32, 36), "ship max HP"), (0x18, (32, 36), "EXP / ship HP"), (0x1C, (32, 36), "gold"), (0x20, (32, 36), "ship ptr"), (0x12C, (32, 36), "phase dmg"), (0x130, (32, 36), "phase dmg (patch)")]),
        ("party records (lbl_8030B7F4)", 0x8030B7F4, [(0xD, (34, 38), "MP"), (0xE, (34, 38), "max MP"), (0x328, (34, 38), "slot table")]),
        ("title work (lbl_80311AD4)", 0x80311AD4, [(0xC, LS, "title state")]),
        ("event flags (lbl_80310B3C)", 0x80310B3C, [(0x84, LS, "bit 31 cleared by autoload")]),
        ("camera manager (lbl_80311970)", 0x80311970, [(0x0, LS, "mode"), (0x8, LS, "eye source"), (0x10, LS, "eye"), (0x1C, LS, "angles")]),
        ("field camera (lbl_80302F18)", 0x80302F18, [(0x24, LS, "look-at"), (0x7C, LS, "eye"), (0x88, LS, "pitch")]),
    ]
    sz_of = {a_: s_ for a_, s_, n_, sec_ in symdata}
    extra.append("\n### Struct fields used by the patches (load/store displacements, matched pairs)\n")
    extra.append("For every function that references the struct's label, each aligned load/store in the USA code with the field's displacement and width is compared with the EU instruction: `same` keeps the displacement, `diff` does not. `same` counts are evidence the layout is unchanged; any `diff` needs a look. Base-register pointers loaded from the label are included; the label base itself is in the address table.\n")
    extra.append("| struct | field | meaning | same | diff | examples of diff |\n|---|---|---|---|---|---|")
    for title, L, flds in fields:
        users_ = []
        for fa, fs, fn_ in funcs:
            if fa in M.match and any(L <= ad < L + max(sz_of.get(L, 4), 4) for _, _, ad in data_refs(usa, fa, fs // 4)):
                users_.append(fa)
        for off, ops, what in flds:
            same = diff = 0
            samples = []
            for fa in users_:
                ea_ = M.match[fa]["eu"]
                for i, j in M.align(fa, ea_)[0]:
                    uw, ew = usa.word(fa + 4 * i), eu.word(ea_ + 4 * j)
                    if uw >> 26 in ops and sx16(uw) == off and (uw >> 16) & 31 not in (13, 2, 1):
                        if ew >> 26 == uw >> 26 and sx16(ew) == off:
                            same += 1
                        else:
                            diff += 1
                            samples.append("0x%08X `%s` -> `%s`" % (fa + 4 * i, dis(uw), dis(ew)))
            if diff:
                problems.append({"usa": L, "meaning": "%s +0x%X (%s)" % (title, off, what), "note": "%d same, %d diff: %s" % (same, diff, samples[:2])})
            extra.append("| %s | +0x%X | %s | %d | %d | %s |" % (title, off, what, same, diff, "; ".join(samples[:3]) or ("no instance in matched code" if not same else "")))

    # -- struct offsets and code differences in the functions the patches read
    extra.append("\n### Code differences in the functions whose fields the patches use\n")
    extra.append("For each matched pair: instructions that differ beyond bl targets, resolved address halves and SDA offsets (so: struct offsets, constants, changed code), and how many USA instructions have no counterpart.\n")
    key_fns = ["fn_8002DC14", "fn_8006F4B0", "fn_80078610", "fn_80144E60", "fn_801453D0", "fn_8014419C", "fn_8015C6C8",
               "fn_8015C6A8", "fn_8012A26C", "fn_8000A118", "fn_80077428", "fn_800C1C24", "fn_8022D384", "fn_8022D1F8",
               "fn_80228A20", "fn_80228AD4", "fn_801A4070", "fn_801A3A10", "fn_801A4684", "fn_801A4354", "fn_801A41C0",
               "fn_801A372C", "fn_8019E294", "fn_801D71BC", "fn_801DBE6C", "fn_801DC420", "fn_801DC288", "fn_8015887C",
               "fn_8021BD7C", "fn_80100270", "fn_801E1C7C", "fn_801E17E0", "fn_80101828", "fn_801D1714",
               "FontIsReady", "FontUpdate", "TextPrint", "TextPrintN", "TextPrintNumber", "TextPrintf", "TextDrawNode",
               "TextFlush", "fn_8029CB74", "fn_8029C8A0", "fn_80290ED4", "fn_80291064", "njProjectScreen", "njClipZ",
               "fn_800B522C", "fn_800B8028", "fn_80029B70", "fn_8002A574", "fn_802297C4", "fn_8021F330", "fn_8021F0EC",
               "fn_800B0FD0", "fn_80097B0C", "fn_80097CEC", "fn_80098DA4", "fn_8028A0F4", "fn_80226D14"]
    extra.append("| function | USA | EU | aligned | non-address differences |\n|---|---|---|---|---|")
    detail = []
    for nm in key_fns:
        ua = byname.get(nm)
        if ua is None or ua not in M.sig:
            continue
        v = M.match.get(ua)
        if not v:
            extra.append("| %s | 0x%08X | UNMATCHED | | |" % (nm, ua))
            continue
        diffs, ins = fn_diff(M, ua)
        n = len(M.sig[ua])
        cov = len(M.align(ua, v["eu"])[0])
        extra.append("| %s | 0x%08X | 0x%08X | %d/%d | %d diffs, %d USA insns unaligned |" % (nm, ua, v["eu"], cov, n, len(diffs), n - cov))
        if diffs or n - cov:
            detail.append((nm, ua, v["eu"], diffs, n - cov))
    for nm, ua, ea, diffs, un in detail:
        extra.append("\n`%s` USA 0x%08X / EU 0x%08X: %d unaligned USA insns" % (nm, ua, ea, un))
        for a, e, du, de in diffs[:25]:
            extra.append("- 0x%08X `%s`  vs  0x%08X `%s`" % (a, du, e, de))
        if len(diffs) > 25:
            extra.append("- ... %d more" % (len(diffs) - 25))

    # -- write
    rows.sort(key=lambda r: r["usa"])
    md = ["# Skies of Arcadia Legends: USA (GEAE8P) -> %s address map for the patches (\"EU\" below: %s)\n" % (
              args.target, args.target),
          "Generated by `tools/dolmatch.py`. Methods: function/exact = unique masked-instruction signature; function/call = found through an aligned `bl` of a matched caller; function/order = matched between two anchors by instruction similarity; in-function/aligned = same instruction in the aligned body of a matched function; data/code-ref = the EU address the same lis/addi/SDA instruction references; data/label-relative = offset inside a label that has a code ref; data/bracket = neighbours' delta (both agree); data/bracket-diverge = neighbours disagree (verify by hand).\n",
          "\nStats: %d USA functions, %d matched, %d unmatched (see dolmap.json); %d addresses below, %d unmapped/need attention.\n" % (
              len(funcs), len(M.match), len(funcs) - len(M.match), len(rows), len([r for r in rows if r["eu"] is None or r["conf"] in ("low", "none")])),
          "\n## Addresses\n", "| USA | symbol / meaning | used | where | EU | method | conf | notes |", "|---|---|---|---|---|---|---|---|"]
    for r in rows:
        md.append("| 0x%08X | %s | %s | %s | %s | %s | %s | %s |" % (
            r["usa"], r["meaning"], r["used_as"], r["where"].replace("|", "/"),
            "0x%08X" % r["eu"] if r["eu"] is not None else "-", r["method"], r["conf"], r["note"].replace("|", "/")))
    md.append("\n## Checks\n")
    md.extend(extra)
    with open(os.path.join(args.out_dir, "address-map.md"), "w") as f:
        f.write("\n".join(md) + "\n")
    with open(os.path.join(args.out_dir, "address-map.json"), "w") as f:
        json.dump(dict(sorted(out_map.items())), f, indent=1)
    # The region's translation table for gamepatch.py (patches/game/<ID>.addresses.json): the
    # addresses the sources use, each with a confident match
    table = {"0x%08X" % r["usa"]: "0x%08X" % r["eu"] for r in rows if r.get("table", True) and r["eu"] is not None
             and (r["conf"] not in ("low", "none") or r["used_as"] == "doc")}
    with open(os.path.join(args.out_dir, "%s.addresses.json" % args.target), "w") as f:
        json.dump({"about": "USA (GEAE8P) -> %s main.dol: every game address the patches (patches/game) and the "
                            "frontend use. Made by tools/dolmatch.py (function signatures, call sites and data "
                            "references); checks and notes in its address-map.md output." % args.target,
                   "addresses": dict(sorted(table.items()))}, f, indent=1)
        f.write("\n")
    for r in rows:
        if r.get("table", True) and (r["eu"] is None or r["conf"] in ("low", "none") and r["used_as"] == "code"):
            print("  TABLE: 0x%08X %s is used by %s but has no confident %s address" % (
                r["usa"], r["meaning"], r["where"], args.target))
    print("addresses: %d mapped of %d; need attention: %d" % (len(out_map), len(rows), len([r for r in rows if r["eu"] is None or r["conf"] in ("low", "none")])))
    for r in rows:
        if r["eu"] is None or r["conf"] in ("low", "none"):
            print("  ATTN 0x%08X %s: %s %s" % (r["usa"], r["meaning"], r["method"], r["note"]))
    for p in problems:
        if "method" not in p:
            print("  NOTE 0x%08X %s: %s" % (p["usa"], p["meaning"], p["note"]))


def build_data_votes(M):
    votes = collections.defaultdict(collections.Counter)
    for ua, v in M.match.items():
        if ua not in M.sig:
            continue
        ea = v["eu"]
        pairs, _ = M.align(ua, ea)
        n = len(M.sig[ua])
        ur = {i: (k, a) for i, k, a in data_refs(M.u, ua, n)}
        m = max((j for _, j in pairs), default=0) + 1
        er = {i: (k, a) for i, k, a in data_refs(M.e, ea, m + 1)}
        for i, j in pairs:
            if i in ur and j in er and ur[i][0] == er[j][0]:
                u, e = ur[i][1], er[j][1]
                if u >= 0x80003100 and e >= 0x80003100:
                    votes[u][e] += 1
    return votes


def fn_diff(M, ua):
    """Non-address differences between a matched pair: (usa addr, eu addr, usa insn, eu insn) for
    aligned instructions whose resolved fields differ, plus the unaligned counts."""
    ea = M.match[ua]["eu"]
    n = len(M.sig[ua])
    pairs, _ = M.align(ua, ea)
    out = []
    # tight compare over the aligned pairs: everything but bl targets and resolved address refs
    ur = {i: a for i, k, a in data_refs(M.u, ua, n)}
    er = {i: a for i, k, a in data_refs(M.e, ea, max((j for _, j in pairs), default=0) + 2)}
    for i, j in pairs:
        uw, ew = M.u.word(ua + 4 * i), M.e.word(ea + 4 * j)
        if uw == ew:
            continue
        op = uw >> 26
        if op == 18:
            continue
        if i in ur and j in er:
            continue
        if (uw >> 16) & 31 in (13, 2) and op in IMM_OPS:
            continue
        if op == 15:  # lis of an address half not resolved: hi halves move
            continue
        out.append((ua + 4 * i, ea + 4 * j, dis(uw), dis(ew)))
    inserted = n - len(pairs)
    return out, inserted


def region_of(syms_data, addr):
    import bisect
    starts = [a for a, s, n, sec in syms_data]
    k = bisect.bisect_right(starts, addr) - 1
    # prefer the last label that starts at or before addr and covers it (or just the nearest)
    while k >= 0:
        a, s, n, sec = syms_data[k]
        if a <= addr < a + max(s, 1):
            return syms_data[k]
        if a + max(s, 1) <= addr:
            break
        k -= 1
    return None


def build_data_map(M, symdata, votes):
    """Consistent votes -> {usa: (eu, count)}"""
    cons = {}
    for u, c in votes.items():
        (e, n), = c.most_common(1)
        tot = sum(c.values())
        if n >= 1 and n * 2 > tot:
            cons[u] = (e, n, tot)
    return cons


def map_data_addr(M, cons, symdata, ua):
    """Return (eu, method, confidence, note) for a USA data address."""
    import bisect
    if ua in cons:
        e, n, tot = cons[ua]
        return e, "code-ref", "high" if n == tot else "medium", "%d/%d refs agree" % (n, tot)
    keys = sorted(cons)
    k = bisect.bisect_left(keys, ua)
    lo = keys[k - 1] if k else None
    hi = keys[k] if k < len(keys) else None
    # label-relative: containing label with a consistent ref inside it
    sym = region_of(symdata, ua)
    if sym:
        a, s, name, sec = sym
        # nearest consistent ref within the label
        inside = [x for x in keys if a <= x < a + max(s, 1)]
        if inside:
            x = inside[0]
            e = cons[x][0]
            return e + (ua - x), "label-relative", "high", "%s +0x%X, via ref 0x%08X" % (name, ua - a, x)
    if lo is not None and hi is not None:
        dl = cons[lo][0] - lo
        dh = cons[hi][0] - hi
        if dl == dh:
            return ua + dl, "bracket", "medium", "neighbours 0x%08X/0x%08X agree (delta %+d)" % (lo, hi, dl)
        return ua + dl, "bracket-diverge", "low", "neighbours disagree: 0x%08X delta %+d, 0x%08X delta %+d" % (lo, dl, hi, dh)
    return None, "none", "none", ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--usa", default=RESEARCH + "/disc/sys/main.dol")
    ap.add_argument("--eu", default=None, help="the other DOL (default: the research checkout's, by --target)")
    ap.add_argument("--symbols", default=DECOMP + "/config/GEAE8P/symbols.txt")
    ap.add_argument("--patches", default=os.path.join(ROOT, "patches"))
    ap.add_argument("--out-dir", default=None, help="default build/eu (GEAP8P), build/jp (GEAJ8P)")
    ap.add_argument("--target", default="GEAP8P", help="the other DOL's disc ID: GEAP8P or GEAJ8P")
    args = ap.parse_args()
    if args.eu is None:
        args.eu = RESEARCH + ("/jp/disc/sys/main.dol" if args.target == "GEAJ8P" else "/eu/disc/sys/main.dol")
    if args.out_dir is None:
        args.out_dir = os.path.join(ROOT, "build", "jp" if args.target == "GEAJ8P" else "eu")
    os.makedirs(args.out_dir, exist_ok=True)

    import hashlib
    sha = hashlib.sha1(open(args.usa, "rb").read()).hexdigest()
    if sha != USA_SHA1:
        print("warning: USA DOL SHA-1 is %s" % sha, file=sys.stderr)
    usa, eu = Dol(args.usa), Dol(args.eu)
    funcs, symdata, byname = load_symbols(args.symbols)
    print("USA functions: %d, data labels: %d" % (len(funcs), len(symdata)))
    print("SDA r2/r13 USA 0x%08X/0x%08X, EU 0x%08X/0x%08X" % (usa.r2, usa.r13, eu.r2, eu.r13))

    M = Matcher(usa, eu, funcs)
    M.run()
    nm = len(M.match)
    print("matched %d / %d USA functions (%.1f%%), unmatched %d" % (nm, len(funcs), 100.0 * nm / len(funcs), len(funcs) - nm))
    meth = collections.Counter(v["method"].split("(")[0] for v in M.match.values())
    print("by method:", dict(meth))

    votes = build_data_votes(M)
    cons = build_data_map(M, symdata, votes)
    for rnd in range(4):
        if not M.tables(cons, symdata):
            break
        M.propagate()
        M.gaps()
        for ua, v in M.match.items():
            if ua in M.sig and "size_eu" not in v:
                v["size_eu"] = M.align(ua, v["eu"])[1] * 4
        votes = build_data_votes(M)
        cons = build_data_map(M, symdata, votes)
    nm = len(M.match)
    print("after tables: matched %d / %d, unmatched %d" % (nm, len(funcs), len(funcs) - nm))
    meth = collections.Counter(v["method"].split("(")[0] for v in M.match.values())
    print("by method:", dict(meth))
    print("unmatched:", ", ".join("%s@0x%08X" % (n, a) for a, sz, n in funcs if a not in M.match))
    print("data refs: %d USA addresses voted, %d consistent" % (len(votes), len(cons)))

    ctx = {"M": M, "usa": usa, "eu": eu, "funcs": funcs, "symdata": symdata, "byname": byname,
           "votes": votes, "cons": cons, "args": args}
    report(ctx)

    fn_json = {}
    for a, s, n in funcs:
        v = M.match.get(a)
        fn_json["0x%08X" % a] = {"name": n, "size_usa": s,
                                 "eu": "0x%08X" % v["eu"] if v else None,
                                 "size_eu": v.get("size_eu") if v else None,
                                 "method": v["method"] if v else None}
    data_json = {"0x%08X" % u: {"eu": "0x%08X" % e, "refs": n, "total": t} for u, (e, n, t) in sorted(cons.items())}
    with open(os.path.join(args.out_dir, "dolmap.json"), "w") as f:
        json.dump({"stats": {"usa_functions": len(funcs), "matched": nm, "unmatched": len(funcs) - nm,
                             "methods": dict(meth)},
                   "sda": {"usa_r2": usa.r2, "usa_r13": usa.r13, "eu_r2": eu.r2, "eu_r13": eu.r13},
                   "functions": fn_json, "data": data_json}, f, indent=1)
    print("wrote", os.path.join(args.out_dir, "dolmap.json"))


if __name__ == "__main__":
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    main()
