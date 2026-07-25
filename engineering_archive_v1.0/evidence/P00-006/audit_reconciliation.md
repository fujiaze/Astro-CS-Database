# AstroCS 旧审计 163 项复核报告

- **Task ID**: P00-006
- **生成时间**: 2026-07-24
- **审计来源**: docs/superpowers/specs/2026-07-18-code-audit-report.md (2026-07-18, 9 子代理扫描)
- **复核总数**: 163

## 总体统计

| 状态 | 数量 | 占比 | 含义 |
|---|---|---|---|
| OPEN | 112 | 68% | 当前源码仍存在 |
| CLOSED | 50 | 30% | 已有代码证据已解决 |
| STALE | 0 | 0% | 路径/架构已变化 |
| UNVERIFIED | 0 | 0% | 无法验证 |
| REJECTED | 1 | 0% | 硬约束无有效来源 |
| **合计** | **163** | **100%** | |

## 按优先级分布

| 优先级 | 总数 | OPEN | CLOSED | STALE | UNVERIFIED | REJECTED |
|---|---|---|---|---|---|---|
| P0P1 | 50 | 44 | 6 | 0 | 0 | 0 |
| P2 | 54 | 38 | 15 | 0 | 0 | 1 |
| P3 | 59 | 27 | 32 | 0 | 0 | 0 |

## 按模块分布

| 模块 | 总数 | OPEN | CLOSED | STALE | UNVERIFIED | REJECTED |
|---|---|---|---|---|---|---|
| astro_image_io | 18 | 9 | 9 | 0 | 0 | 0 |
| calibration | 18 | 15 | 3 | 0 | 0 | 0 |
| dynamic_psf | 15 | 6 | 9 | 0 | 0 | 0 |
| healpix_drizzle | 12 | 6 | 6 | 0 | 0 | 0 |
| healpix_stack | 22 | 20 | 1 | 0 | 0 | 1 |
| orchestrator | 32 | 32 | 0 | 0 | 0 | 0 |
| photometric_calib | 14 | 5 | 9 | 0 | 0 | 0 |
| plate_solve | 20 | 12 | 8 | 0 | 0 | 0 |
| snr_estimator | 12 | 7 | 5 | 0 | 0 | 0 |

## OPEN 项按优先级（P01+ 修复输入）

### Critical (16 项 OPEN)

| ID | 模块 | 标题 | 证据 |
|---|---|---|---|
| B1-C-1 | astro_image_io | 管线阶段枚举仅 5 个，与 9 节点架构不一致 | lib/astro_image_io/include/aio_pipeline.h:26-32 PipelineStag |
| B2-C-1 | calibration | 校准统计信息从未输出 (cal_stats 命名块缺失) | lib/calibration/src/ac_api.cpp:101-117 ac_correct_frame 函数全程 |
| B2-C-2 | calibration | 坏点修复功能已实现但从未被调用 | lib/calibration/src/calibrator.cpp:203-236 calibrate 函数主路径仅做 |
| B2-C-3 | calibration | 无主帧时静默退化，未标记状态 (GAP-020 未修复) | lib/calibration/src/calibrator.cpp:223-233 当 dark/flat/bias  |
| B2-C-4 | calibration | 两套构建脚本产物不一致 | lib/calibration/Makefile:2 用 -O3 -ffast-math -funroll-loops  |
| B3-C-01 | plate_solve | 候选星数量上限硬编码为 60，违反应为 100 的硬约束 | lib/plate_solve/cpp/ipv/src/ipv_select.cpp:280 'int n_target |
| B3-C-02 | plate_solve | 候选半径用 0.55 倍视场对角线，违反应为 0.5 倍硬约束 | lib/plate_solve/cpp/ipv/include/ipv_types.h:200 'double gaia |
| B3-C-05 | plate_solve | RANSAC 内点校验只查位置，违反应同时查方向硬约束 | lib/plate_solve/cpp/ipv/src/ipv_ransac.cpp 经 grep 'cross|叉积| |
| B3-C-06 | plate_solve | Umeyama 拟合缺少 5 轮 MAD 离群值剔除 | lib/plate_solve/cpp/ipv/src/ipv_robust_refine.cpp:974 IRLS 迭 |
| B3-C-07 | plate_solve | 验证集未限制为 1000 颗最亮 Gaia 星 | lib/plate_solve/cpp/ipv/src/ipv_entry.cpp 经 grep '1000|brigh |
| B4-C-1 | dynamic_psf | 缺少高斯 PSF 备选方案 | lib/dynamic_psf/src/dpsf_psf.cpp:392,443 仅 moffat4_fit 和 dps |
| B8-C-1 | healpix_stack | 球面梯度校正在回退路径丢失 Winsorized 参数 | lib/healpix_db/healpix_stack/hp_stack_api.cpp:510-511,534-53 |
| B9-C-1 | orchestrator | CALIBRATE 阶段未写入校准统计命名块 (与 B2-C-1 同源) | lib/orchestrator/cpp/src/orchestrator.cpp 经 grep 'cal_stats' |
| B9-C-2 | orchestrator | 编排器调用 Gaia 查询未限制 1000 颗最亮星 (与 B3-C-07 同源 | lib/orchestrator/cpp/src/orchestrator.cpp:1515,1919 调用 gaia_ |
| B9-C-3 | orchestrator | 任务队列大小限制为 2 未实现 | lib/orchestrator/cpp/src/orchestrator.cpp 经 grep 'queue|task |
| B9-C-4 | orchestrator | PLATESOLVE/PSF 阶段像素值截断丢失精度 | lib/orchestrator/cpp/src/orchestrator.cpp:1429-1437,1606-161 |

### High (28 项 OPEN)

| ID | 模块 | 标题 | 证据 |
|---|---|---|---|
| B1-H-1 | astro_image_io | 大量错误日志直接输出到 stderr，未走模块日志系统 | lib/astro_image_io/src/ 经 grep 'fprintf\s*\(\s*stderr' 检查 7  |
| B1-H-2 | astro_image_io | FITS 关键字段提取不完整，丢失观测元数据 | lib/astro_image_io/src/aio_fits.cpp:320,333,368,369,379 已提取  |
| B1-H-4 | astro_image_io | 命名块覆盖语义不清，可能丢失数据 | lib/astro_image_io/src/aio_pipeline_engine.cpp:43 仅注释'自定义块丢弃 |
| B2-H-1 | calibration | 校准模块线程数硬编码 16，覆盖外部设置 | lib/calibration/src/calibrator.cpp:87,155,217,226 共 4 处 '#pr |
| B2-H-2 | calibration | ac_set_num_threads API 形同虚设 | lib/calibration/src/ac_api.cpp:119-120 ac_set_num_threads 已调 |
| B2-H-3 | calibration | 坏点修复功能日志缺失 | lib/calibration/src/cosmetic_corrector.cpp:118-154 detect_ho |
| B2-H-4 | calibration | 主帧生成无质量校验 | lib/calibration/src/master_generator.cpp:37-43 仅有 ac_log 输出到 |
| B3-H-01 | plate_solve | 主流程用三角匹配，与文档描述的多边形匹配+PROSAC 不一致 | lib/plate_solve/cpp/ipv/src/ipv_solver.cpp:6 注释'triangle_mat |
| B3-H-02 | plate_solve | K-vector 索引构建无验证 | lib/plate_solve/cpp/ipv/src/ipv_kvector.cpp 经 grep '自检|verif |
| B3-H-05 | plate_solve | SIP 多项式阶数自动选择缺失 | lib/plate_solve/cpp/ipv/src/ipv_sip.cpp:64 get_basis_table(o |
| B4-H-1 | dynamic_psf | PSF 拟合中心位置存在系统偏差约 0.5 像素 | lib/dynamic_psf/src/dpsf_psf.cpp:367-368 'double img_cx = x0 |
| B4-H-2 | dynamic_psf | float↔uint16 双重精度损失 + 性能开销 | lib/dynamic_psf/src/dpsf_psf.cpp:392 'DPSF_EXPORT int dpsf_f |
| B5-H-1 | photometric_calib | 测光模块未接收 Gaia 星表块，违反架构契约 | lib/photometric_calib/cpp/src/pc_api.cpp:223 调用 gaia_client_ |
| B5-H-2 | photometric_calib | 光谱积分步长 1.0nm，与算法文档要求的 0.1nm 不一致 | lib/photometric_calib/cpp/src/spectrum_integrator.cpp:241 'c |
| B6-H-1 | snr_estimator | 架构文档数据流表未更新 (snr 块 vs snr_model 块) | docs/ARCHITECTURE.md:113 仍写 'snr (FLOAT32[H,W])'，未更新为 snr_mo |
| B6-H-2 | snr_estimator | GAP-011 状态未更新为已修复 | docs/DESIGN_IMPL_GAP.md:103 GAP-011 标题'已源代码回溯复核 2026-07-16'， |
| B8-H-1 | healpix_stack | 梯度拟合最近控制点查找是 O(n²) 性能瓶颈 | lib/healpix_db/healpix_stack/gradient/gradient_fitter.cpp:16 |
| B8-H-2 | healpix_stack | 文档说 Gauss-Seidel 迭代拟合，实际是一次性拟合 | lib/healpix_db/healpix_stack/healpix_stack.py:422,435 文档说'Ga |
| B8-H-3 | healpix_stack | STACK 阶段是空骨架，与架构文档不符 (GAP-015 未修复) | lib/orchestrator/cpp/src/orchestrator.cpp:2462-2476 run_stag |
| B8-H-4 | healpix_stack | .hiss 多帧堆叠路径无 Winsorized 选项 | lib/healpix_db/healpix_stack/hp_stack_hiss.cpp:240,264 仅实现 w |
| B8-H-5 | healpix_stack | 旧版堆叠引擎用硬编码 3 次迭代，无 Winsorized | lib/healpix_db/healpix_stack/stack_engine.cpp:63 'for (int i |
| B9-H-1 | orchestrator | 配置文件字段解析不完整，多个配置项无法生效 | lib/orchestrator/cpp/src/orchestrator.cpp:7,68,266 load_conf |
| B9-H-2 | orchestrator | PHOTOMETRIC 阶段创建未使用的星检测器/解析器句柄 (资源浪费) | lib/orchestrator/cpp/src/orchestrator.cpp:1739 '确保 PLATESOLV |
| B9-H-3 | orchestrator | Gaia API 返回值约定不统一 (布尔 vs 错误码) | lib/orchestrator/cpp/src/orchestrator.cpp:966-968 注释'gaia_cl |
| B9-H-4 | orchestrator | STACK 阶段空骨架 (与 B8-H-3 同源) | lib/orchestrator/cpp/src/orchestrator.cpp:2462-2476 run_stag |
| B9-H-5 | orchestrator | 测光定标函数有 41 个参数，维护性极差 | lib/orchestrator/cpp/src/orchestrator.cpp:1880-1889 pc_calib |
| B9-H-6 | orchestrator | PLATESOLVE 环境清理依赖 DLL 加载状态 (隐式耦合) | lib/orchestrator/cpp/src/orchestrator.cpp:1173 'if (ipv_solv |
| B9-H-7 | orchestrator | Gaia 查询返回内存用 std::free 释放，违反封装 | lib/orchestrator/cpp/src/orchestrator.cpp:1543-1544 'std::fr |

### Medium (38 项 OPEN)

| ID | 模块 | 标题 | 证据 |
|---|---|---|---|
| B1-M-4 | astro_image_io | 元数据构建未提取 SIP 关键字 | lib/astro_image_io/src/aio_fits.cpp:261-382 build_metadata 函 |
| B2-M-1 | calibration | 多处配置参数硬编码 | lib/calibration/src/calibrator.cpp:87 '#pragma omp parallel  |
| B2-M-2 | calibration | 关键步骤日志缺失 | lib/calibration/src/calibrator.cpp 经 grep 'LOG_|fprintf|log' |
| B2-M-3 | calibration | 主帧文件路径未持久化 | lib/calibration/src/ac_api.cpp 中 ac_generate_master_bias/dar |
| B2-M-4 | calibration | 错误处理不完整 | lib/calibration/src/calibrator.cpp 经 grep 'width|height|size |
| B2-M-5 | calibration | OpenMP 线程数管理混乱 | lib/calibration/src/calibrator.cpp:87 num_threads(16) 硬编码, 与 |
| B3-M-1 | plate_solve | 主流程参数硬编码 | lib/plate_solve/cpp/ipv/src/ipv_entry.cpp:77-86 RANSAC 参数 (r |
| B3-M-2 | plate_solve | 错误恢复机制不完善 | lib/plate_solve/cpp/ipv/src/ipv_entry.cpp 经 grep 'fallback|r |
| B3-M-3 | plate_solve | 日志输出不完善 | lib/plate_solve/cpp/ipv/src/ipv_entry.cpp 经 grep 'LOG_|fprin |
| B3-M-4 | plate_solve | 性能瓶颈未优化 | lib/plate_solve/cpp/ipv/src/ipv_kvector.cpp:83-122 kvector_q |
| B4-M-1 | dynamic_psf | 多处配置参数硬编码 | lib/dynamic_psf/src/dpsf_psf.cpp:306-307 lm_solve(m, NPARAMS |
| B4-M-6 | dynamic_psf | 性能优化未到位（Moffat std::pow） | lib/dynamic_psf/src/dpsf_psf.cpp:91 'std::pow(1.0 + Q, 4.0)' |
| B5-M-2 | photometric_calib | 配置参数硬编码 | lib/photometric_calib/cpp/src/star_matcher.cpp:24 '_IRLS_MAX |
| B6-M-1 | snr_estimator | 日志目录空置（仅 .gitkeep） | lib/snr_estimator/cpp/src/snr_estimator.cpp 有 10+ 处 fprintf( |
| B6-M-2 | snr_estimator | SNR 模型提取缺少 WCS 有效性校验 | lib/snr_estimator/cpp/src/snr_estimator.cpp:219-224 snr_extr |
| B6-M-3 | snr_estimator | 配置参数硬编码 | lib/snr_estimator/cpp/src/snr_estimator.cpp:63,80,128,143,16 |
| B6-M-4 | snr_estimator | 性能优化未到位（IDW 未用 KD-tree） | lib/snr_estimator/cpp/src/snr_estimator.cpp:162-197 IDW 插值使用 |
| B7-M-1 | healpix_drizzle | OpenMP 线程数硬编码 16 | lib/healpix_db/healpix_drizzle/drizzle_engine.cpp:103-104 'c |
| B7-M-2 | healpix_drizzle | SIP 多项式计算用 std::pow 性能差 | lib/healpix_db/healpix_drizzle/wcs_sip.cpp:84 'result += coe |
| B7-M-3 | healpix_drizzle | FITS 数据读取不完整时只警告不报错 | lib/healpix_db/healpix_drizzle/fits_reader.cpp:414-417 'if ( |
| B7-M-4 | healpix_drizzle | 哈希表 reserve 硬编码 4M 桶 | lib/healpix_db/healpix_drizzle/drizzle_engine.cpp:108 'acc.r |
| B8-M-1 | healpix_stack | IDW 权重中距离用度而非弧度，与算法文档不一致 | lib/healpix_db/healpix_stack/gradient/snr_evaluator.cpp:222- |
| B8-M-2 | healpix_stack | 子叶块索引计算存在 bug（nside<64 时不正确） | lib/healpix_db/healpix_stack/gradient/gradient_sampler.cpp:5 |
| B8-M-3 | healpix_stack | 降采样函数名暗示 Morton 位运算，实际用空间网格分组 | lib/healpix_db/healpix_stack/gradient/gradient_sampler.cpp:1 |
| B8-M-4 | healpix_stack | OpenMP 并行未显式控制线程数 | lib/healpix_db/healpix_stack/gradient/spherical_spline.cpp:2 |
| B8-M-7 | healpix_stack | Gaia 客户端创建失败即返回，不支持跳过星拒绝退化路径 | lib/healpix_db/healpix_stack/gradient/gradient_sampler.cpp:3 |
| B8-M-8 | healpix_stack | sigma_clip_method 配置项未做白名单校验 | lib/orchestrator/cpp/src/orchestrator.cpp:2410-2414 从 config |
| B8-M-9 | healpix_stack | JSON 头不压缩，与读取端解压逻辑不对称 | lib/healpix_db/healpix_stack/ahps_writer.cpp:332-333 'uint32 |
| B9-M-1 | orchestrator | NSIDE 自适应公式与 HEALPix 标准不符 | lib/orchestrator/cpp/src/orchestrator.cpp:152-173 注释 '1186.1 |
| B9-M-2 | orchestrator | 日志目录硬编码，各 stage 无独立日志 | lib/orchestrator/cpp/src/orchestrator.cpp:246 Logger::instan |
| B9-M-3 | orchestrator | 星点检测器默认参数硬编码 | lib/orchestrator/cpp/src/orchestrator.cpp:1108-1119 SDetPara |
| B9-M-4 | orchestrator | 检查点'已完成'判定阈值硬编码为 4（旧版 5 阶段） | lib/orchestrator/cpp/src/checkpoint.cpp:730 'if (data.curren |
| B9-M-5 | orchestrator | run_single 旧版路径已废弃但仍保留 | lib/orchestrator/cpp/src/orchestrator.cpp:333-462 run_single |
| B9-M-6 | orchestrator | 新旧管线阶段枚举并存，StageTiming 用旧枚举 | lib/orchestrator/cpp/include/orchestrator.h:37-43 旧版 5 阶段 Pi |
| B9-M-7 | orchestrator | stage1/stage2 未集成检查点（无断点续传） | lib/orchestrator/cpp/src/orchestrator.cpp:2482-2605 run_stag |
| B9-M-8 | orchestrator | Gaia 锥形查询半径 1.2 倍余量可能不足 | lib/orchestrator/cpp/src/orchestrator.cpp:1505-1506 'fov_rad |
| B9-M-9 | orchestrator | PHOTOMETRIC FOV 半径钳位 [1.0, 10.0] 度，与 PLA | lib/orchestrator/cpp/src/orchestrator.cpp:1874 'fov_radius_d |
| B9-M-10 | orchestrator | 文件头注释过时（5 阶段 vs 9 节点） | lib/orchestrator/cpp/src/orchestrator.cpp:3 '串联 5 个阶段 (CALIB |

### Low (30 项 OPEN)

| ID | 模块 | 标题 | 证据 |
|---|---|---|---|
| B1-L-2 | astro_image_io | 函数注释不全（参数含义、返回值未说明） | lib/astro_image_io/src/aio_api.cpp 中 aio_read/aio_read_fits/ |
| B1-L-5 | astro_image_io | 头文件 include 顺序不规范 | lib/astro_image_io/src/aio_api.cpp:1-6 项目头 ../include/astro_ |
| B1-L-6 | astro_image_io | 错误信息字符串不一致（中英文混用） | lib/astro_image_io/src/aio_pipeline_engine.cpp:172,219,489 错 |
| B1-L-8 | astro_image_io | 文件头注释缺失 | lib/astro_image_io/src/aio_compressor.cpp:1-10 文件开头仅条件编译注释，无 |
| B2-L-2 | calibration | 函数注释不全 | lib/calibration/src/ac_api.cpp 第 48-58 行 ac_generate_master_ |
| B2-L-5 | calibration | 头文件 include 顺序不规范 | lib/calibration/src/calibrator.cpp:29-34 项目头 ../include/astr |
| B4-L-5 | dynamic_psf | 文件头注释缺失 | lib/dynamic_psf/src/dpsf_log.cpp:1-10 文件开头直接是 #include，无功能描述 |
| B5-L-4 | photometric_calib | magic number 未提取（积分步长 1.0nm、波长范围 380-780 | lib/photometric_calib/cpp/src/spectrum_integrator.cpp:241 'c |
| B5-L-5 | photometric_calib | 注释不全（图像校正逻辑注释不全） | lib/photometric_calib/cpp/src/image_corrector.cpp:26-57 comp |
| B6-L-4 | snr_estimator | magic number 未提取（控制点采样密度、稀疏化阈值等） | lib/snr_estimator/cpp/src/snr_estimator.cpp:63,80,128,143,16 |
| B7-L-6 | healpix_drizzle | 文件头注释缺失 | lib/healpix_db/healpix_drizzle/wcs_sip.cpp:1 文件开头直接是 '#inclu |
| B7-L-8 | healpix_drizzle | 错误信息字符串不一致 | lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp:82,99,130, |
| B8-L-1 | healpix_stack | 数据块读取失败返回空 vector 语义不清晰 | lib/healpix_db/healpix_stack/ahps_reader.cpp:313-350 readChu |
| B8-L-2 | healpix_stack | 子叶块列表仅实现 Windows 路径 | lib/healpix_db/healpix_stack/stack_db.cpp:376-379 POSIX 分支仍注 |
| B8-L-3 | healpix_stack | 元数据保存用 %g 格式可能丢精度 | lib/healpix_db/healpix_stack/stack_db.cpp:230-234 saveMeta 仍 |
| B8-L-4 | healpix_stack | Winsorized 分支内存占用高 | lib/healpix_db/healpix_stack/gradient/corrected_stacker.cpp: |
| B8-L-5 | healpix_stack | gauge fixing 假设 v[0] 是常数项，依赖实现细节 | lib/healpix_db/healpix_stack/gradient/gradient_fitter.cpp:26 |
| B8-L-6 | healpix_stack | run_stage_stack 返回 true 但未执行任何工作 | lib/orchestrator/cpp/src/orchestrator.cpp:2462-2476 run_stag |
| B8-L-7 | healpix_stack | 大量 fprintf(stderr) 未走统一日志系统 | lib/healpix_db/healpix_stack/*.cpp 共 9 个文件 117 处 fprintf(std |
| B9-L-1 | orchestrator | 旧版 stage_name 函数缺少新节点分支 | lib/orchestrator/cpp/src/orchestrator.cpp:200-209 stage_name |
| B9-L-2 | orchestrator | --threads 参数未生效 | lib/orchestrator/cpp/src/cli_command.cpp:286-289 cmd_run 中 - |
| B9-L-3 | orchestrator | 输出 JSON 字段名 output_ahpx_path 过时 | lib/orchestrator/cpp/src/cli_command.cpp:495 'std::cout << " |
| B9-L-4 | orchestrator | cmd_status 输出固定'no running instance' | lib/orchestrator/cpp/src/cli_command.cpp:369-375 cmd_status  |
| B9-L-5 | orchestrator | cmd_run_batch 错误返回码 4 不规范 | lib/orchestrator/cpp/src/cli_command.cpp:336 'return 4;  //  |
| B9-L-6 | orchestrator | PLATESOLVE 中 FOV 半径上限 30 度硬编码 | lib/orchestrator/cpp/src/orchestrator.cpp:1508 'if (fov_radi |
| B9-L-7 | orchestrator | parse_ra_hms / parse_dec_dms 解析失败返回 0.0  | lib/orchestrator/cpp/src/orchestrator.cpp:793-831 parse_ra_h |
| B9-L-8 | orchestrator | --threads 参数日志说'骨架'但实际已部分支持 | lib/orchestrator/cpp/src/cli_command.cpp:288 LOG_INFO 仍说 '请求 |
| B9-L-9 | orchestrator | cmd_stage1 中 gaia_data/calibration_dir/f | lib/orchestrator/cpp/src/cli_command.cpp:212-215 仍注释 'gaia_d |
| B9-L-10 | orchestrator | cmd_run_batch 返回码逻辑：results 为空时返回 0 | lib/orchestrator/cpp/src/cli_command.cpp:359-363 'for (const |
| B9-L-11 | orchestrator | tests/ 目录缺少 stage handler 单元测试 | lib/orchestrator/cpp/tests/ 目录仅有 test_checkpoint.cpp/test_dl |

## CLOSED 项（已解决）

| ID | 模块 | 标题 | 证据 |
|---|---|---|---|
| B1-H-3 | astro_image_io | XISF 格式错误处理不完整，可能崩溃 | lib/astro_image_io/src/aio_xisf.cpp:307-346 xisf_read_file 函 |
| B3-C-03 | plate_solve | RANSAC 缺少比例预检查，导致明显错误匹配污染结果 | lib/plate_solve/cpp/ipv/src/ipv_ransac.cpp:398,521,632 有 'if |
| B3-C-04 | plate_solve | 精化阶段使用固定阈值，违反应使用动态阈值硬约束 | lib/plate_solve/cpp/ipv/src/ipv_robust_refine.cpp:600-609 co |
| B3-C-08 | plate_solve | 解析统计字段硬编码为 0，无法反映实际匹配数 | lib/plate_solve/cpp/ipv/src/ipv_solver.cpp:344 'result.n_mat |
| B3-H-03 | plate_solve | 迭代变换收敛判定不合理 | lib/plate_solve/cpp/ipv/src/ipv_itertrans.cpp:323-339 有 max_ |
| B3-H-04 | plate_solve | WCS 写回未强制 CD 矩阵无 1/cos(Dec) 因子 | lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp:116-119,155-158 CD 矩 |
| B1-M-1 | astro_image_io | FITS 维度顺序处理可能互换宽高 | lib/astro_image_io/src/aio_fits.cpp:281-282 'meta.geometry.w |
| B1-M-2 | astro_image_io | 缓存加载失败时已分配内存泄漏 | lib/astro_image_io/src/aio_pipeline.cpp:675-782 aio_frame_lo |
| B1-M-3 | astro_image_io | 编排器未传递观测元数据（DATE-OBS/JD-OBS/BUNIT/LONPOL | lib/astro_image_io/src/aio_fits.cpp:331-381 build_metadata 函 |
| B1-M-5 | astro_image_io | 多资源释放顺序不明确 | lib/astro_image_io/src/aio_pipeline_engine.cpp:262-272 aio_p |
| B4-M-2 | dynamic_psf | 关键步骤日志缺失 | lib/dynamic_psf/src/dpsf_psf.cpp 有 20+ 处 dpsf_log 调用: :155 L |
| B4-M-3 | dynamic_psf | 错误处理不完整 | lib/dynamic_psf/src/dpsf_psf.cpp:397 invalid arguments, :413 |
| B4-M-4 | dynamic_psf | 资源管理不严格 | lib/dynamic_psf/src/dpsf_psf.cpp:472 '#pragma omp parallel f |
| B4-M-5 | dynamic_psf | OpenMP 线程数硬编码 | lib/dynamic_psf/src/dpsf_psf.cpp 经 grep 'omp_set_num_threads |
| B4-M-7 | dynamic_psf | 命名不一致 | lib/dynamic_psf/src/dpsf_psf.cpp:367-368 'img_cx', 'img_cy'  |
| B5-M-1 | photometric_calib | 关键步骤日志缺失 | lib/photometric_calib/cpp/src/pc_api.cpp 有 15+ 处 fprintf 日志: |
| B5-M-3 | photometric_calib | 错误处理不完整 | lib/photometric_calib/cpp/src/pc_api.cpp:65-67 检查 n_gaia<=0  |
| B5-M-4 | photometric_calib | 性能优化未到位（F_syn 积分未并行化） | lib/photometric_calib/cpp/src/pc_api.cpp:303 '#pragma omp pa |
| B5-M-5 | photometric_calib | 命名不一致 | lib/photometric_calib/cpp/src/*.cpp 变量命名统一 snake_case (wl_in |
| B5-M-6 | photometric_calib | 注释不全（F_syn 公式无注释） | lib/photometric_calib/cpp/src/spectrum_integrator.cpp:260 '被 |
| B8-M-5 | healpix_stack | HEALPix 像素数计算未强制 int64 防 npface 溢出 | lib/healpix_db/healpix_stack/healpix_core.cpp:55 'int64_t He |
| B1-L-1 | astro_image_io | 代码风格不一致（部分文件用 4 空格，部分用 tab） | lib/astro_image_io/src/*.cpp 经 grep '^\t' 检查无 tab 缩进，所有源文件统一 |
| B1-L-3 | astro_image_io | 变量命名混用（局部变量 camelCase / snake_case 不统一） | lib/astro_image_io/src/aio_pipeline.cpp (base64_encode/xml_e |
| B1-L-4 | astro_image_io | magic number 未提取为常量 | lib/astro_image_io/src/aio_fits.cpp:12-13 已提取 'static const  |
| B1-L-7 | astro_image_io | TODO 注释残留 | lib/astro_image_io/src/ 经 grep 'TODO|FIXME' 检查无残留 |
| B2-L-1 | calibration | 代码风格不一致 | lib/calibration/src/*.cpp 经 grep '^\t' 检查无 tab 缩进，风格统一 |
| B2-L-3 | calibration | 变量命名混用 | lib/calibration/src/calibrator.cpp 变量命名风格统一为 snake_case (med |
| B2-L-4 | calibration | magic number 未提取 | lib/calibration/src/cosmetic_corrector.cpp:119,140 threshold |
| B3-L-1 | plate_solve | 代码风格不一致 | lib/plate_solve/cpp/ipv/src/*.cpp 经 grep '^\t' 检查无 tab 缩进，风格 |
| B3-L-2 | plate_solve | 算法原理注释缺失（如 RANSAC 数学推导） | lib/plate_solve/cpp/ipv/src/ipv_ransac.cpp:1-24 有详细算法注释（PROS |
| B3-L-3 | plate_solve | 变量命名混用 | lib/plate_solve/cpp/ipv/src/ipv_solver.cpp 变量命名风格统一 |
| B4-L-1 | dynamic_psf | 代码风格不一致 | lib/dynamic_psf/src/*.cpp 经 grep '^\t' 检查无 tab 缩进，风格统一 |
| B4-L-2 | dynamic_psf | 函数注释不全（Moffat4 函数原型、拟合算法注释不全） | lib/dynamic_psf/src/dpsf_psf.cpp:12-18 有 MOFFAT4_FWHM_FACTOR |
| B4-L-3 | dynamic_psf | 变量命名混用 | lib/dynamic_psf/src/dpsf_psf.cpp 变量命名风格统一 (SamplePixel/gauss |
| B4-L-4 | dynamic_psf | magic number 未提取（patch 大小 17、最大迭代 100） | lib/dynamic_psf/src/dpsf_psf.cpp:98 max_iter 已作为 lm_solve 函数 |
| B5-L-1 | photometric_calib | 代码风格不一致 | lib/photometric_calib/cpp/src/*.cpp 经 grep '^\t' 检查无 tab 缩进， |
| B5-L-2 | photometric_calib | 函数注释不全 | lib/photometric_calib/cpp/src/pc_api.cpp:1-3 文件头注释 + :27-34  |
| B5-L-3 | photometric_calib | 变量命名混用 | lib/photometric_calib/cpp/src/*.cpp 变量命名风格统一为 snake_case (wl |
| B5-L-6 | photometric_calib | 文件头注释缺失 | lib/photometric_calib/cpp/src/wcs_transform.cpp:1-4 有完整文件头注释 |
| B6-L-1 | snr_estimator | 代码风格不一致 | lib/snr_estimator/cpp/src/snr_estimator.cpp 经 grep '^\t' 检查无 |
| B6-L-2 | snr_estimator | 函数注释不全（SNR 估算公式、IDW 插值算法注释不全） | lib/snr_estimator/cpp/src/snr_estimator.cpp:1-9 文件头注释含 SNR_p |
| B6-L-3 | snr_estimator | 变量命名混用 | lib/snr_estimator/cpp/src/snr_estimator.cpp 变量命名风格统一 (snr_ph |
| B6-L-5 | snr_estimator | 注释不全（乘法模型 SNR = SNR_phot × (IDW/median)  | lib/snr_estimator/cpp/src/snr_estimator.cpp:1-9 文件头注释含乘法模型完整 |
| B6-L-6 | snr_estimator | 文件头注释缺失 | lib/snr_estimator/cpp/src/snr_estimator.cpp:1-9 有完整文件头注释 (模块 |
| B7-L-1 | healpix_drizzle | 代码风格不一致 | lib/healpix_db/healpix_drizzle/*.cpp 经 grep '^\t' 检查仅 Makefi |
| B7-L-2 | healpix_drizzle | 函数注释不全 | lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp:1-3 文件头注释  |
| B7-L-3 | healpix_drizzle | 变量命名混用 | lib/healpix_db/healpix_drizzle/*.cpp 变量命名风格统一 (greatCircleDi |
| B7-L-4 | healpix_drizzle | magic number 未提取（drizzle_factor 1.5、nsid | lib/healpix_db/healpix_drizzle/drizzle_engine.cpp 中 nside/pi |
| B7-L-5 | healpix_drizzle | 注释不全（Sutherland-Hodgman 算法原理无注释） | lib/healpix_db/healpix_drizzle/poly_clip.cpp:133-143 有 Suthe |
| B7-L-7 | healpix_drizzle | TODO 注释残留 | lib/healpix_db/healpix_drizzle/*.cpp 经 grep 'TODO|FIXME' 检查无 |

## REJECTED 项

- **B8-M-6** (healpix_stack): fact2 系数未在 healpix_stack 中实现 — lib/healpix_db/healpix_stack/healpix_core.cpp:441-444 pix2ang 使用 pix2xy+xy2ang (astrometry.net 风格实现), 经 grep 'fact2|npface' 无匹配, 此模块不需要 fact2 系数, 硬约束 'fact2=1.0/(3*npface)' 仅适用于 healpix_browser_qt 模块

## 复核方法

1. 读取 2026-07-18 代码审计文档（P0P1/P2/P3 三个 findings 文件）
2. 对每项提取文件定位与问题描述
3. 使用 Read/Grep 工具读取当前源码对应位置
4. 对照问题描述判断当前状态（OPEN/CLOSED/STALE/UNVERIFIED/REJECTED）
5. 记录证据（文件:行号 或说明）

## 详细复核清单
- P0P1 (50 项): audit_reconciliation_P0P1.json / .md
- P2 (54 项): audit_reconciliation_P2.json / .md
- P3 (59 项): audit_reconciliation_P3.json / .md