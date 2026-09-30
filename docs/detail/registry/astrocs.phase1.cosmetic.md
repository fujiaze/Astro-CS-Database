# 模块 acsd.phase1.cosmetic

> 上游：docs/ACSD_DESIGN.md §8.5（模块与 ABI）
> 科学正本：docs/science/algorithms/COSMETIC_ALGORITHMS.md（ALG-COS-001..005）
> 数据正本：docs/science/DATA_SEMANTICS.md §10（DATA-P1-COS）、
> docs/science/UNCERTAINTY_AND_COVARIANCE.md（修正对协方差的影响）
> API 正本：docs/engineering/PUBLIC_API.md（API-COS-001）、
> docs/engineering/PHASE1_API_V1.md（API-P1-002）

合同三件套落位 `lib/algorithms/cosmetic/`（README/module.yaml/memory.md）；
descriptor 词汇 module_id=`acsd.phase1.cosmetic`（`p1_cosmetic_descriptor`）为
编排层口径；模块级事实以三件套与现行生产实现
`lib/algorithms/calibration/src/cosmetic_corrector.cpp` 为准。

## 职责与明确非职责

生产模块登记（唯一源 = descriptor）。职责：坏点（hot/cold 像素）检测与修复、
坏列检测与修复、异常像素修正与 validity 标志——ALG-COS-001..005。

不做：master 生成/校准算术（P1-CAL）；FITS 读写（astro_image_io）；背景估计；
排异（Phase2 的推断）；参数接线决策（调用方）。**单帧宇宙线（CR）剔除不在本
模块生产域**——ALG-COS 冻结正本未对 CR 剔除选型（正本中 CR 文献仅作领域背景，
坏点检测选型 = Project-defined），生产实现无 CR 检测路径。修正必须可追溯、
可回退。

现状生产调用 `p1_session.cpp` 未接线母版（检测全禁用、恒等 pass，登记见
COSMETIC_ALGORITHMS.md §10）。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `calibrated` | `DATA-P1-CAL` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `cleaned` | `DATA-P1-COS` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

invalid = NaN（透传，不判坏）；掩码极性 1 = 坏点（SCI-CAL-001 §9a）；数据
语义 DATA_SEMANTICS §10（DATA-P1-COS，DATA-P1-COSMETIC 为 descriptor 占位名，
合同以 DATA-P1-COS 为准）。

输入面：定标信号 `y`、cosmetic map、饱和/非线性状态、坏点列表（可含注入）。
输出面：修正后信号（非坏点逐像素恒等，坏点 = 插值或原值回退）、坏列掩膜
（取值语义 1 = 已修 / 2 = 仅标记）、坏点计数（`out_hot` / `out_cold`，结构
过滤后掩码像素数）、坏列 provenance（列索引、已修列数与仅标记列数分账）与
计数入 manifest 逐帧 provenance 块。

### 数值落地口径

检测与修复的统计口径正本 = ALG-COS-001..005；本页只记落地判据：

- 坏点：按 dark/bias 统计的 median±k·MAD 判定，叠加 8 连通结构过滤；
- 坏列：取逐列中位数 `cs[x]` 的相邻差分 `d[x]`，以
  σ_d = 1.482602218505602·MAD(d[x]) 为尺度、阈值 `column_sigma` 判稳健跳变；
  反号跳变按相邻距离就近配对成段，段长 ≤ 2k−1 且不满宽才判坏；修复 = 段外
  锚点线性插值，贴边退化为单侧复制；
- 像素插值：5×5 中值 / 距离反比加权；
- validity 类型：NaN/Inf、坏点、饱和、cosmetic、边界、插值、星轨/严重形变；
- 修复列的方差面走独立入口 `ac_column_variance_inflate`：按修复算子自身的
  插值权重膨胀方差，膨胀系数 κ 由调用方给出、默认保守取 2.0，只对掩膜值为
  「已修」的列施加；干净列与「仅标记」列逐位拷贝输入方差。

## 公共 header、核心 symbol 与生命周期

模块级：API-COS-001（docs/engineering/PUBLIC_API.md，`ac_correct_frame` /
`ac_correct_frame_f64` / `ac_set_num_threads`，头
lib/algorithms/calibration/include/astro_calibration.h）；编排级：API-P1-002
（PHASE1_API_V1 §2，生命周期 create→validate→run→inspect→destroy，
多模块共享）。目标交付形态 acsd_p1_cosmetic.dll + C ABI adapter（entrypoint
未落地）。

entrypoint = 定标信号 + cosmetic map（master_dark / master_bias）→ 修正信号 +
坏列掩膜 + 坏点计数；所有权按 C ABI 规则，坏列 provenance 与计数由调用方写入
manifest。

## Registry descriptor 与配置 schema

module_id=`acsd.phase1.cosmetic`; execution_class=`cpu_heavy`;
parallel_ok=True; 目标交付形态 acsd_p1_cosmetic.dll。

descriptor 侧配置 = cosmetic JSON（`enabled` / `hot_sigma` / `cold_sigma` /
`method` / `max_structure_size`，由 p1_session.cpp 校验；正式版本化 schema
待落地）。算法级字段面：

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `cosmetic_map_path` | —— | —— | master cosmetic map |
| `saturation_level` | —— | ADU/e⁻ | 饱和阈值（元数据或显式） |
| `cr_detection` | true | —— | 无行为承载：单帧 CR 剔除不在本模块生产域，生产路径不读取该键 |
| `cr_sigma` | 5.0 | σ | 无行为承载：同上 |
| `interpolation` | `neighbor` | —— | 修正插值方式 |

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是（资源门拒绝 heavy+serial 组合）; worker 数 =
ThreadBudget.max_workers（禁 hardware_concurrency）。现状并行 = OpenMP 像素域
schedule(static)（ThreadLease 迁移整改点）。

确定性 = 输出 bitwise 与线程数无关（逐像素独立 + 固定遍历顺序）。

## 内存/cache/I-O/所有权

无 cache; 内存 O(n) 额外（检测统计复制 + labels/sizes 向量）; I-O 零文件/网络。

所有权 = 调用方分配 buffer（out / out_hot / out_cold）。

## 错误、日志、指标、取消和 checkpoint

错误码 = `AC_OK` / `AC_ERR_PARAM`（模块级，`ac_correct_frame`；`AC_ERR_MEMORY` /
`AC_ERR_INTERNAL` 死值）；编排级 `ACS_ERR_INTERNAL`（session 映射
rc != AC_OK）。极端情形（大片坏区）一律显式登记：零值填充只作具名降级。

饱和态不进入本模块输入面也不被改写：饱和像素的识别与表示由上游（定标/检测
侧）承载，本模块只按 dark/bias 统计与列跳变判据判坏点/坏列。

无日志（cosmetic 路径零 stderr）。取消 = 帧粒度（session 层；模块内无检查点）；
无 checkpoint。

## 独立 synthetic 验证命令与容差

`TEST-COS-DESIGN-001`（docs/science/algorithms/COSMETIC_ALGORITHMS.md §9：合成
fixture FIX-COS-A..F、NumPy oracle rtol=1e-6/atol=1e-7、解析解 bitwise、I1–I6
不变量）；可执行 `TEST-P1-COS-001` 待建；执行证据 NOT_VERIFIED（验收证据待补）。

Oracle 面：

- 注入已知坏点/坏列，验证检测率与修复正确性；
- 方差更新 Oracle（插值处方差膨胀符合理论）；
- 修正可回退性测试；
- validity 传播到下游产品测试。

## 已知限制

- 缺陷与现行语义的登记面 = COSMETIC_ALGORITHMS.md §10；模块级清单见
  `lib/algorithms/cosmetic/README.md`；
- 现状生产调用未接线母版，检测全禁用、恒等 pass；
- 现状无 ThreadLease，并行度取进程默认 team；
- `cr_detection` / `cr_sigma` 为无行为承载的配置键；
- 目标交付形态 acsd_p1_cosmetic.dll + C ABI adapter 的 entrypoint 未落地；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
