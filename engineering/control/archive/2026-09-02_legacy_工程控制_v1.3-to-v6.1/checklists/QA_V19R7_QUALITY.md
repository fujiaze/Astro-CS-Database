# QA-V19R7 Checklist — 四层统一与代码质量闭环

> 对应 Spec: `工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md` | 约 100 commits | vm-bj 持续运行

## 使用说明
- 每行一项，`[ ]` 待办 / `[x]` 完成 / `[~]` 部分 / `[N/A]` 不适用
- 带 `M` 为机器校验（脚本/CI），`H` 为人工审查，`E` 需证据
- 任一 P0 FAIL 阻断下一阶段；全部 P0+P1 PASS 方可签 `VERDICT: PASS`

---

### 阶段 A — 审查 (10 commits, 只读不改)

#### A1 机器扫描 (2 commits, M+E)
- [ ] A1-01 运行 `tools/docs_machine_consistency.py` 生成 `reports/v19r7_quality/machine_consistency_before.json`，记录 broken 数、缺失符号、孤儿合同 (M+E)
- [ ] A1-02 运行 `tools/file_audit` + `grep` 规范扫描（命名/注释/数值/并发/C ABI/错误/IO/日志），生成 `reports/v19r7_quality/file_audit_before.json` 与 `standards_violations.json` (M+E)
- [ ] A1 Gate: 机器扫描证据已落盘，无代码修改 (H)

#### A2 人工审计 (8 commits, H+E)
- [ ] A2-01 science 域审计：`SCIENCE_SCOPE/CALIBRATION/ASTROMETRY/PHOTOMETRY/PSF` vs `lib/calibration / plate_solve / dynamic_psf / photometric_calib` → `audit_findings_science.md` (H+E)
- [ ] A2-02 noise 域审计：`NOISE_MODEL/UNCERTAINTY_AND_COVARIANCE` vs `lib/snr_estimator` + `TRACEABILITY SCI-NOISE-*` → `audit_findings_noise.md` (H+E)
- [ ] A2-03 drizzle 域审计：`DRIZZLE/HEALPIX_MAPPING/DRIZZLE_GEOMETRY` vs `lib/healpix_db/healpix_drizzle / lib/common` → `audit_findings_drizzle.md` (H+E)
- [ ] A2-04 phase2 域审计：`PHASE2_UPM/INTEGRATION/REJECTION` vs `lib/phase2` (UPM/排异/积分) → `audit_findings_phase2.md` (H+E)
- [ ] A2-05 io 域审计：`DATA_SEMANTICS/COMPATIBILITY/PIPELINE` vs `lib/astro_image_io / lib/orchestrator` → `audit_findings_io.md` (H+E)
- [ ] A2-06 architecture 域审计：`ARCHITECTURE/MODULE_MAP/DEPENDENCY_RULES/OWNERSHIP/THREADING/ERROR/IO/CACHE/PERFORMANCE` vs 全仓 → `audit_findings_architecture.md` (H+E)
- [ ] A2-07 standards 域审计：`docs/standards/* 13项` vs `lib/*` 实测违规清单 → `audit_findings_standards.md` (H+E)
- [ ] A2-08 总表汇总：`reports/v19r7_quality/audit_findings.md` P0/P1/P2 分级 + `audit_stats.json`（文件数/违规数/broken数）+ 优先级排序 (H+E)
- [ ] A Gate: 8 分表 + 总表 + stats 已齐，P0 清单已冻结 (H)

---

### 阶段 B — 改正 (约 68 commits, 最小修改)

#### B1 科学层 (10 commits, H+M+E)
- [ ] B1-01 `docs/science/SCIENCE_SCOPE.md` 范围/假设/失效域对齐代码实际支持域 (H+M)
- [ ] B1-02 `docs/science/CALIBRATION.md` 公式/单位/误差与 `lib/calibration` 对齐 (H+M)
- [ ] B1-03 `docs/science/ASTROMETRY.md` + `docs/science/PSF.md` 与 `plate_solve/ipv + dynamic_psf/star_detector` 对齐 (H+M)
- [ ] B1-04 `docs/science/PHOTOMETRY.md` 与 `photometric_calib`（flux校准）对齐 (H+M)
- [ ] B1-05 `docs/science/NOISE_MODEL.md` 三层模型/符号/单位与 `snr_estimator` 对齐 (H+M)
- [ ] B1-06 `docs/science/UNCERTAINTY_AND_COVARIANCE.md` 协方差未建模声明与 `healpix_drizzle` 量化一致 (H+M)
- [ ] B1-07 `docs/science/DRIZZLE.md` 方差传播 α²v / k_corr=1.4 / 几何缓存与 `healpix_drizzle` 一致 (H+M)
- [ ] B1-08 `docs/science/PHASE2_UPM.md` UPM 权重 `quality×geom×ivar` / control_ivar / frame_id 绑定与 `phase2/upm.cpp+sampler.cpp` 一致 (H+M)
- [ ] B1-09 `docs/science/INTEGRATION.md` ivar 默认 / zero-weight 合同 / support reducer 与 `phase2/integrate.cpp` 一致 (H+M)
- [ ] B1-10 `docs/science/REJECTION.md` 排异语义/归一化/large-scale 与 `phase2/rejection` 一致 (H+M)
- [ ] B1 Gate: 11 份 science 文档 `machine_consistency` 0 broken，TRACEABILITY SCI-* 行可追溯 (M)

#### B2 算法层 (12 commits, H+M+E)
- [ ] B2-01 `docs/algorithms/CALIBRATION_ALGORITHMS.md` 输入/输出/不变量/伪代码/复杂度/oracle 对齐 (H+M)
- [ ] B2-02 `docs/algorithms/PLATESOLVE.md` 同上 (H+M)
- [ ] B2-03 `docs/algorithms/STAR_PSF_ALGORITHMS.md` 同上 (H+M)
- [ ] B2-04 `docs/algorithms/PHOTOMETRIC_FIT.md` 同上 (H+M)
- [ ] B2-05 `docs/algorithms/NOISE_ESTIMATION.md` 同上 (H+M)
- [ ] B2-06 `docs/algorithms/GAIA_QUERY.md` 同上 (H+M)
- [ ] B2-07 `docs/algorithms/DRIZZLE_GEOMETRY.md` + `HEALPIX_MAPPING.md` 同上 (H+M)
- [ ] B2-08 `docs/algorithms/PHASE2_SAMPLER.md` 同上 (H+M)
- [ ] B2-09 `docs/algorithms/UPM_SOLVER.md` 同上 (H+M)
- [ ] B2-10 `docs/algorithms/REJECTION_ALGORITHMS.md` 同上 (H+M)
- [ ] B2-11 `docs/algorithms/INTEGRATION_ALGORITHMS.md` 同上 (H+M)
- [ ] B2-12 算法层交叉一致性：science↔algorithm 符号/公式逐条对齐，无 legacy 描述残留 (H+M)
- [ ] B2 Gate: 12 份 algorithm 文档全量 `machine_consistency` 0 broken (M)

#### B3 架构层 (10 commits, H+M+E)
- [ ] B3-01 `docs/architecture/ARCHITECTURE.md` 概览与 `MODULE_MAP/DATA_FLOW/PIPELINE` 一致 (H+M)
- [ ] B3-02 `docs/architecture/MODULE_MAP.md` 13 模块职责/接口/依赖与 `lib/*` 实际一致 (H+M)
- [ ] B3-03 `docs/architecture/DEPENDENCY_RULES.md` 依赖方向无环，与 `lib/*` include 实测一致 (H+M)
- [ ] B3-04 `docs/architecture/DATA_FLOW.md` + `PIPELINE.md` 数据流与 `orchestrator` 实际管线一致 (H+M)
- [ ] B3-05 `docs/architecture/OWNERSHIP_AND_LIFETIME.md` 所有权/生命周期与 `lib/*` RAII/原子写一致 (H+M)
- [ ] B3-06 `docs/architecture/THREADING_MODEL.md` 线程模型与 `phase2/healpix_drizzle/orchestrator` 实际一致 (H+M)
- [ ] B3-07 `docs/architecture/ERROR_MODEL.md` 错误码表与 `orchestrator.h + lib/*` 全量一致 (H+M)
- [ ] B3-08 `docs/architecture/IO_AND_ATOMICITY.md` + `CACHE_POLICY.md` 原子写/缓存与 `astro_image_io/phase2` 一致 (H+M)
- [ ] B3-09 `docs/architecture/COMPATIBILITY_POLICY.md` 版本/兼容与 `TRACEABILITY` release_gate 一致 (H+M)
- [ ] B3-10 `docs/architecture/PERFORMANCE_MODEL.md` 性能模型与 `docs/performance/BASELINE.md` 实测一致 (H+M)
- [ ] B3 Gate: 12 份 architecture 文档 0 broken，`PUBLIC_API` machine 一致 (M)

#### B4 代码层 (28 commits, H+M+E, 每 commit 单一模块)
- [ ] B4-01 `lib/common` HEALPix NESTED 唯一映射 + `healpix_common.h` 去重 (H+M)
- [ ] B4-02 `lib/common/crypto` SHA-256 唯一实现，去重 orchestrator/ACR 重复 (H+M)
- [ ] B4-03 `lib/astro_image_io` HiPS 读写契约/原子写/hierarchy 对齐 (H+M)
- [ ] B4-04 `lib/astro_image_io` FP64/FP32 双精度路径与 `DATA_SEMANTICS` 一致 (H+M)
- [ ] B4-05 `lib/astro_image_io` pipeline_frame / dataflow 契约与 `THREADING_MODEL` 一致 (H+M)
- [ ] B4-06 `lib/astro_image_io` AIO UPM sparse/dense 容器与 `phase2` 绑定一致 (H+M)
- [ ] B4-07 `lib/calibration` CAL 窗口/数值/错误收口 (H+M)
- [ ] B4-08 `lib/calibration` master 生成/坏点/错误码对齐 (H+M)
- [ ] B4-09 `lib/plate_solve/cpp/ipv` WCS/SIP 序列化与 `COORDINATE_CONVENTION_V2` 一致 (H+M)
- [ ] B4-10 `lib/plate_solve` 数值/CRPIX/误差对齐 (H+M)
- [ ] B4-11 `lib/plate_solve` 坐标契约 A/B/C 层验证对齐 (H+M)
- [ ] B4-12 `lib/gaia_xpsd_client` RA 环绕/polar prune/zero/dangling 收口 (H+M)
- [ ] B4-13 `lib/gaia_xpsd_client` 缓存/key/并发收口 (H+M)
- [ ] B4-14 `lib/dynamic_psf` PSF 质量/数值/A/mad/eccentricity (H+M)
- [ ] B4-15 `lib/star_detector` SDET 阈值/数值 (H+M)
- [ ] B4-16 `lib/photometric_calib` C ABI guards/头文件契约 (H+M)
- [ ] B4-17 `lib/photometric_calib` spectrum/响应曲线/数值收口 (H+M)
- [ ] B4-18 `lib/snr_estimator` Noise 合同 `snr_noise_model_v1` / ivar 语义 (H+M)
- [ ] B4-19 `lib/snr_estimator` SIP variance / k_corr 域收口 (H+M)
- [ ] B4-20 `lib/healpix_db/healpix_drizzle` geometry cache / run generation (H+M)
- [ ] B4-21 `lib/healpix_db/healpix_drizzle` 方差传播/operation_counts (H+M)
- [ ] B4-22 `lib/healpix_db/healpix_drizzle` 原子 run-gen / 线程局部收口 (H+M)
- [ ] B4-23 `lib/orchestrator` `orchestrator.h` 错误表与 `ERROR_MODEL` 全量一致 (H+M)
- [ ] B4-24 `lib/orchestrator` C++17/日志/路径收口 (H+M)
- [ ] B4-25 `lib/phase2` UPM 持久化 `frame_id_by_index` 原子写 (H+M)
- [ ] B4-26 `lib/phase2` ivar 默认/support reducer/zero-weight 状态机 (H+M)
- [ ] B4-27 `lib/phase2` 排异归一化/large-scale/type params (H+M)
- [ ] B4-28 `lib/phase2` ACR dormant 边界/mode 2 权重 (H+M)
- [ ] B4 Gate: 受影响模块单元测试全绿，无科学语义改动（等价门未触发则不改算法） (M)

#### B5 项目层 (8 commits, H+M+E)
- [ ] B5-01 `工程控制/control/PROJECT_STATE.yaml` 迁移 `v1.3 p13-002 → V19R7` 对齐 HEAD (H+M)
- [ ] B5-02 `工程控制/control/CURRENT_TASK.md` 同步到 V19R7 当前阶段 (H)
- [ ] B5-03 `工程控制/control/DECISION_REGISTER.md` 追加 QA 决策 (H)
- [ ] B5-04 `工程控制/control/RISK_REGISTER.csv` 更新风险关闭/新增 (H)
- [ ] B5-05 `工程控制/control/MASTER_TASK_REGISTER.csv` 追加 QA-V19R7 段 (H+M)
- [ ] B5-06 `docs/TRACEABILITY.csv` 补齐 V19R6R2-W1 增量（~12 行）+ 全量 machine 一致 (M)
- [ ] B5-07 `docs/RELEASE_STATUS.md` + `docs/KNOWN_LIMITATIONS.md` 同步到 V19R7（仍 PENDING 真实数据） (H)
- [ ] B5-08 `CHANGELOG.md` + `docs/DEVELOPER_GUIDE.md` + `docs/README-DOCS.md` 同步 (H)
- [ ] B5 Gate: `PROJECT_STATE` 与 `TRACEABILITY` 双 machine 校验 0 broken (M)

---

### 阶段 C — 测试 (8 commits, M+E)

- [ ] C-01 增量测试：每 B 批次后跑受影响模块 `ctest` (M+E)
- [ ] C-02 全量科学回归：`noise_model_science_test(SNR-001..015)` PASS (M+E)
- [ ] C-03 `variance_propagation_test(SNR-011/012+DRZ)` PASS (M+E)
- [ ] C-04 `phase2_synthetic_gate(82项含 PR-UPM-001..010)` PASS (M+E)
- [ ] C-05 `pipeline_frame_contract_test + dataflow_fuzz` PASS (M+E)
- [ ] C-06 编译告警矩阵 `-Wall -Wextra -Wpedantic` 0 first-party warning (M+E)
- [ ] C-07 `comment hygiene` 0 violation + `file_audit 713/713` 0 UNREVIEWED (M+E)
- [ ] C-08 `tools/docs_machine_consistency.py` 0 broken (M+E)
- [ ] C-09 WSL ASan/UBSan 矩阵 `reports/v19r7_quality/sanitizer_matrix.md` 0 错误（MinGW 例外如实记录） (M+E)
- [ ] C-10 代表帧冒烟：GC 5帧 + Victory 20帧 LUM 端到端 PASS（不跑 BASS 全量） (M+E)
- [ ] C-11 性能快照：Drizzle/Phase2/Browser vs `BASELINE.md` <5% 回归 (M+E)
- [ ] C Gate: C-02..C-11 全部 PASS 方可进 D (H)

---

### 阶段 D — 校核 (6 commits, H+M+E)

- [ ] D-01 Fresh Reviewer 全量复核 `reports/v19r7_quality/review_*.md` (H+E)
- [ ] D-02 Repository Auditor 逐文件抽样 `audit_report.md` (H+E)
- [ ] D-03 `self_review/roundN` 记录（至少 1 轮 clean-tree） (H+E)
- [ ] D-04 证据包 `reports/v19r7_quality/` + `run/logs/` + `SHA256SUMS.txt` 齐全 (M+E)
- [ ] D-05 单一干净 HEAD 校验：`git status` clean + 可复现 `toolchain.ps1 build` (M+E)
- [ ] D-06 发布口径：`RELEASE_STATUS.md` `PRE_RELEASE_ENGINEERING_FOUNDATION=V19R7` / `FINAL_REAL_DATA_VALIDATION=PENDING` (H)
- [ ] D Gate: Reviewer `VERDICT: PASS` + Auditor `VERDICT: PASS` 双签 (H)

---

### 全局门 (G-QA, 阻断发布)

| 门 | 阈值 | 证据 |
|---|---|---|
| G-QA-01 四层一致 | machine_consistency broken=0 | `machine_consistency_after.json` |
| G-QA-02 追溯 | TRACEABILITY 全覆盖 ~75 行全 VERIFIED | `TRACEABILITY.csv` |
| G-QA-03 审计 | 713/713 0 UNREVIEWED | `file_audit_after.json` |
| G-QA-04 注释 | 0 violation | `comment_hygiene.json` |
| G-QA-05 编译 | 0 first-party warning | `warnings.log` |
| G-QA-06 科学测试 | SNR/DRZ/phase2 全 PASS | `ctest.log` |
| G-QA-07 消毒 | WSL ASan/UBSan 0 错误 | `sanitizer_matrix.md` |
| G-QA-08 文档同步 | SCIENCE_FREEZE/RELEASE/CHANGELOG/TRACEABILITY 同步 | `docs/*` diff |
| G-QA-09 留痕 | 每 commit evidence 四件套 | `evidence/QA-V19R7-*/` |
| G-QA-10 干净HEAD | git status clean + 可复现 | `git status` + build log |

任一 FAIL 阻断发布，需 `BLOCKED_REPORT.md`。
