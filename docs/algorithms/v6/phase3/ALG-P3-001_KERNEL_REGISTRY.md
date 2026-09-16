> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# ALG-P3-001 — Phase3 采样核 registry 规格（bilinear_4quad 注册前置独立 Oracle）

> 文档 ID：`ALG-P3-001-KERNEL-REGISTRY`
> 状态：`ALG_PROPOSED`（W4 正式冻结）
> 冻结条目：`FZ-P3-KERNEL-REGISTRY`；上位：`DESIGN-P3-001` §3:30-37；`SCI-P3-001-REVIEW`；ADJ-S4；`PROJECT_SPEC` §6。
> 机器可读：`run/v6/alg-p3/spec/alg_p3_spec.json`（`kernels` / `kernel_registry_rules`）；
> Oracle 证据：`run/v6/alg-p3/data/kernel_oracle.json`；检查器：`run/v6/alg-p3/tools/alg_p3_spec_check.py`。

## 0. 为什么核是产品语义

Phase3 的“投影 + 采样”是线性算子 `R`/`S`（`y=R x`、`f=S d`、`C_y=R C_x Rᵀ`、`pi=S p`）。
采样核决定逐像素值、通量语义、相关结构、effective PSF 与点源信息 `W`，因此**核是产品语义**，
不能由现有代码倒推冻结；未注册、未验证的核进入生产即 **REJECT**。

锚：`DESIGN-P3-001` §3:30-37；`PROJECT_SPEC` §6；`FZ-P3-KERNEL-REGISTRY`；ADJ-S4；C-P3-PROP-1/11。

## 1. 注册表（当前状态）

| kernel_id | status | 允许用途 | 通量/面亮度语义 | 误差界 | 边界 | 独立 Oracle | 生产默认 |
|---|---|---|---|---|---|---|---|
| `nearest` | `registered_restricted` | 离散 mask / 诊断 / 显式用户选择 | 仅离散选择，无插值 | 精确最近邻 | 缺 tile/越界 -> NaN | 结构性（无权重算术） | 否 |
| `bilinear_4quad` | `registered_with_oracle` | 连续场 | 四象限最近中心双线性，`Σw=1` | `(h²/8)(max|Fxx|+max|Fyy|)` | 缺 tile/越界 -> NaN，禁零填 | 是（`kernel_oracle.json` ok=true, independent=true） | 否 |
| `bilinear_area_overlap_exact` | `registered_with_oracle` | 连续场 | 精确球面面积重叠（`R`/`S` 定义式） | 无插值近似（解析重叠） | 重叠 0 贡献；无覆盖 -> NaN | 是（同一 Oracle：行/列归一 + 常量场） | 是 |
| `bicubic` | `requires_registration` | 连续场 | 未定 | 未定 | 未定 | 无 | 否 |
| `lanczos` | `requires_registration` | 连续场 | 未定（需 ringing 分析） | 未定 | 未定 | 无 | 否 |

锚：`DESIGN-P3-001` §3（“当前四象限最近中心双线性只能在独立 Oracle 和误差/边界定义后作为一个注册核”）；
`FZ-P3-KERNEL-REGISTRY`（“采样核是产品语义；bilinear_4quad 须先独立 Oracle+误差/边界定义才注册；
nearest 仅 mask/诊断/显式选择；高阶核各自注册带 Oracle”）。

## 2. 注册前置条件（缺一不可）

每个拟注册核必须提供：
1. 唯一 `kernel_id`；
2. 显式 `allowed_uses`（连续场 / 离散 mask / 诊断 / 显式选择）；
3. 通量或面亮度语义声明（对应 `R` 行归一或 `S` 列归一，二者不可混用）；
4. **误差界**（对光滑场的解析展开或严格上界）；
5. **边界定义**（缺 tile / 非有限 / 越界 / 跨 tile / wrap / 极点）；
6. **独立 Oracle 证据**：不调用生产实现、真值为解析式或固定种子 MC、`ok=true` 且 `independent=true`；
7. **负向 mutation** 使门变红（rc≠0）。

`spec_check` 的结构门会在以下任一情况拒绝：核已注册但缺 `oracle_evidence`、证据文件不存在或 `ok!=true`、
缺 `boundary_definition`、`nearest` 被设为科学默认、`bilinear_4quad` 等缺注册前置要求。

## 3. `bilinear_4quad` 的独立 Oracle（注册证据）

### 3.1 定义

“四象限最近中心双线性”：对输出像素中心 `(x,y)`，在每轴取夹住它的 2 个**输入像素中心**（`(j+0.5)h`），
得到 4 个输入样本，权重为线性距离分数 `w = [(1−fx)(1−fy), fx(1−fy), (1−fx)fy, fx·fy]`，`Σw=1`（FP64 显式语义）。

### 3.2 误差界

对 `C²` 连续场 `F`，网格间距 `h`：

```text
|F_interp − F| ≤ (h²/8) · ( max|∂²F/∂x²| + max|∂²F/∂y²| )
```

### 3.3 实测（`run/v6/alg-p3/data/kernel_oracle.json`）

测试场 `F = exp(−((x−x0)²+(y−y0)²)/(2s²))`，`s=3`、`h=1`、网格 141×141、输出在像素中心之间的整数位（误差最大处）：

| 量 | 实测 |
|---|---|
| `max_interp_err` | **0.0273955** |
| 解析上界 `(h²/8)(max|Fxx|+max|Fyy|)` | **0.0277778** |
| `err / bound` | **0.9862**（≤1，满足上界） |
| 权重和最大偏差 `max|Σw−1|` | **0.0** |
| 缺 tile/越界是否 fail-closed（NaN） | **true** |
| 零填引入误差 `zero_fill_error` | **1.3223**（证明零填必须被拒） |

### 3.4 负向 mutation（oracle 内）

| mutation | 注入 | 实测 |
|---|---|---|
| `kernel_bilinear_4quad` | 最近邻替代双线性 | `max_interp_err=0.14183`，`5.11×` 上界 → 红 |
| `kernel_bilinear_4quad__unnormalised` | 面积重叠权重不归一 | `max_interp_err=1.18836`，`42.78×` 上界 → 红 |
| `kernel_bilinear_4quad__zero_fill` | 缺 tile 以 0 填充 | `zero_fill_error=1.3223` → 红 |

结论：`bilinear_4quad` 的误差界、权重归一与边界 fail-closed 均有独立可复跑证据，**方可注册**；
但**不得**因此成为连续场的生产默认核（记为 `production_science_default=false`）。

## 4. `nearest` 的受限语义

- 仅允许：离散 mask、诊断、显式用户选择；
- **不得**作为连续科学场的默认核（`S-KRN-NEAREST`、`G-P3-KRN-02`）；
- 无插值权重算术，缺 tile/越界必须以 NaN + `coverage=0` 表达，禁零填。

锚：`DESIGN-P3-001` §3:30-37；`FZ-P3-KERNEL-REGISTRY`。

## 5. 高阶核（bicubic / lanczos 等）

各自注册并带独立 Oracle；`lanczos` 另需 ringing/负值分析；在注册前 `status=requires_registration`，
进入生产即 **REJECT**（`G-P3-KRN-01`）。

## 6. 验证命令与退出码

```text
python3 run/v6/alg-p3/tools/alg_p3_oracle.py run                    # rc=0，含 kernel_bilinear_4quad PASS
python3 run/v6/alg-p3/tools/alg_p3_oracle.py mutate kernel_bilinear_4quad               # rc=1
python3 run/v6/alg-p3/tools/alg_p3_oracle.py mutate kernel_bilinear_4quad__unnormalised # rc=1
python3 run/v6/alg-p3/tools/alg_p3_oracle.py mutate kernel_bilinear_4quad__zero_fill    # rc=1
python3 run/v6/alg-p3/tools/alg_p3_spec_check.py check             # rc=0（注册证据/边界/用途结构门）
python3 run/v6/alg-p3/tools/alg_p3_spec_check.py mutate S2-bilinear-no-oracle           # rc=1
python3 run/v6/alg-p3/tools/alg_p3_spec_check.py mutate S15-bilinear-missing-boundary   # rc=1
```
