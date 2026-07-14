// gl_renderer.cpp - HEALPix 浏览器 OpenGL 渲染核心实现 (healpix_browser_qt)
// 功能: 球面渲染 (.hcsd) + 单帧切面投影渲染 (.hiss), 内嵌 STF 拉伸着色器
// 用途: 为 widgets/ 层提供纯 C++ OpenGL 3.3 Core 渲染入口, 无 Qt 依赖
// 依赖: browser_backend.h (数据源), stf_engine.h (STF uniform 转换),
//       healpix_math.h (球面坐标转换), logger.h (日志)
//       OpenGL 3.3 Core (wglGetProcAddress 加载 1.2+ 函数, opengl32.lib 链接 1.1 函数)
// 编译: g++ -O2 -std=c++17 -Wall -Wextra -Icore -Iinclude -I../healpix_io/include
//       -c core/gl_renderer.cpp -o core/gl_renderer.o -lopengl32 -lgdi32
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
    //     (wglGetProcAddress 返回 PROC，与各 GL 函数指针类型不兼容)
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

    // 3. 构建球面网格（64×128 分段）
    build_sphere_mesh(64, 128);

    // 4. 构建单帧四边形网格
    build_quad_mesh();

    // 5. 初始化单帧纹理 ID
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
    if (params.mode == RenderMode::SPHERE) {
        return render_sphere(backend, params);
    } else if (params.mode == RenderMode::HISS_POLYGON) {
        return render_hiss_polygon(backend, params);
    } else {
        // 旧 SINGLE_FRAME 已废弃，回退到 hiss_polygon
        LOG_WARN("render: 未知 RenderMode=%d，回退到 HISS_POLYGON", static_cast<int>(params.mode));
        return render_hiss_polygon(backend, params);
    }
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
            sphere_vertex_coords_.push_back({ra, dec});

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
// build_quad_mesh() - 构建全屏四边形网格
// 顶点格式: [x, y, u, v] (4 float)
// 四边形 (-1,-1) → (1,1), texcoord (0,0) → (1,1)
// ============================================================================

void GLRenderer::build_quad_mesh() {
    LOG_INFO("build_quad_mesh: 构建全屏四边形");

    // 4 顶点: position(xy) + texcoord(uv)
    // TRIANGLE_FAN 顺序: 左下 → 右下 → 右上 → 左上
    // 注: OpenGL 纹理 v=0 在底部，但渲染时通常 v=0 在顶部
    //       这里 texcoord (0,0) 对应左下角
    float vertices[] = {
        // x      y     u     v
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

    // ---- 1. 计算相机位置 ----
    // zoom=1.0 → 相机距球心 3.0（全天视角）
    // zoom 越大 → 相机越靠近球面（放大）
    double cam_distance = 3.0 / (view.zoom > 0.001 ? view.zoom : 0.001);
    if (cam_distance < 1.01) cam_distance = 1.01;  // 不穿入球内

    // 相机目标点：球面上 (center_ra, center_dec) 处
    double center_ra_rad = view.center_ra * M_PI / 180.0;
    double center_dec_rad = view.center_dec * M_PI / 180.0;
    double target_x = std::cos(center_dec_rad) * std::cos(center_ra_rad);
    double target_y = std::cos(center_dec_rad) * std::sin(center_ra_rad);
    double target_z = std::sin(center_dec_rad);

    // 相机位置：沿目标点方向后退
    double eye_x = target_x * cam_distance;
    double eye_y = target_y * cam_distance;
    double eye_z = target_z * cam_distance;

    // up 向量：近似 (0, 0, 1)，若几乎平行则用 (0, 1, 0)
    double up_x = 0.0, up_y = 0.0, up_z = 1.0;
    // 检查 view 方向与 up 是否平行
    double dir_x = target_x - eye_x;
    double dir_y = target_y - eye_y;
    double dir_z = target_z - eye_z;
    double dir_len = std::sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);
    if (dir_len > 1e-10) {
        double dot_up = (dir_x * up_x + dir_y * up_y + dir_z * up_z) / dir_len;
        if (std::fabs(dot_up) > 0.99) {
            // 近似平行，改用 Y 轴
            up_x = 0.0; up_y = 1.0; up_z = 0.0;
        }
    }

    // ---- 2. 计算投影/视图矩阵 ----
    float proj_mat[16], view_mat[16], mvp_mat[16];
    double aspect = (double)params.viewport_w / (double)params.viewport_h;
    double fov = view.fov_deg * M_PI / 180.0;
    perspective_matrix(fov, aspect, 0.01, 100.0, proj_mat);
    look_at_matrix(eye_x, eye_y, eye_z,
                   target_x, target_y, target_z,
                   up_x, up_y, up_z, view_mat);
    multiply_matrix(proj_mat, view_mat, mvp_mat);

    // ---- 3. 加载子叶数据，构建 ipix→value 查找表 ----
    // 按 leaf_ipix 分组，每个子叶有自己的 nside 和 ipix→value 映射
    struct LeafMap {
        uint32_t nside;
        std::unordered_map<uint64_t, float> ipix_to_value;
    };
    std::unordered_map<uint64_t, LeafMap> leaf_maps;  // key: leaf_ipix (nside=64)

    std::vector<uint64_t> required = backend.get_required_leaves(view);
    LOG_DEBUG("render_sphere: 需要子叶 %zu 个", required.size());

    for (uint64_t leaf_ipix : required) {
        uint32_t target_nside = backend.decide_target_nside(view, leaf_ipix);
        LeafData leaf = backend.load_leaf(leaf_ipix, target_nside);

        if (leaf.n_pix == 0 || leaf.pixel == nullptr) {
            backend.release_leaf(leaf);
            continue;
        }

        LeafMap lm;
        lm.nside = leaf.nside;
        lm.ipix_to_value.reserve(leaf.n_pix);
        for (uint64_t i = 0; i < leaf.n_pix; i++) {
            lm.ipix_to_value[leaf.ipix[i]] = leaf.pixel[i];
        }
        leaf_maps[leaf_ipix] = std::move(lm);

        backend.release_leaf(leaf);
    }

    // ---- 4. 每顶点查值，更新 VBO ----
    // 顶点格式: [x, y, z, value]
    int num_verts = (int)sphere_vertex_coords_.size();
    std::vector<float> vertex_data(num_verts * 4, 0.0f);

    for (int i = 0; i < num_verts; i++) {
        double ra = sphere_vertex_coords_[i].ra;
        double dec = sphere_vertex_coords_[i].dec;

        // 笛卡尔坐标（单位球）
        double ra_rad = ra * M_PI / 180.0;
        double dec_rad = dec * M_PI / 180.0;
        vertex_data[i * 4 + 0] = (float)(std::cos(dec_rad) * std::cos(ra_rad));
        vertex_data[i * 4 + 1] = (float)(std::cos(dec_rad) * std::sin(ra_rad));
        vertex_data[i * 4 + 2] = (float)(std::sin(dec_rad));

        // 查值: 先找 nside=64 子叶，再在子叶的 nside 层查 ipix
        float value = params.no_data_value;
        uint64_t leaf_ipix = HealpixMath::ang2pix_nest(64, ra, dec);
        auto it = leaf_maps.find(leaf_ipix);
        if (it != leaf_maps.end()) {
            const LeafMap& lm = it->second;
            uint64_t ipix_fine = HealpixMath::ang2pix_nest(lm.nside, ra, dec);
            auto vit = lm.ipix_to_value.find(ipix_fine);
            if (vit != lm.ipix_to_value.end()) {
                value = vit->second;
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
    glEnable(GL_DEPTH_TEST);

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

    LOG_DEBUG("render_sphere: 渲染完成 (子叶=%zu 顶点=%d)",
              leaf_maps.size(), num_verts);
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

    // 每像素 6 顶点（2 三角形）× 4 float (x,y,z,value) = 24 float
    std::vector<float> vertices;
    vertices.reserve(static_cast<size_t>(all.n_pix) * 6 * 4);

    // bbox 估算
    double min_ra = 360.0, max_ra = 0.0, min_dec = 90.0, max_dec = -90.0;

    // HEALPix 像素边长（度）: 近似公式
    double pix_size = 4.0 * 180.0 / (3.0 * static_cast<double>(all.nside));
    LOG_INFO("build_hiss_polygon_mesh: n_pix=%llu nside=%u pix_size=%.6f deg",
             static_cast<unsigned long long>(all.n_pix), all.nside, pix_size);

    for (uint64_t i = 0; i < all.n_pix; ++i) {
        uint64_t ipix = all.ipix[i];
        float value = all.pixel[i];

        // pix2ang_nest 计算像素中心
        double ra, dec;
        HealpixMath::pix2ang_nest(all.nside, ipix, ra, dec);

        // 更新 bbox
        if (ra < min_ra) min_ra = ra;
        if (ra > max_ra) max_ra = ra;
        if (dec < min_dec) min_dec = dec;
        if (dec > max_dec) max_dec = dec;

        // 像素角点（近似: 中心 ± pix_size/2，RA 方向除以 cos(dec)）
        double cos_dec = std::cos(dec * M_PI / 180.0);
        if (std::fabs(cos_dec) < 1e-10) cos_dec = 1e-10;  // 极区兜底
        double d_ra = pix_size / cos_dec;
        double d_dec = pix_size;

        // 4 个角点 (ra, dec)
        double corners[4][2] = {
            {ra - d_ra * 0.5, dec - d_dec * 0.5},  // 左下
            {ra + d_ra * 0.5, dec - d_dec * 0.5},  // 右下
            {ra + d_ra * 0.5, dec + d_dec * 0.5},  // 右上
            {ra - d_ra * 0.5, dec + d_dec * 0.5}   // 左上
        };

        // 转球面坐标 (x, y, z) 并生成 2 个三角形
        // 三角形 1: 角点0, 角点1, 角点2
        // 三角形 2: 角点0, 角点2, 角点3
        int tri_order[6] = {0, 1, 2, 0, 2, 3};
        for (int t = 0; t < 6; ++t) {
            int c = tri_order[t];
            double r_rad = corners[c][0] * M_PI / 180.0;
            double d_rad = corners[c][1] * M_PI / 180.0;
            vertices.push_back(static_cast<float>(std::cos(d_rad) * std::cos(r_rad)));  // x
            vertices.push_back(static_cast<float>(std::cos(d_rad) * std::sin(r_rad)));  // y
            vertices.push_back(static_cast<float>(std::sin(d_rad)));                     // z
            vertices.push_back(value);                                                   // value
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

    LOG_INFO("build_hiss_polygon_mesh: 完成，顶点数=%d", hiss_polygon_vertex_count_);
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

    // 计算 MVP 矩阵（复用球面视角逻辑）
    double zoom = params.view.zoom > 0.001 ? params.view.zoom : 0.001;
    // 固定相机距离 3.0（球面半径=1，安全距离）
    // 用 FOV 控制缩放: FOV = 60/zoom（zoom=1→FOV=60°全天，zoom=13→FOV=4.5°放大）
    double distance = 3.0;
    double fov_deg = 60.0 / zoom;

    // 相机看向数据中心
    double center_ra_rad = params.view.center_ra * M_PI / 180.0;
    double center_dec_rad = params.view.center_dec * M_PI / 180.0;
    double cx = std::cos(center_dec_rad) * std::cos(center_ra_rad);
    double cy = std::cos(center_dec_rad) * std::sin(center_ra_rad);
    double cz = std::sin(center_dec_rad);

    // 相机位置: 沿中心方向后退 distance
    double ex = cx * distance;
    double ey = cy * distance;
    double ez = cz * distance;

    // up 向量: 简化为 (0, 0, 1)
    double up_x = 0.0, up_y = 0.0, up_z = 1.0;

    float mvp[16];
    perspective_matrix(fov_deg, static_cast<double>(params.viewport_w) / params.viewport_h,
                       0.1, 100.0, mvp);
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
    pglBindVertexArray(hiss_polygon_vao_);
    glDrawArrays(GL_TRIANGLES, 0, hiss_polygon_vertex_count_);
    pglBindVertexArray(0);

    LOG_DEBUG("render_hiss_polygon: 绘制 %d 顶点 zoom=%.3f", hiss_polygon_vertex_count_, zoom);
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
// m[0]  = f/aspect
// m[5]  = f
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

// look-at 矩阵（column-major）
// forward = normalize(center - eye)
// side = normalize(forward × up)
// u' = side × forward  (true up)
//
// | side.x   u'.x   -fwd.x   0 |
// | side.y   u'.y   -fwd.y   0 |
// | side.z   u'.z   -fwd.z   0 |
// | -s·eye   -u'·eye fwd·eye 1 |
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

    // column-major 存储
    // Column 0: [side.x, side.y, side.z, -dot(side, eye)]
    m[0]  = (float)sx;
    m[1]  = (float)sy;
    m[2]  = (float)sz;
    m[3]  = (float)(-(sx * eye_x + sy * eye_y + sz * eye_z));

    // Column 1: [u'.x, u'.y, u'.z, -dot(u', eye)]
    m[4]  = (float)ux;
    m[5]  = (float)uy;
    m[6]  = (float)uz;
    m[7]  = (float)(-(ux * eye_x + uy * eye_y + uz * eye_z));

    // Column 2: [-fwd.x, -fwd.y, -fwd.z, dot(fwd, eye)]
    m[8]  = (float)(-fx);
    m[9]  = (float)(-fy);
    m[10] = (float)(-fz);
    m[11] = (float)(fx * eye_x + fy * eye_y + fz * eye_z);

    // Column 3: [0, 0, 0, 1]
    m[12] = 0.0f;
    m[13] = 0.0f;
    m[14] = 0.0f;
    m[15] = 1.0f;
}

// 4×4 矩阵乘法（column-major）
// out = a * b  （先应用 b，再应用 a）
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
