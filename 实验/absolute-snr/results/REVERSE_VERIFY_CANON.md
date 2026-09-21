# reverse_verify 定案结论 · SCI-B（帧级 SNR / 星点通量口径 / SNR 传播设计）

> 上游：ASTROCS_DESIGN.md §2.2 / §4.4 / §5.3 / §12.3；`ENGINEERING_SPEC.md §9`
> 来源：`reverse_verify/docs/{frame-snr-canon,f-instr-canon,snr-propagation-design}.md`
> （2026-09-21 ROOT-CONSOLIDATION 迁入 `实验/SCI-B/docs/`）。本文件只登记**现行结论**。

## 1. 帧级 SNR 定案（`frame-snr-canon.md §0`，逐字）

> **AstroCS 的帧级 SNR 是「点源（PSF）信号 SNR」：** `SNR_frame = F_signal / sigma_F`，
> **`F_signal` 已扣局部背景（天光均值绝不进分子）**，
> **`sigma_F` 由 PSF 加权最优提取的方差给出、且天光散粒噪声必须计入**。
> **固定源通量、天光变亮 ⇒ SNR 严格单调下降；天光→∞ ⇒ SNR→0。**
> 它是**无量纲**量，**不需要** gain / 口径 / 曝光时间等 FITS 头拿不到的物理量；
> **不得**与面亮度 SNR 混用，**不得**被天光抬高。

## 2. 星点通量口径定案（`f-instr-canon.md §0`）

| 项 | 结论 |
|---|---|
| **病灶** | 现生产 `F_instr` = **5×5 固定盒 + 正性截断**（`star_detector.cpp:139-151` 的 `m00`），把**视宁度**读成了**乘性增益** |
| **量级** | seeing 2.0→4.0 px：该口径回收通量变化 **−0.72 等**（假增益 **0.516×**）；孔径 r=2/3/4/6/10 给出 0.493/0.672/0.808/0.925/0.985 |
| **定案** | `F_instr` 改用 **PSF 总通量**（`flux = 2πA·s_x·s_y/3`，β=4 族，与 `docs/science/PSF.md` 一致）；低 S/N 精化 = PSF 加权最优提取（Horne/Naylor） |
| **孔径无关性** | 实测 over seeing 1.5–5.0 px：**M_seeing ≤ 0.004 mag**，假增益 0.998（真值 1.000）；生产口径同测 **1.353 mag / 0.516** |
| **符合性** | 冻结合同 `docs/science/PHOTOMETRY.md:95` 已写明「星点通量来自 PSF 拟合域」⇒ **不是改规范，是修实现符合性** |
| **改动面** | 约 **5 处、~120 行**；库内已有 PSF 总通量解析式（`dpsf_psf.cpp:428`）与解析增长曲线（`snr_science.cpp:118`），`p1_psf.json` 只是没把 flux 列写出来 |
| **诚实边界** | 本仓**无哈勃数据**，改用 L4 真实标定帧作底；`k_photo` **绝对值无物理意义**，全部判据为**尺度无关的星等比** |

## 3. SNR 传播设计结论（`snr-propagation-design.md §0`，含 §13 复核订正）

1. **单帧 SNR 必须是「通量型、信号已扣独立背景、天光只进噪声」**：`SNR_k(F_ref) = F_ref / σ_F,k`，`σ_F,k⁻² = P_kᵀ C_k⁻¹ P_k`。
2. **权重误差的代价是二阶的、且只由帧间散射决定**：`Var_approx/Var_opt = 1 + Var_p(δ) + O(δ³)`；**共模权重误差完全免费**（实测 ratio = 1.000000）。
3. **归一化后的方差必须写成「残差制造者」形式**：`Var(corrected_i) = Σ_j P_ij² σ_j²`，`P = I − H`；朴素写法在 N=8 时高估 `9/7 = 1.2857×`。
4. **稀疏→稠密重建的精度损失可解析给出**：kriging 误差由 `Δ/ℓ` 单参数控制；`Δ ≲ ℓ` 才有效；双线性在 `Δ ≥ 2ℓ` 时比常数还差。实测真实帧 `ℓ ≈ 48 px`，Phase-2 UPM 控制点间距 64 px 与之一致，Phase-1 的 8×8 patch 网格（512 px）**过疏**。
5. **稀疏 SNR 层只在「帧间相对权重随位置变化」时才有增益**。**【§13 复核订正】** 原「帧级标量效率损失 0.063% / 稀疏层 6.8%」是（全帧 4096² + 标量取 patch σ 场均值 + 评价窗口中心 1024²）**组合**的产物，换评价窗口即得 4.42%（70×）；完整物理仿真下理想标量惩罚 **0.0002%**，64 px 稀疏层代价仅 **0.175%（方差）/0.088%（σ）**，比原报告小约 19 倍；稀疏层代价**全部**来自控制点帧间独立测量噪声（真值 oracle 惩罚 1.000000）。生产实际用的整帧未裁剪 MAD 标量在大天光散差下**反而劣于稀疏层**（散差 2.22× 时惩罚 29.4%）。**修正表述**：该数据集上理想标量惩罚 ≲0.01%，稀疏层净亏 ≈0.18%（方差），随帧间天光散差增大而双向放大。
6. **当前生产链成品 σ 相对误差 ≈ +31%（不是 131%）**。**【§13 复核订正】** `σ_MAD/σ_clippedRMS = 1.3127` 是**比值**，σ 相对误差 31.27%（方差高 76.8%、SNR 低 23.8%）；`eps_drizzle = 20.2%` 被脚本注明存在却从未进入 `eps_total` 求和；`eps_theta` 的 3.4%/2200% 无法由所引证据复算（应为 +20.1% / +608.6%）。**修正表述**：修好源污染偏差并传播 `C_θ` 后，良态 ≈ **20.5%**、病态 ≈ **609%**（含 drizzle 相关噪声项）；若暂不计 drizzle 则良态 ≈ 20.2%。

## 4. 诚实登记

- `snr-propagation-design.md §7` 的「未决与待办」保持原样，未随本次迁移改变状态；
- 三份文档中的主线文档订正建议（`frame-snr-canon.md §5`、`f-instr-canon.md §4.1` 差距表）**只登记未改 `docs/`**（`docs/science`、`docs/algorithms` 为只读权威）；
- `f-instr-canon.md` 的底数据是 **L4 真实标定帧**，不是哈勃数据（原文 §3.0 已声明）。

## 5. 复跑

```bash
bash 实验/SCI-B/code/reverse_verify/f_instr/run_all.sh
bash 实验/SCI-B/code/reverse_verify/frame_snr/run_all.sh
bash 实验/SCI-B/code/reverse_verify/p7_noise/run_all.sh      # 若存在
bash 实验/SCI-B/code/reverse_verify/snr_design/run_all.sh
bash 实验/SCI-B/code/reverse_verify/snr_design/audit/run_all_audit.sh
```

实测与退役登记见 `run/ROOT-CONSOLIDATION/logs/migration_rerun.md`。
