# QA-V19R7 A2 审计总表汇总（QA-V19R7-A2-08）

> 任务: QA-V19R7-A2-08 审计总表汇总 | Spec: `工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md` §4.1 A2 | Checklist: `工程控制/checklists/QA_V19R7_QUALITY.md` A2  
> 基线: V19R6R2-W1 HEAD 2767874 / 05d7d6b | 模式: 只读汇总（不改 docs/lib） | 审计员: resident:project | 日期: 2026-08-22  
> 输入: 7 域分表 `audit_findings_{science,noise,drizzle,phase2,io,architecture,standards}.md` + A1 机器 `machine_consistency_before.json` / `traceability_broken.json` / `file_audit_before.json` / `standards_violations.json` / `audit_stats.json(before)`  
> 产出: 本文件 `reports/v19r7_quality/audit_findings.md` + `reports/v19r7_quality/audit_stats.json(汇总版)` + `reports/v19r7_quality/evidence/QA-V19R7-A2-08/EVIDENCE_INDEX.md`

---

## 1. 总述

本次 A2 为 V19R7 入 B 前的只读审计门（A Gate），覆盖科学/算法/架构/IO/标准五层，冻结语义"科学定义=算法=接口=代码=测试"四层统一。7 域分表独立审计后，本总表做**唯一汇总口径**（只读、不改业务代码与科学文档）。

**A Gate 判定: CONDITIONAL PASS（条件通过）** —— 机器一致性 9/9 PASS、追溯 63/63 闭环，但存在 **P0 冻结 4 项**（科学可观测分叉/架构双实现）必须在 B1/B4 冻结修复后方可进入 B2 全量实现；P1 24 项必须在 B 阶段按归口闭环，P2 36 项为表述/可追溯性增强（不阻断但需 B 阶段收口）。

- **总量**: 64 项（P0 4 / P1 24 / P2 36），分属 7 域，分表行级锚点全部保留。
- **机器证据**: `machine_consistency_before.json` 9 checks 0 broken `pass=true`；`TRACEABILITY.csv` 63 行 0 broken；`file_audit_before.json` 873 文件（含第三方）/ 581 源码已扫，`tool_missing=true` 为 P1 证据缺口（A1-02）；`standards_violations.json` CODE 3 / COMMENT 4 共 7 违规已在 A2-07 行级定位。
- **不变式**: 冻结语义（通量守恒 `F_p=Σx_j w_jp`、方差 `variance=sumVarNum/D_p²`、ivar `1/variance`、Moffat4、IRLS、frame_id SHA-256 截断、Drizzle `false_negative=0`、control_variance `k_corr·π/2·σ²/N_retained`）在实现层正确，仅文档/追溯/原子性存在口径分叉。

---

## 2. 方法

1. **输入冻结**: 以 7 域分表为唯一输入（不重审 docs/lib），每表统计 `P0/P1/P2/合计` 并逐项核对 `级别/合同/文档节/代码位置/描述/建议归口` 六要素一致性；两表 `*science/noise` 的 P0 定义为"可观测分叉即 P0"。
2. **计数复核**: `grep -c "^| SC-/NO-/DRZ-/PH2-/IO-/ARC-/STD-"`逐表复核：science 12 / noise 10 / drizzle 8 / phase2 8 / io 7 / architecture 9 / standards 10（分级项，STD-011/012 无违规不计）= 64 项；P0 4 项交叉验证为 SC-01/SC-02/NO-01/DRZ-01。
3. **机器交叉**: 复用 A1 机器 `machine_consistency_before.json` 9 项（config_weight_mode_ivar / frame_id_contract_exact / error_taxonomy / integration_status / rejection_status / stage_ids / snr_constants / product_contracts / drizzle_variance_formula）全 PASS；`TRACEABILITY 63/63` 但 SCOPE/NOISE-WIRE/DRZ-CAND 等 5 行存在分流未结构化（P1）。
4. **分级冻结**: P0 冻结清单需满足"阻断四层统一或可观测误用"，仅 4 项入冻结；其余 60 项按"文档滞后 < 原子性 < 追溯缺失 < 表述"排序为 P1/P2。
5. **归口映射**: 按 Spec §4.2 B1-01..10（科学/噪声/几何/Phase2）、B2（算法文档）、B3（架构/L3）、B4（实现/去重/原子化）、B5（追溯/机器门禁）五桶映射，每项标注主归口与协办。
6. **只读约束**: 本汇总仅写 `reports/`（总表+stats+evidence），不改 `docs/`/`lib/`；可复现性以 `sha256sum audit_findings.md + audit_stats.json` 为准。

---

## 3. 按域统计表

| 域 | 分表 | P0 | P1 | P2 | 合计 | 占比 | 关键分布 |
|---|------|----|----|----|------|------|----------|
| **Science** | `audit_findings_science.md` A2-01 | 2 | 4 | 6 | 12 | 18.8% | CAL失效域、PHOT清洗算法各1 P0；SCOPE追溯断1 P1 |
| **Noise** | `audit_findings_noise.md` A2-02 | 1 | 4 | 5 | 10 | 15.6% | gain域缺失1 P0；k_corr域/分流/file_audit/NOISE-WIRE各1 P1 |
| **Drizzle** | `audit_findings_drizzle.md` A2-03 | 1 | 3 | 4 | 8 | 12.5% | 双HEALPix 1 P0；缓冲系数/方差归一/候选oracle各1 P1 |
| **Phase2** | `audit_findings_phase2.md` A2-04 | 0 | 3 | 5 | 8 | 12.5% | 无P0；权重分层/frame_id二进制/k_corr per-frame各1 P1 |
| **IO** | `audit_findings_io.md` A2-05 | 0 | 3 | 4 | 7 | 10.9% | 无P0；原子写双向滞后/variance hierarchy/frame_id映射各1 P1 |
| **Architecture** | `audit_findings_architecture.md` A2-06 | 0 | 4 | 5 | 9 | 14.1% | 无P0；原子写/依赖分层/所有权/确定性锚点各1 P1 |
| **Standards** | `audit_findings_standards.md` A2-07 | 0 | 3 | 7 | 10 | 15.6% | 无P0；COMMENT轮次词/废话/裸1e-6各1 P1 |
| **合计** | 7 域 | **4** | **24** | **36** | **64** | 100% | P0=6.3% P1=37.5% P2=56.2% |

> 复核: `grep` 行计数 12+10+8+8+7+9+10=64；`audit_stats.json(before)` 的 `traceability 63/63` 与 `machine_consistency 9/9 PASS` 已复用为 A Gate 机器证据，`standards 7 violations` 已在 standards 域行级定位。

---

## 4. P0 冻结清单（4 项，阻断 B1/B4 前必须修复）

| # | 域 | 合同 | 文档节 | 代码位置 | 冻结描述 | 判定理由 | 归口 |
|---|----|------|--------|----------|----------|----------|------|
| **SC-01** | Science | SCI-CAL-001 | `CALIBRATION.md §失效条件"flat_norm=0→显式拒绝"` | `lib/calibration/src/calibrator.cpp:90,120,164 normalize_flat` | 文档约定 `flat=0` 显式拒绝；实现为 `std::max(flat,0.1f)` 静默钳位并继续，无错误码。 | 用户无法区分"平场损坏"与"正常帧"，可观测语义分叉，阻断失效域冻结 | **B1-02** (CAL) 文档向代码对齐或钳位改为显式错误 |
| **SC-02** | Science | SCI-PHOT-001 | `PHOTOMETRY.md §sigma_mag=2.5×MAD(r_inlier)/0.6745, r_i=log10(F_instr/F_syn)` | `lib/photometric_calib/cpp/src/star_matcher.cpp:21,478-525,552-559` | 文档暗示 median 位置估计；实现为 **IRLS+Tukey biweight(c=4.685,50iter,1e-6)** 求 location，`scale=10^{-location}` 非 median，`sigma_residual=MAD(r_inliers)/0.6745` | 算法细节与文档公式不一致，QA 复现偏离，阻断科学等价门 | **B1-04** (PHOT) |
| **NO-01** | Noise | SCI-NOISE-005 / NOISE-WIRE-001 | `NOISE_MODEL.md §科学定义(无gain/readnoise章节)` | `lib/snr_estimator/cpp/include/snr_estimator.h:98-141` + `lib/snr_estimator/cpp/src/noise_model.cpp:457-464 snr_noise_gain_variance` + `lib/snr_estimator/cpp/test/noise_model_science_test.cpp:238-272` | 文档未给出 gain 模型物理域与克制语义；`snr_noise_gain_variance` 仅 `max(signal,0)/gain+(rn/g)²` 作**诊断/交叉验证(SNR-005)**，不得注入 `NoiseWeightModelV1` 生产场（`source==0 empirical`），但文档未显式隔离，易误用。 | 可观测误用风险（用户据 header gain 误判生产 ivar 偏置），阻断噪声管线语义冻结 | **B1-05** (NOISE_MODEL) |
| **DRZ-01** | Drizzle | SCI-DRZ-001 / ALG-HEALPIX-* / ENG-OWN-001 | `HEALPIX_MAPPING.md "lib/common/healpix"` vs `DRIZZLE_GEOMETRY.md "lib/healpix_db/healpix_drizzle"` | `lib/common/healpix/healpix_core.h/.cpp(322L/679L)` vs `lib/healpix_db/healpix_drizzle/healpix_core.h/.cpp` | **两套 HEALPix NESTED 独立实现并存**：`astrocs::healpix::ang2pix_nest` vs `healpix::HealpixCore::ang2pix`，头guard/命名空间/API完全分叉，文档指向分裂，违反 Spec B1-04/B4-01"唯一映射去重"与 MODULE_MAP 单一权威。 | 数值一致性靠手工同步，无编译期阻断，BASS 不同 NSIDE 下 1 ULP 可致 tile 缝，阻断架构冻结 | **B4-01** 去重为单源（drizzle转依赖 `lib/common/healpix`，另一份 deprecated shim+机器门禁） |

> **冻结语义**: 以上 4 项在 B1/B4 修复并经 `tools/docs_machine_consistency.py` 与 `candidate_oracle`/`synthetic_gate` 复核 0 broken 前，**禁止进入 B2 全量实现**；其余 60 项可在 B 阶段并行收口。

---

## 5. P1 清单（24 项，按优先级排序：科学可观测 > 原子性/I/O > 追溯/机器证据 > 文档表述锚点）

| 优先级 | # | 域 | 合同/标准 | 一句话摘要 | 建议归口 |
|--------|---|----|-----------|------------|----------|
| 1 | **SC-03** | Science | SCI-SCOPE缺行 | TRACEABILITY 无 `SCI-SCOPE-*` 行，SCOPE 未建模致 machine 0 broken 虚高 | **B5-06** / B1-01 |
| 2 | **SC-04** | Science | SCI-CAL-001 | "按曝光/滤镜分组/母版σ→ivar"无实现（由 orchestrator/testdata 侧保证，ivar 含母版方差无支撑） | **B1-02** |
| 3 | **NO-03** | Noise | SCI-NOISE-001..015 | 15 行 SCI-NOISE 统一指向 TEST-SNR-001，未显式标注 SNR-011/012委派healpix_drizzle、SNR-015委派phase2 | **B5-06** |
| 4 | **NO-05** | Noise | NOISE-WIRE-001 | `cfg==nullptr/default_config()/生产默认 exact` 的 NOISE-WIRE-001 未入 TRACEABILITY | **B5-06** |
| 5 | **NO-02** | Noise | SCI-NOISE-011/012 | `k_corr=1.4/control_variance` 承载于 UNCERTAINTY 文档但未声明不属于 `lib/snr_estimator` 域，易误套 | **B1-06** |
| 6 | **NO-04** | Noise | A1-02证据缺失 | `tools/file_audit` 缺失致 873/~713+ 覆盖度为替代统计，noise 域 NUMERIC/COMMENT 违规未行级定位 | **A1-02补齐** |
| 7 | **DRZ-03** | Drizzle | SCI-DRZ-014/ALG-DRZ-VAR | variance `sumVarNum/D²` 归一在 sink/writer finalize，未声明为分子中间量，TRACEABILITY 缺 `aio_hips_writer::finalize_tile` | **B1-07** + B5 |
| 8 | **DRZ-02** | Drizzle | ALG-DRZ-GEOM-CACHE-001 | 缓冲系数口径分叉：文档"2.0×hp_res" vs 代码 `1.25×(quick-reject)/3.0×(candidate)/1.25×+1.15畸变+极冠回退(fast)` | **B2-07/B1-06** |
| 9 | **DRZ-04** | Drizzle | SCI-DRZ-001/ALG-DRZ-CAND-001 | `false_negative=0` 9003例 oracle 已实现但 TRACEABILITY 无 `TEST-DRZ-CAND-001` 行 | **B5** |
| 10 | **PH2-01** | Phase2 | SCI-UPM-WEIGHT-001 | `w_UPM=quality×geom×ivar` 若不读下一段易误 raw 含 geom（实为 raw=quality×ivar，归一化×geom） | **B1-08** |
| 11 | **PH2-03** | Phase2 | DATA-UPM-CONTROL-UNC-001 | per-frame `k_corr` provenance 覆盖（`frames[].kcorr`）未在 PHASE2_SAMPLER 文档说明 | **B1-08/B2-09** |
| 12 | **PH2-02** | Phase2 | DATA-UPM-MODEL-001/FRAME-BIND-001 | payload 含 `float32` 裸 bytes（LE/endian/NaN payload）未在 DATA_SEMANTICS 声明 | **B1-08/B2-10** |
| 13 | **IO-001** | IO | ENG-IO-001/IO_STANDARD | HiPS tiles 非原子（`remove→fits_create→write→close`）与 UPM 已原子但文档写"未原子"双向滞后 | **B3-08/B4-03** |
| 14 | **ARC-001** | Architecture | ENG-IO-001 | 同 IO-001 架构视角：`IO_AND_ATOMICITY.md 4-5` UPM段落后1版本，HiPS段缺失 | **B3-08/B4-03** |
| 15 | **IO-002** | IO | DATA-HIPS-VAR/IVAR | `add_var(z,var_n,0.0)` area 占位，Σarea 复用 signal/support 的 AncestorAcc 未文档化 | **B3-08/B4-04** |
| 16 | **IO-003** | IO | DATA-FRAME-ID-001 | `PipelineFrame`（blocks容器）与 frame_id/manifest 契约映射未显式（frame_id 在 phase2/upm + writer properties 协同生成） | **B3-04/B5-06** |
| 17 | **ARC-002** | Architecture | DEPENDENCY_RULES/MODULE_MAP | phase2→aio 仅 .cpp 层（头文件无环）未显式；healpix_db 三子项拆分与 MODULE_MAP 口径不一致 | **B3-03** |
| 18 | **ARC-003** | Architecture | OWNERSHIP_AND_LIFETIME | dense cache `AioUpmDense unique_ptr guard(~448)` 与 `aio_upm_read_all_dynamic delete[]` 未在 OWNERSHIP 文档具名 | **B3-05** |
| 19 | **ARC-004** | Architecture | THREADING_MODEL | 浮点累积顺序固定/计数器锚点未给 file:line（upm compute_raw / drizzle reduction） | **B3-06** |
| 20 | **SC-05** | Science | SCI-PSF-001/ALG-STAR-PSF | "已移除高斯路径"无文件/提交锚点，TRACEABILITY 未标注 | **B1-03** |
| 21 | **SC-06** | Science | SCI-AST-001 | 极区 prune 几何阈值/`|dec|>85°` 保守性证明引用与 gaia RA环绕未回链 ASTROMETRY 失效条件 | **B1-03** |
| 22 | **STD-001** | Standards | COMMENT_STANDARD MUST | 3 处 `V19R4/F-V19R2` 轮次词残留（`representative_probe.cpp:2` `test_spherical_overlap.cpp:1025` `synthetic_gate.cpp:5322`） | **B4-20/B4-25** |
| 23 | **STD-002** | Standards | COMMENT_STANDARD MUST | 1 处废话注释 `checkpoint.cpp:406 // 遍历数组中的每个对象` | **B4-23** |
| 24 | **STD-003** | Standards | CODE/NUMERIC MUST | 测试6+处裸 `1e-6` 无来源（`hiss_correctness_test.cpp:155/187/269` `test_query_pixel.cpp:613` 等） | **B4-03/B4-04** |

> 另有跨域复述项已去重：ARC-001 与 IO-001 同源仅计一次优先级；standards 的 STD-004 跨域原子写在 P2 计（见下节）避免重复。

---

## 6. P2 清单（36 项，表述/锚点/可追溯性增强，不阻断）

| # | 域 | 摘要 |
|---|----|------|
| SC-07 | Science | `dark·t_expo` 是否含 bias 未区分两模式 `(light−dark)/flat` vs `(light−bias−K*(dark−bias))/flat` |
| SC-08 | Science | `flat_norm` 均值/中值未冻结到 median=1.0+0.1 clamp |
| SC-09 | Science | CRPIX 1-based FITS 与 0基 x,y 混用，SIP `A/B` vs `AP/BP` `cd_inv` 与 Y-down `(-1)^j` 仅代码注释 |
| SC-10 | Science | 饱和判据 `qf & SATURATED` + `mag_tolerance=3.0` 未定义来源，MAD=0 处理未回链 |
| SC-11 | Science | Moffat4 椭圆各向异性 `sx,sy,theta` 与 `eccentricity/theta` 四象限消歧未在 PSF.md 说明 |
| SC-12 | Science | `q_psf/residual_scale/0.7316728` trimmed-mean 10-90% 推导与 `2πA sx sy/3` 适用域未展开 |
| NO-06 | Noise | `ivar=1/variance` 未说明 `variance_floor=1e-12` 注册表与 degenerate `r=1,ivar=0` 分支 |
| NO-07 | Noise | `fixed conservative` 掩膜半径"未来 PSF-aware adaptive 需扩展 API"前瞻未标 NOT GUARANTEED |
| NO-08 | Noise | `α²v` 与 Drizzle `Σv_j w²/D²` 未互引 |
| NO-09 | Noise | MAD 常数 `1.4826022185` vs 实现 `1.482602218505602` 截断未声明 |
| NO-10 | Noise | `residual_scale` trimmed-mean `0.731673` 与 `robust_residual_sigma` 推导未展开 |
| DRZ-05 | Drizzle | operation_counts 13 项命名微分叉（文档 `geometry_cache_hits` vs 代码 `op_geometry_cache_hits`） |
| DRZ-06 | Drizzle | 失效域错误码未进 TRACEABILITY（缺 `ERR-DRZ-*` 回填） |
| DRZ-07 | Drizzle | 球面几何三阈值（wcs `1e-11` / hp `hp_res*1e-6` / planar `1e-3 rad` fast path）分散未集中 |
| DRZ-08 | Drizzle | HEALPIX_MAPPING.md 仅35行，无输入/输出/不变量/伪代码/复杂度/oracle（缺 B2-08 模板） |
| PH2-04 | Phase2 | 排异 `P2_REASON/P2_STATUS` 已全集合一致，建议补 `rejection.cpp:1501` large_scale 行号索引 |
| PH2-05 | Phase2 | integration 零权重合同 `w==0 continue` + `ZERO_VALID_WEIGHT` 已正确，建议引 `integrate.cpp:48,65` |
| PH2-06 | Phase2 | Ownership/Threading/Error 契约一致（块级 OpenMP + dense hash）无改动 |
| PH2-07 | Phase2 | UPM 持久化唯一 AIO 原子写路径一致 |
| PH2-08 | Phase2 | frame_id 分层对照 `coverage string basename` vs `payload uint64 SHA-256` 建议在 DATA_SEMANTICS 加表 |
| IO-004 | IO | HiPS 几何 V11 冻结一致，建议增"低阶 tiles 由 NESTED 序直接聚合"句 |
| IO-005 | IO | FP64/FP32 双精度 `bitpix -32/-64` + `astrocs_signal_dtype` 一致，建议补 `data_type 决定存储精度，计算一律 double` |
| IO-006 | IO | `AIO_HIPS_PRODUCT_ALL=7` vs `ALL_V19=31` 向后兼容易混，建议在 COMPATIBILITY 加版本小节 |
| IO-007 | IO | PipelineEngine 顺序 stage+内部并行已合规，建议在 THREADING_MODEL 补调度句 |
| ARC-005 | Architecture | ERROR_MODEL 已与 `orchestrator.h:111-134` 逐值一致，建议补 `ERR-P2-UPM-001` 行锚点 |
| ARC-006 | Architecture | CACHE_POLICY 4 缓存四要素已表格化，建议补 `gaia_client.c` 键校验锚点 |
| ARC-007 | Architecture | PERFORMANCE_MODEL 热路径无 per-pixel alloc 合规，建议补 `<5%回归` 阈值句 |
| ARC-008 | Architecture | MODULE_MAP 14 行 vs lib 13 顶级（`tools` 口径差1非缺陷），healpix_db 三子项拆分 |
| ARC-009 | Architecture | PUBLIC_API 16 符号 `p2_*/aio_*` 均存在，`p2_reject_stack` 为 COMPAT adapter 建议加兼容句 |
| STD-004 | Standards | CODE MUST HiPS tiles 非原子跨域复述（P2 计，避免与 IO-001 重复 P1） |
| STD-005 | Standards | C_ABI 边界已全包裹（`acr_kernels.cpp:42 throw` 经上层 catch），建议加 throws 注释 |
| STD-006 | Standards | CONCURRENCY `omp_set_num_threads` 0命中合规，ACR 调度互斥已合规 |
| STD-007 | Standards | NUMERIC `quiet_NaN`/`area>0&&finite` 守卫合规，`aio_xisf/fits` 的 `w*h*c` 未 `checked_mul` 属 SHOULD |
| STD-008 | Standards | IO `fits_write_chksum`/`model_hash` 合规，仅 HiPS partial-file 策略未显式 |
| STD-009 | Standards | LOGGING `run/logs/orchestrator` 路径合规，未显式每 stage `frame_id` |
| STD-010 | Standards | TEST 用公共生产 API + 82 项 synthetic_gate 合规，随机 seed 未显式注释 |

---

## 7. B1..B5 归口映射表

| 桶 | 任务 | 承接发现 | 产出/门禁 |
|----|------|----------|-----------|
| **B1 科学层** | **B1-01** SCOPE 文档化 | SC-03 | `SCIENCE_SCOPE.md` 补 `SCI-SCOPE-*` 合同段 + `TRACEABILITY.csv` 增行 + 重跑 `tools/docs_machine_consistency.py` |
|  | **B1-02** CAL 校准 | SC-01(P0), SC-04, SC-07, SC-08 | `CALIBRATION.md` 失效域/有效域/分组/ivar传播 + `lib/calibration/src/calibrator.cpp` 钳位→显式拒绝或文档对齐 |
|  | **B1-03** AST+PSF | SC-05, SC-06, SC-09, SC-11, SC-12 | `PSF.md/ASTROMETRY.md` 阈值/椭率/四象限 + `STAR_PSF_ALGORITHMS.md` 高斯路径留痕 |
|  | **B1-04** PHOT | SC-02(P0), SC-10 | `PHOTOMETRY.md` IRLS+Tukey 细节与 median 区分 + 饱和/mag_tolerance 来源 |
|  | **B1-05** NOISE_MODEL | NO-01(P0), NO-06..10 | `NOISE_MODEL.md` gain隔离段 + floor/degenerate/0.731673/MAD 常数截断 |
|  | **B1-06** UNCERTAINTY | NO-02, NO-08, DRZ-02(协办) | `UNCERTAINTY_AND_COVARIANCE.md` k_corr 域边界 + α²v 互引 + Drizzle缓冲分层 |
|  | **B1-07** DRIZZLE 科学 | DRZ-03, DRZ-02(协办) | `DRIZZLE.md` variance分子/归一化在 sink/writer 段 + 缓冲系数分层 |
|  | **B1-08** PHASE2/UPM | PH2-01..03 | `PHASE2_UPM.md/PHASE2_SAMPLER.md/UPM_SOLVER.md` 权重分层 + payload float bytes + per-frame k_corr |
| **B2 算法层** | **B2-07** DRIZZLE_GEOMETRY | DRZ-02, DRZ-05..07 | `DRIZZLE_GEOMETRY.md` 缓冲分层 + 13项计数命名 + 阈值总表 |
|  | **B2-08** HEALPIX_MAPPING | DRZ-08 | `HEALPIX_MAPPING.md` 输入/输出/不变量/伪代码/oracle(1e6) |
|  | **B2-09** PHASE2_SAMPLER | PH2-03 | `PHASE2_SAMPLER.md` per-frame k_corr 优先级与回退 |
|  | **B2-10** UPM_SOLVER | PH2-02 | `UPM_SOLVER.md` payload float32 LE 注 |
|  | **B2-11/12** REJECTION/INTEGRATION | PH2-04..05 | `REJECTION_ALGORITHMS.md/INTEGRATION_ALGORITHMS.md` 行号索引 |
| **B3 架构/L3** | **B3-02** MODULE_MAP | ARC-002/008 | healpix_db 三子项拆分（active/optional/archived） |
|  | **B3-03** DEPENDENCY_RULES | ARC-002 | 头文件无环 vs 实现层 `phase2/src→aio` 区分 |
|  | **B3-04** PIPELINE/DATA_FLOW | IO-003 | frame_id/manifest 在 PIPELINE 末段声明 + PipelineFrame KV 承载说明 |
|  | **B3-05** OWNERSHIP | ARC-003 | dense guard + `delete[]` 责任具名 |
|  | **B3-06** THREADING | ARC-004/006 | 确定性锚点 + ACR互斥锚点 |
|  | **B3-07** ERROR_MODEL | DRZ-06/ARC-005 | `ERR-DRZ-*` + `ERR-P2-UPM-001` 回填 |
|  | **B3-08** IO/ATOMICITY+CACHE | IO-001/002/ARC-001/006 | UPM已原子段更新 + HiPS tiles 原子或 partial 策略 + variance Σarea 复用 + Gaia 键锚点 |
|  | **B3-09** COMPATIBILITY | IO-006/ARC-009 | `ALL=7` vs `ALL_V19=31` 版本小节 + PUBLIC_API 兼容句 |
|  | **B3-10** PERFORMANCE | ARC-007 | 基线 `<5%` 阈值句 |
| **B4 实现** | **B4-01** 去重 | **DRZ-01(P0)** | 单源 HEALPix + deprecated shim + `grep ang2pix` 门禁 |
|  | **B4-03** HiPS原子化 | IO-001/ARC-001/STD-003/004 | `aio_hips_writer.cpp write_fits_image_tmp` 或文档策略 + 测试容差常量化 |
|  | **B4-04** 精度/命名 | IO-002/005, STD-007 | `add_var→add_var_num` 改名 + dtype 注释 + `checked_mul` 注释 |
|  | **B4-20..28** 标准批 | STD-001..010 | 轮次词/废话/容差/C_ABI/日志/测试 seed 等 |
| **B5 追溯/门禁** | **B5-06** TRACEABILITY | SC-03, NO-03/05, DRZ-04, IO-003, PH2-08 | 增 `SCI-SCOPE-*`/`NOISE-WIRE-001`/`TEST-DRZ-CAND-001`/`DATA-FRAME-ID-001` implementation_symbols + `DATA_SEMANTICS` 分层对照 |
|  | **A1-02补齐** | NO-04 | `tools/file_audit` 找回 + `standards_violations.json` 行级证据闭环 |

> 合计 B1 8 项 / B2 6 项 / B3 9 项 / B4 4 批 / B5 1 批 + A1-02 1 项，与 64 项发现全映射（P0 4 项分属 B1-02/B1-04/B1-05/B4-01）。

---

## 8. 下游依赖（A Gate → B）

```
A Gate (本总表, 64项, P0=4, 9/9 PASS)
  ├── 需 P0 冻结后再入 B2: DRZ-01(B4-01) → B2-08(HEALPIX_MAPPING) 阻塞
  │                SC-01(SC-02/NO-01)(B1-02/04/05) → B1 完成后方可 B2 模板化
  ├── B1 科学层 (8): B1-01(SCOPE) ─┐
  │              B1-02(CAL)         ├─→ B3-08(IO) + B5-06(追溯) → 重跑 machine_consistency 0 broken
  │              B1-03(AST/PSF)     │
  │              B1-04(PHOT)        ├─→ B4-03/04(实现) 需科学冻结先行
  │              B1-05(NOISE)       │
  │              B1-06(UNCERTAINTY)─┘
  │              B1-07(DRIZZLE) ─→ B2-07(GEOMETRY)
  │              B1-08(PHASE2)  ─→ B2-09/10 + B3-04(PIPELINE)
  ├── B2 算法层 (6): 依赖 B1 冻结，产出 L2 算法文档，门禁为"每阈值/常数有 file:line"
  ├── B3 架构层 (9): 依赖 B1/B2 口径，产出 L3 修正，门禁为 docs_machine_consistency 9/9 + dependency 无环
  ├── B4 实现层 (4批): 依赖 B1/B3 冻结
  │              B4-01(HEALPix去重) 阻塞 B2-08 与全量 Drizzle 回归
  │              B4-03(HiPS原子化) 阻塞 C 阶段 `test_writer_integration` + `pipeline_frame_contract_test`
  │              B4-20..28(标准批) 可与 B3 并行，但需在 C 前清零 COMMENT/CODE 违规
  └── B5 追溯层 (1批+A1-02): 全程可并行，但 B5-06 必须在 B 末重跑 machine_consistency 0 broken 并生成 D 阶段 SHA
```

**入 B 条件（G-QA A→B）**:
- P0 4 项修复并经 `tools/docs_machine_consistency.py` + `candidate_oracle 9003例` + `synthetic_gate UPMW/PR` 复核通过；
- `file_audit` 工具找回（NO-04）后 `total_files` 与 `source_files_scanned` 复核；
- 本总表 + `audit_stats.json(汇总版)` + `evidence/QA-V19R7-A2-08/EVIDENCE_INDEX.md` 已落盘且 `git add` 可提交（见§9）。

---

## 9. 证据与可复现

- **输入**: 7 域分表（`reports/v19r7_quality/audit_findings_*.md` 各含概述/方法/发现表/统计+evidence索引）+ `reports/v19r7_quality/machine_consistency_before.json`(9/9) + `file_audit_before.json`(873) + `standards_violations.json`(7) + `audit_stats.json(before)` + `docs/TRACEABILITY.csv`(63行)
- **产出**: 本文件 `reports/v19r7_quality/audit_findings.md`；`reports/v19r7_quality/audit_stats.json`(汇总版，含 before 保留)；`reports/v19r7_quality/evidence/QA-V19R7-A2-08/EVIDENCE_INDEX.md`；对偶证据 `evidence/QA-V19R7-A2-08/EVIDENCE_INDEX.md`（若仓库要求双路径）
- **校验**:
  ```
  ls -l reports/v19r7_quality/audit_findings.md reports/v19r7_quality/audit_stats.json
  sha256sum reports/v19r7_quality/audit_findings.md reports/v19r7_quality/audit_stats.json
  cat reports/v19r7_quality/audit_stats.json | python3 -m json.tool
  git add reports/v19r7_quality/audit_findings.md reports/v19r7_quality/audit_stats.json reports/v19r7_quality/evidence/QA-V19R7-A2-08/EVIDENCE_INDEX.md
  git status --porcelain  # 应可提交
  ```
- **不改动**: `docs/` `lib/` 零修改（`git diff -- docs lib` 为空）

---

## 10. 限制与遗留

- `DRIZZLE.md`/`HEALPIX_MAPPING.md` 的缓冲系数分层与 `3.0×`/`1.25×+1.15` 需 B2-07 以 `spherical_overlap.cpp:40,1383` 为准同步，BASS 全量前需 `k_corr` MC 复核。
- `frame_id` payload 含 `float32` 裸 bytes 的跨 endian 稳定性仅在 x86_64 生产路径验证，跨平台 id 稳定性需 B1-08 显式文档化。
- `standards` 的 `NUMERIC/CONCURRENCY` 未做 `clang-tidy`/`TSan/ASan` 运行时全量，建议 C 阶段 G-QA-07 矩阵复核。
- `file_audit` 工具缺失导致 873/~713+ 覆盖度为替代统计，需 A1-02 找回后重算 shipping 分母。

---

*汇总: resident:project（只读汇总 7 域分表，不改 docs/lib）| 证据: `reports/v19r7_quality/evidence/QA-V19R7-A2-08/EVIDENCE_INDEX.md` | 下游: B1-01..08 / B2-07..12 / B3-02..10 / B4-01/03/04/20..28 / B5-06 → C 阶段*

