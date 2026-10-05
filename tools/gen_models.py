"""Genera modelos OBJ de ejemplo para assets/models (peón y jarrón por revolución, engranaje extruido)."""
import math
import os

OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "models")


def lathe(name, profile, segs=40):
    """Superficie de revolución alrededor de Y. profile = [(radio, altura), ...] de abajo hacia arriba."""
    lines = [f"# {name} - generado por tools/gen_models.py", f"o {name}"]
    n = len(profile)
    # longitud acumulada del perfil para la coordenada v
    acc = [0.0]
    for i in range(1, n):
        acc.append(acc[-1] + math.dist(profile[i - 1], profile[i]))
    total = acc[-1] or 1.0
    for s in range(segs + 1):
        a = 2 * math.pi * s / segs
        for (r, y) in profile:
            lines.append(f"v {r * math.sin(a):.5f} {y:.5f} {r * math.cos(a):.5f}")
    for s in range(segs + 1):
        for i in range(n):
            lines.append(f"vt {s / segs:.5f} {acc[i] / total:.5f}")
    # normales a partir de la tangente del perfil
    for s in range(segs + 1):
        a = 2 * math.pi * s / segs
        for i in range(n):
            p0 = profile[max(i - 1, 0)]
            p1 = profile[min(i + 1, n - 1)]
            dr, dy = p1[0] - p0[0], p1[1] - p0[1]
            nr, ny = dy, -dr
            ln = math.hypot(nr, ny) or 1.0
            nr, ny = nr / ln, ny / ln
            lines.append(f"vn {nr * math.sin(a):.5f} {ny:.5f} {nr * math.cos(a):.5f}")
    for s in range(segs):
        for i in range(n - 1):
            a = s * n + i + 1
            b = (s + 1) * n + i + 1
            # orden antihorario visto desde fuera
            lines.append(f"f {a}/{a}/{a} {b}/{b}/{b} {b + 1}/{b + 1}/{b + 1}")
            lines.append(f"f {a}/{a}/{a} {b + 1}/{b + 1}/{b + 1} {a + 1}/{a + 1}/{a + 1}")
    with open(os.path.join(OUT, name + ".obj"), "w", encoding="ascii") as f:
        f.write("\n".join(lines) + "\n")


def pawn():
    prof = [(0.0, 0.0), (0.42, 0.0), (0.43, 0.04), (0.42, 0.08), (0.36, 0.11), (0.35, 0.15),
            (0.30, 0.18), (0.22, 0.22), (0.17, 0.30), (0.14, 0.42), (0.13, 0.54), (0.13, 0.60),
            (0.22, 0.63), (0.23, 0.66), (0.22, 0.69), (0.12, 0.72)]
    cy, rad = 0.86, 0.165
    start = math.asin(max(-1.0, min(1.0, (0.73 - cy) / rad)))
    for k in range(1, 15):
        t = start + (math.pi / 2 - start) * k / 14
        prof.append((max(0.0, rad * math.cos(t)), cy + rad * math.sin(t)))
    prof[-1] = (0.0, prof[-1][1])
    lathe("peon", prof)


def vase():
    prof = [(0.0, 0.0), (0.28, 0.0), (0.30, 0.02)]
    for k in range(1, 30):
        y = 0.02 + 0.98 * k / 29
        r = 0.22 + 0.16 * math.sin(math.pi * y * 1.1) - 0.08 * math.sin(math.pi * y * 3.0) * (y > 0.6)
        prof.append((max(0.08, r), y))
    prof.append((prof[-1][0] + 0.03, 1.02))
    prof.append((prof[-1][0] - 0.05, 1.03))
    lathe("jarron", prof)


def gear(teeth=14, r_out=0.5, r_in=0.42, r_hole=0.12, h=0.18):
    """Engranaje extruido en Y con agujero central y normales planas."""
    pts = []
    for t in range(teeth):
        base = 2 * math.pi * t / teeth
        step = 2 * math.pi / teeth
        for frac, r in ((0.0, r_in), (0.18, r_out), (0.5, r_out), (0.68, r_in)):
            pts.append((r, base + step * frac))
    outer = [(r * math.cos(a), r * math.sin(a)) for r, a in pts]
    m = len(outer)
    hole = [(r_hole * math.cos(2 * math.pi * k / m), r_hole * math.sin(2 * math.pi * k / m)) for k in range(m)]
    v, vn, f = [], [], []

    def add_v(x, y, z):
        v.append((x, y, z))
        return len(v)

    def add_n(x, y, z):
        vn.append((x, y, z))
        return len(vn)

    y0, y1 = -h / 2, h / 2
    n_up, n_dn = add_n(0, 1, 0), add_n(0, -1, 0)
    top_o = [add_v(x, y1, z) for x, z in outer]
    top_h = [add_v(x, y1, z) for x, z in hole]
    bot_o = [add_v(x, y0, z) for x, z in outer]
    bot_h = [add_v(x, y0, z) for x, z in hole]
    for k in range(m):
        k1 = (k + 1) % m
        # caras superior / inferior (anillo entre agujero y contorno)
        f.append(((top_h[k], n_up), (top_o[k1], n_up), (top_o[k], n_up)))
        f.append(((top_h[k], n_up), (top_h[k1], n_up), (top_o[k1], n_up)))
        f.append(((bot_h[k], n_dn), (bot_o[k], n_dn), (bot_o[k1], n_dn)))
        f.append(((bot_h[k], n_dn), (bot_o[k1], n_dn), (bot_h[k1], n_dn)))
        # lateral exterior
        (ax, az), (bx, bz) = outer[k], outer[k1]
        nx, nz = (bz - az), -(bx - ax)
        ln = math.hypot(nx, nz) or 1
        no = add_n(nx / ln, 0, nz / ln)
        f.append(((bot_o[k], no), (top_o[k], no), (top_o[k1], no)))
        f.append(((bot_o[k], no), (top_o[k1], no), (bot_o[k1], no)))
        # lateral del agujero (normal hacia el centro)
        (cx, cz), (dx, dz) = hole[k], hole[k1]
        mx, mz = -(cx + dx) / 2, -(cz + dz) / 2
        ln = math.hypot(mx, mz) or 1
        nh = add_n(mx / ln, 0, mz / ln)
        f.append(((bot_h[k], nh), (top_h[k1], nh), (top_h[k], nh)))
        f.append(((bot_h[k], nh), (bot_h[k1], nh), (top_h[k1], nh)))
    lines = ["# engranaje - generado por tools/gen_models.py", "o engranaje"]
    lines += [f"v {x:.5f} {y:.5f} {z:.5f}" for x, y, z in v]
    lines += [f"vn {x:.5f} {y:.5f} {z:.5f}" for x, y, z in vn]
    lines += ["f " + " ".join(f"{a}//{b}" for a, b in tri) for tri in f]
    with open(os.path.join(OUT, "engranaje.obj"), "w", encoding="ascii") as fh:
        fh.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    pawn()
    vase()
    gear()
    print("Modelos generados en", os.path.abspath(OUT))
