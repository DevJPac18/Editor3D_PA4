#include "render/Renderer.h"
#include "render/GL.h"
#include "render/GLExt.h"
#include <cmath>

bool Renderer::init() {
    glEnable(GL_NORMALIZE);
    glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    shaderOk = shader.init();
    info = glInfoString();
    return true;
}

void Renderer::addItems(SceneNode* n, const Mat4& parent, bool parentVisible) {
    DrawItem it;
    it.node = n;
    it.world = parent * n->localMatrix();
    it.visible = parentVisible && n->visible;
    items.push_back(it);
    Mat4 w = it.world;
    bool v = it.visible;
    for (size_t i = 0; i < n->children.size(); ++i) addItems(n->children[i], w, v);
}

void Renderer::buildList(Scene& scene) {
    items.clear();
    for (size_t i = 0; i < scene.root->children.size(); ++i) addItems(scene.root->children[i], Mat4(), true);
}

bool Renderer::inSubtree(SceneNode* root, SceneNode* n) const {
    return n == root || root->isAncestorOf(n);
}

void Renderer::setupCamera(const Camera& cam, const ViewportPx& vp, Mat4& V) {
    glViewport(vp.x, vp.y, vp.w, vp.h);
    glScissor(vp.x, vp.y, vp.w, vp.h);
    glEnable(GL_SCISSOR_TEST);
    float aspect = vp.h > 0 ? (float)vp.w / (float)vp.h : 1.0f;
    Mat4 P = cam.projection(aspect);
    V = cam.view();
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(P.m);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(V.m);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
}

void Renderer::setupLights(Scene& scene, const Mat4& V) {
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(V.m); // las posiciones de luz se transforman a espacio de vista
    for (int i = 0; i < 8; ++i) glDisable(GL_LIGHT0 + i);
    float amb[4] = {scene.ambient, scene.ambient, scene.ambient, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
    int n = 0;
    hasShadowLight = false;
    for (size_t i = 0; i < items.size() && n < 2; ++i) {
        const DrawItem& it = items[i];
        if (it.node->type != NODE_LIGHT || !it.visible) continue;
        const LightProps& L = it.node->light;
        Vec3 p = it.world.translation();
        float pos[4];
        if (L.directional) {
            Vec3 d = length(p) > 1e-4f ? normalize(p) : Vec3(0, 1, 0);
            pos[0] = d.x; pos[1] = d.y; pos[2] = d.z; pos[3] = 0.0f;
        } else {
            pos[0] = p.x; pos[1] = p.y; pos[2] = p.z; pos[3] = 1.0f;
        }
        GLenum id = GL_LIGHT0 + n;
        glEnable(id);
        glLightfv(id, GL_POSITION, pos);
        Vec3 c = L.color * L.intensity;
        float dif[4] = {c.x, c.y, c.z, 1.0f};
        float am[4] = {c.x * 0.06f, c.y * 0.06f, c.z * 0.06f, 1.0f};
        glLightfv(id, GL_DIFFUSE, dif);
        glLightfv(id, GL_SPECULAR, dif);
        glLightfv(id, GL_AMBIENT, am);
        glLightf(id, GL_CONSTANT_ATTENUATION, 1.0f);
        glLightf(id, GL_LINEAR_ATTENUATION, 0.0f);
        glLightf(id, GL_QUADRATIC_ATTENUATION, 0.0f);
        if (n == 0) {
            hasShadowLight = true;
            shadowLight = Vec4(pos[0], pos[1], pos[2], pos[3]);
        }
        ++n;
    }
    if (n == 0) { // luz por defecto si la escena no tiene luces
        Vec3 d = normalize(Vec3(0.4f, 1.0f, 0.5f));
        float pos[4] = {d.x, d.y, d.z, 0.0f};
        float dif[4] = {0.85f, 0.85f, 0.85f, 1.0f};
        float am[4] = {0.05f, 0.05f, 0.05f, 1.0f};
        glEnable(GL_LIGHT0);
        glLightfv(GL_LIGHT0, GL_POSITION, pos);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
        glLightfv(GL_LIGHT0, GL_SPECULAR, dif);
        glLightfv(GL_LIGHT0, GL_AMBIENT, am);
        hasShadowLight = true;
        shadowLight = Vec4(d.x, d.y, d.z, 0.0f);
        n = 1;
    }
    light0 = n >= 1;
    light1 = n >= 2;
}

void Renderer::applyMaterial(const Material& m) {
    const Vec3& c = m.color;
    float a[4] = {c.x * m.ambient, c.y * m.ambient, c.z * m.ambient, 1.0f};
    float d[4] = {c.x * m.diffuse, c.y * m.diffuse, c.z * m.diffuse, 1.0f};
    float s[4] = {m.specular, m.specular, m.specular, 1.0f};
    float e[4] = {c.x * m.emission, c.y * m.emission, c.z * m.emission, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, a);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, d);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, s);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, e);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, clampf(m.shininess, 1.0f, 128.0f));
}

void Renderer::drawLit(const DrawItem& it, const Mat4& V, bool useShader) {
    const Mesh* mesh = meshes.get(it.node->mesh);
    if (!mesh) return;
    const Material& mat = it.node->material;
    GLuint tex = textures.get(mat.texture);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    if (tex) glScalef(mat.tiling, mat.tiling, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf((V * it.world).m);
    applyMaterial(mat);
    glBindTexture(GL_TEXTURE_2D, tex);
    if (useShader) shader.setTextured(tex != 0);
    else if (tex) glEnable(GL_TEXTURE_2D);
    else glDisable(GL_TEXTURE_2D);
    mesh->draw(true, tex != 0);
    if (!useShader) glDisable(GL_TEXTURE_2D);
}

void Renderer::drawBackground() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glColor3f(0.11f, 0.12f, 0.14f);
    glVertex2f(-1, -1);
    glVertex2f(1, -1);
    glColor3f(0.29f, 0.31f, 0.35f);
    glVertex2f(1, 1);
    glVertex2f(-1, 1);
    glEnd();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void Renderer::drawGrid(const RenderOptions& o) {
    const float y = 0.004f;
    const int N = 10;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glLineWidth(1.0f * o.lineScale);
    glBegin(GL_LINES);
    for (int i = -N; i <= N; ++i) {
        if (i == 0) continue;
        float a = (i % 5 == 0) ? 0.30f : 0.14f;
        glColor4f(0.80f, 0.82f, 0.86f, a);
        glVertex3f((float)i, y, (float)-N); glVertex3f((float)i, y, (float)N);
        glVertex3f((float)-N, y, (float)i); glVertex3f((float)N, y, (float)i);
    }
    glEnd();
    glLineWidth(2.0f * o.lineScale);
    glBegin(GL_LINES);
    glColor4f(0.90f, 0.25f, 0.25f, 0.85f); glVertex3f((float)-N, y, 0); glVertex3f((float)N, y, 0);
    glColor4f(0.25f, 0.45f, 0.95f, 0.85f); glVertex3f(0, y, (float)-N); glVertex3f(0, y, (float)N);
    glColor4f(0.35f, 0.85f, 0.30f, 0.70f); glVertex3f(0, 0, 0); glVertex3f(0, 4.0f, 0);
    glEnd();
    glLineWidth(1.0f);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::drawShadows(const Mat4& V, float floorY) {
    float L[4] = {shadowLight.x, shadowLight.y, shadowLight.z, shadowLight.w};
    if (L[3] > 0.0f && L[1] <= floorY + 0.05f) return;
    if (L[3] == 0.0f && L[1] <= 0.02f) return;
    float P[4] = {0.0f, 1.0f, 0.0f, -floorY};
    float d = P[0] * L[0] + P[1] * L[1] + P[2] * L[2] + P[3] * L[3];
    Mat4 S;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) S.at(r, c) = (r == c ? d : 0.0f) - L[r] * P[c];

    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR); // cada píxel se oscurece una sola vez
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f, -2.0f);
    glColor4f(0.0f, 0.0f, 0.0f, 0.38f);
    glMatrixMode(GL_MODELVIEW);
    for (size_t i = 0; i < items.size(); ++i) {
        const DrawItem& it = items[i];
        SceneNode* n = it.node;
        if (!it.visible || n->type != NODE_MESH || !n->castShadow || n->receiveShadow) continue;
        const Mesh* mesh = meshes.get(n->mesh);
        if (!mesh) continue;
        glLoadMatrixf((V * S * it.world).m);
        mesh->draw(false, false);
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
}

void Renderer::drawSelection(const Mat4& V, SceneNode* sel, const RenderOptions& o) {
    std::vector<const DrawItem*> list;
    for (size_t i = 0; i < items.size(); ++i)
        if (items[i].visible && items[i].node->type == NODE_MESH && inSubtree(sel, items[i].node))
            list.push_back(&items[i]);
    if (list.empty()) return;
    // 1) marcar la silueta en el stencil   2) dibujar alambre grueso fuera de ella = contorno
    glClear(GL_STENCIL_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    glMatrixMode(GL_MODELVIEW);
    for (size_t i = 0; i < list.size(); ++i) {
        const Mesh* m = meshes.get(list[i]->node->mesh);
        if (!m) continue;
        glLoadMatrixf((V * list[i]->world).m);
        m->draw(false, false);
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(3.5f * o.lineScale);
    glColor3f(0.30f, 0.72f, 1.0f);
    for (size_t i = 0; i < list.size(); ++i) {
        const Mesh* m = meshes.get(list[i]->node->mesh);
        if (!m) continue;
        glLoadMatrixf((V * list[i]->world).m);
        m->draw(false, false);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glLineWidth(1.0f);
    glDepthMask(GL_TRUE);
    glDisable(GL_STENCIL_TEST);
}

static void ringVerts(const Vec3& c, const Vec3& axis, float r) {
    Vec3 ref = std::fabs(axis.y) < 0.9f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    Vec3 u = normalize(cross(axis, ref)), v = cross(axis, u);
    glBegin(GL_LINE_LOOP);
    for (int k = 0; k < 40; ++k) {
        float t = 2.0f * PI_F * k / 40;
        Vec3 p = c + (u * std::cos(t) + v * std::sin(t)) * r;
        glVertex3f(p.x, p.y, p.z);
    }
    glEnd();
}

static void jointGlyph(const DrawItem& it) {
    const JointInfo& j = it.node->joint;
    Vec3 c = it.world.translation();
    Vec3 a = normalize(it.world.column(j.axis));
    if (j.type == JOINT_REVOLUTE) {
        ringVerts(c, a, 0.34f);
        glBegin(GL_LINES);
        Vec3 p0 = c - a * 0.2f, p1 = c + a * 0.2f;
        glVertex3f(p0.x, p0.y, p0.z); glVertex3f(p1.x, p1.y, p1.z);
        glEnd();
    } else {
        Vec3 p0 = c - a * 0.25f, p1 = c + a * 0.25f;
        glBegin(GL_LINES);
        glVertex3f(p0.x, p0.y, p0.z); glVertex3f(p1.x, p1.y, p1.z);
        glEnd();
        glPointSize(6.0f);
        glBegin(GL_POINTS);
        glVertex3f(p0.x, p0.y, p0.z); glVertex3f(p1.x, p1.y, p1.z);
        glEnd();
        glPointSize(1.0f);
    }
}

void Renderer::drawHelpers(const Mat4& V, SceneNode* sel, const RenderOptions& o) {
    const Mesh* sph = meshes.get("sphere");
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glMatrixMode(GL_MODELVIEW);
    for (int pass = 0; pass < 2; ++pass) {
        // pasada 0: con prueba de profundidad. pasada 1: "rayos X" tenue para articulaciones ocultas
        if (pass == 0) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        for (size_t i = 0; i < items.size(); ++i) {
            const DrawItem& it = items[i];
            SceneNode* n = it.node;
            if (!it.visible) continue;
            bool isSel = sel && (n == sel);
            Vec3 p = it.world.translation();
            if (n->type == NODE_LIGHT && pass == 0) {
                Vec3 c = n->light.color;
                if (isSel) glColor3f(0.35f, 0.8f, 1.0f); else glColor3f(0.6f + 0.4f * c.x, 0.6f + 0.4f * c.y, 0.4f + 0.3f * c.z);
                if (sph) {
                    glLoadMatrixf((V * Mat4::translate(p) * Mat4::scale(Vec3(0.22f, 0.22f, 0.22f))).m);
                    sph->draw(false, false);
                }
                glLoadMatrixf(V.m);
                glLineWidth(1.5f * o.lineScale);
                glBegin(GL_LINES);
                if (n->light.directional) {
                    Vec3 d = normalize(-p);
                    for (int k = -1; k <= 1; ++k) {
                        Vec3 off = normalize(cross(d, Vec3(0, 1, 0))) * (0.15f * k);
                        Vec3 a = p + off, b = p + off + d * 1.2f;
                        glVertex3f(a.x, a.y, a.z); glVertex3f(b.x, b.y, b.z);
                    }
                } else {
                    for (int k = 0; k < 8; ++k) {
                        float t = 2.0f * PI_F * k / 8;
                        Vec3 d = normalize(Vec3(std::cos(t), -0.7f, std::sin(t)));
                        Vec3 a = p + d * 0.2f, b = p + d * 0.75f;
                        glVertex3f(a.x, a.y, a.z); glVertex3f(b.x, b.y, b.z);
                    }
                }
                glEnd();
                // línea punteada hasta el suelo para ubicar la luz
                glEnable(GL_LINE_STIPPLE);
                glLineStipple(2, 0x3333);
                glColor4f(1.0f, 0.9f, 0.5f, 0.35f);
                glBegin(GL_LINES);
                glVertex3f(p.x, p.y, p.z); glVertex3f(p.x, 0.0f, p.z);
                glEnd();
                glDisable(GL_LINE_STIPPLE);
            } else if (n->isJoint()) {
                glLoadMatrixf(V.m);
                glLineWidth((isSel ? 3.0f : 2.0f) * o.lineScale);
                float a = pass == 0 ? 0.95f : 0.28f;
                if (isSel) glColor4f(0.35f, 0.8f, 1.0f, a); else glColor4f(1.0f, 0.62f, 0.12f, a);
                jointGlyph(it);
            } else if (n->type == NODE_EMPTY && pass == 0) {
                glLoadMatrixf(V.m);
                glLineWidth((isSel ? 2.5f : 1.5f) * o.lineScale);
                glBegin(GL_LINES);
                for (int k = 0; k < 3; ++k) {
                    Vec3 d = normalize(it.world.column(k)) * 0.3f;
                    if (k == 0) glColor4f(0.95f, 0.3f, 0.3f, 0.8f);
                    else if (k == 1) glColor4f(0.4f, 0.9f, 0.35f, 0.8f);
                    else glColor4f(0.35f, 0.55f, 1.0f, 0.8f);
                    glVertex3f(p.x, p.y, p.z);
                    glVertex3f(p.x + d.x, p.y + d.y, p.z + d.z);
                }
                glEnd();
            }
        }
    }
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LINE_SMOOTH);
    glLineWidth(1.0f);
    glDisable(GL_BLEND);
}

void Renderer::render(Scene& scene, const Camera& cam, const ViewportPx& vp, const RenderOptions& o,
                      SceneNode* sel, const GizmoFrame* gizmo, int gizmoMode, int gizmoHover) {
    Mat4 V;
    setupCamera(cam, vp, V);
    glClearDepth(1.0);
    glClearStencil(0);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    drawBackground();
    setupLights(scene, V);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    bool useShader = o.shaders && shaderOk;
    if (useShader) {
        shader.bind();
        shader.setLights(light0, light1);
        glDisable(GL_LIGHTING);
    } else {
        glEnable(GL_LIGHTING);
    }
    if (o.wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // receptores de sombra (suelo) primero: marcan el stencil
    float floorY = 0.0f;
    bool haveFloor = false;
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    for (size_t i = 0; i < items.size(); ++i) {
        const DrawItem& it = items[i];
        if (!it.visible || it.node->type != NODE_MESH || !it.node->receiveShadow) continue;
        drawLit(it, V, useShader);
        if (!haveFloor) { floorY = it.world.translation().y; haveFloor = true; }
    }
    glDisable(GL_STENCIL_TEST);
    for (size_t i = 0; i < items.size(); ++i) {
        const DrawItem& it = items[i];
        if (!it.visible || it.node->type != NODE_MESH || it.node->receiveShadow) continue;
        drawLit(it, V, useShader);
    }
    if (useShader) shader.unbind();
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glLoadMatrixf(V.m);

    if (o.grid) drawGrid(o);
    if (o.shadows && !o.wireframe && hasShadowLight && haveFloor) drawShadows(V, floorY);
    if (sel) drawSelection(V, sel, o);
    if (o.helpers) drawHelpers(V, sel, o);
    if (gizmo) {
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glLoadMatrixf(V.m);
        Gizmo::draw(*gizmo, gizmoMode, gizmoHover, false, o.lineScale);
    }
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
}

int Renderer::pickObject(const Camera& cam, const ViewportPx& vp, int px, int py, const RenderOptions& o) {
    Mat4 V;
    setupCamera(cam, vp, V);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_DITHER);
    glDisable(GL_LINE_SMOOTH);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    const Mesh* sph = meshes.get("sphere");
    glMatrixMode(GL_MODELVIEW);
    for (size_t i = 0; i < items.size(); ++i) {
        const DrawItem& it = items[i];
        if (!it.visible) continue;
        SceneNode* n = it.node;
        glColor3ub((GLubyte)((n->id >> 16) & 255), (GLubyte)((n->id >> 8) & 255), (GLubyte)(n->id & 255));
        Vec3 p = it.world.translation();
        if (n->type == NODE_MESH) {
            const Mesh* m = meshes.get(n->mesh);
            if (!m) continue;
            glLoadMatrixf((V * it.world).m);
            m->draw(false, false);
        } else if (n->type == NODE_LIGHT && sph) {
            glLoadMatrixf((V * Mat4::translate(p) * Mat4::scale(Vec3(0.35f, 0.35f, 0.35f))).m);
            sph->draw(false, false);
        } else if (o.helpers && n->isJoint()) {
            glLoadMatrixf(V.m);
            glLineWidth(7.0f * o.lineScale);
            jointGlyph(it);
            glLineWidth(1.0f);
        } else if (o.helpers && sph) {
            glLoadMatrixf((V * Mat4::translate(p) * Mat4::scale(Vec3(0.14f, 0.14f, 0.14f))).m);
            sph->draw(false, false);
        }
    }
    unsigned char pix[4] = {0, 0, 0, 0};
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(px, py, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pix);
    glEnable(GL_DITHER);
    glDisable(GL_SCISSOR_TEST);
    return (pix[0] << 16) | (pix[1] << 8) | pix[2];
}

int Renderer::pickGizmo(const Camera& cam, const ViewportPx& vp, int px, int py, const GizmoFrame& g, int mode,
                        const RenderOptions& o) {
    Mat4 V;
    setupCamera(cam, vp, V);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_DITHER);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    Gizmo::draw(g, mode, 0, true, o.lineScale);
    unsigned char pix[4] = {0, 0, 0, 0};
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(px, py, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pix);
    glEnable(GL_DITHER);
    glDisable(GL_SCISSOR_TEST);
    int h = pix[0];
    return (h >= 1 && h <= 4 && pix[1] == 0 && pix[2] == 0) ? h : 0;
}
