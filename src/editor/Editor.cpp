// Editor.cpp - Núcleo del editor: ciclo de cuadro, viewport, gizmos, historial y archivos
#include "editor/Editor.h"
#include "render/GL.h"
#include "scene/SceneFactory.h"
#include "io/SceneIO.h"
#include "platform/FileDialog.h"
#include "core/Utils.h"
#include <cmath>
#include <algorithm>

namespace {
const float MENU_H = 26.0f, STATUS_H = 24.0f, LEFT_W = 255.0f, RIGHT_W = 335.0f, BOTTOM_H = 222.0f, GAP = 3.0f;
const float SNAP_MOVE = 0.25f, SNAP_ROT = 15.0f, SNAP_SCALE = 0.1f;
}

// ---------------------------------------------------------------- inicialización y ciclo

bool Editor::init(void* wnd, void* hdc, float uiScale) {
    hwnd = wnd;
    if (!gui.init(hdc, uiScale)) return false;
    gui.onEditBegin = [this]() { pushUndo(); };
    renderer.init();
    opts.shaders = renderer.glslAvailable();
    TexGen::ensureDefaults("assets/textures");
    util::makeDirs("assets/models");
    util::makeDirs("scenes");
    scanResources();
    SceneFactory::buildDemoScene(scene, renderer.meshes);
    camera.reset();
    dirty = false;
    setStatus(renderer.glslAvailable() ? "Escena de demostración cargada. Shaders GLSL activos. F1 = controles."
                                       : "Escena de demostración cargada (pipeline fijo: GLSL no disponible).");
    return true;
}

void Editor::frame(float dt, int pw, int ph) {
    physW = std::max(1, pw);
    physH = std::max(1, ph);
    fpsAcc += dt;
    ++fpsFrames;
    if (fpsAcc >= 0.5f) {
        fps = fpsFrames / fpsAcc;
        fpsAcc = 0;
        fpsFrames = 0;
    }
    if (statusTimer > 0) statusTimer -= dt;

    gui.newFrame(physW, physH, dt);
    computeLayout();

    glViewport(0, 0, physW, physH);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(((Theme::WindowBg >> 16) & 255) / 255.0f, ((Theme::WindowBg >> 8) & 255) / 255.0f,
                 (Theme::WindowBg & 255) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    animator.update(scene, dt);
    handleShortcuts();
    renderer.buildList(scene);
    handleViewport();
    renderer.buildList(scene);
    renderViewport();

    gui.begin2D();
    drawViewportOverlay();
    drawScenePanel(rLeft);
    drawPropertiesPanel(rRight);
    drawResourcesPanel(rRes);
    drawAnimationPanel(rAnim);
    drawStatusBar();
    drawMenuBar();
    drawHelp();
    gui.endFrame();
}

void Editor::computeLayout() {
    float W = gui.W, H = gui.H;
    rMenu = Rect(0, 0, W, MENU_H);
    rStatus = Rect(0, H - STATUS_H, W, STATUS_H);
    float bottomY = H - STATUS_H - BOTTOM_H;
    float top = MENU_H + GAP;
    float midH = std::max(80.0f, bottomY - GAP - top);
    rLeft = Rect(GAP, top, LEFT_W, midH);
    rRight = Rect(W - RIGHT_W - GAP, top, RIGHT_W, midH);
    float vx = rLeft.right() + GAP;
    rView = Rect(vx, top, std::max(80.0f, rRight.x - GAP - vx), midH);
    float resW = std::floor((W - 3 * GAP) * 0.40f);
    rRes = Rect(GAP, bottomY, resW, BOTTOM_H - GAP);
    rAnim = Rect(rRes.right() + GAP, bottomY, std::max(80.0f, W - rRes.right() - 2 * GAP), BOTTOM_H - GAP);
}

ViewportPx Editor::viewportPx() const {
    ViewportPx v;
    float s = gui.scale;
    v.x = (int)std::floor(rView.x * s + 0.5f);
    v.w = std::max(1, (int)std::floor(rView.w * s + 0.5f));
    v.h = std::max(1, (int)std::floor(rView.h * s + 0.5f));
    v.y = physH - (int)std::floor((rView.y + rView.h) * s + 0.5f);
    return v;
}

bool Editor::worldToScreen(const Vec3& p, float& sx, float& sy) const {
    float aspect = rView.h > 0 ? rView.w / rView.h : 1.0f;
    Mat4 VP = camera.projection(aspect) * camera.view();
    Vec4 c = VP * Vec4(p, 1.0f);
    if (c.w <= 1e-5f) return false;
    float nx = c.x / c.w, ny = c.y / c.w;
    sx = rView.x + (nx * 0.5f + 0.5f) * rView.w;
    sy = rView.y + (1.0f - (ny * 0.5f + 0.5f)) * rView.h;
    return true;
}

void Editor::setStatus(const std::string& s) {
    statusMsg = s;
    statusTimer = 5.0f;
}

void Editor::scanResources() {
    std::vector<std::string> ex;
    ex.push_back("bmp");
    ex.push_back("tga");
    textureFiles = util::listFiles("assets/textures", ex);
    std::vector<std::string> mx;
    mx.push_back("obj");
    modelFiles = util::listFiles("assets/models", mx);
}

void Editor::select(int id) {
    if (id != selectedId) gui.focusId = 0;
    selectedId = id;
}

std::string Editor::title() const {
    std::string t = "Editor 3D - Proyecto PA4 - ";
    t += currentFile.empty() ? "Sin título" : util::fileName(currentFile);
    if (dirty) t += " *";
    return t;
}

// ---------------------------------------------------------------- atajos de teclado

void Editor::handleShortcuts() {
    if (gui.isTyping()) return;
    const std::vector<int> ks = gui.keys();
    for (size_t i = 0; i < ks.size(); ++i) {
        int k = ks[i];
        if (gui.ctrl) {
            if (k == 'Z') { if (gui.shift) redo(); else undo(); }
            else if (k == 'Y') redo();
            else if (k == 'S') saveScene(gui.shift);
            else if (k == 'O') openScene();
            else if (k == 'N') newScene();
            else if (k == 'D') duplicateSelected();
            continue;
        }
        switch (k) {
        case 'Q': gizmoMode = GIZMO_SELECT; break;
        case 'W': gizmoMode = GIZMO_MOVE; break;
        case 'E': gizmoMode = GIZMO_ROTATE; break;
        case 'R': gizmoMode = GIZMO_SCALE; break;
        case 'L': localSpace = !localSpace; break;
        case 'F': focusSelected(); break;
        case 'P': selectParent(); break;
        case 'G': opts.grid = !opts.grid; break;
        case 'H': {
            SceneNode* n = selectedNode();
            if (n) { pushUndo(); n->visible = !n->visible; }
            break;
        }
        case VK_DELETE: deleteSelected(); break;
        case VK_SPACE: togglePlay(); break;
        case VK_F1: helpRequested = true; break;
        case VK_ESCAPE: if (!gui.isPopupOpen()) select(-1); break;
        case '1': case VK_NUMPAD1: camera.setView(0, 0); break;
        case '3': case VK_NUMPAD3: camera.setView(90, 0); break;
        case '7': case VK_NUMPAD7: camera.setView(0, 89.5f); break;
        }
    }
}

// ---------------------------------------------------------------- viewport y gizmos

bool Editor::computeGizmoFrame(SceneNode* n, GizmoFrame& f) const {
    if (!n || gizmoMode == GIZMO_SELECT) return false;
    Mat4 W = n->worldMatrix();
    Mat4 P = n->parent ? n->parent->worldMatrix() : Mat4();
    f.origin = W.translation();
    if (gizmoMode == GIZMO_MOVE) {
        if (localSpace) for (int i = 0; i < 3; ++i) f.axis[i] = normalize(W.column(i));
        else { f.axis[0] = Vec3(1, 0, 0); f.axis[1] = Vec3(0, 1, 0); f.axis[2] = Vec3(0, 0, 1); }
    } else if (gizmoMode == GIZMO_SCALE) {
        for (int i = 0; i < 3; ++i) f.axis[i] = normalize(W.column(i));
    } else {
        // anillos alineados con los ejes de cada ángulo de Euler (orden Y, X, Z):
        // arrastrar un anillo modifica exactamente una componente de la rotación
        Mat4 Ry = Mat4::rotateY(n->rotation.y), Rx = Mat4::rotateX(n->rotation.x);
        f.axis[1] = normalize(P.column(1));
        f.axis[0] = normalize((P * Ry).column(0));
        f.axis[2] = normalize((P * Ry * Rx).column(2));
    }
    f.camRight = camera.right();
    f.camUp = camera.up();
    f.size = length(camera.eye() - f.origin) * 0.16f;
    return true;
}

void Editor::handleViewport() {
    SceneNode* sel = selectedNode();
    bool popupOpen = gui.isPopupOpen();
    bool over = rView.contains(gui.mx, gui.my) && !rToolbar.contains(gui.mx, gui.my) &&
                !(popupOpen && gui.lastPopupRect.contains(gui.mx, gui.my));
    ViewportPx vp = viewportPx();
    int px = (int)(gui.mx * gui.scale), pyy = physH - 1 - (int)(gui.my * gui.scale);

    float dx = gui.mx - lastMx, dy = gui.my - lastMy;
    lastMx = gui.mx;
    lastMy = gui.my;
    if (camMode != 0) {
        if (!gui.mouseDown[camButton]) camMode = 0;
        else if (camMode == 1) camera.orbit(dx, dy);
        else camera.pan(dx, dy, rView.h);
        return;
    }
    if (drag.active) {
        if (!gui.mouseDown[0]) drag.active = false;
        else updateGizmoDrag();
        hoverHandle = drag.active ? drag.handle : 0;
        return;
    }
    hoverHandle = 0;
    if (!over || treeDragging || gui.activeId) return;

    GizmoFrame gf;
    if (sel && computeGizmoFrame(sel, gf)) hoverHandle = renderer.pickGizmo(camera, vp, px, pyy, gf, gizmoMode, opts);
    if (gui.wheel != 0) camera.zoom(gui.wheel);
    if (popupOpen) return; // el clic sólo cierra la ventana emergente

    if (gui.mousePressed[1]) {
        camMode = gui.shift ? 2 : 1;
        camButton = 1;
        gui.consume();
    } else if (gui.mousePressed[2]) {
        camMode = 2;
        camButton = 2;
        gui.consume();
    } else if (gui.mousePressed[0] && !gui.consumed) {
        gui.consume();
        if (gui.alt) {
            camMode = 1;
            camButton = 0;
        } else if (hoverHandle) {
            beginGizmoDrag(hoverHandle);
        } else {
            int id = renderer.pickObject(camera, vp, px, pyy, opts);
            select(id > 0 ? id : -1);
            if (gui.doubleClick && id > 0) focusSelected();
        }
    }
}

void Editor::beginGizmoDrag(int handle) {
    SceneNode* n = selectedNode();
    GizmoFrame f;
    if (!n || !computeGizmoFrame(n, f)) return;
    pushUndo();
    drag = GizmoDrag();
    drag.active = true;
    drag.handle = handle;
    drag.nodeId = n->id;
    drag.sx = gui.mx;
    drag.sy = gui.my;
    drag.startPos = n->position;
    drag.startRot = n->rotation;
    drag.startScale = n->scale;
    drag.size = f.size;
    drag.parentInv = n->parent ? n->parent->worldMatrix().inverse() : Mat4();
    drag.camR = camera.right();
    drag.camU = camera.up();
    float dist = length(camera.eye() - f.origin);
    drag.worldPerPx = 2.0f * dist * std::tan(deg2rad(camera.fov) * 0.5f) / std::max(1.0f, rView.h);
    float c0x = 0, c0y = 0;
    worldToScreen(f.origin, c0x, c0y);
    drag.cx = c0x;
    drag.cy = c0y;
    if (handle >= 1 && handle <= 3) {
        Vec3 a = f.axis[handle - 1];
        drag.axis = a;
        float c1x = 0, c1y = 0;
        worldToScreen(f.origin + a * f.size, c1x, c1y);
        float ddx = c1x - c0x, ddy = c1y - c0y, len = std::sqrt(ddx * ddx + ddy * ddy);
        if (len < 4.0f) { ddx = 1; ddy = 0; len = 60; } // eje apuntando a la cámara
        drag.dirX = ddx / len;
        drag.dirY = ddy / len;
        drag.ppu = len / f.size;
        drag.toViewer = dot(a, camera.eye() - f.origin) > 0;
    }
    drag.lastAngle = std::atan2(gui.my - drag.cy, gui.mx - drag.cx);
    drag.accAngle = 0;
}

void Editor::updateGizmoDrag() {
    SceneNode* n = scene.findById(drag.nodeId);
    if (!n) { drag.active = false; return; }
    float dx = gui.mx - drag.sx, dy = gui.my - drag.sy;
    bool snap = gui.ctrl;
    int h = drag.handle;
    if (gizmoMode == GIZMO_MOVE) {
        Vec3 wd;
        if (h >= 1 && h <= 3) {
            float t = (dx * drag.dirX + dy * drag.dirY) / drag.ppu;
            if (snap) t = std::floor(t / SNAP_MOVE + 0.5f) * SNAP_MOVE;
            wd = drag.axis * t;
        } else {
            wd = drag.camR * (dx * drag.worldPerPx) + drag.camU * (-dy * drag.worldPerPx);
            if (snap) for (int i = 0; i < 3; ++i) wd[i] = std::floor(wd[i] / SNAP_MOVE + 0.5f) * SNAP_MOVE;
        }
        n->position = drag.startPos + drag.parentInv.transformDir(wd);
    } else if (gizmoMode == GIZMO_ROTATE && h >= 1 && h <= 3) {
        float a = std::atan2(gui.my - drag.cy, gui.mx - drag.cx);
        float d = a - drag.lastAngle;
        while (d > PI_F) d -= 2 * PI_F;
        while (d < -PI_F) d += 2 * PI_F;
        drag.accAngle += d;
        drag.lastAngle = a;
        float deg = rad2deg(drag.accAngle) * (drag.toViewer ? -1.0f : 1.0f);
        if (snap) deg = std::floor(deg / SNAP_ROT + 0.5f) * SNAP_ROT;
        n->rotation = drag.startRot;
        n->rotation[h - 1] = drag.startRot[h - 1] + deg;
    } else if (gizmoMode == GIZMO_SCALE) {
        if (h >= 1 && h <= 3) {
            float t = (dx * drag.dirX + dy * drag.dirY) / drag.ppu;
            float f = std::max(0.01f, 1.0f + t / drag.size);
            float v = drag.startScale[h - 1] * f;
            if (snap) v = std::max(SNAP_SCALE, std::floor(v / SNAP_SCALE + 0.5f) * SNAP_SCALE);
            n->scale = drag.startScale;
            n->scale[h - 1] = v;
        } else {
            float f = std::max(0.01f, 1.0f + (dx - dy) * 0.01f);
            if (snap) f = std::max(SNAP_SCALE, std::floor(f / SNAP_SCALE + 0.5f) * SNAP_SCALE);
            n->scale = drag.startScale * f;
        }
    }
    n->clampJoint();
}

void Editor::renderViewport() {
    SceneNode* sel = selectedNode();
    GizmoFrame gf;
    bool hasGizmo = sel && computeGizmoFrame(sel, gf);
    opts.lineScale = gui.scale;
    renderer.render(scene, camera, viewportPx(), opts, sel, hasGizmo ? &gf : nullptr, gizmoMode, hoverHandle);
}

// ---------------------------------------------------------------- historial (deshacer / rehacer)

void Editor::pushUndo() {
    std::string s = SceneIO::serialize(scene, nullptr);
    if (!undoStack.empty() && undoStack.back() == s) return;
    undoStack.push_back(s);
    if (undoStack.size() > 120) undoStack.erase(undoStack.begin());
    redoStack.clear();
    dirty = true;
}

void Editor::restoreState(const std::string& s) {
    if (animator.playing) animator.reset();
    SceneIO::deserialize(scene, s, nullptr);
    if (selectedId != CAMERA_ID && !scene.findById(selectedId)) selectedId = -1;
    gui.focusId = 0;
    dirty = true;
}

void Editor::undo() {
    std::string cur = SceneIO::serialize(scene, nullptr);
    while (!undoStack.empty() && undoStack.back() == cur) undoStack.pop_back();
    if (undoStack.empty()) { setStatus("Nada que deshacer."); return; }
    redoStack.push_back(cur);
    std::string s = undoStack.back();
    undoStack.pop_back();
    restoreState(s);
    setStatus("Deshacer.");
}

void Editor::redo() {
    if (redoStack.empty()) { setStatus("Nada que rehacer."); return; }
    undoStack.push_back(SceneIO::serialize(scene, nullptr));
    std::string s = redoStack.back();
    redoStack.pop_back();
    restoreState(s);
    setStatus("Rehacer.");
}

// ---------------------------------------------------------------- archivos

bool Editor::confirmDiscard() {
    if (!dirty) return true;
    bool ok = FileDialog::confirm(hwnd, "Hay cambios sin guardar. ¿Desea descartarlos?", "Editor 3D");
    gui.resetInput();
    return ok;
}

bool Editor::canQuit() { return confirmDiscard(); }

void Editor::newScene() {
    if (!confirmDiscard()) return;
    animator.reset();
    SceneFactory::buildEmptyScene(scene);
    camera.reset();
    undoStack.clear();
    redoStack.clear();
    currentFile.clear();
    dirty = false;
    select(-1);
    setStatus("Escena nueva.");
}

void Editor::loadDemo() {
    if (!confirmDiscard()) return;
    animator.reset();
    SceneFactory::buildDemoScene(scene, renderer.meshes);
    camera.reset();
    undoStack.clear();
    redoStack.clear();
    currentFile.clear();
    dirty = false;
    select(-1);
    setStatus("Escena de demostración cargada.");
}

void Editor::openScene() {
    if (!confirmDiscard()) return;
    std::string path = FileDialog::open(hwnd, L"Escenas Editor 3D (*.e3d)\0*.e3d\0Todos los archivos\0*.*\0",
                                        L"Abrir escena");
    gui.resetInput();
    if (!path.empty()) openScenePath(path);
}

void Editor::openScenePath(const std::string& path) {
    std::string data;
    if (!util::readTextFile(path, data) || data.compare(0, 8, "EDITOR3D") != 0) {
        setStatus("No se pudo abrir: " + path);
        return;
    }
    animator.reset();
    SceneIO::deserialize(scene, data, &camera);
    currentFile = util::makeRelative(path);
    undoStack.clear();
    redoStack.clear();
    dirty = false;
    select(-1);
    setStatus("Escena cargada: " + util::fileName(path));
}

void Editor::saveScene(bool saveAs) {
    std::string path = currentFile;
    if (saveAs || path.empty()) {
        std::string initial = currentFile.empty() ? std::string("escena.e3d") : util::fileName(currentFile);
        path = FileDialog::save(hwnd, L"Escenas Editor 3D (*.e3d)\0*.e3d\0", L"Guardar escena", L"e3d", initial);
        gui.resetInput();
        if (path.empty()) return;
    }
    if (animator.playing) animator.stop(scene); // se guardan los valores base, no los animados
    if (SceneIO::save(path, scene, &camera)) {
        currentFile = util::makeRelative(path);
        dirty = false;
        setStatus("Escena guardada: " + util::fileName(path));
    } else {
        setStatus("Error al guardar: " + path);
    }
}

void Editor::importObjDialog() {
    std::string path = FileDialog::open(hwnd, L"Modelos Wavefront (*.obj)\0*.obj\0", L"Importar modelo OBJ");
    gui.resetInput();
    if (!path.empty()) importObjPath(path);
}

void Editor::importObjPath(const std::string& path) {
    pushUndo();
    SceneNode* n = SceneFactory::importObj(scene, renderer.meshes, path);
    if (!n) {
        undoStack.pop_back();
        setStatus("No se pudo cargar el modelo OBJ: " + util::fileName(path));
        return;
    }
    placeNew(n);
    select(n->id);
    const Mesh* m = renderer.meshes.get(n->mesh);
    setStatus(util::format("Modelo importado: %s (%d vértices, %d triángulos)", util::fileName(path).c_str(),
                           m ? m->vertexCount() : 0, m ? m->triangleCount() : 0));
}

void Editor::dropFile(const std::string& path) {
    std::string ext = util::extension(path);
    if (ext == "obj") importObjPath(path);
    else if (ext == "bmp" || ext == "tga") applyTexture(path);
    else if (ext == "e3d") {
        if (confirmDiscard()) openScenePath(path);
    } else setStatus("Formato no soportado: " + util::fileName(path));
}

// ---------------------------------------------------------------- acciones de edición

void Editor::placeNew(SceneNode* n) {
    if (n->parent != scene.root) return;
    n->position.x = std::floor(camera.target.x * 2.0f + 0.5f) / 2.0f;
    n->position.z = std::floor(camera.target.z * 2.0f + 0.5f) / 2.0f;
}

void Editor::addPrimitive(const std::string& mesh) {
    pushUndo();
    SceneNode* n = SceneFactory::createPrimitive(scene, mesh);
    placeNew(n);
    select(n->id);
    setStatus("Creado: " + n->name);
}

void Editor::addLight(bool directional) {
    pushUndo();
    SceneNode* n = SceneFactory::createLight(scene, directional);
    select(n->id);
    setStatus("Creada: " + n->name + " (se usan las 2 primeras luces visibles)");
}

void Editor::addEmpty() {
    pushUndo();
    SceneNode* n = SceneFactory::createEmpty(scene);
    placeNew(n);
    select(n->id);
    setStatus("Grupo vacío creado: arrastra otros nodos sobre él en el árbol para agruparlos.");
}

void Editor::addRobot() {
    pushUndo();
    Vec3 p(std::floor(camera.target.x + 0.5f), 0, std::floor(camera.target.z + 0.5f));
    SceneNode* n = SceneFactory::createRobotArm(scene, p, false);
    select(n->id);
    setStatus("Brazo robótico creado: mueve sus articulaciones desde el panel Propiedades.");
}

void Editor::addSolarSystem() {
    pushUndo();
    Vec3 p(std::floor(camera.target.x + 0.5f), 2.8f, std::floor(camera.target.z + 0.5f));
    SceneNode* n = SceneFactory::createSolarSystem(scene, p, true);
    select(n->id);
    setStatus("Sistema solar creado (con animaciones). Pulsa Espacio para reproducir.");
}

void Editor::deleteSelected() {
    SceneNode* n = selectedNode();
    if (!n) return;
    pushUndo();
    std::string name = n->name;
    scene.deleteNode(n);
    select(-1);
    setStatus("Eliminado: " + name);
}

void Editor::duplicateSelected() {
    SceneNode* n = selectedNode();
    if (!n) return;
    pushUndo();
    SceneNode* d = scene.duplicate(n);
    if (!d) return;
    if (d->parent == scene.root) d->position.x += 0.75f;
    select(d->id);
    setStatus("Duplicado: " + d->name);
}

void Editor::selectParent() {
    SceneNode* n = selectedNode();
    if (n && n->parent && n->parent != scene.root) select(n->parent->id);
}

void Editor::unparentSelected() {
    SceneNode* n = selectedNode();
    if (!n || n->parent == scene.root) return;
    pushUndo();
    scene.setParent(n, nullptr, true);
    setStatus(n->name + " ahora está en la raíz (se conserva su posición global).");
}

void Editor::focusSelected() {
    SceneNode* n = selectedNode();
    if (!n) return;
    Mat4 W = n->worldMatrix();
    Vec3 center = W.translation();
    float radius = 1.0f;
    const Mesh* m = n->type == NODE_MESH ? renderer.meshes.get(n->mesh) : nullptr;
    if (m) {
        center = W.transformPoint((m->bmin + m->bmax) * 0.5f);
        Vec3 e = (m->bmax - m->bmin) * 0.5f;
        Vec3 we = Vec3(length(W.column(0)) * e.x, length(W.column(1)) * e.y, length(W.column(2)) * e.z);
        radius = std::max(0.3f, length(we));
    } else if (!n->children.empty()) {
        radius = 2.0f;
    }
    camera.focus(center, radius);
}

void Editor::applyTexture(const std::string& path) {
    SceneNode* n = selectedNode();
    if (!n || n->type != NODE_MESH) {
        setStatus("Selecciona primero un objeto con malla para aplicarle la textura.");
        return;
    }
    pushUndo();
    n->material.texture = util::makeRelative(path);
    setStatus("Textura " + util::fileName(path) + " aplicada a " + n->name);
}

void Editor::chooseTexture() {
    std::string path = FileDialog::open(hwnd, L"Imágenes (*.bmp;*.tga)\0*.bmp;*.tga\0", L"Seleccionar textura");
    gui.resetInput();
    if (!path.empty()) applyTexture(path);
}

void Editor::togglePlay() {
    if (!animator.playing) {
        animator.play(scene);
        setStatus(util::format("Reproduciendo %d pistas de animación.", (int)scene.animations.size()));
    } else {
        animator.togglePause();
        setStatus(animator.paused ? "Animación en pausa." : "Animación reanudada.");
    }
}

void Editor::stopAnimation() {
    if (animator.playing) {
        animator.stop(scene);
        setStatus("Animación detenida: se restauraron las transformaciones originales.");
    }
}

void Editor::newAnimation() {
    SceneNode* n = selectedNode();
    if (!n) {
        std::vector<SceneNode*> all;
        scene.collect(all);
        if (all.empty()) return;
        n = all[0];
    }
    pushUndo();
    Animation a;
    a.nodeId = n->id;
    a.prop = AP_ROT_Y;
    if (n->joint.type == JOINT_REVOLUTE) a.prop = AP_ROT_X + n->joint.axis;
    if (n->joint.type == JOINT_PRISMATIC) a.prop = AP_POS_X + n->joint.axis;
    a.from = getAnimProp(n, a.prop);
    a.to = n->isJoint() ? clampf(a.from + 45.0f, n->joint.minV, n->joint.maxV) : a.from + 360.0f;
    if (n->joint.type == JOINT_PRISMATIC) a.to = n->joint.maxV;
    a.pingpong = n->isJoint();
    a.smooth = n->isJoint();
    scene.animations.push_back(a);
    animSel = (int)scene.animations.size() - 1;
    setStatus("Nueva pista de animación para " + n->name);
}

void Editor::animateRobotDemo() {
    SceneNode* robot = nullptr;
    for (size_t i = 0; i < scene.root->children.size(); ++i)
        if (scene.root->children[i]->name.compare(0, 5, "Robot") == 0) { robot = scene.root->children[i]; break; }
    pushUndo();
    if (animator.playing) animator.stop(scene);
    if (!robot) {
        robot = SceneFactory::createRobotArm(scene, Vec3(3.6f, 0, -2.0f), false);
    } else {
        std::vector<SceneNode*> sub;
        scene.collectSubtree(robot, sub);
        for (size_t i = 0; i < sub.size(); ++i) {
            int id = sub[i]->id;
            scene.animations.erase(std::remove_if(scene.animations.begin(), scene.animations.end(),
                                                  [id](const Animation& a) { return a.nodeId == id; }),
                                   scene.animations.end());
        }
    }
    SceneFactory::addRobotAnimations(scene, robot);
    animSel = std::max(0, (int)scene.animations.size() - 1);
    animator.play(scene);
    setStatus("Demo del brazo robótico: 7 pistas sobre la jerarquía de articulaciones.");
}

void Editor::animateSelectedModel() {
    SceneNode* top = scene.topAncestor(selectedNode());
    if (!top) { setStatus("Selecciona un modelo con articulaciones (p. ej. el brazo robótico)."); return; }
    pushUndo();
    size_t before = scene.animations.size();
    SceneFactory::animateJoints(scene, top);
    if (scene.animations.size() == before) {
        setStatus(top->name + " no tiene articulaciones. Defínelas en Propiedades > Articulación.");
        return;
    }
    animSel = (int)scene.animations.size() - 1;
    setStatus(util::format("Se crearon %d pistas para las articulaciones de %s.",
                           (int)(scene.animations.size() - before), top->name.c_str()));
}
