# SCIENCE_OVERVIEW — L0 治理评审层（科学）

> 文档 ID：DOC-REVIEW-SCIENCE-001
> 状态：ACTIVE_INFORMATIVE（L0 治理评审汇总层，不含公式抄写）
> 目标产品：`0.11.0-alpha.2`（根 VERSION，GOV-003 唯一源）
> 科学公式与默认容差的唯一权威：`docs/science/` 与 `docs/algorithms/`（本文件不复制、不改写）。
> L0 负责人汇总权威：`docs/owner/SCIENCE_OVERVIEW.md`（GOV-004）。

## 1. 定位

本文件是 `REVIEW.md` L0 治理评审体系的科学面入口之一，作用是**指路**：
科学定义（SCI）、算法（ALG）的逐条合同与公式以权威层为准，本层只汇总
状态口径与可核证据入口，不维护第二份定义（避免双权威漂移）。

## 2. 科学范围（一句话）

从多帧天文 CCD 图像估计统一天球辐射场（HiPS signal）及其不确定性
（variance/ivar），输出标准 IVOA HiPS 产品与平面 FITS。科学 ID 链
`SCI-SCOPE-001 → SCI-CAL/WCS/PHOT/PSF/NOISE/DRZ/UPM/REJ/INT/CW/P3`
登记于 `docs/contracts/INDEX.yaml`。

## 3. 权威文档入口（ACTIVE_NORMATIVE，均可静态核实）

| 面 | 权威路径 |
|---|---|
| 科学范围与各 SCI 合同 | `docs/science/`（CALIBRATION / ASTROMETRY / PHOTOMETRY / PSF / NOISE_MODEL / CONTROL_WEIGHT_SNR / DRIZZLE / PHASE2_UPM / REJECTION / INTEGRATION / PHASE3_HIPS_TO_FITS / ACR_EQUIVALENCE 等） |
| 算法层 | `docs/algorithms/`（CALIBRATION_ALGORITHMS / PLATESOLVE / PHOTOMETRIC_FIT / STAR_PSF_ALGORITHMS / NOISE_ESTIMATION / DRIZZLE_GEOMETRY / HEALPIX_MAPPING / UPM_SOLVER / PHASE2_SAMPLER / REJECTION_ALGORITHMS / INTEGRATION_ALGORITHMS / PHASE3_RESAMPLE / ACR_EQUIVALENCE 等） |
| 数据语义与产品合同 | `docs/contracts/DATA_SEMANTICS.md`、`DATA_ARTIFACTS.md`、`contracts/data/` |
| 机器索引 | `docs/DOCUMENT_INDEX.yaml`（`tools/doccheck/check_doc_index.py` 覆盖检查） |

## 4. 状态口径（与 L0 owner 层一致）

- 状态词：PASS=当前提交内可核；FAIL=已执行但不符合；NOT_VERIFIED=未在当前提交核实。
- Phase1/2/3 科学实现源码在位（`lib/phase1_session`、`lib/phase2`、
  `lib/phase3_session`）；合成/门禁执行验收以最近一次集成验证为准，未复跑即
  NOT_VERIFIED，不冒充。
- Phase3 当前实现 TAN 投影 + nearest/bilinear 重采样 + FITS 原子写
  （`lib/phase3_session/p3_session.cpp`：proj≠"TAN"→UNSUPPORTED）；
  SIN/ZEA/CAR/AIT、`healpix_interp4`、流式 FITS 接入未实现，不宣称。
- ACR = DORMANT：保留源码与隔离测试；生产构建/加载/路由/benchmark/发布
  不含 ACR/CUDA；当前唯一生产计算后端是纯 CPU（约束 §C）。
- 科学不变性（约束 §E）：迁移不得改公式、权重/variance/ivar/SNR 定义、
  排异规则、归约顺序、精度或默认容差；架构/文档任务一律 `scientific_change: NO`。

## 5. 诚实缺口（不冒充已实现）

1. 真实数据（BASS/32R/接缝）最终验收：PENDING（Windows/Fatduck 侧）。
2. Phase3 扩展（SIN/ZEA/CAR/AIT、healpix_interp4、流式 FITS 接入）：未实现。
3. 执行验收历史存档不冒充当前提交证据；复核入口见 `docs/owner/SCIENCE_OVERVIEW.md`。

---
authoring_task: DOC-L0
authoring_layer: docs/review (L0 governance review)
base_product_version: 0.11.0-alpha.2
