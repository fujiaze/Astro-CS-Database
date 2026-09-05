# 科学总览（Science Overview）

> 文档 ID：DOC-GOV-OWNER-SCIENCE-001
> 状态：ACTIVE_NORMATIVE（GOV-004 建立，SA-GOV-01）
> 目标产品：`0.11.0-alpha.2`（根 VERSION，GOV-003 唯一源）
> 基线提交：`caee3e67e5a209a9e47b514f42b2b63f3dc4da4e`
> 用途：项目负责人 L0 审查入口之一。本文只**汇总权威来源**（不复制公式/函数清单），
> 科学公式与默认容差的唯一权威是 `docs/science/` 与 `docs/algorithms/`。
>
> 状态词约定（全任务统一）：`PASS`=当前提交内可核（文档/合同/源码在位，或已有执行证据）；
> `FAIL`=已执行但不符合要求；`NOT_VERIFIED`=未在当前提交核实（含未复跑执行验收、
> 缺执行证据、或功能不在基线内）。本文区分三类可核对象：
> 「合同冻结」=权威文档在位且入机器索引；「源码在位」=实现符号可在当前提交源码静态核实；
> 「执行验收」=需要实际运行测试/门禁（未复跑一律 NOT_VERIFIED，不写"已实现 PASS"）。

## 1. 科学范围（权威：docs/science/SCIENCE_SCOPE.md）

AstroCS 从多帧天文 CCD 图像估计统一天球辐射场（HiPS signal）及其不确定性
（variance/ivar），输出标准 IVOA HiPS 产品与平面 FITS。科学 ID 链
`SCI-SCOPE-001 → SCI-CAL/WCS/PHOT/PSF/NOISE/DRZ/UPM/REJ/INT/CW/P3` 全量
登记于 `docs/contracts/INDEX.yaml`（contract-index）。

## 2. 冻结的科学权威源（合同冻结 = PASS，均可静态核实）

| 领域 | 权威文档（ACTIVE_NORMATIVE） | 覆盖 |
|---|---|---|
| 校准 | `docs/science/CALIBRATION.md` | master bias/dark/flat 归约 |
| 天体测量/WCS | `docs/science/ASTROMETRY.md` | ICRS、TAN、Gaia 参考 |
| 测光 | `docs/science/PHOTOMETRY.md` | 通量/星等、Gaia 校准 |
| PSF | `docs/science/PSF.md` | 星点/PSF 拟合 |
| 噪声/权重 | `docs/science/NOISE_MODEL.md`、`CONTROL_WEIGHT_SNR.md`、`UNCERTAINTY_AND_COVARIANCE.md` | variance/ivar/SNR 三层模型 |
| Drizzle 投影 | `docs/science/DRIZZLE.md` | 方差传播、HiPS 投影 |
| Phase2 UPM | `docs/science/PHASE2_UPM.md` | 联合光度模型 |
| 排异 | `docs/science/REJECTION.md` | rejection 规则 |
| 积分 | `docs/science/INTEGRATION.md` | 加权积分/ivar |
| Phase3 HiPS→FITS | `docs/science/PHASE3_HIPS_TO_FITS.md` | 平面 FITS、WCS/coverage/validity/provenance |
| ACR 等价性 | `docs/science/ACR_EQUIVALENCE.md`、`docs/algorithms/ACR_EQUIVALENCE.md` | ACR 保留但不进入当前生产路径（约束 §C.1，DORMANT） |

算法层权威：`docs/algorithms/*.md`（CALIBRATION_ALGORITHMS / PLATESOLVE /
PHOTOMETRIC_FIT / STAR_PSF_ALGORITHMS / NOISE_ESTIMATION / DRIZZLE_GEOMETRY /
HEALPIX_MAPPING / UPM_SOLVER / PHASE2_SAMPLER / REJECTION_ALGORITHMS /
INTEGRATION_ALGORITHMS / PHASE3_RESAMPLE / ACR_EQUIVALENCE）。

- 上述文件全部被 `docs/DOCUMENT_INDEX.yaml` 登记为 ACTIVE_NORMATIVE
  （`tools/doccheck/check_doc_index.py` 覆盖检查）；`docs/contracts/INDEX.yaml`
  登记 SCI/ALG 条目 → **合同冻结 = PASS**。
- 本文不复制公式；公式权威见各源文件。

## 3. 数据语义与产品合同（合同冻结 = PASS）

- 数据语义唯一权威：`docs/contracts/DATA_SEMANTICS.md`、`DATA_ARTIFACTS.md`。
- DATA-001 类型化产物合同：`contracts/data/artifact_manifest.schema.json`、
  `artifact_types.registry.json`（type_id schema_version=1）。
- DATA-002 三阶段产品交换合同：`contracts/data/phase_product_exchange.schema.json` +
  `phase_product_exchange_matrix.json` + `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md`；
  角色 `phase1_product_v1 / phase2_mosaic_v1 / phase3_planar_fits_v1` 与 registry type
  强绑定，跨 Phase **仅磁盘交换**（约束 §A.6）。
- DATA-002/RT-001 等在集成提交中已由前台跑过验收（pytest 32/32、24/24，见返回包与
  集成 commit message）；本轮文档任务不重复执行 → 执行验收列 NOT_VERIFIED（本轮），
  合同冻结 PASS。

## 4. 各 Phase 科学实现状态（逐项 PASS/FAIL/NOT_VERIFIED）

> 判据：PASS=「合同冻结」或「源码在位」（当前提交静态核实）；
> NOT_VERIFIED=执行验收未在当前提交复跑，或功能不在基线内；
> FAIL=有执行证据但不符合要求（当前无 FAIL 项，若发现会如实列出）。

### Phase1（单帧校准/定标/投影 → 单帧 HiPS）

| 项 | 状态 | 依据（当前提交内可核） |
|---|---|---|
| SCI 权威冻结 | PASS | `docs/science/*.md` ACTIVE_NORMATIVE + contract-index 登记（静态可核） |
| 实现源码在位（calibration/noise/photometry/stars/wcs/drizzle 引擎） | PASS | `lib/calibration/`、`lib/phase1/{noise,photometry,stars,wcs}/`、`lib/healpix_db/healpix_drizzle/`、`lib/phase1_session/p1_session.cpp`（io_read→calibrate→cosmetic→io_write 链） |
| 合成/单元测试文件在位 | PASS | `tests/unit/p1_*.cpp`、`tests/api/test_p1_api.py` 存在于当前提交 |
| 合成执行验收（当前提交复跑） | NOT_VERIFIED | 本轮文档任务未复跑；历史验收存档 `evidence/v6_1_rework/` 不冒充当前执行证据 |
| 真实数据（BASS/32R）验证 | NOT_VERIFIED | `docs/RELEASE_STATUS.md`/`docs/KNOWN_LIMITATIONS.md`：FINAL_REAL_DATA_VALIDATION=PENDING |

### Phase2（多帧 HiPS → 马赛克 HiPS：UPM/排异/积分）

| 项 | 状态 | 依据 |
|---|---|---|
| UPM/采样/排异/积分 ALG 权威冻结 | PASS | `docs/algorithms/{UPM_SOLVER,PHASE2_SAMPLER,REJECTION_ALGORITHMS,INTEGRATION_ALGORITHMS}.md` + INDEX.yaml |
| 实现源码在位（coverage/sample/upm/reject/integrate/write） | PASS | `lib/phase2/src/*.cpp`、`lib/phase2_session/p2_session.cpp`（coverage→sample→upm→reject→integrate→write 链） |
| 合成验收（Linux 合成域，当前提交复跑） | NOT_VERIFIED | 未复跑；历史存档不冒充当前证据 |
| 真实数据/接缝/32R 在 Windows 最终验收 | NOT_VERIFIED | 待 Fatduck（约束 §E.5；FATDUCK_ACCESS.md） |

### Phase3（HiPS → 平面 FITS + WCS/coverage/validity/provenance）

| 项 | 状态 | 依据（当前提交内可核） |
|---|---|---|
| SCI-P3 / ALG-P3 权威冻结 | PASS | `docs/science/PHASE3_HIPS_TO_FITS.md`、`docs/algorithms/PHASE3_RESAMPLE.md`、`docs/api/PHASE3_API_V1.md` |
| TAN 投影 + WCS + nearest/bilinear 重采样 + FITS 原子写源码在位 | PASS | `lib/phase3_session/{p3_session,p3_wcs,p3_resample,p3_output}.cpp` 静态可核（proj=TAN、CTYPE RA---TAN/DEC--TAN、`p3_sample_nearest/p3_sample_bilinear`） |
| Phase3 合成/单元测试文件在位 | PASS | `tests/unit/p3_{assembly,coverage,interp,output,wcs}_test.cpp`、`tests/api/test_p3_api.py` |
| **SIN / ZEA / CAR / AIT 投影** | **NOT_VERIFIED** | 当前源码仅接受 `projection=TAN`（proj≠"TAN"→UNSUPPORTED）；SIN/ZEA/CAR/AIT 不在本轮基线上 → 不宣称已实现 |
| **`healpix_interp4` 四点插值** | **NOT_VERIFIED** | 当前采样器为 nearest/bilinear，无 `healpix_interp4` 符号 → 不宣称已实现 |
| **流式 FITS 输出接入 Phase3 writer** | **NOT_VERIFIED** | IO-001 冻结 `astrocs.io.fits_stream_v1`（`runtime/io/fits_core.c` + 头 + 契约测试）；Phase3 writer 当前走 CFITSIO 原子写（p3_output.cpp），未见流式写接线 → 不宣称已接入 |

> 诚实标注：控制包任务清单（任务包 07_PHASE3_TASKS.md 等）中 SIN/ZEA/CAR/AIT 与
> `healpix_interp4` 属于后续 Phase3 扩展，当前基线不宣称已实现。REVIEW /
> RELEASE_STATUS / CHANGE_REVIEW / PIPELINE / ARCHITECTURE 各文档对 Phase3 的
> 表述必须与本表一致（验收项：Phase3 状态一致）。

## 5. 科学不变性约束（约束 §E）

- 架构迁移不得同时修改科学公式、权重/variance/ivar/SNR 定义、排异规则、归约顺序、
  精度或默认容差；无 SIMD/FMA/归约顺序改变时迁移默认要求 bitwise 相等。
- 已集成任务（GOV-001..004、DATA-001/002、RT-001、ABI-001、BLD-001/002、LOG-001、
  ARC-001、IO-001）均为架构/合同/文档任务，集成提交标注 `scientific_change: NO`，
  未触碰上述科学面。

## 6. 状态汇总

```text
SCI/ALG 权威冻结:        PASS
Phase1: 合同冻结 PASS; 源码在位 PASS; 合成执行复跑 NOT_VERIFIED; 真实数据 NOT_VERIFIED
Phase2: 合同冻结 PASS; 源码在位 PASS; 合成执行复跑 NOT_VERIFIED; Windows/32R NOT_VERIFIED
Phase3: TAN/WCS/nearest/bilinear/FITS原子写 源码在位 PASS;
        SIN/ZEA/CAR/AIT + healpix_interp4 + 流式FITS接入: NOT_VERIFIED
ACR:    DORMANT（保留源码与隔离测试；生产构建/加载/路由/benchmark/发布不含 ACR/CUDA）
```

---
authoring_task: GOV-004
authoring_owner: SA-GOV-01
base_main_sha: caee3e67e5a209a9e47b514f42b2b63f3dc4da4e
