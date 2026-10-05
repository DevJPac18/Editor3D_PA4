// Mesh.h - Geometría (arreglos de vértices), primitivas y biblioteca de mallas
#pragma once
#include "core/Math3D.h"
#include <vector>
#include <string>
#include <map>

struct Mesh {
    std::vector<float> positions, normals, uvs;
    std::vector<unsigned int> indices;
    Vec3 bmin, bmax;

    void computeBounds();
    int vertexCount() const { return (int)positions.size() / 3; }
    int triangleCount() const { return (int)indices.size() / 3; }
    bool empty() const { return indices.empty(); }
    void draw(bool withNormals = true, bool withUVs = true) const; // vertex arrays (GL 1.1+)
};

namespace MeshGen {
Mesh cube();
Mesh sphere(int segments = 40, int rings = 24);
Mesh cylinder(int segments = 40);
Mesh cone(int segments = 40);
Mesh plane(int divisions = 8);
Mesh torus(int segments = 48, int sides = 24, float R = 0.35f, float r = 0.15f);
}

class MeshLibrary {
public:
    const Mesh* get(const std::string& name); // primitiva o ruta .obj (con caché)
    void forget(const std::string& name);
    static bool isPrimitive(const std::string& name);
    static const std::vector<std::string>& primitives();
    static std::string displayName(const std::string& name);

private:
    std::map<std::string, Mesh> cache;
};
