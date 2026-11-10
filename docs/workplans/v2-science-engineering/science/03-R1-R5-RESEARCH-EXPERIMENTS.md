# A2｜五项研究问题 R1–R5 与真实历史实验的归属审计

> **重要边界**：[#97](https://github.com/fujiaze/Astro-CS-Database/issues/97) 将 R1–R5 称为“五方向科学研究”，不是五篇已证原创论文或五个已量产创新算法。完成这项任务必须判断到底哪些为已有成熟成果、哪些为 ACSD 自己推导的新机制、哪些只是组合和条件适配。论文主张由事实决定，不按预设“必须五项创新”凑数量。

## 原始证据定位（只索引、不复制成新绿灯）

- [#98 R1](https://github.com/fujiaze/Astro-CS-Database/issues/98)、[#99 R2](https://github.com/fujiaze/Astro-CS-Database/issues/99)、[#100 R3](https://github.com/fujiaze/Astro-CS-Database/issues/100)、[#101 R4](https://github.com/fujiaze/Astro-CS-Database/issues/101)、[#102 R5](https://github.com/fujiaze/Astro-CS-Database/issues/102)，各 Issue 有设计/研究与实际实验评论；[#111](https://github.com/fujiaze/Astro-CS-Database/issues/111) 是后来**全新数学基础与二轮科研**。
- [adc7f33d 历史科研档案](https://github.com/fujiaze/Astro-CS-Database/tree/adc7f33d58359ac3d65014f239d7f53fe92e4912/%E5%AE%9E%E9%AA%8C/science-foundation)：`STATISTICS_RESEARCH.md`、`ROUND2_SCIENCE_BUDGET.md`、`ROUND2_SLIVER_RESEARCH.md`、`oracles/`、`evidence/manifest.json` 与 `evidence/round2_manifest.json`；历史提交可在单独 checkout 执行，不可以当前 branch 的缺文件位置伪装已复跑。
- 失败同样是成果：`detector_cone_completeness_counterexample.json`、`statistics_parallel_loo_failed.json`、`hips_orientation_initial_seam_failure.json`、`export_initial_precision_failed.json`、`round2_high_order_focus_header_serialization_failure.json`、各 `*_pre_*`。不能删或回写。
- 旧科研证据从 `main@adc7f33d` 引用；新生产证据必须使用当前 `rebuild/acsd-v2` 实码与公开接口。两组证据绝不互相替代。

## R1：Gaia XP 跨设备经验响应/标度

**论文来源**：Gaia DR3 XP 外定标/合成光度原文、实验光度与稳健最小二乘一手方法。**ACSD待证明**：源谱响应基积分 `q_{ks}=v_k^Tc_s`，联合参数/共享谱潜变量、灰度与 gain/口径不可辨识、不同响应下的条件信号单位和空间 `α(u)` 应用。

**必须取回和复算**：#98 100星合成评论中的模型和输入；历史 `statistics_independent.json` 中同星 Gaia 潜变量的联合相关实验；`GAIA_RESPONSE.md` 中窄带积分/不可辨识反例。先核清哪个脚本、seed和版本真的生成了数字，找不到就标仅 Issue 记录，不假造可重放脚本。重推“恒星标度正确却无法给未知星云 SED 变换”的反例；比较宽/窄、XP 支持域外、未知 gain、真实与近似滤镜模型、同星多帧 Cov。让另一个 Agent 使用不同解析谱从原始式(1)自算 `q`，并与程序比较。**创新候选**：多源潜变量/空间标度与误差跨 Normalize 传播的具体组合；不得把 Gaia 合成光度本身声明原创。

## R2：探测器原生像元到真 HEALPix 叶的包含型球面 Drizzle

**论文来源**：Fruchter–Hook + HEALPix/HPX；**项目新增研究候选**：连续 TAN/SIP 曲边与 12-face 真实叶的包含型算法、双方向独立 Oracle、面积→最终科学误差反推、高精度 fallback。

**必须取回**：`geometry_experiments.json`、`chart_overlap_independent_review.json`、`cross_geometry.json`、`detector_cone_completeness_counterexample.json`、`round2_sliver.json`、`round2_original_sliver_refined.json`、`round2_mp_affine_fallback.json`、`round2_performance_final_kernel.json` 和 `oracles/round2_affine.cpp`（实际文件位置按历史档案核对）；按 [ROUND2_SLIVER_RESEARCH](https://github.com/fujiaze/Astro-CS-Database/blob/adc7f33d58359ac3d65014f239d7f53fe92e4912/%E5%AE%9E%E9%AA%8C/science-foundation/ROUND2_SLIVER_RESEARCH.md) 单独重推切触与微小正支持。定向反证：四角近似/漏叶 cone、正面积界和0界、强SIP、极点、±1e12 signed高对比、FITS卡四舍五入、预算耗尽的诚实未决。分开记录**数学候选**、受限原型代码、生产 C++ 泛化能力和真实吞吐，不能将有限 case PASS 外推到所有观测。

## R3：统一物理噪声/相关性、Fisher与实际场景方差

**成熟基础**：Poisson+read/GLS/Fisher/linear covariance；**项目需证明的组合**：`V_F≠V_B`，同一 `R` 传播 signal/noise，shared root，稀疏支持预测与 unknown 诚实性；几何不确定度区别于随机误差。

**必须取回**：`STATISTICS_RESEARCH.md`、`statistics_independent.json`、`sparse_variance_reference.json`、`round2_science_budget.json`。构造两帧共享floor、同源多输出、缺gain及 sky 同数据、小型有理矩阵验证，并对100000次 Monte Carlo 实际 coverage 给置信区间。特别独立证明即便参考 Fisher 相同，actual-scene 方差也不同；C∞ bump 必须用来反证“稀疏值足以无条件插值”。**不可称原创**：协方差传播/BLUE等已知线性统计定理；真正可能新增的是与空间几何、未知项及科学证书的闭合方式。

## R4：只估计相对天光差模，保护共同低频天体结构

**成熟基础**：Montage overlap background、GLS/profile、图的连通/规范自由度；**项目新增要素**：可比较观测群中的真实低频保护、实际 `T/H/A/E/K` 与同数据 `P=I-EKT` 传播、混响应/无支持时拒绝伪边。

**必须取回**：#101研究评论、`statistics_independent.json` 的同数据背景模拟及 `STATISTICS_RESEARCH.md`；构造共同真星云梯度无差模、仅差模、两组断连、共享噪声、一个 Hα vs Red 不可比较、真实大气差异等精确/统计例。独立证明 `K H=0` 仅在声明 H 子空间保护共同模式，不能宣称任意低频/未知 PSF 条件的无损。查看历史“相对背景 A+G”方案是否已被正式放弃，不得以其作为当前主线。

## R5：可靠小 N 传统 Coadd/选择、混响应有效核和未来引擎边界

**成熟基础**：BLUE/PSD、约束 QP、经典选择统计、条件平均；**项目研究候选**：N≤7 外部可信 Γ 下单步最大 LOO、平移等变、selection 之后 coverage 的条件分界，异响应/PSF 的有效核语义。

**必须取回**：`statistics_parallel_loo_failed.json`（原多点同时拒绝导致 coverage恶化的真实失败）、`statistics_independent.json`（修订路线）、`export_independent.json`（同leaf相关/支持）。对 N=1..7、变星/瞬变、负信号、孤立坏帧、多坏帧、same-data Γ、混响应和 PSF 变化重新构造显式可比较目标；不能用目标函数下降或输出更平滑声称科学无偏。**不得擅自引入 SACoadd 研发算法**；它在其他项目。

## 创新归属与论文潜力的统一裁决

每方向交付一张**事实卡**：最早可核一手文献/他人成果，ACSD所做严格差异、完整数学命题及其条件、唯一独立复现入口、成功/失败证据、在真实数据上尚未验证的假设、是否足以称“方法创新”或仅“工程系统集成”。由独立审稿者试图给出反例并挑战新颖性。不能以项目名、五个研究任务或重复实验数量自动声称五项原创。

## 完成条件

不止重跑旧结果：每项必须能从原方程推导、找到历史证据原文件、说明原实验能与不能证明什么、从未参与该算法代码的审核者重新造一个判别例，并指出当前 V2 生产实现的差异/缺口。所有无法恢复的旧研究脚本、环境和依赖应明确 `HISTORICAL_UNREPRODUCED` 而不是补写虚假来源。
