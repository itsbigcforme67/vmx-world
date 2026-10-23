"""Exports the VMX's skeleton hologram for World (owner 2026-10-23: the x-ray look, and "lean into this skeleton
hologram look for all VMX stuff on the Xbox side"). From the CAD model (XMU GADGET/out: the shells and side button,
plus the device's own caps in tools/hologram_caps) it writes xbox/assets/vmx_holo.bin:
   "VHOL", u32 edges, u32 points
   edges:  int16 x0 y0 z0 x1 y1 z1 each (0.01 mm units): the parts' sharp edges (> 28 degrees) and open boundaries
   points: int16 x y z, int8 nx ny nz, u8 pad (normal x 127): points spread over the faces by area (the glow)
The look is the one tools/hologram_preview.py renders; World draws it in xbox/holo.c.
   XMU\\ GADGET/.venv/bin/python tools/hologram_export.py [points]"""
import math, os, struct, sys
import numpy as np
import trimesh

HERE = os.path.dirname(os.path.abspath(__file__))
XMU = os.path.join(HERE, "..", "..", "XMU GADGET", "out")
PARTS = [os.path.join(XMU, p + ".stl") for p in ("xmu_front", "xmu_rear", "xmu_side_button")] + \
        [os.path.join(HERE, "hologram_caps", "cap_%s.stl" % c) for c in ("up", "down", "left", "right", "a", "b")]
NPTS = int(sys.argv[1]) if len(sys.argv) > 1 else 90000

ms = [trimesh.load(p) for p in PARTS]
edges = []
for m in ms:
    edges.append(m.vertices[m.face_adjacency_edges[m.face_adjacency_angles > math.radians(28)]])
    u, c = np.unique(m.edges_sorted, axis=0, return_counts=True)
    if (c == 1).any(): edges.append(m.vertices[u[c == 1]])
E = np.concatenate(edges)
m = trimesh.util.concatenate(ms)
rng = np.random.default_rng(1)
f = rng.choice(len(m.faces), NPTS, p=m.area_faces / m.area_faces.sum())
uv = rng.random((NPTS, 2)); flip = uv.sum(1) > 1; uv[flip] = 1 - uv[flip]
tri = m.vertices[m.faces[f]]
P = tri[:, 0] + uv[:, :1] * (tri[:, 1] - tri[:, 0]) + uv[:, 1:] * (tri[:, 2] - tri[:, 0])
N = m.face_normals[f]
out = os.path.join(HERE, "..", "xbox", "assets", "vmx_holo.bin")
os.makedirs(os.path.dirname(out), exist_ok=True)
with open(out, "wb") as o:
    o.write(b"VHOL" + struct.pack("<II", len(E), NPTS))
    o.write(np.round(E.reshape(-1, 6) * 100).astype("<i2").tobytes())
    rec = np.zeros(NPTS, dtype=[("p", "<i2", 3), ("n", "i1", 3), ("pad", "u1")])
    rec["p"] = np.round(P * 100); rec["n"] = np.round(N * 127)
    o.write(rec.tobytes())
print(f"{out}: {len(E)} edges, {NPTS} points, {os.path.getsize(out)} bytes")
