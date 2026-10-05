#include "scene/Scene.h"
#include <algorithm>
#include <map>
#include <cstdio>
#include <cstdlib>

static void destroyTree(SceneNode* n) {
    for (size_t i = 0; i < n->children.size(); ++i) destroyTree(n->children[i]);
    delete n;
}

static void detach(SceneNode* n) {
    if (!n->parent) return;
    std::vector<SceneNode*>& c = n->parent->children;
    c.erase(std::remove(c.begin(), c.end(), n), c.end());
    n->parent = nullptr;
}

Scene::Scene() : ambient(0.25f), nextId(1) {
    root = new SceneNode();
    root->id = 0;
    root->name = "__root__";
}

Scene::~Scene() {
    clear();
    delete root;
}

void Scene::clear() {
    for (size_t i = 0; i < root->children.size(); ++i) destroyTree(root->children[i]);
    root->children.clear();
    animations.clear();
    nextId = 1;
    ambient = 0.25f;
}

SceneNode* Scene::createNode(const std::string& name, NodeType type, SceneNode* parent) {
    return createNodeWithId(nextId, name, type, parent);
}

SceneNode* Scene::createNodeWithId(int id, const std::string& name, NodeType type, SceneNode* parent) {
    SceneNode* n = new SceneNode();
    n->id = id;
    n->name = name;
    n->type = type;
    if (id >= nextId) nextId = id + 1;
    if (!parent) parent = root;
    n->parent = parent;
    parent->children.push_back(n);
    return n;
}

void Scene::collectSubtree(SceneNode* n, std::vector<SceneNode*>& out) const {
    out.push_back(n);
    for (size_t i = 0; i < n->children.size(); ++i) collectSubtree(n->children[i], out);
}

void Scene::collect(std::vector<SceneNode*>& out) const {
    for (size_t i = 0; i < root->children.size(); ++i) collectSubtree(root->children[i], out);
}

void Scene::deleteNode(SceneNode* n) {
    if (!n || n == root) return;
    std::vector<SceneNode*> sub;
    collectSubtree(n, sub);
    for (size_t i = 0; i < sub.size(); ++i) {
        int id = sub[i]->id;
        animations.erase(std::remove_if(animations.begin(), animations.end(),
                                        [id](const Animation& a) { return a.nodeId == id; }),
                         animations.end());
    }
    detach(n);
    destroyTree(n);
}

static SceneNode* findRec(SceneNode* n, int id) {
    if (n->id == id) return n;
    for (size_t i = 0; i < n->children.size(); ++i) {
        SceneNode* r = findRec(n->children[i], id);
        if (r) return r;
    }
    return nullptr;
}

SceneNode* Scene::findById(int id) const {
    if (id <= 0) return nullptr;
    return findRec(root, id);
}

SceneNode* Scene::findByName(const std::string& name) const {
    std::vector<SceneNode*> all;
    collect(all);
    for (size_t i = 0; i < all.size(); ++i)
        if (all[i]->name == name) return all[i];
    return nullptr;
}

bool Scene::setParent(SceneNode* n, SceneNode* p, bool keepWorld) {
    if (!n || n == root) return false;
    if (!p) p = root;
    if (n == p || n->isAncestorOf(p) || n->parent == p) return false;
    Mat4 world = n->worldMatrix();
    detach(n);
    n->parent = p;
    p->children.push_back(n);
    if (keepWorld) {
        Mat4 local = p->worldMatrix().inverse() * world;
        decomposeTRS(local, n->position, n->rotation, n->scale);
    }
    return true;
}

static SceneNode* cloneRec(Scene& s, SceneNode* src, SceneNode* parent, std::map<int, int>& idMap) {
    SceneNode* c = s.createNode(src->name, src->type, parent);
    int newId = c->id;
    *c = *src;
    c->id = newId;
    c->parent = parent;
    c->children.clear();
    idMap[src->id] = newId;
    for (size_t i = 0; i < src->children.size(); ++i) cloneRec(s, src->children[i], c, idMap);
    return c;
}

SceneNode* Scene::duplicate(SceneNode* n) {
    if (!n || n == root) return nullptr;
    std::map<int, int> idMap;
    SceneNode* c = cloneRec(*this, n, n->parent, idMap);
    c->name = uniqueName(n->name, true);
    // duplicar también las animaciones del subárbol
    size_t count = animations.size();
    for (size_t i = 0; i < count; ++i) {
        std::map<int, int>::iterator it = idMap.find(animations[i].nodeId);
        if (it != idMap.end()) {
            Animation a = animations[i];
            a.nodeId = it->second;
            animations.push_back(a);
        }
    }
    return c;
}

std::string Scene::uniqueName(const std::string& baseIn, bool forceSuffix) const {
    std::string base = baseIn;
    // quitar sufijo _NN existente
    size_t us = base.find_last_of('_');
    if (us != std::string::npos && us + 1 < base.size()) {
        bool digits = true;
        for (size_t i = us + 1; i < base.size(); ++i)
            if (base[i] < '0' || base[i] > '9') digits = false;
        if (digits) base = base.substr(0, us);
    }
    if (!forceSuffix && !findByName(base)) return base;
    for (int i = 1; i < 10000; ++i) {
        char buf[32];
        snprintf(buf, sizeof(buf), "_%02d", i);
        std::string cand = base + buf;
        if (!findByName(cand)) return cand;
    }
    return base;
}

int Scene::count() const {
    std::vector<SceneNode*> all;
    collect(all);
    return (int)all.size();
}

SceneNode* Scene::topAncestor(SceneNode* n) const {
    if (!n || n == root) return nullptr;
    while (n->parent && n->parent != root) n = n->parent;
    return n;
}
