# Phase3 HiPS Resample 实现级算法合同（ALG-P3-RSMP-IMPL-001）

> ID: ALG-P3-RSMP-IMPL-001  状态: CONTRACT_READY（P3-RSMP-DOC 冻结，
> 2026-09-12，SA-P3-R）  模块: astrocs.p3.resample（迁移合同值；
> registry 生产 descriptor 占位 astrocs.phase3.resample2 由 P3-RSMP-INT
> 对齐）  上游 SCI: SCI-P3-001（docs/science/PHASE3_HIPS_TO_FITS.md，
> FROZEN V5 SCI-007 2026-08-28，零改动；矩阵行 science_id 引用的
> SCI-P3-RES-001 为 MISSING 占位，本域全部科学内容映射声明为
> SCI-P3-001，见 §5）。承接既有 ALG-P3-003 本域子面（G3/G4 施工规格，
> docs/algorithms/PHASE3_RESAMPLE.md，公式零改动）。
> 本文档为 HiPS 重采样域**实现级合同**：逐符号源码行号锚定 + 冻结公式 +
> 错误语义 + 并发/确定性合同 + TEST 设计冻结 + 实测偏差登记。
> 生产源: lib/phase3_session/p3_resample.h（58 行，唯一权威签名头）+
> lib/phase3_session/p3_resample.cpp（239 行），实测 2026-09-12。
> 代码中不得出现第二套数学核心（healpix 权威函数唯一，见 §6）。

## 1 目的与非目标

- 目的: 冻结 Phase3 HiPS tile 重采样域的全部公共消费面——sampler
  生命周期（open/open_ex/set_max_tiles/close）、order 选择
  （p3_order_select）、输入模式守卫（p3_resample_check_mode）、
  NEAREST/BILINEAR 逐输出像素采样（含跨 tile 邻域读取）、tile 缓存——
  作为 P3-RSMP-IMPL/TEST/INT 的合同基线。
- 非目标: 不实现 variance/weight/ivar/support 输入（SCI §9a-10 显式拒，
  输出仅 S+coverage+provenance）；不做 flux-per-pixel 面积换算
  （SCI §9a-8 显式拒）；不做 alpha channel/BLANK int tile（SCI §9a-9
  显式拒）；不改投影（G1/G2 属 ALG-P3-PROJ-IMPL-001）与 FITS 原子写
  （G5 属 writer 域）；不引入第三种采样核（nearest/bilinear 之外显式拒）。
- 本任务（P3-RSMP-DOC）为合同冻结层：只登记，不修生产码；缺陷走 DISP
  登记（§13），由后续 IMPL/INT 任务整改。

## 2 身份与落位

| 项 | 合同值 | 实测 |
|---|---|---|
| module_id（矩阵 CSV 权威） | astrocs.p3.resample | MODULE_MIGRATION_MATRIX.csv P3-RSMP 行 |
| registry 生产 descriptor | astrocs.phase3.resample2（module_adapters.cpp:498 `p3_resample2_descriptor`，module_id 同名） | 实测 |
| dll_target | astrocs_p3_resample.dll | 矩阵 CSV；entrypoint 实测未建（DISP-P3RSMP-005） |
| owner | SA-P3-S26 | 矩阵 CSV |
| 合同目录 | lib/phase3_rsmp/（本任务新建，仅合同文件，无源码） | 生产源仍在 lib/phase3_session/ |
| 依赖 | lib/common/healpix（leaf_to_tile_nest/tile_to_leaf_nest/nested_local_to_fits_index/ang2pix/pix2ang）、lib/phase3_session/hips_properties（properties 严格校验经 p3_sampler_open 间接消费） | §3 |
| 下游 | P3-RSMP-IMPL（实现）、P3-RSMP-TEST（可执行测试）、P3-RSMP-INT（descriptor 对齐 astrocs.p3.resample） | — |

- 静态库落位: astrocs_phase3_session（根 CMakeLists.txt:460-465，
  p3_resample.cpp 为五源文件之一）。
- 会话编排消费: lib/phase3_session/p3_session.cpp（§8 逐点锚定）。
- descriptor 端口绑定（module_adapters.cpp:363-377）: 输入
  DATA-P3-WCS（必需）+ DATA-HIPS-001（必需），输出 DATA-P3-RES
  （可选产出）。

## 3 生产源图（实测，2026-09-12）

```text
lib/phase3_session/p3_resample.h        58 行  唯一权威签名头（10 个公共符号，§4）
lib/phase3_session/p3_resample.cpp     239 行  全部实现（§6 逐符号）
lib/phase3_session/p3_session.cpp      343 行  会话编排消费（§8）
lib/common/healpix/healpix_core.{h,cpp}         权威球面函数（禁止第二套核心）
tests/backend/p3_resample_probe_main.cpp        探针（order/mode/open/nearest/bilinear/pix2ang 六模式）
tests/backend/test_p3_resample.py      156 行  最近邻邻接测试（编译探针+seam/NaN/无静默默认）
tests/backend/test_p3003_parallel_resampler.py 104 行  并行域邻接测试
tests/unit/p3_interp_test.cpp          109 行  单元参考实现（独立参考，非生产自证）
tests/unit/p3_coverage_test.cpp        106 行  同上（coverage 语义）
```

- p3_resample.cpp 内部结构: :18 `kTileWidth=512`（编译期常量，SCI
  §9a-1 tile 宽冻结一致）；:19 `kReaderBuf=512*512`（FITS 读缓冲）；
  :22-36 `TileCache`（FIFO 最旧逐出有界缓存，§7）；:38-47 匿名命名空间
  辅助；:49-80 `read_leaf`；:82-93 `p3_order_select`；:95-107
  `p3_resample_check_mode`；:109-194 `P3SamplerImpl`（open/open_ex/
  set_max_tiles/close）；:196-230 `p3_sample_bilinear`；:232-239
  `p3_sample_nearest`。

## 4 符号冻结（实测签名，不改码）

lib/phase3_session/p3_resample.h 全部公共符号（10 个）：

| 符号 | 头锚 | 实现锚 | 签名要点 |
|---|---|---|---|
| `P3ResampleStatus` | h:12-17 | — | enum: `P3_RS_OK=0 / P3_RS_PARAM=1 / P3_RS_UNSUPPORTED=2 / P3_RS_IO=3` |
| `p3_order_select` | h:21 | cpp:82-93 | `(int max_order, double scale_deg_per_px, int* out_order)` |
| `p3_resample_check_mode` | h:25 | cpp:95-107 | `(const char* input_mode)`；`surface_brightness` 唯一合法 |
| `P3SamplerImpl` | h:28（前置声明） | cpp:38-47 等 | 不透明实现类型 |
| `P3Sampler` | h:29-31 | — | `P3SamplerImpl* impl; char last_error[256];` |
| `p3_sampler_open` | h:32-33 | cpp:109+ | `(const char* hips_dir, P3Sampler* out, char* err)`；= open_ex 缺参薄封装 |
| `p3_sampler_open_ex` | h:36-38 | cpp:130-159 | `(hips_dir, out, int* out_order, char* out_bunit, char* err)` |
| `p3_sampler_set_max_tiles` | h:42 | cpp:161-168 | `(P3Sampler*, int max_tiles)`；≤0 恢复默认 8 |
| `p3_sample_nearest` | h:46-47 | cpp:232-239 | `(const P3Sampler*, const P3WcsDescriptor*, int x, int y, float* value, float* coverage)` |
| `p3_sample_bilinear` | h:51-52 | cpp:196-230 | 同上签名 |
| `p3_sampler_close` | h:54 | cpp:170-180 | `(P3Sampler*)`；释放 impl，置 nullptr |

- 内部符号（合同可见但非导出）: `read_leaf`（cpp:49-80）、
  `TileCache`（cpp:22-36）、`kTileWidth=512`（cpp:18）。
- 头注合同（h:1-3）: 覆盖跨 tile 采样、coverage/mask 二值、NaN 语义
  （SCI §4）、单位固定 surface brightness、未支持输入模式显式拒
  UNSUPPORTED。与 SCI §7 非目标清单一致。

## 5 SCI/ALG 映射声明

- 本域矩阵行 `MOD-astrocs-phase3-resample2` 的 `science_id` 现引用
  `SCI-P3-RES-001`（MISSING 占位，无权威文档）。本合同声明：本域全部
  科学内容以 **SCI-P3-001**（docs/science/PHASE3_HIPS_TO_FITS.md，
  FROZEN）为唯一权威，映射关系
  `SCI-P3-RES-001 ⇒ SCI-P3-001`；矩阵行 id 占位保持原样（
  占位词汇与权威文档分离，照 P3-PROJ-DOC 先例），由 SCI 层收编任务
  统一改名，本任务不改 docs/science/。
- ALG 层: docs/algorithms/PHASE3_RESAMPLE.md（ALG-P3-003 施工规格）
  §2 的 **G3（order 选择）与 G4（leaf 采样）** 为本域子面；本合同承接
  其全部公式（零改动，§6 给出等价性核对）。G1/G2（投影）属
  ALG-P3-PROJ-IMPL-001，G5（FITS 写）属 writer 域，不在本合同。
- DATA 层: 输出张量语义 = DATA-P3-RES（docs/contracts/DATA_SEMANTICS.md
  §29，本任务新建）；输入 properties/tile 语义 = DATA-HIPS-001/
  DATA-TILE-001（§3 冻结）。
- API 层: 公共消费合同 = API-P3-RSMP-001（docs/contracts/PUBLIC_API.md
  本任务新节），登记上面 10 符号。

## 6 逐符号冻结（公式与实现等价性）

### 6.1 G3 order 选择——`p3_order_select`（cpp:82-93）

冻结语义：在线性扫描 `k ∈ [0, max_order]` 中找**最小** k 使

```text
pixel_resolution_arcsec(nside=512 << k) / 3600 ≤ scale_deg_per_px
```

其中 `pixel_resolution_arcsec(nside) = sqrt(4π/(12·nside²))·180·3600/π`
（lib/common/healpix/healpix_core.cpp 权威实现）= `sqrt(π/3)/nside` rad。
与 ALG-P3-003 G3 冻结式 `s_tile_rad = sqrt(π/3)/(2^order·W)`、
`order = clamp(ceil(log2(sqrt(π/3)/(W·s_out))), 0, hips_order)`
**数学等价**（nside = W·2^k，W=512）：取等价形式
`k ≥ log2( sqrt(π/3)/(W·s_out_rad) )`，即同一 ceil 表达式。无更细层时
（扫描完未命中）`*out_order = max_order`（欠采样降级为 survey 原生
分辨率输出，SCI §9a-5 允许并记录）。守卫: `!out_order || max_order<0
|| max_order>20 || !(scale_deg_per_px>0)` → `P3_RS_PARAM`（cpp:84-86）；
kMaxOrder=20 来自 hips_properties.h:23（ARCH-P3 §3）。
会话消费（p3_session.cpp:196-199）: `max_order = 输入实际 order`
（open_ex 返回值，clamp ≤20）——**禁仅写 metadata 的 order**。

### 6.2 输入模式守卫——`p3_resample_check_mode`（cpp:95-107）

`input_mode == "surface_brightness"` 唯一返回 `P3_RS_OK`；其余（含
nullptr/空串/`flux`/`flux_per_pixel`/variance/weight/ivar）返回
`P3_RS_UNSUPPORTED`。SCI §9a-8/§9a-10 显式拒的输入类型由此守卫拦截。
实测偏差: 会话编排层未接线（DISP-P3RSMP-003）。

### 6.3 sampler 生命周期

- `p3_sampler_open_ex`（cpp:130-159）: ①`hips_properties_parse`
  严格校验（必需 keys `hips_order, hips_tile_width, hips_frame,
  dataproduct_type`；`hips_order ∈ [0,20]`；`hips_tile_width` 必须
  512——hips_properties.cpp:122 显式拒非 512；NESTED 唯一）②校验
  `hips_frame` ICRS ③构造 `P3SamplerImpl`（nside=512·2^order）④
  `out_order`/`out_bunit` 回填实际值；BUNIT 缺省 `'ADU'`，
  **绝不缺省 Jy/beam**（头注 + SCI §9a-11）。失败路径: properties
  校验失败/路径不存在 → `P3_RS_IO`；语义不符 → `P3_RS_PARAM` 或
  `P3_RS_UNSUPPORTED`（frame≠ICRS）；err 缓冲写人类可读消息。
- `p3_sampler_open`（cpp:109+）: open_ex 缺参薄封装（out_order/
  out_bunit 传 nullptr）。
- `p3_sampler_set_max_tiles`（cpp:161-168）: 设置 TileCache 容量；
  `max_tiles ≤ 0` 恢复默认 8。负值不报错（按合同恢复默认）。
- `p3_sampler_close`（cpp:170-180）: 释放 impl 并置空；可安全对
  已关闭/空 sampler 调用（幂等）。

### 6.4 tile 寻址与读取——`read_leaf`（cpp:49-80）

冻结映射（全部经 healpix_core 权威函数，禁第二套实现）:

```text
tile_ipix  = leaf_ipix >> 18            # leaf_to_tile_nest(leaf_ipix, 9)（W=512 → shift=log2(512)=9）
local_x,y  = tile_to_leaf_nest 反交织    # leaf local 坐标（NESTED 18 bits）
fits_index = nested_local_to_fits_index(local, 9, 512)   # = (511-x)*512 + y（DATA_SEMANTICS §3 CDS oracle 冻结）
```

- 缓存命中: 直接返回 tile 缓冲（cpp:62-66 路径）。
- 缓存未命中: 定位 `order_{order}/dir{tile_ipix>>28}/Npix{tile}.fits`
 （HiPS 目录布局），FITS float tile 读入 `kReaderBuf`，插入缓存
 （可能触发 FIFO 逐出），再取像素（cpp:68-80 路径）。
- tile 文件不存在/读失败: 返回 false（= 无覆盖路径，**非错误抛出**），
  语义见 §6.6；SCI §9a-9 要求 provenance 记 missing tile——聚合上报
  未接线，DISP-P3RSMP-004。

### 6.5 G4 采样核（cpp:196-239）

- `p3_sample_nearest`（cpp:232-239）: 输出像素中心 → `pix2ang` →
  `ang2pix(nside_leaf)` → 精确 cell（tile=leaf>>18, local 如 §6.4）。
  无插值误差（SCI §9a-12）。tile 像素 NaN → `*value=NaN,
  *coverage=1`；tile 缺失 → `*coverage=0, *value=NaN`。
- `p3_sample_bilinear`（cpp:196-230）: ①输出像素中心 `pix2ang` →
  leaf 3×3 邻域（`healpix_neighbors`）②四象限各自取**最近中心**
  cell（d² 距离比较）③切平面双线性: `den = dx·dy2 - dx2·dy`，
  den≤0（背面/退化）跳过该象限；`|dx|>1e-300` 防 0 除；`u,v ∈
  [0,1]` clamp ④权重 `w00=(1-u)(1-v), w10=u(1-v), w01=(1-u)v,
  w11=uv`（FP64 计算，Σw=1 精确成立——SCI §9a-7 不变量；
  (void)y1 压 unused）⑤任一输入 NaN → 输出 `nanf("")`；
  `*coverage` 恒 1（到达插值即视为有足迹，缺失象限由最近中心
  吸收，见 DISP-P3RSMP-001 偏差登记）。六种坐标退化分支
  （cpp:196-230 分段）对应 SCI §9a-6 的 RA wrap/球面角差处理。
- 与 ALG-P3-003 G4 施工规格"面积重叠分数（投影线性化）"的差异:
  实现为**切平面四象限最近中心双线性**——同属一阶插值族、Σw=1
  不变量一致，离散化方案不同（DISP-P3RSMP-001 如实登记，不修码）。

### 6.6 值语义（SCI §4 冻结，实现一致）

| 输入状态 | *value | *coverage |
|---|---|---|
| tile 存在，像素有限 | 像素值（面亮度） | 1 |
| tile 存在，像素 NaN | NaN（传播，非错误） | 1 |
| tile 缺失/读失败 | NaN | 0 |
| 采样器未打开（impl=nullptr） | NaN | 0 |

- coverage 为**二值语义**（0/1，float 承载）；C=1 ⇔ 采样足迹内存在
  有限 tile 像素（SCI §5）。mask 输出由会话层从 coverage 生成
  （coverage_output 仅 `mask` 合法，p3_session.cpp:128-129）。

## 7 TileCache 并发/确定性合同（cpp:22-36）

- 有界 FIFO 缓存: 容量默认 8（`set_max_tiles` 可调，≤0 恢复 8）；
  插入超容时逐出 `keys` 队首（**最旧插入**，cpp:33 `keys.erase(
  keys.begin())`）——ALG-P3-003 §3 伪代码写 "LRU"，实现为 FIFO
  （无访问序更新），DISP-P3RSMP-002 如实登记。
- 键: `(order, tile_ipix)`；值: tile 缓冲。
- 并发模型: **每 worker 独立 sampler（自含独立 cache）**，跨 worker
  无共享可变状态；HiPS tile 文件只读共享，无写锁（p3_session.cpp:
  217-226 worker 闭包内 `p3_sampler_open_ex` 每 worker 重建）。
  单 sampler 实例本身**非线程安全**（无内部锁），禁止跨线程共享
  同一 `P3Sampler`——合同禁止项。
- 确定性: 逐出策略 FIFO + 逐像素独立采样 ⇒ 输出与 tile 装载顺序、
  worker 数、缓存容量**无关**（同一输出像素的计算路径固定: 邻域
  确定 → 最近中心确定 → 权重确定）。禁 hardware_concurrency 决定
  worker 数（=budget.max_workers，p3_session.cpp:211-214）。

## 8 会话编排合同（p3_session.cpp 消费链，实测）

| 消费点 | 锚 | 合同 |
|---|---|---|
| sampler 打开 | :171（映射 :175-176） | 主线程 open_ex 暴露输入实际 order/BUNIT；失败映射 P3_RS_IO→ACS_ERR_IO、UNSUPPORTED→ACS_ERR_UNSUPPORTED、PARAM→ACS_ERR_PARAM |
| max_tiles 守卫 | :179-194 | 默认 `min(1024, ceil(W·H/512²)+16)`；请求 > 默认 → `ACS_ERR_BUDGET`（可降不可升，P3-006/DOC-003） |
| order 选择 | :196-199 | max_order=输入实际 order（clamp ≤20），p3_order_select 就地选择 |
| worker 池 | :211-214/:246-255 | n_workers=budget.max_workers（禁 hardware_concurrency）；>hpx clamp 行带不空；<2 或未注入 budget → 串行；每 worker 独立 sampler+cache（:217-244 闭包，worker open_ex :222，§7） |
| 采样分派 | :236-237 | nearest/bilinear 按请求 `sampler` 字段分派（缺省 bilinear，parse_request :116-119 白名单） |
| 取消 | :258-263 | 行级取消检查 → `ACS_ERR_CANCELLED`（"cancelled at row N"，:260-261）；worker open 失败 → `ACS_ERR_IO`（:263） |
| provenance | :265-277 | order_sel_used/sampler_used 填实际值（:276-277）；manifest_hash/missing_tiles 恒 nullptr（:271-272，DISP-P3RSMP-004） |
| 请求守卫 | :95-129 | projection 仅 TAN（:97）；frame 仅 icrs（:103）；\|dec\|≤85°（:107）；scale>0（:109）；W/H∈[1,20000]（:114）；sampler∈{nearest,bilinear}（:116-119）；parity∈{east_left,east_right}；bitpix∈{-32,-64}（:127）；coverage_output 仅 mask（:129） |

## 9 错误语义冻结

| 状态 | 触发（实测） | 会话映射 |
|---|---|---|
| P3_RS_OK | 成功 | ACS_OK |
| P3_RS_PARAM | out_order 空/max_order 越界/scale≤0（:84-86）；properties 语义不符 | ACS_ERR_PARAM |
| P3_RS_UNSUPPORTED | input_mode 非 surface_brightness（:95-107）；frame≠ICRS | ACS_ERR_UNSUPPORTED |
| P3_RS_IO | properties 解析失败/HiPS 目录或 tile 文件不可读 | ACS_ERR_IO |
| （采样层无错误码） | tile 缺失 = coverage=0 数据语义（§6.6），非错误 | — |

- 采样函数（nearest/bilinear）无返回码（void 语义经 value/coverage
  表达）；`last_error[256]` 缓冲仅 open/set_max_tiles 路径填充。
- 禁止静默默认: sampler 关闭态调用采样函数 → coverage=0（不 crash、
  不伪造数据）；open 失败必须经 err 缓冲带原因（test_p3_resample.py
  test_06_no_silent_default_open 冻结此行为）。

## 10 资源合同

- 内存: cache 上限 `max_tiles·512²·4 B`（默认 8 tile = 8 MiB float；
  会话守卫默认 ≤1024 tile）；输出平面 2×W·H×4 B（S/C）。
- 每输出像素: 1 次 pix2ang + 1 次 ang2pix（nearest）或 1 次 pix2ang +
  1 次 neighbors + ≤4 次 ang2pix（bilinear）——均为 O(1)。
- 线程: worker 数 = budget.max_workers（≤hpx），禁止硬编码与
  hardware_concurrency（AGENTS 约束一致）；串行阈值 <2 workers。
- 取消: 行级响应（每行首检查），取消后已写行保留、报行号。

## 11 实测偏差与整改登记（不修码，如实冻结）

| ID | 偏差 | 实测依据 | SCI/ALG 依据 | 整改归属 |
|---|---|---|---|---|
| DISP-P3RSMP-001 | bilinear 为切平面**四象限最近中心**双线性；G4 施工规格写"面积重叠分数（投影线性化）"。同族一阶插值、Σw=1 不变量一致，离散化方案不同 | p3_resample.cpp:196-230 | ALG-P3-003 §2 G4 | P3-RSMP-IMPL 决策: 升级实现或修订 ALG 表述（须保持 SCI §9a-7 不变量） |
| DISP-P3RSMP-002 | tile cache 逐出为 FIFO；ALG §3 伪代码写 "LRU" | cpp:22-36（无访问序更新） | ALG-P3-003 §3 | P3-RSMP-IMPL（实现升级 LRU 或 ALG 修订为 FIFO 表述） |
| DISP-P3RSMP-003 | `p3_resample_check_mode` 会话编排层无调用点（flux/variance 输入拒未经会话守卫；能力在内核、探针消费） | grep 全仓: 仅 p3_resample_probe_main.cpp:31 | SCI §9a-8/10 | P3-RSMP-INT 接线（会话请求守卫增加 input_mode 检查） |
| DISP-P3RSMP-004 | provenance.missing_tiles 恒 nullptr（缺 tile 聚合上报未接线；SCI §9a-9 要求记 missing） | p3_session.cpp:265-277 | SCI §9a-9 | P3-RSMP-IMPL/INT |
| DISP-P3RSMP-005 | astrocs_p3_resample.dll 未建（entrypoint 缺失） | 全仓无该 target | 矩阵 dll_target | P3-RSMP-IMPL |

## 12 TEST-P3-RSMP-DESIGN-001 设计冻结（登记面 VERIFIED）

- 定位: 本域可执行测试面在 P3-RSMP-TEST 落地；矩阵行 test_id=
  TEST-P3-RES-001 状态 **DORMANT**。本节冻结**设计**（登记面
  TEST-P3-RSMP-DESIGN-001，VERIFIED），不冒认可执行覆盖。
- 现有证据（引用不冒认，均测试独立参考实现或探针，非生产自证）:
  - tests/backend/p3_resample_probe_main.cpp（探针六模式: order 选择/
    mode 守卫/open/nearest/bilinear/pix2ang——pix2ang 用 nside=4 首子
    像素中心技巧）。
  - tests/backend/test_p3_resample.py（156 行）: test_05_nan_semantics
    （tile 内 NaN → C=1 + 值 NaN，§6.6 行 2）、test_06_no_silent_
    default_open（open 失败必须带原因）、seam 域界断言（1e8-1..
    12e8+1）与连续性（1e-5° 位移）——SYN-007 判据邻接。
  - tests/backend/test_p3003_parallel_resampler.py（104 行，并行域）。
  - tests/unit/p3_interp_test.cpp（109 行）/p3_coverage_test.cpp
    （106 行）: 独立参考实现核对插值/覆盖语义。
- P3-RSMP-TEST 设计要求（合同）: ①order 选择等价性（对拍
  pixel_resolution_arcsec 公式，§6.1）②seam 连续性（SYN-007 预冻结
  容差）③NaN/coverage 值语义全表（§6.6 四行）④缺 tile → C=0
  ⑤nearest 精确 cell ⑥bilinear Σw=1 与背面跳过 ⑦max_tiles 逐出
  行为（FIFO）⑧open 守卫（非 512 tile/frame≠ICRS/order 越界）。
- 判定口径: 可执行测试 VERIFIED 须生产码+测试同在现场（C7），当前
  不可冒认——test_status 维持 DORMANT。

## 13 合同边界与 DISP 登记

- 本合同冻结**现状实现**为合同基线；§11 五项偏差不构成合同违反，
  整改走 P3-RSMP-IMPL/INT，不修生产码于本任务。
- 禁止项（合同）: 第二套 healpix 数学核心；第三种采样核；flux/
  variance/weight 输入静默接受；BUNIT 缺省非 ADU；仅写 metadata 的
  order；hardware_concurrency 决定线程数；静默默认 open。
- DISP 台账: §11 表五项（DISP-P3RSMP-001..005），本任务登记，
  不在本任务闭环。

## 14 SCI 层零改动声明

- 本任务对 docs/science/PHASE3_HIPS_TO_FITS.md 零改动（git status
  验证）；全部科学公式/容差/拒绝清单以 SCI-P3-001 FROZEN V5 为准。
- ALG 层: docs/algorithms/PHASE3_RESAMPLE.md 仅做表述级修订
  （tile cache 逐出表述、实现锚补记），G1-G5 公式零改动。
