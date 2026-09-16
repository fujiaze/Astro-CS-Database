> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# PSFSW 冻结阈值册（ALG-P2-PSFSW-001 落定值）

- 文档 ID：`ALG-P2-PSFSW-001-THRESHOLDS`
- 机器可读同源：`run/v6/alg-p2-psfsw/spec/psfsw_spec.json` → `frozen_thresholds`
- 授权来源：`PSFW_FREEZE_RESEARCH` §11-R6/R7 与 `SCI-P2-001` §6 open item 4 **显式委托** W3 `ALG-P2-PSFSW-001` 落定；正式冻结由 `CONTRACT-FREEZE-001`(W4) 执行。
- 纪律：本册**不修改**任何既有 `FZ-*` 数值/容差，只实例化被委托的量；需负责人签字项 `SO-01..SO-07` 不在本册范围。

## 1. 阈值总表

| ID | 量 | 冻结值 | 单位 | 度量定义 | 失败处置 | 锚 |
|---|---|---|---|---|---|---|
| `PSFSW-T-DEPTH` | 深度稳定性最大相对偏差 | **0.05** | 1 | `max_t abs(W_t - median_t(W_t)) / median_t(W_t)` | `uterms=selection_bias_gate_failed`（`uterms` 即 unavailable 原因） | `SCI-PSFW-001` §7.3；`PSFW_FREEZE_RESEARCH` §5.3 |
| `PSFSW-T-DEPTH-K` | 深度扫描点数下限 | **5** | count | 扫描点个数 `K` | 不可评估 → `insufficient_valid_stars` | 本任务 |
| `PSFSW-T-DEPTH-SPAN` | 深度跨度下限 | **1.0** | mag | 参考星表极限星等跨度（或 `n_common` 变化 ≥ 2 倍） | 不可评估 → `insufficient_valid_stars` | 本任务 |
| `PSFSW-T-DEPTH-RHO` | 单调漂移上限 | **0.8** | 1 | `abs(Spearman rho)`：深度扫描序号 vs `W_psfsw` | `selection_bias_gate_failed` | `SCI-PSFW-001` §7.3（“不随深度单调漂移”） |
| `PSFSW-T-NMIN` | `n_common` 硬下限 | **3** | stars | `n_common` | `insufficient_valid_stars` | `SCI-P2-001` R5 |
| `PSFSW-T-NROBUST` | `n_common` 稳健下限 | **10** | stars | `n_common`；低于则 LOW 档 | 强制 low 档旗标 + 收紧非均匀门 | 本任务 |
| `PSFSW-T-NPREF` | `n_common` 优选值 | **30** | stars | `n_common` | 仅旗标 | 本任务 |
| `PSFSW-T-NU` | 分量空间非均匀上限 | **0.30** | 1 | 每分量 `(p95 - p05) / p50` | `spatial_nonuniformity_gate_failed` | `FZ-DEGRADE-SCALAR`；`SCI-PSFW-001` §6 |
| `PSFSW-T-TREND` | 分量系统趋势上限 | **0.10** | 1 | `max abs(线性拟合值) / p50` | `spatial_nonuniformity_gate_failed` | `FZ-DEGRADE-SCALAR` |
| `PSFSW-T-POWERLOSS` | 标量功率损失上限 | **0.05** | 1 | 标量 vs 空间模型的点源 detection power 相对损失 | `spatial_nonuniformity_gate_failed` | `FZ-DEGRADE-SCALAR`；`UNIFIED` §8 |
| `PSFSW-T-FLUXBIAS` | 标量通量偏差上限 | **0.01** | 1 | 标量 vs 空间模型的注入源 flux 相对偏差 | `spatial_nonuniformity_gate_failed` | `FZ-DEGRADE-SCALAR` |
| `PSFSW-T-WRANGE` | 帧权重动态范围 | **100** | 1 | `max_k W_psfsw,k / min_k W_psfsw,k` | 仅强制旗标 `weight_dynamic_range_exceeded` + 集中度报告 | 本任务（guard） |
| `PSFSW-T-COVMC` | covariance MC 一致性 | **0.03** | 1 | `abs(Var_analytic - Var_MC)/Var_MC` | REJECT | `SC-ADJ-P203` 验证门（rel<3%） |
| `PSFSW-T-BOOT` | 基线声明 bootstrap 重采样下限 | **200** | count | bootstrap 次数 | 声明无效 | 本任务/`SCI-PSFW-001` §4 |
| `PSFSW-T-CI` | 基线声明置信水平 | **0.95** | 1 | 配对 CI | 声明无效 | 本任务 |
| `PSFSW-T-EPSFTOL` | effective PSF 定义复算容差 | **1e-9** | 1 | `max abs(P_eff_op - P_eff_analytic)` | REJECT | 本任务（Oracle V7） |

## 2. 深度稳定性门（`PSFSW-T-DEPTH`/`-K`/`-SPAN`/`-RHO`）执行程序

**目的**：把 `SCI-PSFW-001` §7.3“改变不相关星表深度/检测阈值不得显著改变共同星集 PSFSW”变成可执行门。

1. 固定帧组 `G` 与**参考来源**（外部星表 或 参考叠加），固定帧测量结果不变；
2. 在参考来源上取 `K ≥ 5` 个深度扫描点（极限星等/检测阈值），跨度 ≥ 1.0 mag（或使 `n_common` 变化 ≥ 2 倍）；每点必须满足 `n_common ≥ 3`；
3. 对每点独立执行组内中值归一，得到 `W_t`（同一帧、同一分量公式、仅星集不同）；
4. 计算 `depth_scan_max_rel_dev` 与 `Spearman rho_s`；
5. 门：`depth_scan_max_rel_dev ≤ 0.05` **且** `abs(rho_s) ≤ 0.8`，否则 `unavailable(selection_bias_gate_failed)`。

**为什么 5% 能分离开**：W1 复核 Oracle K8 在同一合成星场上实测——样本派生的 PSFSW 代理随检测阈值移动 13.2%（方向/幅度不受控），`median(source SNR)` 移动 108.9%，而真值 `W_info` 位移 0（图像性质）。5% 卡在样本派生统计量的 13.2% 之下，把“参考来源固定”与“逐帧样本派生”分开；这是一条**先证伪、再声明**的阈值（负向控制 m14 注入 `per_frame_threshold_intersection` 必须使门红）。

**RHO = 0.8 的定位**：主判据是 5% 幅度上限；`rho_s` 是**结构性**补门，用于拒绝“幅度虽未超 5% 但单调随深度漂移”的系统行为（`K=5` 时 `rho_s=0.8` 的 Spearman 双尾 `p≈0.10`）。

## 3. 空间非均匀/趋势门（`PSFSW-T-NU`/`-TREND`/`-POWERLOSS`/`-FLUXBIAS`）

`FZ-DEGRADE-SCALAR` 要求帧级标量**同时**过 (a) 空间残差/趋势门与 (b) 功率损失门。本册落定数值：

- (a) 对 `signal`/`concentration`/`noise`/`background` 四分量分别在有效覆盖上计算 `(p95−p05)/p50` 与线性趋势幅度；任一 `> 0.30` 或趋势 `> 0.10` → 拆 region/tile 或 `unavailable(spatial_nonuniformity_gate_failed)`；
- (b) 标量相对空间模型对注入点源 detection power 的相对损失 `≤ 0.05` 且 flux 偏差 `≤ 0.01`；否则同上；
- LOW 档（`3 ≤ n_common < 10`）时非均匀阈值收紧至 **0.20**（样本更小，允许的空间可变性更小）；
- 不允许“先删科学信息再证明”：不满足时存 map/model/control points（`FZ-DEGRADE-SCALAR` 第 4 条）。

## 4. `n_common` 分档（`PSFSW-T-NMIN`/`-NROBUST`/`-NPREF`）

| 档 | 条件 | 生产许可 | 强制旗标 |
|---|---|---|---|
| HARD_FAIL | `< 3` | 否 | `reason=insufficient_valid_stars`，`weight_value=null` |
| LOW | `[3, 10)` | 是（受限） | `n_common_tier=low`、`confidence_class=low`、`bootstrap_ci_reported=true`、非均匀阈值 0.20、跨组比较禁止 |
| NORMAL | `[10, 30)` | 是 | `n_common_tier=standard` |
| PREFERRED | `≥ 30` | 是 | 无低计数旗标 |

硬下限 3 与 `SCI-P2-001` R5 对齐；10/30 是稳健分档（W1 只建议下限 3，分档为 W3 落定）。

## 5. 版本与复合常数（同属本任务落定）

```text
composite_version = "PSFSW-COMPOSITE-V1"
alpha=2, beta=1, gamma=2, delta=1, C_norm=1.0
truncation: fail_closed_first; component_floors = 1e-12 (S/Conc/N/B)
normalization: scope=group, estimator=median, median_target=1.0
```
`C_norm` 在组内中值归一下严格尺度简并（`PSFSW-G16`）：改变 `C_norm` 不得改变 `W_psfsw`。

## 6. W4 确认与签字

| 项 | 需要 | 处理 |
|---|---|---|
| 本册全部数值 | `CONTRACT-FREEZE-001`(W4) 正式冻结 | 本任务提交，不擅称已冻结 |
| `SO-07`（数值阈值/数据面/标定） | 负责人确认 | 只登记（`SCI-ADJ-001_FREEZE_LIST` §7） |
| `SO-02`（面亮度保持归一） | 负责人签字 | 上游输入口径，本任务不改 |
| `CTRL-F1`/`CTRL-AR033` | 控制器 | 只登记 |