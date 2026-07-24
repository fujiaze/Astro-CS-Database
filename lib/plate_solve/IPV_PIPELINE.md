# IPV Plate Solving 完整流程文档

> 版本: V4.9 (2026-07-04)
> 模块路径: `lib/plate_solve/cpp/ipv/`
> 入口: `IPVSolver::solve()` ([ipv_solver.cpp:296](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/lib/plate_solve/cpp/ipv/src/ipv_solver.cpp#L296))

---

## 一、关于"随机抽样"的澄清

**PROSAC 不是随机抽样**。关键事实:

| 项 | 实际情况 |
|---|---|
| 随机数生成器 | `std::mt19937 gen(42)` 固定种子 ([ipv_ransac.cpp:335](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/lib/plate_solve/cpp/ipv/src/ipv_ransac.cpp#L335)) |
| 可复现性 | 同一帧每次运行结果完全一致 |
| 采样策略 | PROSAC = **PROgressive** SAmple Consensus, 按 vote 降序优先采样高票候选, 非纯随机 |
| 失败帧根因 | 候选对里就没有足够的真实匹配 (polygon_match max_vote=5 区分度低), 不是采样运气问题 |

pass/fail 交替出现是因为**相邻帧星场内容差异**(不同滤镜/曝光/指向), 不是采样随机性。

---

## 二、整体管线 (9 阶段)

```
FITS 图像 ──► [1 StarSelector] ──► U (图像侧星点, 角秒坐标)
                                  W (星表侧星点, 角秒坐标)
                                        │
                                        ▼
                            [2 KVector 构建] (W 距离索引)
                                        │
                                        ▼
                    ┌───────────────────────────────────┐
                    │  [3] 4 flip_mode 循环并行求解      │
                    │  ┌─────────────────────────────┐  │
                    │  │ NONE / FLIP_X /             │  │
                    │  │ FLIP_Y / FLIP_XY            │  │
                    │  └─────────────────────────────┘  │
                    └───────────────────────────────────┘
                                        │
                                        ▼
                            [4] 选最优 mode (score 最大)
                                        │
                                        ▼
                            [5] WCS 输出 (CD/CRVAL/CRPIX)
```

### 主流程 `solve()` ([ipv_solver.cpp:296](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/lib/plate_solve/cpp/ipv/src/ipv_solver.cpp#L296))

| 阶段 | 耗时(典型) | 说明 |
|------|----------|------|
| 1. StarSelector | 1-14s | 选星 + Gaia 查询 (V4.9 密度公式) |
| 2. KVector 构建 | 1-3ms | W 距离索引 |
| 3. 4 flip_mode 循环 | 5-50ms | 多边形匹配 + PROSAC 验证 |
| 4. 选最优 mode | <1ms | score = n_inliers / (1 + RMS) |
| 5. WCS 输出 | <1ms | transform → CD 矩阵 |

---

## 三、阶段 1: StarSelector (`ipv_select.cpp`)

### Step 1: 读取图像
- DLL: `astro_image_io.dll` → `aio_read()`
- 输出: float* 像素数据 + 宽高
- 转 uint16 供星点检测器使用

### Step 2: 星点检测
- DLL: `star_detector.dll` → `sdet_detect_ex()`
- 内部流程:
  1. uint16 → float 转换
  2. 热像素滤波 (radius=1)
  3. 动态区域背景估计 (block=100, overlap=20, sigma=3.0, 3 轮)
  4. 二值化 (>0)
  5. 连通组件分析
  6. 候选提取 (过滤单像素、过小、形状异常)
  7. **Moffat4 拟合** (FWHM + 中心 + 通量)
  8. FWHM 统计 (中位数 + MAD)
  9. 饱和星检测 (半阈值 = (min+max)/2, 连通组件)
  10. 去重 + 排序
- 输出: x, y, flux, saturated[]

### Step 3: 图像侧选星 (U 向量组)
- 函数: `select_image_stars()` ([ipv_select.cpp](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/lib/plate_solve/cpp/ipv/src/ipv_select.cpp))
- **不对称策略** (用户硬约束):
  - 饱和星数 ≥ img_n_target (默认 50): **全选饱和星**
  - 饱和星数 < 50: 饱和星 + 非饱和补足 50 (按 flux 降序)
- 坐标转换 (角秒, 原点图像中心, **Y 轴向上**):
  ```
  U[i].x = (det_x - cx) * s0
  U[i].y = -(det_y - cy) * s0   // Y 轴翻转 (图像 Y 向下, 天球 Dec 向上)
  ```
- 其中 `s0 = 206.265 × pixel_size_um / focal_length_mm` (角秒/像素)

### Step 4: FOV/密度计算
- 函数: `compute_fov_density()` ([ipv_select.cpp:216](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/lib/plate_solve/cpp/ipv/src/ipv_select.cpp#L216))
- 公式:
  ```
  s0 = 206.265 × pixel_size / focal_length        (角秒/像素)
  FOV_diag = sqrt(w² + h²) × s0 / 3600             (度)
  query_radius = FOV_diag × gaia_query_radius_factor (默认 0.55)
  query_area = π × query_radius²                    (平方度)
  img_area = (w × s0 / 3600) × (h × s0 / 3600)     (平方度)
  rho_img = N_img / img_area                        (图像面密度)
  rho_target = gaia_density_ratio × rho_img         (默认 1.2×)
  n_target = ratio × N_img × (query_area / img_area)
  ```
- **n_target 上限** (V4.9):
  - 宽 FOV (>3°): **150** (星密导致六边形形状碰撞, 降上限提高区分度)
  - 窄/中 FOV: 300
  - 下限: 50

### Step 5: V4.9 密度公式估算极限星等 ⭐ 新增
- 函数: `estimate_mag_lim_by_density()` ([ipv_select.cpp:315](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/lib/plate_solve/cpp/ipv/src/ipv_select.cpp#L315))
- **用户指导**: "根据天球平均星点密度, 搞一个根据视场角自动估算需要的极限星等的公式, 保证查出来的星比需要的多就行。初始只需要很小的极限星等"
- 模型 (Gaia DR3 G 波段近似):
  ```
  ρ(G) = 5 × 10^(1.3×(G-10))  颗/平方度
  (G=10 → 5, G=12 → 30, G=14 → 150, G=16 → 800, G=18 → 4000)
  ```
- 反解:
  ```
  G = 10 + log10(ρ_target / 5) / 1.3
  ρ_target = n_required / area_sqdeg
  ```
- **安全余量**: +0.5 mag (放宽星等, 保证查出的星数 > n_required)
- clip 到 [6, 18]
- **替代 V4.8 的 6 次迭代查询**, 一次估算 + 一次查询搞定

### Step 6: Gaia 锥形查询 (一次查询 + 补救)
- DLL: `gaia_client.dll` → `gaia_client_cone_search_for_solver()`
- **主查询**: 用 Step 5 估算的 m_lim
- **补救机制** (最多 2 次):
  - 若返回星数 < n_target: 放宽 m_lim + 1.0 mag 重查
  - 仍不足: 再 + 1.0 mag
- **最终兜底**: mag=22

### Step 7: Gnomonic 投影 + FOV 内过滤
- 函数: `gnomonic_forward_proj()`
- 投影: TAN (gnomonic) 投影到切平面 (xi, eta 角秒)
- 过滤: |xi| < fov_half_w 且 |eta| < fov_half_h
- 不足 2 颗: 放宽到 1.5×FOV

### Step 8: 按星等升序取前 n_target 颗 → W 向量组
- 最亮优先
- 输出 W[i] = {x=xi, y=eta, flux=0, saturated=false}

---

## 四、阶段 2: KVector 构建 (`ipv_kvector.cpp`)

- 函数: `kvector_build(W)`
- 对 W 构建距离索引 (加速范围查询)
- 输出: `KVectorIndex { n_stars, n_pairs, d_min, d_max, ... }`
- **在原始 W 上构建** (镜像翻转不改变星对距离)
- 耗时: ~1-3ms

---

## 五、阶段 3: 4 flip_mode 循环 (`solve_flip_mode`)

### 为什么需要 4 个 flip_mode?
图像侧 U 的 Y 轴已翻转 (天球 Dec 向上), 但星表侧 W 的坐标系方向未知 (取决于相机朝向)。4 种镜像模式覆盖所有可能:
- mode 0 (NONE): 无翻转
- mode 1 (FLIP_X): x → -x
- mode 2 (FLIP_Y): y → -y
- mode 3 (FLIP_XY): x → -x, y → -y

每个 mode 独立求解, 取 score 最大者。

### 单个 flip_mode 流程 (`solve_flip_mode` [ipv_solver.cpp:95](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/lib/plate_solve/cpp/ipv/src/ipv_solver.cpp#L95))

#### 5.1 apply_flip
- 对 W 应用镜像得到 W'
- 耗时: <1ms

#### 5.2 多边形匹配
- **宽 FOV (>3°)**: `polygon_match_adaptive()` — 自适应降阶 (六边形→五边形→四边形→三角形) + 邻星局部化 (r_local=0.15×fov_diag) 抗畸变
- **窄/中 FOV**: `polygon_match()` — 标准六边形匹配 (1 中心星 + 5 邻星)
- 流程:
  1. 选 30 个 pivot (U 侧)
  2. 每个 pivot 取 K=5 邻星组成六边形
  3. k-vector 查询 W 侧距离匹配的星对
  4. 比较六边形形状相似性 (边长 + 角度)
  5. 通过形状约束的候选对累加投票
- 输出: `PolygonMatchResult { votes, max_vote, n_polygon_passed, candidates }`
- **剪枝** (用户硬约束):
  - 三角形: 去除等边三角形 (特征不明显) 和过扁三角形
  - 多边形: **只构造单层, 不嵌套**

#### 5.3 几何投票 (仅窄/中 FOV)
- 函数: `geometric_vote()`
- **宽 FOV 跳过** (用户硬约束): O(N²) 投票在密集星场产生大量高票错误配对, 淹没真实匹配 (Galaxy_Center_02 问题根因)
- 窄/中 FOV: 累加到 polygon_result.votes

#### 5.4 共识提取
- 函数: `extract_consensus(votes, N_U, params)`
- 阈值: vote ≥ vote_threshold (默认 2)
- 输出: `CandidateMatch { u_idx, w_idx, vote }` 列表
- 候选数 < 4: 直接失败 (无法解相似变换)

#### 5.5 PROSAC 验证 ⭐ 核心
- 函数: `prosac_verify()` ([ipv_ransac.cpp:310](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/lib/plate_solve/cpp/ipv/src/ipv_ransac.cpp#L310))
- **模型**: `W = s·R(θ)·U + t` (相似变换, 4 参数: s, θ, tx, ty)
- **随机数**: `std::mt19937 gen(42)` 固定种子, **可复现**
- **三阶段采样**:

| 阶段 | 策略 | 说明 |
|------|------|------|
| 1. 贪心 | top-K (K=15) 全组合 | 优先采高票候选, 105 对 |
| 2. 锚点固定 | 固定 top-1, 枚举其余 | 覆盖 top-1 是真匹配的情况 |
| 3. 迭代 | 池扩展 (T0=M/2 → M) | 100 次无改进则终止 |

- **每对候选**:
  1. `solve_similarity_transform()`: 2 对匹配解析求解 (s, θ, t)
  2. 尺度约束: s ∈ [0.9, 1.1] (用户硬约束: ±10%)
  3. 全量内点验证: |W_pred - W_actual| < τ (默认 3.0")
  4. 记录 best_n_inliers / best_RMS
- **早停条件**: n_inliers ≥ 4 且 RMS < 1.5"
- **Umeyama SVD 精化**: 用所有内点做闭合解 (手写 2x2 SVD, 不依赖 Eigen)
- 成功条件: `n_inliers ≥ 4 且 RMS < 5.0"`

#### 5.6 全量验证 (full_verify_transform)
- 动机: PROSAC 只验证 candidates 中的对, 真实匹配可能不在 candidates (vote < threshold)
- 用 PROSAC 最优变换对所有 (u, w) 做最近邻匹配
- 阈值: 1.5×τ (容纳变换精度误差)
- 找到更多内点 → Umeyama 精化 → 二次全量验证 (用原始 τ 收紧)

### 单 mode 结果
```cpp
FlipModeResult {
    mode, success, score,  // score = n_inliers / (1 + RMS)
    polygon,               // 候选 + 票数
    prosac                 // transform + inliers + RMS
}
```

---

## 六、阶段 4-5: 选最优 + WCS 输出

### 选最优 mode
- score = n_inliers / (1 + RMS)
- 取 4 个 mode 中 success=true 且 score 最大者
- 全失败: 返回 fail_result (best_mode=-1)

### WCS 输出 (`ipv_wcs.cpp`)
- 函数: `build_wcs()`
- 输入: 最优 mode 的 transform (s, θ, tx, ty) + W' + s0 + CRVAL + CRPIX
- 输出: **标准 WCS CD 矩阵** (无 1/cos(Dec) 因子, 用户硬约束)
- CRVAL = (ra0, dec0) 来自 FITS header OBJCTRA/OBJCTDEC
- CRPIX = (img_w/2 + 0.5, img_h/2 + 0.5)

---

## 七、V4.9 性能数据 (全量 790 帧测试)

### 整体
| 指标 | V4.8 (迭代) | V4.9 (密度公式) |
|------|------------|----------------|
| Victory_T4 单帧 | 87s | **14.22s** |
| StarSelector | 43.4s (6 次查询) | **14.3s (1 次查询)** |
| Gaia 查询次数 | 6 | **1** (+ 最多 2 次补救) |
| 极限星等 | 迭代收敛 | **G=10.353** (密度公式) |

### 全量 790 帧
| FOV 类别 | 成功 | 失败 | 成功率 |
|---------|------|------|-------|
| narrow | 38 | 0 | 100% ✅ |
| medium | 366 | 1 | 99.7% ✅ |
| **wide** | **154** | **231** | **40.0%** ❌ |
| **总计** | **558** | **232** | **70.6%** |

- 总耗时: 23.1min, 平均 1.74s/帧
- RMS (成功帧): 中位 1.357px, 均值 1.175px, 最大 2.132px

### 宽 FOV 失败根因 (待解决)
- 失败帧 inliers=0, mode=-1 (4 个 flip_mode 全失败)
- polygon_match max_vote=5 (区分度低)
- N_W=300 满载 → 六边形形状碰撞
- **V4.9 已修改**: 宽 FOV n_target 上限 300→150 (尚未编译验证)

---

## 八、参数清单 (`IPVSolverParams`)

| 参数 | 默认值 | 说明 |
|------|--------|------|
| polygon_sides | 6 | 多边形边数 |
| n_pivot | 30 | pivot 数量 |
| sigma_d_arcsec | 0.0 | 距离容差 (0=自适应) |
| vote_threshold | 2 | 投票阈值 |
| ransac_max_iter | 2000 | PROSAC 最大迭代 |
| ransac_inlier_threshold_arcsec | 3.0 | 内点阈值 (角秒) |
| gaia_density_ratio | 1.2 | Gaia/图像密度比 |
| gaia_query_radius_factor | 0.55 | 查询半径 = FOV_diag × 此值 |
| img_n_target | 50 | 图像侧目标星数 |
| s_min | 0.9 | 尺度下限 (用户: ±10%) |
| s_max | 1.1 | 尺度上限 |
| log_dir | "" | 日志目录 (空=stderr) |

---

## 九、DLL 依赖

| DLL | 用途 |
|-----|------|
| `astro_image_io.dll` | FITS 读取 |
| `star_detector.dll` | 星点检测 (Moffat4 拟合 + 饱和星) |
| `gaia_client.dll` | Gaia DR3 锥形查询 |
| `ipv_solver.dll` | 本模块 (含 StarSelector + KVector + Polygon + PROSAC + WCS) |

---

## 十、关键设计决策 (用户硬约束)

1. **向量匹配使用 gnomonic 投影** (TAN)
2. **Y 轴翻转**: 图像 Y 向下, 天球 Dec 向上
3. **候选半径 = 0.5×FOV 对角线** (防止真匹配被排除)
4. **PROSAC 尺度约束**: s ∈ [0.9, 1.1] (±10%)
5. **Umeyama SVD 替代第二次 RANSAC**
6. **C++ 性能 ≥ Python 版本**
7. **饱和星全选** (星表侧/图像侧对等)
8. **三角形剪枝**: 去除等边 + 过扁
9. **多边形单层构造**: 不嵌套
10. **宽 FOV 跳过 geometric_vote**: 只用 polygon_match
11. **V4.9 密度公式估算极限星等**: 替代迭代查询
12. **V4.9 宽 FOV n_target 上限 150**: 提高区分度
