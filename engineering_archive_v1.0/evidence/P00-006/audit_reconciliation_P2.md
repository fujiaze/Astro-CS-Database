# P2 (Medium) 审计复核报告 - P00-006

> **复核日期**：2026-07-24
> **审计源文档**：`docs/superpowers/specs/2026-07-18-audit-findings-P2.md`
> **复核范围**：9 模块共 54 项 Medium 级问题（参数硬编码 / 日志缺失 / 错误处理 / 性能优化 / 资源管理等）
> **复核方法**：逐项读取 `lib/` 下源码确认当前状态，每项给出文件:行号证据

---

## 汇总

| 状态 | 数量 | 占比 |
|------|------|------|
| **OPEN** | 38 | 70.4% |
| **CLOSED** | 15 | 27.8% |
| **STALE** | 0 | 0.0% |
| **UNVERIFIED** | 0 | 0.0% |
| **REJECTED** | 1 | 1.9% |
| **合计** | 54 | 100% |

**关键发现**：
- 参数硬编码类问题（OpenMP 线程数 16、IRLS 迭代次数、收敛阈值等）普遍仍 OPEN，涉及 B2/B3/B4/B5/B6/B7/B8/B9 共 8 个模块
- 日志缺失类问题分化明显：B4 dynamic_psf、B5 photometric_calib 已补全详细日志（CLOSED），但 B2 calibration、B3 plate_solve、B6 snr_estimator 仍无日志（OPEN）
- B4 dynamic_psf 的 OpenMP 线程数硬编码（B4-M-5）已修复（CLOSED），代码中不再有 `omp_set_num_threads(16)`
- B5 photometric_calib 的 F_syn 积分已并行化（B5-M-4 CLOSED）、F_syn 公式与 IRLS 推导已有注释（B5-M-6 CLOSED）
- B9 orchestrator 模块 10 项全部 OPEN：新旧管线枚举并存、检查点未集成 stage1/stage2、NSIDE 公式错误、文件头注释过时等问题均未修复
- B8 healpix_stack 的 fact2 问题（B8-M-6）为 REJECTED：该模块使用 astrometry.net 风格的 pix2xy+xy2ang 实现，不需要 fact2 系数，硬约束仅适用于 healpix_browser_qt 模块
- B1 astro_image_io 的 SIP 关键字提取（B1-M-4）仍 OPEN：build_metadata 未提取 A_ORDER/B_ORDER 等 SIP 多项式关键字

---

## 按模块明细

### B1 astro_image_io (5 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B1-M-1 | FITS 维度顺序处理可能互换宽高 | CLOSED | `aio_fits.cpp:281-282` `width = hdr.naxis1; height = hdr.naxis2;` 符合 FITS 标准 |
| B1-M-2 | 缓存加载失败时已分配内存泄漏 | CLOSED | `aio_pipeline.cpp:675-782` `aio_frame_load_cache` 先清除旧块(:722-729)，新块赋给 `frame->blocks`(:740)，失败时由 frame 析构释放 |
| B1-M-3 | 编排器未传递观测元数据 | CLOSED | `aio_fits.cpp:331-381` `build_metadata` 已提取 DATE-OBS/JD/BUNIT/LONPOLE/LATPOLE 等全部字段 |
| B1-M-4 | 元数据构建未提取 SIP 关键字 | OPEN | `aio_fits.cpp:261-382` `build_metadata` 经 grep `A_ORDER\|B_ORDER\|AP_ORDER\|BP_ORDER` 无匹配，仅有 CD 矩阵和 CDELT |
| B1-M-5 | 多资源释放顺序不明确 | CLOSED | `aio_pipeline_engine.cpp:262-272` `destroy` 先释放 `block_drop` 数组再 `delete eng`，引擎不持有文件句柄或压缩上下文 |

### B2 calibration (5 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B2-M-1 | 多处配置参数硬编码 | OPEN | `calibrator.cpp:87` `num_threads(16)` 硬编码，sigma_clip 阈值等未参数化 |
| B2-M-2 | 关键步骤日志缺失 | OPEN | `calibrator.cpp` 经 grep `LOG_\|fprintf\|log` 无匹配，校准各步骤无任何日志 |
| B2-M-3 | 主帧文件路径未持久化 | OPEN | `ac_api.cpp` 函数接收 `float*` 数组而非路径，无 `cal_stats` 持久化 |
| B2-M-4 | 错误处理不完整 | OPEN | `calibrator.cpp` 经 grep `width\|height\|size.*mismatch\|dtype` 无匹配，尺寸/类型不匹配未报错 |
| B2-M-5 | OpenMP 线程数管理混乱 | OPEN | `calibrator.cpp:87` `num_threads(16)` 硬编码，外部 API 设置无效 |

### B3 plate_solve (4 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B3-M-1 | 主流程参数硬编码 | OPEN | `ipv_entry.cpp:77-86` RANSAC 参数通过 IpvParams 传递，但 `orchestrator.cpp:1319-1320` 用 `fn_get_default_params` 取默认值，未从 config_json 覆盖 |
| B3-M-2 | 错误恢复机制不完善 | OPEN | `ipv_entry.cpp` 经 grep `fallback\|retry\|degrade\|backup` 无匹配，主算法失败直接返回 |
| B3-M-3 | 日志输出不完善 | OPEN | `ipv_entry.cpp` 经 grep `LOG_\|fprintf\|log` 仅 :27 注释提及 fprintf 用于崩溃诊断，RANSAC 迭代无详细日志 |
| B3-M-4 | 性能瓶颈未优化 | OPEN | `ipv_kvector.cpp:83-122` `kvector_query` 使用二分查找，未用 KD-tree 加速 |

### B4 dynamic_psf (7 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B4-M-1 | 多处配置参数硬编码 | OPEN | `dpsf_psf.cpp:306-307` `lm_solve(..., 1e-8, 200)` 收敛阈值和最大迭代硬编码；`:300` `sx0=0.15*rw` 初始 FWHM 硬编码 |
| B4-M-2 | 关键步骤日志缺失 | CLOSED | `dpsf_psf.cpp` 有 20+ 处 `dpsf_log` 调用（LM converged/iteration limit/Initial params/LM result/Fit done 等） |
| B4-M-3 | 错误处理不完整 | CLOSED | `dpsf_psf.cpp:397` invalid arguments, `:413` empty rect, `:224` Rect too small, `:297` Amplitude<=0 均有错误处理 |
| B4-M-4 | 资源管理不严格 | CLOSED | `dpsf_psf.cpp:472` `#pragma omp parallel for ... reduction(+:success_count)` 使用 reduction 正确避免竞态 |
| B4-M-5 | OpenMP 线程数硬编码 | CLOSED | `dpsf_psf.cpp` 经 grep `omp_set_num_threads` 无匹配，`:472` 未硬编码线程数，原审计所述 16 线程已不存在 |
| B4-M-6 | 性能优化未到位（Moffat std::pow） | OPEN | `dpsf_psf.cpp:91,199` `std::pow(1.0+Q, 4.0)` β=4 整数幂可用连乘 `Q²*Q²` 替代 |
| B4-M-7 | 命名不一致 | CLOSED | `dpsf_psf.cpp:367-368` `img_cx/img_cy` 统一 snake_case，grep `imgC` 无 camelCase 匹配 |

### B5 photometric_calib (6 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B5-M-1 | 关键步骤日志缺失 | CLOSED | `pc_api.cpp` 有 15+ 处 fprintf 日志（校准开始/线程数/无Gaia星/无PSF星/匹配完成/校准完成等） |
| B5-M-2 | 配置参数硬编码 | OPEN | `star_matcher.cpp:24` `_IRLS_MAX_ITER=50`, `:26` `_IRLS_CONVERGE=1e-6`, `:21` Tukey c=4.685 均 `static constexpr`；`pc_api.cpp:113` `match_radius_px=2.0` |
| B5-M-3 | 错误处理不完整 | CLOSED | `pc_api.cpp:65-67` 检查 `n_gaia<=0`, `:81` 检查 `n_psf<=0`, `:247-248` 退化 `scale=1.0` |
| B5-M-4 | F_syn 积分未并行化 | CLOSED | `pc_api.cpp:303` `#pragma omp parallel for num_threads(16) schedule(dynamic,64) reduction(+:n_valid_fsyn)` 已并行化 |
| B5-M-5 | 命名不一致 | CLOSED | `*.cpp` 变量统一 snake_case，函数 camelCase 是统一 C++ 风格 |
| B5-M-6 | 注释不全（F_syn 公式无注释） | CLOSED | `spectrum_integrator.cpp:260` 有 `S(λ)·T(λ)·Q(λ)·λ` 公式注释；`star_matcher.cpp:2-6,21-26` 有 IRLS+Tukey 推导注释 |

### B6 snr_estimator (4 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B6-M-1 | 日志目录空置（仅 .gitkeep） | OPEN | `snr_estimator.cpp` 有 10+ 处 `fprintf(stderr, '[snr]...')`，未写入日志文件，`logs/` 仅 .gitkeep |
| B6-M-2 | SNR 模型提取缺少 WCS 有效性校验 | OPEN | `snr_estimator.cpp:219-224` `snr_extract_model` 直接使用 `wcs->cd[0]/crval1`，未校验 CD 非零或 CRVAL 合理 |
| B6-M-3 | 配置参数硬编码 | OPEN | `snr_estimator.cpp:63,80,128,143,162` 多处 `num_threads(16)`；`:281` `idw_power=2.0`；`:151` `radius=sqrt(w²+h²)` |
| B6-M-4 | IDW 未用 KD-tree | OPEN | `snr_estimator.cpp:162-197` 双重 for 循环 O(N×M)，未用 KD-tree（注：healpix_stack/snr_evaluator.cpp 已用 nanoflann） |

### B7 healpix_drizzle (4 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B7-M-1 | OpenMP 线程数硬编码 16 | OPEN | `drizzle_engine.cpp:103-104` `const int NUM_THREADS = 16; omp_set_num_threads(NUM_THREADS);` |
| B7-M-2 | SIP 多项式计算用 std::pow | OPEN | `wcs_sip.cpp:84` `std::pow(dx, i) * std::pow(dy, j)` 幂次为小整数(≤4)，可用连乘替代 |
| B7-M-3 | FITS 数据读取不完整时只警告不报错 | OPEN | `fits_reader.cpp:414-417` `if (got < data_size)` 仅 `fprintf` 警告并继续处理 |
| B7-M-4 | 哈希表 reserve 硬编码 4M 桶 | OPEN | `drizzle_engine.cpp:108` `acc.reserve(1 << 22)` 硬编码 4M，未根据图像尺寸动态计算 |

### B8 healpix_stack (9 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B8-M-1 | IDW 权重中距离用度而非弧度 | OPEN | `snr_evaluator.cpp:222-224` `greatCircleDistanceDeg` 返回度，`:231` `w = 1.0 / std::pow(gamma, idw_power_)` gamma 单位为度 |
| B8-M-2 | 子叶块索引计算 bug（nside<64） | OPEN | `gradient_sampler.cpp:551-556` nside_i<64 时 `shift_64=0`，`cp_ipix << 0` 无效，索引未扩展（注释承认 nside_i_min=64 保证不触发） |
| B8-M-3 | mortonDownsample 名不副实 | OPEN | `gradient_sampler.cpp:184-292` 函数名暗示 Morton 位运算，实际用 ra/dec 量化空间网格分组（`:207-208` 注释承认） |
| B8-M-4 | OpenMP 并行未显式控制线程数 | OPEN | `spherical_spline.cpp:264` `#pragma omp parallel for` 无 schedule/num_threads；`snr_evaluator.cpp:259` 无 num_threads |
| B8-M-5 | HEALPix 像素数计算未强制 int64 | CLOSED | `healpix_core.cpp:55` `(int64_t)12 * m_nside * m_nside` 12 先转 int64_t，乘法提升为 64 位，nside=16384 不溢出 |
| B8-M-6 | fact2 系数未在 healpix_stack 中实现 | REJECTED | `healpix_core.cpp:441-444` `pix2ang` 使用 `pix2xy+xy2ang`（astrometry.net 风格），grep `fact2\|npface` 无匹配，此模块不需要 fact2，硬约束仅适用于 healpix_browser_qt |
| B8-M-7 | Gaia 客户端创建失败即返回 | OPEN | `gradient_sampler.cpp:322-327` `gaia_client_create_ex` 失败 `return 2`，无"跳过星拒绝"退化路径 |
| B8-M-8 | sigma_clip_method 未做白名单校验 | OPEN | `orchestrator.cpp:2410-2414` 解析 `sigma_clip_method` 后直接赋值，未校验 `{standard, winsorized}` |
| B8-M-9 | JSON 头不压缩，读写不对称 | OPEN | `ahps_writer.cpp:332-333` `headerCompSize=0` 不压缩；`ahps_reader.cpp:227-234` 保留 ZSTD 解压分支（死代码） |

### B9 orchestrator (10 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B9-M-1 | NSIDE 自适应公式与 HEALPix 标准不符 | OPEN | `orchestrator.cpp:152-173` 注释 `1186.18/nside` 与标准 `~211,000/nside arcsec` 不符，偏小约 178 倍，NSIDE 计算偏小 |
| B9-M-2 | 日志目录硬编码，各 stage 无独立日志 | OPEN | `orchestrator.cpp:246` 所有 stage 写 `lib/orchestrator/logs` 单一文件；`:1322` 仅 PLATESOLVE 写 `lib/plate_solve/logs` |
| B9-M-3 | 星点检测器默认参数硬编码 | OPEN | `orchestrator.cpp:1108-1119` SDetParams 各字段硬编码（structureLayers=5/maxStars=2000/fitRadius=0 等），未从 config.json 解析 |
| B9-M-4 | 检查点阈值硬编码为 4 | OPEN | `checkpoint.cpp:730` `if (data.current_stage_id >= 4)` 基于旧版 5 阶段，新版 9 节点需到 6/8 |
| B9-M-5 | run_single 旧版路径仍保留 | OPEN | `orchestrator.cpp:333-462` `run_single` 调用 5 阶段，与新版 9 节点不符，CLI 仍提供 `run` 命令 |
| B9-M-6 | 新旧管线阶段枚举并存 | OPEN | `orchestrator.h:37-43` 旧 5 阶段 / `:53-65` 新 9 节点 / `:79` StageTiming 用旧枚举；`orchestrator.cpp:2566` 所有 stage1 timings 硬编码为 CALIBRATE |
| B9-M-7 | stage1/stage2 未集成检查点 | OPEN | `orchestrator.cpp:2482-2605` `run_stage1` 和 `:2611-2718` `run_stage2` 的 lambda 无 `checkpoint_mgr_` 调用 |
| B9-M-8 | Gaia 锥形查询半径 1.2 倍余量 | OPEN | `orchestrator.cpp:1505-1506` `fov_radius_deg = ... / 2.0 * 1.2` 即 0.6×对角线，硬约束要求 0.5×（无余量） |
| B9-M-9 | PHOTOMETRIC FOV 钳位与 PLATESOLVE 不一致 | OPEN | `orchestrator.cpp:1874` PHOTOMETRIC 钳位 `[1.0, 10.0]`；`:1508` PLATESOLVE 上限 30 度 |
| B9-M-10 | 文件头注释过时 | OPEN | `orchestrator.cpp:3` / `orchestrator.h:2` / `dll_loader.h:7` 均描述旧版 5 阶段，实际已实现 9 节点 |

---

## 按状态分类

### CLOSED (14 项)

| ID | 模块 | 标题 |
|----|------|------|
| B1-M-1 | astro_image_io | FITS 维度顺序正确 |
| B1-M-2 | astro_image_io | 缓存加载无内存泄漏 |
| B1-M-3 | astro_image_io | 元数据已提取 |
| B1-M-5 | astro_image_io | 资源释放顺序正确 |
| B4-M-2 | dynamic_psf | 日志已补全 |
| B4-M-3 | dynamic_psf | 错误处理已完善 |
| B4-M-4 | dynamic_psf | OpenMP 资源管理正确 |
| B4-M-5 | dynamic_psf | 线程数硬编码已移除 |
| B4-M-7 | dynamic_psf | 命名统一 |
| B5-M-1 | photometric_calib | 日志已补全 |
| B5-M-3 | photometric_calib | 错误处理已完善 |
| B5-M-4 | photometric_calib | F_syn 已并行化 |
| B5-M-5 | photometric_calib | 命名统一 |
| B5-M-6 | photometric_calib | 注释已补全 |
| B8-M-5 | healpix_stack | int64 防溢出已实现 |

### REJECTED (1 项)

| ID | 模块 | 标题 | 原因 |
|----|------|------|------|
| B8-M-6 | healpix_stack | fact2 系数未实现 | 此模块使用 astrometry.net 风格 pix2xy+xy2ang，不需要 fact2；硬约束仅适用于 healpix_browser_qt |

---

## 与 P3 复核对比

| 指标 | P2 (Medium) | P3 (Low) |
|------|-------------|----------|
| 总数 | 54 | 59 |
| OPEN | 38 (70.4%) | 27 (45.8%) |
| CLOSED | 15 (27.8%) | 32 (54.2%) |
| REJECTED | 1 (1.9%) | 0 (0.0%) |

P2 的 OPEN 比例（70.4%）显著高于 P3（45.8%），主要因为 P2 涉及更多架构级问题（B9 orchestrator 10 项全 OPEN、B2 calibration 5 项全 OPEN、B3 plate_solve 4 项全 OPEN），而 P3 多为代码风格类问题，已在新一轮代码规范化中修复。
