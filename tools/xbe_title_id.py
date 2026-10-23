"""Sets an XBE's title ID (its certificate's Title ID field): cxbe has no option for it. VMX World is 56580001, the
title that owns every VMX save (docs/DESIGN.md "Title IDs"). Homebrew XBEs aren't signature-checked by the
dashboards we target, so changing the certificate is safe.
   python3 tools/xbe_title_id.py default.xbe 0x56580001"""
import struct, sys

path, tid = sys.argv[1], int(sys.argv[2], 0)
d = bytearray(open(path, "rb").read())
assert d[:4] == b"XBEH", "not an XBE"
base, cert = struct.unpack_from("<I", d, 0x104)[0], struct.unpack_from("<I", d, 0x118)[0]
off = cert - base                                   # the certificate: size, time/date, then the title ID
struct.pack_into("<I", d, off + 8, tid)
open(path, "wb").write(d)
print(f"{path}: title ID {tid:08X}")
