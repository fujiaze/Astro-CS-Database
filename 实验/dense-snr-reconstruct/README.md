# 实验/dense-snr-reconstruct — P4 重建稠密信噪比（科学链第 4 点）

> **单元定位**：由稀疏控制点（P2 sparse_snr_layer，Δ=64 px cell_center_v1）重建稠密 SNR 场，供 P5 逐像素定权（w = SNR²/F_ref² = 1/σ_F²）。最高设计 §2 第 4 点。
> **成稿依据**：独立审计/实验重做/总编对账/分歧台账.md 与 五单元成稿简报.md（唯一事实源）；三路证据 = 同目录下 P4重建稠密SNR/路线1/2/3。
> **复现**：bash code/run_all.sh（固定 seed；首行先跑公共前置步 `实验/shared/synthetic/run_selftests.sh`，随后串行 19 个原实验与 M16 物理前向仿真腿）。
> **公共前置步**：合成数据物理链自检（m16_mask / m16_scene / m16_sampling / noise_selftest 四组件全跑，
> 实测 4/4 PASS、rc=0、约 2 分 32 秒）。**不得只调 `noise_selftest.py`**——它的 12 个用例中只有 A2
> 调用生产实现且生产臂不进入判词，对生产实现零判别力；生产面的门禁责任在
> `m16_scene --selftest` 与 `m16_sampling --selftest`。

## 一句话结论

**重建以自然三次样条钳制算子（natural_bicubic_spline_clip_v1）为生产默认（节点复现 ≤3.6e-15、可分辨域 Δ/ℓ≲1 内最优、收敛阶 −4 闭合），定权恒等式 w = SNR²/F_ref² = 1/σ_F² 机器精度成立且 γ=2 由定义唯一确定；控制值必须携带源项亮度（丢源项 → P5 效率最劣损失 12 倍）；IDW 为备选口径，默认 idw_power=1.0（台账 D-05 终裁）配置化；稀疏控制格不表示 PSF 尺度（3.8 dex，有效域边界）。**

## 文件索引

| 文件 | 内容 |
|---|---|
| REPORT_paper.md | 正式论文（三路证据综合定稿，含【链条位置】节） |
| REPORT_experiment.md | 实验报告（假说/方法/数据/结果/结论/诚实边界/复现命令） |
| refs.md | 文献核验台账（一手 VERIFIED 条目 + 标注级注明） |
| code/ | 三路 19 个可复现脚本（route1/route2/route3 保留原文件名）+ run_all.sh + SEEDS.md |
| code/sim/exp_sim01_m16_forward_snr_truth.py | M16 物理前向仿真腿（最高设计 §12.2 第 1 类数据）：真实 M16 帧作纯信号模板经共享物理链生成仿真采样帧，逐像素 SNR 真值解析已知；含四条归零负例与噪声相关性判据取舍表（详见 REPORT_experiment.md §4.1、REPORT_paper.md §4.8） |
| results/ | 关键结果汇总 summary.json + 三路存档 JSON（route1/route2/route3）+ sim/ 物理前向仿真腿结果 |
| docs/ | 支撑推导（derivations.md：定权恒等式、条件数、收敛阶、IDW 极限、Δ² 律、白性恒等式） |

## M16 物理前向仿真腿

本单元第一次拿到**逐像素绝对 SNR 真值**：真实 HST M16 F657N drz 帧作纯信号模板，经共享物理链
（源/天光/暗流电子域 Poisson、读出电子域 Gaussian、增益+饱和+量化、平场、天空梯度）生成仿真采样帧，
用物理链的逐像素期望量给出 SNR_true = (S_e/g)/√((S_e+B_e+D_e)/g² + RN²/g² + 1/12)。

结论：在 M16 这样的真实结构场上，**三口径排序 E_frame < E_cell < E_dense 在 6/6 个对比度档全部成立**
（跟踪得越少越好），稠密口径的失效来自样条欠冲（负值像素占比 2.5%–22.6%），值域钳制守卫把它从 2.58e4
压到 4.54。**已声明的有效域判据（真方差动态范围 1.78–235）被否证**——窗口内稠密口径照样失效，真正的门是
场的**亚 Δ 空间功率**。因噪声相关性边界（真实 drz 噪声已相关化、仿真帧逐像素独立），IDW 最优幂、Δ² 偏置
系数、白性判据与 E_eff 绝对值外推**一律不在本腿定标**，取舍表见 REPORT_experiment.md §4.1。

## 链条接口速览

- **上游（P2→P4）**：绝对 SNR(x,y) + F_ref,k + 控制点几何（Δ=512/8=64 结构派生，A-P4-01）；SNR_c = F_ref/√(σ_slow²+S_src/g)（亮度携带义务）。
- **P4 变换**：冻结算子词表（默认 natural_bicubic_spline_clip_v1；bilinear=对照/回退；IDW=备选 p=1.0/K=16/γ<1e-10，配置化+日志 p*）；节点复现门 1e-9（浮点性质，实测 3.6e-15）；值域钳制必要；fail-closed 覆盖域；空格≠零。
- **下游（P4→P5）**：w = SNR²/F_ref² = 1/σ_F²（γ=2 恒等式）；m_ref 仅记录参考电平（w 对 m_ref 不变逐位）；k_corr 两因子查表归 P3 单元承载（D-08）。

## 关键终裁索引（与分歧台账对应）

D-04（1.152）、D-05（idw_power=1.0）、D-08（k_corr，P3 承载）、D-10（默认算子）、A-P4-01…07、A-P2-11（γ 恒等式四路）。历史内容与本单元冲突处一律以台账为准，正文内已逐处注明"按分歧台账 D-xx 订正"。
