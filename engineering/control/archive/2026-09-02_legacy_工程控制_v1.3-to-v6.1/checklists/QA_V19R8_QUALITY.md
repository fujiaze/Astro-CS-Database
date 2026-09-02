# QA-V19R8 Checklist — Wiki→代码 六阶段质量闭环

> 对应 Spec: `工程控制/docs/30_WIKI_TO_CODE_QUALITY_V19R8_SPEC.md` | 约 100 commits (S0 4+S1 10+S2 12+S3 12+S4 10+S5 28+S6 14+10缓冲) | vm-bj 持续运行
> 阶段: S0 Wiki与权威链加固 → S1 科学文档精确化 → S2 算法核心文档精确化 → S3 项目/工程/代码开发文档整理 → S4 精细化代码审查(Subagent) → S5 硬性问题修改·测试·Review闭环 → S6 接口与命名质量优化

## 使用说明
- 每行一项，`[ ]` 待办 / `[x]` 完成 / `[~]` 部分 / `[N/A]` 不适用
- 带 `M` 为机器校验（脚本/CI），`H` 为人工审查，`E` 需证据落盘
- 每项按 `P0(阻断)/P1(必须)/P2(建议)` 标注；任一 P0 FAIL 阻断下一阶段；全部 P0+P1 PASS 方可签 `VERDICT: PASS`
- 跨层串行 S0→S6，同层内并行 ≤4 Resident；每 10 commits 一 checkpoint

---

### 阶段 S0 — Wiki与权威链加固 (4 commits, H+M+E)

#### S0 基线与Wiki显式化
- [ ] S0-01 Wiki 核心约束显式化：Wiki 索引与 `docs/README-DOCS.md` L0-L5 体系一一对应，Wiki→L1→L2 引用无断链，矛盾以 Wiki 为准 (H+E) [P0]
- [ ] S0-02 `docs/README-DOCS.md` L0-L5 定义与 `docs/validation/SCIENCE_FREEZE.md` 冻结红线一致，权威链 `Wiki→Science→Algorithm→Architecture→Standards→Modules→Source→Test→Diagnostics→Release` 可机检 (H+M+E) [P0]
- [ ] S0-03 `docs/TRACEABILITY.csv` 64→~75 行初补齐（SCI-/ALG-/DATA-/ENG- 行与 Wiki/L1/L2/L3 合同 ID 对齐，空字段/重复ID/孤儿合同清零） (M+E) [P0]
- [ ] S0-04 机器一致性初扫：运行 `tools/docs_machine_consistency.py` 9 checks 生成 `reports/v19r8_quality/machine_consistency_s0.json` + `machine_consistency_s0.log`，记录 broken/缺失符号/孤儿合同，`audit_stats.json` 初版 (M+E) [P0]
- [ ] S0 Gate: Wiki 索引与 README-DOCS 一致；machine 9/9 0 broken（或 P0 broken 清零计划已冻结）；TRACEABILITY 初补齐可追溯 (H+M) [P0]

---

### 阶段 S1 — 科学文档精确化 (10 commits, H+M+E)

> 11份 L1 逐份按模板：定义/公式/变量/单位/假设/有效域/失效域/系统与随机误差/数值精度/参考文献/ID + 源码符号/文件行锚点一一对应

- [ ] S1-01 `docs/science/SCIENCE_SCOPE.md` 范围/假设/失效域与代码实际支持域一致，变量/单位表完整 (H+M) [P0]
- [ ] S1-02 `docs/science/CALIBRATION.md` 公式/单位/误差与 `lib/calibration/src/calibrator.cpp` 对齐，master 生成/坏点语义一致 (H+M) [P0]
- [ ] S1-03 `docs/science/ASTROMETRY.md` + `docs/science/PSF.md` 与 `lib/plate_solve/cpp/ipv + lib/dynamic_psf + lib/star_detector` 对齐，WCS/SIP/PSF 质量定义一致 (H+M) [P0]
- [ ] S1-04 `docs/science/PHOTOMETRY.md` 与 `lib/photometric_calib`（flux 校准/响应曲线/滤光片）对齐 (H+M) [P0]
- [ ] S1-05 `docs/science/NOISE_MODEL.md` 三层模型/符号/单位与 `lib/snr_estimator` 对齐，SNR-001..015 引用完整 (H+M) [P0]
- [ ] S1-06 `docs/science/UNCERTAINTY_AND_COVARIANCE.md` 协方差未建模声明与 `lib/healpix_db/healpix_drizzle` 量化一致，误差传播假设显式 (H+M) [P1]
- [ ] S1-07 `docs/science/DRIZZLE.md` 方差传播 α²v / k_corr=1.4 / 几何缓存/Run generation 与 `healpix_drizzle` 一致，SCI-DRZ-* 行锚点齐 (H+M) [P0]
- [ ] S1-08 `docs/science/PHASE2_UPM.md` UPM 权重 `quality×geom×ivar` / control_ivar `k_corr×(π/2)×σ_bg²/N_retained` / frame_id 绑定与 `lib/phase2/src/upm.cpp+sampler.cpp` 一致 (H+M) [P0]
- [ ] S1-09 `docs/science/INTEGRATION.md` ivar 默认 / zero-weight 合同 / support reducer `max(accepted support)` 与 `lib/phase2/src/integrate.cpp` 一致 (H+M) [P0]
- [ ] S1-10 `docs/science/REJECTION.md` 排异语义/归一化 `astrocs_median_center_v1` / large-scale / eligibility 分层与 `lib/phase2/src/rejection*` 一致 (H+M) [P0]
- [ ] S1 Gate: 11份 science 文档 machine 9/9 0 broken，TRACEABILITY SCI-* 全 VERIFIED，公式-符号-行锚点抽查一致 (M) [P0]

---

### 阶段 S2 — 算法核心文档精确化 (12 commits, H+M+E)

> 12份 L2 逐份补：输入/输出/前置/后置/不变量/伪代码/复杂度/并行模型/数值风险/fast/reference/oracle/ID + 源码入口一一对应，删 legacy 残留

- [ ] S2-01 `docs/algorithms/CALIBRATION_ALGORITHMS.md` 输入/输出/不变量/伪代码/复杂度/oracle 与 `lib/calibration/src/*` 一致 (H+M) [P0]
- [ ] S2-02 `docs/algorithms/PLATESOLVE.md` 同上，与 `lib/plate_solve/cpp/ipv/src/*` 一致，WCS/SIP 序列化契约对齐 (H+M) [P0]
- [ ] S2-03 `docs/algorithms/STAR_PSF_ALGORITHMS.md` 同上，与 `lib/dynamic_psf + lib/star_detector` 一致 (H+M) [P0]
- [ ] S2-04 `docs/algorithms/PHOTOMETRIC_FIT.md` 同上，与 `lib/photometric_calib/cpp/src/*` 一致 (H+M) [P1]
- [ ] S2-05 `docs/algorithms/NOISE_ESTIMATION.md` 同上，与 `lib/snr_estimator/cpp/src/*` 一致，`snr_noise_model_v1` 语义对齐 (H+M) [P0]
- [ ] S2-06 `docs/algorithms/GAIA_QUERY.md` 同上，与 `lib/gaia_xpsd_client/src/*` 一致，RA 环绕/polar prune 契约齐 (H+M) [P1]
- [ ] S2-07 `docs/algorithms/DRIZZLE_GEOMETRY.md` 同上，与 `lib/healpix_db/healpix_drizzle/*.cpp` 一致，overlap/geometry cache 语义齐 (H+M) [P0]
- [ ] S2-08 `docs/algorithms/HEALPIX_MAPPING.md` 同上，与 `lib/common/healpix*` 一致，NESTED 唯一映射对齐 (H+M) [P0]
- [ ] S2-09 `docs/algorithms/PHASE2_SAMPLER.md` 同上，与 `lib/phase2/src/sampler.cpp` 一致，control estimator 契约齐 (H+M) [P0]
- [ ] S2-10 `docs/algorithms/UPM_SOLVER.md` 同上，与 `lib/phase2/src/upm.cpp` 一致，`frame_id_by_index` 绑定与持久化契约齐 (H+M) [P0]
- [ ] S2-11 `docs/algorithms/REJECTION_ALGORITHMS.md` 同上，与 `lib/phase2/src/rejection*` 一致，WBPP Auto/winsorized/linear_fit 语义齐 (H+M) [P0]
- [ ] S2-12 `docs/algorithms/INTEGRATION_ALGORITHMS.md` 同上，与 `lib/phase2/src/integrate.cpp` 一致 + science↔algorithm 符号/公式逐条对齐无 legacy (H+M) [P0]
- [ ] S2 Gate: 12份 algorithm 文档全量 machine 9/9 0 broken，伪代码-源码入口抽查一致 (M) [P0]

---

### 阶段 S3 — 项目/工程/代码开发文档整理 (12 commits, H+M+E)

#### S3-架构与契约 (4c)
- [ ] S3-01 `docs/architecture/ARCHITECTURE.md` 概览与 `MODULE_MAP/DATA_FLOW/PIPELINE` 一致，13 模块总览与 lib/* 实际一致 (H+M) [P0]
- [ ] S3-02 `docs/architecture/MODULE_MAP.md` + `DEPENDENCY_RULES.md` 13 模块职责/接口/依赖无环，与 `lib/*` include 实测一致 (H+M) [P0]
- [ ] S3-03 `docs/architecture/DATA_FLOW.md` + `PIPELINE.md` + `OWNERSHIP_AND_LIFETIME.md` + `THREADING_MODEL.md` 数据流/所有权/线程与 `lib/orchestrator/phase2/healpix_drizzle` 实际一致 (H+M) [P0]
- [ ] S3-04 `docs/contracts/DATA_SEMANTICS.md` + `PUBLIC_API.md` + `ERROR_MODEL.md` + `IO_AND_ATOMICITY.md` + `CACHE_POLICY.md` + `COMPATIBILITY_POLICY.md` + `PERFORMANCE_MODEL.md` 与 `lib/*` 头文件/错误码/原子写/缓存/版本一致 (H+M) [P0]

#### S3-标准与模块 (4c)
- [ ] S3-05 `docs/standards/*` 13项（CODE/COMMENT/NUMERIC/API/C_ABI/CONCURRENCY/ERROR/IO/LOGGING/TEST/BENCHMARK/DOCUMENTATION/RELEASE）与 `lib/*` 实测违规清单对齐，无过期条款 (H+M) [P1]
- [ ] S3-06 `docs/modules/*.md` 13份按固定模板（职责/接口/数据含义/线程/所有权/错误/测试）与 `lib/*` 实际一致，Public API 头文件/extern C guards 齐 (H+M) [P0]
- [ ] S3-07 代码开发文档：Public API（头文件/extern C/C ABI guards）+ 函数签名（输入/输出/所有权/error_msg/error_capacity/线程）+ 数据契约（FITS/HiPS/UPM/frame_id）+ 错误码（ERROR_MODEL ↔ orchestrator.h 全量）清单齐全 (H+M) [P0]
- [ ] S3-08 所有权/生命周期/线程模型/IO 原子性/性能/兼容 文档与 `lib/phase2/astro_image_io/healpix_drizzle/orchestrator` 实现一致，frame_id 绑定与原子写契约显式 (H+M) [P0]

#### S3-项目与工程控制 (4c)
- [ ] S3-09 `工程控制/control/PROJECT_STATE.yaml` 迁移到 V19R8 对齐 HEAD + `CURRENT_TASK.md` 同步到当前阶段 (H+M) [P0]
- [ ] S3-10 `工程控制/control/DECISION_REGISTER.md` 追加 QA 决策 + `RISK_REGISTER.csv` 更新风险关闭/新增 (H) [P1]
- [ ] S3-11 `工程控制/control/MASTER_TASK_REGISTER.csv` 追加 QA-V19R8 段（S0-S6 90+10 行）+ `docs/TRACEABILITY.csv` 补齐 V19R7 增量 ~12-16 行 + 全量 machine 9/9 0 broken (M) [P0]
- [ ] S3-12 `docs/RELEASE_STATUS.md` + `docs/KNOWN_LIMITATIONS.md` + `CHANGELOG.md` + `docs/DEVELOPER_GUIDE.md` + `docs/README-DOCS.md` 同步到 V19R8（`PRE_RELEASE_ENGINEERING_FOUNDATION=V19R8` / `FINAL_REAL_DATA_VALIDATION=PENDING`） (H) [P0]
- [ ] S3 Gate: L3 12+2 + L4 13 + L5 13 + 项目/工程 0 broken；PUBLIC_API machine 一致；PROJECT_STATE 与 TRACEABILITY 双校验 0 broken (M) [P0]

---

### 阶段 S4 — 精细化代码审查 Subagent精查 (10 commits, 只读不改, H+M+E)

> Subagent 分域并发 ≤4；按域分片：calibration/plate_solve/gaia/dynamic_psf/star/photometric/snr/drizzle/orchestrator/phase2/acr/astro_image_io/common/standards；分类：正确性/数值安全/内存生命周期/错误处理/并发安全/性能/命名/注释/C ABI/API hygiene；分级 P0/P1/P2

- [ ] S4-01 机器一致性复扫：`tools/docs_machine_consistency.py` 9 checks + `tools/file_audit` (713文件) + `grep` 13标准扫描，产出 `reports/v19r8_quality/machine_consistency_before.json` + `file_audit_before.json` + `standards_violations.json` (M+E) [P0]
- [ ] S4-02 calibration/plate_solve 域精查 → `audit_findings_calibration_platesolve.md` (H+E) [P0]
- [ ] S4-03 gaia/dynamic_psf/star/photometric 域精查 → `audit_findings_gaia_psf_photometric.md` (H+E) [P0]
- [ ] S4-04 snr_estimator 域精查（含 NOISE_MODEL/UNCERTAINTY 合同、k_corr/ivar/SIP variance）→ `audit_findings_snr.md` (H+E) [P0]
- [ ] S4-05 healpix_drizzle/common 域精查（含 DRIZZLE/HEALPIX_MAPPING、geometry cache/run generation、α²v）→ `audit_findings_drizzle.md` (H+E) [P0]
- [ ] S4-06 orchestrator/phase2/acr 域精查（含 UPM/排异/积分/ACR dormant、frame_id 绑定、状态机）→ `audit_findings_phase2.md` (H+E) [P0]
- [ ] S4-07 astro_image_io 域精查（含 HiPS 读写/原子写/hierarchy、UPM sparse/dense、FP64）→ `audit_findings_io.md` (H+E) [P0]
- [ ] S4-08 architecture/standards 域精查（含 13标准、模块模板、线程/所有权/错误/IO 合规）→ `audit_findings_architecture_standards.md` (H+E) [P1]
- [ ] S4-09 总表汇总：`reports/v19r8_quality/audit_findings.md` P0/P1/P2 分级 + `audit_stats.json`（文件数/违规数/broken数/P0/P1/P2分布）+ 按 S5 优先级排序 (H+E) [P0]
- [ ] S4-10 P0 冻结：P0 清单经 PM 确认冻结，S5 范围锁定，S4 证据包齐全无代码修改 (H+E) [P0]
- [ ] S4 Gate: 8 分表 + 总表 + stats 已齐，P0 清单已冻结，方可进 S5 (H) [P0]

---

### 阶段 S5 — 硬性问题修改·测试·Review闭环 (28 commits, H+M+E, 每 commit 单一模块/单一目的, 最小修改)

> P0 必改/P1 择改；禁止无关重构/格式化/扩大范围；每 commit 关联合同ID + evidence 四件套 + 受影响模块增量 ctest + Fresh Reviewer/Auditor 双签；核心算法改动同步科学/算法/测试

#### S5-代码硬性问题 (20c, 按模块)
- [ ] S5-01 `lib/common` HEALPix NESTED 唯一映射 + `healpix_common.h` 去重 + crypto SHA-256 唯一实现 (H+M) [P0]
- [ ] S5-02 `lib/astro_image_io` HiPS 读写契约/原子写/hierarchy 对齐 (H+M) [P0]
- [ ] S5-03 `lib/astro_image_io` FP64/FP32 双精度路径与 `DATA_SEMANTICS` 一致 (H+M) [P0]
- [ ] S5-04 `lib/astro_image_io` pipeline_frame / dataflow 契约与 `THREADING_MODEL` 一致 (H+M) [P0]
- [ ] S5-05 `lib/astro_image_io` UPM sparse/dense 容器与 `phase2` 绑定一致 (H+M) [P0]
- [ ] S5-06 `lib/calibration` 窗口/数值/错误收口（master 生成/坏点/溢出） (H+M) [P1]
- [ ] S5-07 `lib/plate_solve/cpp/ipv` WCS/SIP 序列化与 `COORDINATE_CONVENTION_V2` 一致 (H+M) [P0]
- [ ] S5-08 `lib/plate_solve` 数值/CRPIX/坐标契约 A/B/C 层验证对齐 (H+M) [P1]
- [ ] S5-09 `lib/gaia_xpsd_client` RA 环绕/polar prune/zero/dangling/缓存/key/并发收口 (H+M) [P0]
- [ ] S5-10 `lib/dynamic_psf` PSF 质量/数值/A/mad/eccentricity (H+M) [P1]
- [ ] S5-11 `lib/star_detector` SDET 阈值/数值 (H+M) [P1]
- [ ] S5-12 `lib/photometric_calib` C ABI guards/头文件契约 + spectrum/响应曲线/数值收口 (H+M) [P0]
- [ ] S5-13 `lib/snr_estimator` Noise 合同 `snr_noise_model_v1` / ivar 语义 (H+M) [P0]
- [ ] S5-14 `lib/snr_estimator` SIP variance / k_corr 域收口 (H+M) [P0]
- [ ] S5-15 `lib/healpix_db/healpix_drizzle` geometry cache / run generation / 线程局部 (H+M) [P0]
- [ ] S5-16 `lib/healpix_db/healpix_drizzle` 方差传播/operation_counts α²v (H+M) [P0]
- [ ] S5-17 `lib/orchestrator` `orchestrator.h` 错误表与 `ERROR_MODEL` 全量一致 + C++17/日志/路径收口 (H+M) [P0]
- [ ] S5-18 `lib/phase2` UPM 持久化 `frame_id_by_index` 原子写 + 绑定不变性 (H+M) [P0]
- [ ] S5-19 `lib/phase2` ivar 默认/support reducer/zero-weight 状态机 (H+M) [P0]
- [ ] S5-20 `lib/phase2` 排异归一化/large-scale/type params + ACR dormant 边界/mode 2 权重 (H+M) [P0]
- [ ] S5 Gate(代码): 受影响模块单元测试全绿，无科学语义改动（等价门未触发则不改算法） (M) [P0]

#### S5-测试与Review (8c)
- [ ] S5-21 增量测试：每 S5 批次后跑受影响模块 `ctest` 全绿 (M+E) [P0]
- [ ] S5-22 全量科学回归：`noise_model_science_test(SNR-001..015)` + `variance_propagation_test` PASS (M+E) [P0]
- [ ] S5-23 `phase2_synthetic_gate(82项含 PR-UPM-001..010)` + `pipeline_frame_contract_test + dataflow_fuzz` PASS (M+E) [P0]
- [ ] S5-24 编译告警矩阵 `-Wall -Wextra -Wpedantic` 0 first-party warning (M+E) [P0]
- [ ] S5-25 `comment hygiene` 0 violation + `file_audit 713/713` 0 UNREVIEWED + `machine_consistency` 9/9 0 broken (M+E) [P0]
- [ ] S5-26 WSL ASan/UBSan 矩阵 `reports/v19r8_quality/sanitizer_matrix.md` 0 错误（MinGW 例外如实记录） (M+E) [P0]
- [ ] S5-27 代表帧冒烟：GC 5帧 + Victory 20帧 LUM 端到端 PASS + 性能 vs `BASELINE.md` <5% 回归 (M+E) [P0]
- [ ] S5-28 Fresh Reviewer 全量复核 + Repository Auditor 抽样，双签 `VERDICT: PASS` (H+E) [P0]
- [ ] S5 Gate: P0 清零；全量回归+hygiene+machine+ASan/UBSan+冒烟+性能全绿；双签通过 (H) [P0]

---

### 阶段 S6 — 接口与命名质量优化 (14 commits, H+M+E, 文档驱动)

> 命名统一：CODE_STANDARD（snake_case/C API前缀/常量/错误码）、COMMENT_STANDARD（文件头/函数头/行锚点/禁止裸1e-6/轮次词）、API/ABI（头文件契约/稳定性/版本）、Docs→Code→Tests 一致

- [ ] S6-01 `docs/standards/CODE_STANDARD.md` 命名章节与 `lib/*` 实测命名统一（函数/变量 snake_case、常量 UPPER_SNAKE、C API 前缀 `p2_/aio_/ac_/snr_`） (H+M) [P1]
- [ ] S6-02 `docs/standards/COMMENT_STANDARD.md` 注释章节与全仓文件头/函数头/行锚点一致，裸 `1e-6`/轮次词清零 (H+M) [P1]
- [ ] S6-03 `docs/contracts/PUBLIC_API.md` 头文件契约与 `lib/*/include/*.h` 全量一致，`extern "C"` guards 齐全 (H+M) [P0]
- [ ] S6-04 `docs/architecture/ERROR_MODEL.md` ↔ `orchestrator.h` 错误码 `ERR-*` 全量命名统一 (H+M) [P0]
- [ ] S6-05 `lib/common` + `lib/healpix_db` 类型/常量/函数命名统一 (H+M) [P2]
- [ ] S6-06 `lib/astro_image_io` 接口/函数/类型命名统一 (H+M) [P1]
- [ ] S6-07 `lib/calibration` + `lib/plate_solve` + `lib/gaia_xpsd_client` 命名统一 (H+M) [P2]
- [ ] S6-08 `lib/dynamic_psf` + `lib/star_detector` + `lib/photometric_calib` 命名统一 (H+M) [P2]
- [ ] S6-09 `lib/snr_estimator` 命名统一（`snr_*` 前缀/ivar 语义） (H+M) [P1]
- [ ] S6-10 `lib/healpix_db/healpix_drizzle` 命名统一 (H+M) [P1]
- [ ] S6-11 `lib/orchestrator` + `lib/phase2` + `lib/acr` 命名统一（`p2_*` 前缀/状态机/错误码） (H+M) [P1]
- [ ] S6-12 Docs→Code→Tests 命名一致性复核：TRACEABILITY 符号与源码/测试用例名一致 (H+M) [P0]
- [ ] S6-13 全仓 hygiene 复扫：`CODE_STANDARD/COMMENT_STANDARD` 0 violation + `file_audit 713/713` 0 UNREVIEWED (M+E) [P0]
- [ ] S6-14 全仓编译复扫：`-Wall -Wextra -Wpedantic` 0 first-party warning (M+E) [P0]
- [ ] S6 Gate: 命名/注释 0 violation；713/713 0 UNREVIEWED；0 warning；Docs→Code→Tests 一致 (M) [P0]

---

### 全局门 (G-QA, 阻断发布, 任一 FAIL → BLOCKED_REPORT.md)

| 门 | 阈值 | 证据 | 等级 |
|---|---|---|---|
| G-QA-01 文档权威链 | Wiki→L1→L2→L3→L4→L5 machine 9/9 0 broken | `machine_consistency_after.json` | P0 |
| G-QA-02 追溯 | TRACEABILITY ~80行全 VERIFIED 0 broken | `TRACEABILITY.csv` | P0 |
| G-QA-03 审计 | 713/713 0 UNREVIEWED | `file_audit_after.json` | P0 |
| G-QA-04 注释 | COMMENT hygiene 0 violation | `comment_hygiene.json` | P1 |
| G-QA-05 编译 | 0 first-party warning | `warnings.log` | P0 |
| G-QA-06 科学测试 | SNR-001..015 + DRZ variance + phase2 82项 全 PASS | `ctest.log` | P0 |
| G-QA-07 消毒 | WSL ASan/UBSan 0 错误 | `sanitizer_matrix.md` | P0 |
| G-QA-08 文档同步 | SCIENCE_FREEZE/RELEASE/CHANGELOG/TRACEABILITY 同步 | `docs/*` diff | P1 |
| G-QA-09 留痕 | 每 commit evidence 四件套 + Reviewer/Auditor | `evidence/QA-V19R8-*/` | P0 |
| G-QA-10 干净HEAD | git status clean + 可复现 build | `git status` + build log | P0 |

### 交付物总检 (H+M+E)
- [ ] `reports/v19r8_quality/`（audit 分表+总表+stats + machine_consistency_*.json + sanitizer_matrix）齐全 [P0]
- [ ] `evidence/QA-V19R8-*/` 每 commit 四件套齐全 [P0]
- [ ] `run/logs/` + `self_review/roundN/` + `SHA256SUMS.txt` 齐全 [P0]
- [ ] `RELEASE_STATUS.md` `PRE_RELEASE_ENGINEERING_FOUNDATION=V19R8` / `FINAL_REAL_DATA_VALIDATION=PENDING` 口径正确 [P0]
