# frames with many pure-green (YUV-zero style) or magenta pixels, per frame
import subprocess, sys
v=sys.argv[1]; W,H=160,90; N=W*H*3
raw=subprocess.run(["ffmpeg","-v","error","-i",v,"-fps_mode","passthrough","-vf",f"scale={W}:{H}","-f","rawvideo","-pix_fmt","rgb24","-"],capture_output=True,check=True).stdout
n=len(raw)//N; g=[]; m=[]; c=[]; y=[]
for f in range(n):
    b=raw[f*N:(f+1)*N]; gg=mm=cc=yy=0
    for i in range(0,N,3):
        r,gr,bl=b[i],b[i+1],b[i+2]
        if gr>100 and r<60 and bl<60: gg+=1
        elif r>150 and bl>150 and gr<80: mm+=1
        elif r<60 and gr>150 and bl>150: cc+=1
        elif r>150 and gr>150 and bl<80: yy+=1
    if gg>200: g.append((f,gg))
    if mm>200: m.append((f,mm))
    if cc>200: c.append((f,cc))
    if yy>200: y.append((f,yy))
print(n,"frames;",len(g),"green:",[x[0] for x in g][:30]); print(len(m),"magenta:",m[:12]); print(len(c),"cyan:",c[:12]); print(len(y),"yellow:",y[:12])
