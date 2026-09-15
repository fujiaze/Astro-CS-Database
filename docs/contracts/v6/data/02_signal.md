# 02 — signal 对象 schema（Phase1 Drizzle 面亮度 / Phase2 马赛克 / Phase3 平面）

上位锚：`FZ-FORMULA-DRIZZLE-SB`、`FZ-GATE-CONST-SB`、`FZ-COND-FLUX-CONSERV`、`FZ-DEGRADE-SCALAR`；
DESIGN-P1 §9:120-126；UNIFIED §7；ADJ-F-OBS-02/S2/S3；宪章 §4.1/§5.3。
机器：`contracts/proposals/v6/data/astrocs.v6.signal.v1.schema.json`（`signal.v1`）；正例 `examples/signal.example.json`。

## 1. 对象身份

`signal` = 声明单位和像素语义的科学估计量，**不是**权重（宪章 §4.1；UNIFIED §3）。
禁止用一个模糊 `value/weight` 字段承载 signal 与 variance（宪章 §4.1；PROJECT_SPEC §7）。

## 2. 字段规格

| 字段（JSON path） | 类型 | 单位 | 适用域 | fail-closed | 条款锚 |
|---|---|---|---|---|---|
| `signal_schema` | const `astrocs.v6.signal/v1` | — | 全部 | 不匹配即不适用 | 宪章 §4.1 |
| `product_family` | enum | — | Phase1/2/3 | product_family 未声明/与输出模式矛盾 → REJECT | DESIGN-P2 §9；DESIGN-P3 §1 |
| `units` | enum | `ADU/px^2`/`ADU`/`BUNIT(声明)` | 全部 | 缺失或不可判 → unavailable | `FZ-UNIT-SIGNAL-SB`；`FZ-BUNIT-SEMANTICS` |
| `pixel_semantics` | enum | — | 全部 | BUNIT=ADU 且缺此声明 → 单位不可判 REJECT | `FZ-BUNIT-SEMANTICS` |
| `pixel_area_power` | int | — | 全部 | SB 要求 −2、flux 要求 0，否则 REJECT | `FZ-BUNIT-SEMANTICS` |
| `formula_ref` | const | — | Phase1 Drizzle | 归一版本为 legacy 时须显式声明 | `FZ-FORMULA-DRIZZLE-SB` |
| `normalization.version` | string | — | Phase1 | 缺失 → REJECT | SC-ADJ-F02；ADJ-GEN-03 |
| `normalization.kind` | enum | — | Phase1 | legacy_adrop_forward 禁用于绝对面亮度 | ADJ-F-OBS-02 |
| `normalization.pixfrac` | number ∈ (0,1] | — | Phase1 | 超出 (0,1] → REJECT | ADJ-F-OBS-02 |
| `normalization.flux_conservation_factor` | number > 0 | — | Phase1 flux | 缺且用于绝对通量 → REJECT | `FZ-COND-FLUX-CONSERV` |
| `representation` | enum map/model/control_points/scalar | — | Phase1/2 | scalar 未过分位数+双门 → REJECT | `FZ-DEGRADE-SCALAR` |
| `dtype` | enum float32/float64 | — | 全部 | — | 宪章 §5.3 |
| `invalid_policy` | const `nan_or_support_le_0` | — | 全部 | 0 填充/静默丢弃 → REJECT | DATA_SEMANTICS §4/§30.4 |
| `degradation` | object | 各量自身单位 | 仅 scalar | 缺 p05/p50/p95/双门 → REJECT | `FZ-DEGRADE-SCALAR` |
| `constant_field_oracle` | object | — | Drizzle 验收 | 常量 ADU 构造/无条件 pixfrac/S_p=F_p 三错法必红 | `FZ-GATE-CONST-SB` |
| `provenance_ref` | string | — | 全部 | 悬空引用 → REJECT | `FZ-PROV-MINIMAL-SET` |

## 3. 冻结公式（原样继承）

```text
FZ-FORMULA-DRIZZLE-SB:  S_p = Sum_j B_j a_jp / Sum_j a_jp,  B_j = x_j / A_pixel,j
                        （等价组合系数 c_jp = a_jp / Sum_j a_jp）
FZ-FORMULA-DRIZZLE-VAR: variance_p = Sum_j v_j w_jp^2 / D_p^2;  ivar_p = 1/variance_p
FZ-GATE-CONST-SB:       按 B0 构造 x_j = B0*A_pixel_j; S_p = B0 对全部 pixfrac in (0,1];
                        容差 |S_p/B0 - 1| < 1e-3（沿用，不改）
FZ-COND-FLUX-CONSERV:   pixfrac=1: Sum_p F_p = Sum_j x_j 严格;
                        pixfrac<1: 总输出通量 = pixfrac^2 * Sum_j x_j,
                        provenance.flux_conservation_factor = pixfrac^2
```

通量守恒是**条件不变量**：pixfrac<1 时必须显式使用 `flux_conservation_factor`，否则孔径/总通量换算发生 `1/pixfrac^2` 光度零点偏差（ADJ-F-OBS-02 失效域）。

## 4. 三产品族要点

| 产品族 | signal 语义 | 单位 | 备注 |
|---|---|---|---|
| `phase1_drizzle_surface_brightness` | 面亮度 | `ADU/px^2` | 面亮度保持归一（(B)）；常量面亮度门全 pixfrac |
| `phase2_mosaic_surface_brightness` | 面亮度 | `ADU/px^2` | 与 UPM 加性背景分离；variance 从实际组合系数 |
| `phase2_mosaic_per_pixel_flux` | 逐像素通量 | `ADU` | 与面亮度**不得互相替代**（宪章 §4.1） |
| `phase3_plane_surface_brightness` | `y = R x` 行归一 | 面亮度单位 | 常数场不变量要求行归一 |
| `phase3_plane_flux` | `f = S d` 列归一 | 通量单位 | 逐像素立体角 Omega 必需（F3-03） |

## 5. fail-closed 与负向门

正向：按面亮度 B0 构造的常量场在全部 pixfrac 下 `S_p=B0`（|S_p/B0−1|<1e-3）。
负向（须 rc≠0，见 `09_verification.md`）：
- 用每像素常量 ADU `x_j=C` 直接断言 `S_p=C`；
- 把常量面亮度不变量写成对任意 pixfrac 无条件成立；
- 把 `S_p` 定义为 `F_p`（漏 `D_p` 归一）；
- scalar 降级缺分位数或功率损失门；
- BUNIT=ADU 但 provenance 无 `pixel_semantics/pixel_area_power`。

## 6. 迁移建议

- 生产 `phase_product_exchange.schema.json` 的 plane 枚举仅 `{signal,support,variance,ivar,mask}`：V6 目标态的面亮度/signal 语义不变，
  但须补 `pixel_semantics/pixel_area_power`（W6 集成，附 F-UNC-003 联动约束）。
- Drizzle legacy 归一（`w_jp=a_jp/A_drop`）产品读取侧必须区分 `normalization.version`；缺则该产品不可跨 pixfrac 合成（SO-02）。
