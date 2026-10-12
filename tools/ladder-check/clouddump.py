# clouddump.py DIR [DELTA]: over a Windows dump (fN-real.png / fN-interp.png), the cloud-sea region's brightness
# (bottom-left 600x200 of a 1920x1080 picture, scaled with the picture) per picture; prints the pictures darker
# than the median by DELTA (default 25), real and in-between separately. Pure python.
import subprocess, sys, glob, os
d=sys.argv[1]; delta=int(sys.argv[2]) if len(sys.argv)>2 else 25
W,H=60,20; N=W*H
def mean(p):
    raw=subprocess.run(["ffmpeg","-v","error","-i",p,"-vf",f"crop=iw*600/1920:ih*200/1080:0:ih*880/1080,scale={W}:{H}","-f","rawvideo","-pix_fmt","gray","-"],capture_output=True).stdout
    return sum(raw)/N if len(raw)>=N else None
pics=sorted(glob.glob(os.path.join(d,"f*-*.png")), key=lambda p:(int(os.path.basename(p)[1:].split("-")[0]), p))
vals=[(p,mean(p)) for p in pics]
vals=[(p,m) for p,m in vals if m is not None]
if not vals: print("no pictures"); sys.exit(0)
med=sorted(m for p,m in vals)[len(vals)//2]
dark=[(os.path.basename(p),round(m)) for p,m in vals if m<med-delta]
real=[x for x in dark if "real" in x[0]]; interp=[x for x in dark if "interp" in x[0]]
print(f"{len(vals)} pictures; median {med:.1f}; dark real {len(real)}: {real[:20]}; dark in-between {len(interp)}: {interp[:20]}")
