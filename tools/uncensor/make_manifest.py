#!/usr/bin/env python3
"""Build the uncensor manifest the recomp applies at import (no game data: names, indices, SHA-1s).

usage: make_manifest.py OUTDIR [REGION...]   (regions: usa eu jp; default all)
Writes OUTDIR/<region>/<gc path> (reference outputs, for testing the C++ port) and
OUTDIR/uncensor-manifest.json:
  {"version": 1, "games": {"GEAE8P": {"<gc path>": {"gc_sha1", "out_sha1", "size", "dc_disc",
   "dc_path", "dc_sha1", "ops": [...]}}}}
ops, applied in order to the decompressed GameCube file (see uncensor.run):
  {"op": "model", "gc_entry", "slot", "gc_sha1", "jp_entry", "jp_sha1"}  replace an object block
  {"op": "tex", "name", "gc_sha1", "jp_sha1"}                             re-encode a texture in place
  {"op": "tex_add", "name", "after", "jp_sha1"}                           add a Japanese-only texture"""
import hashlib, io, contextlib, json, multiprocessing as mp, os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
from inventory import differing, gc_path, ROOT
GAME = {'usa': 'GEAE8P', 'eu': 'GEAP8P', 'jp': 'GEAJ8P'}
sha1 = lambda b: hashlib.sha1(b).hexdigest()


def job(args):
    region, j, u, rel, outdir = args
    g, grel = gc_path(region, rel)
    if not g or not rel.upper().endswith('.MLD'): return None
    import uncensor
    out = os.path.join(outdir, region, grel); os.makedirs(os.path.dirname(out), exist_ok=True)
    ops = []; buf = io.StringIO()
    try:
        with contextlib.redirect_stdout(buf):
            n = uncensor.run(j, u, g, out, log=lambda s: print(s), ops=ops)
    except Exception as e:
        if os.path.exists(out): os.unlink(out)
        return (region, grel, None, f'{type(e).__name__}: {e}')
    if n == 0: return None
    disc = 1 if '/files1/' in j else 2
    o = open(out, 'rb').read(); gd = open(g, 'rb').read()
    from aklz import decompress
    return (region, grel, {'gc_sha1': sha1(gd), 'out_sha1': sha1(o), 'size': len(o), 'dc_disc': disc,
                           'dc_path': rel.replace(os.sep, '/'), 'dc_sha1': sha1(open(j, 'rb').read()),
                           'dec_sha1': sha1(decompress(o)),
                           'ops': ops}, None)


if __name__ == '__main__':
    outdir = os.path.abspath(sys.argv[1]); regions = sys.argv[2:] or ['usa', 'eu', 'jp']
    files = differing()
    jobs = [(r, j, u, rel, outdir) for r in regions for j, u, rel in files]
    man = {'version': 1, 'games': {GAME[r]: {} for r in regions}}; errors = []
    with mp.Pool(max(1, os.cpu_count() // 2)) as pool:
        for res in pool.imap_unordered(job, jobs):
            if not res: continue
            region, grel, entry, err = res
            if err: errors.append(f'{region} {grel}: {err}'); print('ERROR', region, grel, err, flush=True); continue
            man['games'][GAME[region]][grel.replace(os.sep, '/')] = entry
            print(region, grel, len(entry['ops']), 'ops', flush=True)
    for g in man['games'].values():
        for k in list(g): g[k] = g.pop(k)
    man['games'] = {k: dict(sorted(v.items())) for k, v in man['games'].items()}
    json.dump(man, open(os.path.join(outdir, 'uncensor-manifest.json'), 'w'), indent=1, sort_keys=False)
    print({k: len(v) for k, v in man['games'].items()}, 'files;', len(errors), 'errors')
    for e in errors: print(' ', e)
