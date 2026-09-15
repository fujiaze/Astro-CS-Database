# AstroCS Changelog

## [0.11.0-alpha.2] 2026-09-15 — Current Alpha（V6 并行科学与工程实施包）

> 当前产品版本节：根 `VERSION` = `0.11.0-alpha.2`（GOV-003 唯一源；生成串
> `0.11.0-alpha.2+g<commit12>`，见 `docs/governance/VERSION_NAMESPACES.md`）。
> **状态 = NOT_READY / 未发布**：不得据此宣布发布，`alpha.N` 只在外部审核通过后提升。
> 本节记录 V6 包集成事实；历史轮次节（下方 alpha.1 及 V19R8…V12）仅供追溯，
> 不冒充当前状态（VERSION_NAMESPACES history 命名空间）。

### V6 并行科学与工程实施包（`工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/`，Wave 0–12，`scientific_change` 依各任务卡）

- **冻结口径唯一权威**：`docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json`（96 条款：
  FROZEN 39 / PENDING_OWNER_SIGNOFF 49 / OPEN 8；`baseline_head = ebefe00d`）。
- **Phase2 权重面**：生产科学模式 `{point_information, surface_gls, psfsw_robust}`；文档基线
  `{equal, pixel_ivar}`（非科学最优声明）；`psf_snr_power` **DEFERRED 且生产拒绝**（`C-004.1`）。
- **单一权重词表（C-004.3/DI-01/DI-07）**：canonical = `weight.kind` / `weight.units` /
  `weight.group_normalized` / `weight.normalization.{scope,median_target,constants_version}` /
  `weight.weight_value`；事实源 `contracts/data/v6_weight_vocabulary_v1.json`；
  生产 schema 无第三套名（`docs/contracts/DATA_SEMANTICS.md` §31.4）。
- **单位与量纲**：`signal_sb=ADU/px²`、`sb_variance_out=ADU²/px⁴`、`sb_ivar_out=px⁴/ADU²`、
  `pixel_variance_in=ADU²`、`W_info=ADU⁻²`、`Q=ADU⁻¹`、`flux=ADU`、`psfsw_robust_weight=1`；
  BUNIT 量纲可判 + Phase3 variance 二次律（`FZ-BUNIT-SEMANTICS`/`FZ-P3-BUNIT-QUADRATIC`）。
  `Q=aPᵀC⁻¹d` / `W_info=a²PᵀC⁻¹P`（无量纲前提说明）；`psfsw` **无量纲、不是 ivar/Fisher**，组内 median=1；
  final covariance 只能由实际组合系数经 `R C_in Rᵀ` 传播并输出 effective PSF。
- **文档订正（DOC-CONVERGE-001/W12）**：Phase1 算法规格 §4.2 concentration 单位由 W6 登记的错误值
  `ADU/px` 订正为 `component_flux_unit/px²`（`A_NEA=px²`），同步机器伴生 `alg_p1_001_spec.json`；
  61 份未被覆盖的 `docs/**/v6/**` 文档登记进 `docs/DOCUMENT_INDEX.yaml`（闭合 OI-04 的
  `docs_fully_covered` 红）；
  README/REVIEW 的 V6 现状与本 changelog 同步收敛。
- **验证**：V6 各任务独立 Oracle + 负向 mutation（见各任务 `evidence`）；控制包机器门
  `AGENTS-GOV` / `VERSION-CONSISTENCY` / `VERSION_NAMESPACES` 本地 rc=0。
- **未通过/未决（不得写成已发布或已通过）**：
  - **Linux CI 持续红**：`THREAD-BUDGET` FAIL（P36 回退态 `lib/core/src/module_adapters.cpp` 线程预算旁路）；
    `CTEST-REGISTRATION` FAIL（余 2 项**非 V6** IPV 残项）；`§17.12` 门 7 未满足。
  - **Windows/Fatduck**：Windows CI failure 且 Fatduck 不可达 → **AWAITING_WINDOWS_VALIDATION**；
    32/32 Windows 用例 UNAVAILABLE；`FD-F-003`（Windows 单测 0 用例）OPEN；`§17.12` 门 8 未满足。
  - **SO-05**：16w CPU mean 65.09% < 85%、p50 87.63% < 90%；`memory_growth_unbounded@16w` 未定性。
  - **REAL-SCIENCE-001**：equal/exposure/ivar 三口径为 **DOCUMENTED_BASELINE**（驱动内实现），
    非全链生产路径端到端验收；**Phase2 CLI 真实数据不可达**（`obs=0`）。
  - **F-CAR / F-AIT** legacy 投影错误待负责人授权修正；49 `PENDING_OWNER_SIGNOFF` + 8 `OPEN` 保持 fail-closed。
  - §14.5 六步发布复核逐项 NOT_MET/AWAITING → 包状态 **NOT_READY**；发布决定权只属项目负责人（宪章 §17.12）。

---

## [0.11.0-alpha.1] 2026-09-02 — GOV-005 文档收敛基线（历史节）

> 当时产品版本节（历史值）：根 `VERSION` = `0.11.0-alpha.1`（GOV-003 唯一源；生成串
> `0.11.0-alpha.1+g<commit12>`）。历史轮次节（下方 V19R8…V12）仅供追溯，
> 不冒充当前状态（VERSION_NAMESPACES history 命名空间）。基线提交 `6affe3009985452f5bc0bdf654aa95a4b61b2d2e`。
### P5-SNR 逐源 SNR 科学修正（2026-09-14，负责人授权，scientific_change=YES）

> **负责人授权**：2026-09-14 负责人明示批准修改冻结/科学文档（
> 「前面那个冻结文档我批准你修改。需要把 SNR 改成对的。」），依
> `ASTROCS_PROJECT_CONSTITUTION.md` §1.2 变更流程放行。依据：
> `run/release-rescue/science-phot/PHOTOMETRY_LITERATURE_REVIEW.md` §C.3 / §D.2。

**问题**：现行 `snr_phot = 1/(ln10·sigma_residual)` 把帧级定标散度当成逐源 SNR
（与真值之比跨 3 个数量级，F=1e3→1e6 ADU 时 0.39→0.0004）；`(A−B)/residual_scale` 是
SNR-008 已宣布退休的量却仍在生产路径（`lib/snr_estimator/cpp/src/snr_estimator.cpp`
原 :118/:544 等 4 处）；σ 传递漏 /√N；文献中不存在「不确定度 + 局部 PSF SNR →
帧 SNR 标量」的构造。

**改前 → 改后（逐字段）**

| 字段 | 改前 | 改后 | 单位 |
|---|---|---|---|
| 控制点 `SnrControlPoint*.snr_psf` | `(A−B)/residual_scale`（退休量 SNR-008） | `SNR_F = F·sqrt(Σ_i P_i²/σ_i²)`（Horne 1986 最优提取） | 无量纲 |
| `SnrModel*.snr_phot` | `1/(ln10·sigma_residual)`（帧级常数，非 SNR） | `median(SNR_F)`（IDW 归一化对成员） | 无量纲 |
| `SnrModel*.median_snr` | `median((A−B)/residual_scale)` | `median(SNR_F)` | 无量纲 |
| `SnrModel*.median_source_snr` | （无） | `median(SNR_F)` | 无量纲 |
| `SnrModel*.frame_depth_flux5_adu` | （无） | `F_5 = 5·σ_F(ref)` | ADU |
| `SnrModel*.frame_depth_m5_mag` | （无） | `m_5 = ZP − 2.5·log10(5σ_F(ref))` | mag |
| `PhotometricCalibrationQuality.sigma_location_se_dex` | （无；sigma_residual 被当帧级量） | `1.253·sigma_logflux_dex/√N` | dex |
| `PhotometricCalibrationQuality.sigma_location_se_mag` | （无） | `2.5·sigma_location_se_dex` | mag |

**实现**：新增 `lib/snr_estimator/cpp/src/snr_science.cpp`（Horne 最优提取 / CCD 方程 /
Moffat4 β=4 孔径改正 `f_in(r)=1−(1+r²/(2σ²))⁻³` / 5σ 深度 / 零点标准误）；
`snr_estimator.cpp` 4 处控制点计算改调该实现；`noise_model.cpp::snr_phot_cal_quality`
补 /√N 标准误。

**文档订正（S1–S10，负责人授权）**：`docs/contracts/DATA_SEMANTICS.md`（§14.2/§14.3 单位）、
`docs/contracts/PUBLIC_API.md`、`docs/science/PHOTOMETRY.md`、`docs/science/PSF.md`、
`docs/science/CONTROL_WEIGHT_SNR.md`（`local_snr`/`frame_snr` 重定义为相对质量权重场，新增帧级
科学基准 `m_5`）、`docs/science/NOISE_MODEL.md`、`docs/algorithms/PHOTOMETRIC_FIT.md`（删幽灵量
`zero_point`）、`docs/modules/registry/astrocs.phase1.photometry.md`（ELECTRON→ADU）、
`lib/photometric_calib/README.md`、`lib/photometric_calib/docs/algorithm.md`（标
ARCHIVED_NON_NORMATIVE 并订正 4 处失实）、`lib/photometric_calib/cpp/test/filter_qe_provenance.json`
（57 条补 source/url/retrieved/instrument/uncertainty_note，来源不可证者诚实标 unverified）。
逐条改前/改后对照见 `run/perf-fix/P5-snr/docs/S_CHANGES.md`。

**验证**：①`p1_flux.json`/`p1_wcs.json`/`p1_sources.json` 与改前**逐位相同**
（sha256 `bc18d11f…` / `e7431e90…` / `57ef76fc…`）；②SNR 字段按新定义变化（控制点、
`snr_phot`、`median_snr`、新帧级字段）并有独立 NumPy oracle 支撑（5 真实源 + 2 合成场景，
94 点，rtol 1e-9，实测 max rel residual 3.8e-14）；③全量串行 ctest 全绿（含新增
`p1snr_science_{units,oracle,negative,production,determinism}`）；④确定性（与线程数无关）。


### GOV-001 → GOV-005 治理链（W0/W1 集成，均 scientific_change=NO）

- GOV-001 冻结 Alpha 工程约束：`AstroCS_ENGINEERING_CONSTRAINTS.md` 入 main。
- GOV-002 文档边界与归档：`docs/DOCUMENT_INDEX.yaml`；854+27 文件迁
  `engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/` 与
  `docs/archive/`。
- GOV-003 产品版本单源：根 `VERSION` = `0.11.0-alpha.1`；五个版本命名空间
  （product/module/ABI/data-schema/doc-revision + history 受管空间）；
  CMake/CLI 生成链。
- GOV-004 L0 负责人入口：`REVIEW.md` + `docs/owner/`（SCIENCE/PIPELINE/
  ARCHITECTURE/RELEASE_STATUS/CHANGE_REVIEW）。
- GOV-005（本提交）旧现状清理与记忆收敛：
  - README/HANDOVER 重写：移除 V18/V19/F 盘/旧 exe/旧线程数/旧仓库形态等
    stale-active 表述，指向当前约束/L0/docs 权威；
  - memory.md 收敛为只含稳定目标、当前 SHA/版本、模块索引、开放问题；
    原 2256 行操作日志归档至
    `docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md`
    （ARCHIVED_NON_NORMATIVE，含 ARCHIVED 头与替代链接）；
  - `docs/DOCUMENT_INDEX.yaml`：base_product_version 收敛为 `0.11.0-alpha.1`，
    登记新增归档文件；`docs/archive/history/README.md` 补登记；
  - 验收：控制包 `validators/validate_docs.py`（--candidate-root）与三个
    doccheck 检查器全 PASS（详见 returns/GOV-005）。

### DOC-001 机器追溯合同（SA-QA-29，wave W1，scientific_change=NO）

- `docs/traceability/TRACEABILITY_SPEC.md`：八层追溯合同标准——ID 格式
  （SCI/ALG/DATA/API/ARCH/MOD/SRC/TEST/EVID 正则）、唯一性域、跨层
  parent→child 链（SCI→ALG→DATA/API/ARCH→MOD/SRC→TEST→EVIDENCE）、
  SOURCE SYMBOL 表达（`path::symbol`）；空缺必须显式 `MISSING`/`NONE`，
  禁止空字符串通过。
- `schemas/traceability_matrix.schema.json` + `docs/traceability/TRACEABILITY_LAYERS.csv`：
  JSON/CSV schema（每层必填、状态取值域、ID pattern、模块行键唯一）。
- `docs/traceability/TRACEABILITY_MATRIX.json`（权威）+ `TRACEABILITY_MATRIX.csv`
  （同构视图）：覆盖 25 个已注册模块（services/io、conformance/noop、
  providers/cpu、22 个 registry phase 模块），每行八层全显式；无科学/算法
  合同的模块行显式 `SCI-MISSING`/`ALG-MISSING`（不伪 PASS）。
- `tools/traceability/check_traceability_matrix.py`（机器闭环）：
  `TRACEABILITY_MATRIX_PASS modules=25 errors=0`；空串/断链/重复/悬空引用
  输出具体模块与路径并以非 0 退出，不崩溃。
- 试金石：`tests/traceability/test_traceability_matrix.py` + 六类负面 fixture
  （空串/断链/重复/悬空/坏 ID/缺列）4 项测试 PASS。

### 同波合同面集成（GOV 之外，供追溯）

- ABI-001 C ABI v1 / ARC-001 DLL 边界 schema / LOG-001 结构化日志 /
  DATA-001 类型化产物 / RT-001 类型化运行图 / DATA-002 三阶段产品交换 /
  BLD-001 Windows 工具链 preset / BLD-002 唯一根 CMake / IO-001 FITS 流式接口
  / ABI-002 / RT-002（提交链见 `docs/owner/CHANGE_REVIEW.md` §1）。

### 未完成 / 未验证（alpha.1 期如实登记；V6 收敛标注见括号）

- Windows DLL 化发布安装树、MSVC 编译/测试/32R/真实数据最终验收：NOT_VERIFIED。（V6：Windows CI failure + Fatduck 不可达 → `AWAITING_WINDOWS_VALIDATION`，`FD-F-003` OPEN）
- Phase3 SIN/ZEA/CAR/AIT、`healpix_interp4`、流式 FITS 接入：不在基线（NOT_VERIFIED）。（V6：冻结四投影为 **TAN/SIN/CAR/AIT**（宪章 §18.1），registry v1 已实现并通过独立 Oracle；`healpix_interp4`/流式 FITS 仍未实现）
- 遗留 `astrocs run --phases 1,2,3`：FAIL（未删，冲突约束 §A.4，W4 范围）。（V6：**已删除**，实测 rc=2 `unknown command`；宪章 §3.2 禁止三 Phase 隐式串接）
- 约束 §F.1 每节点唯一真实模块 operation：进行中/未达成（W3/W4）。（V6：P1-001/P2-001/P3-002 已完成三 Phase 节点化，§F.1 **已达成**）
- 版本收敛他人路径遗留（docs/VERSIONING.md、CMake project 字面量、tests/tools
  硬编码）：check_version_namespaces.py `out_of_scope` 登记，待前台/QA 协调。

---
<!-- 以下为历史轮次节（history 命名空间）：V1–V19/V18R2/V19R2/V19R8 等，
     仅供追溯，不代表当前状态。 -->

## [V19R8 Quality Closure 94/95] 2026-08-22 vm-bj — B4 28/28 + B5 8/8 + C 11/11 + D 4/6 (D-01/02 PASS, D-04 22files SHA 22/22 OK, D-05 clean HEAD)
- Evidence: c02 39/39 noise, c03 4000MC 8/8 variance (p50=1.001), c04 79/10 phase2 35.7s, c05 28/28 pipeline, c10 smoke 28/28, machine 9/9, build 0 warnings


## [V19R8] 2026-08-22 — Quality Closure B4 28/28 + B5 docs (76/95 DONE)
- B4 28/28 DONE: calibration/plate_solve/gaia/dynamic_psf/photometric/snr/drizzle/orchestrator/phase2 全锚点 + machine 9/9 + push闭环
- B5 6/8 DONE: PROJECT_STATE/CURRENT_TASK/DECISION/RISK/MASTER+TARC/TRAC 75/RELEASE 7件套同步 V19R8 S0-S6 (B4 gate+TRAC 75)
- TRAC 63→75 rows, `MASTER_TASK_REGISTER QA-V19R8-S3-11` 追加

## [V19R2] 2026-08-15 — Pre-Release Traceable Foundation

### PR#1 UPM 持久化绑定（SCI-UPM-PERSIST-001 门禁）
- 修复 save/load 后 frame_id→参数绑定漂移：frames 列表改用
  frame_id_by_index（index 序）序列化，open 重建双向映射
- p2_upm_open 畸形模型校验：重复 ID/缺列表/行列不匹配/类型损坏稳定报错
  （ERR-P2-UPM-001）；p2_upm_save 校验行数不变量
- PR-UPM-001..010 门禁测试全部通过；向后兼容方案 A（安全迁移）

### 工程冻结（全仓规范 + 逐文件审计 + 质量）
- 文档体系 L0-L5：science/11、algorithms/12、architecture/12、
  standards/13、modules/13；V19 扁平文档迁 docs/history/
- TRACEABILITY.csv 30 行；science_code_mapping.csv；broken=0
- 逐文件审计 713/713（B01-B16），0 UNREVIEWED
- comment hygiene：1240 处轮次/审计标记迁移，434 文件 0 violation
- 编译告警：phase2 补 -Wall -Wextra -Wpedantic，全仓 0 first-party warning
- 修复：unknown UPM frame 显式失败（F-V19R2-UPM-002）、aio_upm 原子写
  （F-V19R2-IO-001）、strncpy 截断（F-V19R2-COV-001）、死代码
  （F-V19R2-REJ-001）、DRZ SIP 测试阈值（F-V19R2-DRZ-001）
- 自审 Round0-6 闭环；P0=0 / P1=0

```text
PRE_RELEASE_ENGINEERING_FOUNDATION=PASS
FINAL_REAL_DATA_VALIDATION=PENDING
```

## [V19] 2026-08-15 — Pre-Release Foundation Closure

### SNR/Noise 科学重构
- 三层模型: PhotometricCalibrationQuality / PsfFitQuality / NoiseWeightModelV1
- NoiseWeightModelV1: source-masked blank-sky 稳健方差 → ivar, 空间平面场,
  gain/read-noise 交叉验证, 经验 fallback
- 旧 `(A-B)/mad` 从科学路径退休 (pedestal invariance 反例, SNR-008)
- 科学矩阵 SNR-001..015: 模块级 32/32 + Drizzle MC 8/8 + phase2 ablation

### Drizzle 深度优化
- 方差传播: sumVarNum += v_j·w_jp², variance/ivar HiPS 产品 (P1-003)
- 操作计数: operation_counts.json (candidates/overlaps/pix2radec/...)
- 科学: SNR-011 MC 4000 实现 p50=1.001, p95=[0.962,1.042]

### Phase2 权重模型
- UPM: quality × support^p × ivar (legacy snr² 仅 ablation)
- Integration: weight_mode=ivar 默认; support 只作 validity
- ACR kernel: mode 2 = support×ivar

### 代码质量 / 文档
- 诊断工具 tools/astrocs_diagnose.py + ERROR_TAXONOMY/DIAGNOSTICS/
  TROUBLESHOOTING
- docs/ 全套开发/科学/发布文档 (18 文件)

### 验收
- phase2 synthetic gate 74/74
- V19 判定: PRE_RELEASE_FOUNDATION_READY=PASS (V20 才签
  PRE_RELEASE_CANDIDATE_READY / FINAL_REAL_DATA_VALIDATION)

## [V18R3] 2026-08-14 — Gaia polar prune/cache/memory-safety/trace

- 保守极冠剪枝、精确 cache key、事务替换、checked public allocation
- 全 first-party warnings 零 (-Wall -Wextra -Wpedantic, 87 compile units)
- GAIA_V18R3_CORRECTNESS_BASELINE=ACCEPTED

## [V18R2] 2026-08-13 — Resource-driven performance closure

- Phase1 ~67.35 s/frame, Drizzle ~64 s/frame (16-frame batch 基线)
- 15 红队假设关闭

## [V17] 2026-08-14 — True Final Freeze (Phase2)

- integration status 显式、large-scale rejection、non-finite 硬门

## [V16] 2026-08-14 — Final Closure AuditFix

- wbpp_current/astrocs_adaptive profile 拆分、RejectionNormalizationPolicy
- 真实 16-exposure E2E, gate 65/65

## [V15] 2026-08-14 — Final Semantic Closure

- rejection 语义冻结、satellite gate、WBPP 2.9.1 策略对齐

## [V14] 2026-08-13 — Phase1/Phase2 Finalization

## [V13] 2026-08-12 — 主线回归与性能收尾

## [V12] 2026-08-11 — Phase2 V1 实施 (gate 22/22)