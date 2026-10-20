"""Splice blocks into an MLD and fix the offsets that point past the splice point."""
import struct
class Mld:
    def __init__(self, data, le):
        self.d=bytearray(data); self.e='<' if le else '>'
    def u(self,p): return struct.unpack_from(self.e+'I',self.d,p)[0]
    def w(self,p,v): struct.pack_into(self.e+'I',self.d,p,v)
    def header(self): return struct.unpack_from(self.e+'5I',self.d,0)
    def entries(self):
        cnt,idx=self.header()[:2]
        return [idx+0x68*i for i in range(cnt)]
    def obj_block(self, entry_index, slot=0):
        e=self.entries()[entry_index]; objp=self.u(e+0x14)
        start=self.u(objp+4+4*slot); size=self.u(start+4)
        return start,size
    def replace(self, start, old_len, new):
        delta=len(new)-old_len
        if delta%4: raise ValueError('delta not 4-aligned')
        # collect pointer slots before mutating
        slots=[]
        cnt,idx,fp,rd,tt=self.header()
        slots+= [8,12,16]
        lists=[]
        for e in self.entries():
            for f in (0x08,0x0c,0x10,0x14,0x18,0x1c,0x20):
                slots.append(e+f)
            for f in (0x08,0x0c,0x14,0x18,0x1c):
                p=self.u(e+f)
                if p: lists.append(p)
        for p in set(lists):
            n=self.u(p)
            slots+= [p+4+4*k for k in range(n)]
        vals={s:self.u(s) for s in set(slots)}
        end=len(self.d)
        self.d[start:start+old_len]=new
        for s,v in vals.items():
            # Lists also hold global texture IDs (0x80000000 | n): only values inside the file are offsets.
            if v>end: continue
            if v>start or (v==start and old_len==0):
                ns=s+delta if s>=start+old_len else s
                self.w(ns, v+delta)
        return delta
    # texture archive
    def tex_data_start(self, tt, n):
        """Field files align the first texture to 32 bytes; battle character files (bchara) don't."""
        s0=tt+4+0x2c*n
        return s0 if bytes(self.d[s0:s0+4]) in (b'GCIX',b'GBIX',b'PVRT',b'GVRT') else (s0+31)&~31
    def tex_aligned(self):
        tt=self.header()[4]; n=self.u(tt); s0=tt+4+0x2c*n
        return self.tex_data_start(tt,n)!=s0 or s0%32==0
    def tex_records(self):
        tt=self.header()[4]; n=self.u(tt); out=[]; data=self.tex_data_start(tt,n)
        for i in range(n):
            r=tt+4+0x2c*i; name=bytes(self.d[r:r+0x20]).split(b'\0')[0].decode(); size=self.u(r+0x28)
            out.append((name,r,data,size)); data+=size
        return out
    def tex_insert_after(self, after_name, record_bytes, tex_bytes):
        recs=self.tex_records(); tt=self.header()[4]
        i=[k for k,r in enumerate(recs) if r[0]==after_name][0]
        end=recs[-1][2]+recs[-1][3]; tail=bytes(self.d[end:])
        table=[bytes(self.d[r:r+0x2c]) for _,r,_,_ in recs]
        blobs=[bytes(self.d[dp:dp+sz]) for _,_,dp,sz in recs]
        aligned=self.tex_aligned()
        table.insert(i+1,record_bytes); blobs.insert(i+1,tex_bytes)
        out=bytearray(struct.pack(self.e+'I',len(table)))+b''.join(table)
        while aligned and (tt+len(out))%32: out+=b'\0'
        out+=b''.join(blobs)
        self.d[tt:]=out+tail

def _rebuild_archive(m, edit):
    """edit(table, blobs) mutates lists of 0x2c-byte records and texture blobs in place."""
    recs=m.tex_records(); tt=m.header()[4]
    end=recs[-1][2]+recs[-1][3]; tail=bytes(m.d[end:])
    table=[bytes(m.d[r:r+0x2c]) for _,r,_,_ in recs]
    blobs=[bytes(m.d[dp:dp+sz]) for _,_,dp,sz in recs]
    aligned=m.tex_aligned()
    edit(table,blobs)
    out=bytearray(struct.pack(m.e+'I',len(table)))+b''.join(table)
    while aligned and (tt+len(out))%32: out+=b'\0'
    out+=b''.join(blobs)
    m.d[tt:]=out+tail

def tex_replace(m, name, blob):
    def edit(table,blobs):
        for i,r in enumerate(table):
            if r[:0x20].split(b'\0')[0].decode()==name:
                rec=bytearray(r); struct.pack_into(m.e+'I',rec,0x28,len(blob)); table[i]=bytes(rec); blobs[i]=blob
    _rebuild_archive(m,edit)
