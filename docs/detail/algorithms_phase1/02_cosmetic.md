# 插件文档：cosmetic（坏点/坏列修正）

> 上游：ASTROCS_DESIGN.md §4.2（Phase1 节点流程）

## 1. 职责与边界

- **职责**：生成/应用坏点（hot/cold 像素）、坏列与异常像素的修正与 validity 标志。**不做宇宙线（CR）剔除**——单帧 CR 剔除不在本模块生产域（ALG-COS 冻结正本未选型：`docs/science/algorithms/COSMETIC_ALGORITHMS.md` 明文 CR 文献仅作领域背景，坏点检测选型 = Project-defined）；生产实现（`lib/algorithms/calibration/src/cosmetic_corrector.cpp`）无 CR 检测路径。
- **不是**：不做背景估计；不做排异（排异是 Phase2 的推断）；修正必须可追溯、可回退。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §4.6（硬约束：cosmetic/validity）
- `docs/detail/PHASE1_DETAILED_DESIGN.md` §5（有效性域）
- `docs/science/UNCERTAINTY_AND_COVARIANCE.md`（修正对协方差的影响）
- `docs/science/algorithms/COSMETIC_ALGORITHMS.md`（ALG-COS 冻结正本：检测统计/插值修复口径）

## 3. 输入/输出数据合同

- **输入**：定标信号 `y`、cosmetic map、饱和/非线性状态、坏点列表（可含注入）。
- **输出**：修正后信号（`cleaned_<base>`；非坏点逐像素恒等，坏点 = 插值或原值回退）、坏列掩膜（`badcol_<base>`，取值语义 1=已修 / 2=仅标记）、坏点计数（`out_hot`/`out_cold`，结构过滤后掩码像素数）；坏列 provenance（列索引、已修列数与仅标记列数分账）与计数写入 manifest。
- 参考：`docs/science/DATA_SEMANTICS.md` §10（DATA-P1-COS：模块输入/输出数据合同正本）。

## 4. 算法与公式要点

- 坏点/热像素：由 master cosmetic map 或暗场统计判定；修正值来源（中值/插值/相邻）必须记录；
- **宇宙线（CR）剔除不在本模块职责域**（见 §1）；生产实现职责面 = hot/cold 像素检测（dark/bias 统计 median±k·MAD + 结构过滤）、坏列检测与修复（判据 = 逐列中位数 `cs[x]` 的一阶差分 `d[x] = cs[x] − cs[x−1]` 的稳健跳变：`σ_d = 1.482602218505602·MAD(d)`、阈值 `column_sigma`，反号跳变按相邻距离就近配对成段，段长 ≤ 2k−1 且不满宽判坏；修复 = 段外锚点线性插值，贴边退化为单侧复制）、5×5 中值/距离反比加权插值；
- validity 类型：NaN/Inf、坏点、饱和、cosmetic、边界、插值、星轨/严重形变；
- 修复列的方差面走独立入口 `ac_column_variance_inflate`：按修复算子自身的插值权重膨胀（`var = (Σw²·var)·κ`，κ 由调用方给出、默认保守取 2.0），只对掩膜值为「已修」的列施加；干净列与「仅标记」列逐位拷贝输入方差。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `cosmetic_map_path` | —— | —— | master cosmetic map |
| `saturation_level` | —— | ADU/e⁻ | 饱和阈值（元数据或显式） |
| `cr_detection` | true | —— | **配置面死键**：本键当前无行为承载，生产路径不读取，存废走变更流程 |
| `cr_sigma` | 5.0 | σ | **配置面死键**：本键当前无行为承载，生产路径不读取，存废走变更流程 |
| `interpolation` | `neighbor` | —— | 修正插值方式 |

## 6. 接口/ABI

- entrypoint：定标信号 + cosmetic map（master_dark/master_bias）→ 修正信号 + 坏列掩膜 + 坏点计数；
- 所有权按 C ABI；坏列 provenance 与计数写入 manifest。

## 7. 错误与边界

- 饱和态不进入本模块输入面也不被改写：饱和像素的识别与表示由上游（定标/检测侧）承载，本模块只按 dark/bias 统计与列跳变判据判坏点/坏列；
- 修正记录（被修列索引、方法、已修与仅标记计数分账）写入 manifest 的逐帧 provenance 块；
- 极端情形（大片坏区）一律显式登记：零值填充只作具名降级。

## 8. 测试与 Oracle

- 注入已知坏点/坏列，验证检测率与修复正确性；
- 方差更新 Oracle（插值处方差符合理论）；
- 修正可回退性测试；
- validity 传播到下游产品测试。
