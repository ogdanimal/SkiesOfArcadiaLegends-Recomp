"""Encode RGBA pixels as a GameCube GVR (RGB5A3, mipmapped) blob with a GCIX header."""
import struct
def rgb5a3(p):
    r,g,b,a=p
    if a>=0xe0: return 0x8000|(r>>3)<<10|(g>>3)<<5|(b>>3)
    return (a>>5)<<12|(r>>4)<<8|(g>>4)<<4|(b>>4)
def tiles(img):
    h=len(img); w=len(img[0]); out=bytearray()
    for ty in range(0,max(h,4),4):
        for tx in range(0,max(w,4),4):
            for y in range(4):
                for x in range(4):
                    yy=min(ty+y,h-1); xx=min(tx+x,w-1)
                    out+=struct.pack('>H',rgb5a3(img[yy][xx]))
    return bytes(out)
def half(img):
    h=len(img); w=len(img[0])
    if w==1 and h==1: return None
    nw,nh=max(1,w//2),max(1,h//2); out=[]
    for y in range(nh):
        row=[]
        for x in range(nw):
            ps=[img[min(2*y+dy,h-1)][min(2*x+dx,w-1)] for dy in (0,1) for dx in (0,1)]
            row.append(tuple(sum(p[i] for p in ps)//4 for i in range(4)))
        out.append(row)
    return out
def encode(img, mips=True):
    h=len(img); w=len(img[0]); data=bytearray(); lvl=img
    while lvl is not None:
        data+=tiles(lvl)
        if not mips: break
        lvl=half(lvl)
    gvrt=bytes(b'\0\0')+bytes([0x01 if mips else 0x00, 0x05])+struct.pack('>HH',w,h)+bytes(data)
    blob=b'GCIX'+struct.pack('<I',8)+b'\0'*8+b'GVRT'+struct.pack('<I',len(gvrt))+gvrt
    while len(blob)%32: blob+=b'\0'
    return blob

# --- CMPR (GameCube DXT1) -------------------------------------------------------------------------
def _565(c): return (c[0]>>3)<<11|(c[1]>>2)<<5|(c[2]>>3)
def _rgb(v): r=(v>>11)&31; g=(v>>5)&63; b=v&31; return ((r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2))
def _block(px):
    """16 RGBA pixels (row-major 4x4) -> 8 bytes of CMPR (big-endian colours, MSB-first indices)."""
    clear=[p[3]<0x80 for p in px]; solid=[p for p,c in zip(px,clear) if not c]
    if not solid: return struct.pack('>HH',0,0xFFFF)+b'\xff'*4
    mean=[sum(p[i] for p in solid)/len(solid) for i in range(3)]
    axis=[1.0,1.0,1.0]
    for _ in range(8):  # power iteration on the colour covariance
        v=[0.0,0.0,0.0]
        for p in solid:
            d=[p[i]-mean[i] for i in range(3)]; t=sum(d[i]*axis[i] for i in range(3))
            for i in range(3): v[i]+=d[i]*t
        n=max(abs(x) for x in v) or 1.0; axis=[x/n for x in v]
    proj=[sum((p[i]-mean[i])*axis[i] for i in range(3)) for p in solid]
    lo=solid[proj.index(min(proj))]; hi=solid[proj.index(max(proj))]
    a,b=_565(hi),_565(lo)
    if any(clear):
        a,b=min(a,b),max(a,b)  # 3-colour mode (c0 <= c1): index 3 is transparent
        ca,cb=_rgb(a),_rgb(b); pal=[ca,cb,tuple((x+y)//2 for x,y in zip(ca,cb))]
    else:
        if a==b: return struct.pack('>HH',a,a)+b'\0'*4
        if a<b: a,b=b,a
        ca,cb=_rgb(a),_rgb(b)
        pal=[ca,cb,tuple((2*x+y)//3 for x,y in zip(ca,cb)),tuple((x+2*y)//3 for x,y in zip(ca,cb))]
    idx=[]
    for p,c in zip(px,clear):
        if c: idx.append(3); continue
        idx.append(min(range(len(pal)),key=lambda k:sum((p[i]-pal[k][i])**2 for i in range(3))))
    rows=bytes(idx[4*y]<<6|idx[4*y+1]<<4|idx[4*y+2]<<2|idx[4*y+3] for y in range(4))
    return struct.pack('>HH',a,b)+rows
def cmpr(img):
    """One level: 8x8 tiles row-major, each four 4x4 sub-blocks (TL, TR, BL, BR); edges repeat."""
    h=len(img); w=len(img[0]); out=bytearray()
    for ty in range(0,max(h,8),8):
        for tx in range(0,max(w,8),8):
            for sy,sx in ((0,0),(0,4),(4,0),(4,4)):
                out+=_block([img[min(ty+sy+y,h-1)][min(tx+sx+x,w-1)] for y in range(4) for x in range(4)])
    return bytes(out)
def reencode_like(orig, img):
    """New pixels in the original blob's own format and mip count; headers and size kept."""
    p=orig.find(b'GVRT'); flags,fmt=orig[p+10]&0x0F,orig[p+11]; w,h=struct.unpack_from('>HH',orig,p+12)
    if (w,h)!=(len(img[0]),len(img)): raise ValueError(f'size {len(img[0])}x{len(img)} vs {w}x{h}')
    if fmt in (0x08,0x09):
        if not flags&8: raise ValueError('external palette not supported')
        palbytes,data=paletted(img, flags&1, orig[p+10]>>4, 4 if fmt==0x08 else 8)
        start=p+16; end=start+len(palbytes)+len(data)
        if end>len(orig): raise ValueError('encoded data longer than the original')
        return orig[:start]+palbytes+data+orig[end:]
    level={0x0E:cmpr,0x05:tiles}.get(fmt)
    if level is None: raise ValueError(f'GVR format {fmt:#x} not supported')
    data=bytearray(); lvl=img
    while lvl is not None:
        data+=level(lvl)
        if not flags&1: break
        lvl=half(lvl)
    start=p+16; end=start+len(data)
    if end>len(orig): raise ValueError('encoded data longer than the original')
    return orig[:start]+bytes(data)+orig[end:]

# --- C4 / C8 (paletted) ---------------------------------------------------------------------------
def _quantize(pixels, n):
    """The exact colours when there are at most n, else median cut weighted by pixel count."""
    uniq=sorted(set(pixels))
    if len(uniq)<=n: return uniq
    boxes=[list(pixels)]
    while len(boxes)<n:
        boxes.sort(key=lambda b: max(max(p[i] for p in b)-min(p[i] for p in b) for i in range(4)) if len(b)>1 else -1)
        b=boxes.pop()
        if len(set(b))<2: boxes.append(b); break
        ch=max(range(4),key=lambda i: max(p[i] for p in b)-min(p[i] for p in b))
        b.sort(key=lambda p:p[ch]); m=len(b)//2; boxes+= [b[:m],b[m:]]
    return [tuple(sum(p[i] for p in b)//len(b) for i in range(4)) for b in boxes if b]
def _pal_word(c, fmt):
    r,g,b,a=c
    if fmt==1: return (r>>3)<<11|(g>>2)<<5|(b>>3)
    if fmt==2: return rgb5a3(c)
    if fmt==0: return (a<<8)|((r*30+g*59+b*11)//100)
    raise ValueError(f'palette format {fmt}')
def _pal_rgba(w, fmt):
    if fmt==1: return _rgb(w)+(255,)
    if fmt==2:
        if w&0x8000: return (((w>>10)&31)*255//31,((w>>5)&31)*255//31,(w&31)*255//31,255)
        return (((w>>8)&15)*17,((w>>4)&15)*17,(w&15)*17,((w>>12)&7)*255//7)
    return (w&255,)*3+(w>>8,)
def paletted(img, levels_flag, fmt, bits):
    """(palette bytes, pixel data) for C4 (bits 4, tiles 8x8) or C8 (bits 8, tiles 8x4), all mips."""
    lv=[img]
    while levels_flag and lv[-1] is not None: lv.append(half(lv[-1]))
    lv=[l for l in lv if l is not None]
    pal=_quantize([p for row in img for p in row], 1<<bits)  # top level; mips map to it
    pal_rgba=[_pal_rgba(_pal_word(c,fmt),fmt) for c in pal]
    cache={}
    def idx(p):
        if p not in cache: cache[p]=min(range(len(pal_rgba)),key=lambda k: sum((p[i]-pal_rgba[k][i])**2 for i in range(4)))
        return cache[p]
    tw,th=(8,8) if bits==4 else (8,4); data=bytearray()
    for l in lv:
        h=len(l); w=len(l[0])
        for ty in range(0,max(h,th),th):
            for tx in range(0,max(w,tw),tw):
                vals=[idx(l[min(ty+y,h-1)][min(tx+x,w-1)]) for y in range(th) for x in range(tw)]
                if bits==4: data+=bytes(vals[i]<<4|vals[i+1] for i in range(0,len(vals),2))
                else: data+=bytes(vals)
    palbytes=b''.join(struct.pack('>H',_pal_word(c,fmt)) for c in pal)+bytes(2*((1<<bits)-len(pal)))
    return palbytes, bytes(data)

def new_cmpr(img, index):
    """A new GCIX/GVRT texture (CMPR, mips down to 1x1) for a texture the GameCube file lacks;
    index = the Dreamcast GBIX index, which the GameCube files keep in GCIX."""
    data=bytearray(); lvl=img
    while lvl is not None:
        data+=cmpr(lvl); lvl=half(lvl)
    gvrt=bytes([0,0,0x01,0x0E])+struct.pack('>HH',len(img[0]),len(img))+bytes(data)
    blob=b'GCIX'+struct.pack('<I',8)+struct.pack('>II',index,0)+b'GVRT'+struct.pack('<I',len(gvrt))+gvrt
    while len(blob)%32: blob+=b'\0'
    return blob
