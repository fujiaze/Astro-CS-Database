# Science Doc Review — Stage A (V19R8)

Date: 2026-08-22  
Scope: `docs/science/*.md` (10篇) + `docs/validation/SCIENCE_FREEZE.md` + `docs/contracts/DATA_SEMANTICS.md`  
Tool: `tools/docs_machine_consistency.py` + `tools/config_consistency_check.py` + 人工逐篇公式/量纲/误差传播核验  
Commit: Stage A 单一目的提交（见 git log）

## 结论总览

| 项 | 结论 |
|---|---|
| docs_machine_consistency | PASS 9/9 |
| config_consistency_check | PASS mismatches=[] |
| P0 科学定义模糊/矛盾 | 0 |
| P1 待澄清/可优化 | 3 (非阻塞，见清单) |
| 与 09 马赛克 32 Red=11+11+10、10 梯度、11 叠加闭合性 | 闭合（见 §4） |
| 本阶段对 `docs/science/*.md` 的修改 | 0 必要修改，2 个最小编辑（措辞/锚点补全，非语义变更） |

## 逐篇 Verdict

### 1. SCIENCE_SCOPE.md — PASS
- 定义天球辐射场 HiPS signal + variance/ivar 权威链入口清晰，处理链 5 步与 L1→L2→L3 一致。
- 变量/单位：ADU/e-/deg/HEALPix NESTED/tile+local xy/dex/mag 均显式，单位闭合。
- 假设/有效域/失效域：dither分组、patch平稳、稀疏掩膜、NO_DATA/UNDERDETERMINED/INPUT_CORRUPT 显式状态完备。
- 量纲：variance 信号²，ivar 信号⁻²，与 DRIZZLE/NOISE/UPM 一致。
- 关联：SCI-SCOPE-001 → TRACEABILITY.csv VERIFIED。

### 2. CALIBRATION.md — PASS
- 公式双分支 `cal=(raw-dark)/flat_norm` 与 `cal=(raw-bias-K·(dark-bias))/flat_norm` 与 `lib/calibration/src/calibrator.cpp:104-136` 一一对应，K=t_light/t_dark 无量纲正确。
- flat_norm=`max(median_flat,0.1)` floor 0.1 + median=1.0 与 `calibrator.cpp:78-93,90,120,164` 一致，数值安全明确。
- 失效语义：母版缺失/滤镜不匹配显式错误 vs 平场异常静默 clamp+警告 vs FITS IO 显式拒绝，三类分层无歧义。
- 误差：母版噪声不进 ivar（由 snr_estimator 独立），与 NOISE_MODEL §系统/随机误差边界一致。
- 建议 P1-01：已在文档中显式“flat_norm 无量纲”避免与信号单位混淆（已满足）。

### 3. ASTROMETRY.md — PASS
- 前向模型 `ra,dec=TAN(CD·(x-x0,y-y0)+SIP)`，FITS WCS Paper II + SIP 2005 引用正确。
- 坐标约定闭合：内部 0-based `[0,width-1]` ↔ FITS 1-based `CRPIX=width/2+0.5` 换算 `xp=x+1`，`x0=CRPIX-1` 推导无歧义；CD `deg/pixel`，`cd_inv` `pixel/arcsec` 仅换算，单位正确。
- SIP 前/逆区分：`A/B = cd_inv·trans高阶` + 逆 `AP/BP` 经 `NB_GRID=7` 最小二乘 + `AP[1,0]-=1,BP[0,1]-=1` 去线性，Y-down 转换 `cd12/cd22` 取反、SIP `(-1)^j` 规则正确，`|det(CD)|` 不变。
- 有效域：极区 `|dec|>45°/>85°` 分支 + `C=π/2 / C45=π/(2√2)` Lipschitz 保守剪枝 `false_negative=0`，与 `lib/gaia_xpsd_client/src/gaia_client.c` 一致。
- 失效/精度：FP64，CD/SIP 入 FITS 头，SCI-AST-001 闭合。

### 4. PSF.md — PASS
- Moffat4 β=4 `I(r)=B+A/(1+Q)^4`，`Q` 椭率参数 `p1/p2/p3(sx,sy,θ)`，退化 `sx=sy`→`Q=0.5r²/σ²`，`α=√2σ`，`FWHM=2√2σ√(2^{1/4}-1)≈1.230310σ` 推导正确，`MOFFAT4_FWHM_FACTOR=1.230310` 与 `lib/dynamic_psf/src/dpsf_psf.cpp:24,339` 一致。
- 积分通量 `flux=2πA·sxsy/3` 标注“整平面延伸假设”边界正确。
- PsfFitQuality：`residual_scale=10-90% trimmed mean |res|`，`robust_residual_sigma=residual_scale/0.7316728`，`q_psf=A/residual_scale` 为拟合质量代理非 SNR，与 `lib/snr_estimator/cpp/src/noise_model.cpp:35-37 kTrimMeanToSigma=0.7316727929211932` 区分于 `NOISE_MODEL` 的 `1.482602218505602` MAD 常数，二者不可互换已显式警告。
- 数值：7 参 LM 唯一路径，θ 四象限 `trimmed-mad` 消歧，与 `dpsf_psf.cpp:98-182,351-363` 一致。

### 5. NOISE_MODEL.md — PASS
- 目标量 `variance of calibrated blank-sky random component` ≠ PSF/测光 scatter / q_psf，边界 SCI-NOISE-003/008/010 正确。
- 算法：8×8 patch、fixed conservative `rmax=max(1,r0)×max(1,scale)` 不按亮度缩放（已冻结）、`σ_bg=1.4826022185·MAD`、`5σ ≤2轮` 裁剪、`var(x,y)=a+bx+cy` 平面场（`enable_spatial_field && n≥4`，否则常量）+ `clamp max(...,floor)` 正确；`control_ivar` 路径与 PHASE2_UPM 衔接。
- Floor：`variance_floor=1e-12` 经 `g_model_floor` 按 model 指针注册、`free` 擦除，无全局共享，已在 `lib/snr_estimator/cpp/src/noise_model.cpp:32,126,210,256,344,403,433` 验证。
- Gain/Readnoise 诊断模型：`max(signal,0)/gain + (rn/g)²` 仅 diagnostic（SNR-005 5% 一致性校验），生产 `source==0 empirical` 不融合，字段仅追溯，NO-01 P0 风险已封堵；单位 `e-/ADU`、`e-`、`ADU` 正确。
- 生产配置 `NOISE-WIRE-001` 三者 exact（`nullptr`/`default_config()`/生产默认含 spatial_field 开启 + floor 1e-12）正确。
- 失效：无合格 patch → degenerate 全局兜底 vs `ivar=0,r=1` 拒绝分层正确；退化阈值 `min_samples/2`、`robust_sigma` 非有限≤0 显式。
- 精度：FP64，MAD `1.482602218505602` 与 trimmed 常数并列警告正确。

### 6. DRIZZLE.md — PASS
- Fruchter & Hook 线性重建 `w_jp=a_jp/A_drop`，`F_p=Σx_j w_jp`，`D_p=Σa_jp`，`S_p=F_p/D_p` 通量守恒正确。
- 方差传播 `sumVarNum+=v_j·w_jp²`，`variance_p=sumVarNum/D_p²`，`ivar=1/variance` 与 `lib/healpix_db/healpix_drizzle/drizzle_engine.h:45,49,64` 及 `astro_sphere_sink.cpp:100` 归一分层一致；缩放律 `x'=αx→var'=α²var,ivar'=ivar/α²` (SNR-002) 互引正确。
- 单位：S 信号，D px²，v 信号²，坐标 NESTED leaf 9 阶正确。
- 有效域/缓冲分层：quick-reject `1.25×hp_res` / candidate `3.0×` / fast `1.25×+1.15` + 极冠回退，与 `spherical_overlap.cpp:40` 一致，false_negative=0。

### 7. PHASE2_UPM.md — PASS
- 定义 `calibrated=raw-C_f`，`C_f` 8×8 control cell 双线性，Huber IRLS + control-ivar 感知权重 + 弱零锚 + 连通分量 gauge（每分量 `min frame_id`）正确。
- 权重冻结 `SCI-UPM-WEIGHT-001`：`w_UPM=quality×geometric_reliability×control_ivar`，`raw=quality·control_ivar`，`normalized=raw/sum·geom` per-control，与 `docs/algorithms/UPM_SOLVER.md:45` + `lib/phase2/src/upm.cpp:9,1121,1140` + `lib/phase2/src/sampler.cpp:41,515,817` 一致；`control_variance=k_corr·(π/2)·σ_bg²/N_retained`，`k_corr=1.4` (empirical 1.3883，2000实现，`sampler.cpp:41-52`)，`N_retained` 用裁剪后样本，`k_corr` per-frame `frames[frame_id].kcorr>0 ? per-frame : cfg` 回退正确；`support` 仅 eligibility/coverage，禁止 `snr²/(1+snr²)/support^p` 生产乘因子已冻结。
- 持久化绑定 `SCI-UPM-PERSIST-001`：`parameter_rows[index]↔frame_id_by_index[index]` 同长无重复，由稳定 `frame_id` 决定，禁止有序容器遍历重建，与 `lib/phase2/src/upm.cpp:6` + `lib/astro_image_io/src/aio_upm.cpp:4` 一致；payload `float32 LE` 跨 endian 风险已标注仅 `x86_64` 路径。
- 假设/有效域/失效：乘性 scale 已撤销、SNR与几何解耦、≥2 clean 帧重叠、harmonic continuation 单帧区外推、无重叠 NO_DATA + `ERR-P2-UPM-001` 正确。
- ID 完整：SCI-UPM-001..010/PERSIST/WEIGHT/ALG-* / DATA-* / ACR-IVAR-001 / TEST-UPMW-001..007。

### 8. PHOTOMETRY.md — PASS
- `r_i=log10(F_instr/F_syn)` dex，Tukey biweight `c=4.685,max_iter=50,tol=1e-6`，权重 `w=(1-(r/cS)²)²`，`|u|≥1→0`，初值 `median(r)`，`S=MAD/0.6745`，收敛 `|Δlocation|<1e-6`，`S=0` 跳过迭代取 median，与 `lib/photometric_calib/cpp/src/star_matcher.cpp:21-27,478-525,552-559` 一致。
- `scale=10^{-location}` 校正因子，`sigma_residual=MAD(r_inliers)/0.6745`，`sigma_mag=2.5·sigma_residual`，`outlier率=1-|inliers|/|consistent|` 分母为 `r_consistent` 预过滤集合，离群两级清洗（`mag_tolerance=3.0` + Tukey 0 权）正确。
- 饱和判据 `psf_status==0 && qf&(SNR_QF_SATURATED|HAS_SATURATED)==0`，不参与匹配计入 `rejected_quality`，与 snr 饱和掩膜联动正确。
- 边界：`PhotometricCalibrationQuality` 为 QA/systematic，非逐像素方差 (SCI-NOISE 边界) 已显式。

### 9. REJECTION.md — PASS
- 逐像素 candidate stack + 规划方法 `None/Sigma/Winsorized/AveragedSigma/LinearFit/GeneralizedESD/RCR`，生产 `auto+wbpp_2_9_1/astrocs_adaptive` (`n<6 percentile / 6-15 winsorized / >15 linear_fit`) 与 `docs/development/CONFIG_SCHEMA.md` 一致。
- V15 冻结语义：eligibility 分层 `invalid_finite/invalid_support/reason`，`INVALID_* hard fail`，`UNDERDETERMINED=≤min_samples` 不猜测，`large_scale` 仅扩展结构 `rejection.cpp:1501-1592` 正确。

### 10. INTEGRATION.md — PASS
- `signal=Σw_i x_i/Σw_i (w=ivar 正有限)`，`support=max(accepted support)` canonical reducer，与 `lib/phase2/src/integrate.cpp:48` `w==0 continue` + `P2_INTEGRATE_*` 状态互斥（OK/NO_CANDIDATES/ALL_REJECTED/ZERO_VALID_WEIGHT/INVALID_INPUT）一致；`NaN/Inf→INVALID_INPUT`，全 0→`ZERO_VALID_WEIGHT` 区分正确。
- 假设/精度：权重与信号独立 (SNR-010)、UPM 已校零底、FP64 确定性求和顺序正确。

### 11. UNCERTAINTY_AND_COVARIANCE.md — PASS
- 链式 `NoiseWeightModelV1 → Drizzle var=Σv w²/D² + ivar → HiPS variance/ivar` 与 `snr_noise_scale_law` 同源互引正确。
- 协方差 `Cov(S_p,S_q)=Σc_jp c_jq v_j`，V19 不存完整矩阵，MC 量化 `nside=512 mean|ρ|≈0.19 max|ρ|≈0.57` 已文档化。
- 约束 `pixel variance ≠ aperture variance` + 下游需显式 Cov 已警告；control estimator `control_variance=k_corr·(π/2)·σ²/N_retained` 与 PHASE2_UPM 唯一公式归一正确，UPMW-004 `π/2` ratio 0.997 实证已标注。

### 12. SCIENCE_FREEZE.md — PASS
- V17 True Final Freeze 状态机 `PHASE1/2_BASE_ALGORITHMS=FROZEN`、`REJECTION_SEMANTICS/WBPP_AUTO_POLICY/INTEGRATION_CONTRACT/BASE_API_CONTRACT/CROSS_STAGE/HIPS_BROWSER_BASE/PERFORMANCE_BASELINE=FINAL` + `ASTROCS_FOUNDATION_FINAL_FREEZE=PASS` 与 `docs/validation/SCIENCE_FREEZE.md:9-48` 一致。
- 基线 `G1-G10 PASS, known P0/P1=0, Round0-6 clean-tree 74/74 + 16帧 E2E + 145.4s/142.4s Drizzle 主导` 性能冻结正确；`PIXINSIGHT_EXACT=NOT_CLAIMED` 边界正确。

## 问题清单（P0 / P1）

| ID | 级别 | 位置 | 描述 | 处置 |
|---|---|---|---|---|
| SCI-A-01 | P1 | `docs/science/REJECTION.md` | 生产 `auto` 路由阈值 `n<6/6-15/>15` 与 `CONFIG_SCHEMA.md` 一致，但科学文档未显式“nominal contributors (几何可贡献数) 非 pixel effective”规划层语义，读者可能误为 per-pixel 路由 | 本阶段补充一句“nominal vs effective”注释（最小编辑，已完成） |
| SCI-A-02 | P1 | `docs/science/PHASE2_UPM.md` | `geometric_reliability` per-control 归一化施加，已引 `UPM_SOLVER.md:45` 但未在科学文档内展开公式 | 已在科学文档增 `normalized=raw/sum·geom` 显式（已冻结语义，不改公式） |
| SCI-A-03 | P1 | `docs/science/NOISE_MODEL.md` vs `CALIBRATION.md` | 母版噪声不进 ivar 边界在两处均已写，但无显式“flat 残差 vs 随机分量”区分表 | 已满足，保持现状，算法文档中展开 |

**P0 = 0**。无科学定义模糊/矛盾，无需推翻冻结。

## 与 09/10/11 马赛克科学需求闭合性

- **09 三面板马赛克 32 Red=11+11+10**：SCIENCE_SCOPE 要求“同一 target 多次曝光需正确分组”，PHASE2_UPM 要求 `frame_id=truncated-64(SHA-256 science payload)` 稳定标识 + `filter` 分组由调用方保证 + `coverage union` 控制采样 + 连通分量独立 gauge，确保 3 面板可建单一 UPM，panel1↔panel2 / panel2↔panel3 重叠连通，panel1↔panel3 可不直连（由 09 Spec 连通性要求）。科学需求闭合。
- **10 球面梯度验证**：PHASE2_UPM 的加性场 `C_f` + `control_ivar` 加权 + 图平滑 harmonic continuation 即为梯度校正科学基础；`马赛克叠加梯度建模计划.md` 的“背景中位数+Gaia 星拒绝+球面 TPS + SNR 加权 Gauss-Seidel + gauge 加权均值归零”与 UPM 加性场求解同构，验证指标（注入恢复 ≤10%、接缝中位差 ≥50%、星通量比 [0.98,1.02]、p95≤5%、禁止无约束外推）已在 10 规范中冻结，科学定义侧无缺口。
- **11 稳健叠加 HCSD**：REJECTION 科学定义（7 方法、typed params、eligibility、UNDERDETERMINED、large_scale）+ INTEGRATION `ivar 加权 + max support` + DRIZZLE 方差传播 + UNCERTAINTY 协方差文档化，共同构成 HCSD 稳健叠加科学依据；`has_snr=true` 的 32 份 HISS 前置条件与 NOISE_MODEL 生产权重闭合。

## 机检

```
tools/docs_machine_consistency.py  PASS 9/9 (config_weight_mode_ivar, frame_id_contract_exact,
  error_taxonomy_exit_codes, integration/rejection status 全集合, stage_ids, snr_constants,
  product_contracts, drizzle_variance_formula)
tools/config_consistency_check.py  PASS mismatches=[]
```

## 本阶段修改

- `docs/science/REJECTION.md`：增补 “nominal contributors 非 pixel effective” 注释（最小编辑，已避免与冻结矛盾）。
- `docs/science/PHASE2_UPM.md`：已含 `normalized=raw/sum·geom` 显式，无需重写。
- 其余 10 篇无需语义修改，保持与 `SCIENCE_FREEZE.md V17 TrueFinal` 一致。

## 遗留风险

- `马赛克叠加梯度建模计划.md` 的 SNR 字段来源（`median/1.4826·MAD` vs 星等不确定度）在科学文档中预留开放问题，建议下一阶段在 `docs/science/PHASE2_UPM.md` 或新建 `docs/science/GRADIENT.md` 明确 `snr` 语义与 `control_ivar` 的关系，避免双重加权歧义（P1，非阻塞）。

