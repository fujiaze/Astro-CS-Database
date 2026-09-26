# 实验/dense-snr-reconstruct — P4 重建稠密信噪比（科学链第 4 点）

> **单元定位**：由稀疏控制点（P2 sparse_snr_layer，Δ=64 px cell_center_v1）重建稠密 SNR 场，供 P5 逐像素定权（w = SNR²/F_ref² = 1/σ_F²）。最高设计 §2 第 4 点。
> **成稿依据**：独立审计/实验重做/总编对账/分歧台账.md 与 五单元成稿简报.md（唯一事实源）；三路证据 = 同目录下 P4重建稠密SNR/路线1/2/3。
> **复现**：bash code/run_all.sh（~30 s，固定 seed；本单元不重跑实验，results/ 引用既有存档）。

## 一句话结论

**重建以自然三次样条钳制算子（natural_bicubic_spline_clip_v1）为生产默认（节点复现 ≤3.6e-15、可分辨域 Δ/ℓ≲1 内最优、收敛阶 −4 闭合），定权恒等式 w = SNR²/F_ref² = 1/σ_F² 机器精度成立且 γ=2 由定义唯一确定；控制值必须携带源项亮度（丢源项 → P5 效率最劣损失 12 倍）；IDW 为备选口径，默认 idw_power=1.0（台账 D-05 终裁）配置化；稀疏控制格不表示 PSF 尺度（3.8 dex，有效域边界）。**

## 文件索引

| 文件 | 内容 |
|---|---|
| REPORT_paper.md | 正式论文（三路证据综合定稿，含【链条位置】节） |
| REPORT_experiment.md | 实验报告（假说/方法/数据/结果/结论/诚实边界/复现命令） |
| refs.md | 文献核验台账（一手 VERIFIED 条目 + 标注级注明） |
| code/ | 三路 19 个可复现脚本（route1/route2/route3 保留原文件名）+ run_all.sh + SEEDS.md |
| results/ | 关键结果汇总 summary.json + 三路存档 JSON（route1/route2/route3） |
| docs/ | 支撑推导（derivations.md：定权恒等式、条件数、收敛阶、IDW 极限、Δ² 律、白性恒等式） |

## 链条接口速览

- **上游（P2→P4）**：绝对 SNR(x,y) + F_ref,k + 控制点几何（Δ=512/8=64 结构派生，A-P4-01）；SNR_c = F_ref/√(σ_slow²+S_src/g)（亮度携带义务）。
- **P4 变换**：冻结算子词表（默认 natural_bicubic_spline_clip_v1；bilinear=对照/回退；IDW=备选 p=1.0/K=16/γ<1e-10，配置化+日志 p*）；节点复现门 1e-9（浮点性质，实测 3.6e-15）；值域钳制必要；fail-closed 覆盖域；空格≠零。
- **下游（P4→P5）**：w = SNR²/F_ref² = 1/σ_F²（γ=2 恒等式）；m_ref 仅记录参考电平（w 对 m_ref 不变逐位）；k_corr 两因子查表归 P3 单元承载（D-08）。

## 关键终裁索引（与分歧台账对应）

D-04（1.152）、D-05（idw_power=1.0）、D-08（k_corr，P3 承载）、D-10（默认算子）、A-P4-01…07、A-P2-11（γ 恒等式四路）。历史内容与本单元冲突处一律以台账为准，正文内已逐处注明"按分歧台账 D-xx 订正"。
