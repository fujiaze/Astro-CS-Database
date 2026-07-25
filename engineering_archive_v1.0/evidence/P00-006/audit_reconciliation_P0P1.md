# P0+P1 审计问题复核报告（2026-07-18 审计 → 2026-07-24 复核）

> **审计来源**: `docs/superpowers/specs/2026-07-18-audit-findings-P0P1.md`
> **复核日期**: 2026-07-24
> **复核范围**: 9 个模块共 50 项 P0+P1 问题（19 Critical + 31 High）
> **复核方法**: 逐项读取 `lib/` 下当前源码对应位置，判断问题是否仍存在

---

## 一、摘要统计

| 状态 | 数量 | 含义 |
|------|------|------|
| **OPEN** | 44 | 问题仍存在，源码未修复 |
| **CLOSED** | 6 | 已解决，源码已修复或功能等价实现 |
| **STALE** | 0 | 路径/架构变化导致问题不再适用 |
| **UNVERIFIED** | 0 | 源码/数据缺失，无法验证 |
| **REJECTED** | 0 | 原硬约束无有效来源或被 ADR 否决 |
| **合计** | **50** | |

### 各模块问题分布

| 模块 | 编号前缀 | Critical | High | 小计 | OPEN | CLOSED |
|------|----------|----------|------|------|------|--------|
| astro_image_io | B1 | 1 | 4 | 5 | 4 | 1 |
| calibration | B2 | 4 | 4 | 8 | 8 | 0 |
| plate_solve | B3 | 8 | 5 | 13 | 9 | 4 |
| dynamic_psf | B4 | 1 | 2 | 3 | 3 | 0 |
| photometric_calib | B5 | 0 | 2 | 2 | 2 | 0 |
| snr_estimator | B6 | 0 | 2 | 2 | 2 | 0 |
| healpix_stack | B8 | 1 | 5 | 6 | 6 | 0 |
| orchestrator | B9 | 4 | 7 | 11 | 10 | 1* |
| **合计** | | **19** | **31** | **50** | **44** | **6** |

> *注：B9-H-4 与 B8-H-3 同源（STACK 空骨架），均计为 OPEN。

### 已关闭（CLOSED）项目一览

| ID | 标题 | 关闭理由 |
|----|------|----------|
| B1-H-3 | XISF 格式错误处理不完整 | 错误分支已补全 fclose + aio_log，无资源泄漏 |
| B3-C-03 | RANSAC 缺少比例预检查 | 已有 s_min/s_max 尺度约束（±10%），功能等价于 scale 预检查 |
| B3-C-04 | 精化阶段使用固定阈值 | 已用 1.4826×MAD + Tukey biweight 动态权重 |
| B3-C-08 | 解析统计字段硬编码为 0 | n_matched 已正确填充，原字段已改名/移除 |
| B3-H-03 | 迭代变换收敛判定不合理 | 已有 max_iterations + HALT_SIGMA 收敛判定 |
| B3-H-04 | WCS 写回未强制 CD 矩阵无 1/cos(Dec) | CD 矩阵用旋转矩阵元素，无 1/cos(Dec) 因子 |

---

## 二、详细复核结果（按模块分组）

### 模块 1：astro_image_io（B1，5 项）

#### B1-C-1 ｜ Critical ｜ OPEN
- **标题**: 管线阶段枚举仅 5 个，与 9 节点架构不一致
- **定位**: `lib/astro_image_io/include/aio_pipeline.h:26-32`
- **证据**: `PipelineStage` 枚举仍为 5 个值（`STAGE_CALIBRATE=0`/`STAGE_PLATESOLVE=1`/`STAGE_PHOTOMETRIC=2`/`STAGE_DRIZZLE=3`/`STAGE_STACK=4`），未扩展到 9 节点架构（缺 `READ_FITS`/`PSF`/`SNR`/`GRADIENT_SPHERE`）。

#### B1-H-1 ｜ High ｜ OPEN
- **标题**: 大量错误日志直接输出到 stderr，未走模块日志系统
- **定位**: `lib/astro_image_io/src/`
- **证据**: 经 grep `fprintf\s*\(\s*stderr` 检查 7 个文件共 207 处 `fprintf(stderr)` 调用（aio_pipeline_engine.cpp:28/aio_compressor.cpp:16/aio_ahpx_writer.cpp:26/aio_healpix_io.cpp:91/aio_ahpx_reader.cpp:25/aio_ahpx_api.cpp:20/aio_log.cpp:1），未走 `aio_log_*` 统一日志系统。

#### B1-H-2 ｜ High ｜ OPEN
- **标题**: FITS 关键字段提取不完整，丢失观测元数据
- **定位**: `lib/astro_image_io/src/aio_fits.cpp:320,333,368,369,379`
- **证据**: 已提取 LONPOLE/LATPOLE/DATE-OBS/EXPTIME/FILTER/BUNIT 字段，但 JD-OBS（儒略日）仍未提取，审计要求的观测元数据部分缺失。

#### B1-H-3 ｜ High ｜ CLOSED
- **标题**: XISF 格式错误处理不完整，可能崩溃
- **定位**: `lib/astro_image_io/src/aio_xisf.cpp:307-346`
- **证据**: `xisf_read_file` 函数错误处理已补全：每个错误分支均有 `std::fclose(fp)` 释放资源 + `aio_log(AIO_LOG_ERROR,...)` 记录错误，无资源泄漏。

#### B1-H-4 ｜ High ｜ OPEN
- **标题**: 命名块覆盖语义不清，可能丢失数据
- **定位**: `lib/astro_image_io/src/aio_pipeline_engine.cpp:43`
- **证据**: 仅注释"自定义块丢弃策略（覆盖默认策略）"，`aio_pipeline.h` 的 `aio_frame_add_block` API 对同名块覆盖/追加/报错语义未在文档和代码中明确；`kv_set` 虽明确"若 key 已存在则覆盖"但普通 `add_block` 未明确。

---

### 模块 2：calibration（B2，8 项）

#### B2-C-1 ｜ Critical ｜ OPEN
- **标题**: 校准统计信息从未输出（cal_stats 命名块缺失）
- **定位**: `lib/calibration/src/ac_api.cpp:101-117`
- **证据**: `ac_correct_frame` 函数全程无 KV 块输出，未调用 `aio_frame_kv_set(frame_, "cal_stats", ...)` 输出校准统计信息。

#### B2-C-2 ｜ Critical ｜ OPEN
- **标题**: 坏点修复功能已实现但从未被调用
- **定位**: `lib/calibration/src/calibrator.cpp:203-236`
- **证据**: `calibrate` 函数主路径仅做 dark/flat/bias 校准，未调用 `ac::correct_frame`（坏点修复）；坏点修复仅作为独立 API `ac_correct_frame` 存在，未集成到校准主流程。

#### B2-C-3 ｜ Critical ｜ OPEN
- **标题**: 无主帧时静默退化，未标记状态（GAP-020 未修复）
- **定位**: `lib/calibration/src/calibrator.cpp:223-233`
- **证据**: 当 dark/flat/bias 全为 nullptr 时直接 `v=light[i]` 返回原图，无任何日志或状态标记告知下游"本次未实际校准"。

#### B2-C-4 ｜ Critical ｜ OPEN
- **标题**: 两套构建脚本产物不一致
- **定位**: `lib/calibration/Makefile` vs `lib/calibration/build.ps1`
- **证据**: Makefile:2 用 `-O3 -ffast-math -funroll-loops` 仅编译 `cosmetic_corrector.cpp` 输出 `cosmetic_corrector.dll`；build.ps1:30 用 `-O2` 编译 4 个文件（master_generator/calibrator/cosmetic_corrector/ac_api）输出 `astro_calibration.dll`；两套脚本编译选项/源文件/输出 DLL 名均不一致。

#### B2-H-1 ｜ High ｜ OPEN
- **标题**: 校准模块线程数硬编码 16，覆盖外部设置
- **定位**: `lib/calibration/src/calibrator.cpp:87,155,217,226`
- **证据**: 共 4 处 `#pragma omp parallel for schedule(static) num_threads(16)` 硬编码 16 线程。

#### B2-H-2 ｜ High ｜ OPEN
- **标题**: ac_set_num_threads API 形同虚设
- **定位**: `lib/calibration/src/ac_api.cpp:119-120`
- **证据**: `ac_set_num_threads` 已调用 `omp_set_num_threads(n)` 使 API 生效，但 `calibrator.cpp` 内部 `num_threads(16)` 子句覆盖全局设置，API 实际仍无效。

#### B2-H-3 ｜ High ｜ OPEN
- **标题**: 坏点修复功能日志缺失
- **定位**: `lib/calibration/src/cosmetic_corrector.cpp:118-154`
- **证据**: `detect_hot_pixels`/`detect_cold_pixels` 函数及 `correct_frame` 主入口均无 LOG/printf/fprintf 日志输出，修复多少坏点/修复前后统计无记录。

#### B2-H-4 ｜ High ｜ OPEN
- **标题**: 主帧生成无质量校验
- **定位**: `lib/calibration/src/master_generator.cpp:37-43`
- **证据**: 仅有 `ac_log` 输出到 stderr，生成主帧后无 mean/std/坏像素比例等质量校验统计。

---

### 模块 3：plate_solve（B3，13 项）

#### B3-C-01 ｜ Critical ｜ OPEN
- **标题**: 候选星数量上限硬编码为 60，违反应为 100 的硬约束
- **定位**: `lib/plate_solve/cpp/ipv/src/ipv_select.cpp:280`
- **证据**: `int n_target_cap = 60;` 硬编码 60，违反硬约束"U组限流: max=100（解决 LDN43 候选爆炸）"。

#### B3-C-02 ｜ Critical ｜ OPEN
- **标题**: 候选半径用 0.55 倍视场对角线，违反应为 0.5 倍硬约束
- **定位**: `lib/plate_solve/cpp/ipv/include/ipv_types.h:200`
- **证据**: `double gaia_query_radius_factor = 0.55;` 默认值 0.55，违反硬约束"Candidate radius for matching must be 0.5×FOV diagonal"。

#### B3-C-03 ｜ Critical ｜ CLOSED
- **标题**: RANSAC 缺少比例预检查，导致明显错误匹配污染结果
- **定位**: `lib/plate_solve/cpp/ipv/src/ipv_ransac.cpp:398,521,632`
- **证据**: 有 `if (tf.s < params.s_min || tf.s > params.s_max) continue;` 尺度约束，`ipv_types.h:191` `s_min=0.90`/`s_max=1.10`（±10% 符合项目硬约束）；功能等价于 scale 预检查，丢弃尺度异常的匹配对。

#### B3-C-04 ｜ Critical ｜ CLOSED
- **标题**: 精化阶段使用固定阈值，违反应使用动态阈值硬约束
- **定位**: `lib/plate_solve/cpp/ipv/src/ipv_robust_refine.cpp:600-609,695`
- **证据**: `compute_mad_sigma` 返回 1.4826×MAD，`irls_fit_one_step` 用 Tukey biweight 权重基于 MAD 动态计算，已实现动态阈值而非固定阈值。

#### B3-C-05 ｜ Critical ｜ OPEN
- **标题**: RANSAC 内点校验只查位置，违反应同时查方向硬约束
- **定位**: `lib/plate_solve/cpp/ipv/src/ipv_ransac.cpp`
- **证据**: 经 grep `cross|叉积|direction` 无匹配，内点校验仅用欧氏距离（位置），无向量叉积（方向）校验。

#### B3-C-06 ｜ Critical ｜ OPEN
- **标题**: Umeyama 拟合缺少 5 轮 MAD 离群值剔除
- **定位**: `lib/plate_solve/cpp/ipv/src/ipv_robust_refine.cpp:974`
- **证据**: IRLS 迭代用 Tukey biweight 降权而非审计要求的"5 轮 MAD 离群值剔除循环（threshold: max(5", 3×1.4826×MAD)）"，Umeyama 拟合后无明确 5 轮剔除。

#### B3-C-07 ｜ Critical ｜ OPEN
- **标题**: 验证集未限制为 1000 颗最亮 Gaia 星
- **定位**: `lib/plate_solve/cpp/ipv/src/ipv_entry.cpp`
- **证据**: 经 grep `1000|brightest|最亮|mag.*sort|截断` 仅 :147 "截断保护"，无按星等升序截断 1000 颗最亮星的逻辑。

#### B3-C-08 ｜ Critical ｜ CLOSED
- **标题**: 解析统计字段硬编码为 0，无法反映实际匹配数
- **定位**: `lib/plate_solve/cpp/ipv/src/ipv_solver.cpp:344`
- **证据**: `result.n_matched = (int)inliers_cur.size();` 已正确填充匹配数；`ipv_types.h` 中无 n_detected/n_catalog 字段，原问题所述字段已改名/移除为 n_matched。

#### B3-H-01 ｜ High ｜ OPEN
- **标题**: 主流程用三角匹配，与文档描述的多边形匹配+PROSAC 不一致
- **定位**: `lib/plate_solve/cpp/ipv/src/ipv_solver.cpp:6,449`
- **证据**: 注释"triangle_match（三角形匹配，替代 polygon_match）"，:449 调用 `triangle_match`；文档 IPV_PIPELINE.md 描述为 polygon_match+PROSAC，代码与文档不一致。

#### B3-H-02 ｜ High ｜ OPEN
- **标题**: K-vector 索引构建无验证
- **定位**: `lib/plate_solve/cpp/ipv/src/ipv_k_vector.cpp`
- **证据**: 经 grep `自检|verify|self_test|sanity|check` 无匹配，`kvector_build` 后无完整性校验或自检。

#### B3-H-03 ｜ High ｜ CLOSED
- **标题**: 迭代变换收敛判定不合理
- **定位**: `lib/plate_solve/cpp/ipv/src/ipv_itertrans.cpp:323-339,439,550,997`
- **证据**: 有 `max_iterations` 参数和 `HALT_SIGMA` 收敛判定，:439 while 循环，:550 HALT_SIGMA 检查，:997 `max_iterations=5`，已区分收敛（`is_ok=true`）与达到最大迭代。

#### B3-H-04 ｜ High ｜ CLOSED
- **标题**: WCS 写回未强制 CD 矩阵无 1/cos(Dec) 因子
- **定位**: `lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp:116-119,155-158,271`
- **证据**: CD 矩阵用 `cos_t`/`sin_t`（旋转矩阵元素），:271 注释"CD = 线性项/3600（度/像素），直接提取，不用 M^-1"，无 1/cos(Dec) 因子。

#### B3-H-05 ｜ High ｜ OPEN
- **标题**: SIP 多项式阶数自动选择缺失
- **定位**: `lib/plate_solve/cpp/ipv/src/ipv_sip.cpp:64,293`
- **证据**: `get_basis_table(order)` 接收 order 参数，:293 `n_pairs<7` 时降级 `order=0`，但阶数由调用方传入，无基于图像畸变程度的自动选择逻辑。

---

### 模块 4：dynamic_psf（B4，3 项）

#### B4-C-1 ｜ Critical ｜ OPEN
- **标题**: 缺少高斯 PSF 备选方案
- **定位**: `lib/dynamic_psf/src/dpsf_psf.cpp:392,443`
- **证据**: 仅 `moffat4_fit` 和 `dpsf_fit_batch`，grep `gauss|高斯|Gaussian|fallback|备选` 仅 `gauss_solve`（线性方程求解器）非 PSF 拟合，无高斯 PSF 备选方案。

#### B4-H-1 ｜ High ｜ OPEN
- **标题**: PSF 拟合中心位置存在系统偏差约 0.5 像素
- **定位**: `lib/dynamic_psf/src/dpsf_psf.cpp:367-368,378`
- **证据**: `double img_cx = x0 + (rect_x0 + rect_x1) / 2.0; double img_cy = y0 + (rect_y0 + rect_y1) / 2.0;` 用 patch 几何中心而非传入的 `cx`/`cy` 参数，:378 `result->cx = img_cx`。

#### B4-H-2 ｜ High ｜ OPEN
- **标题**: float↔uint16 双重精度损失 + 性能开销
- **定位**: `lib/dynamic_psf/src/dpsf_psf.cpp:392`
- **证据**: `DPSF_EXPORT int dpsf_fit(const uint16_t *image, ...)` 入口参数为 `uint16_t`，调用方必须将 float 转 uint16，存在精度损失；`dpsf_image.cpp` 内部虽全程 float 但入口仍 uint16。

---

### 模块 5：photometric_calib（B5，2 项）

#### B5-H-1 ｜ High ｜ OPEN
- **标题**: 测光模块未接收 Gaia 星表块，违反架构契约
- **定位**: `lib/photometric_calib/cpp/src/pc_api.cpp:223`
- **证据**: 调用 `gaia_client_cone_search_with_spectrum` 直接查询 Gaia 客户端，而非从管线帧 `gaia_cat` 块读取，重复查询且数据可能不一致。

#### B5-H-2 ｜ High ｜ OPEN
- **标题**: 光谱积分步长 1.0nm，与算法文档要求的 0.1nm 不一致
- **定位**: `lib/photometric_calib/cpp/src/spectrum_integrator.cpp:241`
- **证据**: `const double wl_step = 1.0;` 步长 1.0nm，未改为算法文档要求的 0.1nm。

---

### 模块 6：snr_estimator（B6，2 项）

#### B6-H-1 ｜ High ｜ OPEN
- **标题**: 架构文档数据流表未更新（snr 块 vs snr_model 块）
- **定位**: `docs/ARCHITECTURE.md:113`
- **证据**: 仍写 `snr (FLOAT32[H,W])`，未更新为 `snr_model`（RAW）块，与代码实际输出格式不符。

#### B6-H-2 ｜ High ｜ OPEN
- **标题**: GAP-011 状态未更新为已修复
- **定位**: `docs/DESIGN_IMPL_GAP.md:103,147,145`
- **证据**: GAP-011 标题"已源代码回溯复核 2026-07-16"，:147 复核记录确认问题存在，:145 批复意见说明设计要求，但状态字段未更新为"已修复"。

---

### 模块 7：healpix_stack（B8，6 项）

#### B8-C-1 ｜ Critical ｜ OPEN
- **标题**: 球面梯度校正在回退路径丢失 Winsorized 参数
- **定位**: `lib/healpix_db/healpix_stack/hp_stack_api.cpp:510-511,534-535`
- **证据**: 回退路径调用 `hp_stack_hiss(hiss_paths, n_frames, output_hcsd_path, sigma, max_iter)` 未透传 `use_winsorized`/`winsorize_low_pct`/`winsorize_high_pct` 参数。

#### B8-H-1 ｜ High ｜ OPEN
- **标题**: 梯度拟合最近控制点查找是 O(n²) 性能瓶颈
- **定位**: `lib/healpix_db/healpix_stack/gradient/gradient_fitter.cpp:16,179`
- **证据**: 注释"暴力查找复杂度 O(n_i × n_j)，对 n=500，10帧 ~ 2.5s，可接受"，:179 暴力遍历找最近控制点，无 KD-tree 加速。

#### B8-H-2 ｜ High ｜ OPEN
- **标题**: 文档说 Gauss-Seidel 迭代拟合，实际是一次性拟合
- **定位**: `lib/healpix_db/healpix_stack/healpix_stack.py:422,435` / `hp_stack_api.cpp:443,465` / `gradient_fitter.h:19`
- **证据**: 文档说"Gauss-Seidel 迭代拟合"和"gradient_max_iter: Gauss-Seidel 最大迭代"；`hp_stack_api.cpp` 也有 Gauss-Seidel 提及和 `gradient_max_iter` 参数，但实际 `gradient_fitter` 是一次性球面样条拟合（`gradient_fitter.h:19` 明确"一次性，无 Gauss-Seidel 迭代"）。

#### B8-H-3 ｜ High ｜ OPEN
- **标题**: STACK 阶段是空骨架，与架构文档不符（GAP-015 未修复）
- **定位**: `lib/orchestrator/cpp/src/orchestrator.cpp:2462-2476`
- **证据**: `run_stage_stack` 函数体仅 `LOG_INFO("[STACK] 跳过: .hcsd 已由 GRADIENT_SPHERE 生成")` + `return true`，空骨架无实际逻辑。

#### B8-H-4 ｜ High ｜ OPEN
- **标题**: .hiss 多帧堆叠路径无 Winsorized 选项
- **定位**: `lib/healpix_db/healpix_stack/hp_stack_hiss.cpp:240,264`
- **证据**: 仅实现 weighted mean+std sigma-clip，grep `winsorized|winsorize` 无匹配，无 Winsorized sigma-clip 选项。

#### B8-H-5 ｜ High ｜ OPEN
- **标题**: 旧版堆叠引擎用硬编码 3 次迭代，无 Winsorized
- **定位**: `lib/healpix_db/healpix_stack/stack_engine.cpp:63`
- **证据**: `for (int iter = 0; iter < 3; iter++)` 硬编码 3 次迭代，无 `max_iter` 参数，无 Winsorized 分支。

---

### 模块 8：orchestrator（B9，11 项）

#### B9-C-1 ｜ Critical ｜ OPEN
- **标题**: CALIBRATE 阶段未写入校准统计命名块（与 B2-C-1 同源）
- **定位**: `lib/orchestrator/cpp/src/orchestrator.cpp`
- **证据**: 经 grep `cal_stats` 无匹配，`run_stage_calibrate` 全程未调用 `aio_frame_kv_set(frame_, "cal_stats", ...)`，与 B2-C-1 同源。

#### B9-C-2 ｜ Critical ｜ OPEN
- **标题**: 编排器调用 Gaia 查询未限制 1000 颗最亮星（与 B3-C-07 同源）
- **定位**: `lib/orchestrator/cpp/src/orchestrator.cpp:1515,1919`
- **证据**: 调用 `gaia_client_cone_search_for_solver` 和 `pc_calibrate_simple_with_gaia` 后无按星等截断 1000 颗逻辑，grep `1000|brightest|最亮` 无匹配。

#### B9-C-3 ｜ Critical ｜ OPEN
- **标题**: 任务队列大小限制为 2 未实现
- **定位**: `lib/orchestrator/cpp/src/orchestrator.cpp`
- **证据**: 经 grep `queue|task_queue|backpressure|max_queue` 无匹配，无任务队列抽象，串行执行无队列限制。

#### B9-C-4 ｜ Critical ｜ OPEN
- **标题**: PLATESOLVE/PSF 阶段像素值截断丢失精度
- **定位**: `lib/orchestrator/cpp/src/orchestrator.cpp:1429-1437,1606-1615`
- **证据**: `if (v > 65535.0f) v = 65535.0f; pixels_u16[i] = static_cast<uint16_t>(v);` float→uint16 截断丢失精度。

#### B9-H-1 ｜ High ｜ OPEN
- **标题**: 配置文件字段解析不完整，多个配置项无法生效
- **定位**: `lib/orchestrator/cpp/src/orchestrator.cpp:7,68,266`
- **证据**: `load_config` 注释"简单读取 JSON 文件（骨架）"和"简易 JSON 字段提取（避免引入 nlohmann::json 依赖）"，`calibration_dir`/`filter`/`threads`/`gaia_data_dir`/`output_root` 等字段未解析。

#### B9-H-2 ｜ High ｜ OPEN
- **标题**: PHOTOMETRIC 阶段创建未使用的星检测器/解析器句柄（资源浪费）
- **定位**: `lib/orchestrator/cpp/src/orchestrator.cpp:1739`
- **证据**: "确保 PLATESOLVE 环境已初始化（复用 gaia_client_handle_）"，PHOTOMETRIC 阶段调用 `init_platesolve_env` 创建 GaiaClient+StarDetector+IPVSolver 三个实例，后两者未使用。

#### B9-H-3 ｜ High ｜ OPEN
- **标题**: Gaia API 返回值约定不统一（布尔 vs 错误码）
- **定位**: `lib/orchestrator/cpp/src/orchestrator.cpp:966-968`
- **证据**: 注释"gaia_client_get_spectrum_params 使用布尔约定（1=成功，0=失败），而非错误码约定"，:968 `if (ret != 1 || ...)`；其他 API（如 `gaia_client_cone_search_for_solver`）用 0=成功约定，返回值不统一。

#### B9-H-4 ｜ High ｜ OPEN
- **标题**: STACK 阶段空骨架（与 B8-H-3 同源）
- **定位**: `lib/orchestrator/cpp/src/orchestrator.cpp:2462-2476`
- **证据**: `run_stage_stack` 空骨架（与 B8-H-3 同源），:2474 `LOG_INFO` 跳过 + `return true`。

#### B9-H-5 ｜ High ｜ OPEN
- **标题**: 测光定标函数有 41 个参数，维护性极差
- **定位**: `lib/orchestrator/cpp/src/orchestrator.cpp:1880-1889`
- **证据**: `pc_calibrate_simple_with_gaia` 函数指针类型声明含 41+ 参数（void*, 6 double, 多个数组+int, float*, int*, double*, double*）。

#### B9-H-6 ｜ High ｜ OPEN
- **标题**: PLATESOLVE 环境清理依赖 DLL 加载状态（隐式耦合）
- **定位**: `lib/orchestrator/cpp/src/orchestrator.cpp:1173`
- **证据**: `if (ipv_solver_handle_ != nullptr && dll_loader_.is_loaded(ModuleId::PLATESOLVE))` PLATESOLVE 环境清理依赖 `dll_loader_.is_loaded` 状态，耦合度高。

#### B9-H-7 ｜ High ｜ OPEN
- **标题**: Gaia 查询返回内存用 std::free 释放，违反封装
- **定位**: `lib/orchestrator/cpp/src/orchestrator.cpp:1543-1544`
- **证据**: `std::free(out_ra); std::free(out_dec);` 直接用 `std::free` 释放 Gaia 客户端返回内存，依赖 `gaia_client.c` 内部用 malloc 的实现细节，跨 DLL 内存管理未封装。

---

## 三、复核结论

### 总体情况
- 50 项 P0+P1 问题中，**44 项仍处于 OPEN 状态**（88%），仅 **6 项已关闭**（12%）。
- 已关闭项目集中在 plate_solve 模块（4 项）和 astro_image_io 模块（1 项），表明 plate_solve 模块在审计后进行了针对性修复。
- calibration（8 项）、dynamic_psf（3 项）、photometric_calib（2 项）、snr_estimator（2 项）、healpix_stack（6 项）模块的所有 P0+P1 问题均未修复。

### 高优先修复建议（按影响面排序）
1. **B2-C-4 两套构建脚本不一致**：影响整个 calibration 模块的构建可重复性，Makefile 与 build.ps1 产物不同，可能导致开发/生产环境行为不一致。
2. **B9-C-3 任务队列大小限制未实现**：违反硬约束"Task queue size limited to 2 to control backpressure"，无背压控制可能导致内存溢出。
3. **B3-C-01/B3-C-02 硬约束违反**：U 组限流 60（应 100）、候选半径 0.55（应 0.5）直接影响 plate_solve 匹配成功率。
4. **B9-C-4 像素值截断**：PLATESOLVE/PSF 阶段 float→uint16 截断丢失精度，影响后续定标精度。
5. **B8 系列 Winsorized 缺失**：healpix_stack 模块 6 项中 4 项与 Winsorized sigma-clip 缺失相关，影响叠加质量。

### 同源问题归类
- **cal_stats 缺失**：B2-C-1 ↔ B9-C-1（calibration 模块未输出 + orchestrator 未写入）
- **1000 颗最亮星限制**：B3-C-07 ↔ B9-C-2（plate_solve 未限制 + orchestrator 未限制）
- **STACK 空骨架**：B8-H-3 ↔ B9-H-4（healpix_stack 侧 + orchestrator 侧）
- **像素截断精度损失**：B4-H-2 ↔ B9-C-4（dynamic_psf 入口 + orchestrator 中间转换）

修复同源问题时，应优先修复上游模块（B2/B3/B4/B8），下游 orchestrator（B9）随之收敛。

---

*报告生成时间: 2026-07-24*
*复核依据: `docs/superpowers/specs/2026-07-18-audit-findings-P0P1.md`*
*证据来源: `lib/` 下当前源码（截至 2026-07-24）*
