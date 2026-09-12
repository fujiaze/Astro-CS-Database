# 科学总览（Science Overview）

> 文档 ID：DOC-GOV-OWNER-SCIENCE-001
> 状态：ACTIVE_NORMATIVE（GOV-004 建立，SA-GOV-01）
> 目标产品：`0.11.0-alpha.2`（根 VERSION，GOV-003 唯一源）
> 建立基线：`caee3e67e5a209a9e47b514f42b2b63f3dc4da4e`（GOV-004，历史值）
> 收敛基线：DOC-CONV-001，BASE_SHA = `da3c4b4aaf64ef9b61039fabd1100ddd1f9b8540`
> 用途：项目负责人 L0 审查入口之一。本文只**汇总权威来源**（不复制公式/函数清单），
> 科学公式与默认容差的唯一权威是 `docs/science/` 与 `docs/algorithms/`。
>
> 状态词约定（全任务统一，DOC-CONV-001 起唯一口径见
> `docs/owner/RELEASE_STATUS.md` §0）：`CONTRACT_READY`=合同/权威文档冻结在位；
> `IMPLEMENTED`=源码在位且**当前提交内实际执行通过**（给出命令与 rc）；
> `INSTALLED`=已进安装树/产品清单且可被 CLI/loader 发现；`VERIFIED`=正式平台
> （Windows x64）与真实数据验收通过；`NOT_IMPLEMENTED`=符号/路径不存在；
> `NOT_VERIFIED`=未在当前提交复跑执行验收；`FAIL`=已执行但不符合要求。
> 历史三级口径（合同冻结/源码在位/执行验收）自本任务起由上述阶梯取代。

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

## 4. 各 Phase 科学实现状态（状态词见文首约定）

> 判据：`CONTRACT_READY`=合同冻结；`IMPLEMENTED`=源码在位**且本提交实测通过**；
> `NOT_IMPLEMENTED`=基线内无该能力；`NOT_VERIFIED`=未在本提交复跑执行验收；
> `FAIL`=有执行证据但不符合要求（当前无 FAIL 项，若发现会如实列出）。
> 本节的实测证据统一来自 BASE=`da3c4b4a`（日志 `run/docconv001/logs/`）。

### Phase1（单帧校准/定标/投影 → 单帧 HiPS）

| 项 | 状态 | 依据（当前提交内可核） |
|---|---|---|
| SCI 权威冻结 | PASS | `docs/science/*.md` ACTIVE_NORMATIVE + contract-index 登记（静态可核） |
| 实现源码在位（calibration/noise/photometry/stars/wcs/drizzle 引擎） | PASS | `lib/calibration/`、`lib/phase1/{noise,photometry,stars,wcs}/`、`lib/healpix_db/healpix_drizzle/`、`lib/phase1_session/p1_session.cpp`（io_read→calibrate→cosmetic→io_write 链） |
| 合成/单元测试文件在位 | `IMPLEMENTED` | `tests/unit/p1_*.cpp`、`tests/api/test_p1_api.py` 存在于当前提交 |
| **Phase1 节点化（IR 节点唯一真实 operation，宪章 §F.1）** | **`IMPLEMENTED`** | `lib/core/src/module_adapters.cpp`:4257 八节点（calibrate/cosmetic/star-psf/wcs/photo/noise-snr/drizzle/writer）各绑唯一真实 operation；ctest `p1001_real_nodes` 本提交实测 PASS（P1-001 `9e09941a`） |
| 合成执行验收（当前提交复跑） | 部分 `IMPLEMENTED` | 节点化/消费者用例本提交实测绿（`p1001_real_nodes`）；原生像素域全量合成链路（真实数据）仍未复跑 → 见下 |
| 真实数据（BASS/32R）验证 | `NOT_VERIFIED` | `docs/RELEASE_STATUS.md`/`docs/KNOWN_LIMITATIONS.md`：FINAL_REAL_DATA_VALIDATION=PENDING；REAL-000 `9f6b72b5` 数据集审计/索引 v1.2/确定性匹配计划已 IMPLEMENTED |
| 真实数据（BASS/32R）验证 | NOT_VERIFIED | `docs/RELEASE_STATUS.md`/`docs/KNOWN_LIMITATIONS.md`：FINAL_REAL_DATA_VALIDATION=PENDING |

### Phase2（多帧 HiPS → 马赛克 HiPS：UPM/排异/积分）

| 项 | 状态 | 依据 |
|---|---|---|
| UPM/采样/排异/积分 ALG 权威冻结 | PASS | `docs/algorithms/{UPM_SOLVER,PHASE2_SAMPLER,REJECTION_ALGORITHMS,INTEGRATION_ALGORITHMS}.md` + INDEX.yaml |
| 实现源码在位（coverage/sample/upm/reject/integrate/write） | PASS | `lib/phase2/src/*.cpp`、`lib/phase2_session/p2_session.cpp`（coverage→sample→upm→reject→integrate→write 链） |
| **Phase2 节点化（IR 七节点唯一真实 operation）** | **`IMPLEMENTED`** | `lib/core/src/module_adapters.cpp`:4282 七节点（coverage/sample/upm_fit/upm_apply/reject/integrate/write）；ctest `p2001_real_nodes`、`p2002_unc_rej_prov` 本提交实测 PASS（P2-001 `439f9f20`、P2-002 `9e0fa3a8`） |
| 不确定度/排异/provenance 产品（DATA-UNC-001 §30） | `IMPLEMENTED` | `contracts/data/phase2_uncertainty_rejection_provenance_v1.json` + `tests/unit/p2002_unc_rej_prov_test.cpp` 本提交实测 PASS；已知遗留 F-P2-002-01/02/03 由 lib/core 与 AIO 域处置 |
| 真实数据/接缝/32R 在 Windows 最终验收 | `NOT_VERIFIED` | 待 Fatduck（约束 §E.5；FATDUCK_ACCESS.md） |

### Phase3（HiPS → 平面 FITS + WCS/coverage/validity/provenance）

| 项 | 状态 | 依据（当前提交内可核） |
|---|---|---|
| SCI-P3 / ALG-P3 权威冻结 | PASS | `docs/science/PHASE3_HIPS_TO_FITS.md`、`docs/algorithms/PHASE3_RESAMPLE.md`、`docs/api/PHASE3_API_V1.md` |
| 会话路径 TAN 投影 + WCS + nearest/bilinear 重采样 + FITS 原子写 | `IMPLEMENTED` | `lib/phase3_session/{p3_session,p3_wcs,p3_resample,p3_output}.cpp`（会话路径仍 TAN-only，`p3_wcs.cpp`:36）；ctest `p3_wcs`/`p3_interp`/`p3_output` 家族在本提交全量构建中 rc=0 |
| **Phase3 节点化（IR 五节点唯一真实 operation）** | **`IMPLEMENTED`** | `lib/core/src/module_adapters.cpp`:4309 五节点（properties/wcs/resample/writer/verify）各绑唯一真实 operation；typed artifact 链 `p3_props.json→p3_wcs.json→p3_resampled.{json,bin}→output_phase3.fits→p3_verify.json`，节点 call_count=1；ctest `p3002_real_nodes`/`p3002_uncertainty` 本提交实测 PASS（P3-002 `1a56ffb7`，科学面 `9662afa8`） |
| **冻结四投影 TAN / SIN / CAR / AIT（宪章 §7.3/§18.1）** | **`IMPLEMENTED`**（registry 面） | `lib/phase3_proj/p3_projection.{h,cpp}` registry v1 恰四行（`:267-273`，`:299` 版本断言）+ 独立 numpy Oracle（`tests/backend/test_p3_projection_oracle.py`）；ctest `p3_projection_units`/`p3_projection_fault` 本提交实测 2/2 PASS；ALG 唯一权威 `docs/algorithms/PHASE3_PROJ_IMPL.md` §15（P3-001 `9953f103`）。**未 INSTALLED**：`lib/phase3_proj/module.yaml`:79-80 `entrypoint: MISSING`，生产会话/DLL 尚未挂载 registry → 不得表述为已安装/已验证；**ZEA 不在冻结四投影内**（旧表述 SIN/ZEA/CAR/AIT 已按 §18.1 更正） |
| Phase3 合成/单元测试文件在位 | `IMPLEMENTED` | `tests/unit/p3_{assembly,coverage,interp,output,wcs}_test.cpp`、`tests/api/test_p3_api.py`、`tests/unit/p3002_*_test.cpp`、`tests/unit/p3_projection_test.cpp` 存在于当前提交 |
| **`healpix_interp4` 四点插值** | **`NOT_IMPLEMENTED`** | `lib/`、`cli/`、`include/`、`runtime/` 全域无 `interp4` 实现符号；当前采样为 nearest/bilinear（G4 冻结权重） |
| **流式 FITS 输出接入 Phase3 writer** | **`NOT_IMPLEMENTED`** | IO-001 冻结 `astrocs.io.fits_stream_v1`（`runtime/io/fits_core.c` + 头 + 契约测试，接口面 `IMPLEMENTED`）；Phase3 writer 走 CFITSIO 原子写（`lib/phase3_session/p3_output.cpp`），未见流式写接线 |

> 诚实标注：负责人裁决 §18.1 冻结的首批投影为 **TAN+SIN+CAR+AIT**，其 registry 实现
> 已落位并通过独立 Oracle 与故障注入（`IMPLEMENTED`），但**生产会话路径与 DLL 挂载
> 尚未切换**（`entrypoint: MISSING`），故不得写作 INSTALLED/VERIFIED；
> `healpix_interp4` 与 Phase3 流式 FITS 接入属后续扩展，当前 `NOT_IMPLEMENTED`。
> REVIEW / RELEASE_STATUS / CHANGE_REVIEW / PIPELINE / ARCHITECTURE 各文档对 Phase3
> 的表述必须与本表一致（验收项：Phase3 状态一致）。

## 5. 科学不变性约束（约束 §E）

- 架构迁移不得同时修改科学公式、权重/variance/ivar/SNR 定义、排异规则、归约顺序、
  精度或默认容差；无 SIMD/FMA/归约顺序改变时迁移默认要求 bitwise 相等。
- 已集成任务（GOV-001..004、DATA-001/002、RT-001、ABI-001、BLD-001/002、LOG-001、
  ARC-001、IO-001）均为架构/合同/文档任务，集成提交标注 `scientific_change: NO`，
  未触碰上述科学面。

## 6. 状态汇总

```text
SCI/ALG 权威冻结:            CONTRACT_READY
Phase1: 合同 CONTRACT_READY; 节点化 IMPLEMENTED(p1001_real_nodes 实测);
        合成执行复跑 部分 IMPLEMENTED; 真实数据 NOT_VERIFIED
Phase2: 合同 CONTRACT_READY; 节点化 IMPLEMENTED(p2001/p2002 实测);
        不确定度/排异/provenance IMPLEMENTED; Windows/32R 真实数据 NOT_VERIFIED
Phase3: 会话路径 TAN/WCS/nearest/bilinear/FITS原子写 IMPLEMENTED;
        节点化 IMPLEMENTED(p3002_real_nodes/uncertainty 实测);
        冻结四投影 TAN/SIN/CAR/AIT registry IMPLEMENTED(p3_projection_units/fault 实测;
        生产挂载 entrypoint=MISSING 未 INSTALLED);
        healpix_interp4 + 流式FITS接入 NOT_IMPLEMENTED
ACR:    DORMANT（保留源码与隔离测试；生产构建/加载/路由/benchmark/发布不含 ACR/CUDA）
```

---
authoring_task: GOV-004
authoring_owner: SA-GOV-01
base_main_sha: caee3e67e5a209a9e47b514f42b2b63f3dc4da4e
convergence_task: DOC-CONV-001
convergence_base_sha: da3c4b4aaf64ef9b61039fabd1100ddd1f9b8540
