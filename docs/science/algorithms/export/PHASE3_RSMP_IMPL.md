# Phase3 HiPS Resample 实现级算法合同（ALG-P3-RSMP-IMPL-001）

> 上游：ACSD_DESIGN.md [D-1]「流程」一节（export 流程）

> 上游 SCI: SCI-P3-001（docs/science/PHASE3_HIPS_TO_FITS.md，
> FROZEN，零改动；矩阵行 science_id 引用的
> SCI-P3-RES-001 为 MISSING 占位，本域全部科学内容映射声明为
> SCI-P3-001（映射声明见本节）。承接既有 ALG-P3-003 本域子面（G3/G4 施工规格，
> docs/science/algorithms/PHASE3_RESAMPLE.md，公式零改动）。
> 本文档为 HiPS 重采样域**实现级合同**：逐符号源码行号锚定 + 冻结公式 +
> 错误语义 + 并发/确定性合同 + TEST 设计冻结 + 实测偏差登记。
> 生产源: lib/algorithms/resample/p3_resample.h（现 204 行）+
> lib/algorithms/resample/p3_resample.cpp（现 606 行）。
> 本层含 `P3SampleRejection` 与 `p3_sample_bilinear_nanmask_ex`（NaN 样本掩膜
> 与对齐口径），逐项登记见 [S-1]「参数与常数」一节。
> 代码中的数学核心唯一 = healpix 权威函数（见 §6）。

## 1 目的与非目标

- 目的: 冻结 Phase3 HiPS tile 重采样域的全部公共消费面——sampler
  生命周期（open/open_ex/set_max_tiles/close）、order 选择
  （p3_order_select）、输入模式守卫（p3_resample_check_mode）、
  NEAREST/BILINEAR 逐输出像素采样（含跨 tile 邻域读取）、tile 缓存——
  作为 P3-RSMP-IMPL/TEST/INT 的合同基线。
- 非目标: 不把 variance/weight/ivar 作为 **signal 的输入模式**（`p3_resample_check_mode`
  拒：`weight`/`flux-per-pixel` ⇒ `P3_RS_UNSUPPORTED`（SCI [S-1]「判据与误差」一节）；`variance`/`ivar`
  依DATA-P3-UNC-001 [S-1]「判据与误差」一节已**转 uncertainty 子产品消费面**
  `p3_uncertainty_open/close/propagate`，故非「不实现」）；support 输入与
  flux-per-pixel 面积换算仍不实现（SCI[S-1]「判据与误差」一节冻结项）；不做 alpha channel/BLANK int tile
  （SCI [S-1]「判据与误差」一节 显式拒）；不改投影（G1/G2 属 ALG-P3-PROJ-IMPL-001）与 FITS 原子写
  （G5 属 writer 域）；不引入第三种采样核（nearest/bilinear 之外显式拒）。
- 本层为合同冻结层：只登记，不修生产码；缺陷走DISP登记，
  由实现层与集成层整改。

## 2 身份与落位

| 项 | 合同值 | 实测 |
|---|---|---|
| module_id（矩阵 CSV 权威） | acsd.p3.resample | MODULE_MIGRATION_MATRIX.csv P3-RSMP 行 |
| registry 生产 descriptor | acsd.phase3.resample2（`lib/infrastructure/scheduler/src/module_adapters.cpp` 的 `p3_resample2_descriptor`，module_id 同名） | 实测 |
| dll_target | acsd_p3_resample.dll | 矩阵 CSV；entrypoint 实测未建（DISP-P3RSMP-005） |
| owner | SA-P3-S26 | 矩阵 CSV |
| 合同目录 | lib/algorithms/resample/（本域合同文件，无源码） | 生产源仍在 lib/phase3_session/ |
| 依赖 | lib/algorithms/shared/healpix（leaf_to_tile_nest/tile_to_leaf_nest/nested_local_to_fits_index/ang2pix/pix2ang）、lib/algorithms/coverage/hips_properties.h 与同目录 hips_properties.cpp（properties 严格校验经 p3_sampler_open 间接消费） | [S-1]「公式与推导」一节 |
| 下游 | P3-RSMP-IMPL（实现）、P3-RSMP-TEST（可执行测试）、P3-RSMP-INT（descriptor 对齐 acsd.p3.resample） | — |

- 静态库落位: acsd_phase3_session（根 `CMakeLists.txt`，
  p3_resample.cpp 为五源文件之一）。
- 会话编排消费: lib/phase3_session/p3_session.cpp（[S-1]「参数与常数」一节逐点锚定）。
- descriptor 端口绑定（`lib/infrastructure/scheduler/src/module_adapters.cpp`）: 输入
  DATA-P3-WCS（必需）+ DATA-HIPS-001（必需），输出 DATA-P3-RES
  （可选产出）。

## 3 生产源图（实测）

```text
lib/algorithms/resample/p3_resample.h        202 行  唯一权威签名头（ALG-P3-003 施工面 11 个
                                                     公共符号，[S-1]「参数与常数」一节；另有 P30 缓存观测面与
                                                     DATA-P3-UNC-001 不确定度面符号，[S-1]「参数与常数」一节 表不重复列）
lib/algorithms/resample/p3_resample.cpp     586 行  全部实现（[S-1]「与上下游的关系」一节 逐符号）
lib/algorithms/resample/tests/p3rsmp/p3_nan_mask_test.cpp  437 行  样本级掩膜Oracle+负例（[S-1]「判据与误差」一节）
lib/phase3_session/p3_session.cpp      441 行  会话编排消费（[S-1]「与上下游的关系」一节）
lib/algorithms/shared/healpix/healpix_core.{h,cpp}         权威球面函数（第二套核心恒不接受）
（探针 / 独立参考实现 / 并行域 / coverage 语义四组判据的载体文件**均已不在本仓**；
  判据本体见 [S-1]「判据与误差」一节，引用时只引判据，不引载体路径。）
```

- p3_resample.cpp 内部结构（按符号名定位，行号仅作导航）: `kTileWidth=512`（编译期常量，SCI [S-1]「判据与误差」一节 tile 宽冻结一致）；`kReaderBuf=512*512`（FITS 读缓冲）；
  `TileCache`（FIFO 最旧逐出有界缓存，§7）；匿名命名空间
  辅助；`read_leaf`；`p3_order_select`；
  `p3_resample_check_mode`；`P3SamplerImpl`（open/open_ex/
  set_max_tiles/close）；`p3_sample_bilinear`；
  `p3_sample_nearest`。

## 4 符号冻结（实测签名，不改码）

lib/algorithms/resample/p3_resample.h 全部公共符号（ALG-P3-003 施工面 11 个；本次新增
`P3SampleRejection` 与 `p3_sample_bilinear_nanmask_ex`，其余 9 个签名零改动）：

| 符号 | 声明（p3_resample.h） | 实现（p3_resample.cpp） | 签名要点 |
|---|---|---|---|
| `P3ResampleStatus` | `lib/algorithms/resample/p3_resample.h` | — | enum: `P3_RS_OK=0 / P3_RS_PARAM=1 / P3_RS_UNSUPPORTED=2 / P3_RS_IO=3` |
| `p3_order_select` | 同上 | 同上 | `(int max_order, double scale_deg_per_px, int* out_order)` |
| `p3_resample_check_mode` | 同上 | 同上 | `(const char* input_mode)`；`surface_brightness` 唯一合法 |
| `P3SamplerImpl` | 同上（前置声明） | 同上（匿名命名空间辅助等） | 不透明实现类型 |
| `P3Sampler` | 同上 | — | `P3SamplerImpl* impl=nullptr;`（错误文本是 `std::string* err` / 内部状态，不存在公共 last_error[256]） |
| `p3_sampler_open` | 同上 | 同上 | `(const char* hips_dir, P3Sampler* out, char* err)`；= open_ex 缺参薄封装 |
| `p3_sampler_open_ex` | 同上 | 同上 | `(const char* product_dir, P3Sampler* out, int* out_order, std::string* out_bunit, std::string* err)`（**std::string***，非 `char*`） |
| `p3_sampler_set_max_tiles` | 同上 | 同上 | `(P3Sampler*, int max_tiles)`；≤0 恢复默认 8 |
| `p3_sample_nearest` | 同上 | 同上 | `(P3Sampler*, double ra_deg, double dec_deg, float* value, int* coverage)`（= `_ex` 薄封装） |
| `p3_sample_nearest_ex` | 同上 | 同上 | 同上 + `uint64_t* leaf_ipix`（nearest 方差传播面） |
| `p3_sample_bilinear` | 同上 | 同上 | `(P3Sampler*, double ra_deg, double dec_deg, float* value, int* coverage)`（= `_ex` 薄封装） |
| `p3_sample_bilinear_ex` | 同上 | 同上 | 同上 + `double weights[4]` + `uint64_t leaf_ipix[4]` |
| `p3_sampler_attach_cache` | 同上 | 同上 | `(P3Sampler* dst, const P3Sampler* src)`：共享有界 LRU（P30；§7） |
| `p3_sampler_cache_stats` | 同上 | 同上 | `(const P3Sampler*, P3CacheStats*)`：命中率/负缓存/逐出可观测计数 |
| `p3_sampler_set_absent_cache` | 同上 | 同上 | 负缓存开关（默认开；回归阴性对照用） |
| `p3_uncertainty_open` / `p3_uncertainty_close` / `p3_uncertainty_propagate` | 同上（三处声明） | 同文件 | DATA-P3-UNC-001 不确定度消费面（variance/ivar 子产品） |

- **签名漂移订正（本次）**：本表早期版本把采样函数记为
  `(const P3Sampler*, const P3WcsDescriptor*, int x, int y, float* value, float* coverage)`
  （像素坐标入参、coverage 为 `float*`）——现行公共面是**天球坐标入参**
  `(P3Sampler*, double ra_deg, double dec_deg, float* value, int* coverage)`
  （coverage 为 `int*` 二值），并新增 `_ex`/`nanmask_ex`/`attach_cache`/
| `p3_sampler_close` | 同上 | 同上 | `(P3Sampler*)`；释放 impl，置 nullptr |
| `P3SampleRejection` | 同上 | — | 样本级掩膜强制计数（`n_rejected_nonfinite` + 三项原因分类 + `n_eligible`） |
| `p3_sample_bilinear_nanmask_ex` | 同上 | 同上 | `(..., double weights[4], uint64_t leaf_ipix[4], P3SampleRejection*)`；= `p3_sample_bilinear_ex` 的唯一实现，额外暴露强制计数 |

- **签名漂移订正（本次）**：本表早期版本把采样函数记为
  `(const P3Sampler*, const P3WcsDescriptor*, int x, int y, float* value, float* coverage)`
  （像素坐标入参、coverage 为 `float*`）——现行公共面是**天球坐标入参**
  `(P3Sampler*, double ra_deg, double dec_deg, float* value, int* coverage)`
  （coverage 为 `int*` 二值），并新增 `_ex`/`nanmask_ex`/`attach_cache`/
  `cache_stats`/`uncertainty_*` 等符号族。消费方按符号名核对，行号仅作导航。

- 内部符号（合同可见但非导出）: `read_leaf`、`SharedTileCache`、
  `kTileWidth=512`（均在 `p3_resample.cpp`）。
- 本表锚注记: 表内**未改动符号**的锚沿用合同冻结时的版本（与本表同批冻结，未随
  P30/DATA-P3-UNC-001 两轮加符号而重排）；本次改动的两个符号按现行源重新锚定。
- 头注合同（`lib/algorithms/resample/p3_resample.h` 文件头）: 覆盖跨 tile 采样、coverage/mask 二值、NaN 语义
  （SCI [S-1]「参数与常数」一节）、单位固定 surface brightness、未支持输入模式显式拒
  UNSUPPORTED。与 SCI [S-1]「参考文献与参考代码」一节 非目标清单一致。

## 5 SCI/ALG 映射声明

- 本域矩阵行 `MOD-acsd-phase3-resample2` 的 `science_id` 现引用
  `SCI-P3-RES-001`（MISSING 占位，无权威文档）。本合同声明：本域全部
  科学内容以 **SCI-P3-001**（docs/science/PHASE3_HIPS_TO_FITS.md，
  FROZEN）为唯一权威，映射关系
  `SCI-P3-RES-001 ⇒ SCI-P3-001`；矩阵行 id 占位保持原样（
  占位词汇与权威文档分离），由 SCI 层统一改名，本域不改 docs/science/。
- ALG 层: docs/science/algorithms/PHASE3_RESAMPLE.md（ALG-P3-003 施工规格）
  [S-1]「公式与推导」一节的**G3（order 选择）与 G4（leaf 采样）** 为本域子面；本合同承接
  其全部公式（零改动，等价性核对见[S-1]「公式与推导」一节）。G1/G2（投影）属
  ALG-P3-PROJ-IMPL-001，G5（FITS 写）属 writer 域，不在本合同。
- DATA 层: 输出张量语义 = DATA-P3-RES（docs/detail/export/modules.md
  [S-5]「输入输出端口」一节，本任务新建）；输入 properties/tile 语义 = DATA-HIPS-001/
  DATA-TILE-001（[S-5]「输入输出端口」一节冻结）。
- API 层: 公共消费合同 = API-P3-RSMP-001（docs/engineering/api/PUBLIC_API.md
  本任务新节），登记上面 10 符号。

## 6 逐符号冻结（公式与实现等价性）

### 6.1 G3 order 选择——`p3_order_select`（`lib/algorithms/resample/p3_resample.cpp`）

冻结语义：在线性扫描 `k ∈ [0, max_order]` 中找**最小** k 使

```text
pixel_resolution_arcsec(nside=512 << k) / 3600 ≤ scale_deg_per_px
```

其中 `pixel_resolution_arcsec(nside) = sqrt(4π/(12·nside²))·180·3600/π`
（lib/algorithms/shared/healpix/healpix_core.cpp 权威实现）= `sqrt(π/3)/nside` rad。
**该式是精确的等面积等效线尺度，不是近似**：HEALPix 在同一 nside 下**所有单元面积严格
相等** = `4π/(12·nside²)`（Górski et al. 2005, ApJ 622, 759 §4；本仓实测
实验/healpix-polar/（叶面积恒等式正本 = `docs/DERIVATIONS-P3.md` [S-7]「叶面积恒等式」一节）：nside=512/1024 × 9 档纬度
面积相对偏差恒为 0.0；同一实验的等经纬网格阴性对照在 dec=89.9° 偏 −20.5%）。
**适用域**：该式是**面平均**判据；HEALPix 单元的局部采样步长（邻元中心角距）随纬度与
方向变化——实测 nside=512 共边邻元步长 ∈ [0.63,0.71]×该尺度、对角邻元 ∈ [1.95,2.94]×
该尺度。要求方向性分辨率保证时须按局部步长另加余量；该尺度是面平均量，各向同性上界另按局部步长给出。
与 ALG-P3-003 G3 冻结式 `s_tile_rad = sqrt(π/3)/(2^order·W)`、
`order = clamp(ceil(log2(sqrt(π/3)/(W·s_out))), 0, hips_order)`
**数学等价**（nside = W·2^k，W=512）：取等价形式
`k ≥ log2( sqrt(π/3)/(W·s_out_rad) )`，即同一 ceil 表达式。无更细层时
（扫描完未命中）`*out_order = max_order`（欠采样降级为 survey 原生
分辨率输出，SCI [S-1]「判据与误差」一节 允许并记录）。守卫: `!out_order || max_order<0
|| max_order>20 || !(scale_deg_per_px>0)` → `P3_RS_PARAM`（同文件）；
kMaxOrder=20 来自 `lib/algorithms/coverage/hips_properties.h`（ARCH-P3 [S-1]「公式与推导」一节）。
会话消费（`lib/phase3_session/p3_session.cpp`）: `max_order = 输入实际 order`
（open_ex 返回值，clamp ≤20）——**禁仅写 metadata 的 order**。

### 6.2 输入模式守卫——`p3_resample_check_mode`（`lib/algorithms/resample/p3_resample.cpp`）

`input_mode == "surface_brightness"` 唯一返回 `P3_RS_OK`；`nullptr`/空串 ⇒ `P3_RS_PARAM`；
`weight`/`flux-per-pixel` ⇒ `P3_RS_UNSUPPORTED`（SCI [S-1]「判据与误差」一节）；其余（含 `flux`/`variance`/`ivar`）
⇒ `P3_RS_PARAM`——**「未支持模式」≠「显式拒」**：`variance`/`ivar` 依DATA-P3-UNC-001 [S-1]「判据与误差」一节已转 uncertainty 子产品消费面（[S-1]「参数与常数」一节 符号表 `p3_uncertainty_*`；同文件逐字注记）。
实测偏差: 会话编排层未接线（DISP-P3RSMP-003）。

### 6.3 sampler 生命周期

- `p3_sampler_open_ex`（`lib/algorithms/resample/p3_resample.cpp`）: ①**输入单位身份裁决**
  （唯一实现 `acsd::phase3::p3_input_unit_verdict`，`lib/algorithms/resample/p3_input_unit.cpp`；
  session 与 scheduler 两个入口同源同判）—— 缺失 / 空串 / 同一逻辑键重复声明（含
  `BUNIT`-`bunit` 大小写别名）/ 单位声明面互相冲突 / 冻结词汇表外串 / 与
  `ACSD_PIXEL_SEMANTICS`、`ACSD_PIXEL_AREA_POWER` 矛盾一律拒绝，**绝不**补写确定量纲；
  未通过即 `P3_RS_PARAM`，`err` 携带机器可判错误码（`P3-INPUT-*`）。②`hips_product_validate`
  → `hips_properties_parse` 严格校验（必需 keys `hips_order, hips_tile_width, hips_frame,
  dataproduct_type`；`hips_order ∈ [0,20]`；`hips_tile_width` 必须
  512——`lib/algorithms/coverage/hips_properties.cpp` 显式拒非 512；NESTED 唯一；`BUNIT`
  逻辑键重复声明亦在此拒）③构造 `P3SamplerImpl`（nside=512·2^order）④
  `out_order`/`out_bunit` 回填实际值；`out_bunit` **只**取裁决给出的冻结 canonical
  `ADU/sr`（**绝不**缺省 `ADU`、**绝不**缺省 Jy/beam、**绝不**在裁决未通过时给串）。
  依据 `docs/science/unified/DATA_SEMANTICS.md`「单位与量纲表」与「面亮度单位的推导」两节（`FZ-UNIT-SIGNAL-SB`
  FROZEN）：采样值是输入 tile 值的凸组合 ⇒ 与输入同量纲；`ADU/sr` = 计数按立体角
  归一的面亮度串，裸 `ADU` = 每像素计数口径，与写盘数值不符且量纲不可判
  （§31.1a 并明确否定 `ADU/px^2`）。**实现锚**：`p3_resample.cpp`
  `p3_sampler_open_ex`（`*out_bunit = unit.bunit_canonical`）。失败路径: 单位身份裁决
  未通过 / properties 结构校验失败 / 路径不存在 → `P3_RS_PARAM`（前者 `err` 带
  `P3-INPUT-*`）；`aio_hips_open` 打开数据集失败 → `P3_RS_IO`；err 缓冲写人类可读消息。
- `p3_sampler_open`（`p3_resample.cpp`）: open_ex 缺参薄封装（out_order/
  out_bunit 传 nullptr）。
- `p3_sampler_set_max_tiles`（同文件）: 设置 TileCache 容量；
  `max_tiles ≤ 0` 恢复默认 8。负值不报错（按合同恢复默认）。
- `p3_sampler_close`（同文件）: 释放 impl 并置空；可安全对
  已关闭/空 sampler 调用（幂等）。

### 6.4 tile 寻址与读取——`read_leaf`（`p3_resample.cpp`）

冻结映射（全部经 healpix_core 权威函数，禁第二套实现）:

```text
tile_ipix  = leaf_ipix >> 18            # leaf_to_tile_nest(leaf_ipix, 9)（W=512 → shift=log2(512)=9）
local_x,y  = tile_to_leaf_nest 反交织    # leaf local 坐标（NESTED 18 bits）
fits_index = nested_local_to_fits_index(local, 9, 512)   # = (511-x)*512 + y（DATA_SEMANTICS §3 CDS oracle 冻结）
```

- 缓存命中: 直接返回 tile 缓冲（`p3_resample.cpp` 路径）。
- 缓存未命中: 定位 `order_{order}/dir{tile_ipix>>28}/Npix{tile}.fits`
 （HiPS 目录布局），FITS float tile 读入 `kReaderBuf`，插入缓存
 （可能触发 FIFO 逐出），再取像素（同文件路径）。
- tile 文件不存在/读失败: 返回 false（= 本次调用返回缺失/coverage=0；**是否属于正常天区缺数须查完整产品、MOC 与损坏状态，不得无条件当合法空洞**），
  语义见 §6.6；SCI [S-1]「判据与误差」一节 要求 provenance 记 missing tile——聚合上报
  未接线，DISP-P3RSMP-004。

### 6.5 G4 采样核（`lib/algorithms/resample/p3_resample.cpp`）

- `p3_sample_nearest`（`p3_resample.cpp`）: 输出像素中心 → `pix2ang` →
  `ang2pix(nside_leaf)` → 精确 cell（tile=leaf>>18, local 如 §6.4）。
  无插值误差（SCI [S-1]「判据与误差」一节）。tile 像素 NaN → `*value=NaN,
  *coverage=1`；tile 缺失 → `*coverage=0, *value=NaN`。
- `p3_sample_bilinear`（薄封装 → `p3_sample_bilinear_nanmask_ex`，
  均在 `p3_resample.cpp`）: ①输出像素中心 `pix2ang` → leaf 3×3 邻域
  （`healpix_neighbors`）②四象限各自取**最近中心** cell（d² 距离比较）
  ③切平面双线性: 以四角中心定义局部坐标解 `(u,v) ∈ [0,1]`（clamp），
  几何权重 `wg = {(1-u)(1-v), u(1-v), (1-u)v, uv}`（FP64，Σwg=1±k·ULP）；
  四角 tile 任一缺失 → `*coverage=0` 且 `*value=NaN`（无覆盖，非错误）
  ④**样本级掩膜 + 重归一**（rule_id `NAN-SAMPLE-MASK-COVERAGE-NAN`，权威 =
  DATA-002 [S-1]「判据与误差」一节`invalid_handling`；本文件 [S-1]「判据与误差」一节DISP 台账与 [S-1]「判据与误差」一节合同外实现同步）:
  合格 = `isfinite(值)`（NaN 与 ±Inf 同类）；被剔除样本**从分子、分母、方差三项
  一并剔除**，剩余合格邻域重归一 `c_k = wg_k / Σ(合格 wg_j)`（FP64，固定 k 序 ⇒
  确定性）；仅 `n_eligible==0` 或有效权重和 `W_p==0` 时 `*value=NaN`（覆盖级 NaN，
  禁零填替代、禁哨兵值）⑤`*coverage` 恒 1（到达插值即视为有足迹；值非有限**不改**
  C —— 4 个 tile 均可读 ⇒ C=1）⑥强制计数: `P3SampleRejection` 暴露
  `n_rejected_nonfinite`（= 值非有限 + 方差非有限 + 权重非正，互斥可加；信号核
  只消费 signal 平面 ⇒ variance 项恒 0；权重由几何唯一确定 ⇒ 权重非正项恒 0）。
  六种坐标退化分支对应 SCI [S-1]「判据与误差」一节 的 RA wrap/球面角差处理；退化象限由最近中心
  填充（见 DISP-P3RSMP-001 偏差登记）。
- **weights[4] 语义（本次对齐）**: 暴露**生效（重归一）权重** `c_k`，被剔除样本恰为
  `0.0`（零合格样本时四权重全 0）。依据 DATA-002 [S-1]「判据与误差」一节规则一「不合格样本从分子、
  分母、**方差**三项一并剔除并重新归一」⇒ 方差传播 `Σ c_k²u_k` 必须消费该权重，
  权重取值 = 重归一生效权重 `c_k`（沿用未重归一几何权重即系统性偏差；回归锚 `p3_nan_mask_test.cpp` T6
  断言两者相对差 >5%）。全部样本合格时 `c_k = wg_k/(1±k·ULP)`，仍满足 Σc_k=1±k·ULP。
- 与 ALG-P3-003 G4 施工规格"面积重叠分数（投影线性化）"的差异:
  实现为**切平面四象限最近中心双线性**——同属一阶插值族、Σw=1
  不变量一致，离散化方案不同（DISP-P3RSMP-001 如实登记，不修码）。

### 6.6 值语义（ALG-P3-003 [S-1]「公式与推导」一节G4与参数冻结 冻结，实现一致）

| 输入状态 | *value | *coverage | n_rejected_nonfinite |
|---|---|---|---|
| tile 存在，四角值全有限 | 像素值（面亮度） | 1 | 0 |
| tile 存在，邻域**部分** ¬isfinite（NaN/±Inf） | 剩余合格邻域**重归一**加权和（有限） | 1 | 被剔除样本数（1..3） |
| tile 存在，邻域**全部** ¬isfinite（零合格样本） | NaN（**覆盖级 NaN**） | 1 | 4（`n_eligible=0`） |
| tile 缺失/读失败（任一角） | NaN | 0 | 0（无候选样本；`n_eligible=0`） |
| 采样器未打开（impl=nullptr） | NaN | 0 | 0 |

- coverage 为**二值语义**（0/1，float 承载）；C=1 ⇔ 采样足迹内**存在 tile 像素**
  （存在判定，ALG-P3-003 [S-1]「公式与推导」一节G4与参数冻结：值非有限**不改** C；4 个 tile 均可读则 C=1；
  任一角 tile 缺失 ⇒ C=0）。mask 输出由会话层从 coverage 生成
  （coverage_output 仅 `mask` 合法，`lib/phase3_session/p3_session.cpp`）。
- **零填口径**: 零合格样本取 NaN（覆盖级 NaN）；`0`/`±Inf`/哨兵属另一形态；
  被剔除样本的权重必须**不进分母**（禁留在分母 ⇒ 系统性偏低），也**不进方差项**。
- 计数语义: `n_rejected_nonfinite` 按原因分类（值非有限 / 方差非有限 / 权重非正，
  互斥可加）；计数 0 与「字段缺失」必须可区分（DATA-002 [S-1]「判据与误差」一节规则三）。信号核的
  方差/权重两类恒 0（不消费方差面；权重由几何唯一确定）。

## 7 TileCache 并发/确定性合同（`SharedTileCache`，`lib/algorithms/resample/p3_resample.cpp`）

- **有界 LRU 缓存（跨 worker 共享、线程安全）**: 结构 = `std::list<uint64_t> lru`
  （front = MRU）+ `unordered_map` + `std::mutex`；`get` 命中时
  `lru.splice(lru.begin(), lru, it->second.it)` ⇒ **访问序更新（真 LRU）**，
  超容时逐出 `lru.back()`（最久未用）。容量默认 8（`set_max_tiles` 可调，≤0 恢复 8），
  生产会话经 `p3_sampler_attach_cache` 让每 worker sampler **共享同一缓存**
  （容量 = max_tiles 总量，与 worker 数无关 ⇒ 峰值内存不随核数增长；
  `lib/phase3_session/p3_session.cpp`）。**DISP-P3RSMP-002（FIFO 登记）已随 P30 缓存改造失效**，
  现行策略 = LRU。
- **负缓存（absent）**: tile 不存在/读失败记一次，之后直接返回缺失，
  语义与"每次 open 都失败"逐位等价（`stat_open_fail` 与负缓存命中分列计数）。
- 键: `(order, tile_ipix)`；值: tile 缓冲（`shared_ptr<const TileData>`，只读共享）。
  每 sampler 另有 `kHotSlots=8` 前端热缓存（命中不取共享锁）。
- 并发模型: 缓存本身线程安全；**单 `P3Sampler` 实例的其余状态非线程安全**
  （无内部锁），同一 `P3Sampler` 的共享面 = 单线程（合同项）。
  HiPS tile 文件只读共享，无写锁（`lib/phase3_session/p3_session.cpp` 的 worker 闭包内
  `p3_sampler_open_ex` 每 worker 重建 + attach 共享缓存）。
- 确定性: 缓存策略（LRU/容量/负缓存开关）只影响 I/O 命中率，**不影响任何像素值**
  （tile 内容只读；同一输出像素的计算路径固定: 邻域确定 → 最近中心确定 → 权重确定）
  ⇒ 输出与 tile 装载顺序、worker 数、缓存容量**无关**。该"只影响 I/O"性质是结构性约束：
  一旦引入跨 tile 的数值状态即失效。禁 hardware_concurrency 决定
  worker 数（=budget.max_workers，同一 `p3_session.cpp`）。

## 8 会话编排合同（`lib/phase3_session/p3_session.cpp` 消费链，实测）

| 消费点 | 锚（p3_session.cpp） | 合同 |
|---|---|---|
| sampler 打开 | 同上（映射分支另见同文件） | 主线程 open_ex 暴露输入实际 order/BUNIT；失败映射 P3_RS_IO→ACS_ERR_IO、UNSUPPORTED→ACS_ERR_UNSUPPORTED、PARAM→ACS_ERR_PARAM |
| max_tiles 守卫 | 同上 | 默认 `min(1024, ceil(W·H/512²)+16)`；请求 > 默认 → `ACS_ERR_BUDGET`（可降不可升） |
| order 选择 | 同上 | max_order=输入实际 order（clamp ≤20），p3_order_select 就地选择 |
| worker 池 | 同上（两处） | n_workers=budget.max_workers（禁 hardware_concurrency）；>hpx clamp 行带不空；<2 或未注入 budget → 串行；每 worker 独立 sampler+cache（同文件闭包，§7） |
| 采样分派 | 同上 | nearest/bilinear 按请求 `sampler` 字段分派（缺省 bilinear，parse_request 白名单，同文件） |
| 取消 | 同上 | 行级取消检查 → `ACS_ERR_CANCELLED`（"cancelled at row N"）；worker open 失败 → `ACS_ERR_IO`（同文件） |
| provenance | 同上 | order_sel_used/sampler_used 填实际值；manifest_hash/missing_tiles 恒 nullptr（DISP-P3RSMP-004） |
| 请求守卫 | 同上 | projection 仅 TAN（projection 分支）；frame 仅 icrs；\|dec\|≤85°；scale>0；W/H∈[1,20000]；sampler∈{nearest,bilinear}；parity∈{east_left,east_right}；bitpix∈{-32,-64}；coverage_output 仅 mask（均在同一文件） |

## 9 错误语义冻结

| 状态 | 触发（实测） | 会话映射 |
|---|---|---|
| P3_RS_OK | 成功 | ACS_OK |
| P3_RS_PARAM | out_order 空/max_order 越界/scale≤0（`lib/algorithms/resample/p3_resample.cpp`）；properties 语义不符 | ACS_ERR_PARAM |
| P3_RS_UNSUPPORTED | input_mode 非 surface_brightness（同上文件）；frame≠ICRS | ACS_ERR_UNSUPPORTED |
| P3_RS_IO | properties 解析失败/HiPS 目录或 tile 文件不可读 | ACS_ERR_IO |
| （采样层返回 P3_RS_OK） | tile 缺失 = coverage=0、signal=NaN；但若损坏的已声明 tile 读失败，同样走此数据分支，需更高层完整性审计 | — |

- 采样函数（nearest/bilinear）无返回码（void 语义经 value/coverage
  表达）；`last_error[256]` 缓冲仅 open/set_max_tiles 路径填充。
- 缺省行为 = 显式定义: sampler 关闭态调用采样函数 → coverage=0（不 crash、
  不伪造数据）；open 失败必须经 err 缓冲带原因（test_p3_resample.py
  test_06_no_silent_default_open 冻结此行为）。

## 10 资源合同

- 内存: cache 上限 `max_tiles·512²·4 B`（默认 8 tile = 8 MiB float；
  会话守卫默认 ≤1024 tile）；输出平面 2×W·H×4 B（S/C）。
- 每输出像素: 1 次 pix2ang + 1 次 ang2pix（nearest）或 1 次 pix2ang +
  1 次 neighbors + ≤4 次 ang2pix（bilinear）——均为 O(1)。
- 线程: worker 数 = budget.max_workers（≤hpx），取值面 = budget 一项——硬编码与
  hardware_concurrency（AGENTS 约束一致）；串行阈值 <2 workers。
- 取消: 行级响应（每行首检查），取消后已写行保留、报行号。

## 11 实测偏差与整改登记（如实冻结）

> 表内 DISP-P3RSMP-001..005 为本合同首版登记的**未修码**偏差；006/007 为本次
> NAN-SAMPLE-MASK 对齐任务的登记（其中「信号核 NaN 处置」一项已按冻结口径落地，
> 见 [S-1]「与上下游的关系」一节，不再单列 DISP）。

| ID | 偏差 | 实测依据 | SCI/ALG 依据 | 整改归属 |
|---|---|---|---|---|
| DISP-P3RSMP-001 | bilinear 为切平面**四象限最近中心**双线性；G4 施工规格写"面积重叠分数（投影线性化）"。同族一阶插值、Σw=1 不变量一致，离散化方案不同 | `lib/algorithms/resample/p3_resample.cpp` | ALG-P3-003 [S-1]「物理模型」一节 G4 | P3-RSMP-IMPL 决策: 升级实现或修订 ALG 表述（须保持 SCI [S-1]「判据与误差」一节不变量） |
| DISP-P3RSMP-002 | tile cache 逐出为 FIFO；ALG [S-1]「公式与推导」一节 伪代码写 "LRU" | `lib/algorithms/resample/p3_resample.cpp`（无访问序更新） | ALG-P3-003 [S-1]「公式与推导」一节 | P3-RSMP-IMPL（实现升级 LRU 或 ALG 修订为 FIFO 表述） |
| DISP-P3RSMP-003 | `p3_resample_check_mode` 会话编排层无调用点（flux/variance 输入拒未经会话守卫；能力在内核、探针消费） | grep 全仓：仅探针载体（已不在本仓） | SCI[S-1]「判据与误差」一节 | P3-RSMP-INT 接线（会话请求守卫增加 input_mode 检查） |
| DISP-P3RSMP-004 | provenance.missing_tiles 恒 nullptr（缺 tile 聚合上报未接线；SCI[S-1]「判据与误差」一节 要求记 missing） | `lib/phase3_session/p3_session.cpp` | SCI[S-1]「判据与误差」一节 | P3-RSMP-IMPL/INT |
| DISP-P3RSMP-005 | acsd_p3_resample.dll 未建（entrypoint 缺失） | 全仓无该 target | 矩阵 dll_target | P3-RSMP-IMPL |
| DISP-P3RSMP-006 | **强制计数的产品承载面未冻结**：DATA-002 [S-1]「判据与误差」一节冻结了计数**字段名** `n_rejected_nonfinite`（按原因分类、per-pixel、mandatory），但**未**冻结 Phase3 产品里的承载面（`phase3_planar_fits_v1` 最小平面集 = signal/support/mask，plane_id 枚举 `{signal,support,variance,ivar,mask,sparse_snr}` 无计数字段；`p3_resampled.json` 的 `planes` 列表亦无）。内核面已按冻结字段名暴露（`P3SampleRejection`），**产品承载面不自行发明** | `eng/contracts/data/phase_product_exchange.schema.json`（无 `invalid_handling`/`count_field` 键，与DATA-002 [S-1]「判据与误差」一节「机器形态」声明不一致）；`eng/contracts/schemas/unified/rejection.schema.json` 仅有可选 `rejected_sample_count`；DATA-P2-REJ-001 登记（`docs/engineering/data/ARTIFACTS.md`，Phase2 诊断平面 `nrej/nused` 通道，语义为 P2 排异原因计数，非本规则） | DATA-002 [S-6]「判据与误差」一节规则三；ALG-P3-003 [S-6]「输入输出端口」一节 G4 | 待合同域登记：候选 =（a）逐像素诊断平面（沿用 [S-6]「输入输出端口」一节 诊断平面通道，不入 science planes 枚举）；（b）`p3_resampled.json` 聚合键（drizzle 先例 `lib/algorithms/drizzle/src/module_entry.cpp` 的 `"n_rejected_nonfinite"`）；（c）provenance 计数。三者均需合同域登记后才可落产品 |
| DISP-P3RSMP-007 | **C（coverage）语义的唯一正本 = `docs/detail/export/modules.md`**：Phase3 侧执行口径 = ALG-P3-003 [S-5]「输入输出端口」一节 G4 / [S-5]「输入输出端口」一节 与 [S-5]「输入输出端口」一节「C 只判足迹内有无 tile 像素，值 NaN 不改 C」（零合格样本 ⇒ `S=NaN` ∧ `C=1`）；[S-5]「输入输出端口」一节「C=1 ⇔ 足迹内存在**有限** tile 像素」与 DATA-002 [S-1]「判据与误差」一节规则二「零合格样本 ⇒ signal=NaN ∧ **support≤0**」按 [S-1]「判据与误差」一节正本订正 | 本层零合格样本输出 `S=NaN` 且 `C=1`（四角上游 support≤0），与 [S-1]「判据与误差」一节一致 | ALG-P3-003 [S-1]「物理模型」一节 G4/[S-1]「参数与常数」一节；DATA-P3-RES（`docs/detail/export/modules.md`）；DATA-002 [S-5]「输入输出端口」一节 规则 2 | 合同域登记面：[S-5]「输入输出端口」一节 与 [S-5]「输入输出端口」一节 的表述按 [S-5]「输入输出端口」一节 正本收敛（订正动作落合同域） |

## 12 TEST-P3-RSMP-DESIGN-001 设计冻结（登记面 VERIFIED）

- 定位: 本域可执行测试面在 P3-RSMP-TEST 落地；矩阵行 test_id=
  TEST-P3-RES-001 状态 **DORMANT**。本节冻结**设计**（登记面
  TEST-P3-RSMP-DESIGN-001，VERIFIED），不冒认可执行覆盖。
- 现有证据（引用不冒认，均测试独立参考实现或探针，非生产自证；载体文件均已不在本仓，
  判据本体见[S-1]「判据与误差」一节）:
  - 探针六模式: order 选择/
    mode 守卫/open/nearest/bilinear/pix2ang——pix2ang 用 nside=4 首子
    像素中心技巧。
  - 最近邻邻接组: test_05_nan_semantics
    （tile 内 NaN → C=1 + 值 NaN，§6.6 行 2）、test_06_no_silent_
    default_open（open 失败必须带原因）、seam 域界断言（1e8-1..
    12e8+1）与连续性（1e-5° 位移）——SYN-007 判据邻接。
  - 并行域组。
  - 独立参考实现核对插值/覆盖语义（不调用生产码 ⇒ 不能作本规则判据）。
  - lib/algorithms/resample/tests/p3rsmp/p3_nan_mask_test.cpp（437 行，本次新增）:
    **生产码在内**的样本级掩膜 Oracle + 负例——手写最小 HiPS（signal+variance，逐像素
    可注入 NaN/±Inf），期望值由本文件独立复算（剩余合格邻域重归一加权和）。覆盖:
    T1 harness 映射自检 / T2 1·2·3 个非有限样本的重归一真值 / T3 零合格样本 S=NaN 且
    C=1 / T4 ±Inf 与 NaN 同类（含混合、全 Inf）/ T5 强制计数分类 / T6 方差按重归一权重
    传播（FP64 1e-12，且与「沿用原几何权重」错值相对差 >5%）/ T6b 零合格样本方差 NaN。
    正例/负例判据: 同一用例对「回退 any_nan→NaN」变体必须判红、对现行实现判绿；
    判红/判绿证据与日志正本 = 实验/engineering-evidence/。
- P3-RSMP-TEST 设计要求（合同）: ①order 选择等价性（对拍
  pixel_resolution_arcsec 公式，[S-1]「与上下游的关系」一节）②seam 连续性（SYN-007 预冻结
  容差）③NaN/coverage 值语义全表（[S-1]「与上下游的关系」一节 五行，含样本级掩膜与覆盖级 NaN）
  ④缺 tile → C=0 ⑤nearest 精确 cell ⑥bilinear Σw=1 与背面跳过
  ⑦max_tiles 逐出行为（FIFO）⑧open 守卫（非 512 tile/frame≠ICRS/order 越界）
  ⑨样本级掩膜 + 重归一 + 强制计数 + 方差重归一权重（本次由
  p3_nan_mask_test.cpp 落地，含「实现回退即判红」的负例注入面）。
- 判定口径: 可执行测试 VERIFIED 须生产码+测试同在现场（C7），当前
  不可冒认——test_status 维持 DORMANT。

## 13 合同边界与 DISP 登记

- 本合同冻结**现状实现**为合同基线；§11 偏差表登记与合同基线的差异，
  整改走 P3-RSMP-IMPL/INT。
- 合同外实现（引入即违约）: 第二套 healpix 数学核心；第三种采样核；flux/
  variance/weight 输入静默接受；**BUNIT 缺省非 canonical `ADU/sr`**
  （缺省必须为 `ADU/sr`，见 §6.3）；仅写 metadata 的
  order；hardware_concurrency 决定线程数；静默默认 open；**静默剔除**
  （¬isfinite 样本必须计数暴露）；**零填替代**（零合格样本必须 NaN）；
  **用未重归一的几何权重传播方差**（DATA-002 [S-1]「判据与误差」一节规则一）。
- DISP 台账: [S-1]「判据与误差」一节表七项（DISP-P3RSMP-001..007）。006 = 强制计数产品承载面未冻结
  → 登记面 = 合同域；007 = C 语义以 `docs/detail/export/modules.md` 为唯一正本 → 订正动作
  登记面 = 合同域。

## 14 SCI 层零改动声明

- 本任务对 docs/science/PHASE3_HIPS_TO_FITS.md 零改动（git status
  验证）；全部科学公式/容差/拒绝清单以 SCI-P3-001 [S-1]冻结为准。
- ALG 层: docs/science/algorithms/PHASE3_RESAMPLE.md 仅做表述级修订
  （tile cache 逐出表述、实现锚补记、[S-1]「公式与推导」一节伪代码与 G4/[S-1]「公式与推导」一节口径对齐），G1-G5 公式零改动。

> 本文引用上游正本（论文式编号，正文引用处均已改为自然语言节名，不再使用跨文档 §N 跳转）：
> - [D-1] docs/ACSD_DESIGN.md（最高设计）。
> - [S-1] docs/science/projection/HIPS_TO_FITS.md（投影导出科学正本 SCI-P3-001）。
> - [S-5] docs/detail/export/modules.md（相关科学正本）。
> - [S-6] docs/engineering/data/ARTIFACTS.md（相关科学正本）。
> - [A-1] docs/science/algorithms/export/PHASE3_RESAMPLE.md（重采样施工规格分册）。
> - [U-1] docs/science/unified/DATA_SEMANTICS.md（数据语义正本）。
> - [S-7] docs/DERIVATIONS-P3.md（推导补遗）。

## 参考文献与参考代码库（含许可证）

- HiPS 层级/tile：IVOA HiPS 1.0（https://www.ivoa.net/documents/HiPS/）；Fernique et al. 2015, A&A 578, A114。
- HEALPix 几何：Górski et al. 2005, ApJ 622, 759；astropy-healpix（BSD-3-Clause）。
- 双线性采样核与 tile 寻址：Project-defined（本文件 §6.4/§6.5，SCI-P3 [S-1]「判据与误差」一节）。
- FITS tile 读取：FITS Standard 3.0；CFITSIO（宽松许可）。


---

## U 承接：`uncertainty_available=false`（fail-closed 唯一出口）

本层产出/消费不确定度子产品时，`uncertainty_available=false` 的处置**承接 《产物与清单登记面》 的 fail-closed 唯一出口**（DATA-UNC-001 规则行与 DATA-P2-PROV-001 显式登记行，按**内容锚**读、不按行号读）：不写 variance/ivar 子产品，manifest 写 `uncertainty_available=false` 并在 diagnostics 标红计数。该键不是失败态，是 unavailable 的显式登记模式；键名与取值口径以该处为唯一权威，本层不另立第二套。


---

## D7-B｜现行 Stage3 NESTED 重采样、有效值、统计和资源正式设计（2026-10-17）

**证据基线**：完整阅读 `63eccbb6133aef56389641d43c55df6e243819c5` 的 427 行原文（原 blob `35e05649bba5bc4a6e2473261299e9d3e32ec317`，清单 36496 bytes）与当前 427 行正文；原 §1–14 的 G3/G4、API、缓存、复杂度、DISP、实验原设计及文献均保留。本文新增的是以 **main 51e06540eb021c7272e87cf67b34e53c0648cf7a** 的源码为事实的 CURRENT 合同与唯一研究 TARGET；原文旧版文件行数、FIFO状态、估计数、会话入口等只保留为历史参照。现行头 `p3_resample.h` 204 行、实现 `p3_resample.cpp` 606 行；科学原理正本在 [RESAMPLE](../../resample/RESAMPLE.md)，P3 实际消费者在 [Export modules](../../../detail/export/modules.md)，FITS 发布见 [FITS 实现](PHASE3_FITS_IMPL.md)，不要从本章倒推出 P1 HEALPix 曲边已经由 P3 修复。

### B1. 输入事实、真正的 HiPS 格式与坐标离散

**CURRENT** 的 `acsd.phase3.properties` 对 `source.hips_dir/signal/properties` 先执行 `p3n_guard_input_units`，随后调用 `p3_sampler_open_ex`；`hips_product_validate` 解析 properties 要求 hips_order ∈[0,20]、hips_tile_width=512、hips_tile_format 包含 fits、hips_frame 属于 equatorial|icrs、dataproduct_type=image、产品至少一个 `NorderK/Dir*/Npix*.fits` tile。其 public `HipsProperties` 与 `hips_product_validate` 的解析语义不得与 AIO `aio_hips_open` 的另层宽松原始解析混淆；AIO 会先验根 `manifest.json` 存在且宣告 signal 子产品，再硬核 tile_width=512/order∈[0,29] 并读取 MOC；**P3 sampler 经过自己的[0,20]严格产品门之后才能到 AIO**。这两个 reader 范围不同但在 P3 已受前置限制。AIO `read_tile_t` 只接受 FITS BITPIX -32/-64，读 -64 时用 f64 临时缓冲，再按 P3 sampler f32 tile 转换，因此当前即使读高精度 tile 也不能声明 P3 之后保留 f64 输入细节。

```text
K=hips_order; leaf_order = K+9; nside_leaf = 512*2^K
leaf = ang2pix_nest(nside_leaf, ICRS ra_deg,dec_deg)
tile = leaf_to_tile_nest(leaf,leaf_order,K) = leaf >> 18
first = tile_to_leaf_nest(tile,K,leaf_order)
local = leaf-first                             // nested morton 18 bits
fits_index = nested_local_to_fits_index(local,9,512)
           = (511-local_x)*512 + local_y      // row-major FITS flat index
tile_file = signal/NorderK/Dir{floor(tile/10000)*10000}/Npix{tile}.fits
```

**目录形态是重要纠偏**：旧版本 `Dir{tile >> 28}` 与真实 IVOA/代码不一致，AIO 当前标准路径优先，若 `tile>=10000` 标准路径不存在，会尝试仅读兼容的旧 `Dir{tile/10000}/Npix{tile%10000}.fits`。只读兼容不能变成新产物的默认写路径，必须分别验证 standard/legacy。MOC 缺失当前 AIO `load_tiles_from_moc` 允许空列表，但存在而读损坏则 fail-closed；产品完成清单缺失直接拒绝，这不是单个尚未观测 tile 的合法空洞。

**Issue #87 已闭合 tile 缺失/损坏的同态折叠**：AIO reader 的整 tile 读取入口返回稳定分类（`OUTSIDE_DECLARED_FOOTPRINT` / `DECLARED_MISSING` / `IO_ERROR` / `INVALID_TILE` / `CHECKSUM_FAILED` / `FOOTPRINT_UNDETERMINED` / `UNKNOWN_LEGACY_COVERAGE`），覆盖身份按 `MOC_LEAF` / `MOC_UNDETERMINED` / `MANIFEST_ONLY` / `UNVERIFIED_LEGACY`（数值=旧 `NONE`）分级并随成功产物落盘；DATASUM/CHECKSUM 存在时校验、缺失时显式计数而非失败；P3 sampler 的合法 absent 负缓存与完整性错误缓存分开，错误命中复现同一 code/path；采样函数对真实故障返回 `P3_RS_INTEGRITY`/`P3_RS_IO`，`p3_op_resample`/`p3_session_run` 在写 FITS 前 fail-closed，不发布 resampled bin、FITS 或 complete manifest。多 worker 按 `tile_ipix` 升序聚合、同一 tile 一条并记录 `worker_min/max/count`。父级覆盖归约仍属独立任务 #95，本 reader 对低阶 MOC 显式判 `FOOTPRINT_UNDETERMINED` 而不静默归约。

**Issue #87/E2-R 收尾覆盖身份与校验一致性**：无可靠覆盖来源时缺失记 `UNKNOWN_LEGACY_COVERAGE`（不再是 `OUTSIDE_DECLARED_FOOTPRINT`）——证据缺席不等于证据为空；该状态非 fail-closed（合法稀疏 HiPS 仍可 best-effort 出图），但 `coverage_verified=false` / `input_completeness=UNVERIFIED_LEGACY` 必须写进 `p3_resampled.json`、writer/verify 产物、run manifest 与 FITS 头 `COVSTAT`/`COVVERIF`，verify 节点对三处同源做一致性对拍。校验快速路径（已读像元位样）在未压缩 float/double 上与磁盘原始字节逐位等价（与独立 cfitsio `fits_verify_chksum` 对拍：float32/64、±0、多 NaN 位型、单/双校验字段、header/data bitflip 全一致）；`BSCALE/BZERO/BLANK` 非平凡或 `fits_is_compressed_image` 判为 tile 压缩时回退 cfitsio 全 HDU 校验。标准 tile-compressed 布局（空 PRIMARY + HDU2 ZIMAGE 压缩图像）由 reader 显式定位后读取，校验按压缩 HDU 的磁盘字节口径执行。

### B2. 实际 G3、G4 与受控矩阵近似

`p3_order_select(max_order,scale_deg_per_px,out)` 实现从0递增取首个 `sqrt(pi/3)/(512*2^k) <= scale_rad` 的层，若到 max_order 仍不足就取 max_order；`kMaxOrder=20`、scale>0；这是等面积等效角宽，而非每个方向最坏叶中心间距，更不意味着尺度超采样创造新像素信息。真实 `p3_op_resample` 使用输入 signal/properties 解析的 order 上限，统一基于完整 WCS 反变换求每个**零基输出像元中心**的 ICRS sky 坐标；P3 的另一个 `p3_session_run` 是兼容/程序内 API 路径，不能用旧 `std::thread` /全帧内存分配叙述替代 DAG 的 bounded-block 生产。

`nearest` 是包含方向的单 HEALPix leaf 零阶取样，保留 leaf 中被读出的已量化面亮度值，不会再引入 bilinear 平均，但不是“真实连续天空无插值误差”；经单 leaf 的阶梯量化仍需 aliasing 对照。当前 `bilinear` 不是严格双线性**不规则四边形有限元**也不是输出 WCS 像元与 leaf 的真实面积交叠。生产首先取中心 ipix 和其邻 leaf，使用输入 sky 点为 gnomonic 切平面原点，按坐标符号划四象限，每象限选最近叶中心（同距离时选较小 ipix），缺象限以整体最近叶中心填补，允许重复叶。用 `x00,x10` 求 u、`y00,y01` 求 v，分别夹到[0,1]，生成四个常规矩形权重 `w=[(1-u)(1-v),u(1-v),(1-u)v,uv]`；**只保证常数再现 Σw=1，不保证一般不规则叶中心的一次梯度精确再现**。

在真实点位置 `q_k=(x_k,y_k)` 与输出目标 `t=(0,0)` 的局部线性源 `B(q)=b0+bx*x+by*y` 中，正确的一次再现当且仅当：
\[
\sum_k c_k=1,\quad\sum_k c_kx_k=0,\quad\sum_k c_ky_k=0.
\]
#90 已给解析非矩形四角反例：`q00=(-e,-e),q10=(e,-e),q01=(-2e,e),q11=(e,e)`，四权重各1/4时在 `B(x,y)=B0+x/e` 上产生 `-0.25` 的目标偏差。此反例为**数学合法非矩形点集**，还不能宣称它正是现行 HEALPix 某个 ipix 的测得偏差。TARGET 可在真实叶中心中找含 t 的非退化三角形，用二维 barycentric 权、条件数和凸包判据做局部一次再现；或精确求真实四边形映射逆解。需对折叠/退化/无有效邻域选择**唯一可报告 fallback**，并在真实 all-face/极区/face seam/亚像元相位独立测其适用性、耗时、PSF变化。目标面积守恒 resampler `area(Q_i∩H_j)` 是**另一个尚未实现的P3研究算法**，切忌把 Stage1 #15 的 Drizzle 球面 overlap 等同为当前 P3 bilinear。

### B3. 两层缺数与有效系数精确含义

CURRENT 函数签名是 `P3ResampleStatus` 返回，`float* value`、`int* coverage` 输出，不是原文过时的 void / float* coverage。合法无覆盖（`OUTSIDE_DECLARED_FOOTPRINT`）的 tile 未读到（bilinear 任一角）⇒ **S=NaN,C=0**；真实完整性故障（声明内缺失/INVALID/CHECKSUM/IO/UNDETERMINED）⇒ 返回 `P3_RS_INTEGRITY`/`P3_RS_IO`，由 run 层 fail-closed，不得折叠成 S=NaN,C=0。四角都来自存在 tile，但值全 NaN/±Inf 或有效权重总和≤0⇒ **S=NaN,C=1**，这表明几何覆盖、科学值资格两个轴分离。只有部分值无效时取有效角集合 `E` 并令：
\[
c_k=\begin{cases}w_k/\sum_{j\in E} w_j & k\in E,\ \sum_{E}w>0\\0&\mathrm{otherwise}\end{cases},\quad
S=\sum_k c_k X_{\mathrm{id}(k)}.
\]
`P3SampleRejection` 在 `p3_sample_bilinear_nanmask_ex` 已暴露每类计数和 `n_eligible`，但 **真实 adapter 调用的是 `p3_sample_bilinear_ex`，其 rejection=NULL，因此计数并未作为 P3 正式 artifact 输出**；不能把 public API 存在说成数据全量留痕。#89已在函数入参合法后 `std::fill(weights,weights+4,0.0)`，故非有限被剔除角不会留下上一轮栈缓冲的权；失效/重复角、0权重角必须分离计数。nearest 存在 tile 中非有限值也可得到 `C=1,S=NaN`。Signal 为有限零、负值时不得视为未覆盖。

`p3_resampled.bin` 当前内部**两个 f32 平面**，先 `signal[W*H]`，后 `coverage[W*H]`，同一未裁剪 W/H、每个平面行主序 `offset=(plane*W*H+y*W+x)*4 bytes`。虽然是两平面中间工作阵列，最终 FITS **只含 PRIMARY signal，不含 COVERAGE extension**；像元 `NaN` 无法唯一决定原始 C=0 还是 C=1 且无有效科学值。不得以 `n_nonfinite/coverage` 简写最终产品包含逐像素拒绝计数。

### B4. 方差API的已修与未闭合的明确边界

`p3_uncertainty_open` 公共库可按 variance→ivar 优先级打开误差子产品并校 order；`p3_uncertainty_propagate` 在 `npts=1/4` 时按生效权重算条件方差。**#88 已按完整 64-bit leaf_ipix 把重复四角归成同一随机变量**：`a_j=Σ_{k:leaf(k)=j} c_k`，在**不同叶条件独立**且方差有效时 `V_out=Σ_unique a_j² V_j`，不是原过时的 `Σ角 c_k²V_leaf(k)`。#89 的被剔除角先清零；方差读侧仍逐角执行负/Inf 损坏校验，ivar=0 判无效。全相关真实式仍为 `V_out=a^T C_leaf a`；前级 P1 drizzle、HiPS 层级、P2校准/天空减除的关联 C 并没有凭 #88 的同一leaf身份聚合而消失。**现行 `p3_op_properties`、`p3_op_resample` 明确不打开这些可调用的 uncertainty API**，真实产品 metadata `uncertainty_available=false`，writer 不输出VARIANCE/IVAR HDU；任何用实测P3 variance HDU证明这些修复的说法都缺生产可达前提。

### B5. 真实 Cache/IO 资源与当前静态风险

`P3Sampler` 仅包含 `P3SamplerImpl* impl`，由 `open/open_ex` 分配、`close` 释放；调用者拥有句柄和 `err` 的 std::string，对同一 sampler 并发读不安全。每工作线程有自己的 sampler/AioHipsDataset 和8槽热缓存；`p3_sampler_attach_cache` 共享 `SharedTileCache` 的 mutex+list+map LRU，容量默认8片、允许 `set_max_tiles` 按资源预算设置，tile的数据为 `shared_ptr<const vector<float>>`，每片512²f32≈1MiB。读取优先热槽→共享LRU→**错误缓存**→合法 absent 负缓存→实际AIO tile；错误缓存与合法 absent 分开（命中复现同一故障身份）。旧“FIFO”仅是历史已被P30修正。实际 AIO Reader 对每 tile 使用独立READONLY CFITSIO句柄，在 _REENTRANT 4.6.4路径**不再使用同一全局Cfitsio写锁**；FITS写端有全局串行锁，不能把两端混称全局I/O串行。

**资源审查必须如实限定当前实现**：SharedTileCache 的成功 tile LRU 按 `cap` 淘汰；合法 absent 是与 LRU map**分离的 unordered_map，没有按cap淘汰**；Issue #87 的完整性错误表是独立 `std::map<tile_ipix, ErrorRec>`（按 tile 升序、上限 4096 条，失败路径专用），同样不占成功 LRU 容量。缺失 tile 负缓存不能无条件称为“与cap同界的有界缓存”。hot槽的 `shared_ptr` 会 pin住已逐出的tile，瞬时同一tile在多worker竞态失误后可能并行重复I/O（底层读在共享锁外），并不能保证“每tile全球唯一open”。峰值由正常LRU `cap*1MiB`、每worker热槽pin、in-flight解码、std::vector neighbors临时分配、Runtime子块和未封顶负缓存键共同决定，须用工作集含大量缺失和并发的真实M42测RSS/IO锁等待/重复open和缺块规模；缓存结构自洽不等于符合总体 OOM 预算。 `p3_sample_bilinear_ex` 每次调用 `healpix::neighbors` 返回动态vector，故旧的“每像素严格O(1)且无分配”不能解释实际内存分配成本。

**CURRENT 真 DAG 资源**：`p3_op_resample` 用 Runtime 单一executor按 `sub_block_px` 分割 W×H，工作线程每块缓冲 `float signal,coverage`，经 AIO RandomWriter 将2×W×H×4B**整幅中间文件**以不相交行区间写到磁盘；仅RAM缓冲按子块受限（不等于磁盘大小不随画幅变化），临时文件全部块成功才fsync+rename。writer随后经有界 `ExportStreamScheduler` 逐块读并写PRIMARY FITS；当前不是“生产整幅 f32 signal+coverage 常驻RAM”，原 §10仅适用于独立全帧`p3_session`/故障注入对照。默认 max_tiles 按输入画幅估测、上界1024、请求可降低，`queue_depth/sub_block_px`来自运行资源配置，实际 worker 从Runtime lease而不是 `hardware_concurrency`；中途取消/作业缺块写入/队列未执行任务都必须 fail-closed。

### B6. 正交独立验证（TARGET；无本批实验执行）

1. **索引与reader**：所有12 faces、nside=512×2^K，核对 `leaf→tile→local→FITS index` 与独立 astropy-healpix/HiPS reader，标准Dir分组、legacy fallback、MOC/manifest 缺失与损坏、输入BITPIX=-32/-64、duplicate required key/非512宽/order>20、极区和跨RA=0。
2. **插值**：取真实HEALPix叶中心＋输出WCS作 oracle，分别对常量 `B0`、切平面`bx*x+by*y`、二次曲率/解析Gaussian/Moffat PSF、星核和扩展天体。每个真实位置记录 `Σc`、`Σcx`、`Σcy`、负权、重复leaf、无凸包、FWHM/孔径星等和空间频谱；真正验证 #90 而不是把任意非矩形反例当真实HEALPix ipix。
3. **NaN/coverage**：tile缺失、损坏tile（不能全归合法无覆盖）、所有角有效、1/2/3/4角非有限、0有效权、退化象限、相同leaf、无tile、finite 0和负值、partial NaN+预填sentinel权重。比较 sample API 与生产`p3_resampled.bin`的两个平面和缺失元数据。
4. **协方差**：注入 `[A,A,B,C]` 权重 `[.5,.5,0,0]` 及 `[A,A,A,A]` 验已修 #88 的对角独立leaf恒等；另注入真实跨leaf相关 covariance 的MC，验证 `a^T C a` 与 `Σa²V` 的不同，不能输出未真实发布的误差HDU。
5. **预算与IO**：1/N Runtime worker，cap_tiles=8/可调，10^3+不存在tile、多tile并发首次缺失、reader read-only同时、负缓存unbounded检测、峰值RSS、CfItsio读写锁等待、sub-block/queue配置和取消恢复；真实 M42/HST 尺度及可用像元掩膜，原参考例据均为历史，不作本轮PASS。

文献：IVOA HiPS REC-1.0（https://www.ivoa.net/documents/HiPS/）；Górski et al. (2005), ApJ 622, 759, DOI 10.1086/427976；Fernique et al. (2015), A&A 578, A114, DOI 10.1051/0004-6361/201526075；FITS 4.0/CFITSIO；具体局部 bilinear 几何与 masked weights 是 ACSD project-defined 而非文献已有保守积分算法。上述外部文献用于方法/标准定位，本 D7-B **未独立重新打开并逐段核验版本/许可证**；真正 MC/HST/M42/CLI E2E/构建均 NOT_RUN。
