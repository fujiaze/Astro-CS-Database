# DERIVATIONS_P2 — P2 跨帧绝对 SNR · 支撑推导（推导腿汇总）

本文汇集论文与实验报告引用的解析推导，全部可在 float64 下机械复核（对应脚本见括号内 [实验] 标注）。

## D1. 双计偏差闭式（P-CST-11）

最优提取方差 σ_F² = 1/Σ(P_i²/σ_i²)。设逐像素方差被错误声明为 σ_i,emp² = σ_shot² + σ_RN²（经验总 rms 含读噪）且增益分支再加 (RN/g)²，而正确声明为 σ_i,corr² = σ_shot²（读噪只计一次），则

    bias(RN) = σ_F,corr/σ_F,emp − 1 = √( ΣP²/σ_corr² · σ_emp²/ΣP² ) − 1 = √( v_emp/v_corr ) − 1

其中 v = ΣP_i²σ_i²（口径自洽：分母分母同窗）。对 Moffat β=4、σ_PSF=1.5 px、窗 half=30：ΣP² = 0.092684087（half=12 给 0.09268653，half=60 给 0.09268408，收敛）。代入登记锚点参数即得 +14.5009%（RN=10 e⁻）与 +38.2524%（RN=50 e⁻）；偏差对增益 g 精确不变（RN 与 (RN/g)² 以 g² 抵消）——这同时锁定量纲约定 [实验:code/audit/route3/exp05_doublecount_corr.py]（闭式复算 max rel 2.35×10⁻¹⁴）、[实验:code/audit/route1/exp04_double_count_bias.py]（MC 复现 ≤3.3×10⁻⁷、增益不变性）。

## D2. 协方差对角近似欠估闭式（P-CST-22）

设像素间等相关 ρ、有效相关邻域 M_eff 个像素，对角近似只计对角元，则

    Var_exact/Var_diag = 1 + ρ(M_eff − 1)

均匀 4-tap、ρ=0.19：欠估 = 3ρ/(1+3ρ) = 0.57/1.57 = 36.31%（登记 36.3%）。23.3% 对应同曲线另一点（ρ=0.101 @4-tap，或 M_eff=2.6 @ρ=0.19）；旧启发式 1+0.75ρ̄ = 1.125 与两者皆非，淘汰 [实验:code/audit/route1/exp10_covariance_diagonal_approx.py]、[实验:code/audit/route2/exp07_correlation_diagonal.py]。

## D3. γ=2 唯一性恒等式（P-CST-21）

帧 k 的换算权 w_k = SNR_k^γ/F_ref^γ。由 SNR_k = F_ref/σ_F,k，有 w_k = F_ref^(2−γ)/σ_F,k^γ · F_ref^(γ−2) = σ_F,k^(−γ)·F_ref^(2−γ)·SNR_k^(γ−2)·…（直接展开）：w_k = SNR_k^γ/F_ref^γ = (F_ref/σ_k)^γ/F_ref^γ = σ_k^(−γ)·F_ref^(γ−2)。要求 w_k ≡ 1/σ_k²（逆方差，共同尺度、与 F_ref 无关）⇒ γ=2（指数唯一匹配）且 F_ref 因子消失。γ=1 时 w_k = F_ref^(−1)·σ_k^(−1)，帧权畸变 = |g_k|⁻¹（实测 ×0.32–×3.16）。数值：γ=2 恒等偏差 4.4×10⁻¹⁶ = 2 ulp [实验:code/audit/route1/exp12_gamma_weight_scale.py]；γ≠2 ⇒ O(1) [实验:code/audit/route3/exp03_median_se_identity.py]。

## D4. 天空预算链（P-CST-08）

单 patch MAD→σ̂ 相对标准误 ≈ c(n)/√N_sky。要求相对 SE ≤ 1.5%：N_sky ≥ (c/0.015)²。登记取 c=1.44（=1.152×1.25，有限 n 修正 × 中位数 SE 系数，0.015 为预算相对容差）⇒ N_sky = 9216 = (1.44/0.015)²（解析恒等）。理论链 1.152×1.2533 = 1.444；直接管线实测 c≈1.449 ⇒ N_min ≈ 9321，与 9216 差 1.2%（阈值自洽）[实验:code/audit/route3/exp01_mad_sigma_budget.py]。c(n=64)=1.152 由 D-04 终裁（MC 1.1508/1.1542）；1.44 支的替代读数 9216→5816.6（= (1.152×1.25/0.015)² 精确复算），5811 属算术漂移。

## D5. control_variance 有限 N 修正（补实验）

奇数 N=2k+1 的样本中位数为第 k+1 阶序统计量，密度 f(x) = n!/(k!k!)·Φ(x)^k(1−Φ(x))^k·φ(x)；Gauss–Legendre 600 节点数值积分给出精确 κ_exact(N) = N·Var(median)/σ²，渐近极限 π/2。纯公式口径偏差 = (π/2)/κ_exact − 1：N=5 时 +9.53%（高估，保守），1% 边界 N≈45–49。端到端口径分解恒等式：端到端偏差 = (π/2)·c_mad2/κ − 1，其中 c_mad2 = E[σ̂²]/σ² 为 MAD plug-in 平方偏置（N=5 时 0.906），与有限 N 中位数高估同量级反号 ⇒ 净偏差 N=5 为 −0.6%、奇 N 全域 ≤1.5%。偶 N 中位数取两中央均值，κ(20)=1.470 < κ(21)=1.538 ⇒ 端到端高估 +5.0%（N=20）[实验:code/audit/supplement_control_variance/finiteN_control_variance.py]。生产链忠实臂（裁剪后中位数口径）低估 1.3–3.2% [实验:code/audit/supplement_control_variance/production_chain_control_variance.py]。D-07 终裁以此三口径定稿。

## D6. m_ref 依赖与权重比值不变性（P-CST-19）

F_ref,k = 10^(−0.4(m_ref−ZP_k))，故 frame_snr = F_ref/σ_F 每 mag 单调降 10^0.4 = 2.5118864… 倍（16 位一致）[实验:code/audit/route3/exp04_refmag_chain.py]。而换算权 w = SNR²/F_ref² = 1/σ_F² 不含 F_ref ⇒ 逐帧权比对 m_ref/ZP 完全不变（实测 rel ≤3.3×10⁻¹⁶，同帧配对更细）。由此：跨帧叠加与重建不依赖 m_ref 档位，但 frame_snr 的绝对数值比较必须同档；m_ref=6.0 为单位制锚点（冻结纪律）。

## D7. 5σ 裁剪误剔率与掩膜 r_local（P-CST-09/15）

高斯尾：2Φ(−5) = 5.733×10⁻⁷/px（误剔率上界；实测帧内裁剪率 0.25% 量级、纯噪声臂 ≈0）[实验:code/audit/route3/exp01_mad_sigma_budget.py]。掩膜亮源局部半径闭式：残余面亮度阈值 k·σ_bg（k=0.1，项目约定）下 r_local(F) = (F/(2π·0.1·σ_bg·q))^(1/2)（q 为峰-等效因子），F=10⁵ ⇒ 17.60 px [推导:route3/exp06 推导腿]。

## D8. 逆方差加权口径（引 Aitken 1935）

线性模型 x = P·F + ε，Cov(ε)=diag(σ_i²) 下的广义最小二乘解 F̂ = Σ(P_i x_i/σ_i²)/Σ(P_i²/σ_i²)，Var(F̂) = 1/Σ(P_i²/σ_i²)——逆方差加权（Aitken 1935；标注级引用，负责人已批）。叠加权重 w = SNR²/F_ref² = 1/σ_F² 与之恒等（D3）；P5 组合 SNR_comb² = Σ SNR_k² 为其在等结构假设下的直接推论（本单元 b4 实测 2.2×10⁻¹⁶）。