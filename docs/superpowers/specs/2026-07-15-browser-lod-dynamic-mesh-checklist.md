# HEALPix 浏览器 LOD + 动态网格重构 checklist

## 阶段1: 动态网格重建（根因修复）

- [ ] 1.1 实现 `build_sphere_mesh_dynamic(view, viewport_w, viewport_h)`
  - 计算 θ_screen = FOV / min(viewport_w, viewport_h)
  - 网格范围 = FOV × 1.2，中心 = (center_ra, center_dec)
  - lat/lon segments = render_range / θ_screen
  - 顶点坐标 = 球面笛卡尔（单位球）
  - 重建 VBO/IBO
- [ ] 1.2 render_sphere 调用动态网格（FOV 变化时重建）
- [ ] 1.3 abstract_view 视口变化时触发重建（resizeEvent）
- [ ] 1.4 移除固定 build_sphere_mesh(256,512) 调用

## 阶段2: LOD 自动阈值

- [ ] 2.1 重写 `decide_target_nside(view, viewport_w, viewport_h)`
  - θ_screen = FOV / min(viewport_w, viewport_h)
  - nside_ideal = round(58.6 / θ_screen) 向下取整到 2 的幂
  - nside_target = clamp(nside_ideal, 64, nside_original)
  - 统一阈值，不按距离分档
- [ ] 2.2 render_sphere 传 viewport 给 backend
- [ ] 2.3 验证 15°/1034px → nside≈4096

## 阶段3: 预加载范围

- [ ] 3.1 get_required_leaves 返回 FOV×1.5 内子叶
- [ ] 3.2 渲染网格只覆盖 FOV×1.2（预加载区不渲染顶点）
- [ ] 3.3 验证预加载区子叶在缓存中，拖动时命中

## 阶段4: uint8 降采样

- [ ] 4.1 LeafData 增加 `uint8_t* pixel_u8` 和 `bool use_u8`
- [ ] 4.2 ud_grade 改用 uint32 累加 + uint8 输出
- [ ] 4.3 release_leaf 释放 pixel_u8
- [ ] 4.4 render_sphere 查值适配：use_u8 时转 float
- [ ] 4.5 CachedLeaf 扩展支持 uint8
- [ ] 4.6 验证内存占用降低

## 阶段5: 缓存失效策略

- [ ] 5.1 FOV 变化 >10% 时全清 leaf_cache_
- [ ] 5.2 拖动时增量更新（保留仍需子叶）
- [ ] 5.3 验证拖动流畅度

## 阶段6: 验证

- [ ] 6.1 nside=65536 .hiss 加载画面清晰
- [ ] 6.2 放大后细节增加（LOD 下钻）
- [ ] 6.3 15° FOV 顶点数 ≈ 77万
- [ ] 6.4 无缝隙、无纯黑
- [ ] 6.5 拖动/缩放流畅
- [ ] 6.6 STF 拉伸正常（用 float32 原始数据计算）
