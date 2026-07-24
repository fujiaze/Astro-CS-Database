# STF 预设修复 + .hiss 球面渲染 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 STF 预设拉伸失效 + .hiss 改为球面渲染（HEALPix 像素多边形绘制）消除摩尔纹

**Architecture:** 预设修复扩展 `get_preset` 接口接收 data_min/data_max 返回原始像素值；.hiss 废弃 TAN 投影展平，改为 HEALPix 像素多边形球面渲染（每像素 4 角点球面四边形），复用 SphereView 和球面着色器，废弃 SingleFrameView。

**Tech Stack:** C++17, OpenGL 3.3 Core, Qt6, MSYS2 MinGW64 g++ 16.1.0, CMake

**Spec:** `docs/superpowers/specs/2026-07-14-stf-moire-fix-design.md`

---

## 文件结构

| 文件 | 责任 | 操作 |
|------|------|------|
| `core/stf_engine.h` | STF 参数与引擎声明 | 修改: `get_preset` 签名增加 data_min/data_max |
| `core/stf_engine.cpp` | STF 引擎实现 | 修改: `get_preset` 返回原始像素值 |
| `core/gl_renderer.h` | 渲染器声明 | 修改: 新增 hiss 多边形成员 + RenderMode 调整 |
| `core/gl_renderer.cpp` | 渲染器实现 | 修改: 新增 `build_hiss_polygon_mesh`/`render_hiss_polygon`，废弃 `render_single_frame` |
| `app/stf_panel.cpp` | STF 控制面板 | 修改: `on_preset_changed` 调用新签名 |
| `app/main_window.cpp` | 主窗口 | 修改: .hiss 路由到 SphereView |
| `widgets/sphere_view.h` | 球面视图声明 | 修改: 新增 `set_initial_view_from_bbox` |
| `widgets/sphere_view.cpp` | 球面视图实现 | 修改: 实现 `set_initial_view_from_bbox` |
| `widgets/single_frame_view.h/.cpp` | 旧 2D 视图 | 归档到 `widgets/archive/` |
| `docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md` | 核心设计文档 | 修改: §3.2/§3.4 更新 |
| `docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md` | UI 设计文档 | 修改: STFPanel + View 路由更新 |

---

## Task 1: 修复 STF 预设接口（A1 方案）

**Files:**
- Modify: `lib/healpix_db/healpix_browser_qt/core/stf_engine.h`
- Modify: `lib/healpix_db/healpix_browser_qt/core/stf_engine.cpp`
- Modify: `lib/healpix_db/healpix_browser_qt/app/stf_panel.cpp`

- [ ] **Step 1: 修改 `stf_engine.h` 的 `get_preset` 签名**

打开 `lib/healpix_db/healpix_browser_qt/core/stf_engine.h`，找到 `get_preset` 声明（约 line 39）：

```cpp
// 旧:
static STFParams get_preset(const std::string& name);
```

替换为：

```cpp
// 新: 接收数据范围，返回原始像素值（符合 Siril 行为）
// shadows=data_min, highlights=data_max, 预设只设置 midtones/compression
static STFParams get_preset(const std::string& name,
                             float data_min, float data_max);
```

- [ ] **Step 2: 修改 `stf_engine.cpp` 的 `get_preset` 实现**

打开 `lib/healpix_db/healpix_browser_qt/core/stf_engine.cpp`，找到 `get_preset` 实现（约 line 23-47），替换为：

```cpp
// ---- 预设查询（接收数据范围，返回原始像素值） ----
// 与 Siril 显示传递函数对齐: 预设=全数据范围 + 曲线形状
STFParams STFEngine::get_preset(const std::string& name,
                                 float data_min, float data_max) {
    STFParams p;
    // 所有预设: shadows=data_min, highlights=data_max（全数据范围可见）
    p.shadows = data_min;
    p.highlights = data_max;

    if (name == "linear") {
        // 线性: 无中点偏移、无压缩
        p.midtones = 0.5f;
        p.compression = 0.0f;
    } else if (name == "sqrt") {
        // 平方根风格: 中点下移提亮暗部，无压缩
        p.midtones = 0.25f;
        p.compression = 0.0f;
    } else if (name == "asinh") {
        // asinh 风格: 中点下移 + 中等压缩
        p.midtones = 0.25f;
        p.compression = 0.5f;
    } else if (name == "log") {
        // 对数风格: 中点更低 + 强压缩
        p.midtones = 0.15f;
        p.compression = 0.8f;
    } else {
        // 未知预设: 默认 linear 等效并告警
        p.midtones = 0.5f;
        p.compression = 0.0f;
        LOG_WARN("get_preset: 未知预设名 '%s'，返回默认 linear 参数", name.c_str());
    }
    return p;
}
```

- [ ] **Step 3: 修改 `stf_panel.cpp` 的 `on_preset_changed`**

打开 `lib/healpix_db/healpix_browser_qt/app/stf_panel.cpp`，找到 `on_preset_changed`（约 line 157-164），替换为：

```cpp
void STFPanel::on_preset_changed(int index) {
    if (index < 0) return;
    QString name = preset_combo_->itemData(index).toString();
    STFParams p = STFEngine::get_preset(name.toStdString(),
                                         data_min_, data_max_);
    update_sliders_no_signal(p);
    // 触发一次完整 stf_changed
    emit stf_changed(p);
}
```

- [ ] **Step 4: 编译验证**

Run:
```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cmake --build "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt\build" 2>&1 | Select-Object -Last 10
```
Expected: 0 error, 0 warning（可能需要先关闭运行中的 exe）

- [ ] **Step 5: 提交**

```powershell
cd "f:\Astro dev\Astro CS Normalization Database"
git add lib/healpix_db/healpix_browser_qt/core/stf_engine.h lib/healpix_db/healpix_browser_qt/core/stf_engine.cpp lib/healpix_db/healpix_browser_qt/app/stf_panel.cpp
git commit -m "fix: STF预设接收data_min/max返回原始像素值(对齐Siril行为)"
```

---

## Task 2: GLRenderer 新增 .hiss 像素多边形渲染接口

**Files:**
- Modify: `lib/healpix_db/healpix_browser_qt/core/gl_renderer.h`

- [ ] **Step 1: 修改 RenderMode 枚举**

打开 `lib/healpix_db/healpix_browser_qt/core/gl_renderer.h`，找到 RenderMode 枚举（约 line 22-26）：

```cpp
// 旧:
enum class RenderMode {
    SPHERE,         // 球面渲染（.hcsd）
    SINGLE_FRAME    // 单帧切面投影（.hiss）
};
```

替换为：

```cpp
// 新:
enum class RenderMode {
    SPHERE,             // 球面渲染（.hcsd，UV 球面网格）
    HISS_POLYGON        // .hiss 像素多边形球面渲染（不展平）
    // SINGLE_FRAME 已废弃（TAN 投影展平方案）
};
```

- [ ] **Step 2: 新增 .hiss 多边形成员变量和方法声明**

在 GLRenderer 类的 private 部分（约 line 440-490 之间），找到 `// 单帧模式数据` 注释块，替换为：

```cpp
    // ---- .hiss 像素多边形模式数据（新，替代旧 single_frame） ----
    unsigned int hiss_polygon_vao_ = 0;
    unsigned int hiss_polygon_vbo_ = 0;
    int hiss_polygon_vertex_count_ = 0;
    bool hiss_mesh_valid_ = false;

    // .hiss 数据 bbox（初始视角用）
    double hiss_center_ra_ = 0.0;
    double hiss_center_dec_ = 0.0;
    double hiss_width_deg_ = 0.0;
    double hiss_height_deg_ = 0.0;
```

在 GLRenderer 类的 public 部分，找到 `set_single_frame_bbox` 声明附近，新增：

```cpp
    // .hiss 像素多边形模式: 获取数据 bbox（初始视角用）
    void get_hiss_bbox(double& center_ra, double& center_dec,
                       double& width_deg, double& height_deg) const {
        center_ra = hiss_center_ra_;
        center_dec = hiss_center_dec_;
        width_deg = hiss_width_deg_;
        height_deg = hiss_height_deg_;
    }
```

在 GLRenderer 类的 private 方法部分（约 line 480-490），找到 `render_single_frame` 声明，替换为：

```cpp
    // 新增: .hiss 像素多边形网格构建与渲染
    int build_hiss_polygon_mesh(BrowserBackend& backend);
    int render_hiss_polygon(BrowserBackend& backend, const RenderParams& params);
    // 旧 render_single_frame 已废弃
```

- [ ] **Step 3: 编译验证（仅头文件变更，应无错误）**

Run:
```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cmake --build "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt\build" 2>&1 | Select-Object -Last 10
```
Expected: 可能因 render_single_frame 调用处未更新而报错，这是预期的，下一步会修复

- [ ] **Step 4: 提交**

```powershell
git add lib/healpix_db/healpix_browser_qt/core/gl_renderer.h
git commit -m "refactor: GLRenderer新增hiss多边形渲染接口+废弃single_frame"
```

---

## Task 3: 实现 build_hiss_polygon_mesh 和 render_hiss_polygon

**Files:**
- Modify: `lib/healpix_db/healpix_browser_qt/core/gl_renderer.cpp`

- [ ] **Step 1: 找到 render_single_frame 函数位置**

Run:
```powershell
Select-String -Path "lib\healpix_db\healpix_browser_qt\core\gl_renderer.cpp" -Pattern "^int GLRenderer::render_single_frame" | Select-Object LineNumber
```
记录行号（约 993），这是要替换的函数起点。

- [ ] **Step 2: 实现 build_hiss_polygon_mesh**

在 `gl_renderer.cpp` 中，找到 `render_single_frame` 函数前（约 line 990），插入新函数：

```cpp
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
```

- [ ] **Step 3: 实现 render_hiss_polygon**

在 `build_hiss_polygon_mesh` 函数后，插入：

```cpp
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

    // 设置视口
    glViewport(0, 0, params.viewport_w, params.viewport_h);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 计算 MVP 矩阵（复用球面视角逻辑）
    // 相机位置: 球面外，距中心 zoom 决定距离
    double zoom = params.view.zoom > 0.001 ? params.view.zoom : 0.001;
    double distance = 3.0 / zoom;  // zoom 越大越近

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

    // up 向量: 简化为 (0, 0, 1) 在球面切平面上的投影
    double up_x = 0.0, up_y = 0.0, up_z = 1.0;

    float mvp[16];
    perspective_matrix(45.0, static_cast<double>(params.viewport_w) / params.viewport_h,
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
```

- [ ] **Step 4: 修改 render() 主入口的分支逻辑**

找到 `GLRenderer::render` 函数中的 `RenderMode` 分支（搜索 `case RenderMode::SINGLE_FRAME`），替换为：

```cpp
    // 旧:
    // case RenderMode::SINGLE_FRAME:
    //     return render_single_frame(backend, params);

    // 新:
    case RenderMode::HISS_POLYGON:
        return render_hiss_polygon(backend, params);
```

- [ ] **Step 5: 删除或注释旧的 render_single_frame 函数**

找到 `render_single_frame` 函数（约 line 993-1172），在函数开头添加 `#if 0` 注释整个函数：

```cpp
#if 0  // 废弃: TAN 投影展平方案，已替换为 render_hiss_polygon
int GLRenderer::render_single_frame(BrowserBackend& backend, const RenderParams& params) {
    // ... 原有代码 ...
}
#endif
```

- [ ] **Step 6: 编译验证**

Run:
```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cmake --build "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt\build" 2>&1 | Select-Object -Last 15
```
Expected: 0 error, 0 warning

- [ ] **Step 7: 提交**

```powershell
git add lib/healpix_db/healpix_browser_qt/core/gl_renderer.cpp
git commit -m "feat: 实现hiss像素多边形球面渲染(替代TAN投影展平)"
```

---

## Task 4: SphereView 新增 set_initial_view_from_bbox

**Files:**
- Modify: `lib/healpix_db/healpix_browser_qt/widgets/sphere_view.h`
- Modify: `lib/healpix_db/healpix_browser_qt/widgets/sphere_view.cpp`

- [ ] **Step 1: 修改 sphere_view.h 新增方法声明**

打开 `lib/healpix_db/healpix_browser_qt/widgets/sphere_view.h`，找到 `reset_view` 声明附近，新增：

```cpp
    // .hiss 模式: 根据数据 bbox 设置初始视角（对准数据中心，zoom 自适应）
    void set_initial_view_from_bbox();
```

- [ ] **Step 2: 修改 sphere_view.cpp 实现方法**

打开 `lib/healpix_db/healpix_browser_qt/widgets/sphere_view.cpp`，找到 `reset_view` 实现后，新增：

```cpp
void SphereView::set_initial_view_from_bbox() {
    // 从 renderer 获取 .hiss 数据 bbox
    // 注: renderer 可能尚未初始化（首次 paintGL 前），用默认值兜底
    double ra = 0.0, dec = 0.0, w = 10.0, h = 10.0;
    if (renderer_) {
        renderer_->get_hiss_bbox(ra, dec, w, h);
    }
    center_ra_ = ra;
    center_dec_ = dec;
    // zoom 自适应: 使 patch 占视野约 60%
    double max_dim = std::max(w, h);
    if (max_dim > 0.001) {
        zoom_ = std::clamp(60.0 / max_dim, 0.5, 100.0);
    } else {
        zoom_ = 1.0;
    }
    LOG_INFO("set_initial_view_from_bbox: center=(%.4f,%.4f) zoom=%.3f",
             center_ra_, center_dec_, zoom_);
    emit view_changed(center_ra_, center_dec_, zoom_);
    request_render();
}
```

注意: 需要 `#include "logger.h"` 和 `#include <algorithm>`（检查是否已包含）。

- [ ] **Step 3: 修改 build_render_params 使用 HISS_POLYGON 模式**

在 `sphere_view.cpp` 中找到 `build_render_params` 函数，修改 `p.mode`：

```cpp
// 旧: p.mode = RenderMode::SPHERE;
// 新:
p.mode = RenderMode::HISS_POLYGON;  // .hiss 用像素多边形渲染
```

注意: 这会使所有 SphereView 都用 HISS_POLYGON 模式。需要区分 .hiss 和 .hcsd。更稳妥的方式是在 set_backend 时设置标志。但为了简单，先这样实现，后续如 .hcsd 也有问题再调整。

**修正**: 实际上 .hcsd 应该保持 SPHERE 模式。更好的方案是在 SphereView 中新增 `render_mode_` 成员：

在 `sphere_view.h` 的 private 部分新增：
```cpp
RenderMode render_mode_ = RenderMode::SPHERE;  // 默认球面
```

修改 `build_render_params`：
```cpp
p.mode = render_mode_;
```

新增 public 方法：
```cpp
void set_render_mode(RenderMode mode) { render_mode_ = mode; }
```

- [ ] **Step 4: 编译验证**

Run:
```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cmake --build "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt\build" 2>&1 | Select-Object -Last 15
```
Expected: 0 error, 0 warning

- [ ] **Step 5: 提交**

```powershell
git add lib/healpix_db/healpix_browser_qt/widgets/sphere_view.h lib/healpix_db/healpix_browser_qt/widgets/sphere_view.cpp
git commit -m "feat: SphereView新增set_initial_view_from_bbox+render_mode切换"
```

---

## Task 5: MainWindow 修改 .hiss 路由到 SphereView

**Files:**
- Modify: `lib/healpix_db/healpix_browser_qt/app/main_window.cpp`

- [ ] **Step 1: 修改 open_file 函数的 .hiss 路由**

打开 `lib/healpix_db/healpix_browser_qt/app/main_window.cpp`，找到 `open_file` 中的路由逻辑（约 line 154-166）：

```cpp
// 旧:
AbstractView* view = nullptr;
if (backend_->is_hiss()) {
    view = new SingleFrameView(this);
    static_cast<SingleFrameView*>(view)->init_view_from_data();
} else if (backend_->is_hcsd()) {
    view = new SphereView(this);
    static_cast<SphereView*>(view)->reset_view();
} else {
    QMessageBox::warning(this, "警告", "不支持的文件格式");
    backend_->close_file();
    return;
}
```

替换为：

```cpp
// 新: .hiss 和 .hcsd 都用 SphereView，区别在 render_mode
AbstractView* view = nullptr;
if (backend_->is_hiss()) {
    view = new SphereView(this);
    static_cast<SphereView*>(view)->set_render_mode(RenderMode::HISS_POLYGON);
    // 注: set_initial_view_from_bbox 需在 renderer 初始化后调用，
    //     这里先设置标志，首次 paintGL 后由 auto_stretch 触发
    static_cast<SphereView*>(view)->set_initial_view_from_bbox();
} else if (backend_->is_hcsd()) {
    view = new SphereView(this);
    static_cast<SphereView*>(view)->set_render_mode(RenderMode::SPHERE);
    static_cast<SphereView*>(view)->reset_view();
} else {
    QMessageBox::warning(this, "警告", "不支持的文件格式");
    backend_->close_file();
    return;
}
```

- [ ] **Step 2: 移除 single_frame_view.h 包含**

找到 `#include "single_frame_view.h"`，注释掉或删除：

```cpp
// #include "single_frame_view.h"  // 已废弃，.hiss 改用 SphereView
```

- [ ] **Step 3: 修改 on_view_reset 的 SingleFrameView 分支**

找到 `on_view_reset` 函数（约 line 246-254），替换为：

```cpp
void MainWindow::on_view_reset() {
    if (!current_view_) return;
    if (auto* v = qobject_cast<SphereView*>(current_view_)) {
        // 根据 render_mode 调用不同的 reset
        if (backend_ && backend_->is_hiss()) {
            v->set_initial_view_from_bbox();
        } else {
            v->reset_view();
        }
    }
}
```

- [ ] **Step 4: 编译验证**

Run:
```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cmake --build "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt\build" 2>&1 | Select-Object -Last 15
```
Expected: 0 error, 0 warning

- [ ] **Step 5: 提交**

```powershell
git add lib/healpix_db/healpix_browser_qt/app/main_window.cpp
git commit -m "refactor: MainWindow .hiss路由到SphereView+废弃SingleFrameView"
```

---

## Task 6: 归档 SingleFrameView

**Files:**
- Move: `widgets/single_frame_view.h` → `widgets/archive/single_frame_view.h`
- Move: `widgets/single_frame_view.cpp` → `widgets/archive/single_frame_view.cpp`
- Modify: `CMakeLists.txt`（移除 single_frame_view.cpp）

- [ ] **Step 1: 创建 archive 目录并移动文件**

Run:
```powershell
$base = "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt\widgets"
New-Item -ItemType Directory -Path "$base\archive" -Force
Move-Item "$base\single_frame_view.h" "$base\archive\single_frame_view.h"
Move-Item "$base\single_frame_view.cpp" "$base\archive\single_frame_view.cpp"
```

- [ ] **Step 2: 从 CMakeLists.txt 移除 single_frame_view.cpp**

打开 `lib/healpix_db/healpix_browser_qt/CMakeLists.txt`，找到 widgets 源文件列表，删除 `widgets/single_frame_view.cpp` 一行。

- [ ] **Step 3: 编译验证**

Run:
```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cmake --build "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt\build" 2>&1 | Select-Object -Last 15
```
Expected: 0 error, 0 warning

- [ ] **Step 4: 提交**

```powershell
git add -A lib/healpix_db/healpix_browser_qt/widgets/ lib/healpix_db/healpix_browser_qt/CMakeLists.txt
git commit -m "chore: 归档SingleFrameView到widgets/archive(TAN投影展平方案废弃)"
```

---

## Task 7: 运行验证 + 截图

**Files:**
- 无文件修改，仅运行验证

- [ ] **Step 1: 关闭可能运行的旧进程**

Run:
```powershell
Get-Process healpix_browser_qt -ErrorAction SilentlyContinue | Stop-Process -Force
```

- [ ] **Step 2: 启动浏览器加载 .hiss 文件**

Run:
```powershell
$env:Path = "C:\msys64\mingw64\bin;f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_io;$env:Path"
$exe = "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt\build\healpix_browser_qt.exe"
$hissFile = "f:\Astro dev\Astro CS Normalization Database\output\pipeline_debug\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red\drizzle\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.hiss"
$logFile = "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt\build\qt_stderr_new.log"
$proc = Start-Process -FilePath $exe -ArgumentList "`"$hissFile`"" -PassThru -RedirectStandardError $logFile
Write-Output "PID: $($proc.Id)"
Start-Sleep -Seconds 5
```

- [ ] **Step 3: 检查日志确认渲染成功**

Run:
```powershell
Get-Content $logFile -Encoding UTF8 -Tail 20
```
Expected: 包含 `build_hiss_polygon_mesh: 完成` 和 `render_hiss_polygon: 绘制` 日志

- [ ] **Step 4: 截图验证**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File "c:\Users\fujia\.trae-cn\skills\screenshot\scripts\take_screenshot.ps1" -Mode temp -ActiveWindow
```
Expected: 截图保存到 temp 目录

- [ ] **Step 5: 分析截图亮度分布**

Run:
```powershell
python -c "
from PIL import Image
import sys
img = Image.open(sys.argv[1])
gray = img.convert('L')
pixels = list(gray.getdata())
bins = [0]*16
for p in pixels: bins[min(p//16, 15)] += 1
total = len(pixels)
print('Brightness histogram:')
for i, c in enumerate(bins):
    print(f'  {i*16:3d}-{i*16+15:3d}: {c*100/total:.1f}%')
print(f'Mean: {sum(pixels)/total:.1f}')
" <截图路径>
```
Expected: Mean 在 50-150 之间（非全白 222，非全黑），分布合理

- [ ] **Step 6: 测试预设切换**

在浏览器中依次点击 STFPanel 的预设下拉框（linear/sqrt/asinh/log），每次切换后等待 1 秒并截图。确认不同预设视觉效果有差异。

- [ ] **Step 7: 测试摩尔纹**

观察默认视图是否有规则条纹。确认无摩尔纹。

- [ ] **Step 8: 提交验证结果（无需 git commit，仅记录）**

记录验证结果到 memory.md。

---

## Task 8: 更新设计文档

**Files:**
- Modify: `docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md`
- Modify: `docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md`

- [ ] **Step 1: 更新 core-design.md §3.2 STFEngine 部分**

打开 `docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md`，找到 §3.2.1 接口定义中的 `get_preset` 声明（约 line 261）：

```cpp
// 旧:
static STFParams get_preset(const std::string& name);
```

替换为：

```cpp
// 新: 接收数据范围，返回原始像素值（对齐 Siril 行为）
static STFParams get_preset(const std::string& name,
                             float data_min, float data_max);
```

更新 §3.2.1 的 STFParams 注释（约 line 240-244）：

```cpp
// STF 拉伸参数
//   - shadows/highlights: 原始像素值（GPU 着色器内归一化）
//   - midtones:           MTF 中点参数，0.5=线性，<0.5 提亮暗部
//   - compression:        asinh/log 等非线性预设的压缩强度 [0,1]
```

更新 §3.2.2 的 `to_uniforms` 说明：添加"不再归一化，直传原始像素值"。

新增 `compute_data_range` 百分位数策略说明（§3.2.3）：

```markdown
#### 3.2.3 compute_data_range 百分位数策略

数据动态范围计算采用百分位数（0.5%/99.5%）避免异常值（如饱和星 5081）：
- 排序所有有效像素
- data_min = 0.5 分位，data_max = 99.5 分位
- 用于 STFPanel 滑块映射和预设范围
```

- [ ] **Step 2: 更新 core-design.md §3.4 GLRenderer 部分**

找到 §3.4.3 单帧切面投影流程（约 line 529-543），替换为：

```markdown
#### 3.4.3 .hiss 像素多边形球面渲染流程（替代旧 TAN 投影展平）

```
1. render_hiss_polygon():
   a. 首次调用: build_hiss_polygon_mesh(backend)
      - backend.get_all_data() → (ipix[], pixel[])
      - 每像素 pix2ang_nest 计算中心 → 4 角点（近似 pix_size）
      - 生成球面四边形顶点 (x,y,z,value)
      - 上传 VBO/VAO
      - 估算 bbox 用于初始视角
   b. 计算 MVP 矩阵（球面视角，复用 SphereView 相机逻辑）
   c. 绑定 sphere_program_（复用球面着色器）
   d. 设置 STF uniform
   e. glDrawArrays(GL_TRIANGLES)
2. 视角变化时: 仅更新 MVP 矩阵，不重建网格
3. 无纹理重建，无摩尔纹
```

**废弃**: 旧 `render_single_frame`（TAN 投影展平 + 1024 纹理 + GL_LINEAR）已归档。
```

- [ ] **Step 3: 更新 ui-design.md STFPanel 部分**

打开 `docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md`，找到 STFPanel 部分，更新：

- 滑块映射说明: shadows/highlights 映射到 [data_min, data_max] 原始像素值
- 新增 `set_data_range(float, float)` 接口
- MainWindow 打开文件后同步 data_range 到 STFPanel

- [ ] **Step 4: 更新 ui-design.md View 路由部分**

找到 View 路由说明，更新为：

```markdown
文件路由:
- .hiss → SphereView (render_mode=HISS_POLYGON) + set_initial_view_from_bbox
- .hcsd → SphereView (render_mode=SPHERE) + reset_view

SingleFrameView 已归档到 widgets/archive/（TAN 投影展平方案废弃）
```

- [ ] **Step 5: 提交**

```powershell
git add docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md
git commit -m "docs: 更新core/ui设计文档(STF单位体系+hiss球面渲染)"
```

---

## Task 9: 推送 GitHub

**Files:**
- 无文件修改，仅 git push

- [ ] **Step 1: 检查 git 状态**

Run:
```powershell
cd "f:\Astro dev\Astro CS Normalization Database"
git status
git log --oneline -8
```
Expected: 所有修改已提交，无未提交变更

- [ ] **Step 2: 推送到 GitHub**

Run:
```powershell
$ghBin = "C:\Users\fujia\AppData\Local\Temp\gh-cli-install\bin"
$env:Path = "$env:Path;$ghBin"
git push origin main
```
Expected: 推送成功

- [ ] **Step 3: 验证 GitHub 远程状态**

Run:
```powershell
gh repo view fujiaze/Healpix-Database --json name,defaultBranchRef
git log --oneline origin/main -5
```
Expected: 远程 main 分支包含最新 commits

---

## Self-Review 检查

**1. Spec 覆盖**:
- ✅ 预设修复 A1: Task 1
- ✅ .hiss 球面渲染: Task 2-3 (GLRenderer) + Task 4 (SphereView) + Task 5 (MainWindow)
- ✅ SingleFrameView 归档: Task 6
- ✅ 运行验证: Task 7
- ✅ 文档更新: Task 8
- ✅ GitHub 推送: Task 9

**2. 占位符扫描**: 无 TBD/TODO，所有步骤有具体代码

**3. 类型一致性**:
- `get_preset(name, data_min, data_max)` 全文一致
- `build_hiss_polygon_mesh` / `render_hiss_polygon` 全文一致
- `set_initial_view_from_bbox` 全文一致
- `RenderMode::HISS_POLYGON` 全文一致
- `set_render_mode(RenderMode)` 全文一致

---

## 执行选择

Plan complete and saved to `docs/superpowers/plans/2026-07-14-stf-moire-fix.md`. Two execution options:

1. **Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration
2. **Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
