# 29 质量优化 V19R7 Spec — 四层统一与代码质量闭环

> 版本: V19R7-DRAFT 2026-08-21 | 目标: 约 100 commit | 模式: 审查→改正→测试→校核 | 基线: V19R6R2-W1 (HEAD 2767874) | 地点: vm-bj Linux

## 1. 背景与问题陈述

### 1.1 当前阶段定位
- **链路验证**: Phase1 单帧 FITS→HiPS（variance/ivar）与 Phase2 UPM/排异/积分→马赛克在合成 + 小规模真实数据上已打通，V17 `ASTROCS_FOUNDATION_FINAL_FREEZE=PASS`，V19R2 `PRE_RELEASE_ENGINEERING_FOUNDATION=PASS`。
- **质量未达标**: `科学文档（L1）≠ 算法文档（L2）≠ 架构文档（L3）≠ 项目文档（L0）≠ 代码（lib/）` 四层未统一；`CHANGELOG/RELEASE_STATUS/PROJECT_STATE/TRACEABILITY` 滞后于 `HEAD V19R6R2-W1`；代码债（命名/数值/C ABI/并发/错误/日志/注释）未收口。
- **决策**: 冻结语义不改，下一阶段不冲 BASS 全量，改为本 Spec 定义的 **质量优化闭环**，留痕约 100 commit 后再签 `FINAL_REAL_DATA_VALIDATION`。

### 1.2 权威链
```
Scientific Requirement → Scientific Definition(science/*.md)
  → Algorithm Contract(algorithms/*.md)
  → Architecture(architecture/*.md + contracts/*.md + modules/*.md)
  → Implementation Standard(standards/*.md)
  → Source(lib/*) → Test → Diagnostics → Release Acceptance
```
唯一追溯矩阵 `docs/TRACEABILITY.csv`，机器校验 `tools/docs_machine_consistency.py`，文档体系 `docs/README-DOCS.md L0-L5`。

### 1.3 冻结红线（不可改）
- V17 冻结: `PHASE1_BASE_ALGORITHMS / PHASE2_BASE_ALGORITHMS / REJECTION_SEMANTICS / WBPP_AUTO_POLICY / INTEGRATION_CONTRACT / BASE_API_CONTRACT / CROSS_STAGE_CONTRACTS / HIPS_BROWSER_BASE / PERFORMANCE_BASELINE`（见 `docs/validation/SCIENCE_FREEZE.md`）
- V11 HiPS 几何/序列化/hierarchy
- 科学等价门：任何算法语义改动需 C/M 逐位或数值等价 + 回归集，否则仅文档/注释/错误/日志收口

## 2. 目标与非目标

### 2.1 目标
1. **四层统一**: 每条 science requirement → algorithm → architecture → module → public_api → implementation_files/symbols → test_ids → diagnostics → error_codes → release_gate 全链可追溯，0 broken
2. **代码质量**: 全仓 `-Wall -Wextra -Wpedantic` 0 first-party warning；`docs/standards/` 13 项标准合规；`comment hygiene 0 violation`；713/713 文件审计 0 UNREVIEWED
3. **测试闭环**: `snr_estimator/noise_model_science_test(SNR-001..015)` + `healpix_drizzle/variance_propagation_test` + `phase2/phase2_synthetic_gate(82项含 PR-UPM-001..010)` + `astro_image_io/pipeline_frame_contract_test+dataflow_fuzz` + `WSL ASan/UBSan 矩阵` 全绿
4. **项目文档同步**: `PROJECT_STATE.yaml / CURRENT_TASK.md / DECISION_REGISTER.md / RISK_REGISTER.csv / MASTER_TASK_REGISTER.csv / RELEASE_STATUS.md / KNOWN_LIMITATIONS.md / CHANGELOG.md / DEVELOPER_GUIDE.md / TRACEABILITY.csv` 同步到同一版本（V19R7）
5. **留痕**: 每原子改正单一目的 commit + `evidence/QA-V19R7-*` 四件套 + 独立 Reviewer/Auditor

### 2.2 非目标
- 不新增科学语义、不改 V17 冻结算法、不引入新 HiPS 产品、不启动 BASS 全量下载
- 不做大规模 cosmetic 重构（`docs/standards/CODE_STANDARD.md` 禁止）
- 不合并 `feature/astrocompute-runtime`（ACR 保持 dormant，`docs/ACR_FOCUSED_CONTROL_PACKAGE_V4` 冻结）

## 3. 范围与对象清单

### 3.1 文档面
| 层 | 路径 | 数量 |
|---|---|---|
| L1 科学 | `docs/science/*.md` | 11 |
| L2 算法 | `docs/algorithms/*.md` | 12 |
| L3 架构 | `docs/architecture/*.md` + `docs/contracts/*.md` | 12+2 |
| L4 标准 | `docs/standards/*.md` | 13 |
| L5 模块 | `docs/modules/*.md` | 13 |
| 项目 | `docs/README-DOCS.md / RELEASE_STATUS.md / KNOWN_LIMITATIONS.md / CHANGELOG.md / DEVELOPER_GUIDE.md / docs/development/* / docs/performance/*` | ~10 |
| 工程控制 | `工程控制/control/*.yaml/csv/md` + `工程控制/docs/*` | ~15 |

### 3.2 代码面
`lib/common / lib/astro_image_io / lib/calibration / lib/plate_solve / lib/gaia_xpsd_client / lib/dynamic_psf / lib/photometric_calib / lib/snr_estimator / lib/healpix_db/* / lib/orchestrator / lib/phase2 / lib/star_detector / lib/acr` 共 13 顶级模块，约 713 文件

### 3.3 追溯面
`docs/TRACEABILITY.csv` 当前 64 行（SCI-/ALG-/DATA-/ENG-），需补齐 V19R6R2-W1 增量（7 blockers 相关合同）并与 `lib/phase2/src/upm.cpp / sampler.cpp / healpix_drizzle/* / snr_estimator/* / astro_image_io/* / orchestrator/*` 符号对齐

## 4. 方法：审查→改正→测试→校核

### 4.1 阶段 A 审查（Review，只读不改，10 commits）
- **A1 机器扫描** (2 commits): `tools/docs_machine_consistency.py` + `tools/file_audit` + `grep` 规范扫描 + `TRACEABILITY` broken 统计
- **A2 人工审计** (8 commits): 按域分片 — science / photometry+noise / drizzle+healpix / phase2+integration / calibration+platesolve / io+orchestrator / browser+acr / standards — 每片产出 `reports/v19r7_quality/audit_findings_<domain>.md` 按 P0/P1/P2 分级

**产出**: `reports/v19r7_quality/audit_findings.md` 总表 + 8 分表 + `audit_stats.json`（文件数/违规数/broken数）

### 4.2 阶段 B 改正（Correction，最小修改，~68 commits）
> 顺序科学→算法→架构→代码→项目，跨层串行，同层内并行（≤4 Resident）

**B1 科学层** (10 commits):
- B1-01..03: `SCIENCE_SCOPE / CALIBRATION / ASTROMETRY` 公式/单位/假设/失效域对齐代码
- B1-04..05: `PHOTOMETRY / PSF` 与 `photometric_calib / dynamic_psf / star_detector` 对齐
- B1-06..08: `NOISE_MODEL / UNCERTAINTY_AND_COVARIANCE / DRIZZLE` 与 `snr_estimator / healpix_drizzle` 对齐（含 variance 传播 α²v、k_corr=1.4、协方差未建模声明）
- B1-09..10: `PHASE2_UPM / INTEGRATION / REJECTION` 与 `phase2` UPM/排异/积分对齐（含 ivar 默认、zero-weight 合同、support reducer）

**B2 算法层** (12 commits):
- 每份 `docs/algorithms/*.md` 补输入/输出/前置/不变量/伪代码/复杂度/oracle 与源码入口一一对应，删除与冻结不一致的 legacy 描述

**B3 架构层** (10 commits):
- `ARCHITECTURE / DATA_FLOW / PIPELINE / MODULE_MAP / DEPENDENCY_RULES / OWNERSHIP_AND_LIFETIME / THREADING_MODEL / ERROR_MODEL / IO_AND_ATOMICITY / CACHE_POLICY / COMPATIBILITY_POLICY / PERFORMANCE_MODEL` 与 `lib/*` 实际依赖/线程/错误码/原子写对齐

**B4 代码层** (28 commits, 按模块):
- `common` 2c: HEALPix NESTED + crypto 唯一性
- `astro_image_io` 4c: HiPS 读写/原子写/契约/FP64
- `calibration` 2c: 窗口/数值/错误
- `plate_solve/ipv` 3c: WCS/SIP/数值
- `gaia_xpsd_client` 2c: zero/dangling/RA环绕
- `dynamic_psf/star_detector` 2c: PSF 质量/数值
- `photometric_calib` 2c: C ABI guards/数值
- `snr_estimator` 2c: Noise 合同/SIP variance
- `healpix_drizzle` 3c: geometry cache/run generation/方差
- `orchestrator` 2c: orchestrator.h 错误表/C++17
- `phase2` 4c: UPM 持久化/ivar/排异/积分状态机

**B5 项目层** (8 commits):
- `PROJECT_STATE.yaml / CURRENT_TASK.md / DECISION_REGISTER.md / RISK_REGISTER.csv / MASTER_TASK_REGISTER.csv` 迁移到 V19R7
- `TRACEABILITY.csv` 补齐 + `RELEASE_STATUS.md / KNOWN_LIMITATIONS.md / CHANGELOG.md / DEVELOPER_GUIDE.md` 同步

### 4.3 阶段 C 测试（Test，改后必测，8 commits）
- C1: 受影响模块增量测试（每 B 批次）
- C2: 全量回归 `noise_model_science_test + variance_propagation_test + phase2_synthetic_gate + pipeline_frame_contract_test + dataflow_fuzz`
- C3: 编译告警矩阵 `-Wall -Wextra -Wpedantic` 0 warning
- C4: `comment hygiene` + `file_audit 713/713`
- C5: `tools/docs_machine_consistency.py` 0 broken
- C6: WSL ASan/UBSan 矩阵（g++15，MinGW 无 ASan 如实记录）
- C7: 代表帧冒烟（GC 5帧 + Victory 20帧 LUM，不跑 BASS 全量）
- C8: 性能快照（Drizzle/Phase2/Browser，与 BASELINE 对比 <5% 回归）

### 4.4 阶段 D 校核（Verification，6 commits）
- D1: Fresh Reviewer 全量复核
- D2: Repository Auditor 逐文件抽样
- D3: `self_review/roundN` 记录
- D4: `reports/v19r7_quality/` 证据包 + `run/logs/` + `SHA256SUMS`
- D5: 单一干净 HEAD 校验（`git status` clean，可复现 build）
- D6: 发布口径更新（`RELEASE_STATUS.md` 仍 `FINAL_REAL_DATA_VALIDATION=PENDING`，仅升工程地基到 V19R7）

## 5. 任务分解与依赖图（约 100 commits）

```
A1(2) → A2(8) → B1(10) → B2(12) → B3(10) → B4(28) → B5(8) → C(8) → D(6) = 92
+ 8 缓冲/修复 = ~100
```

依赖规则：
- A 完成前不进 B；B 每层完成后才进下一层；C 每 B 批次后增量跑，B5 后全量；D 依赖 C 全绿
- 每 commit 单一目的、关联 TRACEABILITY 合同 ID、含 `evidence/QA-V19R7-XXX` 四件套
- 并行上限 4 Resident：`resident:science / resident:architecture / resident:code / resident:project`

详见 `工程控制/tasks/QA-V19R7-QUALITY-OPTIMIZATION.md` 与 `MASTER_TASK_REGISTER.csv` QA 段

## 6. 验收标准（Gate）

| 门 | 阈值 | 工具 |
|---|---|---|
| G-QA-01 四层一致 | `docs_machine_consistency.py` broken=0 | 机器 |
| G-QA-02 追溯 | `TRACEABILITY.csv` 全覆盖，63→~75 合同全 VERIFIED | 人工+机器 |
| G-QA-03 审计 | 713/713 文件 0 UNREVIEWED | file_audit |
| G-QA-04 注释 | 0 violation | hygiene |
| G-QA-05 编译 | 全仓 0 first-party warning | -Wall -Wextra -Wpedantic |
| G-QA-06 科学测试 | SNR-001..015 + DRZ variance + phase2 82项 全 PASS | ctest |
| G-QA-07 消毒 | WSL ASan/UBSan 0 错误（MinGW 例外如实记录） | sanitizer_matrix.md |
| G-QA-08 文档 | SCIENCE_FREEZE/RELEASE_STATUS/CHANGELOG/TRACEABILITY 同步 | 人工 |
| G-QA-09 留痕 | 每 commit evidence 四件套 + Reviewer/Auditor | 证据 |
| G-QA-10 干净HEAD | `git status` clean + 可复现 build | 脚本 |

任一门 FAIL 阻断发布。

## 7. 风险与对策

| 风险 | 对策 |
|---|---|
| R-QA-01 文档滞后扩大 | B5 单独 8 commits，先迁 PROJECT_STATE，再改 TRACEABILITY |
| R-QA-02 科学等价误改 | 任何算法语义改动走 SCIENCE_FREEZE 等价门，否则仅文档/注释/错误收口 |
| R-QA-03 测试环境差异 | MinGW 无 ASan 用 WSL 覆盖，Aladin GUI smoke 标记 KNOWN_LIMITATIONS |
| R-QA-04 范围蔓延 | CODE_STANDARD 禁大规模 cosmetic，Auditor 卡点 |
| R-QA-05 进度拖延 | 每 10 commits 一 checkpoint，向用户汇报 |
| R-QA-06 ACR 干扰 | ACR 保持 dormant，不合 main，仅文档占位 |

## 8. 证据与交付

- 证据目录: `reports/v19r7_quality/` + `evidence/QA-V19R7-*/` + `run/logs/` + `self_review/roundN/`
- 提交: 约 100 commits，message 关联合同 ID（如 `docs(science): sync NOISE_MODEL k_corr=1.4 [SCI-NOISE-007]`）
- 最终交付: 单一干净 HEAD + `SHA256SUMS.txt` + `AUDIT_PACK`（如需）
- 发布口径: `RELEASE_STATUS.md` 保持 `FINAL_REAL_DATA_VALIDATION=PENDING`，仅升 `PRE_RELEASE_ENGINEERING_FOUNDATION=V19R7`

## 9. 时间与运行

- 地点: `vm-bj /home/lighthouse/Astro CS Database`
- 模式: 持续运行，`goal-39fd...` 100 轮上限，未完成不结束
- 节奏: 每 10 commits 一轮汇报，阻塞立即 `BLOCKED_REPORT.md`

## 10. 参考

- `docs/README-DOCS.md` / `docs/validation/SCIENCE_FREEZE.md` / `docs/standards/*` / `docs/architecture/*` / `docs/TRACEABILITY.csv` / `工程控制/docs/27_PROGRESS_MIGRATION_SPEC.md` / `AGENTS.md`
