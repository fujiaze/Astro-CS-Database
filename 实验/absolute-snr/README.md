# 实验/absolute-snr — P2 跨帧绝对 SNR（SCI-B）· 单元入口

> **体系化整理定稿（总编对账后）**。唯一事实源：`独立审计/实验重做/总编对账/分歧台账.md` 与 `五单元成稿简报.md`。
> 单元定位：科学链第 2 创新点（最高设计 §2），P1 测光零点 → **P2 绝对 SNR 标尺** → P3/P4/P5。

## 一句话结论

跨帧绝对 SNR 把单帧测光零点提升为可传递的绝对信噪比标尺：全部 25 个登记常数三腿闭合——双计偏差 +14.5009%/+38.2524% 逐位复现（闭式 2.35×10⁻¹⁴）、对角欠估 36.3% 获闭式 1+ρ(M_eff−1)、9216 天空预算与直接管线测量差 1.2%、γ=2 由恒等式唯一确定（4.4×10⁻¹⁶ = 2 ulp）、1.152 经 D-04 终裁——稀疏控制点 sparse_snr_value = F_ref/σ_F 由此成为 P3 上球的无量纲硬通货。

## 交付物导航

| 文件 | 内容 |
|---|---|
| `REPORT_paper.md` | 正式论文（摘要/引言/链条位置/方法/数据/结果/结论/诚实边界/参考文献；数字带 [文献]/[实验]/[推导] 腿标注） |
| `REPORT_experiment.md` | 实验报告（假说/方法/数据/结果/台账订正/复现命令/诚实边界 + 【链条位置】节） |
| `refs.md` | 文献核验台账（只收一手 VERIFIED；标注级与拒绝采信项单列） |
| `results/AUDIT_KEY_RESULTS.json` | 关键结果 JSON 汇总（逐条注明来源路线与台账裁决号） |
| `code/` | 单元主实验 b1–b7（seed 20260921，`run_all.sh`）+ exp01–06 + reverse_verify |
| `code/audit/` | 独立审计三路重做 + 补实验脚本与结果快照（seed 20260926 / 20260601，`run_all.sh`，见其 README） |
| `docs/` | 支撑推导（DERIVATIONS_P2.md）、台账订正记录（LEDGER_CORRECTIONS_P2.md）、历史正本存档（LEGACY_*） |
| `results/` | 单元主实验 JSON（b*.json、exp0*.json）+ `DOC_CORRECTIONS.md` + `figs/` |

## 固定 seed

- 单元主实验：`SEED_BASE = 20260921`（`code/sci_b_common.py`；无时间/环境相关随机源）。
- 审计三路：`20260926`（写死于脚本）；补实验：`20260601`（主）/`20260605`（生产链臂）/`20260602–04`（独立复核）。
- 两套 seed、两套实现互为独立复现；全部读数为固定 seed 复现值，不重跑即引用 `results/` 既有 JSON。

## 复现

```bash
bash 实验/absolute-snr/code/run_all.sh          # 主实验（约 25–35 min，构建/测试串行加锁）
bash 实验/absolute-snr/code/audit/run_all.sh    # 审计三路 + 补实验（35 脚本，秒级）
```

## 与台账/历史正本的关系

- 本单元历史正本（整理前的 README 与论文精读报告）原样存档于 `docs/LEGACY_README_SCI-B_v1.md` 与 `docs/LEGACY_REPORT_paper_v1.md`，未删除；仍成立部分已吸收进现行文件。
- 与台账冲突处按裁决改写并逐条注明 D-xx/A-P2-xx，明细见 `docs/LEDGER_CORRECTIONS_P2.md`（1.152 终裁与 5811→5816.6、“seed 无关闭式”错误标签、对角欠估闭式统一、γ 恒等门 4 ulp 规则、control_variance N=5 方向词、k_corr 改查表等）。
- 负责人已批事项在本单元的落实：k_corr 查表由 P3 承载（本单元只引机制腿）；插值设置配置化＋运行日志输出不落盘（P4 承载）；掩膜 k=0.1 等规范选择登记为“项目约定，不注文献出处”；反方差口径＋Aitken 1935（标注级）引用进入科学文档。

## 诚实边界速览

1.152 标定登记出处待补登；m_ref=6.0 为单位制锚点（冻结纪律）；k_corr 查表网格属 P3 交付件；P-CST-23 声称系列生成配置欠定；生产链被估量 y 的合同语义待负责人裁决；对 MC 真值的偏置数字带 ±3 pp MC 噪声（低噪证据是臂比值 vs 闭式预言 ≤1.25 pp）。详见 `REPORT_paper.md` §6 与 `REPORT_experiment.md` §7。