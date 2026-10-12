#!/usr/bin/env python3
"""Export named functions from the decomp's symbols.txt as a DolRecomp symbol map.

DolRecomp's --map accepts "address size name" lines. Only functions with real names are written
(dtk's placeholder fn_XXXXXXXX names add nothing).

Usage: python tools/export_map.py [../SkiesOfArcadiaLegends-Decomp] > build/skies.map
"""
import re
import sys
from pathlib import Path

decomp = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / "SkiesOfArcadiaLegends-Decomp")
for line in (decomp / "config/GEAE8P/symbols.txt").read_text().splitlines():
    m = re.match(r"(\S+) = \.(text|init):0x([0-9A-F]{8}); // type:function size:0x([0-9A-F]+)", line)
    if m and not m.group(1).startswith("fn_"):
        print(f"{m.group(3)} {int(m.group(4), 16):08X} {m.group(1)}")
