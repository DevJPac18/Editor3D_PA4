#include "io/ObjLoader.h"
#include "core/Utils.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include <algorithm>

namespace {
struct Key {
    int v, t, n;
    bool operator<(const Key& o) const {
        if (v != o.v) return v < o.v;
        if (t != o.t) return t < o.t;
        return n < o.n;
    }
};

int fixIndex(int idx, int count) {
    if (idx > 0) return idx - 1;
    if (idx < 0) return count + idx;
    return -1;
}
}

bool loadOBJ(const std::string& path, Mesh& out, bool normalizeModel) {
    FILE* f = util::openFile(path, "rb");
    if (!f) return false;
    std::vector<Vec3> P, N;
    std::vector<float> T;
    std::vector<Key> corners; // tríos de esquinas por triángulo
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        char* s = line;
        while (*s == ' ' || *s == '\t') ++s;
        if (s[0] == 'v' && s[1] == ' ') {
            Vec3 p; p.x = (float)strtod(s + 2, &s); p.y = (float)strtod(s, &s); p.z = (float)strtod(s, &s);
            P.push_back(p);
        } else if (s[0] == 'v' && s[1] == 'n') {
            Vec3 n; n.x = (float)strtod(s + 2, &s); n.y = (float)strtod(s, &s); n.z = (float)strtod(s, &s);
            N.push_back(n);
        } else if (s[0] == 'v' && s[1] == 't') {
            float u = (float)strtod(s + 2, &s); float v = (float)strtod(s, &s);
            T.push_back(u); T.push_back(v);
        } else if (s[0] == 'f' && (s[1] == ' ' || s[1] == '\t')) {
            std::vector<Key> poly;
            char* tok = strtok(s + 2, " \t\r\n");
            while (tok) {
                Key k; k.v = k.t = k.n = 0;
                int a = 0, b = 0, c = 0;
                const char* p1 = strchr(tok, '/');
                a = atoi(tok);
                if (p1) {
                    if (p1[1] != '/') b = atoi(p1 + 1);
                    const char* p2 = strchr(p1 + 1, '/');
                    if (p2) c = atoi(p2 + 1);
                }
                k.v = fixIndex(a, (int)P.size());
                k.t = fixIndex(b, (int)T.size() / 2);
                k.n = fixIndex(c, (int)N.size());
                if (k.v >= 0 && k.v < (int)P.size()) poly.push_back(k);
                tok = strtok(NULL, " \t\r\n");
            }
            for (size_t i = 1; i + 1 < poly.size(); ++i) {
                corners.push_back(poly[0]);
                corners.push_back(poly[i]);
                corners.push_back(poly[i + 1]);
            }
        }
    }
    fclose(f);
    if (corners.empty()) return false;

    // normales suavizadas por posición si el archivo no las trae
    std::vector<Vec3> smooth;
    bool needNormals = false;
    for (size_t i = 0; i < corners.size(); ++i)
        if (corners[i].n < 0 || corners[i].n >= (int)N.size()) { needNormals = true; break; }
    if (needNormals) {
        smooth.assign(P.size(), Vec3());
        for (size_t i = 0; i + 2 < corners.size(); i += 3) {
            Vec3 a = P[corners[i].v], b = P[corners[i + 1].v], c = P[corners[i + 2].v];
            Vec3 fn = cross(b - a, c - a);
            smooth[corners[i].v] += fn;
            smooth[corners[i + 1].v] += fn;
            smooth[corners[i + 2].v] += fn;
        }
        for (size_t i = 0; i < smooth.size(); ++i) smooth[i] = normalize(smooth[i]);
    }

    out = Mesh();
    std::map<Key, unsigned int> lookup;
    for (size_t i = 0; i < corners.size(); ++i) {
        Key k = corners[i];
        bool hasN = k.n >= 0 && k.n < (int)N.size();
        if (!hasN) k.n = -1;
        if (!(k.t >= 0 && k.t < (int)T.size() / 2)) k.t = -1;
        std::map<Key, unsigned int>::iterator it = lookup.find(k);
        if (it != lookup.end()) { out.indices.push_back(it->second); continue; }
        Vec3 p = P[k.v];
        Vec3 n = hasN ? normalize(N[k.n]) : smooth[k.v];
        float u = 0, v = 0;
        if (k.t >= 0) { u = T[k.t * 2]; v = T[k.t * 2 + 1]; }
        out.positions.push_back(p.x); out.positions.push_back(p.y); out.positions.push_back(p.z);
        out.normals.push_back(n.x); out.normals.push_back(n.y); out.normals.push_back(n.z);
        out.uvs.push_back(u); out.uvs.push_back(v);
        unsigned int idx = (unsigned int)(out.positions.size() / 3 - 1);
        lookup[k] = idx;
        out.indices.push_back(idx);
    }
    out.computeBounds();
    if (normalizeModel) {
        Vec3 c = (out.bmin + out.bmax) * 0.5f;
        Vec3 e = out.bmax - out.bmin;
        float m = std::max(e.x, std::max(e.y, e.z));
        float s = m > 1e-6f ? 1.0f / m : 1.0f;
        for (size_t i = 0; i + 2 < out.positions.size(); i += 3) {
            out.positions[i] = (out.positions[i] - c.x) * s;
            out.positions[i + 1] = (out.positions[i + 1] - c.y) * s;
            out.positions[i + 2] = (out.positions[i + 2] - c.z) * s;
        }
        out.computeBounds();
    }
    return true;
}
