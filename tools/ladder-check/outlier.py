# frames whose pixels differ strongly from BOTH neighbours while the neighbours agree (single-frame garbage)
import subprocess, sys
v=sys.argv[1]; th=int(sys.argv[2]) if len(sys.argv)>2 else 150; W,H=96,54; N=W*H
raw=subprocess.run(["ffmpeg","-v","error","-i",v,"-fps_mode","passthrough","-vf",f"scale={W}:{H}","-f","rawvideo","-pix_fmt","gray","-"],capture_output=True,check=True).stdout
n=len(raw)//N; hits=[]
for f in range(1,n-1):
    a=raw[(f-1)*N:f*N]; b=raw[f*N:(f+1)*N]; c=raw[(f+1)*N:(f+2)*N]
    bad=sum(1 for i in range(N) if abs(b[i]-a[i])>40 and abs(b[i]-c[i])>40 and abs(a[i]-c[i])<16 and i%W<80)
    if bad>th: hits.append((f,bad))
print(n,"frames;",len(hits),"single-frame outliers:",hits[:40])
