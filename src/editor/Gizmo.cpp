#include "editor/Gizmo.h"
#include "render/GL.h"
#include <cmath>

namespace {

void axisColor(int handle, int hover, bool pick) {
    if (pick) { glColor3ub((GLubyte)handle, 0, 0); return; }
    if (handle == hover) { glColor3f(1.0f, 0.85f, 0.15f); return; }
    switch (handle) {
    case 1: glColor3f(0.95f, 0.25f, 0.25f); break;
    case 2: glColor3f(0.35f, 0.88f, 0.30f); break;
    case 3: glColor3f(0.30f, 0.52f, 1.00f); break;
    default: glColor3f(0.92f, 0.92f, 0.92f); break;
    }
}

void basis(const Vec3& a, Vec3& u, Vec3& v) {
    Vec3 ref = std::fabs(a.y) < 0.9f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    u = normalize(cross(a, ref));
    v = cross(a, u);
}

void cone(const Vec3& base, const Vec3& dir, float len, float radius) {
    Vec3 u, v;
    basis(dir, u, v);
    Vec3 tip = base + dir * len;
    const int N = 16;
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(tip.x, tip.y, tip.z);
    for (int i = 0; i <= N; ++i) {
        float t = 2.0f * PI_F * i / N;
        Vec3 p = base + (u * std::cos(t) + v * std::sin(t)) * radius;
        glVertex3f(p.x, p.y, p.z);
    }
    glEnd();
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(base.x, base.y, base.z);
    for (int i = N; i >= 0; --i) {
        float t = 2.0f * PI_F * i / N;
        Vec3 p = base + (u * std::cos(t) + v * std::sin(t)) * radius;
        glVertex3f(p.x, p.y, p.z);
    }
    glEnd();
}

void box(const Vec3& c, float h, const Vec3* ax) {
    Vec3 X = ax[0] * h, Y = ax[1] * h, Z = ax[2] * h;
    Vec3 p[8];
    for (int i = 0; i < 8; ++i)
        p[i] = c + X * ((i & 1) ? 1.0f : -1.0f) + Y * ((i & 2) ? 1.0f : -1.0f) + Z * ((i & 4) ? 1.0f : -1.0f);
    static const int f[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1}, {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
    glBegin(GL_QUADS);
    for (int i = 0; i < 6; ++i)
        for (int k = 0; k < 4; ++k) glVertex3f(p[f[i][k]].x, p[f[i][k]].y, p[f[i][k]].z);
    glEnd();
}

void line(const Vec3& a, const Vec3& b) {
    glBegin(GL_LINES);
    glVertex3f(a.x, a.y, a.z);
    glVertex3f(b.x, b.y, b.z);
    glEnd();
}

} // namespace

namespace Gizmo {

void draw(const GizmoFrame& f, int mode, int hover, bool pick, float ls) {
    if (mode == GIZMO_SELECT) return;
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    if (!pick) {
        glEnable(GL_LINE_SMOOTH);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    float s = f.size;
    float lw = (pick ? 10.0f : 3.0f) * ls;
    static const Vec3 worldAxes[3] = {Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1)};
    (void)worldAxes;

    if (mode == GIZMO_MOVE || mode == GIZMO_SCALE) {
        for (int i = 0; i < 3; ++i) {
            int h = i + 1;
            axisColor(h, hover, pick);
            glLineWidth(h == hover && !pick ? lw * 1.4f : lw);
            Vec3 end = f.origin + f.axis[i] * s;
            line(f.origin + f.axis[i] * (s * 0.12f), end);
            if (mode == GIZMO_MOVE) cone(end, f.axis[i], s * 0.24f, s * (pick ? 0.10f : 0.065f));
            else box(end, s * (pick ? 0.09f : 0.06f), f.axis);
        }
        axisColor(4, hover, pick);
        if (mode == GIZMO_MOVE) {
            // cuadro centrado orientado a la cámara (movimiento libre en el plano de la vista)
            float q = s * (pick ? 0.13f : 0.09f);
            Vec3 R = f.camRight * q, U = f.camUp * q;
            Vec3 o = f.origin;
            if (!pick && hover != 4) glColor4f(0.95f, 0.95f, 0.95f, 0.85f);
            glBegin(GL_QUADS);
            Vec3 a = o - R - U, b = o + R - U, c = o + R + U, d = o - R + U;
            glVertex3f(a.x, a.y, a.z); glVertex3f(b.x, b.y, b.z); glVertex3f(c.x, c.y, c.z); glVertex3f(d.x, d.y, d.z);
            glEnd();
        } else {
            box(f.origin, s * (pick ? 0.12f : 0.08f), f.axis);
        }
    } else if (mode == GIZMO_ROTATE) {
        const int N = 72;
        for (int i = 0; i < 3; ++i) {
            int h = i + 1;
            axisColor(h, hover, pick);
            glLineWidth(h == hover && !pick ? lw * 1.4f : lw);
            Vec3 u, v;
            basis(f.axis[i], u, v);
            glBegin(GL_LINE_LOOP);
            for (int k = 0; k < N; ++k) {
                float t = 2.0f * PI_F * k / N;
                Vec3 p = f.origin + (u * std::cos(t) + v * std::sin(t)) * (s * 0.9f);
                glVertex3f(p.x, p.y, p.z);
            }
            glEnd();
        }
        if (!pick) {
            // círculo exterior de referencia orientado a la cámara
            glColor4f(0.8f, 0.8f, 0.8f, 0.35f);
            glLineWidth(1.5f * ls);
            glBegin(GL_LINE_LOOP);
            for (int k = 0; k < N; ++k) {
                float t = 2.0f * PI_F * k / N;
                Vec3 p = f.origin + (f.camRight * std::cos(t) + f.camUp * std::sin(t)) * (s * 1.0f);
                glVertex3f(p.x, p.y, p.z);
            }
            glEnd();
        }
    }
    glLineWidth(1.0f);
    if (!pick) glDisable(GL_LINE_SMOOTH);
}

} // namespace Gizmo
