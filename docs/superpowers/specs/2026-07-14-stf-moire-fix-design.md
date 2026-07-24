# STF 单位体系重构与摩尔纹修复设计

> **日期**：2026-07-14
> **状态**：设计阶段，待实现
> **范围**：修复 HEALPix 浏览器 Qt 版本的显示缺陷：
> 1. STF 预设拉伸（linear/sqrt/asinh/log）失效
> 2. .hiss 单帧模式改为球面渲染（不展平），消除摩尔纹
> **关联文档**：
> - `2026-07-13-cpp-qt-browser-core-design.md`（原始核心设计，§3.2/§3.4 需更新）
> - `2026-07-13-cpp-qt-browser-ui-design.md`（原始 UI 设计，STFPanel + View 路由需更新）

---

## 1. 背景与问题

### 1.1 STF 单位体系重构遗留问题

在 2026-07-14 的 STF 归一化 bug 修复中，将 `to_uniforms()` 从归一化模式改为原始像素值直传模式。但 `STFEngine::get_preset()` 仍返回 `shadows=0, highlights=1`（[0,1] 归一化值），导致：

- 选择预设时，`shadows=0, highlights=1` 被当作原始像素值传给 GPU
- 实际数据范围 [99.85, 298.1]，几乎所有像素值 > 1，被裁剪到 highlights 之上
- 画面全白或全黑，预设拉伸完全失效

### 1.2 .hiss 单帧模式问题

当前 .hiss 单帧模式采用 TAN 投影展平 + 1024×1024 固定纹理 + GL_LINEAR 插值，存在两个问题：

1. **摩尔纹**：CPU 端 1024 纹理点采样 HEALPix 离散像素，GPU 用 GL_LINEAR 放大到视口，HEALPix 像素边界产生规则条纹
2. **违背球面直觉**：用户期望"原生直接显示曲面，就像看球面一样，不展平"，数据显示为球面上的一个瓦片/patch

### 1.3 用户需求

- 预设拉伸行为与 Siril 的显示传递函数几种拉伸相同
- .hiss 改为球面渲染，不展平，显示为球面上的一个瓦片
- 直接显示 HEALPix 像素，不插值
- 更新设计文档并推送 GitHub

---

## 2. 设计方案

### 2.1 预设修复（方案 A1）

**核心思路**：扩展 `get_preset` 接口，接收 `data_min/data_max`，返回原始像素值，符合 Siril 行为（预设=全数据范围 + 曲线）。

#### 2.1.1 接口变更

```cpp
// core/stf_engine.h
class STFEngine {
public:
    // 新接口：接收数据范围，返回原始像素值
    static STFParams get_preset(const std::string& name,
                                 float data_min, float data_max);
};
```

#### 2.1.2 预设参数表（对齐 Siril）

| 预设 | shadows | highlights | midtones | compression | Siril 对应 |
|------|---------|------------|----------|-------------|-----------|
| linear | data_min | data_max | 0.5 | 0.0 | 线性 |
| sqrt | data_min | data_max | 0.25 | 0.0 | 平方根 |
| asinh | data_min | data_max | 0.25 | 0.5 | asinh |
| log | data_min | data_max | 0.15 | 0.8 | 对数 |

**设计依据**：
- `shadows=data_min, highlights=data_max`：全数据范围可见，不裁剪
- `midtones` 控制 MTF 曲线中点：0.5=线性，<0.5 提亮暗部
- `compression` 控制 asinh 压缩强度：0=无压缩，1=强压缩
- 与 Siril 的 STF 预设行为一致：预设只改变曲线形状，不改变裁剪范围

#### 2.1.3 调用方变更

`STFPanel::on_preset_changed` 调用新签名：

```cpp
void STFPanel::on_preset_changed(int index) {
    if (index < 0) return;
    QString name = preset_combo_->itemData(index).toString();
    STFParams p = STFEngine::get_preset(name.toStdString(),
                                         data_min_, data_max_);
    update_sliders_no_signal(p);
    emit stf_changed(p);
}
```

### 2.2 .hiss 改为球面渲染（HEALPix 像素多边形绘制）

**核心思路**：废弃 TAN 投影展平，.hiss 数据直接在球面上绘制为 HEALPix 像素多边形，每个像素一个球面四边形色块，不插值，无摩尔纹。

#### 2.2.1 渲染原理

```
每个 HEALPix 像素 ipix
  → pix2ang_nest 计算 4 个角点 (ra, dec)
  → 转球面坐标 (x, y, z) = (cos(dec)cos(ra), cos(dec)sin(ra), sin(dec))
  → 作为 2 个三角形（4 顶点）加入顶点缓冲
  → 顶点属性: aPosition(vec3) + aValue(float=像素值)

GPU 绘制:
  GL_TRIANGLES + STF 着色器
  → 每个像素一个色块，无插值，无纹理，无摩尔纹
  → 球面上显示为瓦片/patch
```

#### 2.2.2 与 .hcsd 球面渲染的统一

| 维度 | .hiss（新） | .hcsd（现有） |
|------|------------|--------------|
| 渲染方式 | HEALPix 像素多边形 | UV 球面网格 + 顶点查值 |
| 数据加载 | 全量（all_ipix_/all_pixel_） | 按需子叶 |
| 网格来源 | HEALPix 像素角点 | UV 球面 64×128 |
| 适用场景 | 单帧数据（几度视场） | 全天球数据 |

**统一 View**：.hiss 和 .hcsd 都用 SphereView（球面渲染），废弃 SingleFrameView。区别仅在数据加载方式和网格构建方式，由 GLRenderer 内部根据 RenderMode 分支处理。

#### 2.2.3 关键改动

**1. 废弃 SingleFrameView，.hiss 路由到 SphereView**

```cpp
// app/main_window.cpp open_file()
if (backend_->is_hiss()) {
    view = new SphereView(this);  // 改: 不再用 SingleFrameView
    // 初始视角对准数据 bbox 中心
    static_cast<SphereView*>(view)->set_initial_view_from_bbox();
} else if (backend_->is_hcsd()) {
    view = new SphereView(this);
    static_cast<SphereView*>(view)->reset_view();
}
```

**2. GLRenderer 新增 HEALPix 像素多边形网格构建**

```cpp
// core/gl_renderer.h
class GLRenderer {
public:
    // 新增: 构建 HEALPix 像素多边形网格（.hiss 球面渲染用）
    // 从 backend 获取全量数据，每像素生成 4 角点顶点
    int build_hiss_polygon_mesh(BrowserBackend& backend);

private:
    // 新增: .hiss 像素多边形 VAO/VBO
    unsigned int hiss_polygon_vao_ = 0;
    unsigned int hiss_polygon_vbo_ = 0;
    int hiss_polygon_vertex_count_ = 0;
    bool hiss_mesh_valid_ = false;

    // 新增: .hiss 初始视角 bbox
    double hiss_center_ra_ = 0.0, hiss_center_dec_ = 0.0;
    double hiss_width_deg_ = 0.0, hiss_height_deg_ = 0.0;

    // 新增: 渲染 .hiss 球面多边形
    int render_hiss_polygon(BrowserBackend& backend, const RenderParams& params);
};
```

**3. build_hiss_polygon_mesh 实现逻辑**

```cpp
int GLRenderer::build_hiss_polygon_mesh(BrowserBackend& backend) {
    LeafData all = backend.get_all_data();
    if (all.n_pix == 0 || all.pixel == nullptr) return -1;

    // 每像素 4 角点，每角点 (x,y,z) + value = 16 字节
    std::vector<float> vertices;
    vertices.reserve(all.n_pix * 4 * 4);  // 4 顶点 × 4 float

    // 同时估算 bbox
    double min_ra = 360, max_ra = 0, min_dec = 90, max_dec = -90;

    for (uint64_t i = 0; i < all.n_pix; ++i) {
        uint64_t ipix = all.ipix[i];
        float value = all.pixel[i];

        // pix2ang_nest 计算 4 个角点
        // HEALPix 像素角点: 通过 pix2ang_nest 中心 + 角点偏移
        // 简化: 用 pix2ang_nest 中心，角点由 nside 分辨率推算
        double ra, dec;
        HealpixMath::pix2ang_nest(all.nside, ipix, ra, dec);

        // 更新 bbox
        if (ra < min_ra) min_ra = ra;
        if (ra > max_ra) max_ra = ra;
        if (dec < min_dec) min_dec = dec;
        if (dec > max_dec) max_dec = dec;

        // 计算像素角点（近似：nside 分辨率决定的像素大小）
        double pix_size = 4.0 * 180.0 / (3.0 * all.nside);  // HEALPix 像素边长（度）
        double d_ra = pix_size / std::cos(dec * M_PI / 180.0);
        double d_dec = pix_size;

        // 4 个角点（球面坐标）
        double corners[4][2] = {
            {ra - d_ra/2, dec - d_dec/2},
            {ra + d_ra/2, dec - d_dec/2},
            {ra + d_ra/2, dec + d_dec/2},
            {ra - d_ra/2, dec + d_dec/2}
        };

        for (int c = 0; c < 4; ++c) {
            double r = corners[c][0] * M_PI / 180.0;
            double d = corners[c][1] * M_PI / 180.0;
            vertices.push_back(std::cos(d) * std::cos(r));  // x
            vertices.push_back(std::cos(d) * std::sin(r));  // y
            vertices.push_back(std::sin(d));                 // z
            vertices.push_back(value);                       // value
        }
    }

    // 保存 bbox 用于初始视角
    hiss_center_ra_ = (min_ra + max_ra) / 2.0;
    hiss_center_dec_ = (min_dec + max_dec) / 2.0;
    hiss_width_deg_ = max_ra - min_ra;
    hiss_height_deg_ = max_dec - min_dec;

    // 上传 VBO/VAO
    // ... (OpenGL 代码)

    hiss_mesh_valid_ = true;
    return 0;
}
```

**4. render_hiss_polygon 渲染逻辑**

```cpp
int GLRenderer::render_hiss_polygon(BrowserBackend& backend, const RenderParams& params) {
    // 首次调用: 构建网格
    if (!hiss_mesh_valid_) {
        build_hiss_polygon_mesh(backend);
    }

    // 设置 MVP 矩阵（球面视角，复用 SphereView 的相机逻辑）
    // ...

    // 绑定 sphere_program_（复用现有球面着色器）
    // aPosition=vec3, aValue=float, STF uniform
    // ...

    // 绘制
    pglBindVertexArray(hiss_polygon_vao_);
    glDrawArrays(GL_TRIANGLES, 0, hiss_polygon_vertex_count_);
    // ...
    return 0;
}
```

**5. RenderMode 调整**

```cpp
enum class RenderMode {
    SPHERE,              // 球面渲染（.hcsd，UV 球面网格）
    HISS_POLYGON         // .hiss 像素多边形球面渲染（新）
    // SINGLE_FRAME 已废弃（TAN 投影展平方案）
};
```

**6. 初始视角对准数据 bbox**

SphereView 新增 `set_initial_view_from_bbox()` 方法：
```cpp
void SphereView::set_initial_view_from_bbox() {
    double ra, dec, w, h;
    renderer_->get_hiss_bbox(ra, dec, w, h);
    center_ra_ = ra;
    center_dec_ = dec;
    // zoom 根据数据大小自动调整，使 patch 占视野约 60%
    double max_dim = std::max(w, h);
    zoom_ = std::clamp(60.0 / max_dim, 0.5, 100.0);
}
```

#### 2.2.4 着色器复用

.hiss 像素多边形渲染复用现有球面着色器（`sphere_program_`）：
- 顶点着色器：`aPosition(vec3) + aValue(float)` → `vValue`
- 片元着色器：STF 拉伸（MTF + asinh 压缩）

无需新增着色器。

#### 2.2.5 优势

1. **消除摩尔纹**：无纹理放大，无 GL_LINEAR 插值，每个 HEALPix 像素直接绘制为色块
2. **球面原生显示**：不展平，数据显示为球面上的瓦片/patch，符合用户期望
3. **无纹理重建**：不涉及 CPU 纹理生成，视角变化只需更新 MVP 矩阵
4. **统一架构**：.hiss 和 .hcsd 都用球面渲染（SphereView），简化 UI 层

#### 2.2.6 性能评估

- **顶点数**：964279 像素 × 4 顶点 = 3.8M 顶点
- **顶点缓冲**：3.8M × 16B = 61MB（可接受）
- **三角形数**：1.9M（现代 GPU 毫秒级绘制）
- **视角变化响应**：只需更新 MVP 矩阵（< 1ms），无网格重建
- **内存**：VBO 61MB + all_ipix_/all_pixel_ 约 8MB = 69MB

### 2.3 文档更新策略

#### 2.3.1 新建文档

`docs/superpowers/specs/2026-07-14-stf-moire-fix-design.md`（本文档）

#### 2.3.2 更新现有文档

**`2026-07-13-cpp-qt-browser-core-design.md`**：
- §3.2 STFEngine 部分：`get_preset` 接口签名更新，`to_uniforms` 注释更新，新增 `compute_data_range` 百分位数策略
- §3.4 GLRenderer 部分：新增 `build_hiss_polygon_mesh` + `render_hiss_polygon`，标记 SINGLE_FRAME 模式废弃
- §3.4.3 单帧切面投影流程：替换为 .hiss 像素多边形球面渲染流程
- §5.2 单帧模式数据流：替换为球面多边形渲染数据流

**`2026-07-13-cpp-qt-browser-ui-design.md`**：
- STFPanel 部分：滑块映射更新，新增 `set_data_range` 接口
- View 路由部分：.hiss 路由到 SphereView，废弃 SingleFrameView
- MainWindow 部分：data_range 同步流程

### 2.4 GitHub 推送

- commit 消息：`fix: STF预设单位重构 + .hiss改球面渲染(像素多边形)消除摩尔纹`
- 推送到 `Healpix-Database` 仓库 main 分支

---

## 3. 验证标准

### 3.1 预设修复验证

1. 打开 .hiss 文件
2. 依次选择 linear/sqrt/asinh/log 预设
3. 每个预设应正确显示图像（非全白/全黑）
4. 不同预设的视觉效果应有明显差异（linear 最暗，log 最亮）
5. 滑块数值显示为原始像素值（如 shadows=99.85, highlights=298.10）

### 3.2 球面渲染验证

1. 打开 .hiss 文件
2. 数据显示为球面上的一个瓦片/patch（非 2D 展平）
3. 初始视角自动对准数据覆盖区域
4. 可旋转球体查看（拖动交互）
5. 可缩放（滚轮交互）
6. 无摩尔纹条纹
7. HEALPix 像素显示为块状（符合天文软件惯例）

### 3.3 回归验证

1. 自动拉伸（MAD）功能正常
2. 滑块手动调整功能正常
3. .hcsd 球面模式不受影响
4. 编译 0 error 0 warning

---

## 4. 影响范围

### 4.1 修改文件清单

| 文件 | 修改内容 |
|------|---------|
| `core/stf_engine.h` | `get_preset` 接口签名增加 data_min/data_max 参数 |
| `core/stf_engine.cpp` | `get_preset` 实现返回原始像素值 |
| `core/gl_renderer.h` | 新增 `build_hiss_polygon_mesh`/`render_hiss_polygon`/hiss bbox 成员；RenderMode 调整 |
| `core/gl_renderer.cpp` | 新增 .hiss 像素多边形网格构建与渲染；废弃 `render_single_frame` |
| `app/stf_panel.cpp` | `on_preset_changed` 调用新签名 |
| `app/main_window.cpp` | .hiss 路由到 SphereView，废弃 SingleFrameView 路由 |
| `widgets/sphere_view.h/.cpp` | 新增 `set_initial_view_from_bbox` 方法 |
| `widgets/single_frame_view.h/.cpp` | 标记废弃（可归档或保留） |
| `docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md` | §3.2/§3.4/§3.4.3/§5.2 更新 |
| `docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md` | STFPanel + View 路由更新 |

### 4.2 不受影响

- `widgets/abstract_view.cpp`（compute_data_range 百分位数已修复）
- `core/healpix_math.cpp`（pix2ang_nest 已可用）
- `.hcsd 球面渲染逻辑`（不修改）

---

## 5. 开放问题

### 5.1 HEALPix 像素角点精度

当前方案用 `pix_size = 4*180/(3*nside)` 近似像素边长，角点为中心 ± pix_size/2。这是近似值，HEALPix 像素实际形状为菱形（赤道带）或扭曲四边形（极区）。

**影响**：像素之间可能有微小缝隙或重叠，视觉上不明显（块状显示本来就接受像素边界）。

**后续优化**：可用 HEALPix 标准的 `pix2ang_nest` 角点计算（boundaries）替代近似，但需移植更复杂的角点算法。

### 5.2 SingleFrameView 处理

SingleFrameView（TAN 投影 2D 展平）将被废弃。处理方式：
- 选项 A：归档到 archive/（保留代码供参考）
- 选项 B：直接删除（YAGNI 原则）

**建议**：归档到 `lib/healpix_db/healpix_browser_qt/widgets/archive/`，保留 TAN 投影实现供未来可能的 2D 查看模式使用。

### 5.3 大数据集顶点数

若 .hiss 像素数超过 5M（如全天 nside=8192 单帧），顶点缓冲 320MB 可能过大。

**缓解**：视口外的像素不绘制（视锥剔除），或按子叶分批绘制。当前 .hiss 文件 964K 像素，无此问题。
