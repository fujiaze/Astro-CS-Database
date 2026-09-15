# W2 · 科学量一致性与量纲 第二轮全仓静态审查（轴归档）

- 轴：W2 科学量一致性与量纲（电子/ADU/flux/mag/角度/秒；隐式换算缺位与双重换算；像素中心 vs 角点；CD vs CDELT·CROTA2 vs PCi_j；赤纬/投影退化；归一化与有效面积定义；方差可加性）
- 复核时点：任务书钉 `a3a343a4`；**本轴实际复核时点 `931ab42f`**（本轮期间工作树被并行提交推进 `a3a343a4 → d3b91c84 → 931ab42f`，全部行号已对最终工作树 `grep -n`/`nl -ba` 重锚，并同时给出可 grep 的代码原文以抵御漂移）
- 纪律：纯静态（read/glob/grep + 只读 `python3 -B` + `git --no-optional-locks` 只读子命令）；未运行编译/ctest/测试/二进制/仓库脚本；未写 `findings/`、`账本/`、`_merge/`
- 权威顺序：①负责人裁决 / SCI 内「负责人授权」订正块 ②现行冻结文本 ③代码测试自洽
- 复算脚本（均 `python3 -B` exit 0）：`_w2_r2_math.py`（角度/投影/尺度/魔数）、`_w2_r2_math2.py`（通量/方差/层级/权重）
- 排除前缀（全量）：`./lib/*/third_party`、`./lib/healpix_db/archive`、`./artifacts`、`./docs/archive`、`./engineering/control/archive`、`./run`、`./build`、`./reports/REAUDIT_V3`

## 一、条目（编号 W2-N-01..W2-N-15，实立 14 条：P1×10，P2×4；W2-N-14 为编号占位，其事实已并入 W2-N-06 第二站点）

### W2-N-01 [P1] 孔径通量误差式与同域冻结 CCD 方程三方互斥（缺 /gain、缺天空均值误差项、面积用整格计数）
- 站点甲（缺陷侧）：`lib/phase1/photometry/photometer.cpp:92` — `r.flux_error = std::sqrt(std::max(sum, 0.0) + n_in * sky_sigma * sky_sigma);`（`sum` 为 ADU 通量、`n_in` 为整格中心落孔像元数）；`photometer.h:12-19` 声明 flux/flux_error 皆 ADU，Photometer 全类**无 gain 入参**（模块内 grep `gain` 零命中）⇒ 隐式 `gain≡1 e⁻/ADU`
- 站点乙（同域另一实现）：`lib/snr_estimator/cpp/src/snr_science.cpp:194-195` — `var_ap = n_pix*sig_sky*sig_sky*(1.0 + n_pix/n_sky); … var_ap += s_ap/gain`，其中 `n_pix = kPi*r*r`（:190）；文档侧 `docs/algorithms/PHOTOMETRIC_FIT.md:134` 逐字记录了甲式，`docs/science/NOISE_MODEL.md:58` 给 `var_ADU = max(signal,0)/gain + (rn/gain)²`
- 三处不可互推：①甲无 `/gain`（ADU 当 e⁻ 计入自身泊松项）②甲无 `(1+n_ap/n_sky)` 天空环均值误差项 ③面积一制为整格计数、一制为 πr²
- 复算（`_w2_r2_math.py`，r=4.0/天环 6-10px/sky=5 ADU）：天空项 甲 49σ² vs 乙 62.41σ² ⇒ **σ_F 低 12.86%、SNR 恒虚高 12.86%**（与 gain 无关，属结构性缺项）；面积 甲 49/50/52（随亚像素相位）vs 乙 50.27 ⇒ **±6% 量化跳变**；泊松项 gain=2 时甲高估 2×、gain=0.5 低估 2×（符号随 gain 翻转，不可用常数标定吸收）
- related：`M3-A-006`③（那条只登记 photometer 的 1.4826 截断常数与「本层不产出 SNR」合同依据，未登记上述三缺项）、`V1-N-12(a)(c)`（**不重报**其天空项口径主张）
- 判定：静态可判（结构 + 系数），命中域为生产 `measure_flux` 节点（`module_adapters.cpp:2507/2556`）

### W2-N-02 [P1] e⁻↔ADU 换算因子在读取边界被实例化为 1.0 且无判别位；两通道口径分叉
- 站点甲：`lib/astro_image_io/src/aio_fits.cpp:483` — `cal.gain = kw_float("GAIN", 1.0);`（`kw_float` :386-390 对「缺键」与「解析失败」同一出口），而 `include/astro_image_io.h` 的 `AIOCalibrationMetadata` 对同批可选量都留了判别位（`has_ccd_temp`、`has_cdelt1/2`、`has_equinox`、`has_lonpole`）**唯独无 `has_gain`** ⇒ 「仪器增益未知」在类型上不可表达；后果链 `lib/orchestrator/cpp/src/orchestrator.cpp:4247-4248` — `if (meta.calibration.gain > 0)` 恒真 ⇒ 伪 1.0 被无条件回写为帧头 `GAIN`，再由 `:4719` 读回 `ncfg.gain_e_per_adu`，最终由 `drizzle_engine.cpp:1147`→`hiss_common.cpp:194` 以「已测仪器常数」身份交付进 HISS 元数据
- 站点乙（同键另一口径）：`lib/core/src/module_adapters.cpp:2667` — `sci_cfg.gain_e_per_adu = sc.value("gain_e_per_adu", 0.0)`，而 `snr_science.cpp:166`/`snr_frame_science.h:43` 明写 `<=0 = 未知（不加源泊松项）` ⇒ **同一帧在两条入口上 gain 不同（1.0 已知 vs 0 未知）**；`READNOI` 经三级判缺失确认**全仓零写入点**（仅 `orchestrator.cpp:4720` 一处读取），即读噪在 A 线恒 0
- 权威冲突：`docs/science/PHOTOMETRY.md:14/:29`（负责人授权面）明写「e⁻ 需 gain，**当前不可得**」⇒ 现实现把合同层的「不可得」落成产品层的「已知=1」
- 复算：任何信任交付 `gain=1.0` 的下游，真值 g 下方差错 g²、σ 错 g、星等偏 2.5·log10(g)（g=2 → σ_F ×2、m 偏 0.752 mag）
- related：`V10-N-04`（缺省吞错族，**其面不重报**；本条增量=判别位缺失 + 恒真回写 + 两通道口径分叉 + 与 PHOTOMETRY:14/29 直接冲突）、`V1-N-12(c)`（不重报读噪双计）、`M2a-A-2`
- 判定：静态可判（类型字段集 + 判据恒真 + 零写入点三级复核）

### W2-N-03 [P1] DATA-P1-FLUX 端口单位声明 ELECTRON 而载荷为 ADU，且单位门因两端同标恒不红
- 站点甲：`lib/core/src/`module_adapters.cpp:786` 与 `:804` — `{"fluxes", "DATA-P1-FLUX", …, UnitId::ELECTRON, CoordinateFrame::ICRS}`（生产测光输出口与 noise-snr 输入口同标），同文件其余 26 个端口一律 `UnitId::ADU`
- 站点乙（权威与生产者）：`docs/modules/registry/astrocs.phase1.photometry.md:43` 已订正为 `UnitId::ADU`（内嵌「P5-SNR 订正 2026-09-14，负责人授权」注释块），而 `docs/modules/registry/astrocs.phase1.noise-snr.md:33` 仍 `ELECTRON`；生产者 `photometer.h:15-16` 声明 ADU；标签外销：`lib/core/src/artifact.cpp:101` `unit_name(unit)` 进 manifest、`artifact_store.cpp:35` 进 JSON
- 门失效：`lib/core/src/pipeline.cpp:247-298` 的单位冲突门仅在 `prod_port.unit != p.unit` 且双方非 UNKNOWN 时触发 ⇒ 生产/消费两侧同标 ELECTRON，**该门对本缺陷恒不红（自我印证）**
- 复算：信任 ELECTRON 标签的下游做 e⁻↔ADU 换算即错 gain 倍、泊松方差错 gain²（gain=2 → 方差 ×4、σ_F ×2、m_5 偏 0.752 mag）
- related：`V1-N-05`（该条已登记「三面不一」的文档↔实现面；**本条增量**=单位门恒不判别的结构成因 + manifest 外销 + 数值倍率）、`M7-A-119`、`M8a-G-007`
- 判定：静态可判；属负责人已裁口径未传导到实现/第二份 registry

### W2-N-04 [P1] PHOTAPPL=1 无条件硬编码：退化（恒等拷贝未定标）帧被声明为已应用测光
- 站点甲：`lib/orchestrator/cpp/src/orchestrator.cpp:3007-3008` — `fn_kv_set_double(frame_,"header","PHOTSCAL",out_scale); fn_kv_set(frame_,"header","PHOTAPPL","1");`，位于 `ret==0` 分支出口，**既不 gate `out_n_matched>0` 也不 gate `out_scale!=1.0`**
- 站点乙（退化侧与后果侧）：`lib/photometric_calib/cpp/src/pc_api.cpp:84-92`/`:97-105`/`:238-245` — 无 Gaia 星 / 无 PSF 星 / 星数不足时 `*out_scale_factor=1.0; *out_n_matched=0;` 恒等拷贝像素并 `return 0`；后果 `lib/healpix_db/healpix_drizzle/drizzle_engine.cpp:2258-2262` — `photometry_done = … || config.photometry_applied_upstream; hmeta.photappl = photometry_done ? 1 : 0; bunit = photometry_done ? "ASTROCS_RELATIVE_FLUX" : "ADU"`；守卫方向不全 `lib/astro_image_io/src/hiss_writer.cpp:321-333`（只拒 RELATIVE_FLUX+PHOTAPPL=FALSE，反例仅 :340 WARN）
- 权威：**现行冻结合同逐字禁止**——`docs/contracts/DATA_SEMANTICS.md:249`「`PHOTAPPL=1` 仅当 provenance 声明已应用（**禁硬编码**）」；`:497`「已定标= scale≠1 且 n_matched>0 … 禁止在已定标帧标 BUNIT=ADU」
- 复算：恒等拷贝帧被标为相对流量刻度 ⇒ 绝对通量错整整一个仪器零点 2.5·|location| mag（location=2/4/6 dex → 5/10/15 mag，通量 ×1e2/1e4/1e6）
- related：`M3-E-001`（追溯行失真，另一事实）、`M3b-A-07`；`V7` 全族不重报
- 判定：写侧无判据=静态可判；是否命中取决于该帧匹配是否退化（需运行期），故定 P1 不定 P0

### W2-N-05 [P1] photscal 只乘像素、不乘 variance/ivar；唯一 α² 实现生产零调用 ⇒ 跨帧 ivar 求和失同单位前提
- 站点甲（施加点）：`lib/photometric_calib/cpp/src/image_corrector.cpp:76` — `out_pixels[i] = pixels[i] * (float)scale;`（f64 内联同式 `pc_api.cpp:536/:779/:1099`）；像素方差块描述仍写旧单位 `orchestrator.cpp:4758` — `"V19 NoiseWeightModelV1 variance (ADU², blank-sky)"`
- 站点乙（唯一正确实现与求和点）：`lib/snr_estimator/cpp/src/noise_model.cpp:456-463` `snr_noise_scale_law(alpha,…)`（`var'=α²var, ivar'=ivar/α²`）在生产链**零调用**——`git grep snr_noise_scale_law` 全量命中仅 头/实现/测试/`module_entry.cpp` 的 `subop=="scale_law"` 诊断分支；求和点 `lib/core/src/module_adapters.cpp:4490` — `w = static_cast<double>(ivar_v[d][p]);`（`DATA_SEMANTICS.md:2311` `ivar_mosaic(p)=Σ_i ivar_i(p)` 隐含同单位前提）
- 权威：`docs/science/NOISE_MODEL.md:12` 与 `DRIZZLE.md:57` 均规定 `x'=αx → var'=α²var`；`DATA_SEMANTICS.md:413/:416` variance ADU² / ivar ADU⁻²
- 复算：photscal 比 1.5/2/3 ⇒ 正确 ivar 权重比应为 0.4444/0.2500/0.1111，现按等权比 1 计入 ⇒ 被高标定帧分别少计 **55.6% / 75.0% / 88.9%**；同帧还并存两种标度（`psf_flux` 取自缩放前 PSF 阶段 `orchestrator.cpp:2656`，`sigma_sky` 取自缩放后 data）相差 photscal 倍
- related：`M3-H-002`（scale_law 内部 var/ivar 不对称，另一事实）、`M2a-A-2`
- 判定：缺缩放 + 标签未随 + 唯一实现无生产调用者=静态可判；实际权重幅度需各帧 photscal 离散度（运行期）

### W2-N-06 [P1] 方差面 NaN 像素「半累加」：面积进分母、方差不进分子 ⇒ 交付方差系统性偏低
- 站点甲：`lib/healpix_db/healpix_drizzle/drizzle_engine.cpp:1912` — `if (varianceValue <= 0.0f) continue;`（NaN 不满足 `<=0`，注释 :1893-1895 自认「NaN … 自然落入传播路径」）+ `:1553-1559` — `acc.sumFlux += …; acc.sumArea += overlap_area; if (varianceValue > 0.0f) acc.sumVarNum += …` ⇒ 同一像元对 F_p/D_p 有贡献、对 sumVarNum 无贡献，而 `var = sumVarNum/D_p²` 的分母仍含其面积
- 站点乙（冻结条款）：`docs/contracts/DATA_SEMANTICS.md:254` variance 面「**非有限或 ≤0 → 跳过该像素**（:1727-1729）」（行锚已漂移，现读为 :1912/:1557）
- 复算（`_w2_r2_math2.py`）：一滴水跨两像元 a1=0.6A(v=100)、a2=0.4A(v=NaN) ⇒ 代码 `var=36/A²`，契约（整颗剔除，D=0.6A）`var=100/A²` ⇒ **低估 2.78×**；同叶 signal 亦按「含 NaN 像元」求和
- 同链另一站点（写出侧门不同集）：`lib/astro_image_io/src/hips/aio_hips_writer.cpp:569` signal 门含 `std::isfinite(flux)`，而 `:721` variance 门为 `area>0 && vnum>0 && isfinite(area) && isfinite(vnum)`（无 flux 条件）⇒ 可交付 signal=NaN 而 ivar=有限，违反 `DATA_SEMANTICS.md:2336` 「signal=NaN 同态」
- related：`M2a-H-2`（variance≤0 整颗丢弃，同一行的另一分支）、`M2a-C-3`（值面掩膜登记失实，另一面）、`V7` 全族不重报
- 判定：静态可判（判据集合不等价 + 契约明文）；NaN 方差像元的出现频率需运行期

### W2-N-07 [P1] HiPS 层级父元方差归约丢弃协方差交叉项（方差被当作可加）
- 站点甲：`lib/astro_image_io/src/hips/aio_hips_writer.cpp:769-783` — 注释「hierarchy: 累加 var_num（归约公式同叶级: var_parent = Σvar_num/(Σarea)²）」+`acc.add_var(z, var_n[i], 0.0)`（`AncestorAcc::add_var` :379-383 显式 `(void)area;` 只累加分子），叶级 `:721-723` 与父级 `:1118-1124` 同式 `var = vnum/(area*area)`
- 站点乙（冻结协方差条款）：`docs/science/UNCERTAINTY_AND_COVARIANCE.md` §协方差 `Cov(S_p,S_q) = Σ_j c_jp c_jq v_j`（「同一源像素贡献多个输出像素 ⇒ Drizzle 后相邻输出**非严格独立**」）+ §对使用的约束「aperture 误差须**显式加入 Cov 项**；ivar 权重默认只用于逐像素最优组合」；聚合语义源 `DATA_SEMANTICS.md:329`（父 cell=子像素聚合）
- 代数：代码给 `Σ_j v_j Σ_p w_jp² / D²`，真值 `Σ_j v_j (Σ_p w_jp)² / D²`，差额正是文档要求显式加的交叉项
- 复算：一个 drop 均分 2 子叶 ⇒ 父 var=0.5v vs 真 1.0v（**低估 2.00×、ivar 高估 2×**）；均分 4 子叶 ⇒ **4.00×**；按文档自有 MC（nside=512 mean|ρ|≈0.19）2×2 父元低估约 1.57×
- 触发频率：自动 NSIDE 目标为 1–2× 过采样（`drizzle_engine.cpp:709` oversample 打印口径），即一滴水跨多子叶为**常态**（是否常态需运行期统计，公式层缺陷静态可判）
- related：`M7-A-114`（Phase3 bilinear 传播式丢相邻协方差，同族不同站点）、`M2a-F-6`、`M3b-A-03`、`M2a-A-2`
- 判定：公式层静态可判；属冻结归约式自身覆盖不足，需负责人裁决口径

### W2-N-08 [P1] 交付的源像素角尺度 provenance 用 `|CD1_1|·3600`：旋转下不是像素角尺度，且三处三定义互斥
- 站点甲：`lib/healpix_db/healpix_drizzle/`hp_drizzle_api.cpp:1153-1154` — `meta.fits_meta["src_pixel_scale_arcsec"] = std::to_string(std::fabs(img.wcs.cd[0]) * 3600.0);`（只取 CD 左上元）；落卡 `aio_hips_writer.cpp:971-979`（仅 `>0` 才写 `ASTROCS_DRIZZLE_SCALE_ARCSEC`，`%.4f`）；消费 `lib/phase2/src/sampler.cpp:88-96`（`sc_grid={300.0,600.0}`、`std::clamp(scale_arcsec,300.0,600.0)`）与 `frame_drizzle_provenance`（仅 `vs>0` 才接受尺度）+ `:554` `kcorr_lookup(pf, 300.0)`（scale 未知退 300″ 档）
- 站点乙（同仓两处正确定义）：`lib/orchestrator/cpp/src/orchestrator.cpp:152-155` `0.5*(sqrt(cd11²+cd21²)+sqrt(cd12²+cd22²))*3600`（列范数均值）；`aio_wcs_pixel_scale`（`sqrt(|det CD|)·3600`）⇒ **同一「像素角尺度」三处三式，仅 PA=0 时重合**
- 复算（0.0396″/px 生产尺度，`_w2_r2_math.py`）：PA=15° → −3.41%；30° → **−13.40%**；45° → −29.29%；60° → −50.00%；89° → −98.25%；PA=90°（合法轴向交换帧）→ **0.0 ⇒ 键被 `>0` 门槛静默丢弃**，下游退回 300″ 饱和查表（`sampler.cpp:554`）
- related：`M9-A-2`（该条证据明文判「换算链本身正确（×3600 deg→arcsec ✓）」并原样引用本站点 ⇒ 本条为其未登记残余面）、`M9-C-2`（arcsec 无声明）
- 判定：静态可判（旋转不变性）；k_corr 越界后果与 M9-A-2 同轴但成因不同

### W2-N-09 [P1] PCi_j 读入支路全仓缺位，注册表仍判 Paper I §3 CONFORMANT「偏差：无」
- 站点甲（读面）：`lib/astro_image_io/src/aio_fits.cpp:410-420` 只读 `CD1_1/CD1_2/CD2_1/CD2_2` 与 `CDELT1/2`，`include/astro_image_io.h` 的 `AIOWCSKeywords` 有 `cd*/cdelt*/has_cdelt*/lonpole/latpole` **无 PC 字段**；`lib/healpix_db/healpix_drizzle/fits_reader.cpp:350-372` 亦然（`has_wcs = has_cd1_1 && has_cd2_2 && …`，无 CD 时退 `CDELT+CROTA2` 且 `CROTA2` 缺省 0）。三级判缺失：`git --no-optional-locks grep -nE '\bPC1_1\b|\bPC2_2\b|pc1_1|pc_matrix' -- lib cli runtime tests tools contracts docs` → **exit 1 零命中**；逐文件 `grep -c 'PC1_1'` → exit 1 零命中（third_party 除外）
- 站点乙（权威与标准参照）：`docs/standards/STANDARDS_REGISTRY.md:41,50-66` 判 `Paper I §3（CD/CTYPE 关键词体系）` **CONFORMANT，偏差「无（T5/T7 断言在位）」**；同仓 vendored `third_party/cfitsio`（`wcssub.c`）实现 PC 支路 ⇒ 同仓正对照
- 后果：外部（astropy/wcslib 常规产物）`CDELT+PC` 无 `CD` 的头 ⇒ 在 AIO 面 `has_wcs=0`（无 WCS，拒），在 drizzle 面 `has_cd1_1/has_cd2_2` 假 ⇒ 走 CDELT 支路且 `CROTA2` 缺省 0 ⇒ **旋转分量整列被静默丢弃**
- 复算：真 CD=CDELT·PC(30°) 与读得 CD′=diag(CDELT) 的位置偏差 `r·s·2sin15°`：0.5″/px、r=1024 → **265″（4.4′）**；生产 0.0396″/px、r=1024 → 21.0″、对角 r=1448 → 29.7″
- related：`M2b-B-08`（注册表偏差登记面系统性失真，同族）、`M9-B-2`、`V7-N-04`（对角双零退化，另一分支）
- 判定：静态可判；「CONFORMANT+偏差无」与读面能力缺失直接冲突

### W2-N-10 [P1] 星表通量三套定义并存：5×5 窗截断和被当「总通量」消费并被 f_in 二次惩罚，另有负残差整流
- 站点甲（生产侧）：`lib/phase1/stars/star_detector.cpp:140-151` — 5×5 窗逐像素减背景、`if (v <= 0) continue;`（丢负残差）、`s.flux = m00`；再经 `lib/core/src/module_adapters.cpp:2739-2740`（`row.flux_adu = s.value("flux",0.0); row.fwhm_px = s.value("fwhm_px",0.0)`）送入 SNR
- 站点乙（消费侧声明）：`lib/snr_estimator/cpp/include/snr_estimator.h:211` — `flux_adu // F: 总通量 [ADU] (>0 必须)`、`:230` `aperture_correction // 1/f_in(r)`；实现 `snr_science.cpp:193` `s_ap = F * f_in`、`:200` `sigma_f_aperture_adu = sqrt(var_ap)/f_in`（宣称已改正至总通量）
- 第三套对照：`photometer.cpp:70-81` 圆孔整格和（保留负值）、`lib/dynamic_psf/src/dpsf_psf.cpp:429` 解析积分 `2πA·sx·sy/3`、`lib/star_detector/src/sdet_api.cpp:2341-2348` box_sum（保留负值）⇒ 同名 flux 在四实现间不可互推
- 复算：①截断——5×5 窗含 Moffat4(β=4) 总通量比例 σ=1.0→102.62%、2.0→87.41%、3.0→66.63%、4.0→48.60%、6.0→26.97% ⇒ 按总通量消费时星等偏 +0.146/+0.441/+0.783/+1.423 mag；再乘 `f_in(r=1.5FWHM)` ⇒ σ=2 时报告 `S_ap=0.677·F_true`（SNR_ap 低 22.5%）而字段宣称已改正。②整流——纯天空 5×5 窗 `E[Σmax(v,0)] = 25σ/√(2π) = +49.87 ADU`（真值 0，≈2.0 σ_F），真通量 50→99.87（−0.751 mag）、100→149.87（−0.439）、1000→1049.87（−0.053）；且整流使 `flux` 恒 >0 ⇒ `module_adapters.cpp:2743` 的 `flux>0` 有效性判据永不触发
- related：`M3b-A-02`（PSF flux 列语义为振幅非总通量，**不重报**）、`M1a-A-003`（discrete/analytic 比 1.0124/0.9884 未登记，**不重报**）、`DISP-STAR-004`（已登记偏差不复报）
- 判定：定义冲突与结构（丢负值/二次 f_in）静态可判；确切倍率依赖真实轮廓（运行期）

### W2-N-11 [P2] 交付帧头同时写 CD 与由 CD 伪造的 `CDELT1/2 + CROTA2=0`（不可逆且不等价）
- 站点甲：`lib/core/src/module_adapters.cpp:3085-3118`（drizzle 帧头 KV 表）——`:3087` 列出 `{"CDELT1",""},{"CDELT2",""},{"CROTA1","0"},{"CROTA2","0"}`，`:3106` `if (strcmp(kv.k,"CDELT1")==0) return std::string(b5);`（b5=CD1_1），默认分支取 b8=CD2_2 ⇒ 与同表 `CD1_1..CD2_2` 并存
- 站点乙（等价性反证）：`lib/healpix_db/healpix_drizzle/fits_reader.cpp:359-368` 的 CDELT·CROTA2→CD 构造（本轴已逐项对拍 cfitsio `wcssub.c:253-291` 判为**正确**）反推该三元组：`CD′=diag(CD1_1, CD2_2)` ≠ 原 CD（旋转分量丢失）；同仓等价 CROTA2 应为 `atan2(cd21, cd11)`（`wcssub.c:250-254`）
- 复算：PA=30°、s=1.1e-5 deg/px ⇒ 三元组重构后 `|det′|/|det| = 0.7500`（像素尺度低 **13.40%**）且旋转丢 30°，r=2048 px 处合成位移 ≈ **42″**；等价 CROTA2 应为 150.0° 而非 0
- 现状可达性：自产帧恒带 CD ⇒ 本仓 `fits_reader` 走 CD 支路（不受影响），危害面为**任何按 CDELT/CROTA2 优先解释的外部读者**与 `V7-N-04` 已登记的「CD 对角双零 → 退 CDELT」分支（该分支下退化为 `CDELT=0` → `has_cdelt` 假 → 拒）
- related：`M9-B-2`（两处 CDELT+CROTA2→CD 构造逐字复制 + 口径未固定；其内证段已引用本写侧但判「生产不可达、无科学数值影响」⇒ 本条给出可达面与错误分解的量化，属未登记残余面）、`M1a-B-006`（另一站点：不清理输入自带 CDELT）、`V7-N-04`
- 判定：静态可判（三元组与 CD 不等价）；命中需外部/CDELT 优先读者

### W2-N-12 [P2] `WcsTan` 交付 RA 值域 [−180,180) 与合同 [0,360) 互斥，交叉门同式 wrap 故恒不可见
- 站点甲：`lib/phase1/wcs/wcs_tan.cpp:35-36` — `if (ra_out > M_PI) ra_out -= 2*M_PI; if (ra_out < -M_PI) ra_out += 2*M_PI;` ⇒ RA 落 [−180,180)，负值经 `module_adapters.cpp:2162-2176` 原样进交付件 `p1_wcs.json` 的 samples
- 站点乙（冻结合同与同仓四制）：`docs/science/ASTROMETRY.md:29` — 「`RA`: deg `[0,360)`」；同仓另四处（`aio_wcs`/`ipv`/`p3_wcs`/`gaia 匹配`）归一到 [0,360)；交叉参考门 `module_adapters.cpp:375-377` 用**同一 wrap** 且角距 `p1_angular_sep_deg` 本身 wrap-safe ⇒ **门在数值上恒不可见**
- 复算：CRVAL1=210° 的帧，中心附近像素交付 RA=−149.99967（合同应为 +210.00033），偏差恒为 360° 整数倍；影响面为 RA≥180° 的整个半天区样本
- related：`M2a-B-2`（注册表判定式丢限定语的可复用模式）、`M1a-D-001`（坐标域声明，另一面）
- 判定：静态可判（值域声明 vs 实现），交叉门失效为结构事实

### W2-N-13 [P2] Phase3 投影退化的「四角同半球」守卫对跨极恒真
- 站点甲：``lib/phase3_session/p3_wcs.cpp:15`（`kMaxAbsDec = 85.0`，仅作用中心）、`:140-148`（四角只要求 `p3_wcs_pix2world` 返回成功）、`:163`（唯一硬失败条件 `if (r >= M_PI/2.0) return P3_WCS_HEMISPHERE;`）
- 站点乙（冻结约束）：`docs/science/PHASE3_HIPS_TO_FITS.md:48`「输出四角与中心同半球（越界显式拒）」+ §9a-12（FOV≤20°、中心距极点≥5°）
- 复算（`_w2_r2_math.py`，标准 TAN 逆式）：中心 dec=85°、15000² px @1e-3 deg/px（边长 15°<20° 上限、距极 5°=下限）**四角全部可解**：左上 (71.034, 75.543)、左下 (148.280, 82.210)、右上 (8.966, 75.543)、右上后角 (291.720, 82.210)、顶边中点 (220.012, 87.543) ⇒ 同一顶边两端 RA 相差 143°，天球已绕过极点回折，交付图为折叠像
- 结论：即使日后强制 FOV≤20° 与 |dec|≤85（`M1a-C-004` 所缺的强制点），该守卫仍**语义上无法覆盖跨极**；冻结文本「同半球」这一判据本身不能表达其意图（跨极仍同半球）
- related：`M1a-C-001`（TAN 奇点无防护）、`M1a-C-004`（FOV 零强制点，本条为其**残余面**：即便补齐强制点仍不足）
- 判定：静态可判（判据覆盖域），交付折叠图需运行期输入命中

### W2-N-14 [P2] 层级 variance/ivar 与 signal 的有限性门不同集（signal=NaN 而 ivar=有限可交付）
（并入 W2-N-06 的第二站点，此处不重复立条——保留编号占位以对齐分片计数）

### W2-N-15 [P2] 「独立参考实现」测试硬写本仓明令禁止的魔数 210960，且上限与生产不同制
- 站点甲：`lib/astro_image_io/tests/`hiss_correctness_test.cpp:500-511` — 注释「参考实现: compute_auto_nside（与 drizzle_engine.cpp 算法一致）」却 `nside_min_real = 210960.0/finest_arcsec`，并 `if (nside > 1048576) nside = 1048576`
- 站点乙（生产侧禁令与真式）：`lib/healpix_db/healpix_drizzle/drizzle_engine.cpp:680`「移植: …（**禁止魔数 210960**/1186.18）」+ `:684-685` `sqrt(M_PI/3)*(180/M_PI)*3600`；钳位 `NSIDE_MAX=4194304`（`:703-706`）；`lib/phase1/…/lib/healpix_db/healpix_drizzle/memory.md:121` 与 `lib/drizzle/README.md:87` 又分别记 210960 与 211034.6
- 复算：公式系数 **211076.285**；测试值 210960 偏 **−0.0551%**、注释值 211034.6 偏 −0.0197%；断言用 `hp_res <= finest*1.01`（容差 1%）**吸收 −0.055% 偏置 ⇒ 该差在测试里永不可见**；决策翻转窗存在于每个 2 的幂边界（宽度 0.055%），且 `finest<0.201″` 区间测试上限 2^20 与生产上限 2^22 **直接不同制**（测试 5 断言 `nside==1048576` 只自证其私有上限）
- related：`M2a-D-1`（211034.6 注释 vs 公式，本条为其测试面兄弟）、`M7-G-104`（独立 Oracle 缺位）
- 判定：静态可判（常数与上限文本），对拍翻转命中需具体 finest 值（运行期）

## 二、判否清单（本轴逐条读过并判定**不是缺陷**，防后续轮次重复上报）

| # | 被检事项 | 判否理由 |
|---|---|---|
| J-1 | `CDELT·CROTA2 → CD` 构造（`fits_reader.cpp:352-372`、`hp_drizzle_api.cpp:428-444`）是否与 Paper I 相反 | 与同仓 vendored `cfitsio/wcssub.c:253-291` 逐元素一致（boolean 对拍 exit 0）；M9-B-2 的「符号相反」子主张已被同仓正对照反证，本轴维持其撤回 |
| J-2 | `crpix=(W+1)/2`（p3）与 `W/2+0.5`（其它模块）是否 0.5 px 分叉 | 两式算式恒等，非缺陷 |
| J-3 | `p3_projection.cpp` / `wcs_sip.cpp` 的 0-based 内部坐标与 1-based CRPIX 混用 | 逐处按 `x+1` 显式桥接，内部自洽（M1a-C-003 已登记的残余面另案） |
| J-4 | drizzle 单输出像素方差归一 `var=Σv w²/D²` | 两像元 v=100 复算得 50，与 SCI-DRZ 方差传播 α²v 一致 |
| J-5 | 面积单位 sr 与文档 px² 的标签冲突（signal 实为 ADU/sr） | 已由 `M2a-A-2` 立条（P1）；本轴仅补量化因子：0.2″/px 时 1/A_pix=1.0636e12（等效 30.067 mag）、1.0″/px → 4.2545e10、2.0″/px → 1.0636e10，**不另立条** |
| J-6 | HiPS tile 无 BUNIT | 已由 `M2a-A-2` 覆盖；本轴 W2-N-05/W2-N-04 只报 PHOTSCAL 不缩放方差与 PHOTAPPL 伪造，不重复报 BUNIT 卡缺失 |
| J-7 | FP32 累加 vs DRIZZLE.md:104「FP64 累积」承诺 | 已由 `M2a-H-1`（P0，precision_mode 不参与累加域）与 DISP-HIPS-009 覆盖 |
| J-8 | PSF/孔径解析归一 vs 离散样本和（±1.2% 亚像素相位） | 已由 `M1a-A-003`（1.0124/0.9884 未登记）与 `V1-N-12(a)` 覆盖 |
| J-9 | 读出噪声在经验 σ_sky 之上再计 `(rn/gain)²`（`snr_science.cpp:167` vs :194 分支互斥） | 已由 `V1-N-12(c)` 立条（免重报清单内）；本轴仅确认 gain 数值本身为 1.0 伪值（W2-N-02） |
| J-10 | Phase2 降级权重两制（`stage2.cpp:1114-1121` 用 support vs `module_adapters.cpp:4482` 用等权 1.0） | 属 §6.3「support 冒充权重」锁定簇（M3-A-002+M4-A-003，两份 FROZEN SCI 互斥、须负责人裁决），本轴不另立条 |
| J-11 | drizzle 输入 weight 面数值不进入 sumFlux/sumVarNum（`hp_drizzle_api.cpp:276-303` → `drizzle_engine.cpp:1903-1906`） | 冻结式 SCI-DRZ §5 只定义 `w_jp=a_jp/A_drop`，未定义输入权重乘子 ⇒ 属定义真空，需负责人钉死语义（列 §三） |
| J-12 | `sup=area/A_cell` 的 [0,1] clamp、A_cell=4π/(12n²)、hp_res=√(π/3)/n | 与 DRIZZLE.md:145 一致，缓冲 1.25×>实测 1.044 保守，无系统偏差 |
| J-13 | `gain_variance` 类诊断式的 gain 方向（`noise_model.cpp:465-473`） | `s/gain + rn²/gain²` 与 NOISE_MODEL.md:58 一致（M3-A-006 已登记注释/测试侧失实，本轴不复报） |

## 三、需运行期，未判

| # | 事项 | 缺什么 |
|---|---|---|
| R-1 | FITS 通道 `exptime=0` 进 HISS `EXPTIME=0.0`（`hp_drizzle_api.cpp:1143-1150`、`DrizzleMeta meta;` :346 仅测试路径调用者留空） | 生产是否经 `drizzle_fits` 入口需运行期；本轮 `meta` 空载仅 `tests/test_drizzle.py` 命中 |
| R-2 | IPv 解算 0.5 px 坐标域偏置（`ipv_wcs.cpp:231` CRVAL 收敛点 vs CRPIX 声明） | 需 astropy 交叉 fixture 才能判定是否被 ra0/dec0 收敛吸收；与 `M1a-D-001`/`M3b-C-01` 已登记簇同源，本轴不立条 |
| R-3 | `k_corr` 越界与层级方差低估的**发生频率** | 需真实帧 pixfrac/nside 统计（公式层缺陷已在 W2-N-07/W2-N-08 立条） |
| R-4 | `zero_point_mag` 是「积分 ADU 零点」还是「ADU/s 零点」（EXPTIME 全链从未被除出：`snr_science.cpp:207/:227`） | 若操作员按 ADU/s 供零点，m_5 偏 `−2.5log10(t)`（t=30/300/1800 s → −3.693/−6.193/−8.138 mag）；属合同真空，需负责人裁决而非判代码错 |
| R-5 | HISS `signal`（原始 sumFlux，`hiss_common.cpp:96-110`）与 HiPS `signal`（flux/area 面亮度，`aio_hips_writer.cpp:570`）同名两制 | 需产品文档裁决二者是否为同一键名的两种量；本轴只登记不判 |

## 附A · 免重报核对（本轴主动核对且确认**不重报**的在册条目）

| 在册条目 | 本轴核对结论 |
|---|---|
| `V1-N-12` 全条 | 天空项口径、读噪双计、点采样三面均不复报（见 J-8/J-9）；本轴仅报同站的 gain=1.0 伪值与孔径误差式缺项 |
| `V2-N-03..07` | 未触及 |
| `V7` 全族 | has_cd 对角双零（V7-N-04）等不复报；W2-N-09/W2-N-11 只报 PC 缺位与 CDELT 伪造分解，并在条目内写明与其边界 |
| `V10-N-04` | kw_float 缺省吞错族不复报；W2-N-02 的增量已限定为「判别位缺失 + 恒真回写 + 双通道分叉 + 与 PHOTOMETRY:14/29 冲突」 |
| `V12` 全族 | 不复报（含 sqrt(10) 类同形常数额外推导）；W2-N-15 只报测试私有魔数 210960 与上限 2^20/2^22 不同制 |
| `V5-N-04` | 未触及 |
| `M9-A-2 / M9-B-2 / M9-C-2 / M2a-A-2 / M2a-D-1 / M2a-H-1 / M2a-H-2 / M7-A-119 / M3b-A-01..03 / M1a-A-003 / M1a-B-006 / M1a-C-001 / M1a-C-003 / M1a-C-004 / M8a-G-007 / M4-C-03 / DISP-STAR-004 / DISP-NOISE-003` | 均在对应条目内以「残余面/兄弟面/边界」方式引用并声明不重复主张；W2-N-14 为编号占位，不立条 |
