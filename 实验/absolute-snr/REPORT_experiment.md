# P2 跨帧绝对 SNR · 实验报告（REPORT_experiment）

**单元**：实验/absolute-snr（SCI-B，科学链创新点 P2）
**事实源**：独立审计/实验重做/总编对账/分歧台账.md、五单元成稿简报.md。整理前的历史正本不再随单元保存；其仍成立部分已吸收进本报告与 `REPORT_paper.md`，失效部分按台账订正并逐条注明（明细见 `docs/LEDGER_CORRECTIONS_P2.md`）。
**正式论文**：REPORT_paper.md（本报告为其实验证据底稿）。

---

## 【链条位置】

- **上游接口（P1 → P2）**：输入逐帧测光零点 ZP_k（mag）与逐星产品（F、FWHM）。P2 生成 F_ref,k = 10^(−0.4(m_ref−ZP_k)) [ADU]；恒等式 F_ref,k·k_photo,k = F0 锁定与 P1 的自洽 [实验:code/audit/route1/exp04_double_count_bias.py]。P1 低星数帧零点不确定度被低估（MAD 有限样本偏差 1.49×，n=3）会传入 P2，按 P1 单元精度约定处理。
- **本环产出**：① frame_snr（无量纲，依赖 m_ref，须同档比较）；② 逐源 σ_F/SNR_F（ADU 域）；③ 稀疏控制点 sparse_snr_value = F_ref/σ_F（无量纲，Δ=64 px 网格；**边界（P2-B2）**：口径与 schema 已冻结、Phase2 消费面已接线并红绿验证，但 Phase1 侧尚无产者与 HiPS 载体 ⇒ 生产链当前不产出该层，见 REPORT_paper §6）；④ 叠加权唯一换算口径 w = SNR²/F_ref² = 1/σ_F² [ADU⁻²]。
- **下游消费**：P3 上球（消费控制点＋预测方差；k_corr 查表由 P3 承载，D-08）；P4 稠密重建（插值配置化＋运行日志输出不落盘，已批，承载于 P4）；P5 加性天光去除（w 组合，SNR_comb² = ΣSNR_k²）。
- **精度约定**：m_ref 随产品落盘；换算权逐帧比对对 m_ref/ZP 的**代数**不变性 ≤3.3×10⁻¹⁶，但生产组成下只是近似——m_ref 档实测漂移 1.9%–4.2%（f4 生产驱动最大 4.20%、exp04 生产臂 3.83%、exp05 1.95%；零点灵敏度 1.85% 是另一量），故必须同档比较（P2-M2）[实验:code/audit/route3/exp04_refmag_chain.py]；量纲错位 ⇒ SNR 偏一个 gain 因子（增益不变性检验锁定约定）[实验:code/audit/route1/exp04_double_count_bias.py]；控制点精度约定 1.5%（N_sky=9216 口径）经接口核对传递（首轮 **IDW 代理算子**：RMS 0.229、通胀 1.06×，属代理自身性质；**冻结默认算子** natural_bicubic_spline_clip_v1 在生产 SparseSnrReconstructor 直调下实测 T ≈ 0.87 衰减、同几何 disc 0.1206，见 REPORT_paper §4.9，P2-M5）[实验:code/audit/route3/exp04_refmag_chain.py]。**数值精度**：全部复算 FP64；1e-9 级判据仅 FP64 有定义（FP32 负控 3.4×10⁻⁸，P2-m8）。

## 1 假说与判定

| ID | 假说 | 判定 | 证据 |
|---|---|---|---|
| H1 | 天光只经散粒噪声进 σ_F：固定源通量抬升天光 ⇒ 帧级 SNR 单调下降、天光主导段斜率 −1/2 | 成立（斜率 −0.4879/−0.4972；SNR(10⁶)/SNR(0)=2.1%/1.1%） | [实验:code/b1_sky_scan.py] |
| H2 | σ_F⁻²=ΣP_i²/σ_i² 与 MC 经验散度一致 | 成立（26+29 点 max|z|=2.68 ≤3σ） | [实验:code/b1_sky_scan.py][实验:code/b2_noise_terms.py] |
| H3 | 生产口径与 Horne 口径一致 | 有条件成立；"经验总 σ + RN 项"臂双计读噪（发现→修复 FIX-407 闭环） | [实验:code/b2_noise_terms.py] |
| H4/H5 | 负例：算术常数无散粒 ⇒ 度量恒零；传统"信号含天光"口径失真判红 | 成立（6.7×10⁻¹⁶；×3419/×1.03e5 判红） | [实验:code/b1_sky_scan.py] |
| H6 | 局部背景估计偏差 δB 有可量化 SNR 边界 | 成立（1% 边界 δB*=2.78 e⁻ 解析，实测交叉 3.44 e⁻） | [实验:code/b1_sky_scan.py] |
| H8 | 三口径无全局最优、适用域可判 | 成立（地面稀疏全胜、HST 帧级胜、Δ*≈16 px） | [实验:code/b3_domain_map.py] |
| H9/H10 | SNR_comb²=ΣSNR_k² 恒等；拟合/堆叠权重分离 | 成立（2.2×10⁻¹⁶；杠杆方差比 1.523 vs 预言 1.504） | [实验:code/b4_integration.py] |
| H11 | w=SNR(F_ref)²/F_ref² ≡ 1/σ_F²；γ=2 唯一 | 成立（1.1×10⁻¹⁶；γ=2 恒等偏差 4.4×10⁻¹⁶=2 ulp，γ=1 ⇒ 帧权 ×0.32–×3.16） | [实验:code/b4_integration.py][实验:code/audit/route1/exp12_gamma_weight_scale.py] |
| H12 | 方差按 C_out=R C_in Rᵀ 传播；对角近似欠估 | 成立（对角元比 0.9980；对角近似宣称方差仅为实际的 32.5%；欠估闭式 1+ρ(M_eff−1)，36.31%@ρ=0.19、4-tap） | [实验:code/b5_phase3_transfer.py][实验:code/audit/route1/exp10_covariance_diagonal_approx.py] |
| H13 | "RMSE≡s_field"类门是恒真门、不作证据 | 成立（对抗场下门仍绿 ⇒ 移除；替代判据 E 双向可假） | [实验:code/b6_gates_audit.py] |
| A1 | κ_MAD、FWHM/σ（Gauss/Moffat4）、截尾均值、中位数 SE 四常数为解析闭式 | 成立（1.5×10⁻¹⁶～10⁻¹³ 级一致；Moffat4 冻结值截断 rel +1.908×10⁻⁶） | [实验:code/audit/route1/exp01_robust_statistics_constants.py][实验:code/audit/route2/exp03_closed_form_constants.py][实验:code/audit/route3/exp02_profile_constants.py] |
| A2 | c(n=64)=1.152（vs 1.144） | **终裁 1.152（D-04）**：MC 1.1508（+0.105%；另一路 MC 1.1542 对应 +0.19%）；1.144 支算术漂移 5811→5816.6 | [实验:code/audit/route1/exp01_robust_statistics_constants.py][实验:code/audit/route1/exp03_sky_budget_constant.py] |
| A3 | 9216=(1.44/0.015)² 与直接管线测量自洽 | 成立（c≈1.449 ⇒ N_min≈9321，差 1.2%） | [实验:code/audit/route3/exp01_mad_sigma_budget.py] |
| A4 | 双计偏差登记值 +14.5009%/+38.2524% 存在闭式且逐位复现（05"seed 无关闭式"为错误标签，订正） | 成立（闭式复算 2.35×10⁻¹⁴；Moffat4 MC 复现 3.3×10⁻⁷；增益不变性精确） | [实验:code/audit/route3/exp05_doublecount_corr.py][实验:code/audit/route1/exp04_double_count_bias.py] |
| A5 | 对角欠估 36.3% 有闭式；23.3% 与之同族 | 成立（1+ρ(M_eff−1)；23.3%=ρ0.101/4-tap 或 M_eff2.6/ρ0.19，A-P2-08） | [实验:code/audit/route1/exp10_covariance_diagonal_approx.py][实验:code/audit/route2/exp07_correlation_diagonal.py] |
| A6 | control_variance N=5 渐近式方向与幅值（D-07 终裁） | 纯公式口径**高估 9.53%**（保守）；端到端 ±1.5%；生产链裁剪臂低估 1.3–3.2%；偶 N 高估 +5.0%@N=20 | [实验:code/audit/supplement_control_variance/finiteN_control_variance.py][实验:code/audit/supplement_control_variance/production_chain_control_variance.py] |
| A7 | k_corr=1.4 为声明量、标定 1.3883 系几何专属单次 MC（D-08） | 成立（AR(1) 膨胀律 (1+ρ)/(1−ρ) 机制验证；改两因子查表，P3 承载） | [实验:code/audit/route1/exp14_kcorr_inflation.py] |
| A8 | 《已确立》§3 常数表实存（A-P2-01）；孔径组 diagnostic-only（P-CST-25 豁免） | 成立（行漂移 +2/+3 内容均在；生产链零消费者） | grep 取证（route1 §P-CST-25） |
| A9 | 截断半窗生产域偏置 ≤6.5×10⁻⁵；自洽口径截断偏低（05"恒偏绿"符号笔误，A-P2-09） | 成立 | [实验:code/audit/route3/exp06_mask_window_cond.py][实验:code/audit/route2/exp10_truncation_window.py] |

## 2 方法

1. **物理前向仿真腿**（seed 20260921）：电子域 Moffat β=4 + 天光/暗流 Poisson + 高斯读噪 + 增益/量化；真值 = 逐帧最优提取通量经验散度；每点 N_MC=1000 帧（code/sci_b_common.py 公共库）。
2. **审计重做腿**（seed 20260926，三路互不通信）：35 个纯 Python+numpy 脚本逐项闭合 P-CST-01…25；每脚本内置"真值无效应 ⇒ 归零/判红"负例；单脚本秒级。
3. **补实验腿**（seed 20260601/05）：control_variance 有限 N 的解析（Gauss–Legendre 600 节点）+MC 双路、三口径（纯公式/端到端/生产链裁剪臂）、独立 seed 抽检。
4. **真实数据腿**：testdata M42 三帧（地面）+ HST M16（高对比），1 px 棋盘 hold-out。
5. **判定纪律**：负例均非退化；恒真门（RMSE≡s_field、平坦场排序）移出证据；每个数字标注三腿来源（文献 / 实验脚本 / 推导）。

## 3 数据

- 单元主实验 results：results/b1_sky_scan.json、b2_noise_terms.json、b3_domain_map.json、b4_integration.json、b5_phase3_transfer.json、b6_gates_audit.json、exp0*.json 系列。
- 审计重做快照：code/audit/results/route1|route2|route3|supplement_control_variance/*.json（35 份，逐字快照）。
- 关键读数汇总（注明来源路线）：results/AUDIT_KEY_RESULTS.json。
- 三类数据覆盖：物理前向仿真（b1/b2）、纯解析合成（audit 全部 + exp0*）、testdata 真实（b3/b4/exp0*_e3）。

## 4 结果（要点）

数字全部带腿标注；完整论证见 REPORT_paper.md §4；来源路线见 results/AUDIT_KEY_RESULTS.json。

1. **常数体系**：κ_MAD=1.482602218505602、Gauss FWHM/σ=2.3548200450309493、Moffat4 闭式 1.2303076525901024（冻结值截断 +1.908×10⁻⁶）、截尾均值 0.7316730952806134、√(π/2)=1.2533141373155001 —— 解析恒等式级闭合 [实验:code/audit/*][推导]。
2. **预算链**：c(n=64)=1.152（D-04 终裁）；c_eff=1.4751±0.023（管线级，与理论 1.152×1.2533=1.444 差 +2.4%≈1.1σ）；N_min≈9321 vs 9216（1.2%）；5811→5816.6 订正 [实验:code/audit/route1/exp03_sky_budget_constant.py][实验:code/audit/route3/exp01_mad_sigma_budget.py]。
3. **双计偏差**：闭式逐位复现（2.35×10⁻¹⁴）；登记 +14.5009%/+38.2524%；本单元对 MC 真值偏置 +12.8%~+36.6%（±3 pp MC 噪声，闭式预言差 ≤1.25 pp）；修复闭环 [实验:code/audit/route3/exp05_doublecount_corr.py][实验:code/b2_noise_terms.py]。
4. **对角欠估**：1+ρ(M_eff−1) 闭式；36.31%@ρ=0.19、4-tap；23.3% 同族；1+0.75ρ̄ 淘汰 [实验:code/audit/route1/exp10_covariance_diagonal_approx.py]。
5. **权重唯一性**：γ=2 恒等偏差 4.4×10⁻¹⁶=2 ulp；γ≠2 ⇒ O(1) 畸变；恒等门改 4 ulp 规则（A-P2-10）[实验:code/audit/route1/exp12_gamma_weight_scale.py][实验:code/audit/route1/exp07_weight_and_coadd_identities.py]。
6. **控制点方差**：N=5 纯公式高估 9.53%（方向词订正，D-07）；端到端 ±1.5%；生产链裁剪臂低估 1.3–3.2%；偶 N 效应 +5.0%@N=20 [实验:code/audit/supplement_control_variance/*]。
7. **接口传递**：1.5% 控制点精度穿过 P4（首轮 IDW 代理算子通胀 1.06×；冻结默认算子在真实控制网格上 T ≈ 0.87 衰减、disc 0.1206，见 REPORT_paper §4.9）；SNR_comb²=ΣSNR_k²（2.2×10⁻¹⁶）；对角近似宣称方差仅为实际 32.5% [实验:code/audit/route3/exp04_refmag_chain.py][实验:code/b4_integration.py][实验:code/b5_phase3_transfer.py]。
8. **适用域**：地面稀疏胜帧级（0.0413–0.0825 vs 0.0506–0.1691 dex）、HST 帧级胜（Δ*≈16 px）、稠密 64 MiB/帧超预算 64 倍（诊断地位）[实验:code/b3_domain_map.py]。
9. **逐像素绝对 SNR 重建四臂**（解析臂 `D_core`，n=1463，两 seed 同向；seed 20260921 与换 seed 20260922 复跑一致）[实验:code/b7_absolute_snr_recon.py][实验:results/b7_absolute_snr_recon.json]：
   模型 `I = S_src + S_sky + N_local`，`sigma_slow² = Var[N_local] + S_sky/g`，`sigma_w² = sigma_slow² + S_src/g`，**分子只取源**。
   - `T_full`（本单元推导出的模型）：中位 `|SNR/SNR_true − 1|` = **0.0054**、p95 = **0.0296** ⇒ 复原真值；方差面比值中位 1.0031。
   - `T_slow`（漏源项）：中位比值 **1.657**，解析预言 1.645（对拍差 0.70%）⇒ 偏高。
   - `T_naive_sky`（天光双计）：中位比值 **0.874**，解析预言 0.871（对拍差 0.31%）⇒ 偏低。
   - `T_traditional`（字面「天光进分子」）：**1.314** ⇒ 偏高（全域 1.4e5 倍，与传统口径失真同源）。
   - `T_null`（真值无源）：源项 `median(|S_src_hat|/sigma_slow)` = **0**、`T_full ≡ T_slow`（相对差 0.0）；但 `T_naive_sky` **不收敛**（比值 1.36802，与预言相对差 1.1e-16）。有源帧上同一判据 37.31 ≫ 0.05 ⇒ 归零判据非退化。
   - 边界：盲检测消融臂检测完备性 0.43、端到端 `D_core` 中位相对误差 **0.376**（判红），差距全在**分子侧**（检测 + PSF 尺度）而非方差模型；B 臂（真实 HST M16 星云结构）源项只捕获 **14.2%** ⇒「源是稀疏的」先验在延展发射上失效；M42 臂稳健二阶差分 σ 与生产方差面比值 **0.986**，均值版被星云结构污染 8.64 倍，逐源 SNR 与通量 Spearman ρ = 0.9969，**增益不可自估**（lever_var = 0.024 < 0.15）。

## 5 可判定结论

按「成立 / 不成立 / 证据不足」三值对每条假说给出判定；逐条证据见 §1 表格与 §4。

| 判定 | 条目 |
|---|---|
| **成立** | H1、H2、H4/H5、H6、H8、H9/H10、H11、H12、H13；A1、A2、A3、A4、A5、A6、A7、A8、A9；§4.9 四臂（`T_full`/`T_slow`/`T_naive_sky`/`T_traditional`/`T_null`）；§4.9 的 m_ref 精度约定 |
| **有条件成立** | H3：生产口径与 Horne 口径一致，但「经验总 σ + RN 项」臂双计读噪（发现→修复闭环） |
| **不成立（已证伪并订正）** | ① 「seed 无关闭式」标签——双计偏差存在闭式且逐位复现（2.35×10⁻¹⁴）；② 「方向恒偏绿」符号——自洽口径截断**偏低**；③ control_variance N=5「低估 8.5%」方向词——纯公式口径**高估 9.53%**；④ 「1.06×/0.2287 是重建算子通胀」——那是 IDW 代理算子自身性质，冻结默认算子实为**衰减**（T ≈ 0.87）；⑤ 「帧级未裁剪 MAD 高 31.27% ⇒ 帧级 SNR 偏低 23.8%」——归因错误，两条路径不同源 |
| **证伪的任务书前提** | ① 「`T_null` 三臂必须收敛」错：`T_full` 与 `T_slow` 恒等、`T_naive_sky` 按预言发散 1.368 倍，照原话写归零判据在两臂恒真、一臂恒假，**不携带信息**，必须逐臂写；② 「本地噪声与天光散粒在空间上缓变」按字面为假——只有**方差图**缓变（散布 2.0%），噪声实现是白的（lag-1 自相关 −0.4985 ≈ −1/2）；③ 「`T_naive_sky` = 把天光加进分子」自相矛盾——按其公式（分子纯源）实测偏低 0.874、按字面（分子含天光）实测偏高 1.314，两个方向相反的变体被当成一个 |
| **证据不足** | ① 稀疏控制点 sparse_snr_value：口径与 schema 已冻结、Phase2 消费面已接线并红绿验证，但 **Phase1 侧无产者与 HiPS 载体**，生产链当前不产出该层（见 `REPORT_paper.md` §6 的 P2-B2 条）；② 1.152 的标定登记出处待补登（数值已终裁，不影响结论）；③ k_corr 几何查表网格属 P3 交付件；④ 生产链被估量 y 的合同语义待裁决 |

## 6 按分歧台账对既有内容的订正

| 位置 | 原内容 | 订正 | 依据 |
|---|---|---|---|
| 历史正本及本单元转引 | 1.144 支（9216→5811） | 取 1.152；5811→5816.6 | D-04 |
| 05 正向规格 P-CST-11 | "seed 无关闭式" | 错误标签：闭式存在且逐位复现 | A-P2-05/06 + 路线3 复算 |
| 05/02 | 对角欠估 23.3%/36.3% 两个孤立数 | 同一条 1+ρ(M_eff−1) 曲线两点 | A-P2-08 |
| 05 截断条目 | "方向恒偏绿" | 自洽口径截断偏低（符号笔误） | A-P2-09 |
| 恒等门 2.22×10⁻¹⁶ | 固定浮点门 | ulp 计数规则（≤4 ulp 绿） | A-P2-10 |
| control_variance N=5 | "低估 8.5%，保守" | 纯公式口径高估 9.53%（保守）；口径区分 | D-07 |
| k_corr=1.4 冻结常数 | 单一常数 | 两因子几何查表（P3 承载）；1.3883=标定几何专属单次 MC | D-08（负责人已批） |
| 《已确立》§3 | 审查称"系统性幻觉锚" | 实存，行漂移 +2/+3，定性撤销 | A-P2-01 |
| 掩膜 k=0.1 | 有文献倾向 | 项目约定，不注文献出处（负责人已批） | 已批事项 |
| 权重处引用 | 无 | 反方差口径＋Aitken 1935（标注级）进入科学文档 | 已批事项 |

## 7 复现命令

### 7.1 公共前置步（合成数据物理链自检）

```bash
bash 实验/shared/synthetic/run_selftests.sh
```

先证明合成器链本身可信（能红能绿），再让本单元跑真实验。**不得只调
`noise_selftest.py`**：它只验独立重实现，对生产实现零判别力——生产面的门禁责任在
`m16_scene --selftest` 与 `m16_sampling --selftest`。任一组件判红时退出码为 1，
实验读数不得作为证据。

### 7.2 本单元入口

```bash
# A. 单元主实验（seed 20260921；需构建与网络）
bash 实验/absolute-snr/code/run_all.sh

# B. 独立审计三路 + 补实验（seed 20260926 / 20260601+05）
bash 实验/absolute-snr/code/audit/run_all.sh

# C. 帧级 SNR 与星点通量口径（解析/物理红线 + 生产盘点）
bash 实验/absolute-snr/code/reverse_verify/frame_snr/run_all.sh

# D. SNR 设计数值实验（exp1–exp5）
bash 实验/absolute-snr/code/reverse_verify/snr_design/run_all.sh

# E. SNR 设计审计复算（可从仓库根直接跑，无需切换工作目录）
bash 实验/absolute-snr/code/reverse_verify/snr_design/audit/run_all_audit.sh

# F. F-INSTR 星点通量口径（需真实标定帧产品树）
bash 实验/absolute-snr/code/reverse_verify/f_instr/run_all.sh
```

所有入口均从**仓库根**执行，脚本内部从自身位置推导仓库根，不依赖调用者的工作目录。
需要真实标定帧产品树的入口（C 的 P13 项、D 的 exp2/exp3、E 的三项真实数据段、F 全部）
用环境变量 `P2_NORM_DIR` 指定产品树，默认落点为
`run/RELEASE-02/L4-rebuild/norm`，布局须为 `<norm>/<tile>/calibrated_*.fts`。
缺少该产品树时相关项以明确诊断退出（码 2 或列明缺哪一项），不以 `IndexError` 形式失败。

**seed 说明**：主实验 SEED_BASE=20260921（code/sci_b_common.py，无时间/环境随机源）；审计三路 20260926（写死于脚本）；补实验主 20260601、生产链臂 20260605、独立复核 20260602–04。两套 seed、两套实现互为独立复现。

## 8 诚实边界

1. **1.152 标定登记出处**待补登（数值已终裁，不影响结论）。
2. **m_ref=6.0** 为单位制锚点（冻结纪律），无一手文献锚。
3. **k_corr 查表网格**属 P3 交付件，本文只引机制腿（AR(1) 膨胀律），不引查表数值。
4. **P-CST-23 声称系列**生成配置欠定，S2 语义下部分复原（同号同量级）。
5. **生产链被估量 y 合同语义**（裁剪后中位数 vs 全样本中位数）待负责人裁决。
6. **对 MC 真值的偏置数字**带 ±3 pp MC 噪声；臂比值 vs 闭式预言（≤1.25 pp）为低噪证据。
7. **仿真坐标为声明量**；真实数据 hold-out 真值自带 0.0224 dex 噪声，RMSE 为扣除后上界且未触零。
8. **文献降级项**：Serfling 1980 / Cramér 1946 §28.5 书目级（小节号存疑）；Moffat 1969 bibcode 级；Aitken 1935 标注级（批准引用）。详见 refs.md。
9. **fail-closed 状态机**为规范转写自检，lib/ 实现侧无对应符号，不构成对实现的验证。
10. **§4.9 四臂的适用域**：盲检测臂判红（检测完备性 0.43），差距在分子侧；B 臂在延展发射上「源稀疏」先验失效（源项捕获 14.2%）。四臂结论只在**检测完备且源确实稀疏**的域内成立，不得外推到延展发射或盲检测场景。
11. **§4.9 的网格原点**：生产噪声模型控制点在 `(i+0.5)Δ`，与 `cell_center_v1` 差 0.5 px，生产重建算子正确 fail-closed；本实验显式声明 `grid_origin = 0.5` 使两者重合，未改生产代码。

## 9 佐证来源

本单元每条断言由三类证据腿支撑，正文以 `[文献]` / `[实验]` / `[推导]` 标注来源；文献台账见
`refs.md`，论文参考文献见 `REPORT_paper.md` 文末。

| 腿 | 内容 | 落点 |
|---|---|---|
| 文献腿 | 一手出处逐条核验（DOI / bibcode / arXiv + 核验方式），核验层级不足者如实标注，不冒充 VERIFIED | `refs.md` §A（VERIFIED 19 条）、§B（标注级 3 条）；`REPORT_paper.md` 参考文献 1–19 |
| 实验腿 | 固定 seed 的可复跑脚本，产物落 `results/` 与 `code/audit/results/`，每个度量内置「真值无效应 ⇒ 度量归零/判红」负例，无恒真门 | `code/`（`b1`–`b7`、`exp01`–`exp06`、`audit/`、`reverse_verify/`）与 `results/` |
| 推导腿 | 解析闭式与结构论证，独立于数值实验自洽 | `docs/DERIVATIONS_P2.md`；正文 `[推导]` 标注 |
| 生产实现面 | 判据走生产 ABI 的同尺度入口，而非 Python 重实现 | `code/reverse_verify/frame_snr/`（T12/P12 直调 `snr_science.cpp`）、`code/audit/route3/exp11_frozen_operator_transfer.py`（只读编译生产 `SparseSnrReconstructor`） |
| 外部对拍 | 与成熟天文软件的口径对拍（缺库时登记 UNAVAILABLE，不计失败） | `code/reverse_verify/frame_snr/crosscheck_photutils.py` |
| 合成器链自检 | 实验读数可作为证据的前置条件：证明的是分布与口径正确，不是代码自洽 | `实验/shared/synthetic/run_selftests.sh`（见 §7.1） |

被拒绝采信的文献主张与未决登记项分别见 `refs.md` §C 与 §D，不进入正文。
