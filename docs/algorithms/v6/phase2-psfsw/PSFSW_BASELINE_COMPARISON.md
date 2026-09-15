# PSFSW 基线比较协议

- 文档 ID：`ALG-P2-PSFSW-001-BASELINE`
- 协议 ID：`PSFSW-BASELINE-COMPARISON-V1`；机器可读同源 `psfsw_spec.json` → `baseline_comparison`
- 上游：`DESIGN-P2-001` §6.3/§10；`SCI-PSFW-001` §4 表/§5/§7.4；`SCI-P2-001` §4.4；`FZ-MODE-BASELINE`

## 1. 四基线定义

| `baseline_id` | 权重 | 备注 |
|---|---|---|
| `equal` | `w_k=1` | 文档基线模式（`FZ-MODE-BASELINE`） |
| `exposure` | `w_k=t_k`（有效曝光时间，来自 provenance） | **比较协议标识**，不是 `weight_mode` 合法值；不扩展冻结枚举（`C-004.3`） |
| `pixel_ivar` | `w_k(p)=1/v_k(p)` | 文档基线模式（`FZ-MODE-BASELINE`）；不得宣称对任意 PSF 的点源最优 |
| `point_information` | `W_info,k=a_k²P_kᵀC_k⁻¹P_k` | 生产模式 `point_information` 作为 `W_info` 基线（`FZ-FORMULA-WINFO`） |

`exposure` 若缺 `t_k` provenance → 该基线不可用并显式登记（不得用占位权重冒充）。

## 2. 计算协议

对每个基线，使用与 `psfsw_robust` **相同**的组合机制：

```text
alpha_k(p) = w_k(p) v_k(p) / sum_j w_j(p) v_j(p)
I_out(p)   = sum_k alpha_k(p) d_k(p)
C_out      = R C_in R^T           # R 用该基线的实际系数
P_eff      = 该基线实际算子的脉冲响应（归一约定声明）
```

禁止对基线使用 `Var = 1/w` 之类反推；所有基线的 variance 都从实际系数传播（`FZ-FORMULA-COV-PROP`）。

## 3. 报告指标（缺一不可）

`point_source_detection_power`、`photometric_variance`、`effective_psf_fwhm`、`effective_psf_encircled_energy`、`flux_bias`、`surface_brightness_bias`、`artifact_metrics`。
禁止只以“看起来更好”通过（`SCI-PSFW-001` §7.5）。

## 4. 预注册与样本隔离

- 验收数据预注册：M42、银心、合成集；
- `calibration_sample_id != acceptance_sample_id`（门 `PSFSW-G24`）；
- 预注册记录必须含数据 ID/版本哈希、指标、判定规则与基线集合；预注册后不得改判定规则。

## 5. 声明门（统计）

只允许：

- `better_than(baseline_id)`：配对 bootstrap（≥200 次、95%）改进量 **CI 下界 > 0**；
- `non_inferior_to(baseline_id)`：**CI 下界 > −0.02**（不劣界，可配置，须版本化）。

禁止声明：`fisher_optimal`、`equals_ivar`、`equivalent_to_W_info`、`looks_better`；禁止把 `psfsw_robust` 的经验成功替代 Q/W 与 covariance 的科学产品（`SCI-PSFW-001` §8 末条）。

## 6. 与冻结模式枚举的关系（合规声明）

- 生产模式枚举保持 `{point_information, surface_gls, psfsw_robust}`（`FZ-MODE-PRODUCTION`）；
- 文档基线模式枚举保持 `{equal, pixel_ivar}`（`FZ-MODE-BASELINE`）；
- `exposure` 与 `baseline_id` 是**比较协议字段**，归属 `SCHEMA-INTEGRATE-001`(W6) 归一，本协议不发明 `weight_mode` 新值、不改 contracts（`C-004.3`/`C-004.5`）。
