// ObjLoader.h - Carga de modelos Wavefront OBJ (v, vt, vn, f)
#pragma once
#include "render/Mesh.h"
#include <string>

// normalize = true: centra el modelo y lo escala para que su mayor dimensión mida 1.
bool loadOBJ(const std::string& path, Mesh& out, bool normalize);
