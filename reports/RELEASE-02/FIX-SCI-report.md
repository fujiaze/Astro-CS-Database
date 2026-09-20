# RELEASE-02 / FIX-SCI 冻结科学文档变更 claim 执行报告

- 任务：`工程控制/RELEASE-02/tasks/FIX-SCI.md`
- 执行者：FIX-SCI SubAgent；工作目录 `/workspace/Astro CS Database`
- 日期：2026-09-18
- 纪律：零 git 写操作；未跑 `ninja`/`cmake`/全量 `ctest`；只跑轻量 Python 检查器；`TMPDIR=/dev/shm/astrocs_sci`（源码树外）
- 科学立场：独立证据（一手文献 DOI/式号 + 开源 文件:行 + 独立复算）优先，不盲从文档也不盲从现有代码

## 0 摘要

| 项 | 结论 | 落地 |
|---|---|---|
| P0-1 `DRIZZLE.md` §5/§7 归一化互斥 | §5 的 legacy 归一 `w=a/A_drop` 给 `S_p=B0/pixfrac²`，与 §7 的 `S_p=B0` 互斥；**§5 错**（Fruchter & Hook 式(5) 是一致加权均值，drop 面积相消）。订正 §5/§7/§11 为面亮度保持归一 `S_p=Σ_j B_j a_jp/Σ_j a_jp` | 文档已改；claim `FIX-SCI-DRZ-001`；实现仍 legacy，登记 **DISP-DRZ-009**（未改代码） |
| P0-2 `ASTROMETRY.md` §5a 1px 平移口径 | §5a 旧"0-based 内部约定 / 常量 1px 平移 / 不是数学内容差"是**标签错误**；求解器拟合自变量是 sdet 半整数像素中心 `det_x=i+0.5`，`u=det_x−w/2=p−CRPIX=q`，输出 `x=u+CRPIX=p` 已是 **1-based FITS**，**实现与 Paper I 逐式一致、无 1px/0.5px 误差** | 文档已改；claim `FIX-SCI-WCS-001`；建议 comment-only 修正 `ipv_wcs.h` 注释 + 测试面 relabel（未改代码） |
| 第 3 项 SCI-S2 34 项复核 | 冻结科学文档中：2 项 P0（上表）+ 3 项 doc-only 已落地（PHOTOMETRY 行锚/F_syn、PHASE3 §9a-10）；其余涉**实现常数/代码**或属 UNRESOLVED，登记待裁决 | claim `FIX-SCI-S2-PHOT-001`、`FIX-SCI-S2-P3-001`；详见 §5 |

## 1 交付物

- 变更 claim：`工程控制/RELEASE-02/change-claims/FIX-SCI-DRZ-001.md`、`FIX-SCI-WCS-001.md`、`FIX-SCI-S2-PHOT-001.md`、`FIX-SCI-S2-P3-001.md`
- 文档订正：`docs/science/DRIZZLE.md`、`docs/science/ASTROMETRY.md`、`docs/science/PHOTOMETRY.md`、`docs/science/PHASE3_HIPS_TO_FITS.md`、`docs/algorithms/DRIZZLE_GEOMETRY.md`、`docs/standards/STANDARDS_REGISTRY.md`、`docs/plugins/algorithms_phase1/08_drizzle.md`
- 本报告

---

## 2 P0-1：DRIZZLE §5/§7 归一化互斥

### 2.1 问题

`DRIZZLE.md` §5 写 `w_jp=a_jp/A_drop,j`、`F_p=Σ x_j w_jp`、`D_p=Σ a_jp`、`S_p=F_p/D_p`；§7 断言常数面亮度场 `B0` ⇒ `S_p=B0`。令 `A_drop,j=pixfrac²·A_pixel,j`、`x_j=B0·A_pixel,j`，§5 给 `S_p=B0/pixfrac²`（pixfrac=0.8 → 1.5625×，0.5 → 4×），仅 pixfrac=1 与 §7 一致。RELEASE-01 `GAP_AUDIT.md:108` 登记为 P0。

### 2.2 证据链

- **一手文献**：Fruchter & Hook 2002, PASP 114, 144（DOI `10.1086/338393`；arXiv:astro-ph/9808087v2 §2 式(2)–(5) 逐字核验）：`I_p = [Σ_i d_i a_ip w_i s²]/[Σ_i a_ip w_i]`，`s²=A_out/A_in` 用于保面亮度；**同一权重 `a_ip·w_i` 同入分子分母**（一致加权均值），`A_drop` 在均匀 drop 尺度下相消。常数面亮度 `d_i=B0·A_in` ⇒ 输出面亮度 `=B0`，与 pixfrac 无关。
- **DrizzlePac Handbook §2.3.2（p.17）**："the weights of the individual output pixels … are independent of the choice of p [pixfrac]"。
- **开源对照（只读）**：drizzlepac `src/cdrizzlebox.c` `update_data()`（`output=(output·vc+dow·d)/(vc+dow)`）+ `do_kernel_square()` `dover/=jaco`（`jaco=A_drop`）后 `dow=dover·w`——drop 面积同入分子分母；SWarp `src/resample.c` L845-847/L875 以 `A_out/A_in` 面积比保面亮度（无 drop/pixfrac）。
- **独立代数复算**：legacy `S_p=B0/pixfrac²`；目标 `S_p=Σ_j B_j a_jp/Σ_j a_jp=B0`（全 pixfrac）；`w_SB=a_jp/A_pixel,j=pixfrac²·w_legacy`；`Var(S_p)=Σ_j v_j w_SB,jp²/D_p²`（legacy 方差额外 ×`1/pixfrac⁴`，SNR 不变）。
- **本仓权威目标态**：`docs/design/PHASE1_DETAILED_DESIGN.md:120-126`、`docs/plugins/algorithms_phase1/08_drizzle.md:28-29`、`docs/contracts/v6/data/02_signal.md:38-46`（`FZ-FORMULA-DRIZZLE-SB`/`FZ-COND-FLUX-CONSERV`/`FZ-GATE-CONST-SB`）、裁决 SUP-02/03（`docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json:2554-2580`）——**均与文献一致，唯 §5 落后**。
- **实现现状**：`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1531` `weight=overlap_area/drop_area`、`:1553-1554`；`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:707` `sig=flux/area`——legacy。

### 2.3 结论与改动

订正 `docs/science/DRIZZLE.md`：§2 符号表 `w_jp=a_jp/A_pixel,j`、新增 `A_pixel,j`；§3 单位；§5 重建式改为 `B_j=x_j/A_pixel,j`、`S_p=Σ_j B_j a_jp/Σ_j a_jp` + 式(5) 等价形式 + legacy 关系；§5 方差权重一致性注；§7 通量守恒改为条件不变量 `Σ_p F_p=pixfrac²·Σ_j x_j`、常量场 `S_p=B0` 声明全 pixfrac；§10 增 legacy 禁用；§11 Oracle 改 `FZ-GATE-CONST-SB`；§14a 补式(5)+drizzlepac/SWarp/Handbook 对照。同步 `docs/algorithms/DRIZZLE_GEOMETRY.md`（登记 DISP-DRZ-009 + 最小修复）、`docs/standards/STANDARDS_REGISTRY.md`（§3 CONFORMANT→PARTIAL + DISP-DRZ-009）、`docs/plugins/algorithms_phase1/08_drizzle.md`（legacy 禁用注 + 锚点）。

### 2.4 实现发现（未改代码，登记 DISP-DRZ-009）

最小修复：`drizzle_engine.cpp:1531` `weight = overlap_area / drop_area` → `weight = overlap_area * (pixfrac*pixfrac) / drop_area`（等价 `overlap_area/A_pixel,j`，因 `A_drop,j=pixfrac²·A_pixel,j`）。`pixfrac=1`（默认 `config/defaults.json`）数值逐位不变。
**验证命令（前台）**：`ctest -R 'p1drz|drizzle'`，重点 `p1drz` 常量面亮度门（`FZ-GATE-CONST-SB`，全 `pixfrac∈(0,1]`）、`variance_propagation_test`、`candidate_oracle_test` 9003 例；负向：常量 ADU 构造 / `S_p=F_p` / legacy `w=a/A_drop` 必红。

---

## 3 P0-2：ASTROMETRY §5a 像素原点口径

### 3.1 问题与结论

§5a 旧文称求解器内部为"0-based 自洽约定 `u=x−CRPIX`、参考像素 0-based `x=CRPIX`、与标准差**常量 1px**、且**不是数学内容差**"。**该叙述是标签错误**：求解器拟合自变量是 sdet **半整数像素中心** `det_x=i+0.5`，`u=det_x−w/2=(i+1)−(w/2+0.5)=p−CRPIX=q`（Paper I §2.1.1），迭代反演 `x=u+CRPIX=p` 返回 **1-based FITS 像素**。**实现与 Paper I 逐式一致，无 1px（也无 0.5px）误差。**

### 3.2 证据链

- **Paper I**：Greisen & Calabretta 2002, A&A 395, 1061（DOI `10.1051/0004-6361:20021326`，arXiv:astro-ph/0207407 §2.1.1 式(1)、§2.1.4）：`q_i=Σ m_ij(p_j−r_j)`，`p_j` 1-based、整数像素号=像素中心；参考像素在 `p=CRPIX`。
- **本仓代码**：`lib/algorithms/star_detection/src/sdet_api.cpp:546-549`（`dx=x+0.5−cx`）；`lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp:943,947`（`cx=img_w/2`，`U=det_x−cx`）；`.../ipv_wcs.cpp:158-162`（`CRPIX=cx+0.5`）、`:942-946`（`out.x=u+crpix`）。
- **第三方复算（2026-09-18）**：astropy 7.0.1 `all_pix2world([[512.5,512.5]], origin=1)` 与 `[[511.5,511.5]], origin=0` 同给 CRVAL ⇒ `out.x=512.5` 是 origin=1（1-based）对象；fixture（`run/p1wcs_wcs003/wcs003_cross_input.json`）独立 TAN+SIP 复算：生产前向与 `u=x_f−crpix` 逐位一致，0-based 解释差 1.65–2.45e-4 deg。
- **E2E 外部闭环**（`docs/algorithms/GATES_AND_TOLERANCES.md` §3）：T4 GC panel1 `s0=6.31″/px` solved median **0.1077 px**、match 31.32%（任何 0.5px 平移会 ≥3.15″、match≈0）；T2/T3 solved−header ≈0.06 px ⇒ 排除 0.5/1px 系统差。
- **开源对照**：astrometry.net `util/anwcs.c`（`CRPIX=W/2+0.5`）、`util/sip.c`（`u=px−crpix` 后 `px=U+crpix`）同约定；astropy `origin` 语义；WCSLIB 为 Paper I 参考实现。
- **既有登记**：`run/RELEASE-01/science/SCI-S2-topics.md` §3-2（WCS-003-F1）自述"当前流水线无实际 1px 错误 … 冻结合同条款标签错误是潜在隐患"；`工程控制/RELEASE-01/GAP_AUDIT.md:109`；M1a-C-003。

### 3.3 改动

订正 `docs/science/ASTROMETRY.md` §5a：拆"标准口径（Paper I）"与"求解器口径（与 Paper I 一致）"；给出 `det_x=i+0.5`、`u=p−CRPIX=q`、`x=u+CRPIX=p` 推导；删除"常量 1px/不是数学内容差"；明确真正 0-based 的只有 p1_sources 整数下标（astropy `origin=0`）与 p3 产品网格下标（`fits_pixel_1based`）；修正 `ipv_wcs.cpp:942-946` 锚（原 `:869-872` 是 TAN 投影）；§14a 与文件头同步。同步 `docs/standards/STANDARDS_REGISTRY.md` STD-F1 三处标签（§2.2 条款行、§3 偏差索引、D.drizzle 无涉）由"ipv 内部 0-based"改为"求解器输出即 1-based FITS `p`"。

### 3.4 实现发现（未改代码）

1. comment-only：`ipv_wcs.h:43,57-60,70-71` 把 1-based `p` 标为"0-based FITS 像素"。
2. 测试面 relabel：`tests/unit/p1wcs/p1wcs_astropy_cross.py:296-302` 对已是 1-based 的 `x_f` 用 `origin=0`+`crpix+1`（等价 `origin=1`+原 `crpix`）；建议改为 `origin=1` 去桥接以消除错误叙事。
3. **风险 R2（中）**：链路正确依赖"sdet 半整数探测 + `cx=w/2`"与"p1_sources/闭环用 phase1 整数下标"的 0.5 相消，**无测试锁定**。建议跨探测合成测试（同天体过 `StarDetector` 与 `sdet_detect_ex`，断言 `x` 差恰 0.5、重建 `p` 一致）+ 保留 T4 大尺度闭环帧。
**验证命令（前台）**：`ctest -R 'p1wcs|p3wcs'`；真实帧 `closure_metric.py` T2/T3/T4 逐轴均值偏移。

---

## 4 科学正确性裁决（两 P0 的关键判据）

- **DRIZZLE**：以 Fruchter & Hook 式(5) + DrizzlePac Handbook "pixfrac 不改变权重" + drizzlepac/SWarp 源码为准 → **§5 错、§7 对**；实现 legacy 属代码缺陷（DISP-DRZ-009）。
- **ASTROMETRY**：以 Paper I §2.1.1/§2.1.4 + sdet 半整数探测代码 + astropy 复算 + T4 大尺度闭环为准 → **实现对、§5a 旧文错（标签）**；不改代码。

---

## 5 第 3 项：SCI-S2 34 项复核

> 34 项 = RELEASE-01 S2 的"我方有误/需订正"（P0 2 / P1 8 / P2 24），逐条见 `reports/RELEASE-01/science/SCI-S2-topics.md` §2 各主题表 + §3 清单（15 条）+ §6 UNRESOLVED（11 条）。下按"是否冻结科学文档 / 现状 / 处置"分类。

### 5.1 冻结科学文档（`docs/science/**`）

| SCI-S2 项 | 对象 | 现状 | 处置（本次） |
|---|---|---|---|
| P0 DRIZZLE §5/§7 | DRIZZLE.md | 互斥 | **已落地** FIX-SCI-DRZ-001 |
| P0 ASTROMETRY §5a | ASTROMETRY.md | 标签错 | **已落地** FIX-SCI-WCS-001 |
| §3-2 trimmed-mean 常数 0.7316727929211932 → 0.7316730952806139 | PSF.md §2/§9/§9a/§13/§14、NOISE_MODEL.md §14 | 文档与实现同用旧值；**被 `tools/docs_machine_consistency.py` 强制**（`:72,360-361,448,463`） | **未落地**：独立复算确认精确值 `0.7316730952806139`（central-80% of |r| 半正态均值；与 SCI-S2 一致），但改文档须同步 `noise_model.cpp:37`/`snr_science.cpp:39`/`dpsf_psf.cpp` 与一致性检查器，属**代码+文档+检查器协同**，本次不构建故不动（见 §7 风险） |
| §3-3 Moffat4 FWHM 1.230310 vs 精确 1.2303076525901；s_factor 3.7172 vs √(2ln1000)=3.716922 | PSF.md §2/§5/§7/§9a/§14 | 文档把舍入当不变量 | **未落地**（同 §3-2 的代码常数 `MOFFAT4_FWHM_FACTOR` 联动）；独立复算已确认精确值 |
| §3-10 mag_tolerance 行锚 | PHOTOMETRY.md §2/§5 | 标 `star_matcher.cpp:241`（实为 psf_valid 诊断） | **已落地** FIX-SCI-S2-PHOT-001（`pc_api.cpp:139,398`） |
| §3-12 F_syn 绝对刻度表述 | PHOTOMETRY.md §6 | 表述过强 | **已落地** FIX-SCI-S2-PHOT-001（降级为相对刻度） |
| §3-11 / §6-4 PHASE3 §9a-10 与更新块冲突 | PHASE3_HIPS_TO_FITS.md §1/§9a-10 | 同合同互斥 | **已落地** FIX-SCI-S2-P3-001 |
| §3-7 PSF.md §4 BAD 状态 | PSF.md §8 | 现无 "BAD"（`:68-77` 已无该状态） | 无回退（RELEASE-01 已订正） |
| §2-T3 / §6-6 SIP 系数单位与 FITS 头关系 | ASTROMETRY.md §5 | UNRESOLVED（astropy 对拍已过，倾向文档表述） | 登记待裁决（不在本次两项 P0 授权内） |
| §2-T5 / §6-1 frame_snr 语义 | CONTROL_WEIGHT_SNR.md / UNIFIED_MODEL.md / 07_noise_snr.md | 两权威打架（UNRESOLVED） | **已裁决（订正 2026-09-20）**：帧级 SNR = 通量型「真实信号/噪声」`F_ref/σ_F`（§9.39 C1，`工程控制/RELEASE-02/GAP_AUDIT.md:1100-1110`）；与 `CONTROL_WEIGHT_SNR` 的冲突经查是**命名冲突** ⇒ 改名消歧走变更 claim（SD-19，`ACCEPTANCE.md:119`）。旧文 =「上呈负责人（需唯一 canonical 裁决）」 |
| §2-T7 / §6-2 UPM 加性 vs 乘性 | PHASE2_UPM.md / 插件 docs | 设计-实现冲突（UNRESOLVED） | **已裁决（订正 2026-09-20）**：UPM = **纯加性**（§9.38 A2，`工程控制/RELEASE-02/GAP_AUDIT.md:1036-1043`；§9.40 C3:`:1122-1127`）⇒ `PHASE2_UPM.md:182` UNRESOLVED 关闭；`g_k ≡ 1` 本期不启用。旧文 =「上呈负责人」 |
| §3-14 / §6-8 负 median flat 拒绝 | CALIBRATION.md §4/§5/§8 vs ALG/实现 | 文档-实现冲突（OWNER-04 未裁决） | 上呈负责人 |
| §6-5 Phase3 只存对角方差（ρ=0.19） | UNCERTAINTY_AND_COVARIANCE.md | 已如实登记下界；是否补相关核未裁决 | 上呈负责人 |
| §3-15 / §6-9 UNIT-001 换算落盘 | CLI/ALG | 声明制已冻结、落盘未闭环 | 归 CLI 域 |

### 5.2 非冻结文档（`docs/plugins/**`、`docs/algorithms/**`、研究包）

| SCI-S2 项 | 对象 | 现状核对 | 处置 |
|---|---|---|---|
| §3-1 插件校准方差重复计 bias | `docs/plugins/algorithms_phase1/01_calibration.md:28-32` | **未改**（仍 `V(r)+V(b)+α²[V(d)+V(b)]`）——RELEASE-01 未落地该公式订正 | 报告登记（非冻结文档；建议按 `V(r)+(1−α)²V(b)+α²V(d)+y²V(f)`/f² 一般式 + 不同 master 分支；属变更 claim 面，归前台） |
| §3-8 LM 参数陈旧 | `docs/algorithms/STAR_PSF_ALGORITHMS.md:73` | §3 仍 `iter≤50 tol=1e-6`，但 §10 `:158-159` 已显式登记"实测 tol=1e-8/max_iter=200，§3 为旧稿"（DISP-PSF-003） | 无回退（差异已登记；正文更新归 P1-PSF-IMPL） |
| §3-5/§3-6 SCAMP/RCR 引用 | SCIENTIFIC_REFERENCES.md / REJECTION.md | RELEASE-01 已补 | 无回退 |
| §3-4 孔径 flux_error 漏项 | `photometer.cpp:94`（legacy wrapper） | 未改 | 报告登记（归 P1-PHOT-IMPL / DISP-PHOT-008） |
| §3-9 生产无逐源 FLUXERR | `docs/plugins/algorithms_phase1/06_photometry.md:23` | 未改 | 报告登记（归前台） |
| §3-13 V6 头注释/J 向量 | `v6_calibration_covariance.h` | 未改 | 报告登记（report-only） |
| §6-3 母版方差传播缺口 | CALIBRATION.md / UNIFIED_MODEL.md | UNRESOLVED | 上呈负责人 |
| §6-7 MRS/N* 选型 | NOISE_MODEL / 研究包 | UNRESOLVED | 上呈负责人 |
| §6-10 SIRIL 溯源/GPL 边界 | star_detection | 需网络核验 commit | 报告登记 |
| §6-11 若干卷页 | 多处 | 需网络核验 | 报告登记 |

**结论**：FIX-SCI.md 第 3 项所述"非冻结文档的订正 RELEASE-01 已完成"与仓库现状**不完全一致**（`01_calibration.md` 方差式未改、`STAR_PSF_ALGORITHMS.md` §3 正文未改）；冻结科学文档中，本次落地了 P0 两项 + 3 项 doc-only，其余因涉及实现常数/代码或属 UNRESOLVED 未动，逐条已登记。

---

## 6 轻量检查器复跑原始输出

```text
$ python3 tools/science_contract_lint.py docs/science/DRIZZLE.md docs/science/ASTROMETRY.md docs/science/PHOTOMETRY.md docs/science/PHASE3_HIPS_TO_FITS.md
SCIENCE_CONTRACT_LINT_PASS kind=sci files=4 sections=15   (rc=0)

$ python3 ci/run_checks.py --check CHK-SCI-REF --quiet
verdict=FAIL entries=1 steps=8 pass=7 fail=1 timeout=0 prereq=0 skip_platform=0 skip_waivable=0
    - DOC-LINE-ANCHORS: FAIL rc=1
  # 唯一错误: [C4_symbol_binding] P2SMP-CELLSIDE: BINDING_VIOLATION symbol now at lines [630,772,773]
  # -> 来自并行分片的 lib/algorithms/coverage/src/sampler.cpp / include/.../sampler.h 改动（mtime 14:41，晚于本次编辑）；
  #    本 claim 涉及的 45 条 DRIZZLE/ASTROMETRY/PHOTOMETRY/PHASE3 锚全部 OK（by_status OK=868 EXEMPT=9）。

$ python3 ci/run_checks.py --check CHK-DANGLING --quiet
verdict=PASS entries=1 steps=2 pass=2 fail=0 timeout=0 prereq=0 skip_platform=0 skip_waivable=0

$ python3 tools/docs_machine_consistency.py --quiet
DOCS_MACHINE_CONSISTENCY FAIL checks=11 failed=1 numeric_constants_single_spelling
  # 违规 2 处均为**既存、非本次文件**：docs/references/SCIENTIFIC_REFERENCES.md:170、tools/quality/plane_stretch.py:49（均为 1.4826 写法）；
  # 本 claim 文件 0 违规；该工具未登记于 ci/checks.json（非机器门项）。

$ python3 docs/standards/checks/check_standards_registry.py
STANDARDS_REGISTRY_PASS  (C4/C9 全 pass；本次 STD-F1 标签订正未破坏偏差反向一致性)
```

独立复算脚本（本次内联运行，输出已记录）：trimmed-mean→σ = `0.7316730952806139`（= 1/central-80%-|r| 半正态均值）；Moffat4 FWHM/σ = `1.2303076525901024`；√(2ln1000) = `3.7169221888498383`；√(π/2) = `1.2533141373155001`。

---

## 7 未决/风险

1. **实现未同步（DRIZZLE）**：`drizzle_engine.cpp:1531` legacy 归一未改（DISP-DRZ-009）。默认 pixfrac=1 无影响；pixfrac<1 的绝对面亮度偏 `1/pixfrac²`。需构建后全量 drizzle 回归。
2. **实现注释/测试面未同步（WCS）**：`ipv_wcs.h:43,57-60,70-71` 注释与 `p1wcs_astropy_cross.py` 的 `origin=0`+`crpix+1` relabel 未改（comment/test-only）。
3. **R2 耦合风险（WCS）**：sdet 半整数探测与 `cx=w/2`/p1_sources 整数下标相消关系无测试锁定；建议跨探测合成测试 + T4 大尺度闭环帧（`run/std_f1_adj/STD_F1_ADJ_REPORT.md` §8.1 / REAL-001 同指）。
4. **常数订正需协同**（PSF/NOISE trimmed-mean、Moffat FWHM）：改文档须同步实现常数与 `tools/docs_machine_consistency.py`，否则破坏 docs↔code 一致性门。本次只复算登记，未改。
5. **CHK-SCI-REF 红来自并行分片**（P2SMP-CELLSIDE symbol binding），非本 claim；前台统一提交前需该分片收敛或协调。
6. **UNRESOLVED 上呈**：frame_snr 语义、UPM 加性/乘性、负 median flat、Phase3 相关核、SIP 单位表述——均属"需唯一 canonical / 产品方向取舍"，按控制包 §3a 上呈负责人，本报告给出现状与证据位置。
   **订正（2026-09-20）**：其中 **frame_snr 语义**（§9.39 C1，`工程控制/RELEASE-02/GAP_AUDIT.md:1100-1110`；SD-19）与 **UPM 加性/乘性**（§9.38 A2:`:1036-1043` + §9.40 C3:`:1122-1127`）**已裁决**，从本列表删除；**保留**：SIP 单位表述、负 median flat、Phase3 相关核（只存对角方差）、母版方差、MRS/N* 选型。旧文 = 上述五项并列为「上呈负责人」。

## 8 前台待跑的构建/回归命令（本次不构建）

```bash
export TMPDIR=/dev/shm/astrocs_tmp   # 源码树外
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
ctest --test-dir build -R 'p1drz|drizzle' --output-on-failure      # DISP-DRZ-009 落地后
ctest --test-dir build -R 'p1wcs|p3wcs' --output-on-failure        # WCS 口径
python3 ci/run_checks.py --check CHK-SCI-REF
python3 ci/run_checks.py --check CHK-DANGLING
python3 tools/science_contract_lint.py docs/science/*.md
```
