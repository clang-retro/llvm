# Writes test programs for llvm-exe2cmd: mk.py KIND OUT
import struct, sys

kind, out = sys.argv[1], sys.argv[2]

def mz(image, relocs, cs=0, ip=0, ss=0, minalloc=0):
    hdr = (0x1c + 4 * len(relocs) + 15) // 16 * 16
    size = hdr + len(image)
    h = struct.pack("<2sHHHHHHHHHHHHH", b"MZ", size % 512, (size + 511) // 512,
                    len(relocs), hdr // 16, minalloc, 0xffff, ss, 0x100, 0,
                    ip, cs, 0x1c, 0)
    h += b"".join(struct.pack("<HH", o, s) for s, o in relocs)
    return h + bytes(hdr - len(h)) + image

def marker(word):
    return b"\xeb\x06CP/M" + struct.pack("<H", word)

if kind == "com":
    data = marker(0x1234) + b"\x90" * 8
elif kind == "com-plain":
    data = b"\xc3" * 20
elif kind == "exe":
    # 32 bytes of code, then a 300-byte data group at paragraph 2.
    code = marker(2) + b"\x90" * 24
    data = mz(code + bytes(300), [(0, 6)], minalloc=4)
elif kind == "exe-reloc":
    code = marker(2) + b"\x90" * 24
    data = mz(code + bytes(300), [(0, 6), (0, 0x10)])
elif kind == "exe-nomarker":
    data = mz(b"\x90" * 32 + bytes(300), [])
elif kind == "exe-entry":
    code = marker(2) + b"\x90" * 24
    data = mz(code + bytes(300), [(0, 6)], ip=4)
open(out, "wb").write(data)
