# QA-V19R7 A2-01 Science 域审计分表

> Spec: `工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md` §4.1 A2-01  
> Checklist: `工程控制/checklists/QA_V19R7_QUALITY.md` A2-01  
> 基线: V19R6R2-W1 HEAD 2767874 | 模式: 只读不改 | 审计员: resident:science | 日期: 2026-08-22  
> 范围: `docs/science/SCIENCE_SCOPE.md, CALIBRATION.md, ASTROMETRY.md, PHOTOMETRY.md, PSF.md` vs `lib/calibration, lib/plate_solve/cpp/ipv, lib/dynamic_psf, lib/photometric_calib, lib/gaia_xpsd_client`  
> 追溯: `docs/TRACEABILITY.csv` SCI-* 行 | 机器扫描: `reports/v19r7_quality/machine_consistency_before.json` (broken=0, 待复核已完成)

---

## 1. 概述

本分表对 science 五份文档的“公式/单位/假设/失效域/有效域/不保证”逐节对照 lib 实际实现。判定分级：

- **P0 不一致 (blocking)**: 文档公式/语义与代码行为可观测分叉，或失效域/错误码对用户可见的差异。
- **P1 缺失溯源 (must-fix)**: 合同 ID 未在 TRACEABILITY 或文档/代码中建立完整 Requirement→Impl→Test 链，或关键参数未归档。
- **P2 表述不清 (suggestion)**: 单位/假设/边界描述存在歧义，虽不立即导致错误但影响四层统一和可维护性。

审计覆盖 5 份文档约 62+53+54+54+58=281 行正文，对照约 7 个实现目录 60+ 文件。machine_consistency 已就绪 (0 broken)，本表不再标注“待 machine_consistency 复核”。

**结论先行**: 共发现 **P0: 2, P1: 4, P2: 6**，合计 12 项。无阻断性科学等价误改，但有 2 项 P0 需在 B1 科学层改正中以“文档向代码对齐或代码钳位改为显式错误”方式收口。

---

## 2. 方法

1. 逐文档标注“科学定义/变量/单位/假设/有效域/不保证/失效条件/数值精度/ID”九节。
2. 对每节检索 `lib/*` 对应符号：C ABI 头 `astro_calibration.h`, `photometric_calib.h`, `dynamic_psf.h`, `ipv_wcs.h`, 核心 `calibrator.cpp`, `dpsf_psf.cpp`, `pc_api.cpp`, `star_matcher.cpp`, `spectrum_integrator.cpp`, `gaia_client.c`。
3. 以 `grep -rn` 与文件行号锚定，对比公式、常数、分支、错误码、单位换算。
4. 查 TRACEABILITY.csv 中 `SCI-CAL-001, SCI-AST-001, SCI-PHOT-001, SCI-PSF-*, SCI-SCOPE-*` 行的 requirement→impl→test 闭环。
5. 分级后关联合同 ID 与建议归口 (B1-01..04)。

---

## 3. 逐文档发现表

| # | 级别 | 合同 | 文档节 | 代码位置 | 描述 | 建议归口 |
|---|------|------|--------|----------|------|----------|
| SC-01 | **P0** | SCI-CAL-001 | CALIBRATION.md §失效条件 “除零（flat_norm=0）→ 显式拒绝” | `lib/calibration/src/calibrator.cpp:90,120,164` `normalize_flat` | 文档约定 flat=0 为显式拒绝；实现为 `std::max(flat,0.1f)` 静默钳位 (flat_norm 最小 0.1) 并继续流水，无错误码返回。用户无法区分“平场缺失/损坏”与“正常校准”帧。可观测语义分叉。 | B1-02 |
| SC-02 | **P0** | SCI-PHOT-001 | PHOTOMETRY.md §科学定义 `sigma_mag = 2.5×MAD(r_inlier)/0.6745` 与 `r_i=log10(F_instr/F_syn)` | `lib/photometric_calib/cpp/src/star_matcher.cpp:21,478-525,552-559` | 文档暗示以 **median** 为位置估计；实现以 **IRLS + Tukey biweight (c=4.685, 50 iter, 1e-6)** 求 `location`，再以 `MAD(r_inliers)/0.6745` 求 `sigma_residual`。`location` 不等于 median，`scale=10^{-location}` 亦非文档描述的“median(r)”。算法细节与文档公式不一致，QA 复现会偏离。 | B1-04 |
| SC-03 | **P1** | — (SCI-SCOPE 缺行) | SCIENCE_SCOPE.md 全文 ID 段 | `docs/TRACEABILITY.csv` 全表 63 行 | TRACEABILITY 无 `SCI-SCOPE-*` 合同行；SCIENCE_SCOPE 被视为 L1 入口但未进入追溯矩阵，导致 “科学定义=算法=接口=代码” 链在 scope 层断开。machine_consistency 当前报 0 broken 是因 scope 未被建模，并非已覆盖。 | B5-06 / B1-01 |
| SC-04 | **P1** | SCI-CAL-001 | CALIBRATION.md §有效域/§系统误差 “masterDark按曝光分组 / masterFlat按滤镜 / 母版σ计入ivar” | `lib/calibration/src/master_generator.cpp` + `lib/calibration/include/astro_calibration.h:33-57` + `lib/calibration/src/photometry_apply.cpp` | “按曝光/滤镜分组”与“母版噪声传播至 ivar”在 lib/calibration 层无实现：分组由 `orchestrator / testdata` 侧保证，ivar 由 `snr_estimator` 独立估计，不含母版方差项。文档声称的系统误差传播无代码/测试支撑，溯源链缺 `TEST-CAL-*` 对应断言。 | B1-02 |
| SC-05 | **P1** | SCI-PSF-001 / ALG-STAR-PSF-* | PSF.md §数值精度 “Levenberg-Marquardt 类求解（已审计，V19 移除未用高斯路径）” | `lib/dynamic_psf/src/dpsf_psf.cpp:98-182 (lm_solve)` + `docs/algorithms/STAR_PSF_ALGORITHMS.md` | 所谓“已移除高斯路径”无文件/提交锚点；`dpsf_psf.cpp` 仅保留 LM，未见被删路径的留痕或 TRACEABILITY 标注。属于追溯缺失，影响审计可重现。 | B1-03 |
| SC-06 | **P1** | SCI-AST-001 | ASTROMETRY.md §有效域 “极区有 provably-conservative prune（V18R3）” | `lib/plate_solve/cpp/ipv/src/ipv_select.cpp`, `ipv_polygon.cpp` + `lib/gaia_xpsd_client/src/gaia_client.c` (RA环绕) | 极区 prune 在 IPV 侧有实现但未在 ASTROMETRY.md 给出几何阈值/前提（如 `|dec|>85°` 时的保守性证明引用）；`gaia_xpsd_client` RA 环绕处理 (`dra>180→360-dra`) 亦未回链到 ASTROMETRY 失效条件。 | B1-03 |
| SC-07 | **P2** | SCI-CAL-001 | CALIBRATION.md §科学定义 `cal=(raw−bias−dark·t_expo)/flat_norm` | `lib/calibration/src/calibrator.cpp:104-136` `calibrate()` | 公式中 `dark·t_expo` 未说明 dark 是否已含 bias；代码分两支：`dark_opt==0: (light−dark)/flat` (Dark含Bias)，`dark_opt==1: (light−bias−K*(dark−bias))/flat`，`K=t_light/t_dark`。文档未区分两模式，易误读为连续曝光缩放而非 bias 差分缩放。 | B1-02 |
| SC-08 | **P2** | SCI-CAL-001 | CALIBRATION.md §变量/单位 “flat_norm 为归一化平场（均值或中值=1 约定，模块文档记录具体选择）” | `lib/calibration/src/calibrator.cpp:78-93` | 代码确定为 **median=1.0** 且 `<0.1 clamp`；但 science 层仍写“均值或中值”，未冻结到 median。L1 与 L2/L3 存在表述分叉。 | B1-02 |
| SC-09 | **P2** | SCI-AST-001 | ASTROMETRY.md §变量/单位 “x,y：像素（0基）；CD：deg/pixel；SIP：多项式” + §科学定义 `ra,dec=TAN(CD·(x−x0,y−y0)+SIP)` | `lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp:276-277,154,276` | 代码 CRPIX 为 **1-based FITS** (`width/2+0.5`)，与文档 “0基 x,y” 及 “x0,y0” 混用；SIP 前向 `A/B` 与逆向 `AP/BP` 的 `cd_inv` (pixel/arcsec) 换算在文档未展开，Y-down 转换 (`cd12,cd22` 取反 + SIP `(-1)^j`) 仅在代码注释 529-541 行，无文档说明。 | B1-03 |
| SC-10 | **P2** | SCI-PHOT-001 | PHOTOMETRY.md §假设/§失效条件 “大气/仪器零点…亮星非饱和” | `lib/photometric_calib/cpp/src/pc_api.cpp:72-98` + `star_matcher.cpp:241-248` | “非饱和”在代码中以 `psf_status==0` 且 `qf & SATURATED` 判别 (snr_psf_fit_quality 阶段)，但 PHOTOMETRY.md 未定义饱和判据与星等容忍 `mag_tolerance=3.0` 的来源，`sigma_residual` 的 MAD=0 显式处理 (snr_estimator 侧) 亦未回链。 | B1-04 |
| SC-11 | **P2** | SCI-PSF-001 | PSF.md §科学定义 Moffat4 `I(r)=B+A/(1+Q)^4` | `lib/dynamic_psf/src/dpsf_psf.cpp:13-18,81-83,144` | 文档为各向同性 σ 模型；代码为椭圆各向异性 `sx,sy,theta` 且 `Q = p1 dx²+2p2 dxdy+p3 dy²`，`FWHM=1.2303·σ` 推导亦仅在代码注释。文档未说明椭率 `eccentricity` 与 `theta` 四象限消歧 (代码 351-363)，L1 不完整。 | B1-03 |
| SC-12 | **P2** | SCI-PSF-001 | PSF.md §PsfFitQuality / PHOTOMETRY.md §系统误差 | `lib/snr_estimator/cpp/src/noise_model.cpp:35-37,309-324` + `lib/dynamic_psf/include/dynamic_psf.h:30-31` | 文档正确声明 `q_psf=A/residual_scale` 与 `residual_scale/0.7316728` 为 Gaussian 假设，但未说明“ trimmed mean 10-90% ”的 `0.731673` 推导前提 (见 noise_model.cpp:36) 与 `flux = 2π A sx sy/3` 解析积分 (dpsf_psf.cpp:368) 的适用域。 | B1-03 |

*注: P0 判定遵循“可观测分叉即 P0”。SC-01 flat 静默钳位与 SC-02 IRLS 细节是唯一两项直接影响输出或 QA 复现的 science 层分叉。*

---

## 4. 统计小结

| 级别 | 数量 | 占比 | 主要分布 |
|------|------|------|----------|
| **P0 不一致** | **2** | 16.7% | CALIBRATION 失效域、PHOTOMETRY 清洗算法 |
| **P1 缺失溯源** | **4** | 33.3% | SCOPE 追溯缺行、CAL 分组/传播、PSF 审计留痕、AST 极区阈值 |
| **P2 表述不清** | **6** | 50.0% | 单位/基坐标/SIP/Moffat 椭率/饱和判据 等 |
| **合计** | **12** | 100% | — |

- **按文档**: CALIBRATION 4项 (1P0+1P1+2P2), ASTROMETRY 3项 (1P1+2P2), PHOTOMETRY 3项 (1P0+2P2), PSF 3项 (1P1+2P2), SCOPE 1项 (1P1)；另跨文档 2 项。
- **按归口**: B1-01 (SCOPE) 1项, B1-02 (CAL) 4项, B1-03 (AST+PSF) 6项, B1-04 (PHOT) 2项。
- **machine_consistency**: `docs/TRACEABILITY.csv` 63 行 0 broken (A1 已就绪)；但 SCOPE 未建模导致覆盖度虚高，需 B5-06 补 `SCI-SCOPE-*` 行并重跑 `tools/docs_machine_consistency.py`。
- **风险**: 2 项 P0 不阻塞真实数据冒烟，但阻断“四层统一 0 broken”与审计闭环；建议 B1 阶段优先收口 SC-01/SC-02。

---

## 5. 证据与追溯

- 输入: `docs/science/*.md` (5), `lib/calibration/*`, `lib/plate_solve/cpp/ipv/*`, `lib/dynamic_psf/*`, `lib/photometric_calib/*`, `lib/gaia_xpsd_client/*`, `docs/TRACEABILITY.csv`, `reports/v19r7_quality/machine_consistency_before.json`
- 产出: 本文件 `reports/v19r7_quality/audit_findings_science.md` + `evidence/QA-V19R7-A2-01/EVIDENCE_INDEX.md`
- 方法: 逐节对照 + 符号表 + 行号锚定 + TRACEABILITY 闭环检查
- machine_consistency: 0 broken (A1-01 已生成 `machine_consistency_before.json` / `traceability_broken.json`)，本表已完成人工复核。

