> 【存档说明】本文件为体系化整理前的历史正本快照（成稿整理时原样保存，未删改任何内容）。
> 现行单元入口见 `../README.md`；正式论文见 `../REPORT_paper.md`；实验报告见 `../REPORT_experiment.md`。
> 与分歧台账冲突处（1.152 终裁、k_corr 查表、control_variance N=5 方向词、γ 恒等式门值等）以现行文件与台账裁决为准。

---

# 跨帧绝对 SNR 传递链（创新点二）精读报告

> **报告性质**：SCI-504 执行子代理产出的**论文式中文精读报告**，面向负责人阅读。
> **对象**：实验单元 `实验/absolute-snr/`（内部代号 SCI-B；`docs/` 中的旧路径名 `实验/SCI-B` 见 §5.3-C1）。
> **纪律声明**：本报告只**新建**本文件；未修改 `code/`、`results/`、`lib/` 生产代码与任何正式科学文档；未执行任何 git 写操作。全部数字来自 `results/*.json` 现行版本（`generated_at` 见表 A-0）、`README.md`、`results/DOC_CORRECTIONS.md`、`results/REVIEW.md`，以及本轮**只读实跑**（`ctest -R p1snr_science`，见 §4.3）。
> **版本锚**：实验单元 seed `20260921`；生产修复 commit `cdcc0975`（SCI-501 / FIX-407，RELEASE-05）。
> **一句话结论**：**猜想成立，但成立在明确的条件集上**——帧级 `SNR=F_signal/σ_F`、`σ_F⁻²=ΣP_i²/σ_i²`（Horne 1986）在物理 Monte Carlo 真值下 26+29 个扫描点全部 ≤3σ；天光只进噪声项，固定源通量抬升天光时 SNR 单调下降、天光主导段斜率 ≈ −1/2。工程上有两处必须按冻结口径执行：**σ_sky 入参语义显式声明（防读噪双计，已修复并有保护测试）**与**权重只在 Phase2 由未加权原始 SNR 现场换算**。

---

## 摘要

跨帧绝对 SNR 是 ACSD 三个核心创新点中的第二个：把每一帧的点源信噪比写成**与天光无关、且跨帧可比**的绝对量，使 Phase2 能用逆方差权重做科学叠加、Phase3 能按协方差传播不确定度。本报告对该创新点的实验单元（`实验/absolute-snr/`，结果 `results/b1..b6*.json`）做逐项精读与独立复核。

**主要结论（猜想是否成立、在什么条件下成立）**：

1. **定义式成立**。帧级 `SNR = F_signal/σ_F`，`σ_F⁻² = Σ_i P_i²/σ_i²`，`σ_i² = σ_sky,i² + (RN/g)² + F·P_i/g`（Horne 1986, PASP 98, 609）。在完整物理前向仿真（电子域 Poisson + 高斯读出 + 增益）下，定义式与 Monte Carlo 经验散度在 26 个天光点（max|z|=2.06）与 29 个噪声项扫描点（max|z|=2.68）上**全部落在 3σ 内**。
2. **天光只进噪声项**。固定真实源通量、只抬升天光（散粒噪声如实进噪声项）时，SNR 严格单调下降：亮源斜率 **−0.4879**、暗源 **−0.4972**（理论 −1/2）；`SNR(10⁶)/SNR(0)` 降到 **2.11% / 1.07%**。传统"信号含天光"口径在同一天光范围内**上升**，B=10⁶ 时相对真值 **×3419（亮源）/ ×1.03×10⁵（暗源）**，失真达 3 个数量级。
3. **发现并闭环了一处真实生产缺陷（D1）**：调度器把**含读噪**的经验空天总 rms 填入 `sigma_sky_adu`，而 gain>0 分支又加一次 `(RN/g)²`，造成**读噪双计**、σ_F 高估、SNR 低估。修复 = `SnrSourceParams.sigma_sky_source` 显式语义枚举（`SHOT_ONLY` / `EMPIRICAL_TOTAL_RMS`），生产调用点声明 `EMPIRICAL_TOTAL_RMS`，保护测试 `p1snr_science_skysource` 双向可假（正确口径 zA=1.27 绿、双计臂 zB=25.6 红）。
4. **另发现一处文档/实验域单位混用（D2）**：`1.44/√N` 是**相对**标准误，dex 口径应为 `1.44/ln10/√N ≈ 0.625/√N`；当作 dex 用会高估 2.3026 倍（=ln10）。
5. **非退化判据被修正**：「帧级臂 RMSE ≤ K·s_field」类判据对**任意**真值场恒真（对抗打乱场下权重效率损失 E=7.17 仍绿），**不得充当证据**；替代判据为权重效率损失 `E=Var_w/Var_opt−1`，双向可假已实测。
6. **三口径没有全局最优、只有适用域**：默认 `sparse_reconstruct`(Δ=64) 在地面视宁度受限域三帧全部胜出帧级标量；HST 类高对比结构域帧级标量更优（Δ*≈16 px）；稠密口径 4096² 需 **67,108,864 B = 64 MiB/帧 = 1 MiB 预算的 64 倍**，稠密超门成立。

**工程结论**：入库量必须是**未加权原始通量型 SNR**（PixInsight PSFSNR 是功率比、PSFSW 是权重，二者都不入库）；权重在 Phase2 由 `w_k=SNR_k²/F_ref,k²≡1/σ_F,k²` 现场换算；`F_ref` 逐帧独立锚定 `m_ref=6.0` 且**必须同帧配对**；HST 类高对比数据默认给帧级标量兜底，不得静默用稀疏口径冒充空间精度。

---

## 1. 引言

### 1.1 创新点定位与验收口径

最高设计把"跨帧绝对 SNR"列为创新点二，验收面在 `ACCEPTANCE_SPEC.md` §2.2 的七行判据表：SNR 不含天光信号、真值一致性、方法学对标、稀疏层与重建、逆方差集成、传递到 Phase3、非退化负例。本报告按这七行逐项给出结论与证据（§4）。

要解决的问题是**跨帧可比性**：同一批数据里各帧的天光、读出噪声、PSF、零点都不同，若帧级 SNR 的分子里混入天光，天光越亮的帧"信噪比"反而越高，Phase2 的逆方差权重就会被系统性带偏；若各帧的参考通量 `F_ref` 不独立锚定，跨帧权重之间就不可比。因此创新点二的核心不是"算一个 SNR 数字"，而是**定义一条从单帧到叠加、再到 Phase3 的、口径唯一且可核验的传递链**。

### 1.2 本报告要回答的三个问题

1. **猜想是否成立**：`SNR=F_signal/σ_F`、天光只进噪声项，是否在真值已知的物理仿真下成立？成立到什么统计强度？
2. **在什么条件下成立**：适用域边界在哪（口径选择、`F_ref` 锚定范围、σ_sky 语义、失效域）？
3. **工程上怎么做**：入库什么量、在哪一步换算权重、失败时怎么 fail-closed、有哪些必须避免的退化判据？

### 1.3 方法与证据的三重结构

实验单元按最高设计 §12.2 要求配齐三类数据（§3.1），并配三重佐证：论文 DOI/arXiv、开源实现文件:行、仓内实测（`results/evidence_web.json` 记录外部链接可解析性；Horne 1986 DOI 10.1086/131801、Zackay & Ofek 2017 DOI 10.3847/1538-4357/836/2/187 与 arXiv:1512.06872/1512.06879 均核验可解析）。本报告只复述**可核验**的引用，不可核验者显式标注（§7）。

---

## 2. 方法

### 2.1 帧级绝对 SNR 的定义主线

推导主线（只保留必要步骤）。设点源在帧内的归一化轮廓为 `P_i`（`Σ_i P_i = 1`），第 `i` 个像素的噪声方差（ADU²，`F` 为 ADU 域总通量、`g` 为 e⁻/ADU）：

```text
σ_i² = σ_sky,i² + (RN/g)² + F·P_i/g            (1)  逐像素噪声：天光/暗流散粒 + 读出 + 源泊松
σ_F⁻² = Σ_i P_i² / σ_i²                        (2)  Horne 1986 最优提取的方差（匹配滤波）
SNR_frame = F_signal / σ_F                     (3)  帧级绝对 SNR（F_signal 已扣局部背景）
```

由 (2)(3) 直接得到本创新点的两条可判据性质：

- **天空受限极限**（源泊松项不主导）：`σ_F → σ_pix/√(Σ_i P_i²)`，即噪声等效面积 `A_NEA = 1/ΣP_i²` 的形式；
- **天光主导极限**：`σ_F² ≈ (Σ_i P_i²)·σ_sky² ∝ B` ⇒ `SNR ∝ B^(−1/2)`，log-log 斜率 **−1/2**，`B→∞` 时 `SNR→0`。

这两条正是判据的阈值来源：斜率门取理论 −1/2 的 ±15% 带，趋零门取 `SNR(10⁶)/SNR(0) < 3%`。

**关键点**：天光只出现在 (1) 的噪声项，**绝不进分子**。分子 `F_signal` 由独立局部背景扣除得到（背景环带 `r∈[10,30]` px，`N_sky=2600`；本单元实际用 2516 个合格像素）。

### 2.2 σ_sky 的语义必须显式声明（D1 的规范面）

式 (1) 要求 `σ_sky` 只承载**一种**语义。冻结口径（`docs/plugins/algorithms_phase1/07_noise_snr.md` §4.2a）：

| `sigma_sky_source` | 含义 | 是否再加 `(RN/g)²` |
|---|---|---|
| `shot_noise_only` | 天光 + 暗流的**散粒**方差（不含读噪） | **加**（需 `gain>0 && read_noise_e>0`） |
| `empirical_total_rms` | 经验**总** rms（含读噪，如 `noise_model` 空天稳健尺度） | **不加**（已含读噪） |
| （gain≤0，PSF 行路径） | gain 未知 | 不加（provenance=3） |

规范另要求：声明与实际来源不一致 ⇒ **fail-closed 拒绝**，不得静默择一。此条的实现状态见 §5.3-C5（属未落地条款，已如实登记）。

### 2.3 跨帧可比性：F_ref 逐帧锚定与权重现场换算

帧间可比靠**同一星等参考电平**：`F_ref,k = 10^(−0.4(m_ref − ZP_k))`，`m_ref = 6.0`，**逐帧独立**、**必须同帧配对**（定义 `F_ref` 与换算 `F_ref` 必须同源）。由此定义帧权重：

```text
w_k = SNR_k(F_ref)² / F_ref,k² ≡ 1/σ_F,k²        (4)
SNR_combined² = Σ_k SNR_k²                       (5)
Var(F̂) = 1 / Σ_k W_k   （Q/W 点源信息量形式）     (6)
```

式 (4) 是"权重不入库、Phase2 现场换算"这一产品决定的数学依据：入库的是未加权原始 SNR，权重在消费时派生。式 (5) 是独立帧逆方差组合的等价表述（`SNR²` 可加）。式 (6) 是 Q/W 形式的点源信息量。

### 2.4 传递到 Phase3：协方差与输出 PSF

重采样把输入协方差按线性算子传播，点源信息量必须按**输出 PSF**与**完整输出协方差**重算：

```text
C_out = R · C_in · Rᵀ                            (7)
Var(F̂_out) = 1 / (P_outᵀ · C_out⁻¹ · P_out)      (8)
```

只取对角（`Σ_k c_k² u_k`）会丢弃相关噪声贡献，必须量化其低估幅度。

### 2.5 三条 SNR 口径与精度度量

| 口径 | 构造 | 用途 |
|---|---|---|
| `dense` | 逐 32 px patch 稳健 MAD σ 场（1.4826×MAD + 5σ×2 裁剪） | 精度基准；存储 67.11 MB/帧（4096²） |
| `sparse_reconstruct`(Δ) | Δ×Δ cell 稳健 MAD → 规则网格双线性重建 | **默认**（Δ=64，复用 UPM 8×8/tile 控制网格） |
| `frame_reconstruct` | 帧级标量（patch σ 场中位数） | 单帧级对照；另设"整帧未裁剪 MAD"错误构造作对照 |

评价量：`RMSE(log10 ρ)`（两侧中位归一）、`level_bias = median log10(σ̂/σ_true)`、存储字节/帧、重建时间，以及**权重效率损失**

```text
E = Var_w / Var_opt − 1        （E=0 ⇔ σ̂ ∝ σ_true；全局尺度相消）   (9)
```

真实数据面用 **1 px 棋盘 hold-out**（`fam=(y+x)%2`，每个 32×32 评价 cell 内恰好 512 个真值像素）：估计量只用族 0 像素、真值只用族 1 像素（零像素重叠）；真值自身噪声 `eps_ref = 1.166/ln10/√512 = 0.0224 dex`（Rousseeuw–Croux 渐近式），并报告扣除后的 RMSE。

### 2.6 判据非退化设计（本单元最重要的方法学贡献）

- **真值无效应 ⇒ 度量归零**（代数恒等，只作回归哨兵，**不计入非退化证据**）：算术常数臂 ΔSNR≡0；平坦 σ 场时帧级臂 RMSE≡0；等杠杆拟合时堆叠权重损失≡0；ZP 散度=0 时 `F_ref` 错配损失≡0；`σ̂∝σ_true` 或整体乘常数时 E≡0（尺度相消）。
- **双向可假（真正的非退化证据）**：故障注入必须翻红——天光泄漏进信号项 `β=1e-3`（0.1% 天光）、中位分半 ±10% 乘性偏差、打乱场、静默降级。
- **缺数据不得当阴性**：任何 (面,Δ) 臂缺失 ⇒ `skipped=true` 显式记录并由硬门 `N2_no_arm_skipped` 判红；`delta_star.status ∈ {crossed, no_crossing, not_computed}`，`not_computed` 由 `N3_delta_star_all_computed` 判红。
- **门的证据等级分类**：定义型/代数恒等（哨兵）、元证据（刻意展示恒真性）、规范转写自检（非实现对拍）、MC 收敛检查、恒真门（改名 `DEGENERATE_*`，不作证据）、携带数据信息的判据（其余全部）。

---

## 3. 实验设计

### 3.1 三类数据（最高设计 §12.2）

1. **HST 真实信号模板 + 完整物理前向仿真**：`testdata/HST_M16/…_m16_f657n_v1_drz.fits`（只读），2048² 中心裁剪，缩放到中位 200 e⁻ 后加天光 200 e⁻/px、暗流、读出噪声与增益；真值 σ 逐像素解析已知（含源泊松项）。
2. **纯解析代数合成**：SE(ℓ) 高斯随机场调制的 σ 场 `σ=10^(s·g)`（ℓ、s 由构造已知）+ 异方差高斯噪声；`s=0` 为"真值无空间效应"负例。B1/B2/B4 用电子域 Poisson + 高斯读噪 + 可选量化的合成帧。
3. **testdata 真实数据**：M42 T2/M1·M2·M4 Red 300 s 三帧（只读），2048² 中心裁剪，1 px 棋盘 hold-out 真值。

数据指针与只读声明见 `实验/absolute-snr/data/README.md`；缺件时脚本显式失败（fail-closed），不静默降级、不自动下载。

### 3.2 冻结参数与统计量

| 项 | 值 | 来源 |
|---|---|---|
| 增益 / 读出 / 暗流 / PSF σ | `g=1.3` e⁻/ADU、`RN=10` e⁻、`D=0.5` e⁻/px、`σ_PSF=1.5` px（Moffat4 β=4，FWHM=1.230310σ=1.845 px） | `b1.frozen_config` |
| stamp / 背景环带 | 61×61、`r∈[10,30]` px（`n_sky=2516` 合格像素） | `b1.frozen_config` |
| 天光扫描 | B = 0 … 10⁶ e⁻/px（13 档）× 亮源 F=3000 e⁻ / 暗源 F=100 e⁻ | `b1.frozen_config` |
| 噪声项扫描 | RN 0–50、暗流 0–1000、PSF σ 0.8–5、增益 0.5–4、源强 10–10⁴ e⁻（共 29 点） | `b2.frozen_config` |
| MC 帧数 | B1/B2：1000；B4：4000；B5：2000 | 各 `frozen_config.n_mc` |
| 三口径网格 | Δ ∈ {16,32,64,128,256,512}；合成 ℓ ∈ {16…256} × s ∈ {0,0.03,0.10,0.30}（20 面） | `b3.frozen_config` |
| 存储预算 | 1 MiB/帧（`budget_bytes=1048576`） | `b3.frozen_config` |
| seed | `20260921`（全部单元；无时间/环境相关随机源） | 各 `frozen_config.seed_base` |

**声明**：上述坐标（g、RN 等）是**声明量**，取自同口径的先行实验，不代表任何真实相机；本单元不做物理闭合反推（符合最高设计 §2.1）。

### 3.3 扫描 / 注入 / 负例矩阵

- **天光扫描**：定义式 + MC 真值 + 三条生产口径臂（`skyonly+RN`、`empirical+RN`、`empirical only`）+ 传统口径臂 + 不扣背景臂 + 算术常数臂；
- **噪声项扫描**：29 点，逐点对拍定义式、MC 真值、两条生产臂与闭式双计预言；
- **生产 C++ 对拍**：`code/prod_snr_driver.cpp` 只读链接 `lib/algorithms/noise_snr/cpp/src/snr_science.cpp`，40 个随机参数点，容差 1e-12；
- **故障注入**：天光泄漏系数 `β ∈ {0,1e-4,1e-3,3e-3,1e-2,3e-2,1e-1}`；
- **背景偏差边界**：`δB` 扫描 + 线性天光梯度 + 环带中心偏移 2 px；
- **集成/传递对拍**：ΣSNR²、ivar/等权/SNR 权重、Q/W、PSF 失配、拟合权重 vs 堆叠权重、`F_ref` 锚定与 ZP 散度错配、`C_out` 与对角近似；
- **退化门审查**：三种真值场（平滑 GRF、对抗打乱、平坦）+ 恒真门演示 + E 判据双向用例 + fail-closed 规范转写用例。

### 3.4 阈值来源与判定规则（阈值唯一来源 = 解析误差预算或 MC 置信区间）

- 定义式 vs MC：`|z| ≤ 3`（z 用 χ² 的 std 标准误），并要求 95% CI 覆盖率 ≥0.90（多重比较下允许 ≤2/26 点出界）；
- 生产臂 vs MC：`z = (mean(σ̂_arm) − σ_F,mc)/√(SE_arm² + SE_mc²)`；
- 单调性：秩相关 + 相邻 95% CI + 大天光段显著性；
- 斜率：理论 −1/2 的 ±15% 带；
- 趋零：`SNR(10⁶)/SNR(0) < 3%`；
- 效率损失：`E=0` 为绿、注入偏差必须红（双向可假）；
- 存储：1 MiB/帧预算。

---

## 4. 结果

### 4.1 猜想一：SNR 不含天光（核心）

![图 1 天光扫描：定义式、MC 95% CI、修复前双计臂与传统口径](results/figs/fig1_sky_scan.png)

**图 1**（`results/figs/fig1_sky_scan.png`，由 `code/make_figures.py` 从 `b1_sky_scan.json` 生成）：左亮源 F=3000 e⁻、右暗源 F=100 e⁻；黑实线 = 定义式，灰带 = MC 95% CI，红虚线 = 修复前"经验 σ + RN"双计臂（本报告 §4.3 的负例），橙点线 = 传统"信号含天光"口径。

**表 1 天光扫描关键点（亮源 F=3000 e⁻；MC 真值 N=1000 帧）**

| B [e⁻/px] | SNR_def | SNR_MC（95% CI） | 生产 skyonly+RN | 修复前 empirical+RN（双计） | 传统口径 |
|---|---|---|---|---|---|
| 0 | 43.3196 | 43.2069 [41.312, 45.101] | 43.3369 | 38.4674 | 38.9 |
| 10³ | 23.5136 | 22.9502 [21.944, 23.956] | 23.5187 | 22.7643 | 105.9 |
| 10⁵ | 2.8805 | 2.9549 [2.825, 3.084] | 2.8804 | 2.8784 | 987.1 |
| 10⁶ | 0.9131 | 0.9082 [0.868, 0.948] | 0.9131 | 0.9148 | 3123.1 |

*表 1 数据源：`results/b1_sky_scan.json:sky_scan_bright[0,7,11,12]`（字段 `snr_def`/`snr_emp`/`snr_emp_ci95`/`snr_prod_skyonly_rn`/`snr_prod_empirical_rn`/`snr_arm_trad`）。*

- **单调下降**：定义式严格单调；MC 秩相关 **ρ=−0.956（亮，p=3.4e-7）/ −0.9945（暗，p=3.9e-12）**；B≥1000 段相邻 95% CI 不重叠。
- **趋零与斜率**：天光主导段 log-log 斜率 **−0.4879（亮）/ −0.4972（暗）**，落在 −1/2 的 15% 带内；`SNR(10⁶)/SNR(0) = 2.108% / 1.071%`。
- **真值一致性**：定义式与 MC 最大相对偏差 4.41%（亮）/ 4.18%（暗）；max|z| = **2.06（亮）/ 1.79（暗）**；95% CI 覆盖 **12/13（亮）+ 13/13（暗）= 25/26**，满足 ≥0.90 门（注：README 写作"24/26"，与 JSON 的 12/13 不符，见 §5.3-C3）。
- **负例 1（算术常数 +200 ADU、无散粒噪声）**：ΔSNR 最大 **6.7e-16**（机器零）——真值无效应 ⇒ 度量归零。**该臂判别力≈0**（对"扣局部背景"类估计量是解析恒等变换），只作回归哨兵。
- **负例 2（传统"信号含天光"）**：单调上升，B=10⁶ 时相对真值 **×3419.4（亮）/ ×1.025×10⁵（暗）**；不扣背景的帧级臂 **×8040.1（亮）/ ×2.409×10⁵（暗）**。
- **故障注入**：天光泄漏进信号项 `F_signal += β·b̂·n_pix`，**β=1e-3（0.1% 天光）即让单调性门翻红**（`G7_leak_first_red_beta=0.001`，`G7_gate_has_teeth=true`）⇒ 门有牙齿。
- **背景估计偏差边界**：灵敏度 `dF̂/dδB = 10.7893 = 1/ΣP²`；**1% SNR 偏差对应 δB* = 2.78 e⁻**（= B=1000 e⁻/px 的 0.28%）；对称环带对**线性天光梯度**一阶不敏感（`grad_star_1pct` 在中心对称构型下未触 1%，JSON 记为 `NaN`），环带中心偏 2 px 时 `g* = 2.01 e⁻/px²`。

**判定**：`ACCEPTANCE_SPEC` §2.2 第 1 行（SNR 不含天光信号）**PASS**。

### 4.2 定义式对 MC 真值与生产 C++ 对拍

- **29 点噪声项扫描**：定义式 vs MC `max|z| = 2.6808`（全 ≤3σ）；"天光散粒 + RN"臂 `max|z| = 2.6986`（全 ≤3σ）；含源泊松项的总口径 `max|z| = 2.7107`。
- **天空受限极限**：源泊松项不主导（dominance<5%）的 2 个点上，`σ_F` 与 `σ_pix/√ΣP²` 最大相对差 **1.08%**。
- **生产 C++ 逐点对拍**：40 个随机参数点上 `snr_optimal`/`σ_F`/孔径 SNR/`ΣP²`/`f_in` 与 Python 镜像最大相对差 **2.24e-14**（容差 1e-12）⇒ 镜像与生产实现逐位一致。
- **修复前双计臂（负例）**：见 §4.3。

**判定**：`ACCEPTANCE_SPEC` §2.2 第 2 行（真值一致性）**PASS**。

### 4.3 D1：σ_sky 读噪双计——发现 → 修复 → 修复后结果

这是本单元**唯一一处触及生产码的真实缺陷**，必须完整呈现三段。

**(a) 发现（缺陷形态与量化）**

- **设计意图**：`σ_i² = σ_sky,散粒² + (RN/g)² + F·P_i/g`（读噪只出现一次）。
- **调用点缺陷**：`module_adapters.cpp` 把**经验空天总 rms**（`noise_sigma`，1.4826×MAD，**含读噪**）填入 `cfg.sigma_sky_adu`，同时把 `snr.gain_e_per_adu`/`snr.read_noise_e` 配置进去；`snr_science.cpp` 的 gain>0 分支据此**再加一次** `(RN/g)²`。
- **实测**（`b2_noise_terms.json`，N_MC=1000）：基准点（F=1000 e⁻、B=100 e⁻/px、RN=10 e⁻、g=1.3、D=0.5）σ_F 高估 **+12.8%**；RN=50 e⁻ 最坏点 **+34.0%**；B1 暗源 B=0 最坏 **+36.6%**；天光主导点（B≥10⁵）偏差 <1%（亮 −0.72%、暗 −1.29%）。帧级实证：B=0 时双计臂 SNR=38.47 vs 定义式/真值 43.32/43.21（**−11.2%**）。
- **闭式预言正确性**：预言 `σ_F(emp+RN)/σ_F(correct)−1` 与实测**臂比值**在 29 点上最大差 **1.25 pp** ⇒ 偏差机制被解析刻画，不是随机涨落。

**（本轮新增的独立复核，诚实边界）**：`b2` 中同一配置（F=1000、B=100、RN=10、g=1.3、D=0.5、σ_PSF=1.5）出现两行（`read_noise_e=10` 与 `source_flux_e=1000`），两条**臂**几乎相同（emp 臂 52.7022 vs 52.6900，skyonly 臂 46.0281 vs 46.0325），臂比值 −1 分别为 14.500% 与 14.463%，均与闭式预言 14.501% 吻合到 0.04 pp；但**对 MC 真值的偏置**分别是 +15.92% 与 +12.79%——差异来自分母 `sigma_f_mc`（45.4637 vs 46.7146，2.8%）。即：**"+12.8%/+34.0%"这类对 MC 真值的偏置带有约 ±3 pp 的 MC 噪声**（N=1000 时 σ_F 估计量的相对标准误 ≈2.2%），而"臂比值 vs 闭式预言"几乎无噪（≤1.25 pp）。结论方向不变、量级不变，但引用具体偏置数字时应说明这一不确定度。

**(b) 修复（SCI-501 / FIX-407，commit `cdcc0975`）**

- `SnrSourceParams.sigma_sky_source` 显式语义枚举：`SNR_SIGMA_SKY_UNSPECIFIED=0`（legacy 缺省，等价 SHOT_ONLY 组合，保持既有调用点逐位不变）、`SHOT_ONLY=1`、`EMPIRICAL_TOTAL_RMS=2`；
- 生产调用点 `module_adapters.cpp` 对 `noise_sigma`（经验总 rms）**显式声明** `EMPIRICAL_TOTAL_RMS`；
- `snr_science.cpp` 据此**不再**叠加 `(RN/g)²`；`SnrSourceResult.sigma_sky_source_effective` 落 provenance（1=已加 / 2=未加 / 3=gain≤0）；
- 口径正本落 `07_noise_snr.md` §4.2a、`NOISE_MODEL.md` §9a、`CONTROL_WEIGHT_SNR.md` §8a-5。

**(c) 修复后结果（保护测试双向可假）**

**表 2 D1 修复后的保护测试与量化**

| 项 | 值 | 证据 |
|---|---|---|
| 正确口径臂 vs 独立 MC 真值 | `zA = 1.266`（≤3σ）**绿** | `p1snr_science_skysource`（本轮实跑，13/13 checks passed） |
| 双计臂 vs 独立 MC 真值 | `zB = 25.563`（>3σ）**红**（预期） | 同上 |
| legacy 缺省 vs 双计臂 | **逐位一致**（`memcmp` 相等）⇒ 向后兼容锁 | `p1snr_science_test.cpp` 用例③ |
| 双计膨胀实测锚值 | `infl = +19.02%`（RN=50 e⁻、g=1.3、B=100 e⁻/px、F=1e4 ADU） | 本轮实跑输出；锚值注释见测试源码 |
| 修复前对 MC 真值偏置 | +12.8%（基准点）~ +34.0%（RN=50）~ +36.6%（B1 暗源 B=0） | `b2_noise_terms.json:gates.G4c_*`、`b1_sky_scan.json:gates_faint.G4c_*` |
| 仓内 `p1snr_science` 全套 | **6/6 passed**（本轮只读实跑，0.81 s） | `ctest -R p1snr_science` |

*注：`infl=+19.0%` 小于纯读噪粗估 `√((B+2RN²)/(B+RN²))=1.40`（+40%），因为该点含源泊松项（F=1e4 ADU），读噪在总方差中的占比被稀释；这正是"双计效应随天光/源强升高而消失"的同一机制。*

**判定**：D1 **发现→修复→修复后**闭环；生产行为已按冻结口径修正，并有双向可假的负例保护。**诚实说明**：`results/b1..b6*.json` 中的 `prod empirical+RN` 臂是**修复前口径**的负例臂（`FINDING_doublecount_*` 为红才是预期），**不代表当前生产行为**。

### 4.4 三口径适用域与存储代价

![图 2 三口径适用域与存储代价](results/figs/fig2_domain_map.png)

**图 2**（`results/figs/fig2_domain_map.png`）：(a) 纯合成面上 sparse vs frame 的 `RMSE(log10 ρ)` 随 `Δ/ℓ` 变化（按 `s_field` 分组）；(b) 真实两域 Δ=64 三口径柱状对比；(c) 存储代价与 1 MiB/帧预算线。

**表 3 Δ=64 三口径实测（`RMSE(log10 ρ)`；真实数据为 1 px 棋盘 hold-out 值，括号为扣除真值噪声 0.0224 dex 后）**

| 面 | ℓ_meas [px] | s_field | dense | sparse(Δ=64) | frame_median | Δ* [px] |
|---|---|---|---|---|---|---|
| M42 M1（地面） | 23.4 | 0.0484 | **0.0372** | 0.0440 (0.0379) | 0.0532 | 256 |
| M42 M2（地面） | 214.3 | 0.1549 | **0.0345** | 0.0825 (0.0794) | 0.1691 | 256 |
| M42 M4（地面） | 28.0 | 0.0502 | **0.0367** | 0.0413 (0.0347) | 0.0506 | 512 |
| HST M16（高对比） | 32.3 | 0.1131 | 0.0940 | 0.1174 (0.1152) | **0.0538** | 16 |
| 合成 GRF ℓ=16 s=0.30 | 26.6 | 0.2314 | **0.0791** | 0.1899 (0.1885) | 0.2307 | — |
| 合成 GRF ℓ=64 s=0.30 | 82.5 | 0.2938 | **0.0190** | 0.0815 (0.0783) | 0.2934 | — |
| 合成 GRF ℓ=256 s=0.30 | 267.1 | 0.3000 | **0.0161** | 0.0097 (0.0000) | 0.2998 | — |

*数据源：`results/b3_domain_map.json:gates.delta64_detail`、`faces.*.delta_star`。*

- **地面视宁度受限域**：sparse(Δ=64) 三帧全部优于帧级标量（0.0413–0.0825 vs 0.0506–0.1691）；dense 最准（0.0345–0.0372），代价 67.11 MB/帧 ⇒ **默认值 Δ=64 在地面视宁度受限域成立**。
- **HST 高对比结构域**：帧级标量最好（0.0538 vs sparse 0.1174 / dense 0.0940）；sparse 的 cell 偏差随 Δ 单调恶化（+0.0194 @16 → +0.0297 @32 → +0.0582 @64 → +0.1377 @128 → +0.2626 @256 → +0.4444 dex @512），机制是**cell 稳健 MAD 被 cell 内未分辨结构抬偏**，不是插值误差 ⇒ **失效边界 Δ*≈16 px**，须给帧级标量兜底。
- **失效边界不是单一 `Δ/ℓ≈1`**：无源污染合成面上 sparse 在 Δ=512 仍全部胜出，但余量随 ℓ 变小迅速收窄——ℓ=16 → **0.6%**、ℓ=32 → 1.8%、ℓ=64 → 2.8%、ℓ=128 → 16.2%、ℓ=256 → 35.4%（s=0.30 行，Δ=512）。⇒ 边界是「**Δ/ℓ × σ 场幅度 × 未分辨结构污染**」的联合判据。
- **缺臂与缺算被硬门封死**：本轮 `N2_no_arm_skipped=true`（0 条臂被跳过）、`N3_delta_star_all_computed=true`（无 `not_computed`）；平坦场排序门已改名 `DEGENERATE_flat_field_ranking_never_true` 且**不计 PASS**。
- **存储代价（4096²/帧，float32）**：dense **67,108,864 B = 64 MiB = 67.11 MB = 1 MiB 预算的 64 倍**（稠密超门成立）；sparse Δ=64 = **16,384 B**（预算的 1.6%）；Δ=256 = 1,024 B；frame = 4 B。dense 在 2048² 裁剪下仍 16 MiB/帧。

**判定**：`ACCEPTANCE_SPEC` §2.2 第 4 行（稀疏层与重建）**PASS**。

### 4.5 逆方差集成、F_ref 锚定与 Phase3 传递

**表 4 集成与传递对拍（实测 vs 解析/预言）**

| 项 | 实测 | 解析/预言 | 相对差 | 证据 |
|---|---|---|---|---|
| `SNR_combined² = ΣSNR_k²` | — | — | **2.2e-16** | `b4.part_ab.identity_rel_dev` |
| ivar 组合方差 | 1213.08 | 1207.06 | +0.50% | `b4.part_ab.mc.var_ivar(_pred)` |
| 等权组合方差 | 8263.45 | 8149.74 | +1.40% | `var_equal(_pred)` |
| `w∝SNR` 组合方差 | 1732.58 | 1712.57 | +1.17% | `var_snr_weight(_pred)` |
| Q/W 点源 `Var=1/ΣW` | 1275.12 | 1260.41 | +1.17% | `b4.part_c.mc.var_qw(_pred)` |
| 单帧方差 | 1751.09 | 1794.26 | −2.41% | `var_single(_pred)` |
| PSF 失配惩罚 | 3304.53 | — | 2.59× 且偏差 +85.8 | `var_psf_mismatch` |
| 拟合权重 vs 堆叠权重 | 1.5226 | 1.5044 | — | `b4.part_d.naive_over_optimal_*` |
| `w_k ≡ 1/σ_F,k²` | — | — | **2.22e-16** | `b4.part_e.identity_w_vs_snr_rel_dev` |
| `C_out` 对角元 MC/解析 | 0.9980 | 1 | — | `b5.gates.H1_diag_ratio_mc_over_analytic` |
| 完整/对角方差 | 1.373 | 1.389（`1+0.75ρ_out`） | 1.2% | `b5.gates.H2_*` |
| `Var(F̂_out)` | 9.105 | 9.071 | +0.4% | `b5.gates.H3_var_full_*` |
| 对角近似"宣称/实际" | 0.3246 | — | 宣称方差仅实际 **32.5%** | `b5.gates.H3_diag_claim_over_actual` |

- **F_ref 锚定适用域**：`w_k=SNR_k(F_ref)²/F_ref,k²≡1/σ_F,k²` 恒等（2.2e-16）；锚定权重相对该源电平 oracle 权重的 scatter 比：m=4 → 0.9999、m=6 → **1.0000**、m=8 → 0.9998、m=10 → 1.0059、m=12 → **1.0362** ⇒ 在 `|m−m_ref|≤4` 内 ≤0.6%，6 等（10 倍通量）处 3.6%。
- **F_ref 错配负例**：ZP 散度 0/0.1/0.3/0.6/1.0 mag ⇒ 效率损失 0/0.56%/4.74%/15.76%/**30.18%**（散度=0 时严格归零）⇒ "逐帧独立 + 同帧配对"是**硬约束**。
- **拟合权重 ≠ 堆叠权重**：带杠杆 `h` 的拟合值方差为 `σ²h`，不能按 `1/σ²` 当独立测量堆叠——用 1/σ² 堆叠高杠杆拟合值使方差高 **52.3%**（解析 50.4%）；等杠杆设计下损失 ≡0；Huber（k=1.345, IRLS）在 5%×10σ 离群下把截距偏差从 **0.426 压到 0.084**，干净数据下两者无偏（−0.0054 vs −0.0057）。
- **Phase3 传递**：`C_out=R C_in Rᵀ`（对角元 0.9980）；只取对角使输出方差低估 **1.37 倍**，其"宣称方差"只有实际散度的 **32.5%**；用输入 PSF 忽略重采样给 6.448（低估 29%）⇒ **必须按输出 PSF + 完整 C_out 重算**。

**判定**：`ACCEPTANCE_SPEC` §2.2 第 5、6 行（逆方差集成、传递到 Phase3）**PASS**。

### 4.6 判据非退化审查与 D2（误差预算单位）

![图 3 负例与替代判据的双向可假](results/figs/fig3_negatives_gates.png)

**图 3**（`results/figs/fig3_negatives_gates.png`）：左 = B1 三条负例相对真值的偏差（算术常数恒 0、传统口径随天光爆炸、不扣背景更甚）；右 = 替代判据 `E` 的绿/红用例（无偏绿、均匀尺度归零、中位分半 ±10% 红、打乱红）。

- **恒真门确认**：`RMSE(常数场, 中位归一真值场)` 在数学上恒等于真值场 `log10` 的中位归一离散度，故 `RMSE ≤ K·s_field`（K=1.25）对**任意**真值场恒绿——平滑 GRF、**对抗打乱**、平坦三种场全部绿；对抗打乱场下帧级口径的权重效率损失达 **E=7.17**（科学上完全不可用）而门仍绿 ⇒ **该门不携带信息、不得充当证据**。
- **替代判据 E 双向可假**：无偏 E=0 绿；`σ̂∝σ_true`（纯尺度）E=0（尺度相消）；**中位分半 ±10% 乘性偏差 E=0.0274 红**；打乱场 E=0.8773 红；平坦场帧级臂 RMSE≡0 归零。**注意**：E 对**均匀**电平误差按设计不敏感（均匀 ×1.10 时 E=0），因此该用例命名为"分半乘性偏差"，不能宣称"E 能抓均匀电平错误"。
- **D2 单位审查**：`NOISE_MODEL.md` §5a 的 `1.44/√N` 是**相对**标准误（实测 N=1024 时相对 SE=0.03623，与 Rousseeuw–Croux `1.166/√N=0.03644` 一致；文档常数偏保守 ~24%）；换算到 dex 应为 `1.44/ln10/√N = 0.01954`，而 EXP-205 直接把 1.44 当 dex 常数用（0.045）⇒ **高估 2.3026 倍 = ln10**，其 τ_A 精度门与 τ_B 偏差门相应放宽 2.3 倍。**该订正已落 `NOISE_MODEL.md` §5a 与 §9a**（本轮核对：文档已含单位声明）。
- **fail-closed 规范转写自检**：稀疏层在且有效 ⇒ `sparse_reconstruct`；无稀疏层 ⇒ 按帧级执行但**显式**记 `snr_path_effective=frame_reconstruct` + 计数；稀疏层损坏 ⇒ 显式失败（`FZ-SNR-SPARSE-CORRUPT`）；dense 路径无稠密数据 ⇒ 显式失败；5/5 用例符合预期，**静默降级变体被判红**。**范围声明**：这是按 `07_noise_snr.md` §4.2 **转写**的状态机，`lib/` 实现侧目前**没有**对应符号（`grep` 无实现命中）⇒ 只证明"规范转写自洽 + 缺臂可判红"，**不构成对实现的验证**。

**判定**：`ACCEPTANCE_SPEC` §2.2 第 7 行（非退化负例）**PASS（有范围限定）**——恒真门已被识别、改名、移出 PASS 计数；fail-closed 一栏只到规范转写层级。

### 4.7 验收项总表

| 验收项（`ACCEPTANCE_SPEC` §2.2） | 结论 | 关键证据 |
|---|---|---|
| SNR 不含天光信号 | **PASS** | §4.1：ρ≤−0.956、斜率 −0.4879/−0.4972、2.11%/1.07%、传统口径 ×3419/×1.0e5、泄漏 β=1e-3 翻红 |
| 真值一致性 | **PASS** | §4.2：26+29 点全部 ≤3σ（max\|z\|=2.68）；C++ 对拍 2.24e-14 |
| 方法学对标 | **PASS** | PSFSNR（功率比）vs 通量型；PSFSW 是权重不入库（§7） |
| 稀疏层与重建 | **PASS** | §4.4：地面三帧 sparse 胜、HST 帧级胜、N2/N3 绿、存储 64× 超门 |
| 逆方差集成 | **PASS** | §4.5：ΣSNR² 2.2e-16、Q/W +1.17%、拟合/堆叠分离 52.3% vs 50.4% |
| 传递到 Phase3 | **PASS** | §4.5：C_out 0.9980、输出 PSF 重算、对角低估 1.37× |
| 非退化负例 | **PASS（范围限定）** | §4.6：恒真门识别+改名、E 双向可假、fail-closed 仅规范转写 |

---

## 5. 讨论

### 5.1 猜想在什么条件下成立

**成立的条件集（缺一不可）**：

1. **分子扣净局部背景**：不扣背景的帧级臂在 B=10⁶ 时失真 ×8040（亮）/×2.4×10⁵（暗）——比传统口径更糟。背景偏差 1% SNR 对应 `δB*=2.78 e⁻`（0.28% 天光），所以背景估计必须独立于源并足够稳健。
2. **σ_sky 语义唯一且显式**：式 (1) 要求 `σ_sky` 只承载散粒或总 rms 之一；混用即双计（D1）。此条现已由 `sigma_sky_source` 枚举 + 保护测试锁定。
3. **F_ref 逐帧独立且同帧配对**：`m_ref=6.0` 的形式外推在 `|m−m_ref|≤4` 内 ≤0.6% 误差；跨帧混用参考（ZP 散度 1 mag）即 30% 效率损失。
4. **σ_F 由最优提取给出**：`σ_F⁻²=ΣP_i²/σ_i²` 是匹配滤波的结果；用孔径/盒测光会引入 seeing 相关的假增益（见 `results/REVERSE_VERIFY_CANON.md` 的 `F_instr` 定案，属相邻主题）。
5. **口径按域选择**：默认 sparse(Δ=64) 只在地面视宁度受限域成立；HST 类高对比结构域必须给帧级标量兜底。

**不成立/失效的域**：HST 高对比结构域（Δ*≈16 px）；平坦 σ 场（真值无空间效应，任何"空间口径优于帧级"的排序判据都退化）；大 Δ/ℓ（>~10 且场幅度小时余量趋零）。

### 5.2 工程上怎么做（落地清单）

1. **入库量**：通量型**未加权原始 SNR** `F_signal/σ_F`；PSFSNR（功率比）与 PSFSW（权重）都不入库。
2. **权重换算**：Phase2 消费时现场 `w_k=SNR_k²/F_ref,k²`；入库权重 = 违反"权重不落盘"。
3. **σ_sky 契约**：调用点必须声明 `sigma_sky_source`；生产路径声明 `EMPIRICAL_TOTAL_RMS`（`noise_sigma` = 1.4826×MAD 经验总 rms），provenance 落 `sigma_sky_source_effective`。
4. **默认路径与兜底**：默认 `sparse_reconstruct`(Δ=64) + 帧级标量兜底；无稀疏层时**显式**记 `snr_path_effective`，损坏时 fail-closed。
5. **Phase3**：按 `C_out=R C_in Rᵀ` 传播，点源信息量按输出 PSF + 完整 C_out 重算；只取对角会低估 1.37 倍。
6. **判据**：空间精度判据用 `E=Var_w/Var_opt−1`；禁止使用"帧级 RMSE ≤ K·s_field"类恒真门；缺臂/缺算必须判红。

### 5.3 与正式科学文档的一致性核对（冲突清单）

本轮逐条核对 `docs/science/PSF_SIGNAL_WEIGHT.md` §7a、`docs/science/CONTROL_WEIGHT_SNR.md` §8a/§8b、`docs/plugins/algorithms_phase1/07_noise_snr.md` §4.2a、`docs/science/NOISE_MODEL.md` §5a/§9a 与本报告引用的数字。**科学结论无冲突**；以下为口径/数值/落地层面的差异，均已登记：

- **C1（路径漂移，非科学）**：`PSF_SIGNAL_WEIGHT.md` §7a 与 `CONTROL_WEIGHT_SNR.md` §8a 引用实验单元为 `实验/SCI-B/`，实际目录为 `实验/absolute-snr/`（`CONTROL_WEIGHT_SNR.md` §8b 与 `NOISE_MODEL.md` §9a 又用后者）。同一文档集内混用两种路径，建议统一（本报告统一用 `实验/absolute-snr/`）。
- **C2（数值不一致）**：§7a-7 与 §8b 写 HST cell 稳健 MAD 偏差"随 Δ 从 **+0.029 增到 +0.301** dex"；`b3_domain_map.json` 实测 sparse 臂 `level_bias_dex` 为 Δ=32 **+0.0297**、Δ=64 +0.0582、Δ=128 +0.1377、Δ=256 **+0.2626**、Δ=512 +0.4444。趋势一致（单调恶化），但 **+0.301 与 +0.127（README §3.3）无法从现行 JSON 复现**。本报告采用 JSON 值。
- **C3（数值不一致）**：§7a-2 与 `DOC_CORRECTIONS.md` D6 写 `w_k≡1/σ_F,k²` 恒等相对偏差 **1.1e-16**；`b4_integration.json:part_e.identity_w_vs_snr_rel_dev` = **2.22e-16**。两者都是机器零，但数字不同（本报告采用 JSON 值）。同类：README §3.1 写 95% CI 覆盖 "24/26"，JSON 为 12/13 + 13/13 = **25/26**。
- **C4（门名/语义，同文档内不一致）**：`CONTROL_WEIGHT_SNR.md` §8b 仍写"须走 `N1_flat_field_sparse_never_wins` 用例"，而该门在 `b3_domain_map.json` 已改名 `DEGENERATE_flat_field_ranking_never_true`、明确为恒真门且不计 PASS（与 §7a-6 的"不得充当证据"一致）。建议 §8b 同步改名并去掉"用例"措辞。
- **C5（规范严于实现，未落地条款）**：§4.2a 决策树第 4 条与 `NOISE_MODEL.md` §9a 要求"`sigma_sky_source` 缺失或与实际来源不一致 ⇒ **fail-closed 拒绝**"；实现侧保留了 legacy 缺省 `UNSPECIFIED=0`（**静默**等价 SHOT_ONLY），并由保护测试逐位锁定向后兼容；"声明与实际来源不一致"的运行时校验在 `snr_science.cpp` 内也不存在（该层拿不到"实际来源"）。**属未落地条款，应登记为 FIX 待办**，不得据此宣称已 fail-closed。
- **C6（验收措辞）**：`ACCEPTANCE_SPEC` §2.2 第 7 行写"恒真门**不出现**"；实际是"恒真门存在但已被识别、改名、移出 PASS 计数并给出替代判据"。建议把判据改写成"不得把恒真门计入证据"，与 §7a-6 一致。
- **C7（符号约定歧义，建议加单位）**：`PSF_SIGNAL_WEIGHT.md` §7a-1 写 `σ_i²=(sky+dark+RN²+F·P_i)/g²`，其中 `F` 若按同行"帧级 SNR=F_signal/σ_F"的 ADU 语义读会差一个 `g`；`07_noise_snr.md` §4.2a 写 `F·P_i/g`（`F`=ADU）。两式在"电子域 F_e"与"ADU 域 F"两种约定下等价，建议在 §7a-1 标注单位以免误读。

### 5.4 诚实边界

1. **仿真坐标是声明量**：`g=1.3`、`RN=10`、`D=0.5`、`σ_PSF=1.5` 取自同口径先行实验，不代表任何真实相机；本单元不反解物理参数。
2. **实测偏置数字带 MC 噪声**："+12.8%/+34.0%"是 N_MC=1000 下对 MC 真值的偏置，同配置两次独立实现给出 +12.79% 与 +15.92%（§4.3(a) 复核），σ_F 估计量相对标准误 ≈2.2% ⇒ 引用时应带 ±3 pp 量级不确定度；**低噪声的稳健表述是"臂比值 vs 闭式预言 ≤1.25 pp"**。
3. **真实数据面没有解析真值**：1 px 棋盘 hold-out，真值自身带 0.0224 dex 噪声；扣除后 RMSE **仍未触零**（估计量高于真值噪声地板）。
4. **ℓ 在真实面是测量量**：合成面用构造 ℓ（已知），真实面用平面去趋势 ACF 的 1/e 交叉（合成面上系统性偏高 1.3–1.6×）⇒ 真实面 `Δ/ℓ` 有该量级系统不确定度；因此边界以"**Δ*(px) + 机制**"陈述，`Δ/ℓ` 只作辅助。
5. **B5 的 `C_out=R C_in Rᵀ` 按构造成立**（`Y=R@X` ⇒ 协方差恒等式）：MC 只检验实现的采样误差，**不**验证"drizzle 的方差确实按 `R C_in Rᵀ` 传播"这一物理命题；5×5 带内 Frobenius 7.5% 由元素级 MC 噪声主导；`1+0.75ρ_out` 是启发式近邻式（门限放到 10%）。
6. **Q/W 的"公共 PSF 堆叠"对照是保守近似**：只做了"窄帧配宽 PSF"的失配惩罚，未实现真正的卷积/重采样到公共 PSF 的完整流程。
7. **D1 结论依赖调用约定**：只有在 `gain>0` 且 `sigma_sky_adu` 填经验总 rms 时才成立；若上游改填"天光+暗流散粒"则无偏差（两种口径都测过）。生产默认 gain=0 走 PSF 行路径，不受影响。
8. **fail-closed 状态机未落地**：`b6` 的 5/5 用例是**规范转写自检**，`lib/` 无对应实现（`grep` 无命中）⇒ 不得据此外推实现行为。
9. **SWarp 引用未逐行核验**：`src/coadd.c` 可解析（HTTP 200）但未定位到研究包所述行；已逐行核验的替代开源证据是 Siril 1.2.4 `median_and_mean.c:868-870` 与 photutils `errors.py:12`。
10. **未覆盖**：宇宙线/坏点/饱和、拖线源、跨波段、真实 UPM 天光面拟合（属 SCI-C）；本单元只做 SNR 传递链本身。
11. **未做更大 N_MC 的收敛性复核**：`max|z|=2.68` 接近 3σ 门，未独立提高统计量检验这些点是否只是"刚好过关"（独立审稿同样未做）。
12. **并发环境痕迹**：`p1noise_numpy_oracle` 曾因并发 FIX-405 把 `noise_model.cpp` 改到不可编译中间态而失败（非本单元回归）；**本轮实跑 `p1snr_science` 6/6 passed**，该并发红已消除。文档行号引用对 HEAD 有效，工作树并发改动会使行号漂移。
13. **s=0 合成面的 `delta_star=16` 是退化产物**：平坦场帧级臂 RMSE≡0，任何非零 sparse RMSE 都"立即交叉"，**不可读作真实失效边界**。
14. **本报告未重跑 b1–b6**（只读纪律）：数字取自现行 `results/*.json`；审稿记录中 NaN 传播缺陷修复前后的差异见 `results/REVIEW.md`（阻断项 B1）。
15. **图件未逐像素复核**：只读确认 `make_figures.py` 只读 JSON、只写 `results/figs`；三张图的坐标与数据源已按生成脚本核对。

---

## 6. 结论

1. **猜想成立**：帧级绝对 SNR `SNR=F_signal/σ_F`、`σ_F⁻²=ΣP_i²/σ_i²`（Horne 1986）在物理 MC 真值下成立（26+29 点全部 ≤3σ，max|z|=2.68）；天光只进噪声项，固定源通量抬升天光时 SNR 单调下降、天光主导段斜率 −0.4879/−0.4972（理论 −1/2）、`SNR(10⁶)/SNR(0)`=2.11%/1.07%；传统"信号含天光"口径失真 3 个数量级。
2. **成立的条件是明确的**：分子扣净局部背景、σ_sky 语义唯一显式、`F_ref` 逐帧独立且同帧配对、口径按域选择（地面 sparse(Δ=64)、HST 帧级兜底）。
3. **发现并闭环了一处真实生产缺陷**：σ_sky 读噪双计（σ_F 高估 +12.8%~+34.0%），已按 `sigma_sky_source` 显式语义修复（commit `cdcc0975`，SCI-501 / FIX-407），保护测试双向可假（zA=1.27 绿 / zB=25.6 红 / legacy 逐位一致 / infl=+19.0%），本轮实跑 `p1snr_science` 6/6 passed。
4. **判据方法学被修正**：恒真门（帧级 RMSE ≤ K·s_field）被识别、改名、移出证据；替代判据 `E` 双向可假；缺臂/缺算由硬门判红。
5. **集成与传递链对拍通过**：ΣSNR² 2.2e-16、Q/W +1.17%、拟合/堆叠权重分离（52.3% vs 50.4%）、`C_out` 0.9980、对角近似低估 1.37×。
6. **遗留**：§5.3 的 C1–C7 与 §5.4 的 15 条边界中，C5（fail-closed 未落地）与"fail-closed 状态机未实现"是**唯一需要继续推进的工程项**，其余为文档口径统一或方法学固有限制。

---

## 7. 参考文献

**一手文献（DOI/arXiv 已核验可解析；核验记录 `results/evidence_web.json`）**

1. Horne, K. 1986, *An optimal extraction algorithm for CCD spectroscopy*, PASP **98**, 609. DOI: [10.1086/131801](https://doi.org/10.1086/131801)。—— 式 (2) `σ_F⁻²=ΣP_i²/σ_i²` 的来源。
2. Zackay, B. & Ofek, E. O. 2017, *How to COAAD Images. I. Optimal Source Detection and Photometry*, ApJ **836**, 187. DOI: [10.3847/1538-4357/836/2/187](https://doi.org/10.3847/1538-4357/836/2/187)；arXiv: [1512.06872](https://arxiv.org/abs/1512.06872)。—— 多图像点源最优组合。
3. Zackay, B. & Ofek, E. O. 2017, *How to COAAD Images. II. A Coaddition Image that is Optimal for Any Purpose*, ApJ **836**, 188；arXiv: [1512.06879](https://arxiv.org/abs/1512.06879)。—— proper coadd 与相关噪声。
4. Fruchter, A. S. & Hook, R. N. 2002, *Drizzle: A Method for the Linear Reconstruction of Undersampled Images*, PASP **114**, 144. DOI: [10.1086/338393](https://doi.org/10.1086/338393)。—— `C_out=R C_in Rᵀ` 的重采样背景（**文章级/未逐页核验**：本单元只引用其思想，未逐式核对）。
5. Rousseeuw, P. J. & Croux, C. 1993, *Alternatives to the Median Absolute Deviation*, JASA **88**, 1273. DOI: [10.1080/01621459.1993.10476408](https://doi.org/10.1080/01621459.1993.10476408)。—— `1.166/√N` 渐近相对标准误（**文章级/未逐页核验**，数值由本单元 MC 独立复核为 0.0364 vs 实测 0.0362）。
6. Naylor, T. 1998, *An optimal extraction algorithm for imaging photometry*, MNRAS **296**, 339. DOI: [10.1046/j.1365-8711.1998.01314.x](https://doi.org/10.1046/j.1365-8711.1998.01314.x)。—— 成像域最优提取（**文章级/未逐页核验**，由 `PSF_SIGNAL_WEIGHT.md` §9 引用）。

**开源实现对照（项目 + 版本 + 文件:行；仅对照不复制 GPL 代码）**

7. Siril 1.2.4（GPL-3.0）：`src/stacking/median_and_mean.c:868-870`，`pweights[layer][i]=1.f/(pscale²·bgnoise²)` —— 逆方差型帧权重 + 稳健背景噪声（已核验可解析）。
8. photutils（BSD-3-Clause）：`photutils/utils/errors.py:12`，`calc_total_error(data, bkg_error, effective_gain)` —— 背景误差与源泊松误差分离，与式 (1) 同构（已核验可解析）。
9. PixInsight Reference, *New Image Weighting Algorithms*（https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html）—— 方法学对照；**PSFSNR 是功率比、PSFSW 是权重，二者都不入库**（**文章级/未逐式核验常数**）。
10. SWarp（GPL-3.0）`src/coadd.c` 的逆方差组合行：**未能在本单元核验**（HTTP 200 可解析但未定位到所述行，见 `DOC_CORRECTIONS.md` D5）—— 本报告不将其作为已核验证据。

**仓内权威文档与证据**

11. `docs/science/PSF_SIGNAL_WEIGHT.md` §7a（SCI-B 定案结论，7 条）。
12. `docs/science/CONTROL_WEIGHT_SNR.md` §8a/§8b（帧级 SNR 定义与跨帧可比性、三口径适用域图谱）。
13. `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.2a（σ_sky 口径冻结，防读噪双计）。
14. `docs/science/NOISE_MODEL.md` §5a/§9a（相对标准误单位订正；σ_sky 入参口径）。
15. `ACCEPTANCE_SPEC.md` §2.2（SCI-B 验收判据表）。
16. `实验/absolute-snr/README.md`、`results/b1..b6*.json`、`results/DOC_CORRECTIONS.md`（D1–D7）、`results/REVIEW.md`（独立审稿，239 行）、`results/COMPARISON_TABLES.md`（T1–T8，自动生成）、`results/REVERSE_VERIFY_CANON.md`。
17. 生产实现：`lib/algorithms/noise_snr/cpp/src/snr_science.cpp`（Horne 最优提取 + σ_sky 语义分支）、`lib/algorithms/noise_snr/cpp/include/snr_estimator.h`（枚举与 provenance）、`lib/infrastructure/scheduler/src/module_adapters.cpp`（调用点声明 `EMPIRICAL_TOTAL_RMS`）、`lib/algorithms/noise_snr/tests/p1noise/p1snr_science_test.cpp`（保护测试）。

---

## 附录 A. 关键数字 → 证据索引表

**表 A-0 证据文件版本锚（各 JSON 的 `generated_at`，UTC）**

| 文件 | generated_at | wall_s |
|---|---|---|
| `b1_sky_scan.json` | 2026-09-21T12:23:18Z | 29.42 |
| `b2_noise_terms.json` | 2026-09-21T12:23:36Z | 17.49 |
| `b3_domain_map.json` | 2026-09-21T12:26:40Z | 184.06 |
| `b4_integration.json` | 2026-09-21T12:27:08Z | 27.16 |
| `b5_phase3_transfer.json` | 2026-09-21T12:27:12Z | 4.32 |
| `b6_gates_audit.json` | 2026-09-21T12:27:13Z | 0.22 |

**表 A-1 数字索引（报告正文引用处 → 证据文件:字段）**

| # | 报告中的数字 | 证据文件 | 字段 / 位置 |
|---|---|---|---|
| 1 | 亮源斜率 −0.4879 | `results/b1_sky_scan.json` | `gates_bright.G2_large_sky_slope_def` = −0.4879107345007782 |
| 2 | 暗源斜率 −0.4972 | `results/b1_sky_scan.json` | `gates_faint.G2_large_sky_slope_def` = −0.49719839095455826 |
| 3 | SNR(10⁶)/SNR(0) = 2.108% / 1.071% | `results/b1_sky_scan.json` | `gates_bright.G2_snr_max_over_min` / `gates_faint.G2_snr_max_over_min` |
| 4 | max\|z\| = 2.06（26 天光点）/ 2.68（29 噪声点） | `b1` / `b2_noise_terms.json` | `b1.gates_bright.G3_max_abs_z_sigma` / `b2.gates.G1_def_max_abs_z` |
| 5 | 秩相关 −0.956 / −0.9945 | `results/b1_sky_scan.json` | `gates_bright.G1_spearman_rho_emp` / `gates_faint.G1_spearman_rho_emp` |
| 6 | 95% CI 覆盖 12/13 + 13/13 | `results/b1_sky_scan.json` | `gates_bright.G3_def_in_mc_ci95_frac`=0.9231 / `gates_faint`=1.0 |
| 7 | 传统口径 ×3419.4（亮）/ ×1.025e5（暗） | `results/b1_sky_scan.json` | `gates_bright.G6_trad_distortion_at_max_sky` / `gates_faint.G6_trad_distortion_at_max_sky` |
| 8 | 不扣背景 ×8040.1 / ×2.409e5 | `results/b1_sky_scan.json` | `gates_bright.G6_raw_frame_distortion_at_max_sky` / `gates_faint.G6_raw_frame_distortion_at_max_sky` |
| 9 | 泄漏 β=1e-3 翻红 | `results/b1_sky_scan.json` | `gates_bright.G7_leak_first_red_beta`=0.001、`G7_gate_has_teeth`=true |
| 10 | δB\*=2.78 e⁻（1% SNR）；灵敏度 10.789 | `results/b1_sky_scan.json` | `bias_boundary.delta_bg_star_1pct_e` / `bias_boundary.sensitivity_dF_dB` |
| 11 | 环带偏移 2 px 时 g\*=2.011 e⁻/px² | `results/b1_sky_scan.json` | `gradient_boundary.grad_star_1pct_e_per_px2_offset2px` |
| 12 | 算术常数负例 6.7e-16 | `results/b1_sky_scan.json` | `gates_bright.G5_arith_null_max_rel_change` |
| 13 | 双计基准点 +12.8% | `results/b2_noise_terms.json` | `gates.G4c_bias_at_base_point`=0.1279130 |
| 14 | 双计最坏 +34.0%（RN=50） | `results/b2_noise_terms.json` | `gates.G4c_max_bias_over_all_points`=0.3395416；行 `scans.read_noise_e[5].bias_empirical_rn` |
| 15 | 双计最坏 +36.6%（B1 暗源 B=0） | `results/b1_sky_scan.json` | `gates_faint.G4c_empirical_rn_max_rel_dev`=0.3661993 |
| 16 | 闭式预言 vs 臂比值 ≤1.25 pp | `results/b2_noise_terms.json` | `gates.G4c_pred_vs_arm_ratio_max_abs_diff`=0.0125009 |
| 17 | C++ 对拍 2.24e-14（40 点） | `results/b2_noise_terms.json` | `cpp_crosscheck.worst_rel_diff`=2.2392e-14、`n_points`=40、`tolerance`=1e-12 |
| 18 | 天空受限极限 1.08% | `results/b2_noise_terms.json` | `gates.G3_sky_limited_max_rel_diff`=0.0107501（2 点） |
| 19 | 修复后 zA=1.266 / zB=25.563 / infl=+19.02% | `results/DOC_CORRECTIONS.md` D1（zA/zB）+ 本轮实跑 `build/eng/tests/unit/p1noise/p1snr_science_test skysource` | 测试输出；锚值注释 `lib/algorithms/noise_snr/tests/p1noise/p1snr_science_test.cpp` |
| 20 | 仓内 `p1snr_science` 6/6 passed | 本轮实跑 `ctest --test-dir build -R p1snr_science` | 输出 "100% tests passed, 0 tests failed out of 6" |
| 21 | Δ=64 三口径 RMSE（M1/M2/M4/HST） | `results/b3_domain_map.json` | `gates.delta64_detail[*].at_delta64.{dense,sparse,frame_median}.rmse` |
| 22 | Δ\* = 256/256/512/16 px | `results/b3_domain_map.json` | `faces.testdata_m42[i].delta_star.delta_star_px`、`faces.hst_m16.delta_star.delta_star_px` |
| 23 | HST cell 偏差 +0.0297@32 → +0.2626@256 dex | `results/b3_domain_map.json` | `faces.hst_m16.rows[arm=sparse].level_bias_dex` |
| 24 | Δ=512 余量 0.6%/1.8%/2.8%/16.2%/35.4% | `results/b3_domain_map.json` | `faces.synthetic_grf[meta.s_field_true_log10=0.30].rows[delta_px=512]`（sparse vs frame_median 计算） |
| 25 | dense 67,108,864 B = 64 MiB = 64× 预算 | `results/b3_domain_map.json` | `faces.hst_m16.cost.dense_bytes_4096` / `dense_over_budget_factor`；`sparse_bytes_4096["64"]`=16384 |
| 26 | 缺臂/缺算硬门绿 | `results/b3_domain_map.json` | `gates.N2_no_arm_skipped`=true、`N3_delta_star_all_computed`=true、`DEGENERATE_flat_field_ranking_never_true` |
| 27 | ΣSNR² 恒等 2.2e-16 | `results/b4_integration.json` | `part_ab.identity_rel_dev`=2.2204e-16 |
| 28 | ivar 1213.08 vs 1207.06；等权 8263.45；w∝SNR 1732.58 | `results/b4_integration.json` | `part_ab.mc.{var_ivar,var_ivar_pred,var_equal,var_equal_pred,var_snr_weight,var_snr_weight_pred}` |
| 29 | Q/W 1275.12 vs 1260.41 | `results/b4_integration.json` | `part_c.mc.var_qw` / `var_qw_pred` |
| 30 | 拟合 vs 堆叠 1.5226 vs 1.5044；Huber 0.426→0.084 | `results/b4_integration.json` | `part_d.naive_over_optimal_measured/_pred`、`part_d.robust` |
| 31 | `w_k≡1/σ_F,k²` 2.22e-16 | `results/b4_integration.json` | `part_e.identity_w_vs_snr_rel_dev` |
| 32 | F_ref 锚定 0.9999/1.0000/0.9998/1.0059/1.0362 | `results/b4_integration.json` | `part_e.mag_rows[*].anchored_over_oracle` |
| 33 | ZP 散度 1.0 mag ⇒ E=30.18% | `results/b4_integration.json` | `part_e.mismatch_negative[*].eff_loss` |
| 34 | `C_out` 对角比 0.9980；完整/对角 1.373 vs 1.389；Var 9.105 vs 9.071；宣称/实际 0.3246 | `results/b5_phase3_transfer.json` | `gates.H1_diag_ratio_mc_over_analytic`、`H2_full_over_diag_measured`、`H2_pred_1_plus_0p75rho`、`H3_var_full_mc`、`H3_var_full_pred`、`H3_diag_claim_over_actual` |
| 35 | 恒真门：对抗场 E=7.17 仍绿 | `results/b6_gates_audit.json` | `tautology.rows[1].eff_loss`=7.16695、`tautology.all_green`=true |
| 36 | E 双向：0.0274 红 / 0.8773 红 / 均匀尺度 0 | `results/b6_gates_audit.json` | `eff_loss_injection[*]` |
| 37 | c_est 高估 2.3026 = ln10 | `results/b6_gates_audit.json` | `c_est_audit.e11_over_correct_factor`、`gates.H3_factor` |
| 38 | fail-closed 5/5 + 静默降级判红 | `results/b6_gates_audit.json` | `fail_closed.all_match`、`gates.H4_silent_fallback_detected`、`gates.H4_scope_note` |
| 39 | 冻结参数（g/RN/D/σ_PSF/环带/N_MC/seed） | 各 `results/*.json` | `frozen_config`（b1/b2/b3/b4/b5） |
| 40 | 独立审稿的阻断与修正闭环 | `results/REVIEW.md` §3/§5；`实验/absolute-snr/README.md` §9 | 审稿意见 B1、I1–I6、M1–M12 与修正对照表 |

---

## 附录 B. 图表清单与复现命令

**图表清单（3 图 + 4 表 = 7 件）**

| 编号 | 类型 | 内容 | 文件/来源 |
|---|---|---|---|
| 图 1 | 图 | 天光扫描（亮/暗源，定义式 + MC CI + 双计臂 + 传统口径） | `results/figs/fig1_sky_scan.png` |
| 图 2 | 图 | 三口径适用域（合成 Δ/ℓ、真实两域 Δ=64、存储代价） | `results/figs/fig2_domain_map.png` |
| 图 3 | 图 | 负例与替代判据 E 的双向可假 | `results/figs/fig3_negatives_gates.png` |
| 表 1 | 表 | 天光扫描关键点 | `b1_sky_scan.json:sky_scan_bright` |
| 表 2 | 表 | D1 修复后的保护测试与量化 | `DOC_CORRECTIONS.md` D1 + 本轮实跑 |
| 表 3 | 表 | Δ=64 三口径 RMSE 与 Δ\* | `b3_domain_map.json:gates.delta64_detail` |
| 表 4 | 表 | 集成与传递对拍 | `b4/b5*.json` |

**复现命令（只读；写目标仅 `results/` 与 `run/`）**

```bash
# 一键复跑（固定 seed；构建/测试串行加锁；约 25–35 min）
bash 实验/absolute-snr/code/run_all.sh

# 分步
python3 实验/absolute-snr/code/b1_sky_scan.py      # 天光扫描 + 核心负例
python3 实验/absolute-snr/code/b2_noise_terms.py   # 噪声项组成 + C++ 对拍
python3 实验/absolute-snr/code/b3_domain_map.py    # 三口径适用域图谱
python3 实验/absolute-snr/code/b4_integration.py   # 逆方差集成对拍
python3 实验/absolute-snr/code/b5_phase3_transfer.py
python3 实验/absolute-snr/code/b6_gates_audit.py   # 退化门审查 + fail-closed
python3 实验/absolute-snr/code/b6_gates_audit.py --self-test
python3 实验/absolute-snr/code/make_figures.py
python3 实验/absolute-snr/code/make_tables.py

# 仓内测试（串行加锁）
flock /tmp/astrocs_build.lock ctest --test-dir build --output-on-failure -R "p1snr_science"
```

**本报告新增的只读复核（不写入仓库）**

```bash
# 保护测试双向可假（本轮实跑，13/13 checks passed）
./build/eng/tests/unit/p1noise/p1snr_science_test skysource
# → sig_emp=39.223227 ADU  sigma_F(A)=134.892160  sigma_F(B)=160.546050  MC=133.555082  zA=1.266  zB=25.563  infl=19.02%

# 全套 p1snr_science（本轮实跑，6/6 passed，0.81 s）
ctest --test-dir build -R p1snr_science
```
