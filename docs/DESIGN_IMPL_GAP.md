# 设计与实施割裂记录（DESIGN_IMPL_GAP）

> 创建日期: 2026-07-16
> 目的: 记录架构文档/spec 设计与实际代码实施之间的偏差与断层，供后续开发参考。
> 本文档随割裂点发现持续更新；每项割裂需标注：发现日期、类型、描述、影响、建议修复方式。

---

## 1. 模块归档状态割裂

### GAP-001：healpix_browser_cpp/web 文档标称已归档但代码仍在原位 —— 已修复

- **发现日期**: 2026-07-16
- **修复日期**: 2026-07-16
- **状态**: 已修复
- **描述**:
  旧 PROJECT_ARCHITECTURE.md 声称 `healpix_browser_cpp/`（C++ HTTP 后端 + winsock2）和 `healpix_browser_web/`（WebGL 前端）已归档到 `lib/healpix_db/archive/`，由 `healpix_browser_qt/` 替代。
  **实际状态**：archive/ 下确实有副本，但原位 `lib/healpix_db/healpix_browser_cpp/` 和 `lib/healpix_db/healpix_browser_web/` 仍然存在（双重存在）。
- **修复方式**:
  - 2026-07-16 删除 `healpix_browser_cpp/` 顶层（与 archive/healpix_browser_cpp/ 字节级重复，已验证）
  - `healpix_browser_web/` 原位本就不存在（仅 archive/ 下有副本）
- **影响**: 已消除

### GAP-002：healpix_io 实际代码已移至 archive/ 但仍标为活跃 —— 已修复

- **发现日期**: 2026-07-16
- **修复日期**: 2026-07-16
- **状态**: 已修复
- **描述**:
  旧 PROJECT_ARCHITECTURE.md 将 `healpix_io` 标为活跃模块（.hiss/.hcsd 读写）。
  **实际状态**：`lib/healpix_db/healpix_io/` 下只剩 `ARCHIVED.md` 和 `archive/` 子目录（含实际代码 healpix_io.h/.cpp/.py 等），原位代码已被移走。
- **修复方式**:
  - healpix_browser_qt 依赖从 `../healpix_io/` 迁移至 `lib/astro_image_io/`（aio 模块已完整接管 healpix_io API，含兼容宏）
  - 修改 CMakeLists.txt/Makefile/deploy.ps1：HIO_DIR → AIO_DIR（../../astro_image_io），链接库名 healpix_io → astro_image_io，添加 AIO_ENABLE_HEALPIX 定义
  - browser_backend.cpp: #include "healpix_io.h" → #include "aio_healpix_io.h"
  - 验证：astro_image_io.dll 构建成功（9 个 HEALPix I/O 符号全部导出），healpix_browser_qt CMake 完整构建成功（34/34）
- **影响**: 已消除

### GAP-003：healpix_browser（PyQt5+vispy）已废弃但未归档 —— 已修复

- **发现日期**: 2026-07-16
- **修复日期**: 2026-07-16
- **状态**: 已修复
- **描述**:
  `lib/healpix_db/healpix_browser/`（PyQt5+vispy 旧版浏览器）文档标称已废弃，但代码仍在原位，未移入 archive/。
- **修复方式**: 移至 `lib/healpix_db/archive/legacy/healpix_browser_python/`
- **影响**: 已消除

### GAP-004：healpix_lod 已废弃但未归档 —— 已修复

- **发现日期**: 2026-07-16
- **修复日期**: 2026-07-16
- **状态**: 已修复
- **描述**:
  `lib/healpix_db/healpix_lod/`（旧版 LOD 金字塔，.ahpl 文件）文档标称已废弃，由 healpix_browser_qt 内存 ud_grade 替代，但代码仍在原位。
- **修复方式**: 移至 `lib/healpix_db/archive/legacy/healpix_lod/`
- **影响**: 已消除

---

## 2. 模块仓库状态割裂

### GAP-005：plate_solve 本地 .git 目录损坏（缺少 objects/）—— 已修复

- **发现日期**: 2026-07-16
- **修复日期**: 2026-07-16
- **状态**: 已修复
- **描述**:
  plate_solve 本地 `lib/plate_solve/.git` 目录缺少 objects/，git 命令全部失败。远端仓库 `PlateSolve-IPV-Cpp` 存在但体积大（含 astrometry 等），完整 clone 耗时过长；部分克隆（--filter=blob:none）可获取元数据但 commit 时因 blob 缺失卡住。
- **修复方式**:
  1. 删除损坏的 .git（终止残留 git 进程后重命名删除）
  2. git init -b main 重建仓库
  3. git add . 暂存当前干净状态（2536 文件，已删除 astrometry + 历史日志 5123 文件约 475MB）
  4. git commit（308f209）
  5. git push --force origin main（0ede51e → 308f209，远端被覆盖为干净历史）
- **影响**:
  - 远端历史被覆盖为全新历史（丢失旧 commit 历史，但不含废弃的 astrometry 等大文件）
  - 本地仓库恢复正常工作

### GAP-006：data_pipeline 从 astro_image_io 拆分未完成 —— 待修复

- **发现日期**: 2026-07-16
- **修正日期**: 2026-07-16（纠正误判：data_pipeline 是数据总线，非孤岛模块）
- **状态**: 待修复（拆分中间态）
- **描述**:
  `lib/data_pipeline/` 是数据总线模块（PipelineFrame + PipelineEngine + 命名块容器），2026-07-12 从 astro_image_io 拆分为独立仓库（Astro-Data-Pipeline）。
  **当前状态**：拆分未完成——astro_image_io 中仍保留 aio_pipeline.cpp/aio_pipeline_engine.cpp 副本，data_pipeline 中的同名文件是正式归属。两模块导出同名 C API（aio_frame_*/aio_engine_*），若同时加载会符号冲突。
- **影响**:
  - 模块边界不清（astro_image_io 保留应属于 data_pipeline 的代码）
  - 潜在符号冲突（两模块导出同名 C API）
  - 消费者（orchestrator 等）实际依赖 astro_image_io 的副本而非 data_pipeline
- **建议修复**:
  1. astro_image_io 移除 aio_pipeline.cpp/aio_pipeline_engine.cpp，改为依赖 data_pipeline
  2. 或确认 astro_image_io 为唯一消费者，将 data_pipeline 合并回 astro_image_io
  3. 文档中明确数据总线的归属

---

## 6. 管线流程文档与实现割裂（2026-07-16 发现）

> 来源：基于 `docs/PIPELINE_OVERVIEW.md`（12 步流程）vs 实际代码实现的对比分析。

### GAP-011：SNR 接口链路断裂（严重，已源代码回溯复核 2026-07-16）

- **发现日期**: 2026-07-16

- **类型**: 管线断层

- **优先级**: 高

- **描述**:
  本割裂指"orchestrator 编排时调用的 SNR 估算函数"与"drizzle 阶段实际读取的内存块名"不匹配，导致整个 SNR²加权叠加链路从源头就断开。
  
  **名词解释**：
  
  - **snr_estimate**：snr_estimator 模块导出的旧版 C API。输入图像像素数组和 PSF 拟合结果，输出稠密 SNR 图（与图像同尺寸的二维 float 数组），写入 PipelineFrame 的 "snr" 命名块。该 API 在头文件注释中已明确"保留用于测试/调试，管线中不再调用"。
  - **snr_extract_model**：snr_estimator 模块 2026-07-16 新增的 C API。输入 PSF 拟合结果和 WCS 参数，输出稀疏控制点模型（SnrModel 结构体：含控制点数组、帧级全局 SNR 标量、归一化基准、IDW 幂次），应写入 "snr_model" 命名块。
  - **snr 块**：稠密 SNR 图，类型为像素块（FLOAT32 数组），尺寸等于图像 [H*W]。
  - **snr_model 块**：稀疏控制点模型，类型为原始字节块（AIO_BLOCK_RAW），按 [点数 uint32 + 控制点数组 + 3 个 double 参数] 序列化。
  - **AioBlock**：astro_image_io 模块定义的命名块结构体，包含 type/data/count 等字段，是 PipelineFrame 中数据传递的统一容器。
  
  **核查到的实际链路**：
  
  1. orchestrator 的 SNR 阶段处理器（run_stage_snr）通过 DLL 函数指针调用 snr_estimate，把稠密 SNR 图写入 "snr" 块；
  2. drizzle 阶段处理器在 hp_drizzle_run 内只查找 "snr_model" 块；
  3. 当找不到 "snr_model" 块时，drizzle 把 SNR 指针置空，drizzle 引擎内部把所有像素的 SNR 视为 1.0；
  4. snr_extract_model 新 API 已实现并提交（commit 405d153），但 orchestrator 仍调旧版 snr_estimate，未切换；
  5. 由此整条 SNR²加权链路在 stage1→stage2 之间断裂，stage2 的"SNR²加权叠加"实际为等权叠加。
  
  **涉及模块**：orchestrator（调用方）、snr_estimator（被调用方）、healpix_drizzle（下游消费者）。

- **影响**:
  
  - SNR²加权叠加完全失效，stage2 实际为等权叠加；
  - 高信噪比区域无法获得更高权重，最终天球数据库精度退化；
  - snr_extract_model 新 API 已实现但闲置，是无效投入。

- **建议修复**:
  
  - 把 orchestrator 的 run_stage_snr 改为调用 snr_extract_model；
  - 把输出从 "snr" 块改为 "snr_model" 块（按 snr_estimator.h 规定的序列化格式）；
  - 释放时调 snr_free_model 释放控制点数组；
  - 修复后用单帧端到端测试验证 drizzle 日志中能看到 "snr_model 块加载: n_points=..." 而非 "无 snr_model 块"。

- **批复意见**:这里要求hiss中存储稀疏的控制点，而非稠密的SNR图层。SNR计算阶段不直接计算出稠密图层，而是直接计算出控制点，随着drizzle步骤一起转化到球面坐标系上，落盘。后续步骤在使用的时候再展开计算。

- **复核记录**: 2026-07-16 源代码回溯验证：orchestrator.cpp run_stage_snr 中函数指针名为 "snr_estimate"（确认旧版）；hp_drizzle_api.cpp 中读取 "snr_model" 块且找不到时走 nullptr（确认断裂）。

### GAP-012：CCD QE 曲线未使用（严重，已源代码回溯复核 2026-07-16）

- **发现日期**: 2026-07-16

- **类型**: 算法实现缺失

- **优先级**: 高

- **描述**:
  本割裂指"测光定标算法文档中描述的 CCD 量子效率曲线 Q(λ)"在 C API 实际实现中未传入，导致 F_syn 积分不完整。
  
  **名词解释**：
  
  - **CCD QE 曲线 Q(λ)**：CCD 量子效率曲线，描述不同波长下 CCD 的响应比例（0~1），是测光定标公式 F_syn = ∫ S(λ)·T(λ)·Q(λ) dλ 中的关键项。
  - **滤光片透过率 T(λ)**：滤光片波长-透过率数组（filter_wl/filter_trans/filter_count 三个参数）。
  - **Gaia DR3SP 光谱 S(λ)**：Gaia DR3 BP/RP 光谱，波长数组 spectrum_wl + uint8 光谱数据（在 DLL 内部由 gaia_client 查询）。
  - **F_syn**：合成流量，即理论积分流量，由 Gaia 光谱×滤光片透过率×CCD QE 曲线积分得到。
  - **pc_calibrate_simple_with_gaia**：photometric_calib 模块的扩展 C API，接受 gaia_client handle 在 DLL 内部查询 Gaia 光谱。
  
  **核查到的实际情况**：
  
  1. photometric_calib 模块的头文件中，pc_calibrate_simple_with_gaia 函数参数列表完整包含：gaia_client_handle、锥形搜索中心与半径、星等范围、滤光片透过率数组（filter_wl/filter_trans/filter_count）、光谱波长数组（spectrum_wl/spectrum_count）、图像与 PSF 数据、WCS/SIP 参数；
  2. 参数列表中**没有** qe_curve 相关参数（无 qe_wl/qe_trans/qe_count 等）；
  3. 函数内部注释说明"F_syn = Akima+Simpson 积分"，但积分项实际只有 S(λ)·T(λ)，缺少 Q(λ)；
  4. 算法文档 algorithm.md 中公式明确含 Q(λ)；
  5. 配置文件 stage1_config.json 中有 "qe_curve": "GSENSE2020BSI" 字段，data/response_curves/qe_curves.json 文件存在，但 orchestrator 没有把 QE 曲线数据传给 C API。
  
  **涉及模块**：photometric_calib（C API 实现缺失参数）、orchestrator（未传递 QE 数据）、algorithm.md（文档描述含 Q(λ)）。

- **影响**:
  
  - F_syn 积分不完整（少了 Q(λ) 项），理论流量偏大；
  - 全局 scale 因子偏小（scale = median(F_syn/F_instr)，F_syn 偏大→scale 偏小）；
  - 后续 SNR_phot 计算受影响（sigma_residual 来自测光残差）；
  - 测光定标精度低于算法文档描述。

- **建议修复**:
  
  - 扩展 pc_calibrate_simple_with_gaia C API 签名，添加 qe_wl/qe_trans/qe_count 三个参数（参考 filter_wl 的传入方式）；
  - 在 DLL 内部把 Q(λ) 加入 F_syn 积分（S(λ)·T(λ)·Q(λ)）；
  - orchestrator 从 stage1_config.json 读取 qe_curve 字段，加载对应的 qe_curves.json 数据传给 C API；
  - 同时更新 pc_calibrate_simple（无 gaia 版本）保持参数一致。

- **批复意见**:必须修复。星点通量应该是光谱，ccd curve，filter curve共同计算出来的

- **复核记录**: 2026-07-16 源代码回溯验证：photometric_calib.h 中 pc_calibrate_simple_with_gaia 参数列表确认无 qe_curve 相关参数。

### GAP-013：photometric_calib C API 是简化版（严重，已源代码回溯复核 2026-07-16）

- **发现日期**: 2026-07-16

- **类型**: 算法实现不完整

- **优先级**: 高

- **描述**:
  本割裂指"算法文档描述的完整测光定标流程"与"C API 实际实现的简化版流程"之间存在显著差距。
  
  **名词解释**：
  
  - **IRLS**：Iteratively Reweighted Least Squares，迭代重加权最小二乘。算法文档描述的版本：50 次迭代收敛，每次迭代根据残差用 Tukey biweight 函数重新计算权重，逐步剔除异常恒星。
  - **Tukey biweight**：一种稳健权重函数 w(r) = (1-(r/c)²)² 当 |r|<c，否则 w=0。比 MAD 清洗更平滑。
  - **LOOCV**：Leave-One-Out Cross-Validation，留一交叉验证。算法文档描述用于自动选择多项式阶数。
  - **MAD 清洗**：Median Absolute Deviation 离群值清洗，C API 实际使用的简化方法，sigma=3.0 一次性剔除。
  - **scale 因子**：图像流量到测光坐标系的转换因子。简化版用全局标量 scale = median(F_syn/F_instr)；完整版应该是多项式曲面 scale(x,y)。
  - **GRADIENT_2D 阶段**：spec §2.3.2 中独立的 step4 C++化阶段，乘性梯度曲面拟合 + 图像校正（IRLS+Tukey+Ridge+LOOCV），是 C API 缺失功能的补完。
  
  **核查到的实际情况**：
  
  1. photometric_calib.h 头文件注释明确写"简化版测光校准 C API"；
  2. C API 算法步骤：①WCS 投影 Gaia 星到像素坐标（TAN+SIP 投影）②暴力最近邻匹配 PSF 星和 Gaia 星（距离<3px）③MAD 离群清洗（r=log10(F_instr/F_syn), sigma=3.0）④scale = median(F_syn/F_instr)（全局标量）⑤I_cal = I * scale；
  3. 完整版算法文档描述的 IRLS+Tukey biweight 50 次迭代 + LOOCV 自动选阶 在 C API 中均未实现；
  4. 多项式曲面拟合（scale(x,y)）在 C API 中未实现，只算全局标量；
  5. PIPELINE_OVERVIEW.md 步骤4"测光定标"描述"CCD 响应 + 滤镜透过率 + Gaia 积分流量 → 迭代线性定标"，与 C API 简化版不符；
  6. 实际管线中，GRADIENT_2D 阶段（stage 5）独立完成乘性梯度曲面拟合 + IRLS + Tukey + Ridge + LOOCV，是简化版 C API 的功能补完，但 PIPELINE_OVERVIEW.md 未列出此阶段。
  
  **涉及模块**：photometric_calib（C API 简化版）、gradient_2d（曲面拟合补完）、PIPELINE_OVERVIEW.md（文档未描述 GRADIENT_2D）。

- **影响**:
  
  - 单看 PHOTOMETRIC 阶段，定标精度低于算法文档描述（无 IRLS 迭代，无曲面拟合）；
  - 加上 GRADIENT_2D 阶段才达到算法文档描述的精度，但文档未说明此补完关系；
  - 新开发者读 PHOTOMETRIC 阶段代码会误以为定标就是简化版。

- **建议修复**:
  
  - 方案A（推荐）：在 PIPELINE_OVERVIEW.md 中明确 PHOTOMETRIC 是简化版 + GRADIENT_2D 补完曲面拟合，两者合起来才是完整定标；
  - 方案B：在 photometric_calib C API 中实现完整 IRLS+Tukey+LOOCV，把 GRADIENT_2D 合并回 PHOTOMETRIC；
  - 方案C：保持现状但在 algorithm.md 中标注"简化版 C API + GRADIENT_2D 补完"。

- **批复意见**:这步骤不拟合天光，要求确切将图像转化到测光坐标系，不仅仅是测光零点，还有亮度比例也要正确

- **复核记录**: 2026-07-16 源代码回溯验证：photometric_calib.h 注释确认"简化版"；C API 步骤明确无 IRLS。

### GAP-014：stage1 节点拆分与实际不符（中，已源代码回溯复核 2026-07-16）

- **发现日期**: 2026-07-16

- **类型**: 文档与实现结构不一致

- **优先级**: 中

- **描述**:
  本割裂指"PIPELINE_OVERVIEW.md 描述的第一阶段 8 步"与"orchestrator 实际执行的 8 个节点"在步骤划分上不一致。
  
  **名词解释**：
  
  - **PipelineStageV2**：orchestrator 内部定义的阶段枚举，第一阶段 8 个节点 READ_FITS/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/GRADIENT_2D/SNR/DRIZZLE，第二阶段 2 个节点 GRADIENT_SPHERE/STACK，共 10 节点。
  - **READ_FITS**：orchestrator 实际流水线的第 0 步，调用 astro_image_io 读取 FITS 文件构造 PipelineFrame。PIPELINE_OVERVIEW.md 未列出此步骤（直接从"基础校准"开始）。
  - **GRADIENT_2D**：orchestrator 实际流水线的第 5 步，调用 gradient_2d 模块做乘性梯度曲面拟合 + 图像校正。PIPELINE_OVERVIEW.md 未列出此步骤。
  
  **核查到的差异**：
  
  1. PIPELINE_OVERVIEW.md 第一阶段列出 8 步：基础校准/板解算/PSF/测光定标/帧级基准SNR/稀疏SNR控制点/Drizzle重采样/HISS落盘；
  2. orchestrator 实际第一阶段 8 节点：READ_FITS/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/GRADIENT_2D/SNR/DRIZZLE；
  3. 文档缺 READ_FITS（stage 0，读 FITS 构造 PipelineFrame）；
  4. 文档缺 GRADIENT_2D（stage 5，乘性梯度曲面拟合）；
  5. 文档把 SNR 拆为 2 步（帧级基准 SNR + 稀疏控制点），实际只有 1 个 SNR 节点（snr_estimator 一次输出）；
  6. 文档把 Drizzle 拆为 2 步（重采样 + HISS 落盘），实际只有 1 个 DRIZZLE 节点（hp_drizzle_run 一次完成）；
  7. 文档 8 步 vs 实际 8 节点数量相同但划分不同。
  
  **涉及模块**：PIPELINE_OVERVIEW.md（文档描述）、orchestrator（实际执行）。

- **影响**:
  
  - 文档误导对管线结构的理解，新开发者可能找不到 READ_FITS 和 GRADIENT_2D 在哪里；
  - 文档拆分 SNR 和 Drizzle 为 2 步，实际只有 1 步，可能误以为有独立节点。

- **建议修复**:
  
  - 更新 PIPELINE_OVERVIEW.md 第一阶段表格为实际 8 节点结构：READ_FITS/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/GRADIENT_2D/SNR/DRIZZLE；
  - SNR 节点说明改为"一次输出帧级基准 + 稀疏控制点"；
  - DRIZZLE 节点说明改为"一次完成重采样 + HISS 落盘"。

- **批复意见**:没看太明白什么问题

- **复核记录**: 2026-07-16 源代码回溯验证：orchestrator.h PipelineStageV2 枚举确认 10 节点；PIPELINE_OVERVIEW.md 表格确认 8 步但缺 READ_FITS 和 GRADIENT_2D。

### GAP-015：stage2 4 步合并为 1 函数 + STACK 空骨架（中，已源代码回溯复核 2026-07-16）

- **发现日期**: 2026-07-16

- **类型**: 文档与实现结构不一致

- **优先级**: 中

- **描述**:
  本割裂指"PIPELINE_OVERVIEW.md 描述的第二阶段 4 步"与"orchestrator 实际执行 2 节点（其中 STACK 是空骨架）"不一致。
  
  **名词解释**：
  
  - **hp_stack_gradient_corrected**：healpix_stack 模块导出的单一 C API，注释明确"完整流程: 采样 → Gauss-Seidel 拟合 → 校正叠加 → .hcsd 输出"，一个函数完成 4 件事。
  - **run_stage_gradient_sphere**：orchestrator 中 GRADIENT_SPHERE 阶段处理器，调用 hp_stack_gradient_corrected。
  - **run_stage_stack**：orchestrator 中 STACK 阶段处理器，注释明确"跳过: .hcsd 已由 GRADIENT_SPHERE 生成"，实际是空骨架。
  - **Gauss-Seidel 拟合**：球面 TPS 梯度拟合的迭代求解器。
  - **sigma-clip**：离群值剔除方法，阈值 sigma=3.0，最大迭代 5 次。
  
  **核查到的差异**：
  
  1. PIPELINE_OVERVIEW.md 第二阶段列出 4 步：梯度校准/区域 SNR 拟合/离群值剔除/SNR²加权叠加；
  2. orchestrator 实际第二阶段 2 节点：GRADIENT_SPHERE（stage 8）+ STACK（stage 9）；
  3. GRADIENT_SPHERE 节点调用 hp_stack_gradient_corrected，一个函数完成"采样→Gauss-Seidel 拟合→校正叠加→.hcsd 输出"4 件事，对应文档 4 步；
  4. STACK 节点是空骨架，注释明确".hcsd 已由 GRADIENT_SPHERE 生成"，直接返回成功；
  5. 文档 4 步实际是 1 个函数的 4 个内部步骤，而非 4 个独立节点；
  6. 第二阶段实际只有 1 个有效节点（GRADIENT_SPHERE），STACK 是占位。
  
  **涉及模块**：PIPELINE_OVERVIEW.md（文档描述）、orchestrator（实际执行）、healpix_stack（hp_stack_gradient_corrected 实现）。

- **影响**:
  
  - 文档高估了 stage2 的节点独立性，新开发者可能误以为 4 步是 4 个独立节点；
  - STACK 阶段作为空骨架存在，增加理解成本。

- **建议修复**:
  
  - 方案A：更新 PIPELINE_OVERVIEW.md 第二阶段表格为实际 2 节点结构（GRADIENT_SPHERE 含 4 个内部步骤 + STACK 空骨架说明）；
  - 方案B：拆分 hp_stack_gradient_corrected 为 4 个独立步骤函数，让 STACK 节点有实际内容；
  - 方案C：删除 STACK 节点，第二阶段只保留 GRADIENT_SPHERE。

- **批复意见**:

- **复核记录**: 2026-07-16 源代码回溯验证：hp_stack_api.h 中 hp_stack_gradient_corrected 注释确认 4 步合并；orchestrator.cpp run_stage_stack 注释确认空骨架。

### GAP-016：NSIDE 自适应未实现（中，已源代码回溯复核 2026-07-16）

- **发现日期**: 2026-07-16

- **类型**: 功能未实现

- **优先级**: 中

- **描述**:
  本割裂指"PIPELINE_OVERVIEW.md 描述的 NSIDE 自适应（1-2x 采样率）"在 orchestrator 实际代码中固定使用 32768。
  
  **名词解释**：
  
  - **NSIDE**：HEALPix 球面细分参数，nside=32768 对应球面像素约 0.629 角秒，nside 越大分辨率越高。
  - **1-2x 采样率**：HEALPix 输出网格的采样率应处于原始图像采样率的 1 倍到 2 倍之间，避免欠采样（丢失细节）或过采样（无谓增加存储）。
  - **nside_strategy**：stage1_config.json 中的配置字段，值 "1x_to_2x_drizzle" 表示启用 1-2x 自适应策略。
  - **drizzle 调用**：orchestrator 调用 hp_drizzle_run 时传入 nside 参数，固定值 32768。
  
  **核查到的实际情况**：
  
  1. orchestrator 调用 hp_drizzle_run 时 nside 参数固定传 32768（注释明确"nside=32768 默认"）；
  2. stage1_config.json 中有 "nside_strategy": "1x_to_2x_drizzle" 字段；
  3. orchestrator 没有读取 nside_strategy 字段，没有根据原始图像采样率计算合适的 nside；
  4. 对所有 FOV 和采样率的图像都用 nside=32768，可能导致宽 FOV 过采样（无谓增加存储）或窄 FOV 欠采样（丢失细节）；
  5. PIPELINE_OVERVIEW.md 步骤7"Drizzle 重采样"描述"NSIDE 自适应 1-2x 采样率"。
  
  **涉及模块**：orchestrator（实际固定 32768）、stage1_config.json（有配置但未读取）、PIPELINE_OVERVIEW.md（文档描述自适应）。

- **影响**:
  
  - 固定 NSIDE 可能导致过采样或欠采样；
  - 宽 FOV 图像（如 50mm 镜头）会生成过大的 .hiss 文件；
  - 窄 FOV 图像（如 2000mm 镜头）可能丢失细节。

- **建议修复**:
  
  - 在 orchestrator 中实现 NSIDE 自适应逻辑：从 WCS CD 矩阵计算原始图像采样率（角秒/像素），再根据 nside_strategy 计算合适的 nside（HEALPix 像素分辨率 = 3600×60/nside 角秒，确保 1-2x 采样率）；
  - 从 stage1_config.json 读取 nside_strategy 字段；
  - 把计算出的 nside 传给 hp_drizzle_run 而非固定 32768。

- **批复意见**:这个自适应是 默认缺省，也可以用户传入参数指定

- **复核记录**: 2026-07-16 源代码回溯验证：orchestrator.cpp 中 fn_drizzle 调用确认 nside 固定 32768。

### GAP-017：Winsorized sigma clip 未实现（中，已源代码回溯复核 2026-07-16）

- **发现日期**: 2026-07-16

- **类型**: 功能未实现

- **优先级**: 中

- **描述**:
  本割裂指"PIPELINE_OVERVIEW.md 描述的 Winsorized sigma clip"在 corrected_stacker 实际代码中是普通 sigma-clip。
  
  **名词解释**：
  
  - **sigma-clip**：离群值剔除方法，阈值 sigma=3.0，最大迭代 5 次。每次迭代计算剩余数据的标准差，剔除 |x-mean|>3sigma 的点，直到收敛或达到最大迭代次数。
  - **Winsorized sigma clip**：稳健版 sigma-clip，不直接剔除离群值，而是把超出阈值的值"缩尾"到阈值边界（如 95% 分位数），减少极端值对均值/标准差的影响，比普通 sigma-clip 更稳健。
  - **CorrectedStackParams**：corrected_stacker 模块的参数结构体，含 sigma（默认 3.0）和 max_iter（默认 5）。
  - **sigma_clip_method**：stage2_config.json 中的配置字段，值 "winsorized" 表示启用 Winsorized sigma clip。
  
  **核查到的实际情况**：
  
  1. corrected_stacker.h 中 CorrectedStackParams 结构体只有 sigma 和 max_iter 两个字段，没有 winsorized 标志或缩尾分位数参数；
  2. 头文件注释只说"sigma-clip"，未提 Winsorized；
  3. stage2_config.json 中有 "sigma_clip_method": "winsorized" 字段；
  4. orchestrator 没有把 sigma_clip_method 字段传给 hp_stack_gradient_corrected；
  5. hp_stack_gradient_corrected 的参数列表中也没有 sigma_clip_method 参数；
  6. 实际执行的是普通 sigma-clip，不是 Winsorized。
  
  **涉及模块**：corrected_stacker（实际普通 sigma-clip）、stage2_config.json（有配置但未传递）、orchestrator（未读取配置）、PIPELINE_OVERVIEW.md（文档描述 Winsorized）。

- **影响**:
  
  - 离群值剔除不如 Winsorized 稳健，极端值可能影响均值/标准差估计；
  - 多帧叠加时若某帧有 cosmic ray 或 bad pixel，普通 sigma-clip 可能需要更多迭代才能剔除。

- **建议修复**:
  
  - 方案A：在 CorrectedStackParams 中添加 winsorized 标志和缩尾分位数参数，在 corrected_stacker.cpp 中实现 Winsorized 逻辑；
  - 方案B：扩展 hp_stack_gradient_corrected C API 签名添加 sigma_clip_method 参数，orchestrator 从 stage2_config.json 读取并传递；
  - 方案C：更新 PIPELINE_OVERVIEW.md 文档为"普通 sigma-clip"，与实际实现一致。

- **批复意见**:要求实现

- **复核记录**: 2026-07-16 源代码回溯验证：corrected_stacker.h 中 CorrectedStackParams 确认只有 sigma 和 max_iter，无 winsorized 相关字段。

### GAP-018：区域 SNR 拟合 vs IDW 评估（低，已源代码回溯复核 2026-07-16）

- **发现日期**: 2026-07-16

- **类型**: 术语差异

- **优先级**: 低

- **描述**:
  本割裂指"PIPELINE_OVERVIEW.md 描述的区域 SNR 拟合"在 snr_evaluator 实际代码中是 IDW 评估，非独立"拟合"步骤。
  
  **名词解释**：
  
  - **区域 SNR 拟合**：PIPELINE_OVERVIEW.md 步骤10 描述的概念，暗示对每个区域做某种参数化拟合（如多项式、样条）得到局部 SNR。
  - **IDW 评估**：Inverse Distance Weighting，反距离加权插值。snr_evaluator 实际使用的方法：从稀疏控制点用 KD-tree K 近邻搜索，按球面大圆距离的幂次反比作为权重，加权平均得到查询点 SNR。
  - **SnrEvaluator**：healpix_stack/gradient 模块中的 C++ 类，提供 build（构建 KD-tree）和 evaluate/evaluateBatch（单点/批量评估）方法。
  - **KD-tree**：k-dimensional tree，k 维空间分割数据结构，用于快速 K 近邻搜索。snr_evaluator 用 nanoflann 库实现。
  - **球面大圆距离 γ**：球面上两点间最短弧长，IDW 权重 w_i = 1/γ_i^idw_power。
  
  **核查到的实际情况**：
  
  1. snr_evaluator.h 头文件注释明确"SNR 稀疏控制点模型 IDW 评估器"；
  2. 算法步骤：①控制点 (ra,dec) 转 3D 笛卡尔坐标，用 nanoflann 建 KD-tree ②查询点 (ra,dec) K 近邻搜索（默认 K=16）③球面大圆距离 γ → IDW 权重 w_i = 1/γ_i^idw_power ④SNR(ra,dec) = snr_phot × (Σ w_i·snr_psf_i / Σ w_i) / median_snr；
  3. 这是纯插值/评估过程，没有参数化拟合（无最小二乘、无多项式系数求解）；
  4. PIPELINE_OVERVIEW.md 步骤10"区域 SNR 拟合"用词暗示拟合，但实际是 IDW 评估；
  5. 流程概述.txt 第 19 行原文也是"拟合各区域的局部 SNR"。
  
  **涉及模块**：snr_evaluator（实际 IDW 评估）、PIPELINE_OVERVIEW.md（文档用"拟合"）、流程概述.txt（原文用"拟合"）。

- **影响**:
  
  - 术语误导，新开发者可能误以为有独立拟合步骤；
  - 实际 IDW 评估是无参数插值，速度比参数化拟合快但精度略低。

- **建议修复**:
  
  - 更新 PIPELINE_OVERVIEW.md 步骤10 描述为"区域 SNR IDW 评估"或"区域 SNR 球面 IDW 重建"；
  - 流程概述.txt 也建议同步更新（用户原文）。

- **批复意见**:

- **复核记录**: 2026-07-16 源代码回溯验证：snr_evaluator.h 注释和算法步骤确认 IDW 评估非拟合。

### GAP-019：data_pipeline 数据总线未被 orchestrator 使用（低，已源代码回溯复核 2026-07-16）

- **发现日期**: 2026-07-16

- **类型**: 架构与实现不一致

- **优先级**: 低

- **描述**:
  本割裂指"PIPELINE_OVERVIEW.md 核心设计原则中描述的 data_pipeline 数据总线"在 orchestrator 实际加载的 DLL 列表中不存在，数据总线实际由 astro_image_io 提供。
  
  **名词解释**：
  
  - **data_pipeline**：2026-07-12 从 astro_image_io 拆分出的独立 GitHub 仓库（Astro-Data-Pipeline），提供 PipelineFrame + PipelineEngine + 命名块容器的 C API（aio_frame_*/aio_engine_*）。
  - **astro_image_io**：原始的 I/O + 数据总线模块，拆分后仍保留 aio_pipeline.cpp/aio_pipeline_engine.cpp 副本（与 data_pipeline 同名同 API）。
  - **ModuleId 枚举**：orchestrator 内部 DllLoader 的模块标识枚举，10 个值：AIO/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/GRADIENT_2D/SNR/DRIZZLE/GRADIENT_SPHERE/STACK。
  - **AIO 模块**：ModuleId::AIO 对应 astro_image_io.dll，提供 aio_frame_create/aio_frame_get_block/aio_frame_kv_get 等函数。
  - **PipelineEngine**：data_pipeline 中定义的阶段执行引擎，以 PipelineStageHandler 回调为执行单元。orchestrator 未使用，而是手动串行调用各 stage handler。
  
  **核查到的实际情况**：
  
  1. orchestrator.cpp 中 init_dlls 加载 10 个模块，列表为 AIO/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/GRADIENT_2D/SNR/DRIZZLE/GRADIENT_SPHERE/STACK，没有 DATA_PIPELINE；
  2. orchestrator 通过 ModuleId::AIO 加载 astro_image_io.dll，调用 aio_frame_* 系列函数操作 PipelineFrame；
  3. data_pipeline 独立仓库存在但无消费者（与 GAP-006 相关）；
  4. orchestrator 未使用 PipelineEngine，而是手动串行调用 run_stage_read_fits/run_stage_calibrate/.../run_stage_drizzle；
  5. astro_image_io 仍保留 aio_pipeline.cpp/aio_pipeline_engine.cpp 副本，与 data_pipeline 同名同 API，存在潜在符号冲突；
  6. PIPELINE_OVERVIEW.md 核心设计原则中"数据总线：data_pipeline（PipelineFrame + PipelineEngine）"与实际不符。
  
  **涉及模块**：orchestrator（加载 AIO 不加载 DATA_PIPELINE）、data_pipeline（独立仓库无消费者）、astro_image_io（保留副本）、PIPELINE_OVERVIEW.md（文档描述 data_pipeline）。

- **影响**:
  
  - data_pipeline 作为独立仓库存在但无消费者，是无效投入；
  - 与 GAP-006 相关（拆分未完成）；
  - 文档误导，新开发者可能误以为 orchestrator 用 data_pipeline。

- **建议修复**:
  
  - 与 GAP-006 一并解决：明确数据总线归属，要么 astro_image_io 移除副本依赖 data_pipeline，要么 data_pipeline 合并回 astro_image_io；
  - 更新 PIPELINE_OVERVIEW.md 核心设计原则中"数据总线"描述为实际归属模块（astro_image_io）；
  - 若保留 data_pipeline 独立仓库，orchestrator 应改为加载 data_pipeline.dll 而非 astro_image_io.dll（需要重命名 C API 避免符号冲突）。

- **批复意见**:

- **复核记录**: 2026-07-16 源代码回溯验证：orchestrator.cpp init_dlls 中 ModuleId 列表确认无 DATA_PIPELINE。

### GAP-020：基础校准退化路径（低，已源代码回溯复核 2026-07-16）

- **发现日期**: 2026-07-16

- **类型**: 退化路径

- **优先级**: 低

- **描述**:
  本割裂指"PIPELINE_OVERVIEW.md 描述的基础校准（Bias/Dark/Flat + 坏点修复）"在 orchestrator 实际调用 ac_calibrate_frame 时传 nullptr，走退化路径（out=light，校准未执行）。
  
  **名词解释**：
  
  - **ac_calibrate_frame**：calibration 模块导出的 C API，接受原始图像 + master_dark + master_flat + master_bias，输出校准后图像。
  - **master_dark/flat/bias**：主暗场/主平场/主偏置，由多帧 dark/flat/bias 图像中值叠加得到，是校准的参考帧。
  - **退化路径**：当 master_dark/flat/bias 传 nullptr 时，calibration 模块内部直接把 out=light（输出等于输入），跳过校准。
  - **calib_params_json**：orchestrator 配置中的校准参数 JSON，应含 master_dark/flat/bias 文件路径。
  
  **核查到的实际情况**：
  
  1. orchestrator.cpp run_stage_calibrate 中调用 ac_calibrate_frame 时，第 4/5/6 参数（master_dark/flat/bias）传 nullptr；
  2. 代码注释明确"TODO: master_dark/flat/bias 应从 config_json 加载, 当前传 nullptr 走退化路径 (out=light)"；
  3. calibration 模块本身实现了完整的 Bias/Dark/Flat 校准 + 坏点修复功能，但 orchestrator 没有把参考帧传进去；
  4. 实际管线中校准步骤被跳过，原始 FITS 图像直接进入下一阶段；
  5. PIPELINE_OVERVIEW.md 步骤1"基础校准"描述"Bias/Dark/Flat 校准 + 坏点修复"；
  6. 可能是测试配置（开发期间不校准加速迭代），也可能是配置加载逻辑未实现。
  
  **涉及模块**：orchestrator（传 nullptr）、calibration（功能实现但未被调用）、PIPELINE_OVERVIEW.md（文档描述完整校准）。

- **影响**:
  
  - 管线中校准步骤被跳过，原始图像直接进入后续阶段；
  - Bias/Dark/Flat 误差会影响测光定标精度（PHOTOMETRIC 阶段）；
  - 坏点修复未执行，bad pixel 可能被误识别为星点（PLATESOLVE 阶段）；
  - 若是测试配置则影响可控，若是生产环境则需修复。

- **建议修复**:
  
  - 确认是否为测试配置（检查 stage1_config.json 中是否有 calibration_dir 字段）；
  - 若是测试配置，在文档中标注"测试期间跳过校准"；
  - 若是生产环境，实现从 config_json 加载 master_dark/flat/bias 路径，读取并传给 ac_calibrate_frame；
  - 同时检查坏点修复是否也走退化路径。

- **批复意见**:

- **复核记录**: 2026-07-16 源代码回溯验证：orchestrator.cpp run_stage_calibrate 中 ac_calibrate_frame 调用确认第 4/5/6 参数为 nullptr，注释明确"走退化路径"。

---

## 3. spec 设计与实施完成度割裂

### GAP-007：.trae/specs/ 下多个 spec 实施状态未同步

- **发现日期**: 2026-07-16
- **类型**: spec 管理待办
- **描述**:
  `.trae/specs/` 下存在 10+ 个 spec 目录（affine-diagnostics、center-scale-search、gaia-multi-db、healpix-github-repo、initial-wcs-rewrite、io-migration、ipv-search-pruning、module-adaptation、pipeline-engine、psf-block-extension、star-count-sweep、vector-match-solver），部分 spec 有 tasks.md 但无完成标记，部分只有 spec.md。
  本次整理范围不含 .trae/specs/，但需记录此割裂。
- **影响**: 难以判断哪些设计已实施、哪些仍在设计中
- **建议修复**: 后续专项梳理 .trae/specs/，为每个 spec 标注实施状态（已实施/进行中/废弃/待启动）

### GAP-008：4 处断层待修复（来自 memory.md）

- **发现日期**: 2026-07-16（memory.md 记录）
- **类型**: 已知断层
- **描述**:
  memory.md 记录依赖链后续任务中含"4 处断层修复: drizzle落盘/hiss格式/Python绑定/stack加权"。
- **影响**: 端到端管线可能在这些节点断裂
- **建议修复**: 按依赖顺序逐个修复，每个断层独立 spec

---

## 4. 文档体系割裂

### GAP-009：根目录非 git 仓库但旧文档未说明

- **发现日期**: 2026-07-16
- **类型**: 文档遗漏
- **描述**:
  旧 PROJECT_ARCHITECTURE.md 未明确说明根目录非 git 仓库、各模块独立 git 管理。本次整理发现根目录无 .git，导致原 spec 中的"git 提交历史清洁"验证标准需调整。
- **影响**: 新开发者可能误以为根目录可 git 操作
- **建议修复**: 新 ARCHITECTURE.md §1 已补充说明（本次已修复）

### GAP-010：各模块 memory.md 与根 memory.md 职责重叠

- **发现日期**: 2026-07-16
- **类型**: 文档组织
- **描述**:
  根 memory.md 含全项目进度与模块索引，各模块 memory.md 含模块详细记录，但部分内容重复（如 PSF 块扩展、photometric-sigma-residual 等）。
- **影响**: 更新时需多处同步，易遗漏
- **建议修复**: 明确职责：根 memory.md 仅做索引与全局进度，模块详情只在模块 memory.md

---

## 5. 待修复断层清单（汇总）

| 编号       | 断层                                                                                 | 来源                   | 优先级     |
| -------- | ---------------------------------------------------------------------------------- | -------------------- | ------- |
| GAP-008a | drizzle 落盘                                                                         | memory.md            | 高       |
| GAP-008b | hiss 格式                                                                            | memory.md            | 高       |
| GAP-008c | Python 绑定                                                                          | memory.md            | 中       |
| GAP-008d | stack 加权                                                                           | memory.md            | 中       |
| GAP-001  | ~~healpix_browser_cpp 顶层重复~~ 已修复（2026-07-16 删除顶层，archive 副本保留）                     | 本次盘点                 | 已修复     |
| GAP-002  | ~~healpix_io 代码位置~~ 已修复（2026-07-16 依赖迁移至 astro_image_io）                           | 本次盘点                 | 已修复     |
| GAP-003  | ~~healpix_browser 未归档~~ 已修复（2026-07-16 归档至 archive/legacy/）                        | 本次盘点                 | 已修复     |
| GAP-004  | ~~healpix_lod 未归档~~ 已修复（2026-07-16 归档至 archive/legacy/）                            | 本次盘点                 | 已修复     |
| GAP-005  | ~~plate_solve .git 损坏~~ 已修复（git init + push --force，308f209）                       | 本次盘点                 | 已修复     |
| GAP-006  | data_pipeline 从 astro_image_io 拆分未完成（数据总线，非孤岛，2026-07-16 纠正误判）                     | 本次盘点                 | 高       |
| GAP-007  | spec 实施状态同步                                                                        | 本次盘点                 | 低（文档治理） |
| GAP-011  | SNR 接口链路断裂（orchestrator 调稠密 SNR snr_estimate，drizzle 期望稀疏控制点 snr_model 块，SNR²加权失效） | PIPELINE_OVERVIEW 对比 | 高       |
| GAP-012  | CCD QE 曲线未使用（pc_calibrate_simple_with_gaia C API 签名无 qe_curve 参数）                  | PIPELINE_OVERVIEW 对比 | 高       |
| GAP-013  | photometric_calib C API 是简化版（无 IRLS 迭代，曲面拟合在 GRADIENT_2D 阶段补完）                     | PIPELINE_OVERVIEW 对比 | 高       |
| GAP-014  | stage1 节点拆分与实际不符（文档缺 READ_FITS/GRADIENT_2D，SNR/Drizzle 各拆 2 步但实际各 1 节点）            | PIPELINE_OVERVIEW 对比 | 中       |
| GAP-015  | stage2 4 步合并为 1 函数 hp_stack_gradient_corrected + STACK 空骨架                         | PIPELINE_OVERVIEW 对比 | 中       |
| GAP-016  | NSIDE 自适应未实现（orchestrator 固定传 32768，未读 nside_strategy 配置）                          | PIPELINE_OVERVIEW 对比 | 中       |
| GAP-017  | Winsorized sigma clip 未实现（CorrectedStackParams 仅 sigma/max_iter，实际普通 sigma-clip）   | PIPELINE_OVERVIEW 对比 | 中       |
| GAP-018  | 区域 SNR 拟合 vs IDW 评估（snr_evaluator 实际用 KD-tree IDW 评估而非拟合）                          | PIPELINE_OVERVIEW 对比 | 低       |
| GAP-019  | data_pipeline 数据总线未被 orchestrator 使用（实际加载 astro_image_io.dll 提供数据总线）               | PIPELINE_OVERVIEW 对比 | 低       |
| GAP-020  | 基础校准退化路径（ac_calibrate_frame 第 4/5/6 参数 master_dark/flat/bias 传 nullptr）            | PIPELINE_OVERVIEW 对比 | 低       |

---

## 更新记录

- 2026-07-16: 初始创建，记录 GAP-001 ~ GAP-010 共 10 项割裂点
- 2026-07-16: 关闭 GAP-001（删除 healpix_browser_cpp 顶层重复）、GAP-002（healpix_browser_qt 依赖迁移至 astro_image_io）、GAP-003（healpix_browser 归档至 archive/legacy/）、GAP-004（healpix_lod 归档至 archive/legacy/）。spec: docs/superpowers/specs/2026-07-16-healpix-db-legacy-archive.md
- 2026-07-16: 纠正 GAP-006（data_pipeline 是数据总线非孤岛）+ 纠正 ARCHITECTURE.md data_pipeline 描述
- 2026-07-16: 新增 GAP-011 ~ GAP-020 共 10 项管线流程割裂点（基于 PIPELINE_OVERVIEW.md vs 实际代码对比）。3 项高优先级：SNR 链路断裂/CCD QE 未用/C API 简化版
- 2026-07-16: GAP-011 ~ GAP-020 详情重写——对每项做源代码回溯验证（核查 orchestrator.cpp/snr_estimator.h/photometric_calib.h/hp_drizzle_api.cpp/hp_stack_api.h/corrected_stacker.h/snr_evaluator.h 等关键文件），补充名词解释（中文描述变量名/API 名/调用链），添加"批复意见"留空字段供用户填写，添加"复核记录"标注源代码验证结论。汇总表 GAP-011~020 描述同步更新。
- 2026-07-18: 新增 GAP-021（GRADIENT_2D 节点归档）——用户审阅 PROJECT_OVERVIEW.md 后纠正：stage1 不做曲面拟合和图像亮度修正（那是 stage2 马赛克阶段的事），PSF 后只做测光坐标系校准（PHOTOMETRIC 已完成）。归档 gradient_2d 模块代码到 archive/，stage1 重排为 7 节点。spec: docs/superpowers/specs/2026-07-18-gradient-2d-archive.md

---

## 6. stage1 节点归档（2026-07-18 发现）

### GAP-021：GRADIENT_2D 节点归档（中，2026-07-18 用户纠正）

- **发现日期**: 2026-07-18

- **类型**: 设计纠正

- **优先级**: 中

- **描述**:
  本割裂指"PROJECT_OVERVIEW.md 第3节描述的 stage1 第5节点 GRADIENT_2D 做乘性梯度曲面拟合 + 图像亮度修正"与用户期望的 stage1 职责不符。
  
  **名词解释**：
  
  - **GRADIENT_2D 节点**：原 stage1 第5节点（stage=5），调用 gradient_2d.dll 的 gradient_2d_calibrate 函数，做 2D 多项式曲面拟合（IRLS+Tukey+Ridge+LOOCV）+ 图像亮度修正，输入 r=log10(F_instr/F_syn)，输出乘性梯度曲面 M(x,y) 并应用到图像 I_cal = I/M。
  - **PHOTOMETRIC 节点**：stage1 第4节点（stage=4），调用 photometric_calib.dll，做 PSF 流量 vs Gaia 积分流量 F_syn=∫S(λ)·T(λ)·Q(λ)dλ 的 IRLS+Tukey 稳健回归求全局 scale，并把 scale 应用到图像 I_cal = I×scale。
  - **测光坐标系校准**：把图像流量值通过线性变换转换为标准测光坐标系的流量值，本质就是 PHOTOMETRIC 节点做的"求 scale + 应用 scale"。
  - **马赛克阶段**：stage2 多帧合并阶段，做球面 TPS 梯度拟合 + 校正叠加，是曲面拟合和图像亮度修正的合适位置。
  
  **用户纠正**：
  
  1. stage1 在 PSF 后只做"测光坐标系校准"——这已经是 PHOTOMETRIC 节点做的事（PSF 流量 vs Gaia 积分流量的线性拟合 + 应用 scale 到图像）；
  2. 曲面拟合和图像亮度修正是 stage2 马赛克阶段做的事，不应在 stage1 单帧预处理中做；
  3. SNR 计算使用乘法模型结合测光不确定度（sigma_residual）和 PSF SNR（snr_psf）——SNR 模型已符合期望，无需修改；
  4. 结论：归档 GRADIENT_2D 节点，stage1 重排为 7 节点（READ_FITS/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE）。
  
  **涉及模块**：orchestrator（删除 PipelineStageV2::GRADIENT_2D 枚举 + run_stage_gradient_2d 函数）、photometric_calib（gradient_2d 目录归档到 archive/）、PROJECT_OVERVIEW.md/PIPELINE_OVERVIEW.md（文档同步）。

- **影响**:
  
  - 正面：stage1 职责更清晰，避免在单帧预处理中做过度的曲面拟合（稀疏采样下多项式易过拟合）；
  - 正面：与用户期望的"PSF 后只做测光坐标系校准，曲面修正留到马赛克阶段"对齐；
  - 中性：gradient_2d 模块代码归档到 archive/ 保留，stage2 设计时可参考；
  - 风险：stage 序号重排（SNR 6→5, DRIZZLE 7→6, GRADIENT_SPHERE 8→7, STACK 9→8）可能影响已有检查点文件，但检查点按 frame_name 存储，未硬编码 stage 数字。

- **修复方式**:
  
  - 代码归档：lib/photometric_calib/cpp/gradient_2d/ → lib/photometric_calib/archive/gradient_2d/
  - orchestrator 代码：删除 PipelineStageV2::GRADIENT_2D 枚举 + ModuleId::GRADIENT_2D 枚举 + run_stage_gradient_2d 函数 + dll_loader 中所有 GRADIENT_2D case + Makefile 中 -I 路径 + stage1_config.json modules 列表项；stage 序号重排
  - 文档：PROJECT_OVERVIEW.md 改为 9 节点表格；PIPELINE_OVERVIEW.md 同步；本 GAP 记录归档决策
  - 编译验证：orchestrator.exe 编译通过

- **批复意见**: 用户审阅 PROJECT_OVERVIEW.md 后明确纠正："我们不拟合曲面，不修正图像亮度，那个是在后面马赛可阶段做的事，在解析后，dynamicpsf后，只做坐标系校准，通过图像的psf流量和积分流量的拟合，使图像转换为标准的测光坐标系，然后计算SNR 使用乘法模型结合测光不确定度和psf snr"。

- **复核记录**: 2026-07-18 执行归档：代码移动到 archive/，orchestrator 枚举删除+stage 重排，文档同步更新。
