# 30 Wiki→代码 质量优化 V19R8 Spec — Wiki核心约束的六阶段闭环

> 版本: V19R8-DRAFT 2026-08-22 | 目标: 约 100 commits (90 +10 缓冲) | 新范式: S0→S1→S2→S3→S4→S5→S6 | 基线: V19R7 (HEAD V19R6R2-W1 延续) | 地点: vm-bj Linux `/home/lighthouse/Astro CS Database`

## 1. 背景与问题陈述

### 1.1 阶段定位
- **链路已打通**: Phase1 单帧 FITS→HiPS (variance/ivar) 与 Phase2 UPM/排异/积分→马赛克在合成+小规模真实数据已打通；V17 `ASTROCS_FOUNDATION_FINAL_FREEZE=PASS`，V19R2 `PRE_RELEASE_ENGINEERING_FOUNDATION=PASS`。
- **质量未达标**: `Wiki ≠ 科学(L1) ≠ 算法(L2) ≠ 架构(L3) ≠ 标准(L4) ≠ 模块(L5) ≠ 代码(lib/713文件) ≠ 测试` 多层未精确对齐；`TRACEABILITY.csv` 64行滞后；Wiki 作为核心约束未显式加固；P0/P1 问题未系统性精查；接口/命名风格未统一。
- **决策**: 继承 V19R7「四层统一与代码质量闭环」成果，按用户新范式重排为 **Wiki→科学→算法→项目/工程/代码文档→P0/P1精查→修改/测试/Review→接口命名统一** 六阶段闭环，冻结语义不改，不冲 BASS 全量，产出约 100 commits 后签 `PRE_RELEASE_ENGINEERING_FOUNDATION=V19R8`，`FINAL_REAL_DATA_VALIDATION` 仍 PENDING。

### 1.2 新范式六阶段
```
S0 Wiki与权威链加固 ─→ S1 科学文档精确化 ─→ S2 算法核心文档精确化 ─→ S3 项目/工程/代码开发文档整理
        ─→ S4 精细化代码审查(Subagent) ─→ S5 硬性问题修改·测试·Review闭环 ─→ S6 接口与命名质量优化
```
跨层串行（S0→S6 必须顺序），同层内并行 ≤4 Resident；每阶段 Gate 全绿方可进下一阶段；每 10 commits 一 checkpoint。

### 1.3 权威链（唯一追溯）
```
Wiki(核心约束)
 → Science L1 (docs/science/*.md 11份 SCI-*)
  → Algorithm L2 (docs/algorithms/*.md 12份 ALG-*)
   → Architecture L3 (docs/architecture/*.md 12份 + docs/contracts/*.md 2份)
    + Contracts (DATA_SEMANTICS/PUBLIC_API) + Standards L4 (docs/standards/*.md 13项)
     → Modules L5 (docs/modules/*.md 13份)
      → Source (lib/ 13模块 ~713文件) → Test (lib/*/tests + ctest) → Diagnostics (reports/ + tools/*) → Release
```
- 唯一追溯矩阵 `docs/TRACEABILITY.csv`（当前 64 行 → 目标 ~75-85 行，SCI-/ALG-/DATA-/ENG- 全 VERIFIED）。
- 机器校验 `tools/docs_machine_consistency.py` 9 checks（L0-L5 完整性 / 合同-文档-符号-测试-诊断-错误码一致性）；辅助 `tools/file_audit`、`tools/comment_hygiene`。
- 文档体系 `docs/README-DOCS.md` L0-L5 定义为唯一权威分层声明。

### 1.4 冻结红线（不可改）
- V17 冻结: `PHASE1_BASE_ALGORITHMS / PHASE2_BASE_ALGORITHMS / REJECTION_SEMANTICS / WBPP_AUTO_POLICY / INTEGRATION_CONTRACT / BASE_API_CONTRACT / CROSS_STAGE_CONTRACTS / HIPS_BROWSER_BASE / PERFORMANCE_BASELINE`（`docs/validation/SCIENCE_FREEZE.md`）。
- V11 HiPS 几何/序列化/hierarchy 不可改。
- 科学等价门: 任何算法语义改动需 **C/M逐位或数值等价 + 回归集全绿**（`noise_model_science_test`/`variance_propagation`/`phase2_synthetic_gate 82项`），否则仅限文档/注释/错误/日志收口（`docs/standards/CODE_STANDARD.md` 禁止无关 cosmetic）。
- 不新增科学语义、不跑 BASS 全量、不合 `feature/astrocompute-runtime`（ACR dormant，`docs/ACR_FOCUSED_CONTROL_PACKAGE_V4` 冻结）。

## 2. 目标与非目标

### 2.1 目标
1. **S0**: Wiki 核心约束显式化，L0-L5 体系与 `TRACEABILITY.csv` + machine 9/9 0 broken 对齐，Wiki→L1→L2→L3→L4→L5 权威链可机检。
2. **S1**: 11 份 L1 科学文档公式/单位/假设/失效域/误差逐条对齐代码，machine 0 broken。
3. **S2**: 12 份 L2 算法文档输入/输出/前置/不变量/伪代码/复杂度/oracle 逐一对应源码入口，machine 0 broken，无 legacy 残留。
4. **S3**: L3 11份+L4 13标准+L5 13模块 + 项目文档（接口/函数/数据契约/错误码/线程/所有权/IO/性能/兼容）+ 工程控制 7件套（PROJECT_STATE/RISK/DECISION/TRACEABILITY/RELEASE_STATUS/CHANGELOG/DEVELOPER_GUIDE）全部同步到 V19R8。
5. **S4**: Subagent 分域精查产出 P0/P1/P2 问题报告（分表+总表+stats.json）并冻结 P0 清单。
6. **S5**: P0 必改/P1 择改闭环，最小修改 + machine 9/9 回归 + CTest/ASan/UBSan + Fresh Reviewer/Auditor 双签。
7. **S6**: 文档驱动的接口/函数/类型/常量/错误码命名统一，`CODE_STANDARD/COMMENT_STANDARD` 0 violation，713/713 file_audit 0 UNREVIEWED，`-Wall -Wextra -Wpedantic` 0 warning。

### 2.2 非目标
- 不改 V17/V11 冻结算法与 HiPS 几何；不新增 HiPS 产品；不启动 BASS 全量下载。
- 不做大规模 cosmetic 重构；不合 ACR；不引入新科学语义。

## 3. 范围与对象清单

| 域 | 路径 | 数量 | 备注 |
|---|---|---|---|
| Wiki 核心 | `docs/README-DOCS.md` + Wiki 镜像/索引 + `docs/validation/SCIENCE_FREEZE.md` | 1 套 | S0 加固对象 |
| L1 科学 | `docs/science/*.md` | 11 | SCIENCE_SCOPE/CALIBRATION/ASTROMETRY/PHOTOMETRY/PSF/NOISE_MODEL/UNCERTAINTY/DRIZZLE/PHASE2_UPM/INTEGRATION/REJECTION |
| L2 算法 | `docs/algorithms/*.md` | 12 | CALIBRATION/PLATESOLVE/STAR_PSF/PHOTOMETRIC_FIT/NOISE_ESTIMATION/GAIA_QUERY/DRIZZLE_GEOMETRY/HEALPIX_MAPPING/PHASE2_SAMPLER/UPM_SOLVER/REJECTION/INTEGRATION |
| L3 架构 | `docs/architecture/*.md` + `docs/contracts/*.md` | 12+2 | ARCH/MODULE_MAP/DEPENDENCY/DATA_FLOW/PIPELINE/OWNERSHIP/THREADING/ERROR/IO/CACHE/COMPATIBILITY/PERFORMANCE + DATA_SEMANTICS/PUBLIC_API |
| L4 标准 | `docs/standards/*.md` | 13 | CODE/COMMENT/NUMERIC/API/C_ABI/CONCURRENCY/ERROR/IO/LOGGING/TEST/BENCHMARK/DOCUMENTATION/RELEASE (13项) |
| L5 模块 | `docs/modules/*.md` | 13 | common/astro_image_io/calibration/plate_solve/gaia_xpsd_client/dynamic_psf/photometric_calib/snr_estimator/healpix_drizzle/healpix_browser_qt/orchestrator/phase2/star_detector/acr |
| 项目文档 | `docs/DEVELOPER_GUIDE.md / docs/RELEASE_STATUS.md / docs/KNOWN_LIMITATIONS.md / CHANGELOG.md / docs/development/* / docs/performance/BASELINE.md` | ~10 | 含接口定义/函数定义/数据契约 |
| 工程控制 | `工程控制/control/*.yaml/csv/md` + `工程控制/docs/*` | ~15 | PROJECT_STATE/MASTER_TASK/RISK/DECISION/TRACEABILITY等 |
| 代码 | `lib/common / lib/astro_image_io / lib/calibration / lib/plate_solve / lib/gaia_xpsd_client / lib/dynamic_psf / lib/photometric_calib / lib/snr_estimator / lib/healpix_db/* / lib/orchestrator / lib/phase2 / lib/star_detector / lib/acr` | 13 模块 ~713 文件 |  |
| 追溯 | `docs/TRACEABILITY.csv` | 64→~80 行 | 需补 V19R7 增量 |

## 4. 方法：六阶段流水

### 4.1 依赖图与 Commit 预算

```
S0(4) → S1(10) → S2(12) → S3(12) → S4(10) → S5(28) → S6(14) = 90
+ 10 缓冲/回滚 = 100 commits
```

| 阶段 | 预算 | 性质 | 并行 | Gate |
|---|---|---|---|---|
| S0 Wiki与权威链加固 | 4c | 文档+机检 | 1 | machine 9/9 0 broken + L0-L5 索引齐 |
| S1 科学文档精确化 | 10c | 文档精确化 | ≤4 | L1 11份 0 broken |
| S2 算法核心文档精确化 | 12c | 文档精确化 | ≤4 | L2 12份 0 broken |
| S3 项目/工程/代码开发文档整理 | 12c | 文档整理 | ≤4 | L3/L4/L5 + 项目/工程 0 broken |
| S4 精细化代码审查 | 10c | 只读审计 | Subagent ≤4并发 | 分表+总表+stats + P0冻结 |
| S5 硬性问题修改·测试·Review闭环 | 28c | 最小修改+测试+Review | ≤4 Resident | P0清零+回归全绿+双签 |
| S6 接口与命名质量优化 | 14c | 命名/注释统一 | ≤4 | 0 violation + 0 warning + 713/713 |

- 跨层串行：S0 未绿不进 S1，依此类推；S4 完成前不进 S5。
- 每 commit 单一目的（`AGENTS.md`），关联 TRACEABILITY 合同 ID，含 `evidence/QA-V19R8-*` 四件套（TASK/TEST/EVIDENCE/REVIEW 摘要）。
- 并行上限 4：`resident:wiki / resident:science / resident:algorithm / resident:code / resident:project` 按域分片。

详见 `工程控制/tasks/QA-V19R8-QUALITY-OPTIMIZATION.md` 与 `MASTER_TASK_REGISTER.csv` QA-V19R8 段。

### 4.2 各阶段概要

**S0 Wiki与权威链加固 (4c)** — 输入: Wiki + `README-DOCS.md` + `TRACEABILITY.csv` + `tools/docs_machine_consistency.py` 9 checks；方法: 固化 Wiki 为 L0 核心约束，补齐 Wiki→L1→L2→L3→L4→L5 索引与 machine 锚点，修复 TRACEABILITY 断链；产出: Wiki 索引更新 + README-DOCS 同步 + machine 0 broken 初扫报告 `reports/v19r8_quality/machine_consistency_s0.json`。

**S1 科学文档精确化 (10c)** — 输入: L1 11份 + `lib/*` 实现；方法: 每份按科学模板逐条对齐代码（定义/公式/变量/单位/假设/有效域/失效域/系统与随机误差/数值精度/参考文献/ID），符号与文件行锚点一一对应；产出: 11份 science 修订 + `machine_consistency_s1.json`。

**S2 算法核心文档精确化 (12c)** — 输入: L2 12份 + `lib/*/src/*` 入口；方法: 每份补输入/输出/前置/后置/不变量/伪代码/复杂度/并行模型/数值风险/fast/reference/oracle/ID，与源码入口一一对应，删 legacy；产出: 12份 algorithm 修订 + `machine_consistency_s2.json`。

**S3 项目/工程/代码开发文档整理 (12c)** — 输入: L3/L4/L5 + `lib/*` + 工程控制 7件套；方法: 补齐接口定义/函数定义/数据契约/错误码/线程/所有权/IO/性能/兼容 + 13标准与13模块模板对齐 + PROJECT_STATE/RISK/DECISION/TRACEABILITY/RELEASE_STATUS/CHANGELOG/DEVELOPER_GUIDE 同步；产出: 架构/标准/模块/项目文档修订 + `machine_consistency_s3.json`。

**S4 精细化代码审查 (10c, 只读)** — Subagent 分域并发 ≤4，按域分片精查，产出分表+总表+stats。

**S5 硬性问题修改·测试·Review闭环 (28c)** — P0必改/P1择改，最小修改，单目的 commit + evidence 四件套 + machine 9/9 回归 + CTest/冒烟/ASan/UBSan + Fresh Reviewer/Auditor 双签。

**S6 接口与命名质量优化 (14c)** — 文档驱动的接口/函数/类型/常量/错误码命名统一，风格与注释质量收口至 0 violation。

### 4.3 阶段输入/输出/验收速查

| 阶段 | 输入 | 输出 | 验收 | 证据 |
|---|---|---|---|---|
| S0 | Wiki + README-DOCS + TRACEABILITY + machine 9 checks | Wiki 索引 + README-DOCS 修订 + TRACEABILITY ~75行 + machine_s0.json | Wiki→L1→L2 无断链，9/9 0 broken | reports/v19r8_quality/machine_consistency_s0.json |
| S1 | L1 11份 + lib/* 实现 + SCI-* 合同 | 11份 science 修订 | L1 0 broken，SCI-* 全 VERIFIED，行锚点抽查一致 | machine_consistency_s1.json |
| S2 | L2 12份 + lib/*/src/* 入口 | 12份 algorithm 修订 | L2 0 broken，伪代码-入口一致，无 legacy | machine_consistency_s2.json |
| S3 | L3/L4/L5 + lib/* + 工程控制7件套 | 架构/标准/模块/项目文档修订 | L3/L4/L5+项目/工程 0 broken，PUBLIC_API 一致 | machine_consistency_s3.json |
| S4 | 全仓 + 13标准 + lib/713文件 | 分表+总表+stats + P0冻结 | 8分表+总表+stats 齐，P0冻结 | reports/v19r8_quality/audit_findings*.md |
| S5 | P0/P1 清单 + lib/* | 代码修复 + 回归全绿 + 双签 | P0清零，回全绿，双签 | evidence/QA-V19R8-S5-*/ + sanitizer_matrix |
| S6 | S4 命名/注释 P1/P2 + CODE/COMMENT 标准 | 命名统一 + hygiene 0 violation | 0 violation, 713/713, 0 warning | file_audit_after + hygiene log |

### 4.4 工程执行纪律
- 每个 commit 前明确需求/影响范围/风险/验收标准（AGENTS.md 开发纪律）。
- 禁止破坏性 Git 操作；所有长任务 timeout + 日志可恢复（AGENTS.md Linux环境）。
- 科学定义=算法=接口=代码=测试 保持一致；核心算法改动同步三文档与测试。
- 性能优化先分析 CPU/内存/IO/等待/重复计算/复杂度，禁止无意义全量基准重跑。
- commit 保持单一目的；memory.md 记录稳定结论，logs 记录过程。

## 5. 约束详述

### 5.1 权威链与追溯规范
- Wiki 为核心约束，任何 L1-L5 与代码变更不得与 Wiki 矛盾；矛盾时以 Wiki 为准并先修 Wiki（S0 Gate）。
- `docs/TRACEABILITY.csv` 为唯一追溯矩阵，字段 `requirement_id/authority_doc/algorithm_id/module/public_api/implementation_files/implementation_symbols/test_ids/diagnostic_ids/error_codes/release_gate/status` 缺一不可；每行关联 machine 可检符号/文件/测试。
- `tools/docs_machine_consistency.py` 9 checks 必须 0 broken（含 L0-L5 引用完整性、合同-符号-测试-诊断一致性、空字段、重复 ID、孤儿合同）。
- `docs/README-DOCS.md` L0-L5 体系为权威分层声明，S0 需确保其与 Wiki 与 TRACEABILITY 一致。

### 5.2 文档精确化验收（模板字段）
- **科学 L1 每份**必须含: 定义/公式/变量表/单位/假设/有效域/失效域/系统误差/随机误差/数值精度/参考文献/合同ID；公式编号与代码实现行锚点一一对应（如 `DRIZZLE.md: α²v → drizzle_engine.cpp:xxx`）。
- **算法 L2 每份**必须含: 输入/输出/前置/后置/不变量/伪代码/复杂度/并行模型/数值风险/fast path/reference path/oracle/合同ID；伪代码与源码入口（函数名/文件）一一对应。
- **架构 L3 / 模块 L5** 按既有模板（含职责/接口/依赖/数据流/所有权/线程/错误/IO/性能/兼容）；与 `lib/*` 实际依赖/线程/错误码/原子写一致。
- 验收: machine_consistency 对应 L1/L2/L3/L5 0 broken；人工抽查符号/行锚点一致。

### 5.3 代码开发文档规范（S3）
- **Public API**: 头文件清单 `docs/contracts/PUBLIC_API.md` 与 `lib/*/include/*.h` + `extern "C"` + C ABI guards 一一对应。
- **函数签名**: 每个 exported 函数注明 输入/输出/所有权（谁分配谁释放）/ `error_msg`/`error_capacity`/线程安全性（可重入/需外同步/线程局部）。
- **数据契约**: FITS/HiPS (signal/support/ivar) / UPM (sparse/dense, frame_id 绑定) / frame_id / WCS/SIP 序列化格式。
- **错误码**: `ERROR_MODEL.md ↔ orchestrator.h` 全量一致（`ERR-P2-UPM-001` 等），退出码与 `orchestrator --help` 一致。
- **所有权/生命周期**: `OWNERSHIP_AND_LIFETIME.md` 与 RAII/原子写/close 语义一致。
- **线程模型**: `THREADING_MODEL.md` 与 `phase2/healpix_drizzle/orchestrator` 实际线程池/局部缓存一致。

### 5.4 精查规范（S4 Subagent）
- **并发**: Subagent 分域并发 ≤4；按域分片: `calibration/plate_solve/gaia/dynamic_psf/star/photometric/snr/drizzle/orchestrator/phase2/acr/astro_image_io/common/standards`（13域合并为 ≤10 审计单元）。
- **分级**: P0=阻断（语义分叉/数值错误/内存越界/泄漏/并发 data race/未定义行为）必改；P1=可观测错误处理缺失/原子性缺失/追溯缺失 择改；P2=表述/锚点/可追溯性增强 建议。
- **分类**: 正确性/数值安全/内存生命周期/错误处理/并发安全/性能/命名/注释/C ABI/API hygiene 九类。
- **产出**: `reports/v19r8_quality/audit_findings_<domain>.md` 分表 + `audit_findings.md` 总表 + `audit_stats.json`（文件数/违规数/broken数/P0/P1/P2分布）+ 冻结 P0 清单（PM 确认）。

### 5.5 整改规范（S5）
- **顺序**: 跨层串行 S4→S5→S6；同层内并行 ≤4 Resident；每 commit 单一目的、最小修改（禁止无关重构/格式化/扩大范围）。
- **职责/接口/数据含义明确**（AGENTS.md）；核心算法改动必须同步科学/算法/测试（否则仅文档/注释/错误/日志收口，走科学等价门）。
- **留痕**: `AGENTS.md` 单目的 commit + `evidence/QA-V19R8-*` 四件套（TASK/TEST/EVIDENCE/REVIEW）+ 独立 Fresh Reviewer / Repository Auditor 双签。
- **可恢复**: 执行命令必须 timeout + log（`run/logs/`），超时可恢复；失败立即 `BLOCKED_REPORT.md`。

### 5.6 测试门（S5）
- **增量**: 受影响模块 `ctest` 每 S5 批次后必跑。
- **全量回归**: `snr_estimator/noise_model_science_test(SNR-001..015)` + `healpix_drizzle/variance_propagation_test` + `phase2/phase2_synthetic_gate 82项(含 PR-UPM-001..010)` + `astro_image_io/pipeline_frame_contract_test+dataflow_fuzz` 全 PASS。
- **Hygiene**: `CODE_STANDARD/COMMENT_STANDARD` 0 violation；`file_audit 713/713` 0 UNREVIEWED；`tools/docs_machine_consistency.py` 9/9 0 broken。
- **消毒**: WSL ASan/UBSan（g++15）0 错误；MinGW 无 ASan 如实记录（`reports/v19r8_quality/sanitizer_matrix.md`）。
- **冒烟**: 代表帧端到端（GC 5帧 + Victory 20帧 LUM）PASS，不跑 BASS 全量。
- **性能**: Drizzle/Phase2/Browser vs `docs/performance/BASELINE.md` <5% 回归。
- 未绿不进 S6。

### 5.7 命名统一（S6）
- **CODE_STANDARD**: `snake_case`（函数/变量）、`UPPER_SNAKE`（常量/宏）、`p2_/aio_/ac_/snr_` 等 C API 前缀、错误码 `ERR-*`。
- **COMMENT_STANDARD**: 文件头/函数头/行锚点齐全；禁止裸 `1e-6`（须具名常量+单位）；禁止轮次词（`轮次/阶段一` 等）。
- **API/ABI**: 头文件契约稳定性（`PUBLIC_API.md` 冻结）、`extern "C"` guards、C++17 一致。
- **一致性**: Docs→Code→Tests 命名一致；9类精查中命名/注释类 P1/P2 在 S6 清零。

### 5.8 证据与交付
- **目录**: `reports/v19r8_quality/`（audit 分表+总表+stats + machine_consistency_* + sanitizer_matrix）+ `evidence/QA-V19R8-*/` 四件套 + `run/logs/` + `self_review/roundN/` + `SHA256SUMS.txt`。
- **Commit 预算**: S0 4c | S1 10c | S2 12c | S3 12c | S4 10c | S5 28c | S6 14c =90 +10 缓冲 =100；每 10 commits 一 checkpoint 向用户汇报。
- **Commit message 模板**: `type(scope): summary [TR-ID] [Sxx-NN]` 例 `docs(science): sync DRIZZLE α²v [SCI-DRZ-014] [S1-07]`；type ∈ {docs,fix,refactor,chore,quality}。
- **Gate G-QA-01..10 全绿方发布**，任一 FAIL → `BLOCKED_REPORT.md` 阻断：

| 门 | 阈值 | 工具/证据 |
|---|---|---|
| G-QA-01 文档权威链 | Wiki→L1→L2→L3→L4→L5 machine 9/9 0 broken | `machine_consistency_after.json` |
| G-QA-02 追溯 | TRACEABILITY ~80行全 VERIFIED 0 broken | `TRACEABILITY.csv` |
| G-QA-03 审计 | 713/713 0 UNREVIEWED | `file_audit_after.json` |
| G-QA-04 注释 | COMMENT hygiene 0 violation | `comment_hygiene.json` |
| G-QA-05 编译 | 全仓 0 first-party warning | `-Wall -Wextra -Wpedantic` log |
| G-QA-06 科学测试 | SNR-001..015 + DRZ variance + phase2 82项 全 PASS | `ctest.log` |
| G-QA-07 消毒 | WSL ASan/UBSan 0 错误 | `sanitizer_matrix.md` |
| G-QA-08 文档同步 | SCIENCE_FREEZE/RELEASE/CHANGELOG/TRACEABILITY 同步 | `docs/*` diff |
| G-QA-09 留痕 | 每 commit evidence 四件套 + Reviewer/Auditor | `evidence/QA-V19R8-*/` |
| G-QA-10 干净HEAD | `git status` clean + 可复现 build | `git status` + build log |

### 5.9 变更管理与回滚
- 任何跨层文档→代码语义变更需先更上游文档（Wiki→L1→L2→L3）再改代码，machine 增量校验通过方可提交。
- 每 commit 可独立 revert，不捆绑多域；回滚后 TRACEABILITY 状态回退为 TODO 并重跑 machine。
- 缓冲 10c 用于回滚/修复：S1-S3 各预留 1c，S5 预留 4c，S6 预留 2c，超限需 PM 审批并追加 BLOCKED_REPORT。

### 5.10 分工与并发约束
- Resident 并行 ≤4，同模块同文件禁止并发修改；Subagent S4 按域分片并发 ≤4，合并由单一 Resident 串行汇总。
- 跨层依赖强制串行：S0 Gate 未绿禁止 S1 任何 commit；S4 P0 未冻结禁止 S5 代码修改。
- 每 10 commits checkpoint 产出 `reports/v19r8_quality/checkpoint_N.md`（已合/待合/风险/broken 趋势）。

### 5.11 规范引用（尽可能详细，参考历史工程包）
- 本 Spec 约束写法参考 `27_PROGRESS_MIGRATION_SPEC.md` 的冻结红线/权威链/Gate/证据四段式；每任务含 输入/输出/步骤/验收/证据/Commit message 六要素（同 V19R7 历史包 175L/163L/306L）。
- 数值/并发/C ABI/错误/IO/日志/测试/文档/发布 13 标准逐条可勾选，S4 grep 扫描与 S6 hygiene 复扫闭环；AGENTS.md 单目的 commit + timeout+log 可恢复纪律贯穿全链。

## 6. 验收标准（阶段 Gate）

| 阶段 | Gate 条件 |
|---|---|
| S0 | Wiki 索引与 README-DOCS L0-L5 一致；machine 9/9 0 broken（`machine_consistency_s0.json`） |
| S1 | L1 11份 0 broken；SCI-* 全 VERIFIED |
| S2 | L2 12份 0 broken；ALG-* 全 VERIFIED；无 legacy 残留 |
| S3 | L3/L4/L5 + 项目/工程 0 broken；PUBLIC_API machine 一致；PROJECT_STATE/RISK/DECISION/TRACEABILITY 同步 |
| S4 | 分表+总表+stats 齐全；P0 清单冻结（PM 确认） |
| S5 | P0 清零；全量回归+ hygiene + machine + ASan/UBSan + 冒烟 + 性能 全绿；Reviewer/Auditor 双签 |
| S6 | CODE/COMMENT 0 violation；713/713 0 UNREVIEWED；0 warning；Docs→Code→Tests 命名一致 |

## 7. 风险与对策

| 风险 | 对策 | 责任 |
|---|---|---|
| R-QA-01 文档滞后扩大 | S3 单独 12c，先迁 PROJECT_STATE，再改 TRACEABILITY；每阶段 machine 增量校验 | resident:project |
| R-QA-02 科学等价误改 | 走 SCIENCE_FREEZE 等价门，否则仅文档/注释/错误收口；Auditor 卡点 | resident:science |
| R-QA-03 测试环境差异 | MinGW 无 ASan 用 WSL 覆盖；Aladin GUI smoke 标记 KNOWN_LIMITATIONS | resident:code |
| R-QA-04 范围蔓延 | CODE_STANDARD 禁大规模 cosmetic；S5 最小修改；Auditor 卡点 | Auditor |
| R-QA-05 进度拖延 | 每 10 commits checkpoint 汇报；S0→S6 跨层串行可视化 | PM |
| R-QA-06 ACR 干扰 | ACR dormant 不合 main，仅文档占位；S5 权重策略走 CPU canonical | resident:architecture |
| R-QA-07 Wiki 分叉 | S0 设 Wiki 唯一权威，矛盾先修 Wiki；machine 校验 Wiki→L1 引用 | resident:wiki |
| R-QA-08 命名风暴 | S6 文档驱动命名，禁止无文档支撑的重命名；分模块小步提交 | resident:code |

## 8. 时间与运行

- **地点**: `vm-bj /home/lighthouse/Astro CS Database`
- **模式**: 持续运行，未完成不结束；每 10 commits 一 checkpoint 向用户汇报；阻塞立即 `BLOCKED_REPORT.md`。
- **分支**: `main`（ACR 保持 `feature/astrocompute-runtime` 独立）。
- **日志**: 所有长任务 timeout + `run/logs/` 可恢复（AGENTS.md 工程管理）。

## 9. 术语与缩写

| 缩写 | 全称 | 说明 |
|---|---|---|
| UPM | Unified Photometric Model | Phase2 统一测光模型，权重 `quality×geom×ivar` |
| ivar | inverse variance | `1/variance`，缺失报 `ERR-P2-UPM-001` |
| k_corr | Drizzle 修正因子 | MC 校准 `1.4`，`control_variance = k_corr×(π/2)×σ_bg²/N` |
| P0/P1/P2 | 优先级 | P0阻断必改 / P1必须择改 / P2建议 |
| machine 9/9 | 机器一致性9 checks | docs_machine_consistency.py 9项全绿 0 broken |

## 10. 参考

- `docs/README-DOCS.md` / `docs/validation/SCIENCE_FREEZE.md` / `docs/standards/*` / `docs/architecture/*` / `docs/contracts/*` / `docs/modules/*` / `docs/TRACEABILITY.csv` / `工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md`（V19R7 基线，175L）/ `工程控制/docs/27_PROGRESS_MIGRATION_SPEC.md`（冻结红线/权威链/Gate/证据写法参考）/ `AGENTS.md` / `PROJECT_MANAGER.md`
