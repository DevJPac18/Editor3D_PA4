// Renderer.h - Dibuja la escena con OpenGL 2.0 (luces, materiales, texturas, sombras, selección)
#pragma once
#include "scene/Scene.h"
#include "render/Camera.h"
#include "render/Mesh.h"
#include "render/Texture.h"
#include "render/Shader.h"
#include "editor/Gizmo.h"
#include <vector>
#include <string>

struct RenderOptions {
    bool grid = true;
    bool shadows = true;
    bool wireframe = false;
    bool shaders = true;
    bool helpers = true;
    float lineScale = 1.0f;
};

struct ViewportPx { int x = 0, y = 0, w = 1, h = 1; }; // coordenadas de ventana GL (y desde abajo)

struct DrawItem {
    SceneNode* node;
    Mat4 world;
    bool visible;
};

class Renderer {
public:
    bool init();
    void buildList(Scene& scene); // calcula matrices de mundo recorriendo la jerarquía
    void render(Scene& scene, const Camera& cam, const ViewportPx& vp, const RenderOptions& o,
                SceneNode* selected, const GizmoFrame* gizmo, int gizmoMode, int gizmoHover);
    int pickObject(const Camera& cam, const ViewportPx& vp, int px, int py, const RenderOptions& o);
    int pickGizmo(const Camera& cam, const ViewportPx& vp, int px, int py, const GizmoFrame& g, int mode,
                  const RenderOptions& o);

    MeshLibrary meshes;
    TextureManager textures;
    bool glslAvailable() const { return shaderOk; }
    std::string info;

private:
    Shader shader;
    bool shaderOk = false;
    std::vector<DrawItem> items;
    bool hasShadowLight = false;
    Vec4 shadowLight;
    bool light0 = false, light1 = false;

    void addItems(SceneNode* n, const Mat4& parent, bool parentVisible);
    void setupCamera(const Camera& cam, const ViewportPx& vp, Mat4& V);
    void setupLights(Scene& scene, const Mat4& V);
    void applyMaterial(const Material& m);
    void drawLit(const DrawItem& it, const Mat4& V, bool useShader);
    void drawBackground();
    void drawGrid(const RenderOptions& o);
    void drawShadows(const Mat4& V, float floorY);
    void drawSelection(const Mat4& V, SceneNode* sel, const RenderOptions& o);
    void drawHelpers(const Mat4& V, SceneNode* sel, const RenderOptions& o);
    bool inSubtree(SceneNode* root, SceneNode* n) const;
};
