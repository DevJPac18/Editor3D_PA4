#include "scene/SceneFactory.h"
#include "io/ObjLoader.h"
#include "core/Utils.h"
#include <cctype>
#include <cmath>

namespace {

const char* TEX_BRICK = "assets/textures/brick.bmp";
const char* TEX_METAL = "assets/textures/metal.bmp";
const char* TEX_FLOOR = "assets/textures/floor.bmp";
const char* TEX_CRATE = "assets/textures/crate.bmp";
const char* TEX_CONCRETE = "assets/textures/concrete.bmp";
const char* TEX_MARBLE = "assets/textures/marble.bmp";
const char* TEX_SUN = "assets/textures/sun.bmp";
const char* TEX_EARTH = "assets/textures/earth.bmp";
const char* TEX_MOON = "assets/textures/moon.bmp";

SceneNode* part(Scene& s, SceneNode* parent, const std::string& name, const std::string& mesh,
                const Vec3& pos, const Vec3& rot, const Vec3& scl, const Vec3& color,
                const std::string& tex, float spec, float shin) {
    SceneNode* n = s.createNode(s.uniqueName(name, false), NODE_MESH, parent);
    n->mesh = mesh;
    n->position = pos;
    n->rotation = rot;
    n->scale = scl;
    n->material.color = color;
    n->material.texture = tex;
    n->material.specular = spec;
    n->material.shininess = shin;
    n->expanded = false;
    return n;
}

SceneNode* joint(Scene& s, SceneNode* parent, const std::string& name, const Vec3& pos, JointType type, int axis,
                 float mn, float mx) {
    SceneNode* n = s.createNode(s.uniqueName(name, false), NODE_EMPTY, parent);
    n->position = pos;
    n->joint.type = type;
    n->joint.axis = axis;
    n->joint.minV = mn;
    n->joint.maxV = mx;
    return n;
}

void addAnim(Scene& s, SceneNode* n, int prop, float from, float to, float dur, bool pingpong, bool smooth,
             float delay = 0.0f) {
    if (!n) return;
    Animation a;
    a.nodeId = n->id;
    a.prop = prop;
    a.from = from;
    a.to = to;
    a.duration = dur;
    a.delay = delay;
    a.loop = true;
    a.pingpong = pingpong;
    a.smooth = smooth;
    s.animations.push_back(a);
}

SceneNode* findIn(SceneNode* root, const std::string& prefix) {
    if (root->name.compare(0, prefix.size(), prefix) == 0) return root;
    for (size_t i = 0; i < root->children.size(); ++i) {
        SceneNode* r = findIn(root->children[i], prefix);
        if (r) return r;
    }
    return nullptr;
}

void collectJoints(SceneNode* n, std::vector<SceneNode*>& out) {
    if (n->isJoint()) out.push_back(n);
    for (size_t i = 0; i < n->children.size(); ++i) collectJoints(n->children[i], out);
}

} // namespace

namespace SceneFactory {

SceneNode* createPrimitive(Scene& s, const std::string& mesh, SceneNode* parent) {
    std::string base = MeshLibrary::displayName(mesh);
    SceneNode* n = s.createNode(s.uniqueName(base, true), NODE_MESH, parent);
    n->mesh = mesh;
    if (mesh == "plane") {
        n->scale = Vec3(4, 1, 4);
        n->castShadow = false;
    } else if (mesh == "torus") {
        n->position.y = 0.15f;
    } else {
        n->position.y = 0.5f;
    }
    return n;
}

SceneNode* createEmpty(Scene& s, SceneNode* parent) {
    return s.createNode(s.uniqueName("Grupo", true), NODE_EMPTY, parent);
}

SceneNode* createLight(Scene& s, bool directional) {
    SceneNode* n = s.createNode(s.uniqueName(directional ? "LuzDireccional" : "Luz", true), NODE_LIGHT);
    n->position = directional ? Vec3(4, 6, 3) : Vec3(-3, 5, 3);
    n->light.directional = directional;
    n->castShadow = false;
    return n;
}

SceneNode* importObj(Scene& s, MeshLibrary& lib, const std::string& pathIn) {
    std::string path = util::makeRelative(pathIn);
    const Mesh* m = lib.get(path);
    if (!m) return nullptr;
    std::string name = util::stem(path);
    if (!name.empty()) name[0] = (char)toupper((unsigned char)name[0]);
    SceneNode* n = s.createNode(s.uniqueName(name, true), NODE_MESH);
    n->mesh = path;
    float sc = 1.5f;
    n->scale = Vec3(sc, sc, sc);
    n->position.y = -m->bmin.y * sc; // apoyado sobre el suelo
    return n;
}

SceneNode* createRobotArm(Scene& s, const Vec3& pos, bool withAnimations) {
    const Vec3 yellow(0.98f, 0.70f, 0.10f), dark(0.24f, 0.25f, 0.27f), steel(0.78f, 0.79f, 0.82f);
    SceneNode* root = s.createNode(s.uniqueName("Robot", false), NODE_EMPTY);
    root->position = pos;

    part(s, root, "Base", "cylinder", Vec3(0, 0.15f, 0), Vec3(), Vec3(1.3f, 0.3f, 1.3f), dark, TEX_METAL, 0.6f, 40);
    SceneNode* waist = joint(s, root, "Cintura", Vec3(0, 0.3f, 0), JOINT_REVOLUTE, 1, -170, 170);
    part(s, waist, "Torreta", "cylinder", Vec3(0, 0.25f, 0), Vec3(), Vec3(0.85f, 0.5f, 0.85f), yellow, "", 0.55f, 48);

    SceneNode* shoulder = joint(s, waist, "Hombro", Vec3(0, 0.62f, 0), JOINT_REVOLUTE, 2, -100, 100);
    part(s, shoulder, "EjeHombro", "cylinder", Vec3(), Vec3(90, 0, 0), Vec3(0.5f, 0.62f, 0.5f), dark, TEX_METAL, 0.7f, 64);
    part(s, shoulder, "Brazo", "cube", Vec3(0, 0.95f, 0), Vec3(), Vec3(0.32f, 1.9f, 0.32f), yellow, "", 0.55f, 48);

    SceneNode* elbow = joint(s, shoulder, "Codo", Vec3(0, 1.9f, 0), JOINT_REVOLUTE, 2, -140, 140);
    part(s, elbow, "EjeCodo", "cylinder", Vec3(), Vec3(90, 0, 0), Vec3(0.42f, 0.5f, 0.42f), dark, TEX_METAL, 0.7f, 64);
    part(s, elbow, "Antebrazo", "cube", Vec3(0, 0.75f, 0), Vec3(), Vec3(0.26f, 1.5f, 0.26f), yellow, "", 0.55f, 48);

    SceneNode* wrist = joint(s, elbow, "Muñeca", Vec3(0, 1.5f, 0), JOINT_REVOLUTE, 2, -120, 120);
    part(s, wrist, "EjeMuñeca", "sphere", Vec3(), Vec3(), Vec3(0.34f, 0.34f, 0.34f), dark, TEX_METAL, 0.8f, 80);

    SceneNode* roll = joint(s, wrist, "Rotor", Vec3(0, 0.18f, 0), JOINT_REVOLUTE, 1, -180, 180);
    part(s, roll, "Palma", "cube", Vec3(0, 0.06f, 0), Vec3(), Vec3(0.56f, 0.12f, 0.24f), dark, TEX_METAL, 0.7f, 64);

    SceneNode* fl = joint(s, roll, "DedoIzq", Vec3(-0.2f, 0.12f, 0), JOINT_PRISMATIC, 0, -0.24f, -0.06f);
    part(s, fl, "GarraIzq", "cube", Vec3(0, 0.17f, 0), Vec3(), Vec3(0.06f, 0.34f, 0.18f), steel, TEX_METAL, 0.9f, 96);
    SceneNode* fr = joint(s, roll, "DedoDer", Vec3(0.2f, 0.12f, 0), JOINT_PRISMATIC, 0, 0.06f, 0.24f);
    part(s, fr, "GarraDer", "cube", Vec3(0, 0.17f, 0), Vec3(), Vec3(0.06f, 0.34f, 0.18f), steel, TEX_METAL, 0.9f, 96);

    // pose inicial
    waist->rotation.y = -25;
    shoulder->rotation.z = 30;
    elbow->rotation.z = 65;
    wrist->rotation.z = 45;

    if (withAnimations) addRobotAnimations(s, root);
    return root;
}

void addRobotAnimations(Scene& s, SceneNode* r) {
    if (!r) return;
    addAnim(s, findIn(r, "Cintura"), AP_ROT_Y, -70, 50, 4.0f, true, true);
    addAnim(s, findIn(r, "Hombro"), AP_ROT_Z, 10, 50, 2.6f, true, true);
    addAnim(s, findIn(r, "Codo"), AP_ROT_Z, 35, 100, 2.1f, true, true);
    addAnim(s, findIn(r, "Muñeca"), AP_ROT_Z, 10, 75, 1.7f, true, true);
    addAnim(s, findIn(r, "Rotor"), AP_ROT_Y, 0, 360, 3.0f, false, false);
    addAnim(s, findIn(r, "DedoIzq"), AP_POS_X, -0.23f, -0.07f, 0.9f, true, true);
    addAnim(s, findIn(r, "DedoDer"), AP_POS_X, 0.23f, 0.07f, 0.9f, true, true);
}

void animateJoints(Scene& s, SceneNode* root) {
    std::vector<SceneNode*> joints;
    collectJoints(root, joints);
    for (size_t i = 0; i < joints.size(); ++i) {
        SceneNode* j = joints[i];
        int prop = (j->joint.type == JOINT_PRISMATIC ? AP_POS_X : AP_ROT_X) + j->joint.axis;
        float from, to;
        if (j->joint.type == JOINT_PRISMATIC) {
            from = j->joint.minV;
            to = j->joint.maxV;
        } else {
            float cur = j->jointValue();
            from = clampf(cur - 40.0f, j->joint.minV, j->joint.maxV);
            to = clampf(cur + 40.0f, j->joint.minV, j->joint.maxV);
        }
        addAnim(s, j, prop, from, to, 1.8f + 0.4f * (float)i, true, true);
    }
}

SceneNode* createSolarSystem(Scene& s, const Vec3& pos, bool withAnimations) {
    SceneNode* root = s.createNode(s.uniqueName("SistemaSolar", false), NODE_EMPTY);
    root->position = pos;
    SceneNode* sun = part(s, root, "Sol", "sphere", Vec3(), Vec3(), Vec3(1.2f, 1.2f, 1.2f), Vec3(1.0f, 0.85f, 0.5f), TEX_SUN, 0.0f, 8);
    sun->material.emission = 0.85f;
    sun->castShadow = false;

    SceneNode* orbitE = joint(s, root, "OrbitaTierra", Vec3(), JOINT_REVOLUTE, 1, -360, 360);
    SceneNode* earth = part(s, orbitE, "Tierra", "sphere", Vec3(2.4f, 0, 0), Vec3(0, 0, 23), Vec3(0.5f, 0.5f, 0.5f), Vec3(1, 1, 1), TEX_EARTH, 0.35f, 40);
    SceneNode* orbitM = joint(s, orbitE, "OrbitaLuna", Vec3(2.4f, 0, 0), JOINT_REVOLUTE, 1, -360, 360);
    part(s, orbitM, "Luna", "sphere", Vec3(0.55f, 0, 0), Vec3(), Vec3(0.16f, 0.16f, 0.16f), Vec3(1, 1, 1), TEX_MOON, 0.1f, 16);

    SceneNode* orbitS = joint(s, root, "OrbitaSaturno", Vec3(), JOINT_REVOLUTE, 1, -360, 360);
    orbitS->rotation.y = 140;
    SceneNode* saturn = part(s, orbitS, "Saturno", "sphere", Vec3(3.9f, 0, 0), Vec3(0, 0, 20), Vec3(0.45f, 0.45f, 0.45f), Vec3(0.92f, 0.8f, 0.55f), TEX_MARBLE, 0.3f, 30);
    part(s, saturn, "Anillo", "torus", Vec3(), Vec3(), Vec3(1.9f, 0.12f, 1.9f), Vec3(0.85f, 0.75f, 0.55f), "", 0.2f, 20);

    if (withAnimations) {
        addAnim(s, orbitE, AP_ROT_Y, 0, 360, 10.0f, false, false);
        addAnim(s, earth, AP_ROT_Y, 0, 360, 2.5f, false, false);
        addAnim(s, orbitM, AP_ROT_Y, 0, 360, 3.0f, false, false);
        addAnim(s, orbitS, AP_ROT_Y, 140, 500, 18.0f, false, false);
        addAnim(s, sun, AP_ROT_Y, 0, 360, 20.0f, false, false);
    }
    return root;
}

void buildEmptyScene(Scene& s) {
    s.clear();
    s.ambient = 0.22f;
    SceneNode* floor = s.createNode("Suelo", NODE_MESH);
    floor->mesh = "plane";
    floor->scale = Vec3(20, 1, 20);
    floor->receiveShadow = true;
    floor->castShadow = false;
    floor->material.color = Vec3(0.9f, 0.9f, 0.9f);
    floor->material.texture = TEX_FLOOR;
    floor->material.tiling = 10;
    floor->material.specular = 0.1f;
    SceneNode* light = s.createNode("Luz", NODE_LIGHT);
    light->position = Vec3(-4, 7, 4);
    light->light.color = Vec3(1.0f, 0.97f, 0.9f);
    light->castShadow = false;
}

void buildDemoScene(Scene& s, MeshLibrary& lib) {
    buildEmptyScene(s);
    SceneNode* fill = createLight(s, true);
    fill->name = "LuzRelleno";
    fill->position = Vec3(5, 4, -3);
    fill->light.color = Vec3(0.55f, 0.62f, 0.8f);
    fill->light.intensity = 0.45f;

    SceneNode* cube = createPrimitive(s, "cube");
    cube->position = Vec3(-0.6f, 0.6f, 1.2f);
    cube->scale = Vec3(1.2f, 1.2f, 1.2f);
    cube->rotation.y = 45;
    cube->material.color = Vec3(1, 1, 1);
    cube->material.texture = TEX_BRICK;
    cube->material.specular = 0.15f;
    cube->material.shininess = 16;

    SceneNode* sph = createPrimitive(s, "sphere");
    sph->position = Vec3(-3.0f, 0.7f, -0.2f);
    sph->scale = Vec3(1.4f, 1.4f, 1.4f);
    sph->material.color = Vec3(0.86f, 0.87f, 0.9f);
    sph->material.texture = TEX_METAL;
    sph->material.specular = 0.95f;
    sph->material.shininess = 96;

    SceneNode* cyl = createPrimitive(s, "cylinder");
    cyl->position = Vec3(1.3f, 0.8f, 0.0f);
    cyl->scale = Vec3(0.9f, 1.6f, 0.9f);
    cyl->material.color = Vec3(1, 1, 1);
    cyl->material.texture = TEX_CONCRETE;
    cyl->material.specular = 0.2f;

    SceneNode* box = createPrimitive(s, "cube");
    box->name = "Caja_01";
    box->position = Vec3(3.0f, 0.6f, 2.2f);
    box->scale = Vec3(1.2f, 1.2f, 1.2f);
    box->rotation.y = 20;
    box->material.color = Vec3(1, 1, 1);
    box->material.texture = TEX_CRATE;
    box->material.specular = 0.1f;

    SceneNode* tor = createPrimitive(s, "torus");
    tor->position = Vec3(-2.8f, 0.2f, 2.8f);
    tor->scale = Vec3(1.3f, 1.3f, 1.3f);
    tor->material.color = Vec3(0.85f, 0.25f, 0.22f);
    tor->material.specular = 0.7f;
    tor->material.shininess = 64;

    SceneNode* cone = createPrimitive(s, "cone");
    cone->position = Vec3(-4.6f, 0.6f, 1.8f);
    cone->scale = Vec3(0.9f, 1.2f, 0.9f);
    cone->material.color = Vec3(0.25f, 0.55f, 0.9f);
    cone->material.specular = 0.5f;

    if (util::fileExists("assets/models/peon.obj")) {
        SceneNode* pawn = importObj(s, lib, "assets/models/peon.obj");
        if (pawn) {
            pawn->name = "Peon_01";
            pawn->position.x = 0.6f;
            pawn->position.z = 3.4f;
            pawn->material.color = Vec3(1, 1, 1);
            pawn->material.texture = TEX_MARBLE;
            pawn->material.specular = 0.6f;
            pawn->material.shininess = 64;
        }
    }

    createRobotArm(s, Vec3(3.6f, 0, -2.0f), true);

    Animation a; // Cubo_01 girando en Y (como en el enunciado)
    a.nodeId = cube->id;
    a.prop = AP_ROT_Y;
    a.from = 0;
    a.to = 360;
    a.duration = 3.0f;
    a.loop = true;
    s.animations.insert(s.animations.begin(), a);
}

} // namespace SceneFactory
