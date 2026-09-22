# Control-Weight SNR / Frame Quality Science (SCI-CW)

> 上游：ASTROCS_DESIGN.md §2.2（创新点二：跨帧绝对信噪比）、§5.3（SNR 重建与逆方差叠加）

> ID: SCI-CW-001..008  状态: FROZEN (2026-08-27, G3 SCI-002 补冻)  上游: SCI-SCOPE-001,
> SCI-NOISE (逐像素 σ/variance/ivar)  下游 ALG: ALG-CW-001..  模块: `phase2` sampler/
> stage2 控制权重 (local_snr_map, frame_snr_medians, quality)

> 补齐 SCI-002 要求的 `local_snr` 与 `frame quality` 权威定义（此前仅散落在
> code + `docs/architecture/execution_inventory.csv`，无 science authority）。

> **重定义注记**：
> **本文件（Phase2 stage2 内部）**所称 `local_snr` / `frame_snr` 实为**相对质量权重场**（`quality_weight =
> frame_quality_scalar × local_quality_proxy/median`），**不是科学信噪比**；科学 SNR 由
> 逐源 `σ_F` 定义，帧级科学基准为 5σ 点源深度 `m_5`（§2a）。字段名 `snr` 仅为兼容保留，
> 其语义为 "SNR-equivalent relative quality, not a calibrated signal-to-noise ratio"。
>
> **同名两义分离（负责人 2026-09-19 裁决 A1/C1，claim `FIX-SCI-SNR-CANON-001`；关闭 §9 的 UNRESOLVED）**：
> **Phase1 HiPS 文件头的 `frame_snr` 是科学量**——帧级未加权原始信噪比（**点源（PSF）信号 SNR，纯信号/噪声**：
> `SNR = F_signal/σ_F`，`F_signal` 已扣局部背景、天光**只作噪声项**进 `σ_F`；固定源通量下天光增大 ⇒ SNR 单调下降），
> 定义与红线见 `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1 与 `docs/design/UNIFIED_MODEL.md` §2。
> **本文件的 `local_snr`/`frame_snr` 是 stage2 内部相对质量场**，与上述科学量**禁止同名互指**；
> stage2 侧字段改名 `quality_weight`（`snr_v` 为别名）登记为实现跟随项。

## 1 目的与非目标

- **目的**：定义 phase2 控制采样/加权积分所用的**相对质量权重场**（`quality_weight =
  frame_quality_scalar × local_quality_proxy/median`）与**帧/星点质量**，作为
  `support × snr²` 控制权重中的质量权重因子；该权重场**不是科学信噪比**——其数值来源为
  帧级定标散度与逐星拟合质量代理，科学 SNR 由逐源 `σ_F`（PSF 拟合协方差或 CCD 方程）
  定义，帧级科学基准为 5σ 点源深度 `m_5`（§2a）。与 SCI-NOISE 的逐像素 `variance/ivar`
  （随机噪声倒数权重）**区隔**，二者量纲语义不同、不混用。
- **非目标**：不定义测光零点/PSF/astrometry 质量（SCI-PHOT/SCI-PSF/SCI-WCS）；不生产
  完整质量评分（仅相位/星点目录质量位）；不替代 SCI-INT 的 `support` canonica reducer。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `local_snr` | 区域级局部**相对质量权重**（有局部星点可得时；**不是**科学信噪比） | `stage2.cpp:383-396`（`local_snr_map`） |
| `frame_snr`（stage2 内部，建议改名 `quality_weight`） | 整帧 Phase1 SNR 目录值的**中位数（回退质量基准）**（**不是**科学信噪比；Phase1 HiPS 文件头的同名 `frame_snr` 是科学量，见 §0 注记与 §2a） | `stage2.cpp:74,250,402`（`frame_snr_medians`） |
| `quality_weight` | `frame_quality_scalar × local_quality_proxy/median`，无量纲相对质量权重（S4 重定义的规范名；`snr_v` 为其别名） | 本文件 §4 |
| `m_5` | 5σ 点源深度 `ZP − 2.5·log10(5·sigma_F(ref))`（唯一帧级科学基准，单位 mag） | 本文件 §2a |
| `sigma_F` | 逐源通量不确定度（科学 SNR 定义量；PSF 拟合协方差或 CCD 方程，当前实现不产出） | 消费侧定义；本文件 §2a 登记边界 |
| snr_available | 该控制观测是否含真实可用的局部质量权重（字段名沿用 `snr`） | `stage2.cpp:387` |
| `frame quality` | 每星点 Phase1 SNR 目录质量位（uint32 位掩码） | `sampler.cpp:152,266`（`quality`, `out_qual`） |
| `kSnrCatalogMax` | SNR 目录质量槽上限 | `sampler.cpp:585` |
| `w_snr` | 质量权重因子 `= snr_v²` | 积分/排异权重 |

## 2a 帧级科学基准与 SNR 定义

- **逐源科学 SNR**：由逐源通量不确定度 `σ_F` 定义，`SNR_F = F/σ_F`（PSF 加权最优提取，
  或 CCD 方程 + 孔径改正）。当前实现**不产出** `σ_F`，故本合同不把任何现有标量称为科学 SNR。
- **帧级科学基准**：唯一允许的帧级科学基准是 **5σ 点源深度**
  `m_5 = ZP − 2.5·log10(5·σ_F(ref))`（单位 **mag**）。`σ_F(ref)` 必须**显式绑定**参考轮廓/
  孔径/背景（PSF 或孔径、背景估计域、像素标度）；空间变化时应给**深度图**而非单标量。
- **质量权重场定位**：本文件 §4 的 `quality_weight`（`local_snr`/`frame_snr`）为**相对质量
  权重场**，**无量纲**，仅供采样/加权（`support × snr_v²`），不得解释为 `m_5` 或 `SNR_F`。
- **Phase1 产品面帧级 SNR（同名不同物）**：Phase1 HiPS 文件头的 `frame_snr` 是**科学量**（点源 PSF 信号 SNR，纯信号/噪声，§0 注记），不属于本文件的相对质量权重场；本文件「帧级科学基准 = `m_5`」不改变其定义，二者不得互指。
- 推导与文献锚见 `run/release-rescue/science-phot/PHOTOMETRY_LITERATURE_REVIEW.md` §C.3.1
  （Horne 1986 最优提取；5σ 深度 Tonry et al. 2012 / Huang et al. 2017 / Ivezić et al. 2019）。

## 3 物理量和单位

- `local_snr`、`frame_snr`：**无量纲**（相对质量权重倍率 `quality_weight`，**不是**校准信噪比）；
  `snr_v²`：无量纲权重因子；`m_5`：**mag**；`sigma_F`：ADU（逐源通量不确定度，定义于 ADU 标度）；
- `frame quality`：**uint32 位掩码**（星点目录质量位），非浮点标量；
- 坐标：frame_id（无单位）、tile（HEALPix 级 11 分块）、8×8 区域 gx/gy（像素单元）。

## 4 连续定义

```text
# 像素级相对质量权重（stage2 排异/积分，历史 legacy 分支 （已按 §9.73 A44 作废：键不存在；权重是派生量）：生产禁用）
#   历史权重分支号 （已按 §9.73 A44 作废：键不存在；权重是派生量） = 实现事实（stage2_common.cpp:377-385 / stage2.cpp:1106-1141）：
#     0 = support × snr_v²（本文件，legacy/ablation/诊断）
#     1 = 等权 1.0
#     2 = 逐样本 ivar 逆方差权重（生产默认，无 fallback；见 SCI-UPM §5:54 /
#         DATA-UNC-001 §51 / DESIGN §4.3「SNR → 逆方差权重，不是直接用 SNR 加权」）
for 每个候选 s:
  snr_v = local_snr_map[key(frame_id,tile,gx,gy)]        # 有局部星点 → 局部相对质量权重
          else frame_snr_by_id[frame_id]                # 缺失 → 整帧质量权重中位数
  quality_weight[s] = snr_v                              # 相对质量权重，非科学 SNR
  weights[s] = support[s] × snr_v²                      # 禁止 snr=1.0 伪装 unknown
  # ↑ 仅历史 legacy 分支（ablation）；生产用逐样本 ivar （已按 §9.73 A44 作废：键不存在；权重是派生量）。

# local_snr_map 构造（stage2.cpp:383-396）
for 每个控制观测 o:
  if !o.snr_available: continue                          # 不入局部 map，像素级回退整帧 median
  key = (o.frame_id, tile, x/64, y/64)                   # x,y 由 HEALPix level-9 leaf → local xy
  local_snr_map[key] = o.snr

# frame_snr_medians（stage2.cpp:74,250）
frame_snr[i] = median(帧 i 相对质量权重目录值)   # 分布摘要，非科学信噪比

# frame quality（sampler.cpp:152,242,266）
for 每个控制星 s（半径内）:
  out_qual |= quality[s]                                 # 质量位 OR 累积
```

## 5 规则 / 显式行为

- **局部优先，整帧回退**：有局部星点的 cell 用 `local_snr`（局部相对质量权重）；无局部星点回退整帧质量权重中位数；
- **snr=1.0 不允许作为 unknown 伪装**：缺失走整帧 median 回退并计数
  `local_snr_unavailable`（stage2.cpp:380-389,400-405）；
- **snr_available 位保留**：即使回退为整帧 median，snr_available 仍记录；
- **质量控制位为 OR 累积**（非均值/加权），表达"半径内任一惊星目录质量满足"的覆盖性语义；
- **与 SCI-NOISE 区隔**：`variance/ivar` 为逐像素随机噪声权重；`local_snr/frame_snr` 为
  区域/帧级**相对质量权重倍率**（`quality_weight`，非科学信噪比）；二者**不混用**。
- **分支号与生产面**：本文件的 `support × snr_v²` 是历史 legacy 分支 （已按 §9.73 A44 作废：键不存在；权重是派生量）（legacy/
  ablation/诊断，实现锚 `stage2_common.cpp:377-385`、`stage2.cpp:1123-1141`）；
  **生产默认 = 逐样本 `ivar` 逆方差权重** （已按 §9.73 A44 作废：键不存在；权重是派生量）、无 fallback（ivar 缺失 =
  显式科学错误 `rc=2/7`），见 SCI-UPM §5:54、DATA-UNC-001 §51、DESIGN §4.3/§4.4。
  本文件不定义 mode 2 的权重语义。

## 6 独立不变量

- **局部优先不变量**：存在 snr_available 局部观测的 cell 优先用局部相对质量权重，不回退；
- **无伪 unknown**：`snr=1.0` 不作为缺失标记（缺失→回退整帧 median 而非伪装 1.0）；
- **量纲区隔**：`quality_weight`/相对质量权重（无量纲）与 `variance/ivar`（ADU²/ADU⁻²）不混用；
- **质量位非浮点**：`frame quality` 为位掩码，不参与算术权重，仅作覆盖性 OR。

## 7 不可接受变化（部分）

- 以 `snr=1.0` 替代缺失回退（伪装 unknown）；
- 将 `local_snr` 与逐像素 `variance/ivar` 当作同一权重语义；
- 将 `frame quality` 位掩码当作浮点权重参与数值积分；
- 把 stage2 的 `quality_weight`/`local_snr`/`frame_snr_medians` 声明或解释为科学信噪比（`m_5`、`SNR_F` 或 Phase1 产品面的 `frame_snr`）。

## 8 关联与追溯

- 实现：`lib/algorithms/coverage/tools/stage2.cpp`（`local_snr_map`, `frame_snr_medians`,
  `frame_snr_by_id`, `local_snr_unavailable`, `local_snr_used`, `frame_snr_fallback`）、
  `lib/algorithms/coverage/src/sampler.cpp`（`quality`, `out_qual`, `kSnrCatalogMax`）。
- 公开 API：见 `docs/TRACEABILITY.csv`；测试：见 `lib/algorithms/coverage/tests/synthetic_gate.cpp`
  （UPMW-* 权重相关）。
- 权威文件：本文件 `docs/science/CONTROL_WEIGHT_SNR.md`（SCI-CW-001..008）。

## 8a. SCI-B 定案结论（帧级 SNR 定义与跨帧可比性，实验闭环）

> 依据：`实验/absolute-snr/`（`results/b1_sky_scan.json`、`b2_noise_terms.json`、`b4_integration.json`，复跑 `code/run_all.sh`）。本节确认 §2a 的帧级定义并给出实验边界，不改任何定义与权重语义。

1. **定义已被物理 MC 证实**：固定真实源通量、只抬升天光时，`SNR=F_signal/σ_F` 单调下降，天光主导段 log-log 斜率 −0.4879（亮源）/−0.4972（暗源）（理论 −1/2），`SNR(10⁶)/SNR(0)=2.11%/1.07%`；定义式与 MC 经验散度 max|z|=2.06（26 点）。
2. **"信号含天光"的传统口径失真可量化**：同一天光范围内传统口径上升 3 个数量级（B=10⁶ 时相对真值 ×3419 亮源 / ×1.0e5 暗源）；不扣局部背景的帧级臂 ×8040 ⇒ **必须独立估计并扣除局部背景**（1% SNR 偏差对应背景偏差 δB*=2.78 e⁻ ≈ 0.28% 天光；实测 1% 交叉 3.44 e⁻）。
3. **跨帧可比性硬约束**：逐帧 `F_ref,k`、同帧配对、`m_ref=6.0`；`SNR_combined²=ΣSNR_k²` 相对偏差 2.2e-16，Q/W 信息量 `Var=1/ΣW` 实测 1275 vs 解析 1260（+1.2%）；逆方差组合严格优于等权与 `w∝SNR`。
4. **量纲区隔复核**：`quality_weight`（无量纲相对质量）与 `variance/ivar`（ADU²）不混用——§5/§6 的不变量在本单元以数值方式复核（权重换算恒等、组合方差解析对拍）。
5. **生产调用点读噪双计：已修复**。原缺陷：`module_adapters.cpp` 把**含读噪**的经验空天总 rms 填入 `sigma_sky_adu`，而 gain>0 时 `snr_science.cpp` 再加一次 `(RN/g)²` ⇒ σ_F 高估（基准点 +12.8%、RN=50 时 +34.0%，天光主导时消失）。**修复**：`SnrSourceParams.sigma_sky_source` 显式声明语义（`SHOT_ONLY` / `EMPIRICAL_TOTAL_RMS`），生产调用点声明 `EMPIRICAL_TOTAL_RMS`（`noise_sigma` = `StarDetector::estimate_background` 的**整帧 2 轮裁剪 RMS**，含读噪的经验总 rms；噪声模型 A 的 `1.4826×MAD` 稳健尺度是另一生产者，承载逐像素 `variance`）⇒ 不再叠加 `(RN/g)²`；`sigma_sky_source_effective` 落 provenance。**保护测试** `p1snr_science_skysource`：正确口径与独立 MC 真值 zA=1.27（≤3σ）绿、双计臂 zB=25.6（>3σ）红、legacy 缺省与双计臂逐位一致（向后兼容）。PSF 行路径（`snr_estimator.cpp`，gain 未知不加 RN 项）不受影响。量化见 `实验/absolute-snr/results/DOC_CORRECTIONS.md` D1。
6. **误差预算常数单位**：`NOISE_MODEL.md:86` 的 `1.44/√N` 是**相对**标准误（实测 1.166/√N），换算到 dex 为 `1.44/ln10/√N`；直接当 dex 常数用会高估 2.303 倍（`DOC_CORRECTIONS.md` D2）。
7. **稀疏层表示与载体（负责人裁决）**：`sparse_snr_layer` 的控制点直接存**绝对**通量型 SNR `SNR_c = F_ref/σ_F,c`（与帧级 SNR 同物理定义、同逐帧参考通量 `F_ref`，无量纲）；Phase2 由控制点**直接重建**为稠密 `SNR(x,y)`，逐像素权重同式 `w(x,y)=SNR(x,y)²/F_ref²`。帧级 SNR 与稀疏层是**相互独立**的两个对象——帧级 SNR **不作**稀疏层的尺度基准，也不参与其还原；帧级 SNR 自身的定义、红线与独立用途（帧级参考电平、`frame_reconstruct` 口径、缺逐像素 ivar 时的帧级逆方差权重链）见 §2a 与 `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1。**方向性约束（若未来要求两者一致）**：只允许 `frame_snr := median_p(SNR_c)`（由**绝对控制点导出帧级摘要**，或按足迹加权的摘要），**禁止**反向（`SNR_c := frame_snr × 相对场`）——反向把帧级估计量的偏差乘进每一个控制点，且在 `median_p(SNR_c) ≠ frame_snr` 时**不可逆**。本节 §2a 的帧级科学基准与 §4 的 `quality_weight` 语义均不因此改变。

## 8b. 三口径适用域图谱（SCI-B 定案；选型依据）

> 依据：`实验/absolute-snr/results/b3_domain_map.json`（`gates.delta64_detail`、`faces.synthetic_grf`、`faces.hst_m16`）与 `b6_gates_audit.json`。三条口径指 `dense` / `sparse_reconstruct`(Δ=64) / `frame_reconstruct`（§4.2 路径表）。

| 口径 | 优的域 | 劣的域 | 依据 |
|---|---|---|---|
| `sparse_reconstruct`（**默认**） | **地面视宁度受限域**（σ 场有空间结构、源污染低）：三帧全部胜出帧级标量 | 高对比结构域（HST 类）：cell 稳健 MAD 被 cell 内**未分辨结构**抬偏，偏差随 Δ 从 +0.0297（Δ=32）增到 +0.2626（Δ=256）、+0.4444（Δ=512）dex | `b3_domain_map.json` |
| `frame_reconstruct` | **HST 类高对比结构域且重建算子为 `bilinear_regular_grid_v1` 或默认 `natural_bicubic_spline_clip_v1` 时**：帧级标量更优（Δ≥32 起） | 有明显帧内 σ 梯度时丢失空间信息 | 同上 + EXP-04 §4.4/§5.3 |
| `dense` | 精度基准 | **存储门**：4096² = 67,108,864 B = 64 MiB/帧 = 1 MiB 预算的 **64 倍** ⇒ 稠密超门结论成立 | 同上 |

> **HST 类域的结论与重建算子绑定（条件式；不得写成无条件判定）**：换用 `natural_bicubic_spline_clip_mesh_median_v1`（3×3 mesh 中值前置滤波 + 自然边界双三次样条 + 值域钳制）后，同一 HST 域上稀疏臂 **E=0.0490 优于帧级臂 0.0530** ⇒ 「HST 类域帧级更优」只在双线性/默认算子下成立。Δ\*（该算子 E 首次劣于帧级臂的最小 Δ）实测：`bilinear_regular_grid_v1` **32 px**、默认 `natural_bicubic_spline_clip_v1` **32 px**、`..._mesh_median_v1` **128 px**；生产 Δ=64 落在前两者的失效区内（分别差 2.1 / 2.6 倍），落在后者的有效区内。算子定义与选择规则见 `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.5，判据与数字正本见 `实验/absolute-snr` EXP-04 §3.2/§4.4/§5.3。

- **失效边界不是单一 `Δ/ℓ≈1`，而是「`Δ/ℓ` × σ 场幅度 × 未分辨结构污染」三因子联合判据**：无源污染合成面上 sparse 在 Δ=512 px 仍胜（余量 ℓ=16 → 0.6%、ℓ=32 → 1.8%、ℓ=64 → 2.8%、ℓ=128 → 16%、ℓ=256 → 35%），而 HST M16 真实结构面上 Δ*=16 px（Δ/ℓ=0.50）；
- **判据必须非退化**：空间权重/σ 场的精度判据用**权重效率损失** `E = Var_w/Var_opt − 1`（全局尺度相消；`E=0 ⇔ σ̂ ∝ σ_true`）。「帧级臂 RMSE ≤ K·s_field」类判据对**任意**真值场恒真（对抗打乱场下 E=7.17 仍绿），**不得充当证据**（`b6_gates_audit.json::tautology_demo`）；平坦 σ 场（真值无空间效应）时任何「空间口径优于帧级」的排序判据都退化，须走 `DEGENERATE_flat_field_ranking_never_true` 用例（该门不计证据）；
- **E 对乘性偏差免疫，不能替代水平偏差判据**：E 由 `w = 1/σ̂²` 的比值定义，整体乘性缩放完全相消——实测两臂可以 E 完全相同（均 0.0530）而水平偏差相差 **8.7 倍**（0.094 vs 0.818 dex）。⇒ 选型必须同时报 E 与水平偏差，**不得**用「E 相同」推断「两臂等价」；
- **控制点局部 σ 的估计器要求（数值准确的必要条件）**：稀疏层「表示正确」（控制点存绝对 SNR）**不等于**「数值准确」——后者完全由局部 σ 估计器决定。朴素地把估计作用域从整帧换成区域（同一 recipe + 更小窗口）**不够**：结构污染只是被挪到更小尺度，cell 内的未分辨结构仍被算进稳健尺度，偏差随 Δ 单调增大。控制点的局部 σ **必须**用**结构感知**估计器：mesh 局部背景扣除后的逐区域残差稳健尺度，或跨帧差分（唯一零结构偏差口径）；估计器标识、`sigma_rho` 与 `quality_flags` 随稀疏层入 manifest（正本见 `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.5）。
- **兜底**：HST 类数据默认给**帧级标量兜底**，不静默用稀疏口径冒充空间精度；该兜底与「稀疏臂换用 `..._mesh_median_v1` 后可胜出」并不冲突——两者是同一域上的两条可选路径，取舍见 `07_noise_snr.md` §4.5 的选择规则。
- **三口径全部保留，无退役项**：稀疏层改存**绝对** SNR 是**表示与载体**的变更，**不改变本图谱**——图谱比较的是「三条口径重建稠密 SNR 的精度与存储量」，与「控制点存相对值还是绝对值」正交（重建算子与误差仍在 manifest 显式声明）。`frame_reconstruct` 有独立适用域（HST 类高对比结构域）且是「输入无稀疏层」时 `snr_path_effective` 的唯一合法取值 ⇒ **不作废**；`dense` 仍是精度基准；`sparse_reconstruct` 仍是默认。

## 9 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动 §2a/§4 任何定义与权重语义。

- **5σ 点源深度 m5**：Tonry, J. L. et al. 2012, ApJ 750, 99（Pan-STARRS 3π）；Huang, S. et al. 2017, ApJ 838, 110（HSC 深度）；Ivezić, Ž. et al. 2019, ApJ 873, 111（LSST 深度定义）。
- **逐源最优提取 SNR_F=F/σ_F**：Horne 1986, PASP 98, 609；Naylor 1998, MNRAS 296, 339。
- **PixInsight PSFSNR/PSFSW 方法学**：PixInsight Reference, New Image Weighting Algorithms（https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html）。**核验状态**：方法学文档，未逐式核验常数。
- **稳健噪声 MRS/N***：Starck & Murtagh 2006, Astronomical Image and Data Analysis, 2nd ed., Springer（ISBN 978-3-540-33023-3）。
- **已定案（原 UNRESOLVED，两篇权威打架）**：冲突根因是**同名两义**——本文件 §2a 的 `frame_snr` 是 Phase2 stage2 内部相对质量权重场，而 `docs/design/UNIFIED_MODEL.md` §2 与 `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1 的 `frame_snr` 是 Phase1 HiPS 文件头的科学量。负责人 2026-09-19 裁决 A1/C1（claim `FIX-SCI-SNR-CANON-001`）确认：**帧级 SNR = 点源（PSF）信号 SNR，纯信号/噪声**（`F_signal` 已扣局部背景、天光只进 `σ_F`、天光增大 ⇒ SNR 单调下降），故 Phase1 产品面按科学量定义；本文件的 stage2 字段是相对质量场，须改名 `quality_weight` 以消除同名互指。**UNRESOLVED 关闭**。

参考代码库（含许可证；仅对照不复制 GPL 代码）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）：WCS/投影、统计、单位。
- photutils（BSD-3-Clause，https://github.com/astropy/photutils）：检测/质心、背景估计、PSF 与孔径测光。
- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）：背景网格、检测/去混叠、FLUXERR。
- ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）与 LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）：母版约定与 ISR 顺序。
- SWarp（GPL-3.0，https://github.com/astromatic/swarp）/ SCAMP（GPL-3.0，https://github.com/astromatic/scamp）：马赛克背景与相对定标。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）：drizzle 与相关噪声。
- astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）/ healpy（GPL-2.0，https://github.com/healpy/healpy）：HEALPix 几何。
- reproject（BSD-3-Clause，https://github.com/astropy/reproject）：WCS 重采样与方差传播。
- NumPy/SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

