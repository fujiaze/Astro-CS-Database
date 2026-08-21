# QA-V19R7 — 四层统一与代码质量优化（约 100 commits）

> Spec: `工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md`
> Checklist: `工程控制/checklists/QA_V19R7_QUALITY.md`
> 基线: V19R6R2-W1 (HEAD 2767874) | 模式: 审查→改正→测试→校核 | 地点: vm-bj Linux

## 0. 任务总览

本任务为 umbrella，统管 QA-V19R7 全流程，目标约 100 commits，持续在 vm-bj 运行至 `G-QA-01..10` 全绿。禁止未完成结束。

### 0.1 Commit 预算

```
A 审查  10c | B1 科学 10c | B2 算法 12c | B3 架构 10c | B4 代码 28c | B5 项目 8c | C 测试 8c | D 校核 6c = 92c
+ 8 缓冲/回滚 = 100c
```

### 0.2 阶段依赖

```
A1(2) → A2(8) ─┬─→ B1(10) → B2(12) → B3(10) → B4(28) → B5(8) → C(8) → D(6)
                └── 每 10c checkpoint 向用户汇报，未绿不进下一阶段
并行上限 4 Resident: resident:science / resident:architecture / resident:code / resident:project
```

### 0.3 文件面

- L1 科学 11 + L2 算法 12 + L3 架构 12 + L4 标准 13 + L5 模块 13 + 项目 ~10 + 工程控制 ~15
- 代码 13 模块 713 文件 | 追溯 64→~75 行

---

## 1. 阶段 A — 审查（只读不改，10 commits）

### QA-V19R7-A1-01 — 机器一致性初扫（M+E）

- **目标**: 产出 `reports/v19r7_quality/machine_consistency_before.json` 与 broken 统计
- **输入**: `docs/TRACEABILITY.csv` + `docs/science|algorithms|architecture|modules` + `lib/*` 符号表 + `tools/docs_machine_consistency.py`
- **步骤**: 运行 machine_consistency 脚本；统计 requirement→algorithm→module→api→impl→test→diagnostic 断链；记录孤儿合同与缺失符号
- **输出**: `reports/v19r7_quality/machine_consistency_before.json` + `broken_before.log`
- **验收**: 脚本零异常，JSON 可复现，broken 数入 `audit_stats.json`
- **证据**: `evidence/QA-V19R7-A1-01/` 四件套 | **Commit**: `chore(qa): machine consistency before snapshot [A1-01]`

### QA-V19R7-A1-02 — 文件审计与标准扫描（M+E）

- **目标**: 产出 `file_audit_before.json` 与 `standards_violations.json`
- **输入**: 全仓文件清单 + `tools/file_audit` + `grep` 13 标准扫描
- **步骤**: 跑 file_audit (713 文件)；grep 扫描 CODE/NUMERIC/CONCURRENCY/C_ABI/ERROR/IO/LOGGING/COMMENT 违规；分类 P0/P1/P2
- **输出**: `file_audit_before.json` + `standards_violations.json` + `audit_stats.json`
- **验收**: 审计覆盖 713/713，违规可定位到文件/行
- **证据**: `evidence/QA-V19R7-A1-02/` | **Commit**: `chore(qa): file audit before snapshot [A1-02]`

### QA-V19R7-A2-01 — science 域人工审计（H+E）

- **范围**: `SCIENCE_SCOPE/CALIBRATION/ASTROMETRY/PHOTOMETRY/PSF` vs `lib/calibration|plate_solve|dynamic_psf|photometric_calib`
- **产出**: `reports/v19r7_quality/audit_findings_science.md` (P0/P1/P2 + 文件/行号 + 关联 TR 合同)
- **方法**: 逐节对照公式/单位/假设/失效域与代码实现；标记不一致为 P0，缺失溯源为 P1，表述不清为 P2
- **证据**: `evidence/QA-V19R7-A2-01/` | **Commit**: `docs(qa): audit findings science [A2-01]`

### QA-V19R7-A2-02 — noise 域审计（H+E）

- **范围**: `NOISE_MODEL/UNCERTAINTY_AND_COVARIANCE` vs `lib/snr_estimator` + `TRACEABILITY SCI-NOISE-*`
- **重点**: 三层模型符号、k_corr=1.4 域、α²v 方差、协方差未建模声明、ivar 语义
- **产出**: `audit_findings_noise.md` | **Commit**: `docs(qa): audit findings noise [A2-02]`

### QA-V19R7-A2-03 — drizzle 域审计（H+E）

- **范围**: `DRIZZLE/HEALPIX_MAPPING/DRIZZLE_GEOMETRY` vs `lib/healpix_db/healpix_drizzle|common`
- **重点**: 几何缓存、run generation、false_negative=0、variance 传播、operation_counts
- **产出**: `audit_findings_drizzle.md` | **Commit**: `docs(qa): audit findings drizzle [A2-03]`

### QA-V19R7-A2-04 — phase2 域审计（H+E）

- **范围**: `PHASE2_UPM/INTEGRATION/REJECTION` vs `lib/phase2` (upm/sampler/integrate/rejection/acr)
- **重点**: UPM 权重 `quality×geom×ivar`、control_ivar、frame_id 绑定、排异归一化、large-scale、integration 状态机
- **产出**: `audit_findings_phase2.md` | **Commit**: `docs(qa): audit findings phase2 [A2-04]`

### QA-V19R7-A2-05 — io 域审计（H+E）

- **范围**: `DATA_SEMANTICS/COMPATIBILITY/PIPELINE` vs `lib/astro_image_io|orchestrator`
- **重点**: HiPS signal/support/ivar、原子写、hierarchy、FP64、PipelineFrame
- **产出**: `audit_findings_io.md` | **Commit**: `docs(qa): audit findings io [A2-05]`

### QA-V19R7-A2-06 — architecture 域审计（H+E）

- **范围**: `ARCHITECTURE/MODULE_MAP/DEPENDENCY_RULES/OWNERSHIP/THREADING/ERROR/IO/CACHE/PERFORMANCE` vs 全仓
- **产出**: `audit_findings_architecture.md` | **Commit**: `docs(qa): audit findings architecture [A2-06]`

### QA-V19R7-A2-07 — standards 域审计（H+E）

- **范围**: `docs/standards/* 13项` vs `lib/*` 实测违规
- **重点**: CODE/COMMENT/NUMERIC/C_ABI/CONCURRENCY/ERROR/IO/LOGGING/TEST/BENCHMARK/DOCUMENTATION
- **产出**: `audit_findings_standards.md` | **Commit**: `docs(qa): audit findings standards [A2-07]`

### QA-V19R7-A2-08 — 总表汇总（H+E）

- **目标**: 汇总 8 分表为 `audit_findings.md` + `audit_stats.json` + P0 冻结清单与优先级
- **输出**: 总表含 P0(阻断)/P1(必须)/P2(建议) 三级，关联合同 ID，按 B1→B5 顺序排序
- **验收**: 总表覆盖全部审计域，P0 清单经 PM 确认冻结
- **证据**: `evidence/QA-V19R7-A2-08/` | **Commit**: `docs(qa): audit findings summary [A2-08]`

**A Gate**: 8 分表+总表+stats 齐全，P0 清单冻结，方可进 B。

---

## 2. 阶段 B1 — 科学层改正（10 commits, 最小修改）

> 每 commit 单一文档，关联 TR 合同，machine_consistency 增量校验

| ID | 文档 | 对齐对象 | 关键合同 | 验收 |
|---|---|---|---|---|
| B1-01 | SCIENCE_SCOPE.md | 全仓支持域 | SCI-SCOPE-* | 范围/假设/失效域与代码实际一致 |
| B1-02 | CALIBRATION.md | lib/calibration | SCI-CAL-* | 公式/单位/误差与 calibrator.cpp 一致 |
| B1-03 | ASTROMETRY.md + PSF.md | plate_solve/ipv + dynamic_psf/star_detector | SCI-AST-*/SCI-PSF-* | WCS/SIP/PSF 质量与 ipv_wcs/star_detector 一致 |
| B1-04 | PHOTOMETRY.md | photometric_calib | SCI-PHOT-* | flux 校准与 pc_api 一致 |
| B1-05 | NOISE_MODEL.md | snr_estimator | SCI-NOISE-001..015 | 三层模型/符号/单位一致 |
| B1-06 | UNCERTAINTY_AND_COVARIANCE.md | healpix_drizzle + snr_estimator | SCI-NOISE-012 | 协方差未建模声明与量化一致 |
| B1-07 | DRIZZLE.md | healpix_drizzle/common | SCI-DRZ-* | α²v/k_corr=1.4/几何缓存一致 |
| B1-08 | PHASE2_UPM.md | phase2/upm+sampler | SCI-UPM-* + SCI-UPM-PERSIST-001 | 权重/ivar/frame_id 绑定一致 |
| B1-09 | INTEGRATION.md | phase2/integrate | ALG-INTEGRATE-* | ivar 默认/zero-weight/support reducer 一致 |
| B1-10 | REJECTION.md | phase2/rejection | SCI-REJ-* | 语义/归一化/large-scale 一致 |

**B1 Gate**: 11 份 science 文档 machine_consistency 0 broken，SCI-* 全 VERIFIED。

---

## 3. 阶段 B2 — 算法层改正（12 commits）

> 每份 algorithm 补输入/输出/前置/不变量/伪代码/复杂度/oracle + 源码入口，删 legacy 残留

| ID | 文档 | 对齐源码 | 验收 |
|---|---|---|---|
| B2-01 | CALIBRATION_ALGORITHMS.md | lib/calibration/src/* | 入口/复杂度/oracle 一致 |
| B2-02 | PLATESOLVE.md | lib/plate_solve/cpp/ipv/src/* | 同上 |
| B2-03 | STAR_PSF_ALGORITHMS.md | lib/dynamic_psf + lib/star_detector | 同上 |
| B2-04 | PHOTOMETRIC_FIT.md | lib/photometric_calib/cpp/src/* | 同上 |
| B2-05 | NOISE_ESTIMATION.md | lib/snr_estimator/cpp/src/* | 同上 |
| B2-06 | GAIA_QUERY.md | lib/gaia_xpsd_client/src/* | 同上 |
| B2-07 | DRIZZLE_GEOMETRY.md | lib/healpix_db/healpix_drizzle/*.cpp | 同上 |
| B2-08 | HEALPIX_MAPPING.md | lib/common/healpix* | 同上 |
| B2-09 | PHASE2_SAMPLER.md | lib/phase2/src/sampler.cpp | 同上 |
| B2-10 | UPM_SOLVER.md | lib/phase2/src/upm.cpp | 同上 |
| B2-11 | REJECTION_ALGORITHMS.md | lib/phase2/src/rejection* | 同上 |
| B2-12 | INTEGRATION_ALGORITHMS.md | lib/phase2/src/integrate.cpp | 同上 + science↔algorithm 符号对齐 |

**B2 Gate**: 12 份 algorithm 文档 0 broken，无冻结不一致描述。

---

## 4. 阶段 B3 — 架构层改正（10 commits）

| ID | 文档 | 对齐对象 | 验收 |
|---|---|---|---|
| B3-01 | ARCHITECTURE.md | MODULE_MAP+DATA_FLOW+PIPELINE | 概览一致 |
| B3-02 | MODULE_MAP.md | lib/* 13 模块 | 职责/接口/依赖一致 |
| B3-03 | DEPENDENCY_RULES.md | include 实测 | 依赖无环 |
| B3-04 | DATA_FLOW.md + PIPELINE.md | lib/orchestrator | 数据流一致 |
| B3-05 | OWNERSHIP_AND_LIFETIME.md | RAII/原子写 | 所有权一致 |
| B3-06 | THREADING_MODEL.md | phase2/drizzle/orchestrator | 线程一致 |
| B3-07 | ERROR_MODEL.md | orchestrator.h + lib/* | 错误码全量一致 |
| B3-08 | IO_AND_ATOMICITY.md + CACHE_POLICY.md | astro_image_io/phase2 | 原子写/缓存一致 |
| B3-09 | COMPATIBILITY_POLICY.md | TRACEABILITY release_gate | 版本一致 |
| B3-10 | PERFORMANCE_MODEL.md | docs/performance/BASELINE.md | 性能模型一致 |

**B3 Gate**: 12 份 architecture 文档 0 broken，PUBLIC_API machine 一致。

---

## 5. 阶段 B4 — 代码层改正（28 commits）

> 每 commit 单一模块，最小修改，禁止 cosmetic，关联合同 ID

| ID | 模块 | 内容 | 关键标准 | 验收 |
|---|---|---|---|---|
| B4-01 | common | HEALPix NESTED 唯一映射去重 | CODE/NUMERIC | 单一映射，无重复 |
| B4-02 | common/crypto | SHA-256 唯一实现 | CODE/IO | 3 份重复删除 |
| B4-03 | astro_image_io | HiPS 读写/原子写/hierarchy | IO/C_ABI | 契约一致 |
| B4-04 | astro_image_io | FP64/FP32 双精度 | NUMERIC | 与 DATA_SEMANTICS 一致 |
| B4-05 | astro_image_io | PipelineFrame/dataflow 契约 | CONCURRENCY | 与 THREADING 一致 |
| B4-06 | astro_image_io | AIO UPM sparse/dense 容器 | OWNERSHIP | 与 phase2 绑定一致 |
| B4-07 | calibration | CAL 窗口/数值 | NUMERIC/ERROR | 窗口/溢出收口 |
| B4-08 | calibration | master 生成/坏点/错误码 | ERROR | 错误表一致 |
| B4-09 | plate_solve/ipv | WCS/SIP 序列化 | NUMERIC/IO | 与 V2 契约一致 |
| B4-10 | plate_solve | CRPIX/数值 | NUMERIC | 数值安全 |
| B4-11 | plate_solve | 坐标契约 A/B/C 层 | CODE | 验证对齐 |
| B4-12 | gaia_xpsd_client | RA 环绕/polar prune | ALGORITHM | 极区正确 |
| B4-13 | gaia_xpsd_client | 缓存/key/并发 | CONCURRENCY | 无竞态 |
| B4-14 | dynamic_psf | PSF 质量/数值 | NUMERIC | A/mad/偏心 |
| B4-15 | star_detector | SDET 阈值/数值 | NUMERIC | 阈值一致 |
| B4-16 | photometric_calib | C ABI guards | C_ABI | 头文件 guards |
| B4-17 | photometric_calib | spectrum/响应曲线 | NUMERIC | 数值一致 |
| B4-18 | snr_estimator | Noise 合同 | CODE | ivar 语义 |
| B4-19 | snr_estimator | SIP variance/k_corr | NUMERIC | 域收口 |
| B4-20 | healpix_drizzle | geometry cache/run-gen | CACHE/CONCURRENCY | 缓存正确 |
| B4-21 | healpix_drizzle | 方差/operation_counts | NUMERIC/LOGGING | α²v 正确 |
| B4-22 | healpix_drizzle | 原子 run-gen/线程局部 | CONCURRENCY | 无竞态 |
| B4-23 | orchestrator | orchestrator.h 错误表 | ERROR | 与 ERROR_MODEL 一致 |
| B4-24 | orchestrator | C++17/日志/路径 | CODE/LOGGING | 路径/日志收口 |
| B4-25 | phase2 | UPM 持久化 frame_id_by_index | IO/OWNERSHIP | 原子写+绑定 |
| B4-26 | phase2 | ivar/support/zero-weight | CODE | 状态机正确 |
| B4-27 | phase2 | 排异归一化/large-scale | ALGORITHM | 语义冻结 |
| B4-28 | phase2/acr | ACR dormant/mode2 | ARCHITECTURE | 边界正确 |

**B4 Gate**: 受影响模块单元测试全绿，无科学语义改动（否则走等价门）。

---

## 6. 阶段 B5 — 项目层改正（8 commits）

| ID | 文件 | 内容 | 验收 |
|---|---|---|---|
| B5-01 | 工程控制/control/PROJECT_STATE.yaml | v1.3 p13-002 → V19R7 迁移 | 与 HEAD 一致 |
| B5-02 | 工程控制/control/CURRENT_TASK.md | 同步到 V19R7 当前阶段 | 与 Spec 一致 |
| B5-03 | 工程控制/control/DECISION_REGISTER.md | 追加 QA 决策 | 可追溯 |
| B5-04 | 工程控制/control/RISK_REGISTER.csv | 更新风险关闭/新增 | 风险闭环 |
| B5-05 | 工程控制/control/MASTER_TASK_REGISTER.csv | 追加 QA-V19R7 段 | 任务可追溯 |
| B5-06 | docs/TRACEABILITY.csv | 补齐 V19R6R2-W1 增量 ~12 行 | machine 0 broken |
| B5-07 | docs/RELEASE_STATUS.md + KNOWN_LIMITATIONS.md | 同步到 V19R7（仍 PENDING） | 口径一致 |
| B5-08 | CHANGELOG.md + DEVELOPER_GUIDE.md + README-DOCS.md | 同步 | 文档一致 |

**B5 Gate**: PROJECT_STATE 与 TRACEABILITY 双 machine 校验 0 broken。

---

## 7. 阶段 C — 测试（8 commits）

| ID | 内容 | 工具 | 验收 |
|---|---|---|---|
| C-01 | 每 B 批次增量 ctest | ctest | 受影响模块 PASS |
| C-02 | noise_model_science_test (SNR-001..015) | ctest | 全 PASS |
| C-03 | variance_propagation_test | ctest | 全 PASS |
| C-04 | phase2_synthetic_gate (82项) | ctest | 全 PASS |
| C-05 | pipeline_frame_contract_test + dataflow_fuzz | ctest | 全 PASS |
| C-06 | 编译告警矩阵 -Wall -Wextra -Wpedantic | build log | 0 first-party warning |
| C-07 | comment hygiene + file_audit | hygiene/file_audit | 0 violation + 713/713 |
| C-08 | machine_consistency 0 broken | machine_consistency | 0 broken |
| C-09 | WSL ASan/UBSan 矩阵 | sanitizer_matrix.md | 0 错误（MinGW 例外如实） |
| C-10 | 代表帧冒烟 GC5+V20 LUM | run logs | 端到端 PASS |
| C-11 | 性能快照 vs BASELINE | BASELINE.md | <5% 回归 |

**C Gate**: C-02..C-11 全部 PASS 方可进 D。

---

## 8. 阶段 D — 校核（6 commits）

| ID | 内容 | 验收 |
|---|---|---|
| D-01 | Fresh Reviewer 全量复核 | review_*.md VERDICT:PASS |
| D-02 | Repository Auditor 抽样 | audit_report.md VERDICT:PASS |
| D-03 | self_review/roundN | 至少 1 轮 clean-tree |
| D-04 | 证据包 reports/v19r7_quality + run/logs + SHA256SUMS | 齐全可复现 |
| D-05 | 单一干净 HEAD 校验 | git status clean + toolchain build |
| D-06 | 发布口径更新 | RELEASE_STATUS V19R7 / PENDING |

**D Gate**: Reviewer + Auditor 双签 PASS。

---

## 9. 全局 Gate（阻断发布）

| 门 | 阈值 | 证据 |
|---|---|---|
| G-QA-01 | machine broken=0 | machine_consistency_after.json |
| G-QA-02 | TRACEABILITY ~75 行全 VERIFIED | TRACEABILITY.csv |
| G-QA-03 | 713/713 0 UNREVIEWED | file_audit_after.json |
| G-QA-04 | 0 violation | comment_hygiene.json |
| G-QA-05 | 0 first-party warning | warnings.log |
| G-QA-06 | SNR/DRZ/phase2 全 PASS | ctest.log |
| G-QA-07 | WSL ASan/UBSan 0 错误 | sanitizer_matrix.md |
| G-QA-08 | SCIENCE_FREEZE/RELEASE/CHANGELOG 同步 | docs diff |
| G-QA-09 | 每 commit 四件套 | evidence/QA-V19R7-*/ |
| G-QA-10 | git clean + 可复现 | git status + build log |

任一 FAIL 阻断，需 BLOCKED_REPORT.md。

---

## 10. 证据与提交规范

- **证据目录**: `reports/v19r7_quality/` + `evidence/QA-V19R7-*/` + `run/logs/` + `self_review/roundN/`
- **Commit 模板**: `type(scope): summary [TR-ID] [QA-xxx]` 例 `docs(science): sync NOISE_MODEL k_corr=1.4 [SCI-NOISE-007] [B1-05]`
- **类型**: docs/science|algorithms|architecture|code|project|chore|quality
- **留痕**: 每 commit 关联 TRACEABILITY 合同 ID，含 TASK/TEST/EVIDENCE/REVIEW 四件套摘要

---

## 11. 风险与依赖

| 风险 | 对策 | 责任 |
|---|---|---|
| R-QA-01 文档滞后扩大 | B5 单独 8c，先迁 PROJECT_STATE | resident:project |
| R-QA-02 科学等价误改 | 走 SCIENCE_FREEZE 等价门，否则仅文档收口 | resident:science |
| R-QA-03 测试环境差异 | MinGW 无 ASan 用 WSL 覆盖 | resident:code |
| R-QA-04 范围蔓延 | CODE_STANDARD 禁 cosmetic，Auditor 卡点 | Auditor |
| R-QA-05 进度拖延 | 每 10c checkpoint 汇报 | PM |
| R-QA-06 ACR 干扰 | ACR dormant 不合 main | resident:architecture |

---

## 12. 时间与运行

- **地点**: vm-bj `/home/lighthouse/Astro CS Database`
- **模式**: 持续运行，goal 100 轮上限，未完成不结束
- **节奏**: 每 10 commits 一轮汇报，阻塞立即 BLOCKED_REPORT.md
- **分支**: main（ACR 保持 feature/astrocompute-runtime 独立）
