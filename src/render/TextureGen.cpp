// TextureGen.cpp - Texturas procedurales (ruido de valor tileable) guardadas como BMP
#include "render/Texture.h"
#include "core/Math3D.h"
#include "core/Utils.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <algorithm>

namespace {

const int S = 256;

float hashf(int x, int y, int seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFFFF) / 16777215.0f;
}

float sstep(float t) { return t * t * (3.0f - 2.0f * t); }
int wrapi(int a, int p) { return ((a % p) + p) % p; }

// ruido de valor periódico (px, py) para que la textura se repita sin costuras
float vnoise(float x, float y, int px, int py, int seed) {
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    float fx = x - x0, fy = y - y0;
    float a = hashf(wrapi(x0, px), wrapi(y0, py), seed);
    float b = hashf(wrapi(x0 + 1, px), wrapi(y0, py), seed);
    float c = hashf(wrapi(x0, px), wrapi(y0 + 1, py), seed);
    float d = hashf(wrapi(x0 + 1, px), wrapi(y0 + 1, py), seed);
    float u = sstep(fx), v = sstep(fy);
    return lerpf(lerpf(a, b, u), lerpf(c, d, u), v);
}

// fbm en coordenadas u,v en [0,1)
float fbm(float u, float v, int fx, int fy, int oct, int seed) {
    float sum = 0, amp = 0.5f, norm = 0;
    for (int o = 0; o < oct; ++o) {
        sum += amp * vnoise(u * fx, v * fy, fx, fy, seed + o * 31);
        norm += amp;
        amp *= 0.5f;
        fx *= 2;
        fy *= 2;
    }
    return sum / norm;
}

struct Image {
    std::vector<unsigned char> px;
    Image() : px(S * S * 3, 0) {}
    void set(int x, int y, Vec3 c) {
        size_t i = ((size_t)y * S + x) * 3;
        px[i] = (unsigned char)(clampf(c.x, 0, 1) * 255.0f + 0.5f);
        px[i + 1] = (unsigned char)(clampf(c.y, 0, 1) * 255.0f + 0.5f);
        px[i + 2] = (unsigned char)(clampf(c.z, 0, 1) * 255.0f + 0.5f);
    }
};

Vec3 mix(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }

Image brick() {
    Image im;
    const int BW = 64, BH = 32, M = 3;
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            int row = y / BH;
            int off = (row % 2) * (BW / 2);
            int bx = (x + off) % S;
            int col = bx / BW;
            int lx = bx % BW, ly = y % BH;
            float u = x / (float)S, v = y / (float)S;
            float n = fbm(u, v, 16, 16, 4, 11);
            Vec3 c;
            if (lx < M || ly < M) {
                c = Vec3(0.72f, 0.70f, 0.64f) * (0.75f + 0.35f * n);
            } else {
                float id = hashf(col, row, 7);
                Vec3 base = mix(Vec3(0.55f, 0.18f, 0.12f), Vec3(0.70f, 0.32f, 0.20f), id);
                c = base * (0.72f + 0.5f * n);
                int e = std::min(std::min(lx - M, BW - 1 - lx), std::min(ly - M, BH - 1 - ly));
                if (e < 3) c = c * (0.75f + 0.08f * e);
                if (hashf(x, y, 3) > 0.97f) c = c * 0.8f;
            }
            im.set(x, y, c);
        }
    return im;
}

Vec3 woodColor(float u, float v, int seed, float plankId, const Vec3& dark, const Vec3& light) {
    float n1 = fbm(u, v, 2, 16, 4, seed);
    float n2 = fbm(u, v, 32, 4, 3, seed + 5);
    float g = 0.5f + 0.5f * std::sin((v * S * 0.30f) + n1 * 14.0f + plankId * 3.7f);
    g = std::pow(g, 1.6f);
    return mix(dark, light, clampf(g * 0.65f + n2 * 0.45f, 0, 1));
}

Image wood() {
    Image im;
    const int PH = 64;
    Vec3 dark(0.33f, 0.18f, 0.08f), light(0.64f, 0.42f, 0.22f);
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            int plank = y / PH;
            float u = x / (float)S, v = y / (float)S;
            Vec3 c = woodColor(u, v, 21, (float)plank, dark, light);
            c = c * (0.9f + 0.2f * hashf(plank, 0, 9));
            int ly = y % PH;
            int joint = (int)(hashf(plank, 1, 5) * S);
            if (ly < 2 || std::abs(x - joint) < 2) c = c * 0.45f;
            im.set(x, y, c);
        }
    return im;
}

Image crate() {
    Image im;
    const int B = 28;
    Vec3 dark(0.42f, 0.27f, 0.13f), light(0.76f, 0.56f, 0.33f);
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            float u = x / (float)S, v = y / (float)S;
            bool frame = x < B || x >= S - B || y < B || y >= S - B;
            float d1 = std::fabs((float)x - (float)y) / 1.4142f;
            float d2 = std::fabs((float)x + (float)y - (S - 1)) / 1.4142f;
            bool diag = !frame && (d1 < 17 || d2 < 17);
            Vec3 c;
            if (frame) {
                bool vert = (x < B || x >= S - B) && !(y < B || y >= S - B);
                c = vert ? woodColor(v, u, 41, 1, dark, light) : woodColor(u, v, 41, 2, dark, light);
                int e = std::min(std::min(x, S - 1 - x), std::min(y, S - 1 - y));
                int inner = std::min(std::min(std::abs(x - B), std::abs(x - (S - B))), std::min(std::abs(y - B), std::abs(y - (S - B))));
                if (e < 3 || inner < 3) c = c * 0.45f;
                int cx = x < S / 2 ? B / 2 : S - B / 2, cy = y < S / 2 ? B / 2 : S - B / 2;
                if ((x - cx) * (x - cx) + (y - cy) * (y - cy) < 10) c = Vec3(0.25f, 0.25f, 0.27f);
            } else if (diag) {
                float along = (d1 < 17) ? (x + y) / (2.0f * S) : (x - y + S) / (2.0f * S);
                c = woodColor(along, (d1 < 17 ? d1 : d2) / 34.0f, 43, 3, dark, light);
                float dd = d1 < 17 ? d1 : d2;
                if (dd > 14) c = c * 0.5f;
            } else {
                int plank = y / 50;
                c = woodColor(u, v, 47, (float)plank, dark, light) * 0.82f;
                if (y % 50 < 2) c = c * 0.5f;
            }
            im.set(x, y, c);
        }
    return im;
}

Image metal() {
    Image im;
    const int C = 32;
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            float u = x / (float)S, v = y / (float)S;
            float brushed = fbm(u, v, 64, 2, 3, 61);
            float n = fbm(u, v, 8, 8, 4, 62);
            Vec3 c = Vec3(0.60f, 0.61f, 0.64f) * (0.80f + 0.18f * brushed + 0.12f * n);
            float cx = (float)(x % C) - C / 2 + 0.5f, cy = (float)(y % C) - C / 2 + 0.5f;
            bool flip = ((x / C) + (y / C)) % 2 == 0;
            float a = (cx + (flip ? cy : -cy)) / 1.4142f;
            float b = (cx - (flip ? cy : -cy)) / 1.4142f;
            float e = (a / 11.0f) * (a / 11.0f) + (b / 3.2f) * (b / 3.2f);
            if (e < 1.0f) c = c * (1.12f + 0.22f * (-b / 3.2f));
            else if (e < 1.35f) c = c * 0.78f;
            im.set(x, y, c);
        }
    return im;
}

Image floorTiles() {
    Image im;
    const int T = 64;
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            float u = x / (float)S, v = y / (float)S;
            float n = fbm(u, v, 8, 8, 5, 71);
            int lx = x % T, ly = y % T;
            Vec3 c;
            if (lx < 2 || ly < 2) c = Vec3(0.20f, 0.20f, 0.22f);
            else {
                float id = hashf(x / T, y / T, 72);
                c = Vec3(0.50f, 0.51f, 0.53f) * (0.88f + 0.16f * id) * (0.85f + 0.3f * n);
                if (lx < 4 || ly < 4) c = c * 1.08f;
                if (lx > T - 3 || ly > T - 3) c = c * 0.85f;
            }
            im.set(x, y, c);
        }
    return im;
}

Image concrete() {
    Image im;
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            float u = x / (float)S, v = y / (float)S;
            float n = fbm(u, v, 8, 8, 5, 81);
            float m = fbm(u, v, 32, 32, 2, 82);
            Vec3 c = Vec3(0.64f, 0.63f, 0.61f) * (0.72f + 0.38f * n + 0.08f * m);
            float h = hashf(x, y, 83);
            if (h > 0.985f) c = c * 0.65f;
            else if (h < 0.01f) c = c * 1.15f;
            im.set(x, y, c);
        }
    return im;
}

Image marble() {
    Image im;
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            float u = x / (float)S, v = y / (float)S;
            float n = fbm(u, v, 4, 4, 6, 91);
            float s = std::sin((u * 2.0f + v * 1.0f) * 2.0f * PI_F + n * 9.0f);
            float vein = std::pow(1.0f - std::fabs(s), 10.0f);
            Vec3 c = Vec3(0.93f, 0.92f, 0.89f) * (0.92f + 0.08f * n);
            c = mix(c, Vec3(0.42f, 0.42f, 0.46f), vein * 0.7f);
            im.set(x, y, c);
        }
    return im;
}

Image checker() {
    Image im;
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            bool a = ((x / 32) + (y / 32)) % 2 == 0;
            im.set(x, y, a ? Vec3(0.88f, 0.88f, 0.88f) : Vec3(0.22f, 0.24f, 0.28f));
        }
    return im;
}

Image sun() {
    Image im;
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            float n = fbm(x / (float)S, y / (float)S, 8, 8, 5, 101);
            im.set(x, y, mix(Vec3(1.0f, 0.45f, 0.05f), Vec3(1.0f, 0.95f, 0.55f), n * n * 1.4f));
        }
    return im;
}

Image earth() {
    Image im;
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            float u = x / (float)S, v = y / (float)S;
            float n = fbm(u, v, 4, 4, 6, 111);
            Vec3 c;
            if (n > 0.53f) c = mix(Vec3(0.18f, 0.45f, 0.14f), Vec3(0.55f, 0.45f, 0.26f), clampf((n - 0.53f) * 5.0f, 0, 1));
            else c = mix(Vec3(0.03f, 0.12f, 0.40f), Vec3(0.10f, 0.32f, 0.62f), n * 1.7f);
            if (std::fabs(v - 0.5f) > 0.42f + 0.03f * n) c = Vec3(0.92f, 0.94f, 0.97f);
            float cl = fbm(u, v, 8, 8, 4, 113);
            if (cl > 0.6f) c = mix(c, Vec3(1, 1, 1), clampf((cl - 0.6f) * 3.0f, 0, 0.8f));
            im.set(x, y, c);
        }
    return im;
}

Image moon() {
    Image im;
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            float n = fbm(x / (float)S, y / (float)S, 8, 8, 5, 121);
            Vec3 c = Vec3(0.58f, 0.58f, 0.60f) * (0.75f + 0.4f * n);
            for (int k = 0; k < 18; ++k) {
                float cx = hashf(k, 1, 122) * S, cy = hashf(k, 2, 122) * S, r = 6 + hashf(k, 3, 122) * 18;
                float dx = std::fabs(x - cx), dy = std::fabs(y - cy);
                dx = std::min(dx, S - dx); dy = std::min(dy, S - dy);
                float d = std::sqrt(dx * dx + dy * dy) / r;
                if (d < 0.85f) c = c * 0.78f;
                else if (d < 1.0f) c = c * 1.15f;
            }
            im.set(x, y, c);
        }
    return im;
}

} // namespace

namespace TexGen {

bool saveBMP(const std::string& path, int w, int h, const std::vector<unsigned char>& rgb) {
    int stride = (w * 3 + 3) & ~3;
    unsigned int dataSize = (unsigned int)(stride * h);
    unsigned char hdr[54] = {0};
    hdr[0] = 'B'; hdr[1] = 'M';
    unsigned int fileSize = 54 + dataSize;
    auto w32 = [&](int off, unsigned int v) { hdr[off] = v & 255; hdr[off + 1] = (v >> 8) & 255; hdr[off + 2] = (v >> 16) & 255; hdr[off + 3] = (v >> 24) & 255; };
    w32(2, fileSize); w32(10, 54); w32(14, 40); w32(18, (unsigned)w); w32(22, (unsigned)h);
    hdr[26] = 1; hdr[28] = 24; w32(34, dataSize); w32(38, 2835); w32(42, 2835);
    FILE* f = util::openFile(path, "wb");
    if (!f) return false;
    fwrite(hdr, 1, 54, f);
    std::vector<unsigned char> row(stride, 0);
    for (int r = 0; r < h; ++r) {
        int src = h - 1 - r;
        for (int x = 0; x < w; ++x) {
            row[x * 3 + 0] = rgb[((size_t)src * w + x) * 3 + 2];
            row[x * 3 + 1] = rgb[((size_t)src * w + x) * 3 + 1];
            row[x * 3 + 2] = rgb[((size_t)src * w + x) * 3 + 0];
        }
        fwrite(row.data(), 1, stride, f);
    }
    fclose(f);
    return true;
}

void ensureDefaults(const std::string& dir) {
    util::makeDirs(dir);
    struct Entry { const char* name; Image (*gen)(); };
    const Entry entries[] = {
        {"brick.bmp", brick}, {"wood.bmp", wood}, {"crate.bmp", crate}, {"metal.bmp", metal},
        {"floor.bmp", floorTiles}, {"concrete.bmp", concrete}, {"marble.bmp", marble},
        {"checker.bmp", checker}, {"sun.bmp", sun}, {"earth.bmp", earth}, {"moon.bmp", moon}};
    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i) {
        std::string path = dir + "/" + entries[i].name;
        if (util::fileExists(path)) continue;
        Image im = entries[i].gen();
        saveBMP(path, S, S, im.px);
    }
}

} // namespace TexGen
