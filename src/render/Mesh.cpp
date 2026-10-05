#include "render/Mesh.h"
#include "render/GL.h"
#include "io/ObjLoader.h"
#include "core/Utils.h"
#include <cmath>

void Mesh::computeBounds() {
    if (positions.empty()) { bmin = bmax = Vec3(); return; }
    bmin = bmax = Vec3(positions[0], positions[1], positions[2]);
    for (size_t i = 0; i + 2 < positions.size(); i += 3) {
        Vec3 p(positions[i], positions[i + 1], positions[i + 2]);
        if (p.x < bmin.x) bmin.x = p.x;
        if (p.y < bmin.y) bmin.y = p.y;
        if (p.z < bmin.z) bmin.z = p.z;
        if (p.x > bmax.x) bmax.x = p.x;
        if (p.y > bmax.y) bmax.y = p.y;
        if (p.z > bmax.z) bmax.z = p.z;
    }
}

void Mesh::draw(bool withNormals, bool withUVs) const {
    if (indices.empty()) return;
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, positions.data());
    bool n = withNormals && normals.size() == positions.size();
    bool t = withUVs && uvs.size() / 2 == positions.size() / 3;
    if (n) { glEnableClientState(GL_NORMAL_ARRAY); glNormalPointer(GL_FLOAT, 0, normals.data()); }
    if (t) { glEnableClientState(GL_TEXTURE_COORD_ARRAY); glTexCoordPointer(2, GL_FLOAT, 0, uvs.data()); }
    glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, indices.data());
    if (n) glDisableClientState(GL_NORMAL_ARRAY);
    if (t) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

namespace {
struct Builder {
    Mesh& m;
    explicit Builder(Mesh& mesh) : m(mesh) {}
    unsigned int vert(const Vec3& p, const Vec3& n, float u, float v) {
        m.positions.push_back(p.x); m.positions.push_back(p.y); m.positions.push_back(p.z);
        m.normals.push_back(n.x); m.normals.push_back(n.y); m.normals.push_back(n.z);
        m.uvs.push_back(u); m.uvs.push_back(v);
        return (unsigned int)(m.positions.size() / 3 - 1);
    }
    void tri(unsigned int a, unsigned int b, unsigned int c) {
        m.indices.push_back(a); m.indices.push_back(b); m.indices.push_back(c);
    }
};
}

namespace MeshGen {

Mesh cube() {
    Mesh m;
    Builder b(m);
    struct Face { Vec3 n, u, v; };
    const Face faces[6] = {
        {Vec3(1, 0, 0), Vec3(0, 0, -1), Vec3(0, 1, 0)},
        {Vec3(-1, 0, 0), Vec3(0, 0, 1), Vec3(0, 1, 0)},
        {Vec3(0, 1, 0), Vec3(1, 0, 0), Vec3(0, 0, -1)},
        {Vec3(0, -1, 0), Vec3(1, 0, 0), Vec3(0, 0, 1)},
        {Vec3(0, 0, 1), Vec3(1, 0, 0), Vec3(0, 1, 0)},
        {Vec3(0, 0, -1), Vec3(-1, 0, 0), Vec3(0, 1, 0)}};
    for (int f = 0; f < 6; ++f) {
        const Face& F = faces[f];
        Vec3 c = F.n * 0.5f;
        unsigned int i0 = b.vert(c - F.u * 0.5f - F.v * 0.5f, F.n, 0, 0);
        unsigned int i1 = b.vert(c + F.u * 0.5f - F.v * 0.5f, F.n, 1, 0);
        unsigned int i2 = b.vert(c + F.u * 0.5f + F.v * 0.5f, F.n, 1, 1);
        unsigned int i3 = b.vert(c - F.u * 0.5f + F.v * 0.5f, F.n, 0, 1);
        b.tri(i0, i1, i2);
        b.tri(i0, i2, i3);
    }
    m.computeBounds();
    return m;
}

Mesh sphere(int seg, int rings) {
    Mesh m;
    Builder b(m);
    for (int r = 0; r <= rings; ++r) {
        float phi = PI_F * r / rings;
        float y = std::cos(phi), rr = std::sin(phi);
        for (int s = 0; s <= seg; ++s) {
            float th = 2.0f * PI_F * s / seg;
            Vec3 n(rr * std::sin(th), y, rr * std::cos(th));
            b.vert(n * 0.5f, n, (float)s / seg, 1.0f - (float)r / rings);
        }
    }
    for (int r = 0; r < rings; ++r)
        for (int s = 0; s < seg; ++s) {
            unsigned int a = r * (seg + 1) + s, c = a + seg + 1;
            b.tri(a, c, a + 1);
            b.tri(a + 1, c, c + 1);
        }
    m.computeBounds();
    return m;
}

Mesh cylinder(int seg) {
    Mesh m;
    Builder b(m);
    for (int s = 0; s <= seg; ++s) {
        float th = 2.0f * PI_F * s / seg;
        Vec3 d(std::sin(th), 0, std::cos(th));
        b.vert(Vec3(d.x * 0.5f, -0.5f, d.z * 0.5f), d, (float)s / seg, 0);
        b.vert(Vec3(d.x * 0.5f, 0.5f, d.z * 0.5f), d, (float)s / seg, 1);
    }
    for (int s = 0; s < seg; ++s) {
        unsigned int b0 = s * 2, t0 = s * 2 + 1, b1 = (s + 1) * 2, t1 = (s + 1) * 2 + 1;
        b.tri(b0, b1, t1);
        b.tri(b0, t1, t0);
    }
    for (int cap = 0; cap < 2; ++cap) {
        float y = cap == 0 ? 0.5f : -0.5f;
        Vec3 n(0, cap == 0 ? 1.0f : -1.0f, 0);
        unsigned int c = b.vert(Vec3(0, y, 0), n, 0.5f, 0.5f);
        unsigned int first = (unsigned int)(m.positions.size() / 3);
        for (int s = 0; s <= seg; ++s) {
            float th = 2.0f * PI_F * s / seg;
            float x = std::sin(th) * 0.5f, z = std::cos(th) * 0.5f;
            b.vert(Vec3(x, y, z), n, 0.5f + x, 0.5f - z);
        }
        for (int s = 0; s < seg; ++s) {
            if (cap == 0) b.tri(c, first + s, first + s + 1);
            else b.tri(c, first + s + 1, first + s);
        }
    }
    m.computeBounds();
    return m;
}

Mesh cone(int seg) {
    Mesh m;
    Builder b(m);
    for (int s = 0; s <= seg; ++s) {
        float th = 2.0f * PI_F * s / seg;
        float thm = 2.0f * PI_F * (s + 0.5f) / seg;
        Vec3 n = normalize(Vec3(std::sin(th), 0.5f, std::cos(th)));
        Vec3 nm = normalize(Vec3(std::sin(thm), 0.5f, std::cos(thm)));
        b.vert(Vec3(std::sin(th) * 0.5f, -0.5f, std::cos(th) * 0.5f), n, (float)s / seg, 0);
        b.vert(Vec3(0, 0.5f, 0), nm, (float)(s + 0.5f) / seg, 1);
    }
    for (int s = 0; s < seg; ++s) b.tri(s * 2, (s + 1) * 2, s * 2 + 1);
    Vec3 n(0, -1, 0);
    unsigned int c = b.vert(Vec3(0, -0.5f, 0), n, 0.5f, 0.5f);
    unsigned int first = (unsigned int)(m.positions.size() / 3);
    for (int s = 0; s <= seg; ++s) {
        float th = 2.0f * PI_F * s / seg;
        float x = std::sin(th) * 0.5f, z = std::cos(th) * 0.5f;
        b.vert(Vec3(x, -0.5f, z), n, 0.5f + x, 0.5f + z);
    }
    for (int s = 0; s < seg; ++s) b.tri(c, first + s + 1, first + s);
    m.computeBounds();
    return m;
}

Mesh plane(int div) {
    Mesh m;
    Builder b(m);
    for (int j = 0; j <= div; ++j)
        for (int i = 0; i <= div; ++i) {
            float x = -0.5f + (float)i / div, z = -0.5f + (float)j / div;
            b.vert(Vec3(x, 0, z), Vec3(0, 1, 0), x + 0.5f, 0.5f - z);
        }
    for (int j = 0; j < div; ++j)
        for (int i = 0; i < div; ++i) {
            unsigned int a = j * (div + 1) + i, bb = a + 1, c = a + div + 1, d = c + 1;
            b.tri(a, c, bb);
            b.tri(bb, c, d);
        }
    m.computeBounds();
    return m;
}

Mesh torus(int seg, int sides, float R, float r) {
    Mesh m;
    Builder b(m);
    for (int i = 0; i <= seg; ++i) {
        float u = 2.0f * PI_F * i / seg;
        Vec3 dir(std::sin(u), 0, std::cos(u));
        for (int j = 0; j <= sides; ++j) {
            float v = 2.0f * PI_F * j / sides;
            Vec3 n = dir * std::cos(v) + Vec3(0, 1, 0) * std::sin(v);
            b.vert(dir * R + n * r, n, 3.0f * i / seg, (float)j / sides);
        }
    }
    for (int i = 0; i < seg; ++i)
        for (int j = 0; j < sides; ++j) {
            unsigned int a = i * (sides + 1) + j, c = (i + 1) * (sides + 1) + j;
            b.tri(a, c, c + 1);
            b.tri(a, c + 1, a + 1);
        }
    m.computeBounds();
    return m;
}

} // namespace MeshGen

const std::vector<std::string>& MeshLibrary::primitives() {
    static std::vector<std::string> p;
    if (p.empty()) {
        p.push_back("cube"); p.push_back("sphere"); p.push_back("cylinder");
        p.push_back("cone"); p.push_back("plane"); p.push_back("torus");
    }
    return p;
}

bool MeshLibrary::isPrimitive(const std::string& name) {
    const std::vector<std::string>& p = primitives();
    for (size_t i = 0; i < p.size(); ++i)
        if (p[i] == name) return true;
    return false;
}

std::string MeshLibrary::displayName(const std::string& name) {
    if (name == "cube") return "Cubo";
    if (name == "sphere") return "Esfera";
    if (name == "cylinder") return "Cilindro";
    if (name == "cone") return "Cono";
    if (name == "plane") return "Plano";
    if (name == "torus") return "Toro";
    return util::fileName(name);
}

const Mesh* MeshLibrary::get(const std::string& name) {
    if (name.empty()) return nullptr;
    std::map<std::string, Mesh>::iterator it = cache.find(name);
    if (it != cache.end()) return it->second.empty() ? nullptr : &it->second;
    Mesh m;
    if (name == "cube") m = MeshGen::cube();
    else if (name == "sphere") m = MeshGen::sphere();
    else if (name == "cylinder") m = MeshGen::cylinder();
    else if (name == "cone") m = MeshGen::cone();
    else if (name == "plane") m = MeshGen::plane();
    else if (name == "torus") m = MeshGen::torus();
    else if (util::extension(name) == "obj") loadOBJ(name, m, true);
    Mesh& stored = cache[name];
    stored = m;
    return stored.empty() ? nullptr : &stored;
}

void MeshLibrary::forget(const std::string& name) { cache.erase(name); }
