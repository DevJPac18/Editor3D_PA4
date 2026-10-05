// SceneIO.h - Guardado y carga de escenas en formato de texto (.e3d)
#pragma once
#include "scene/Scene.h"
#include "render/Camera.h"
#include <string>

namespace SceneIO {
std::string serialize(const Scene& scene, const Camera* cam);
bool deserialize(Scene& scene, const std::string& data, Camera* cam);
bool save(const std::string& path, const Scene& scene, const Camera* cam);
bool load(const std::string& path, Scene& scene, Camera* cam);
}
