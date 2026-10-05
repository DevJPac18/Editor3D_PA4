// SceneNode.h - Nodo del grafo de escena (jerarquía + transformación + material)
#pragma once
#include "core/Math3D.h"
#include <string>
#include <vector>

enum NodeType { NODE_EMPTY = 0, NODE_MESH = 1, NODE_LIGHT = 2 };
enum JointType { JOINT_NONE = 0, JOINT_REVOLUTE = 1, JOINT_PRISMATIC = 2 };

// Material tipo Phong: color base + coeficientes de iluminación + textura
struct Material {
    Vec3 color;
    float ambient, diffuse, specular, shininess, emission, tiling;
    std::string texture;
    Material() : color(0.8f, 0.8f, 0.82f), ambient(0.35f), diffuse(0.9f), specular(0.3f),
                 shininess(32.0f), emission(0.0f), tiling(1.0f) {}
};

struct LightProps {
    Vec3 color;
    float intensity;
    bool directional;
    LightProps() : color(1, 1, 1), intensity(1.0f), directional(false) {}
};

// Articulación: un grado de libertad sobre un eje local (rotación o traslación) con límites
struct JointInfo {
    JointType type;
    int axis;      // 0 = X, 1 = Y, 2 = Z
    float minV, maxV;
    JointInfo() : type(JOINT_NONE), axis(1), minV(-180.0f), maxV(180.0f) {}
};

struct SceneNode {
    int id;
    std::string name;
    NodeType type;
    std::string mesh;          // "cube", "sphere", ... o ruta a .obj

    Vec3 position, rotation, scale; // rotación en grados (Euler, orden Y-X-Z)
    bool visible, castShadow, receiveShadow, expanded;

    Material material;
    LightProps light;
    JointInfo joint;

    SceneNode* parent;
    std::vector<SceneNode*> children;

    SceneNode();

    Mat4 localMatrix() const;  // T * Ry * Rx * Rz * S
    Mat4 worldMatrix() const;  // padre->world * local
    Vec3 worldPosition() const { return worldMatrix().translation(); }

    bool isJoint() const { return joint.type != JOINT_NONE; }
    float jointValue() const;
    void setJointValue(float v);
    void clampJoint();

    bool isAncestorOf(const SceneNode* n) const;
    int depth() const;
};
