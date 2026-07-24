# HEALPix 浏览器 LOD + 动态网格重构 spec

## 背景

nside=65536 的 .hiss 文件（61.6M 像素）在球面渲染模式下「整体非常模糊，放大也不会变清晰」。

## 根因

球面网格 256×512=131841 顶点**均布全球**，15° FOV 下仅约 460 顶点落在视场内，每顶点覆盖 ~1700 屏幕像素（欠采样 40 倍）。固定网格无法适应缩放，放大时顶点密度不足导致模糊。

## 修复目标

1. 顶点密度匹配屏幕像素（每顶点≈1px），放大变清晰
2. LOD 自动按屏幕分辨率停止下钻（不加载超分辨率子叶）
3. 预加载保证缩放流畅
4. uint8 降采样降低内存和计算开销

## 设计

### 1. 动态重建球面网格

**触发**：FOV 或视口变化超过阈值时重建 VBO。

**网格范围**：以视角中心为圆心，覆盖 `FOV_render = FOV × 1.2`（渲染余量），预加载区 `FOV_preload = FOV × 1.5`。

**顶点密度**：
- 屏幕像素角分辨率 `θ_screen = FOV / min(viewport_w, viewport_h)`
- 顶点间距 `θ_vertex = θ_screen`（每顶点 1 屏幕像素）
- lat segments = `FOV_ver_render / θ_vertex`
- lon segments = `FOV_horiz_render / θ_vertex`
- 网格以 (center_ra, center_dec) 为中心，覆盖 render 范围

**实现**：`build_sphere_mesh_dynamic(view, viewport_w, viewport_h)` 替代固定 `build_sphere_mesh(256, 512)`。

### 2. LOD 自动阈值（重写 decide_target_nside）

**公式**：
```
θ_screen = FOV / min(viewport_w, viewport_h)  // 度/像素
θ_hp(nside) = 360 / sqrt(12 * nside²)         // 度/HEALPix像素
nside_ideal = sqrt(12) / (θ_screen * sqrt(360²/12))  // 使 θ_hp ≈ θ_screen
            = 1 / (θ_screen / 58.6°)  // 简化
nside_target = min(nside_ideal_rounded_to_pow2, nside_original)
nside_target = max(nside_target, 64)  // 下限
```

**统一阈值**：全视场用同一 nside_target，不再按中心/中间/边缘分三档。

### 3. 预加载范围

`get_required_leaves` 返回 `FOV × 1.5` 内的子叶（含预加载区）。
渲染网格覆盖 `FOV × 1.2`，预加载区子叶在缓存中但不渲染（顶点在 render 范围外不生成）。

### 4. uint8 降采样

**LeafData 扩展**：
```cpp
struct LeafData {
    ...
    uint8_t* pixel_u8;  // uint8 降采样结果（owned=true 时分配）
    bool use_u8;         // true=pixel_u8 有效, false=pixel(float) 有效
};
```

**ud_grade 改造**：
- 累加用 `uint32_t`（避免溢出）
- 均值转 `uint8_t`（`min(sum/count, 255)`）
- 输出 `pixel_u8`，`use_u8=true`

**render_sphere 查值**：
- `use_u8=true` 时 `value = pixel_u8[idx] / 255.0 * data_max`（归一化到 float）
- 或直接传 uint8 给着色器，片元着色器归一化（省 CPU 转换）

### 5. 跨帧缓存失效策略

- 视角拖动：增量更新（保留仍需子叶，加载新增，清理离开预加载区的）
- FOV 变化 >10%：全清缓存（LOD 阈值变了，nside_target 变了）
- 文件切换：全清

## 验收标准

1. nside=65536 .hiss 加载后画面清晰，放大后细节增加
2. 15° FOV 下顶点数 ≈ viewport_w × viewport_h（约 77万），无欠采样
3. LOD 阈值自动匹配屏幕：15°/1034px → nside≈4096（不加载 65536 全分辨率）
4. 拖动流畅（缓存命中率高，新加载子叶数少）
5. uint8 降采样内存比 float32 省 4 倍
6. 无缝隙、无纯黑

## 影响文件

- `core/browser_backend.h` - LeafData 扩展、decide_target_nside 声明
- `core/browser_backend.cpp` - ud_grade uint8、decide_target_nside 重写、get_required_leaves 预加载范围
- `core/gl_renderer.h` - build_sphere_mesh_dynamic 声明、CachedLeaf 扩展 uint8
- `core/gl_renderer.cpp` - 动态网格构建、render_sphere 查值适配 uint8、缓存失效策略
- `widgets/abstract_view.cpp` - 视口变化时触发网格重建

## 风险

- 动态重建 VBO 可能在缩放时有短暂卡顿（单次重建 <50ms 可接受）
- uint8 降采样丢失数值精度（显示用，可接受；STF 计算仍用 float32 原始数据）
