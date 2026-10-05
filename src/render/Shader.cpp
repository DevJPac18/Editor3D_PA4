#include "render/Shader.h"
#include "render/GLExt.h"
#include <vector>

// Los shaders leen el estado fijo de OpenGL (gl_LightSource, gl_FrontMaterial...),
// así el mismo código de la aplicación sirve para el pipeline fijo y el programable.
static const char* VERT_SRC =
    "#version 110\n"
    "varying vec3 vN;\n"
    "varying vec3 vP;\n"
    "void main() {\n"
    "  vec4 p = gl_ModelViewMatrix * gl_Vertex;\n"
    "  vP = p.xyz;\n"
    "  vN = gl_NormalMatrix * gl_Normal;\n"
    "  gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;\n"
    "  gl_Position = gl_ProjectionMatrix * p;\n"
    "}\n";

static const char* FRAG_SRC =
    "#version 110\n"
    "uniform sampler2D uTex;\n"
    "uniform float uUseTex;\n"
    "uniform float uLight0;\n"
    "uniform float uLight1;\n"
    "varying vec3 vN;\n"
    "varying vec3 vP;\n"
    "void addLight(vec4 lpos, vec3 ldif, vec3 lspec, vec3 lamb, vec3 N, vec3 V,\n"
    "              inout vec3 amb, inout vec3 dif, inout vec3 spe) {\n"
    "  vec3 L = (lpos.w == 0.0) ? normalize(lpos.xyz) : normalize(lpos.xyz - vP);\n"
    "  float nl = max(dot(N, L), 0.0);\n"
    "  amb += lamb * gl_FrontMaterial.ambient.rgb;\n"
    "  dif += ldif * gl_FrontMaterial.diffuse.rgb * nl;\n"
    "  if (nl > 0.0) {\n"
    "    vec3 H = normalize(L + V);\n"
    "    spe += lspec * gl_FrontMaterial.specular.rgb * pow(max(dot(N, H), 0.0), gl_FrontMaterial.shininess);\n"
    "  }\n"
    "}\n"
    "void main() {\n"
    "  vec3 N = normalize(vN);\n"
    "  vec3 V = normalize(-vP);\n"
    "  vec3 amb = gl_LightModel.ambient.rgb * gl_FrontMaterial.ambient.rgb;\n"
    "  vec3 dif = vec3(0.0);\n"
    "  vec3 spe = vec3(0.0);\n"
    "  if (uLight0 > 0.5) addLight(gl_LightSource[0].position, gl_LightSource[0].diffuse.rgb,\n"
    "      gl_LightSource[0].specular.rgb, gl_LightSource[0].ambient.rgb, N, V, amb, dif, spe);\n"
    "  if (uLight1 > 0.5) addLight(gl_LightSource[1].position, gl_LightSource[1].diffuse.rgb,\n"
    "      gl_LightSource[1].specular.rgb, gl_LightSource[1].ambient.rgb, N, V, amb, dif, spe);\n"
    "  vec3 texc = vec3(1.0);\n"
    "  if (uUseTex > 0.5) texc = texture2D(uTex, gl_TexCoord[0].st).rgb;\n"
    "  float hemi = 0.5 + 0.5 * N.y;\n"
    "  vec3 col = (amb * (0.75 + 0.5 * hemi) + dif) * texc + spe + gl_FrontMaterial.emission.rgb * texc;\n"
    "  gl_FragColor = vec4(col, 1.0);\n"
    "}\n";

unsigned int Shader::compile(unsigned int type, const char* src) {
    GLuint s = gl2CreateShader(type);
    gl2ShaderSource(s, 1, &src, NULL);
    gl2CompileShader(s);
    GLint ok = 0;
    gl2GetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        gl2GetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        if (len > 1 && gl2GetShaderInfoLog) {
            std::vector<char> buf(len + 1, 0);
            gl2GetShaderInfoLog(s, len, NULL, buf.data());
            errorLog += buf.data();
        }
        if (gl2DeleteShader) gl2DeleteShader(s);
        return 0;
    }
    return s;
}

bool Shader::init() {
    if (!hasGLSL()) {
        errorLog = "OpenGL 2.0 / GLSL no disponible";
        return false;
    }
    GLuint vs = compile(GL_VERTEX_SHADER, VERT_SRC);
    GLuint fs = compile(GL_FRAGMENT_SHADER, FRAG_SRC);
    if (!vs || !fs) return false;
    GLuint p = gl2CreateProgram();
    gl2AttachShader(p, vs);
    gl2AttachShader(p, fs);
    gl2LinkProgram(p);
    GLint ok = 0;
    gl2GetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        gl2GetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        if (len > 1 && gl2GetProgramInfoLog) {
            std::vector<char> buf(len + 1, 0);
            gl2GetProgramInfoLog(p, len, NULL, buf.data());
            errorLog += buf.data();
        }
        return false;
    }
    program = p;
    locTex = gl2GetUniformLocation(p, "uTex");
    locUseTex = gl2GetUniformLocation(p, "uUseTex");
    locL0 = gl2GetUniformLocation(p, "uLight0");
    locL1 = gl2GetUniformLocation(p, "uLight1");
    gl2UseProgram(p);
    if (locTex >= 0) gl2Uniform1i(locTex, 0);
    gl2UseProgram(0);
    return true;
}

void Shader::bind() { if (program) gl2UseProgram(program); }
void Shader::unbind() { if (program) gl2UseProgram(0); }
void Shader::setTextured(bool on) { if (program && locUseTex >= 0) gl2Uniform1f(locUseTex, on ? 1.0f : 0.0f); }
void Shader::setLights(bool l0, bool l1) {
    if (!program) return;
    if (locL0 >= 0) gl2Uniform1f(locL0, l0 ? 1.0f : 0.0f);
    if (locL1 >= 0) gl2Uniform1f(locL1, l1 ? 1.0f : 0.0f);
}
