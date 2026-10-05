// Editor.h - Aplicación editora: une escena, render, animación, gizmos e interfaz
#pragma once
#include "scene/Scene.h"
#include "render/Renderer.h"
#include "render/Camera.h"
#include "anim/Animator.h"
#include "ui/Gui.h"
#include "editor/Gizmo.h"
#include <string>
#include <vector>

class Editor {
public:
    bool init(void* hwnd, void* hdc, float uiScale);
    void frame(float dt, int pw, int ph);
    void dropFile(const std::string& path);
    bool wantsQuit() const { return quit; }
    bool canQuit(); // pregunta por cambios sin guardar
    std::string title() const;
    Gui gui;

private:
    enum { CAMERA_ID = -2 };

    void* hwnd = nullptr;
    Scene scene;
    Renderer renderer;
    Camera camera;
    Animator animator;
    RenderOptions opts;

    int selectedId = -1;
    int gizmoMode = GIZMO_MOVE;
    bool localSpace = false;
    int hoverHandle = 0;

    struct GizmoDrag {
        bool active = false;
        int handle = 0, nodeId = -1;
        float sx = 0, sy = 0, dirX = 1, dirY = 0, ppu = 1, cx = 0, cy = 0;
        float lastAngle = 0, accAngle = 0, size = 1, worldPerPx = 0.01f;
        bool toViewer = false;
        Vec3 startPos, startRot, startScale, axis, camR, camU;
        Mat4 parentInv;
    } drag;

    int camMode = 0, camButton = 1; // 1 = orbitar, 2 = desplazar
    float lastMx = 0, lastMy = 0;

    std::vector<std::string> undoStack, redoStack;
    std::string currentFile;
    bool dirty = false, quit = false, helpRequested = false;
    std::string statusMsg;
    float statusTimer = 0;
    float fps = 0, fpsAcc = 0;
    int fpsFrames = 0;
    int physW = 1, physH = 1;

    // layout
    Rect rMenu, rLeft, rRight, rView, rRes, rAnim, rStatus, rToolbar;
    float treeScroll = 0, propScroll = 0, propContentH = 0, resScroll = 0;
    int resTab = 0, animSel = 0;
    int treeDragId = 0;
    bool treeDragging = false;
    float treeDragX = 0, treeDragY = 0;
    std::vector<std::string> textureFiles, modelFiles;

    // cursor de maquetación del panel de propiedades
    float px0 = 0, py = 0, pw0 = 0;

    // --- núcleo (Editor.cpp)
    SceneNode* selectedNode() const { return scene.findById(selectedId); }
    void select(int id);
    void computeLayout();
    ViewportPx viewportPx() const;
    bool worldToScreen(const Vec3& p, float& sx, float& sy) const;
    bool computeGizmoFrame(SceneNode* n, GizmoFrame& f) const;
    void handleShortcuts();
    void handleViewport();
    void beginGizmoDrag(int handle);
    void updateGizmoDrag();
    void renderViewport();
    void setStatus(const std::string& s);
    void scanResources();

    void pushUndo();
    void undo();
    void redo();
    void restoreState(const std::string& s);

    bool confirmDiscard();
    void newScene();
    void loadDemo();
    void openScene();
    void openScenePath(const std::string& path);
    void saveScene(bool saveAs);
    void importObjDialog();
    void importObjPath(const std::string& path);
    void addPrimitive(const std::string& mesh);
    void addLight(bool directional);
    void addEmpty();
    void addRobot();
    void addSolarSystem();
    void deleteSelected();
    void duplicateSelected();
    void selectParent();
    void unparentSelected();
    void focusSelected();
    void applyTexture(const std::string& path);
    void chooseTexture();
    void togglePlay();
    void stopAnimation();
    void newAnimation();
    void animateRobotDemo();
    void animateSelectedModel();
    void placeNew(SceneNode* n);

    // --- paneles (EditorPanels.cpp)
    Rect panel(const Rect& r, const std::string& title);
    void drawMenuBar();
    void drawScenePanel(const Rect& r);
    void drawPropertiesPanel(const Rect& r);
    void drawResourcesPanel(const Rect& r);
    void drawAnimationPanel(const Rect& r);
    void drawViewportOverlay();
    void drawStatusBar();
    void drawHelp();
    void drawNodeIcon(SceneNode* n, float x, float y);

    void propsNode(SceneNode* n);
    void propsCamera();
    void propsScene();
    void section(const std::string& title);
    void propLabel(const std::string& l);
    bool propVec3(const std::string& label, Vec3& v, float speed, const char* fmt, const char* key, float mn = -1e9f);
    bool propFloat(const std::string& label, float& v, float speed, const char* fmt, const char* key, float mn, float mx);
    bool propSlider(const std::string& label, float& v, float mn, float mx, const char* fmt, const char* key);
};
