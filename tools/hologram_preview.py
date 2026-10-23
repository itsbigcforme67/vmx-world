"""A look at the VMX as a green hologram in World's PLAY screen (owner 2026-10-23: "upgrade the VMX render image to a
green hologram 3D model of a VMX ... just see how that would look"). A preview only: it renders the real CAD model
(XMU GADGET/out/*.stl) the way a hologram might look (glowing feature edges, see-through faces brighter at grazing
angles, scanlines, a projector glow under it) with the VMX's live 64x64 picture on its screen, and lays it over a
World PLAY screenshot. Writes docs/hologram/*.png and a turntable GIF.
   XMU\\ GADGET/.venv/bin/python tools/hologram_preview.py [world_screenshot.ppm]"""
import math, os, sys
import numpy as np
import trimesh
from PIL import Image, ImageDraw
from scipy.ndimage import gaussian_filter

HERE = os.path.dirname(os.path.abspath(__file__))
XMU = os.path.join(HERE, "..", "..", "XMU GADGET", "out")
OUT = os.path.join(HERE, "..", "docs", "hologram")
SHOT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "xbox", "build", "shots", "p1.ppm")
PARTS = ["xmu_front", "xmu_rear", "xmu_keypad", "xmu_dpad", "xmu_jewel_a", "xmu_jewel_b", "xmu_side_button"]
SCREEN = dict(x0=-10.05, x1=10.05, y0=32.97, y1=53.07, z=4.05)      # the active area (xmu_screen.py)
GREEN = np.array([0.35, 1.0, 0.45])

def load():
    ms = [trimesh.load(os.path.join(XMU, p + ".stl")) for p in PARTS]
    m = trimesh.util.concatenate(ms)
    edges = []
    for part in ms:                                       # sharp edges + open boundaries, per part
        sharp = part.face_adjacency_edges[part.face_adjacency_angles > math.radians(28)]
        edges.append(part.vertices[sharp])
        e = part.edges_sorted; u, c = np.unique(e, axis=0, return_counts=True)
        if (c == 1).any(): edges.append(part.vertices[u[c == 1]])
    E = np.concatenate(edges)
    # points over the faces (more on bigger faces) for the see-through glow
    rng = np.random.default_rng(1)
    area = m.area_faces; n = 260000
    f = rng.choice(len(m.faces), n, p=area / area.sum())
    uv = rng.random((n, 2)); flip = uv.sum(1) > 1; uv[flip] = 1 - uv[flip]
    tri = m.vertices[m.faces[f]]
    P = tri[:, 0] + uv[:, :1] * (tri[:, 1] - tri[:, 0]) + uv[:, 1:] * (tri[:, 2] - tri[:, 0])
    return P, m.face_normals[f], E

def camera(yaw, pitch):
    cy, sy, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)
    Ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]]); Rx = np.array([[1, 0, 0], [0, cp, -sp], [0, sp, cp]])
    return Rx @ Ry

CENTRE = np.array([0.0, 31.0, 0.0])
def project(X, R, cx, cy, scale, centre=None):
    centre = CENTRE if centre is None else centre
    V = (X - centre) @ R.T
    d = 160.0 / (160.0 - V[..., 2])                       # gentle perspective, the camera 160 mm away
    return np.stack([cx + V[..., 0] * d * scale, cy - V[..., 1] * d * scale], -1), V

def render(P, N, E, screen_bits, yaw, pitch, W, H, cx, cy, scale, t=0.0):
    R = camera(yaw, pitch)
    acc = np.zeros((H, W))
    # see-through faces: brighter where the surface turns away (fresnel), additive, no depth test
    p2, _ = project(P, R, cx, cy, scale)
    view = (N @ R.T)[:, 2]
    w = 0.025 + 0.22 * (1 - np.abs(view)) ** 2
    xi, yi = p2[:, 0].astype(int), p2[:, 1].astype(int)
    ok = (xi >= 0) & (yi >= 0) & (xi < W) & (yi < H)
    np.add.at(acc, (yi[ok], xi[ok]), w[ok])
    acc = gaussian_filter(acc, 0.8)
    # the edges, bright lines
    e2, eV = project(E, R, cx, cy, scale)
    lines = Image.new("F", (W, H), 0.0); d = ImageDraw.Draw(lines)
    depth = eV[..., 2].mean(1); near = (depth - depth.min()) / (np.ptp(depth) + 1e-6)
    for (a, b), k in zip(e2, near):
        d.line([tuple(a), tuple(b)], fill=float(0.35 + 0.65 * k), width=1)
    L = np.asarray(lines)
    # the screen: the VMX's picture on the glass, lit pixels as little quads
    scr = Image.new("F", (W, H), 0.0); ds = ImageDraw.Draw(scr)
    sx = (SCREEN["x1"] - SCREEN["x0"]) / 64; sy = (SCREEN["y1"] - SCREEN["y0"]) / 64
    for j in range(64):
        for i in range(64):
            if not screen_bits[j, i]: continue
            x, y = SCREEN["x0"] + i * sx, SCREEN["y1"] - j * sy
            q = np.array([[x, y, SCREEN["z"]], [x + sx * .9, y, SCREEN["z"]], [x + sx * .9, y - sy * .9, SCREEN["z"]], [x, y - sy * .9, SCREEN["z"]]])
            q2, _ = project(q, R, cx, cy, scale); ds.polygon([tuple(v) for v in q2], fill=1.0)
    S = np.asarray(scr)
    # the glass hides what's behind it (the shell's insides), so the picture reads
    gl = Image.new("F", (W, H), 0.0)
    g4 = np.array([[SCREEN["x0"] - .6, SCREEN["y1"] + .6, SCREEN["z"]], [SCREEN["x1"] + .6, SCREEN["y1"] + .6, SCREEN["z"]],
                   [SCREEN["x1"] + .6, SCREEN["y0"] - .6, SCREEN["z"]], [SCREEN["x0"] - .6, SCREEN["y0"] - .6, SCREEN["z"]]])
    g2, _ = project(g4, R, cx, cy, scale); ImageDraw.Draw(gl).polygon([tuple(v) for v in g2], fill=1.0)
    G = gaussian_filter(np.asarray(gl), 1.0)
    acc = acc * (1 - 0.85 * G) + G * 0.05; L = L * (1 - 0.9 * G)
    core = np.clip(acc * 1.0 + L * 0.9 + S * 1.35, 0, 3)
    glow = gaussian_filter(core, 6)
    img = core + glow * 1.4
    # scanlines, a slow bright band rolling down, a little flicker
    rows = np.arange(H)[:, None]
    img = img * (0.72 + 0.28 * ((rows % 3) != 0))
    band = np.exp(-(((rows - (t * H * 1.3) % (H * 1.3)) / 9.0) ** 2)) * 0.5
    img = img * (1 + band) * (0.94 + 0.06 * math.sin(t * 40))
    return img

def projector(W, H, cx, base_y):
    """the emitter under the hologram: a glowing ellipse and a faint cone of light up into the model"""
    lay = Image.new("F", (W, H), 0.0); d = ImageDraw.Draw(lay)
    for k in range(14, 0, -1):
        d.ellipse([cx - 9 * k, base_y - 2.2 * k, cx + 9 * k, base_y + 2.2 * k], fill=0.06 * (15 - k))
    a = gaussian_filter(np.asarray(lay), 3)
    yy, xx = np.mgrid[0:H, 0:W]
    cone = np.clip(1 - np.abs(xx - cx) / (40 + (base_y - yy) * 0.55), 0, 1) * np.clip((yy - (base_y - 260)) / 260, 0, 1) * (yy < base_y)
    return a + cone * 0.09

def tint(img):
    v = np.clip(img, 0, None)
    rgb = np.clip(v[..., None] * GREEN[None, None] * 255 * 0.8 + np.clip(v - 1.6, 0, None)[..., None] * 120, 0, 255)
    return rgb

def world_background(W=640, H=480):            # world.c's background: the gradient and the grid floor
    img = np.zeros((H, W, 3))
    k = np.arange(H)[:, None] / H
    img[..., 0] = 0x0b * (1 - k) + 0x02 * k; img[..., 1] = 0x1a * (1 - k) + 0x06 * k; img[..., 2] = 0x0d * (1 - k) + 0x03 * k
    im = Image.fromarray(img.astype(np.uint8)); d = ImageDraw.Draw(im)
    for kk in range(-10, 11): d.line([(320 + kk * 24, 200), (320 + kk * 120, 480)], fill=(0x10, 0x30, 0x1a))
    y, j = 210, 0
    while y < 480: d.line([(0, y), (639, y)], fill=(0x10, 0x30, 0x1a)); j += 1; y += 6 + j * 4
    return np.asarray(im).astype(float)

def main():
    os.makedirs(OUT, exist_ok=True)
    shot = np.asarray(Image.open(SHOT).convert("RGB")).astype(float)
    bits = (shot[189:189 + 192:3, 224:224 + 192:3, 1] > 128).astype(np.uint8)     # the VMX picture in the screenshot
    P, N, E = load(); print(len(P), "face points,", len(E), "edges")
    bg = world_background()
    base = shot.copy(); base[96:440, 60:580] = bg[96:440, 60:580]                     # the render taken out
    W, H = 640, 480
    proj = projector(W, H, 320, 432)
    def frame(yaw, pitch, t):
        holo = render(P, N, E, bits, yaw, pitch, W, H, 320, 255, 4.8, t)
        out = np.clip(base + tint(holo + proj), 0, 255).astype(np.uint8)
        im = Image.fromarray(out); d = ImageDraw.Draw(im)
        return im
    frame(math.radians(-24), math.radians(-12), 0.15).save(os.path.join(OUT, "play_hologram.png")); print("play_hologram.png")
    # while playing: the head fills the view, the screen at about 3 TV pixels per VMX pixel, the body fading into the beam
    global CENTRE
    CENTRE = np.array([0.0, 43.0, 0.0])
    holo = render(P, N, E, bits, math.radians(-14), math.radians(-8), W, H, 320, 250, 9.4, 0.2)
    yy = np.arange(H)[:, None]; holo = holo * np.clip((440 - yy) / 120.0, 0.15, 1)
    out = np.clip(base + tint(holo + projector(W, H, 320, 470) * 0.8), 0, 255).astype(np.uint8)
    Image.fromarray(out).save(os.path.join(OUT, "playing_hologram.png")); print("playing_hologram.png")
    CENTRE = np.array([0.0, 31.0, 0.0])
    # a close-up, bigger, on black
    holo = render(P, N, E, bits, math.radians(-30), math.radians(-14), 640, 640, 320, 330, 8.5, 0.3)
    Image.fromarray(np.clip(tint(holo + projector(640, 640, 320, 610) * 1.2), 0, 255).astype(np.uint8)).save(os.path.join(OUT, "closeup.png")); print("closeup.png")
    # a turntable: it turns slowly back and forth, as it would idle in PLAY
    frames = []
    for k in range(36):
        a = k / 36 * 2 * math.pi
        frames.append(frame(math.radians(-24 + 22 * math.sin(a)), math.radians(-12 + 3 * math.sin(2 * a)), k / 36).resize((480, 360), Image.LANCZOS))
    frames[0].save(os.path.join(OUT, "play_hologram.gif"), save_all=True, append_images=frames[1:], duration=80, loop=0)
    print("play_hologram.gif")

if __name__ == "__main__":
    main()
