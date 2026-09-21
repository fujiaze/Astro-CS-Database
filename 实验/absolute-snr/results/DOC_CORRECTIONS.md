# SCI-B 对研究包/先行实验的补充与订正条目（DOC_CORRECTIONS）

> **归属声明**：本文件属 SCI-B 实验单元。`docs/research/SNR_WEIGHT_RESEARCH_PACK.md` 由 DOC-404 负责修订，本单元**只读**，故所有补充/订正写在本文件，供 DOC-404 与研究包维护者采纳。
> 每条含：现象 → 证据（文件:行 / 可复现命令 / 实测数字）→ 建议处理（FIX/DOC 域）→ 本单元是否已闭环。
> 复现：`bash 实验/absolute-snr/code/run_all.sh`；原始数据 `results/b1..b6*.json`。

---

## D1【重要·FIX 域】生产路径"经验 σ_sky（含读噪）+ (RN/g)²"双计读出噪声

**现象**：σ_F 被系统性高估（SNR 被低估），只在读噪相对天光散粒不可忽略时出现，且随天光升高而消失。

**证据链**
- 设计意图：`lib/algorithms/noise_snr/cpp/include/.../snr_science.h` 与 `docs/plugins/algorithms_phase1/07_noise_snr.md` 规定 `σ_i² = σ_sky,散粒² + (RN/g)² + F·P_i/g`（读噪只出现一次）。
- 调用点：`lib/infrastructure/scheduler/src/module_adapters.cpp:4258` `cfg.sigma_sky_adu = src_frame->value("noise_sigma", 0.0)` —— `noise_sigma` 由 `noise_model` 的经验空天稳健尺度给出（`noise_model.cpp` §5，**含读出噪声的总 rms**）。
- 同时：`module_adapters.cpp:3944-3945` 把 `snr.gain_e_per_adu` / `snr.read_noise_e` 配置进 `cfg`，`snr_science.cpp` 的 gain>0 分支据此再加一次 `(RN/g)²`。
- 实测（`code/b2_noise_terms.py`，N_MC=1000，闭式预言 vs 蒙特卡洛臂）：
  - 基准点（F=1000 e⁻、B=100 e⁻/px、RN=10 e⁻、g=1.3、D=0.5）：σ_F 高估 **+12.8%**；
  - 最坏点（RN=50 e⁻）：**+34.0%**；
  - 天光主导点（B≥10⁵）：偏差 <1%，与"天光散粒 σ + RN 项"路径在 3σ 内一致；
  - 闭式预言 `σ_F(emp+RN)/σ_F(correct)−1` 与实测臂比值最大差 **1.25 pp**（预言本身正确）。
- 对照（正确口径，均 ≤3σ）：PSF 行路径 `snr_estimator.cpp:83`（`sigma_sky_adu = residual_scale/0.7316727929211932`，gain 未知 ⇒ 不加 RN 项）与"天光散粒 σ + RN 项"臂。
- 帧级 SNR 实证（`results/b1_sky_scan.json`）：B=0 时"经验+RN"臂 SNR=38.47 vs 定义式/真值 43.32/43.21（−11.2%）。

**建议处理（FIX 域，本单元不改生产码）**：在 `07_noise_snr.md` 明确 `sigma_sky_adu` 的口径（**天光+暗流散粒，不含读噪**），并在调用点做**显式契约检查**：当 `gain_e_per_adu>0 && read_noise_e>0` 而 `sigma_sky_adu` 来源为经验总 rms 时，要么扣除 RN 项，要么 fail-closed 拒绝（禁止静默双计）。补一条负例锁定：RN 主导点 σ_F 偏差 ≤5%。

**闭环状态（RELEASE-05 / SCI-501 已闭环）**：生产码已按 DOC-502 冻结口径修复 ——
`SnrSourceParams.sigma_sky_source` 显式声明语义（`SHOT_ONLY` / `EMPIRICAL_TOTAL_RMS`），
生产调用点 `module_adapters.cpp` 对 `noise_sigma`（经验总 rms）声明 `EMPIRICAL_TOTAL_RMS`，
`snr_science.cpp` 据此**不再**叠加 `(RN/g)²`；`SnrSourceResult.sigma_sky_source_effective`
落 provenance（1=已加 / 2=未加 / 3=gain≤0）。保护测试：`p1snr_science_skysource`
（MC 3σ 双向：正确口径 zA=1.27 绿、双计臂 zB=25.6 红；legacy 缺省与双计臂逐位一致）。

---

## D2【重要·DOC/实验域】EXP-205 的误差预算常数 `c_est=1.44` 单位混用（相对 vs dex）

**现象**：`run/RELEASE-02/实验/E11-SNR三口径精度/code/snr3_common.py:741` `eps_est(n)=c_est/√n`（`c_est=1.44`）被直接当作 **dex** 量使用（进入 `τ_A=K·√(eps_repr²+eps_est²)` 与 `τ_B=2·eps_est`），而 `docs/science/NOISE_MODEL.md:86` 的原文是「SE(σ̂)/σ ≈ 1.44/√N_sky」——这是**相对**标准误（无量纲），不是 dex。

**证据（`code/b6_gates_audit.py::c_est_unit_audit`，N=1024，4000 次 MC）**
| 量 | 值 |
|---|---|
| 1.4826×MAD 的**相对** SE（实测） | 0.03623 |
| 1.166/√N（Rousseeuw-Croux 渐近式） | 0.03644 |
| 文档常数 1.44/√N（相对口径） | 0.04500（比实测保守 24%） |
| **dex** 口径的实测 SE | 0.01576 |
| 正确换算 1.44/ln10/√N | 0.01954 |
| EXP-205 实际使用值 | 0.04500 |
| **高估因子** | **2.3026 = ln10** |

**后果**：EXP-205 的精度门 τ_A 与偏差门 τ_B 的噪声项被放宽 2.3 倍，直接解释其自报的"P2 偏差门灵敏度不足"；其"帧级精度门恒绿"结论不受影响（该门另有恒真性，见 D3）。

**建议处理（DOC/实验域）**：① `NOISE_MODEL.md:86` 补一句单位声明（"相对"），或直接给 dex 口径 `1.44/ln10/√N ≈ 0.625/√N`；② EXP-205 的 τ_A/τ_B 若要复用，需按 dex 口径重算后再判断门限。

**闭环状态**：已量化 + 登记；EXP-205 属 `run/` 临时产物（不入库），其结论已在 SCI-B 内以正确口径重做。

---

## D3【重要·方法域】"帧级臂 RMSE 恒等于 s_field"类判据是恒真门，无证据资格

**现象**：判据 `RMSE(帧级常数场, 中位归一真值场) ≤ K·s_field`（EXP-205 的 SP-0 风格，K=1.25）对**任意**真值场恒真。

**证明与演示（`code/b6_gates_audit.py::tautology_demo`，`results/b6_gates_audit.json`）**
- 数学：常数场 `c=median(truth)` ⇒ `RMSE = RMS(log10 truth − median log10 truth) ≡ s_field`（中位归一离散度），与 K≥1 无关。
- 数值：平滑 GRF、**对抗打乱场**、平坦场三种真值全部绿；对抗打乱场下帧级口径的**权重效率损失 E=7.17**（科学上完全不可用）而门仍绿。

**替代判据（本单元采用）**：权重效率损失 `E = Var_w/Var_opt − 1`（全局尺度相消；E=0 ⇔ σ̂∝σ_true）。双向可假已实测：无偏 E=0 绿、纯尺度 E=0、+10% 电平偏差 E=0.0274 红、打乱 E=0.877 红。
**另注**：平坦 σ 场（真值无空间效应）时，任何"空间口径 RMSE < 帧级 RMSE"的排序判据都退化（帧级 RMSE≡0），EXP-205 的 P1"sparse 在 Δ≥96 反超帧级"里至少有一部分是**重建算子噪声**而非信息（本单元 `results/b3_domain_map.json` 的 `N1_flat_field_sparse_never_wins` 用例）。

**建议处理（DOC 域）**：研究包与 `ACCEPTANCE_SPEC` 相关判据表应把"帧级精度门"标注为**恒真、不作证据**，并替换为效率损失类判据。

**闭环状态**：已在 SCI-B 内替换并给出正/负例。

---

## D4【次要·DOC 域】三口径失效边界不是单一 `Δ/ℓ≈1`，而是三因子联合判据

**现象**：EXP-205 报告 face A 的边界 `Δ/ℓ≈0.98`；本单元在**无源污染**的 SE-GRF 合成面上（ℓ∈{16,32,64,128,256}×s∈{0,0.03,0.10,0.30}，20 面）在 s≥0.03 时把 Δ 加到 512 px 仍未见 sparse 反超帧级；而在 HST M16 真实结构面上 Δ=32 px 就反超。

**证据**（修正后完整数据，0 条臂缺失）：`results/b3_domain_map.json`（`gates.delta64_detail`、`faces.synthetic_grf`、`faces.hst_m16`）。机制差异来自 **cell 稳健 MAD 被 cell 内未分辨结构抬偏**（HST 面偏差 +0.029→+0.301 dex 随 Δ 增长），而不是插值误差或 Δ/ℓ 本身。**量化**：无源污染面上 sparse 在 Δ=512 仍胜，但余量 ℓ=16 → 0.6%、ℓ=32 → 1.8%、ℓ=64 → 2.8%、ℓ=128 → 16%、ℓ=256 → 35%；HST 面 Δ*=16 px（Δ/ℓ=0.50）。
**建议**：研究包把失效边界表述为「Δ/ℓ × σ 场幅度 × 未分辨结构污染」的联合判据，并给 HST 类数据的**帧级标量兜底**。

---

## D5【次要·DOC 域】研究包中 SWarp `src/coadd.c` 的逐行引用未能在本单元核验

**现象**：`docs/research/SNR_WEIGHT_RESEARCH_PACK.md` 引用 SWarp `src/coadd.c` 的逆方差组合公式（`out=Σ(x_k/var_k)/Σ(1/var_k)`）。
**本单元核验**：`https://raw.githubusercontent.com/astromatic/swarp/master/src/coadd.c` 可解析（HTTP 200，71,800 B），但未在该文件中定位到所述行（该镜像可能滞后/结构不同）。
**已逐行核验的替代开源证据**：Siril 1.2.4 `src/stacking/median_and_mean.c:868-870`（`pweights[layer][i]=1.f/(pscale²·bgnoise²)`）、photutils `photutils/utils/errors.py:12`（`calc_total_error(data, bkg_error, effective_gain)`）。
**建议**：DOC-404 复核该引用的版本/行号，或改用官方发布 tarball 的行号。

---

## D6【确认·无需修改】`SNR_combined²=ΣSNR_k²`、Q/W 信息量、F_ref 锚定换算的严格性

- `SNR_combined²=ΣSNR_k²`：相对偏差 2.2e-16（`results/b4_integration.json`）；
- `w_k=SNR_k(F_ref)²/F_ref,k² ≡ 1/σ_F,k²`：1.1e-16；
- Q/W：`Var=1/ΣW` 实测 1275 vs 解析 1260（+1.2%）。
即：研究包"权重在 Phase2 由 SNR 现场换算"的路线在本单元被独立证实，且入库量必须是**未加权原始 SNR**（PSFSNR 的功率比口径不可入库，PSFSW 是权重也不可入库）。

---

## D7【确认·无需修改】PSF 行路径 σ_sky 语义与噪声模型稳健尺度常数

- `snr_estimator.cpp:83` `sigma_sky_adu = residual_scale/0.7316727929211932` 与 `noise_model.cpp` 的 1.4826×MAD 稳健尺度在"天光散粒"语义下与 MC 真值一致（B1/B2 全部 ≤3σ）⇒ 该路径**不**受 D1 影响。
- `NOISE_MODEL.md:86` 的 1.44/√N 作为**相对**预算常数偏保守 24%（实测 1.166/√N），作为 a priori 预算可接受；仅当被当 dex 用时出错（见 D2）。
