#include "render/GLExt.h"
#include <cstring>
#include <cstdlib>

E3D_PFN_CREATESHADER gl2CreateShader = nullptr;
E3D_PFN_SHADERSOURCE gl2ShaderSource = nullptr;
E3D_PFN_COMPILESHADER gl2CompileShader = nullptr;
E3D_PFN_GETSHADERIV gl2GetShaderiv = nullptr;
E3D_PFN_GETSHADERINFOLOG gl2GetShaderInfoLog = nullptr;
E3D_PFN_CREATEPROGRAM gl2CreateProgram = nullptr;
E3D_PFN_ATTACHSHADER gl2AttachShader = nullptr;
E3D_PFN_LINKPROGRAM gl2LinkProgram = nullptr;
E3D_PFN_GETPROGRAMIV gl2GetProgramiv = nullptr;
E3D_PFN_GETPROGRAMINFOLOG gl2GetProgramInfoLog = nullptr;
E3D_PFN_USEPROGRAM gl2UseProgram = nullptr;
E3D_PFN_GETUNIFORMLOCATION gl2GetUniformLocation = nullptr;
E3D_PFN_UNIFORM1I gl2Uniform1i = nullptr;
E3D_PFN_UNIFORM1F gl2Uniform1f = nullptr;
E3D_PFN_DELETESHADER gl2DeleteShader = nullptr;
static E3D_PFN_SWAPINTERVAL wglSwapInterval = nullptr;
static bool gGLSL = false;

static void* getProc(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if (p == (void*)0 || p == (void*)1 || p == (void*)2 || p == (void*)3 || p == (void*)-1) return nullptr;
    return p;
}

#define LOADGL(var, type, name) var = (type)getProc(name)

bool loadGLExtensions() {
    LOADGL(gl2CreateShader, E3D_PFN_CREATESHADER, "glCreateShader");
    LOADGL(gl2ShaderSource, E3D_PFN_SHADERSOURCE, "glShaderSource");
    LOADGL(gl2CompileShader, E3D_PFN_COMPILESHADER, "glCompileShader");
    LOADGL(gl2GetShaderiv, E3D_PFN_GETSHADERIV, "glGetShaderiv");
    LOADGL(gl2GetShaderInfoLog, E3D_PFN_GETSHADERINFOLOG, "glGetShaderInfoLog");
    LOADGL(gl2CreateProgram, E3D_PFN_CREATEPROGRAM, "glCreateProgram");
    LOADGL(gl2AttachShader, E3D_PFN_ATTACHSHADER, "glAttachShader");
    LOADGL(gl2LinkProgram, E3D_PFN_LINKPROGRAM, "glLinkProgram");
    LOADGL(gl2GetProgramiv, E3D_PFN_GETPROGRAMIV, "glGetProgramiv");
    LOADGL(gl2GetProgramInfoLog, E3D_PFN_GETPROGRAMINFOLOG, "glGetProgramInfoLog");
    LOADGL(gl2UseProgram, E3D_PFN_USEPROGRAM, "glUseProgram");
    LOADGL(gl2GetUniformLocation, E3D_PFN_GETUNIFORMLOCATION, "glGetUniformLocation");
    LOADGL(gl2Uniform1i, E3D_PFN_UNIFORM1I, "glUniform1i");
    LOADGL(gl2Uniform1f, E3D_PFN_UNIFORM1F, "glUniform1f");
    LOADGL(gl2DeleteShader, E3D_PFN_DELETESHADER, "glDeleteShader");
    LOADGL(wglSwapInterval, E3D_PFN_SWAPINTERVAL, "wglSwapIntervalEXT");

    const char* ver = (const char*)glGetString(GL_VERSION);
    int major = ver ? atoi(ver) : 1;
    gGLSL = major >= 2 && gl2CreateShader && gl2ShaderSource && gl2CompileShader && gl2GetShaderiv &&
            gl2CreateProgram && gl2AttachShader && gl2LinkProgram && gl2GetProgramiv && gl2UseProgram &&
            gl2GetUniformLocation && gl2Uniform1i && gl2Uniform1f;
    return true;
}

bool hasGLSL() { return gGLSL; }

bool hasExtension(const char* name) {
    const char* ext = (const char*)glGetString(GL_EXTENSIONS);
    return ext && strstr(ext, name) != nullptr;
}

void setVSync(bool on) {
    if (wglSwapInterval) wglSwapInterval(on ? 1 : 0);
}

std::string glInfoString() {
    const char* r = (const char*)glGetString(GL_RENDERER);
    const char* v = (const char*)glGetString(GL_VERSION);
    std::string s = "OpenGL ";
    s += v ? v : "?";
    s += " - ";
    s += r ? r : "?";
    return s;
}
