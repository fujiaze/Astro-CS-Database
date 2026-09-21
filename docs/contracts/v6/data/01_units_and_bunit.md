> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# 01 — 单位表与 BUNIT 语义（frozen units / FZ-UNIT-* / FZ-BUNIT-SEMANTICS）

> 上游：ASTROCS_DESIGN.md §3.1（数据对象）、§8.4（模块与 ABI）

上位锚：`SCI-ADJ-001_FREEZE_LIST.md` §1/§3.2；`reports/v6/science-adjudication/adjudications.json` `units_table`/`freeze_table`；
`docs/science/v6/observation/OBSERVATION_MODEL_REVIEW.md` §7 门 U1/U2；`docs/contracts/DATA_SEMANTICS.md` §30.4；宪章 §4.1/§5.3；UNIFIED §7。
机器：`contracts/proposals/v6/data/astrocs.v6.units.v1.schema.json`（`units.v1`）。

## 1. 冻结单位表（原样继承，不得改名/改单位）

| 符号 | 单位 | 含义 | 方差单位 | ivar 单位 | 冻结条目 |
|---|---|---|---|---|---|
| `signal_sb` | `ADU/px^2` | Phase1 Drizzle/HiPS 面亮度 signal | `ADU^2/px^4` | `px^4/ADU^2` | `FZ-UNIT-SIGNAL-SB` |
| `pixel_variance_in` | `ADU^2` | 输入源像素逐像素方差 v_j | — | — | `FZ-UNIT-VAR-IN` |
| `sb_variance_out` | `ADU^2/px^4` | Phase1 输出面亮度方差 variance_p | — | — | `FZ-UNIT-VAR-SB` |
| `sb_ivar_out` | `px^4/ADU^2` | Phase1 输出 ivar | — | — | `FZ-UNIT-IVAR-SB` |
| `W_info` | `ADU^-2` | 点源信息权重 = 1/Var(F_hat) | — | — | `FZ-UNIT-WINFO` |
| `Q` | `ADU^-1` | 点源线性充分统计量 | — | — | `FZ-UNIT-Q` |
| `flux` (F_hat) | `ADU` | 点源通量估计 | `ADU^2` | `ADU^-2` | `FZ-UNIT-FLUX` |
| `psfsw_robust_weight` | `1` | 无量纲组内相对复合权重 | — | — | `FZ-UNIT-PSFSW` |
| `phase2_mosaic_signal` | `BUNIT(声明)` | Phase2 马赛克 signal；面亮度产品则 `ADU/px^2` | `BUNIT^2` | `1/BUNIT^2` | `FZ-UNIT-SIGNAL-SB` |
| `phase3_var_out` | `BUNIT^2` | Phase3 输出方差 = 主 HDU BUNIT 平方 | — | `1/BUNIT^2` | `FZ-P3-BUNIT-QUADRATIC` |

**二次律（`FZ-P3-BUNIT-QUADRATIC`）**：`variance = signal^2`、`ivar = 1/variance`；Phase3 输出 variance BUNIT = (主 HDU signal BUNIT)²。
W_info 严格为 `signal^-2`（`FZ-UNIT-WINFO`）；`psfsw_robust_weight` 严格无量纲 = `1`（`FZ-UNIT-PSFSW`）。

量纲代数自洽（门 U1，OBSERVATION_MODEL_REVIEW §7）：`x_j = B·A_pixel`、`S_p = F_p/D_p`、`variance_p = v·w²/D²` 三条链一致。
其中 `Q_k = [signal]^-1`、`W_info = [signal]^-2`、`F_hat = [signal]`、`Var(F_hat) = [signal]^2`。

## 2. 适用域

- 以上单位适用于**全部** Phase1/2/3 产品、合同与 schema；单位串不因 Phase 改变，只有 BUNIT 声明方式不同。
- `ADU` 是 canonical 例示（`e^-` 等标度同构，但必须在 provenance 声明，不得静默替换）。
- 混合积分通量/面亮度、未知单位、缺必要响应的输入必须被拒绝（`DESIGN-P2-001` §1；`DESIGN-P3-001` §1）。

## 3. BUNIT 语义（`FZ-BUNIT-SEMANTICS`）

写盘 BUNIT 必须**量纲可判**，满足 (a) 或 (b)：

```text
(a) BUNIT 显式含 px 幂次: canonical "ADU/px^2"(signal SB) 与 "ADU^2/px^4"(variance SB)
(b) BUNIT = "ADU" 时 provenance 必须声明 pixel_semantics = "surface_brightness"
    且 pixel_area_power = -2 且给出目标像素面积
```

仅写 `ADU` 而无 (b) 声明 = 单位不可判 → 产品标 `unavailable` 或 `REJECT`。

`pixel_area_power` 的 canonical 缺省（`units.v1.bunit_semantics.pixel_area_power_defaults`）：

| 量 | pixel_area_power |
|---|---|
| signal_sb | `-2` |
| sb_variance_out | `-4` |
| sb_ivar_out | `+4` |
| flux / Q / W_info / psfsw_robust_weight | `0` |

> 说明：FREEZE_LIST §3.3 只显式冻结 `pixel_semantics=surface_brightness + pixel_area_power=-2`（signal）。方差/ivar 的幂次由二次律唯一导出（−4/+4），
> 本表把它写全以便量纲门可执行；不改变 signal 的冻结值。

## 4. fail-closed 条件

| 条件 | 处置 | 门 |
|---|---|---|
| signal 缺 units 或 BUNIT 不可判 | `unavailable`/`REJECT` | `G-BUNIT-SEMANTICS` |
| `surface_brightness` signal 的 pixel_area_power ≠ −2 | REJECT | `G-PIXEL-AREA-POWER` |
| variance BUNIT ≠ (signal BUNIT)² | REJECT | `G-BUNIT-QUADRATIC`（`FZ-P3-BUNIT-QUADRATIC`） |
| W_info 单位 ≠ `ADU^-2` | REJECT | `G-WINFO-UNIT` |
| psfsw_robust_weight 单位 ≠ `1`（出现 flux^-2/ivar） | REJECT | `G-PSFSW-UNIT` |
| 缺 `flux_conservation_factor` 却用于绝对通量/aperture | REJECT | `G-FLUX-CONSERV-FACTOR`（`FZ-COND-FLUX-CONSERV`） |

## 5. 迁移建议

- 历史 `DRIZZLE.md` §3 把输入 `v_j(ADU^2)` 与输出 `variance_p(ADU^2/px^4)` 同名写作 `variance`：V6 目标态以 §5 推导为准，
  改名 `pixel_variance_in` / `sb_variance_out`；属 `SO-01`，只登记不擅改冻结正文。
- 现有产品以 `BUNIT=ADU` 写在盘上而无 pixel 语义：读取侧必须要求 provenance 补 `pixel_semantics/pixel_area_power`，否则标 `unavailable`。
