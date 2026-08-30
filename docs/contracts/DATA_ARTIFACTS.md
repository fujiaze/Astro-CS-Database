# DATA-001 DataArtifact 注册表与 weight 歧义映射

> 权威: docs/contracts/DATA_SEMANTICS.md（语义）+ 本表（artifact schema 登记 + 歧义映射）
> 生成: V6 重构 DATA-001

## 1. DataArtifact schema 清单

| schema_id | 内容 | scalar | shape/axis | unit | coordinate | invalid | ownership | serialization |
|---|---|---|---|---|---|---|---|---|
| DATA-IMG-RAW-001 | raw 亮场 | f32/f64/u16 | [H,W], y,x | ADU | pixel(0-based) | NaN=invalid | unique→borrowed | FITS/XISF |
| DATA-IMG-CAL-001 | calibrated 亮场 | f32/f64 | [H,W], y,x | ADU(e⁻ 可选) | pixel | NaN=invalid; 负值保留 | unique | FITS |
| DATA-IMG-VAR-001 | variance | f32/f64 | [H,W] | ADU² | pixel | 0=无覆盖; NaN/负=损坏 | unique | FITS |
| DATA-IMG-IVAR-001 | inverse variance | f32/f64 | [H,W] | ADU⁻² | pixel | ivar=0 显式不可用 | unique | FITS |
| DATA-IMG-WEIGHT-001 | quality weight | f32 | [H,W] | 无量纲[0,1] | pixel | 0=不合格 | unique | FITS |
| DATA-IMG-SUPPORT-001 | support | f32/u8 | [H,W] | [0,1] | pixel | 0=无覆盖 | unique | FITS/HiPS |
| DATA-IMG-MASK-001 | bad-pixel mask | u8 | [H,W] | 位掩码 | pixel | 1=坏点 | unique | FITS |
| DATA-WCS-001 | WCS 描述 | struct | n/a | deg/px | ICRS | 无解=显式错误 | shared | header/JSON |
| DATA-CAT-PSF-001 | PSF catalog | struct[] | [N] | px,deg,ADU | ICRS+pixel | 失败不输出伪有效 | unique | FITS/CSV |
| DATA-CAT-PHOT-001 | photometry catalog | struct[] | [N] | e⁻,mag | ICRS | 饱和拒=rejected_quality | unique | FITS/CSV |
| DATA-HIPS-SIGNAL-001 | HiPS signal 数据集 | f32/f64 | HEALPix NESTED | 面亮度 | ICRS | NaN/support=0 | shared/persisted | HiPS |
| DATA-HIPS-VAR-001 | HiPS variance | f32/f64 | HEALPix | ADU² | ICRS | 0=无覆盖 | persisted | HiPS |
| DATA-HIPS-IVAR-001 | HiPS ivar | f32/f64 | HEALPix | ADU⁻² | ICRS | ivar=0 | persisted | HiPS |
| DATA-UPM-MODEL-001 | UPM 加性模型 | double[] | [n_frame×n_cell] | ADU | control cell | NO_DATA 显式 | persisted | UPM file |
| DATA-UPM-CONTROL-UNC-001 | control variance/ivar | f64 | [n_control] | ADU²/ADU⁻² | control cell | ivar≤0→rc=2 | unique | JSON |
| DATA-REJ-MAP-001 | rejection map | u8 | [H,W] | 位掩码 | pixel | reason code 每样本 | persisted | FITS |
| DATA-FRAME-ID-001 | frame identity | uint64 | scalar | 无量纲 | 科学 payload 派生 | 重复拒绝 | shared | JSON/manifest |
| DATA-P3-FITS-001 | 平面 FITS | f32/f64 | [W_out,H_out] | 面亮度(禁默认 Jy/beam) | TAN/ICRS | NaN+coverage | persisted | FITS |

## 2. weight/value/scale/sigma/snr 歧义映射（DATA-001 登记）

控制包 06 要求：对现有 `weight/scale/sigma/value/snr` 字段逐个映射或登记 ambiguity；
未消除 ambiguity 不得进 G2 consumer API。

| 字段名 | 出现位置 | 语义 | DATA ID | 歧义状态 |
|---|---|---|---|---|
| `weights` (integrate) | lib/phase2/include/astro/phase2/integrate.h | 候选栈数值权重 = support×SNR² 或等权(1.0) | DATA-IMG-WEIGHT-001 数值权重 | 已消除(与 UPM 权重分离命名) |
| `weights` (rejection) | lib/phase2/include/astro/phase2/rejection.h | 随样本携带到候选栈的数值权重 | 同上 | 已消除 |
| `upm.robust_control_weight` | upm.cpp | UPM 控制点权重 = quality×geom×control_ivar | DATA-UPM-CONTROL-UNC-001 | 已消除(禁止与 integration weight 混名) |
| `support` | integrate/upm/sampler | 覆盖支撑 [0,1] | DATA-IMG-SUPPORT-001 | 明确 |
| `scale_deg_per_px` | p3_session | 输出像元角尺度 | DATA-P3-FITS-001 s_out | 明确(单位 deg/px) |
| `sigma` | master_generator/rejection | MAD 转 σ 系数 1.4826 / 拒绝阈值倍数 | SCI-NOISE/SCI-REJ | 明确(无量纲倍数) |
| `snr` | CW/sampler | 区域级 SNR 权重因子 snr_v² | SCI-CW-001 | 明确(与 ivar 语义分离) |
| `value` (integrate) | integrate.h | 候选样本值 | DATA-IMG-CAL-001 标度 | 明确 |
| `quality` | sampler | 帧/星点质量位掩码 | SCI-CW-001 | 明确 |
| `k_corr` | sampler.cpp:672 | Drizzle 相关校正 1.4 | SCI-UPM-WEIGHT-001 | 明确 |

结论：`weight` 在 integrate 与 UPM 两处语义已显式分离命名（`stack.*.v1` vs
`upm.robust_control_weight.v1`），无未消除歧义；`scale/sigma/snr/value/quality` 均有
明确 DATA/SCI 归属。G2 consumer API 可安全引用。

## 3. 机器校验

- `tools/check_data_artifacts.py`（DATA-001 新增）：校验本表 schema_id 唯一、
  DATA-SEMANTICS.md 中声明的 DATA-* ID 全部在本表登记、无重复。
