// Formato (una instrucción por línea):
//   EDITOR3D 1
//   AMBIENT a
//   CAMERA tx ty tz distancia yaw pitch fov
//   NODE id idPadre tipo "nombre" "malla"
//   TRS px py pz rx ry rz sx sy sz
//   FLAGS visible proyectaSombra recibeSombra expandido
//   MAT r g b ambiente difusa especular brillo emision repeticion "textura"
//   LIGHT r g b intensidad direccional
//   JOINT tipo eje min max
//   ANIM idNodo propiedad desde hasta duracion retardo repetir idaVuelta suave activa
#include "io/SceneIO.h"
#include "core/Utils.h"
#include <cstdlib>
#include <sstream>
#include <vector>

namespace {

std::string q(const std::string& s) {
    std::string r = "\"";
    for (size_t i = 0; i < s.size(); ++i) r += (s[i] == '"' ? '\'' : s[i]);
    return r + "\"";
}

std::string f(float v) { return util::format("%.6g", v); }

void writeNode(std::string& out, const SceneNode* n) {
    int pid = (n->parent && n->parent->parent) ? n->parent->id : 0;
    out += util::format("NODE %d %d %d ", n->id, pid, (int)n->type) + q(n->name) + " " + q(n->mesh) + "\n";
    out += "TRS " + f(n->position.x) + " " + f(n->position.y) + " " + f(n->position.z) + " " +
           f(n->rotation.x) + " " + f(n->rotation.y) + " " + f(n->rotation.z) + " " +
           f(n->scale.x) + " " + f(n->scale.y) + " " + f(n->scale.z) + "\n";
    out += util::format("FLAGS %d %d %d %d\n", n->visible ? 1 : 0, n->castShadow ? 1 : 0,
                        n->receiveShadow ? 1 : 0, n->expanded ? 1 : 0);
    const Material& m = n->material;
    out += "MAT " + f(m.color.x) + " " + f(m.color.y) + " " + f(m.color.z) + " " + f(m.ambient) + " " +
           f(m.diffuse) + " " + f(m.specular) + " " + f(m.shininess) + " " + f(m.emission) + " " +
           f(m.tiling) + " " + q(m.texture) + "\n";
    if (n->type == NODE_LIGHT)
        out += "LIGHT " + f(n->light.color.x) + " " + f(n->light.color.y) + " " + f(n->light.color.z) + " " +
               f(n->light.intensity) + util::format(" %d\n", n->light.directional ? 1 : 0);
    if (n->joint.type != JOINT_NONE)
        out += util::format("JOINT %d %d ", (int)n->joint.type, n->joint.axis) + f(n->joint.minV) + " " +
               f(n->joint.maxV) + "\n";
    for (size_t i = 0; i < n->children.size(); ++i) writeNode(out, n->children[i]);
}

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> t;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) ++i;
        if (i >= line.size()) break;
        if (line[i] == '"') {
            size_t j = line.find('"', i + 1);
            if (j == std::string::npos) j = line.size();
            t.push_back(line.substr(i + 1, j - i - 1));
            i = j + 1;
        } else {
            size_t j = i;
            while (j < line.size() && line[j] != ' ' && line[j] != '\t' && line[j] != '\r') ++j;
            t.push_back(line.substr(i, j - i));
            i = j;
        }
    }
    return t;
}

float F(const std::vector<std::string>& t, size_t i) { return i < t.size() ? (float)atof(t[i].c_str()) : 0.0f; }
int I(const std::vector<std::string>& t, size_t i) { return i < t.size() ? atoi(t[i].c_str()) : 0; }

} // namespace

namespace SceneIO {

std::string serialize(const Scene& scene, const Camera* cam) {
    std::string out = "EDITOR3D 1\n";
    out += "AMBIENT " + f(scene.ambient) + "\n";
    if (cam)
        out += "CAMERA " + f(cam->target.x) + " " + f(cam->target.y) + " " + f(cam->target.z) + " " +
               f(cam->distance) + " " + f(cam->yaw) + " " + f(cam->pitch) + " " + f(cam->fov) + "\n";
    for (size_t i = 0; i < scene.root->children.size(); ++i) writeNode(out, scene.root->children[i]);
    for (size_t i = 0; i < scene.animations.size(); ++i) {
        const Animation& a = scene.animations[i];
        out += util::format("ANIM %d %d ", a.nodeId, a.prop) + f(a.from) + " " + f(a.to) + " " + f(a.duration) +
               " " + f(a.delay) + util::format(" %d %d %d %d\n", a.loop ? 1 : 0, a.pingpong ? 1 : 0,
                                                 a.smooth ? 1 : 0, a.enabled ? 1 : 0);
    }
    return out;
}

bool deserialize(Scene& scene, const std::string& data, Camera* cam) {
    if (data.compare(0, 8, "EDITOR3D") != 0) return false;
    scene.clear();
    std::istringstream in(data);
    std::string line;
    SceneNode* cur = nullptr;
    while (std::getline(in, line)) {
        std::vector<std::string> t = tokenize(line);
        if (t.empty()) continue;
        const std::string& k = t[0];
        if (k == "AMBIENT") scene.ambient = F(t, 1);
        else if (k == "CAMERA" && cam) {
            cam->target = Vec3(F(t, 1), F(t, 2), F(t, 3));
            cam->distance = F(t, 4); cam->yaw = F(t, 5); cam->pitch = F(t, 6); cam->fov = F(t, 7);
            if (cam->fov < 5) cam->fov = 45;
            if (cam->distance < 0.3f) cam->distance = 10;
        } else if (k == "NODE") {
            int id = I(t, 1), pid = I(t, 2), type = I(t, 3);
            SceneNode* parent = pid > 0 ? scene.findById(pid) : nullptr;
            if (id <= 0 || scene.findById(id)) id = scene.nextId;
            cur = scene.createNodeWithId(id, t.size() > 4 ? t[4] : "Nodo", (NodeType)type, parent);
            cur->mesh = t.size() > 5 ? t[5] : "";
        } else if (!cur) {
            if (k == "ANIM") {} else continue;
        }
        if (k == "TRS" && cur) {
            cur->position = Vec3(F(t, 1), F(t, 2), F(t, 3));
            cur->rotation = Vec3(F(t, 4), F(t, 5), F(t, 6));
            cur->scale = Vec3(F(t, 7), F(t, 8), F(t, 9));
        } else if (k == "FLAGS" && cur) {
            cur->visible = I(t, 1) != 0; cur->castShadow = I(t, 2) != 0;
            cur->receiveShadow = I(t, 3) != 0; cur->expanded = I(t, 4) != 0;
        } else if (k == "MAT" && cur) {
            Material& m = cur->material;
            m.color = Vec3(F(t, 1), F(t, 2), F(t, 3));
            m.ambient = F(t, 4); m.diffuse = F(t, 5); m.specular = F(t, 6);
            m.shininess = F(t, 7); m.emission = F(t, 8); m.tiling = F(t, 9);
            m.texture = t.size() > 10 ? t[10] : "";
            if (m.tiling <= 0) m.tiling = 1;
        } else if (k == "LIGHT" && cur) {
            cur->light.color = Vec3(F(t, 1), F(t, 2), F(t, 3));
            cur->light.intensity = F(t, 4);
            cur->light.directional = I(t, 5) != 0;
        } else if (k == "JOINT" && cur) {
            cur->joint.type = (JointType)I(t, 1);
            cur->joint.axis = I(t, 2) % 3;
            cur->joint.minV = F(t, 3);
            cur->joint.maxV = F(t, 4);
        } else if (k == "ANIM") {
            Animation a;
            a.nodeId = I(t, 1); a.prop = I(t, 2); a.from = F(t, 3); a.to = F(t, 4);
            a.duration = F(t, 5); a.delay = F(t, 6);
            a.loop = I(t, 7) != 0; a.pingpong = I(t, 8) != 0; a.smooth = I(t, 9) != 0;
            a.enabled = t.size() > 10 ? I(t, 10) != 0 : true;
            if (a.prop < 0 || a.prop >= AP_COUNT) a.prop = AP_ROT_Y;
            scene.animations.push_back(a);
        }
    }
    return true;
}

bool save(const std::string& path, const Scene& scene, const Camera* cam) {
    return util::writeTextFile(path, serialize(scene, cam));
}

bool load(const std::string& path, Scene& scene, Camera* cam) {
    std::string data;
    if (!util::readTextFile(path, data)) return false;
    return deserialize(scene, data, cam);
}

} // namespace SceneIO
