# For each in-between picture fN-interp.png: pixels that differ strongly from BOTH real neighbours (f(N-1)-real, fN-real)
# while those agree -> garbage. Prints the worst frames. Pure python over 96x54 downscales.
import subprocess, sys, glob, os
d=sys.argv[1]; W,H=96,54; N=W*H
def load(p):
    return subprocess.run(["ffmpeg","-v","error","-i",p,"-vf",f"scale={W}:{H}","-f","rawvideo","-pix_fmt","gray","-"],capture_output=True).stdout
frames=sorted(int(os.path.basename(p)[1:].split("-")[0]) for p in glob.glob(os.path.join(d,"f*-interp.png")))
real={}; res=[]
for n in frames:
    for k in (n-1,n):
        if k not in real and os.path.exists(f"{d}/f{k}-real.png"): real[k]=load(f"{d}/f{k}-real.png")
    if n-1 not in real or n not in real: continue
    a,c,b=real[n-1],real[n],load(f"{d}/f{n}-interp.png")
    if len(b)<N: continue
    bad=sum(1 for i in range(N) if abs(b[i]-a[i])>40 and abs(b[i]-c[i])>40 and abs(a[i]-c[i])<16)
    res.append((bad,n))
res.sort(reverse=True)
print(len(res),"in-betweens scored; over 50 bad px:",sum(1 for b,n in res if b>50),"; worst:",res[:8])
