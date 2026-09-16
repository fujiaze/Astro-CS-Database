# Control-Weight SNR / Frame Quality Science (SCI-CW)

> ID: SCI-CW-001..008  状态: FROZEN (2026-08-27, G3 SCI-002 补冻)  上游: SCI-SCOPE-001,
> SCI-NOISE (逐像素 σ/variance/ivar)  下游 ALG: ALG-CW-001..  模块: `phase2` sampler/
> stage2 控制权重 (local_snr_map, frame_snr_medians, quality)

> 补齐 SCI-002 要求的 `local_snr` 与 `frame quality` 权威定义（此前仅散落在
> code + `docs/architecture/execution_inventory.csv`，无 science authority）。

> **P5-SNR 重定义注记（2026-09-14，负责人授权；依据 PHOTOMETRY_LITERATURE_REVIEW D.2 S4）**：
> 本文件所称 `local_snr` / `frame_snr` 实为**相对质量权重场**（`quality_weight =
> frame_quality_scalar × local_quality_proxy/median`），**不是科学信噪比**；科学 SNR 由
> 逐源 `σ_F` 定义，帧级科学基准为 5σ 点源深度 `m_5`（§2a）。字段名 `snr` 仅为兼容保留，
> 其语义为 "SNR-equivalent relative quality, not a calibrated signal-to-noise ratio"。

## 1 目的与非目标

- **目的**：定义 phase2 控制采样/加权积分所用的**相对质量权重场**（`quality_weight =
  frame_quality_scalar × local_quality_proxy/median`）与**帧/星点质量**，作为
  `support × snr²` 控制权重中的质量权重因子；该权重场**不是科学信噪比**——其数值来源为
  帧级定标散度与逐星拟合质量代理，科学 SNR 由逐源 `σ_F`（PSF 拟合协方差或 CCD 方程）
  定义，帧级科学基准为 5σ 点源深度 `m_5`（§2a）。与 SCI-NOISE 的逐像素 `variance/ivar`
  （随机噪声倒数权重）**区隔**，二者量纲语义不同、不混用。
  <!-- (P5-SNR 订正 2026-09-14，负责人授权；依据 PHOTOMETRY_LITERATURE_REVIEW D.2 S4) -->
- **非目标**：不定义测光零点/PSF/astrometry 质量（SCI-PHOT/SCI-PSF/SCI-WCS）；不生产
  完整质量评分（仅相位/星点目录质量位）；不替代 SCI-INT 的 `support` canonica reducer。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `local_snr` | 区域级局部**相对质量权重**（有局部星点可得时；**不是**科学信噪比） | `stage2.cpp:383-396`（`local_snr_map`） |
| `frame_snr` | 整帧 Phase1 SNR 目录值的**中位数（回退质量基准）**（**不是**科学信噪比） | `stage2.cpp:74,250,402`（`frame_snr_medians`） |
| `quality_weight` | `frame_quality_scalar × local_quality_proxy/median`，无量纲相对质量权重（S4 重定义的规范名；`snr_v` 为其别名） | 本文件 §4 |
| `m_5` | 5σ 点源深度 `ZP − 2.5·log10(5·sigma_F(ref))`（唯一帧级科学基准，单位 mag） | 本文件 §2a |
| `sigma_F` | 逐源通量不确定度（科学 SNR 定义量；PSF 拟合协方差或 CCD 方程，当前实现不产出） | 消费侧定义；本文件 §2a 登记边界 |
| snr_available | 该控制观测是否含真实可用的局部质量权重（字段名沿用 `snr`） | `stage2.cpp:387` |
| `frame quality` | 每星点 Phase1 SNR 目录质量位（uint32 位掩码） | `sampler.cpp:152,266`（`quality`, `out_qual`） |
| `kSnrCatalogMax` | SNR 目录质量槽上限 | `sampler.cpp:585` |
| `w_snr` | 质量权重因子 `= snr_v²` | 积分/排异权重 |

## 2a 帧级科学基准与 SNR 定义（P5-SNR 新增）

- **逐源科学 SNR**：由逐源通量不确定度 `σ_F` 定义，`SNR_F = F/σ_F`（PSF 加权最优提取，
  或 CCD 方程 + 孔径改正）。当前实现**不产出** `σ_F`，故本合同不把任何现有标量称为科学 SNR。
- **帧级科学基准**：唯一允许的帧级科学基准是 **5σ 点源深度**
  `m_5 = ZP − 2.5·log10(5·σ_F(ref))`（单位 **mag**）。`σ_F(ref)` 必须**显式绑定**参考轮廓/
  孔径/背景（PSF 或孔径、背景估计域、像素标度）；空间变化时应给**深度图**而非单标量。
- **质量权重场定位**：本文件 §4 的 `quality_weight`（`local_snr`/`frame_snr`）为**相对质量
  权重场**，**无量纲**，仅供采样/加权（`support × snr_v²`），不得解释为 `m_5` 或 `SNR_F`。
- 推导与文献锚见 `run/release-rescue/science-phot/PHOTOMETRY_LITERATURE_REVIEW.md` §C.3.1
  （Horne 1986 最优提取；5σ 深度 Tonry et al. 2012 / Huang et al. 2017 / Ivezić et al. 2019）。
  <!-- (P5-SNR 订正 2026-09-14，负责人授权；依据 PHOTOMETRY_LITERATURE_REVIEW D.2 S4) -->

## 3 物理量和单位

- `local_snr`、`frame_snr`：**无量纲**（相对质量权重倍率 `quality_weight`，**不是**校准信噪比）；
  `snr_v²`：无量纲权重因子；`m_5`：**mag**；`sigma_F`：ADU（逐源通量不确定度，定义于 ADU 标度）；
  <!-- (P5-SNR 订正 2026-09-14，负责人授权；依据 PHOTOMETRY_LITERATURE_REVIEW D.2 S4) -->
- `frame quality`：**uint32 位掩码**（星点目录质量位），非浮点标量；
- 坐标：frame_id（无单位）、tile（HEALPix 级 11 分块）、8×8 区域 gx/gy（像素单元）。

## 4 连续定义

```text
# 像素级相对质量权重（stage2 排异/积分，weight_mode=0 legacy：生产禁用）
#   weight_mode 分支号 = 实现事实（stage2_common.cpp:377-385 / stage2.cpp:1106-1141）：
#     0 = support × snr_v²（本文件，legacy/ablation/诊断）
#     1 = 等权 1.0
#     2 = 逐样本 ivar 逆方差权重（生产默认，无 fallback；见 SCI-UPM §5:54 /
#         DATA-UNC-001 §51 / DESIGN §4.3「SNR → 逆方差权重，不是直接用 SNR 加权」）
for 每个候选 s:
  snr_v = local_snr_map[key(frame_id,tile,gx,gy)]        # 有局部星点 → 局部相对质量权重
          else frame_snr_by_id[frame_id]                # 缺失 → 整帧质量权重中位数
  quality_weight[s] = snr_v                              # 相对质量权重，非科学 SNR
  weights[s] = support[s] × snr_v²                      # 禁止 snr=1.0 伪装 unknown
  # ↑ 仅 weight_mode=0（legacy/ablation）；生产 weight_mode=2 用逐样本 ivar。

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
- **分支号与生产面**：本文件的 `support × snr_v²` 是 `weight_mode=0`（legacy/
  ablation/诊断，实现锚 `stage2_common.cpp:377-385`、`stage2.cpp:1123-1141`）；
  **生产默认 `weight_mode=2`** = 逐样本 `ivar` 逆方差权重、无 fallback（ivar 缺失 =
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
- 把 `quality_weight`/`local_snr`/`frame_snr` 声明或解释为科学信噪比（`m_5` 或 `SNR_F`）。
  <!-- (P5-SNR 订正 2026-09-14，负责人授权；依据 PHOTOMETRY_LITERATURE_REVIEW D.2 S4) -->

## 8 关联与追溯

- 实现：`lib/algorithms/coverage/tools/stage2.cpp`（`local_snr_map`, `frame_snr_medians`,
  `frame_snr_by_id`, `local_snr_unavailable`, `local_snr_used`, `frame_snr_fallback`）、
  `lib/algorithms/coverage/src/sampler.cpp`（`quality`, `out_qual`, `kSnrCatalogMax`）。
- 公开 API：见 `docs/TRACEABILITY.csv`；测试：见 `lib/algorithms/coverage/tests/synthetic_gate.cpp`
  （UPMW-* 权重相关）。
- 权威文件：本文件 `docs/science/CONTROL_WEIGHT_SNR.md`（SCI-CW-001..008）。
