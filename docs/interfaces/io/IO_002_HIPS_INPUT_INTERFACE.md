# IO-002 HiPS 输入读取接口冻结合同

> doc_id: DOC-IO-INTERFACE-002
> doc_status: ACTIVE_NORMATIVE
> task_id: IO-002 · wave: W2 · owner: SA-IO-07
> commit: `feat(io): IO-002 建立HiPS输入合同`（前台集成）
> source: `tasks/03_RUNTIME_DATA_IO_TASKS.md` IO-002 / `05_FIXED_SUBAGENT_BINDINGS.yaml`
> SA-IO-07 / 冻结约束 `AstroCS_ENGINEERING_CONSTRAINTS.md` F.3（DLL C ABI 边界）、
> F.4（接口字段/单位/shape/坐标/invalid/所有权逐项明确）、E（阶段间只经磁盘产品交换）
> 上游: DOC-IO-INTERFACE-001（IO-001 FITS 流式接口）——IO-002 的 tile FITS 平面读取
> 全部复用 IO-001 的 fits_core 契约与错误码

## 1. 目标与范围

IO-002 在 IO-001（流式 FITS C ABI）之上建立 **HiPS 输入读取合同**（W2 宿主基础设施，
非 W3 科学迁移）：给定一个 IVOA HiPS 1.4 兼容目录（Phase1/Phase2 在磁盘上产出的
标准 HiPS 产品集或子产品目录），IO-002 负责：

1. 解析并校验 `properties` 元数据（hips_version/order/tile_width/tile_format/frame/…）；
2. 解析并校验 **NESTED tile address**（`NorderK/DirD/NpixN.fits` 布局、K 与 ipix 域、
   FIRSTPIX/LASTPIX/NSIDE 一致性）；
3. 校验 **tile width / tile order**（tile_width 必须为 2 的幂且与 order 的 HEALPix
   分辨率关系自洽；拒绝布局不符）；
4. 读取 **FITS-only scientific plane**（signal/support/variance/ivar 等科学平面
   **只接受 FITS**；`hips_tile_format` 为 png/jpg/tsv 等一律拒绝）；
5. 支持 **partial tree**（只读叶级/局部 tile，不要求全树存在；缺 tile 有明确状态
   `MISSING`，不虚构数据）；
6. **MOC optional hint**：`Moc.fits`（IVOA MOC UNIQ 序）可作为叶级 tile 集合的
   可选提示枚举叶 tile；无 MOC 时按调用方显式 NESTED ipix 定位单个 tile；
7. 明确 **缺 tile 状态**：文件不存在 / 非法 tile / 无 properties / 空 plane 各自
   映射稳定错误码；**不做父 order 静默回退**（调用方请求 order K tile 缺失时，
   绝不返回 order K′<K 的父 tile 内容或把低阶 hierarchy 当该 tile 交付）。

本任务**不改科学公式**（`scientific_change=false`）；不做 tiles 解码、投影/天球坐标
转换（科学层属于 P1/P2/P3 模块）；不做 HiPS 输出/原子发布（IO-003 范围）；
`lib/astro_image_io`（aio_hips_*，CFITSIO 静态链）与 `lib/healpix_db` 保持原样不修改。

## 2. 模块归属与目录

| 内容 | 路径 |
| --- | --- |
| 本冻结合同 | `docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md` |
| HiPS 输入 C ABI | `modules/services/io/include/astrocs/io/hips_input_v1.h` |
| HiPS 输入核心（C 实现、私有、DLL 内） | `runtime/io/hips_core.c` |
| 契约/负测（Python，astropy/astropy_healpix 可选 oracle） | `tests/io/` |
| C 层自检驱动 | `modules/services/io/tests/hips_core_selftest.c` |
| fixture 重建生成器 | `tests/io/make_hips_fixture.py` |

允许写路径：`runtime/io/** lib/io/** lib/astro_image_io/** lib/healpix_db/**
modules/services/io/** tests/io/** docs/interfaces/io/**`。

## 3. 输入合同（IVOA HiPS 兼容子集）

IO-002 读取的 HiPS 目录是 **磁盘上已发布的 HiPS 产品**。两档入口：

- **子产品目录**（`aio_hips_*` 产线布局，`docs/ARCHITECTURE.md` §产品流）：
  `<out>/signal|support|variance|ivar/` —— 目录内含 `properties`、可选 `Moc.fits`、
  `NorderK/DirD/NpixN.fits` tile 树。子产品目录与 `properties` 同层。
- **产品集根目录**（Phase1 输出根，含多个子产品目录）：入口接受
  `product = signal|support|snr|variance|ivar` 选择子目录（跨阶段磁盘交换语义）。

### 3.1 properties 必填键（缺失/非法一律拒绝）

| 键 | 含义 | 校验 |
| --- | --- | --- |
| `hips_version` | IVOA HiPS 版本 | 必须存在；本合同接受 "1.4"（含 "1.4" 前缀） |
| `hips_order` | tile order K | 十进制整数，0 ≤ K ≤ 29；K 与 NSIDE=2^(K+9) 一致 |
| `hips_tile_width` | tile 宽 TW | 十进制整数；**必须为 2 的幂**且 1 ≤ TW ≤ 16384；TW=512 标准 |
| `hips_tile_format` | tile 格式 | **必须为 `fits`**（科学平面 FITS-only；png/jpg/tsv 等拒绝） |
| `hips_frame` | 参考系 | 必须为 `equatorial`（ICRS，本产品科学域） |

### 3.2 建议键（可选，存在则校验自洽）

| 键 | 说明 |
| --- | --- |
| `hips_status` | 任意字符串，不校验 |
| `creator_did` / `obs_title` / `obs_creator` | 溯源键，读入 properties 视图，不校验格式 |
| `moc_sky_fraction` 等 | 不参与读端校验 |

### 3.3 NESTED tile address 布局（严格）

tile 文件相对路径固定为

```text
Norder{K}/Dir{D}/Npix{N}.fits
```

其中：

- `{K}` = `hips_order`（十进制，无前导零歧义）；
- tile ipix 为 order-K NESTED 单元号，`0 ≤ ipix < 12·4^K`；
- `D = ipix / 10000`，`N = ipix % 10000`（IVOA HiPS 目录约定，`%` 为 C/Python 余数语义，
  恒有 `ipix = 10000·D + N`、`0 ≤ N < 10000`）；
- 调用方以 **NESTED ipix** 定位 tile；实现内部做 `D/N` 拆分并拒绝越界 ipix
  （ipix ≥ 12·4^K → `ACS_HIPS_ERR_ADDRESS`）。

tile FITS 头内卡（存在时校验一致）：

| 卡 | 校验 |
| --- | --- |
| `PIXTYPE` | 若存在必须为 `HEALPIX` |
| `ORDERING` | 若存在必须为 `NESTED`（本合同只支持 NESTED） |
| `COORDSYS` | 若存在必须为 `C`（equatorial/ICRS） |
| `NSIDE` | 若存在必须等于 `2^(K+9)`（tile 像素分辨率） |
| `FIRSTPIX` | 若存在必须为 `0` |
| `LASTPIX` | 若存在必须为 `TW*TW-1` |

**不做父 order 静默回退**：上述任意卡与 properties/请求不一致、或目标 tile 文件
不存在，直接返回对应错误/`MISSING`；绝不返回其它 order 的 tile。

### 3.4 科学平面（FITS-only）与 dtype

- 只有 `hips_tile_format=fits` 的 tile 被接受；读 plane 走 IO-001 `acs_fio_reader_*`。
- 支持的 dtype：`-32`(f32)/`-64`(f64)（与 DATA-HIPS-* 冻结一致：signal f32/f64，
  support f32/u8 但 HiPS 标准存 f32/f64；variance/ivar f32/f64）。u8 仅按 BITPIX=8
  读取为字节平面并置 `data_bitpix=8`（支持域内，不自动转换）。
- 其它 BITPIX（16/32/64）→ `ACS_HIPS_ERR_TILE_INVALID`（布局不符拒绝）。
- tile FITS 必须是 2D 图像 HDU（NAXIS=2），`NAXIS1=NAXIS2=TW`，否则布局不符拒绝。
- 单元（BUNIT）不强制：signal 面亮度、support 无量纲[0,1]等单元语义由 DATA-001
  schema 冻结；读端透出 header 供上层比对，不隐式做单位换算。

### 3.5 partial tree / MOC optional hint / 缺 tile 状态

- **partial tree**：实现不扫描整个 tile 树；只按请求 NESTED ipix 拼路径定位。
  目录缺任意子树/部分 tile 不影响其它 tile 读取（`aio_hips_tile_count` 仅在
  有 MOC 时给出叶级集合大小）。
- **MOC optional hint**：`Moc.fits` 存在且为 IVOA MOC（BINTABLE + `UNIQ` 列 +
  `MOCORDER`）时，读端可解析出叶级（order == hips_order）UNIQ → 还原 NESTED ipix
  列表（`*_tile_count/_tile_ipix/_tile_exists` 由此枚举）；MOC 缺失/损坏不算错误
  （optional），只影响枚举能力。
- **缺 tile 状态**：请求的 ipix 在 order-K 域内但文件不存在 →
  `ACS_HIPS_ERR_TILE_MISSING`（不报 IO/不虚构）；文件存在但非法（卡冲突/尺寸不符/
  dtype 不支持/截断）→ `ACS_HIPS_ERR_TILE_INVALID` 或 IO-001 错误码透传。

### 3.6 properties 视图

`acs_hips_props_get` 提供 key→value 查询；`acs_hips_props_serialize` 提供整表
`key=value\n` 序列化视图（供上层 manifest/trace 落点）。properties 的 key/value
均为定长 UTF-8 缓冲（见头文件宏）。

## 4. 错误码（v1 冻结）

| 码 | 名称 | 语义 |
| --- | --- | --- |
| 0 | `ACS_HIPS_OK` | 成功 |
| 1 | `ACS_HIPS_ERR_PARAM` | 参数非法（NULL/越界/非法 ipix/product 串） |
| 2 | `ACS_HIPS_ERR_ABI_MISMATCH` | ABI 版本/结构大小失配（同 IO-001 语义） |
| 3 | `ACS_HIPS_ERR_NOMEM` | 内存不足 |
| 4 | `ACS_HIPS_ERR_IO` | 底层 I/O 失败 |
| 5 | `ACS_HIPS_ERR_UNSUPPORTED` | 不支持（非 fits 格式/非 equatorial/非 NESTED 等） |
| 6 | `ACS_HIPS_ERR_CANCELLED` | 取消（透传 IO-001 取消语义） |
| 7 | `ACS_HIPS_ERR_STATE` | 句柄状态错误 |
| 8 | `ACS_HIPS_ERR_PROPERTIES` | properties 缺失/非法键值（必填键缺/值不合法） |
| 9 | `ACS_HIPS_ERR_ADDRESS` | tile 地址/布局不符（ipix 越界/Norder 不符/路径不合规） |
| 10 | `ACS_HIPS_ERR_TILE_MISSING` | tile 不存在（order-K 域内但无文件；不做父回退） |
| 11 | `ACS_HIPS_ERR_TILE_INVALID` | tile 文件存在但布局/头/dtype 非法（含卡冲突、尺寸、截断透传） |
| 12 | `ACS_HIPS_STATUS_COUNT` | 哨兵 |

错误码 0–7 数值与 IO-001 `acs_fio_status`（对齐 common_abi_v1）一致，8–11 为
HiPS 输入扩展。错误文本同 IO-001 约定：`char err[96]` 只做日志，不承载状态机。

## 5. C ABI（hips_input_v1.h 摘要）

```c
int acs_hips_open_v1(const char* base_dir_utf8, const char* product,   /* 可 NULL="." */
                     const acs_fio_trace_hooks_v1* hooks,
                     acs_hips_handle_v1* out, char* err, size_t err_cap);
void acs_hips_close_v1(acs_hips_handle_v1 h);

int acs_hips_props_get_v1(acs_hips_handle_v1 h, const char* key,
                          char* out, size_t out_cap, char* err, size_t err_cap);
int acs_hips_props_serialize_v1(acs_hips_handle_v1 h,
                                char* out, size_t out_cap, size_t* out_len,
                                char* err, size_t err_cap);

/* 布局/元数据查询 */
int acs_hips_get_order_v1(acs_hips_handle_v1 h, int32_t* out_order);
int acs_hips_get_tile_width_v1(acs_hips_handle_v1 h, int32_t* out_width);
int acs_hips_tile_count_v1(acs_hips_handle_v1 h, int64_t* out_count);      /* 有 MOC 才 >0 */
int acs_hips_tile_ipix_v1(acs_hips_handle_v1 h, int64_t index, uint64_t* out_ipix);
int acs_hips_tile_exists_v1(acs_hips_handle_v1 h, uint64_t ipix, int* out_exists);

/* tile 状态与科学平面读取 */
int acs_hips_tile_status_v1(acs_hips_handle_v1 h, uint64_t ipix,
                            int32_t* out_status /*ACS_HIPS_TILE_* */);
int acs_hips_read_tile_plane_f32_v1(acs_hips_handle_v1 h, uint64_t ipix,
                                    float* out, int64_t out_elem_capacity,
                                    int64_t* out_got, char* err, size_t err_cap);
int acs_hips_read_tile_plane_f64_v1(acs_hips_handle_v1 h, uint64_t ipix,
                                    double* out, int64_t out_elem_capacity,
                                    int64_t* out_got, char* err, size_t err_cap);
```

- `tile_status`：`ACS_HIPS_TILE_PRESENT=1` / `ACS_HIPS_TILE_MISSING=2` /
  `ACS_HIPS_TILE_INVALID=3`（存在但非法）。读取接口只接受 PRESENT；MISSING/INVALID
  → 对应错误码。**MISSING 绝不触发父 order 静默回退。**
- `out` 缓冲由调用方分配（元素容量 = TW²；容量不足 → `ACS_HIPS_ERR_PARAM`）。
- f32/f64 接口内部按 tile 实际 BITPIX（-32/-64/8）读取：-32 原样，-64 原样，
  8（u8）按字节提升 f32/f64；其余 BITPIX 在 `tile_status` 即判 INVALID。
- 布局/地址校验在 open 后按 properties 就绪；tile 校验在每个 tile 访问时做。
- 并发：reentrant；句柄非共享（同 IO-001 句柄语义）。cancel 经 hooks 透传给
  fits_core（IO-001 取消语义）。

## 6. tile FITS 读取与 IO-001 的复用

IO-002 **不重新实现 FITS 解析**：`acs_hips_read_tile_plane_f*` 内部对拼出的
`<dir>/NorderK/DirD/NpixN.fits` 调用 IO-001 `acs_fio_reader_open_v1` /
`acs_fio_read_plane_v1`（期望 shape=TW×TW、BITPIX 0=按文件），失败映射：

| fits_core 返回 | HiPS 映射 |
| --- | --- |
| OK | 继续读 plane |
| `ACS_FIO_ERR_TRUNCATED` / `BAD_HEADER` | `ACS_HIPS_ERR_TILE_INVALID` |
| `ACS_FIO_ERR_MISMATCH` | `ACS_HIPS_ERR_TILE_INVALID`（布局不符） |
| `ACS_FIO_ERR_NANINF` | 透传（读端默认放行 NaN；严格策略由上层用 strict 语义自行扫） |
| 其它 IO/取消 | 透传原码 |

plane 读回后按调用方 dtype 目标转换；`NAXIS1!=NAXIS2!=TW`、卡冲突等由 tile 头校验
前置拦截。

## 7. fixture 可重建（验收）

`tests/io/make_hips_fixture.py` 是 **fixture 重建生成器**：以固定随机种子合成
小 HiPS（可选 order，默认 K=1，TW=512 数据为梯度块）并写

```text
<tmp>/hips_fixture/
  properties                 # hips_version/order/tile_width/frame/tile_format=fits/…
  Moc.fits                   # UNIQ BINTABLE（MOCORDER=K），叶级 ipix 集合
  Norder1/Dir0/Npix0.fits …  # K=1 叶 tile (ipix 0..n-1, n 由种子决定 = partial)
  Norder1/Dir0/Npix1.fits …  # 等
```

并可用 `--rebuild <dir>` 一键重建（验收：fixture 可重建）。契约测试从生成器构造
正/负 fixture：

- 正例：完整合规 partial tree + properties + MOC → 读 tile 与生成数据一致；
- 负例（全部拒绝）：
  1. tile width 非 2 的幂（properties `hips_tile_width=513`）→ PROPERTIES/UNSUPPORTED；
  2. 布局不符（Moc.fits 的 UNIQ 非 NESTED 序、或 Norder 目录与 order 不符、
     tile FITS NAXIS1≠TW）→ ADDRESS/TILE_INVALID；
  3. 缺 properties → PROPERTIES；
  4. 未知 frame（`hips_frame=galactic`）→ UNSUPPORTED；
  5. PNG-only（`hips_tile_format=png`）→ UNSUPPORTED；
  6. parent fallback（请求缺 tile ipix，父 order 存在同族 tile）→ TILE_MISSING
     （**不返回父 tile 内容/不回退**）。

## 8. 验收映射（tests/io/）

| 验收 | 测试 |
| --- | --- |
| 非 2 次幂 tile width | `test_reject_tile_width_not_pow2` |
| 布局不符（Norder/ipix/NAXIS 冲突） | `test_reject_layout_mismatch_*` |
| 缺 properties | `test_reject_missing_properties` |
| 未知 frame | `test_reject_unknown_frame` |
| PNG-only | `test_reject_png_only` |
| parent fallback 拒绝 | `test_no_parent_fallback_missing_tile` |
| partial tree + 缺 tile 状态 | `test_tile_status_present_missing_invalid` |
| MOC optional hint（有/无 MOC 均可） | `test_moc_hint_optional_enum` |
| fixture 可重建 | `test_fixture_rebuild_deterministic` |
| FITS-only 科学平面往返 | `test_read_signal_plane_matches_fixture` |

## 9. 与相邻接口/实现的边界

- IO-001 fits_core：tile FITS 平面读取的唯一底层（§6）；错误码 0–7 对齐。
- `lib/astro_image_io`（aio_hips_*，CFITSIO 链）：历史 HiPS 读写实现（writer 产线、
  reader 供浏览器）；**IO-002 不修改**。IO-002 是 modules/services/io 下与 IO-001
  同层的输入合同骨架；产线 writer 与 IO-002 的磁盘布局合同相同（§3.3）。
- `lib/healpix_db`：浏览器/历史参考；不修改。
- DATA-001 artifact schema / DATA-HIPS-*：科学 dtype/unit/invalid 语义权威；
  IO-002 只透出 header/plane，不做单位换算。
- IO-003：HiPS 原子输出/manifest —— 在 IO-002 读端之外；本任务不实现写端。
- MOC 解析：读端只解析 IO-002 支持的叶级 UNIQ 集合（BINTABLE+`UNIQ`+`MOCORDER`），
  与 `aio_hips_reader` 的 MOC 用法语义一致（optional）。

## 10. 已知限制（v1 骨架）

1. Linux 控制节点（本任务执行环境）无 MSVC/Windows DLL 构建；产出 C ABI + C 核心 +
   Linux `.so` 技术预览与全部契约/负测。Windows 正式 DLL 构建（astrocs_io.dll）在
   W6 用同一源码执行。
2. 只支持 `hips_frame=equatorial`、`ORDERING=NESTED`、`hips_tile_format=fits`；
   galactic/tsv/png/jpg 目录拒绝（科学平面 FITS-only）。
3. MOC 仅作 optional hint：无 MOC 时 tile 枚举接口返回 0；单个 ipix 定位不受影响。
4. 不做父 order 静默回退；也**不提供**显式父 tile 定位/层级回退接口（v1 最小合同；
   浏览器 LOD 需要时由上层按 Norder 拼接，超出本任务范围）。
5. `tile_status` 只区分 PRESENT/MISSING/INVALID；不细分 FITS 内部错误（透传 err 文本）。
6. u8 tile（BITPIX=8）在支持域内（DATA-IMG-SUPPORT-001 u8 语义），但 HiPS 标准
   图像产品一般存 f32/f64；f32/f64 接口读取时提升。
7. 大数据 tile 树扫描/远程 HiPS（http）不在本任务（磁盘输入合同；网络属未来 GUI/服务层）。
