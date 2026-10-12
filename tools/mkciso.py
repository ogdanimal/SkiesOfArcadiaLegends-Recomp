#!/usr/bin/env python3
# usage: tools/mkciso.py GAME.iso GAME.ciso   (to test the import with a CISO; dolphin-tool makes ISO / GCZ / WIA / RVZ)
# ISO -> CISO (Nintendont / Dolphin CISOBlob layout): 0x8000-byte header "CISO", u32 LE block size,
# a map of one byte per block (1 = stored); stored blocks follow in order.
import sys
src, dst, bs = sys.argv[1], sys.argv[2], 0x200000
HEADER = 0x8000
with open(src, 'rb') as f, open(dst, 'wb') as o:
    o.write(b'\0' * HEADER)
    flags = []
    while True:
        block = f.read(bs)
        if not block:
            break
        used = any(block)
        flags.append(1 if used else 0)
        if used:
            o.write(block.ljust(bs, b'\0'))
    assert len(flags) <= HEADER - 8
    o.seek(0)
    o.write(b'CISO' + bs.to_bytes(4, 'little') + bytes(flags))
print(len(flags), 'blocks,', sum(flags), 'stored')
