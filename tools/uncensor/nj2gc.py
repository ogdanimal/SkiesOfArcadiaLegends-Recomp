"""Convert a Dreamcast (little-endian) Ninja NJCM chunk-model payload to the GameCube byte order."""
import struct
class Conv:
    def __init__(self, d):
        self.d=d; self.o=bytearray(len(d)); self.done=bytearray(len(d)); self.notes=[]
    def u32(self,p): return struct.unpack_from('<I',self.d,p)[0]
    def u16(self,p): return struct.unpack_from('<H',self.d,p)[0]
    def sw32(self,p,n=1):
        for i in range(n):
            q=p+4*i; self.o[q:q+4]=self.d[q:q+4][::-1]; self.done[q:q+4]=b'\1'*4
    def sw16(self,p,n=1):
        for i in range(n):
            q=p+2*i; self.o[q:q+2]=self.d[q:q+2][::-1]; self.done[q:q+2]=b'\1'*2
    def keep(self,p,n):
        self.o[p:p+n]=self.d[p:p+n]; self.done[p:p+n]=b'\1'*n
    def obj(self,p,seen=None,root=False):
        seen=seen if seen is not None else set()
        while (p or root) and p not in seen and p+0x34<=len(self.d):
            seen.add(p); root=False
            self.sw32(p,13)
            model,child,sib=self.u32(p+4),self.u32(p+0x2c),self.u32(p+0x30)
            if model: self.model(model)
            if child: self.obj(child,seen)
            p=sib
    def model(self,p):
        if self.done[p]: return
        self.sw32(p,6)
        vl,pl=self.u32(p),self.u32(p+4)
        if vl: self.vlist(vl)
        if pl: self.plist(pl)
    def vlist(self,p):
        while True:
            h=self.u32(p); t=h&0xff
            if t==0xff: self.sw32(p); return  # end word
            size=h>>16
            self.sw32(p,1+size)
            p+=4+size*4
    def plist(self,p):
        while True:
            h=self.u16(p); t=h&0xff
            if t==0xff: self.sw16(p); return
            if t==0: self.sw16(p); p+=2; continue
            if 1<=t<=7: self.sw16(p); p+=2; continue
            if 8<=t<=15: self.sw16(p,2); p+=4; continue
            size=self.u16(p+2); self.sw16(p,2); body=p+4
            if 16<=t<=31:
                # material: colours as BGRA byte quads (diffuse, ambient, specular per flag bits)
                n=0
                for bit in (1,2,4):
                    if t-16 & bit: self.keep(body+4*n,4); n+=1
                if size*2>4*n: self.sw16(body+4*n,(size*2-4*n)//2)
            else:
                self.sw16(body,size)
            p=body+size*2
    def result(self):
        o=bytearray(self.o)
        for i in range(len(o)):
            if not self.done[i]: o[i]=self.d[i]  # alignment padding
        return bytes(o), self.done
def convert(d, roots=(0,)):
    c=Conv(d)
    for r in roots: c.obj(r,root=True)
    return c
