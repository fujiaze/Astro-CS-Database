# Normalize 第二批独立复核与科学裁决（633ffc31）

> 前台独立审查 2026-10-16。对照 4f7ecb6b80ba90341da25ddb8482a50ef206c475 → 633ffc31de173ae2748f8761e18f96954f02c2b4，3 次提交。核对远端 GitHub 实际 C++、验证代码、evidence log 和相关科学正本；**没有重新运行计算节点 /testdata 真实 FITS 或独立编译节点环境**。数值属于 Agent 的可检索运行证据，不冒充前台复测。科学公式权威：docs/science/noise_snr/NOISE_SNR.md §11.4、docs/science/psf/PSF.md §4、docs/engineering/contracts/NORMALIZE_DATA.md §7.1。

## 1. 审核结论

**本轮三个生产改动都应保留**：秩亏 GLS 改为 pivoted Householder QR 行空间投影；PSF 库内实现连续单位总通量的 Gauss–Legendre 像元积分；local-factor/v2 writer/reader 在独立小样例可从 R+v 重建 C。已提交日志中的 1655 s → 138 s 是总墙钟约 12x；精度段 1594 s → 77 s 为约 21x，必须区别表述。

### A. 背景基共线时，源通量仍可辨识：数学修复正确

设有噪声子空间中的源响应 t 与背景响应 Z_w，条件源信息为

\[
I_{F\mid\beta}=\|(I-\Pi_{\mathrm{col}(Z_w)})t\|_2^2.
\]

即使背景 β 本身因列共线而不能唯一估计，只要 I_F>0，F 仍可辨且 V_F=1/I_F。只有 t 落在背景列空间时才应判 unavailable。当前 sparse_reference.cpp 已实现背景 rank 与 flux 方向分离，verify_round2.cpp 的正反例与本公式一致。**不要恢复旧版“全参数矩阵奇异就拒绝 F”的判据。**

需要加强**数值实现**：当前 C++ 以 `I_F=t_norm2-proj` 两个近似相等的量相减，接近背景共线时会发生灾难性消去。按正交基构造残差向量 e=t−Σ q_i(q_iᵀt)，必要时再重正交，直接求 Σe_i²；记录 ‖t‖、‖e‖ 和有效 rank/tolerance。新 QR 的 rank_tol 作用于 Q 的奇异值尺度，旧 Jacobi 秩门限作用于 H=QᵀQ 的特征值尺度，不能因配置数值同为 1e-10 就称统计截断相同。实际 covariance_rank 626→627、V_F 约 0.01% 变化需按统一奇异值基准复核 near-null 灵敏度。

### B. SNR 141→13.6：幅度合乎物理归一，但还不能称观测绝对准确

实跑 sigma=2.211089 pixel 的圆对称 Moffat4，单位总通量归一系数

\[
a_{\rm norm}=3/(2\pi\sigma^2)=0.0976627476,\quad 1/a_{\rm norm}=10.2393187.
\]

旧中心裸值模板比归一像元响应幅度大约 10.2 倍。Agent 记录 V_F: 49.957517737→5439.964080409（倍率 108.8918），sqrt 方差倍率 10.4351，SNR: 141.48147354→13.55819838（倍率同为 10.4351），新旧都严格满足 `SNR·sqrt(V_F)=F_ref=1000 ADU`。这证明主要差异与模型归一修复相符，而非新 solver 自身把统计效率压低十倍。

源码中的 Gauss–Legendre 双轴变换 Jacobian 0.25 正确，测试包含像元面积积分/解析边界/空间平移等变/随机 Monte Carlo。但 verify_round3_psf_pixel_integration.cpp 注释声称的 “201×201 全平面积分趋于 1”大网格单元块实际只创建了点数组，**没有调用函数/断言**，不能把它计为已经实测的独立 full-plane 求积项；order=10 vs 11 的差只能叫自检估计，不是任意 PSF 参数下的严格真实误差上界。

**该 13.5582 是给定 explicit_linear_flux=1000 ADU、当前使用的 circularized frame-median σ、指定 conditional noise 和背景模型下的 reference SNR，不是自动的 Gaia XP 共通绝对 5σ 深度。** 目前 normalize_precision.cpp 的 PSF sigma 从每帧 p1_sources.json 中 sx/sy 的各自中位数再几何平均得到；尚未使用真实逐位置椭圆/旋转 PSF。sed_response 在 parse_reference_config 中明确 unavailable，master uncertainty 仍有 omitted 项。只凭像元积分库单点数学准确不能声称整个真实仪器 PSF/SED/noise 标度精确。

**同意在改变 P4/P5 物理统计语义、将 canonical SNR 作为跨设备公开精度之前，先进行一轮有边界的独立科学复核。** 目标是 P1 模型，不要求再跑三阶段全量任务：
1. 不同 σx、σy、θ、亚像元相位和边界/欠采样的独立真实像元足迹求积与模板归一；实际空间 PSF 模型提供对应局部参数及拟合留出星误差；
2. 指定真实 F_ref、增益/读噪/背景与 photo scale 的独立 photon+RN 前向小样本，比较已知点源 flux 的重复估计散度、偏差及区间覆盖，和 C=R diag(v)Rᵀ 的 GLS 条件 V_F 对拍；
3. 少量真实 WCS 已解帧，独立留出星的 PSF/aperture residual 和背景失配，明确 per-frame response/光度标度/caliber/omitted_terms；
4. 真正从 sidecar 按每控制点的 PSF+R+v+Z 复算 V_F 与 SNR，不能只对照源程序自行写出的帧级汇总。

若缺增益、SED 或空间 PSF 的真实来源，继续给出 qualified approximate/conditional 控制点，允许成像与研发，但不能无条件当作跨设备标准精度。P4 typed Runtime 的**纯结构性迁移可以并行开展**；P5 SED/空间 PSF/α 的最终物理判定以及向下游承诺参考控制精度以本轮独立验证为先。

## 2. 额外源码缺陷 / 元数据语义

| 项 | 现行位置与实际问题 | 正向整改定义 |
|---|---|---|
| R2-F01 | normalize_precision.cpp::stage_frame_precision 使用 `profile_enclosed=min(所有窗口 ΣP)`，随后给每个 control support_note 写同一个最小值 | 每个 control 保存自己的 f_enc/f_out/PSF 参数与积分误差，帧级 extrema 单列为摘要，不冒充逐点实测 |
| R2-F02 | normalize_precision.cpp covariance payload 的 support_note 仍写 `local-factor/v1`，而 normalize_science.cpp 发布正式 representation 为 `local-factor/v2`、把旧文本发布到 representation_note | 正式 representation、sidecar metadata 和说明字段同一版本；v1 只用于历史兼容 |
| R2-F03 | normalize_science.cpp::reconstruct_factor_covariance 目前只检查 sr 指数为 0/-1/-2，没有比较被解析的线性单位部分，因而 `adu^2` 因子和 `electron^2/sr^2` 派生单位可能同时通过 | 校验同一 linear_unit，明确实际 photoapplied signal 的参考线性单位；增加 sr 指数正确但线性量纲错配的负例 |
| R2-F04 | QR 最大/最小 pivot norm 比的平方写作 solver_condition_number；真实例子约 5.31e10 | 标明 `condition_estimate_method=pivot_norm_ratio_squared`，不当作 κ₂(C) 独立准确值；检查 near-null 方向/秩门限敏感度 |
| R2-F05 | parse_reference_config 仅完整实现 explicit_linear_flux，sed_response 尚不可求积分 | 声明本次 F_ref=1000 ADU 属于显式数值参考通量，不能称 Gaia XP 已跨设备对齐的共同 SED/通带 |

## 3. 执行结论

本轮代码中的 QR/PSF/factor-v2 可以保留并继续复用。**确认**背景共线只损失背景参数独立可辨识性，不自动损失通量可辨识性。**建议**在完成 P4/P5 的物理模型扩展前完成针对性参考 SNR 独立复核；先修 R2-F01/02/03 这三项确定的语义/单位错误，R2-F04 的数值诊断需正确定义。不要借此次审查改 Mosaic 默认权重/背景/Export，也不需另立科学模型。工程事实报告须记录真正复跑的输入、SHA、结果与未运行域。
