#include "anim/Animator.h"

void Animator::save(SceneNode* n) {
    for (size_t i = 0; i < saved.size(); ++i)
        if (saved[i].id == n->id) return;
    Saved s;
    s.id = n->id;
    s.p = n->position;
    s.r = n->rotation;
    s.s = n->scale;
    saved.push_back(s);
}

void Animator::play(Scene& s) {
    if (playing) {
        paused = false;
        return;
    }
    saved.clear();
    playing = true;
    paused = false;
    time = 0.0f;
    apply(s);
}

void Animator::togglePause() {
    if (playing) paused = !paused;
}

void Animator::stop(Scene& s) {
    if (!playing) return;
    for (size_t i = 0; i < saved.size(); ++i) {
        SceneNode* n = s.findById(saved[i].id);
        if (!n) continue;
        n->position = saved[i].p;
        n->rotation = saved[i].r;
        n->scale = saved[i].s;
    }
    reset();
}

void Animator::reset() {
    saved.clear();
    playing = false;
    paused = false;
    time = 0.0f;
}

void Animator::update(Scene& s, float dt) {
    if (!playing || paused) return;
    time += dt;
    apply(s);
}

void Animator::apply(Scene& s) {
    for (size_t i = 0; i < s.animations.size(); ++i) {
        const Animation& a = s.animations[i];
        if (!a.enabled) continue;
        SceneNode* n = s.findById(a.nodeId);
        if (!n) continue;
        save(n); // guarda el valor base la primera vez que se anima el nodo
        setAnimProp(n, a.prop, a.evaluate(time));
    }
}
