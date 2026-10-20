"""FOLD's raw assets -> PNG for the mockup. Usage: asset_png.py in out
   metal.rgb: "METL", u16 w, u16 h, RGB.   vmx_*.rgba: "VMXF", u16 w, h, screen x, screen y, RGBA (screen = 64 x scale)."""
import struct, sys, zlib
d = open(sys.argv[1], "rb").read()
if d[:4] == b"METL":
    w, h = struct.unpack("<HH", d[4:8]); px, bpp, ct, off = d[8:], 3, 2, None
elif d[:4] == b"VMXF":
    w, h, sx, sy = struct.unpack("<HHHH", d[4:12]); px, bpp, ct, off = d[12:], 4, 6, (sx, sy)
else:
    sys.exit("unknown asset")
raw = b"".join(b"\0" + px[y * w * bpp:(y + 1) * w * bpp] for y in range(h))
ch = lambda t, b: struct.pack(">I", len(b)) + t + b + struct.pack(">I", zlib.crc32(t + b) & 0xffffffff)
open(sys.argv[2], "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, ct, 0, 0, 0))
                              + ch(b"IDAT", zlib.compress(raw, 9)) + ch(b"IEND", b""))
print(sys.argv[2], w, h, "screen at", off) if off else print(sys.argv[2], w, h)
