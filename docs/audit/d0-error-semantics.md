# D0｜四种误差/未知语义的代码映射与反例

> 目的：把“确定数值包围 / 随机统计 / 物理系统偏差 / 未知”从文档措辞落到公共字段与失败状态，防止把区间当方差、把 unknown 当零。受检 HEAD `0e5ccda1f`。不改代码。

## 1. 定义与字段

| 语义 | 数学对象 | 公共字段/类型 | 允许的声明 |
|---|---|---|---|
| 确定数值包围 | 可证区间 \([a_L,a_U]\) | `Interval`、`ExactInterval`、`SignalEstimate.numerical_lower/upper`、`Variance.numeric_enclosure`、`assessment.validity=certified` | 只对算法/浮点/几何误差；区间必须包含真值 |
| 随机统计 | 协方差 \(C\) 或方差 \(V\) | `CovarianceAction`、`Variance.value`、`Variance.state=known/conditional` | 需要完整随机模型；unknown 不填 0 |
| 物理系统偏差 | 有界或未定的模型偏差 | `ErrorRecord.unknown_model_terms`、`finite_model_bias`、`assessment.omitted` | 不得包装成均值为零的随机方差 |
| 未知 | 没有可用模型/数据 | `noise=null`、`UnknownTerm`、`Variance.state=unknown`、`science=conditional`、`Code::unavailable` | 必须具名；下游不得把它当 0 |

## 2. 规则与反例

### R1 数值包围不能加进方差

`variance.value` 是随机方差；`numeric_enclosure` 是确定区间。把几何区间宽度加到 `Variance.value` 会把系统/数值误差伪装成随机噪声。反例：`STATISTICS_SKY_COADD` §2.1 的式 (3a)/(3b)；代码侧 `Variance` 同时有 `value` 与 `numeric_enclosure`，二者不得相加。

### R2 unknown 不是零

`SigmaEstimate.noise=null` 表示未知，不是 `C=0`。反例：X01 无信息请求时 normalize 的 noise 曾为空且无 issue；现在 `product.collect_signal` 会写 `no_noise` 并置 `assessment.noise=unknown`。
`covariance_apply` 对带 material `UnknownTerm` 的模型返回 `unavailable`，不返回零方差（`src/uncertainty/api.cpp`）。

### R3 `certified` 只用于确定量

`validity=certified` 只说明数值/几何包围可证；不说明随机方差已知。X01 的 `measure` 曾把数据残差加进半径，使“certified”区间变成统计区间；R1 修复后半径回到 1e-10 相对量级（`normalize-fix-report.md` §13.1），这是 `certified` 的正确用法。

### R4 物理系统偏差必须有界或具名

`ErrorRecord.unknown_model_terms` 只记录“未建模”；若偏差有界，应进入 `finite_model_bias`；若无界，必须使下游 `science=conditional`，不能只写一句注释。反例：PSF 形状偏差、Gaia XP 有限分辨率（`GAIA_RESPONSE` §4）都不能转成随机方差。

### R5 同一随机量只传播一次

`CovarianceAction` 的 root/shared_id 是同一随机变量；同一 `SharedId` 跨帧必须在 joint model 合并，不能按帧独立加方差。反例：CE-C/CE-H（`review-statistics-fix.md`）与 coadd 的 `W C Wᵀ` 要求。

### R6 选择后的条件 coverage

固定 mask 的 `C_z=P C Pᵀ` 是条件协方差；数据依赖选择后的 coverage 需前向 MC/bootstrap。反例：`STATISTICS_SKY_COADD` §2.3 的 `z` 与选择事件。

## 3. 当前未闭合

- 产品层 full 协方差重开在 `to_csr`/sky.controls 上有 4096 上界，full 仍以 `signal_only` 降级；这不影响四种语义的字段规则，但让 `C` 的端到端验证受限。
- `Variance.state=conditional` 与 `assessment.science=conditional` 的映射尚无自动一致性检查；只能人工核对。
- 系统偏差的“有界”字段在 PSF/XP 链上多为 `omitted` 文本，缺少可计算的 `finite_model_bias` 实例。

## 4. 裁决

四种语义在公共类型上已有独立载体，且关键反例（数据残差进半径、unknown 填零、逐叶对角化）已有修复记录；D0 仍不能标 PASS，因为 full 协方差端到端与系统偏差的有限界实例未闭合。
