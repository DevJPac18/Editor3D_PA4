// Math3D.h - Vectores y matrices 4x4 (column-major, compatibles con OpenGL)
#pragma once
#include <cmath>

const float PI_F = 3.14159265358979f;
inline float deg2rad(float d) { return d * PI_F / 180.0f; }
inline float rad2deg(float r) { return r * 180.0f / PI_F; }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(float s) const { return Vec3(x / s, y / s, z / s); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const Vec3& o) const { return !(*this == o); }
    float& operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }
    float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
};

inline Vec3 mulComp(const Vec3& a, const Vec3& b) { return Vec3(a.x * b.x, a.y * b.y, a.z * b.z); }
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(const Vec3& v) {
    float l = length(v);
    return l > 1e-8f ? v / l : v;
}

struct Vec4 {
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
    Vec4(const Vec3& v, float d) : x(v.x), y(v.y), z(v.z), w(d) {}
};

struct Mat4 {
    float m[16]; // column-major: m[col*4 + row]

    Mat4() { setIdentity(); }
    void setIdentity() {
        for (int i = 0; i < 16; ++i) m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
    float& at(int r, int c) { return m[c * 4 + r]; }
    float at(int r, int c) const { return m[c * 4 + r]; }

    Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += at(i, k) * b.at(k, j);
                r.at(i, j) = s;
            }
        return r;
    }
    Vec4 operator*(const Vec4& v) const {
        return Vec4(at(0, 0) * v.x + at(0, 1) * v.y + at(0, 2) * v.z + at(0, 3) * v.w,
                    at(1, 0) * v.x + at(1, 1) * v.y + at(1, 2) * v.z + at(1, 3) * v.w,
                    at(2, 0) * v.x + at(2, 1) * v.y + at(2, 2) * v.z + at(2, 3) * v.w,
                    at(3, 0) * v.x + at(3, 1) * v.y + at(3, 2) * v.z + at(3, 3) * v.w);
    }
    Vec3 transformPoint(const Vec3& p) const {
        Vec4 r = (*this) * Vec4(p, 1.0f);
        if (std::fabs(r.w) > 1e-8f && r.w != 1.0f) return Vec3(r.x / r.w, r.y / r.w, r.z / r.w);
        return Vec3(r.x, r.y, r.z);
    }
    Vec3 transformDir(const Vec3& d) const {
        Vec4 r = (*this) * Vec4(d, 0.0f);
        return Vec3(r.x, r.y, r.z);
    }
    Vec3 column(int c) const { return Vec3(m[c * 4], m[c * 4 + 1], m[c * 4 + 2]); }
    Vec3 translation() const { return column(3); }

    static Mat4 translate(const Vec3& t) {
        Mat4 r; r.at(0, 3) = t.x; r.at(1, 3) = t.y; r.at(2, 3) = t.z; return r;
    }
    static Mat4 scale(const Vec3& s) {
        Mat4 r; r.at(0, 0) = s.x; r.at(1, 1) = s.y; r.at(2, 2) = s.z; return r;
    }
    static Mat4 rotateX(float deg) {
        Mat4 r; float c = std::cos(deg2rad(deg)), s = std::sin(deg2rad(deg));
        r.at(1, 1) = c; r.at(1, 2) = -s; r.at(2, 1) = s; r.at(2, 2) = c; return r;
    }
    static Mat4 rotateY(float deg) {
        Mat4 r; float c = std::cos(deg2rad(deg)), s = std::sin(deg2rad(deg));
        r.at(0, 0) = c; r.at(0, 2) = s; r.at(2, 0) = -s; r.at(2, 2) = c; return r;
    }
    static Mat4 rotateZ(float deg) {
        Mat4 r; float c = std::cos(deg2rad(deg)), s = std::sin(deg2rad(deg));
        r.at(0, 0) = c; r.at(0, 1) = -s; r.at(1, 0) = s; r.at(1, 1) = c; return r;
    }
    static Mat4 perspective(float fovyDeg, float aspect, float n, float f) {
        Mat4 r;
        float t = 1.0f / std::tan(deg2rad(fovyDeg) * 0.5f);
        r.at(0, 0) = t / aspect; r.at(1, 1) = t;
        r.at(2, 2) = (f + n) / (n - f); r.at(2, 3) = 2.0f * f * n / (n - f);
        r.at(3, 2) = -1.0f; r.at(3, 3) = 0.0f;
        return r;
    }
    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = normalize(center - eye);
        Vec3 s = normalize(cross(f, up));
        Vec3 u = cross(s, f);
        Mat4 r;
        r.at(0, 0) = s.x; r.at(0, 1) = s.y; r.at(0, 2) = s.z;
        r.at(1, 0) = u.x; r.at(1, 1) = u.y; r.at(1, 2) = u.z;
        r.at(2, 0) = -f.x; r.at(2, 1) = -f.y; r.at(2, 2) = -f.z;
        r.at(0, 3) = -dot(s, eye); r.at(1, 3) = -dot(u, eye); r.at(2, 3) = dot(f, eye);
        return r;
    }

    Mat4 inverse() const {
        const float* a = m;
        float inv[16];
        inv[0] = a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15] + a[9]*a[7]*a[14] + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
        inv[4] = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15] - a[8]*a[7]*a[14] - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
        inv[8] = a[4]*a[9]*a[15] - a[4]*a[11]*a[13] - a[8]*a[5]*a[15] + a[8]*a[7]*a[13] + a[12]*a[5]*a[11] - a[12]*a[7]*a[9];
        inv[12] = -a[4]*a[9]*a[14] + a[4]*a[10]*a[13] + a[8]*a[5]*a[14] - a[8]*a[6]*a[13] - a[12]*a[5]*a[10] + a[12]*a[6]*a[9];
        inv[1] = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15] - a[9]*a[3]*a[14] - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
        inv[5] = a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15] + a[8]*a[3]*a[14] + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
        inv[9] = -a[0]*a[9]*a[15] + a[0]*a[11]*a[13] + a[8]*a[1]*a[15] - a[8]*a[3]*a[13] - a[12]*a[1]*a[11] + a[12]*a[3]*a[9];
        inv[13] = a[0]*a[9]*a[14] - a[0]*a[10]*a[13] - a[8]*a[1]*a[14] + a[8]*a[2]*a[13] + a[12]*a[1]*a[10] - a[12]*a[2]*a[9];
        inv[2] = a[1]*a[6]*a[15] - a[1]*a[7]*a[14] - a[5]*a[2]*a[15] + a[5]*a[3]*a[14] + a[13]*a[2]*a[7] - a[13]*a[3]*a[6];
        inv[6] = -a[0]*a[6]*a[15] + a[0]*a[7]*a[14] + a[4]*a[2]*a[15] - a[4]*a[3]*a[14] - a[12]*a[2]*a[7] + a[12]*a[3]*a[6];
        inv[10] = a[0]*a[5]*a[15] - a[0]*a[7]*a[13] - a[4]*a[1]*a[15] + a[4]*a[3]*a[13] + a[12]*a[1]*a[7] - a[12]*a[3]*a[5];
        inv[14] = -a[0]*a[5]*a[14] + a[0]*a[6]*a[13] + a[4]*a[1]*a[14] - a[4]*a[2]*a[13] - a[12]*a[1]*a[6] + a[12]*a[2]*a[5];
        inv[3] = -a[1]*a[6]*a[11] + a[1]*a[7]*a[10] + a[5]*a[2]*a[11] - a[5]*a[3]*a[10] - a[9]*a[2]*a[7] + a[9]*a[3]*a[6];
        inv[7] = a[0]*a[6]*a[11] - a[0]*a[7]*a[10] - a[4]*a[2]*a[11] + a[4]*a[3]*a[10] + a[8]*a[2]*a[7] - a[8]*a[3]*a[6];
        inv[11] = -a[0]*a[5]*a[11] + a[0]*a[7]*a[9] + a[4]*a[1]*a[11] - a[4]*a[3]*a[9] - a[8]*a[1]*a[7] + a[8]*a[3]*a[5];
        inv[15] = a[0]*a[5]*a[10] - a[0]*a[6]*a[9] - a[4]*a[1]*a[10] + a[4]*a[2]*a[9] + a[8]*a[1]*a[6] - a[8]*a[2]*a[5];
        float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
        Mat4 r;
        if (std::fabs(det) < 1e-12f) return r;
        det = 1.0f / det;
        for (int i = 0; i < 16; ++i) r.m[i] = inv[i] * det;
        return r;
    }
};

// Descompone una matriz afín en traslación, rotación Euler (orden Ry*Rx*Rz) y escala.
inline void decomposeTRS(const Mat4& M, Vec3& t, Vec3& rotDeg, Vec3& s) {
    t = M.translation();
    Vec3 c0 = M.column(0), c1 = M.column(1), c2 = M.column(2);
    s = Vec3(length(c0), length(c1), length(c2));
    if (s.x < 1e-8f) s.x = 1e-8f;
    if (s.y < 1e-8f) s.y = 1e-8f;
    if (s.z < 1e-8f) s.z = 1e-8f;
    c0 = c0 / s.x; c1 = c1 / s.y; c2 = c2 / s.z;
    // R[fila][col] : columna c => (c.x, c.y, c.z)
    float r02 = c2.x, r12 = c2.y, r22 = c2.z;
    float r10 = c0.y, r11 = c1.y;
    float r00 = c0.x, r20 = c0.z;
    float sa = clampf(-r12, -1.0f, 1.0f);
    float a = std::asin(sa);
    float b, c;
    if (std::fabs(sa) < 0.9999f) {
        b = std::atan2(r02, r22);
        c = std::atan2(r10, r11);
    } else {
        c = 0.0f;
        b = std::atan2(-r20, r00);
    }
    rotDeg = Vec3(rad2deg(a), rad2deg(b), rad2deg(c));
}
