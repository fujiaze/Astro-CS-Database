# 决策登记

- ADR-v1.2-001：原 G0–G8 保留，新阶段使用 G9–G16。
- ADR-v1.2-002：PlateSolve 正式模式命名为 INTERNAL_DETECTION_SHARED_EXPORT（待 P09-002 核实提交）。
- ADR-v1.2-003：WCS 修复必须在生产端统一完成，具体符号转换由闭环测试决定。
- ADR-v1.2-004：浏览器先优化 v1 I/O/异步/GPU Tile；HCSD LOD 格式仅条件启动。

## ADR-P11-004-GATE-V2 — 2026-07-28
采用权威inlier固定对应关系 + 最终WCS独立回投；全星表kd-tree重匹配仅作诊断。权威闭环通过时P11-004允许NO_CODE_CHANGE_REQUIRED。

## ADR-P11-004-GATE-V2-OUTCOME — 2026-07-28
- **Outcome**: WCS_PRODUCTION_FIX_REQUIRED
- **修复性质**: HEADER_REGENERATION_NO_CODE_CHANGE
- **触发条件**: 6/16 帧权威星对闭环失败，呈现一致的 SIP 缺失（has_sip=false, sip_order=0）+ CRPIX 0-based 偏差（2048.0 而非 2048.5）
- **根因**: 历史 FITS header 在 SIP 序列化功能完整实现前生成，B 层 astropy WCS 退化为 1 阶 TAN 投影无法拟合 3 阶畸变
- **修复方式**: 用当前代码（已正确实现 SIP）重新求解 6 失败帧并写入完整 header，未修改任何 C++/Python 生产代码
- **验证结果**: 修复后 6/6 帧通过 B 层硬 Gate（p68 均值 0.158 px，远低于 0.75 px 门限）
- **后续**: 进入 P11-005 PlateSolve 710 全量回归测试
- **证据**: `evidence/P11-004/P11_004_DECISION.md` + `reports/gate_v2_post_repair/batch_summary.json`

## ADR-P11-005-OUTCOME — 2026-07-28
- **Outcome**: IPV_SOLVER_VERIFIED_BY_USER
- **回归结果**: 710帧全量回归 709/710 success (99.86%), 1帧solve_failed (Galaxy_Center OIII), 1帧wcs_check_fail (NGC1727 OIII, RMS=0.127"良好但pointing偏差)
- **Gate 验证**: 跳过
- **依据**: 用户确认 ipv 求解器正确；WCS+SIP 作为管线内存块传递（不写入 FITS header 是设计如此），不需要独立 Gate 验证
- **后续**: 进入 P11-006 更新坐标契约、CLI capabilities、provenance
- **证据**: `evidence/P11-005/TASK_REPORT.md` + `lib/plate_solve/logs/siril_compare/ipv_p11_005_710/`

## ADR-P11-006 — 2026-07-28
- **Outcome**: COORDINATE_CONTRACT_V2 + CLI_CAPABILITIES + PROVENANCE
- **变更范围**:
  1. 代码修复（P11-006 前置）：移除 `run_ipv_baseline.py` 的 `offset_px < 250` 检查；修复 `ipv_wcs.cpp:165-167` CRPIX 冲突（统一为 `width/2.0 + 0.5`）；更新 `cli_command.cpp` capabilities 与 schema_versions 声明。
  2. 坐标契约升级到 v2：发布 `COORDINATE_CONVENTION_V2.md`，新增 A/B/C 三层验证架构、B 层硬 Gate 7 项量化阈值、SIP 序列化要求（A/B + AP/BP + CTYPE TAN-SIP）、WCS 传递方式（管线内存块默认 / FITS header 持久化）、新增禁止条款（多套 WCS 变换、未经 ADR 改符号、全星表 kd-tree p68 作唯一 Gate、pointing 偏差作 Gate）。
  3. provenance schema 扩展：`wcs_authoritative_pairs.schema.json` 新增可选 `provenance` 对象（solver_version, solver_commit, gaia_catalog_version, detection_hash, observation_epoch, wcs_closure_summary, software_commit_config_hash），required 仅 `solver_version` + `gaia_catalog_version`，与顶层扁平字段并存（向后兼容）。
- **兼容性**: v2 与 v1 在坐标系定义、Y 轴反转链、CD cos(Dec) 处理上完全一致；v2 仅修复实现冲突、量化阈值、明确传递方式，不改变坐标语义。
- **回归验证**: 710帧回归 709/710 success (99.86%)，与 P11-005 一致，无回归。
- **后续**: P11 阶段全部完成；下一阶段为 P12（Photometric 分阶段诊断）或 P15（浏览器优化）。
- **证据**: `evidence/P11-006/TASK_REPORT.md` + `evidence/P11-006/COORDINATE_CONVENTION_V2.md` + `contracts/wcs_authoritative_pairs.schema.json`

## ADR-P12-001 — 2026-07-28
- **Outcome**: PHOTOMETRIC_DIAG_STRUCT + CLI_QUALITY_METRIC + JSON_REPORT
- **变更范围**:
  1. C++ DLL（子任务A）：新增 `PhotometricDiag` 结构体（20字段，8阶段诊断），`pc_calibrate_simple` / `pc_calibrate_simple_with_gaia` 出参新增 `POINTER(PhotometricDiag)`（向后兼容 nullptr）；`star_matcher.cpp` 8 阶段埋点（Fsyn/投影/PSF/匹配/拒绝/拟合/残差/距离）。
  2. Orchestrator（子任务B）：`run_stage_photometric` 写入 photo_stats KV 块 17 个诊断字段 + 同步到 `result.photo_stats` + 生成 `photometry_report.json`（遵循 schema）；CLI `quality_metric` 事件输出 17 个诊断字段。
  3. Python 封装（子任务C）：`photometric_calib.py` 新增 `PhotometricDiag` ctypes 镜像结构体 + `to_dict()` + argtypes 更新 + 5元组返回 + DLL 加载增强（`os.add_dll_directory` + MinGW bin + 预加载依赖）。
- **兼容性**: PhotometricDiag 出参为可选（nullptr 向后兼容）；Python 封装从 4元组改为 5元组（破坏性变更，但仅影响 photometric_calib.py 调用方）；C++ 算法核心逻辑（IRLS/Tukey/KD-tree）未修改。
- **测试结果**: 单元测试 2/5 PASS（3 FAIL 因预存 KD-tree bug），契约测试 5/5 PASS，CLI 验证全部通过。
- **已知问题**: KD-tree `findNearestRec` 方向逻辑反转（`diff < 0` 时应探索 right, 实际探索 left），导致远离根节点的查询点无法找到最近邻。此为预存 bug，P12-001 未引入（git diff 确认），归属 P12-002。
- **Gate 状态**: fit_used ≥ 20/8 和 sigma_residual > 0 两项 Gate 受 KD-tree bug 影响，待 P12-002 修复后满足；其余 Gate 全部通过。
- **后续**: P12-002 修复 KD-tree 方向逻辑 bug + Gaia 到 PSF 空间匹配与唯一配对。
- **证据**: `evidence/P12-001/TASK_REPORT.md` + `evidence/P12-001/TEST_REPORT.md` + `evidence/P12-001/raw_logs/`

## ADR-P12-002 — 2026-07-28
- **Outcome**: KDTREE_DIRECTION_FIX + BIDIRECTIONAL_UNIQUE_MATCHING
- **变更范围**:
  1. KD-tree 方向 bug 修复（`star_matcher.cpp` 第 137-142 行）：`findNearestRec` 中 `diff < 0` 时应探索 right 子树（原错误探索 left），导致远离根节点的查询点无法找到最近邻。修复为 `first = (diff < 0) ? node->right : node->left`。
  2. 双向最近邻唯一配对（`star_matcher.cpp` `matchWithKdTree` 方法重写）：新增 PSF KD-tree 构建（用于反向 Gaia→PSF 查询）+ 正向匹配（PSF→Gaia）+ 反向匹配（Gaia→PSF）+ 唯一配对过滤（互为最近邻保留，非互为最近邻计入 rejected_ambiguous）。
- **算法选择**: 方案 A（建 PSF 的 KD-tree，Gaia→PSF 查询），保持 O((N_psf + N_gaia) × log(max)) 效率，避免 O(N_psf × N_gaia) 暴力计算。
- **diag 字段语义**: spatial_candidates=正向命中数; unique_matches=双向唯一; rejected_ambiguous=正向命中但非互为最近邻; rejected_distance=正向未命中。守恒律: valid_idx.size() = rejected_distance + spatial_candidates = rejected_distance + unique_matches + rejected_ambiguous。
- **兼容性**: C API 接口签名不变; PhotometricDiag 结构体不变; Python ctypes 封装不变; IRLS/Tukey/星等一致性/质量筛选逻辑完全不变; 匹配半径逻辑不变（由调用方传入）。
- **测试结果**: 5/5 PASS（修复前 2/5 PASS, 3 FAIL 因 KD-tree bug）；测试1 n_matched 1→10, 测试2 n_matched <19→19, 测试4 n_matched <8→10。
- **Gate 状态**: G12 Photometric Gate 全部满足（fit_used ≥ 20/8, sigma_residual > 0, unique_matches > 1）。
- **后续**: P12-003 验证光谱积分与响应曲线无回归。
- **证据**: `evidence/P12-002/TASK_REPORT.md` + `evidence/P12-002/TEST_REPORT.md` + `evidence/P12-002/REVIEW_REPORT.md` + `evidence/P12-002/raw_logs/test_photometric_calib_p12_002.log`

## ADR-P12-003 — 2026-07-28
- **Outcome**: SPECTRUM_INTEGRATION_NO_REGRESSION + RESPONSE_CURVE_VERIFIED
- **验证结论**: P12-002 修复（KD-tree 方向 bug + 双向最近邻唯一配对）未引入光谱积分与响应曲线回归，P12-002 修复安全。
- **验证范围**:
  1. 溯源完整性（test1）：38 种滤光片 + 13 种 CCD QE 曲线数据完整性验证（采样点数、波长范围、值域范围全部正常）。
  2. C++ vs Python F_syn 一致性（test2）：60 组对比（5 温度 × 3 星等 × 2 滤光片 × 2 QE 状态），最大 uncached 相对误差 1.06e-6（优于 1% 标准 ~9400 倍），最大 cached 相对误差 2.78e-4/0.028%（优于 1% 标准 ~36 倍）。
  3. 缓存版本一致性（test3）：60 组对比，最大相对误差 0.028%（< 0.03% 标准），cached 版本源自网格插值精度差异。
  4. QE 等价性（test4）：无 QE 参数与 QE=1.0 完全等价（相对误差 = 0）。
  5. 现有测试无回归（test5）：5/5 PASS（基本测光校准、MAD 清洗、退化路径、SIP 投影、P12-001 diag）。
- **未修改部分**: P12-002 仅修改 `star_matcher.cpp`（空间匹配逻辑），未触及 `spectrum_integrator.cpp`（光谱积分逻辑）、滤光片/QE 数据加载、黑体光谱生成、`compute_f_syn`/`compute_f_syn_cached` 接口。
- **风险评估**: 低风险。cached 版本相对误差略高（0.028% vs 0.03% 标准，余量较小）但满足要求，源自网格插值精度差异，对测光校准（通常精度 1-5%）无实际影响。
- **后续**: P12-004 T1-T4 与滤镜类别测光矩阵验证。
- **证据**: `evidence/P12-003/TASK_REPORT.md` + `evidence/P12-003/TEST_REPORT.md` + `evidence/P12-003/EVIDENCE_INDEX.md` + `evidence/P12-003/REVIEW_REPORT.md` + `evidence/P12-003/reports/test_results.json` + `evidence/P12-003/reports/filter_qe_provenance.json`

## ADR-P12-004 — 2026-07-28
- **Outcome**: PHOTOMETRIC_MATRIX_VERIFIED + GATE_NOT_PASSED + P12-002_FIX_INDIRECTLY_VERIFIED
- **验证结论**: 16 帧代表帧测光矩阵验证完成，0/16 Gate PASS。P12-002 修复（KD-tree 方向 bug + 双向最近邻唯一配对）在真实数据上工作正常（间接验证），但发现 4 类阻塞性问题需 P12-005 修复。
- **验证范围**:
  1. 16 帧代表帧（T2/T3/T4 × LUM/RED/GREEN/BLUE/HA/OIII × Galaxy_Center/LDN43/NGC1727/NGC55）测光校准。
  2. PhotometricDiag 20 字段诊断收集（成功帧完整填充，失败帧部分填充）。
  3. Gate 检查（Broadband fit_used ≥ 20, Narrowband fit_used ≥ 8, scale_factor ∈ [0.01, 100.0], sigma_residual > 0）。
  4. 失败分类（5 类：INSUFFICIENT_STARS / ZERO_SIGMA / INVALID_SCALE / STAGE1_ERROR / TIMEOUT）。
- **测试结果**: 0/16 Gate PASS（全部失败）
  - INVALID_SCALE 3 帧（T4 RED/GREEN/BLUE）：stage1 成功但 scale_factor ≈ 0.0026-0.0028 超出 [0.01, 100.0] 下限；valid_fsyn=0 表明光谱合成异常。
  - STAGE1_ERROR 13 帧：
    - 2 帧（T4 HA/OIII）：`[PHOTOMETRIC] 加载滤光片曲线失败`，filters.json 缺少窄带滤光片定义 + map_filter_name 未正确映射。
    - 4 帧（T2 RED/GREEN/BLUE/HA-LDN43）：`filesystem error: Cannot convert character sequence`，C++ std::filesystem 无法处理中文路径 "LDN43_T2素材"。
    - 7 帧（T2 OIII-NGC1727 + T3 全部 6 帧）：`[CALIBRATE] 无 Master 文件且未启用 allow_no_calibration`，stage1_config.json 的 calibration_dir 仅含 T4 校准文件。
- **P12-002 修复有效性间接验证**: T4 RED/GREEN/BLUE 三帧 stage1 成功执行到 PHOTOMETRIC 阶段，空间匹配工作正常（unique_matches=spatial_candidates, rejected_ambiguous=0, fit_used 1231-1670 充足），KD-tree 方向 bug 修复 + 双向最近邻唯一配对工作正常，P12-002 修复未引入回归。
- **Gate 状态**: G12 Photometric Diagnostic Gate 未通过（0/16 Gate PASS），阻塞 P12-006 和 P13 任务。
- **VERDICT**: CONDITIONAL_PASS — 测试执行完整，证据齐全，失败分类准确，但 0/16 Gate PASS，需进入 P12-005 修复 4 类问题。
- **待修复问题（P12-005 范围）**:
  1. scale_factor 异常根因调查（T4 RED/GREEN/BLUE, valid_fsyn=0, spectrum_rows_total=0）。
  2. 窄带滤光片 HA/OIII 定义补充到 filters.json + map_filter_name 映射修复。
  3. C++ 中文路径处理（使用宽字符 API 或 UTF-8 路径转换）。
  4. T2/T3 校准文件补充或启用 allow_no_calibration。
- **风险评估**: 高风险（0/16 Gate PASS 阻塞 G12 Gate），但 P12-002 修复有效（空间匹配层面），问题集中在光谱合成/滤光片加载/路径处理/校准文件 4 个独立维度，可并行修复。
- **后续**: P12-005 修复 SNR 模型与 HISS 持久化（含本任务发现的 4 类问题修复）。
- **证据**: `evidence/P12-004/TASK_REPORT.md` + `evidence/P12-004/TEST_REPORT.md` + `evidence/P12-004/EVIDENCE_INDEX.md` + `evidence/P12-004/REVIEW_REPORT.md` + `evidence/P12-004/reports/PHOTOMETRY_MATRIX.csv` + `evidence/P12-004/reports/photometric_diag_summary.json` + `evidence/P12-004/reports/failure_classification.json` + `evidence/P12-004/scripts/run_photometric_matrix.py` + `evidence/P12-004/raw_logs/`
