// Scene.h - Contenedor del grafo de escena y de las animaciones
#pragma once
#include "scene/SceneNode.h"
#include "anim/Animation.h"
#include <string>
#include <vector>

class Scene {
public:
    Scene();
    ~Scene();

    SceneNode* root; // nodo raíz invisible
    std::vector<Animation> animations;
    float ambient;   // luz ambiental global
    int nextId;

    SceneNode* createNode(const std::string& name, NodeType type, SceneNode* parent = nullptr);
    SceneNode* createNodeWithId(int id, const std::string& name, NodeType type, SceneNode* parent);
    void deleteNode(SceneNode* n);
    SceneNode* findById(int id) const;
    SceneNode* findByName(const std::string& name) const;
    bool setParent(SceneNode* n, SceneNode* newParent, bool keepWorld);
    SceneNode* duplicate(SceneNode* n);
    void clear();

    void collect(std::vector<SceneNode*>& out) const;            // DFS de todos los nodos
    void collectSubtree(SceneNode* n, std::vector<SceneNode*>& out) const;
    std::string uniqueName(const std::string& base, bool forceSuffix) const;
    int count() const;
    SceneNode* topAncestor(SceneNode* n) const; // hijo directo de la raíz que contiene a n

private:
    Scene(const Scene&);
    Scene& operator=(const Scene&);
};
