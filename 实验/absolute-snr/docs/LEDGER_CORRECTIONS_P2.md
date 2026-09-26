# LEDGER_CORRECTIONS_P2 — 本单元按分歧台账的订正记录

本文件登记实验/absolute-snr 单元在体系化整理中对既有内容（历史正本、转引表述）按《分歧台账》与《五单元成稿简报》执行的订正。每条注明台账编号与订正去向。

| # | 台账编号 | 议题 | 订正内容 | 去向 |
|---|---|---|---|---|
| 1 | D-04 | 1.152 vs 1.144 | 取 1.152（20 万次 MC +0.19%；1.144 支算术漂移 5811→5816.6）；本单元与历史正本中 9216→5811 的读数一律改 5816.6 | REPORT_paper §4.2、REPORT_experiment A2、AUDIT_KEY_RESULTS |
| 2 | D-07 | control_variance N=5 方向词 | “渐近式低估 8.5%”改为“纯公式口径对真方差高估 9.53%（保守）”；端到端 ±1.5%；生产链裁剪臂低估 1.3–3.2%；口径区分写明 | REPORT_paper §4.6、REPORT_experiment A6 |
| 3 | D-08 | k_corr=1.4 | 冻结常数改两因子公式 k_shape×k_geo + (ρ,pixfrac,帧数,patch) 几何查表，由 P3 单元承载（负责人已批）；1.3883 = 标定几何专属单次 MC（带 1.27–1.43）；本单元只引 AR(1) 机制腿 | REPORT_paper §4.6、REPORT_experiment A7、refs.md D 项 |
| 4 | A-P2-01 | 《已确立》§3 幻觉锚指控 | 定性撤销：常数表实存（行 108–130），行漂移 +2/+3 内容均在 | REPORT_paper §1、AUDIT_KEY_RESULTS.registry_findings |
| 5 | A-P2-05/06 | P-CST-11 “seed 无关闭式” | 错误标签订正：闭式存在且逐位复现（路线3 2.35×10⁻¹⁴、路线1 MC 3.3×10⁻⁷）；+1.91×10⁻⁶ = 闭式−登记尾差 | REPORT_paper §4.3、REPORT_experiment A4 |
| 6 | A-P2-07 | P-CST-23 声称系列 | 部分复原（S2 语义 + β≈2.5 + 窗帽 256：3 点同号同单调、2 点同量级），属登记缺失非捏造嫌疑 | REPORT_paper §4.7、诚实边界 |
| 7 | A-P2-08 | 23.3% vs 36.3% | 同族：同一条 1+ρ(M_eff−1) 曲线两点（23.3% = ρ0.101/4-tap 或 M_eff2.6/ρ0.19） | REPORT_paper §4.4 |
| 8 | A-P2-09 | 截断方向 | 05 “恒偏绿”为符号笔误：自洽口径截断偏低；S2 语义下正偏差与登记系列同号 | REPORT_paper §4.7 |
| 9 | A-P2-10 | 恒等门 2.22×10⁻¹⁶ | 1 ulp 固定门随机置位即红（实测 max 2.5 ulp）；判定规则改 ulp 计数（≤4 ulp 绿） | REPORT_paper §4.5 |
| 10 | A-P2-11 | γ=2 | 定义性指数：恒等式唯一确定（4.4×10⁻¹⁶=2 ulp），不需标定证据 | REPORT_paper §4.5 |
| 11 | 已批事项 | 掩膜 k=0.1/0.75·FWHM | 登记为项目约定，不注文献出处（Bertin & Arnouts 仅作实践参照，不作为取值出处） | REPORT_paper §4.7、refs.md A#9 |
| 12 | 已批事项 | 逆方差口径＋Aitken 1935 | 进入科学文档（标注级：DOI 未核，卷期页在案，负责人批准） | REPORT_paper §3.1、参考文献、refs.md B 节 |
| 13 | 已批事项 | 插值设置 | 配置化＋运行日志输出不落盘——P4 单元承载，本单元接口约定处仅声明 | REPORT_paper §2 链条位置 |
| 14 | 路线2 R5 | Fruchter & Hook DOI | 旧 DOI 10.1086/341773 实测为他文；正确 10.1086/338393，仓内引用一律订正 | refs.md A#8 |
| 15 | 路线3 L2 | 稳健尺度归属 | PHASE2_SAMPLER.md:219 归属注应指 Croux & Rousseeuw 1992 | refs.md A#3 |

**历史正本处置**：整理前 README（30.6 KB 实验报告）与 REPORT_paper.md（35 KB 精读报告）原样存档于 docs/LEGACY_README_SCI-B_v1.md 与 docs/LEGACY_REPORT_paper_v1.md；其中仍成立的部分（三口径适用域、双计读噪发现→修复闭环、逆方差集成对拍、恒真门审查、fail-closed 范围声明）已吸收进现行文件；失效/冲突部分以上表为准。