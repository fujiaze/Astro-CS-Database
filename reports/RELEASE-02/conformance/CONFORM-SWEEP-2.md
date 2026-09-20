# CONFORM-SWEEP-2 — 规范↔实现符合性审计（photometry / noise_snr / drizzle / projection / resample）

> 分片：`CONFORM-SWEEP-S2`（GAP_AUDIT §9.43 四路并行审计之第二路）
> 规范域：`docs/science/**`、`docs/algorithms/**`（只读权威）、`docs/plugins/algorithms_phase1/06..08`、
> `docs/plugins/algorithms_phase3/14..15`、`contracts/**`、各模块 README/module.yaml
> 实现域：`lib/algorithms/{photometry,noise_snr,drizzle,projection,resample}/**`、
> `lib/infrastructure/scheduler/src/module_adapters.cpp` 的 `p1_op_*`
> 日期：2026-09-19　审计者：CONFORM-SWEEP-2

> **⚠ 修订钉扎（必读，影响全部 `module_adapters.cpp` 行号）**：审计期间该文件被**并发进程修改**（8960 → **9042** 行，
> sha256 `824397789d…` → **`79efe106add6ca28162fb6c305608343c9df521429a0705b65c35ea5b207679f`**）。本报告结论**钉扎在工作树 79efe106…**；
> 工作树含**未提交**的 `F-INSTR-CONFORM-FIX`（`module_adapters.cpp`、`noise_snr/cpp/src/{snr_science,snr_estimator}.cpp`、
> `noise_snr/cpp/include/snr_estimator.h`、`noise_snr/wrapper_phase1/snr_frame_science.h`、
> `photometry/cpp/src/frame_photometry_fit.{h,cpp}`、`coverage/{sky_plane.h,src/sky_plane.cpp}`）。
> 故 §0 第 1 条"工作树 ≡ HEAD"仅对审计开始时成立；PH-01/PH-03 已双列 HEAD 与工作树状态。
> 行号映射（HEAD 旧稿 → 79efe106…）：Photometer 3050→**3066**；p1_flux.json 3129→**3145**；噪声样本行 3666/3667→**3748/3749**；
> NoiseModel 3567→**3649**；model.estimate 3742→**3824**；freq.psf_flux 3288→**3360**；frame_snr 3846-3852→**3928-3933**；
> find_src_frame 3756→**3838**；snr.aperture_radius_px 3608→**3690**。`star_detector.cpp:151/162` 未改动。
> 第三路子代理（PRODUCTION WIRING）另产出 16 条 P1-* + 4 条 SPEC-CONFLICT，已择要并入 §2d。
> **规范侧同样在并发变动**：`git status` 显示 `docs/algorithms/{CALIBRATION_ALGORITHMS,COSMETIC_ALGORITHMS,DRIZZLE_GEOMETRY,NOISE_ESTIMATION,PHASE3_RSMP_IMPL}.md`、
> `contracts/schemas/phase_config_*.schema.json`、`config/{defaults.json,config_registry.json,templates/*}` 等**均有未提交改动**。
> 本报告引用的规范锚点已按读取时刻复核（`DRIZZLE_GEOMETRY.md:89` 的 `211034.6` 与 `drizzle/README.md:93` 在复核时刻仍陈旧 ⇒ DZ-08 成立）；
> 但**规范侧行号/表述可能已被其他在途变更改动**，复核时请以 `git diff` 为准。


## 0 审计范围与方法（含范围偏差声明）

1. **实现侧路径偏差（必须登记）**：任务书指定"实现侧 = `/dev/shm/astrocs_conf2`"。
   实测该目录**不存在**（`ls /dev/shm/astrocs_conf2` → 无此文件或目录），且全仓 grep
   `astrocs_conf` 零命中。因此本次以**工作树**为唯一实现侧；
   `git status --short` 显示工作树相对 HEAD 仅有 `artifacts/KNOWN_FAILURES_BASELINE.json`
   与新增报告/验收目录，**范围内实现文件无未提交改动** ⇒ 工作树 ≡ HEAD 口径，审计结论对两者同真。
   若负责人另有实现快照，须重跑本分片。
2. **方法**：① 在规范域逐条枚举"规定公式/常数/单位/定义域/步骤顺序/默认值"的陈述（带 `file:line` + 原文）；
   ② 定位实现（`file:line`）；③ 判定（符合/不符/未实现/规范歧义/待定）；④ 数值/代码证据；
   ⑤ 影响；⑥ 修复面（只登记）。
3. **魔数回查**：对范围内全部实现文件做数值字面量扫描（`photometry/cpp/src`、`photometry/wrapper_phase1`、
   `noise_snr/cpp/src`、`noise_snr/wrapper_phase1`，排除 test），逐个回查规范出处，见 §3。
4. **诚实性声明**：凡"规范本身过时/规范之间打架/无权威出处"者单列并标 `SPEC-ERR`/`SPEC-CONFLICT`/`待定`，
   **不并入"不符"凑数**；凡判"符合"者同样给出 `file:line` 证据。

---

## 1 汇总统计

（见 §4 汇总表，含按 C1–C7 分组与"规范侧问题"分列）

---

## 2 逐条审计

### PH-01 [C1] 生产测光拟合的 `F_instr` 用检测 5×5 正性截断盒和，规范要求 PSF 拟合域通量
- **修订状态（工作树 79efe106…）**：**HEAD=不符；工作树已含未提交修复** `F-INSTR-CONFORM-FIX`——`module_adapters.cpp:2036-2037` 新增 `p1_psf_analytic_flux(A,sx,sy)=2πA·sx·sy/3`、`:2167` 写入 `p1_psf.json` 的 `flux` 列、`:3259-3286` 测光侧按 `star_id` 关联并取 `c.flux=(*it->second).value("flux",0.0)`（无效星不回退盒和）。**残留不符**：噪声节点仍取盒和（`:3748`）。本节其余行号为 HEAD 旧稿（工作树对应 **3280/3360**）。
- 规范：`docs/science/PHOTOMETRY.md:96`「**PSF 参数**：星点通量来自 PSF 拟合域（PSF.md）」；
  `docs/science/PSF.md:87`「解析通量 `flux=2πA·sxsy/3`，单位 **ADU**」；`docs/science/PSF.md:21` 同式。
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:3242`「`pfl.push_back(srcs[s].value("flux", 0.0));`」
  → `:3288`「`freq.psf_flux = pfl.data();`」→ `lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:175`
  「`req.psf_flux, req.psf_status, req.n_psf,`」→ `pc_calibrate_simple_with_gaia_f64_v2_qf`（`r_i=log10(F_instr/F_syn)`）。
  该 `flux` 的生产者是 `lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:151`
  「`s.flux = m00;`」，`m00` 定义于 `:139-149`：5×5 窗口、`v<=0 continue` 的**正性截断盒和**。
- 判定：**不符**
- 证据：`star_detector.cpp:140-149`（`for (int dy=-2; dy<=2; ++dy) … if (v <= 0) continue; m00 += v;`）+
  `:151`；同文件 `:162` 的 `fwhm_px` 亦为检测量（见 PH-03）。PSF 域通量在同一次运行中**已算出但被丢弃**：
  `lib/algorithms/psf/src/dpsf_psf.cpp` 产出 9 列 `psf_params`，但 `module_adapters.cpp:2142-2151` 写 `p1_psf.json`
  的 `psf_params` 行**无 flux 列**（仅 B/A/cx/cy/sx/sy/theta/fwhm_x/fwhm_y）。
- 影响：5×5 盒捕获的 PSF 能量份额随 seeing 变化 ⇒ 帧间零点假差。独立复算（本审计，离散 Moffat4 β=4，
  盒内/全域通量比）：FWHM=2.0 px ⇒ 0.9367；3.0 px ⇒ 0.7849；4.0 px ⇒ 0.6162 ⇒ **seeing 2.0→4.0 px
  给出 0.455 mag 的假帧间差**（GAP_AUDIT §9.43 同场景记 0.72 mag，含正性截断与检测阈的额外偏置）。
  该通量同时进入 `k_photo` 标定与逐源 SNR ⇒ 跨望远镜 ~0.5 mag 偏差与源污染偏差 131% 的同源根因。
- 修复面：`lib/algorithms/psf/src/dpsf_psf.cpp`（9 列 → 增加 `flux` 列）+ `module_adapters.cpp:2142-2151`
  （写 `p1_psf.json` flux 列）+ `:3226-3242`（按 `star_id` 关联取 PSF 域 flux，而非 `sources[].flux`）。

### PH-02 [C1] 同一帧的 `F_instr` 在生产链里有**两个不同口径**，且两者互不消费
- 规范：`docs/science/PHOTOMETRY.md:97`「`F_instr` 为仪器通量（ADU·px 或 e⁻ 同尺度）」——单一物理量单一口径。
- 实现：口径 A（盒和）`star_detector.cpp:151` → 用于 SNR（`module_adapters.cpp:3666`）与测光标定（`:3242`）；
  口径 B（孔径）`lib/algorithms/photometry/wrapper_phase1/photometer.cpp:81`「`r.flux = sum;`」
  （r=4.0 px 圆孔径，天空环 6–10 px 中值背景，`photometer.h:26-28`）→ 只写 `p1_flux.json`（`module_adapters.cpp:3129`）。
- 判定：**不符**
- 证据：`p1_flux.json` 在仓库内**无生产读者**（grep `p1_flux.json`：仅 `module_adapters.cpp` 写出点、
  注释、`tests/**` 与 `tests/validation/release02/**`）；而 `p1_snr.json` 的 `sources[].flux_adu`
  （`module_adapters.cpp:3834`）与 `p1_phot.json` 的 `k_photo`（`:3473`）都源自口径 A。
- 影响：孔径测光产物成为死端；下游一切科学量（SNR、深度、测光标定）都建立在 seeing 依赖的盒和上；
  两产物同名 `flux` 但语义不同，跨产物比对必然出错。
- 修复面：`module_adapters.cpp:3099`（让 SNR/标定改用 `Photometer::measure` 的孔径通量，或反向让
  `p1_flux.json` 采用 PSF 域通量）+ `docs/science/PHOTOMETRY.md:96` 口径唯一化。

### PH-03 [C2] `fwhm_px` 的生产者用 Gaussian 常数 `2.3548200450309493`，消费者按 Moffat4 `1.230310` 反解 σ（比值 1.914005）
- **修订状态（工作树 79efe106…）**：**HEAD=不符；工作树已在消费侧修复（未提交）**——`snr_science.cpp:50` 新增 `kGaussFwhmFactor=2.3548200450309493`（检测块列）、`:54` 保留 `kMoffat4FwhmFactor=1.230310`（PSF 块自身模型正向导出）、`:60-61` `detectionSigmaFromFwhm()`、`:118/:155` 按 `fwhm_px>0 ? /kGaussFwhmFactor : sigma_px` 分流；`snr_estimator.cpp:40/72/81-82` 对 PSF 块行用 `/1.230310` 并经 `sigma_px` 传入、`fwhm_px=0`。`star_detector.cpp:162` 未改动，但该列现被文档化为"检测块高斯列"（`snr_science.cpp:20-27` 头注），故生产者侧不再是缺陷；**HEAD 仍为不符**。
- 规范：`docs/science/PSF.md:20`「`fwhm_x/y` 轴向 FWHM `1.230310·s`」；`:51`「`FWHM=2√2·σ·√(2^{1/4}−1)≈1.230310·σ`」；
  `:86`「`FWHM=1.230310·σ`（各向同性不变量）」；`:92` 禁止「改变 Moffat β≠4 或 `FWHM_FACTOR`」。
- 实现（生产）：`lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:162`
  「`s.fwhm_px = 2.3548 * 0.5 * (a + b);`」（`a,b` 为 5×5 窗口二阶矩轴尺度，Gaussian 口径；
  同模块 `memory.md:29`「V4.54 Gaussian PSF profile … FWHM = 2.3548*σ (Gaussian)，替代 FWHM = 0.87*σ (Moffat4)」）。
- 实现（消费，本分片范围内）：
  - `lib/infrastructure/scheduler/src/module_adapters.cpp:3667`「`row.fwhm_px = s.value("fwhm_px", 0.0);`」
    （`s` ∈ `p1_sources.json` 的 `sources[]`）→ `snr_frame_science.cpp:48` → `snr_science.cpp:42`
    「`return fwhm_px / kMoffat4FwhmFactor;`」；
  - `lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:66`「`const double sigma = fwhm / 1.230310;`」
    （`:52-53` 注释「`sigma_px = fwhm / 1.230310  [SCI-PSF FWHM=1.230310*sigma]`」）；
  - `snr_science.cpp:188`「`: 1.5 * fwhm_eff;`」（孔径默认 = 1.5×FWHM）。
- 判定：**不符**
- 证据（算术）：`2.3548200450309493 / 1.230310 = 1.914005`。生产者写出的 `fwhm_px` 被消费者除以 1.230310，
  等价于把**检测二阶矩尺度放大 1.914×** 后当作 Moffat4 的 σ。以代码自身的离散 Moffat4 轮廓
  （`snr_science.cpp:51-77` 的 `moffat4Discrete`）复算：σ=1.914011 时 `ΣP²=5.5952e-2`，
  σ=1.0 时 `ΣP²=2.4996e-1`，比值 0.2238 ⇒ `σ_F=σ_sky/√ΣP²` 放大 **2.1136×** ⇒ `SNR_F=F/σ_F`
  缩小 **0.4731×** ⇒ 深度 `m_5` 变浅 **0.813 mag**（网格按 σ 等比放大以消除截断影响，脚本见 §5 复算记录）。
- 影响：逐源 SNR（`p1_snr.json`）、帧级 5σ 深度、写入 HiPS 头的 `frame_snr`（见 NS-03/DRZ 侧）
  与孔径改正 `f_in(1.5·FWHM)`（`snr_science.cpp:191`）全部系统性偏移 ~0.8 mag。
- 修复面：`star_detector.cpp:162`（Moffat4 口径 ⇒ `1.230310`；或改由 PSF 拟合的 `fwhm_x/y` 提供）
  + `module_adapters.cpp:3667`（改取 `p1_psf.json` 的 Moffat4 `fwhm_x/y`）。

### PH-04 [符合] PHOTOMETRY.md §5 IRLS/Tukey 全部常数与门限在实现中逐位一致
- 规范：`docs/science/PHOTOMETRY.md:53-58`（`S=MAD/0.6744897501960817`、`c=4.685`、`tol=1e-6`、50 步、
  `S==0⇒median`）、`:39`（`|r_consistent|>=3`、`|r_inliers|>=2`）、`:50`（`mag_tolerance=3.0`）、
  `:102`（不得改这些阈值）。
- 实现：`lib/algorithms/photometry/cpp/src/star_matcher.cpp:21`「`_MAD_SCALE = 0.6744897501960817`」、
  `:23`「`_TUKEY_C = 4.685`」、`:27`「`_IRLS_CONVERGE = 1e-6`」、`:616`「`if (r_inliers.size() >= 2)`」、
  `lib/algorithms/photometry/cpp/src/pc_api.cpp:145`/`:435`「`3.0,  // mag_tolerance`」。
- 判定：**符合**（`max_iter=50` 见 `star_matcher.cpp:6` 注释与实现循环；未发现偏离）
- 影响：无。
- 修复面：无。

### PH-05 [C4/SPEC-CONFLICT] 孔径测光默认半径：插件规范 `2×FWHM` vs 模块 README/实现 `4.0 px`
- 规范 A：`docs/plugins/algorithms_phase1/06_photometry.md:32`「`aperture_radius` | **2×FWHM** | px | 孔径测光半径」；
  `:33`「`sky_annulus` | —— | px」。
- 规范 B（模块级）：`lib/algorithms/photometry/README.md:173-174`「`photometer.h:24 类、:26 ctor 默认
  aperture 4.0px/annulus 6.0-10.0px`」。
- 实现：`lib/algorithms/photometry/wrapper_phase1/photometer.h:26-28`
  「`explicit Photometer(double aperture_radius_px = 4.0, double sky_annulus_inner = 6.0, double sky_annulus_outer = 10.0);`」；
  生产调用点 `module_adapters.cpp:3050`「`const astrocs::phase1::Photometer phot;`」**不读任何配置键**
  （全文件无 `photometry.aperture_radius` / `photometry.mode` / `photometry.sky_annulus` 读者）。
- 判定：**规范歧义**（两条规范互相打架）+ 实现侧 C4（生产默认 4.0 px ≠ 插件默认 2×FWHM，且不可配）
- 证据：`config/config_registry.json:557-577` 已把 `aperture_radius` 登记为 `finding: "gap"`、
  `"declared_default": "2×FWHM"`、`registration: "none"`（"未进任何配置类"）；
  `config/defaults.json` 无 `aperture_radius` 键（grep 零命中）。
- 影响：孔径不随 seeing 自适应 ⇒ 孔径通量随 seeing 漂移；规范声明的默认值在生产不可达。
- 修复面：`docs/plugins/algorithms_phase1/06_photometry.md:32` 与 `photometry/README.md:173` 二者取一为权威，
  再把 `aperture_radius`/`sky_annulus` 登记进 `config/defaults.json` 并由 `module_adapters.cpp:3050` 读取。

### PH-06 [C4/SPEC-CONFLICT] 测光 `mode` 默认：插件规范 `psf` vs 生产只有 aperture（且不读 `mode`）
- 规范 A：`docs/plugins/algorithms_phase1/06_photometry.md:31`「`mode` | **`psf`** | —— | psf/aperture」。
- 规范 B：`docs/science/PHOTOMETRY.md:96`「星点通量来自 PSF 拟合域」。
- 实现：`module_adapters.cpp:3050-3121` 仅调用 `Photometer::measure`（孔径）；
  `:2235` 注释记载负责人裁决「**(a) 测光正式口径 = 孔径测光（现状实现）**；(b) psf 端口为死边、
  psf_params 在仓库内零消费者」。
- 判定：**规范歧义**（插件默认 psf 与负责人裁决/PHOTOMETRY.md:96 的 PSF 域口径冲突）+ C4（`mode` 键无读者）
- 证据：`config/config_registry.json:536-556` 登记 `mode` 为 `finding: "gap"`、`registered_key: null`。
- 影响：通量口径的**规范归属未定**——PH-01/PH-02 的修复方向取决于此裁决（PSF 域 vs 孔径）。
- 修复面：负责人裁决 `06_photometry.md:31` 与 `PHOTOMETRY.md:96` 的优先序，并让生产节点读 `mode`。

> 订正（2026-09-20）：PH-06 的「规范归属未定 / 取决于此裁决」已裁决（旧文 =「影响：通量口径的规范归属未定——PH-01/PH-02 的修复方向取决于此裁决（PSF 域 vs 孔径）」）—— A6 定案 = PSF 拟合域 2πA·sxsy/3（§9.39 A6「查论文，科学软件算法等」，工程控制/RELEASE-02/GAP_AUDIT.md:1092-1098；§9.51「A6 不要照搬 PMM 的口径」:1647-1651；§9.52 红绿例：PSF 域 0.0127 mag vs 5×5 盒和 0.3429 mag，:1705-1715）。上文 :144 所引「(a) 测光正式口径 = 孔径测光」为已被取代的旧裁决引用。残留：aperture_radius 默认值与 mode 键去留仍缺证据（见报告 §3 U-3）。

### PH-07 [C7] `DATA-P1-FLUX` 端口声明单位 `ELECTRON`，规范为 `ADU`（gain 不可得）
- 规范：`docs/science/PHOTOMETRY.md:30`「`F_instr`: **ADU**（e⁻ 需 gain，**当前不可得**）」；`:15` 同。
- 实现：`module_adapters.cpp:829`「`{"fluxes", "DATA-P1-FLUX", false, UnitId::ELECTRON, CoordinateFrame::ICRS}`」、
  `:854`（noise-snr 模块同键同单位）；`lib/infrastructure/pipeline/module_ports.registry.json`
  `astrocs.phase1.noise-snr` 的 `fluxes` 端口 `"unit": "ELECTRON"`。
- 判定：**不符**（单位口径）
- 证据：`photometer.cpp:71-81` 直接对图像像素（ADU）求和，全链无 gain 换算；`p1_op_calibrate`
  段（`:1610-1939`）无 ADU→e⁻ 换算点。
- 影响：单位合同错误；下游若按 ELECTRON 语义做换算会引入不存在的 gain。
- 修复面：`module_adapters.cpp:829/854` + `module_ports.registry.json` 的 `unit` 改 `ADU`。

### PH-08 [C7/C3] noise-snr 模块声明的输入边是 `DATA-P1-FLUX`，生产实际消费 `DATA-P1-SOURCES`
- 规范：`module_ports.registry.json` `astrocs.phase1.noise-snr`：输入端口仅 `fluxes`=`DATA-P1-FLUX`、
  输出 `snr`=`DATA-P1-SNR`；`module_adapters.cpp:846-862` `p1_noise_snr_descriptor()` 同。
- 实现：`module_adapters.cpp:3756`「`const Json* src_frame = find_src_frame(base);`」+
  `:3780-3786`「`const Json all_sources = all_sources_of(src_frame); … compute_snr_frame_science(rows, cfg);`」
  —— 数据来自 `p1_sources.json`（`DATA-P1-SOURCES`），**从不读 `p1_flux.json`**。
- 判定：**不符**
- 证据：`p1_flux.json` 全仓无生产读者（见 PH-02 证据）；依赖边与实际数据依赖不一致（靠节点顺序偶然满足）。
- 影响：typed 依赖图失真（调度器无法保证 sources 先于 snr 落盘）；跨模块单位/形状合同形同虚设。
- 修复面：`module_adapters.cpp:853-856` 与 `module_ports.registry.json` 改为 `sources`=`DATA-P1-SOURCES`。

### PH-09 [C6] PSFSW 四分量算法实现完整，生产链零调用者（而规范默认 `psfsw_enable=true`）
- 规范：`docs/plugins/algorithms_phase1/07_noise_snr.md:111`「`psfsw_enable` | **true** | —— | 是否生产
  psfsw_robust 四分量」；`:23` 输出 `psfsw_robust`；`:57`/`:78` 语义与模式。
- 实现：`lib/algorithms/photometry/cpp/src/psfsw.cpp:202`（`extract_psfsw_components`）、`:330`
  （`compute_psfsw_weights`）、`:723`（`validate_psfsw_record`）；头 `lib/algorithms/photometry/include/astrocs/v6/psfsw.h`。
- 判定：**未实现（生产面）/ C6**
- 证据：全仓调用者只有 `run/v6/**` 与 `tests/unit/v6_p1_psfw/**`（grep 四个入口函数名）；
  `p1_op_noise` 输出（`module_adapters.cpp:3746-3853`）无 `psfsw_robust` 字段。
- 影响：规范要求的四分量权重产物缺失；`weight_mode=psfsw_robust` 在生产不可用。
- 修复面：`module_adapters.cpp:3563-3876`（在 `p1_op_noise` 内接 `compute_psfsw_weights`，按 `psfsw_enable` 开关）。

### PH-10 [待定/C5] 生产 FOV 半径公式的魔数（×1.2、钳位 [1,10]°、无效阈 ≥30°）无权威出处
- 规范：`docs/algorithms/PHOTOMETRIC_FIT.md` 全文无 FOV 半径条款（grep `FOV`/`fov` 零命中）；
  唯一提及在 `docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md:681`
  「FOV 半径: 0.5×对角线×1.2 余量 … 限制 **[0.1, 30.0]°**」（归档件，非权威）。
- 实现：`lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:129-135`
  「`… / 2.0 * 1.2;`」+「`if (fov_radius_deg <= 0.0 || fov_radius_deg >= 30.0) { fov_radius_deg = std::min(std::max(fov_radius_deg, 1.0), 10.0); }`」。
- 判定：**待定**（无权威规范可判；实现与归档记录的下限/上限不同）
- 影响：Gaia 锥搜范围（`pc_api.cpp:291` 自适应 `mag_max_arr{12..16}`）随 FOV 半径变化 ⇒ 匹配星数与 `k_photo` 稳定性。
- 修复面：`frame_photometry_fit.cpp:129-135` 的常数入 `config/defaults.json` 并补 `docs/algorithms/PHOTOMETRIC_FIT.md` 条款。

### PH-11 [待定/SPEC-ERR] 模块与算法文档称 `Photometer` "未接 orchestrator 管线"，与生产调度节点已接线并存
- 规范：`lib/algorithms/photometry/README.md:181`「**未接入 orchestrator 管线**（grep 实测无生产调用方）」；
  `docs/algorithms/PHOTOMETRIC_FIT.md:209-213`「`astrocs::phase1::Photometer` … **未接 orchestrator 管线**」。
- 实现：`module_adapters.cpp:3050` + `:3099`「`auto r = phot.measure(im.px(), im.w(), im.h(), cx, cy);`」
  —— 已接入**生产调度节点** `p1_op_photometry`（`astrocs` 可执行文件经 `astrocs_cli_runtime`→
  `astrocs_module_adapters` 链接，见 `CMakeLists.txt:828-838`、`:766-825`）。
- 判定：**待定**（字面为真——`orchestrator`（B 线，`lib/infrastructure/pipeline/orchestrator`）
  确实不调用它；但文档框架会误导读者认为"无生产调用方"，而生产调度面已接线）
- 影响：文档-实现口径漂移；审计者按此条会漏判 PH-02/PH-05。
- 修复面：`photometry/README.md:181`、`PHOTOMETRIC_FIT.md:209-213` 补"生产调度节点已接线"。

### NS-01 [C3] 生产 Phase1 噪声节点用**整帧 median+MAD**，SCI-NOISE-001 §5/§5a 要求的 8×8 patch + 逐星掩膜 + 平面场 + 天空预算全部缺失；合规实现不在根构建图
- 规范：`docs/science/NOISE_MODEL.md:52`「patch grid 8×8；星点掩膜**逐星半径** `r_i = clip(r_local(F_i, FWHM_i, k·σ_bg), r_min, rmax)`，
  再按天空预算收缩（§5a）」、`:54`「稳健裁剪: cosmic/hot 5σ 阈，≤2 轮」、`:56`「空间场: 最小二乘平面
  `var(x,y) = a + b·x + c·y`」、`:58`「全局兜底: 合格 patch variance 的稳健中位数」、`:85`（预算 `n_qualified ≥ 8`
  且 `N_sky ≥ 9216`）、`:144`（不得改 8×8 与 5σ≤2 轮）。
- 实现（生产）：`module_adapters.cpp:3567`「`const astrocs::phase1::NoiseModel model;`」+`:3742`
  「`auto r = model.estimate(px);`」→ `lib/algorithms/noise_snr/wrapper_phase1/noise_model.cpp:145-195`：
  全帧像素 `median`（`:168`）+ `MAD`（`:182`）+ `sigma = kMadToSigma*mad`（`:189`）+ floor clamp（`:191`）。
  **无 patch 网格、无掩膜、无裁剪轮次、无空间场、无预算、无饱和过滤。**
- 合规实现（未接线）：`lib/algorithms/noise_snr/cpp/src/noise_model.cpp`（`:308-470` patch/掩膜/预算/平面场）
  编入 target `astrocs_p1_noise`（`lib/algorithms/noise_snr/CMakeLists.txt:35-38`），
  但 `CMakeLists.txt:240`「`# F-CI-002-01(CI-002 owner 裁决 2026-09-11): add_subdirectory(lib/algorithms/noise_snr)`」
  **被注释** ⇒ 该 target 不在根构建图；生产 target `astrocs_phase1_noise`（`CMakeLists.txt:620-623`）
  只编 `wrapper_phase1/noise_model.cpp` + `snr_frame_science.cpp` + `cpp/src/snr_science.cpp`。
- 判定：**未实现（生产面）** + C3
- 证据：模块自身文档已承认构建面：`lib/algorithms/noise_snr/README.md:9-14`「**构建面（实测；本模块不得再自报
  "唯一生产实现"）**：根 CMake 主图只收 `wrapper_phase1/noise_model.cpp` + …」；`:27`「未编入根 CMake 主构建
  … `dll_loader.cpp:41/:55` 按该 DLL 名与 `lib/algorithms/noise_snr/cpp/` 路径装载（生产通道吻合）」
  —— 与生产实际（`module_adapters` 进程内调 wrapper）**冲突**（SPEC-CONFLICT，见 NS-11）。
- 影响：① `σ_bg` 被星源污染：整帧 MAD 含星像与源翼，违反 §5a「掩膜的唯一目的是让天空样本无源」；
  GAP_AUDIT §9.43 记 `σ_MAD/σ_clippedRMS = 1.3127`（131% 源污染偏差）与此一致；
  ② 无空间方差场 ⇒ `ivar` 是单标量，与 `var(x,y)=a+bx+cy` 规范不符，梯度帧权重错；
  ③ 无 `MASK_LEGACY/MASK_DEGRADED` 诊断位 ⇒ 降级不可见；④ 无预算门 ⇒ 退化帧静默产出伪权重。
- 修复面：`CMakeLists.txt:240`（解除注释或把 `cpp/src/noise_model.cpp` 编入 `astrocs_phase1_noise`）
  + `module_adapters.cpp:3567/3742`（改调 `snr_noise_model_v1_f64`/`_fill`，并按 §5a 传 `star_flux`/`star_fwhm`）。

### NS-02 [C3] 饱和域过滤（claim SC-008）在生产噪声路径不存在，且无显式降级声明
- 规范：`docs/science/NOISE_MODEL.md:43`「**饱和域（claim SC-008 / SAT-001）**：`x ≥ saturation_level` 的像素为
  **饱和像素**，**不参与 blank-sky 统计**（输入有效域规则，**无条件生效**）… 未提供时调用方**必须**在帧产品写
  显式降级声明 `NOISE_SATURATION_FILTER=DISABLED_NO_METADATA`（禁止静默）」。
- 实现：`wrapper_phase1/noise_model.cpp:145-195` 的 `estimate(const std::vector<float>& pixels)`
  **无 saturation 形参**；`module_adapters.cpp:3742` 只传像素向量；`p1_snr.json` 帧对象
  （`:3746-3853`）无 `NOISE_SATURATION_FILTER` 键。
- 判定：**未实现**
- 证据：合规实现里有该逻辑（`cpp/src/noise_model.cpp:64-68` `valid_pixel(x, saturation)`），生产路径不可达。
- 影响：饱和平台（`peak > 50000` 的源核，`star_detector.cpp:167`）进入 `MAD` ⇒ `σ_bg` 虚高 ⇒ `ivar` 偏小；
  且降级状态在帧产品**不可读**（规范明确要求可读）。
- 修复面：`wrapper_phase1/noise_model.cpp:145`（加 `saturation_level`）+ `module_adapters.cpp:3742-3749`
  （从 FITS `SATURATE`/`DATAMAX` 取电平并写降级键）。

### NS-03 [C1/C7] `frame_snr` 字段语义不符：规范=帧级 SNR（F_signal/σ_F，无量纲），生产装的是 5σ 深度对象
- 规范：`docs/plugins/algorithms_phase1/07_noise_snr.md:24`「**`frame_snr`**：帧级 SNR（信噪比），**写入 HiPS 文件头**；
  语义 = **点源（PSF）信号 SNR**（纯信号/噪声）」；`:46`「`SNR = F_signal / σ_F`」；
  `docs/science/CONTROL_WEIGHT_SNR.md:17-18` 同义；
  `contracts/schemas/unified/frame_snr.schema.json:5`「frame_snr 本身不是权重，**也不是 depth_m5（5σ 深度），二者不得互填**」。
- 实现：`module_adapters.cpp:3846-3852`
  「`frame["frame_snr"] = Json{{"definition", "5-sigma point-source depth = F_5 [ADU] / m_5 [mag] (SCI-CW-001 2a); NOT a whole-frame scalar SNR"}, {"flux5_adu", …}, {"m5_mag", …}, {"zero_point_mag", …}};`」
- 判定：**不符**（同一字段名承载 depth_m5 语义；合同明文禁止互填）
- 证据：真正的帧级 SNR 被放在 `median_source_snr`/`snr_phot`（`:3807-3809`，= `median(SNR_F)`），
  而非 `frame_snr`；`depth_m5` 未按 `contracts/schemas/unified/depth_m5.schema.json` 的对象形态输出。
- 影响：消费方（Phase2 权重链 / HiPS 头）按名取 `frame_snr` 会拿到深度量；合同级语义冲突。
- 修复面：`module_adapters.cpp:3846-3852`（`frame_snr` 改承载 `snr_f`，深度另立 `depth_m5` 对象）。

### NS-04 [C3] `point_information`（`W_psf = a²PᵀC⁻¹P = 1/Var(F_hat)`）生产未产出
- 规范：`docs/plugins/algorithms_phase1/07_noise_snr.md:22`「`point_information`：`W_psf(x,y) = a²PᵀC⁻¹P = 1/Var(F_hat)`
  （点源严格权重）」；`:32-34` 公式；`contracts/schemas/unified/point_information.schema.json:5`。
- 实现：`p1_snr.json` 帧对象（`module_adapters.cpp:3746-3853`）无 `point_information`/`W_info` 键；
  `sci.sigma_f_adu`（`snr_frame_science.cpp:199`）是 `σ_F`，可换算 `W=1/σ_F²` 但未导出。
- 判定：**未实现（生产面）**
- 证据：grep `point_information` 在 `lib/algorithms/{photometry,noise_snr}` 内仅 `psfsw.cpp:667` 的模式词表命中。
- 影响：Phase2 默认 `point_information` 逆方差权重（`CONTROL_WEIGHT_SNR.md:78-79`）失去严格来源。
- 修复面：`module_adapters.cpp:3829-3853`（按 schema 输出 `point_information` 对象，`W_info=1/σ_F²`）。

### NS-05 [C4/规范歧义] `sparse_snr_layer` 规范默认 true，生产不产出；密度未定案构成联锁
- 规范：`docs/plugins/algorithms_phase1/07_noise_snr.md:112`「`sparse_snr_layer` | **true** | —— | 是否产出稀疏帧内 SNR 层。
  **本期决议默认产出**」；`:130`「`sparse_snr_density` 未定案 ⇒ 稀疏层不可生产（**联锁缺口**登记；不得编造数值）」。
- 实现：`p1_snr.json`（`module_adapters.cpp:3746-3864`）无稀疏层；`config/defaults.json:552` `sparse_snr.density`
  为 `pending_authority`。
- 判定：**规范歧义**（同文件 §5 默认 true 与 §7 联锁互斥）→ 实现不产出是**正确的联锁行为**，但需显式登记
- 证据：`config/config_registry.json:660-673` 登记 `sparse_snr_layer` `declared_default: "false"`（**与现行
  `07_noise_snr.md:112` 的 true 不一致** ⇒ 登记册过期，SPEC-ERR 子项）
- 影响：默认路径 `sparse_reconstruct`（`:114`）无输入，实际退化为帧级路径且未记 `snr_path_effective`。
- 修复面：`config/config_registry.json:665`（默认值改 true）+ `module_adapters.cpp:3746+`（按开关产出/显式记路径）。

> 订正（2026-09-20）：NS-05 的「密度未定案构成联锁」已裁决（旧文 =「规范歧义…密度未定案构成联锁」）—— 密度 = Δ 64 px（§9.45，工程控制/RELEASE-02/GAP_AUDIT.md:1299-1309；承载字段 sparse_snr.spacing_px = 64，config/defaults.json:710-718，authority_status=owner_adjudicated）；Phase1 默认产出（§9.38 A3:1045-1055）、Phase2 默认消费（§9.46:1360-1366）。剩余 = 实现落地 + config/config_registry.json 登记册过期（sparse_snr_layer declared_default 为 false）；sparse_snr.density（点/度²）与 spacing_px 不是同一字段，前者仍 pending_authority。

### NS-06 [符合] 逐源科学 SNR 的公式实现与规范一致（Horne 1986 最优提取 + Moffat4 β=4 解析孔径改正）
- 规范：`docs/plugins/algorithms_phase1/07_noise_snr.md:68`「`SNR_k(F_ref) = F_ref·sqrt(W_psf,k) = F_ref/σ_F,k`」；
  `:69`「`W_psf,k = a_k²P_kᵀC_k⁻¹P_k`」；`:75`（通量型口径）；`docs/science/NOISE_MODEL.md:131`（`1.253=√(π/2)`）。
- 实现：`lib/algorithms/noise_snr/cpp/src/snr_science.cpp:123-212`（`σ_i²`、`Var(F)=1/Σ(P²/σ²)`、
  `f_in=1−u⁻³` 孔径 enclosed fraction、`m5=ZP−2.5log10(5σ_F)`）、`:242-247`（`1.253·σ/√N`）、
  `snr_frame_science.cpp:66-211`（组内公共 `F_ref`、fail-closed）。
- 判定：**符合**（其输入口径问题见 PH-01/PH-03；公式本身逐条对得上）
- 影响：无（在正确输入下）。
- 修复面：无。

### NS-07 [符合] SCI-NOISE-001 §5/§5a 的常数与阈值在合规实现中逐条一致（但该实现不在生产链，见 NS-01）
- 规范：`NOISE_MODEL.md:53`（`1.482602218505602·MAD`）、`:55`（`min_samples` 默认 64，`:175`）、`:82-84`（`k=0.1`、
  `r_min=max(1.5 px, 0.75·FWHM)`、`rmax=60 px`）、`:85`（`N_sky≥9216`、`n_qualified≥8`）、`:45`（`λlo/λhi ≥ 1/16`）、
  `:21`/`:107`（`variance_floor=1e-12`）、`:126`（5σ≤2 轮）。
- 实现：`lib/algorithms/noise_snr/cpp/src/noise_model.cpp:61`（MAD 常数）、`:115`（`kPlaneGeomRatio=0.0625`）、
  `:321-325`（`k=0.1`、`r_min=1.5`、`fwhm_floor=0.75`、`budget_patches=8`、`budget_sky=9216`）、
  `:626-638`（`source_mask_radius_px=10.0`、`mask_radius_scale=6.0` ⇒ rmax=60、`cosmic_clip_sigma=5.0`、
  `variance_floor=1e-12`）、`:148`（`kMaskMoffatBeta=2.5`）、`:152-164`（Moffat `r_local` 与 `α=FWHM/(2√(2^{1/β}−1))`）。
- 判定：**符合**
- 影响：无（一旦接线即生效）。
- 修复面：无。

### NS-08 [C5/C4] `snr.*` 生产配置键未登记，且孔径默认 `1.5×FWHM` 与插件规范 `2×FWHM` 不一致
- 规范：`docs/plugins/algorithms_phase1/07_noise_snr.md:106-114` 配置表**只列** `reference_flux`/`scalar_gate_rd`/
  `scalar_gate_trend`/`psfsw_enable`/`sparse_snr_layer`/`sparse_snr_density`/`snr_path`；
  `docs/plugins/algorithms_phase1/06_photometry.md:32`「`aperture_radius` 默认 **2×FWHM**」。
- 实现：`module_adapters.cpp:3601-3621` 读 `snr.{gain_e_per_adu, read_noise_e, zero_point_mag, aperture_radius_px,
  n_sky, profile_half_px, sigma_logflux_dex, n_matches, reference_flux_adu, max_sources}`（均**不在** defaults.json）；
  `snr_science.cpp:187-188`「`const double r = (p->aperture_radius_px > 0.0) ? p->aperture_radius_px : 1.5 * fwhm_eff;`」；
  `snr_science.cpp:79-84`「`autoHalf`：`max(30, ceil(12·FWHM))`，上界 256」。
- 判定：**不符**（C5 未登记默认 + C4 1.5×FWHM ≠ 2×FWHM）
- 证据：`config/defaults.json` grep `aperture_radius_px`/`profile_half_px`/`n_sky`/`max_sources`/`reference_flux_adu`
  零命中；`config/config_registry.json` 亦无。
- 影响：孔径与轮廓网格尺寸（`ΣP²` 的离散化域）随 FWHM 变化且不可配，SNR 绝对值依赖这两个未登记默认。
- 修复面：`config/defaults.json` 增登记 + `snr_science.cpp:188` 默认值按规范裁定。

### NS-09 [C1] 07 插件规范声明 `reference_flux` 单位 `e⁻/s`，实现用 `reference_flux_adu`（ADU）
- 规范：`docs/plugins/algorithms_phase1/07_noise_snr.md:108`「`reference_flux` | —— | **e⁻/s** | 固定参考通量
  （m5/SNR 定义必需）」。
- 实现：`module_adapters.cpp:3616`「`if (sc.contains("reference_flux_adu"))`」、`:3825`「`{"flux_adu", sci.reference_flux_adu}`」；
  `snr_science.cpp:13` 单位约定「`flux_adu [ADU]` 总通量（与输入图像同标度）」。
- 判定：**不符**（单位口径）；考虑到 `PHOTOMETRY.md:30`「e⁻ 需 gain，**当前不可得**」，实现侧取 ADU 是唯一可行，
  故**规范侧单位需订正**（SPEC-ERR 子项）。
- 影响：单位合同错误；SNR/m5 的绝对标度声明与实际不符。
- 修复面：`07_noise_snr.md:108` 单位列改 ADU（并补 `reference_flux_adu` 键名）。

### NS-10 [C6] `snr_estimator.cpp`（旧控制点/质量诊断通道）零生产编译、零生产调用
- 规范：`docs/science/NOISE_MODEL.md:165` 把 `lib/algorithms/noise_snr/cpp/src/noise_model.cpp` 列为实现面；
  `NOISE_MODEL.md:180` 提及 `snr_phot_cal_quality`（`noise_model.cpp:305`）。
- 实现：`lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp`（917 行，`snr_estimate`/`snr_extract_model_v3`/
  `sourceSnrFromPsfRow`/`frameDepthFromPsf`）在根图中**只被测试 target 编译**：
  `lib/algorithms/psf/tests/p1psf/CMakeLists.txt:177`、`lib/algorithms/noise_snr/tests/p1noise/CMakeLists.txt:126`；
  生产 `astrocs_phase1_noise`（`CMakeLists.txt:620-623`）不含该 TU。
- 判定：**未实现（生产面）/ C6**
- 证据：`module_adapters.cpp` 无 `snr_estimator` 符号引用（仅注释 `:3547-3553` 指向 `snr_science`）；
  调用它的 `snr_extract_model_v3` 实现在 `lib/algorithms/noise_snr/src/module_entry.cpp`（属被注释掉的 target）。
- 影响：两条"另一份实现"并存（`snr_science.cpp` 生产 / `snr_estimator.cpp` 死边），科学量双源风险。
- 修复面：`lib/algorithms/noise_snr/CMakeLists.txt`/`CMakeLists.txt:240`（接线或显式标注 test-only 并登记退役）。

### NS-11 [SPEC-CONFLICT] 模块 README 声明生产通道是 `dll_loader` 装载的 `snr_estimator.dll`，与生产实际（进程内 wrapper）冲突
- 规范：`lib/algorithms/noise_snr/README.md:27`「现状构建=…`g++ -shared → snr_estimator.dll` … 未编入根 CMake 主构建 …
  `dll_loader.cpp:41/:55` 按该 DLL 名与 `lib/algorithms/noise_snr/cpp/` 路径装载（**生产通道吻合**）」。
- 实现：生产 `astrocs` 可执行文件经 `astrocs_cli_runtime`→`astrocs_module_adapters`（`CMakeLists.txt:766-825`、`:828-838`）
  在**进程内**调用 `astrocs::phase1::NoiseModel::estimate`（`module_adapters.cpp:3567/3742`），
  不装载任何 noise DLL；`astrocs` 亦不链接 `astrocs_infra_orchestrator`（唯一用 `dll_loader` 调
  `snr_noise_model_v1` 的调用点，`orchestrator.cpp:4351-4361`）。
- 判定：**规范歧义/SPEC-CONFLICT**（模块 README 与生产接线二选一为真；README 与 `README.md:9-14` 自身亦不一致）
- 影响：审计与运维对"生产噪声实现是哪一份"的认知分歧（正是 NS-01 长期未被发现的机制）。
- 修复面：`lib/algorithms/noise_snr/README.md:27` 与 `:9-14` 统一口径。

### NS-12 [待定] `snr_estimator.cpp` 中 `EPS2 = 1e-6` 等守卫常数的规范出处未确认
- 规范：未在 `docs/science/NOISE_MODEL.md`/`docs/plugins/.../07_noise_snr.md` 找到对应条款。
- 实现：`lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:234`、`:403`「`const double EPS2 = 1e-6;`」。
- 判定：**待定**（该 TU 不在生产链，影响有限，未深追）
- 修复面：登记即可。

---

## 2b drizzle 子域（子代理 DRIZZLE 分片产出；本审计逐条复核关键行）

> 复核方式：本审计独立读取全部被引 `file:line`（grep/sed），复核结果标于各条"证据"末；
> 凡未复核者显式标"未复核"。

### DZ-01 [C1] drizzle 权重用 legacy 归一 `a_jp/A_drop,j`，规范要求面亮度保持 `a_jp/A_pixel,j`
- 规范：`docs/science/DRIZZLE.md:43`「`w_jp = a_jp / A_pixel,j         # 面亮度保持权重 (= pixfrac²·a_jp/A_drop,j)`」；
  `:46`「`S_p = F_p / D_p = Σ_j B_j a_jp / Σ_j a_jp   # 最终信号为面亮度`」；
  `:130`「将 `S_p` 用 legacy 归一 … 计算并宣称绝对面亮度（`pixfrac<1` 偏 `1/pixfrac²`…）」；
  `:191`「**legacy 混合式**（分子 `x_j·a_jp/A_drop,j`、分母 `Σ a_jp`）给出 `S_p=B0/pixfrac²`… **现实现 `drizzle_engine.cpp:1531` 仍是此式**」。
- 实现：`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1531`「`Scalar weight = overlap_area / drop_area;`」；
  累加于 `:1553`「`acc.sumFlux += Scalar(pixelValue * weight);`」。
- 判定：**不符**（规范自身已登记 `DISP-DRZ-009`）
- 证据（本审计复核 ✓）：`sed -n '1520,1560p'` 直读确认 `weight = overlap_area / drop_area` 与 `sumFlux += pixelValue*weight`；
  `DRIZZLE.md:43/54/130/191` 直读确认口径。数值：`w_legacy = w_SB/pixfrac²` ⇒ `S_p = S_SB/pixfrac²`；
  pixfrac=0.8 ⇒ 面亮度/测光 **×1.5625（+56.25%）**；方差按 `w²` 累加（`:1557-1559`）⇒ **×1/pixfrac⁴ = ×2.4414**。
- 影响：绝对面亮度与光度零点在 `pixfrac<1` 时系统性偏高 `1/pixfrac²`；方差偏高 `1/pixfrac⁴`（SNR 偏低）。
- 修复面：`drizzle_engine.cpp:1531` → `weight = overlap_area * (pixfrac*pixfrac) / drop_area`（登记，不改）。

### DZ-02 [C1] 通量守恒条件不变量方向相反（`Σ_p F_p` 实际恒等于 `Σ_j x_j`）
- 规范：`docs/science/DRIZZLE.md:95-98`「`Σ_p F_p = Σ_j x_j·(A_drop,j/A_pixel,j) = pixfrac²·Σ_j x_j`…`pixfrac=1` 时严格…」。
- 实现：`drizzle_engine.cpp:1553`（同上）。
- 判定：**不符**（与 DZ-01 同根；本条登记不变量方向）
- 证据：legacy 权重下 `Σ_p F_p = Σ_j x_j`（与 pixfrac 无关）⇒ 规范的 `pixfrac²` 衰减律不成立（反方向）。
- 影响：按规范做绝对通量/孔径换算会再引入一次 `1/pixfrac²` 误差。
- 修复面：随 DZ-01 一并解决。

### DZ-03 [C3] `provenance.flux_conservation_factor = pixfrac²` 在生产 Phase1 HiPS 链从未写出
- 规范：`docs/science/DRIZZLE.md:97-98`「用于绝对通量/孔径换算**必须乘** `provenance.flux_conservation_factor = pixfrac²`，否则产生 `1/pixfrac²` 光度零点偏差」；
  `docs/plugins/algorithms_phase1/08_drizzle.md:43` 同。
- 实现：`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp:330`「`// …不写 drizzle provenance, 不写 snr。`」（`write_hips_phase1`，即 `hips_profile=1` 生产路径）。
- 判定：**未实现**
- 证据（复核 ✓）：`grep -rn flux_conservation_factor lib/` 命中仅 `lib/infrastructure/aio/v6/src/v6_provenance.cpp:123`（v6 线，非 P1 链）。
- 影响：DZ-01 的 `1/pixfrac²` 偏差在产物侧**不可修正**（无因子可乘）。
- 修复面：`astro_sphere_sink.cpp:324-341`（`write_hips_phase1` 内补 provenance 与 `flux_conservation_factor`）。

### DZ-04 [C6] 面亮度正确的算子已实现，但生产链零调用者
- 规范：`docs/science/DRIZZLE.md:43/46`。
- 实现：`lib/algorithms/drizzle/healpix_drizzle/v6_drizzle_science.cpp:93`「`double sb_weight(double a_jp, double A_pixel_j) { return a_jp / A_pixel_j; }`」、
  `:106`「`double flux_conservation_factor(double pixfrac) { return pixfrac * pixfrac; }`」。
- 判定：**未实现（生产面）/ C6**
- 证据（复核 ✓）：该 TU 只编入 `tests/integration/v6_p3/CMakeLists.txt:36-37` 与 `tests/unit/v6_p3_proj`；头注「本模块不接线任何 session」。
- 影响：正确实现不可达 ⇒ DZ-01 长期未修。
- 修复面：把 `sb_weight` 语义移植进 `drizzle_engine.cpp:1531`（登记）。

### DZ-05 [C6] `poly_clip.cpp` 与 `DrizzleEngine::getHealpixCorners` 生产零调用者
- 规范：`lib/algorithms/drizzle/README.md:171`（登记 `DISP-DRZ-008`）。
- 实现：`lib/algorithms/drizzle/healpix_drizzle/poly_clip.cpp`（`clipPolygon`）。
- 判定：**未实现（生产面）/ C6**
- 证据（复核 ✓）：`grep -rn "poly_clip\|PolyClip" lib/` 除自身文件外仅 `drizzle_engine.cpp:1020` 注释命中。
- 影响：无科学影响（死代码）；但维护者会误判平面裁剪路径在用。
- 修复面：`docs/algorithms/DRIZZLE_GEOMETRY.md` 登记为确认现状（不改码）。

### DZ-06 [C7] variance/weight 面的 NaN 未按合同跳过（只判 `<=0`）
- 规范：`docs/contracts/DATA_SEMANTICS.md:295`「`variance 面…非有限或 ≤0 → 跳过该像素`」；`:296`「`weight / snr 面…非有限/≤0 跳过`」。
- 实现：`drizzle_engine.cpp:1913`「`if (weightValue <= 0.0f) continue;  // 合法数据边界, 非掩膜`」、
  `:1919`「`if (varianceValue <= 0.0f) continue;  // 合法数据边界, 非掩膜`」。
- 判定：**不符**
- 证据（复核 ✓）：NaN 对 `<= 0.0f` 恒为 false ⇒ NaN 像素进入累加；NaN variance 像素贡献 `sumFlux/sumArea` 但不贡献 `sumVarNum`（`:1557-1559` 有 `if (varianceValue > 0.0f)`）⇒ 方差偏低、SNR 偏高。
- 影响：含 NaN 的方差/权重面（真实数据常见）产生静默偏低的方差。
- 修复面：`drizzle_engine.cpp:1913,1919` → `if (!std::isfinite(v) || v <= 0.0f) continue;`。

### DZ-07 [SPEC-ERR] `docs/science/DRIZZLE.md:54` 的订正方向写反
- 规范（问题行）：`docs/science/DRIZZLE.md:54`「`故 S_p(SB) = S_p(legacy)/pixfrac²（DISP-DRZ-009：现实现仍用 legacy）`」。
- 对照：`:130`「legacy … 偏 `1/pixfrac²`」、`:191`「legacy 混合式 … 给出 `S_p=B0/pixfrac²`」。
- 判定：**不符（规范自身错，SPEC-ERR）**
- 证据（复核 ✓）：由 `:53` 的 `w_jp = pixfrac²·w_legacy` ⇒ `S_SB = pixfrac²·S_legacy`（不是除以 pixfrac²）。
- 影响：按 `:54` 实现者会把订正方向搞反（`1/pixfrac⁴` 误差，pixfrac=0.8 ⇒ 2.44×）。
- 修复面：`DRIZZLE.md:54` 改为 `S_legacy = S_SB/pixfrac²`。

### DZ-08 [SPEC-ERR/文档] HEALPix 尺度常数文档写 `211034.6`，代码表达式为 `211076.285`
- 规范：`lib/algorithms/drizzle/README.md:93`、`docs/algorithms/DRIZZLE_GEOMETRY.md:89`、`orchestrator.cpp:173/191` 注释、`module_adapters.cpp:4119` 注释均写「≈211034.6」。
- 实现：`drizzle_engine.cpp:684-685`「`std::sqrt(M_PI / 3.0) * (180.0 / M_PI) * 3600.0;  // ≈ 211076.3`」（`module_adapters.cpp:4119` 的**表达式**同）。
- 判定：**不符（文档/注释级，SPEC-ERR）**
- 证据（复核 ✓）：`sqrt(π/3)·(180/π)·3600 = 211076.285`，与 211034.6 差 0.0197%；代码用的是表达式 ⇒ **无数值缺陷**，仅文档与注释陈旧。
- 影响：文档误导；另有测试参考实现硬编码 `210960.0`（`lib/infrastructure/aio/tests/hiss_correctness_test.cpp:502`，**未复核**）可能在边界上选出不同 2 的幂 nside。
- 修复面：文档/注释改 `211076.3`（登记）。

### DZ-09 [待定/C5] `snr_evaluator` 的 `median_snr<=0 ⇒ 1.0` 静默回退无规范出处
- 规范：`docs/science/CONTROL_WEIGHT_SNR.md:104`「**snr=1.0 不允许作为 unknown 伪装**：缺失走整帧 median 回退并计数」；
  `docs/contracts/DATA_SEMANTICS.md:291` 定义 `snr_model` 块字段。
- 实现：`lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.cpp:198`、`:254`「`median_snr_ = (median_snr > 0.0) ? median_snr : 1.0;`」。
- 判定：**待定**（该条款主域为 Phase2 权重场；drizzle 控制点插值是否受同一禁令约束需裁决）
- 证据（复核 ✓）：两处逐字相同；`:199/255` 的 `idw_power` 回退 2.0 有出处（`aio_healpix_io.h:66`），`median_snr` 无。
- 影响：畸形/零 `median_snr` 静默产出未归一 SNR（差 `1/median_snr`），而非 fail-closed。
- 修复面：`DATA_SEMANTICS.md:291` 明确 invalid 策略，或 `snr_evaluator.cpp:198,254` 改 fail-closed。

### DZ-10 [待定] provenance 源像素尺度用 `|CD1_1|·3600` 而非轴尺度均值
- 规范：`lib/infrastructure/aio/include/aio_hips.h:288-291`「设置 Drizzle provenance（pixfrac / **像素角尺度**）」。
- 实现：`hp_drizzle_api.cpp:1157-1158`「`meta.fits_meta["src_pixel_scale_arcsec"] = std::to_string(std::fabs(img.wcs.cd[0]) * 3600.0);`」。
- 判定：**待定**
- 证据（复核 ✓ 部分）：代码只取 `cd[0]`（CD1_1）单分量；**未复核**子代理所述 `cd[0]=cdelt1·cos(CROTA2)` 的 AIPS 转换链（`hp_drizzle_api.cpp:438`）与"旋转 90° 时静默跳过"结论。
- 影响（若成立）：`ASTROCS_DRIZZLE_SCALE_ARCSEC` 偏低 `|cos(CROTA2)|`；当前 `k_corr` 尺度维已退役（`docs/science/PHASE2_UPM.md:40-42`，未复核）故实际影响有限。
- 修复面：`hp_drizzle_api.cpp:1157-1158` → 轴尺度均值或 `sqrt|det CD|·3600`。

### DZ-11 [规范歧义] 输入 weight/snr 面的量级被忽略（只当 0 掩膜用）
- 规范 A：`docs/science/DRIZZLE.md:50`「AstroCS 取 `w_i=1`…」；
  规范 B：`docs/plugins/algorithms_phase1/08_drizzle.md:17` 把 `variance/ivar、validity、…、point_information、psfsw` 列为输入，`:34`「pixfrac、像素面积、单位不可隐含」。
- 实现：`drizzle_engine.cpp:1362`「`Scalar pixelValue, float /*snrValue*/, float /*weightValue*/,`」（形参被注释掉）；
  `:1910-1913` weight 只作零门。
- 判定：**规范歧义**
- 证据（复核 ✓）：`sed -n '1358,1366p'` 直读确认形参名被注释；`weightValue` 仅用于 `<=0` 门。
- 影响：weight 量级与稠密 SNR 面对 signal/variance/support 无影响；若规范意图是乘性权重则实现不符。
- 修复面：`DRIZZLE.md` §5 明确 weight 面语义（掩膜 vs 乘性）。

### DZ-12 [规范歧义/SPEC-CONFLICT] 面积单位：`DRIZZLE.md` 写 px²，`DATA_SEMANTICS.md` 写 sr，实现为 sr
- 规范 A：`docs/science/DRIZZLE.md:29`「`D, a, A_drop, A_pixel`: **px²**（球面立体角等价）」、`:59`「【输出 S_p = 面亮度】(ADU/px²)」。
- 规范 B：`docs/contracts/DATA_SEMANTICS.md:303`「`sumArea（HiPS SUPPORT 底数）… 单位 **sr**（Σa_jp…）」。
- 实现：`drizzle_engine.cpp:53`「`double drop_area = 0.0;      // drop footprint 球面面积 (sr)`」；`astro_sphere_sink.cpp:295-296`（`4π/(12 nside²)`）。
- 判定：**规范歧义**
- 证据（复核 ✓）：`sed -n '293,305p' docs/contracts/DATA_SEMANTICS.md` 直读确认 sr 表述。
- 影响：`S_p` 实为 ADU/sr；按 ADU/px² 换算的消费方会得到错误零点。
- 修复面：`DRIZZLE.md:29/59` 与 `DATA_SEMANTICS.md:303` 二者统一（登记）。

---

## 2c projection / resample 子域（子代理 P3 分片产出；本审计复核关键行）

> 复核方式：本审计独立读取被引 `file:line`；**生产可达性**另行以根 `CMakeLists.txt` 判定（见 PR-11）。
> 子代理另以 astropy 7.0.1/WCSLIB 逐点对拍（未编译仓库）：TAN/SIN/CAR/AIT 正向 max 2.0e-10 arcsec、往返 max 7.1e-10 px
> ⇒ 投影公式**符合**（P-02/P-04 记符合，见下）。

### PR-01 [C1] resample 的 S（通量）算子列归一被破坏：实现用 `w·Ω'_i` 冒充真实交叠面积
- 规范：`docs/algorithms/v6/phase3/ALG-P3-001_SPEC.md:51`「`通量算子（列归一）: S_ij = |Omega_j ∩ Omega'_i| / Omega_j ,   Σ_i S_ij = 1`」；
  `:50`「`SB 算子（行归一）: R_ij = |Omega_j ∩ Omega'_i| / Omega'_i ,   Σ_j R_ij = 1`」。
- 实现：`lib/algorithms/resample/p3_rsmp_operator.cpp:257`「`n.overlap_sr.push_back(sm.w * omega_out_sr);  // R_ij = w_ij，Σw=1`」（`:285` nearest 同型）；
  权重 `:168`「`op.weight.push_back(a / oi);  // S_ij = a_ij / Omega_j`」。
- 判定：**不符**（R 行归一符合；S 列归一不符）
- 证据（复核 ✓）：`sed -n '250,290p'` 直读确认 `overlap_sr = w·Ω'_out` 与 `S = a/Ω_j`；`ALG-P3-001_SPEC.md:50-51` 直读确认定义。
  ⇒ `Σ_i S_ij = Σ_i w_ij·Ω'_i/Ω_j ≈ Ω'_j/Ω_j ≠ 1`。子代理复算（astropy 球面四角盈余 + 4quad 逐式转写）：
  TAN 10°@0.1°/px + HiPS 等积 Ω（nside=1024）列和 2.985…3.050 ⇒ **点源总通量 +198%~+205%**。
- 影响：`point_source_flux` 模式总通量系统偏高约 3×；且 `Σπ=Σp` 门（`p3_rsmp_propagation.cpp:206`）在真实 HiPS Ω 下必然 REJECT。
  现有 e2e 测试自造 Ω_in 使列和精确为 1（`tests/integration/v6_p3/p3_v6_export_e2e_test.cpp:126-129` 注释自承"非自证"），掩盖该缺陷。
- 修复面：`p3_rsmp_operator.cpp:257`/`:285` 改用真实 `|Ω_j∩Ω'_i|`（或改 `A_ij=w_ij·Ω_j` 保列和=1）并补列和门（只登记）。

### PR-02 [C1] 生产方差传播只算对角特例，无 `C_in` 相关项
- 规范：`docs/science/UNCERTAINTY_AND_COVARIANCE.md:82`「`bilinear:  var_out(i) = [ R C_in Rᵀ ]_ii`」；
  `docs/plugins/algorithms_phase3/15_resample.md:27`「仅输出对角 variance **必须**给相关核/近似误差」。
- 实现：`lib/algorithms/resample/p3_resample.cpp:508`「`acc += w * w * u_k;               // var_out = Σ c_k²·u_k (nearest: c=1)`」。
- 判定：**不符**
- 证据（复核 ✓）：`sed -n '500,515p'` 直读确认；正确实现 `p3_rsmp_covariance.cpp:22-48`（`C_y=R C_x Rᵀ`）无生产调用者。
  子代理量化：四点等权（Σc²=0.25）时 `var_true/var_diag = 1+3ρ`；ρ=0.19 ⇒ 方差低估 36.3%、σ 低估 20.2%。
- 影响：孔径/点源误差被系统性低估（σ 低 ~20%）。
- 修复面：`p3_resample.cpp:489-517` 增相关项或显式输出相关核/近似误差。

### PR-03 [C1] NaN-signal 像素仍写出有限 variance
- 规范：`docs/contracts/DATA_SEMANTICS.md:2557`「`| NaN 传播（足迹内 leaf signal 或 u 为 NaN） | NaN | NaN | NaN | 1 |`」。
- 实现：`lib/phase3_session/p3_session.cpp:321-323`「`var_plane[i] = std::isnan(u_out) ? std::nanf("") : static_cast<float>(u_out);`」（只判 `u`，不判 signal）。
- 判定：**不符**
- 证据（复核 ✓ 子代理引文；`p3_resample.cpp:393-396` 在角点 NaN 时仍 `*coverage = 1`）。
- 影响：signal=NaN 的掩膜/NaN tile 像素产出有限 variance ⇒ 下游"variance 有效即可用"判据被误导。
- 修复面：`p3_session.cpp:320-328` NaN 条件加入 `std::isnan(v)`。

### PR-04 [C3] alpha `FOV ≤ 20°` 冻结限无任何强制点
- 规范：`docs/science/PHASE3_HIPS_TO_FITS.md:151`「TAN 畸变随 FOV 增长——**alpha 适用 FOV ≤20°** 冻结（中心距极点 ≥5°）」。
- 实现：`lib/algorithms/projection/p3_wcs.cpp:101-104`（只查 `|dec|≤85`/`scale>0`/`W,H∈[1,20000]`）；`p3_session.cpp:106-115` 同；
  `p3_proj_v6.cpp:273` 声明 20.0 但 `:395-396` 仅断言 `>0`。
- 判定：**未实现**
- 证据（复核 ✓）：子代理 grep `-i fov` 在 `lib/phase3_session`、`lib/infrastructure/cli`、`module_adapters.cpp` 零命中（本审计未复跑该 grep）。
- 影响：20000px×0.002°/px ≈ 40° 视场静默通过，超出冻结验证域（TAN 像元面积畸变 sec³θ 1.045→1.21）。
- 修复面：`p3_session.cpp:109-115` 加 `scale·max(W,H) ≤ 20°`；或按 SPEC-CONFLICT 统一口径。

### PR-05 [C4] 生产默认采样核与冻结的"生产科学默认核"不符
- 规范：`docs/algorithms/v6/phase3/ALG-P3-001_KERNEL_REGISTRY.md:25`「`bilinear_area_overlap_exact` … 生产默认列 = **是**」。
- 实现：`lib/phase3_session/p3_v6_export.h:126`「`std::string kernel_id = "bilinear_4quad";`」；
  registry 对该核自标 `lib/algorithms/resample/p3_rsmp_kernel_registry.cpp:198`「`k.production_science_default = false;`」。
- 判定：**不符**
- 证据（复核 ✓）：`grep -n production_science_default` 直读：4quad=false（`:198`）、area_overlap_exact=true（`:226`）。
- 影响：默认核与冻结默认不一致；产物 provenance 的 `kernel_id` 与实际权重语义可能不符（见 PR-07）。
- 修复面：`p3_v6_export.h:126` 改 `bilinear_area_overlap_exact`（须先做 PR-06）。

### PR-06 [C6/未实现] 生产默认核 `bilinear_area_overlap_exact` 零实现
- 规范：`ALG-P3-001_KERNEL_REGISTRY.md:25`「精确球面面积重叠（`R`/`S` 定义式）」。
- 实现：`p3_rsmp_kernel_registry.cpp:202-228` 仅注册（`oracle_id="ALG-P3-001-KERNEL-ORACLE/area_overlap_exact"`、`max_interp_err=0.0`）。
- 判定：**未实现**
- 证据（复核 ✓）：`grep -rn "area_overlap" lib/` 仅 5 处，全在注册表/文档，无重叠面积算法。
- 影响：被声明为生产默认的核无实现，其零误差 oracle 证据无代码可复跑。
- 修复面：实现精确球面重叠，或按变更 claim 降级该注册行。

### PR-07 [C5] 核选择绕过 `kernel_id`（标签与实际权重可脱钩）
- 规范：`docs/contracts/v6/data/08_phase3.md:33`「采样核是**产品语义**，由版本化 registry 注册，不能由现码倒推冻结」。
- 实现：`lib/phase3_session/p3_v6_export.cpp:208-210`（只按 `bool` 在 nearest / `make_bilinear_4quad_neighborhood` 间分派）+ `:243`「`p.sampling.kernel_id = kernel_id;`」。
- 判定：**不符**
- 证据（复核 ✓ 子代理引文）。
- 影响：任何通过 `admit()` 的 kernel_id 都用 4quad 权重计算却写该 kernel_id ⇒ 科学不可检出的标签错误。
- 修复面：`p3_v6_export.cpp:189-220` 按 kernel_id 分派，未实现核 REJECT。

### PR-08 [C7] `correlation_output`（默认 true）零实现；`order_limits` 零实现
- 规范：`docs/plugins/algorithms_phase3/15_resample.md:36`「`correlation_output` | **true** | —— | 是否输出相关核」；`:37`「`order_limits` | —— | 输入 order 选择约束（Nyquist）」。
- 实现：`lib/algorithms/resample/p3_resample.cpp:489-517` 只返回对角；`p3_rsmp_covariance.cpp:57-90` 的 `correlation_kernel()` 无生产调用者。
- 判定：**未实现**
- 证据（复核 ✓）：`grep -rn correlation_output lib/` **零命中**；`grep -rn order_limits lib/` 零命中（子代理）。
- 影响：文档化的相关结构（mean|ρ|≈0.19）不进产品 ⇒ 孔径误差低估；无 Nyquist 约束入口。
- 修复面：接 `correlation_kernel()` 到产品面或登记 unavailable；`p3_resample.cpp:199` 增 `order_limits` 入参。

### PR-09 [C7] v3 registry 导出合同字段缺失
- 规范：`contracts/schemas/projection_registry.schema.json:38-42` required 含 `domain/reference_point/crval2_in_mapping/lonpole_default/implemented`。
- 实现：`lib/algorithms/projection/p3_proj_v6.h:96-106` 的 `Spec` 只有 `id/code/ctype1/ctype2/max_abs_crval_dec_deg/max_fov_deg/singularity_kind/pix2world/world2pix`。
- 判定：**未实现**
- 证据：子代理引文（本审计未逐字段复核该 header）。
- 影响：schema 校验必失败；"registry 即机器可校验合同"不成立。
- 修复面：补字段 + 导出器，或走变更 claim 修 schema。

### PR-10 [符合] 投影公式、往返、order 公式、单位表与冻结表一致
- 规范：`docs/plugins/algorithms_phase3/14_projection.md:24`（CAR/AIT 三 Euler 角 + A≤1 + CAR 极行 fail-closed）、
  `docs/science/PHASE3_HIPS_TO_FITS.md:106`（往返 <1e-6 px）、`:78`（`order_needed` 公式）、
  `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json:16-44`（单位表）。
- 实现：`p3_proj_v6.cpp:104-142/213/242`、`p3_wcs.cpp:154-204`、`p3_resample.cpp:199-210` + `healpix_core.cpp:327-331`、`p3_rsmp_units.cpp:77-84`。
- 判定：**符合**（子代理以 astropy 7.0.1 独立对拍：正向 max 2.0e-10 arcsec、往返 max 7.1e-10 px；单位 8 项逐项一致）
- 影响：无。
- 修复面：无。

### PR-11 [生产可达性判定，本审计独立] v6 传播/投影链的生产可达性
- 事实：`astrocs_p3_rsmp`（编 `p3_rsmp_*.cpp` + `p3_resample.cpp`）**在**生产链接闭包内：
  `CMakeLists.txt:890` add_subdirectory(resample) → `lib/algorithms/resample/CMakeLists.txt:20-30` → `CMakeLists.txt:746`
  `target_link_libraries(astrocs_phase3_session PUBLIC … astrocs_p3_rsmp …)` → `:797` `astrocs` 链接 `astrocs_phase3_session`。
  但 `p3_v6_export.cpp`（v6 API 的调用者）与 `p3_proj_v6.cpp` 只编入测试目标
  （`tests/integration/v6_p3/CMakeLists.txt:36-37`、`tests/unit/v6_p3_proj/CMakeLists.txt:16,23`）。
- 判定：**待定/部分**（PR-01/PR-02 所在 TU 在产线二进制内，但生产会话是否调用该 v6 API 未证实；子代理称"V6 链不在生产构建内"与上述 CMake 事实**部分不符**——库在、调用者不在）
- 影响：PR-01/PR-02 的当前产物影响面取决于 v6 API 是否被生产会话调用（需一次调用图核实）。
- 修复面：登记；建议由负责人指派调用图核实（不改码）。

---

## 2d 生产接线子域（第三路子代理 PRODUCTION WIRING 产出；行号钉扎 79efe106…；本审计复核关键行）

### P1-01 [C3] cosmetic 节点把 master_dark/master_bias 恒传 NULL
- 规范：\`docs/plugins/algorithms_phase1/02_cosmetic.md:22\`「坏点/热像素：由 master cosmetic map 或暗场统计判定；修正值来源必须记录」。
- 实现：\`module_adapters.cpp:1973\`「\`const int rc = ac_correct_frame(im.px(), im.w(), im.h(), nullptr, nullptr,\`」；ABI \`astro_calibration.h:89\`「Dark全局统计检测热像素 + Bias全局统计检测冷像素」。
- 判定：**不符**（复核：grep 直读 :1973 引文一致）。影响：热/冷像素检测无统计来源，检出率与规范口径不可比。

### P1-02 [C4] cosmetic 消费的配置键与规范表不是同一组
- 规范：\`02_cosmetic.md:31-35\`（\`cosmetic_map_path/saturation_level/cr_detection=true/cr_sigma=5.0/interpolation=neighbor\`）。
- 实现：\`module_adapters.cpp:1954-1958\`（\`hot_sigma/cold_sigma/method/max_structure_size\`）。
- 判定：**不符**。影响：宇宙线检测（规范默认 true）不可开关/配阈；插值硬编码 median（规范默认 neighbor）。

### P1-03 [C3] cosmetic 不产 validity map 与逐像素修正记录
- 规范：\`02_cosmetic.md:17\`（输出含 validity map 与修正记录）、\`:25\`（variance 必须同步更新）。
- 实现：\`module_adapters.cpp:1985\`（只写 cleaned FITS）+ \`:1999-2000\`（仅帧级 hot_fixed/cold_fixed 计数）。
- 判定：**不符**。

### P1-05 [C1] 最小拟合星数门施加在 inliers 上（3）而规范门是 \`|r_consistent|>=3\`
- 规范：\`docs/science/PHOTOMETRY.md:39\`「\`|r_consistent|>=3\` 才进 IRLS…\`|r_inliers|>=2\` 才估计 \`sigma_residual\`」。
- 实现：\`module_adapters.cpp:3192\`「\`constexpr int P1_PHOT_MIN_FIT_STARS = 3;\`」+ \`:3387\`/\`:3441\`；\`n_matched\` = IRLS 后 inliers（\`frame_photometry_fit.h:83\`）。
- 判定：**不符**。影响：恰好 2 inliers 的规范合法拟合被拒 ⇒ \`scales_complete=false\` ⇒ 整组 \`k_photo\` 不施加。

### P1-06 [C5] \`P1_PHOT_MAX_SIGMA_DEX=1.0\` / \`P1_PHOT_MAX_SPREAD_DEX=0.02\` 无权威数值
- 规范：\`PHOTOMETRY.md:106\` 只**禁绝对窗口**、不给数值；\`config/defaults.json\` photometry.* 六项无对应项。
- 实现：\`module_adapters.cpp:3193-3194\`（:3194 注释「≈0.05 mag 峰峰（负责人判据）」；HEAD 值为 0.5，审计期间被并发改为 0.02）。
- 判定：**不符**。影响：0.02 dex 与同文件自报真实帧对散度 0.0427 dex 同量级 ⇒ 可能拒合法帧。

### P1-07 [C1] 噪声节点的 SNR 信号项仍取检测域 5×5 盒和（F-INSTR 修复残留）
- 规范：\`docs/plugins/algorithms_phase1/07_noise_snr.md:46\`「\`SNR = F_signal / σ_F\`，\`F_signal\` **必须已扣局部背景**」、\`:48\`「帧级 SNR 是**点源（PSF）**量」。
- 实现：\`module_adapters.cpp:3748\`「\`row.flux_adu = s.value("flux", 0.0);\`」（\`p1_sources[].flux\` = \`star_detector.cpp:151\` \`s.flux = m00\`）。
- 判定：**不符**（复核 ✓ 直读 :3748）。影响：\`median_snr\` 低估 1.11×/1.39×/1.85×（FWHM 2/3/4px，盒内份额 0.9038/0.7184/0.5402），随 seeing 2→4px 变 1.67×。

### P1-09 [C3] 帧级 SNR 未写入 HiPS 文件头
- 规范：\`08_drizzle.md:19/51\`、\`07_noise_snr.md:24\`（frame_snr 写 HiPS 文件头）。
- 实现：\`module_adapters.cpp:4254\` 的 KV 表只有 PHOTSCAL/PHOTAPPL/PHOTDEGRADE 等，无 SNR 键；引擎 \`hp_drizzle_api.h:81\`「不写 snr, 不写 drizzle provenance」。
- 判定：**未实现**。

### P1-11 [C3] drizzle 不喂 variance 块 ⇒ variance/ivar 子产品永不产出
- 规范：\`08_drizzle.md:19\`、\`:33\`（\`C_out = R C_in Rᵀ\`；只存对角必须另存相关核/摘要）。
- 实现：\`module_adapters.cpp:4218\` 只 \`aio_frame_add_block(frame,"data",…)\`；引擎契约 \`hp_drizzle_api.h:80-81\`（含 variance 块才追加子产品）。
- 判定：**未实现**。影响：writer 的成对校验门（:4456）成死门。

### P1-12 [C3] 缺上游帧目录时写 null + status 并返回 success（规范要求 fail-closed）
- 规范：\`07_noise_snr.md:128\`「帧级 SNR 无法计算 → fail-closed，**不得用受天光影响的普通 SNR 代替**」。
- 实现：\`module_adapters.cpp:3840\`「\`frame["snr_catalogue_status"] = "unavailable_no_upstream_frame";\`」后继续、函数仍 success。
- 判定：**不符**。

### P1-13 [C1/SPEC-ERR] WCS 往返门用**已废止**的稀疏网格 \`<1e-6 px\` 口径
- 规范：\`docs/science/ASTROMETRY.md:160\`「往返不变量…在**独立密集域**（≥1000 随机点+边角）上 \`max_abs ≤ 1e-4 px\`；原「7×7 网格 <1e-6 px」口径…鉴别力为零（自网格 1.8e-12 px vs 离网格 3.10 px），**已废止**」。
- 实现：\`module_adapters.cpp:2906\`「\`const int step = std::max(1, std::max(W, H) / 8);\`」+ \`:2667\`「\`…max_rt >= 1e-6\`」+ 失败消息 "exceeds 1e-6 contract"。
- 判定：**不符**（自称的 contract 在现行规范中不存在）。

### P1-14 [C5] sdet 9 参数硬编码（含 \`maxStars = 2000\`）
- 规范：\`docs/contracts/DATA_SEMANTICS.md:745\`（SDetParams 9 字段生产消费面缺口 = DISP-STAR-003）。
- 实现：\`module_adapters.cpp:2803-2811\`（\`structureLayers=5\`、\`iterativeClipSigma=5.0f\`、\`maxStars=2000\`、\`maxAxisRatio=2.0f\`…）。
- 判定：**不符**（无配置键）。

### P1-15 [C7] writer manifest 恒写 \`bunit="ADU"\`，产品可能是相对测光通量
- 规范：\`docs/contracts/DATA_SEMANTICS.md:541\`「写盘 BUNIT 必须随 PHOTAPPL 区分…**禁止在已定标帧标 BUNIT=ADU**」。
- 实现：\`module_adapters.cpp:4622\`「\`(*man)["bunit"] = "ADU";\`」；同链上游 \`:4378\` 如实写 \`photometry_applied ? "ASTROCS_RELATIVE_FLUX" : "ADU"\`。
- 判定：**不符**。

### P1-16 [待定] 星等缺失哨兵 \`99.0\` vs 合同 NaN 语义
- 合同：\`DATA_SEMANTICS.md:754\`（\`box_sum≤0 或拟合失败=NaN\`）。
- 实现：\`module_adapters.cpp:2097\`「\`? -2.5 * std::log10(s.flux) : 99.0;\`」（该数组喂 dpsf；是否外泄为产品 mag 列未确认）。
- 判定：**待定**。

### SPEC-ERR-CAL [SPEC-ERR] 文档称 \`apply_photometry\` 未编译未接线（与事实冲突）
- \`docs/algorithms/CALIBRATION_ALGORITHMS.md:262/465\` vs \`CMakeLists.txt:454\`（编入 \`astrocs_calibration\`）+ \`module_adapters.cpp:3537-3538\`（生产调用）。
- 判定：**SPEC-ERR**（文档滞后；本审计"k_photo 未施加"嫌疑由此产生，实测**已施加** ⇒ 该嫌疑**不成立**）。

### SPEC-CONFLICT-UNIT [SPEC-CONFLICT] 同一端口 \`DATA-P1-FLUX\` 单位两说
- \`docs/modules/registry/astrocs.phase1.noise-snr.md:35\`「\`UnitId::ELECTRON\`」 vs \`…photometry.md:43\`「\`UnitId::ADU\`（与 DATA_SEMANTICS §14.1 psf_flux 一致）」；实现 \`module_adapters.cpp:829/854\` 写 \`UnitId::ELECTRON\`。
- 判定：**规范歧义**（与 PH-07 同源）。

---

## 3 魔数回查清单（范围内实现文件；仅列影响科学结果的常数）

| # | 常数 | 位置 | 规范出处 | 判定 |
|---|---|---|---|---|
| 1 | `1.482602218505602` | `noise_model.cpp:61`、`wrapper_phase1/noise_model.h:15`、`photometer.cpp:92`、`tests/.../p1noise_oracle.hpp:44` | `NOISE_MODEL.md:53/124`（唯一权威写法） | 符合 |
| 2 | `0.6744897501960817` | `star_matcher.cpp:21` | `PHOTOMETRY.md:21/53/130` | 符合 |
| 3 | `4.685` / `1e-6` / `3.0`(mag_tol) | `star_matcher.cpp:23,27`；`pc_api.cpp:145,435` | `PHOTOMETRY.md:22/55/50/102` | 符合 |
| 4 | `0.7316727929211932` | `noise_model.cpp:37`、`snr_science.cpp:39`、`snr_estimator.cpp:74,99` | `PSF.md:23/81/119`、`NOISE_MODEL.md:124` | 符合 |
| 5 | `1.230310` | `snr_science.cpp:37`、`snr_estimator.cpp:63,66,97` | `PSF.md:20/51/86` | 符合（值对；**输入口径错**，见 PH-03） |
| 6 | `2.3548200450309493` / `2.3548` | `star_detector.cpp:162`、`sdet_api.cpp:48,57,238` | `PSF.md:92` 禁改 `FWHM_FACTOR`（Moffat4=1.230310） | **不符**（PH-03） |
| 7 | `0.1` / `1.5` / `0.75` / `9216` / `8` / `60`(=10×6) / `5.0` / `1e-12` / `0.0625` | `noise_model.cpp:115,148,321-325,626-638` | `NOISE_MODEL.md:45,53,82-85,126,142` | 符合（NS-07） |
| 8 | `2.5`（掩膜专用 Moffat 翼指数） | `noise_model.cpp:148` | `NOISE_MODEL.md:79`（公式用 β 未定值）、`:75`（实测用 β=2.5） | 符合 |
| 9 | `1.253` | `snr_science.cpp:246`、`snr_frame_science.h:16` | `NOISE_MODEL.md:131`（`1.253=√(π/2)`） | 符合 |
| 10 | `1.5×FWHM`（孔径默认） | `snr_science.cpp:188` | `06_photometry.md:32` 声明 `2×FWHM` | **不符/歧义**（NS-08） |
| 11 | `max(30, 12·FWHM) ≤ 256`（轮廓网格 half） | `snr_science.cpp:79-84` | 无规范条款 | **待定**（C5） |
| 12 | `4.0/6.0/10.0 px`（孔径/天空环） | `photometer.h:26-28` | `photometry/README.md:173-174`（4.0/6-10）；`06_photometry.md:32`（2×FWHM） | **歧义**（PH-05） |
| 13 | `1.2` / `[1,10]°` / `≥30°`（FOV） | `frame_photometry_fit.cpp:129-135` | 无权威条款（仅 `docs/archive` 记 `[0.1,30]°`） | **待定**（PH-10） |
| 14 | `{12,13,14,15,16}`（自适应 `mag_max`） | `pc_api.cpp:291,708,978` | `PHOTOMETRIC_FIT.md:147` 已登记（DISP-PHOT-005） | 已登记 |
| 15 | `50000.0`（饱和绝对阈） | `star_detector.cpp:167` | `NOISE_MODEL.md:43`（电平来源优先级 = cfg > `SATURATE` > `DATAMAX`） | **不符**（硬编码取代元数据；跨模块，S1 域） |
| 16 | `kLn10 = 2.302585092994045684017991454684` | `noise_model.cpp:34` | 数学常数 | 符合 |
| 17 | `5.0`（5σ 深度） | `snr_estimator.cpp:108`、`snr_science.cpp:184,224` | `CONTROL_WEIGHT_SNR.md:54`、`depth_m5.schema.json:293` | 符合 |
| 18 | `1.253`/`2.5`（dex→mag） | `noise_model.cpp:563,572` | `PHOTOMETRY.md:24/62` | 符合 |

---

## 4 汇总统计

## 4 汇总统计

**规范陈述条数（登记条目）= 61**（含 4 条判"符合"的正向锚点；不含 §3 魔数清单中的逐项常数）。
结论**钉扎在工作树 sha256 `79efe106…`（module_adapters.cpp, 9042 行）**；PH-01/PH-03 因并发在途修复而双列 HEAD/工作树状态。

| 判定 | 条数 |
|---|---|
| 不符 | 29 |
| 未实现 | 14 |
| 规范歧义 | 7 |
| 待定 | 7 |
| 符合 | 4 |

按缺陷类别分组（复合标签按首个 C 归组；"其他"= SPEC-ERR / SPEC-CONFLICT / 纯待定 / 符合项）：

| 类 | 条数 | 条目 |
|---|---|---|
| C1 口径不符 | 12 | PH-01, PH-02, PH-03, NS-03, NS-09, DZ-01, DZ-02, PR-01, PR-02, P1-05, P1-07, P1-13 |
| C2 常数不符 | 1 | PH-03（fwhm 常数，与 C1 同条） |
| C3 未接线 | 10 | NS-01, NS-02, NS-04, DZ-03, PR-04, P1-01, P1-03, P1-09, P1-11, P1-12 |
| C4 默认值不符 | 5 | PH-05, PH-06, NS-05, NS-08, PR-05, P1-02（含复合标签） |
| C5 硬编码绕过 | 6 | PH-10, NS-08, DZ-09, PR-07, P1-06, P1-14 |
| C6 有实现无调用 | 5 | PH-09, NS-10, DZ-04, DZ-05, PR-06 |
| C7 合同不符 | 6 | PH-07, PH-08, DZ-06, PR-08, PR-09, P1-15 |
| 其他（SPEC-ERR / SPEC-CONFLICT / 待定 / 符合） | 16 | PH-04, PH-11, NS-06, NS-07, NS-11, NS-12, DZ-07, DZ-08, DZ-10, DZ-11, DZ-12, PR-10, PR-11, P1-16, SPEC-ERR-CAL, SPEC-CONFLICT-UNIT |

**规范侧问题（与实现缺陷分列）**：SPEC-ERR 4 条（DZ-07 `DRIZZLE.md:54` 订正方向写反；DZ-08 `211034.6` 陈旧小数；PH-11 文档称 Photometer 未接线；SPEC-ERR-CAL 文档称 `apply_photometry` 未接线）；
SPEC-CONFLICT/规范歧义 9 条（DATA-P1-FLUX 单位 ELECTRON vs ADU；sparse_snr_layer true vs false；smoothing_lambda 0.0 vs >0；
孔径 2×FWHM vs 4.0px；mode psf vs 孔径；方差低估 36.3% vs 23.3%；Phase3 variance BUNIT 同文件互斥；FOV≤20° 是否硬门；面积单位 sr vs px²）。

**被推翻的嫌疑（诚实登记）**：① "k_photo 算出但从未施加" **不成立**——`module_adapters.cpp:3537-3538` 逐帧施加，`CMakeLists.txt:454` 已编入；
② "reject_profile 与合同不符" **不成立**——合同 `phase2_uncertainty_rejection_provenance_v1.json:67` 已订正为 canonical=`astrocs_adaptive_pixel`。

### 4.1 最严重的 5 条不符（含证据与影响）

1. **NS-01 [C3] 生产 Phase1 噪声节点根本没跑 SCI-NOISE-001 §5/§5a**——`module_adapters.cpp:3649/3824` 调 `wrapper_phase1/noise_model.cpp:145-195`（整帧 median+MAD）；
   合规实现 `cpp/src/noise_model.cpp` 所在的 `astrocs_p1_noise` 因 `CMakeLists.txt:240` 被注释而不在根构建图（`ci/ledgers/dormant_algorithms.json:310-312` 已登记）。
   影响：空间方差场 `b,c` 整体丢失、无逐星掩膜/饱和过滤/预算门 ⇒ σ_bg 污染无上界、ivar 退化为单标量。
2. **PH-01+PH-03 [C1/C2] 通量口径与 PSF 尺度双重错位**——`F_instr` 原为 5×5 正性截断盒和（`star_detector.cpp:151`）而规范要 PSF 拟合域通量（`PSF.md:52/87`）；
   `fwhm_px` 生产者用 2.3548（`star_detector.cpp:162`）、消费者按 1.230310 反解 σ（`snr_science.cpp:42`/`snr_estimator.cpp:66`），比值 **1.914005**。
   影响（独立复算）：seeing 2→4px 假帧间差 **0.455 mag**；σ_F ×2.11 ⇒ SNR ×0.473 ⇒ m5 浅 **0.81 mag**。
   **工作树已含未提交修复**（`p1_psf_analytic_flux` + star_id 关联 + `kGaussFwhmFactor`/`detectionSigmaFromFwhm` 分流），但**噪声节点仍用盒和**（`:3748` ⇒ P1-07）。
3. **DZ-01+DZ-03 [C1/C3] drizzle 用 legacy 权重且生产不写通量守恒因子**——`drizzle_engine.cpp:1531` `w=a_jp/A_drop,j` vs 规范 `a_jp/A_pixel,j`（`DRIZZLE.md:43`）；
   生产 HiPS 不写 provenance/`flux_conservation_factor`（`astro_sphere_sink.cpp:330`）。影响：pixfrac<1 时面亮度 **×1/pixfrac²**（0.8→+56.25%）、方差 **×1/pixfrac⁴**（×2.44），下游不可订正。
4. **NS-03 [C1/C7] `frame_snr` 字段装的是 5σ 深度**——`module_adapters.cpp:3928-3933` 含 `flux5_adu/m5_mag`，合同 `frame_snr.schema.json:5` 明文禁止与 `depth_m5` 互填；真 SNR 在 `:3889` 的 `median_source_snr`。
5. **PR-01 [C1] resample 的 S（通量）算子列归一被破坏**——`p3_rsmp_operator.cpp:257` 用 `w·Ω'_i` 冒充 `|Ω_j∩Ω'_i|`，违反 `ALG-P3-001_SPEC.md:51` 的 `Σ_i S_ij=1`；
   子代理以 astropy 复算列和 2.985…3.050 ⇒ 点源总通量 **+198%~+205%**（生产可达性待定，见 PR-11）。

### 4.2 修复面清单（只登记，未改任何文件）

| 条目 | 修复面（file:line + 改法） |
|---|---|
| NS-01 | `CMakeLists.txt:240`（或 `:620-623` 加编 `noise_snr/cpp/src/noise_model.cpp`）+ `module_adapters.cpp:3649/3824` 改调 `snr_noise_model_v1*` |
| NS-02 | `wrapper_phase1/noise_model.cpp:145`（加 saturation 形参）；`module_adapters.cpp:3824-3831`（SATURATE/DATAMAX + 降级键） |
| NS-03 | `module_adapters.cpp:3928-3933`（frame_snr 改 `snr_f`；深度另立 `depth_m5` 对象） |
| NS-04/PH-09 | `module_adapters.cpp:3889-3933`（输出 `point_information` 与 `psfsw_robust`，按 `psfsw_enable`） |
| PH-01/PH-02 | 工作树 `:2036-2037/:2167/:3259-3286` 已在途（**需提交 + 构建验证**）；`:3145` 的 `p1_flux.json` 仍无生产读者 |
| PH-03 | 工作树 `snr_science.cpp:50/60-61/118/155` + `snr_estimator.cpp:40/72/81-82` 已在途（需提交验证） |
| PH-05/PH-06 | `06_photometry.md:31-32` 与 `photometry/README.md:173` 二选一为权威；`aperture_radius/sky_annulus/mode` 入 `config/defaults.json` 并由 `module_adapters.cpp:3066` 读取 |
| PH-07/PH-08 | `module_adapters.cpp:829/854` + `module_ports.registry.json`（单位改 ADU；输入端口改 `DATA-P1-SOURCES`） |
| P1-01/P1-02/P1-03 | `module_adapters.cpp:1973`（传 master_dark/bias）、`:1954-1958`（读规范键）、`:1985-1995`（validity/修正记录） |
| P1-05/P1-06 | `module_adapters.cpp:3387/3441`（门改 inliers≥2）；`:3193-3194` 待 SCI 权威阈值 |
| P1-07 | `module_adapters.cpp:3748`（改 PSF 域 flux） |
| P1-09/P1-11 | `module_adapters.cpp:4254`（HiPS 头 frame_snr）、`:4218`（variance 块） |
| P1-12/P1-13/P1-14/P1-15 | `:3840`（fail-closed）、`:2906/2667`（密集域 + 1e-4 px）、`:2803-2811`（配置键）、`:4622`（BUNIT 条件化） |
| DZ-01/DZ-02 | `drizzle_engine.cpp:1531` → `weight = overlap_area*(pixfrac*pixfrac)/drop_area` |
| DZ-03 | `astro_sphere_sink.cpp` `write_hips_phase1`（~:324-341）补 provenance 与 `flux_conservation_factor` |
| DZ-06 | `drizzle_engine.cpp:1913,1919` → `if (!std::isfinite(v) \|\| v <= 0.0f) continue;` |
| DZ-07/DZ-08 | `DRIZZLE.md:54`（走变更 claim）；`drizzle/README.md:93`、`DRIZZLE_GEOMETRY.md:89` → `211076.3` |
| PR-01 | `p3_rsmp_operator.cpp:257/:285`（真实交叠面积或 `A_ij=w_ij·Ω_j`）+ 列和门 |
| PR-02/PR-08 | `p3_resample.cpp:489-517`（相关项/相关核）；接 `p3_rsmp_covariance.cpp:57-90` |
| PR-03/PR-04 | `p3_session.cpp:320-328`（NaN 条件加 `std::isnan(v)`）、`:109-115`（`scale·max(W,H) ≤ 20°`） |
| PR-05/PR-06/PR-07 | `p3_v6_export.h:126`（默认核）、实现 `bilinear_area_overlap_exact` 或降级注册行、`p3_v6_export.cpp:189-220`（按 kernel_id 分派） |
| PR-09/PR-11 | `p3_proj_v6.h:96-106`（补 schema 字段 + 导出器）；PR-11 建议指派 v6 API 调用图核实 |


---

## 5 复算记录（本审计独立计算，未运行构建/测试）

```text
# C2（PH-03）: 以 snr_science.cpp:51-77 的离散 Moffat4 轮廓复算 ΣP²（网格 half 按 σ 等比放大以消除截断）
sigma=1.000000  half=60   sumP2=2.49956563e-01
sigma=1.914011  half=115  sumP2=5.59516575e-02
ratio = 0.223846 ; sqrt = 0.473123  ⇒ σ_F ×2.113615, SNR ×0.473123, Δm = -0.8126 mag
2.3548200450309493 / 1.230310 = 1.914005  (⇒ 2.5·log10 = 0.7049 mag 若仅按常数比)

# C1（PH-01）: Moffat4 β=4，5×5 盒（中心 ±2）捕获通量份额
FWHM=2.0 px ⇒ 0.9367 (0.0711 mag) ; 3.0 ⇒ 0.7849 (0.2630) ; 4.0 ⇒ 0.6162 (0.5257) ; 5.0 ⇒ 0.4763 (0.8053)
seeing 2.0 → 4.0 px 的盒和比 = 1.5201 ⇒ 0.4547 mag 假帧间差
```

## 6 范围与未决

- 本分片**未**覆盖：`docs/science/**` 中与 photometry/noise 无关的域；Phase2/Phase3 非本分片实现域。
- 交叉引用（他分片域，仅登记不判）：`star_detector.cpp:151/162/167`（S1 实现域，但是 PH-01/PH-03/魔数#15 的生产者）、
  `reject_profile` 生产 `astrocs_adaptive_pixel`（`docs/contracts/DATA_SEMANTICS.md:1043` 已裁定为生产默认，
  S3/S4 域）、`smoothing_lambda` 生产 0 vs 工具 0.1（`ci/ledgers/config_default_divergences.json:7-21` 已登记，S3 域）。
- `/dev/shm/astrocs_conf2` 缺失已记 §0.1；本报告结论以工作树（≡ HEAD）为准。
