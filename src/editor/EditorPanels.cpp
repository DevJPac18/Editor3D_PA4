// EditorPanels.cpp - Paneles de la interfaz: menú, escena, propiedades, recursos, animación
#include "editor/Editor.h"
#include "render/GL.h"
#include "scene/SceneFactory.h"
#include "core/Utils.h"
#include <cmath>
#include <algorithm>
#include <functional>

namespace {
int ID(const char* s, int k = 0) { return (int)util::hash(s, (uint32_t)k); }
const float LABEL_W = 84.0f;
const float ROW = 22.0f;

uint32_t hexOf(const Vec3& c) {
    int r = (int)(clampf(c.x, 0, 1) * 255 + 0.5f), g = (int)(clampf(c.y, 0, 1) * 255 + 0.5f),
        b = (int)(clampf(c.z, 0, 1) * 255 + 0.5f);
    return (uint32_t)((r << 16) | (g << 8) | b);
}

void collectJoints(SceneNode* n, std::vector<SceneNode*>& out) {
    if (n->isJoint()) out.push_back(n);
    for (size_t i = 0; i < n->children.size(); ++i) collectJoints(n->children[i], out);
}

std::vector<std::string> list(const char* const* items, int n) {
    std::vector<std::string> v;
    for (int i = 0; i < n; ++i) v.push_back(items[i]);
    return v;
}
}

Rect Editor::panel(const Rect& r, const std::string& title) {
    gui.rect(r, Theme::PanelBg);
    gui.rect(r.x, r.y, r.w, 25, Theme::HeaderBg);
    gui.text(r.x + 10, r.y + (25 - gui.textHeight()) * 0.5f, title, Theme::Text, true);
    gui.rect(r.x, r.y + 25, r.w, 1, Theme::Border);
    return Rect(r.x, r.y + 26, r.w, r.h - 26);
}

// ---------------------------------------------------------------- barra de menú

void Editor::drawMenuBar() {
    gui.rect(rMenu, Theme::HeaderBg);
    gui.rect(0, rMenu.bottom() - 1, gui.W, 1, Theme::Border);
    float x = 6;
    auto slot = [&](const char* label) {
        float w = gui.textWidth(label) + 22;
        Rect r(x, 2, w, rMenu.h - 4);
        x += w + 2;
        return r;
    };

    {
        const char* it[] = {"Nuevo\tCtrl+N", "Abrir...\tCtrl+O", "Guardar\tCtrl+S", "Guardar como...\tCtrl+Shift+S", "-",
                            "Cargar escena de demostración", "Importar modelo OBJ...", "-", "Salir"};
        switch (gui.menu(ID("m_file"), slot("Archivo"), "Archivo", list(it, 9))) {
        case 0: newScene(); break;
        case 1: openScene(); break;
        case 2: saveScene(false); break;
        case 3: saveScene(true); break;
        case 5: loadDemo(); break;
        case 6: importObjDialog(); break;
        case 8: if (canQuit()) quit = true; break;
        }
    }
    {
        const char* it[] = {"Cubo", "Esfera", "Cilindro", "Cono", "Plano", "Toro", "-", "Grupo vacío (pivote)",
                            "Luz puntual", "Luz direccional", "-", "Brazo robótico (articulado)",
                            "Sistema solar (jerarquía animada)", "-", "Modelo OBJ..."};
        int s = gui.menu(ID("m_create"), slot("Crear"), "Crear", list(it, 15));
        static const char* prims[6] = {"cube", "sphere", "cylinder", "cone", "plane", "torus"};
        if (s >= 0 && s <= 5) addPrimitive(prims[s]);
        else if (s == 7) addEmpty();
        else if (s == 8) addLight(false);
        else if (s == 9) addLight(true);
        else if (s == 11) addRobot();
        else if (s == 12) addSolarSystem();
        else if (s == 14) importObjDialog();
    }
    {
        const char* it[] = {"Deshacer\tCtrl+Z", "Rehacer\tCtrl+Y", "-", "Duplicar\tCtrl+D", "Eliminar\tSupr", "-",
                            "Seleccionar padre\tP", "Quitar de la jerarquía", "Ocultar / mostrar\tH"};
        switch (gui.menu(ID("m_edit"), slot("Editar"), "Editar", list(it, 9))) {
        case 0: undo(); break;
        case 1: redo(); break;
        case 3: duplicateSelected(); break;
        case 4: deleteSelected(); break;
        case 6: selectParent(); break;
        case 7: unparentSelected(); break;
        case 8: {
            SceneNode* n = selectedNode();
            if (n) { pushUndo(); n->visible = !n->visible; }
            break;
        }
        }
    }
    {
        std::vector<std::string> it;
        it.push_back(std::string(opts.grid ? "!" : "?") + "Cuadrícula\tG");
        it.push_back(std::string(opts.shadows ? "!" : "?") + "Sombras proyectadas");
        it.push_back(std::string(opts.wireframe ? "!" : "?") + "Modo alambre (wireframe)");
        it.push_back(std::string(opts.shaders ? "!" : "?") + "Shaders GLSL (OpenGL 2.0)");
        it.push_back(std::string(opts.helpers ? "!" : "?") + "Ayudas (luces, articulaciones)");
        it.push_back("-");
        it.push_back("Enfocar selección\tF");
        it.push_back("Restablecer cámara");
        it.push_back("Vista frontal\t1");
        it.push_back("Vista lateral\t3");
        it.push_back("Vista superior\t7");
        switch (gui.menu(ID("m_view"), slot("Ver"), "Ver", it)) {
        case 0: opts.grid = !opts.grid; break;
        case 1: opts.shadows = !opts.shadows; break;
        case 2: opts.wireframe = !opts.wireframe; break;
        case 3:
            if (renderer.glslAvailable()) opts.shaders = !opts.shaders;
            else setStatus("GLSL no disponible en este equipo: se usa el pipeline fijo.");
            break;
        case 4: opts.helpers = !opts.helpers; break;
        case 6: focusSelected(); break;
        case 7: camera.reset(); break;
        case 8: camera.setView(0, 0); break;
        case 9: camera.setView(90, 0); break;
        case 10: camera.setView(0, 89.5f); break;
        }
    }
    {
        const char* it[] = {"Reproducir / Pausa\tEspacio", "Detener (restaurar)", "-", "Nueva pista de animación",
                            "Animar articulaciones del modelo", "Demo: animar brazo robótico", "-",
                            "Eliminar todas las animaciones"};
        switch (gui.menu(ID("m_anim"), slot("Animación"), "Animación", list(it, 8))) {
        case 0: togglePlay(); break;
        case 1: stopAnimation(); break;
        case 3: newAnimation(); break;
        case 4: animateSelectedModel(); break;
        case 5: animateRobotDemo(); break;
        case 7:
            if (!scene.animations.empty()) {
                stopAnimation();
                pushUndo();
                scene.animations.clear();
                setStatus("Se eliminaron todas las animaciones.");
            }
            break;
        }
    }
    {
        const char* it[] = {"Controles\tF1", "Acerca de"};
        switch (gui.menu(ID("m_help"), slot("Ayuda"), "Ayuda", list(it, 2))) {
        case 0: helpRequested = true; break;
        case 1:
            setStatus("Editor 3D - Proyecto PA4 - C++ / OpenGL 2.0 (GLSL 1.10) / Win32. " + renderer.info);
            break;
        }
    }
    std::string f = currentFile.empty() ? std::string("Sin título") : currentFile;
    if (dirty) f += "  (modificado)";
    gui.text(gui.W - gui.textWidth(f) - 12, (rMenu.h - gui.textHeight()) * 0.5f, f, Theme::TextDim);
}

// ---------------------------------------------------------------- árbol de escena

void Editor::drawNodeIcon(SceneNode* n, float x, float y) {
    if (!n) { // cámara
        gui.rect(x + 1, y + 4, 9, 7, 0xb4b9c1);
        gui.triangle(x + 10, y + 7.5f, x + 14, y + 4, x + 14, y + 11, 0xb4b9c1);
        return;
    }
    if (n->type == NODE_LIGHT) {
        gui.circle(x + 7, y + 7, 3.5f, 0xffd24a);
        for (int k = 0; k < 8; ++k) {
            float t = k * PI_F / 4;
            gui.line(x + 7 + std::cos(t) * 5, y + 7 + std::sin(t) * 5, x + 7 + std::cos(t) * 7, y + 7 + std::sin(t) * 7,
                     0xffd24a, 1.2f);
        }
    } else if (n->type == NODE_MESH) {
        uint32_t c = hexOf(n->material.color);
        if (n->mesh == "sphere") gui.circle(x + 7, y + 7, 5.5f, c);
        else if (n->mesh == "cylinder" || n->mesh == "cone") {
            gui.rect(x + 3, y + 3, 8, 9, c);
            gui.circle(x + 7, y + 3, 4, c);
        } else if (n->mesh == "torus") {
            gui.circle(x + 7, y + 7, 5.5f, c, false, 1.0f, 2.2f);
        } else if (n->mesh == "plane") {
            gui.rect(x + 1, y + 8, 12, 3, c);
        } else {
            gui.rect(x + 2, y + 2, 10, 10, c);
            gui.rectOutline(Rect(x + 2, y + 2, 10, 10), 0x000000, 0.4f);
        }
    } else if (n->isJoint()) {
        gui.circle(x + 7, y + 7, 5, Theme::Orange, false, 1.0f, 1.8f);
        gui.circle(x + 7, y + 7, 2, Theme::Orange);
    } else {
        gui.rect(x + 1, y + 4, 12, 8, 0xd9a441);
        gui.rect(x + 1, y + 2, 5, 3, 0xd9a441);
    }
}

void Editor::drawScenePanel(const Rect& r) {
    Rect c = panel(r, "Escena");
    float bw = (c.w - 16 - 8) / 3;
    float by = c.y + 5;
    SceneNode* sel = selectedNode();
    if (gui.button(ID("sc_dup"), Rect(c.x + 8, by, bw, ROW), "Duplicar", false, sel != nullptr)) duplicateSelected();
    if (gui.button(ID("sc_del"), Rect(c.x + 12 + bw, by, bw, ROW), "Eliminar", false, sel != nullptr)) deleteSelected();
    if (gui.button(ID("sc_up"), Rect(c.x + 16 + 2 * bw, by, bw, ROW), "A la raíz", false,
                   sel && sel->parent != scene.root))
        unparentSelected();

    Rect listR(c.x, by + ROW + 6, c.w, c.bottom() - (by + ROW + 6) - 4);
    struct Row { SceneNode* n; int depth; };
    std::vector<Row> rows;
    Row cam = {nullptr, 0};
    rows.push_back(cam);
    std::function<void(SceneNode*, int)> add = [&](SceneNode* n, int d) {
        Row rr = {n, d};
        rows.push_back(rr);
        if (n->expanded)
            for (size_t i = 0; i < n->children.size(); ++i) add(n->children[i], d + 1);
    };
    for (size_t i = 0; i < scene.root->children.size(); ++i) add(scene.root->children[i], 0);

    const float RH = 21.0f;
    float contentH = rows.size() * RH + 6;
    if (gui.mouseIn(listR) && gui.wheel != 0) treeScroll -= gui.wheel * RH * 3;
    treeScroll = clampf(treeScroll, 0, std::max(0.0f, contentH - listR.h));

    SceneNode* dropTarget = nullptr;
    bool overRow = false;
    float th = gui.textHeight();
    gui.pushClip(listR);
    for (size_t i = 0; i < rows.size(); ++i) {
        float ry = listR.y + 3 + i * RH - treeScroll;
        if (ry + RH < listR.y || ry > listR.bottom()) continue;
        SceneNode* n = rows[i].n;
        Rect rr(listR.x + 4, ry, listR.w - 8, RH);
        int nid = n ? n->id : (int)CAMERA_ID;
        bool isSel = nid == selectedId;
        bool hov = gui.mouseIn(rr);
        if (hov) { overRow = true; dropTarget = n; }
        if (isSel) gui.rect(rr, Theme::Accent);
        else if (hov) gui.rect(rr, Theme::WidgetHover);
        if (treeDragging && hov && n && n->id != treeDragId) gui.rectOutline(rr, 0x6cc4ff, 1.0f, 2.0f);

        float ix = rr.x + 4 + rows[i].depth * 14;
        bool hasKids = n && !n->children.empty();
        Rect arrow(ix, ry, 14, RH);
        if (hasKids) {
            float ax = ix + 7, ay = ry + RH * 0.5f;
            if (n->expanded) gui.triangle(ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3, Theme::TextDim);
            else gui.triangle(ax - 2, ay - 4, ax + 3, ay, ax - 2, ay + 4, Theme::TextDim);
        }
        drawNodeIcon(n, ix + 15, ry + 3.5f);
        std::string label = n ? n->name : std::string("Cámara");
        uint32_t tc = (n && !n->visible) ? Theme::TextDim : Theme::Text;
        float tx = ix + 34;
        gui.text(tx, ry + (RH - th) * 0.5f, gui.fitText(label, rr.right() - 28 - tx), tc);

        Rect eye(rr.right() - 22, ry, 20, RH);
        if (n) {
            float ex = eye.x + 10, ey = ry + RH * 0.5f;
            uint32_t ec = n->visible ? (isSel ? 0xffffff : Theme::TextDim) : 0x5a5f67;
            gui.circle(ex, ey, 5, ec, false, 1.0f, 1.3f);
            if (n->visible) gui.circle(ex, ey, 2, ec);
            else gui.line(ex - 6, ey + 5, ex + 6, ey - 5, ec, 1.3f);
        }
        if (n && gui.clickIn(eye)) {
            pushUndo();
            n->visible = !n->visible;
        } else if (hasKids && gui.clickIn(arrow)) {
            n->expanded = !n->expanded;
        } else if (gui.clickIn(rr)) {
            select(nid);
            if (n) {
                treeDragId = n->id;
                treeDragX = gui.mx;
                treeDragY = gui.my;
                treeDragging = false;
                if (gui.doubleClick) focusSelected();
            }
        }
    }
    gui.popClip();
    if (contentH > listR.h) {
        float sh = listR.h * listR.h / contentH;
        float sy = listR.y + (listR.h - sh) * (treeScroll / (contentH - listR.h));
        gui.rect(listR.right() - 4, sy, 3, sh, Theme::TextDim, 0.5f);
    }

    // arrastrar y soltar en el árbol = cambiar el padre (jerarquía)
    if (treeDragId) {
        if (gui.mouseDown[0]) {
            if (!treeDragging && (std::fabs(gui.mx - treeDragX) > 5 || std::fabs(gui.my - treeDragY) > 5))
                treeDragging = true;
        } else {
            SceneNode* dn = scene.findById(treeDragId);
            if (treeDragging && dn && listR.contains(gui.mx, gui.my)) {
                SceneNode* np = overRow ? dropTarget : nullptr; // fuera de filas o sobre "Cámara" = raíz
                SceneNode* realParent = np ? np : scene.root;
                if (np != dn && !dn->isAncestorOf(np) && dn->parent != realParent) {
                    pushUndo();
                    scene.setParent(dn, np, true);
                    if (np) np->expanded = true;
                    setStatus(dn->name + (np ? " ahora es hijo de " + np->name : std::string(" movido a la raíz")));
                }
            }
            treeDragId = 0;
            treeDragging = false;
        }
    }
    if (treeDragging) {
        SceneNode* dn = scene.findById(treeDragId);
        if (dn) {
            std::string g = dn->name;
            float w = gui.textWidth(g) + 16;
            gui.rect(gui.mx + 12, gui.my + 6, w, 20, Theme::Accent, 0.85f);
            gui.text(gui.mx + 20, gui.my + 6 + (20 - th) * 0.5f, g);
        }
    }
}

// ---------------------------------------------------------------- propiedades

void Editor::section(const std::string& t) {
    py += 8;
    gui.text(px0, py, t, Theme::Text, true);
    py += gui.textHeight() + 4;
    gui.rect(px0, py, pw0, 1, Theme::Border);
    py += 7;
}

void Editor::propLabel(const std::string& l) {
    gui.text(px0, py + (ROW - gui.textHeight()) * 0.5f, gui.fitText(l, LABEL_W - 4), Theme::TextDim);
}

bool Editor::propVec3(const std::string& label, Vec3& v, float speed, const char* fmt, const char* key, float mn) {
    propLabel(label);
    float fx = px0 + LABEL_W, fw = (pw0 - LABEL_W - 8) / 3;
    static const uint32_t cols[3] = {0xd84a4a, 0x52b83f, 0x4a7ae8};
    bool ch = false;
    for (int i = 0; i < 3; ++i)
        ch |= gui.dragFloat(ID(key, i), Rect(fx + i * (fw + 4), py, fw, ROW), v[i], speed, fmt, mn, 1e9f, cols[i]);
    py += ROW + 4;
    return ch;
}

bool Editor::propFloat(const std::string& label, float& v, float speed, const char* fmt, const char* key, float mn,
                       float mx) {
    propLabel(label);
    bool ch = gui.dragFloat(ID(key), Rect(px0 + LABEL_W, py, pw0 - LABEL_W, ROW), v, speed, fmt, mn, mx);
    py += ROW + 4;
    return ch;
}

bool Editor::propSlider(const std::string& label, float& v, float mn, float mx, const char* fmt, const char* key) {
    propLabel(label);
    bool ch = gui.slider(ID(key), Rect(px0 + LABEL_W, py, pw0 - LABEL_W, ROW), v, mn, mx, fmt);
    py += ROW + 4;
    return ch;
}

void Editor::drawPropertiesPanel(const Rect& r) {
    Rect c = panel(r, "Propiedades");
    if (gui.mouseIn(c) && gui.wheel != 0) propScroll -= gui.wheel * 60;
    propScroll = clampf(propScroll, 0, std::max(0.0f, propContentH - c.h));
    gui.pushClip(c);
    px0 = c.x + 10;
    pw0 = c.w - 24;
    py = c.y + 2 - propScroll;
    float startY = py;
    SceneNode* n = selectedNode();
    if (selectedId == CAMERA_ID) propsCamera();
    else if (n) propsNode(n);
    else propsScene();
    propContentH = py - startY + 16;
    gui.popClip();
    if (propContentH > c.h) {
        float sh = c.h * c.h / propContentH;
        float sy = c.y + (c.h - sh) * (propScroll / (propContentH - c.h));
        gui.rect(c.right() - 5, sy, 3, sh, Theme::TextDim, 0.5f);
    }
}

void Editor::propsNode(SceneNode* n) {
    float th = gui.textHeight();
    section("Objeto");
    propLabel("Nombre:");
    std::string nm = n->name;
    if (gui.inputText(ID("p_name"), Rect(px0 + LABEL_W, py, pw0 - LABEL_W, ROW), nm) && !nm.empty()) n->name = nm;
    py += ROW + 4;

    propLabel("Tipo:");
    std::string type = n->type == NODE_MESH ? "Malla" : n->type == NODE_LIGHT ? "Luz"
                       : n->isJoint()        ? "Articulación (pivote)"
                                             : "Grupo vacío (pivote)";
    gui.text(px0 + LABEL_W + 6, py + (ROW - th) * 0.5f, type);
    py += ROW + 2;

    std::vector<SceneNode*> all;
    scene.collect(all);
    std::vector<std::string> names;
    std::vector<SceneNode*> cands;
    names.push_back("(ninguno: raíz)");
    cands.push_back(nullptr);
    int pidx = 0;
    for (size_t i = 0; i < all.size(); ++i) {
        SceneNode* a = all[i];
        if (a == n || n->isAncestorOf(a)) continue;
        names.push_back(std::string(a->depth() * 3, ' ') + a->name);
        cands.push_back(a);
        if (a == n->parent) pidx = (int)names.size() - 1;
    }
    propLabel("Padre:");
    if (gui.combo(ID("p_parent"), Rect(px0 + LABEL_W, py, pw0 - LABEL_W, ROW), names, pidx)) {
        if (scene.setParent(n, cands[pidx], true))
            setStatus(n->name + " ahora depende de " + (cands[pidx] ? cands[pidx]->name : std::string("la raíz")) +
                      " (posición global conservada).");
    }
    py += ROW + 4;

    gui.checkbox(ID("p_vis"), px0, py, "Visible", n->visible);
    if (n->type == NODE_MESH) {
        gui.checkbox(ID("p_cast"), px0 + 92, py, "Proyecta sombra", n->castShadow);
        py += 23;
        gui.checkbox(ID("p_recv"), px0, py, "Recibe sombras (superficie de suelo)", n->receiveShadow);
    }
    py += 25;

    section("Transformación (relativa al padre)");
    bool tch = false;
    tch |= propVec3("Posición", n->position, 0.02f, "%.2f", "p_pos");
    tch |= propVec3("Rotación", n->rotation, 0.5f, "%.1f", "p_rot");
    tch |= propVec3("Escala", n->scale, 0.01f, "%.2f", "p_scl", 0.001f);
    if (tch) n->clampJoint();
    Vec3 wp = n->worldPosition();
    gui.text(px0, py + 1, util::format("Posición global: (%.2f, %.2f, %.2f)", wp.x, wp.y, wp.z), Theme::TextDim);
    py += 21;
    float bw = (pw0 - 8) / 3;
    if (gui.button(ID("p_reset"), Rect(px0, py, bw, ROW), "Restablecer")) {
        pushUndo();
        n->rotation = Vec3();
        n->scale = Vec3(1, 1, 1);
        n->clampJoint();
    }
    if (gui.button(ID("p_focus"), Rect(px0 + bw + 4, py, bw, ROW), "Enfocar (F)")) focusSelected();
    if (gui.button(ID("p_up"), Rect(px0 + 2 * (bw + 4), py, bw, ROW), "Ir al padre (P)", false,
                   n->parent && n->parent != scene.root))
        selectParent();
    py += ROW + 4;

    // --- articulación propia
    section("Articulación");
    const char* jt[] = {"Ninguna", "Rotación (revoluta)", "Traslación (prismática)"};
    int jti = (int)n->joint.type;
    propLabel("Tipo:");
    if (gui.combo(ID("p_jtype"), Rect(px0 + LABEL_W, py, pw0 - LABEL_W, ROW), list(jt, 3), jti)) {
        n->joint.type = (JointType)jti;
        if (jti == JOINT_REVOLUTE) { n->joint.minV = -180; n->joint.maxV = 180; }
        else if (jti == JOINT_PRISMATIC) { n->joint.minV = -1; n->joint.maxV = 1; }
        n->clampJoint();
    }
    py += ROW + 4;
    if (n->isJoint()) {
        const char* ax[] = {"Eje X", "Eje Y", "Eje Z"};
        int ai = n->joint.axis;
        propLabel("Eje local:");
        if (gui.combo(ID("p_jaxis"), Rect(px0 + LABEL_W, py, pw0 - LABEL_W, ROW), list(ax, 3), ai)) n->joint.axis = ai;
        py += ROW + 4;
        bool rev = n->joint.type == JOINT_REVOLUTE;
        propLabel("Límites:");
        float hw = (pw0 - LABEL_W - 4) / 2;
        gui.dragFloat(ID("p_jmin"), Rect(px0 + LABEL_W, py, hw, ROW), n->joint.minV, rev ? 0.5f : 0.01f,
                      rev ? "mín %.0f°" : "mín %.2f", -1e6f, n->joint.maxV);
        gui.dragFloat(ID("p_jmax"), Rect(px0 + LABEL_W + hw + 4, py, hw, ROW), n->joint.maxV, rev ? 0.5f : 0.01f,
                      rev ? "máx %.0f°" : "máx %.2f", n->joint.minV, 1e6f);
        py += ROW + 4;
        float v = n->jointValue();
        if (propSlider("Valor:", v, n->joint.minV, n->joint.maxV, rev ? "%.1f°" : "%.3f", "p_jval"))
            n->setJointValue(v);
    } else {
        gui.text(px0, py, "Convierte este nodo en articulación para", Theme::TextDim);
        py += 17;
        gui.text(px0, py, "moverlo con límites (como el brazo robótico).", Theme::TextDim);
        py += 20;
    }

    // --- articulaciones de todo el modelo (control tipo "panel de robot")
    SceneNode* top = scene.topAncestor(n);
    std::vector<SceneNode*> joints;
    if (top) collectJoints(top, joints);
    if (!joints.empty()) {
        section("Articulaciones del modelo \"" + top->name + "\"");
        for (size_t i = 0; i < joints.size(); ++i) {
            SceneNode* j = joints[i];
            Rect lr(px0, py, LABEL_W, ROW);
            bool cur = j == n;
            bool hov = gui.mouseIn(lr);
            gui.text(px0, py + (ROW - th) * 0.5f, gui.fitText(j->name, LABEL_W - 4),
                     cur ? 0x6cc4ff : (hov ? Theme::Text : Theme::TextDim), cur);
            if (gui.clickIn(lr)) select(j->id);
            float v = j->jointValue();
            bool rev = j->joint.type == JOINT_REVOLUTE;
            if (gui.slider(ID("p_mj", j->id), Rect(px0 + LABEL_W, py, pw0 - LABEL_W, ROW), v, j->joint.minV,
                           j->joint.maxV, rev ? "%.1f°" : "%.3f"))
                j->setJointValue(v);
            py += ROW + 3;
        }
        py += 2;
        float hb = (pw0 - 4) / 2;
        if (gui.button(ID("p_rest"), Rect(px0, py, hb, ROW), "Pose de reposo")) {
            pushUndo();
            for (size_t i = 0; i < joints.size(); ++i) joints[i]->setJointValue(0.0f);
        }
        if (gui.button(ID("p_animj"), Rect(px0 + hb + 4, py, hb, ROW), "Animar articulaciones")) animateSelectedModel();
        py += ROW + 4;
    }

    if (n->type == NODE_MESH) {
        section("Malla");
        std::vector<std::string> meshNames, meshIds;
        const std::vector<std::string>& prims = MeshLibrary::primitives();
        for (size_t i = 0; i < prims.size(); ++i) {
            meshNames.push_back(MeshLibrary::displayName(prims[i]));
            meshIds.push_back(prims[i]);
        }
        for (size_t i = 0; i < modelFiles.size(); ++i) {
            meshNames.push_back("OBJ: " + util::fileName(modelFiles[i]));
            meshIds.push_back(modelFiles[i]);
        }
        int mi = -1;
        for (size_t i = 0; i < meshIds.size(); ++i)
            if (meshIds[i] == n->mesh) mi = (int)i;
        if (mi < 0) {
            meshNames.push_back("OBJ: " + util::fileName(n->mesh));
            meshIds.push_back(n->mesh);
            mi = (int)meshIds.size() - 1;
        }
        propLabel("Geometría:");
        if (gui.combo(ID("p_mesh"), Rect(px0 + LABEL_W, py, pw0 - LABEL_W, ROW), meshNames, mi)) n->mesh = meshIds[mi];
        py += ROW + 4;
        const Mesh* m = renderer.meshes.get(n->mesh);
        gui.text(px0, py, m ? util::format("%d vértices, %d triángulos", m->vertexCount(), m->triangleCount())
                            : std::string("No se pudo cargar la malla"),
                 Theme::TextDim);
        py += 20;

        Material& mt = n->material;
        section("Material (Phong)");
        propLabel("Color:");
        gui.colorEdit(ID("p_color"), Rect(px0 + LABEL_W, py, 64, ROW), mt.color);
        uint32_t hx = hexOf(mt.color);
        gui.text(px0 + LABEL_W + 72, py + (ROW - th) * 0.5f, util::format("#%06X", hx), Theme::TextDim);
        py += ROW + 4;
        propSlider("Ambiental:", mt.ambient, 0, 1, "%.2f", "p_ka");
        propSlider("Difusa:", mt.diffuse, 0, 1, "%.2f", "p_kd");
        propSlider("Especular:", mt.specular, 0, 1, "%.2f", "p_ks");
        propSlider("Brillo:", mt.shininess, 1, 128, "%.0f", "p_sh");
        propSlider("Emisión:", mt.emission, 0, 1, "%.2f", "p_ke");

        section("Textura");
        unsigned int tex = renderer.textures.get(mt.texture);
        if (tex) gui.image(tex, px0, py, 58, 58);
        else {
            gui.rect(px0, py, 58, 58, Theme::WidgetBg);
            gui.textCentered(Rect(px0, py, 58, 58), "Ninguna", Theme::TextDim);
        }
        gui.rectOutline(Rect(px0, py, 58, 58), Theme::Border);
        float tx = px0 + 66, tw = pw0 - 66;
        gui.text(tx, py + 2, gui.fitText(mt.texture.empty() ? std::string("(sin textura)") : util::fileName(mt.texture), tw),
                 mt.texture.empty() ? Theme::TextDim : Theme::Text);
        float hb = (tw - 4) / 2;
        if (gui.button(ID("p_tsel"), Rect(tx, py + 30, hb, ROW), "Seleccionar...")) chooseTexture();
        if (gui.button(ID("p_tclr"), Rect(tx + hb + 4, py + 30, hb, ROW), "Quitar", false, !mt.texture.empty())) {
            pushUndo();
            mt.texture.clear();
        }
        py += 64;
        propFloat("Repetición:", mt.tiling, 0.02f, "%.2f", "p_tile", 0.05f, 100.0f);
    }

    if (n->type == NODE_LIGHT) {
        LightProps& L = n->light;
        section("Luz");
        propLabel("Color:");
        gui.colorEdit(ID("p_lcol"), Rect(px0 + LABEL_W, py, 64, ROW), L.color);
        py += ROW + 4;
        propSlider("Intensidad:", L.intensity, 0, 3, "%.2f", "p_lint");
        gui.checkbox(ID("p_ldir"), px0, py, "Direccional (la posición indica la dirección)", L.directional);
        py += 25;
        gui.text(px0, py, "Se usan las 2 primeras luces visibles", Theme::TextDim);
        py += 17;
        gui.text(px0, py, "(GL_LIGHT0 y GL_LIGHT1). La primera proyecta sombras.", Theme::TextDim);
        py += 20;
    }
}

void Editor::propsCamera() {
    section("Cámara del editor (orbital)");
    propVec3("Objetivo", camera.target, 0.02f, "%.2f", "c_tgt");
    propFloat("Distancia:", camera.distance, 0.05f, "%.2f", "c_dist", 0.3f, 250.0f);
    propFloat("Yaw:", camera.yaw, 0.5f, "%.1f°", "c_yaw", -1e6f, 1e6f);
    propFloat("Pitch:", camera.pitch, 0.5f, "%.1f°", "c_pitch", -89.5f, 89.5f);
    propSlider("Campo visión:", camera.fov, 15, 100, "%.0f°", "c_fov");
    Vec3 e = camera.eye();
    gui.text(px0, py, util::format("Posición del ojo: (%.2f, %.2f, %.2f)", e.x, e.y, e.z), Theme::TextDim);
    py += 22;
    float bw = (pw0 - 4) / 2;
    if (gui.button(ID("c_reset"), Rect(px0, py, bw, ROW), "Restablecer")) camera.reset();
    if (gui.button(ID("c_front"), Rect(px0 + bw + 4, py, bw, ROW), "Vista frontal (1)")) camera.setView(0, 0);
    py += ROW + 4;
    if (gui.button(ID("c_side"), Rect(px0, py, bw, ROW), "Vista lateral (3)")) camera.setView(90, 0);
    if (gui.button(ID("c_top"), Rect(px0 + bw + 4, py, bw, ROW), "Vista superior (7)")) camera.setView(0, 89.5f);
    py += ROW + 4;
    section("Proyección");
    gui.text(px0, py, "Perspectiva (gluPerspective equivalente)", Theme::TextDim);
    py += 17;
    gui.text(px0, py, util::format("Planos: cercano %.2f, lejano %.0f", camera.nearPlane, camera.farPlane), Theme::TextDim);
    py += 20;
}

void Editor::propsScene() {
    section("Escena");
    std::vector<SceneNode*> all;
    scene.collect(all);
    int meshes = 0, lights = 0, joints = 0;
    for (size_t i = 0; i < all.size(); ++i) {
        if (all[i]->type == NODE_MESH) ++meshes;
        if (all[i]->type == NODE_LIGHT) ++lights;
        if (all[i]->isJoint()) ++joints;
    }
    gui.text(px0, py, util::format("%d nodos: %d mallas, %d luces, %d articulaciones", (int)all.size(), meshes, lights, joints),
             Theme::TextDim);
    py += 18;
    gui.text(px0, py, util::format("%d pistas de animación", (int)scene.animations.size()), Theme::TextDim);
    py += 22;
    propSlider("Luz ambiental:", scene.ambient, 0, 1, "%.2f", "s_amb");

    section("Visualización");
    gui.checkbox(ID("s_grid"), px0, py, "Cuadrícula y ejes", opts.grid);
    py += 23;
    gui.checkbox(ID("s_shadow"), px0, py, "Sombras proyectadas (stencil)", opts.shadows);
    py += 23;
    gui.checkbox(ID("s_wire"), px0, py, "Modo alambre", opts.wireframe);
    py += 23;
    if (renderer.glslAvailable()) {
        gui.checkbox(ID("s_glsl"), px0, py, "Shaders GLSL (Phong por píxel)", opts.shaders);
        py += 23;
    }
    gui.checkbox(ID("s_help"), px0, py, "Ayudas visuales (luces, pivotes)", opts.helpers);
    py += 25;

    section("Sistema");
    std::string info = renderer.info;
    while (!info.empty()) { // ajustar texto largo en varias líneas
        std::string lineTxt = info;
        while (gui.textWidth(lineTxt) > pw0 && lineTxt.size() > 1) util::popUtf8(lineTxt);
        gui.text(px0, py, lineTxt, Theme::TextDim);
        py += 17;
        info = info.substr(lineTxt.size());
    }
    gui.text(px0, py, renderer.glslAvailable() ? "GLSL 1.10 disponible" : "GLSL no disponible: pipeline fijo", Theme::TextDim);
    py += 22;

    section("Sugerencias");
    const char* tips[] = {"Selecciona un objeto en el visor o en el árbol.",
                          "Arrastra un nodo sobre otro en el árbol",
                          "para crear jerarquías (padre / hijo).",
                          "Haz clic en una pieza del robot para ver",
                          "los controles de todas sus articulaciones.",
                          "Arrastra archivos .obj, .bmp o .e3d a la ventana."};
    for (int i = 0; i < 6; ++i) {
        gui.text(px0, py, tips[i], Theme::TextDim);
        py += 17;
    }
}

// ---------------------------------------------------------------- recursos

void Editor::drawResourcesPanel(const Rect& r) {
    Rect c = panel(r, "Recursos");
    float x = c.x + 8, y = c.y + 6;
    if (gui.button(ID("r_t0"), Rect(x, y, 90, ROW), "Texturas", resTab == 0)) { resTab = 0; resScroll = 0; }
    if (gui.button(ID("r_t1"), Rect(x + 94, y, 110, ROW), "Modelos OBJ", resTab == 1)) { resTab = 1; resScroll = 0; }
    float bx = c.right() - 8 - 88;
    if (gui.button(ID("r_imp"), Rect(bx, y, 88, ROW), "Importar...")) {
        if (resTab == 0) chooseTexture();
        else importObjDialog();
    }
    if (gui.button(ID("r_scan"), Rect(bx - 92, y, 88, ROW), "Actualizar")) scanResources();

    Rect lr(c.x + 4, y + ROW + 6, c.w - 8, c.bottom() - (y + ROW + 6) - 4);
    float contentH = 0;
    if (gui.mouseIn(lr) && gui.wheel != 0) resScroll -= gui.wheel * 40;
    SceneNode* n = selectedNode();
    gui.pushClip(lr);
    if (resTab == 0) {
        const float tile = 70, gap = 12;
        int cols = std::max(1, (int)((lr.w - gap) / (tile + gap)));
        for (size_t i = 0; i < textureFiles.size(); ++i) {
            int col = (int)i % cols, row = (int)i / cols;
            float tx = lr.x + gap + col * (tile + gap), ty = lr.y + 4 + row * (tile + 24) - resScroll;
            Rect tr(tx, ty, tile, tile);
            const std::string& path = textureFiles[i];
            gui.image(renderer.textures.get(path), tx, ty, tile, tile);
            bool cur = n && n->type == NODE_MESH && n->material.texture == path;
            bool hov = gui.mouseIn(tr);
            if (cur) gui.rectOutline(Rect(tx - 2, ty - 2, tile + 4, tile + 4), 0x6cc4ff, 1.0f, 2.0f);
            else if (hov) gui.rectOutline(tr, Theme::Text);
            gui.textCentered(Rect(tx - 6, ty + tile + 2, tile + 12, 18), gui.fitText(util::fileName(path), tile + 12),
                             cur ? 0x6cc4ff : Theme::TextDim);
            if (gui.clickIn(tr)) applyTexture(path);
            contentH = std::max(contentH, ty + tile + 24 + resScroll - lr.y);
        }
        if (textureFiles.empty()) gui.text(lr.x + 8, lr.y + 8, "No hay texturas en assets/textures", Theme::TextDim);
    } else {
        for (size_t i = 0; i < modelFiles.size(); ++i) {
            float ry = lr.y + 4 + i * 28 - resScroll;
            Rect rr(lr.x + 4, ry, lr.w - 8, 26);
            if (gui.mouseIn(rr)) gui.rect(rr, Theme::WidgetHover);
            gui.text(rr.x + 8, ry + (26 - gui.textHeight()) * 0.5f, util::fileName(modelFiles[i]));
            const Mesh* m = renderer.meshes.get(modelFiles[i]);
            std::string info = m ? util::format("%d tri.", m->triangleCount()) : std::string("error");
            gui.text(rr.right() - 100 - gui.textWidth(info), ry + (26 - gui.textHeight()) * 0.5f, info, Theme::TextDim);
            if (gui.button(ID("r_add", (int)i), Rect(rr.right() - 86, ry + 2, 82, 22), "Agregar")) importObjPath(modelFiles[i]);
            contentH = std::max(contentH, ry + 30 + resScroll - lr.y);
        }
        if (modelFiles.empty()) gui.text(lr.x + 8, lr.y + 8, "No hay modelos en assets/models", Theme::TextDim);
    }
    gui.popClip();
    resScroll = clampf(resScroll, 0, std::max(0.0f, contentH - lr.h));
}

// ---------------------------------------------------------------- animación

void Editor::drawAnimationPanel(const Rect& r) {
    Rect c = panel(r, "Animación");
    std::vector<Animation>& A = scene.animations;
    float th = gui.textHeight();
    const float ctrlW = 236;
    Rect ctrl(c.right() - ctrlW - 8, c.y + 8, ctrlW, c.h - 16);
    float x = c.x + 10, y = c.y + 8;
    float leftW = ctrl.x - 14 - x;

    std::vector<std::string> labels;
    for (size_t i = 0; i < A.size(); ++i) {
        SceneNode* an = scene.findById(A[i].nodeId);
        labels.push_back(util::format("%d. %s  -  %s%s", (int)i + 1, an ? an->name.c_str() : "(?)",
                                      animPropName(A[i].prop), A[i].enabled ? "" : "  (inactiva)"));
    }
    if (labels.empty()) labels.push_back("(sin pistas de animación)");
    animSel = std::max(0, std::min(animSel, (int)A.size() - 1));

    gui.text(x, y + (ROW - th) * 0.5f, "Pista:", Theme::TextDim);
    float lw = leftW - 60 - 2 * 76;
    int sel = animSel;
    if (gui.combo(ID("a_list"), Rect(x + 60, y, lw, ROW), labels, sel)) animSel = sel;
    if (gui.button(ID("a_new"), Rect(x + 60 + lw + 4, y, 72, ROW), "Nueva")) newAnimation();
    if (gui.button(ID("a_del"), Rect(x + 60 + lw + 80, y, 72, ROW), "Eliminar", false, !A.empty())) {
        pushUndo();
        A.erase(A.begin() + animSel);
        if (animSel > 0) --animSel;
    }
    y += ROW + 10;

    float half = (leftW - 12) / 2;
    float x2 = x + half + 12;
    if (!A.empty()) {
        Animation& a = A[animSel];
        std::vector<SceneNode*> all;
        scene.collect(all);
        std::vector<std::string> nn;
        int ni = -1;
        for (size_t k = 0; k < all.size(); ++k) {
            nn.push_back(std::string(all[k]->depth() * 2, ' ') + all[k]->name);
            if (all[k]->id == a.nodeId) ni = (int)k;
        }
        gui.text(x, y + (ROW - th) * 0.5f, "Objeto:", Theme::TextDim);
        if (gui.combo(ID("a_node"), Rect(x + 60, y, half - 60, ROW), nn, ni) && ni >= 0) a.nodeId = all[ni]->id;
        std::vector<std::string> props;
        for (int p = 0; p < AP_COUNT; ++p) props.push_back(animPropName(p));
        gui.text(x2, y + (ROW - th) * 0.5f, "Tipo:", Theme::TextDim);
        int pi = a.prop;
        if (gui.combo(ID("a_prop"), Rect(x2 + 50, y, half - 50, ROW), props, pi)) a.prop = pi;
        y += ROW + 8;

        SceneNode* an = scene.findById(a.nodeId);
        bool rot = a.prop >= AP_ROT_X && a.prop <= AP_ROT_Z;
        float sp = rot ? 0.5f : 0.01f;
        gui.text(x, y + (ROW - th) * 0.5f, "Inicio:", Theme::TextDim);
        gui.dragFloat(ID("a_from"), Rect(x + 60, y, half - 60 - 62, ROW), a.from, sp, "%.2f");
        if (gui.button(ID("a_fromc"), Rect(x + half - 58, y, 58, ROW), "Actual", false, an != nullptr)) {
            pushUndo();
            a.from = getAnimProp(an, a.prop);
        }
        gui.text(x2, y + (ROW - th) * 0.5f, "Fin:", Theme::TextDim);
        gui.dragFloat(ID("a_to"), Rect(x2 + 50, y, half - 50 - 62, ROW), a.to, sp, "%.2f");
        if (gui.button(ID("a_toc"), Rect(x2 + half - 58, y, 58, ROW), "Actual", false, an != nullptr)) {
            pushUndo();
            a.to = getAnimProp(an, a.prop);
        }
        y += ROW + 8;
        gui.text(x, y + (ROW - th) * 0.5f, "Duración (s):", Theme::TextDim);
        gui.dragFloat(ID("a_dur"), Rect(x + 92, y, half - 92, ROW), a.duration, 0.02f, "%.2f", 0.05f, 600.0f);
        gui.text(x2, y + (ROW - th) * 0.5f, "Retardo (s):", Theme::TextDim);
        gui.dragFloat(ID("a_delay"), Rect(x2 + 92, y, half - 92, ROW), a.delay, 0.02f, "%.2f", 0.0f, 600.0f);
        y += ROW + 10;
        gui.checkbox(ID("a_loop"), x, y, "Repetir", a.loop);
        gui.checkbox(ID("a_ping"), x + 92, y, "Ida y vuelta", a.pingpong);
        gui.checkbox(ID("a_smooth"), x + 210, y, "Suavizado", a.smooth);
        gui.checkbox(ID("a_on"), x + 318, y, "Activa", a.enabled);
        y += 26;
        if (an) {
            float cur = getAnimProp(an, a.prop);
            gui.text(x, y, util::format("Valor actual de %s: %.2f    |    Pistas con \"Actual\": posa el objeto y captura.",
                                        animPropName(a.prop), cur),
                     Theme::TextDim);
        }
    } else {
        gui.text(x, y + 4, "Selecciona un objeto y pulsa \"Nueva\" para crear una pista.", Theme::TextDim);
        gui.text(x, y + 24, "Cada pista interpola una propiedad (posición, rotación o escala)", Theme::TextDim);
        gui.text(x, y + 44, "entre Inicio y Fin. Varias pistas se reproducen a la vez.", Theme::TextDim);
    }

    // --- controles de reproducción
    gui.rect(ctrl, Theme::WidgetBg);
    gui.rectOutline(ctrl, Theme::Border);
    float bw = 62, bh = 42, bx = ctrl.x + (ctrl.w - 3 * bw - 12) / 2, by = ctrl.y + 10;
    bool playingNow = animator.playing && !animator.paused;
    Rect bPlay(bx, by, bw, bh), bPause(bx + bw + 6, by, bw, bh), bStop(bx + 2 * (bw + 6), by, bw, bh);
    if (gui.button(ID("a_play"), bPlay, "", playingNow)) {
        if (!animator.playing) togglePlay();
        else if (animator.paused) togglePlay();
    }
    if (gui.button(ID("a_pause"), bPause, "", animator.paused)) {
        if (animator.playing) togglePlay();
    }
    if (gui.button(ID("a_stop"), bStop, "")) stopAnimation();
    float cx = bPlay.x + bw / 2, cy = by + bh / 2;
    gui.triangle(cx - 7, cy - 10, cx + 10, cy, cx - 7, cy + 10, 0x4cd964);
    cx = bPause.x + bw / 2;
    gui.rect(cx - 8, cy - 9, 5, 18, 0xe8e8e8);
    gui.rect(cx + 3, cy - 9, 5, 18, 0xe8e8e8);
    cx = bStop.x + bw / 2;
    gui.rect(cx - 8, cy - 8, 16, 16, 0xe8e8e8);
    by += bh + 10;
    std::string state = !animator.playing ? "Detenido" : (animator.paused ? "En pausa" : "Reproduciendo");
    gui.text(ctrl.x + 12, by, util::format("%s   t = %.2f s", state.c_str(), animator.time),
             animator.playing ? Theme::Text : Theme::TextDim);
    by += 22;
    float prog = (!A.empty() && animSel < (int)A.size()) ? A[animSel].progress(animator.time) : 0.0f;
    Rect bar(ctrl.x + 12, by, ctrl.w - 24, 8);
    gui.rect(bar, 0x2b2e34);
    gui.rect(bar.x, bar.y, bar.w * prog, bar.h, Theme::AccentHover);
    by += 18;
    if (gui.button(ID("a_robot"), Rect(ctrl.x + 12, by, ctrl.w - 24, ROW), "Demo: animar brazo robótico"))
        animateRobotDemo();
    by += ROW + 6;
    if (gui.button(ID("a_joints"), Rect(ctrl.x + 12, by, ctrl.w - 24, ROW), "Animar articulaciones del modelo"))
        animateSelectedModel();
}

// ---------------------------------------------------------------- superposición del visor

void Editor::drawViewportOverlay() {
    gui.rectOutline(rView, Theme::Border);
    float x = rView.x + 8, y = rView.y + 8;
    float x0 = x;
    const char* names[4] = {"Seleccionar (Q)", "Mover (W)", "Rotar (E)", "Escalar (R)"};
    for (int i = 0; i < 4; ++i) {
        float w = gui.textWidth(names[i]) + 18;
        if (gui.button(ID("tb", i), Rect(x, y, w, 24), names[i], gizmoMode == i)) gizmoMode = i;
        x += w + 2;
    }
    x += 8;
    std::string sp = localSpace ? "Ejes: locales (L)" : "Ejes: globales (L)";
    float w = gui.textWidth(sp) + 18;
    if (gui.button(ID("tb_space"), Rect(x, y, w, 24), sp)) localSpace = !localSpace;
    x += w;
    rToolbar = Rect(x0, y, x - x0, 24);

    std::string info = util::format("%s  |  %.0f FPS", (opts.shaders && renderer.glslAvailable()) ? "GLSL 1.10" : "Pipeline fijo", fps);
    gui.text(rView.right() - gui.textWidth(info) - 10, y + 4, info, Theme::TextDim);

    if (animator.playing) {
        std::string a = util::format(animator.paused ? "En pausa   t = %.2f s" : "Reproduciendo   t = %.2f s", animator.time);
        float aw = gui.textWidth(a) + 24;
        Rect ar(rView.right() - aw - 8, y + 28, aw, 22);
        gui.rect(ar, animator.paused ? 0x5a4a20 : 0x1f6b3a, 0.85f);
        gui.textCentered(ar, a);
    }

    SceneNode* sel = selectedNode();
    if (sel) {
        std::string s = sel->name;
        if (sel->isJoint()) s += util::format("   (articulación: %.1f)", sel->jointValue());
        gui.text(rView.x + 10, y + 34, s, 0x9fd0ff);
    }

    // indicador de ejes
    Mat4 V = camera.view();
    float cx = rView.x + 44, cy = rView.bottom() - 44;
    gui.circle(cx, cy, 36, 0x000000, true, 0.22f);
    struct Ax { Vec3 d; uint32_t c; const char* n; float z; };
    Ax ax[3] = {{Vec3(1, 0, 0), 0xe04848, "X", 0}, {Vec3(0, 1, 0), 0x58c040, "Y", 0}, {Vec3(0, 0, 1), 0x4a78f0, "Z", 0}};
    for (int i = 0; i < 3; ++i) ax[i].z = V.transformDir(ax[i].d).z;
    std::sort(ax, ax + 3, [](const Ax& a, const Ax& b) { return a.z < b.z; });
    for (int i = 0; i < 3; ++i) {
        Vec3 v = V.transformDir(ax[i].d);
        float ex = cx + v.x * 26, ey = cy - v.y * 26;
        gui.line(cx, cy, ex, ey, ax[i].c, 2.0f);
        gui.circle(ex, ey, 7.5f, ax[i].c);
        gui.textCentered(Rect(ex - 8, ey - 8, 16, 16), ax[i].n, 0xffffff, true);
    }

    std::string h = "Clic: seleccionar  ·  Gizmo: transformar (Ctrl = pasos)  ·  Der.: orbitar  ·  Medio/Shift+der.: desplazar  ·  Rueda: zoom";
    h = gui.fitText(h, rView.w - 110);
    gui.text(rView.right() - gui.textWidth(h) - 10, rView.bottom() - 22, h, Theme::TextDim);
}

void Editor::drawStatusBar() {
    gui.rect(rStatus, Theme::HeaderBg);
    gui.rect(0, rStatus.y, gui.W, 1, Theme::Border);
    float ty = rStatus.y + (rStatus.h - gui.textHeight()) * 0.5f;
    SceneNode* n = selectedNode();
    std::string sel = selectedId == CAMERA_ID ? std::string("Cámara") : (n ? n->name : std::string("-"));
    const char* modes[4] = {"Selección", "Mover", "Rotar", "Escalar"};
    std::string right = util::format("Nodos: %d   |   Selección: %s   |   Modo: %s   |   Deshacer: %d", scene.count(),
                                     sel.c_str(), modes[gizmoMode], (int)undoStack.size());
    float rw = gui.textWidth(right);
    gui.text(gui.W - rw - 12, ty, right, Theme::TextDim);
    std::string left = statusTimer > 0 ? statusMsg : std::string("Listo.  F1 = controles.");
    gui.text(10, ty, gui.fitText(left, gui.W - rw - 40), statusTimer > 0 ? Theme::Text : Theme::TextDim);
}

void Editor::drawHelp() {
    int HID = ID("help_popup");
    if (helpRequested) {
        gui.openPopup(HID, Rect());
        helpRequested = false;
    }
    if (gui.popupId != HID) return;
    Rect r(gui.W / 2 - 310, gui.H / 2 - 265, 620, 530);
    if (!gui.beginPopup(HID, r)) return;
    gui.text(r.x + 18, r.y + 14, "Controles del Editor 3D", Theme::Text, true);
    static const char* rows[][2] = {
        {"Clic izquierdo", "Seleccionar objeto / arrastrar manijas del gizmo"},
        {"Doble clic", "Enfocar el objeto"},
        {"Clic derecho + arrastrar", "Orbitar la cámara (también Alt + clic izq.)"},
        {"Medio o Shift + derecho", "Desplazar la cámara (pan)"},
        {"Rueda del ratón", "Acercar / alejar"},
        {"Q / W / E / R", "Seleccionar / Mover / Rotar / Escalar"},
        {"L", "Gizmo de traslación en ejes locales o globales"},
        {"Ctrl al arrastrar el gizmo", "Ajuste por pasos (0.25 / 15° / 0.1)"},
        {"F  /  P  /  H", "Enfocar / seleccionar padre / ocultar"},
        {"Supr  /  Ctrl+D", "Eliminar / duplicar (con sus hijos)"},
        {"Ctrl+Z  /  Ctrl+Y", "Deshacer / rehacer"},
        {"Ctrl+N / Ctrl+O / Ctrl+S", "Nueva / abrir / guardar escena (.e3d)"},
        {"Espacio", "Reproducir / pausar animaciones"},
        {"1 / 3 / 7", "Vista frontal / lateral / superior"},
        {"G", "Mostrar / ocultar cuadrícula"},
        {"Campos numéricos", "Arrastrar = cambiar (Shift fino, Ctrl rápido); clic = escribir"},
        {"Árbol de escena", "Arrastrar un nodo sobre otro = emparentar (jerarquía)"},
        {"Articulaciones", "Panel Propiedades: deslizadores de todo el modelo"},
        {"Archivos", "Arrastra .obj, .bmp/.tga o .e3d sobre la ventana"}};
    float y = r.y + 44;
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
        gui.text(r.x + 18, y, rows[i][0], 0x9fd0ff);
        gui.text(r.x + 222, y, rows[i][1], Theme::Text);
        y += 22;
    }
    if (gui.button(ID("help_close"), Rect(r.right() - 110, r.bottom() - 36, 94, 26), "Cerrar")) gui.closePopup();
    gui.endPopup();
}
