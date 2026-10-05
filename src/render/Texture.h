// Texture.h - Carga de imágenes (BMP / TGA), caché de texturas OpenGL y texturas procedurales
#pragma once
#include <string>
#include <vector>
#include <map>

class TextureManager {
public:
    unsigned int get(const std::string& path); // 0 si no se puede cargar
    void forget(const std::string& path);
    // Devuelve RGBA en orden de filas de OpenGL (primera fila = abajo)
    static bool loadImage(const std::string& path, int& w, int& h, std::vector<unsigned char>& rgba);

private:
    std::map<std::string, unsigned int> cache;
    float maxAniso = -1.0f;
};

namespace TexGen {
// Genera (si no existen) las texturas de ejemplo en la carpeta indicada
void ensureDefaults(const std::string& dir);
bool saveBMP(const std::string& path, int w, int h, const std::vector<unsigned char>& rgbTopDown);
}
