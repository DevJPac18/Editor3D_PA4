#include "anim/Animation.h"
#include "scene/SceneNode.h"
#include <cmath>
#include <algorithm>

const char* animPropName(int p) {
    static const char* names[AP_COUNT] = {
        "Posición X", "Posición Y", "Posición Z",
        "Rotación X", "Rotación Y", "Rotación Z",
        "Escala uniforme", "Escala X", "Escala Y", "Escala Z"};
    return (p >= 0 && p < AP_COUNT) ? names[p] : "?";
}

float getAnimProp(const SceneNode* n, int p) {
    switch (p) {
    case AP_POS_X: return n->position.x;
    case AP_POS_Y: return n->position.y;
    case AP_POS_Z: return n->position.z;
    case AP_ROT_X: return n->rotation.x;
    case AP_ROT_Y: return n->rotation.y;
    case AP_ROT_Z: return n->rotation.z;
    case AP_SCALE: return n->scale.x;
    case AP_SCALE_X: return n->scale.x;
    case AP_SCALE_Y: return n->scale.y;
    case AP_SCALE_Z: return n->scale.z;
    }
    return 0.0f;
}

void setAnimProp(SceneNode* n, int p, float v) {
    switch (p) {
    case AP_POS_X: n->position.x = v; break;
    case AP_POS_Y: n->position.y = v; break;
    case AP_POS_Z: n->position.z = v; break;
    case AP_ROT_X: n->rotation.x = v; break;
    case AP_ROT_Y: n->rotation.y = v; break;
    case AP_ROT_Z: n->rotation.z = v; break;
    case AP_SCALE: n->scale = Vec3(v, v, v); break;
    case AP_SCALE_X: n->scale.x = v; break;
    case AP_SCALE_Y: n->scale.y = v; break;
    case AP_SCALE_Z: n->scale.z = v; break;
    }
}

float Animation::progress(float time) const {
    float t = time - delay;
    float d = duration > 1e-4f ? duration : 1e-4f;
    if (t <= 0) return 0.0f;
    float c = t / d;
    if (pingpong) {
        c = loop ? std::fmod(c, 2.0f) : std::min(c, 2.0f);
        return c <= 1.0f ? c : 2.0f - c;
    }
    return loop ? std::fmod(c, 1.0f) : std::min(c, 1.0f);
}

float Animation::evaluate(float time) const {
    float u = progress(time);
    if (smooth) u = u * u * (3.0f - 2.0f * u);
    return from + (to - from) * u;
}
