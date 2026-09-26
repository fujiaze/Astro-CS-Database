# P2 跨帧绝对 SNR · 实验报告（REPORT_experiment）

**单元**：实验/absolute-snr（SCI-B，科学链创新点 P2）
**事实源**：独立审计/实验重做/总编对账/分歧台账.md、五单元成稿简报.md；历史正本（存档 docs/LEGACY_README_SCI-B_v1.md、docs/LEGACY_REPORT_paper_v1.md）仍成立部分已吸收，失效部分按台账订正并注明。
**正式论文**：REPORT_paper.md（本报告为其实验证据底稿）。

---

## 【链条位置】

- **上游接口（P1 → P2）**：输入逐帧测光零点 ZP_k（mag）与逐星产品（F、FWHM）。P2 生成 F_ref,k = 10^(−0.4(m_ref−ZP_k)) [ADU]；恒等式 F_ref,k·k_photo,k = F0 锁定与 P1 的自洽 [实验:code/audit/route1/exp04_double_count_bias.py]。P1 低星数帧零点不确定度被低估（MAD 有限样本偏差 1.49×，n=3）会传入 P2，按 P1 单元精度约定处理。
- **本环产出**：① frame_snr（无量纲，依赖 m_ref，须同档比较）；② 逐源 σ_F/SNR_F（ADU 域）；③ 稀疏控制点 sparse_snr_value = F_ref/σ_F（无量纲，Δ=64 px 网格）；④ 叠加权唯一换算口径 w = SNR²/F_ref² = 1/σ_F² [ADU⁻²]。
- **下游消费**：P3 上球（消费控制点＋预测方差；k_corr 查表由 P3 承载，D-08）；P4 稠密重建（插值配置化＋运行日志输出不落盘，已批，承载于 P4）；P5 加性天光去除（w 组合，SNR_comb² = ΣSNR_k²）。
- **精度约定**：m_ref 随产品落盘；换算权逐帧比对对 m_ref/ZP 不变（≤3.3×10⁻¹⁶）[实验:code/audit/route3/exp04_refmag_chain.py]；量纲错位 ⇒ SNR 偏一个 gain 因子（增益不变性检验锁定约定）[实验:code/audit/route1/exp04_double_count_bias.py]；控制点精度约定 1.5%（N_sky=9216 口径）穿过 P4 接口仅通胀 1.06× [实验:code/audit/route3/exp04_refmag_chain.py]。

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
| A2 | c(n=64)=1.152（vs 1.144） | **终裁 1.152（D-04）**：MC 1.1508（+0.19%）；1.144 支算术漂移 5811→5816.6 | [实验:code/audit/route1/exp01_robust_statistics_constants.py][实验:code/audit/route1/exp03_sky_budget_constant.py] |
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
5. **判定纪律**：负例均非退化；恒真门（RMSE≡s_field、平坦场排序）移出证据；每个数字标注 [文献]/[实验:文件]/[推导] 三腿来源。

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
7. **接口传递**：1.5% 控制点精度穿过 P4（通胀 1.06×）；SNR_comb²=ΣSNR_k²（2.2×10⁻¹⁶）；对角近似宣称方差仅为实际 32.5% [实验:code/audit/route3/exp04_refmag_chain.py][实验:code/b4_integration.py][实验:code/b5_phase3_transfer.py]。
8. **适用域**：地面稀疏胜帧级（0.0413–0.0825 vs 0.0506–0.1691 dex）、HST 帧级胜（Δ*≈16 px）、稠密 64 MiB/帧超预算 64 倍（诊断地位）[实验:code/b3_domain_map.py]。

## 5 按分歧台账对既有内容的订正

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

## 6 复现命令

```bash
# A. 单元主实验（seed 20260921，约 25–35 min，构建/测试串行加锁）
bash 实验/absolute-snr/code/run_all.sh

# B. 独立审计三路 + 补实验（seed 20260926 / 20260601+05，秒级 ×35）
bash 实验/absolute-snr/code/audit/run_all.sh
# 或分目录：
for s in 实验/absolute-snr/code/audit/route1/*.py; do python3 "$s"; done
for s in 实验/absolute-snr/code/audit/route2/*.py; do python3 "$s"; done
for s in 实验/absolute-snr/code/audit/route3/*.py; do python3 "$s"; done
for s in 实验/absolute-snr/code/audit/supplement_control_variance/*.py; do python3 "$s"; done
```

**seed 说明**：主实验 SEED_BASE=20260921（code/sci_b_common.py，无时间/环境随机源）；审计三路 20260926（写死于脚本）；补实验主 20260601、生产链臂 20260605、独立复核 20260602–04。两套 seed、两套实现互为独立复现。

## 7 诚实边界

1. **1.152 标定登记出处**待补登（数值已终裁，不影响结论）。
2. **m_ref=6.0** 为单位制锚点（冻结纪律），无一手文献锚。
3. **k_corr 查表网格**属 P3 交付件，本文只引机制腿（AR(1) 膨胀律），不引查表数值。
4. **P-CST-23 声称系列**生成配置欠定，S2 语义下部分复原（同号同量级）。
5. **生产链被估量 y 合同语义**（裁剪后中位数 vs 全样本中位数）待负责人裁决。
6. **对 MC 真值的偏置数字**带 ±3 pp MC 噪声；臂比值 vs 闭式预言（≤1.25 pp）为低噪证据。
7. **仿真坐标为声明量**；真实数据 hold-out 真值自带 0.0224 dex 噪声，RMSE 为扣除后上界且未触零。
8. **文献降级项**：Serfling 1980 / Cramér 1946 §28.5 书目级（小节号存疑）；Moffat 1969 bibcode 级；Aitken 1935 标注级（批准引用）。详见 refs.md。
9. **fail-closed 状态机**为规范转写自检，lib/ 实现侧无对应符号，不构成对实现的验证（历史正本 I2 降级维持）。
