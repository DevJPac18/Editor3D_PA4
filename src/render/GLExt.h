// GLExt.h - Carga manual de las funciones de OpenGL 2.0 (GLSL) en Windows
#pragma once
#include "render/GL.h"
#include <string>

typedef char E3D_GLchar;
typedef GLuint(APIENTRY* E3D_PFN_CREATESHADER)(GLenum);
typedef void(APIENTRY* E3D_PFN_SHADERSOURCE)(GLuint, GLsizei, const E3D_GLchar**, const GLint*);
typedef void(APIENTRY* E3D_PFN_COMPILESHADER)(GLuint);
typedef void(APIENTRY* E3D_PFN_GETSHADERIV)(GLuint, GLenum, GLint*);
typedef void(APIENTRY* E3D_PFN_GETSHADERINFOLOG)(GLuint, GLsizei, GLsizei*, E3D_GLchar*);
typedef GLuint(APIENTRY* E3D_PFN_CREATEPROGRAM)(void);
typedef void(APIENTRY* E3D_PFN_ATTACHSHADER)(GLuint, GLuint);
typedef void(APIENTRY* E3D_PFN_LINKPROGRAM)(GLuint);
typedef void(APIENTRY* E3D_PFN_GETPROGRAMIV)(GLuint, GLenum, GLint*);
typedef void(APIENTRY* E3D_PFN_GETPROGRAMINFOLOG)(GLuint, GLsizei, GLsizei*, E3D_GLchar*);
typedef void(APIENTRY* E3D_PFN_USEPROGRAM)(GLuint);
typedef GLint(APIENTRY* E3D_PFN_GETUNIFORMLOCATION)(GLuint, const E3D_GLchar*);
typedef void(APIENTRY* E3D_PFN_UNIFORM1I)(GLint, GLint);
typedef void(APIENTRY* E3D_PFN_UNIFORM1F)(GLint, GLfloat);
typedef void(APIENTRY* E3D_PFN_DELETESHADER)(GLuint);
typedef BOOL(APIENTRY* E3D_PFN_SWAPINTERVAL)(int);

extern E3D_PFN_CREATESHADER gl2CreateShader;
extern E3D_PFN_SHADERSOURCE gl2ShaderSource;
extern E3D_PFN_COMPILESHADER gl2CompileShader;
extern E3D_PFN_GETSHADERIV gl2GetShaderiv;
extern E3D_PFN_GETSHADERINFOLOG gl2GetShaderInfoLog;
extern E3D_PFN_CREATEPROGRAM gl2CreateProgram;
extern E3D_PFN_ATTACHSHADER gl2AttachShader;
extern E3D_PFN_LINKPROGRAM gl2LinkProgram;
extern E3D_PFN_GETPROGRAMIV gl2GetProgramiv;
extern E3D_PFN_GETPROGRAMINFOLOG gl2GetProgramInfoLog;
extern E3D_PFN_USEPROGRAM gl2UseProgram;
extern E3D_PFN_GETUNIFORMLOCATION gl2GetUniformLocation;
extern E3D_PFN_UNIFORM1I gl2Uniform1i;
extern E3D_PFN_UNIFORM1F gl2Uniform1f;
extern E3D_PFN_DELETESHADER gl2DeleteShader;

bool loadGLExtensions();   // requiere un contexto activo
bool hasGLSL();            // OpenGL >= 2.0 y funciones cargadas
bool hasExtension(const char* name);
void setVSync(bool on);
std::string glInfoString();
