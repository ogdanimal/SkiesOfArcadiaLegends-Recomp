#!/usr/bin/env python3
"""Drive a running moderngekko-run through its automation directory (--automation-dir).

Each command is a key=value file dropped into <dir>/commands/; the runtime processes them in
name order and reports progress in <dir>/status.txt.

usage: AUTO=build/auto tools/drive.py STEP [STEP...]
  wait:SECONDS            wall-clock wait
  shot:PATH               screenshot (PNG), waits until the file is fully written
  tap:BUTTON[:FRAMES]     press for FRAMES (default 6): a b x y z start l r dpad_up ...
  hold:k=v,k=v:FRAMES     arbitrary pad fields, e.g. hold:main_x=1:20 (sticks are -1..1)
  press:k=v,k=v           set pad fields and keep them until release (other commands still run)
  release                 let go of everything press set
  save:PATH / load:PATH   savestate
  peek:ADDR:SIZE          print SIZE bytes of guest memory at ADDR (hex), with floats
  poke:ADDR:HEX           write bytes (hex string) to guest memory at ADDR
  pokef:ADDR:FLOAT[,...]  write big-endian floats to guest memory at ADDR
  config:SYS.SECTION.KEY=V  set a Dolphin config value live (SYS = Dolphin, Graphics, GCPad, ...: Dolphin's names, not the .ini files'), e.g. config:Dolphin.Skies.Widescreen=0
  stop                    shut the runtime down cleanly
  status                  print the interesting status.txt fields
  speed:SECONDS           sample status once a second; print mean/min/max speed and fps
                          (SPEED_TRACE=1 also prints every sample)

Android: set ADB_AUTO to the app's automation directory on the device
(/sdcard/Android/data/org.dolphinemu.dolphinemu.debug/files/Automation, created to opt in).
Commands are pushed with adb; load: pushes a local savestate first, and save:/shot:/peek: pull the
result back to the local PATH. ADB (adb executable) and SERIAL select the device.
"""
import itertools
import os
import shlex
import struct
import subprocess
import sys
import tempfile
import time

A = os.environ.get("AUTO", "build/auto")
ADB_AUTO = os.environ.get("ADB_AUTO")
ADB = os.environ.get("ADB", "adb.exe")  # Windows' adb from WSL
SERIAL = os.environ.get("SERIAL")  # only needed with several devices
seq = itertools.count(int(time.time() * 1000))


def q(text):
    """Quoted for the device shell (adb shell joins its arguments into one command line)."""
    return shlex.quote(text)


def adb(*args, **kw):
    return subprocess.run([ADB, *(["-s", SERIAL] if SERIAL else []), *args], check=True, capture_output=True, **kw)


def host_path(path):
    """A local path as a Windows adb.exe (run from WSL) sees it."""
    if ADB.endswith(".exe"):
        return subprocess.run(["wslpath", "-w", path], check=True, capture_output=True,
                              text=True).stdout.strip()
    return path


def status():
    try:
        if ADB_AUTO:
            text = adb("shell", f"cat {q(ADB_AUTO + '/status.txt')} 2>/dev/null || true", text=True).stdout
        else:
            text = open(os.path.join(A, "status.txt")).read()
    except FileNotFoundError:
        return {}
    return dict(line.split("=", 1) for line in text.splitlines() if "=" in line)


def send(**kv):
    before = int(status().get("processed_commands", 0))
    name = f"{next(seq)}.txt"
    body = "".join(f"{k}={v}\n" for k, v in kv.items())
    if ADB_AUTO:
        with tempfile.NamedTemporaryFile("w", suffix=".txt", dir="/tmp", delete=False) as f:
            f.write(body)
        adb("push", host_path(f.name), f"{ADB_AUTO}/{name}.tmp")
        os.unlink(f.name)
        adb("shell", f"mv {q(f'{ADB_AUTO}/{name}.tmp')} {q(f'{ADB_AUTO}/commands/{name}')}")
    else:
        tmp = os.path.join(A, name + ".tmp")
        with open(tmp, "w") as f:
            f.write(body)
        os.rename(tmp, os.path.join(A, "commands", name))
    for _ in range(600):
        s = status()
        if int(s.get("processed_commands", 0)) > before:
            if s.get("last_error"):
                print("error:", s["last_error"])
            return s
        time.sleep(0.05)
    print("timeout waiting for", kv)
    return {}


def runtime_path(path):
    """The path as the runtime sees it: a Windows runtime (driven from WSL) needs C:\\... paths."""
    parts = path.split("/")
    if len(parts) > 3 and parts[1] == "mnt" and len(parts[2]) == 1:
        return parts[2].upper() + ":\\" + "\\".join(parts[3:])
    return path


def device_size(path):
    out = adb("shell", f"stat -c %s {q(path)} 2>/dev/null || echo -1", text=True).stdout
    return int(out.split()[0])


def wait_written(path, device=False):
    last = -1
    for _ in range(200):
        if device:
            size = device_size(path)
        else:
            size = os.path.getsize(path) if os.path.exists(path) else -1
        if size > 0 and size == last:
            return
        last = size
        time.sleep(0.1)


for step in sys.argv[1:]:
    op, _, arg = step.partition(":")
    if op == "wait":
        time.sleep(float(arg))
    elif op == "shot" and ADB_AUTO:
        path = os.path.abspath(arg)
        remote = f"{ADB_AUTO}/out/{os.path.basename(path)}"
        send(command="screenshot", path=remote)
        wait_written(remote, device=True)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        adb("pull", remote, host_path(path))
        print("shot", path)
    elif op == "shot":
        path = os.path.abspath(arg)
        send(command="screenshot", path=runtime_path(path))
        wait_written(path)
        print("shot", path)
    elif op in ("save", "load") and ADB_AUTO:
        path = os.path.abspath(arg)
        remote = f"{ADB_AUTO}/states/{os.path.basename(path)}"
        if op == "load":
            adb("shell", f"mkdir -p {q(ADB_AUTO + '/states')}")
            if device_size(remote) != os.path.getsize(path):
                adb("push", host_path(path), remote)
        send(command=op + "_state", path=remote)
        time.sleep(2)
        if op == "save":
            wait_written(remote, device=True)
            adb("pull", remote, host_path(path))
            print("saved", path)
    elif op == "tap":
        button, _, frames = arg.partition(":")
        frames = int(frames or 6)
        send(command="pad_frames", port=0, frames=frames, **{button: 1})
        time.sleep(frames / 60 + 0.15)
    elif op == "hold":
        fields, _, frames = arg.rpartition(":")
        send(command="pad_frames", port=0, frames=frames, **dict(p.split("=") for p in fields.split(",")))
        time.sleep(int(frames) / 60 + 0.15)
    elif op == "press":
        send(command="pad", port=0, **dict(p.split("=") for p in arg.split(",")))
        time.sleep(0.1)
    elif op == "release":
        send(command="clear_pad", port=0)
        time.sleep(0.1)
    elif op in ("save", "load"):
        send(command=op + "_state", path=runtime_path(os.path.abspath(arg)))
        time.sleep(2)
    elif op == "peek":
        addr, _, size = arg.partition(":")
        if ADB_AUTO:
            # The device writes the bytes under the automation directory; pull them back.
            remote = f"{ADB_AUTO}/out/peek.bin"
            adb("shell", f"rm -f {q(remote)}")
            send(command="read_memory", address="0x" + addr.removeprefix("0x"), size=int(size, 0),
                 path=remote)
            wait_written(remote, device=True)
            with tempfile.TemporaryDirectory() as tmp:
                local = os.path.join(tmp, "peek.bin")
                adb("pull", remote, host_path(local))
                data = open(local, "rb").read()
        else:
            out = os.path.abspath(os.path.join(A, "peek.bin"))
            if os.path.exists(out):
                os.unlink(out)
            send(command="read_memory", address="0x" + addr.removeprefix("0x"), size=int(size, 0),
                 path=runtime_path(out))
            wait_written(out)
            data = open(out, "rb").read()
        base = int(addr, 16)
        for i in range(0, len(data), 16):
            row = data[i:i + 16]
            floats = " ".join(f"{struct.unpack('>f', row[j:j + 4])[0]:.6g}"
                              for j in range(0, len(row) - 3, 4))
            print(f"{base + i:08X}  {row.hex(' ')}  | {floats}")
    elif op == "poke":
        addr, _, data = arg.partition(":")
        send(command="write_memory", address="0x" + addr.removeprefix("0x"), data=data)
    elif op == "pokef":
        addr, _, values = arg.partition(":")
        data = b"".join(struct.pack(">f", float(v)) for v in values.split(","))
        send(command="write_memory", address="0x" + addr.removeprefix("0x"), data=data.hex())
    elif op == "config":
        loc, _, value = arg.partition("=")
        system, section, key = loc.split(".", 2)
        send(command="set_config", system=system, section=section, key=key, value=value)
    elif op == "stop":
        send(command="stop")
    elif op == "status":
        s = status()
        print({k: s.get(k) for k in ("state", "fps", "speed", "frame_count", "last_error")})
    elif op == "speed":
        speeds, fps = [], []
        end = time.time() + float(arg)
        while time.time() < end:
            s = status()
            if s.get("state") == "running":
                speeds.append(float(s.get("speed", 0)))
                fps.append(float(s.get("fps", 0)))
                if os.environ.get("SPEED_TRACE"):
                    print(f"  t={len(speeds)} speed={speeds[-1]:.3f} frame={s.get('frame_count')} "
                          f"wall={time.time():.1f}")
            time.sleep(1)
        if speeds:
            print(f"speed mean={sum(speeds) / len(speeds):.3f} min={min(speeds):.3f} "
                  f"max={max(speeds):.3f} fps mean={sum(fps) / len(fps):.2f} n={len(speeds)}")
        else:
            print("speed: no running samples")
    else:
        sys.exit(f"unknown step: {step}")
