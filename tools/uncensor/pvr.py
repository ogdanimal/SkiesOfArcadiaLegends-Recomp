import struct, zlib
VQ_MIP_OFF={1:0,2:1,4:2,8:6,16:0x16,32:0x56,64:0x156,128:0x556,256:0x1556,512:0x5556,1024:0x15556}
def tw(x,y):
    r=0;i=0
    while (1<<i) <= max(x,y):
        r|=((y>>i)&1)<<(2*i) | ((x>>i)&1)<<(2*i+1); i+=1
    return r
def px(v,pf):
    if pf==0: a=255 if v&0x8000 else 0; r=(v>>10)&31; g=(v>>5)&31; b=v&31; return (r*255//31,g*255//31,b*255//31,a)
    if pf==1: r=(v>>11)&31; g=(v>>5)&63; b=v&31; return (r*255//31,g*255//63,b*255//31,255)
    if pf==2: return (((v>>8)&15)*17,((v>>4)&15)*17,(v&15)*17,((v>>12)&15)*17)
    return (255,0,255,255)
def decode(chunk):
    """chunk = bytes starting at 'PVRT'"""
    size=struct.unpack('<I',chunk[4:8])[0]; pf,df=chunk[8],chunk[9]; w,h=struct.unpack('<HH',chunk[12:16])
    d=chunk[16:8+size]
    img=[[(0,0,0,0)]*w for _ in range(h)]
    if df in (3,4,0x10,0x11):
        if df in (3,4): ncb=256
        else:
            mip= df==0x11
            if w<=16: ncb=16
            elif w==32: ncb=64 if mip else 32
            elif w==64: ncb=256 if mip else 128
            else: ncb=256
        cb=[struct.unpack('<4H',d[i*8:i*8+8]) for i in range(ncb)]
        off=ncb*8 + (VQ_MIP_OFF[w] if df in (4,0x11) else 0)
        idx=d[off:]
        for y in range(h//2):
            for x in range(w//2):
                e=cb[idx[tw(x,y)] % ncb]
                img[2*y][2*x]=px(e[0],pf); img[2*y+1][2*x]=px(e[1],pf); img[2*y][2*x+1]=px(e[2],pf); img[2*y+1][2*x+1]=px(e[3],pf)
    elif df in (1,2):
        top=d[len(d)-w*h*2:] if df==2 else d
        for y in range(h):
            for x in range(w):
                o=tw(x,y)*2; img[y][x]=px(struct.unpack('<H',top[o:o+2])[0],pf)
    elif df==9:
        for y in range(h):
            for x in range(w):
                o=(y*w+x)*2; img[y][x]=px(struct.unpack('<H',d[o:o+2])[0],pf)
    else:
        raise ValueError(f'PVR data format {df:#x} not supported')
    if pf not in (0,1,2):
        raise ValueError(f'PVR pixel format {pf:#x} not supported')
    return img,(pf,df,w,h)
def png(path, img, scale=1, bg=(40,40,40)):
    h=len(img); w=len(img[0]); rows=[]
    for y in range(h*scale):
        r=bytearray([0])
        for x in range(w*scale):
            p=img[y//scale][x//scale]; a=p[3]/255
            r+=bytes(int(p[i]*a+bg[i]*(1-a)) for i in range(3))
        rows.append(bytes(r))
    raw=b''.join(rows)
    def ch(t,dat): return struct.pack('>I',len(dat))+t+dat+struct.pack('>I',zlib.crc32(t+dat)&0xffffffff)
    open(path,'wb').write(b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack('>IIBBBBB',w*scale,h*scale,8,2,0,0,0))+ch(b'IDAT',zlib.compress(raw,6))+ch(b'IEND',b''))
def montage(imgs, gap=4):
    H=max(len(i) for i in imgs); W=sum(len(i[0]) for i in imgs)+gap*(len(imgs)-1)
    out=[[(255,255,255,255)]*W for _ in range(H)]; x0=0
    for im in imgs:
        for y,row in enumerate(im):
            out[y][x0:x0+len(row)]=row
        x0+=len(im[0])+gap
    return out
