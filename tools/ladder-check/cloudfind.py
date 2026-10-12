# cloudfind.py VIDEO [DELTA]: frames whose cloud-sea region (bottom-left, below the ladder) is darker than
# the video's median by DELTA (default 25) -> the two-frame "dark blocky garbage" cloud sea (NOTES, 2026-10-10).
# Region: 600x200 at (0,880) of 1080p, scaled with the video. Pure python (no numpy on WSL).
import subprocess, sys
v=sys.argv[1]; delta=int(sys.argv[2]) if len(sys.argv)>2 else 25
W,H=60,20; N=W*H
raw=subprocess.run(["ffmpeg","-v","error","-i",v,"-fps_mode","passthrough",
    "-vf",f"crop=iw*600/1920:ih*200/1080:0:ih*880/1080,scale={W}:{H}","-f","rawvideo","-pix_fmt","gray","-"],
    capture_output=True,check=True).stdout
n=len(raw)//N
mean=[sum(raw[f*N:(f+1)*N])/N for f in range(n)]
med=sorted(mean)[n//2]
hits=[f for f in range(n) if mean[f]<med-delta]
# group consecutive frames into events
ev=[]; 
for f in hits:
    if ev and f==ev[-1][-1]+1: ev[-1].append(f)
    else: ev.append([f])
print(f"{n} frames; median {med:.1f}; {len(hits)} dark frames in {len(ev)} events:",
      [(e[0],len(e),round(min(mean[x] for x in e))) for e in ev][:40])
