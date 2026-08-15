// gl_renderer.cpp - HEALPix 浏览器 OpenGL 渲染核心实现 (healpix_browser_qt)
// 功能: 球面渲染 (.hcsd) + 单帧切面投影渲染 (.hiss), 内嵌 STF 拉伸着色器
// 用途: 为 widgets/ 层提供纯 C++ OpenGL 3.3 Core 渲染入口, 无 Qt 依赖
// 依赖: browser_backend.h (数据源), stf_engine.h (STF uniform 转换),
// healpix_math.h (球面坐标转换), logger.h (日志)
// OpenGL 3.3 Core (wglGetProcAddress 加载 1.2+ 函数, opengl32.lib 链接 1.1 函数)
// 编译: g++ -O2 -std=c++17 -Wall -Wextra -Icore -Iinclude -I../../astro_image_io/include
// -c core/gl_renderer.cpp -o core/gl_renderer.o -lopengl32 -lgdi32
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md §3.4

#include "gl_renderer.h"
#include "healpix_math.h"
#include "logger.h"

// Windows 头文件（wglGetProcAddress 加载 OpenGL 3.3 函数）
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// OpenGL 1.1 头文件（提供 1.1 函数声明 + GL 类型 + 1.1 常量，链接 opengl32.lib）
#include <GL/gl.h>

// 数学常量（MSYS2 MinGW 需 _USE_MATH_DEFINES 才能暴露 M_PI）
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ============================================================================
// OpenGL 1.2+ 常量定义（Windows <GL/gl.h> 仅含 1.1，需手动补充）
// ============================================================================
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER                   0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW                    0x88E4
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW                   0x88E8
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER                0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER                  0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS                 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS                    0x8B82
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0                       0x84C0
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE                  0x812F
#endif
#ifndef GL_R32F
#define GL_R32F                           0x822E
#endif
#ifndef GL_RED
#define GL_RED                            0x1903
#endif
#ifndef GL_TEXTURE_WRAP_S
#define GL_TEXTURE_WRAP_S                 0x2802
#endif
#ifndef GL_TEXTURE_WRAP_T
#define GL_TEXTURE_WRAP_T                 0x2803
#endif

// ============================================================================
// OpenGL 1.2+ 函数指针类型定义
// ============================================================================
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;

typedef void (APIENTRY *PFN_glGenVertexArrays)(GLsizei n, GLuint* arrays);
typedef void (APIENTRY *PFN_glBindVertexArray)(GLuint array);
typedef void (APIENTRY *PFN_glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
typedef void (APIENTRY *PFN_glGenBuffers)(GLsizei n, GLuint* buffers);
typedef void (APIENTRY *PFN_glBindBuffer)(GLenum target, GLuint buffer);
typedef void (APIENTRY *PFN_glBufferData)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void (APIENTRY *PFN_glBufferSubData)(GLenum target, GLsizeiptr offset, GLsizeiptr size, const void* data);
typedef void (APIENTRY *PFN_glDeleteBuffers)(GLsizei n, const GLuint* buffers);
typedef GLuint (APIENTRY *PFN_glCreateShader)(GLenum type);
typedef void (APIENTRY *PFN_glShaderSource)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
typedef void (APIENTRY *PFN_glCompileShader)(GLuint shader);
typedef void (APIENTRY *PFN_glGetShaderiv)(GLuint shader, GLenum pname, GLint* params);
typedef void (APIENTRY *PFN_glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef GLuint (APIENTRY *PFN_glCreateProgram)(void);
typedef void (APIENTRY *PFN_glAttachShader)(GLuint program, GLuint shader);
typedef void (APIENTRY *PFN_glLinkProgram)(GLuint program);
typedef void (APIENTRY *PFN_glGetProgramiv)(GLuint program, GLenum pname, GLint* params);
typedef void (APIENTRY *PFN_glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void (APIENTRY *PFN_glUseProgram)(GLuint program);
typedef void (APIENTRY *PFN_glDeleteProgram)(GLuint program);
typedef void (APIENTRY *PFN_glDeleteShader)(GLuint shader);
typedef GLint (APIENTRY *PFN_glGetUniformLocation)(GLuint program, const GLchar* name);
typedef void (APIENTRY *PFN_glUniform1f)(GLint location, GLfloat v0);
typedef void (APIENTRY *PFN_glUniform1i)(GLint location, GLint v0);
typedef void (APIENTRY *PFN_glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRY *PFN_glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (APIENTRY *PFN_glEnableVertexAttribArray)(GLuint index);
typedef void (APIENTRY *PFN_glDisableVertexAttribArray)(GLuint index);
typedef void (APIENTRY *PFN_glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
typedef void (APIENTRY *PFN_glActiveTexture)(GLenum texture);

// ============================================================================
// 静态函数指针（init() 中通过 wglGetProcAddress 加载）
// ============================================================================
static PFN_glGenVertexArrays      pglGenVertexArrays = nullptr;
static PFN_glBindVertexArray      pglBindVertexArray = nullptr;
static PFN_glDeleteVertexArrays   pglDeleteVertexArrays = nullptr;
static PFN_glGenBuffers           pglGenBuffers = nullptr;
static PFN_glBindBuffer           pglBindBuffer = nullptr;
static PFN_glBufferData           pglBufferData = nullptr;
static PFN_glBufferSubData        pglBufferSubData = nullptr;
static PFN_glDeleteBuffers        pglDeleteBuffers = nullptr;
static PFN_glCreateShader         pglCreateShader = nullptr;
static PFN_glShaderSource         pglShaderSource = nullptr;
static PFN_glCompileShader        pglCompileShader = nullptr;
static PFN_glGetShaderiv          pglGetShaderiv = nullptr;
static PFN_glGetShaderInfoLog     pglGetShaderInfoLog = nullptr;
static PFN_glCreateProgram        pglCreateProgram = nullptr;
static PFN_glAttachShader         pglAttachShader = nullptr;
static PFN_glLinkProgram          pglLinkProgram = nullptr;
static PFN_glGetProgramiv         pglGetProgramiv = nullptr;
static PFN_glGetProgramInfoLog    pglGetProgramInfoLog = nullptr;
static PFN_glUseProgram           pglUseProgram = nullptr;
static PFN_glDeleteProgram        pglDeleteProgram = nullptr;
static PFN_glDeleteShader         pglDeleteShader = nullptr;
static PFN_glGetUniformLocation   pglGetUniformLocation = nullptr;
static PFN_glUniform1f            pglUniform1f = nullptr;
static PFN_glUniform1i            pglUniform1i = nullptr;
static PFN_glUniform4f            pglUniform4f = nullptr;
static PFN_glUniformMatrix4fv     pglUniformMatrix4fv = nullptr;
static PFN_glEnableVertexAttribArray pglEnableVertexAttribArray = nullptr;
static PFN_glDisableVertexAttribArray pglDisableVertexAttribArray = nullptr;
static PFN_glVertexAttribPointer  pglVertexAttribPointer = nullptr;
static PFN_glActiveTexture        pglActiveTexture = nullptr;

// OpenGL 函数加载状态
static bool gl_functions_loaded_ = false;

// ============================================================================
// OpenGL 1.2+ 函数加载（通过 wglGetProcAddress）
// 返回 0=成功, <0=失败
// ============================================================================
static int load_gl_functions() {
    if (gl_functions_loaded_) return 0;

    LOG_INFO("load_gl_functions: 开始加载 OpenGL 1.2+ 函数指针");

    // 逐个加载，任一失败则返回错误
    // 注: 先转 void* 再转目标类型，避免 -Wcast-function-type 警告
    // (wglGetProcAddress 返回 PROC，与各 GL 函数指针类型不兼容)
    #define LOAD_GL_FUNC(name) \
        p##name = reinterpret_cast<PFN_##name>( \
            reinterpret_cast<void*>(wglGetProcAddress(#name))); \
        if (!p##name) { \
            LOG_ERROR("load_gl_functions: 加载 " #name " 失败"); \
            return -1; \
        }

    LOAD_GL_FUNC(glGenVertexArrays)
    LOAD_GL_FUNC(glBindVertexArray)
    LOAD_GL_FUNC(glDeleteVertexArrays)
    LOAD_GL_FUNC(glGenBuffers)
    LOAD_GL_FUNC(glBindBuffer)
    LOAD_GL_FUNC(glBufferData)
    LOAD_GL_FUNC(glBufferSubData)
    LOAD_GL_FUNC(glDeleteBuffers)
    LOAD_GL_FUNC(glCreateShader)
    LOAD_GL_FUNC(glShaderSource)
    LOAD_GL_FUNC(glCompileShader)
    LOAD_GL_FUNC(glGetShaderiv)
    LOAD_GL_FUNC(glGetShaderInfoLog)
    LOAD_GL_FUNC(glCreateProgram)
    LOAD_GL_FUNC(glAttachShader)
    LOAD_GL_FUNC(glLinkProgram)
    LOAD_GL_FUNC(glGetProgramiv)
    LOAD_GL_FUNC(glGetProgramInfoLog)
    LOAD_GL_FUNC(glUseProgram)
    LOAD_GL_FUNC(glDeleteProgram)
    LOAD_GL_FUNC(glDeleteShader)
    LOAD_GL_FUNC(glGetUniformLocation)
    LOAD_GL_FUNC(glUniform1f)
    LOAD_GL_FUNC(glUniform1i)
    LOAD_GL_FUNC(glUniform4f)
    LOAD_GL_FUNC(glUniformMatrix4fv)
    LOAD_GL_FUNC(glEnableVertexAttribArray)
    LOAD_GL_FUNC(glDisableVertexAttribArray)
    LOAD_GL_FUNC(glVertexAttribPointer)
    LOAD_GL_FUNC(glActiveTexture)

    #undef LOAD_GL_FUNC

    gl_functions_loaded_ = true;
    LOG_INFO("load_gl_functions: OpenGL 1.2+ 函数全部加载成功");
    return 0;
}

// ============================================================================
// 着色器源码（内嵌字符串，GLSL 3.30 core）
// ============================================================================

// ---- 球面顶点着色器 ----
static const char* kSphereVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in float aValue;
uniform mat4 uMVPMatrix;
out float vValue;
void main() {
    vValue = aValue;
    gl_Position = uMVPMatrix * vec4(aPosition, 1.0);
}
)";

// ---- 球面片元着色器（STF 拉伸：MTF + asinh 压缩） ----
static const char* kSphereFragmentShader = R"(
#version 330 core
in float vValue;
uniform float uShadows;
uniform float uHighlights;
uniform float uMidtones;
uniform float uNoData;
uniform float uCompression;
out vec4 FragColor;

float mtf(float x, float m) {
    if (abs(m - 0.5) < 1e-10) return x;
    float denom = (2.0 * m - 1.0) * x - m;
    if (abs(denom) < 1e-30) denom = 1e-30;
    float r = ((m - 1.0) * x) / denom;
    return clamp(r, 0.0, 1.0);
}

float asinhCompress(float x, float c) {
    if (c < 1e-6) return x;
    float scale = max((1.0 - c) / c, 1e-6);
    float xv = x / scale;
    float yv = 1.0 / scale;
    float r = log(xv + sqrt(xv * xv + 1.0)) / log(yv + sqrt(yv * yv + 1.0));
    return clamp(r, 0.0, 1.0);
}

void main() {
    if (vValue <= uNoData) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float range = uHighlights - uShadows;
    if (range < 1e-30) range = 1.0;
    float x = (vValue - uShadows) / range;
    x = clamp(x, 0.0, 1.0);
    float result = mtf(x, uMidtones);
    if (uCompression > 1e-6) {
        result = asinhCompress(result, uCompression);
    }
    result = floor(result * 255.0 + 0.5) / 255.0;
    FragColor = vec4(vec3(result), 1.0);
}
)";

// ---- 单帧四边形顶点着色器（全屏四边形 + texcoord） ----
static const char* kQuadVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)";

// ---- 单帧四边形片元着色器（纹理采样 + STF 拉伸） ----
static const char* kQuadFragmentShader = R"(
#version 330 core
in vec2 vTexCoord;
uniform sampler2D uTexture;
uniform float uShadows;
uniform float uHighlights;
uniform float uMidtones;
uniform float uNoData;
uniform float uCompression;
out vec4 FragColor;

float mtf(float x, float m) {
    if (abs(m - 0.5) < 1e-10) return x;
    float denom = (2.0 * m - 1.0) * x - m;
    if (abs(denom) < 1e-30) denom = 1e-30;
    float r = ((m - 1.0) * x) / denom;
    return clamp(r, 0.0, 1.0);
}

float asinhCompress(float x, float c) {
    if (c < 1e-6) return x;
    float scale = max((1.0 - c) / c, 1e-6);
    float xv = x / scale;
    float yv = 1.0 / scale;
    float r = log(xv + sqrt(xv * xv + 1.0)) / log(yv + sqrt(yv * yv + 1.0));
    return clamp(r, 0.0, 1.0);
}

void main() {
    float vValue = texture(uTexture, vTexCoord).r;
    if (vValue <= uNoData) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float range = uHighlights - uShadows;
    if (range < 1e-30) range = 1.0;
    float x = (vValue - uShadows) / range;
    x = clamp(x, 0.0, 1.0);
    float result = mtf(x, uMidtones);
    if (uCompression > 1e-6) {
        result = asinhCompress(result, uCompression);
    }
    result = floor(result * 255.0 + 0.5) / 255.0;
    FragColor = vec4(vec3(result), 1.0);
}
)";

// ---- 经纬线网格顶点着色器（球面位置 + MVP） ----
static const char* kGridVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPosition;
uniform mat4 uMVPMatrix;
void main() {
    gl_Position = uMVPMatrix * vec4(aPosition, 1.0);
}
)";

// ---- 经纬线网格片元着色器（固定半透明绿色） ----
static const char* kGridFragmentShader = R"(
#version 330 core
uniform vec4 uGridColor;
out vec4 FragColor;
void main() {
    FragColor = uGridColor;
}
)";

// ============================================================================
// 辅助：编译单个着色器
// 返回 shader id (>0) 或 0（失败）
// ============================================================================
static unsigned int compile_single_shader(unsigned int shader_type,
                                          const char* source,
                                          const char* shader_name) {
    unsigned int shader = pglCreateShader(shader_type);
    if (shader == 0) {
        LOG_ERROR("compile_single_shader: glCreateShader 失败 (%s)", shader_name);
        return 0;
    }

    pglShaderSource(shader, 1, &source, nullptr);
    pglCompileShader(shader);

    int success = 0;
    pglGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        pglGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        LOG_ERROR("compile_single_shader: %s 编译失败:\n%s", shader_name, log);
        pglDeleteShader(shader);
        return 0;
    }

    LOG_INFO("compile_single_shader: %s 编译成功 (id=%u)", shader_name, shader);
    return shader;
}

// 辅助：链接着色器程序
// 返回 program id (>0) 或 0（失败）
static unsigned int link_program(unsigned int vs, unsigned int fs,
                                 const char* program_name) {
    unsigned int program = pglCreateProgram();
    if (program == 0) {
        LOG_ERROR("link_program: glCreateProgram 失败 (%s)", program_name);
        return 0;
    }

    pglAttachShader(program, vs);
    pglAttachShader(program, fs);
    pglLinkProgram(program);

    int success = 0;
    pglGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        pglGetProgramInfoLog(program, sizeof(log), nullptr, log);
        LOG_ERROR("link_program: %s 链接失败:\n%s", program_name, log);
        pglDeleteProgram(program);
        return 0;
    }

    // 着色器已链接到程序，可删除独立着色器对象
    pglDeleteShader(vs);
    pglDeleteShader(fs);

    LOG_INFO("link_program: %s 链接成功 (id=%u)", program_name, program);
    return program;
}

// ============================================================================
// 构造 / 析构
// ============================================================================

GLRenderer::GLRenderer()
    : sphere_program_(0), quad_program_(0),
      sphere_vao_(0), sphere_vbo_(0), sphere_ibo_(0), sphere_index_count_(0),
      quad_vao_(0), quad_vbo_(0),
      current_frame_(0),
      cached_stf_(), cached_data_min_(0.0f), cached_data_max_(1.0f),
      cached_no_data_(0.0f),
      single_frame_texture_(0),
      sf_center_ra_(0.0), sf_center_dec_(0.0),
      sf_width_deg_(0.0), sf_height_deg_(0.0),
      sf_texture_valid_(false),
      sf_last_view_ra_(0.0), sf_last_view_dec_(0.0), sf_last_view_zoom_(-1.0),
      sf_nside_(0),
      initialized_(false) {
}

GLRenderer::~GLRenderer() {
    // 析构时若未显式 cleanup，尝试释放（但 OpenGL 上下文可能已销毁）
    if (initialized_) {
        LOG_WARN("~GLRenderer: 析构时仍处于初始化状态，建议先调用 cleanup()");
    }
}

// ============================================================================
// init() - 初始化渲染器（需在 OpenGL 上下文 makeCurrent 后调用）
// ============================================================================

int GLRenderer::init() {
    if (initialized_) {
        LOG_WARN("init: 渲染器已初始化，跳过");
        return 0;
    }

    LOG_INFO("init: 开始初始化 GLRenderer");

    // 1. 加载 OpenGL 1.2+ 函数指针
    if (load_gl_functions() != 0) {
        LOG_ERROR("init: OpenGL 函数加载失败");
        return -1;
    }

    // 2. 编译着色器
    if (compile_shaders() != 0) {
        LOG_ERROR("init: 着色器编译失败");
        return -2;
    }

    // 3. 构建初始球面网格 (首次用固定 256×512, render_sphere 会按 FOV 动态重建)
    build_sphere_mesh(256, 512);

    // 4. 构建单帧四边形网格
    build_quad_mesh();

    // 5. 构建经纬线网格 (30° 网格)
    build_grid_mesh();

    // 6. 初始化单帧纹理 ID
    single_frame_texture_ = 0;

    initialized_ = true;
    LOG_INFO("init: GLRenderer 初始化完成");
    return 0;
}

bool GLRenderer::is_initialized() const {
    return initialized_;
}

// ============================================================================
// cleanup() - 释放 OpenGL 资源
// ============================================================================

void GLRenderer::cleanup() {
    if (!initialized_) return;

    LOG_INFO("cleanup: 开始释放 GLRenderer 资源");

    // 释放球面网格
    if (sphere_vao_) { pglDeleteVertexArrays(1, &sphere_vao_); sphere_vao_ = 0; }
    if (sphere_vbo_) { pglDeleteBuffers(1, &sphere_vbo_); sphere_vbo_ = 0; }
    if (sphere_ibo_) { pglDeleteBuffers(1, &sphere_ibo_); sphere_ibo_ = 0; }

    // 释放四边形网格
    if (quad_vao_) { pglDeleteVertexArrays(1, &quad_vao_); quad_vao_ = 0; }
    if (quad_vbo_) { pglDeleteBuffers(1, &quad_vbo_); quad_vbo_ = 0; }

    // 释放着色器程序
    if (sphere_program_) { pglDeleteProgram(sphere_program_); sphere_program_ = 0; }
    if (quad_program_) { pglDeleteProgram(quad_program_); quad_program_ = 0; }

    // 释放子叶纹理
    for (auto& lt : leaf_textures_) {
        if (lt.texture_id) glDeleteTextures(1, &lt.texture_id);
    }
    leaf_textures_.clear();

    // 释放单帧纹理
    if (single_frame_texture_) {
        glDeleteTextures(1, &single_frame_texture_);
        single_frame_texture_ = 0;
    }

    // 释放经纬线网格
    if (grid_vao_) { pglDeleteVertexArrays(1, &grid_vao_); grid_vao_ = 0; }
    if (grid_vbo_) { pglDeleteBuffers(1, &grid_vbo_); grid_vbo_ = 0; }
    if (grid_program_) { pglDeleteProgram(grid_program_); grid_program_ = 0; }
    grid_vertex_count_ = 0;
    grid_mesh_valid_ = false;

    sphere_vertex_coords_.clear();
    sf_texture_valid_ = false;
    initialized_ = false;

    LOG_INFO("cleanup: GLRenderer 资源释放完成");
}

// ============================================================================
// render() - 主渲染入口
// ============================================================================

int GLRenderer::render(BrowserBackend& backend, const RenderParams& params) {
    if (!initialized_) {
        LOG_ERROR("render: 渲染器未初始化");
        return -1;
    }

    if (params.viewport_w <= 0 || params.viewport_h <= 0) {
        LOG_ERROR("render: 无效视口尺寸 %dx%d", params.viewport_w, params.viewport_h);
        return -2;
    }

    // 设置视口
    glViewport(0, 0, params.viewport_w, params.viewport_h);

    // 清屏
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 缓存 STF 参数
    cached_stf_ = params.stf;
    cached_data_min_ = params.data_min;
    cached_data_max_ = params.data_max;
    cached_no_data_ = params.no_data_value;

    // 帧计数递增
    current_frame_++;

    // 按模式分发
    int ret = 0;
    if (params.mode == RenderMode::SPHERE) {
        ret = render_sphere(backend, params);
    } else if (params.mode == RenderMode::HISS_POLYGON) {
        ret = render_hiss_polygon(backend, params);
    } else {
        // 旧 SINGLE_FRAME 已废弃，回退到 hiss_polygon
        LOG_WARN("render: 未知 RenderMode=%d，回退到 HISS_POLYGON", static_cast<int>(params.mode));
        ret = render_hiss_polygon(backend, params);
    }

    // 叠加经纬线网格 (若可见)
    if (params.grid_visible && ret == 0) {
        render_grid(params);
    }

    return ret;
}

// ============================================================================
// update_stf() - 更新 STF 参数（仅更新缓存，下一帧 render 时生效）
// ============================================================================

void GLRenderer::update_stf(const STFParams& stf, float data_min, float data_max) {
    cached_stf_ = stf;
    cached_data_min_ = data_min;
    cached_data_max_ = data_max;
    LOG_DEBUG("update_stf: shadows=%g highlights=%g midtones=%g compression=%g min=%g max=%g",
              stf.shadows, stf.highlights, stf.midtones, stf.compression, data_min, data_max);
}

// ============================================================================
// set_single_frame_bbox() - 设置单帧模式数据边界框
// ============================================================================

void GLRenderer::set_single_frame_bbox(double center_ra, double center_dec,
                                        double width_deg, double height_deg) {
    sf_center_ra_ = center_ra;
    sf_center_dec_ = center_dec;
    sf_width_deg_ = width_deg;
    sf_height_deg_ = height_deg;
    sf_texture_valid_ = false;  // bbox 变化，纹理需重建
    LOG_INFO("set_single_frame_bbox: center=(%.4f, %.4f) size=%.4f x %.4f deg",
             center_ra, center_dec, width_deg, height_deg);
}

// ============================================================================
// get_loaded_leaves() - 获取已加载子叶列表（调试用）
// ============================================================================

std::vector<uint64_t> GLRenderer::get_loaded_leaves() const {
    std::vector<uint64_t> result;
    result.reserve(leaf_textures_.size());
    for (const auto& lt : leaf_textures_) {
        result.push_back(lt.leaf_ipix);
    }
    return result;
}

// ============================================================================
// compile_shaders() - 编译球面 + 四边形着色器程序
// ============================================================================

int GLRenderer::compile_shaders() {
    LOG_INFO("compile_shaders: 开始编译着色器");

    // ---- 球面着色器 ----
    unsigned int sphere_vs = compile_single_shader(GL_VERTEX_SHADER,
                                                    kSphereVertexShader,
                                                    "sphere_vertex");
    if (sphere_vs == 0) return -1;

    unsigned int sphere_fs = compile_single_shader(GL_FRAGMENT_SHADER,
                                                    kSphereFragmentShader,
                                                    "sphere_fragment");
    if (sphere_fs == 0) {
        pglDeleteShader(sphere_vs);
        return -2;
    }

    sphere_program_ = link_program(sphere_vs, sphere_fs, "sphere_program");
    if (sphere_program_ == 0) return -3;

    // ---- 单帧四边形着色器 ----
    unsigned int quad_vs = compile_single_shader(GL_VERTEX_SHADER,
                                                  kQuadVertexShader,
                                                  "quad_vertex");
    if (quad_vs == 0) return -4;

    unsigned int quad_fs = compile_single_shader(GL_FRAGMENT_SHADER,
                                                  kQuadFragmentShader,
                                                  "quad_fragment");
    if (quad_fs == 0) {
        pglDeleteShader(quad_vs);
        return -5;
    }

    quad_program_ = link_program(quad_vs, quad_fs, "quad_program");
    if (quad_program_ == 0) return -6;

    // ---- 经纬线网格着色器 ----
    unsigned int grid_vs = compile_single_shader(GL_VERTEX_SHADER,
                                                   kGridVertexShader,
                                                   "grid_vertex");
    if (grid_vs == 0) return -7;

    unsigned int grid_fs = compile_single_shader(GL_FRAGMENT_SHADER,
                                                   kGridFragmentShader,
                                                   "grid_fragment");
    if (grid_fs == 0) {
        pglDeleteShader(grid_vs);
        return -8;
    }

    grid_program_ = link_program(grid_vs, grid_fs, "grid_program");
    if (grid_program_ == 0) return -9;

    LOG_INFO("compile_shaders: 所有着色器编译链接完成");
    return 0;
}

// ============================================================================
// build_sphere_mesh() - 构建 UV 球面网格
// segments_lat: 纬度分段数, segments_lon: 经度分段数
// 顶点格式: [x, y, z, value] (4 float), value 初始为 0
// 顶点数: (segments_lat+1) * (segments_lon+1)
// ============================================================================

void GLRenderer::build_sphere_mesh(int segments_lat, int segments_lon) {
    LOG_INFO("build_sphere_mesh: lat=%d lon=%d", segments_lat, segments_lon);

    sphere_vertex_coords_.clear();

    int num_verts = (segments_lat + 1) * (segments_lon + 1);
    // 每顶点 4 float: x, y, z, value
    std::vector<float> vertices(num_verts * 4, 0.0f);

    int vi = 0;
    for (int i = 0; i <= segments_lat; i++) {
        // 纬度: -90° → 90°
        double lat = -M_PI / 2.0 + M_PI * (double)i / segments_lat;
        double dec = lat * 180.0 / M_PI;  // 转度

        for (int j = 0; j <= segments_lon; j++) {
            // 经度: 0° → 360°
            double lon = 2.0 * M_PI * (double)j / segments_lon;
            double ra = lon * 180.0 / M_PI;  // 转度
            if (ra >= 360.0) ra -= 360.0;

            // 单位球笛卡尔坐标
            float x = (float)(std::cos(lat) * std::cos(lon));
            float y = (float)(std::cos(lat) * std::sin(lon));
            float z = (float)(std::sin(lat));

            vertices[vi * 4 + 0] = x;
            vertices[vi * 4 + 1] = y;
            vertices[vi * 4 + 2] = z;
            vertices[vi * 4 + 3] = 0.0f;  // value 初始 0

            // 缓存 (ra, dec) 供 render 时查值
            SphereVertexCoord vc;
            vc.ra = ra;
            vc.dec = dec;
            vc.x = x;
            vc.y = y;
            vc.z = z;
            vc.leaf_ipix = HealpixMath::ang2pix_nest(64, ra, dec);
            sphere_vertex_coords_.push_back(vc);

            vi++;
        }
    }

    // 构建索引（三角形列表）
    // 每个 grid cell → 2 个三角形
    int num_indices = segments_lat * segments_lon * 6;
    std::vector<unsigned int> indices(num_indices);
    int ii = 0;
    for (int i = 0; i < segments_lat; i++) {
        for (int j = 0; j < segments_lon; j++) {
            int v0 = i * (segments_lon + 1) + j;
            int v1 = v0 + 1;
            int v2 = (i + 1) * (segments_lon + 1) + j;
            int v3 = v2 + 1;

            // 三角形 1: v0, v2, v1
            indices[ii++] = v0;
            indices[ii++] = v2;
            indices[ii++] = v1;
            // 三角形 2: v1, v2, v3
            indices[ii++] = v1;
            indices[ii++] = v2;
            indices[ii++] = v3;
        }
    }
    sphere_index_count_ = num_indices;

    // 创建 VAO / VBO / IBO
    pglGenVertexArrays(1, &sphere_vao_);
    pglGenBuffers(1, &sphere_vbo_);
    pglGenBuffers(1, &sphere_ibo_);

    pglBindVertexArray(sphere_vao_);

    // VBO: 顶点数据（位置 + 值）
    pglBindBuffer(GL_ARRAY_BUFFER, sphere_vbo_);
    pglBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                  vertices.data(), GL_DYNAMIC_DRAW);

    // IBO: 索引数据
    pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ibo_);
    pglBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                  indices.data(), GL_STATIC_DRAW);

    // 顶点属性 0: aPosition (vec3)
    pglEnableVertexAttribArray(0);
    pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // 顶点属性 1: aValue (float)
    pglEnableVertexAttribArray(1);
    pglVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                           (void*)(3 * sizeof(float)));

    pglBindVertexArray(0);

    LOG_INFO("build_sphere_mesh: 完成，顶点数=%d 索引数=%d", num_verts, num_indices);
}

// ============================================================================
// build_sphere_mesh_dynamic() - 按 FOV 和视口动态构建球面网格
// 顶点密度 ≈ 屏幕像素密度 (每顶点约 1 屏幕像素)
// 网格覆盖 FOV×1.2 (渲染余量), 以视角中心为局部 UV 网格
// ============================================================================

void GLRenderer::build_sphere_mesh_dynamic(const ViewParams& view,
                                            int viewport_w, int viewport_h) {
    // 屏幕像素角分辨率 (度/像素)
    double fov_deg = view.fov_deg;
    if (fov_deg < 0.01) fov_deg = 60.0;
    if (fov_deg > 170.0) fov_deg = 170.0;

    double aspect = (viewport_w > 0 && viewport_h > 0)
                    ? (double)viewport_w / (double)viewport_h : 1.0;
    double fov_ver = fov_deg;
    double fov_hor = fov_deg * aspect;

    // 渲染范围 = FOV × 1.2 (余量, 避免边缘裁剪)
    double render_fov_ver = fov_ver * 1.2;
    double render_fov_hor = fov_hor * 1.2;

    // 顶点间距 = 屏幕像素角分辨率 × 2 (每顶点 2 像素, 顶点数减 4 倍)
    // GPU 线性插值填充顶点间像素, 视觉质量几乎不变
    double theta_screen = (fov_ver / (double)viewport_h) * 2.0;  // 度/顶点
    if (theta_screen < 1e-6) theta_screen = 0.01;

    // 分段数 = 渲染范围 / 顶点间距
    int segments_lat = std::max(8, (int)(render_fov_ver / theta_screen) + 1);
    int segments_lon = std::max(8, (int)(render_fov_hor / theta_screen) + 1);

    // 上限保护 (避免极端情况爆内存, 8M 顶点 = 128MB VBO)
    const int MAX_SEGMENTS = 2048;
    if (segments_lat > MAX_SEGMENTS) segments_lat = MAX_SEGMENTS;
    if (segments_lon > MAX_SEGMENTS) segments_lon = MAX_SEGMENTS;

    LOG_INFO("build_sphere_mesh_dynamic: fov=%.2f viewport=%dx%d theta=%.4f°/px "
             "segments=%dx%d (render范围 %.2fx%.2f°)",
             fov_deg, viewport_w, viewport_h, theta_screen,
             segments_lat, segments_lon, render_fov_ver, render_fov_hor);

    sphere_vertex_coords_.clear();

    // 网格中心 = 视角中心
    double center_ra = view.center_ra;
    double center_dec = view.center_dec;

    int num_verts = (segments_lat + 1) * (segments_lon + 1);
    std::vector<float> vertices(num_verts * 4, 0.0f);

    // 纬度范围: center_dec ± render_fov_ver/2
    // 经度范围: center_ra ± render_fov_hor/2
    double dec_min = center_dec - render_fov_ver / 2.0;
    double dec_max = center_dec + render_fov_ver / 2.0;
    // 经度需考虑 cos(dec) 缩放 (实际 RA 跨度 = fov_hor / cos(dec))
    double cos_dec = std::cos(center_dec * M_PI / 180.0);
    if (std::abs(cos_dec) < 0.01) cos_dec = 0.01;  // 极区兜底
    double ra_span = render_fov_hor / cos_dec;
    double ra_min = center_ra - ra_span / 2.0;

    int vi = 0;
    for (int i = 0; i <= segments_lat; i++) {
        double dec = dec_min + (dec_max - dec_min) * (double)i / segments_lat;
        double dec_rad = dec * M_PI / 180.0;

        for (int j = 0; j <= segments_lon; j++) {
            double ra = ra_min + ra_span * (double)j / segments_lon;
            if (ra >= 360.0) ra -= 360.0;
            if (ra < 0.0) ra += 360.0;
            double ra_rad = ra * M_PI / 180.0;

            // 单位球笛卡尔坐标
            float x = (float)(std::cos(dec_rad) * std::cos(ra_rad));
            float y = (float)(std::cos(dec_rad) * std::sin(ra_rad));
            float z = (float)(std::sin(dec_rad));

            vertices[vi * 4 + 0] = x;
            vertices[vi * 4 + 1] = y;
            vertices[vi * 4 + 2] = z;
            vertices[vi * 4 + 3] = 0.0f;

            // 预计算笛卡尔坐标和 leaf_ipix, 查值时直接用 (避免每帧 cos/sin/ang2pix)
            SphereVertexCoord vc;
            vc.ra = ra;
            vc.dec = dec;
            vc.x = x;
            vc.y = y;
            vc.z = z;
            vc.leaf_ipix = HealpixMath::ang2pix_nest(64, ra, dec);
            sphere_vertex_coords_.push_back(vc);
            vi++;
        }
    }

    // 索引 (三角形列表)
    int num_indices = segments_lat * segments_lon * 6;
    std::vector<unsigned int> indices(num_indices);
    int ii = 0;
    for (int i = 0; i < segments_lat; i++) {
        for (int j = 0; j < segments_lon; j++) {
            int v0 = i * (segments_lon + 1) + j;
            int v1 = v0 + 1;
            int v2 = (i + 1) * (segments_lon + 1) + j;
            int v3 = v2 + 1;
            indices[ii++] = v0; indices[ii++] = v2; indices[ii++] = v1;
            indices[ii++] = v1; indices[ii++] = v2; indices[ii++] = v3;
        }
    }
    sphere_index_count_ = num_indices;

    // 重建 VAO/VBO/IBO (先删旧的)
    if (sphere_vao_) { pglDeleteVertexArrays(1, &sphere_vao_); sphere_vao_ = 0; }
    if (sphere_vbo_) { pglDeleteBuffers(1, &sphere_vbo_); sphere_vbo_ = 0; }
    if (sphere_ibo_) { pglDeleteBuffers(1, &sphere_ibo_); sphere_ibo_ = 0; }

    pglGenVertexArrays(1, &sphere_vao_);
    pglGenBuffers(1, &sphere_vbo_);
    pglGenBuffers(1, &sphere_ibo_);

    pglBindVertexArray(sphere_vao_);

    pglBindBuffer(GL_ARRAY_BUFFER, sphere_vbo_);
    pglBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                  vertices.data(), GL_DYNAMIC_DRAW);

    pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ibo_);
    pglBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                  indices.data(), GL_STATIC_DRAW);

    pglEnableVertexAttribArray(0);
    pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    pglEnableVertexAttribArray(1);
    pglVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                           (void*)(3 * sizeof(float)));

    pglBindVertexArray(0);

    // 记录当前网格参数 (用于 need_rebuild_mesh 检测)
    mesh_center_ra_ = view.center_ra;
    mesh_center_dec_ = view.center_dec;
    mesh_fov_ = fov_deg;
    mesh_viewport_w_ = viewport_w;
    mesh_viewport_h_ = viewport_h;

    LOG_INFO("build_sphere_mesh_dynamic: 完成，顶点数=%d 索引数=%d",
             num_verts, num_indices);
}

// ============================================================================
// need_rebuild_mesh() - 检测是否需要重建网格
// 触发条件: 首次、FOV 变化>20%、视口变化>10%、中心移动超过半个视场
// ============================================================================

bool GLRenderer::need_rebuild_mesh(const ViewParams& view, int viewport_w, int viewport_h) const {
    if (mesh_fov_ < 0) return true;  // 首次

    // FOV 变化 >20% (避免缩放时频繁重建网格, 影响流畅性)
    double fov_ratio = std::abs(view.fov_deg - mesh_fov_) / std::max(mesh_fov_, 1.0);
    if (fov_ratio > 0.20) return true;

    // 视口变化 >10%
    if (viewport_w > 0 && mesh_viewport_w_ > 0) {
        double vp_ratio = std::abs((double)viewport_w - (double)mesh_viewport_w_) / (double)mesh_viewport_w_;
        if (vp_ratio > 0.10) return true;
    }

    // 中心移动超过半个视场 (避免拖动时频繁重建)
    double half_fov = view.fov_deg / 2.0;
    double dr = std::abs(view.center_ra - mesh_center_ra_);
    if (dr > 180.0) dr = 360.0 - dr;  // 跨 0/360°
    double dd = std::abs(view.center_dec - mesh_center_dec_);
    if (dr > half_fov * 0.5 || dd > half_fov * 0.5) return true;

    return false;
}

// ============================================================================
// build_quad_mesh() - 构建全屏四边形网格
// 顶点格式: [x, y, u, v] (4 float)
// 四边形 (-1,-1) → (1,1), texcoord (0,0) → (1,1)
// ============================================================================

void GLRenderer::build_quad_mesh() {
    LOG_INFO("build_quad_mesh: 构建全屏四边形");

    // 4 顶点: position(xy) + texcoord(uv)
    // TRIANGLE_FAN 顺序: 左下 → 右下 → 右上 → 左上
    // 注: OpenGL 纹理 v=0 在底部，但渲染时通常 v=0 在顶部
    // 这里 texcoord (0,0) 对应左下角
    float vertices[] = {
        // x y u v
        -1.0f, -1.0f, 0.0f, 0.0f,  // 左下 (v0)
         1.0f, -1.0f, 1.0f, 0.0f,  // 右下 (v1)
         1.0f,  1.0f, 1.0f, 1.0f,  // 右上 (v2)
        -1.0f,  1.0f, 0.0f, 1.0f,  // 左上 (v3)
    };
    // TRIANGLE_FAN: 三角形1=(v0,v1,v2), 三角形2=(v0,v2,v3) → 覆盖整个四边形

    pglGenVertexArrays(1, &quad_vao_);
    pglGenBuffers(1, &quad_vbo_);

    pglBindVertexArray(quad_vao_);

    // VBO
    pglBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
    pglBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 顶点属性 0: aPosition (vec2)
    pglEnableVertexAttribArray(0);
    pglVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // 顶点属性 1: aTexCoord (vec2)
    pglEnableVertexAttribArray(1);
    pglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                           (void*)(2 * sizeof(float)));

    pglBindVertexArray(0);

    LOG_INFO("build_quad_mesh: 完成");
}

// ============================================================================
// upload_leaf_texture() - 上传子叶数据为 1D R32F 纹理
// 用途: 将子叶像素值上传为纹理（备用接口，当前 render_sphere 用方案 B 顶点查值）
// ============================================================================

unsigned int GLRenderer::upload_leaf_texture(const LeafData& leaf) {
    if (leaf.n_pix == 0 || leaf.pixel == nullptr) {
        LOG_WARN("upload_leaf_texture: 空子叶数据 (leaf_ipix=%llu)",
                 (unsigned long long)leaf.leaf_ipix);
        return 0;
    }

    unsigned int tex_id = 0;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 将子叶像素值作为 1D 纹理上传（宽度=n_pix, 高度=1, R32F 格式）
    // 注: n_pix 可能很大（nside=8192 时单子叶约 2M 像素），用 1D 纹理存储
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F,
                 (int)leaf.n_pix, 1, 0,
                 GL_RED, GL_FLOAT, leaf.pixel);
    glBindTexture(GL_TEXTURE_2D, 0);

    LOG_DEBUG("upload_leaf_texture: leaf=%llu n_pix=%llu tex=%u",
              (unsigned long long)leaf.leaf_ipix,
              (unsigned long long)leaf.n_pix, tex_id);
    return tex_id;
}

// ============================================================================
// evict_unused_leaves() - LRU 淘汰过期子叶纹理
// ============================================================================

void GLRenderer::evict_unused_leaves(size_t max_leaves) {
    if (leaf_textures_.size() <= max_leaves) return;

    // 按 last_used_frame 升序排序（最久未用的在前）
    std::sort(leaf_textures_.begin(), leaf_textures_.end(),
              [](const LeafTexture& a, const LeafTexture& b) {
                  return a.last_used_frame < b.last_used_frame;
              });

    // 删除最旧的，直到数量 <= max_leaves
    size_t to_remove = leaf_textures_.size() - max_leaves;
    for (size_t i = 0; i < to_remove; i++) {
        if (leaf_textures_[i].texture_id) {
            glDeleteTextures(1, &leaf_textures_[i].texture_id);
        }
    }
    leaf_textures_.erase(leaf_textures_.begin(),
                         leaf_textures_.begin() + to_remove);

    LOG_DEBUG("evict_unused_leaves: 淘汰 %zu 个，剩余 %zu", to_remove, leaf_textures_.size());
}

// ============================================================================
// render_sphere() - 球面渲染（.hcsd 模式）
// 方案 B: CPU 端每顶点查值，作为 vertex attribute 上传
// ============================================================================

int GLRenderer::render_sphere(BrowserBackend& backend, const RenderParams& params) {
    const ViewParams& view = params.view;

    // ---- 1. 球心相机: 相机在球心(0,0,0)，向外看天球外表面 ----
    // 与 render_hiss_polygon 统一相机模型, 遵守赤道坐标系约定
    // FOV 范围保护: [0.01°, 170°], 防止 tan(fov/2) 退化
    double fov_deg = view.fov_deg;
    if (fov_deg < 0.01) fov_deg = 60.0;
    if (fov_deg > 170.0) fov_deg = 170.0;

    // 相机朝向: 从球心看向 (center_ra, center_dec) 方向
    double center_ra_rad = view.center_ra * M_PI / 180.0;
    double center_dec_rad = view.center_dec * M_PI / 180.0;
    double fx = std::cos(center_dec_rad) * std::cos(center_ra_rad);
    double fy = std::cos(center_dec_rad) * std::sin(center_ra_rad);
    double fz = std::sin(center_dec_rad);

    // 相机位置 = 球心 (0, 0, 0), 看向目标 = forward
    double eye_x = 0.0, eye_y = 0.0, eye_z = 0.0;
    double target_x = fx, target_y = fy, target_z = fz;

    // up 向量: 北天极方向投影到 forward 垂直平面
    // up = world_up - (world_up · forward) * forward, world_up=(0,0,1)
    double dot = fz;  // world_up · forward = sin(dec)
    double up_x = -dot * fx;
    double up_y = -dot * fy;
    double up_z = 1.0 - dot * fz;
    double up_len = std::sqrt(up_x * up_x + up_y * up_y + up_z * up_z);
    if (up_len < 1e-10) {
        // 极区兜底: dec=±90° 时 up 退化，用 (0,1,0) 替代
        up_x = 0.0; up_y = 1.0; up_z = 0.0;
    } else {
        up_x /= up_len; up_y /= up_len; up_z /= up_len;
    }

    // ---- 2. 计算投影/视图矩阵 ----
    float proj_mat[16], view_mat[16], mvp_mat[16];
    double aspect = (double)params.viewport_w / (double)params.viewport_h;
    perspective_matrix(fov_deg, aspect, 0.01, 100.0, proj_mat);
    look_at_matrix(eye_x, eye_y, eye_z,
                   target_x, target_y, target_z,
                   up_x, up_y, up_z, view_mat);
    multiply_matrix(proj_mat, view_mat, mvp_mat);

    // ---- 2.5 动态重建球面网格 (FOV/视口/中心变化时) ----
    if (need_rebuild_mesh(view, params.viewport_w, params.viewport_h)) {
        build_sphere_mesh_dynamic(view, params.viewport_w, params.viewport_h);
        // 不全清子叶缓存: 增量更新逻辑会自动清理不需要的子叶、加载新增的子叶
        // FOV 变化时 nside_target 变化, 但旧子叶仍可复用 (LOD 阈值只影响新加载的子叶)
        // 注: 若 nside_target 变化导致已缓存子叶的分辨率不匹配, 查值时仍用旧 nside,
        // 视觉上会有轻微分辨率差异, 但避免了反复加载/ud_grade 的性能开销
        cache_fov_ = view.fov_deg;
    }

    // ---- 3. 子叶数据跨帧缓存 (避免每帧重新 load_leaf + ud_grade) ----
    // 增量更新: 只加载缓存中没有的子叶, 保留仍需要的旧子叶
    // 清理: 移除不在当前 required 列表中的旧子叶
    std::vector<uint64_t> required = backend.get_required_leaves(view);
    LOG_DEBUG("render_sphere: 需要子叶 %zu 个 (缓存 %zu)", required.size(), leaf_cache_.size());

    // 构建 required 集合用于清理
    std::unordered_set<uint64_t> required_set(required.begin(), required.end());

    // 清理缓存中不在 required 列表里的子叶
    for (auto it = leaf_cache_.begin(); it != leaf_cache_.end(); ) {
        if (required_set.find(it->first) == required_set.end()) {
            it = leaf_cache_.erase(it);
        } else {
            ++it;
        }
    }

    // 加载缓存中缺失的子叶, 或 nside 不匹配的子叶 (放大后 nside_target 增大, 旧缓存分辨率不够)
    int newly_loaded = 0;
    int reloaded = 0;
    for (uint64_t leaf_ipix : required) {
        // LOD 自动阈值: 按屏幕分辨率停止下钻
        uint32_t target_nside = backend.decide_target_nside(view, leaf_ipix,
                                                             params.viewport_w, params.viewport_h);

        auto cache_it = leaf_cache_.find(leaf_ipix);
        if (cache_it != leaf_cache_.end() && cache_it->second.nside >= target_nside) {
            continue;  // 已缓存且 nside 足够, 跳过
        }

        // nside 不匹配或未缓存: 释放旧缓存, 重新加载
        if (cache_it != leaf_cache_.end()) {
            cache_it->second.release();
            leaf_cache_.erase(cache_it);
            reloaded++;
        }

        LeafData leaf = backend.load_leaf(leaf_ipix, target_nside);

        if (leaf.n_pix == 0 || (leaf.pixel == nullptr && leaf.pixel_u8 == nullptr)) {
            backend.release_leaf(leaf);
            continue;
        }

        CachedLeaf cl;
        cl.nside = leaf.nside;
        cl.n = leaf.n_pix;
        cl.use_u8 = leaf.use_u8;
        if (leaf.owned) {
            // ud_grade 结果: 转移指针所有权
            cl.ipix = leaf.ipix;
            if (leaf.use_u8) {
                cl.pixel_u8 = leaf.pixel_u8;
            } else {
                cl.pixel = leaf.pixel;
            }
            cl.owned = true;
            leaf.ipix = nullptr;
            leaf.pixel = nullptr;
            leaf.pixel_u8 = nullptr;
            leaf.n_pix = 0;
            leaf.owned = false;
        } else {
            // 零拷贝切片: 指针指向 backend 内部 (float32)
            cl.ipix = leaf.ipix;
            cl.pixel = leaf.pixel;
            cl.owned = false;
            cl.use_u8 = false;
        }
        backend.release_leaf(leaf);
        leaf_cache_[leaf_ipix] = std::move(cl);
        newly_loaded++;
    }

    // ---- 4. 每顶点查值，更新 VBO ----
    int num_verts = (int)sphere_vertex_coords_.size();
    std::vector<float> vertex_data(num_verts * 4, 0.0f);

    // uint8 反归一化参数
    float u8_range = params.data_max - params.data_min;
    float u8_min = params.data_min;

    for (int i = 0; i < num_verts; i++) {
        const auto& vc = sphere_vertex_coords_[i];

        // 笛卡尔坐标 (预计算, 直接用)
        vertex_data[i * 4 + 0] = vc.x;
        vertex_data[i * 4 + 1] = vc.y;
        vertex_data[i * 4 + 2] = vc.z;

        // 查值: 先找 nside=64 子叶，再在子叶的 nside 层二分查找 ipix
        // 若 ipix 找不到 (数据是拼接图非全天覆盖, ud_grade 后无数据像素不输出):
        // 用位运算降到更粗 nside, 在数组中查找属于同一粗像素的任意子像素
        // HEALPix nested: nside 降半 = ipix 右移 2 位, 粗像素范围 [coarse<<shift, (coarse+1)<<shift)
        float value = params.no_data_value;
        auto it = leaf_cache_.find(vc.leaf_ipix);
        if (it != leaf_cache_.end()) {
            const CachedLeaf& lm = it->second;
            uint64_t ipix_fine = HealpixMath::ang2pix_nest(lm.nside, vc.ra, vc.dec);

            // 逐级降低 nside 查找: shift=0(原始), 2(nside/2), 4(nside/4), ...
            // 限制最大 shift=4 (nside/16): 只填充小的拼接缝隙, 不填充大的无数据区域
            // 超过 shift=4 仍找不到则返回 no_data (保持黑色, 避免远处像素错误填充)
            const uint32_t MAX_SHIFT = 4;
            uint32_t shift = 0;
            while (shift <= MAX_SHIFT) {
                uint64_t ipix_lo = ipix_fine >> shift;       // 当前粗像素 id
                uint64_t range_lo = ipix_lo << shift;        // 粗像素在 fine 层的起始 ipix
                uint64_t range_hi = (ipix_lo + 1) << shift;  // 粗像素在 fine 层的结束 ipix (不含)

                // 在排序数组中查找 >= range_lo 的第一个
                const uint64_t* begin = lm.ipix;
                const uint64_t* end = lm.ipix + lm.n;
                const uint64_t* bit = std::lower_bound(begin, end, range_lo);
                if (bit != end && *bit < range_hi) {
                    // 找到属于同一粗像素的子像素, 用它的值
                    size_t idx = bit - begin;
                    if (lm.use_u8 && lm.pixel_u8) {
                        value = (float)lm.pixel_u8[idx] / 255.0f * u8_range + u8_min;
                    } else if (lm.pixel) {
                        value = lm.pixel[idx];
                    }
                    break;
                }

                // 当前 nside 找不到, 降一级 (shift += 2)
                shift += 2;
            }
        }
        vertex_data[i * 4 + 3] = value;
    }

    // 更新 VBO
    pglBindBuffer(GL_ARRAY_BUFFER, sphere_vbo_);
    pglBufferSubData(GL_ARRAY_BUFFER, 0,
                     vertex_data.size() * sizeof(float),
                     vertex_data.data());

    // ---- 5. 绘制 ----
    // 球心相机: 从球内看球内壁, 禁用深度测试和背面剔除
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    pglUseProgram(sphere_program_);

    // 设置 MVP uniform
    GLint mvp_loc = pglGetUniformLocation(sphere_program_, "uMVPMatrix");
    if (mvp_loc >= 0) {
        pglUniformMatrix4fv(mvp_loc, 1, GL_FALSE, mvp_mat);
    }

    // 设置 STF uniform
    STFEngine::GPUUniforms u = STFEngine::to_uniforms(
        params.stf, params.data_min, params.data_max, params.no_data_value);

    GLint loc_shadows = pglGetUniformLocation(sphere_program_, "uShadows");
    GLint loc_highlights = pglGetUniformLocation(sphere_program_, "uHighlights");
    GLint loc_midtones = pglGetUniformLocation(sphere_program_, "uMidtones");
    GLint loc_nodata = pglGetUniformLocation(sphere_program_, "uNoData");
    GLint loc_compression = pglGetUniformLocation(sphere_program_, "uCompression");

    if (loc_shadows >= 0)     pglUniform1f(loc_shadows, u.shadows);
    if (loc_highlights >= 0)  pglUniform1f(loc_highlights, u.highlights);
    if (loc_midtones >= 0)    pglUniform1f(loc_midtones, u.midtones);
    if (loc_nodata >= 0)      pglUniform1f(loc_nodata, u.no_data);
    if (loc_compression >= 0) pglUniform1f(loc_compression, u.compression);

    // 绘制球面
    pglBindVertexArray(sphere_vao_);
    glDrawElements(GL_TRIANGLES, sphere_index_count_, GL_UNSIGNED_INT, (void*)0);
    pglBindVertexArray(0);

    glDisable(GL_DEPTH_TEST);

    LOG_DEBUG("render_sphere: 渲染完成 (缓存子叶=%zu 顶点=%d)",
              leaf_cache_.size(), num_verts);
    return 0;
}

// ============================================================================
// build_hiss_polygon_mesh() - 构建 .hiss HEALPix 像素多边形网格
// 每像素生成 4 角点（球面四边形），作为 2 个三角形（6 顶点）绘制
// 返回 0=成功, <0=失败
// ============================================================================

int GLRenderer::build_hiss_polygon_mesh(BrowserBackend& backend) {
    LOG_INFO("build_hiss_polygon_mesh: 开始构建 .hiss 像素多边形网格");

    LeafData all = backend.get_all_data();
    if (all.n_pix == 0 || all.pixel == nullptr) {
        LOG_ERROR("build_hiss_polygon_mesh: 无有效数据");
        return -1;
    }

    // ---- 自动 ud_grade 降采样 (避免大数据集卡死 GPU/主线程) ----
    // 每像素 6 顶点 × 4 float = 24 float = 96 bytes
    // 阈值 4M 像素 → 顶点数据 ≈ 384 MB (GPU 可接受, 主线程构建 < 1s)
    // nside=65536 (n_pix=61.6M) → nside=16384 (n_pix=3.85M)
    const uint64_t MAX_PIX_FOR_MESH = 4000000;
    std::vector<uint64_t> ds_ipix;   // 降采样后 ipix (若触发)
    std::vector<float> ds_pixel;     // 降采样后 pixel (若触发)
    const uint64_t* use_ipix = all.ipix;
    const float* use_pixel = all.pixel;
    uint64_t use_n_pix = all.n_pix;
    uint32_t use_nside = all.nside;

    if (all.n_pix > MAX_PIX_FOR_MESH && all.nside > 256) {
        // 计算目标 nside: 每次减半直到 n_pix 估计 < 阈值
        // n_pix ∝ nside², 降 nside 到 1/2 → n_pix 降到 1/4
        uint32_t target_nside = all.nside;
        uint64_t est_n_pix = all.n_pix;
        while (est_n_pix > MAX_PIX_FOR_MESH && target_nside > 256) {
            target_nside >>= 1;
            est_n_pix >>= 2;
        }

        // 转换为 vector 调用 HealpixMath::ud_grade
        std::vector<uint64_t> src_ipix(all.ipix, all.ipix + all.n_pix);
        std::vector<float> src_pixel(all.pixel, all.pixel + all.n_pix);

        LOG_INFO("build_hiss_polygon_mesh: 自动降采样 nside %u → %u (n_pix %llu → ~%llu)",
                 all.nside, target_nside,
                 static_cast<unsigned long long>(all.n_pix),
                 static_cast<unsigned long long>(est_n_pix));

        auto graded = HealpixMath::ud_grade(all.nside, src_ipix, src_pixel, target_nside);
        ds_ipix = std::move(graded.ipix);
        ds_pixel = std::move(graded.pixel);

        use_ipix = ds_ipix.data();
        use_pixel = ds_pixel.data();
        use_n_pix = ds_ipix.size();
        use_nside = target_nside;

        LOG_INFO("build_hiss_polygon_mesh: 降采样完成 n_pix=%llu nside=%u",
                 static_cast<unsigned long long>(use_n_pix), use_nside);
    }

    // 每像素 6 顶点（2 三角形）× 4 float (x,y,z,value) = 24 float
    std::vector<float> vertices;
    vertices.reserve(static_cast<size_t>(use_n_pix) * 6 * 4);

    // bbox 估算 (仅统计有效像素, 跳过 value<=0 的 no_data)
    double min_ra = 360.0, max_ra = 0.0, min_dec = 90.0, max_dec = -90.0;
    uint64_t skipped_no_data = 0;

    // HEALPix 像素尺寸 (用于 bbox 估算, 单位: 度)
    // 像素面积 Ω = π/(3·nside²) sr, 等面积正方形边长 a = √(π/3)/nside rad
    // → a_deg = √(π/3)/nside × 180/π ≈ 58.6/nside deg
    double pix_size = std::sqrt(M_PI / 3.0) / static_cast<double>(use_nside) * 180.0 / M_PI;
    LOG_INFO("build_hiss_polygon_mesh: n_pix=%llu nside=%u pix_size=%.6f deg",
             static_cast<unsigned long long>(use_n_pix), use_nside, pix_size);

    for (uint64_t i = 0; i < use_n_pix; ++i) {
        uint64_t ipix = use_ipix[i];
        float value = use_pixel[i];

        // 跳过 no_data 像素 (value<=0, 与片元着色器 uNoData 阈值一致)
        // 提升渲染流畅性, 避免无效多边形占用 GPU 资源
        if (value <= 0.0f) {
            ++skipped_no_data;
            continue;
        }

        // pix2ang_nest 计算像素中心
        double ra, dec;
        HealpixMath::pix2ang_nest(use_nside, ipix, ra, dec);

        // 更新 bbox (仅有效像素)
        if (ra < min_ra) min_ra = ra;
        if (ra > max_ra) max_ra = ra;
        if (dec < min_dec) min_dec = dec;
        if (dec > max_dec) max_dec = dec;

        // 球面切平面基构造菱形角点 (真实 HEALPix 菱形, 无 cos_dec 发散)
        // 中心 (ra,dec) → 笛卡尔 c + 切平面基 (east, north)
        // 4 角点在十字方向 (下/右/上/左), 投影回单位球
        double ra_rad = ra * M_PI / 180.0;
        double dec_rad = dec * M_PI / 180.0;
        double cd = std::cos(dec_rad);
        double sd = std::sin(dec_rad);
        double cr = std::cos(ra_rad);
        double sr = std::sin(ra_rad);
        // 中心笛卡尔坐标
        double cx = cd * cr;
        double cy = cd * sr;
        double cz = sd;
        // 切平面基 (单位向量, 与 c 正交)
        // east = ∂c/∂ra / |∂c/∂ra| = (-sin_ra, cos_ra, 0)
        double ex = -sr;
        double ey = cr;
        double ez = 0.0;
        // north = ∂c/∂dec / |∂c/∂dec| = (-sin_dec*cos_ra, -sin_dec*sin_ra, cos_dec)
        double nx = -sd * cr;
        double ny = -sd * sr;
        double nz = cd;
        // 菱形半对角线 (弧度)
        // HEALPix 像素面积 Ω = π/(3·nside²) sr
        // 等面积正方形边长 a = √(π/3)/nside rad
        // 菱形 (正方形旋转45°) 半对角线 h = a/√2 = √(π/6)/nside rad
        // 对角线在 north/east 方向, 相邻像素刚好拼接无重叠/无缝隙
        //
        // 小膨胀系数 1.02 (2%): 仅覆盖浮点误差导致的亚像素缝隙
        // (drizzle 加权积分无像素缝隙, 但菱形顶点投影回球面有浮点误差)
        double h = std::sqrt(M_PI / 6.0) / static_cast<double>(all.nside) * 1.02;
        // 4 角点 (十字菱形: 下→右→上→左)
        double corners_xyz[4][3] = {
            {cx - h * nx, cy - h * ny, cz - h * nz},  // 下
            {cx + h * ex, cy + h * ey, cz + h * ez},  // 右
            {cx + h * nx, cy + h * ny, cz + h * nz},  // 上
            {cx - h * ex, cy - h * ey, cz - h * ez}   // 左
        };
        // 生成 2 个三角形: 下→右→上, 下→上→左
        int tri_order[6] = {0, 1, 2, 0, 2, 3};
        for (int t = 0; t < 6; ++t) {
            int ci = tri_order[t];
            double px = corners_xyz[ci][0];
            double py = corners_xyz[ci][1];
            double pz = corners_xyz[ci][2];
            // 归一化投影回单位球
            double pl = std::sqrt(px * px + py * py + pz * pz);
            if (pl > 1e-10) { px /= pl; py /= pl; pz /= pl; }
            vertices.push_back(static_cast<float>(px));   // x
            vertices.push_back(static_cast<float>(py));   // y
            vertices.push_back(static_cast<float>(pz));   // z
            vertices.push_back(value);                    // value
        }
    }

    // 保存 bbox
    hiss_center_ra_ = (min_ra + max_ra) * 0.5;
    hiss_center_dec_ = (min_dec + max_dec) * 0.5;
    hiss_width_deg_ = max_ra - min_ra;
    hiss_height_deg_ = max_dec - min_dec;
    LOG_INFO("build_hiss_polygon_mesh: bbox center=(%.4f,%.4f) size=%.4fx%.4f deg",
             hiss_center_ra_, hiss_center_dec_, hiss_width_deg_, hiss_height_deg_);

    // 创建 VAO/VBO
    if (hiss_polygon_vao_ == 0) {
        pglGenVertexArrays(1, &hiss_polygon_vao_);
    }
    if (hiss_polygon_vbo_ == 0) {
        pglGenBuffers(1, &hiss_polygon_vbo_);
    }

    pglBindVertexArray(hiss_polygon_vao_);
    pglBindBuffer(GL_ARRAY_BUFFER, hiss_polygon_vbo_);
    pglBufferData(GL_ARRAY_BUFFER,
                  vertices.size() * sizeof(float),
                  vertices.data(),
                  GL_STATIC_DRAW);

    // 顶点属性: location=0 (vec3 position), location=1 (float value)
    // stride = 4 * sizeof(float) = 16 字节
    pglEnableVertexAttribArray(0);
    pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    pglEnableVertexAttribArray(1);
    pglVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));

    pglBindVertexArray(0);
    pglBindBuffer(GL_ARRAY_BUFFER, 0);

    hiss_polygon_vertex_count_ = static_cast<int>(vertices.size() / 4);  // 每 4 float = 1 顶点
    hiss_mesh_valid_ = true;

    LOG_INFO("build_hiss_polygon_mesh: 完成，顶点数=%d 跳过no_data=%llu/%llu",
             hiss_polygon_vertex_count_,
             static_cast<unsigned long long>(skipped_no_data),
             static_cast<unsigned long long>(all.n_pix));
    return 0;
}

// ============================================================================
// render_hiss_polygon() - 渲染 .hiss 像素多边形球面
// 复用 sphere_program_ 着色器（aPosition + aValue + STF）
// 返回 0=成功, <0=失败
// ============================================================================

int GLRenderer::render_hiss_polygon(BrowserBackend& backend, const RenderParams& params) {
    // 首次调用: 构建网格
    if (!hiss_mesh_valid_) {
        if (build_hiss_polygon_mesh(backend) != 0) {
            return -1;
        }
    }

    // 设置视口（render() 已设置，这里冗余但安全）
    glViewport(0, 0, params.viewport_w, params.viewport_h);

    // 球心相机: 相机在球心(0,0,0)，向外看
    // FOV 由 params.view.fov_deg 直接传入（SphereView 控制）
    // FOV 范围保护: [0.5°, 170°], 防止 tan(fov/2) 退化 (fov=180° → tan(90°)=∞ → 黑屏)
    double fov_deg = params.view.fov_deg;
    if (fov_deg < 0.01) fov_deg = 60.0;    // 下限兜底 (无效值用缺省)
    if (fov_deg > 170.0) fov_deg = 170.0; // 上限保护 (全天不超过 170°)

    // 相机朝向: 用 widget 层传入的 forward (双向量四元数导航, 自由滚动)
    // 不再从 ra/dec 重算, 直接使用 forward_*
    double fx = params.view.forward_x;
    double fy = params.view.forward_y;
    double fz = params.view.forward_z;
    double f_len = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (f_len < 1e-10) {
        fx = 1.0; fy = 0.0; fz = 0.0;  // 兜底
    } else {
        fx /= f_len; fy /= f_len; fz /= f_len;
    }

    // 相机位置 = 球心 (0, 0, 0)
    double ex = 0.0, ey = 0.0, ez = 0.0;
    // 看向目标 = 球心 + forward
    double cx = fx, cy = fy, cz = fz;

    // up 向量: 携带式 (由 widget 层维护, 自由滚动模式, 非 north-up)
    // 直接用 params.view.up_*, 不再重算, 消除极区旋转感
    double up_x = params.view.up_x;
    double up_y = params.view.up_y;
    double up_z = params.view.up_z;
    // 归一化 (防止传入非单位向量)
    double up_len = std::sqrt(up_x * up_x + up_y * up_y + up_z * up_z);
    if (up_len < 1e-10) {
        up_x = 0.0; up_y = 0.0; up_z = 1.0;  // 兜底
    } else {
        up_x /= up_len; up_y /= up_len; up_z /= up_len;
    }

    float mvp[16];
    perspective_matrix(fov_deg, static_cast<double>(params.viewport_w) / params.viewport_h,
                       0.01, 100.0, mvp);
    float view_mat[16];
    look_at_matrix(ex, ey, ez, cx, cy, cz, up_x, up_y, up_z, view_mat);
    float final_mvp[16];
    multiply_matrix(mvp, view_mat, final_mvp);

    // 设置 STF uniform
    STFEngine::GPUUniforms u = STFEngine::to_uniforms(
        params.stf, params.data_min, params.data_max, params.no_data_value);

    pglUseProgram(sphere_program_);

    GLint loc_mvp = pglGetUniformLocation(sphere_program_, "uMVPMatrix");
    pglUniformMatrix4fv(loc_mvp, 1, GL_FALSE, final_mvp);

    GLint loc_shadows = pglGetUniformLocation(sphere_program_, "uShadows");
    GLint loc_highlights = pglGetUniformLocation(sphere_program_, "uHighlights");
    GLint loc_midtones = pglGetUniformLocation(sphere_program_, "uMidtones");
    GLint loc_nodata = pglGetUniformLocation(sphere_program_, "uNoData");
    GLint loc_compression = pglGetUniformLocation(sphere_program_, "uCompression");

    pglUniform1f(loc_shadows, u.shadows);
    pglUniform1f(loc_highlights, u.highlights);
    pglUniform1f(loc_midtones, u.midtones);
    pglUniform1f(loc_nodata, u.no_data);
    pglUniform1f(loc_compression, u.compression);

    // 绘制
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);  // 禁用背面剔除，球心相机内外都能看到
    pglBindVertexArray(hiss_polygon_vao_);
    glDrawArrays(GL_TRIANGLES, 0, hiss_polygon_vertex_count_);
    pglBindVertexArray(0);

    LOG_DEBUG("render_hiss_polygon: 绘制 %d 顶点 fov=%.3f ra=%.4f dec=%.4f",
              hiss_polygon_vertex_count_, fov_deg,
              params.view.center_ra, params.view.center_dec);
    return 0;
}

// ============================================================================
// render_single_frame() - 单帧切面投影渲染（.hiss 模式）
// 使用 TAN (gnomonic) 投影将 HEALPix 数据投影到 1024×1024 R32F 纹理
// ============================================================================

int GLRenderer::render_single_frame(BrowserBackend& backend, const RenderParams& params) {
    const ViewParams& view = params.view;

    // ---- 1. 获取全量数据（.hiss 模式） ----
    LeafData all_data = backend.get_all_data();
    if (all_data.n_pix == 0 || all_data.pixel == nullptr) {
        LOG_ERROR("render_single_frame: 无数据");
        return -1;
    }

    sf_nside_ = all_data.nside;

    // ---- 2. 边界框检测/估算 ----
    if (sf_width_deg_ <= 0.0 || sf_height_deg_ <= 0.0) {
        // 未设置 bbox，采样 1000 像素估算
        LOG_INFO("render_single_frame: 未设置 bbox，从数据采样估算");
        double min_ra = 360.0, max_ra = 0.0, min_dec = 90.0, max_dec = -90.0;
        uint64_t sample_step = all_data.n_pix / 1000;
        if (sample_step == 0) sample_step = 1;
        uint64_t sampled = 0;
        for (uint64_t i = 0; i < all_data.n_pix; i += sample_step) {
            double ra, dec;
            HealpixMath::pix2ang_nest(all_data.nside, all_data.ipix[i], ra, dec);
            if (ra < min_ra) min_ra = ra;
            if (ra > max_ra) max_ra = ra;
            if (dec < min_dec) min_dec = dec;
            if (dec > max_dec) max_dec = dec;
            sampled++;
        }
        sf_center_ra_ = (min_ra + max_ra) / 2.0;
        sf_center_dec_ = (min_dec + max_dec) / 2.0;
        sf_width_deg_ = max_ra - min_ra;
        sf_height_deg_ = max_dec - min_dec;
        if (sf_width_deg_ <= 0) sf_width_deg_ = 1.0;
        if (sf_height_deg_ <= 0) sf_height_deg_ = 1.0;
        sf_texture_valid_ = false;
        LOG_INFO("render_single_frame: 估算 bbox center=(%.4f,%.4f) size=%.4fx%.4f sampled=%llu",
                 sf_center_ra_, sf_center_dec_, sf_width_deg_, sf_height_deg_,
                 (unsigned long long)sampled);
    }

    // ---- 3. 视角变化检测 ----
    // 比较 view 与缓存，变化则重建纹理
    bool view_changed = (std::fabs(view.center_ra - sf_last_view_ra_) > 1e-6 ||
                         std::fabs(view.center_dec - sf_last_view_dec_) > 1e-6 ||
                         std::fabs(view.zoom - sf_last_view_zoom_) > 1e-6);
    if (view_changed || !sf_texture_valid_) {
        // 需要重建纹理
        // 注: 当前实现用 bbox 中心作为投影中心，视角变化时调整投影中心
        // 为简化，此处用 view.center 作为投影中心（若未设置则用 bbox 中心）
        double proj_ra = (view.center_ra != 0.0 || view.center_dec != 0.0) ?
                         view.center_ra : sf_center_ra_;
        double proj_dec = (view.center_ra != 0.0 || view.center_dec != 0.0) ?
                          view.center_dec : sf_center_dec_;

        // 构建 ipix→value 查找表
        std::unordered_map<uint64_t, float> ipix_map;
        ipix_map.reserve(all_data.n_pix);
        for (uint64_t i = 0; i < all_data.n_pix; i++) {
            ipix_map[all_data.ipix[i]] = all_data.pixel[i];
        }

        // ---- 4. 构建 1024×1024 R32F 纹理 ----
        const int TEX_SIZE = 1024;
        std::vector<float> tex_data(TEX_SIZE * TEX_SIZE, params.no_data_value);

        double ra0 = proj_ra * M_PI / 180.0;
        double dec0 = proj_dec * M_PI / 180.0;
        double cos_dec0 = std::cos(dec0);
        double sin_dec0 = std::sin(dec0);

        // 视场大小（考虑 zoom）
        double view_width = sf_width_deg_ / (view.zoom > 0.001 ? view.zoom : 0.001);
        double view_height = sf_height_deg_ / (view.zoom > 0.001 ? view.zoom : 0.001);
        double half_w_rad = view_width * M_PI / 360.0;   // 半宽弧度
        double half_h_rad = view_height * M_PI / 360.0;   // 半高弧度

        for (int j = 0; j < TEX_SIZE; j++) {
            // y 方向: 上→下 对应 +half_h → -half_h
            double y = half_h_rad * (1.0 - 2.0 * (double)j / (TEX_SIZE - 1));
            for (int i = 0; i < TEX_SIZE; i++) {
                // x 方向: 左→右 对应 -half_w → +half_w
                double x = half_w_rad * (2.0 * (double)i / (TEX_SIZE - 1) - 1.0);

                // TAN (gnomonic) 逆投影: (x, y) → (ra, dec)
                double rho = std::sqrt(x * x + y * y);
                double c = std::atan(rho);

                double sin_c = std::sin(c);
                double cos_c = std::cos(c);

                double dec, ra;
                if (rho < 1e-12) {
                    dec = proj_dec;
                    ra = proj_ra;
                } else {
                    dec = std::asin(cos_c * sin_dec0 + (y * sin_c * cos_dec0) / rho);
                    ra = ra0 + std::atan2(x * sin_c,
                                          rho * cos_dec0 * cos_c - y * sin_dec0 * sin_c);
                }

                // 转度
                double ra_deg = ra * 180.0 / M_PI;
                double dec_deg = dec * 180.0 / M_PI;
                // 归一化 ra 到 [0, 360)
                while (ra_deg < 0.0) ra_deg += 360.0;
                while (ra_deg >= 360.0) ra_deg -= 360.0;

                // HEALPix 查值
                uint64_t ipix = HealpixMath::ang2pix_nest(sf_nside_, ra_deg, dec_deg);
                auto it = ipix_map.find(ipix);
                if (it != ipix_map.end()) {
                    tex_data[j * TEX_SIZE + i] = it->second;
                } else {
                    tex_data[j * TEX_SIZE + i] = params.no_data_value;
                }
            }
        }

        // 上传纹理
        if (single_frame_texture_ == 0) {
            glGenTextures(1, &single_frame_texture_);
        }
        glBindTexture(GL_TEXTURE_2D, single_frame_texture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, TEX_SIZE, TEX_SIZE, 0,
                     GL_RED, GL_FLOAT, tex_data.data());
        glBindTexture(GL_TEXTURE_2D, 0);

        sf_texture_valid_ = true;
        sf_last_view_ra_ = view.center_ra;
        sf_last_view_dec_ = view.center_dec;
        sf_last_view_zoom_ = view.zoom;

        LOG_INFO("render_single_frame: 纹理重建 %dx%d (proj=%.4f,%.4f zoom=%.3f)",
                 TEX_SIZE, TEX_SIZE, proj_ra, proj_dec, view.zoom);
    }

    // ---- 5. 绘制全屏四边形 ----
    glDisable(GL_DEPTH_TEST);

    pglUseProgram(quad_program_);

    // 绑定纹理
    pglActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, single_frame_texture_);
    GLint tex_loc = pglGetUniformLocation(quad_program_, "uTexture");
    if (tex_loc >= 0) pglUniform1i(tex_loc, 0);

    // 设置 STF uniform
    STFEngine::GPUUniforms u = STFEngine::to_uniforms(
        params.stf, params.data_min, params.data_max, params.no_data_value);

    GLint loc_shadows = pglGetUniformLocation(quad_program_, "uShadows");
    GLint loc_highlights = pglGetUniformLocation(quad_program_, "uHighlights");
    GLint loc_midtones = pglGetUniformLocation(quad_program_, "uMidtones");
    GLint loc_nodata = pglGetUniformLocation(quad_program_, "uNoData");
    GLint loc_compression = pglGetUniformLocation(quad_program_, "uCompression");

    if (loc_shadows >= 0)     pglUniform1f(loc_shadows, u.shadows);
    if (loc_highlights >= 0)  pglUniform1f(loc_highlights, u.highlights);
    if (loc_midtones >= 0)    pglUniform1f(loc_midtones, u.midtones);
    if (loc_nodata >= 0)      pglUniform1f(loc_nodata, u.no_data);
    if (loc_compression >= 0) pglUniform1f(loc_compression, u.compression);

    // 绘制（TRIANGLE_FAN: 4 顶点 → 2 三角形 → 全屏四边形）
    pglBindVertexArray(quad_vao_);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    pglBindVertexArray(0);

    LOG_DEBUG("render_single_frame: 渲染完成");
    return 0;
}

// ============================================================================
// 矩阵运算（4×4, column-major）
// ============================================================================

// 透视投影矩阵（column-major）
// m[0] = f/aspect
// m[5] = f
// m[10] = (far+near)/(near-far)
// m[11] = -1
// m[14] = 2*far*near/(near-far)
// 其余 = 0
void GLRenderer::perspective_matrix(double fov_deg, double aspect,
                                     double near_val, double far_val, float* m) {
    // 初始化为零
    for (int i = 0; i < 16; i++) m[i] = 0.0f;

    double fov_rad = fov_deg * M_PI / 180.0;
    double f = 1.0 / std::tan(fov_rad / 2.0);

    m[0]  = (float)(f / aspect);                    // column 0, row 0
    m[5]  = (float)f;                                // column 1, row 1
    m[10] = (float)((far_val + near_val) / (near_val - far_val));  // column 2, row 2
    m[11] = -1.0f;                                   // column 2, row 3
    m[14] = (float)((2.0 * far_val * near_val) / (near_val - far_val));  // column 3, row 2
}

// look-at 矩阵（column-major, 与 perspective_matrix 一致）
// forward = normalize(center - eye)
// side = normalize(forward × up)
// u' = side × forward (true up)
//
// View 矩阵 (row-major 概念):
// | side.x side.y side.z -dot(side,eye) |
// | u'.x u'.y u'.z -dot(u',eye) |
// | -fwd.x -fwd.y -fwd.z dot(fwd,eye) |
// | 0 0 0 1 |
//
// Column-major 存储: col_i = V 的第 i 列
// col0 = (side.x, u'.x, -fwd.x, 0)
// col1 = (side.y, u'.y, -fwd.y, 0)
// col2 = (side.z, u'.z, -fwd.z, 0)
// col3 = (-dot(s,e), -dot(u,e), dot(f,e), 1)
void GLRenderer::look_at_matrix(double eye_x, double eye_y, double eye_z,
                                 double center_x, double center_y, double center_z,
                                 double up_x, double up_y, double up_z, float* m) {
    // forward = center - eye
    double fx = center_x - eye_x;
    double fy = center_y - eye_y;
    double fz = center_z - eye_z;
    double fl = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (fl < 1e-10) fl = 1e-10;
    fx /= fl; fy /= fl; fz /= fl;

    // side = forward × up
    double sx = fy * up_z - fz * up_y;
    double sy = fz * up_x - fx * up_z;
    double sz = fx * up_y - fy * up_x;
    double sl = std::sqrt(sx * sx + sy * sy + sz * sz);
    if (sl < 1e-10) sl = 1e-10;
    sx /= sl; sy /= sl; sz /= sl;

    // u' = side × forward
    double ux = sy * fz - sz * fy;
    double uy = sz * fx - sx * fz;
    double uz = sx * fy - sy * fx;

    // column-major 存储 (与 perspective_matrix 一致)
    // Column 0: [side.x, u'.x, -fwd.x, 0]
    m[0]  = (float)sx;
    m[1]  = (float)ux;
    m[2]  = (float)(-fx);
    m[3]  = 0.0f;

    // Column 1: [side.y, u'.y, -fwd.y, 0]
    m[4]  = (float)sy;
    m[5]  = (float)uy;
    m[6]  = (float)(-fy);
    m[7]  = 0.0f;

    // Column 2: [side.z, u'.z, -fwd.z, 0]
    m[8]  = (float)sz;
    m[9]  = (float)uz;
    m[10] = (float)(-fz);
    m[11] = 0.0f;

    // Column 3: [-dot(side,eye), -dot(u',eye), dot(fwd,eye), 1]
    m[12] = (float)(-(sx * eye_x + sy * eye_y + sz * eye_z));
    m[13] = (float)(-(ux * eye_x + uy * eye_y + uz * eye_z));
    m[14] = (float)(fx * eye_x + fy * eye_y + fz * eye_z);
    m[15] = 1.0f;
}

// 4×4 矩阵乘法（column-major）
// out = a * b （先应用 b，再应用 a）
// out[col*4 + row] = sum_k a[k*4 + row] * b[col*4 + k]
void GLRenderer::multiply_matrix(const float* a, const float* b, float* out) {
    float result[16];
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                // a 是 column-major: a[k*4 + row] = a(row, k)
                // b 是 column-major: b[col*4 + k] = b(k, col)
                sum += a[k * 4 + row] * b[col * 4 + k];
            }
            result[col * 4 + row] = sum;
        }
    }
    // 复制到 out（避免 a 或 b 与 out 相同时的覆盖问题）
    for (int i = 0; i < 16; i++) out[i] = result[i];
}

// ============================================================================
// build_grid_mesh() - 构建经纬线网格 (30° 网格)
// RA 线: RA=0,30,60,...,330 (12条), 每条 Dec 从 -90° 到 90°, 采样 180 段
// Dec 线: Dec=-60,-30,0,30,60 (5条), 每条 RA 从 0° 到 360°, 采样 360 段
// 顶点格式: vec3 position (球面笛卡尔坐标, 单位球)
// 用 GL_LINES 绘制
// ============================================================================

int GLRenderer::build_grid_mesh() {
    LOG_INFO("build_grid_mesh: 构建 30° 经纬线网格");

    std::vector<float> vertices;
    const int RA_SEGMENTS = 180;  // 每条 RA 线采样段数 (Dec -90→90)
    const int DEC_SEGMENTS = 360; // 每条 Dec 线采样段数 (RA 0→360)
    const double DEG2RAD = M_PI / 180.0;

    // RA 线 (经线): RA=0,30,60,...,330
    for (int ra_i = 0; ra_i < 12; ++ra_i) {
        double ra = ra_i * 30.0;
        double ra_rad = ra * DEG2RAD;
        for (int s = 0; s < RA_SEGMENTS; ++s) {
            // 线段: (dec_s, dec_s+1)
            double dec1 = -90.0 + 180.0 * s / RA_SEGMENTS;
            double dec2 = -90.0 + 180.0 * (s + 1) / RA_SEGMENTS;
            double d1_rad = dec1 * DEG2RAD;
            double d2_rad = dec2 * DEG2RAD;

            // 顶点 1
            vertices.push_back((float)(std::cos(d1_rad) * std::cos(ra_rad)));
            vertices.push_back((float)(std::cos(d1_rad) * std::sin(ra_rad)));
            vertices.push_back((float)(std::sin(d1_rad)));
            // 顶点 2
            vertices.push_back((float)(std::cos(d2_rad) * std::cos(ra_rad)));
            vertices.push_back((float)(std::cos(d2_rad) * std::sin(ra_rad)));
            vertices.push_back((float)(std::sin(d2_rad)));
        }
    }

    // Dec 线 (纬线): Dec=-60,-30,0,30,60
    for (int dec_i = -2; dec_i <= 2; ++dec_i) {
        double dec = dec_i * 30.0;
        double dec_rad = dec * DEG2RAD;
        for (int s = 0; s < DEC_SEGMENTS; ++s) {
            // 线段: (ra_s, ra_s+1)
            double ra1 = 360.0 * s / DEC_SEGMENTS;
            double ra2 = 360.0 * (s + 1) / DEC_SEGMENTS;
            double r1_rad = ra1 * DEG2RAD;
            double r2_rad = ra2 * DEG2RAD;

            // 顶点 1
            vertices.push_back((float)(std::cos(dec_rad) * std::cos(r1_rad)));
            vertices.push_back((float)(std::cos(dec_rad) * std::sin(r1_rad)));
            vertices.push_back((float)(std::sin(dec_rad)));
            // 顶点 2
            vertices.push_back((float)(std::cos(dec_rad) * std::cos(r2_rad)));
            vertices.push_back((float)(std::cos(dec_rad) * std::sin(r2_rad)));
            vertices.push_back((float)(std::sin(dec_rad)));
        }
    }

    grid_vertex_count_ = static_cast<int>(vertices.size() / 3);

    // 创建 VAO / VBO
    if (grid_vao_ == 0) pglGenVertexArrays(1, &grid_vao_);
    if (grid_vbo_ == 0) pglGenBuffers(1, &grid_vbo_);

    pglBindVertexArray(grid_vao_);
    pglBindBuffer(GL_ARRAY_BUFFER, grid_vbo_);
    pglBufferData(GL_ARRAY_BUFFER,
                  vertices.size() * sizeof(float),
                  vertices.data(),
                  GL_STATIC_DRAW);

    // 顶点属性: location=0 (vec3 position)
    pglEnableVertexAttribArray(0);
    pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    pglBindVertexArray(0);
    pglBindBuffer(GL_ARRAY_BUFFER, 0);

    grid_mesh_valid_ = true;
    LOG_INFO("build_grid_mesh: 完成，顶点数=%d (RA线12条 + Dec线5条)", grid_vertex_count_);
    return 0;
}

// ============================================================================
// render_grid() - 渲染经纬线网格
// 复用 render_hiss_polygon 的球心相机 MVP 矩阵
// 用 grid_program_ 着色器 (固定半透明绿色)
// GL_LINES 绘制
// ============================================================================

int GLRenderer::render_grid(const RenderParams& params) {
    if (!grid_mesh_valid_) {
        if (build_grid_mesh() != 0) {
            LOG_ERROR("render_grid: 网格构建失败");
            return -1;
        }
    }

    // 球心相机 MVP (复用 render_hiss_polygon 逻辑, 用传入 forward)
    double fov_deg = params.view.fov_deg;
    if (fov_deg < 0.01) fov_deg = 60.0;
    if (fov_deg > 170.0) fov_deg = 170.0;

    double fx = params.view.forward_x;
    double fy = params.view.forward_y;
    double fz = params.view.forward_z;
    double f_len = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (f_len < 1e-10) {
        fx = 1.0; fy = 0.0; fz = 0.0;
    } else {
        fx /= f_len; fy /= f_len; fz /= f_len;
    }

    double ex = 0.0, ey = 0.0, ez = 0.0;
    double cx = fx, cy = fy, cz = fz;

    // up 向量: 携带式 (由 widget 层维护, 自由滚动模式)
    double up_x = params.view.up_x;
    double up_y = params.view.up_y;
    double up_z = params.view.up_z;
    double up_len = std::sqrt(up_x * up_x + up_y * up_y + up_z * up_z);
    if (up_len < 1e-10) {
        up_x = 0.0; up_y = 0.0; up_z = 1.0;  // 兜底
    } else {
        up_x /= up_len; up_y /= up_len; up_z /= up_len;
    }

    float proj_mat[16];
    perspective_matrix(fov_deg, static_cast<double>(params.viewport_w) / params.viewport_h,
                       0.01, 100.0, proj_mat);
    float view_mat[16];
    look_at_matrix(ex, ey, ez, cx, cy, cz, up_x, up_y, up_z, view_mat);
    float final_mvp[16];
    multiply_matrix(proj_mat, view_mat, final_mvp);

    // 绘制网格
    pglUseProgram(grid_program_);

    GLint loc_mvp = pglGetUniformLocation(grid_program_, "uMVPMatrix");
    if (loc_mvp >= 0) pglUniformMatrix4fv(loc_mvp, 1, GL_FALSE, final_mvp);

    GLint loc_color = pglGetUniformLocation(grid_program_, "uGridColor");
    if (loc_color >= 0) pglUniform4f(loc_color, 0.0f, 1.0f, 0.0f, 0.3f);  // 半透明绿色

    // 启用混合 (半透明)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    pglBindVertexArray(grid_vao_);
    glDrawArrays(GL_LINES, 0, grid_vertex_count_);
    pglBindVertexArray(0);

    glDisable(GL_BLEND);

    LOG_DEBUG("render_grid: 绘制 %d 顶点 (GL_LINES)", grid_vertex_count_);
    return 0;
}
