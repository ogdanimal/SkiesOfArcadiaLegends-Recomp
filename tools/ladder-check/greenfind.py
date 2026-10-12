import subprocess, sys
v=sys.argv[1]; W,H=160,90
raw=subprocess.run(["ffmpeg","-v","error","-i",v,"-fps_mode","passthrough","-vf",f"scale={W}:{H}","-f","rawvideo","-pix_fmt","rgb24","-"],capture_output=True,check=True).stdout
n=len(raw)//(W*H*3); out=[]
for f in range(n):
    b=raw[f*W*H*3:(f+1)*W*H*3]; g=0
    for i in range(0,len(b),3):
        if b[i+1]>100 and b[i]<60 and b[i+2]<60: g+=1
    if g>200: out.append((f,g))
print(n,"frames;",len(out),"green:",out[:60])
