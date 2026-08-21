# QA-V19R8 — Wiki→代码 六阶段质量优化（约 100 commits）

> Spec: `工程控制/docs/30_WIKI_TO_CODE_QUALITY_V19R8_SPEC.md`
> Checklist: `工程控制/checklists/QA_V19R8_QUALITY.md`
> 基线: V19R7 (HEAD V19R6R2-W1 延续) | 新范式: S0→S1→S2→S3→S4→S5→S6 | 地点: vm-bj Linux `/home/lighthouse/Astro CS Database`

## 0. 任务总览（Umbrella）

本任务为 umbrella，统管 QA-V19R8 全流程。按用户新范式重排 Spec/Checklist/Tasks 三件套，覆盖 **Wiki→科学→算法→项目/工程/代码文档→P0/P1精查→修改/测试/Review→接口命名统一** 全链。冻结语义不改，不跑 BASS 全量，不合 ACR dormant，持续运行至 `G-QA-01..10` 全绿。

### 0.1 Commit 预算（与 MASTER_TASK_REGISTER 对齐）

```
S0 Wiki与权威链加固          4c
S1 科学文档精确化            10c
S2 算法核心文档精确化        12c
S3 项目/工程/代码开发文档整理 12c
S4 精细化代码审查(Subagent)   10c
S5 硬性问题修改·测试·Review  28c
S6 接口与命名质量优化        14c
─────────────────────────────────
小计 90c + 10c 缓冲/回滚 = 100c
```

`MASTER_TASK_REGISTER.csv` QA-V19R8 段共 90 行主任务 + 10 行缓冲占位，task_id `QA-V19R8-S0-01 .. QA-V19R8-S6-14`。

### 0.2 阶段依赖（跨层串行，同层并行≤4）

```
S0(4) → S1(10) → S2(12) → S3(12) → S4(10) → S5(28) → S6(14)
  │       │       │       │       │       │       │
  └─Gate  └─Gate  └─Gate  └─Gate  └─P0冻结 └─回归+双签 └─0 violation
每 10c checkpoint 向用户汇报；任一 Gate FAIL 阻断下一阶段，需 BLOCKED_REPORT.md
并行上限 4 Resident: resident:wiki / resident:science / resident:algorithm / resident:code / resident:project
Subagent 阶段 S4 并发 ≤4
```

### 0.3 文件面

- Wiki + L1 11 + L2 12 + L3 12+2 + L4 13 + L5 13 + 项目 ~10 + 工程控制 ~15
- 代码 13 模块 ~713 文件 | 追溯 64→~80 行 | 证据 `reports/v19r8_quality/` + `evidence/QA-V19R8-*/` + `run/logs/` + `self_review/`

### 0.4 通用规范（所有子任务适用）

- **输入/输出/验收/证据/依赖** 每任务显式；**Commit message** 模板 `type(scope): summary [TR-ID] [Sxx-NN]`，type ∈ {docs,fix,refactor,chore,quality}，关联 TRACEABILITY 合同 ID。
- **最小修改**: 禁止无关重构/格式化/扩大范围（`AGENTS.md` + `CODE_STANDARD.md`）；职责/接口/数据含义明确。
- **科学等价门**: 算法语义改动需 C/M 逐位或数值等价 + 回归集全绿，否则仅文档/注释/错误/日志收口。
- **证据四件套**: 每 commit 产出 `evidence/QA-V19R8-Sxx-NN/` 含 TASK.md / TEST.log / EVIDENCE/* / REVIEW.md 摘要；长任务 timeout + `run/logs/` 可恢复。
- **Review**: 每批次独立 Fresh Reviewer + Repository Auditor 双签；`AGENTS.md` 单目的 commit。

---

## 1. 阶段 S0 — Wiki与权威链加固（4 commits, H+M+E）

### QA-V19R8-S0-01 — Wiki 核心约束显式化（H+E）

- **目标**: Wiki 作为唯一核心约束，Wiki 索引与 `docs/README-DOCS.md` L0-L5 一一对应，矛盾以 Wiki 为准
- **输入**: Wiki 全量 + `docs/README-DOCS.md` + `docs/validation/SCIENCE_FREEZE.md`
- **步骤**: 梳理 Wiki 目录与 L0-L5 映射表；补齐 Wiki→L1→L2 引用锚点；标注 Wiki 与现有文档不一致项并先修 Wiki
- **输出**: Wiki 索引修订（如有）+ `reports/v19r8_quality/wiki_authority_map.md` 映射表
- **验收**: Wiki→L1→L2 引用无断链，权威链可机检
- **证据**: `evidence/QA-V19R8-S0-01/` 四件套 | **Commit**: `docs(wiki): solidify Wiki as core authority [S0-01]`

### QA-V19R8-S0-02 — README-DOCS L0-L5 权威链对齐（H+M+E）

- **目标**: `docs/README-DOCS.md` L0-L5 定义与 `SCIENCE_FREEZE.md` 冻结红线一致
- **输入**: `docs/README-DOCS.md` + `SCIENCE_FREEZE.md` + `docs/TRACEABILITY.csv`
- **步骤**: 校验 L0 项目入口 / L1 科学 / L2 算法 / L3 架构+契约 / L4 标准 / L5 模块分层声明与权威链一致；补缺失层说明
- **输出**: `docs/README-DOCS.md` 修订版
- **验收**: 权威链 `Wiki→Science→Algorithm→Architecture→Standards→Modules→Source→Test→Diagnostics→Release` 文内可机检
- **证据**: `evidence/QA-V19R8-S0-02/` | **Commit**: `docs(readme): align L0-L5 authority chain [S0-02]`

### QA-V19R8-S0-03 — TRACEABILITY 初补齐（M+E）

- **目标**: `docs/TRACEABILITY.csv` 64→~75 行初补齐，SCI-/ALG-/DATA-/ENG- 与 Wiki/L1/L2/L3 合同 ID 对齐
- **输入**: `docs/TRACEABILITY.csv` + `docs/science|algorithms|architecture|contracts|standards|modules` + `lib/*` 符号表
- **步骤**: 补 V19R7 增量合同行（~11 行）；校验 `requirement_id/authority_doc/algorithm_id/module/public_api/implementation_files/implementation_symbols/test_ids/diagnostic_ids/error_codes/release_gate/status` 字段完整；清重复ID/孤儿合同/空字段
- **输出**: `docs/TRACEABILITY.csv` 修订（~75 行）
- **验收**: 字段完整，无重复ID/孤儿合同
- **证据**: `evidence/QA-V19R8-S0-03/` | **Commit**: `docs(trace): preliminary补齐 to ~75 rows [S0-03]`

### QA-V19R8-S0-04 — 机器一致性初扫 9/9（M+E）

- **目标**: 产出 `machine_consistency_s0.json` + `audit_stats.json` 初版，记录 broken/缺失符号/孤儿合同
- **输入**: 全量 `docs/*` + `docs/TRACEABILITY.csv` + `tools/docs_machine_consistency.py` (9 checks) + `lib/*` 符号表
- **步骤**: 运行 `tools/docs_machine_consistency.py` 9 checks；统计 requirement→algorithm→module→api→impl→test→diagnostic 断链；记录缺失符号与孤儿合同；生成 `audit_stats.json`（文件数/违规数/broken数）
- **输出**: `reports/v19r8_quality/machine_consistency_s0.json` + `machine_consistency_s0.log` + `audit_stats.json`
- **验收**: 脚本零异常，JSON 可复现，S0 Gate broken 清零或清零计划已冻结
- **证据**: `evidence/QA-V19R8-S0-04/` | **Commit**: `chore(qa): machine 9/9 before snapshot S0 [S0-04]`

**S0 Gate**: Wiki 索引与 README-DOCS 一致；machine 9/9 0 broken；TRACEABILITY 初补齐可追溯。未绿不进 S1。

---

## 2. 阶段 S1 — 科学文档精确化（10 commits, H+M+E）

> 每 commit 单一文档，关联 TR 合同，machine 增量校验；模板字段：定义/公式/变量/单位/假设/有效域/失效域/系统与随机误差/数值精度/参考文献/ID + 源码符号/文件行锚点一一对应

| ID | 文档 | 对齐对象 | 关键合同 | 验收 | Commit |
|---|---|---|---|---|---|
| S1-01 | SCIENCE_SCOPE.md | 全仓支持域 | SCI-SCOPE-* | 范围/假设/失效域与代码实际一致 | `docs(science): sync SCOPE [S1-01]` |
| S1-02 | CALIBRATION.md | lib/calibration | SCI-CAL-* | 公式/单位/误差与 calibrator.cpp 一致 | `docs(science): sync CALIBRATION [S1-02]` |
| S1-03 | ASTROMETRY.md + PSF.md | plate_solve/ipv + dynamic_psf/star_detector | SCI-AST-*/SCI-PSF-* | WCS/SIP/PSF 质量与 ipv_wcs/star_detector 一致 | `docs(science): sync ASTROMETRY+PSF [S1-03]` |
| S1-04 | PHOTOMETRY.md | photometric_calib | SCI-PHOT-* | flux 校准/响应曲线与 pc_api 一致 | `docs(science): sync PHOTOMETRY [S1-04]` |
| S1-05 | NOISE_MODEL.md | snr_estimator | SCI-NOISE-001..015 | 三层模型/符号/单位与 snr_estimator 一致 | `docs(science): sync NOISE_MODEL [S1-05]` |
| S1-06 | UNCERTAINTY_AND_COVARIANCE.md | healpix_drizzle + snr_estimator | SCI-NOISE-012 | 协方差未建模声明与量化一致 | `docs(science): sync UNCERTAINTY [S1-06]` |
| S1-07 | DRIZZLE.md | healpix_drizzle/common | SCI-DRZ-* | α²v/k_corr=1.4/几何缓存与 drizzle_engine 一致 | `docs(science): sync DRIZZLE α²v k_corr [S1-07]` |
| S1-08 | PHASE2_UPM.md | phase2/upm+sampler | SCI-UPM-* + SCI-UPM-PERSIST-001 | 权重/ivar/frame_id 绑定与 upm.cpp+sampler.cpp 一致 | `docs(science): sync PHASE2_UPM [S1-08]` |
| S1-09 | INTEGRATION.md | phase2/integrate | ALG-INTEGRATE-* | ivar 默认/zero-weight/support reducer 一致 | `docs(science): sync INTEGRATION [S1-09]` |
| S1-10 | REJECTION.md | phase2/rejection | SCI-REJ-* | 语义/归一化/large-scale 与 rejection 一致 + machine 9/9 0 broken | `docs(science): sync REJECTION [S1-10]` |

每项 **输入**: 对应 `docs/science/*.md` + `lib/*` 实现 + `TRACEABILITY.csv` SCI-* 行；**步骤**: 逐节对照公式/单位/假设/失效域/误差与代码实现，补行锚点；**输出**: 修订后 md；**证据**: `evidence/QA-V19R8-S1-NN/`；**依赖**: `S0 Gate`，S1 内按序但可 ≤4 并行（注意 S1-07/08/09 同为 phase2/drizzle 需串行校验）。

**S1 Gate**: 11 份 science 文档 machine 9/9 0 broken，SCI-* 全 VERIFIED，公式-符号-行锚点抽查一致。

---

## 3. 阶段 S2 — 算法核心文档精确化（12 commits, H+M+E）

> 每份 algorithm 补输入/输出/前置/后置/不变量/伪代码/复杂度/并行模型/数值风险/fast/reference/oracle/ID + 源码入口一一对应，删 legacy 残留

| ID | 文档 | 对齐源码 | 验收 | Commit |
|---|---|---|---|---|
| S2-01 | CALIBRATION_ALGORITHMS.md | lib/calibration/src/* | 入口/复杂度/oracle 一致 | `docs(algorithms): sync CALIBRATION_ALGORITHMS [S2-01]` |
| S2-02 | PLATESOLVE.md | lib/plate_solve/cpp/ipv/src/* | WCS/SIP 序列化契约一致 | `docs(algorithms): sync PLATESOLVE [S2-02]` |
| S2-03 | STAR_PSF_ALGORITHMS.md | lib/dynamic_psf + lib/star_detector | 同上 | `docs(algorithms): sync STAR_PSF [S2-03]` |
| S2-04 | PHOTOMETRIC_FIT.md | lib/photometric_calib/cpp/src/* | 同上 | `docs(algorithms): sync PHOTOMETRIC_FIT [S2-04]` |
| S2-05 | NOISE_ESTIMATION.md | lib/snr_estimator/cpp/src/* | snr_noise_model_v1 一致 | `docs(algorithms): sync NOISE_ESTIMATION [S2-05]` |
| S2-06 | GAIA_QUERY.md | lib/gaia_xpsd_client/src/* | RA环绕/polar prune 一致 | `docs(algorithms): sync GAIA_QUERY [S2-06]` |
| S2-07 | DRIZZLE_GEOMETRY.md | lib/healpix_db/healpix_drizzle/*.cpp | geometry cache/run-gen 一致 | `docs(algorithms): sync DRIZZLE_GEOMETRY [S2-07]` |
| S2-08 | HEALPIX_MAPPING.md | lib/common/healpix* | NESTED 唯一映射一致 | `docs(algorithms): sync HEALPIX_MAPPING [S2-08]` |
| S2-09 | PHASE2_SAMPLER.md | lib/phase2/src/sampler.cpp | control estimator 一致 | `docs(algorithms): sync PHASE2_SAMPLER [S2-09]` |
| S2-10 | UPM_SOLVER.md | lib/phase2/src/upm.cpp | frame_id 绑定/持久化一致 | `docs(algorithms): sync UPM_SOLVER [S2-10]` |
| S2-11 | REJECTION_ALGORITHMS.md | lib/phase2/src/rejection* | WBPP/归一化一致 | `docs(algorithms): sync REJECTION_ALGORITHMS [S2-11]` |
| S2-12 | INTEGRATION_ALGORITHMS.md | lib/phase2/src/integrate.cpp | + science↔algorithm 符号逐条对齐 | `docs(algorithms): sync INTEGRATION_ALGORITHMS [S2-12]` |

每项 **输入/输出/证据/依赖** 同 S1 模式；**依赖**: `S1 Gate`，S2 内 science↔algorithm 符号对齐在 S2-12 集中校验。

**S2 Gate**: 12 份 algorithm 文档 9/9 0 broken，无冻结不一致 legacy 描述，伪代码-源码入口抽查一致。

---

## 4. 阶段 S3 — 项目/工程/代码开发文档整理（12 commits, H+M+E）

### QA-V19R8-S3-01 — ARCHITECTURE 概览对齐（H+M+E）

- **输入**: `docs/architecture/ARCHITECTURE.md` + `MODULE_MAP/DATA_FLOW/PIPELINE` + `lib/*` 实际结构
- **步骤**: 校正概览与 MODULE_MAP/DATA_FLOW/PIPELINE 一致性，13 模块总览与 `lib/*` 实际一致
- **输出**: `ARCHITECTURE.md` 修订；**验收**: 概览与分架构文档无矛盾；**证据**: `evidence/QA-V19R8-S3-01/` | **Commit**: `docs(arch): align ARCHITECTURE overview [S3-01]`

### QA-V19R8-S3-02 — MODULE_MAP + DEPENDENCY_RULES（H+M+E）

- **输入**: `MODULE_MAP.md` + `DEPENDENCY_RULES.md` + `lib/*` include 实测
- **步骤**: 校验 13 模块职责/接口/依赖方向无环，与 `lib/*/CMakeLists.txt` + `#include` 实测一致
- **输出**: 两份修订；**验收**: 依赖无环，machine 一致 | **Commit**: `docs(arch): align MODULE_MAP+DEPENDENCY [S3-02]`

### QA-V19R8-S3-03 — DATA_FLOW + PIPELINE + OWNERSHIP + THREADING（H+M+E）

- **输入**: `DATA_FLOW.md/PIPELINE.md/OWNERSHIP_AND_LIFETIME.md/THREADING_MODEL.md` + `lib/orchestrator/phase2/healpix_drizzle`
- **步骤**: 数据流与 orchestrator 实际管线一致；所有权/生命周期与 RAII/原子写一致；线程模型与实际线程池/局部缓存一致
- **输出**: 四份修订；**验收**: 数据流/线程/所有权与实现一致 | **Commit**: `docs(arch): align DATA_FLOW/PIPELINE/OWNERSHIP/THREADING [S3-03]`

### QA-V19R8-S3-04 — 契约与策略文档（H+M+E）

- **输入**: `DATA_SEMANTICS.md/PUBLIC_API.md/ERROR_MODEL.md/IO_AND_ATOMICITY.md/CACHE_POLICY.md/COMPATIBILITY_POLICY.md/PERFORMANCE_MODEL.md` + `lib/*` 头文件/错误码/原子写/版本
- **步骤**: PUBLIC_API 与头文件 machine 一致；ERROR_MODEL ↔ `orchestrator.h` 全量一致；IO 原子写/缓存/兼容/性能与实现一致
- **输出**: 七份修订；**验收**: PUBLIC_API machine 一致，错误码全量一致 | **Commit**: `docs(arch): align contracts+policies [S3-04]`

### QA-V19R8-S3-05 — L4 13项标准对齐（H+M+E）

- **输入**: `docs/standards/*` 13项 + `lib/*` 实测违规清单
- **步骤**: 逐标准校验 CODE/COMMENT/NUMERIC/API/C_ABI/CONCURRENCY/ERROR/IO/LOGGING/TEST/BENCHMARK/DOCUMENTATION/RELEASE 与实现一致，清过期条款
- **输出**: standards 修订；**验收**: 无过期条款，与 grep 违规清单对齐 | **Commit**: `docs(standards): align 13 standards [S3-05]`

### QA-V19R8-S3-06 — L5 13份模块文档（H+M+E）

- **输入**: `docs/modules/*.md` 13份 + `lib/*` 实际接口/线程/所有权/测试
- **步骤**: 按固定模板（职责/接口/数据含义/线程/所有权/错误/测试）逐模块对齐，Public API 头文件/extern C guards 齐全
- **输出**: 13 份修订；**验收**: 模板完整，与实现一致 | **Commit**: `docs(modules): align 13 modules [S3-06]`

### QA-V19R8-S3-07 — 代码开发文档：Public API + 函数签名 + 数据契约 + 错误码（H+M+E）

- **输入**: `docs/contracts/PUBLIC_API.md` + `lib/*/include/*.h` + `ERROR_MODEL.md` + `orchestrator.h`
- **步骤**: 头文件/extern C/C ABI guards 清单；每个 exported 函数 注 输入/输出/所有权/error_msg/error_capacity/线程；FITS/HiPS/UPM/frame_id 数据契约；ERROR_MODEL ↔ orchestrator.h 全量错误码表
- **输出**: `PUBLIC_API.md` + `ERROR_MODEL.md` + `docs/development/API_REFERENCE.md`（如需）修订；**验收**: 头文件/符号/错误码全量一致 | **Commit**: `docs(dev): code API/contract/error_code [S3-07]`

### QA-V19R8-S3-08 — 所有权/线程/IO/性能/兼容 开发文档（H+M+E）

- **输入**: `OWNERSHIP_AND_LIFETIME.md/THREADING_MODEL.md/IO_AND_ATOMICITY.md/PERFORMANCE_MODEL.md/COMPATIBILITY_POLICY.md` + `lib/phase2/astro_image_io/healpix_drizzle`
- **步骤**: frame_id 绑定与原子写契约显式；线程模型与 phase2/drizzle/orchestrator 一致；性能/兼容与 BASELINE/TRACEABILITY release_gate 一致
- **输出**: 五份修订；**验收**: 与实现一致 | **Commit**: `docs(dev): ownership/threading/IO/perf/compat [S3-08]`

### QA-V19R8-S3-09 — PROJECT_STATE + CURRENT_TASK 迁移（H+M+E）

- **输入**: `工程控制/control/PROJECT_STATE.yaml` + `CURRENT_TASK.md` + HEAD + Spec
- **步骤**: 迁移 `PROJECT_STATE.yaml` 到 V19R8 对齐 HEAD；`CURRENT_TASK.md` 同步到 S3 阶段
- **输出**: 两文件修订；**验收**: 与 HEAD/ Spec 一致 | **Commit**: `chore(control): migrate PROJECT_STATE to V19R8 [S3-09]`

### QA-V19R8-S3-10 — DECISION + RISK 更新（H+E）

- **输入**: `DECISION_REGISTER.md` + `RISK_REGISTER.csv` + S0-S2 决策与风险
- **步骤**: 追加 QA-V19R8 决策；更新风险关闭/新增（R-QA-01..08）
- **输出**: 两文件修订；**验收**: 可追溯 | **Commit**: `chore(control): update DECISION+RISK [S3-10]`

### QA-V19R8-S3-11 — MASTER_TASK + TRACEABILITY 补齐（M+E）

- **输入**: `MASTER_TASK_REGISTER.csv` + `docs/TRACEABILITY.csv` + S0-S3 增量
- **步骤**: 追加 QA-V19R8 段（S0-S6 90+10 行）；TRACEABILITY 补齐 V19R7 增量 ~12-16 行；machine 9/9 全量校验
- **输出**: 两文件修订 + `machine_consistency_s3.json`；**验收**: 0 broken | **Commit**: `chore(control): MASTER_TASK+TRACEABILITY [S3-11]`

### QA-V19R8-S3-12 — RELEASE_STATUS + KNOWN_LIMITATIONS + CHANGELOG + DEVELOPER_GUIDE + README-DOCS 同步（H+E）

- **输入**: `docs/RELEASE_STATUS.md/KNOWN_LIMITATIONS.md/CHANGELOG.md/DEVELOPER_GUIDE.md/README-DOCS.md` + HEAD
- **步骤**: 同步到 V19R8口径（`PRE_RELEASE_ENGINEERING_FOUNDATION=V19R8` / `FINAL_REAL_DATA_VALIDATION=PENDING`）；CHANGELOG 追加 V19R8 条目
- **输出**: 五文件修订；**验收**: 口径一致 | **Commit**: `docs(release): sync to V19R8 [S3-12]`

**S3 Gate**: L3 12+2 + L4 13 + L5 13 + 项目/工程 9/9 0 broken；PUBLIC_API machine 一致；PROJECT_STATE 与 TRACEABILITY 双校验 0 broken。

---

## 5. 阶段 S4 — 精细化代码审查（Subagent精查，10 commits, 只读不改, H+M+E）

> Subagent 分域并发 ≤4；按域分片：calibration/plate_solve/gaia/dynamic_psf/star/photometric/snr/drizzle/orchestrator/phase2/acr/astro_image_io/common/standards；分类九类：正确性/数值安全/内存生命周期/错误处理/并发安全/性能/命名/注释/C ABI/API hygiene；分级 P0/P1/P2

### QA-V19R8-S4-01 — 机器复扫 + 文件审计（M+E）

- **目标**: 产出 `machine_consistency_before.json` + `file_audit_before.json` + `standards_violations.json` + `audit_stats.json`
- **输入**: 全仓文件清单 + `tools/docs_machine_consistency.py` (9 checks) + `tools/file_audit` + `grep` 13标准
- **步骤**: 跑 machine 9/9；file_audit 713 文件；grep 扫描 CODE/NUMERIC/CONCURRENCY/C_ABI/ERROR/IO/LOGGING/COMMENT 违规；分类 P0/P1/P2 与九类
- **输出**: `reports/v19r8_quality/machine_consistency_before.json` + `file_audit_before.json` + `standards_violations.json` + `audit_stats.json`
- **验收**: 覆盖 713/713，违规可定位文件/行；**证据**: `evidence/QA-V19R8-S4-01/` | **Commit**: `chore(qa): file audit + machine before S4 [S4-01]`

### QA-V19R8-S4-02 — calibration/plate_solve 域精查（H+E）

- **范围**: `lib/calibration + lib/plate_solve/cpp/ipv` vs SCIENCE/ALGORITHM/ARCHITECTURE 对应段
- **重点**: 窗口/数值/错误；WCS/SIP/CRPIX/数值/坐标契约 A/B/C
- **产出**: `reports/v19r8_quality/audit_findings_calibration_platesolve.md` (P0/P1/P2 + 文件/行 + TR 合同) | **Commit**: `docs(qa): audit calibration+platesolve [S4-02]`

### QA-V19R8-S4-03 — gaia/dynamic_psf/star/photometric 域精查（H+E）

- **范围**: `lib/gaia_xpsd_client + lib/dynamic_psf + lib/star_detector + lib/photometric_calib`
- **重点**: RA环绕/polar prune/zero/dangling/缓存/并发；PSF 质量/A/mad/eccentricity；C ABI guards/响应曲线
- **产出**: `audit_findings_gaia_psf_photometric.md` | **Commit**: `docs(qa): audit gaia+psf+photometric [S4-03]`

### QA-V19R8-S4-04 — snr_estimator 域精查（H+E）

- **范围**: `lib/snr_estimator` vs `NOISE_MODEL/UNCERTAINTY_AND_COVARIANCE` + TRACEABILITY SCI-NOISE-*
- **重点**: 三层模型/k_corr=1.4/α²v/ivar 语义/SIP variance/协方差未建模
- **产出**: `audit_findings_snr.md` | **Commit**: `docs(qa): audit snr [S4-04]`

### QA-V19R8-S4-05 — healpix_drizzle/common 域精查（H+E）

- **范围**: `lib/healpix_db/healpix_drizzle + lib/common` vs DRIZZLE/HEALPIX_MAPPING
- **重点**: 几何缓存/run generation/false_negative=0/variance/operation_counts/HEALPix NESTED/crypto
- **产出**: `audit_findings_drizzle.md` | **Commit**: `docs(qa): audit drizzle+common [S4-05]`

### QA-V19R8-S4-06 — orchestrator/phase2/acr 域精查（H+E）

- **范围**: `lib/orchestrator + lib/phase2 + lib/acr` vs PHASE2_UPM/INTEGRATION/REJECTION
- **重点**: UPM 权重/ivar/frame_id 绑定/持久化/排异归一化/large-scale/integration状态机/ACR dormant/mode2
- **产出**: `audit_findings_phase2.md` | **Commit**: `docs(qa): audit orchestrator+phase2+acr [S4-06]`

### QA-V19R8-S4-07 — astro_image_io 域精查（H+E）

- **范围**: `lib/astro_image_io + lib/orchestrator` vs DATA_SEMANTICS/COMPATIBILITY/PIPELINE
- **重点**: HiPS signal/support/ivar/原子写/hierarchy/FP64/PipelineFrame/UPM sparse/dense
- **产出**: `audit_findings_io.md` | **Commit**: `docs(qa): audit io [S4-07]`

### QA-V19R8-S4-08 — architecture/standards 域精查（H+E）

- **范围**: `docs/architecture/* + docs/standards/* 13项 + docs/modules/* 13份` vs `lib/*` 实测
- **重点**: 13标准/COMMENT/CODE/CONCURRENCY/ERROR/IO/LOGGING/TEST/模块模板合规
- **产出**: `audit_findings_architecture_standards.md` | **Commit**: `docs(qa): audit arch+standards [S4-08]`

### QA-V19R8-S4-09 — 总表汇总（H+E）

- **目标**: 汇总分表为 `audit_findings.md` 总表 + `audit_stats.json` + 优先级排序（按 S5 顺序）
- **输入**: S4-02..08 分表 + `audit_stats.json` + `machine_consistency_before.json`
- **步骤**: 合并去重，按 P0/P1/P2 三级 + 九类分类，关联合同 ID，按 S5 模块顺序排序
- **输出**: `reports/v19r8_quality/audit_findings.md` + `audit_stats.json` 更新
- **验收**: 总表覆盖全部域，P0 清单明确；**证据**: `evidence/QA-V19R8-S4-09/` | **Commit**: `docs(qa): audit findings summary [S4-09]`

### QA-V19R8-S4-10 — P0 冻结（H+E）

- **目标**: P0 清单经 PM 确认冻结，S5 范围锁定，S4 证据包齐全无代码修改
- **输入**: `audit_findings.md` + `audit_stats.json` + PM 确认
- **步骤**: PM 冻结 P0 清单；S5 范围锁定；校验 S4 10 commits 均只读无代码修改
- **输出**: `reports/v19r8_quality/p0_frozen.md` + `evidence/QA-V19R8-S4-10/`
- **验收**: P0 清单已冻结；**Commit**: `docs(qa): P0 frozen [S4-10]`

**S4 Gate**: 8分表+总表+stats 齐全，P0 清单已冻结，方可进 S5。S4 阶段禁止任何代码/文档修改（只读审计）。

---

## 6. 阶段 S5 — 硬性问题修改·测试·Review闭环（28 commits, H+M+E）

> P0必改/P1择改；每 commit 单一模块/单一目的、最小修改、关联合同ID + evidence 四件套 + 受影响模块增量 ctest；核心算法改动同步科学/算法/测试；失败立即 BLOCKED_REPORT.md

### S5-代码硬性问题（20c）

| ID | 模块 | 内容 | 关键标准 | 验收 | Commit |
|---|---|---|---|---|---|
| S5-01 | common | HEALPix NESTED 唯一映射去重 + crypto SHA-256 唯一实现 | CODE/NUMERIC | 单一映射，无重复 | `fix(common): ... [S5-01]` |
| S5-02 | astro_image_io | HiPS 读写/原子写/hierarchy | IO/C_ABI | 契约一致 | `fix(aio): ... [S5-02]` |
| S5-03 | astro_image_io | FP64/FP32 双精度 | NUMERIC | 与 DATA_SEMANTICS 一致 | `fix(aio): ... [S5-03]` |
| S5-04 | astro_image_io | PipelineFrame/dataflow 契约 | CONCURRENCY | 与 THREADING 一致 | `fix(aio): ... [S5-04]` |
| S5-05 | astro_image_io | UPM sparse/dense 容器 | OWNERSHIP | 与 phase2 绑定一致 | `fix(aio): ... [S5-05]` |
| S5-06 | calibration | 窗口/数值/错误收口 | NUMERIC/ERROR | 窗口/溢出收口 | `fix(cal): ... [S5-06]` |
| S5-07 | plate_solve/ipv | WCS/SIP 序列化 | NUMERIC/IO | 与 V2 契约一致 | `fix(ipv): ... [S5-07]` |
| S5-08 | plate_solve | CRPIX/坐标契约 A/B/C | CODE | 验证对齐 | `fix(platesolve): ... [S5-08]` |
| S5-09 | gaia_xpsd_client | RA环绕/polar prune/缓存/并发 | ALGORITHM/CONCURRENCY | 极区/竞态收口 | `fix(gaia): ... [S5-09]` |
| S5-10 | dynamic_psf | PSF 质量/数值 | NUMERIC | A/mad/偏心收口 | `fix(dpsf): ... [S5-10]` |
| S5-11 | star_detector | SDET 阈值/数值 | NUMERIC | 阈值一致 | `fix(sdet): ... [S5-11]` |
| S5-12 | photometric_calib | C ABI guards + spectrum | C_ABI/NUMERIC | guards/数值一致 | `fix(pc): ... [S5-12]` |
| S5-13 | snr_estimator | Noise 合同 ivar 语义 | CODE | ivar 语义一致 | `fix(snr): ... [S5-13]` |
| S5-14 | snr_estimator | SIP variance/k_corr | NUMERIC | 域收口 | `fix(snr): ... [S5-14]` |
| S5-15 | healpix_drizzle | geometry cache/run-gen/线程局部 | CACHE/CONCURRENCY | 缓存/竞态正确 | `fix(drizzle): ... [S5-15]` |
| S5-16 | healpix_drizzle | 方差/operation_counts α²v | NUMERIC/LOGGING | α²v 正确 | `fix(drizzle): ... [S5-16]` |
| S5-17 | orchestrator | 错误表/C++17/日志/路径 | ERROR/CODE | 与 ERROR_MODEL 一致 | `fix(orch): ... [S5-17]` |
| S5-18 | phase2 | UPM 持久化 frame_id_by_index 原子写 | IO/OWNERSHIP | 原子写+绑定 | `fix(phase2): ... [S5-18]` |
| S5-19 | phase2 | ivar/support/zero-weight 状态机 | CODE | 状态机正确 | `fix(phase2): ... [S5-19]` |
| S5-20 | phase2/acr | 排异/large-scale + ACR dormant | ALGORITHM/ARCH | 语义冻结/边界正确 | `fix(phase2): ... [S5-20]` |

每项 **输入**: 对应 P0/P1 问题行 + 关联 TRACEABILITY 合同 + 实现文件；**步骤**: 最小修改单文件/单域，关联合同 ID；**输出**: 代码修订；**验收**: 受影响模块 ctest PASS，无科学语义改动（否则走等价门）；**证据**: `evidence/QA-V19R8-S5-NN/`；**依赖**: `S4 P0冻结`，同层内并行≤4。

### S5-测试与Review（8c）

| ID | 内容 | 工具 | 验收 | 证据/Commit |
|---|---|---|---|---|
| S5-21 | 增量 ctest（每批次） | ctest | 受影响模块 PASS | `evidence/QA-V19R8-S5-21/` |
| S5-22 | noise_model_science_test(SNR-001..015) + variance_propagation_test | ctest | 全 PASS | `evidence/...S5-22/` |
| S5-23 | phase2_synthetic_gate(82项) + pipeline_frame_contract+dataflow_fuzz | ctest | 全 PASS | `evidence/...S5-23/` |
| S5-24 | 编译告警矩阵 `-Wall -Wextra -Wpedantic` | build log | 0 first-party warning | `warnings.log` + `evidence/...S5-24/` |
| S5-25 | hygiene + file_audit + machine 9/9 | hygiene/file_audit/machine | 0 violation + 713/713 + 0 broken | `evidence/...S5-25/` |
| S5-26 | WSL ASan/UBSan 矩阵 | sanitizer_matrix.md | 0 错误（MinGW 例外如实） | `reports/v19r8_quality/sanitizer_matrix.md` |
| S5-27 | 代表帧冒烟 GC5+V20 LUM + 性能 vs BASELINE <5% | run logs + BASELINE.md | 端到端 PASS + <5% 回归 | `evidence/...S5-27/` |
| S5-28 | Fresh Reviewer + Repository Auditor 双签 | review_*.md | VERDICT:PASS 双签 | `reports/v19r8_quality/review_*.md` |

**S5 Gate**: P0 清零；全量回归+hygiene+machine+ASan/UBSan+冒烟+性能全绿；双签通过。未绿不进 S6。

---

## 7. 阶段 S6 — 接口与命名质量优化（14 commits, H+M+E, 文档驱动）

> 命名统一：CODE_STANDARD（snake_case/C API前缀/常量/错误码）、COMMENT_STANDARD（文件头/函数头/行锚点/禁止裸1e-6/轮次词）、API/ABI（头文件契约/稳定性/版本）、Docs→Code→Tests 一致

| ID | 范围 | 内容 | 验收 | Commit |
|---|---|---|---|---|
| S6-01 | standards | CODE_STANDARD 命名章节与 `lib/*` 实测命名统一 | snake_case/UPPER_SNAKE/前缀一致 | `quality(standards): naming CODE [S6-01]` |
| S6-02 | standards | COMMENT_STANDARD 与全仓文件头/函数头/行锚点一致 | 裸1e-6/轮次词清零 | `quality(standards): naming COMMENT [S6-02]` |
| S6-03 | contracts | PUBLIC_API ↔ `lib/*/include/*.h` + extern C guards | 头文件/符号全量一致 | `quality(api): PUBLIC_API guards [S6-03]` |
| S6-04 | architecture | ERROR_MODEL ↔ `orchestrator.h` ERR-* 全量 | 错误码命名统一 | `quality(error): ERROR_MODEL [S6-04]` |
| S6-05 | common/healpix | 类型/常量/函数命名统一 | 前缀/常量一致 | `quality(common): naming [S6-05]` |
| S6-06 | astro_image_io | 接口/函数/类型命名统一 | aio_ 前缀一致 | `quality(aio): naming [S6-06]` |
| S6-07 | calibration/plate_solve/gaia | 命名统一 | 前缀一致 | `quality(cal+ipv+gaia): naming [S6-07]` |
| S6-08 | dynamic_psf/star/photometric | 命名统一 | 前缀一致 | `quality(psf+sdet+pc): naming [S6-08]` |
| S6-09 | snr_estimator | 命名统一 snr_* / ivar | 语义一致 | `quality(snr): naming [S6-09]` |
| S6-10 | healpix_drizzle | 命名统一 | 前缀一致 | `quality(drizzle): naming [S6-10]` |
| S6-11 | orchestrator/phase2/acr | 命名统一 p2_* / 状态机/错误码 | 前缀一致 | `quality(phase2): naming [S6-11]` |
| S6-12 | 全仓 | Docs→Code→Tests 命名一致性复核 | TRACEABILITY 符号与源码/测试一致 | `quality(trace): Docs→Code→Tests [S6-12]` |
| S6-13 | 全仓 | hygiene 复扫 CODE/COMMENT 0 violation + file_audit 713/713 | 0 violation + 0 UNREVIEWED | `chore(qa): hygiene 0 violation [S6-13]` |
| S6-14 | 全仓 | 编译复扫 `-Wall -Wextra -Wpedantic` 0 warning | 0 first-party warning | `chore(qa): warnings 0 [S6-14]` |

每项 **输入**: 对应标准/契约/模块 + S4 命名/注释类 P1/P2 问题行；**步骤**: 文档驱动重命名（先更文档再改代码），小步提交，禁止无文档支撑的重命名；**输出**: 代码/文档修订；**验收**: `file_audit`/`hygiene`/`warnings` 门全绿；**证据**: `evidence/QA-V19R8-S6-NN/`；**依赖**: `S5 Gate`。

**S6 Gate**: CODE/COMMENT 0 violation；713/713 0 UNREVIEWED；0 warning；Docs→Code→Tests 命名一致。

---

## 8. 全局 Gate（阻断发布，任一 FAIL → BLOCKED_REPORT.md）

| 门 | 阈值 | 证据 |
|---|---|---|
| G-QA-01 文档权威链 | Wiki→L1→L2→L3→L4→L5 machine 9/9 0 broken | `reports/v19r8_quality/machine_consistency_after.json` |
| G-QA-02 追溯 | TRACEABILITY ~80行全 VERIFIED 0 broken | `docs/TRACEABILITY.csv` |
| G-QA-03 审计 | 713/713 0 UNREVIEWED | `file_audit_after.json` |
| G-QA-04 注释 | COMMENT hygiene 0 violation | `comment_hygiene.json` |
| G-QA-05 编译 | 0 first-party warning | `warnings.log` |
| G-QA-06 科学测试 | SNR-001..015 + DRZ variance + phase2 82项 全 PASS | `ctest.log` |
| G-QA-07 消毒 | WSL ASan/UBSan 0 错误 | `reports/v19r8_quality/sanitizer_matrix.md` |
| G-QA-08 文档同步 | SCIENCE_FREEZE/RELEASE/CHANGELOG/TRACEABILITY 同步 | `docs/*` diff |
| G-QA-09 留痕 | 每 commit 四件套 + Reviewer/Auditor | `evidence/QA-V19R8-*/` |
| G-QA-10 干净HEAD | git status clean + 可复现 build | `git status` + build log |

---

## 9. 证据与提交规范

- **证据目录**: `reports/v19r8_quality/`（audit 分表+总表+stats + machine_consistency_*.json + sanitizer_matrix + review_*.md）+ `evidence/QA-V19R8-*/` 四件套 + `run/logs/` + `self_review/roundN/` + `SHA256SUMS.txt`
- **Commit 模板**: `type(scope): summary [TR-ID] [Sxx-NN]` 例 `fix(drizzle): correct α²v variance [SCI-DRZ-014] [S5-16]`；关联 TRACEABILITY 合同 ID
- **类型**: docs/science|algorithms|architecture|code|project|chore|quality；单目的 commit（AGENTS.md）
- **留痕**: 每 commit 含 TASK/TEST/EVIDENCE/REVIEW 摘要；Reviewer/Auditor 独立双签
- **可恢复**: 执行命令 timeout + log，超时可恢复；阻塞立即 `BLOCKED_REPORT.md`

---

## 10. 风险与依赖

| 风险 | 对策 | 责任 |
|---|---|---|
| R-QA-01 文档滞后扩大 | S3 12c 单独，先迁 PROJECT_STATE，再改 TRACEABILITY；每阶段 machine 增量校验 | resident:project |
| R-QA-02 科学等价误改 | 走 SCIENCE_FREEZE 等价门，否则仅文档/注释/错误收口；Auditor 卡点 | resident:science |
| R-QA-03 测试环境差异 | MinGW 无 ASan 用 WSL 覆盖；GUI smoke 标记 KNOWN_LIMITATIONS | resident:code |
| R-QA-04 范围蔓延 | CODE_STANDARD 禁 cosmetic；S5 最小修改；Auditor 卡点 | Auditor |
| R-QA-05 进度拖延 | 每 10c checkpoint 汇报；S0→S6 可视化 | PM |
| R-QA-06 ACR 干扰 | ACR dormant 不合 main，仅文档占位 | resident:architecture |
| R-QA-07 Wiki 分叉 | S0 设 Wiki 唯一权威，矛盾先修 Wiki；machine 校验 Wiki→L1 | resident:wiki |
| R-QA-08 命名风暴 | S6 文档驱动命名，禁止无文档支撑重命名；分模块小步提交 | resident:code |

---

## 11. 时间与运行

- **地点**: vm-bj `/home/lighthouse/Astro CS Database`
- **模式**: 持续运行，未完成不结束；每 10 commits 一 checkpoint 向用户汇报；阻塞立即 BLOCKED_REPORT.md
- **分支**: main（ACR 保持 `feature/astrocompute-runtime` 独立）
- **日志**: 所有长任务 timeout + `run/logs/` 可恢复

## 12. 参考

- `docs/README-DOCS.md` / `docs/validation/SCIENCE_FREEZE.md` / `docs/standards/*` / `docs/architecture/*` / `docs/contracts/*` / `docs/modules/*` / `docs/TRACEABILITY.csv` / `工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md`（V19R7 175L）/ `工程控制/docs/27_PROGRESS_MIGRATION_SPEC.md`（冻结红线/权威链/Gate/证据写法参考）/ `AGENTS.md`
