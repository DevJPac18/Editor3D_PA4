#include "scene/SceneNode.h"

SceneNode::SceneNode()
    : id(0), type(NODE_EMPTY), scale(1, 1, 1), visible(true), castShadow(true),
      receiveShadow(false), expanded(true), parent(nullptr) {}

Mat4 SceneNode::localMatrix() const {
    return Mat4::translate(position) * Mat4::rotateY(rotation.y) * Mat4::rotateX(rotation.x) *
           Mat4::rotateZ(rotation.z) * Mat4::scale(scale);
}

Mat4 SceneNode::worldMatrix() const {
    if (parent) return parent->worldMatrix() * localMatrix();
    return localMatrix();
}

float SceneNode::jointValue() const {
    if (joint.type == JOINT_PRISMATIC) return position[joint.axis];
    return rotation[joint.axis];
}

void SceneNode::setJointValue(float v) {
    v = clampf(v, joint.minV, joint.maxV);
    if (joint.type == JOINT_PRISMATIC) position[joint.axis] = v;
    else if (joint.type == JOINT_REVOLUTE) rotation[joint.axis] = v;
}

void SceneNode::clampJoint() {
    if (joint.type != JOINT_NONE) setJointValue(jointValue());
}

bool SceneNode::isAncestorOf(const SceneNode* n) const {
    for (const SceneNode* p = n ? n->parent : nullptr; p; p = p->parent)
        if (p == this) return true;
    return false;
}

int SceneNode::depth() const {
    int d = 0;
    for (const SceneNode* p = parent; p && p->parent; p = p->parent) ++d;
    return d;
}
