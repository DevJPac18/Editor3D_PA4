// Animator.h - Reproduce las animaciones de la escena (play / pausa / stop)
#pragma once
#include "scene/Scene.h"
#include <vector>

class Animator {
public:
    bool playing = false;
    bool paused = false;
    float time = 0.0f;

    void play(Scene& s);      // si está en pausa, reanuda
    void togglePause();
    void stop(Scene& s);      // detiene y restaura los valores originales
    void reset();             // olvida el estado sin restaurar (al cargar otra escena)
    void update(Scene& s, float dt);
    void apply(Scene& s);

private:
    struct Saved { int id; Vec3 p, r, s; };
    std::vector<Saved> saved;
    void save(SceneNode* n);
};
