# reverse_verify 定案结论 · SCI-A（低阶空间乘法增益）

> 上游：ASTROCS_DESIGN.md §2.1 / §4.2 / §12.3；`ENGINEERING_SPEC.md §9`（有长期价值的结论并入正式文档）
> 来源：`reverse_verify/docs/p1-spatial-gain.md`（2026-09-21 ROOT-CONSOLIDATION 迁入 `实验/photometric-magnitude/docs/p1-spatial-gain.md`）
> 本文件只登记**现行结论**；完整推导、逐条数值与复跑细节见原文。

## 1. 定案（现行结论）

| # | 问题 | 判定 | 关键数值 |
|---|---|---|---|
| 1 | 低阶空间乘法增益 `m(x,y)` 能否用星点拟合出来？ | **能** | 合成（真值 pp 7%，N=200，逐星散差 0.010 dex）：帧间乘性残差场 **9.36% → 2.30%（order 1）/ 0.19%（order 2）** |
| 2 | 阶数取多少？ | **order 1 为默认；order 2 只在星多且覆盖好时开** | order 1 负例噪声底（N=200）中位 1.23%；order 2 = 2.72%；order 3 = 5.40%。order 1 分半一致性 corr 中位 **0.997**；order 2 仅 **0.929**（p10 0.41） |
| 3 | 会不会过拟合（负例归零）？ | **度量能归零**（收到统计噪声底） | 真值 m≡1、N=200、σ_int=0.010 dex 时，order 1 拟合出的 m 幅度 **1.24%（中位）**（σ_int=0 时 **0.03%**），校正后残差场 **0.095%** |
| 4 | 真实数据（49 帧）有效吗？ | **未获验证（关键负面结果）** | 拟合孔径 r=4 px 内：3×3 峰峰 6.75% → 4.59%（order 1，70% 帧对改善，**in-sample**）；换独立孔径 r=6 px **反而变差**（3.81%→4.86%、3.49%→6.08%）。孔径探针 r=3/4/6/10 = 7.78%/4.93%/3.81%/4.25% ⇒ 主要是**孔径/PSF 系统差**，不是纯乘法增益 |
| 5 | 空间乘法能不能从**背景**拟合？ | **不能**（数学退化） | m→m+δ/(a·S0), g→g−δ 给出同一个 y：`max\|Δy\| = 2.27e−13 ADU`（梯度自身 pp 69 ADU）；同一对模型在星点上流量比 pp **9.78%** |
| 6 | 与 `k_photo` 的关系 | **m 归一化到星集合几何均值 1** | order=0 时严格退化为现有标量 `k_photo=10^(−location)` |
| 7 | 实现改动面 | **新增，不改冻结 ABI**；但**前置条件未满足** | 1 个新 C++ 入口 + 1 个新 apply 函数 + `p1_op_photometry` 内 1 处替换。**前置**：`F_instr` 必须换成 PSF 稳健口径并做孔径无关性验收（见 `实验/absolute-snr/results/REVERSE_VERIFY_CANON.md`） |

## 2. 诚实登记（未达标项，不得放宽）

- 独立 C++ Oracle **C3**（高阶噪声底 p90 ≥ 3× order1）**未达标**：实测 **2.89×**（中位比 3.6×）。方向性结论成立，预先写定的 p90 阈值**未满足** ⇒ 登记 FAIL。
- 独立 C++ Oracle **C5**（N=20 时 order 2 伪 m p90 ≥ 1.0× 真值 pp）**未达标**：实测 **0.86×**。余量几乎为零但**没有**越过预先写定的危险线 ⇒ 登记 FAIL。
- 真实数据**逐帧绝对 m 幅度不可辨识**（联合差分模型只有相对增益可辨识）；生产链逐帧 Gaia 绝对锚点不存在于 L4 产物中 ⇒ 本轮**不作为结论**。
- **真实数据上空间增益的有效性未获验证** ⇒ **本轮不宣称真实数据已被校准。**

## 3. 复跑

```bash
# (A) 独立 C++ Oracle（判据内嵌源码顶部；退出码 0 = 全 PASS）
bash 实验/photometric-magnitude/code/reverse_verify/p1_spatial_gain/build_oracle.sh
run/reverse_verify/p1_spatial_gain/p1sg_oracle

# (B) 合成实验（150 MC，约 7 min）
python3 实验/shared/synthetic/synth_gain.py

# (C) 真实数据（49 帧；联合拟合约 9 min）
python3 实验/photometric-magnitude/code/reverse_verify/p1_spatial_gain/src/real_gain.py
```

实测见 `run/ROOT-CONSOLIDATION/logs/migration_rerun.md`。
