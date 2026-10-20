"""Convert a DC MLD object block (wrapper + NJTL + POF0 + NJCM + POF0) to GC byte order."""
import struct,sys,os
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from nj2gc import convert
def sw32(b): return b''.join(b[i:i+4][::-1] for i in range(0,len(b),4))
def block_to_gc(blk):
    out=bytearray(blk)
    out[0:16]=sw32(blk[0:16])
    p=16
    while p+8<=len(blk):
        tag=blk[p:p+4]; size=struct.unpack_from('<I',blk,p+4)[0]
        if tag not in (b'NJTL',b'NJCM',b'POF0'):
            p+=4; continue
        out[p+4:p+8]=struct.pack('>I',size)
        body=blk[p+8:p+8+size]
        if tag==b'NJTL':
            lst,cnt=struct.unpack_from('<II',body,0)
            nb=bytearray(body); nb[0:8]=sw32(body[0:8])
            for i in range(cnt): q=lst+12*i; nb[q:q+12]=sw32(body[q:q+12])
            out[p+8:p+8+size]=nb
        elif tag==b'NJCM':
            o,_=convert(body).result(); out[p+8:p+8+size]=o
        p+=8+size; p=(p+3)&~3
    return bytes(out)
