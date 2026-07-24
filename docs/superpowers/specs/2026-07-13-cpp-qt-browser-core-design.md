# C++ Qt 浏览器 - 核心算法库设计

> **日期**：2026-07-13
> **状态**：设计阶段，待实现
> **范围**：本文档仅覆盖**核心算法库（core/）**的设计。UI 前端（widgets/ + app/）见独立文档 `2026-07-13-cpp-qt-browser-ui-design.md`。
> **原则**：UI 与算法严格分离。算法是后端核心（纯 C++17 + OpenGL，无 Qt 依赖），UI 是前端（Qt6 widget 封装）。

---

## 1. 背景与目标

### 1.1 问题

现有 HEALPix 浏览器采用 `healpix_browser_cpp`（C++ 后端 + winsock2 HTTP 服务器，localhost:18080）+ `healpix_browser_web`（WebGL 前端）架构。该架构存在严重通讯问题：

- **JSON + base64 编码开销**：Float32Array 经 base64 编码传输，前端解码后写入 WebGL 纹理，数据膨胀约 33% + 双重拷贝
- **HTTP 往返延迟**：每次视角变化需 HTTP 请求子叶数据，单次往返 5-50ms（localhost）
- **双进程边界**：C++ 后端进程 ↔ 浏览器进程，跨进程通讯无法避免序列化开销
- **WebGL 限制**：GPU 资源管理受浏览器沙箱约束，无法直接复用 C++ 内存

用户偏好（user_profile）：WebGL 通讯开销过大，倾向 C++ 实现浏览器端。

### 1.2 目标

用 C++ 重写浏览器，消除 HTTP + base64 通讯开销，数据通过 C++ 内存直传 OpenGL 纹理。核心算法库无 Qt 依赖，可嵌入任意 UI 框架。

### 1.3 非目标

- 不优化 drizzle 阶段性能（26.3s，另立 spec）
- 不实现光度马赛克梯度校正（已设计，另立 spec）
- 不处理 plate_solve 模糊解算问题（另立 spec）
- 不做 790 帧全回归测试（另立 spec）

### 1.4 与现有模块的关系

| 现有模块 | 处理方式 |
|---------|---------|
| `healpix_browser_cpp/`（C++ 后端 + HTTP） | BrowserBackend 数据层逻辑移植到新 core/；HTTP 服务器部分废弃；整个目录归档到 `lib/healpix_db/archive/` |
| `healpix_browser_web/`（WebGL 前端） | STF 算法、球面渲染逻辑、视角交互逻辑移植到 C++；整个目录归档到 `lib/healpix_db/archive/` |
| `healpix_io/`（.hiss/.hcsd 读写 DLL） | 复用，不修改。core/ 通过 `hiss_read`/`hcsd_read`/`hcsd_read_leaf` 读数据 |
| `healpix_lod/`、`healpix_browser/`（已废弃） | 保持现状，不处理 |

---

## 2. 模块定位与三层架构

### 2.1 新模块路径

```
lib/healpix_db/healpix_browser_qt/    ← 新模块
├── core/                              ← 本文档范围（算法核心，无 Qt 依赖）
│   ├── browser_backend.h/.cpp
│   ├── stf_engine.h/.cpp
│   ├── healpix_math.h/.cpp
│   ├── gl_renderer.h/.cpp
│   └── shaders/                       ← GLSL 着色器源码（内嵌字符串 + .glsl 文件）
├── widgets/                           ← UI 文档范围（Qt6 widget）
├── app/                               ← UI 文档范围（demo exe）
├── include/
│   └── healpix_browser_core.h         ← 对外统一头文件
├── tests/
│   ├── test_browser_backend.cpp
│   ├── test_stf_engine.cpp
│   ├── test_healpix_math.cpp
│   └── test_gl_renderer.cpp
├── Makefile
├── memory.md
└── README.md
```

### 2.2 三层架构图

```
┌─────────────────────────────────────────────────────────────┐
│  app/ - demo exe（UI 文档范围）                             │
│  MainWindow + File > Open + STF 控制面板                    │
│  按扩展名路由：.hiss → SingleFrameView，.hcsd → SphereView  │
└─────────────────────────┬───────────────────────────────────┘
                          │ 创建 widget 并嵌入
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  widgets/ - Qt6 widget 层（UI 文档范围）                    │
│  AbstractView (QOpenGLWidget)                              │
│    ├── SingleFrameView  - 2D 切面投影（TAN Projection）     │
│    └── SphereView       - 3D UV 球面 64×128 分段            │
└─────────────────────────┬───────────────────────────────────┘
                          │ 调用 core/ 的 C++ 类
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  core/ - 核心算法库（本文档范围，纯 C++17 + OpenGL）        │
│                                                             │
│  BrowserBackend   - 文件管理 + 按需子叶加载 + ud_grade      │
│  STFEngine        - MTF + MAD 自动拉伸 + 4 预设 + asinh     │
│  HealpixMath      - pix2angNest/ang2pixNest/query_disc      │
│  GLRenderer       - OpenGL 渲染（球面网格/切面纹理/着色器）  │
└─────────────────────────────────────────────────────────────┘
                          │ dlopen / 链接
                          ▼
              healpix_io.dll（现有，不修改）
```

### 2.3 编译产物

| 产物 | 类型 | 用途 | 依赖 |
|------|------|------|------|
| `libhealpix_browser_core.a` | 静态库 | 嵌入大工程时链接，或被 widgets/ 链接 | healpix_io.dll, OpenGL32, gdi32 |
| `libhealpix_browser_qt.a` | 静态库 | 含 widget 类（UI 文档） | core + Qt6::OpenGLWidgets |
| `healpix_browser_qt.exe` | 可执行 | demo 验证程序（UI 文档） | core + widgets + Qt6::Widgets |

**为何用静态库而非 DLL**：core/ 是渲染逻辑，与宿主同进程同 OpenGL 上下文，静态链接避免 DLL 跨边界传递 OpenGL 句柄的复杂性。未来若需独立 DLL 也可调整。

---

## 3. core/ 核心库详细设计

### 3.1 BrowserBackend（数据加载与按需子叶）

**职责**：管理 .hiss/.hcsd 文件句柄，响应视角请求按需加载子叶，视角相关压缩，ud_grade 降采样。

**移植来源**：`healpix_browser_cpp/include/browser_backend.h` + `browser_backend.cpp`，去掉 HTTP 服务器部分，保留全部数据层逻辑。

#### 3.1.1 接口定义

```cpp
// core/browser_backend.h
#ifndef BROWSER_BACKEND_H
#define BROWSER_BACKEND_H

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>

// 视角参数（widget 层填充后传给 core）
struct ViewParams {
    double center_ra;    // 中心赤经（度）
    double center_dec;   // 中心赤纬（度）
    double zoom;         // 缩放级别（1.0 = 全天，越大越放大）
    double fov_deg;      // 视场大小（度）
};

// 子叶数据（内存由 BrowserBackend 持有，调用者用完调 release_leaf）
struct LeafData {
    uint64_t leaf_ipix;  // nside=64 子叶 ipix
    uint64_t n_pix;      // 像素数
    uint64_t* ipix;      // ipix 数组（malloc 分配）
    float* pixel;        // 像素值数组（malloc 分配）
    uint32_t nside;      // 实际 nside（可能被降采样）

    LeafData() : leaf_ipix(0), n_pix(0), ipix(nullptr), pixel(nullptr), nside(0) {}
};

class BrowserBackend {
public:
    BrowserBackend();
    ~BrowserBackend();

    // 文件管理
    int open_file(const std::string& path);  // 0=成功, <0=失败
    void close_file();
    bool is_open() const;
    bool is_hiss() const;   // 单帧模式
    bool is_hcsd() const;   // 球面模式
    uint32_t get_nside() const;
    uint64_t get_n_pix() const;
    const std::string& get_file_path() const;
    const std::string& get_filter() const;   // 滤光片名（从 meta 读取）

    // 按需加载（球面模式）
    std::vector<uint64_t> get_required_leaves(const ViewParams& view) const;
    LeafData load_leaf(uint64_t leaf_ipix, uint32_t target_nside);
    uint32_t decide_target_nside(const ViewParams& view, uint64_t leaf_ipix) const;

    // 降采样（NESTED 位运算，4 相邻像素均值合并）
    LeafData ud_grade(const LeafData& input, uint32_t target_nside);

    // 全量数据（仅 .hiss 模式，单帧切面投影用）
    LeafData get_all_data();

    // 释放 LeafData 内存
    void release_leaf(LeafData& leaf);

    // HEALPix 角度计算辅助（公开，供 GLRenderer 使用）
    static void ipix_to_angle(uint32_t nside, uint64_t ipix, bool nested,
                              double& ra, double& dec);
    static double angular_distance(double ra1, double dec1,
                                   double ra2, double dec2);

private:
    std::string file_path_;
    std::string filter_;
    bool is_hiss_;
    uint32_t nside_;
    uint64_t n_pix_;
    int nested_;
    mutable std::mutex mutex_;

    // .hiss 模式下缓存的全部数据
    uint64_t* all_ipix_;
    float* all_pixel_;

    // 内部辅助
    void free_all_data();
};

#endif
```

#### 3.1.2 视角相关压缩策略（沿用现有设计）

| 视角区域 | 加载层级 | nside | 备注 |
|---------|---------|-------|------|
| 中心区域（距视角中心 < fov_deg/4） | 全分辨率 | 8192 | 视野中心，最高细节 |
| 中间区域（< fov_deg/2） | 中等降采样 | 2048 | 4×4 像素均值合并 |
| 边缘区域 | 高强度压缩 | 256 | 32×32 像素均值合并 |
| 屏幕外 | 不加载 | — | 不传输到 GPU |

#### 3.1.3 ud_grade 降采样算法

NESTED 排序位运算：`ipix_coarse = ipix_fine >> (2 × log2(nside_fine / nside_coarse))`。4 个相邻像素求均值合并。与现有 `browser_backend.cpp` 实现一致，仅做接口整理。

### 3.2 STFEngine（显示拉伸引擎）

**职责**：对天文图像做非破坏性显示拉伸，不修改原始数据。在 CPU 端计算拉伸参数，GPU 端执行拉伸（uniform 传参）。

**移植来源**：`healpix_browser_web/js/stf.js`（MTF 公式、MAD 自动拉伸、4 预设、asinh 压缩）。

#### 3.2.1 接口定义

```cpp
// core/stf_engine.h
#ifndef STF_ENGINE_H
#define STF_ENGINE_H

#include <cstdint>
#include <vector>
#include <string>

// STF 拉伸参数
struct STFParams {
    float shadows;       // 暗部裁剪点 [0,1)
    float highlights;    // 亮部裁剪点 (0,1]
    float midtones;      // 中点参数 (0,1)，0.5=线性，<0.5 提亮暗部
    float compression;   // asinh/log 预设压缩强度 [0,1]

    STFParams()
        : shadows(0.0f), highlights(1.0f), midtones(0.5f), compression(0.0f) {}

    bool validate() const;
};

class STFEngine {
public:
    STFEngine();

    // 预设名称 → (midtones, compression)
    // linear: (0.5, 0.0)
    // sqrt:   (0.25, 0.0)
    // asinh:  (0.25, 0.5)
    // log:    (0.15, 0.8)
    // 新接口（2026-07-14 更新）: 接收 data_min/data_max，返回原始像素值
    // shadows=data_min, highlights=data_max（全数据范围可见，对齐 Siril 行为）
    static STFParams get_preset(const std::string& name,
                                float data_min, float data_max);

    // 标量 MTF: MTF(x, m) = ((m-1)*x) / ((2m-1)*x - m)
    static float mtf(float x, float m);

    // MAD 自动拉伸：基于中位数绝对偏差估算 shadows/highlights
    // data: 像素值数组（原始 float，非归一化）
    // 返回 STFParams（shadows/highlights 为原始像素值范围，GPU 着色器内归一化）
    static STFParams auto_stretch(const float* data, size_t n,
                                  float no_data_value = 0.0f);

    // 将 STFParams 转换为 GPU uniform（归一化到 [0,1]）
    // 需要先知道数据范围 [min, max] 用于归一化
    struct GPUUniforms {
        float shadows;
        float highlights;
        float midtones;
        float compression;
        float no_data;
    };
    static GPUUniforms to_uniforms(const STFParams& params,
                                   float data_min, float data_max,
                                   float no_data_value = 0.0f);
};

#endif
```

#### 3.2.2 核心算法

**MTF 公式**（与 stf.js 一致）：
```
MTF(x, m) = ((m-1)·x) / ((2m-1)·x - m)
满足：MTF(0,m)=0, MTF(1,m)=1, MTF(m,m)=0.5
```

**MAD 自动拉伸**：
1. 计算中位数 `median`
2. 计算 MAD = median(|v - median|)
3. sigma = 1.4826 × MAD
4. shadows = median - 3 × sigma（裁剪到 [min, max]）
5. highlights = median + 3 × sigma
6. midtones = clamp(median, shadows, highlights) 归一化后

**asinh 压缩**（GPU 着色器内执行）：
```
asinh(x, c) = log(x/scale + sqrt((x/scale)² + 1)) / log(1/scale + sqrt(1/scale² + 1))
其中 scale = max((1-c)/c, 1e-6)
```

### 3.3 HealpixMath（球面坐标转换）

**职责**：HEALPix 球面坐标转换，供 GLRenderer 顶点查值使用。

**移植来源**：`healpix_browser_web/js/webgl-renderer.js` 的 `pix2angNest` 方法（从 C++ `browser_backend.cpp` 移植的 JS 版，现回迁 C++，支持任意 nside，BigInt 避免 nside=8192 精度丢失）。

#### 3.3.1 接口定义

```cpp
// core/healpix_math.h
#ifndef HEALPIX_MATH_H
#define HEALPIX_MATH_H

#include <cstdint>
#include <vector>
#include <utility>

class HealpixMath {
public:
    // NESTED 排序：ipix → (ra, dec)，单位度
    // nside: HEALPix 分辨率参数
    // ipix: 像素索引 [0, 12*nside²)
    // 返回 ra ∈ [0, 360), dec ∈ [-90, 90]
    static void pix2ang_nest(uint32_t nside, uint64_t ipix,
                             double& ra, double& dec);

    // NESTED 排序：(ra, dec) → ipix
    static uint64_t ang2pix_nest(uint32_t nside, double ra, double dec);

    // 球面圆盘查询：返回圆盘内所有 ipix
    // ra, dec: 圆盘中心（度）
    // radius_deg: 圆盘半径（度）
    static std::vector<uint64_t> query_disc(uint32_t nside,
                                            double ra, double dec,
                                            double radius_deg);

    // ud_grade 降采样（与 BrowserBackend.ud_grade 共用实现）
    // 输入 nside=N 的像素，降采样到 target_nside = N / 2^k
    struct GradeResult {
        uint32_t nside;
        std::vector<uint64_t> ipix;
        std::vector<float> pixel;
    };
    static GradeResult ud_grade(uint32_t src_nside,
                                const std::vector<uint64_t>& src_ipix,
                                const std::vector<float>& src_pixel,
                                uint32_t target_nside);

    // 大圆距离（度）
    static double angular_distance(double ra1, double dec1,
                                   double ra2, double dec2);
};

#endif
```

#### 3.3.2 实现要点

- **nside=8192 精度**：`npface = nside²` 最大 67108864，超过 `int32` 但在 `int64` 范围内。C++ 用 `uint64_t` 计算 `ip_low = ipix % npface`，`ix`/`iy` 最大 8191 用 `uint32_t`。
- **三区域分块**（北极 / 赤道 / 南极），与 HEALPix 标准一致。
- **query_disc**：遍历 nside 层所有 ipix 过滤（优化：可用 rings2nest 索引加速，但 nside=64 子叶查询时 49152 个子叶遍历可接受）。

### 3.4 GLRenderer（OpenGL 渲染核心）

**职责**：裸 OpenGL 渲染逻辑，不依赖 Qt。接收 OpenGL 上下文（由 widget 层创建并 makeCurrent），执行球面/切面渲染。

**移植来源**：`healpix_browser_web/js/webgl-renderer.js` 的球面网格构建、纹理管理、着色器编译、绘制逻辑。WebGL 1.0/2.0 → OpenGL 3.3 Core Profile。

#### 3.4.1 接口定义

```cpp
// core/gl_renderer.h
#ifndef GL_RENDERER_H
#define GL_RENDERER_H

#include <cstdint>
#include <vector>
#include <memory>
#include "browser_backend.h"
#include "stf_engine.h"

// 渲染模式
enum class RenderMode {
    SPHERE,             // 球面渲染（.hcsd，UV 球面网格）
    HISS_POLYGON        // .hiss 像素多边形球面渲染（不展平，每像素 4 角点）
    // SINGLE_FRAME 已废弃（TAN 投影展平方案，2026-07-14 替换为 HISS_POLYGON）
};

// 渲染参数（widget 层填充）
struct RenderParams {
    RenderMode mode;
    ViewParams view;             // 视角
    STFParams stf;               // STF 拉伸参数
    float data_min;              // 数据范围（归一化用）
    float data_max;
    float no_data_value;         // 无数据标记（默认 0.0）
    int viewport_w;              // 视口宽（像素）
    int viewport_h;              // 视口高（像素）
};

class GLRenderer {
public:
    GLRenderer();
    ~GLRenderer();

    // 初始化（需在 OpenGL 上下文 makeCurrent 后调用）
    // 返回 0=成功, <0=失败
    int init();
    bool is_initialized() const;

    // 释放资源（OpenGL 上下文销毁前调用）
    void cleanup();

    // 主渲染入口（每帧调用）
    // backend: 数据源（已 open_file）
    // params: 渲染参数
    // 返回 0=成功, <0=失败
    int render(BrowserBackend& backend, const RenderParams& params);

    // 更新 STF 参数（无需重建网格，仅更新 uniform）
    void update_stf(const STFParams& stf, float data_min, float data_max);

    // 单帧模式：设置数据边界框（初始化时计算一次）
    void set_single_frame_bbox(double center_ra, double center_dec,
                               double width_deg, double height_deg);

    // 球面模式：获取当前已加载的子叶列表（调试用）
    std::vector<uint64_t> get_loaded_leaves() const;

private:
    // 着色器程序
    unsigned int sphere_program_;     // 球面着色器
    unsigned int quad_program_;       // 单帧四边形着色器

    // 球面网格
    unsigned int sphere_vao_;
    unsigned int sphere_vbo_;
    unsigned int sphere_ibo_;
    int sphere_index_count_;

    // 单帧四边形
    unsigned int quad_vao_;
    unsigned int quad_vbo_;

    // 子叶纹理管理（球面模式）
    // leaf_ipix → (texture_id, nside, last_used_frame)
    struct LeafTexture {
        uint64_t leaf_ipix;
        unsigned int texture_id;
        uint32_t nside;
        uint64_t last_used_frame;
    };
    std::vector<LeafTexture> leaf_textures_;
    uint64_t current_frame_;

    // STF uniform 缓存
    STFParams cached_stf_;
    float cached_data_min_;
    float cached_data_max_;
    float cached_no_data_;

    // 单帧模式数据
    unsigned int single_frame_texture_;
    double sf_center_ra_, sf_center_dec_, sf_width_deg_, sf_height_deg_;
    bool sf_texture_valid_;

    // 内部方法
    int compile_shaders();
    void build_sphere_mesh(int segments_lat, int segments_lon);
    void build_quad_mesh();
    unsigned int upload_leaf_texture(const LeafData& leaf);
    void evict_unused_leaves(size_t max_leaves);
    int render_sphere(BrowserBackend& backend, const RenderParams& params);
    int render_single_frame(BrowserBackend& backend, const RenderParams& params);

    // 矩阵运算（4×4，column-major）
    static void perspective_matrix(double fov_deg, double aspect,
                                   double near, double far, float* m);
    static void look_at_matrix(double eye_x, double eye_y, double eye_z,
                               double center_x, double center_y, double center_z,
                               double up_x, double up_y, double up_z, float* m);
    static void multiply_matrix(const float* a, const float* b, float* out);
};

#endif
```

#### 3.4.2 球面渲染流程

```
1. widget 层 makeCurrent → 调用 GLRenderer.render(backend, params)
2. render_sphere():
   a. 根据 params.view 计算相机位置（球面外，距中心 zoom 决定距离）
   b. backend.get_required_leaves(view) → 需要的子叶列表
   c. 对每个需要的子叶：
      - 若已在 leaf_textures_ 中 → 更新 last_used_frame
      - 否则 backend.load_leaf(ipix, target_nside) → upload_leaf_texture() → 加入 leaf_textures_
   d. evict_unused_leaves(max=100) → 淘汰 LRU 子叶纹理
   e. 绑定 sphere_program_，设置 uniform（MVP 矩阵、STF 参数）
   f. 球面网格分块绘制：每个子叶对应一段顶点，绑定对应纹理
      ── 或简化方案：球面网格每个顶点查 ipix 值，作为 attribute 传入 ──
   g. glDrawElements
3. widget 层 swapBuffers
```

**球面网格构建**（UV 球面 64×128 分段）：
- 顶点 (x, y, z) = 球面坐标 → (ra, dec) → ipix → 查值
- 顶点属性：`a_position (vec3)` + `a_value (float, 原始像素值)`
- 顶点值在 CPU 端查得（通过 HealpixMath.ang2pix_nest + 子叶纹理 Map 查找）

**子叶纹理 vs 顶点查值**（两种方案，选其一）：

| 方案 | 描述 | 优点 | 缺点 |
|------|------|------|------|
| A. 子叶纹理 | 每个子叶上传为 1D 纹理，片元着色器查值 | 纹理复用，视角变化时纹理不重建 | 着色器需 ipix→texel 映射，复杂 |
| B. 顶点查值 | CPU 端每个顶点查值，作为 attribute 传 GPU | 简单，着色器逻辑少 | 视角变化时需重建顶点值（64×128=8192 顶点，可接受） |

**选择方案 B**（顶点查值）：逻辑直接，8192 顶点查值 < 1ms，着色器简单（无需 ipix→texel 映射）。与现有 WebGL 实现的子叶纹理方案（方案 A）不同，但接口不变，实现时若性能不足可切换回方案 A。

#### 3.4.3 .hiss 像素多边形球面渲染流程（2026-07-14 替换旧 TAN 投影展平）

```
1. render_hiss_polygon():
   a. 首次调用: build_hiss_polygon_mesh(backend)
      - backend.get_all_data() → (ipix[], pixel[])
      - 每像素 pix2ang_nest 计算中心 → 4 角点（近似 pix_size）
      - 生成球面四边形顶点 (x,y,z,value)
      - 上传 VBO/VAO（GL_STATIC_DRAW）
      - 估算 bbox 用于初始视角
   b. 计算 MVP 矩阵:
      - distance=3.0 固定，FOV=60/zoom 控制缩放
      - 相机看向 (center_ra, center_dec) 方向
   c. 绑定 sphere_program_（复用球面着色器）
   d. 设置 STF uniform
   e. glDrawArrays(GL_TRIANGLES)
2. 视角变化时: 仅更新 MVP 矩阵，不重建网格
3. 无纹理重建，无摩尔纹
```

**废弃**: 旧 `render_single_frame`（TAN 投影展平 + 1024 纹理 + GL_LINEAR）已归档。

#### 3.4.4 着色器源码

**球面顶点着色器**（OpenGL 3.3 Core，对应 WebGL SPHERE_VERTEX_SHADER）：
```glsl
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in float aValue;
uniform mat4 uMVPMatrix;
out float vValue;
void main() {
    vValue = aValue;
    gl_Position = uMVPMatrix * vec4(aPosition, 1.0);
}
```

**球面片元着色器**（STF 拉伸，对应 SPHERE_FRAGMENT_SHADER）：
```glsl
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
```

**单帧四边形着色器**（对应 QUAD_VERTEX_SHADER / QUAD_FRAGMENT_SHADER，纹理采样版）：
```glsl
// vertex
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}

// fragment（与球面片元相同，增加纹理采样）
#version 330 core
in vec2 vTexCoord;
uniform sampler2D uTexture;
uniform float uShadows, uHighlights, uMidtones, uCompression, uNoData;
out vec4 FragColor;
// ... mtf / asinhCompress 同上 ...
void main() {
    float vValue = texture(uTexture, vTexCoord).r;
    // ... 同球面片元拉伸逻辑 ...
}
```

---

## 4. 对外统一头文件

```cpp
// include/healpix_browser_core.h
#ifndef HEALPIX_BROWSER_CORE_H
#define HEALPIX_BROWSER_CORE_H

#include "browser_backend.h"
#include "stf_engine.h"
#include "healpix_math.h"
#include "gl_renderer.h"

#endif
```

嵌入大工程时只需 `#include "healpix_browser_core.h"` 并链接 `libhealpix_browser_core.a`。

---

## 5. 数据流

### 5.1 球面模式（.hcsd）

```
用户拖动鼠标 → widget 更新 ViewParams
              → GLRenderer.render(backend, params)
              → backend.get_required_leaves(view) → 需要的子叶列表
              → 对每个子叶：
                  backend.load_leaf(ipix, target_nside)
                  → healpix_io.dll hcsd_read_leaf(path, leaf_ipix, ...)
                  → 返回 (ipix[], pixel[])
                  → HealpixMath.ud_grade (若需降采样)
              → 球面网格 64×128 顶点：
                  每顶点 (ra, dec) → HealpixMath.ang2pix_nest → 查子叶 Map → value
              → 上传顶点属性 (position + value)
              → STF uniform 设置
              → glDrawElements
              → GPU 内 MTF + asinh + uint8 binning
              → framebuffer → 屏幕
```

**全程 C++ 内存直传**，无 HTTP、无 base64、无 JSON 序列化。子叶数据 `float*` 直接填充顶点 attribute buffer。

### 5.2 单帧模式（.hiss）

```
用户打开 .hiss → backend.open_file → hiss_read 全量加载到 all_ipix_/all_pixel_
              → 首次渲染：采样 1000 像素估算边界框
              → 视角变化时重建 1024×1024 R32F 纹理：
                  每像素 TAN 逆投影 → (ra, dec) → ang2pix_nest → Map 查值
              → 全屏四边形绘制，纹理采样 + STF 拉伸
```

---

## 6. 与 UI 层接口契约

UI 层（widgets/ + app/）通过以下 C++ 类接口调用 core/：

| UI 操作 | 调用的 core 接口 |
|---------|-----------------|
| 打开文件 | `BrowserBackend::open_file(path)` |
| 初始化渲染 | `GLRenderer::init()`（OpenGL 上下文 makeCurrent 后） |
| 每帧绘制 | `GLRenderer::render(backend, params)` |
| STF 滑块调整 | `GLRenderer::update_stf(params, min, max)` |
| 鼠标拖动 | UI 层更新 ViewParams，触发重绘 |
| 滚轮缩放 | UI 层更新 ViewParams.zoom，触发重绘 |
| 文件关闭 | `GLRenderer::cleanup()` + `BrowserBackend::close_file()` |

**契约要点**：
1. UI 层负责创建 OpenGL 上下文（QOpenGLWidget）并 `makeCurrent()`，core/ 不管理上下文
2. UI 层负责事件处理（鼠标/键盘/触摸），core/ 不接收事件
3. UI 层负责窗口管理（创建/销毁/resize），core/ 仅接收 viewport_w/viewport_h
4. core/ 是无状态渲染（除纹理缓存），可被任意 widget 创建并独占使用

---

## 7. 编译与构建

### 7.1 Makefile（core 静态库）

```makefile
# lib/healpix_db/healpix_browser_qt/Makefile（core 部分）
CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra -fopenmp
INCLUDES = -Icore -Iinclude -I../healpix_io/include

# core 静态库
CORE_SRCS = core/browser_backend.cpp core/stf_engine.cpp \
            core/healpix_math.cpp core/gl_renderer.cpp
CORE_OBJS = $(CORE_SRCS:.cpp=.o)

libhealpix_browser_core.a: $(CORE_OBJS)
	ar rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(CORE_OBJS) libhealpix_browser_core.a
```

### 7.2 依赖

- **编译时**：`healpix_io/include/healpix_io.h`（头文件）
- **链接时**：`healpix_io.dll`（运行时加载，或静态链接 `libhealpix_io.a`）
- **运行时**：OpenGL32.dll（系统）、gdi32.dll（系统）
- **OpenMP**：`libgomp-1.dll`（与现有模块一致）

### 7.3 编译环境

- g++ 16.1.0（MSYS2 MinGW64），与现有模块一致
- Windows 10+，OpenGL 3.3+ 支持保证

---

## 8. 测试策略

### 8.1 单元测试（core/）

| 测试文件 | 测试内容 |
|---------|---------|
| `test_browser_backend.cpp` | open_file（.hiss/.hcsd）、load_leaf、ud_grade、get_all_data |
| `test_stf_engine.cpp` | MTF 公式正确性、MAD 自动拉伸、4 预设参数、asinh 压缩 |
| `test_healpix_math.cpp` | pix2ang/ang2pix 往返一致性、query_disc 圆盘查询、ud_grade 降采样 |
| `test_gl_renderer.cpp` | 着色器编译、球面网格构建、纹理上传（需 OpenGL 上下文，用 GLFW 创建测试窗口） |

### 8.2 集成测试

- 用现有 `output/pipeline_debug/` 下的 `.hiss` 文件测试单帧模式
- 用合成 `.hcsd` 文件测试球面模式（按需子叶加载）
- 视角变化时子叶加载/淘汰正确性

### 8.3 性能验证

| 指标 | 目标 | 现有 WebGL 基线 |
|------|------|----------------|
| 单帧首次渲染 | < 200ms | ~500ms（含 HTTP + base64） |
| 球面单帧渲染 | < 16ms（60fps） | ~50-100ms（HTTP 往返主导） |
| 视角变化响应 | < 50ms | 200-500ms |
| 内存占用 | < 500MB | ~300MB（浏览器进程） |

---

## 9. 归档计划（WebGL 浏览器）

新模块完成后，将现有 WebGL 浏览器归档：

```
lib/healpix_db/archive/
├── healpix_browser_cpp/     ← 从 lib/healpix_db/healpix_browser_cpp/ 移动
│   └── （原内容，含 HTTP 服务器）
└── healpix_browser_web/     ← 从 lib/healpix_db/healpix_browser_web/ 移动
    └── （原内容，含 WebGL 前端）
```

归档时：
- 在 `archive/` 下新建 `ARCHIVE_INDEX.md` 记录归档原因、日期、被替代关系
- 更新 `PROJECT_ARCHITECTURE.md` §2.1 模块清单：`healpix_browser_cpp` / `healpix_browser_web` 标记为"已归档"，新增 `healpix_browser_qt` 模块
- 更新 `lib/healpix_db/memory.md` 记录架构变更

---

## 10. 验证标准

1. **功能完整性**：
   - .hiss 单帧切面投影渲染正确（与现有 WebGL 版视觉一致）
   - .hcsd 球面按需子叶加载正确（视角变化时子叶加载/淘汰）
   - STF 拉伸 4 预设 + 自动拉伸 + 手动滑块均可用
   - 拖动平移/缩放交互正确（单帧 2D + 球面 3D）

2. **性能达标**：
   - 球面渲染 60fps（< 16ms/帧）
   - 视角变化响应 < 50ms
   - 无 HTTP/base64 通讯开销

3. **架构合规**：
   - core/ 无 Qt 依赖（`grep -r "Q" core/` 应无结果，除注释）
   - core/ 可被任意 UI 框架链接使用
   - widgets/ 仅依赖 core/ + Qt6，不含业务逻辑

4. **归档完成**：
   - `healpix_browser_cpp/` + `healpix_browser_web/` 移至 `archive/`
   - `PROJECT_ARCHITECTURE.md` 更新模块清单
   - 现有测试数据可正常加载渲染

5. **嵌入就绪**：
   - demo exe 验证 core/ + widgets/ 可用
   - `include/healpix_browser_core.h` 统一头文件可用
   - 静态库 `libhealpix_browser_core.a` 可被外部工程链接

---

## 11. 开放问题

### 11.1 球面渲染纹理方案

本文档选定方案 B（顶点查值，CPU 端每顶点查值作为 attribute）。实现时若发现 8192 顶点查值耗时过大，可切换方案 A（子叶纹理 + 着色器查值）。两者接口不变，仅 GLRenderer 内部实现差异。

### 11.2 后台线程纹理重建

单帧模式视角变化时重建 1024×1024 纹理（~100-500ms）。是否用后台线程预生成下一帧纹理？当前设计为同步重建（简单）。若交互卡顿明显，后续优化为后台线程 + 双缓冲。

### 11.3 OpenGL 版本

目标 OpenGL 3.3 Core Profile（对应 WebGL 2.0 能力）。若需兼容老旧显卡，可降级到 OpenGL 2.1 + 扩展，但增加着色器复杂度。当前假设 Win10+ 显卡支持 3.3。

---

## 附录：与现有 WebGL 浏览器的对照

| 维度 | 现有 WebGL 浏览器 | 新 C++ Qt 浏览器 |
|------|------------------|------------------|
| 架构 | C++ HTTP 后端 + WebGL 前端（双进程） | C++ 核心 + Qt widget（单进程） |
| 通讯 | JSON + base64 over HTTP | C++ 内存直传（无序列化） |
| 渲染 | WebGL 2.0（浏览器沙箱） | OpenGL 3.3 Core（原生） |
| 数据传输 | Float32Array → base64 → HTTP → 解码 → WebGL 纹理 | float* → OpenGL 纹理（零拷贝） |
| UI 框架 | HTML/CSS/JS | Qt6 QOpenGLWidget |
| 独立入口 | 下拉框切换模式（混用风险） | 双 widget 独立类（无混用） |
| 文件选择 | 命令行参数 | QFileDialog（UI 改进） |
| 嵌入能力 | 无法嵌入（浏览器进程） | 静态库 + widget 可嵌入 Qt 工程 |
| 依赖 | winsock2 + WebView2 | Qt6 + OpenGL32 |
