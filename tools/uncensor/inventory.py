#!/usr/bin/env python3
"""Run uncensor.py over every Dreamcast file that differs between the Japanese and US discs, for each
GameCube version, and report what converted, what was skipped and why.

usage: inventory.py OUTDIR [REGION...]   (regions: usa eu jp; default all)
Writes OUTDIR/<region>/<gc path> and OUTDIR/report.tsv."""
import multiprocessing as mp, os, sys, io, contextlib
HERE = os.path.dirname(os.path.abspath(__file__))
# The extracted discs (see README.md): $SKIES_RESEARCH, else ~/projects/GameCube-Research/targets/SkiesOfArcadiaLegends
ROOT = os.environ.get('SKIES_RESEARCH') or os.path.expanduser('~/projects/GameCube-Research/targets/SkiesOfArcadiaLegends')
sys.path.insert(0, HERE)
GC = {'usa': f'{ROOT}/disc/files', 'eu': f'{ROOT}/eu/disc/files', 'jp': f'{ROOT}/jp/disc/files'}


def differing():
    seen = {}
    for disc in (1, 2):
        jroot = f'{ROOT}/dcjp/files{disc}'; uroot = f'{ROOT}/dc/files{disc}'
        for dp, _, fs in os.walk(jroot):
            for f in fs:
                j = os.path.join(dp, f); rel = os.path.relpath(j, jroot); u = os.path.join(uroot, rel)
                if rel.lower() in seen or not os.path.exists(u): continue
                if open(j, 'rb').read() != open(u, 'rb').read():
                    seen[rel.lower()] = (j, u, rel)
    return sorted(seen.values(), key=lambda t: t[2])


def gc_path(region, rel):
    want = rel.lower(); root = GC[region]
    for dp, _, fs in os.walk(root):
        for f in fs:
            p = os.path.relpath(os.path.join(dp, f), root)
            if p.lower() == want: return os.path.join(root, p), p
    return None, None


def job(args):
    region, j, u, rel, outdir = args
    g, grel = gc_path(region, rel)
    if not g: return (region, rel, 'no GC file', '', '')
    if not rel.upper().endswith('.MLD'): return (region, rel, 'not an MLD (manual)', '', '')
    import uncensor
    out = os.path.join(outdir, region, grel); os.makedirs(os.path.dirname(out), exist_ok=True)
    buf = io.StringIO()
    try:
        with contextlib.redirect_stdout(buf):
            n = uncensor.run(j, u, g, out, log=lambda s: print(s))
    except Exception as e:
        if os.path.exists(out): os.unlink(out)
        return (region, rel, 'ERROR', f'{type(e).__name__}: {e}', buf.getvalue().replace('\n', ' | ')[:2000])
    if n == 0 and os.path.exists(out): os.unlink(out)
    lines = [l.strip() for l in buf.getvalue().splitlines()]
    skipped = [l for l in lines if 'skipped' in l and 'not matched' not in l]
    changed = [l for l in lines if 'replaced' in l or 'added' in l]
    return (region, rel, f'{n} changes', ' | '.join(changed)[:2000], ' | '.join(skipped)[:2000])


if __name__ == '__main__':
    outdir = os.path.abspath(sys.argv[1]); regions = sys.argv[2:] or ['usa', 'eu', 'jp']
    files = differing()
    print(len(files), 'differing Dreamcast files', flush=True)
    jobs = [(r, j, u, rel, outdir) for r in regions for j, u, rel in files]
    with mp.Pool(max(1, os.cpu_count() // 2)) as pool, open(os.path.join(outdir, 'report.tsv'), 'w') as rep:
        rep.write('region\tdc file\tresult\tchanged\tskipped\n')
        for row in pool.imap_unordered(job, jobs):
            rep.write('\t'.join(row) + '\n'); rep.flush()
            print(row[0], row[1], row[2], flush=True)
