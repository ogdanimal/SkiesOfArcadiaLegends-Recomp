#!/usr/bin/env python3
"""Make a GameCube (USA) MLD with the Japanese Dreamcast originals of censored models and textures.

usage: uncensor.py DCJP.MLD DCUS.MLD GCUS.mld(AKLZ or raw) OUT.mld [--raw]

For every object block that differs between the Japanese and US Dreamcast files, the GameCube block
is replaced by the converted Japanese one, but only when the US Dreamcast block converts exactly to
the GameCube one (so the GameCube model is the Dreamcast model, not a Legends remake). Textures the
Japanese file has and the US one lacks are added; same-name textures whose pixels differ are
re-encoded from the Japanese PVR (GVR RGB5A3, mipmapped)."""
import hashlib, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from aklz import decompress
from aklzc import compress
from mldsplice import Mld, tex_replace
from block2gc import block_to_gc
from pvr import decode
from gvr import reencode_like, new_cmpr

def entry_keys(m):
    keys={}
    for i,e in enumerate(m.entries()):
        k=(m.u(e+4), bytes(m.d[e+0x24:e+0x38]))
        keys.setdefault(k,[]).append(i)
    return keys

def block_textures(blk):
    """Texture names in a Dreamcast object block's NJTL (they name the character: tc0023xx = Dyne)."""
    p=blk.find(b'NJTL')
    if p<0: return []
    size=struct.unpack_from('<I',blk,p+4)[0]; c=blk[p+8:p+8+size]
    lst,cnt=struct.unpack_from('<II',c,0); names=[]
    for i in range(min(cnt,256)):
        q=lst+12*i
        if q+4>len(c): break
        np_=struct.unpack_from('<I',c,q)[0]; e=c.find(b'\0',np_)
        if 0<=np_<len(c) and e>np_: names.append(c[np_:e].decode('latin1'))
    return names

def block_textures_be(blk):
    """block_textures for a GameCube (big-endian) block."""
    p=blk.find(b'NJTL')
    if p<0: return []
    size=struct.unpack_from('>I',blk,p+4)[0]; c=blk[p+8:p+8+size]
    lst,cnt=struct.unpack_from('>II',c,0); names=[]
    for i in range(min(cnt,256)):
        q=lst+12*i
        if q+4>len(c): break
        np_=struct.unpack_from('>I',c,q)[0]; e=c.find(b'\0',np_)
        if 0<=np_<len(c) and e>np_: names.append(c[np_:e].decode('latin1'))
    return names

def owners(names):
    return sorted({n[:6] for n in names})

def obj_count(m,i):
    p=m.u(m.entries()[i]+0x14)
    return m.u(p) if p else 0

# Only character textures are taken from the Japanese disc: same-name area textures (tt*) differ
# by re-encoding or carry translated text.
CONTENT_PREFIXES=('tc',)

def sha1(b): return hashlib.sha1(b).hexdigest()

def run(jpf, usf, gcf, outf, raw=False, log=print, allow=None, ops=None):
    """ops (a list) receives the edits in order, for the manifest the recomp applies."""
    ops=[] if ops is None else ops
    jp=Mld(open(jpf,'rb').read(),True); us=Mld(open(usf,'rb').read(),True)
    g=open(gcf,'rb').read(); gc=Mld(decompress(g) if g[:4]==b'AKLZ' else g,False)
    kj,ku,kg=entry_keys(jp),entry_keys(us),entry_keys(gc)
    if not set(ku)<=set(kj) or any(len(kj[k])!=len(v) for k,v in ku.items()):
        log('  the US Dreamcast file has entries the Japanese one lacks, skipped'); return 0
    # Entries only the Japanese file has (e.g. the drunk sailor in the Sailors' Island tavern) need
    # script work, not a swap; the shared ones are matched by key, as indices can differ.
    jidx={}
    for key,idx in ku.items():
        for n,i in enumerate(idx): jidx[i]=kj[key][n]
    pairs=[]
    for key,idx in ku.items():
        gi=kg.get(key)
        if not gi or len(gi)!=len(idx):
            log(f"  entry {key[0]:#x} {key[1].split(bytes(1))[0].decode()}: not matched on the GameCube, skipped"); continue
        if len(idx)==1:
            pairs.append((idx[0],gi[0])); continue
        # Same id and name more than once (e.g. identical NPCs), and the GameCube orders entries
        # differently: pair each one with the GameCube entry whose first object is its own.
        def first(m,i,conv):
            if not obj_count(m,i) or not m.u(m.u(m.entries()[i]+0x14)+4): return None
            a,s=m.obj_block(i,0); b=bytes(m.d[a:a+s]); return block_to_gc(b) if conv else b
        free=list(gi)
        for i in idx:
            want=first(us,i,True)
            hit=[x for x in free if want is not None and first(gc,x,False)==want]
            j=hit[0] if hit else free[0]
            free.remove(j); pairs.append((i,j))
    changed=0; needed=set()
    for i,gi in sorted(pairs):
      ji=jidx[i]
      if obj_count(us,i)!=obj_count(gc,gi) or obj_count(jp,ji)!=obj_count(us,i):
        log(f'  entry {i}: object counts differ, skipped'); continue
      for k in range(obj_count(us,i)):
        if not us.u(us.u(us.entries()[i]+0x14)+4+4*k): continue
        a,asz=jp.obj_block(ji,k); b,bsz=us.obj_block(i,k); c,csz=gc.obj_block(gi,k)
        jb=bytes(jp.d[a:a+asz]); ub=bytes(us.d[b:b+bsz])
        if jb==ub: continue
        if block_to_gc(jb)==bytes(gc.d[c:c+csz]):
            log(f'  entry {i} slot {k}: shares a block already replaced'); continue
        if block_to_gc(ub)!=bytes(gc.d[c:c+csz]):
            log(f'  entry {i} slot {k}: GameCube block is not the US Dreamcast one, skipped'); continue
        who=owners(block_textures(ub)+block_textures(jb))
        # Besides characters: models that use a texture the US localisation added (the tavern sign's
        # chicken leg, tt04a500/501), which the Japanese model doesn't.
        added=set(block_textures(ub))-set(r[0] for r in jp.tex_records())
        if not any(w.startswith(CONTENT_PREFIXES) for w in who) and not added:
            log(f'  entry {i} slot {k}: model differs but names no character {who}, skipped'); continue
        if allow is not None and not set(who)&allow:
            log(f'  entry {i} slot {k}: model differs, not on the list {who}, skipped'); continue
        ops.append({'op':'model','gc_entry':gi,'slot':k,'gc_sha1':sha1(bytes(gc.d[c:c+csz])),'jp_entry':ji,'jp_sha1':sha1(jb)})
        gc.replace(c,csz,block_to_gc(jb)); changed+=1; needed|=set(block_textures(jb))
        log(f'  entry {i} slot {k}: model replaced ({bsz:#x} -> {asz:#x}) {who}')
    jt={r[0]:bytes(jp.d[r[2]:r[2]+r[3]]) for r in jp.tex_records()}
    ut={r[0]:bytes(us.d[r[2]:r[2]+r[3]]) for r in us.tex_records()}
    order=[r[0] for r in jp.tex_records()]
    for idx,name in enumerate(order):
        blob=jt[name]
        if name in ut and ut[name]==blob: continue
        if name in ut and not name.startswith(CONTENT_PREFIXES): continue
        if allow is not None and name[:6] not in allow:
            log(f'  texture {name}: differs, not on the list, skipped'); continue
        if not name.startswith(CONTENT_PREFIXES):
            if name not in ut: log(f'  texture {name}: Japanese-only, not a character texture, skipped')
            continue
        p=blob.find(b'PVRT'); img,info=decode(blob[p:])
        q=blob.find(b'GBIX'); gv=new_cmpr(img, struct.unpack_from('<I',blob,q+8)[0] if q>=0 else 0)
        gcnames=[r[0] for r in gc.tex_records()]
        if name in ut:
            if name not in gcnames: log(f'  texture {name}: not in the GameCube file, skipped'); continue
            orig=[bytes(gc.d[r[2]:r[2]+r[3]]) for r in gc.tex_records() if r[0]==name][0]
            ops.append({'op':'tex','name':name,'gc_sha1':sha1(orig),'jp_sha1':sha1(blob)})
            tex_replace(gc,name,reencode_like(orig,img)); log(f'  texture {name}: replaced ({info[2]}x{info[3]})')
        else:
            if name not in needed:
                log(f'  texture {name}: Japanese-only, no swapped model uses it, skipped'); continue
            prev=[n for n in order[:idx] if n in gcnames]
            pr=[r for r in gc.tex_records() if r[0]==prev[-1]][0][1]
            rec=name.encode().ljust(0x20,b'\0')+bytes(gc.d[pr+0x20:pr+0x28])+struct.pack('>I',len(gv))
            ops.append({'op':'tex_add','name':name,'after':prev[-1],'jp_sha1':sha1(blob)})
            gc.tex_insert_after(prev[-1],rec,gv); log(f'  texture {name}: added ({info[2]}x{info[3]})')
        changed+=1
    # Textures the US localisation added that no model uses any more (the chicken-leg sign once the
    # Japanese house is in): zero their pixels, keeping headers and sizes, so the file compresses
    # into the original's size.
    if changed:
        used=set()
        for gi in range(len(gc.entries())):
            for k in range(obj_count(gc,gi)):
                if gc.u(gc.u(gc.entries()[gi]+0x14)+4+4*k):
                    a,s=gc.obj_block(gi,k); used|=set(block_textures_be(bytes(gc.d[a:a+s])))
        for name in sorted(set(r[0] for r in us.tex_records())-set(r[0] for r in jp.tex_records())):
            recs=[r for r in gc.tex_records() if r[0]==name]
            if not recs or name in used: continue
            dp,sz=recs[0][2],recs[0][3]; p=bytes(gc.d[dp:dp+sz]).find(b'GVRT')
            if p<0: continue
            gc.d[dp+p+16:dp+sz]=bytes(sz-p-16)
            ops.append({'op':'tex_blank','name':name}); log(f'  texture {name}: unused now, blanked')
    if changed==0:
        return 0
    data=bytes(gc.d)
    # Same compressed size as the original, so the disc layout (and save states' copy of it) holds.
    open(outf,'wb').write(data if raw else compress(data, size=len(g) if g[:4]==b'AKLZ' else None))
    return changed

if __name__=='__main__':
    a=sys.argv[1:]
    if len(a)<4: sys.exit(__doc__)
    print(run(a[0],a[1],a[2],a[3],'--raw' in a),'changes')
