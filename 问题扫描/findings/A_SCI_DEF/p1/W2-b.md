# W2 片3-4 · P1 四条（HEAD 931ab42f 重锚）

### W2-N-07（P1）HiPS 层级父元方差归约**丢弃协方差交叉项**（方差被当作可加）⇒ 父元方差低估 2×/4×
- 甲：`aio_hips_writer.cpp:769-783` 注释「归约公式同叶级 `var_parent=Σvar_num/(Σarea)²」+ `acc.add_var(z,var_n[i],0.0)`，而 `AncestorAcc::add_var:379-383` **显式 `(void)area;` 只累加分子**；父级 `:1118-1124` 再 `var=vnum/(area*area)`。乙：`UNCERTAINTY_AND_COVARIANCE.md` §协方差 定义 `Cov(S_p,S_q)=Σ_j c_jp c_jq v_j`，并要求 aperture 误差**显式加入 Cov 项**。
- 代数：代码给 `Σ_j v_j·Σ_p w_jp²/D²`，真值 `Σ_j v_j·(Σ_p w_jp)²/D²`，**差额正是文档要求显式加的交叉项**。复算：一 drop 均分 2 子叶 ⇒ 父 var=0.5v 对真 1.0v（**低估 2.00×**），均分 4 子叶 ⇒ **4.00×**；按文档自有 MC（nside=512，mean|ρ|≈0.19）2×2 父元低估约 **1.57×**。自动 NSIDE 目标 1–2× 过采样（`drizzle_engine.cpp:709`）⇒ 一滴水跨多子叶是**常态**。
- **属冻结归约式自身覆盖不足 ⇒ 需负责人裁口径**。related `M7-A-114`（Phase3 bilinear 同族不同站点）、`M2a-F-6`、`M2a-A-2`、`W2-N-05`

### W2-N-08（P1）交付的源像素角尺度 provenance 用 `|CD1_1|·3600`——**旋转下不是像素角尺度**，且同一量三处三定义互斥
- 甲：`hp_drizzle_api.cpp:1153-1154` 只取 CD 左上元；落卡 `aio_hips_writer.cpp:971-979`（仅 >0 才写）；消费 `sampler.cpp:88-96` 的 `std::clamp(scale_arcsec,300.0,600.0)` + `:554 kcorr_lookup(pf,300.0)`。乙（同仓两处正确定义）：`orchestrator.cpp:152-155` 的 0.5×(‖cd col1‖+‖cd col2‖)×3600、`aio_wcs_pixel_scale` 的 √|det CD|×3600 ⇒ **仅 PA=0 时重合**。
- 复算（0.0396″/px 生产尺度）：PA=15° → −3.41%；30° → −13.40%；45° → **−29.29%**；60° → −50.00%；89° → −98.25%；**PA=90°（合法轴向交换帧）→ 0.0 ⇒ 键被 >0 门槛静默丢弃、下游退回 300″ 饱和档**。
- **关键关联**：`M9-A-2` 的证据明文判「换算链本身正确（×3600 ✓）」并原样引用本站点 ⇒ **本条是其未登记残余面**。related `M9-A-2`、`M9-C-2`、`W2-N-11`

### W2-N-09（P1）**`PCi_j` 读入支路全仓缺位**，注册表仍判 Paper I §3 `CONFORMANT`、偏差「无」⇒ 旋转分量整列被静默丢弃
- 甲：`aio_fits.cpp:410-420` 只读 CD 与 CDELT，`AIOWCSKeywords` **无 PC 字段**；`fits_reader.cpp:350-372` 的 `has_wcs` 只看 CD 四元，无 CD 时退 `CDELT+CROTA2` 且 **CROTA2 缺省 0（该文件自印 default 0）**。三级判缺失：对 `PC1_1`/`PC2_2`/`pc_matrix` 的 git grep 与逐文件 grep **-c** 均 **exit 1 零命中**（third_party 除外）。
- 乙：`STANDARDS_REGISTRY.md:41`/`:50-66` 判 Paper I §3 CONFORMANT、偏差「无（T5/T7 断言在位）」；**同仓 vendored `third_party/cfitsio/wcssub.c` 实现了 PC 支路 ⇒ 同仓正对照**。
- 后果：外部（astropy/wcslib 常规产物）的 `CDELT+PC` 无 CD 头 ⇒ AIO 面 `has_wcs=0`（无 WCS 被拒）、drizzle 面走 CDELT 支路 ⇒ **旋转分量丢失**。复算：0.5″/px、r=1024、PC(30°) ⇒ **位移 265″（4.4′）**；生产 0.0396″/px、r=1024 → 21.0″、对角 r=1448 → 29.7″。related `M2b-B-08`（登记面系统性失真）、`M9-B-2`、`V7-N-04`

### W2-N-10（P1）星表通量**三套定义并存**：5×5 窗截断和被当「总通量」消费并被 `f_in` 二次惩罚，另有**负残差整流**
- 甲：`star_detector.cpp:140-151` 5×5 窗减背景、**`if (v<=0) continue;`（丢负残差）**、`s.flux=m00`；经 `module_adapters.cpp:2739-2740` 送入 SNR。乙：`snr_estimator.h:211` 注为「F：总通量 [ADU]」、`:230 aperture_correction=1/f_in(r)`，实现 `snr_science.cpp:193` `s_ap=F*f_in`、`:200 sigma_f_aperture_adu=sqrt(var_ap)/f_in`（**宣称已改正至总通量**）。第三套对照：`photometer.cpp:70-81`（整格和保留负值）、`dpsf_psf.cpp:429`（解析积分）、`sdet_api.cpp:2341-2348 box_sum`（保留负值）⇒ **同名 flux 四实现互不可推**。
- 复算：①截断——5×5 窗占 Moffat4(β=4) 总通量比例 σ=1.0→102.62%、2.0→87.41%、3.0→66.63%、4.0→48.60%、6.0→26.97% ⇒ 按总通量消费时**星等偏 +0.146/+0.441/+0.783/+1.423 mag**；再乘 f_in(r=1.5FWHM) ⇒ σ=2 时报告 `S_ap=0.677·F_true`（**SNR_ap 低 22.5%**）而字段宣称已改正。②整流——纯天空 5×5 窗 `E[Σmax(v,0)] = 25σ/√(2π) = +49.87 ADU`（**真值 0，≈2.0σ_F**）⇒ 真通量 50→99.87（−0.751 mag）、1000→1049.87（−0.053）；且**整流使 flux 恒 >0 ⇒ `module_adapters.cpp:2743` 的 `flux>0` 有效性判据永不触发**。
- related `M3b-A-02`、`M1a-A-003`、`DISP-STAR-004`（不复报）、`M3b-C-01`、`W2-N-01`
