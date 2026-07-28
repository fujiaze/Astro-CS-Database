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
