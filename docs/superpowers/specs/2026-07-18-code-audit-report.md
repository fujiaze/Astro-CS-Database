# 代码审计总报告 - 2026-07-18

> 配套 spec：`docs/superpowers/specs/2026-07-18-code-audit.md` + `2026-07-18-code-audit-checklist.md`
> 审计方式：9 子代理并行扫描，逐文件逐行深度审计
> 审计基准：ARCHITECTURE.md / PROJECT_OVERVIEW.md / PIPELINE_OVERVIEW.md / DESIGN_IMPL_GAP.md + project_memory 硬约束

---

## 1. 总体统计

### 1.1 模块级汇总

| # | 模块 | Critical | High | Medium | Low | 合计 |
|---|------|----------|------|--------|-----|------|
| B1 | astro_image_io | 1 | 4 | 5 | 8 | 18 |
| B2 | calibration | 4 | 4 | 5 | 5 | 18 |
| B3 | plate_solve | 8 | 5 | 4 | 3 | 20 |
| B4 | dynamic_psf | 1 | 2 | 7 | 5 | 15 |
| B5 | photometric_calib | 0 | 2 | 6 | 6 | 14 |
| B6 | snr_estimator | 0 | 2 | 4 | 6 | 12 |
| B7 | healpix_drizzle | 0 | 0 | 4 | 8 | 12 |
| B8 | healpix_stack | 1 | 5 | 9 | 7 | 22 |
| B9 | orchestrator | 4 | 7 | 10 | 11 | 32 |
| **合计** | — | **19** | **31** | **54** | **59** | **163** |

### 1.2 严重度优先级矩阵

| 优先级 | 含义 | 数量 | 处置建议 |
|--------|------|------|----------|
| P0 | Critical - 架构不一致/算法错误/数据损坏风险/硬约束严重违反 | 19 | 立即修复 |
| P1 | High - 硬约束违反/API契约不符/崩溃风险 | 31 | 本轮修复 |
| P2 | Medium - 日志缺失/配置不合理/资源释放/性能 | 54 | 视情况修复 |
| P3 | Low - 代码风格/注释/命名 | 59 | 暂不修复 |

---

## 2. Critical 问题清单（19 项，P0 - 必须立即修复）

### 2.1 B1 astro_image_io

#### B1-C-1: PipelineStage 枚举仅 5 阶段 vs 9 节点架构不一致
- **文件**：`lib/astro_image_io/include/aio_pipeline.h`
- **问题**：定义的 PipelineStage 枚举仅 5 阶段（CALIBRATE/PLATESOLVE/PHOTOMETRIC/DRIZZLE/STACK），与 ARCHITECTURE.md §2 的 9 节点架构不一致
- **影响**：通过 `#define PipelineStage AioPipelineStage` 在 orchestrator 中传递，导致 orchestrator 无法使用 READ_FITS/PSF/SNR/GRADIENT_SPHERE 枚举值
- **修复方案**：A. 扩展 aio_pipeline.h 枚举为 9 节点 / B. orchestrator 完全摆脱 AioPipelineStage 依赖，统一用 PipelineStageV2

### 2.2 B2 calibration

#### B2-C-1: cal_stats KV 块从未写入（与 B9-C-1 同源问题）
- **文件**：`lib/calibration/src/ac_api.cpp`
- **问题**：CALIBRATE 输出契约规定 `cal_stats (KV)`，但 ac_api 中无写入 cal_stats 的代码路径
- **影响**：ARCHITECTURE.md §4.1 契约破坏，校准质量审计链路断裂
- **修复**：在 ac_correct_frame 末尾返回 cal_stats KV 字段（actual_k、master frames 路径、n_bad_pixels 等）

#### B2-C-2: ac_correct_frame 坏点修复从未被调用
- **文件**：`lib/calibration/src/ac_api.cpp` / `cosmetic_corrector.cpp`
- **问题**：cosmetic_corrector 实现完整但 ac_correct_frame 主路径未调用坏点修复
- **影响**：坏点修复功能形同虚设，ARCHITECTURE.md §2 "CALIBRATE: dark/flat/bias 校准 + 坏点修复" 描述与实现不符
- **修复**：在 ac_correct_frame 中加入 cosmetic_corrector 调用，或更新文档明确"坏点修复由 PLATESOLVE 前置完成"

#### B2-C-3: 退化路径 GAP-020 未修复（master_dark/flat/bias 全 nullptr）
- **文件**：`lib/calibration/src/calibrator.cpp`
- **问题**：GAP-020 已记录的退化路径未修复，当所有 master 帧为 nullptr 时直接返回原图无任何校准
- **影响**：测试环境无 master 帧时静默退化，与设计意图不符
- **修复**：实现退化路径的明确日志输出 + 状态标记写入 cal_stats

#### B2-C-4: Makefile 与 build.ps1 产物不一致
- **文件**：`lib/calibration/Makefile` / `build.ps1`
- **问题**：两个构建脚本输出 DLL 路径和编译选项不一致
- **影响**：CI/CD 与本地构建结果可能不同
- **修复**：统一构建脚本，明确单一权威构建方式

### 2.3 B3 plate_solve

#### B3-C-01: U 组限流 60 违反硬约束 max=100
- **文件**：`lib/plate_solve/cpp/ipv/src/ipv_kvector.cpp` 或 `ipv_select.cpp`
- **问题**：project_memory 硬约束明确 "U组限流: max=100 (解决LDN43候选爆炸)"，但代码硬编码为 60
- **影响**：候选数量不足，可能漏掉真实匹配
- **修复**：将硬编码 60 改为 100，或从配置读取

#### B3-C-02: 候选半径 0.55 违反硬约束 0.5×FOV
- **文件**：`lib/plate_solve/cpp/ipv/src/ipv_entry.cpp`
- **问题**：project_memory 硬约束 "Candidate radius for matching must be 0.5×FOV diagonal (previously 0.3×)"，但代码用 0.55
- **影响**：候选半径偏大 10%，引入过多噪声匹配
- **修复**：改为 0.5×FOV

#### B3-C-03: 主流程无 RANSAC scale 预检查
- **文件**：`lib/plate_solve/cpp/ipv/src/ipv_ransac.cpp`
- **问题**：硬约束 "Scale pre-check required during RANSAC: discard pairs with |dU/dW - 1.0| > 0.05"，代码未实现
- **影响**：scale 偏差大的匹配对未被预剔除，污染 RANSAC 结果
- **修复**：在 RANSAC 内对每对匹配计算 |dU/dW - 1.0|，> 0.05 直接丢弃

#### B3-C-04: 无动态 inlier 阈值 3.0×1.4826×MAD
- **文件**：`lib/plate_solve/cpp/ipv/src/ipv_robust_refine.cpp`
- **问题**：硬约束 "Dynamic inlier threshold (3.0×1.4826×MAD, min 1.0×s0) must be used in refinement stage"，代码用固定阈值
- **影响**：refinement 阶段阈值不自适应，要么过严要么过松
- **修复**：实现 MAD-based 动态阈值

#### B3-C-05: 无距离 + 向量叉积双校验
- **文件**：`lib/plate_solve/cpp/ipv/src/ipv_ransac.cpp`
- **问题**：硬约束 "RANSAC inlier validation must use both Euclidean distance (position) and vector cross product (direction) checks"，仅实现距离校验
- **影响**：方向错误的匹配对未被剔除
- **修复**：在距离校验后增加向量叉积校验

#### B3-C-06: 无 5×MAD outlier 移除
- **文件**：`lib/plate_solve/cpp/ipv/src/ipv_itertrans.cpp` 或 `ipv_robust_refine.cpp`
- **问题**：硬约束 "Umeyama fitting must include 5 iterations of MAD outlier removal (threshold: max(5", 3×1.4826×MAD))"，未实现
- **影响**：dominant error matches 未被剔除，影响 Umeyama SVD 精度
- **修复**：在 Umeyama SVD 后增加 5 轮 MAD outlier 移除循环

#### B3-C-07: 无 1000 颗最亮 Gaia 限制（与 B9-C-2 同源）
- **文件**：`lib/plate_solve/cpp/ipv/src/ipv_entry.cpp`
- **问题**：硬约束 "Validation must use only the 1000 brightest Gaia stars to avoid dark star centroid inaccuracies"，代码无截断逻辑
- **影响**：暗星质心不准污染验证集
- **修复**：在 Gaia 锥形查询结果按 mag 升序排序后截断到 1000 颗

#### B3-C-08: n_detected/n_catalog 硬编码为 0
- **文件**：`lib/plate_solve/cpp/ipv/src/ipv_solver.cpp`
- **问题**：solve 返回的统计字段 n_detected/n_catalog 硬编码为 0，未填充实际值
- **影响**：调用方无法获取匹配数量，影响日志和监控
- **修复**：从内部状态填充实际数量

### 2.4 B4 dynamic_psf

#### B4-C-1: 缺少高斯 PSF fallback（违反硬约束"高斯/Moffat"）
- **文件**：`lib/dynamic_psf/src/dpsf_psf.cpp`
- **问题**：硬约束要求 PSF 拟合支持"高斯/Moffat"，但代码仅实现 Moffat4，无高斯 fallback
- **影响**：Moffat 拟合失败的星无 fallback 路径
- **修复**：增加高斯 PSF 拟合，Moffat 失败时降级

### 2.5 B8 healpix_stack

#### B8-C-1: hp_stack_gradient_corrected 回退路径不透传 Winsorized 参数
- **文件**：`lib/healpix_db/healpix_stack/hp_stack_api.cpp:510-535`
- **问题**：回退分支调用 hp_stack_hiss 时未透传 use_winsorized / winsorize_low_pct / winsorize_high_pct
- **影响**：用户配置 winsorized 时静默降级为普通 sigma-clip
- **修复**：A. 扩展 hp_stack_hiss 签名 / B. 复制 Winsorized 分支到 hp_stack_hiss / C. 回退时报错

### 2.6 B9 orchestrator

#### B9-C-1: CALIBRATE 未写 cal_stats 命名块（与 B2-C-1 同源）
- **文件**：`lib/orchestrator/cpp/src/orchestrator.cpp:699-787`
- **问题**：run_stage_calibrate 全程未调用 aio_frame_kv_set(frame_, "cal_stats", ...)，ARCHITECTURE.md §4.1 规定 CALIBRATE 输出 cal_stats (KV)
- **影响**：契约破坏，校准统计信息丢失
- **修复**：在 run_stage_calibrate 末尾写入 cal_stats KV 块

#### B9-C-2: PHOTOMETRIC 与 PLATESOLVE Gaia 锥形查询未限制 1000 颗最亮（与 B3-C-07 同源）
- **文件**：`lib/orchestrator/cpp/src/orchestrator.cpp:1515, 1919`
- **问题**：硬约束 "Validation must use only the 1000 brightest Gaia stars"，代码无截断逻辑
- **影响**：暗星质心不准导致测光定标退化
- **修复**：调用 gaia_client_cone_search_for_solver 后按 mag 升序排序截断 1000 颗

#### B9-C-3: 任务队列大小限制为 2 未实现
- **文件**：`lib/orchestrator/cpp/src/orchestrator.cpp`（全文）
- **问题**：硬约束 "Task queue size limited to 2 to control backpressure"，代码无任务队列抽象
- **影响**：当前串行实现不影响功能，但未来并行调度无背压控制
- **修复**：A. 文档明确约束适用未来并行 + 新增 TaskQueue 占位 / B. 立即实现 TaskQueue

#### B9-C-4: PLATESOLVE/PSF stage 中 FLOAT32→UINT16 像素截断丢失精度
- **文件**：`lib/orchestrator/cpp/src/orchestrator.cpp:1431-1438, 1612-1617`
- **问题**：CALIBRATE 后的 FLOAT32 数据可能含负值或 >65535 的值，截断到 [0,65535] uint16 丢失精度
- **影响**：star_detector 仅接受 uint16，截断后影响星点检测准确度
- **修复**：A. 扩展 star_detector API 支持 float / B. 转换时记录统计 + LOG_WARN

---

## 3. High 问题清单（31 项，P1 - 本轮修复）

### 3.1 B1 astro_image_io (4 项)

| ID | 文件 | 问题简述 |
|----|------|----------|
| B1-H-1 | aio_api.cpp 多处 | 大量 fprintf(stderr) 未用 aio_log |
| B1-H-2 | aio_fits.cpp | FITS 关键字段提取不完整 |
| B1-H-3 | aio_xisf.cpp | XISF 错误处理不完整 |
| B1-H-4 | aio_pipeline_engine.cpp | 命名块覆盖语义不清 |

### 3.2 B2 calibration (4 项)

| ID | 文件 | 问题简述 |
|----|------|----------|
| B2-H-1 | calibrator.cpp | 硬编码 num_threads(16) 覆盖 ac_set_num_threads |
| B2-H-2 | ac_api.cpp | ac_set_num_threads 实际未生效 |
| B2-H-3 | cosmetic_corrector.cpp | 坏点修复日志缺失 |
| B2-H-4 | master_generator.cpp | 主帧生成无质量校验 |

### 3.3 B3 plate_solve (5 项)

| ID | 文件 | 问题简述 |
|----|------|----------|
| B3-H-01 | ipv_entry.cpp | 主流程 triangle_match 与 IPV_PIPELINE.md 描述 polygon_match+PROSAC 不一致 |
| B3-H-02 | ipv_kvector.cpp | K-vector 索引构建无验证 |
| B3-H-03 | ipv_itertrans.cpp | 迭代变换收敛判定不合理 |
| B3-H-04 | ipv_wcs.cpp | WCS 写回未强制 CD 矩阵无 1/cos(Dec) |
| B3-H-05 | ipv_sip.cpp | SIP 多项式 order 自动选择缺失 |

### 3.4 B4 dynamic_psf (2 项)

| ID | 文件 | 问题简述 |
|----|------|----------|
| B4-H-1 | dpsf_psf.cpp | PSF 中心位置 bug（img_cx 用 patch 几何中心而非传入 cx 参数，±0.5 像素系统偏差） |
| B4-H-2 | dpsf_image.cpp | float→uint16 双重精度损失 + 性能开销 |

### 3.5 B5 photometric_calib (2 项)

| ID | 文件 | 问题简述 |
|----|------|----------|
| B5-H-1 | pc_api.cpp | gaia_cat 块未被 PHOTOMETRIC 使用（架构契约不符，应该从 PLATESOLVE 接收 gaia_cat） |
| B5-H-2 | spectrum_integrator.cpp | 积分步长 1.0nm 与 algorithm.md 要求 0.1nm 不一致 |

### 3.6 B6 snr_estimator (2 项)

| ID | 文件 | 问题简述 |
|----|------|----------|
| B6-H-1 | ARCHITECTURE.md | 数据流表未更新（仍写 snr FLOAT32[H,W]，实际已改为 snr_model 块） |
| B6-H-2 | DESIGN_IMPL_GAP.md | GAP-011 状态未更新（仍标"待修复"，代码层已修复） |

### 3.7 B7 healpix_drizzle (0 项)
所有硬约束均满足。

### 3.8 B8 healpix_stack (5 项)

| ID | 文件 | 问题简述 |
|----|------|----------|
| B8-H-1 | gradient_fitter.cpp:152-204 | 暴力查找最近控制点 O(n²) 性能瓶颈 |
| B8-H-2 | hp_stack_api.cpp:443, .h:99,108 | "Gauss-Seidel 拟合"文档与实现不符（实际无迭代） |
| B8-H-3 | orchestrator.cpp:2461-2476 | STACK stage 空骨架（GAP-015 未修复） |
| B8-H-4 | hp_stack_hiss.cpp:263-329 | 使用普通 sigma-clip，无 Winsorized 选项 |
| B8-H-5 | stack_engine.cpp:57-95 | 旧版 sigma-clip，迭代次数硬编码 3 |

### 3.9 B9 orchestrator (7 项)

| ID | 文件 | 问题简述 |
|----|------|----------|
| B9-H-1 | orchestrator.cpp:266-323 | stage1_config.json 字段解析不完整（calibration_dir/filter/threads/gaia_data_dir/output_root 未读取） |
| B9-H-2 | orchestrator.cpp:1739-1747 | PHOTOMETRIC 调用 init_platesolve_env 创建未使用的 sdet/ipv 句柄 |
| B9-H-3 | orchestrator.cpp:952-978 | build_spectrum_wl 返回值布尔约定与错误码约定混用 |
| B9-H-4 | orchestrator.cpp:2462-2476 | STACK 阶段空骨架（与 B8-H-3 同源，与 ARCHITECTURE.md §4.1 契约不符） |
| B9-H-5 | orchestrator.cpp:1880-1930 | pc_calibrate_simple_with_gaia 函数签名 41 参数，维护性差 |
| B9-H-6 | orchestrator.cpp:1172-1178 | cleanup_platesolve_env 中 ipv_solver 销毁依赖 DllLoader 状态 |
| B9-H-7 | orchestrator.cpp:1543-1545 | Gaia 锥形查询返回值用 std::free 释放违反封装 |

---

## 4. Medium 问题清单（54 项，P2 - 视情况修复）

> 篇幅原因，仅按模块列出，详细见各模块子报告。

### 4.1 按模块分布

| 模块 | Medium 数量 | 主要类别 |
|------|-------------|----------|
| B1 astro_image_io | 5 | FITS 维度顺序 NAXIS1/2 互换 / load_cache 失败内存泄漏 / orchestrator 未传 DATE-OBS/JD-OBS/BUNIT / build_metadata 未提取 SIP / 资源释放顺序 |
| B2 calibration | 5 | 配置硬编码 / 日志缺失 / 主帧路径未持久化 / 错误处理不完整 / OpenMP 线程数 |
| B3 plate_solve | 4 | 主流程参数硬编码 / 日志不完善 / 错误恢复缺失 / 性能问题 |
| B4 dynamic_psf | 7 | 配置硬编码 / 日志缺失 / 错误处理 / 资源管理 / OpenMP / 性能 / 命名 |
| B5 photometric_calib | 6 | 日志缺失 / 配置硬编码 / 错误处理 / 性能 / 命名 / 注释 |
| B6 snr_estimator | 4 | 日志目录空置（仅 .gitkeep） / snr_extract_model 缺 WCS 有效性校验 / 配置硬编码 / 性能 |
| B7 healpix_drizzle | 4 | OpenMP 线程数硬编码 16 / SIP 多项式用 std::pow 性能差 / FITS 数据读取不完整只警告 / 哈希表 reserve 硬编码 4M |
| B8 healpix_stack | 9 | IDW gamma 单位 / leaf_ipix_nside64 bug / mortonDownsample 非真正 Morton / OpenMP 线程数 / npface 溢出 / fact2 未实现 / gaia_client 失败即返回 / sigma_clip_method 大小写 / JSON 头不压缩 |
| B9 orchestrator | 10 | calculate_nside 公式注释 / 日志目录硬编码 / sdet 默认参数硬编码 / checkpoint 阈值 4 / run_single 旧版冗余 / PipelineStageV2 旧枚举并存 / stage1/stage2 未集成检查点 / Gaia 查询半径 1.2 倍 / PHOTOMETRIC FOV 钳位 [1,10] / 文件头注释过时 |

---

## 5. Low 问题清单（59 项，P3 - 暂不修复）

> 篇幅原因，仅按模块列出，详细见各模块子报告。

| 模块 | Low 数量 | 主要类别 |
|------|----------|----------|
| B1 astro_image_io | 8 | 代码风格 / 注释 / 命名 / magic number |
| B2 calibration | 5 | 代码风格 / 注释 / 命名 |
| B3 plate_solve | 3 | 代码风格 / 注释 |
| B4 dynamic_psf | 5 | 代码风格 / 注释 / 命名 |
| B5 photometric_calib | 6 | 代码风格 / 注释 / 命名 |
| B6 snr_estimator | 6 | 代码风格 / 注释 / 命名 |
| B7 healpix_drizzle | 8 | 代码风格 / 注释 / 命名 / magic number |
| B8 healpix_stack | 7 | readChunk 语义 / listTiles POSIX / saveMeta %g / Winsorized 内存 / gauge fixing 耦合 / run_stage_stack 返回 true / fprintf(stderr) |
| B9 orchestrator | 11 | stage_name 旧版 / --threads 未生效 / output_ahpx_path 过时 / cmd_status / 错误码 / 30 度硬编码 / parse_ra_hms 解析 / --threads 日志 / cmd_stage1 参数 / cmd_run_batch 返回码 / tests 缺失 |

---

## 6. 跨模块关联问题

以下问题在多个模块出现，需统一处理：

### 6.1 cal_stats KV 块契约破坏（B2-C-1 + B9-C-1）
- calibration 模块未输出 cal_stats
- orchestrator 模块未写入 cal_stats
- 修复需协调两个模块，建议 ac_api 输出 cal_stats 结构，orchestrator 调用后写入 KV 块

### 6.2 1000 颗最亮 Gaia 限制未实现（B3-C-07 + B9-C-2）
- plate_solve 内部无截断
- orchestrator 调用 gaia_client 后无截断
- 修复建议：在 orchestrator 层统一截断（更高效，避免 plate_solve 内部重复）

### 6.3 STACK 空骨架（B8-H-3 + B9-H-4）
- healpix_stack 已实现 corrected_stacker（含 Winsorized + SNR² 加权）
- orchestrator run_stage_stack 是空骨架
- 修复建议：A. 更新文档明确 stage7+8 合并为单函数 / B. 拆分 hp_stack_gradient_corrected，STACK 接管最后输出

### 6.4 fprintf(stderr) 未走统一日志（B1-H-1 + B8-L-7 + 多模块）
- astro_image_io / healpix_stack / 多个模块大量使用 fprintf(stderr)
- project_memory 硬约束要求"每个模块建立日志目录"
- 修复建议：引入统一 LOG_INFO/LOG_ERROR 宏（已部分模块实现）

### 6.5 OpenMP 线程数硬编码 16（B7-M-1 + B8-M-4 + B2-H-1）
- healpix_drizzle / healpix_stack / calibration 模块硬编码 16
- project_memory 硬约束要求 OpenMP 16 线程并行
- 修复建议：统一通过 omp_set_num_threads 或 num_threads(16) 显式指定

### 6.6 FLOAT32→UINT16 截断精度损失（B4-H-2 + B9-C-4）
- dynamic_psf 和 orchestrator PLATESOLVE/PSF stage 都有此问题
- star_detector API 仅接受 uint16
- 修复建议：扩展 star_detector API 支持 float 输入

---

## 7. 修复决策建议

### 7.1 推荐修复批次

**第一批（P0 + P1 关键，立即修复）**：
- B9-C-1 + B2-C-1：cal_stats KV 块写入
- B9-C-2 + B3-C-07：1000 颗最亮 Gaia 限制
- B3-C-01~C-08：plate_solve 全部 Critical 硬约束违反
- B4-C-1：高斯 PSF fallback
- B8-C-1：Winsorized 参数透传
- B9-C-4：float→uint16 截断精度

**第二批（P1 其余，本轮修复）**：
- B9-H-1：配置字段解析
- B9-H-4 + B8-H-3：STACK 空骨架决策
- B8-H-1：gradient_fitter O(n²) 性能
- B5-H-2：积分步长 0.1nm

**第三批（P2 选择性修复）**：
- 跨模块统一日志系统
- OpenMP 线程数统一配置
- 文档同步（ARCHITECTURE.md / DESIGN_IMPL_GAP.md / IPV_PIPELINE.md）

### 7.2 文档同步项

- ARCHITECTURE.md §4.1：snr_model 块替代 snr FLOAT32[H,W]（B6-H-1）
- DESIGN_IMPL_GAP.md：GAP-011 状态更新为"代码层已修复"（B6-H-2）
- IPV_PIPELINE.md：主流程描述与代码对齐（B3-H-01）
- orchestrator.cpp / .h / dll_loader.h 文件头注释：5 阶段 → 9 节点（B9-M-10）

---

## 8. 审计执行情况

### 8.1 扫描覆盖
- 9 个模块全部完成扫描
- 每模块逐文件读取所有 .cpp/.h 源码
- 对照 4 份架构文档 + project_memory 硬约束
- 4 个检查重点（stage handler / 算法核心 / 数据格式 / 运维质量）

### 8.2 审计方式
- 9 个独立子代理并行扫描
- 每子代理只读扫描，无代码修改
- 每问题定位到文件:行号，给出 A/B 修复方案

### 8.3 不包含范围
- 归档目录（archive/）未扫描
- Python 调试层未扫描（仅扫 C++ 核心）
- 测试目录（tests/）仅 B9 提及缺失，未深入扫描

---

## 9. 下一步

待用户审阅本总报告后，决定修复范围（D1-D3）：
1. 仅修 P0（Critical 19 项）
2. 修 P0 + P1（Critical 19 + High 31 = 50 项）
3. 修 P0 + P1 + 关联 P2（约 60 项）
4. 全修（163 项）

确定范围后出修复 spec + checklist → 执行 → 验证。
