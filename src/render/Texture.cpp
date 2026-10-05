#include "render/Texture.h"
#include "render/GL.h"
#include "render/GLExt.h"
#include "core/Utils.h"
#include <cstdio>
#include <cstring>

static unsigned int rd16(const unsigned char* p) { return p[0] | (p[1] << 8); }
static unsigned int rd32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24); }

static bool loadBMP(const std::vector<unsigned char>& d, int& w, int& h, std::vector<unsigned char>& out) {
    if (d.size() < 54 || d[0] != 'B' || d[1] != 'M') return false;
    unsigned int offset = rd32(&d[10]);
    int width = (int)rd32(&d[18]);
    int height = (int)rd32(&d[22]);
    int bpp = (int)rd16(&d[28]);
    unsigned int comp = rd32(&d[30]);
    if ((bpp != 24 && bpp != 32) || (comp != 0 && comp != 3) || width <= 0 || height == 0) return false;
    bool topDown = height < 0;
    if (topDown) height = -height;
    int bytes = bpp / 8;
    int stride = ((width * bytes) + 3) & ~3;
    if (offset + (size_t)stride * height > d.size()) return false;
    w = width; h = height;
    out.resize((size_t)w * h * 4);
    for (int y = 0; y < h; ++y) {
        int srcRow = topDown ? (h - 1 - y) : y; // salida: fila 0 = abajo
        const unsigned char* src = &d[offset + (size_t)srcRow * stride];
        unsigned char* dst = &out[(size_t)y * w * 4];
        for (int x = 0; x < w; ++x) {
            dst[x * 4 + 0] = src[x * bytes + 2];
            dst[x * 4 + 1] = src[x * bytes + 1];
            dst[x * 4 + 2] = src[x * bytes + 0];
            dst[x * 4 + 3] = bytes == 4 ? src[x * bytes + 3] : 255;
        }
    }
    if (bpp == 32) { // muchas BMP de 32 bits tienen alfa = 0: tratarlas como opacas
        bool allZero = true;
        for (size_t i = 3; i < out.size(); i += 4) if (out[i]) { allZero = false; break; }
        if (allZero) for (size_t i = 3; i < out.size(); i += 4) out[i] = 255;
    }
    return true;
}

static bool loadTGA(const std::vector<unsigned char>& d, int& w, int& h, std::vector<unsigned char>& out) {
    if (d.size() < 18) return false;
    int idLen = d[0], type = d[2];
    w = (int)rd16(&d[12]); h = (int)rd16(&d[14]);
    int bpp = d[16];
    bool topOrigin = (d[17] & 0x20) != 0;
    if ((type != 2 && type != 10) || (bpp != 24 && bpp != 32) || w <= 0 || h <= 0) return false;
    int bytes = bpp / 8;
    size_t pos = 18 + idLen;
    std::vector<unsigned char> pix((size_t)w * h * 4);
    size_t count = (size_t)w * h, i = 0;
    while (i < count) {
        if (type == 2) {
            if (pos + bytes > d.size()) return false;
            pix[i * 4 + 0] = d[pos + 2]; pix[i * 4 + 1] = d[pos + 1]; pix[i * 4 + 2] = d[pos];
            pix[i * 4 + 3] = bytes == 4 ? d[pos + 3] : 255;
            pos += bytes; ++i;
        } else {
            if (pos >= d.size()) return false;
            unsigned char hdr = d[pos++];
            int n = (hdr & 0x7F) + 1;
            if (hdr & 0x80) {
                if (pos + bytes > d.size()) return false;
                for (int k = 0; k < n && i < count; ++k, ++i) {
                    pix[i * 4 + 0] = d[pos + 2]; pix[i * 4 + 1] = d[pos + 1]; pix[i * 4 + 2] = d[pos];
                    pix[i * 4 + 3] = bytes == 4 ? d[pos + 3] : 255;
                }
                pos += bytes;
            } else {
                for (int k = 0; k < n && i < count; ++k, ++i) {
                    if (pos + bytes > d.size()) return false;
                    pix[i * 4 + 0] = d[pos + 2]; pix[i * 4 + 1] = d[pos + 1]; pix[i * 4 + 2] = d[pos];
                    pix[i * 4 + 3] = bytes == 4 ? d[pos + 3] : 255;
                    pos += bytes;
                }
            }
        }
    }
    out.resize(pix.size());
    for (int y = 0; y < h; ++y) {
        int src = topOrigin ? (h - 1 - y) : y;
        memcpy(&out[(size_t)y * w * 4], &pix[(size_t)src * w * 4], (size_t)w * 4);
    }
    return true;
}

bool TextureManager::loadImage(const std::string& path, int& w, int& h, std::vector<unsigned char>& rgba) {
    std::string data;
    if (!util::readTextFile(path, data)) return false;
    std::vector<unsigned char> d(data.begin(), data.end());
    std::string ext = util::extension(path);
    if (ext == "bmp") return loadBMP(d, w, h, rgba);
    if (ext == "tga") return loadTGA(d, w, h, rgba);
    return loadBMP(d, w, h, rgba) || loadTGA(d, w, h, rgba);
}

unsigned int TextureManager::get(const std::string& path) {
    if (path.empty()) return 0;
    std::map<std::string, unsigned int>::iterator it = cache.find(path);
    if (it != cache.end()) return it->second;
    int w = 0, h = 0;
    std::vector<unsigned char> rgba;
    GLuint tex = 0;
    if (loadImage(path, w, h, rgba)) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        if (maxAniso < 0) {
            maxAniso = 0;
            if (hasExtension("GL_EXT_texture_filter_anisotropic"))
                glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
        }
        if (maxAniso > 1.0f) glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso < 8 ? maxAniso : 8);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    cache[path] = tex;
    return tex;
}

void TextureManager::forget(const std::string& path) {
    std::map<std::string, unsigned int>::iterator it = cache.find(path);
    if (it == cache.end()) return;
    if (it->second) glDeleteTextures(1, &it->second);
    cache.erase(it);
}
