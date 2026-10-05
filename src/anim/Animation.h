// Animation.h - Pista de animación de una propiedad de un nodo
#pragma once

struct SceneNode;

enum AnimProp {
    AP_POS_X = 0, AP_POS_Y, AP_POS_Z,
    AP_ROT_X, AP_ROT_Y, AP_ROT_Z,
    AP_SCALE, AP_SCALE_X, AP_SCALE_Y, AP_SCALE_Z,
    AP_COUNT
};

const char* animPropName(int prop);
float getAnimProp(const SceneNode* n, int prop);
void setAnimProp(SceneNode* n, int prop, float v);

struct Animation {
    int nodeId;
    int prop;
    float from, to;
    float duration;   // segundos por ciclo
    float delay;      // retardo inicial
    bool loop;        // repetir
    bool pingpong;    // ida y vuelta
    bool smooth;      // interpolación suavizada (smoothstep)
    bool enabled;

    Animation() : nodeId(-1), prop(AP_ROT_Y), from(0), to(360), duration(3.0f), delay(0),
                  loop(true), pingpong(false), smooth(false), enabled(true) {}

    float evaluate(float time) const;
    float progress(float time) const; // 0..1 dentro del ciclo actual
};
