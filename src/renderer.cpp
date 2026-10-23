#include "renderer.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "opengl32.lib")

namespace EarthRender {

namespace {

// OpenGL 3.3 Core constants & function pointers resolved via wglGetProcAddress
using ptrdiff_t_gl = std::ptrdiff_t;
using GLsizeiptr = ptrdiff_t_gl;
using GLchar = char;

constexpr GLenum GL_ARRAY_BUFFER_EXT              = 0x8892;
constexpr GLenum GL_ELEMENT_ARRAY_BUFFER_EXT      = 0x8893;
constexpr GLenum GL_STATIC_DRAW_EXT               = 0x88E4;
constexpr GLenum GL_DYNAMIC_DRAW_EXT              = 0x88E8;
constexpr GLenum GL_FRAGMENT_SHADER_EXT           = 0x8B30;
constexpr GLenum GL_VERTEX_SHADER_EXT             = 0x8B31;
constexpr GLenum GL_COMPILE_STATUS_EXT            = 0x8B81;
constexpr GLenum GL_LINK_STATUS_EXT               = 0x8B82;
constexpr GLenum GL_CURRENT_PROGRAM_EXT           = 0x8B8D;
constexpr GLenum GL_VERTEX_ARRAY_BINDING_EXT      = 0x85B5;
constexpr GLenum GL_ARRAY_BUFFER_BINDING_EXT      = 0x8894;
constexpr GLenum GL_ELEMENT_ARRAY_BUFFER_BINDING_EXT = 0x8895;
constexpr GLenum GL_ACTIVE_TEXTURE_EXT            = 0x84E0;
constexpr GLenum GL_TEXTURE0_EXT                  = 0x84C0;
constexpr GLenum GL_CLAMP_TO_EDGE_EXT             = 0x812F;
constexpr GLenum GL_BLEND_SRC_RGB_EXT             = 0x80C9;
constexpr GLenum GL_BLEND_DST_RGB_EXT             = 0x80C8;
constexpr GLenum GL_BLEND_SRC_ALPHA_EXT           = 0x80CB;
constexpr GLenum GL_BLEND_DST_ALPHA_EXT           = 0x80CA;
constexpr GLenum GL_PIXEL_UNPACK_BUFFER_EXT       = 0x88EC;
constexpr GLenum GL_PIXEL_UNPACK_BUFFER_BINDING_EXT = 0x88EF;
constexpr GLenum GL_SAMPLER_BINDING_EXT           = 0x8919;

using PFNGLCREATESHADERPROC             = GLuint (APIENTRY*)(GLenum);
using PFNGLSHADERSOURCEPROC             = void (APIENTRY*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
using PFNGLCOMPILESHADERPROC            = void (APIENTRY*)(GLuint);
using PFNGLGETSHADERIVPROC              = void (APIENTRY*)(GLuint, GLenum, GLint*);
using PFNGLDELETESHADERPROC             = void (APIENTRY*)(GLuint);
using PFNGLCREATEPROGRAMPROC            = GLuint (APIENTRY*)();
using PFNGLATTACHSHADERPROC             = void (APIENTRY*)(GLuint, GLuint);
using PFNGLLINKPROGRAMPROC              = void (APIENTRY*)(GLuint);
using PFNGLGETPROGRAMIVPROC             = void (APIENTRY*)(GLuint, GLenum, GLint*);
using PFNGLUSEPROGRAMPROC               = void (APIENTRY*)(GLuint);
using PFNGLDELETEPROGRAMPROC            = void (APIENTRY*)(GLuint);
using PFNGLGETUNIFORMLOCATIONPROC       = GLint (APIENTRY*)(GLuint, const GLchar*);
using PFNGLUNIFORMMATRIX4FVPROC         = void (APIENTRY*)(GLint, GLsizei, GLboolean, const GLfloat*);
using PFNGLUNIFORM4FPROC                = void (APIENTRY*)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
using PFNGLUNIFORM1FPROC                = void (APIENTRY*)(GLint, GLfloat);
using PFNGLUNIFORM1IPROC                = void (APIENTRY*)(GLint, GLint);
using PFNGLGENVERTEXARRAYSPROC          = void (APIENTRY*)(GLsizei, GLuint*);
using PFNGLBINDVERTEXARRAYPROC          = void (APIENTRY*)(GLuint);
using PFNGLDELETEVERTEXARRAYSPROC       = void (APIENTRY*)(GLsizei, const GLuint*);
using PFNGLGENBUFFERSPROC               = void (APIENTRY*)(GLsizei, GLuint*);
using PFNGLBINDBUFFERPROC               = void (APIENTRY*)(GLenum, GLuint);
using PFNGLBUFFERDATAPROC               = void (APIENTRY*)(GLenum, GLsizeiptr, const void*, GLenum);
using PFNGLDELETEBUFFERSPROC            = void (APIENTRY*)(GLsizei, const GLuint*);
using PFNGLENABLEVERTEXATTRIBARRAYPROC  = void (APIENTRY*)(GLuint);
using PFNGLVERTEXATTRIBPOINTERPROC      = void (APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
using PFNGLACTIVETEXTUREPROC            = void (APIENTRY*)(GLenum);
using PFNGLBLENDFUNCSEPARATEPROC        = void (APIENTRY*)(GLenum, GLenum, GLenum, GLenum);
using PFNGLBINDSAMPLERPROC              = void (APIENTRY*)(GLuint, GLuint);

struct GLProcs {
    PFNGLCREATESHADERPROC            CreateShader = nullptr;
    PFNGLSHADERSOURCEPROC            ShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC           CompileShader = nullptr;
    PFNGLGETSHADERIVPROC             GetShaderiv = nullptr;
    PFNGLDELETESHADERPROC            DeleteShader = nullptr;
    PFNGLCREATEPROGRAMPROC            CreateProgram = nullptr;
    PFNGLATTACHSHADERPROC            AttachShader = nullptr;
    PFNGLLINKPROGRAMPROC             LinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC            GetProgramiv = nullptr;
    PFNGLUSEPROGRAMPROC              UseProgram = nullptr;
    PFNGLDELETEPROGRAMPROC           DeleteProgram = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC      GetUniformLocation = nullptr;
    PFNGLUNIFORMMATRIX4FVPROC        UniformMatrix4fv = nullptr;
    PFNGLUNIFORM4FPROC               Uniform4f = nullptr;
    PFNGLUNIFORM1FPROC               Uniform1f = nullptr;
    PFNGLUNIFORM1IPROC               Uniform1i = nullptr;
    PFNGLGENVERTEXARRAYSPROC         GenVertexArrays = nullptr;
    PFNGLBINDVERTEXARRAYPROC         BindVertexArray = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC      DeleteVertexArrays = nullptr;
    PFNGLGENBUFFERSPROC              GenBuffers = nullptr;
    PFNGLBINDBUFFERPROC              BindBuffer = nullptr;
    PFNGLBUFFERDATAPROC              BufferData = nullptr;
    PFNGLDELETEBUFFERSPROC           DeleteBuffers = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC     VertexAttribPointer = nullptr;
    PFNGLACTIVETEXTUREPROC           ActiveTexture = nullptr;
    PFNGLBLENDFUNCSEPARATEPROC       BlendFuncSeparate = nullptr;
    PFNGLBINDSAMPLERPROC             BindSampler = nullptr;

    bool Load() {
        #define RESOLVE_GL(field, name) \
            field = reinterpret_cast<decltype(field)>(wglGetProcAddress(name)); \
            if (!field) return false;

        RESOLVE_GL(CreateShader,            "glCreateShader");
        RESOLVE_GL(ShaderSource,            "glShaderSource");
        RESOLVE_GL(CompileShader,           "glCompileShader");
        RESOLVE_GL(GetShaderiv,             "glGetShaderiv");
        RESOLVE_GL(DeleteShader,            "glDeleteShader");
        RESOLVE_GL(CreateProgram,           "glCreateProgram");
        RESOLVE_GL(AttachShader,            "glAttachShader");
        RESOLVE_GL(LinkProgram,             "glLinkProgram");
        RESOLVE_GL(GetProgramiv,            "glGetProgramiv");
        RESOLVE_GL(UseProgram,              "glUseProgram");
        RESOLVE_GL(DeleteProgram,           "glDeleteProgram");
        RESOLVE_GL(GetUniformLocation,      "glGetUniformLocation");
        RESOLVE_GL(UniformMatrix4fv,        "glUniformMatrix4fv");
        RESOLVE_GL(Uniform4f,               "glUniform4f");
        RESOLVE_GL(Uniform1f,               "glUniform1f");
        RESOLVE_GL(Uniform1i,               "glUniform1i");
        RESOLVE_GL(GenVertexArrays,         "glGenVertexArrays");
        RESOLVE_GL(BindVertexArray,         "glBindVertexArray");
        RESOLVE_GL(DeleteVertexArrays,      "glDeleteVertexArrays");
        RESOLVE_GL(GenBuffers,              "glGenBuffers");
        RESOLVE_GL(BindBuffer,              "glBindBuffer");
        RESOLVE_GL(BufferData,              "glBufferData");
        RESOLVE_GL(DeleteBuffers,           "glDeleteBuffers");
        RESOLVE_GL(EnableVertexAttribArray, "glEnableVertexAttribArray");
        RESOLVE_GL(VertexAttribPointer,     "glVertexAttribPointer");
        RESOLVE_GL(ActiveTexture,           "glActiveTexture");
        RESOLVE_GL(BlendFuncSeparate,       "glBlendFuncSeparate");
        BindSampler = reinterpret_cast<PFNGLBINDSAMPLERPROC>(wglGetProcAddress("glBindSampler"));

        #undef RESOLVE_GL
        return true;
    }
} gl;

// Saves and restores OpenGL state around our custom pass so Earth.exe's Ion renderer is untouched
struct ScopedGLState {
    GLint prev_prog = 0;
    GLint prev_vao = 0;
    GLint prev_vbo = 0;
    GLint prev_ebo = 0;
    GLint prev_pbo_unpack = 0;
    GLint prev_active_tex = 0;
    GLint prev_tex2d = 0;
    GLint prev_sampler0 = 0;
    GLint prev_unpack_align = 4;
    GLint prev_viewport[4] = {};
    GLint prev_scissor_box[4] = {};
    GLboolean prev_color_mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLboolean prev_depth_test = GL_FALSE;
    GLboolean prev_depth_mask = GL_TRUE;
    GLint prev_depth_func = GL_LESS;
    GLboolean prev_blend = GL_FALSE;
    GLint prev_blend_src_rgb = GL_ONE;
    GLint prev_blend_dst_rgb = GL_ZERO;
    GLint prev_blend_src_a = GL_ONE;
    GLint prev_blend_dst_a = GL_ZERO;
    GLboolean prev_cull = GL_FALSE;
    GLboolean prev_scissor = GL_FALSE;

    ScopedGLState() {
        glGetIntegerv(GL_CURRENT_PROGRAM_EXT, &prev_prog);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING_EXT, &prev_vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING_EXT, &prev_vbo);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_EXT, &prev_ebo);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING_EXT, &prev_pbo_unpack);
        glGetIntegerv(GL_ACTIVE_TEXTURE_EXT, &prev_active_tex);
        gl.ActiveTexture(GL_TEXTURE0_EXT);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex2d);
        if (gl.BindSampler) {
            glGetIntegerv(GL_SAMPLER_BINDING_EXT, &prev_sampler0);
            gl.BindSampler(0, 0);
        }
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack_align);
        glGetIntegerv(GL_VIEWPORT, prev_viewport);
        glGetIntegerv(GL_SCISSOR_BOX, prev_scissor_box);
        glGetBooleanv(GL_COLOR_WRITEMASK, prev_color_mask);
        prev_depth_test = glIsEnabled(GL_DEPTH_TEST);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &prev_depth_mask);
        glGetIntegerv(GL_DEPTH_FUNC, &prev_depth_func);
        prev_blend = glIsEnabled(GL_BLEND);
        glGetIntegerv(GL_BLEND_SRC_RGB_EXT, &prev_blend_src_rgb);
        glGetIntegerv(GL_BLEND_DST_RGB_EXT, &prev_blend_dst_rgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA_EXT, &prev_blend_src_a);
        glGetIntegerv(GL_BLEND_DST_ALPHA_EXT, &prev_blend_dst_a);
        prev_cull = glIsEnabled(GL_CULL_FACE);
        prev_scissor = glIsEnabled(GL_SCISSOR_TEST);
    }

    ~ScopedGLState() {
        gl.UseProgram(prev_prog);
        gl.BindVertexArray(prev_vao);
        gl.BindBuffer(GL_ARRAY_BUFFER_EXT, prev_vbo);
        gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER_EXT, prev_ebo);
        gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER_EXT, prev_pbo_unpack);
        gl.ActiveTexture(GL_TEXTURE0_EXT);
        glBindTexture(GL_TEXTURE_2D, prev_tex2d);
        if (gl.BindSampler) {
            gl.BindSampler(0, prev_sampler0);
        }
        gl.ActiveTexture(prev_active_tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, prev_unpack_align);
        glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
        glScissor(prev_scissor_box[0], prev_scissor_box[1], prev_scissor_box[2], prev_scissor_box[3]);
        glColorMask(prev_color_mask[0], prev_color_mask[1], prev_color_mask[2], prev_color_mask[3]);
        if (prev_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        glDepthMask(prev_depth_mask);
        glDepthFunc(prev_depth_func);
        if (prev_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        gl.BlendFuncSeparate(prev_blend_src_rgb, prev_blend_dst_rgb, prev_blend_src_a, prev_blend_dst_a);
        if (prev_cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        if (prev_scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    }
};

GLuint CompileProgram(const char* vs_src, const char* fs_src) {
    GLuint vs = gl.CreateShader(GL_VERTEX_SHADER_EXT);
    gl.ShaderSource(vs, 1, &vs_src, nullptr);
    gl.CompileShader(vs);
    GLint ok = 0;
    gl.GetShaderiv(vs, GL_COMPILE_STATUS_EXT, &ok);
    if (!ok) {
        gl.DeleteShader(vs);
        return 0;
    }

    GLuint fs = gl.CreateShader(GL_FRAGMENT_SHADER_EXT);
    gl.ShaderSource(fs, 1, &fs_src, nullptr);
    gl.CompileShader(fs);
    gl.GetShaderiv(fs, GL_COMPILE_STATUS_EXT, &ok);
    if (!ok) {
        gl.DeleteShader(vs);
        gl.DeleteShader(fs);
        return 0;
    }

    GLuint prog = gl.CreateProgram();
    gl.AttachShader(prog, vs);
    gl.AttachShader(prog, fs);
    gl.LinkProgram(prog);
    gl.DeleteShader(vs);
    gl.DeleteShader(fs);
    gl.GetProgramiv(prog, GL_LINK_STATUS_EXT, &ok);
    if (!ok) {
        gl.DeleteProgram(prog);
        return 0;
    }
    return prog;
}

// Compact 5x7 ASCII bitmap font for crisp VR nametags, distances, and UI controls (ASCII 32..95)
const uint8_t kFont5x7[64][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x14,0x08,0x3E,0x08,0x14}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x08,0x14,0x22,0x41,0x00}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x00,0x41,0x22,0x14,0x08}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x32,0x49,0x79,0x41,0x3E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x00,0x41,0x41,0x7F,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
};

void FormatDistance(double meters, char* out, size_t cap) {
    if (meters < 1000.0) {
        std::snprintf(out, cap, "%.1fM", meters);
    } else if (meters < 1000000.0) {
        std::snprintf(out, cap, "%.1fKM", meters * 0.001);
    } else {
        std::snprintf(out, cap, "%.0fKM", meters * 0.001);
    }
}

void FormatScale(double scale, char* out, size_t cap) {
    if (scale < 2.5) {
        std::snprintf(out, cap, "%.1fX HUMAN", scale);
    } else if (scale < 1000.0) {
        std::snprintf(out, cap, "%.0fX GIANT", scale);
    } else if (scale < 1000000.0) {
        std::snprintf(out, cap, "%.1fKX TITAN", scale * 0.001);
    } else {
        std::snprintf(out, cap, "%.1fMX ORBIT", scale * 1e-6);
    }
}

enum ButtonId : int {
    BTN_TAB_TOGGLE         = 1,
    BTN_RECENTER           = 2,
    BTN_CLOSE_PANEL        = 3,

    BTN_START_SERVER_OPEN  = 10,
    BTN_START_SERVER_PW    = 11,
    BTN_FIND_SESSIONS      = 12,
    BTN_CLOSE_SERVER       = 13,
    BTN_TOGGLE_LOCK        = 14,
    BTN_LEAVE_SESSION      = 15,

    BTN_TOGGLE_3D_AVATARS  = 20,
    BTN_TOGGLE_XRAY        = 21,
    BTN_TOGGLE_MARKERS     = 22,
    BTN_TOGGLE_MIC         = 23,

    BTN_PW_BACKSPACE       = 30,
    BTN_PW_CLEAR           = 31,
    BTN_PW_CONFIRM         = 32,
    BTN_PW_CANCEL          = 33,
    BTN_PW_KEY_BASE        = 100, // 100..115 for '0'..'9', 'A'..'F'

    BTN_KICK_PLAYER_BASE   = 200, // 200..203 (Kick from current session only)
    BTN_BAN_PLAYER_BASE    = 220, // 220..223 (Ban across sessions)
    BTN_UNBAN_PLAYER_BASE  = 240, // 240..243 (Unban banned player)
    BTN_TOGGLE_BAN_VIEW    = 260, // Toggle between Connected Players / Sessions and Banned Players list
    BTN_JOIN_SESSION_BASE  = 300  // 300..302
};

constexpr char kKeypadChars[16] = {
    '1', '2', '3', 'A',
    '4', '5', '6', 'B',
    '7', '8', '9', 'C',
    '0', 'D', 'E', 'F'
};

} // namespace

StereoAvatarRenderer::StereoAvatarRenderer() = default;

StereoAvatarRenderer::~StereoAvatarRenderer() {
    Shutdown();
}

void StereoAvatarRenderer::LoadConfig(const std::string& ini_path) {
    int v_3d = GetPrivateProfileIntA("Visuals", "Show3DAvatars", 1, ini_path.c_str());
    int v_xray = GetPrivateProfileIntA("Visuals", "ShowXRayThroughEarth", 1, ini_path.c_str());
    int v_markers = GetPrivateProfileIntA("Visuals", "ShowFloatingMarkers", 1, ini_path.c_str());
    show_3d_avatars_.store(v_3d != 0);
    show_xray_through_earth_.store(v_xray != 0);
    show_floating_markers_.store(v_markers != 0);
}

bool StereoAvatarRenderer::EnsureInitialized() {
    if (gl_ready_) return true;
    if (!wglGetCurrentContext()) return false;
    if (!gl.Load()) return false;

    ScopedGLState saved_state;
    if (!BuildShaders()) return false;
    BuildMeshes();
    BuildFontTexture();

    gl_ready_ = true;
    return true;
}

void StereoAvatarRenderer::Shutdown() {
    if (!gl_ready_) return;
    gl_ready_ = false;
}

bool StereoAvatarRenderer::BuildShaders() {
    const char* avatar_vs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;

uniform mat4 uMVP;
uniform mat4 uModel;

out vec3 vNormal;
out vec3 vViewDir;
out vec4 vColor;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vNormal = normalize(mat3(uModel) * aNormal);
    vViewDir = normalize(-worldPos.xyz);
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

    const char* avatar_fs = R"(#version 330 core
in vec3 vNormal;
in vec3 vViewDir;
in vec4 vColor;

uniform vec4 uPlayerTint;
uniform float uXRayMode;
uniform float uVoiceLevel;

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(vViewDir);
    vec3 L = normalize(vec3(0.35, 0.8, 0.48));

    // If vertex alpha < 0.5, treat RGB as player-tinted accent material
    vec3 baseCol = (vColor.a < 0.6) ? mix(vColor.rgb, uPlayerTint.rgb, 0.85) : vColor.rgb;

    float ndotl = max(dot(N, L), 0.0);
    float hemi = 0.45 + 0.55 * (N.y * 0.5 + 0.5);
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 2.5);

    if (uXRayMode > 0.5) {
        // Holographic X-Ray shader when rendered through the planet
        vec3 holo = mix(baseCol * 0.55, uPlayerTint.rgb * 1.35, fresnel);
        holo += uPlayerTint.rgb * (uVoiceLevel * 0.6);
        float alpha = clamp(0.48 + fresnel * 0.52, 0.45, 0.95);
        FragColor = vec4(holo, alpha);
    } else {
        vec3 lit = baseCol * (0.35 * hemi + 0.65 * ndotl) + uPlayerTint.rgb * (fresnel * 0.35 + uVoiceLevel * 0.35);
        FragColor = vec4(lit, 1.0);
    }
}
)";

    const char* marker_vs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

uniform mat4 uMVP;

out vec2 vUV;
out vec4 vColor;

void main() {
    vUV = aUV;
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

    const char* marker_fs = R"(#version 330 core
in vec2 vUV;
in vec4 vColor;

uniform sampler2D uTex;

out vec4 FragColor;

void main() {
    float mask = texture(uTex, vUV).r;
    if (vColor.a * mask < 0.02) discard;
    FragColor = vec4(vColor.rgb, vColor.a * mask);
}
)";

    avatar_prog_ = CompileProgram(avatar_vs, avatar_fs);
    if (!avatar_prog_) return false;
    u_avatar_mvp_   = gl.GetUniformLocation(avatar_prog_, "uMVP");
    u_avatar_model_ = gl.GetUniformLocation(avatar_prog_, "uModel");
    u_avatar_tint_  = gl.GetUniformLocation(avatar_prog_, "uPlayerTint");
    u_avatar_xray_  = gl.GetUniformLocation(avatar_prog_, "uXRayMode");
    u_avatar_voice_ = gl.GetUniformLocation(avatar_prog_, "uVoiceLevel");

    marker_prog_ = CompileProgram(marker_vs, marker_fs);
    if (!marker_prog_) return false;
    u_marker_mvp_ = gl.GetUniformLocation(marker_prog_, "uMVP");
    u_marker_tex_ = gl.GetUniformLocation(marker_prog_, "uTex");

    gl.GenVertexArrays(1, &marker_vao_);
    gl.GenBuffers(1, &marker_vbo_);
    gl.BindVertexArray(marker_vao_);
    gl.BindBuffer(GL_ARRAY_BUFFER_EXT, marker_vbo_);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<void*>(0));
    gl.EnableVertexAttribArray(1);
    gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<void*>(3 * sizeof(float)));
    gl.EnableVertexAttribArray(2);
    gl.VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<void*>(5 * sizeof(float)));

    return true;
}

void StereoAvatarRenderer::BuildMeshes() {
    auto add_tri = [](std::vector<Vertex3D>& v,
                      EarthMath::Vec3f p0, EarthMath::Vec3f p1, EarthMath::Vec3f p2,
                      float r, float g, float b, float a) {
        EarthMath::Vec3d u{p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
        EarthMath::Vec3d w{p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
        EarthMath::Vec3f n = u.Cross(w).Normalized().ToFloat();
        v.push_back({p0.x, p0.y, p0.z, n.x, n.y, n.z, r, g, b, a});
        v.push_back({p1.x, p1.y, p1.z, n.x, n.y, n.z, r, g, b, a});
        v.push_back({p2.x, p2.y, p2.z, n.x, n.y, n.z, r, g, b, a});
    };

    auto add_box = [&](std::vector<Vertex3D>& v,
                       float cx, float cy, float cz,
                       float hx, float hy, float hz,
                       float r, float g, float b, float a) {
        EarthMath::Vec3f p[8] = {
            {cx - hx, cy - hy, cz - hz},
            {cx + hx, cy - hy, cz - hz},
            {cx + hx, cy + hy, cz - hz},
            {cx - hx, cy + hy, cz - hz},
            {cx - hx, cy - hy, cz + hz},
            {cx + hx, cy - hy, cz + hz},
            {cx + hx, cy + hy, cz + hz},
            {cx - hx, cy + hy, cz + hz},
        };
        const int faces[6][4] = {
            {0, 1, 2, 3}, // Front (-Z)
            {5, 4, 7, 6}, // Back (+Z)
            {4, 0, 3, 7}, // Left (-X)
            {1, 5, 6, 2}, // Right (+X)
            {3, 2, 6, 7}, // Top (+Y)
            {4, 5, 1, 0}, // Bottom (-Y)
        };
        for (int f = 0; f < 6; ++f) {
            add_tri(v, p[faces[f][0]], p[faces[f][1]], p[faces[f][2]], r, g, b, a);
            add_tri(v, p[faces[f][0]], p[faces[f][2]], p[faces[f][3]], r, g, b, a);
        }
    };

    // 1. Build 3D VR Headset (HMD) + Stylized Head + Gaze Indicator
    std::vector<Vertex3D> hmd_verts;
    add_box(hmd_verts, 0.0f,  0.00f,  0.03f, 0.085f, 0.10f, 0.09f, 0.22f, 0.24f, 0.28f, 1.0f);
    add_box(hmd_verts, 0.0f,  0.01f, -0.06f, 0.105f, 0.055f, 0.05f, 0.12f, 0.13f, 0.16f, 1.0f);
    add_box(hmd_verts, 0.0f,  0.01f, -0.112f, 0.095f, 0.042f, 0.006f, 0.9f, 0.95f, 1.0f, 0.2f);
    add_box(hmd_verts, -0.092f, 0.01f, 0.02f, 0.01f, 0.02f, 0.09f, 0.7f, 0.7f, 0.8f, 0.2f);
    add_box(hmd_verts,  0.092f, 0.01f, 0.02f, 0.01f, 0.02f, 0.09f, 0.7f, 0.7f, 0.8f, 0.2f);
    add_box(hmd_verts,  0.0f,  0.105f, 0.01f, 0.018f, 0.008f, 0.09f, 0.7f, 0.7f, 0.8f, 0.2f);
    add_box(hmd_verts,  0.0f,  0.01f, -0.18f, 0.015f, 0.015f, 0.06f, 1.0f, 1.0f, 1.0f, 0.2f);

    hmd_vertex_count_ = static_cast<GLsizei>(hmd_verts.size());
    gl.GenVertexArrays(1, &hmd_vao_);
    gl.GenBuffers(1, &hmd_vbo_);
    gl.BindVertexArray(hmd_vao_);
    gl.BindBuffer(GL_ARRAY_BUFFER_EXT, hmd_vbo_);
    gl.BufferData(GL_ARRAY_BUFFER_EXT, hmd_verts.size() * sizeof(Vertex3D), hmd_verts.data(), GL_STATIC_DRAW_EXT);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(0));
    gl.EnableVertexAttribArray(1);
    gl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(3 * sizeof(float)));
    gl.EnableVertexAttribArray(2);
    gl.VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(6 * sizeof(float)));

    // 2. Build 6DoF VR Controller + Tracking Ring + Laser Pointer Beam
    std::vector<Vertex3D> ctrl_verts;
    add_box(ctrl_verts, 0.0f, -0.01f,  0.05f, 0.020f, 0.018f, 0.075f, 0.18f, 0.20f, 0.24f, 1.0f);
    add_box(ctrl_verts, 0.0f,  0.015f, -0.035f, 0.045f, 0.012f, 0.030f, 0.28f, 0.30f, 0.35f, 1.0f);
    add_box(ctrl_verts, 0.0f,  0.012f,  0.01f, 0.015f, 0.006f, 0.015f, 0.9f, 0.95f, 1.0f, 0.2f);
    add_box(ctrl_verts, 0.0f,  0.00f, -0.14f, 0.0035f, 0.0035f, 0.075f, 1.0f, 1.0f, 1.0f, 0.2f);

    ctrl_vertex_count_ = static_cast<GLsizei>(ctrl_verts.size());
    gl.GenVertexArrays(1, &ctrl_vao_);
    gl.GenBuffers(1, &ctrl_vbo_);
    gl.BindVertexArray(ctrl_vao_);
    gl.BindBuffer(GL_ARRAY_BUFFER_EXT, ctrl_vbo_);
    gl.BufferData(GL_ARRAY_BUFFER_EXT, ctrl_verts.size() * sizeof(Vertex3D), ctrl_verts.data(), GL_STATIC_DRAW_EXT);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(0));
    gl.EnableVertexAttribArray(1);
    gl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(3 * sizeof(float)));
    gl.EnableVertexAttribArray(2);
    gl.VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(6 * sizeof(float)));

    // 3. Build Voice Activity Speaking Halo Ring around HMD
    std::vector<Vertex3D> halo_verts;
    constexpr int kSegments = 20;
    for (int i = 0; i < kSegments; ++i) {
        float a0 = static_cast<float>(i) * (2.0f * 3.14159265f / kSegments);
        float a1 = static_cast<float>(i + 1) * (2.0f * 3.14159265f / kSegments);
        float r_in = 0.16f, r_out = 0.19f;
        EarthMath::Vec3f p0{std::cos(a0) * r_in,  0.14f, std::sin(a0) * r_in};
        EarthMath::Vec3f p1{std::cos(a0) * r_out, 0.14f, std::sin(a0) * r_out};
        EarthMath::Vec3f p2{std::cos(a1) * r_out, 0.14f, std::sin(a1) * r_out};
        EarthMath::Vec3f p3{std::cos(a1) * r_in,  0.14f, std::sin(a1) * r_in};
        add_tri(halo_verts, p0, p1, p2, 0.25f, 1.0f, 0.45f, 1.0f);
        add_tri(halo_verts, p0, p2, p3, 0.25f, 1.0f, 0.45f, 1.0f);
        add_tri(halo_verts, p0, p2, p1, 0.25f, 1.0f, 0.45f, 1.0f);
        add_tri(halo_verts, p0, p3, p2, 0.25f, 1.0f, 0.45f, 1.0f);
    }
    halo_vertex_count_ = static_cast<GLsizei>(halo_verts.size());
    gl.GenVertexArrays(1, &halo_vao_);
    gl.GenBuffers(1, &halo_vbo_);
    gl.BindVertexArray(halo_vao_);
    gl.BindBuffer(GL_ARRAY_BUFFER_EXT, halo_vbo_);
    gl.BufferData(GL_ARRAY_BUFFER_EXT, halo_verts.size() * sizeof(Vertex3D), halo_verts.data(), GL_STATIC_DRAW_EXT);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(0));
    gl.EnableVertexAttribArray(1);
    gl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(3 * sizeof(float)));
    gl.EnableVertexAttribArray(2);
    gl.VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(6 * sizeof(float)));
}

void StereoAvatarRenderer::BuildFontTexture() {
    constexpr int kTexW = 512;
    constexpr int kTexH = 16;
    std::vector<uint8_t> pixels(kTexW * kTexH, 0);

    for (int g = 0; g < 64; ++g) {
        int base_x = g * 8 + 1;
        for (int col = 0; col < 5; ++col) {
            uint8_t bits = kFont5x7[g][col];
            for (int row = 0; row < 7; ++row) {
                if ((bits >> row) & 1) {
                    pixels[(row + 1) * kTexW + (base_x + col)] = 255;
                }
            }
        }
    }
    for (int y = 12; y < 16; ++y) {
        for (int x = 0; x < 8; ++x) {
            pixels[y * kTexW + x] = 255;
        }
    }

    gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &font_tex_);
    glBindTexture(GL_TEXTURE_2D, font_tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE_EXT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE_EXT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, kTexW, kTexH, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
}

void StereoAvatarRenderer::AppendBillboardQuad(
    std::vector<Vertex2D>& verts,
    const EarthMath::Vec3f& center,
    const EarthMath::Vec3f& right,
    const EarthMath::Vec3f& up,
    float half_w, float half_h,
    float u0, float v0, float u1, float v1,
    float r, float g, float b, float a) {

    EarthMath::Vec3f p0{
        center.x - right.x * half_w - up.x * half_h,
        center.y - right.y * half_w - up.y * half_h,
        center.z - right.z * half_w - up.z * half_h
    };
    EarthMath::Vec3f p1{
        center.x + right.x * half_w - up.x * half_h,
        center.y + right.y * half_w - up.y * half_h,
        center.z + right.z * half_w - up.z * half_h
    };
    EarthMath::Vec3f p2{
        center.x + right.x * half_w + up.x * half_h,
        center.y + right.y * half_w + up.y * half_h,
        center.z + right.z * half_w + up.z * half_h
    };
    EarthMath::Vec3f p3{
        center.x - right.x * half_w + up.x * half_h,
        center.y - right.y * half_w + up.y * half_h,
        center.z - right.z * half_w + up.z * half_h
    };

    verts.push_back({p0.x, p0.y, p0.z, u0, v1, r, g, b, a});
    verts.push_back({p1.x, p1.y, p1.z, u1, v1, r, g, b, a});
    verts.push_back({p2.x, p2.y, p2.z, u1, v0, r, g, b, a});

    verts.push_back({p0.x, p0.y, p0.z, u0, v1, r, g, b, a});
    verts.push_back({p2.x, p2.y, p2.z, u1, v0, r, g, b, a});
    verts.push_back({p3.x, p3.y, p3.z, u0, v0, r, g, b, a});
}

void StereoAvatarRenderer::AppendBillboardText(
    std::vector<Vertex2D>& verts,
    const std::string& text,
    const EarthMath::Vec3f& center,
    const EarthMath::Vec3f& right,
    const EarthMath::Vec3f& up,
    float char_height,
    float r, float g, float b, float a) {

    if (text.empty()) return;
    float char_width = char_height * 0.72f;
    float total_w = static_cast<float>(text.size()) * char_width;
    float start_x = -total_w * 0.5f + char_width * 0.5f;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        if (c < 32 || c > 95) c = '?';
        int idx = c - 32;

        float u0 = (static_cast<float>(idx * 8 + 1)) / 512.0f;
        float u1 = (static_cast<float>(idx * 8 + 6)) / 512.0f;
        float v0 = 1.0f / 16.0f;
        float v1 = 8.0f / 16.0f;

        float offset_x = start_x + static_cast<float>(i) * char_width;
        EarthMath::Vec3f glyph_center{
            center.x + right.x * offset_x,
            center.y + right.y * offset_x,
            center.z + right.z * offset_x
        };
        AppendBillboardQuad(verts, glyph_center, right, up,
                            char_width * 0.45f, char_height * 0.5f,
                            u0, v0, u1, v1, r, g, b, a);
    }
}

void StereoAvatarRenderer::AnchorMenuInFrontOfHMD(const EarthMath::Vec3d& hmd_pos, const EarthMath::Quatd& hmd_rot) {
    // OpenVR HMD local forward is -Z
    EarthMath::Vec3d forward = hmd_rot.Rotate(EarthMath::Vec3d(0.0, 0.0, -1.0));
    // Flatten forward slightly onto horizontal plane so panel stays upright and comfortable
    EarthMath::Vec3d flat_fwd(forward.x, forward.y * 0.25, forward.z);
    if (flat_fwd.LengthSq() < 1e-4) {
        flat_fwd = EarthMath::Vec3d(0.0, 0.0, -1.0);
    } else {
        flat_fwd = flat_fwd.Normalized();
    }

    panel_center_ = hmd_pos + flat_fwd * 0.92 + EarthMath::Vec3d(0.0, -0.05, 0.0);

    // Normal points from panel back toward the user's HMD
    EarthMath::Vec3d n = (hmd_pos - panel_center_).Normalized();
    EarthMath::Vec3d world_up(0.0, 1.0, 0.0);
    EarthMath::Vec3d r = world_up.Cross(n);
    if (r.LengthSq() < 1e-4) {
        r = EarthMath::Vec3d(1.0, 0.0, 0.0);
    } else {
        r = r.Normalized();
    }
    EarthMath::Vec3d u = n.Cross(r).Normalized();

    panel_right_  = r.ToFloat();
    panel_up_     = u.ToFloat();
    panel_normal_ = n.ToFloat();
}

void StereoAvatarRenderer::ToggleStandaloneMenu(const EarthMath::Vec3d& hmd_pos, const EarthMath::Quatd& hmd_rot) {
    standalone_menu_pinned_ = !standalone_menu_pinned_;
    if (standalone_menu_pinned_) {
        tab_expanded_ = true;
        AnchorMenuInFrontOfHMD(hmd_pos, hmd_rot);
    }
    menu_visible_ = standalone_menu_pinned_ || prev_native_menu_open_;
}

void StereoAvatarRenderer::OnCharInput(char c) {
    if (pw_modal_mode_ == PasswordModalMode::None) return;
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_') {
        if (pw_input_buffer_.size() < 12) {
            pw_input_buffer_.push_back(c);
        }
    }
}

void StereoAvatarRenderer::OnBackspaceInput() {
    if (pw_modal_mode_ == PasswordModalMode::None) return;
    if (!pw_input_buffer_.empty()) {
        pw_input_buffer_.pop_back();
    }
}

void StereoAvatarRenderer::BuildMenuButtons(
    EarthNet::NetworkManager& network,
    EarthVoice::GlobalVoiceManager& voice,
    const std::vector<EarthNet::RemotePlayerState>& remote_players) {

    active_buttons_.clear();
    if (!menu_visible_) return;

    // Top Options Tab Header Bar (always visible when menu is shown)
    active_buttons_.push_back({
        BTN_TAB_TOGGLE, -0.20f, 0.255f, 0.165f, 0.022f,
        tab_expanded_ ? "MULTIPLAYER [OPEN]" : "MULTIPLAYER [TAB]",
        0.10f, 0.42f, 0.72f, 0.96f,
        1.0f, 1.0f, 1.0f, 0, false
    });

    active_buttons_.push_back({
        BTN_RECENTER, 0.105f, 0.255f, 0.105f, 0.022f,
        "RECENTER",
        0.16f, 0.22f, 0.32f, 0.94f,
        0.85f, 0.95f, 1.0f, 0, false
    });

    active_buttons_.push_back({
        BTN_CLOSE_PANEL, 0.295f, 0.255f, 0.068f, 0.022f,
        "HIDE",
        0.38f, 0.14f, 0.18f, 0.94f,
        1.0f, 0.85f, 0.85f, 0, false
    });

    if (!tab_expanded_) {
        return;
    }

    // If Password Entry Keypad Modal is open:
    if (pw_modal_mode_ != PasswordModalMode::None) {
        // 4x4 Alphanumeric/PIN keypad on the left/center
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                int idx = row * 4 + col;
                float bx = -0.28f + col * 0.11f;
                float by =  0.04f - row * 0.062f;
                char label[4] = { kKeypadChars[idx], '\0', '\0', '\0' };
                active_buttons_.push_back({
                    BTN_PW_KEY_BASE + idx, bx, by, 0.048f, 0.025f,
                    label,
                    0.15f, 0.21f, 0.32f, 0.95f,
                    1.0f, 1.0f, 1.0f, 0, false
                });
            }
        }

        // Action buttons on the right side of the keypad
        active_buttons_.push_back({
            BTN_PW_BACKSPACE, 0.22f, 0.04f, 0.125f, 0.025f,
            "BACKSPACE",
            0.28f, 0.22f, 0.15f, 0.95f,
            1.0f, 0.90f, 0.70f, 0, false
        });
        active_buttons_.push_back({
            BTN_PW_CLEAR, 0.22f, -0.022f, 0.125f, 0.025f,
            "CLEAR",
            0.28f, 0.18f, 0.18f, 0.95f,
            1.0f, 0.85f, 0.85f, 0, false
        });
        active_buttons_.push_back({
            BTN_PW_CONFIRM, 0.22f, -0.084f, 0.125f, 0.026f,
            (pw_modal_mode_ == PasswordModalMode::HostWithPassword) ? "START SERVER" : "JOIN SESSION",
            0.12f, 0.55f, 0.30f, 0.96f,
            1.0f, 1.0f, 1.0f, 0, false
        });
        active_buttons_.push_back({
            BTN_PW_CANCEL, 0.22f, -0.146f, 0.125f, 0.025f,
            "CANCEL",
            0.52f, 0.14f, 0.16f, 0.95f,
            1.0f, 0.9f, 0.9f, 0, false
        });
        return;
    }

    // Normal Multiplayer Tab View
    EarthNet::SessionState state = network.GetSessionState();
    auto banned_list = network.GetBannedPlayers();

    if (state == EarthNet::SessionState::Offline) {
        // Row 1: Start Open Server | Start Password Server | Find Public Sessions
        active_buttons_.push_back({
            BTN_START_SERVER_OPEN, -0.245f, 0.135f, 0.125f, 0.025f,
            "START SERVER",
            0.12f, 0.52f, 0.28f, 0.95f,
            1.0f, 1.0f, 1.0f, 0, false
        });
        active_buttons_.push_back({
            BTN_START_SERVER_PW, 0.0f, 0.135f, 0.110f, 0.025f,
            "HOST W/ PASS",
            0.14f, 0.38f, 0.68f, 0.95f,
            1.0f, 1.0f, 1.0f, 0, false
        });
        active_buttons_.push_back({
            BTN_FIND_SESSIONS, 0.245f, 0.135f, 0.125f, 0.025f,
            show_ban_list_ ? "VIEW SESSIONS" : "FIND SESSIONS",
            0.12f, 0.46f, 0.58f, 0.95f,
            1.0f, 1.0f, 1.0f, 0, false
        });

        if (!banned_list.empty()) {
            char ban_lbl[32];
            std::snprintf(ban_lbl, sizeof(ban_lbl), show_ban_list_ ? "BACK" : "BANS (%zu)", banned_list.size());
            active_buttons_.push_back({
                BTN_TOGGLE_BAN_VIEW, 0.310f, 0.072f, 0.062f, 0.018f,
                ban_lbl,
                0.42f, 0.16f, 0.22f, 0.95f,
                1.0f, 0.90f, 0.90f, 0, false
            });
        } else {
            show_ban_list_ = false;
        }

        if (show_ban_list_) {
            size_t count = std::min<size_t>(banned_list.size(), 3);
            for (size_t i = 0; i < count; ++i) {
                float row_y = 0.020f - static_cast<float>(i) * 0.056f;
                active_buttons_.push_back({
                    BTN_UNBAN_PLAYER_BASE + static_cast<int>(i),
                    0.275f, row_y, 0.082f, 0.021f,
                    "UNBAN",
                    0.14f, 0.52f, 0.32f, 0.95f,
                    1.0f, 1.0f, 1.0f,
                    banned_list[i].peer_id,
                    false
                });
            }
        } else {
            // Public Sessions Browser list (up to 3 rows)
            auto sessions = network.GetPublicSessions();
            size_t count = std::min<size_t>(sessions.size(), 3);
            for (size_t i = 0; i < count; ++i) {
                float row_y = 0.020f - static_cast<float>(i) * 0.056f;
                active_buttons_.push_back({
                    BTN_JOIN_SESSION_BASE + static_cast<int>(i),
                    0.275f, row_y, 0.082f, 0.021f,
                    sessions[i].has_password ? "JOIN [PW]" : "JOIN",
                    0.14f, 0.50f, 0.36f, 0.95f,
                    1.0f, 1.0f, 1.0f,
                    sessions[i].lobby_id,
                    sessions[i].has_password
                });
            }
        }
    } else if (state == EarthNet::SessionState::Hosting) {
        // Hosting controls: Close Server | Lock/Unlock Session | Banned List View Toggle
        active_buttons_.push_back({
            BTN_CLOSE_SERVER, -0.245f, 0.135f, 0.120f, 0.025f,
            "CLOSE SERVER",
            0.58f, 0.15f, 0.18f, 0.95f,
            1.0f, 1.0f, 1.0f, 0, false
        });

        bool locked = network.IsSessionLocked();
        active_buttons_.push_back({
            BTN_TOGGLE_LOCK, 0.005f, 0.135f, 0.118f, 0.025f,
            locked ? "LOCKED: YES" : "LOCKED: NO",
            locked ? 0.62f : 0.14f,
            locked ? 0.42f : 0.46f,
            locked ? 0.10f : 0.32f,
            0.95f,
            1.0f, 1.0f, 1.0f, 0, false
        });

        char ban_btn_txt[32];
        if (show_ban_list_) {
            std::snprintf(ban_btn_txt, sizeof(ban_btn_txt), "PLAYERS (%zu)", remote_players.size());
        } else {
            std::snprintf(ban_btn_txt, sizeof(ban_btn_txt), "BANS (%zu)", banned_list.size());
        }
        active_buttons_.push_back({
            BTN_TOGGLE_BAN_VIEW, 0.250f, 0.135f, 0.115f, 0.025f,
            ban_btn_txt,
            show_ban_list_ ? 0.16f : 0.38f,
            show_ban_list_ ? 0.42f : 0.18f,
            show_ban_list_ ? 0.62f : 0.28f,
            0.95f,
            1.0f, 1.0f, 1.0f, 0, false
        });

        if (show_ban_list_) {
            // Banned Players list with [UNBAN] buttons (up to 3 rows)
            size_t count = std::min<size_t>(banned_list.size(), 3);
            for (size_t i = 0; i < count; ++i) {
                float row_y = 0.020f - static_cast<float>(i) * 0.056f;
                active_buttons_.push_back({
                    BTN_UNBAN_PLAYER_BASE + static_cast<int>(i),
                    0.275f, row_y, 0.082f, 0.021f,
                    "UNBAN",
                    0.14f, 0.52f, 0.32f, 0.95f,
                    1.0f, 1.0f, 1.0f,
                    banned_list[i].peer_id,
                    false
                });
            }
        } else {
            // Connected Players list with separate [KICK] and [BAN] buttons (up to 3 rows)
            size_t count = std::min<size_t>(remote_players.size(), 3);
            for (size_t i = 0; i < count; ++i) {
                float row_y = 0.020f - static_cast<float>(i) * 0.056f;
                active_buttons_.push_back({
                    BTN_KICK_PLAYER_BASE + static_cast<int>(i),
                    0.185f, row_y, 0.055f, 0.021f,
                    "KICK",
                    0.62f, 0.38f, 0.12f, 0.95f,
                    1.0f, 0.95f, 0.85f,
                    remote_players[i].peer_id,
                    false
                });
                active_buttons_.push_back({
                    BTN_BAN_PLAYER_BASE + static_cast<int>(i),
                    0.308f, row_y, 0.055f, 0.021f,
                    "BAN",
                    0.62f, 0.14f, 0.18f, 0.95f,
                    1.0f, 0.90f, 0.90f,
                    remote_players[i].peer_id,
                    false
                });
            }
        }
    } else {
        // Joining or InSession (client in someone else's session)
        show_ban_list_ = false;
        active_buttons_.push_back({
            BTN_LEAVE_SESSION, 0.0f, 0.135f, 0.185f, 0.025f,
            "LEAVE SESSION",
            0.58f, 0.15f, 0.18f, 0.95f,
            1.0f, 1.0f, 1.0f, 0, false
        });
    }

    // Bottom Quick-Toggles Row (Visuals & Microphone)
    bool v_3d = show_3d_avatars_.load();
    bool v_xr = show_xray_through_earth_.load();
    bool v_mk = show_floating_markers_.load();
    bool v_mc = !voice.IsMicMuted();

    active_buttons_.push_back({
        BTN_TOGGLE_3D_AVATARS, -0.282f, -0.235f, 0.088f, 0.021f,
        v_3d ? "AVATARS:ON" : "AVATARS:OFF",
        v_3d ? 0.14f : 0.22f, v_3d ? 0.42f : 0.22f, v_3d ? 0.30f : 0.26f, 0.94f,
        1.0f, 1.0f, 1.0f, 0, false
    });
    active_buttons_.push_back({
        BTN_TOGGLE_XRAY, -0.094f, -0.235f, 0.088f, 0.021f,
        v_xr ? "X-RAY:ON" : "X-RAY:OFF",
        v_xr ? 0.14f : 0.22f, v_xr ? 0.42f : 0.22f, v_xr ? 0.30f : 0.26f, 0.94f,
        1.0f, 1.0f, 1.0f, 0, false
    });
    active_buttons_.push_back({
        BTN_TOGGLE_MARKERS, 0.094f, -0.235f, 0.088f, 0.021f,
        v_mk ? "NAMETAG:ON" : "NAMETAG:OFF",
        v_mk ? 0.14f : 0.22f, v_mk ? 0.42f : 0.22f, v_mk ? 0.30f : 0.26f, 0.94f,
        1.0f, 1.0f, 1.0f, 0, false
    });
    active_buttons_.push_back({
        BTN_TOGGLE_MIC, 0.282f, -0.235f, 0.088f, 0.021f,
        v_mc ? "MIC:LIVE" : "MIC:MUTED",
        v_mc ? 0.14f : 0.48f, v_mc ? 0.42f : 0.16f, v_mc ? 0.30f : 0.18f, 0.94f,
        1.0f, 1.0f, 1.0f, 0, false
    });
}

void StereoAvatarRenderer::ExecuteButtonAction(
    const UIButton& btn,
    EarthNet::NetworkManager& network,
    EarthVoice::GlobalVoiceManager& voice,
    const EarthMath::Vec3d& hmd_pos,
    const EarthMath::Quatd& hmd_rot) {

    switch (btn.id) {
        case BTN_TAB_TOGGLE:
            tab_expanded_ = !tab_expanded_;
            break;
        case BTN_RECENTER:
            AnchorMenuInFrontOfHMD(hmd_pos, hmd_rot);
            break;
        case BTN_CLOSE_PANEL:
            if (standalone_menu_pinned_) {
                standalone_menu_pinned_ = false;
            } else {
                tab_expanded_ = false;
            }
            break;
        case BTN_START_SERVER_OPEN:
            pw_modal_mode_ = PasswordModalMode::None;
            network.StartServer("");
            break;
        case BTN_START_SERVER_PW:
            pw_input_buffer_.clear();
            pw_modal_mode_ = PasswordModalMode::HostWithPassword;
            break;
        case BTN_FIND_SESSIONS:
            show_ban_list_ = false;
            network.RefreshPublicSessions();
            break;
        case BTN_TOGGLE_BAN_VIEW:
            show_ban_list_ = !show_ban_list_;
            break;
        case BTN_CLOSE_SERVER:
        case BTN_LEAVE_SESSION:
            network.CloseServerOrLeave();
            break;
        case BTN_TOGGLE_LOCK:
            network.SetSessionLocked(!network.IsSessionLocked());
            break;
        case BTN_TOGGLE_3D_AVATARS:
            ToggleShow3DAvatars();
            break;
        case BTN_TOGGLE_XRAY:
            ToggleShowXRayThroughEarth();
            break;
        case BTN_TOGGLE_MARKERS:
            ToggleShowFloatingMarkers();
            break;
        case BTN_TOGGLE_MIC:
            voice.ToggleMicMuted();
            break;
        case BTN_PW_BACKSPACE:
            if (!pw_input_buffer_.empty()) pw_input_buffer_.pop_back();
            break;
        case BTN_PW_CLEAR:
            pw_input_buffer_.clear();
            break;
        case BTN_PW_CONFIRM: {
            PasswordModalMode mode = pw_modal_mode_;
            pw_modal_mode_ = PasswordModalMode::None;
            if (mode == PasswordModalMode::HostWithPassword) {
                network.StartServer(pw_input_buffer_);
            } else if (mode == PasswordModalMode::JoinWithPassword && pending_join_lobby_id_ != 0) {
                network.JoinPublicSession(pending_join_lobby_id_, pw_input_buffer_);
            }
            pw_input_buffer_.clear();
            break;
        }
        case BTN_PW_CANCEL:
            pw_modal_mode_ = PasswordModalMode::None;
            pw_input_buffer_.clear();
            break;
        default:
            if (btn.id >= BTN_PW_KEY_BASE && btn.id < BTN_PW_KEY_BASE + 16) {
                if (pw_input_buffer_.size() < 12) {
                    pw_input_buffer_.push_back(kKeypadChars[btn.id - BTN_PW_KEY_BASE]);
                }
            } else if (btn.id >= BTN_KICK_PLAYER_BASE && btn.id < BTN_KICK_PLAYER_BASE + 16) {
                if (btn.target_id != 0) {
                    network.KickPlayer(btn.target_id);
                }
            } else if (btn.id >= BTN_BAN_PLAYER_BASE && btn.id < BTN_BAN_PLAYER_BASE + 16) {
                if (btn.target_id != 0) {
                    network.BanPlayer(btn.target_id);
                }
            } else if (btn.id >= BTN_UNBAN_PLAYER_BASE && btn.id < BTN_UNBAN_PLAYER_BASE + 16) {
                if (btn.target_id != 0) {
                    network.UnbanPlayer(btn.target_id);
                }
            } else if (btn.id >= BTN_JOIN_SESSION_BASE && btn.id < BTN_JOIN_SESSION_BASE + 16) {
                if (btn.target_id != 0) {
                    if (btn.requires_pw) {
                        pending_join_lobby_id_ = btn.target_id;
                        pw_input_buffer_.clear();
                        pw_modal_mode_ = PasswordModalMode::JoinWithPassword;
                    } else {
                        network.JoinPublicSession(btn.target_id, "");
                    }
                }
            }
            break;
    }
}

void StereoAvatarRenderer::UpdateMenuInteraction(
    const VRMenuInputState& input,
    EarthNet::NetworkManager& network,
    EarthVoice::GlobalVoiceManager& voice) {

    // Detect when the in-game Google Earth VR menu opens
    if (input.native_menu_open && !prev_native_menu_open_) {
        tab_expanded_ = true;
        // Position the Multiplayer Options Panel comfortably in front of the user or docked alongside the native menu
        double native_dist = (input.native_menu_pos - input.hmd_pos).Length();
        if (std::isfinite(native_dist) && native_dist > 0.25 && native_dist < 4.5) {
            EarthMath::Vec3d to_hmd = (input.hmd_pos - input.native_menu_pos).Normalized();
            EarthMath::Vec3d world_up(0.0, 1.0, 0.0);
            EarthMath::Vec3d right = world_up.Cross(to_hmd);
            if (right.LengthSq() < 1e-4) right = EarthMath::Vec3d(1.0, 0.0, 0.0);
            else right = right.Normalized();

            // Place docked slightly to the right of the native menu, facing the player
            panel_center_ = input.native_menu_pos + right * 0.45 + EarthMath::Vec3d(0.0, 0.04, 0.0);
            EarthMath::Vec3d n = (input.hmd_pos - panel_center_).Normalized();
            EarthMath::Vec3d r = world_up.Cross(n).Normalized();
            EarthMath::Vec3d u = n.Cross(r).Normalized();
            panel_right_  = r.ToFloat();
            panel_up_     = u.ToFloat();
            panel_normal_ = n.ToFloat();
        } else {
            AnchorMenuInFrontOfHMD(input.hmd_pos, input.hmd_rot);
        }
    }
    prev_native_menu_open_ = input.native_menu_open;
    menu_visible_ = input.native_menu_open || standalone_menu_pinned_;

    if (!menu_visible_) {
        has_laser_hit_ = false;
        hovered_button_id_ = -1;
        for (int c = 0; c < 2; ++c) prev_trigger_down_[c] = input.ctrl_trigger_down[c];
        return;
    }

    auto remote_players = network.GetActiveRemotePlayers();
    BuildMenuButtons(network, voice, remote_players);

    // Raycast both 6DoF VR controllers (and HMD gaze fallback if no controllers are active) against the 3D panel plane
    has_laser_hit_ = false;
    laser_ctrl_idx_ = -1;
    hovered_button_id_ = -1;

    EarthMath::Vec3d N(panel_normal_.x, panel_normal_.y, panel_normal_.z);
    EarthMath::Vec3d R(panel_right_.x,  panel_right_.y,  panel_right_.z);
    EarthMath::Vec3d U(panel_up_.x,     panel_up_.y,     panel_up_.z);

    auto test_ray = [&](const EarthMath::Vec3d& ray_origin, const EarthMath::Quatd& ray_rot, int ctrl_idx) -> bool {
        EarthMath::Vec3d ray_dir = ray_rot.Rotate(EarthMath::Vec3d(0.0, 0.0, -1.0)).Normalized();
        double denom = ray_dir.Dot(N);
        if (denom >= -0.04) return false; // Must point toward front face of panel
        double t = (panel_center_ - ray_origin).Dot(N) / denom;
        if (t < 0.03 || t > 6.0) return false;

        EarthMath::Vec3d hit = ray_origin + ray_dir * t;
        EarthMath::Vec3d rel = hit - panel_center_;
        float lx = static_cast<float>(rel.Dot(R));
        float ly = static_cast<float>(rel.Dot(U));

        if (std::fabs(lx) <= 0.44f && std::fabs(ly) <= 0.33f) {
            has_laser_hit_ = true;
            laser_ctrl_idx_ = ctrl_idx;
            laser_local_x_ = lx;
            laser_local_y_ = ly;
            laser_start_pos_ = ray_origin;
            laser_hit_pos_ = hit;
            return true;
        }
        return false;
    };

    // Prefer whichever controller is actively pulling trigger, otherwise Right (1) then Left (0), then HMD gaze
    int order[2] = {1, 0};
    if (input.ctrl_trigger_down[0] && !input.ctrl_trigger_down[1]) {
        order[0] = 0;
        order[1] = 1;
    }
    for (int i = 0; i < 2; ++i) {
        int c = order[i];
        if (input.ctrl_active[c]) {
            if (test_ray(input.ctrl_pos[c], input.ctrl_rot[c], c)) {
                break;
            }
        }
    }
    if (!has_laser_hit_) {
        // Fallback to HMD center gaze ray so desktop/keyboard users can also point at buttons with their view
        test_ray(input.hmd_pos, input.hmd_rot, -1);
    }

    if (has_laser_hit_) {
        for (const auto& btn : active_buttons_) {
            if (std::fabs(laser_local_x_ - btn.cx) <= btn.half_w &&
                std::fabs(laser_local_y_ - btn.cy) <= btn.half_h) {
                hovered_button_id_ = btn.id;
                break;
            }
        }
    }

    // Check if trigger was just pressed on either controller or desktop click
    bool clicked = input.desktop_click_pressed;
    for (int c = 0; c < 2; ++c) {
        if (input.ctrl_trigger_down[c] && !prev_trigger_down_[c]) {
            clicked = true;
        }
        prev_trigger_down_[c] = input.ctrl_trigger_down[c];
    }

    if (clicked && hovered_button_id_ != -1) {
        for (const auto& btn : active_buttons_) {
            if (btn.id == hovered_button_id_) {
                ExecuteButtonAction(btn, network, voice, input.hmd_pos, input.hmd_rot);
                break;
            }
        }
    }
}

void StereoAvatarRenderer::RenderMultiplayerMenuForEye(
    std::vector<Vertex2D>& ui_verts,
    const EarthMath::Vec3d& eye_pos_start,
    EarthNet::NetworkManager& network,
    EarthVoice::GlobalVoiceManager& /*voice*/,
    const std::vector<EarthNet::RemotePlayerState>& remote_players) {

    if (!menu_visible_) return;

    constexpr float kSolidU0 = 1.0f / 512.0f;
    constexpr float kSolidV0 = 13.0f / 16.0f;
    constexpr float kSolidU1 = 6.0f / 512.0f;
    constexpr float kSolidV1 = 15.0f / 16.0f;

    EarthMath::Vec3f base_eye = (panel_center_ - eye_pos_start).ToFloat();

    // Helper to convert (local_x, local_y, z_layer_offset) on the 3D panel into eye-relative 3D coordinates
    auto to_eye = [&](float lx, float ly, float z_off) -> EarthMath::Vec3f {
        return EarthMath::Vec3f{
            base_eye.x + panel_right_.x * lx + panel_up_.x * ly + panel_normal_.x * z_off,
            base_eye.y + panel_right_.y * lx + panel_up_.y * ly + panel_normal_.y * z_off,
            base_eye.z + panel_right_.z * lx + panel_up_.z * ly + panel_normal_.z * z_off
        };
    };

    if (tab_expanded_) {
        // Outer glowing cyan/blue frame + dark glass panel background
        AppendBillboardQuad(ui_verts, to_eye(0.0f, 0.0f, 0.0f), panel_right_, panel_up_,
                            0.395f, 0.295f, kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                            0.18f, 0.55f, 0.92f, 0.92f);
        AppendBillboardQuad(ui_verts, to_eye(0.0f, 0.0f, 0.001f), panel_right_, panel_up_,
                            0.388f, 0.288f, kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                            0.05f, 0.07f, 0.12f, 0.92f);

        // Status banner strip
        AppendBillboardQuad(ui_verts, to_eye(0.0f, 0.198f, 0.002f), panel_right_, panel_up_,
                            0.370f, 0.022f, kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                            0.09f, 0.13f, 0.21f, 0.95f);

        std::string status_str = network.GetStatusMessage();
        if (status_str.size() > 48) status_str.resize(48);
        AppendBillboardText(ui_verts, status_str, to_eye(0.0f, 0.198f, 0.0035f),
                            panel_right_, panel_up_, 0.0135f,
                            0.75f, 0.92f, 1.0f, 1.0f);

        if (pw_modal_mode_ != PasswordModalMode::None) {
            // Password Keypad Modal Header & Input Field
            std::string title = (pw_modal_mode_ == PasswordModalMode::HostWithPassword)
                ? "SET SERVER PASSWORD (OR EMPTY FOR OPEN)"
                : "ENTER PASSWORD TO JOIN SESSION";
            AppendBillboardText(ui_verts, title, to_eye(0.0f, 0.145f, 0.0035f),
                                panel_right_, panel_up_, 0.014f,
                                1.0f, 0.92f, 0.60f, 1.0f);

            AppendBillboardQuad(ui_verts, to_eye(-0.04f, 0.100f, 0.002f), panel_right_, panel_up_,
                                0.26f, 0.022f, kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                                0.03f, 0.04f, 0.07f, 0.96f);

            std::string pw_disp = "PASSWORD: [ " + pw_input_buffer_ + "_ ]";
            AppendBillboardText(ui_verts, pw_disp, to_eye(-0.04f, 0.100f, 0.0035f),
                                panel_right_, panel_up_, 0.015f,
                                0.40f, 1.0f, 0.65f, 1.0f);
        } else {
            EarthNet::SessionState state = network.GetSessionState();
            if (show_ban_list_) {
                AppendBillboardText(ui_verts, "BANNED PLAYERS (CLICK UNBAN TO ALLOW REJOINING)",
                                    to_eye(-0.04f, 0.072f, 0.0035f),
                                    panel_right_, panel_up_, 0.0125f,
                                    1.0f, 0.72f, 0.72f, 0.96f);

                auto banned = network.GetBannedPlayers();
                if (banned.empty()) {
                    AppendBillboardText(ui_verts, "NO PLAYERS ARE CURRENTLY BANNED",
                                        to_eye(0.0f, -0.025f, 0.0035f),
                                        panel_right_, panel_up_, 0.0125f,
                                        0.70f, 0.76f, 0.85f, 0.90f);
                } else {
                    size_t count = std::min<size_t>(banned.size(), 3);
                    for (size_t i = 0; i < count; ++i) {
                        float row_y = 0.020f - static_cast<float>(i) * 0.056f;
                        AppendBillboardQuad(ui_verts, to_eye(-0.055f, row_y, 0.002f), panel_right_, panel_up_,
                                            0.235f, 0.021f, kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                                            0.18f, 0.09f, 0.12f, 0.92f);

                        std::string b_txt = banned[i].name + " [BANNED]";
                        AppendBillboardText(ui_verts, b_txt, to_eye(-0.055f, row_y, 0.0035f),
                                            panel_right_, panel_up_, 0.0125f,
                                            1.0f, 0.88f, 0.88f, 1.0f);
                    }
                }
            } else if (state == EarthNet::SessionState::Offline) {
                AppendBillboardText(ui_verts, "OPEN PUBLIC SESSIONS (STEAM LOBBY BROWSER)",
                                    to_eye(-0.04f, 0.072f, 0.0035f),
                                    panel_right_, panel_up_, 0.013f,
                                    0.65f, 0.82f, 1.0f, 0.95f);

                auto sessions = network.GetPublicSessions();
                if (sessions.empty()) {
                    AppendBillboardText(ui_verts, "NO SESSIONS LISTED - CLICK [FIND SESSIONS] OR [START SERVER]",
                                        to_eye(0.0f, -0.025f, 0.0035f),
                                        panel_right_, panel_up_, 0.0115f,
                                        0.65f, 0.70f, 0.80f, 0.90f);
                } else {
                    size_t count = std::min<size_t>(sessions.size(), 3);
                    for (size_t i = 0; i < count; ++i) {
                        float row_y = 0.020f - static_cast<float>(i) * 0.056f;
                        AppendBillboardQuad(ui_verts, to_eye(-0.055f, row_y, 0.002f), panel_right_, panel_up_,
                                            0.235f, 0.021f, kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                                            0.09f, 0.12f, 0.18f, 0.92f);

                        char row_txt[96];
                        std::snprintf(row_txt, sizeof(row_txt), "%s (%d/%d) %s",
                                      sessions[i].host_name.c_str(),
                                      sessions[i].member_count,
                                      sessions[i].max_members,
                                      sessions[i].has_password ? "[PW]" : "[OPEN]");
                        AppendBillboardText(ui_verts, row_txt, to_eye(-0.055f, row_y, 0.0035f),
                                            panel_right_, panel_up_, 0.0125f,
                                            0.95f, 0.98f, 1.0f, 1.0f);
                    }
                }
            } else {
                // Hosting or InSession: Show Connected Players list
                std::string hdr = (state == EarthNet::SessionState::Hosting)
                    ? "PLAYERS IN YOUR SERVER (KICK OR BAN)"
                    : "PLAYERS IN CURRENT SESSION";
                AppendBillboardText(ui_verts, hdr, to_eye(0.0f, 0.072f, 0.0035f),
                                    panel_right_, panel_up_, 0.013f,
                                    0.65f, 0.95f, 0.80f, 0.95f);

                if (remote_players.empty()) {
                    AppendBillboardText(ui_verts, "WAITING FOR OTHER PLAYERS TO JOIN...",
                                        to_eye(0.0f, -0.025f, 0.0035f),
                                        panel_right_, panel_up_, 0.0125f,
                                        0.70f, 0.76f, 0.85f, 0.90f);
                } else {
                    size_t count = std::min<size_t>(remote_players.size(), 3);
                    for (size_t i = 0; i < count; ++i) {
                        float row_y = 0.020f - static_cast<float>(i) * 0.056f;
                        AppendBillboardQuad(ui_verts, to_eye(-0.110f, row_y, 0.002f), panel_right_, panel_up_,
                                            0.220f, 0.021f, kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                                            0.09f, 0.13f, 0.19f, 0.92f);

                        std::string p_txt = remote_players[i].name;
                        if (remote_players[i].voice_level > 0.03f) {
                            p_txt += " [MIC]";
                        }
                        AppendBillboardText(ui_verts, p_txt, to_eye(-0.110f, row_y, 0.0035f),
                                            panel_right_, panel_up_, 0.0125f,
                                            0.95f, 1.0f, 0.95f, 1.0f);
                    }
                }
            }
        }
    }

    // Render all active interactive buttons
    for (const auto& btn : active_buttons_) {
        bool hovered = (btn.id == hovered_button_id_);
        if (hovered) {
            // Bright highlight border around hovered button
            AppendBillboardQuad(ui_verts, to_eye(btn.cx, btn.cy, 0.0025f), panel_right_, panel_up_,
                                btn.half_w + 0.004f, btn.half_h + 0.004f,
                                kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                                0.35f, 0.95f, 1.0f, 0.98f);
        }

        float r = hovered ? std::min(1.0f, btn.bg_r + 0.16f) : btn.bg_r;
        float g = hovered ? std::min(1.0f, btn.bg_g + 0.16f) : btn.bg_g;
        float b = hovered ? std::min(1.0f, btn.bg_b + 0.16f) : btn.bg_b;

        AppendBillboardQuad(ui_verts, to_eye(btn.cx, btn.cy, 0.003f), panel_right_, panel_up_,
                            btn.half_w, btn.half_h,
                            kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                            r, g, b, btn.bg_a);

        AppendBillboardText(ui_verts, btn.label, to_eye(btn.cx, btn.cy, 0.0045f),
                            panel_right_, panel_up_, 0.0128f,
                            btn.fg_r, btn.fg_g, btn.fg_b, 1.0f);
    }

    // Render 3D Laser Pointer Beam & Cursor Dot when pointing at the Multiplayer Panel
    if (has_laser_hit_) {
        // Draw glowing cursor dot on the panel surface
        EarthMath::Vec3f cursor_center = to_eye(laser_local_x_, laser_local_y_, 0.006f);
        AppendBillboardQuad(ui_verts, cursor_center, panel_right_, panel_up_,
                            0.009f, 0.009f, kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                            0.20f, 1.0f, 0.85f, 0.98f);
        AppendBillboardQuad(ui_verts, cursor_center, panel_right_, panel_up_,
                            0.0045f, 0.0045f, kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                            1.0f, 1.0f, 1.0f, 1.0f);

        // If ray came from a VR controller, draw a thin segmented laser line from controller to hit point
        if (laser_ctrl_idx_ >= 0) {
            constexpr int kBeamSteps = 12;
            for (int s = 0; s < kBeamSteps; ++s) {
                double t = (static_cast<double>(s) + 0.5) / static_cast<double>(kBeamSteps);
                EarthMath::Vec3d pt = laser_start_pos_ + (laser_hit_pos_ - laser_start_pos_) * t;
                EarthMath::Vec3f pt_eye = (pt - eye_pos_start).ToFloat();
                AppendBillboardQuad(ui_verts, pt_eye, panel_right_, panel_up_,
                                    0.0025f, 0.0025f, kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                                    0.25f, 0.95f, 1.0f, 0.75f);
            }
        }
    }
}

void StereoAvatarRenderer::RenderEyePass(
    int /*eye_idx*/,
    const EyeRenderParams& eye,
    const EarthMath::Sim3d& start_from_keyhole,
    const EarthMath::Vec3d& local_hmd_start_pos,
    const std::vector<EarthNet::RemotePlayerState>& remote_players,
    EarthNet::NetworkManager& network,
    EarthVoice::GlobalVoiceManager& voice) {

    int32_t vp_x = eye.viewport[0];
    int32_t vp_y = eye.viewport[1];
    int32_t vp_w = eye.viewport[2] - eye.viewport[0];
    int32_t vp_h = eye.viewport[3] - eye.viewport[1];
    if (vp_w <= 0 || vp_h <= 0) return;

    glViewport(vp_x, vp_y, vp_w, vp_h);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vp_x, vp_y, vp_w, vp_h);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_CULL_FACE);

    EarthMath::Vec3d eye_pos_start{
        eye.start_from_eye_row_major[3],
        eye.start_from_eye_row_major[7],
        eye.start_from_eye_row_major[11]
    };
    EarthMath::Vec3d eye_pos_kh = start_from_keyhole.TransformPointFromStartToKeyhole(eye_pos_start);

    float view_rot_only[16];
    std::memcpy(view_rot_only, eye.eye_from_start_row_major, sizeof(view_rot_only));
    view_rot_only[3]  = 0.0f;
    view_rot_only[7]  = 0.0f;
    view_rot_only[11] = 0.0f;

    float proj[16];
    std::memcpy(proj, eye.clip_from_eye_row_major, sizeof(proj));
    constexpr float z_near = 0.01f;
    constexpr float z_far  = 2000.0f;
    proj[10] = -(z_far + z_near) / (z_far - z_near);
    proj[11] = -(2.0f * z_far * z_near) / (z_far - z_near);

    float view_proj[16];
    EarthMath::Mat4fMultiplyRowMajor(view_proj, proj, view_rot_only);

    EarthMath::Vec3f cam_right{
        eye.start_from_eye_row_major[0],
        eye.start_from_eye_row_major[4],
        eye.start_from_eye_row_major[8]
    };
    EarthMath::Vec3f cam_up{
        eye.start_from_eye_row_major[1],
        eye.start_from_eye_row_major[5],
        eye.start_from_eye_row_major[9]
    };

    const bool draw_3d = show_3d_avatars_.load();
    const bool draw_xray = show_xray_through_earth_.load();
    const bool draw_markers = show_floating_markers_.load();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    gl.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    std::vector<Vertex2D> marker_vertices;

    // 1. Render 3D VR Avatars (HMD + Left/Right 6DoF Controllers + Voice Halo)
    if (draw_3d && !remote_players.empty()) {
        gl.UseProgram(avatar_prog_);

        for (const auto& player : remote_players) {
            const auto& snap = player.interp_snap;
            bool occluded_by_earth = EarthMath::IsOccludedByEarthSphere(eye_pos_kh, snap.hmd_pos_kh);
            if (occluded_by_earth && !draw_xray) {
                continue;
            }

            EarthMath::Vec3d hmd_start_pos = start_from_keyhole.TransformPointFromKeyholeToStart(snap.hmd_pos_kh);
            EarthMath::Quatd hmd_start_rot = start_from_keyhole.TransformRotFromKeyholeToStart(snap.hmd_rot_kh);

            double rel_scale = start_from_keyhole.scale * snap.viewer_scale;
            EarthMath::Vec3d rel_hmd = hmd_start_pos - eye_pos_start;
            double dist_start = rel_hmd.Length();

            double proj_lambda = 1.0;
            if (dist_start > 400.0) {
                proj_lambda = 400.0 / dist_start;
            }
            double render_scale = rel_scale * proj_lambda;

            gl.Uniform4f(u_avatar_tint_, player.color_rgb[0], player.color_rgb[1], player.color_rgb[2], 1.0f);
            gl.Uniform1f(u_avatar_xray_, occluded_by_earth ? 1.0f : 0.0f);
            gl.Uniform1f(u_avatar_voice_, player.voice_level);

            float model_mat[16], mvp_mat[16];
            hmd_start_rot.ToRowMajorMat4f(model_mat, rel_hmd * proj_lambda, render_scale);
            EarthMath::Mat4fMultiplyRowMajor(mvp_mat, view_proj, model_mat);
            gl.UniformMatrix4fv(u_avatar_model_, 1, GL_TRUE, model_mat);
            gl.UniformMatrix4fv(u_avatar_mvp_,   1, GL_TRUE, mvp_mat);

            gl.BindVertexArray(hmd_vao_);
            glDrawArrays(GL_TRIANGLES, 0, hmd_vertex_count_);

            if (player.voice_level > 0.02f) {
                double halo_scale = render_scale * (1.0 + 0.35 * player.voice_level);
                hmd_start_rot.ToRowMajorMat4f(model_mat, rel_hmd * proj_lambda, halo_scale);
                EarthMath::Mat4fMultiplyRowMajor(mvp_mat, view_proj, model_mat);
                gl.UniformMatrix4fv(u_avatar_model_, 1, GL_TRUE, model_mat);
                gl.UniformMatrix4fv(u_avatar_mvp_,   1, GL_TRUE, mvp_mat);
                gl.BindVertexArray(halo_vao_);
                glDrawArrays(GL_TRIANGLES, 0, halo_vertex_count_);
            }

            gl.BindVertexArray(ctrl_vao_);
            for (int c = 0; c < 2; ++c) {
                if (!snap.ctrl_active[c]) continue;
                EarthMath::Vec3d ctrl_start_pos = start_from_keyhole.TransformPointFromKeyholeToStart(snap.ctrl_pos_kh[c]);
                EarthMath::Quatd ctrl_start_rot = start_from_keyhole.TransformRotFromKeyholeToStart(snap.ctrl_rot_kh[c]);
                EarthMath::Vec3d rel_ctrl = (ctrl_start_pos - eye_pos_start) * proj_lambda;

                ctrl_start_rot.ToRowMajorMat4f(model_mat, rel_ctrl, render_scale);
                EarthMath::Mat4fMultiplyRowMajor(mvp_mat, view_proj, model_mat);
                gl.UniformMatrix4fv(u_avatar_model_, 1, GL_TRUE, model_mat);
                gl.UniformMatrix4fv(u_avatar_mvp_,   1, GL_TRUE, mvp_mat);
                glDrawArrays(GL_TRIANGLES, 0, ctrl_vertex_count_);
            }
        }
    }

    // 2. Build Floating Markers / Nametags / Distance & Scale Readouts
    if (draw_markers && !remote_players.empty()) {
        for (const auto& player : remote_players) {
            const auto& snap = player.interp_snap;
            bool occluded_by_earth = EarthMath::IsOccludedByEarthSphere(eye_pos_kh, snap.hmd_pos_kh);
            if (occluded_by_earth && !draw_xray) {
                continue;
            }

            EarthMath::Vec3d hmd_start_pos = start_from_keyhole.TransformPointFromKeyholeToStart(snap.hmd_pos_kh);
            double rel_scale = start_from_keyhole.scale * snap.viewer_scale;

            EarthMath::Vec3d hmd_to_target = hmd_start_pos - local_hmd_start_pos;
            double dist_from_hmd = hmd_to_target.Length();
            if (dist_from_hmd < 1e-4) continue;
            EarthMath::Vec3d dir_from_hmd = hmd_to_target / dist_from_hmd;

            double apparent_rad = (0.21 * rel_scale) / dist_from_hmd;
            bool is_microscopic = (apparent_rad < 0.035);

            double billboard_dist = std::clamp(dist_from_hmd, 0.85, 2.20);
            EarthMath::Vec3d anchor_start = local_hmd_start_pos + dir_from_hmd * billboard_dist;
            EarthMath::Vec3f anchor_eye = (anchor_start - eye_pos_start).ToFloat();

            float s = static_cast<float>(billboard_dist);

            constexpr float kSolidU0 = 1.0f / 512.0f;
            constexpr float kSolidV0 = 13.0f / 16.0f;
            constexpr float kSolidU1 = 6.0f / 512.0f;
            constexpr float kSolidV1 = 15.0f / 16.0f;

            if (is_microscopic) {
                float ret_r = 0.014f * s;
                float bar_t = 0.0025f * s;
                EarthMath::Vec3f top_p{anchor_eye.x + cam_up.x * ret_r, anchor_eye.y + cam_up.y * ret_r, anchor_eye.z + cam_up.z * ret_r};
                EarthMath::Vec3f bot_p{anchor_eye.x - cam_up.x * ret_r, anchor_eye.y - cam_up.y * ret_r, anchor_eye.z - cam_up.z * ret_r};
                EarthMath::Vec3f lft_p{anchor_eye.x - cam_right.x * ret_r, anchor_eye.y - cam_right.y * ret_r, anchor_eye.z - cam_right.z * ret_r};
                EarthMath::Vec3f rgt_p{anchor_eye.x + cam_right.x * ret_r, anchor_eye.y + cam_right.y * ret_r, anchor_eye.z + cam_right.z * ret_r};

                AppendBillboardQuad(marker_vertices, top_p, cam_right, cam_up, bar_t, ret_r * 0.45f,
                                    kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                                    player.color_rgb[0], player.color_rgb[1], player.color_rgb[2], 0.95f);
                AppendBillboardQuad(marker_vertices, bot_p, cam_right, cam_up, bar_t, ret_r * 0.45f,
                                    kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                                    player.color_rgb[0], player.color_rgb[1], player.color_rgb[2], 0.95f);
                AppendBillboardQuad(marker_vertices, lft_p, cam_right, cam_up, ret_r * 0.45f, bar_t,
                                    kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                                    player.color_rgb[0], player.color_rgb[1], player.color_rgb[2], 0.95f);
                AppendBillboardQuad(marker_vertices, rgt_p, cam_right, cam_up, ret_r * 0.45f, bar_t,
                                    kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                                    player.color_rgb[0], player.color_rgb[1], player.color_rgb[2], 0.95f);
            }

            float vertical_offset = is_microscopic ? (0.055f * s) : std::clamp(static_cast<float>(0.18 * rel_scale * (billboard_dist / dist_from_hmd)) + 0.035f * s, 0.05f * s, 0.22f * s);
            EarthMath::Vec3f card_center{
                anchor_eye.x + cam_up.x * vertical_offset,
                anchor_eye.y + cam_up.y * vertical_offset,
                anchor_eye.z + cam_up.z * vertical_offset
            };

            std::string line1 = player.name;
            if (player.voice_level > 0.03f) {
                line1 += " [MIC]";
            }

            double real_earth_dist_m = (snap.hmd_pos_kh - eye_pos_kh).Length();
            char dist_buf[32], scale_buf[32], line2_buf[128];
            FormatDistance(real_earth_dist_m, dist_buf, sizeof(dist_buf));
            FormatScale(snap.viewer_scale, scale_buf, sizeof(scale_buf));
            std::snprintf(line2_buf, sizeof(line2_buf), "%s / %s%s",
                          dist_buf, scale_buf, occluded_by_earth ? " / X-RAY" : "");
            std::string line2 = line2_buf;

            float char_h1 = 0.016f * s;
            float char_h2 = 0.0115f * s;
            size_t max_chars = std::max(line1.size(), line2.size());
            float card_half_w = std::max(0.08f * s, static_cast<float>(max_chars) * char_h2 * 0.39f + 0.015f * s);
            float card_half_h = 0.022f * s;

            EarthMath::Vec3f border_center{
                card_center.x + cam_up.x * (card_half_h + 0.002f * s),
                card_center.y + cam_up.y * (card_half_h + 0.002f * s),
                card_center.z + cam_up.z * (card_half_h + 0.002f * s)
            };
            AppendBillboardQuad(marker_vertices, border_center, cam_right, cam_up,
                                card_half_w, 0.0025f * s,
                                kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                                player.color_rgb[0], player.color_rgb[1], player.color_rgb[2], 0.95f);

            AppendBillboardQuad(marker_vertices, card_center, cam_right, cam_up,
                                card_half_w, card_half_h,
                                kSolidU0, kSolidV0, kSolidU1, kSolidV1,
                                0.05f, 0.07f, 0.11f, 0.82f);

            EarthMath::Vec3f line1_pos{
                card_center.x + cam_up.x * (0.0075f * s),
                card_center.y + cam_up.y * (0.0075f * s),
                card_center.z + cam_up.z * (0.0075f * s)
            };
            bool speaking = (player.voice_level > 0.03f);
            AppendBillboardText(marker_vertices, line1, line1_pos, cam_right, cam_up, char_h1,
                                speaking ? 0.35f : 1.0f,
                                speaking ? 1.0f  : 1.0f,
                                speaking ? 0.55f : 1.0f,
                                1.0f);

            EarthMath::Vec3f line2_pos{
                card_center.x - cam_up.x * (0.0095f * s),
                card_center.y - cam_up.y * (0.0095f * s),
                card_center.z - cam_up.z * (0.0095f * s)
            };
            AppendBillboardText(marker_vertices, line2, line2_pos, cam_right, cam_up, char_h2,
                                0.75f, 0.88f, 1.0f, 0.95f);
        }
    }

    // 3. Append the In-Game 3D VR Multiplayer Options Tab & Interactive Laser UI
    if (menu_visible_) {
        RenderMultiplayerMenuForEye(marker_vertices, eye_pos_start, network, voice, remote_players);
    }

    if (!marker_vertices.empty()) {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        gl.UseProgram(marker_prog_);
        gl.UniformMatrix4fv(u_marker_mvp_, 1, GL_TRUE, view_proj);
        gl.ActiveTexture(GL_TEXTURE0_EXT);
        glBindTexture(GL_TEXTURE_2D, font_tex_);
        gl.Uniform1i(u_marker_tex_, 0);

        gl.BindVertexArray(marker_vao_);
        gl.BindBuffer(GL_ARRAY_BUFFER_EXT, marker_vbo_);
        gl.BufferData(GL_ARRAY_BUFFER_EXT,
                      marker_vertices.size() * sizeof(Vertex2D),
                      marker_vertices.data(),
                      GL_DYNAMIC_DRAW_EXT);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(marker_vertices.size()));
    }
}

void StereoAvatarRenderer::RenderFrame(
    const EarthMath::Sim3d& start_from_keyhole,
    const EarthMath::Vec3d& local_hmd_start_pos,
    const EyeRenderParams eyes[2],
    const std::vector<EarthNet::RemotePlayerState>& remote_players,
    EarthNet::NetworkManager& network,
    EarthVoice::GlobalVoiceManager& voice) {

    if (remote_players.empty() && !menu_visible_) return;
    if (!start_from_keyhole.IsValid()) return;
    if (!EnsureInitialized()) return;

    ScopedGLState saved_state;

    for (int eye_idx = 0; eye_idx < 2; ++eye_idx) {
        RenderEyePass(eye_idx, eyes[eye_idx], start_from_keyhole, local_hmd_start_pos,
                      remote_players, network, voice);
    }
}

} // namespace EarthRender
