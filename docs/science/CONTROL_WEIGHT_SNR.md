# Control-Weight SNR / Frame Quality Science (SCI-CW)

> ID: SCI-CW-001..008  状态: FROZEN (2026-08-27, G3 SCI-002 补冻)  上游: SCI-SCOPE-001,
> SCI-NOISE (逐像素 σ/variance/ivar)  下游 ALG: ALG-CW-001..  模块: `phase2` sampler/
> stage2 控制权重 (local_snr_map, frame_snr_medians, quality)

> 补齐 SCI-002 要求的 `local_snr` 与 `frame quality` 权威定义（此前仅散落在
> code + `docs/architecture/execution_inventory.csv`，无 science authority）。

## 1 目的与非目标

- **目的**：定义 phase2 控制采样/加权积分所用的**区域级 SNR** 与**帧/星点质量**，作为
  `support × snr²` 控制权重中的 SNR 因子；与 SCI-NOISE 的逐像素 `variance/ivar`
  （随机噪声倒数权重）**区隔**，二者量纲语义不同、不混用。
- **非目标**：不定义测光零点/PSF/astrometry 质量（SCI-PHOT/SCI-PSF/SCI-WCS）；不生产
  完整质量评分（仅相位/星点目录质量位）；不替代 SCI-INT 的 `support` canonica reducer。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `local_snr` | 区域级局部 SNR（有局部星点可得时） | `stage2.cpp:383-396`（`local_snr_map`） |
| `frame_snr` | 整帧 Phase1 SNR 目录中位数（回退基准） | `stage2.cpp:74,250,402`（`frame_snr_medians`） |
| `snr_available` | 该控制观测是否含真实可用局部 SNR | `stage2.cpp:387` |
| `frame quality` | 每星点 Phase1 SNR 目录质量位（uint32 位掩码） | `sampler.cpp:145,259`（`quality`, `out_qual`） |
| `kSnrCatalogMax` | SNR 目录质量槽上限 | `sampler.cpp:568` |
| `w_snr` | SNR 权重因子 `= snr_v²` | 积分/排异权重 |

## 3 物理量和单位

- `local_snr`、`frame_snr`：**无量纲**（SNR 倍率）；`snr_v²`：无量纲权重因子；
- `frame quality`：**uint32 位掩码**（星点目录质量位），非浮点标量；
- 坐标：frame_id（无单位）、tile（HEALPix 级 11 分块）、8×8 区域 gx/gy（像素单元）。

## 4 连续定义

```text
# 像素级 SNR 权重（stage2 排异/积分，weight_mode=2）
for 每个候选 s:
  snr_v = local_snr_map[key(frame_id,tile,gx,gy)]        # 有局部星点 → 局部 SNR
          else frame_snr_by_id[frame_id]                # 缺失 → 整帧 SNR 中位数
  weights[s] = support[s] × snr_v²                      # 禁止 snr=1.0 伪装 unknown

# local_snr_map 构造（stage2.cpp:383-396）
for 每个控制观测 o:
  if !o.snr_available: continue                          # 不入局部 map，像素级回退整帧 median
  key = (o.frame_id, tile, x/64, y/64)                   # x,y 由 HEALPix level-9 leaf → local xy
  local_snr_map[key] = o.snr

# frame_snr_medians（stage2.cpp:74,250）
frame_snr[i] = median(帧 i SNR 目录值)

# frame quality（sampler.cpp:145,235,259）
for 每个控制星 s（半径内）:
  out_qual |= quality[s]                                 # 质量位 OR 累积
```

## 5 规则 / 显式行为

- **局部优先，整帧回退**：有局部星点的 cell 用 `local_snr`；无局部星点回退整帧 SNR 中位数；
- **snr=1.0 不允许作为 unknown 伪装**：缺失走整帧 median 回退并计数
  `local_snr_unavailable`（stage2.cpp:380-389,400-405）；
- **`snr_available` 位保留**：即使回退为整帧 median，`snr_available` 仍记录；
- **质量控制位为 OR 累积**（非均值/加权），表达"半径内任一惊星目录质量满足"的覆盖性语义；
- **与 SCI-NOISE 区隔**：`variance/ivar` 为逐像素随机噪声权重；`local_snr/frame_snr` 为
  区域/帧级 SNR 倍率权重；二者**不混用**。

## 6 独立不变量

- **局部优先不变量**：存在 `snr_available` 局部观测的 cell 优先用局部 SNR，不回退；
- **无伪 unknown**：`snr=1.0` 不作为缺失标记（缺失→回退整帧 median 而非伪装 1.0）；
- **量纲区隔**：SNR（无量纲）与 `variance/ivar`（ADU²/ADU⁻²）不混用；
- **质量位非浮点**：`frame quality` 为位掩码，不参与算术权重，仅作覆盖性 OR。

## 7 不可接受变化（部分）

- 以 `snr=1.0` 替代缺失回退（伪装 unknown）；
- 将 `local_snr` 与逐像素 `variance/ivar` 当作同一权重语义；
- 将 `frame quality` 位掩码当作浮点权重参与数值积分。

## 8 关联与追溯

- 实现：`lib/phase2/tools/stage2.cpp`（`local_snr_map`, `frame_snr_medians`,
  `frame_snr_by_id`, `local_snr_unavailable`, `local_snr_used`, `frame_snr_fallback`）、
  `lib/phase2/src/sampler.cpp`（`quality`, `out_qual`, `kSnrCatalogMax`）。
- 公开 API：见 `docs/TRACEABILITY.csv`；测试：见 `lib/phase2/tests/synthetic_gate.cpp`
  （UPMW-* 权重相关）。
- 权威文件：本文件 `docs/science/CONTROL_WEIGHT_SNR.md`（SCI-CW-001..008）。
