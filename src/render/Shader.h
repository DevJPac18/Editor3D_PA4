// Shader.h - Programa GLSL 1.10 (OpenGL 2.0): iluminación Phong por píxel + textura
#pragma once
#include <string>

class Shader {
public:
    bool init();                 // compila y enlaza; false si no hay soporte
    void bind();
    void unbind();
    void setTextured(bool on);
    void setLights(bool l0, bool l1);
    bool ok() const { return program != 0; }
    const std::string& log() const { return errorLog; }

private:
    unsigned int program = 0;
    int locTex = -1, locUseTex = -1, locL0 = -1, locL1 = -1;
    std::string errorLog;
    unsigned int compile(unsigned int type, const char* src);
};
