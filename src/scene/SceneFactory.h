// SceneFactory.h - Creación de primitivas, luces, modelos articulados y escena de demostración
#pragma once
#include "scene/Scene.h"
#include "render/Mesh.h"
#include <string>

namespace SceneFactory {
SceneNode* createPrimitive(Scene& s, const std::string& mesh, SceneNode* parent = nullptr);
SceneNode* createEmpty(Scene& s, SceneNode* parent = nullptr);
SceneNode* createLight(Scene& s, bool directional);
SceneNode* importObj(Scene& s, MeshLibrary& lib, const std::string& path);

// Brazo robótico de 6 articulaciones: cintura, hombro, codo, muñeca, rotor y pinza (2 dedos)
SceneNode* createRobotArm(Scene& s, const Vec3& pos, bool withAnimations);
void addRobotAnimations(Scene& s, SceneNode* robotRoot);
// Sistema planetario: jerarquía de órbitas anidadas (Sol > Tierra > Luna, Saturno > anillo)
SceneNode* createSolarSystem(Scene& s, const Vec3& pos, bool withAnimations);
// Anima todas las articulaciones de un modelo (ida y vuelta dentro de sus límites)
void animateJoints(Scene& s, SceneNode* root);

void buildDemoScene(Scene& s, MeshLibrary& lib);
void buildEmptyScene(Scene& s);
}
