# AstroCS Data Semantics（跨阶段唯一数据合同）

权威：本文档。任何模块/文档不得出现第二套定义。

## 1. Sky coordinates

- RA/Dec 以**度**为单位（0..360 / -90..90），ICRS/equatorial。
- 内部球面计算（healpix_core）使用弧度；公共 ABI 一律度。

## 2. HEALPix

- **NESTED** 是唯一允许的 ordering（ring 未迁移）。
- `order K` → `nside = 2^K`；`leaf_order = tile_order + 9`（512×512 tile）。
- `nside` 恒为 2 的幂；非法值拒绝。

## 3. FITS tile local-pixel 映射（V5/V11 冻结）

```text
leaf local（NESTED, 18 bits）= interleave(x, y)，x=偶数位, y=奇数位
FITS index = (511 - x) * 512 + y
```

由 CDS Hipsgen 外部 oracle 冻结（205625/205627 点），writer/reader/browser
共用同一 `nested_local_to_fits_index` / `fits_index_to_nested_local`。

## 4. signal / support / invalid

- `signal`：科学表面亮度（float32/64），**不使用 display stretch**；负值保留，
  不自动加 pedestal、不无故 clamp。
- `support`：覆盖/有效支持度 [0,1]；`support=0` 表示无覆盖。
- `invalid`：`NaN` 或 `support<=0`；有效样本判定为 `finite && support>0`。
- 无有效样本（all-rejected / denominator≈0）必须有明确 status，禁止静默
  输出 0 或 ±Inf（integrate.cpp status=1/2 guard）。

## 4a. variance / ivar 产品（DATA-HIPS-VAR-001 / DATA-HIPS-IVAR-001）

- `variance`：逐像素随机方差（信号单位²），Drizzle 方差传播
  `variance_p = Σ v_j·w_jp² / D_p²`（SCI-DRZ-014）；无覆盖像素=0。
- `ivar`：逆方差 `1/variance`；variance=0/缺失 → ivar=0（显式不可用，
  禁止伪装）；NaN/负 variance 视为产品损坏。
- 相邻像素非严格独立（协方差已文档化，见
  docs/science/UNCERTAINTY_AND_COVARIANCE.md），pixel variance ≠
  aperture variance。
- HiPS 子产品位：`AIO_HIPS_PRODUCT_VARIANCE=8`、`AIO_HIPS_PRODUCT_IVAR=16`。
- **无覆盖值歧义消解（DATA-UNC-001 登记，2026-09-09）**：本节
  "无覆盖像素=0 / ivar=0" 与 §12.4 P1 写侧 "variance/ivar=NaN" 存在
  预存双值（finding F-UNC-001，归 Phase1 域任务消歧，本文不在
  此处单方改写）；Phase2 合成与 Phase3 传播**输出产品**的无覆盖/无效
  语义以 §30 为唯一权威（NaN 同态，§30.1/§30.4）；ivar=0 仅保留为
  Phase2 读侧对逐帧 ivar 产品的合法零权重消费态（§20.1/§21）。

## 5. frame identity / manifest（DATA-FRAME-ID-001，V19R4 冻结）

- frame_id：`p2_frame_id(path)` =
  `truncated-64(canonical SHA-256 of science payload identity)`；
  输入字段精确为：关键 properties 白名单（creator_did/obs_title/
  obs_filter/obs_exptime/obs_date/hips_order/hips_release_date/
  hips_pixel_scale/moc_sky_fraction）+ signal tile 像素 + support tile
  像素 + SNR catalogue 内容；与路径/重命名/换根目录无关；任何科学
  payload 变化 → 改变；取 SHA-256 前 16 hex 字符（大端序）为 uint64；
  与输入顺序无关；参考帧 = 每分量最小 frame_id。
- 碰撞策略：64 位截断哈希（工程上忽略碰撞；重复 frame_id 在 UPM
  构建/持久化层显式拒绝）。
- 禁止描述为 FNV-1a / 路径派生（旧文档已修正，docs exact checker
  的 frame_id_contract_exact 全仓校验）。
- input_manifest_hash：输入集合与配置的稳定摘要（stage2 diagnostics）。
- HiPS properties 中 `hips_creation_date` 为真实 UTC 时间，不伪造。

## 6. precision

- control 采样：float32 读入，double 统计（median/MAD）。
- UPM 求解：double（IRLS/CG）；输出产品按 config float32/64。
- block/micro-chunk：只改变执行顺序，不改变科学结果（块不变量）。

## 7. Cross-stage handoff（Phase1 → Phase2）

Phase2 只消费 Phase1 输出：

```text
signal / support / MOC(union tiles) / SNR catalogue / quality flags /
frame_id / manifest / RA-Dec 度 / NESTED / 512-tile 映射
```

契约测试：见 `docs/development/TESTING.md`（cross-stage contract test）。

## 8. Gaia XPSD 星表输入与星表行（DATA-GAIA-001）

> ID: DATA-GAIA-001  状态: CONTRACT_READY（CAT-GAIA-DOC 冻结，2026-09-05）
> 模块: lib/gaia_xpsd_client（astrocs.catalog.gaia）；ALG: ALG-GAIA-001；
> SRC: lib/gaia_xpsd_client/src/gaia_client.c

### 8.1 输入：本地 XPSD 数据集目录

- 形态：目录内 ≤32 个 `.xpsd` 文件（MAX_FILES），PixInsight XPSD 格式
  （魔数 `XPSD0100` + XML 头 + 6 棵投影树四叉索引 + LZ4/zlib(+shuffle) 压缩
  数据块）；GaiaDR3（32B 记录，无光谱）或 GaiaDR3SP（384B 记录，含光谱）。
- 来源：离线本地文件（无网络 I/O）；目录路径由调用方提供，模块不写任何
  输入文件。
- dataset identity：db_type（由 `DatabaseIdentifier` 含 GaiaDR3SP 与否判定）
  + file_count，进入查询缓存键（ALG-GAIA-001 §2.7）。

### 8.2 输出：星表行（三个变体）

坐标 frame 一律 ICRS/J2000，RA/Dec 单位度，`RA∈[0,360)`、`Dec∈[-90,90]`；
星等无量纲（G/BP/RP）；光谱流量 W·m⁻²·nm⁻¹，波长 nm。

| 字段 | dtype | 单位/域 | invalid / sentinel |
|---|---|---|---|
| ra, dec | float64 | deg, ICRS J2000 | 无 NaN 值输出（量化解码有界） |
| magG / magBP / magRP | float64 | mag | `raw×0.001−1.5`；DR3 数据下 BP/RP 恒 0（sentinel，非真值） |
| flux_min, flux_mul | float32 | W·m⁻²·nm⁻¹ | 无光谱记录时 0（sentinel） |
| 光谱字节块 | uint8[343]/星 | 行主序 `out_spectra[i*spec_n + j]` | 解码 `byte*flux_mul+flux_min`；`out_spectra=NULL` 表示无光谱 |
| out_match_idx | int32 | 坐标序 | −1 = 该坐标未匹配 |
| out_ra/out_dec (solver) | float64 | deg | out_mag float32 mag |
| out_count | int32 | 行数 | 0 = 空结果（合法，非错误） |

shape 契约：`out_stars` 为 `out_count` 行连续数组（C ABI 顶层 malloc，调用方
free）；`out_spectra` 为 `out_count × global_spec_count` 字节（global_spec_count
= 第一个含光谱文件的 spectrum_count，回退 343）。

明确**不输出**：`source_id`（恒 0 占位）、parallax/pmra/pmdec（输出结构体中
**未初始化**，调用方不得使用——现状契约，CAT-GAIA-IMPL 迁移时需显式置 0 或
剔除）、误差列、proper motion 消化坐标。

### 8.3 缓存与溯源语义

- 查询缓存（60s TTL/64 条）键=参数 double 逐位 + dataset identity + version=2：
  命中即同一查询精确重复，语义与冷路径 bitwise 等价（ALG-GAIA-001 I3）。
- 模块不产生科学产物文件、不参与 artifact store；溯源由上游调用方在
  manifest 记录数据目录/db_type/查询参数（cache provenance 责任在调用侧，
  matrix 科学专项"cache provenance"由此承担）。

## 9. Phase1 校准模块输入/输出数据（DATA-P1-CAL）

> ID: DATA-P1-CAL  状态: CONTRACT_READY（P1-CAL-DOC 冻结，2026-09-07）
> 模块: lib/calibration（astrocs.p1.calibration）；SCI: SCI-CAL-001；
> ALG: ALG-CAL-001..004；SRC: lib/calibration/include/astro_calibration.h。
> 本节冻结现有 C API 的真实数据语义（P1-CAL-DOC 源码核对），迁移 DLL
> astrocs_p1_calibration.dll 由 P1-CAL-IMPL 建立（语义不变）。

### 9.1 输入（内存数组，无文件 I/O）

行主序 `idx = y·w + x`，0-based 像素坐标（无 WCS/坐标变换，frame identity
沿用 §5：逐像素算术不改变 frame_id）；单位除注明外均为 ADU。

| 数组 | dtype/shape | 单位/域 | invalid / NULL 语义 |
|---|---|---|---|
| stack（bias/dark/flat 帧） | float32 或 float64（f64 ABI）`[n_frames][h][w]` 连续 | ADU | 单帧元素可为 NaN（统计跳过，ALG-CAL-001 F1.3）；`n_frames==1` 直接拷贝含 NaN 原样保留 |
| master_bias / master_dark | `[h][w]` | ADU | 可为 NULL（calibrate: 不减；cosmetic: 不检测对应类） |
| master_flat | `[h][w]` | 无量纲（约定已 median≈1.0 归一，ALG-CAL-002 产物） | 可为 NULL（跳过除法）；floor 0.1 下界在 calibrate 内施加 |
| light（data） | `[h][w]` | ADU | NaN 直传输出（ALG-CAL-003，DISP-CAL-004） |
| 掩码 hot/cold（ac_correct_frame 内部） | char `[h][w]` | 0/1 | 极性 **1=坏点**（SCI-CAL-001 §9a） |
| sigma_low/sigma_high/hot_sigma/cold_sigma | float，无量纲 | MAD 倍数 | NaN 行为未定义（前置条件，负面测试覆盖）；sigma<=0 = 禁用对应检测 |
| K（dark_scale_factor） | float/double，无量纲 | =t_light/t_dark | 由调用方计算（曝光秒，FITS EXPTIME）；dark_opt=1 缺 bias/dark 时回退标准分支且 K=1.0 |
| combine | int 0=mean / 1=median | — | 其他值按 mean 路径（实现按 `==AC_COMBINE_MEDIAN` 判定） |
| method | int 0=median / 1=IDW（名义 bilinear） | — | 其他值按 IDW 路径 |

### 9.2 输出

| 数组 | dtype/shape | 值域 | invalid |
|---|---|---|---|
| out（master 或校准帧） | float32/float64 `[h][w]` | ADU（可负，**不 clamp 不加 pedestal**，§4 负值保留） | 全 NaN 像素列 → NaN（合法输出，非错误）；参数错误时不写 out |
| actual_k | float*/double* 单值 | 无量纲 | dark_opt=1 生效=入参 K；标准分支=1.0；参数错误=k_init；可 NULL |
| out_hot / out_cold | int* 单值 | 像素计数 | 结构过滤后掩码像素数；可 NULL |
| ac_version() | const char* 静态串 | — | `"Astro Calibration C++ v1.0.0"` |
| 错误码 | int | — | 0=AC_OK；−1=AC_ERR_PARAM（空指针/非正维度）；−2/−3 定义但**从未返回**（DISP-CAL-001） |

### 9.3 落盘产物（调用方侧，非本模块合同）

phase1_session 将 out 写为 `calibrated_<原名>.fits`（float32 ADU）；母版
分组（曝光/滤镜）与文件命名由 orchestrator 层负责（SCI-CAL-001 §4）。
模块本身零文件/网络 I/O（stderr 日志除外，ALG-CAL 文档 §4）。

### 9.4 variance / mask 边界

本模块不输出 variance/ivar（snr_estimator 独立估计，§4a）；坏点掩码
1=坏点仅在修复路径内部使用，不作为产品输出；`support`/coverage 概念
不在本层（Phase2/3 处理）。

## 10. Phase1 cosmetic 模块输入/输出数据（DATA-P1-COS）

> ID: DATA-P1-COS  状态: CONTRACT_READY（P1-COS-DOC 冻结，2026-09-07）
> 模块: lib/cosmetic（astrocs.p1.cosmetic，迁移目标；现行实现唯一生产源
> lib/calibration/src/cosmetic_corrector.cpp，经 astrocs_calibration 编译）；
> SCI: SCI-CAL-001；ALG: ALG-COS-001..005；上游: DATA-P1-CAL（§9）。
> 本节冻结 `ac_correct_frame(+_f64)` 的真实数据语义（P1-COS-DOC 源码
> 核对），迁移 DLL astrocs_p1_cosmetic.dll 由 P1-COS-IMPL 建立
> （语义不变）。与 §9 的重叠处（掩码极性、method、sigma 语义）以
> SCI-CAL-001 为共同权威，本节只登记 cosmetic 路径专属细化。

### 10.1 输入（内存数组，无文件 I/O）

行主序 `idx = y·w + x`，0-based；单位 ADU（σ 倍数/计数除外）；
无坐标变换（frame identity 沿用 §5）。

| 数组 | dtype/shape | 单位/域 | invalid / NULL 语义 |
|---|---|---|---|
| data（待修复帧，通常为 §9 校准帧） | float32（f64 ABI 经 double→float 降级，DISP-COS-004）`[h][w]` | ADU | NaN 逐像素透传（检测比较恒 false，不判坏——DISP-COS-002）；与 out 重叠未定义（DISP-COS-010） |
| master_dark（热检测源） | float32 `[h][w]` | ADU | 可为 NULL（热检测关闭，ALG-COS-004）；含 NaN → 阈值非数值、检测静默全 false（DISP-COS-002） |
| master_bias（冷检测源） | float32 `[h][w]` | ADU | 可为 NULL（冷检测关闭）；NaN 同上 |
| hot_sigma / cold_sigma | float，无量纲 | MAD 倍数（σ=1.4826·mad 换算） | <=0 = 禁用对应检测（ALG-COS-004）；mad=0 时阈值=±med（σ=0） |
| method | int 0=median / 1=IDW（名义 bilinear） | — | 非 0 一律按 IDW 路径（DISP-COS-003） |
| max_structure_size | int，像素个数 | 连通域尺寸上限 | sizes >= max_size 的 8 连通域不判坏（保留原值）；<=0 → 全域清除（负面现状，ALG §9） |
| out_hot / out_cold | int* 单值出参 | — | 可 NULL（不输出计数） |

### 10.2 输出

| 数组 | dtype/shape | 值域 | invalid |
|---|---|---|---|
| out（修复帧） | float32（f64 ABI 经 float→double 回转）`[h][w]` | ADU（可负；不 clamp；无 pedestal） | 非坏点逐像素恒等；坏点=插值或原值回退（空邻域）；全坏邻域回退原值；NaN 输入透传；参数错误时不写 out |
| out_hot / out_cold | int 单值 | 像素计数 | ALG-COS-002 结构过滤后掩码像素数（≤候选数）；dark/bias 未接线时=0；可 NULL |
| 错误码 | int | — | 0=AC_OK；−1=AC_ERR_PARAM（data/out 空指针、w/h 非正）；−2/−3 定义但**从未返回**（DISP-COS-001） |

### 10.3 mask 与 coverage 边界

- 掩码极性 **1=坏点**（SCI-CAL-001 §9a，hot/cold/all_bad 合并掩码均为
  0/1 char，`all_bad = hot | cold` 位或合并）；掩码为模块内部产物，
  不作为产品输出（与 §9.4 一致）。
- **no fabrication of valid coverage**：修复仅发生在被检测判坏的像素，
  非坏点逐像素恒等；空邻域回退原值而非虚构好值；生产调用现状
  （p1_session.cpp:294-307 dark/bias 传 NULL）下模块为恒等 pass、
  out_hot=out_cold=0——不做无依据的"已修复"声明（DISP-COS-009）。
- variance/ivar/coverage 不在本层（§4a/§9.4 同界）。


## 11. Phase1 drizzle 模块输入/输出数据（DATA-P1-DRZ）

> ID: DATA-P1-DRZ  状态: CONTRACT_READY（P1-DRZ-DOC 冻结，2026-09-07）
> 模块: lib/drizzle（astrocs.p1.drizzle，迁移目标；现行实现唯一生产源
> lib/healpix_db/healpix_drizzle/，经根 CMake 静态库 astrocs_drizzle
> 编译，CMakeLists.txt:356-366）；SCI: SCI-DRZ-001（含 014/015/016）；
> ALG: ALG-DRZ-001；上游: DATA-P1-CAL（§9）。本节冻结
> hp_drizzle_run / hp_drizzle_run_hips 帧通道真实数据语义（P1-DRZ-DOC
> 源码核对），迁移 DLL astrocs_p1_drizzle.dll 由 P1-DRZ-IMPL 建立
> （语义不变）。编排现状（registry descriptor
> lib/core/src/module_adapters.cpp:570-587 p1_drizzle_descriptor）将
> 本模块登记为 ports calibrated→stacked、data_id=DATA-P1-STACK；本节
> DATA-P1-DRZ 为按 SCI-DRZ-001 语义新建的模块级合同，descriptor 引用
> 由 P1-DRZ-INT 对齐。禁止声明 IMPLEMENTED。

### 11.1 输入（PipelineFrame 命名块；文件通道 FITS 另注）

| 块/参数 | dtype/shape | 单位/域 | invalid / NULL 语义 |
|---|---|---|---|
| "data" 块 | float32 或 float64（二选一）`[H][W]` 行主序（api.cpp:486-503） | ADU | 多通道 channels≠1 拒绝（BLOCKER）；值 NaN/Inf 静默跳过（等效掩膜，不进累加器，无计数暴露——DISP-DRZ-004，drizzle_engine.cpp:1712） |
| "header" KV: CD1_1..CD2_2 或 CDELT1/2+CRVAL1/2+CRPIX1/2 | double | 度/像素、度、像素（1-based） | 两者均缺 → 无 WCS，帧通道返回 -9（api.cpp:541-545）；CDELT+CROTA2 构造 CD（api.cpp:538） |
| "header" KV: SIP A/B/AP/BP 系数 | double[] | 无量纲 | gate A_ORDER 存在才载入（api.cpp:552-557）；reverse 通道 sip_order 校验 [0,5]（DISP-DRZ-001） |
| "header" KV: "PRECISION" | 字符串 "fp32"/"fp64" | — | precision_mode=-1 时读取；缺省 FP32；编排经 aio_frame_kv_set 写入（orchestrator.cpp:3313-3325） |
| "snr_model" 块（可选） | 稀疏控制点（ra/dec/snr_psf + snr_phot/median_snr/idw_power） | 度、度、无量纲 | 缺块/0 点 → 不写 SNR 子块；KD-tree IDW 重建逐像素 SNR（snr_evaluator.h） |
| nside | int，2 的幂 | — | 非法（≤0 或非 2 的幂）拒绝（api.cpp:398-402）；auto 模式钳位 [16,2^22]（compute_auto_nside） |
| nested | int 1/0 | — | 仅 1=NESTED；0=RING 硬拒绝（drizzle_engine.cpp:1575-1579） |
| pixfrac | double | drop 与源像素之比（无量纲） | 引擎层 (0,1] 严格拒绝 ≤0/>1（:1567-1574，不夹逼）；文件通道 API 层接受 0.0 的双轨见 DISP-DRZ-003 |
| variance 面（可选，帧内块） | float32，随 data 布局 | ADU² | 非有限或 ≤0 → 跳过该像素（:1727-1729）；无 variance 输入 → 不产 variance/ivar 产品 |
| weight / snr 面（可选，文件通道 FITS） | float32 `[H][W]` | 无量纲 | 读失败 rc=8/9；尺寸不匹配 rc=7/9；非有限/≤0 跳过 |

### 11.2 输出（HEALPix NESTED tile 产品 + 统计）

| 数组/文件 | dtype/shape | 单位/值域 | invalid |
|---|---|---|---|
| sumFlux（tile 累加量，HiPS SIGNAL 底数） | float32 或 float64（随 precision_mode）`[512][512]`/tile | ADU·w 加权和（原始和，未归一） | S_p=F_p/D_p 归一不在 drizzle 层——由 aio_hips_writer finalize_tile 完成（aio_hips_writer.cpp:566-631；DISP-DRZ-007） |
| sumArea（HiPS SUPPORT 底数） | 同上 `[512][512]`/tile | sr（Σa_jp；support=Σarea/A_p 归一在 sink/writer） | covered_area≤0 → variance/ivar 记 NaN（合法，aio_hips_writer.cpp:615-622） |
| sumVarNum → variance/ivar 产品 | 同上 | ADU²；ivar=1/variance | 仅当 varianceValue>0 累加（drizzle_engine.cpp:1531-1534）；无 variance 输入不产 variance 产品（AIO_HIPS_PRODUCT_ALL 非 V19） |
| nContrib（tile 内 leaf 计数） | int `[512][512]`/tile | 贡献源像素数 | 0 = touched 集合外（不写） |
| SNR 控制点子块（有 snr_model 时） | local_ipix + snr | 像素序、无量纲 | 逐 tile 内嵌 HissSnrBlock |
| HiPS 产品集 | hips_dir 目录树（Norder/Shard 目录 + properties） | — | 直写硬门 tile_depth=9、nside≥512（astro_sphere_sink.cpp:36-51）；overwrite 清理由编排层（orchestrator.cpp:3345-3354） |
| legacy .hiss（仅 legacy_hiss_compare=true） | HissWriter 文件 | — | 非正式产品；HISS_VERIFY 验证通道（orchestrator.cpp:3397 起） |
| operation_counts.json | JSON 剖面文件 | — | 与 .hiss 同目录（api.cpp:1074-1117） |
| HpDrizzleResult 统计 | int64/int/double + error_msg[512] | — | n_healpix_pixels/n_source_pixels/nside/nested/pixfrac/elapsed_sec；错误时 error_msg 非空 |

### 11.3 坐标、面亮度语义与边界

- 坐标: 源平面像素 (x,y) →（WCS/SIP TAN）→ (ra,dec) 度 → NESTED
  ipix（仅 NESTED；parent=ipix>>2d 位分解）；RA 域 [0,360)、dec
  [-90,90] 越界拒绝（drizzle_engine.cpp:1169-1171，SNR 面）。
- 面亮度语义（SCI-DRZ-001 §5）: drizzle 层只产原始累加量
  （sumFlux/sumArea/sumVarNum）；S=F/D、variance=sumVarNum/D²、
  ivar=1/variance 的归一与 NaN 语义在 astro_image_io finalize 层
  （variance = var_num_sum/covered_area²，covered_area≤0 → NaN）。
- 确定性: 同输入同线程数 bitwise 可复现（schedule(static) + 按线程序
  合并，drizzle_engine.cpp:1670-1671,1762-1785；1/N 合同见
  ALG-DRZ-001 §6）。
- 取消: 模块内无取消机制（长 run 不可中断；编排取消点=帧/tile 粒度
  为编排层合同，PHASE1_API_V1 头部）。

## 12. Phase1 HiPS writer 模块输入/输出数据（DATA-P1-HIPS）

> ID: DATA-P1-HIPS  状态: CONTRACT_READY（P1-HIPS-DOC 冻结，2026-09-07）
> 模块: lib/hips（astrocs.p1.hips_writer，迁移目标；现行实现唯一生产源
> lib/astro_image_io/src/hips/aio_hips_writer.cpp，合同头
> lib/astro_image_io/include/aio_hips.h，经根 CMake 静态库 astrocs_hips
> 编译，CMakeLists.txt:298-309）；迁移 DLL astrocs_p1_hips_writer.dll 由
> P1-HIPS-IMPL 建立（语义不变，禁止声明 IMPLEMENTED）。ALG:
> ALG-HIPS-001..005；上游: DATA-P1-DRZ（§11，AstroSphereTileView 直供）；
> SCI: SCI-DRZ-001（:130/:145 共享锚）；发布合同: IO-003（§12.5 对齐
> 边界）。本节是 HiPS 产品集（signal/support/variance/ivar Image HiPS +
> SNR Catalogue HiPS）单位/dtype/shape/invalid 的唯一权威；§4/§4a 的
> 产品位语义在此落地为逐文件语义。

### 12.1 输入（aio_hips_product_begin 参数 + 逐 tile AstroSphereTileView + SNR 点）

| 参数/字段 | dtype/shape | 单位/域 | invalid / NULL 语义 |
|---|---|---|---|
| out_dir | 字符串路径 | — | 目录不存在逐级创建（make_dirs）；同 out_dir 重跑覆盖直写（无清理责任，见 §12.5） |
| nside | int，2 的幂 | — | ≥512 强制（<512 返回 NULL，aio_hips_writer.cpp:399-401）；叶阶 leaf_order=ilog2(nside) |
| tile_width | int | — | 恒 512（≠512 拒绝 :402；tile_order=leaf_order−9） |
| data_type | 0=float32 / 1=float64 | — | 其他值拒绝（:403）；决定存储 bitpix −32/−64 与 hierarchy 累加器轨（f32 产品 float 累加，DISP-HIPS-009） |
| flags | 位或 SIGNAL=1/SUPPORT=2/SNR=4/VARIANCE=8/IVAR=16（ALL=7/ALL_V19=31） | — | 越位拒绝（:404）；variance/ivar 请求须配 var_num_sum 输入 |
| creator_did / obs_title / obs_filter / exposure_s / obs_date | 字符串/字符串/double(s)/字符串 | —（ADU 无关） | filter/obs_date 可 NULL（不写对应 properties 键）；exposure≤0 不写 obs_exptime/t_min/t_max；缺省 did=ivo://astrocs/phase1、title="AstroCS Phase1"（:414-415） |
| moc_order | uint | — | 0=auto（=tile_order）；>0 取 min(moc_order, tile_order) 静默钳位（:419，DISP-HIPS-005） |
| AstroSphereTileView: parent_ipix | uint64 | NESTED ipix（Norder K） | ≥12·4^K 拒绝 rc=−3（:433-437）；width/leaf_order/dtype 不匹配拒绝 rc=−2（:428-431） |
| AstroSphereTileView: flux_sum | float32 或 float64 `[512×512]` NESTED local 行主序 | ADU（drizzle 层 ADU·w 加权和，§11.2） | 非 NULL 强制；无效像素处理见 §12.4 |
| AstroSphereTileView: covered_area | 同上 | sr | 归一分母；≤0/非有限 → signal=NaN、support=0 |
| AstroSphereTileView: valid_mask | uint8 `[512×512]` | — | 可 NULL（=全有效，:466/:606） |
| AstroSphereTileView: var_num_sum | 同 flux_sum dtype `[512×512]` | ADU²（Σ v_j·w_jp²，drizzle 侧分子） | variance/ivar 产品时强制非 NULL（缺失 rc=−2 :575-576）；≤0/非有限 → 该像素 variance/ivar=NaN |
| SNR 点（aio_hips_write_snr_points 累计缓存） | AioHipsSnrPoint: star_id int64 / ra,dec double / snr double / quality_flags uint（位 1=PSF_OK,2=saturated,4=has_saturated,8=photo_matched,16=photo_rejected）/ photometric_status uint（0=unmatched,1=used,2=rejected） | 度、度、无量纲 | SNR 产品关闭时忽略；无点 → 不写 snr 目录（:888） |

### 12.2 输出（`<out_dir>` HiPS 产品集目录树）

| 文件/数组 | dtype/shape | 单位/值域 | invalid |
|---|---|---|---|
| signal/NorderK/DirD/NpixN.fits | float32 或 float64 `[512×512]` NESTED local（bitpix −32/−64 随 data_type） | ADU（面亮度 signal=flux_sum/covered_area） | 无效像素 IEEE NaN 填充（无 FITS BLANK 整型卡；:483-485） |
| support/…fits | 同上 | 无量纲 [0,1]（covered_area/A_cell，>1 钳 1.0；A_cell=4π/(12·nside²)） | 无效像素 0.0 |
| variance/…fits | 同上 | ADU²（var_num_sum/covered_area²） | 无效或 area·vnum≤0/非有限 → NaN（:615-621）；tile 全无效 → 写请求 rc=−5 不落盘（:629-632） |
| ivar/…fits | 同上 | 1/ADU²（=1/variance，有限域互为倒数） | 同 variance（NaN 对应） |
| hierarchy 低阶 tiles（signal/support/variance/ivar 同目录树，nside=2^(k+9), k<tile_order） | 同上 `[512×512]` | 同上（父 cell=子像素聚合；f32 产品 float 累加 DISP-HIPS-009） | 空 acc 父 cell 照写全 NaN（DISP-HIPS-011） |
| Moc.fits（每子产品） | BINTABLE 列 UNIQ（int64 域 4·4^m+(c>>2(K−m))) | 无量纲（UNIQ 编码） | 空集不写（:246）；moc_order<K 低阶 UNIQ 对自家 reader 无效（DISP-HIPS-005） |
| properties（每子产品） | 文本 key=value 逐行 | — | 直写无原子性/无转义（DISP-HIPS-010）；时间键=真实 UTC（字节不跨运行复现） |
| snr/NorderK/DirD/NpixN.tsv | 文本列 star_id(ra int64)/ra/dec(%.12f deg)/snr(FP32 %.9g、FP64 %.17g)/quality_flags/photometric_status（SNR-PREC-001 round-trip 精度） | 度、度、无量纲 | 头注释行起；无点不写目录 |
| snr/metadata.xml + snr/properties + snr/Moc.fits | VOTable 1.3 / 文本 / BINTABLE | — | hips_cat_nrows=点数；hips_initial_ra/dec=源位置中位数（真实值，:960-973） |
| metadata.fits（每 Image 子产品） | FITS 表头卡 PIXTYPE/ORDERING=NESTED/NSIDE/HIPSTILEWIDTH/DATAPRODTYPE | — | remove+create 直写（:770） |
| manifest.json（根级） | JSON: format_version/hips_version="1.4"/nside/tile_width/data_type/products/n_leaf_tiles/moc_sky_fraction/astrocs_covered_sky_fraction/signal_dtype | — | 无 COMPLETE 状态字/无哈希清单（§12.5） |

### 12.3 坐标、索引与面亮度/方差语义

- 全局像素 = Norder K 的 NESTED ipix（parent_ipix）+ tile 内局部 NESTED
  索引（512×512 展平）；FITS 行主序落盘索引 =
  nested_local_to_fits_index(i,9,512)（三处 scatter :464/:604/:809），
  映射式=§3 冻结的 (511−x)·512+y（CDS Hipsgen 冻结，共享权威
  lib/common/healpix/healpix_core.cpp:287-296）。层级 tile 同式。
- 目录布局：`Norder{K}/Dir{ipix/10000}/Npix{ipix%10000}.fits`（万进制
  分片，tile_rel_path :135-142）；hierarchy 逐阶同布局（k<K）。
- 面亮度链（与 §4/§4a、SCI-DRZ-001 :145 一致）：drizzle 层产原始累加量
  （§11.2）→ writer 归一 signal=flux_sum/covered_area、
  support=min(covered_area/A_cell,1)、variance=var_num_sum/covered_area²、
  ivar=1/variance；F=signal×support×A_cell 闭合（gate7 复检同式）。
- 天区度量：moc_area_sr=Σ A_cell(K)（逐叶 tile 登记制，
  aio_hips_writer.cpp:530）；hips/moc_sky_fraction=moc_area_sr/4π；
  astrocs_covered_sky_fraction=covered_area_sr/4π；hips_pixel_scale=
  3600·180/π·√(π/3)/nside arcsec（:704-705）。

### 12.4 invalid 与精度规则（汇总）

- 无效判定域（signal/support）：`valid && area>0 && isfinite(flux) &&
  isfinite(area)`（:476,:481-485）；variance/ivar 额外要求 `vnum>0 &&
  isfinite(vnum)`（:617-621）。违反 → signal=NaN、support=0.0、
  variance/ivar=NaN。IEEE NaN 填充，不使用 FITS BLANK。
- 全无效 variance tile：写请求返回 −5、不落盘（调用方预判跳过，
  astro_sphere_sink.cpp:123-144 计数不 abort）——显式失败而非空产品。
- 存储 dtype 双轨 f32/f64 由 data_type 一次固化；归一运算在 double 域
  （:469-477）后截断到存储 dtype；hierarchy 累加器 f32 产品为 float
  （精度口径 TEST-HIPS-DESIGN-001：f32 路径 rtol=1e-6，f64 bitwise）。
- SNR TSV 数值 round-trip 无损（FP32 %.9g / FP64 %.17g，SNR-PREC-001）；
  ra/dec 列 %.12f deg（亚角秒级足够，源为 deg 值）。
- FITS 完整性：逐 tile DATASUM/CHECKSUM（fits_write_chksum :230，MOC
  :275）；头卡 NSIDE/FIRSTPIX="0"/LASTPIX="262143"（声明性，DISP-HIPS-012）。

### 12.5 发布/IO-003 对齐边界（原子性与 tree hash 归属）

- C++ writer（本节 12.1-12.4 产出）**不提供**原子发布：文件先
  `std::remove` 后 create 直写（:185-186/:251/:770）、properties/manifest
  直写、abort 仅释放句柄不删除文件（:1133-1137，DISP-HIPS-001）、
  finalize 中途失败（−3..−8）已写子产品残留——writer 层合同如实登记为
  "非原子、覆盖式、无回滚"。
- 原子发布语义由 IO-003（docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md；
  runtime/io/hips_output_store.py）在 Python 发布层承接：临时目录写 →
  fsync → fitsverify → 逐文件 sha256 清单 → 原子 rename → manifest 置
  COMPLETE。**整树哈希（tree hash）唯一权威在 IO-003 发布层**；writer 层
  完整性证据=逐 tile FITS DATASUM/CHECKSUM + manifest 计数字段
  （n_leaf_tiles 等）。
- 对齐规则：调用方（P1-HIPS-INT 接线）必须让 writer 输出到发布层 staging
  区、经 IO-003 门禁后对外可见；不得以 writer 直写目录冒认 IO-003 原子
  语义；同 out_dir 重跑的旧残留清理由发布层 staging 隔离解决（writer 自身
  不清理）。对照：HISS 容器有 .partial/atomic_replace（独立通道，
  hiss_stream_writer.cpp:259-260,:644-655），与本边界无关。

## 13. Phase1 noise 模块输入/输出数据（DATA-P1-NOISE）

> ID: DATA-P1-NOISE  状态: CONTRACT_READY（P1-NOISE-DOC 冻结，2026-09-07）
> 模块: lib/snr_estimator;lib/phase1/noise（astrocs.p1.noise-snr，迁移目标
> astrocs_p1_noise.dll；现行实现唯一生产源 lib/snr_estimator/cpp/src/
> noise_model.cpp，合同头 lib/snr_estimator/cpp/include/snr_estimator.h）。
> ALG: ALG-NOISE-001..003（NOISE_ESTIMATION §13.1 逐符号锚）；SCI:
> SCI-NOISE-001..015（NOISE_MODEL.md，FROZEN，共享引用不改动）；编排级
> 合同 API-P1-006（PHASE1_API_V1 §2）。本节是该模块单位/dtype/shape/
> invalid 的唯一权威；§4a 产品语义（ivar=1/variance、ivar=0 显式不可用）
> 在此落地为模块级 I/O 语义。

### 13.1 输入（snr_noise_model_v1 / _f64 参数 + SnrNoiseModelConfig）

| 参数/字段 | dtype/shape | 单位/域 | invalid / NULL 语义 |
|---|---|---|---|
| data | float32（v1）/ float64（f64）`[h·w]` 行主序 0-based | ADU（或 e⁻，同输入标度） | 非 NULL 强制；h>0/w>0 否则 rc=3（noise_model.cpp:118）；NaN/Inf 与饱和像素过滤不统计（valid_pixel :64-68） |
| source_mask | float32 `[h·w]` | —（≠0 即源掩膜 1） | 可 NULL（改用 star 坐标通道）；非 NULL 时忽略 star_x/y（互斥，DISP-NOISE-006）；掩膜像素不参与统计 |
| star_x / star_y | double `[n_stars]` | pixel（0-based） | 可 NULL/n_stars=0；掩膜半径 rmax=max(1,r0)·max(1,scale)（默认 10·6=60 px）统一不按亮度缩放；非有限坐标跳过该星（:148-151） |
| cfg（SnrNoiseModelConfig） | 结构体按 snr_estimator.h:98-112 | — | 可 NULL（=default_config）；patch_grid_x/y≥2、cosmic_clip_sigma≥1、min_patch_samples≥1（默认 64）、max_clip_rounds≥0 静默钳位（DISP-NOISE-007）；gain_e_per_adu/read_noise_e/use_gain_model 三字段现状零读取（DISP-NOISE-003） |
| variance_floor | double | ADU² | 默认 1e-12；build 阶段 ≤0 不 clamp（原值直通），fill 阶段 ≤0 回退 1e-12（DISP-NOISE-002） |

### 13.2 输出（NoiseWeightModelV1 + fill 逐像素场）

| 字段/数组 | dtype/shape | 单位/值域 | invalid |
|---|---|---|---|
| NoiseWeightModelV1（snr_estimator.h:117-134） | ctrl_* double `[n_control_points]`（patch 中心 0-based 像素坐标/σ/variance/ivar） | σ: ADU；variance: ADU²；ivar: ADU⁻² | 合格 patch 的 max(patch_var, floor) 与 1/var；n_qualified_patches+n_rejected_patches==64（8×8） |
| sigma_bg_global / variance_bg_global / ivar_bg_global | double 标量 | ADU / ADU² / ADU⁻² | 全局兜底=合格 patch variance 稳健中位数（非退化）；完全退化 rc=1 时 ivar_bg_global==0.0（显式不可用，禁止伪装——§4a） |
| source / has_spatial_field / degenerate | uint8 标志 | — | source=0（empirical blank-sky 唯一生产基线）；has_spatial_field=1 须 enable_spatial_field==1 且 n_control_points>=4；degenerate=1=无合格 patch 且全帧兜底退化 |
| fill 输出 out_variance / out_ivar | float32 `[h·w]` 行主序 | ADU² / ADU⁻² | 任一可 NULL（可空输出，双 NULL 拒绝 rc=3 :426）；平面预测 max(a+b·x+c·y, floor)，负预测 clamp 至 floor；无合格 patch 时 ivar=0 拒绝加权（SCI §7） |

### 13.3 坐标与精度规则（汇总）

- 像素域 0-based 坐标（GLOSSARY `pixel_coordinate`），无 WCS/天球参与；
  patch 中心 ctrl_x_px/ctrl_y_px 为 patch 几何中心（(x0+x1)/2）。
- FP64 全链路统计（f32 输入升 double 计）；fill 输出 float32 截断
  （HISS SNR 子块冻结格式，诊断值非科学累加值）。
- 量纲不变量：ivar=1/max(variance,floor) 精确互倒（SCI §7）；gain 模型
  var_ADU=max(signal,0)/gain+(rn/gain)² 仅诊断（SNR-005，不入生产——
  §4a/SCI §10 域外引用）。

## 14. Phase1 photometry 模块输入/输出数据（DATA-P1-PHOT）

> ID: DATA-P1-PHOT  状态: CONTRACT_READY（P1-PHOT-DOC 冻结，2026-09-07）
> 模块: lib/photometric_calib;lib/phase1/photometry（astrocs.p1.photometry，
> 迁移目标 astrocs_p1_photometry.dll；现行实现唯一生产源
> lib/photometric_calib/cpp/src/pc_api.cpp，合同头
> lib/photometric_calib/cpp/include/photometric_calib.h）。
> ALG: ALG-PHOT-001..002（PHOTOMETRIC_FIT §13.1 逐符号锚）；SCI:
> SCI-PHOT-001（docs/science/PHOTOMETRY.md，FROZEN T103 2026-08-23，共享
> 引用不改动）；编排级合同 API-P1-005（PHASE1_API_V1）。本节是该模块
> 单位/dtype/shape/invalid 的唯一权威；descriptor 端口编目（psf→
> DATA-P1-PSF/sources→DATA-P1-SOURCES/fluxes→DATA-P1-FLUX，
> module_adapters.cpp:531-548）为编排层词汇，由 P1-PHOT-INT 对齐，
> 不得反向作为冻结依据。


### 14.1 输入（生产通道 pc_calibrate_simple_with_gaia_v2 / _f64_v2）

| 参数 | dtype/shape | 单位/域 | invalid / NULL 语义 |
|---|---|---|---|
| pixels | float32(v2)/float64(f64_v2) `[h·w]` 行主序 | ADU | h·w=0/NULL → rc=−1；NaN 像素参与校正（乘法透传），匹配仅用 PSF/Gaia 表 |
| width/height | int32 标量 | px | ≤0 → rc=−1 |
| psf 块（orchestrator :2560 起拆列） | psf_cx/psf_cy double `[n_psf]` px；psf_flux double `[n_psf]` ADU；psf_status int32 `[n_psf]` 0=ok | pixel/ADU | n_psf=0 → 退化 scale=1.0 rc=0；status≠0 行仅记 records status=3/reject=6，不入匹配 |
| psf_star_ids | int64 `[n_psf]` | — | 原样回传 records.star_id（lineage） |
| gaia_client_handle | opaque 句柄 | — | NULL → rc=−2；DLL 内锥形搜索（ra/dec/radius），失败 rc=−3 |
| mag_max | double | mag | 声明但被自适应 mag_max_arr{12..16} 覆盖（DISP-PHOT-005） |
| match_radius_px | double | px | 合同值 2.0（DISP-PHOT-002：旧文档 3px 失实） |
| mag_tolerance | double | mag | 合同值 3.0 |
| filter_wl/trans、qe_wl/trans | double `[count]` | nm / [0,1] | 递增 wl；prepare_filter_cache 失败 → 退化 rc=0（:911-923） |
| spectrum_wl | double `[343]`（336..1020nm step 2nm） | nm | XPSD 固定网格；uint8 谱 F(λ)=byte·flux_mul+flux_min（spectrum_integrator.cpp:62-64） |
| WCS/SIP | crval2/crpix2/CD4 元 double；sip_order int32(≤2)；a/b/ap/bp double `[36]`（i*6+j） | deg/px | sip_order=0 → 纯 TAN；无 AP/BP 前向 SIP 牛顿迭代一次（wcs_transform.cpp:219） |


### 14.2 输出

| 输出 | dtype/shape | 单位/域 | 语义 |
|---|---|---|---|
| out_pixels | 同输入 dtype `[h·w]` | ADU | I_cal=I·scale（f32 通道 ImageCorrector :63-77；f64 内联 pc_api.cpp:1023-1028）；退化=恒等拷贝 |
| out_scale_factor | double 标量 | 无量纲 | 10^(−location)（IRLS/Tukey，star_matcher.cpp:527-529）；退化/一致集空=1.0 |
| out_sigma_residual | double 标量 | dex（log10 flux-ratio） | MAD(r_inliers)/0.6745（:551-560）；下游换算 sigma_mag/sigma_cal_rel 由 snr_phot_cal_quality 承担（API-NOISE-001 边界） |
| out_n_matched | int32 标量 | 颗 | IRLS inliers 数（fit_used） |
| out_diag | PhotometricDiag（头 :21-45） | 计数/dex/px | 17 字段分阶段诊断；rejected_quality 为混合计数（DISP-PHOT-006） |
| out_records | PcMatchRecord `[n_psf]`（头 :47-59） | ADU/dex | status 0=unmatched/1=matched+used/2=matched+rejected/3=psf-invalid；reject_reason 0..6；residual=r（未匹配 NaN）；dr3sp_id=位置量化哈希（pc_api.cpp:743-752，XPSD 无 source_id） |


### 14.3 坐标与精度规则（汇总）

- 帧内像素坐标 0-based double（PSF cx/cy）；Gaia 投影 WCS TAN+SIP
  （CRPIX 1-based 内部换算，wcs_transform.cpp:241-242）；match_distance_*
  单位 px；Gaia 输入 ra/dec ICRS deg。
- FP64 全链路（f32 输入升 double）；out_pixels f32 通道经 float 截断
  （诊断级截断在 ImageCorrector，f64 通道全 double）。
- r 方向恒为 log10(F_instr/F_syn)（F_instr=PSF flux ADU，F_syn=W·m⁻²·nm⁻¹
  积分值）——scale 为无量纲乘性因子，量纲比进入 log 前由合同锚定，禁止
  反向（SCI-PHOT-001 §10）。
- determinism=fixed_reduction_order：F_syn 逐星独立（OpenMP dynamic,64）、
  像素逐元素独立（static）→ 输出 bitwise 与线程数无关（README §7）。

## 15. Phase1 star-psf 模块输入/输出数据（DATA-P1-PSF）

> ID: DATA-P1-PSF  状态: CONTRACT_READY（P1-PSF-DOC 冻结，2026-09-07）
> 模块: lib/dynamic_psf（astrocs.p1.psf，迁移目标 astrocs_p1_psf.dll；现行实现
> 唯一生产源 lib/dynamic_psf/src/dpsf_psf.cpp，合同头
> lib/dynamic_psf/include/dynamic_psf.h）。ALG: ALG-STARPSF-001
> （STAR_PSF_ALGORITHMS §11 逐符号锚）；SCI: SCI-P1-PSF-001（本任务冻结层，
> SCI-PSF-001 docs/science/PSF.md 共享引用不改动）；编排级合同 API-P1-003
> （PHASE1_API_V1 §2）。本节是该模块单位/dtype/shape/invalid 的唯一权威；
> descriptor 端口编目（psf→DATA-P1-PSF/sources→DATA-P1-SOURCES，
> module_adapters.cpp:499-503）与本节一致。**双 [N,9] 布局并存**（15.2 A/B）
> 为冻结事实，禁止混淆。

### 15.1 输入（dpsf_fit_batch_f / _d 生产通道，orchestrator.cpp:2314/:2339）

| 参数/字段 | dtype/shape | 单位/域 | invalid / NULL 语义 |
|---|---|---|---|
| image（cleaned 块） | float32（f 通道）/ float64（d 通道）`[h·w]` 行主序 | ADU | NULL/h≤0/w≤0 → rc=−1；FP64 通道不降级（PREC-105 同族约束） |
| cx_arr / cy_arr（来自 sources 块 star_det v1 [N,6] 列 [0]/[1]，dynamic_psf.h:79-107） | double `[n_stars]` | pixel（0-based） | NULL/count≤0 → −1；越界星 fitRadius 裁窗 clamp（dpsf_psf.cpp:533-536）；空 rect → 该星 INVALID_PARAMS |
| params（DPSFFitParams） | 结构体（dynamic_psf.h:38-42） | px（fitRadius） | NULL → 默认 fitRadius=8/maxIter=200/tolerance=1e-8（:717-719/:845-847）；maxIter/tolerance 模块侧死参数（DISP-PSF-003，编排配置 psf.max_iterations/tolerance 不生效，orchestrator.cpp:2278-2286） |

饱和列 star_det v1 [4]=saturated/[5]=has_saturated 现状不消费（dpsf_psf.cpp:741
仅解包 [0]/[1]）；饱和剔除决策归 star_detector 侧，P1-PSF-TEST 专项覆盖。

### 15.2 输出

**布局 A：编排层 `psf` 块（必需块，PHOTOMETRIC 消费，缺失退出码 3，
orchestrator.cpp:2563-2570）**——DPSFFitResult 序列化（:2376-2387）：

| 列 | dtype/单位 | 语义 / invalid |
|---|---|---|
| [0]=status | double（整值 0..3） | 拟合状态码（STAR_PSF_ALGORITHMS §11.2 四码语义）；PHOTOMETRIC 仅 status=0 行入匹配（pc 匹配侧 status≠0 记 reject） |
| [1]=B | double ADU | 截尾中位背景（拟合收敛值） |
| [2]=flux | double ADU | Moffat4 解析积分 2π·A·sx·sy/3（dpsf_psf.cpp:374-375） |
| [3]=cx / [4]=cy | double pixel（0-based 全图坐标） | 中心（局部坐标已回移 :383/:755-757）；非 OK 行=memset 0（单星语义，:229） |
| [5]=fwhm | double pixel | (fwhm_x+fwhm_y)/2 平均（:2385） |
| [6]=A | double ADU | 振幅 |
| [7]=mad | double ADU | 10–90% 截尾均值 \|残差\|（residual_scale，:190-220） |
| [8]=eccentricity | double [0,1) | √(1−(smin/smax)²)（:378） |

块 schema 注记 "PSF 拟合结果: status,B,flux,cx,cy,fwhm,A,mad,eccentricity"
（:2389-2390）；dims=[N,9] FLOAT64（:2389）。

**布局 B：批 API `out_psf_params`（dpsf_fit_batch_f32/f64，dynamic_psf.h:84-108）**：

| 列 | dtype/单位 | 语义 / invalid |
|---|---|---|
| [0]=B / [1]=A | double ADU | 背景/振幅；**仅成功星行**（B2-A2 compact，见下） |
| [2]=cx / [3]=cy | double pixel | 全图坐标（局部回移） |
| [4]=sx / [5]=sy | double pixel | σ 分量 |
| [6]=theta | double 弧度 | 旋转角（x 轴起边，4 候选消歧后） |
| [7]=fwhm_x / [8]=fwhm_y | double pixel | 1.230310·sx/sy |

`out_n_valid` 仅计 DPSF_FIT_OK（:804/:925）。**行语义（B2-A2 修正，
RESCUE-P0-05）**：成功行按检测下标升序 **compact** 写入 `out_psf_params`
第 0..n_valid−1 行；失败星不占据参数行（不再留下 NaN 洞）。逐星真值由可选
`out_status`（`psf_status:INT32[N]`，`DPSF_PSF_STATUS_{OK,FIT_FAILED,RECT_EMPTY,ALLOC_FAILED}`
=0/1/2/3）按检测下标报告；调用方**必须**用 `out_status` 做星 ID↔参数行映射，
**禁止按 `i < out_n_valid` 截断前缀**（否则 NaN/错位行贴真实 star_id、后续有效星被丢弃）。
两布局列序不同（A=状态/派生量序，B=参数序），跨层传递禁止直接复用同一缓冲。

**附属产出：`star_measurements` 权威块 FLOAT64 [N,15]（schema
astrocs-star-measurements-1，orchestrator.cpp:2403-2411 注释）**：列
[0]=star_id(int64 as double)，[1]=x，[2]=y，[3]=flux_inst（PSF flux），[4]=
flux_uncertainty（PSF mad proxy），[5]=background（PSF B），[6]=psf_status，
[7]=fwhm，[8]=A，[9]=B，[10]=mad，[11]=eccentricity，[12]=mag（detector），
[13]=saturated，[14]=has_saturated。PSF 不输出独立 background_rms，SNR 使用
A/B/mad（与冻结 SNR 定义一致）；下游必须经 star_id 连接，禁止数组行号隐式连接
（:2409-2411）。该块列语义由 P1-STAR-INT 承接（matrix depends_on_int=P1-STAR-INT）。

### 15.3 坐标与精度规则（汇总）

- 像素坐标一律 0-based double 全图系（cx/cy）；θ 弧度 x 轴起边；FWHM/σ 像素；
  B/A/flux/mad 为 ADU（flux 为解析积分，非计数）；eccentricity 无量纲。
- 确定性：逐星独立拟合，OpenMP dynamic 调度仅影响线程分配；成功行在
  **串行的按检测下标升序 compact 段**写入（B2-A2），bitwise 与线程数无关
  （fixed_reduction_order，README §7）。
- FP64 通道（dpsf_fit_batch_d/f64）不降级；FP32 通道不经 uint16 有损转换
  （PREC-105）。
- determinism/dtype 变更属科学改动，须走 owner 流程；本节禁止被编排层词汇反向
  改写（descriptor 占位 ALG-002/TEST-P1-PSF-001 由 P1-PSF-INT 对齐）。

## 16. Phase1 装配会话输入/输出数据（DATA-P1-SESSION）

> ID: DATA-P1-SESSION  状态: CONTRACT_READY（P1-SESSION-DOC 冻结，2026-09-07）
> 模块: lib/phase1_session（MOD-astrocs-phase1-session，module_id=
> astrocs.phase1.session；assembly 层，迁移矩阵无 P1-SESSION 行，不设独立
> DLL，现状=静态库 astrocs_phase1_session，根 CMakeLists.txt:448-452）。
> ALG: 不新设装配算法——本节只登记 config/manifest/artifact 的
> schema/单位/dtype/shape，科学语义引用既有 DATA-P1-CAL（§9）、
> DATA-P1-COS（§10）等冻结节。编排级合同 API-P1-001
> （docs/api/PHASE1_API_V1.md，FROZEN）；入口符号合同 API-P1-SESSION
> （PUBLIC_API.md）。本节是该会话数据面的唯一权威，禁止被编排层占位
> 词汇反向改写。

### 16.1 config JSON（p1_session_validate 键集，p1_session.cpp:115-143）

| 键 | 必/可 | dtype | 默认 | 消费段 / 错误 |
|---|---|---|---|---|
| input_lights | 必 | UTF-8 string 非空数组 | 无 | io_read；缺失/非数组/空/元素非串 → ACS_ERR_PARAM（:116-127） |
| output_dir | 必 | string | 无 | calibrate/io_write；缺失/非串 → PARAM（:116-119） |
| master_bias / master_dark / master_flat | 可 | string 或 null | null（:127-131） | io_read；类型错 → PARAM |
| cosmetic | 可 | object（值 numeric/bool） | `{}` | cosmetic；非 object/值类型错 → PARAM（:132-140） |
| cosmetic.enabled | 可 | bool | true（run 开关 :284 `value("enabled", true)`） | cosmetic 段跳过开关 |
| dark_optimization | 可 | bool | false | calibrate（:141-143） |
| dark_scale_factor | 可（validate 不验） | float | 1.0（run :225 `value()` 兜底） | calibrate dark 缩放因子 |

config 在 run 内二次解析（validate 先行的合同，:155-159 parse 失败→PARAM）。

### 16.2 host services 数据面（include/astrocs/common_abi_v1.h:110-117）

- `allocator`：handle 内部与 inspect 输出缓冲分配（inspect 缓冲
  p1_session.cpp:349-357 host alloc，调用方经 host free 释放）。
- `cancel`（:92-97 单向置位）：检查点=io_read 文件粒度（:177-181）、
  calibrate 帧粒度（:228-231）、cosmetic 帧粒度（:289）。
- `budget`（:100-108）：`max_workers` → `ac_set_num_threads` 注入
  （:162-165）；`available_cpus` 上限快照。

### 16.3 manifest JSON（p1_session_inspect 输出，dump(2) :348）

| 字段 | dtype | 语义 |
|---|---|---|
| kind | string，恒 "astrocs_phase1_session" | 会话标识（:98 初始化） |
| stages | array[object] | 逐段条目（:170/:199/:285/:321 push） |
| stages[].name | string | io_read / calibrate / cosmetic / io_write（canonical 4 段） |
| stages[].status | string | running → ok / fail / cancelled |
| stages[].files | uint64 | io_read 读取文件数（:191） |
| stages[].frames / .per_frame | uint64 / array | calibrate 成功帧数与逐帧记录（:277-279） |
| artifacts | array[string] | 已落盘校准帧路径（:274-275 push） |
| frames | uint64 | 总成功帧数（:333） |
| status | string | created（未 run 无错 :342-343）→ complete（:334）/ failed（:345） |
| error / error_kind | string | 脱敏错误摘要（:346）与错误类别（"input" :185/:204-212 等） |

### 16.4 artifact（校准输出帧，:256-269）

| 项 | 值 |
|---|---|
| 路径 | `<output_dir>/calibrated_<basename>`（:258-261） |
| dtype | float32 像素平面（复用 aio_read_fits 读结构覆写像素 :256-258 后 aio_write_fits :262；像素 dtype 随输入帧 float32 通道） |
| shape | [h, w]（与输入光帧及母版一致；不匹配→PARAM :218-221/:236-239） |
| 单位 | ADU（与 DATA-P1-CAL 像素语义一致，§9） |
| 格式 | 仅 FITS（aio_write_fits）；XISF 仅读侧自动探测（aio_read :71-74） |
| invalid | 像素域 invalid 语义承 DATA-P1-CAL（§9），本层不新增定义 |
| 释放 | AIOImageData 必经 canonical deleter aio_free_image_data（:56-61，IO-002） |

### 16.5 边界

- 本节不定义校准/cosmetic 公式（ALG-CAL-001..006 / ALG-COS-001..005
  权威）与任何其他阶段算法；不描述 Phase2/3 数据面。
- API-P1-001 冻结 7-stage 序列与现状 4 段（CAL+COS）的差距在
  lib/phase1_session/README.md §3 如实声明；补齐归 P1-SESSION-IMPL。
- assembly 层禁止声明 IMPLEMENTED 于无证据处；descriptor 端口编目
  （module_adapters.cpp:316-606）与本节冲突时以本节+各冻结 DATA 节为准。

## 17. Phase1 star-detection 模块输入/输出数据（DATA-P1-STAR）

> ID: DATA-P1-STAR  状态: CONTRACT_READY（P1-STAR-DOC 冻结，2026-09-07）
> 模块: lib/star_detector;lib/phase1/stars（astrocs.p1.star_detection，迁移目标
> astrocs_p1_star_detection.dll；现行实现唯一生产源
> lib/star_detector/src/sdet_api.cpp:1599-2353 生产核心 sdet_detect_impl，
> 合同头 lib/star_detector/include/star_detector.h:1-73）。ALG:
> ALG-STARDET-001（STAR_DETECTION_ALGORITHMS §11 逐符号锚）；SCI:
> SCI-P1-STAR-001（docs/science/STAR_DETECTION.md，本任务冻结层，共享 SCI
> 引用不改动）；编排级合同 API-P1-003（PHASE1_API_V1 §2：一帧只做一次权威
> 检测，PLATESOLVE 禁重检测）。本节是该模块单位/dtype/shape/invalid 的唯一
> 权威；descriptor astrocs.phase1.star-psf（module_adapters.cpp:492-510）为
> 编排层词汇，由 P1-PSF-INT 对齐，不得反向作为冻结依据。

### 17.1 输入（生产通道 sdet_detect_ex / sdet_detect_ex_f64，orchestrator.cpp:2172-2196）

| 参数/字段 | dtype/shape | 单位/域 | invalid / NULL 语义 |
|---|---|---|---|
| image | uint16（FP32 通道，float→uint16 clamp [0,65535] 转换，orchestrator.cpp:2179-2187，DISP-STAR-001）/ double（FP64 通道全程不降级，PREC-105 同族）`[h·w]` 行主序 0-based | ADU | NULL / h≤0 / w≤0 → rc=−1（sdet_api.cpp:1612、:2325、:2340） |
| handle（sdet_create 预建，orchestrator.cpp:1593-1612 参数构造） | StarDetectorHandle | — | NULL → −1；句柄级互斥使用（PHASE1_API_V1 §2 表行 handle 级 no/no）；SDetParams 9 字段生产消费面缺口=DISP-STAR-003（ALG-STARDET-001 §11.1/§11.3） |
| extra_names / extra_count | const char** / int | — | 可 NULL/0（生产调用传 nullptr,0，orchestrator.cpp:2175-2177、:2195-2196）；名称解析 parse_extra_name（sdet_api.cpp:779-802），不识别名该列全 0 |

### 17.2 输出（malloc 10 数组，唯一释放入口 sdet_free_detect_ex，sdet_api.cpp:2357-2373）

| 数组 | dtype/shape | 单位/值域 | invalid |
|---|---|---|---|
| x / y | double `[n]` | pixel（0-based，像素中心=索引+0.5） | n=0 时全 NULL（空场合法 rc=0，:2252-2263）；分配失败 → rc=−1（:2264-2270） |
| flux | float `[n]` | ADU（正常星=Moffat4 振幅 A，非解析积分流量） | 饱和星拟合失败=0.0f 哨兵（:1519-1531） |
| mag | float `[n]` | mag（正常星=−2.5·log10(Σ_box(pixel−B_fit))，box 半径=候选 R 钳 [5,200]；饱和星=−2.5·log10(A)，量纲差异=DISP-STAR-004） | box_sum≤0 或拟合失败=NaN（:2177-2198）；NaN 恒排末尾（:941-956） |
| saturated / has_saturated | int `[n]` | 0/1 | saturated=(A_fit>dynrange)（:2159）；has_saturated 恒=saturated（:2203，DISP-STAR-004） |
| extras 列（可选） | float `[n]` | 各 extra_name 定义 | — |

- 编排序列化：**star_det 权威块 FLOAT64 `[N,6]`**（列 [0..5]=x,y,flux,mag,
  saturated,has_saturated；orchestrator.cpp:2218-2246，必需块，写入失败→
  阶段失败 :2242-2247）+ **star_det_psf_compat 兼容视图 FLOAT32 `[N,4]`**
  （x,y,flux,mag，:2249-2257）。
- 消费方合同：PSF 拟合仅取列 [0]/[1]（DATA-P1-PSF §15.1，dpsf_psf.cpp:741
  现状只解包 x/y）；PLATESOLVE fallback 读 star_det 块显式 DETECTOR_FALLBACK
  并按「像素中心=索引+0.5」−0.5 转统一契约（orchestrator.cpp:1826-1829）；
  PLATESOLVE 禁止调用 sdet_detect_ex 重检测（:1748-1755）。

### 17.3 排序/截断/精度规则（汇总）

- 输出全序：mag 升序 stable_sort + NaN 末尾（sdet_api.cpp:941-956）；
  dedup（饱和优先保、正常星 d²≤1.0，:822-939）；输出层 maxStars 截断保最亮
  （:2240-2242），候选层截断 maxStars×2（:2028-2034）。
- FP64 通道（sdet_detect_ex_f64，:2343-2355）全程 double 不降级，仅
  out_flux/out_mag 按 ABI 保持 float32；FP32 通道经 uint16 量化
  （DISP-STAR-001）。
- determinism=fixed_reduction_order（module.yaml；输出 bitwise 与线程数
  无关，ALG-STARDET-001 §5）；dtype/determinism 变更属科学改动，须走
  owner 流程；本节禁止被编排层词汇反向改写。

### 17.4 错误/边界

- 入口 rc：0=成功（含 0 星空场：输出指针全 NULL、*out_count=0，非错误，
  :2252-2263）；−1=参数无效/句柄 NULL/分配失败（:1612、:2264-2270）。
- 编排级：det_ret≠0 或 det_count≤0 → 退出码 STAR_DETECT_FAILED
  （orchestrator.cpp:2200-2212）；star_det 写块失败 → 阶段失败
  （:2242-2247）；缓冲在 PIPELINE 收尾释放（:2466）。
- 释放纪律：10 数组必须经 sdet_free_detect_ex 整组释放（:2357-2373），
  禁止逐数组 free（extras 数组同组释放）。

## 18. Phase1 wcs（plate_solve）模块输入/输出数据（DATA-P1-WCS）

> ID: DATA-P1-WCS  状态: CONTRACT_READY（P1-WCS-DOC 冻结，2026-09-07）
> 模块: lib/plate_solve;lib/phase1/wcs（astrocs.p1.wcs，迁移目标
> astrocs_p1_wcs.dll；现行实现生产源 lib/plate_solve/cpp/ipv/
> ipv_entry.cpp:237-649 12 个 C ABI 导出 + 内核 ipv_solver/ipv_select/
> ipv_triangle/ipv_itertrans/ipv_robust_refine/ipv_wcs/ipv_sip，合计
> 13821 行；唯一权威签名头 lib/plate_solve/cpp/ipv/include/ipv_api.h）。
> ALG: ALG-WCS-001（PLATESOLVE.md §11 逐符号锚）；SCI: SCI-WCS-001
> （docs/science/ASTROMETRY.md，FROZEN T102 2026-08-23，共享 SCI 引用
> 不改动）；编排级合同 API-P1-004（PHASE1_API_V1 §2：一帧只做一次权威
> 求解，PLATESOLVE 消费 PSF 星点禁重检测）。本节是该模块单位/dtype/
> shape/invalid 的唯一权威；descriptor astrocs.phase1.wcs-platesolve
> （module_adapters.cpp:512-526）为编排层词汇，由 P1-WCS-INT 对齐，不得
> 反向作为冻结依据。

### 18.1 输入（生产通道 ipv_solve_from_detections_v1，orchestrator.cpp:1967）

| 参数/字段 | dtype/shape | 单位/域 | invalid / NULL 语义 |
|---|---|---|---|
| data 块（图像由 PipelineFrame 提供，求尺寸与边缘过滤用） | AIO_BLOCK 类型 [h·w] | ADU | data 块缺失 → BLOCK_MISSING（orchestrator.cpp:1808-1816） |
| detections | double `[n,6]` 行主序 | 列 [0..5]=det_x,det_y,flux,mag,sat,has_sat；det_x/det_y 为 pixel（**IPV 接口契约：像素中心=索引+0.5**） | orchestrator 由 star_measurements 统一契约（index-is-center）**+0.5 显式转换**构造（:1867）；NULL/0 星 → ret=0（ipv_entry.cpp:524 入口校验） |
| n_detections | int | — | 过滤后 0 星 → BLOCK_MISSING（orchestrator.cpp:1895-1900）；<3 星求解器显式失败 |
| image_width/height | int | pixel | 边缘过滤 5px 裁剪（:1866-1868） |
| ra0/dec0（OBJCTRA/OBJCTDEC，header 必需；config 可覆盖 initial_ra/initial_dec/focal_length/pixel_size） | double | deg | 缺失 → 阶段失败（:1933-1944 约束注释：必须使用 OBJCTRA/OBJCTDEC 作初始指向） |
| focal_length_mm/pixel_size_um（FOCALLEN/XPIXSZ） | double | mm / μm | 驱动 s0=206.265·pixel_um/focal_mm（ipv_select.cpp:49,:253）；畸值 → 匹配失败显式 NO_SOLUTION |
| star_measurements（orchestrator 侧权威源，非 C ABI 直入） | FLOAT64 `[N,≥15]` 行主序 | 列 0 star_id,1 x,2 y,3 flux_inst,4 flux_unc,5 background,6 psf_status,7 fwhm,8 A,9 B,10 mad,11 ecc,12 mag,13 saturated,14 has_saturated（:1846-1849） | 缺失/格式错 → BLOCK_MISSING（:1830-1839）；过滤 status∉{0,3}、sat r[13]、fwhm r[7]∉[0.5,20]、边缘 5px（:1852-1862） |
| star_det（fallback 源，orchestrator 侧） | FLOAT64 `[N,6]` | DATA-P1-STAR §17.2 编排序列化 | 仅 PSF 有效星不足时补充（显式 DETECTOR_FALLBACK）；坐标已是 +0.5 契约（:1878）；严重饱和（d[4]/d[5]≠0）排除；与 PSF 星 <0.5px 去重（:1891-1897） |
| gaia 句柄（ipv_set_gaia_handle） | intptr_t | Gaia DR3SP | 调用方保证生存期；句柄级互斥 |

### 18.2 输出（IpvWcsResult POD，ipv_api.h:39-61，success=1 时唯一权威）

| 字段 | dtype/shape | 单位/值域 | invalid |
|---|---|---|---|
| success | int | 0/1 | 0=失败（error_msg[256] 填充，ipv_entry.cpp:141 set_error_msg；:181-187/:218-224 异常路径）；**CD det 退化坍缩禁止冒充解**（DISP-WCS-001 失败-置信度语义，PLATESOLVE.md §11.3） |
| cd[4] | double `[2×2]` | deg/pixel（Y-down，cd12/cd22 已取反 ipv_wcs.cpp:542-544） | trans 线性项 det<1e-15 仅 warn 跳过 SIP（:322-325，DISP-WCS-004） |
| crval[2] | double | deg（ICRS/J2000） | 收敛失败 → success=0 |
| crpix[2] | double | pixel，1-based，=(w/2+0.5, h/2+0.5)（ipv_wcs.cpp:274-277） | **退化坍缩时近似 CRPIX 的输出不可判**（DISP-WCS-001） |
| sip_a/b/ap/bp[36] | double（i*6+j 索引） | 无量纲畸变系数 | order≤1 全 0；AP[6]−=1、BP[1]−=1 约定（:456-461）；网格拟合失败仅 warn（:477） |
| rms_px / rms_arcsec | double | pixel / arcsec | n_pairs=0 时 =0.0（:513-517），须与 success 联合解读 |
| n_pairs / n_detected / n_catalog | int | — | 匹配对/检测星/星表星计数 |
| trans_order | int | 1/2/3 | 失败时 trans_order=0（fail_result，ipv_solver.cpp:396-398） |
| ctype1/ctype2[16] | char | "RA---TAN(-SIP)"/"DEC--TAN(-SIP)" | — |
| error_msg[256] | char | — | 空串=无错；NULL 参数/C++ 异常填充（ipv_entry.cpp:181-187/:218-224） |

- 编排写回（唯一权威落位，orchestrator.cpp:2003-2050）：header KV
  CTYPE1/2、CRVAL1/2、CRPIX1/2、CD1_1/CD1_2/CD2_1/CD2_2、RADESYS=ICRS、
  EQUINOX=2000.0；sip_order>0 时 A_ORDER/B_ORDER + A_i_j/B_i_j；
  ap_order>0 时 AP_ORDER/BP_ORDER + AP_i_j/BP_i_j。求解失败 →
  PLATESOLVE_FAILED（:1980），**不写任何 WCS 头**（失败不留半成品）。
- inlier 缓冲（ipv_get_last_inliers，ipv_api.h:203-221）：double 9 列
  `[n,9]` = det_x,det_y,gaia_ra,gaia_dec,pred_x,pred_y,residual_x,
  residual_y,residual_dist；det_x/det_y 为 +0.5 契约 pixel；gaia_* 为
  deg（ICRS）；residual_* 为 pixel/pixel/pixel。

### 18.3 排序/截断/精度规则（汇总）

- 输出全序：无排序输出（WCS 头标量 + SIP 系数表）；inlier 缓冲按匹配对
  序稳定。坐标契约双轨见 §18.1/§18.2（统一契约 ↔ IPV 接口契约由
  orchestrator :1867 显式桥接；CRPIX 1-based 与 +0.5 契约自洽）。
- FP64 全链路：detections/IpvWcsResult 全 double，无量化降级（PREC-105
  同族合规）；Gaia 查询 mag 上限自适应（ipv_select.cpp:429 m_lim）。
- determinism=fixed_reduction_order（module.yaml；投票矩阵整数归并
  ipv_triangle.cpp:347-357，线程数无关；§11.4 F5 冻结断言）；
  dtype/determinism 变更属科学改动，须走 owner 流程；本节禁止被编排层
  词汇反向改写。

### 18.4 错误/边界

- C ABI：ret=0 失败/1 成功；result->success 同步；error_msg 载因；
  NULL solver/result/句柄 → ret=0（ipv_entry.cpp:524 入口校验域）。
- 求解器：三角形 0 匹配 / iter_trans 全阶失败（ipv_solver.cpp:883-921）→
  fail_result（trans_order=0, success=false）显式返回；极区跨界保守不剪枝
  （PLATESOLVE.md §4）；RA 环绕 dra=360−dra+cos(dec) 缩放（§4）。
- 编排级：BLOCK_MISSING（data/star_measurements 缺失、0 有效星）
  :1814/:1832/:1896；PLATESOLVE_FAILED :1980；DLL 未加载 :1763 退出码 2
  语义同 PSF 先例。
- 释放纪律：ipv_solve_destroy（ipv_entry.cpp:249）整句柄释放；gaia/
  detector 句柄由调用方（orchestrator init :1621-1643）管理。

## 19. Phase2 coverage（lib/phase2）模块输入/输出数据（DATA-COV-001）

> ID: DATA-COV-001  状态: CONTRACT_READY（P2-COV-DOC 冻结，2026-09-07）
> 模块: lib/phase2 coverage sources（astrocs.p2.coverage，迁移目标
> astrocs_p2_coverage.dll；现行实现生产源 lib/phase2/src/coverage.cpp
> 239 行 + 唯一权威签名头 lib/phase2/include/astro/phase2/coverage.h，
> 2 个 C ABI 导出，SRC-COV-001）。本节是该模块单位/dtype/shape/invalid
> 的唯一权威；ALG: ALG-COV-001（docs/algorithms/PHASE2_COVERAGE.md）；
> 编排级合同 API-P2-001（docs/api/PHASE2_API_V1.md，FROZEN，所有权图
> Coverage 行）；descriptor astrocs.phase2.coverage
> （module_adapters.cpp:623-638）为编排层词汇（端口坐标 PIXEL 登记与
> NESTED 球面实际不符，以本节为准修订），由 P2-COV-INT 对齐，不得反向
> 作为冻结依据。registry 端口语义沿用 DATA-P2-COV 端口名（本节冻结后
> 为其权威定义）。

### 19.1 输入（p2_coverage_build，coverage.cpp:144）

| 参数/字段 | dtype/shape | 单位/域 | invalid / NULL 语义 |
|---|---|---|---|
| hips_paths | `const char* const*` `[n_inputs]` | 文件系统路径（HiPS 目录） | NULL/空串 → rc=1 "empty path at index %llu"（:165-170）；顶层 NULL → rc=1 "no inputs"（:154-157，status 不一致=DISP-COV-001） |
| n_inputs | uint64 标量 | 无量纲 | 0 → rc=1 "no inputs"（:154-157） |
| out | P2CoverageResult* 调用方分配 | — | NULL → rc=1（:147）；两阶段协议：先 union_cells=NULL 容量查询，再分配 K 后二次调用（§19.2） |

每帧 HiPS 兼容域（inspect_frame :59-140 逐项校验，全部经 AIO
`aio_hips_open(path, AIO_HIPS_RD_SIGNAL)` :61 只读 properties/Moc.fits，
不读像素）:

| properties 键 | 值域 | 拒绝行为 |
|---|---|---|
| hips_order | int ≥0（叶级 order） | 缺失/负 → rc=1 "missing hips_order"（:88-92） |
| hips_tile_width | 必须 =512 | 其他 → rc=1 "unsupported tile_width=%d"（:93-97） |
| hips_version | 非空 | 缺失 → rc=1 "missing hips_version"（:98-102） |
| hips_frame | ∈ {equatorial, icrs} | 其他 → rc=1 "unsupported hips_frame=%s"（:103-108） |
| obs_filter | 字符串（passband 名） | 与基准帧非空不一致 → rc=1 "filter mismatch"（:181-193）；**空串静默放行**（DISP-COV-003） |
| （Moc.fits） | 叶级 NESTED tile ipix 列表 | AIO 层读取，aio_hips_reader.cpp:141/:166-170 |

### 19.2 输出（P2CoverageResult，coverage.h:40-48，rc=0 时唯一权威）

| 字段 | dtype/shape | 单位/值域 | invalid |
|---|---|---|---|
| n_inputs | uint64 标量 | 无量纲 | 回填实际帧数（:216） |
| inputs | P2HipsInputInfo `[n_inputs]` 调用方分配（coverage.h:31-38） | — | union_cells/inputs 非空才回填（:219-228）；两阶段第一次调用不写 |
| n_union_cells | uint64 标量 K | 无量纲 | 两阶段第一次调用即有效（容量查询，:218） |
| union_cells | P2MocCell `[K]` 调用方分配（coverage.h:26-29） | order=uint64（=target_order，:221）、ipix=uint64（NESTED 父单元索引，<12·4^order，去重升序 :212-214） | 第二次调用回填（:219-224）；K=0 合法（空 MOC，rc=0） |
| target_order | int 标量 | 无量纲（HEALPix order） | = min(逐帧 hips_order)（:194-196，冻结：禁低 order 插值伪装分辨率） |
| status | int | 0=ok（:229） | 错误路径 1（:168/:177/:190/:200）；"no inputs" 分支 rc=1 而 status=0（DISP-COV-001） |
| error | char[512] | — | rc=1 时载因；memset（:151）先于各错误分支 strncpy（如 :155），error 有效 |

P2HipsInputInfo 逐字段（coverage.h:31-38，回填锚 :113-143）:

| 字段 | dtype/shape | 单位/值域 | invalid |
|---|---|---|---|
| hips_path | char[1024] | 路径字符串 | 调用方输入原样回填（:111） |
| frame_id | char[64] | 无量纲标识 | 路径基名截断（:113-118，DISP-COV-002 唯一性风险）；不保证全局唯一 |
| max_leaf_order | int | 无量纲（HEALPix order） | = 该帧 properties hips_order（:119） |
| n_tiles | int | 无量纲 | 该帧叶级 tile 数（AIO Moc.fits 读取，回填 :136；初值 :120） |
| filter_passband | char[64] | 无量纲字符串 | properties obs_filter（:86/:121-123）；缺失=空串（DISP-COV-003） |
| frame_type | char[32] | "equatorial"/"icrs" | properties hips_frame（:85/:124-126） |

### 19.3 数据语义（唯一权威，对齐 docs/api/PHASE2_API_V1.md 所有权图）

- **coverage 是几何集合量，非权重**（P2-COV 合同红线，ALG-COV-001 §7）:
  union MOC cell / target_order / n_tiles / 覆盖帧数为 NESTED
  equatorial/ICRS 球面几何登记，无量纲整数；禁止被任何下游作为科学
  权重/置信度使用（科学权重唯一冻结式 `w_UPM = quality_factor ×
  geometric_reliability × control_ivar`，docs/science/PHASE2_UPM.md §5，
  support 仅 eligibility/coverage 语义）。
- **coverage/support/validity 分离**（matrix P2-COV 专项）: 本模块输出
  仅 coverage；support（样本级 [0,1]，SCI-INT-001 §2）与 validity
  （finite/accepted 标志，SCI-INT-001 §5）不在本节合同域，任何把
  union cell 计数当 support 数值或 validity 判定的消费均违反本节。
- **无交集/depth/missing-tiles 输出**（DISP-COV-004，如实登记）:
  现状仅 union（coverage.cpp:204-214）；intersection/逐 cell depth/
  missing tile 列表不输出，下游以逐帧 tile 集合自行推导；语义澄清:
  覆盖度几何 ≠ UPM 权重因子 geometric_reliability（乘数恒 1.0 缺陷
  归 P2-UPM 域，PUBLIC_API.md API-UPM-001 DISP-UPM-003）。
- **所有权**（对齐 API-P2-001 §1 所有权图 Coverage 行
  docs/api/PHASE2_API_V1.md:15）: P2CoverageResult 及其数组全部调用方
  分配；`p2_coverage_free` 仅 memset 清零 POD（coverage.cpp:233-237），
  不释放堆、无所有权转移；重复 build 幂等（每次全量重扫，无缓存，
  DISP-COV-005）。
- **dtype/确定性**: 全链路整数（无浮点），bitwise 确定；输出 MOC
  升序唯一（sort+unique :212-214）；determinism=fixed_reduction_order
  （module.yaml）；坐标契约 = HEALPix NESTED 父单元索引（z-order 2D
  移位 `t >> 2·(order_f−target_order)`，:207-209），禁止以 PIXEL 坐标
  解读（registry descriptor 像素登记由 P2-COV-INT 修订）。

### 19.4 错误/边界

- C ABI: rc=0 成功（含 K=0）/1 失败（error[512] 载因）；错误码映射
  （API-P2-001 §4）: INVALID_INPUT 类 → ACS_ERR_PARAM（编排层，
  p2_session.cpp:125-141 map_rc 落 ACS_ERR_STATE 由 map_rc 表实现）。
- 全部拒绝路径显式 rc=1（§19.1 表）；并发合同 reentrant=yes /
  threadsafe=no（独立对象）/ internal_parallel=none / 取消点=无
  （API-P2-001 §2 行 1）；阶段级取消由编排 session 阶段边界提供
  （p2_session.cpp:119-121）。
- 释放纪律: p2_coverage_free（:233-237，memset :235）调用方收尾
  （PHASE2_API_V1 §1 表行登记）；无隐藏全局状态（单文件 static/匿名
  ns 函数，无模块级可变状态）。

## 20. Phase2 mosaic write（lib/phase2 tools）模块输入/输出数据（DATA-P2-HIPS）

> ID: DATA-P2-HIPS  状态: CONTRACT_READY（P2-HIPS-DOC 冻结，2026-09-07）
> 模块: lib/phase2/tools/stage2.cpp 生产工具 astrocs-stage2
> （astrocs.p2.hips_writer；迁移目标 astrocs_p2_hips_writer.dll 为矩阵
> 合同值，尚未存在，由 P2-HIPS-IMPL 建立，禁止声明 IMPLEMENTED）。
> 本节是 Phase2 马赛克（HiPS）输出单位/dtype/shape/invalid 的唯一
> 权威；ALG: ALG-P2-HIPS-001..004（docs/algorithms/
> PHASE2_MOSAIC_WRITE.md，算法级逐符号锚由该文档登记）；编排级合同
> API-P2-001（docs/api/PHASE2_API_V1.md，FROZEN）；§4/§4a 的
> signal/support/invalid 与 variance/ivar 产品语义在此落地为 Phase2
> 输出侧逐文件语义；输入读合同: IO-002（docs/interfaces/io/
> IO_002_HIPS_INPUT_INTERFACE.md）；发布合同: IO-003（docs/interfaces/
> io/IO_003_ATOMIC_OUTPUT_PUBLISH.md，§20.3 对齐边界）；配置 schema
> 唯一权威签名源 lib/phase2/include/astro/phase2/stage2_common.h
> :16-99（P2Stage2Config），公共消费面同步冻结于 PUBLIC_API.md
> API-P2-HIPS-001。

### 20.1 输入（stage2 配置 schema 公共消费面 + 每帧 Phase1 HiPS 产品）

P2Stage2Config 公共关键字段（唯一签名源 stage2_common.h:16-99，行号
为该头实测；本表冻结公共消费面语义，完整字段集以头文件为准；单位
权威=本节，消费面副本见 API-P2-HIPS-001）:

| 字段 | 默认（锚 :16-99） | 单位/域 | 语义/约束 |
|---|---|---|---|
| hips | —（:19） | 文件系统路径 `[n_frames]` | 每帧 Phase1 HiPS 目录；空集 → rc=2/3 拒绝 |
| target_order_spec / target_order | "auto" / −1（:20-21） | 无量纲（HEALPix order） | target_order≥0 显式采用，否则 = cov.target_order；**不得高于输入最高 order**（stage2.cpp:203-205，禁插值伪装分辨率，对齐 §19.2 target_order 冻结语义） |
| precision | 0（:50） | 无量纲 | 0=float32 / 1=float64 输出 dtype（stage2.cpp:529） |
| memory_limit_mb | 24576（:51） | MB | 执行内存预算（CON-002 全局执行预算域） |
| reject_method / reject_profile / reject_underdetermined_n | P2_REJECT_AUTO / "wbpp_2_9_1" / 2（:52-54） | 无量纲 | planning 层解析为显式方法；profile 版本化冻结（WBPP 2.9.1 同名语义） |
| reject_normalization | "astrocs_median_center_v1"（:56） | 无量纲 | 判定工作域归一；mask 应用回原始 calibrated 值 |
| large_scale_enabled（及 min_structure_pixels/low_grow/high_grow） | false / 8 / 2 / 2（:60-63） | 无量纲 | astrocs.large_scale_rejection.v1，默认关闭（WBPP largeScaleClip 默认一致） |
| weight_mode | 2（:90） | 无量纲 | 2=ivar 逆方差（科学默认）；1=等权；0=support×snr²（legacy/诊断） |
| legacy_allow_weight_fallback | false（:94） | bool | ivar 产品缺失 → rc=7 显式科学错误；仅显式 true 允许降级 support 并 diagnostics 标红（stage2.cpp:565-578） |
| acr_route | "auto"（:95） | 无量纲 | 集成执行路由 |
| out_hips | —（:97） | 文件系统路径 | 输出 HiPS 产品集根目录 |
| diagnostics | true（:98） | bool | true → 落 `<out_hips>/diagnostics.json`（§20.3 provenance 链） |

每帧 HiPS 输入产品（读合同 IO-002；与 DATA-P1-HIPS §12.2 输出
一一对应）:

| 读端（stage2.cpp 锚） | 产品 | dtype/单位 | 消费语义 |
|---|---|---|---|
| AIO_HIPS_RD_SIGNAL（:536） | 每帧 signal/ | float32/64，ADU surface brightness | 逐样本积分分母侧科学值（SCI-INT §5 加权积分输入） |
| AIO_HIPS_RD_SUPPORT（:537） | 每帧 support/ | float32/64，无量纲 [0,1] | eligibility/覆盖支持度（禁作科学权重，§20.3 红线） |
| AIO_HIPS_RD_IVAR（:557，weight_mode=2 时强制打开） | 每帧 ivar/ | 1/ADU²（§4a，DATA-HIPS-IVAR-001） | 逆方差积分权重；缺失帧 → rc=7 或显式降级标红 |

### 20.2 输出（`<out_hips>` Phase2 马赛克产品集）

产品集注册（aio_hips_product_begin，stage2.cpp:592-596，
flags=AIO_HIPS_PRODUCT_SIGNAL|AIO_HIPS_PRODUCT_SUPPORT）: **仅
signal + support 两个 Image HiPS**；无 variance/ivar/snr 产品
（DISP-P2HIPS-001，如实登记——ivar 为输入侧逐帧产品，Phase2 不
输出合成 ivar/variance）。
**目标态合同（DATA-UNC-001，2026-09-09 冻结）**：Phase2 马赛克产品
集目标含 variance/ivar/nused/nrej 子产品与 provenance 键（§30.1–
§30.3，DATA-P2-VAR-001/DATA-P2-REJ-001/DATA-P2-PROV-001）；实现
不得先行，接线整改归 P2-001（writer 通道
`aio_hips_write_variance_tile` 已具备，属接线缺口非能力缺口，
DISP-P2HIPS-001 整改去向不变）；本节现状描述在实现落地前保持有效。
几何: NESTED（§2 唯一 ordering），
`nside = 2^(target_order+9)`（stage2.cpp:525），叶级阶
K=target_order+9，tile 512×512（tile_order=K−9，§2）；
`A_cell = 4π/(12·nside²)`（stage2.cpp:527-528）。

| 文件/数组 | dtype/shape | 单位/值域 | invalid |
|---|---|---|---|
| signal/NorderK/DirD/NpixN.fits | float32（precision=0 默认）/float64（=1）（stage2.cpp:529）`[512×512]` NESTED local | ADU surface brightness（writer finalize 单位名 "surface brightness"，aio_hips_writer.cpp:1035） | 无效像素 IEEE NaN（writer :483-485 else 分支置 NaN，无 FITS BLANK 整型卡，同 §12.2） |
| support/…fits | 同上 `[512×512]` | 无量纲 [0,1] = covered_area/A_cell（writer :478，>1 钳 1.0 :479；A_cell 同 §12.2 公式） | 无效像素 0.0（writer else 分支 sup 保持初值 0.0） |
| Moc.fits（每子产品） | BINTABLE 列 UNIQ | 无量纲（UNIQ 编码） | AIO writer finalize 生成（DATA-P1-HIPS §12.2 同构） |
| diagnostics.json | JSON 文本 | — | diagnostics=true 时落 `<out_hips>/diagnostics.json`（stage2.cpp:1748-1749）；键集见 API-P2-HIPS-001 |

写出 tile 集: cov.n_union_cells 顺序逐 tile（stage2.cpp:659-660），
探测读零帧的 tile 跳过（frames.empty() → continue，:669）；写后
HIPS_VERIFY AIO reader 回读 signal/support tile 数（:1659-1676，
signal 回读失败 rc=7 :1665）。

### 20.3 数据语义（唯一权威）

- **signal/support 逆变换合同**（matrix 专项）: 加权积分输出
  signal（SCI-INT §5）与 support，逆变换回 `flux_sum = signal×area`
  （`area = support×A_cell`；stage2.cpp:1227-1228 chunk 路径 /
  :1588-1589 CPU 二次积分路径）与 `covered_area = area`，经
  AstroSphereTileView（flux_sum/covered_area/valid_mask，
  :1620-1628）进 AIO writer，按 DATA-P1-HIPS §12.2 同一公式落盘
  （signal=flux_sum/covered_area、support=covered_area/A_cell）——
  Phase2 输出单位公式与 P1 writer 严格同源，禁止第二套定义。
- **四概念分离**（matrix P2-HIPS 专项）:
  - signal = 加权积分信号 → flux_sum 逆变换（ADU surface
    brightness）——唯一科学信号产品；
  - support = covered_area/A_cell ∈ [0,1]（样本级支持度）——仅
    eligibility/覆盖语义，禁止作科学权重（§4；科学权重唯一冻结式
    `w_UPM = quality_factor × geometric_reliability × control_ivar`，
    docs/science/PHASE2_UPM.md §5）；
  - ivar/variance = **输入侧逐帧产品**（§4a，DATA-HIPS-VAR-001/
    DATA-HIPS-IVAR-001），Phase2 仅作 weight_mode=2 积分权重消费
    （stage2.cpp:1106-1117），不输出 Phase2 合成 variance/ivar
    产品（DISP-P2HIPS-001；目标态合同 DATA-P2-VAR-001 §30.1——
    weight_mode=2 且无 fallback 时输出 variance/ivar 子产品，
    `ivar_mosaic(p) = wsum(p)`，合成公式与 invalid policy 见 §30.1）；weight_mode 语义: 2=逐样本 ivar
    （ivar 缺失/无效样本 fallback support :1113-1114，计入
    fallback 统计）、0=support×snr²（legacy/诊断，local snr map
    → frame snr fallback，赋值 :1136，CPU 路径重复 :1396）、
    1=等权（std::fill 1.0 :1139，CPU 路径重复 :1399）；
  - mask（rejection reasons → accepted 标志 + large_scale 连通
    grow，p2_large_scale_apply :1549 + 二次积分 :1554-1565）:
    仅供二次积分 accepted 判定，**不输出产品、不进入权重式**。
  - 红线: **禁 support 冒充 ivar**——weight_mode=2 且 ivar 产品
    缺失帧 → 默认 rc=7 显式科学错误；仅 legacy_allow_weight_fallback
    =true 显式降级 support 并 diagnostics 标红（stage2.cpp:565-578）。
    ivar（1/ADU²）与 support（无量纲）量纲不同，任何静默互换违反
    本节。
- **序合同（HIPS-IMG-001，§3）**: 输出 FITS tile 行主序
  `(511−x)·512+y`；stage2 集成缓冲为 FITS 行主序，写入前按
  `nested_local_to_fits_index` 逆映射转 NESTED local 序（ACR 路径
  :1024-1040，CPU 路径 :1607-1619）——writer 约定 view 缓冲为
  NESTED local 序，漏转表现为 tile 内像素 16px 周期 comb/重复星点
  （stage2.cpp:1024-1027 注释冻结）。
- **provenance 链**: input_manifest_hash =
  sha256(canonical(sorted(frame_id|filter=;order=;frame=;)))
  （逐帧 meta 拼接 :230-236 + sort :238-239 + sha256 :243-244，
  stage2.cpp）→ p2_stage2_make_upm_cfg 注入 UPM（:428-430）→ UPM
  持久层 model_hash + diagnostics.json（:1746-1749）；HiPS
  properties 未写 provenance/manifest 键（DISP-P2HIPS-002，如实
  登记）。
- **原子发布边界（对齐 §12.5）**: stage2 直写 out_hips
  （aio_hips_product_begin :592 起），非原子、覆盖式、无回滚；原子
  rename/manifest COMPLETE 发布语义由 IO-003 编排层承接
  （DISP-P2HIPS-003）——与 DATA-P1-HIPS §12.5 同源对齐，禁止以
  stage2 直写目录冒认 IO-003 原子发布。
- **编排层词汇注记**: registry descriptor p2_write_descriptor
  （lib/core/src/module_adapters.cpp:739-756，module_id=
  "astrocs.phase2.write" :679）端口表 integrated=DATA-P2-INT in /
  mosaic=DATA-P2-RES out（UnitId::ADU/CoordinateFrame::PIXEL，
  :684-687）为编排层词汇，与球面 NESTED 马赛克实际不符（产品为
  NESTED 球面 tile，非 PIXEL 平面），以本节为准修订，P2-XX-INT
  对齐，不得反向作为冻结依据；DATA-P2-INT 端口（集成内部视图）
  逐符号数据合同见 §21（P2-INT-DOC 冻结，2026-09-09）。
- **dtype/确定性**: 全浮点输出限 float32/float64 IEEE 域
  （precision 唯一选择 :528）；整数登记量（nside/order/tile 计数/
  UNIQ）bitwise 确定；浮点积分确定性受 acr_route/execution
  预算（CON-002，CLI/退出码同源 API-P2-HIPS-001）控制。
- **交叉引用**: 上游 ALG-P2-HIPS-001..004（docs/algorithms/
  PHASE2_MOSAIC_WRITE.md）；同文档相关节: DATA-P2-INT（暂无独立
  节，见上注记）、DATA-P1-HIPS（§12，单位公式同源）、
  DATA-HIPS-VAR-001/DATA-HIPS-IVAR-001（§4a，输入侧 ivar/variance）、
  DATA-COV-001（§19，target_order/union MOC 上游合同）；下游
  TEST-P2-HIPS-001（MISSING，P2-HIPS-DOC 登记，由 P2-HIPS-TEST
  落地 + EVIDENCE）。

### 20.4 错误/边界

- 退出码（stage2 工具进程级，与 PUBLIC_API API-P2-HIPS-001 同表）:
  2=config/CLI 解析（:133/:140/:146/:153/:160-165）；3=coverage/
  target_order（:195/:201/:207）；4=frame_id/sampler（:227/:283-319）；
  5=UPM 构建/持久化（:437/:456/:477/:488）；6=写路径/集成块
  （:517/:546/:589/:601/:653/:687/:793/:1055 等）；7=ivar 门
  （:574）/HIPS_VERIFY（:1665）。
- 无隐藏全局状态: stage2 为单进程顺序工具；UPM 句柄收尾
  p2_upm_close（成功/失败路径均覆盖）；诊断日志经 log/log_flush
  串行化，不并发写。
- 取消/重入: 工具无取消检查点（进程级信号由运行环境终止，无部分
  产品回滚——见 §20.3 原子发布边界）；同 out_hips 重跑覆盖直写
  （无清理责任，同 §12.1 out_dir 语义）。
- 并发: aio_hips writer 句柄单线程使用；tile 间顺序写（:660-661
  循环序），chunk 内多线程仅限积分计算（CON-002 cpu-workers），
  不改变输出 tile 顺序。

## 21. Phase2 integration（lib/phase2）模块输入/输出数据（DATA-P2-INT）

> ID: DATA-P2-INT  状态: CONTRACT_READY（P2-INT-DOC 冻结，2026-09-09）
> 模块: lib/phase2/src/integrate.cpp（76 行）+ 唯一权威签名头
> lib/phase2/include/astro/phase2/integrate.h（74 行）
> （astrocs.p2.integration；合同三件套 lib/phase2_int/，迁移目标
> astrocs_p2_integration.dll 为矩阵合同值，尚未存在，由 P2-INT-IMPL
> 建立，禁止声明 IMPLEMENTED）。本节是 Phase2 逐像素积分内核
> in/out 单位/dtype/shape/invalid 的唯一权威；ALG: ALG-P2-INT-001
> （docs/algorithms/PHASE2_INTEGRATION.md，逐符号锚与 §11 冻结附录）；
> SCI 上游: SCI-INT-001（docs/science/INTEGRATION.md，FROZEN，零改动）；
> API 面: API-P2-INT-001（PUBLIC_API.md）；编排级合同 API-P2-001
> （docs/api/PHASE2_API_V1.md，FROZEN）。本节冻结完成后，§20.3
> 「编排层词汇注记」所述 DATA-P2-INT 端口合同由本节承接（P2-XX-INT
> descriptor 对齐仍归后任务）。

### 21.1 输入（P2PixelStack，integrate.h:36-42，逐像素候选栈）

调用约定: 每次调用一个像素栈（count 个候选）；frame-major 批量
缓冲由调用方逐像素切出（stage2.cpp:1213-1223/:1515-1527、
acr_kernels.cpp:189-195）。字段可空语义为冻结合同（§20 同构）:

| 字段 | dtype/shape | 单位/域 | 可空语义 | 约束（违规→INVALID_INPUT） |
|---|---|---|---|---|
| values | f64 `[count]` | ADU（与校准后同标度，SCI §3） | 不可空（null+count==0 → NO_CANDIDATES :23-26；null 且 count>0 → NO_CANDIDATES，values==null 同分支） | accepted 样本须 finite（:37） |
| weights | f64 `[count]` | 1/ADU²（数值权重，策略在调用方；本层无 ivar 语义，SCI §3） | 可空=等权 1.0（:45 w=weights?w[i]:1.0） | accepted 样本须 finite ∧ ≥0（:44-48）；w==0 合法零贡献（:49） |
| support | f64 `[count]` | 无量纲 [0,1]（几何覆盖，§4；禁作科学权重，§20.3 红线） | 可空=1.0（输出侧 :71 空支撑守恒） | accepted 样本须 finite ∧ >0（:38-42） |
| accepted | u8 `[count]` | 无量纲 0/1（排异层产物，DATA-P2-REJ） | 可空=全接受（:34-36） | — |
| count | u32 标量 | 无量纲 | — | 0 → NO_CANDIDATES |

预检合同: `p2_validate_candidate_weights(weights,count)`（:10-17）
→ 0 合规 / 1 违规（null→0；任一 !finite 或 <0→1；w==0 合规）；
调用方权重构造后必经此门（stage2.cpp:1141/:1402）。

### 21.2 输出（P2PixelResult，integrate.h:53-63）

| 字段 | dtype/shape | 单位/值域 | invalid/未定 |
|---|---|---|---|
| signal | f64 标量 | ADU（=Σwᵢxᵢ/Σwᵢ，仅正权重 eligible 样本） | 非正权状态（§21.3 非 OK）时无定义，调用方须按 status 门禁消费（stage2.cpp `(status==0)?signal:0` 同型，acr_kernels.cpp:199-200） |
| support | f64 标量 | 无量纲 [0,1] = max reducer 输出（support 输入空 → 1.0，:71 空支撑守恒） | 同上；现状实现含 DISP-P2INT-001（零权重 accepted 样本不进 max，保守方向，ALG §11.3） |
| n_used | u32 标量 | 无量纲 = n_positive_weight（通过门正权样本数，:56） | — |
| n_candidates | u32 标量 | 无量纲 =count | — |
| n_accepted | u32 标量 | 无量纲 | — |
| n_finite | u32 标量 | 无量纲 | — |
| n_positive_weight | u32 标量 | 无量纲 | — |
| status | u32 枚举 | 无量纲 0..4（P2IntegrateStatus） | — |

### 21.3 状态机（唯一权威=integrate.h:45-51；判据=ALG-P2-INT-001 §4）

| status | 值 | 触发 | 锚 |
|---|---|---|---|
| OK | 0 | ≥1 正权重 eligible 样本 | integrate.cpp:72 |
| NO_CANDIDATES | 1 | count==0 ∨ values==null | :23-26 |
| ALL_REJECTED | 2 | n_accepted==0（全拒） | :65-69 |
| ZERO_VALID_WEIGHT | 3 | n_accepted>0 ∧ n_positive_weight==0（全零权重，**不做除法**） | :49/:65-69 |
| INVALID_INPUT | 4 | accepted 样本 values 非 finite ∨ support 非 finite/≤0 ∨ weights 非 finite/负 | :37/:38-42/:44-48 |

rc（函数返回）: 0=语义由 status 承载；1=stack/result null（:20-21）。
无除零路径（wsum==0 时 :65-69 分支先行）；无静默 0/±Inf 输出
（§4 无有效样本须显式 status 同源条款）。

### 21.4 单位/dtype/确定性（唯一权威）

- 全浮点为 IEEE f64（核内）；f32/f64 写盘转换在 Stage2 precision
  （§20.2/:529），本层不输出文件产品。weights（1/ADU²）与
  support（无量纲）量纲不同，本层分开消费、禁止互换（§20.3
  红线在本层的镜像）。
- 确定性: 候选索引 i=0..count-1 固定序单栈归约（:30-57）；
  vs/wsum 双累加器顺序确定（:52-53），signal=vs/wsum 单除法
  （:70）→ 同输入 bitwise 确定且与 worker 数无关（像素间并行在
  调用方，像素内无跨 worker 归约；Stage2 per-thread 统计 thread id
  定序归并 stage2.cpp:1305-1313；ACR :218/:228 schedule(static)）
  ——ALG-P2-INT-001 §6 parallel reduction tolerance 合同。
- 整数登记量（status/counters）bitwise 确定；无隐藏全局状态
  （纯函数，reentrant=yes）。

### 21.5 错误/边界

- 错误面=§21.3 五态 + rc=1 null 栈；无部分输出承诺（INVALID_INPUT
  时 n_used=已计数部分，:58-63，其余计数器为已处理前缀值）。
- 取消/重入: 内核无取消检查点（迁移 ThreadLease 接线归
  P2-INT-IMPL，与 DISP-COV-005 同构）；可重入（无全局状态）。
- 缺陷登记（不改码）: DISP-P2INT-001（sup_max 漏计零权重 accepted
  样本，:54-55 vs integrate.h:17，整改归 P2-INT-IMPL/TEST）；DISP-
  P2INT-002（INTEGRATION.md:58 表述矛盾，ALG §11.3）。SCI §5:63
  引用行号 10-79/1-75 超出实测 76/74 行（锚漂移，行号权威=ALG
  文档 §3 实测）。

### 21.6 交叉引用

- 上游: SCI-INT-001（FROZEN）；ALG-P2-INT-001（本域算法权威）；
  DATA-P2-REJ（accepted 掩码，P2-REJ 域）；DATA-P2-COR（values
  校准值，Phase1 校准链）；ivar 输入权重语义=§4a/
  DATA-HIPS-IVAR-001（weight_mode=2 消费在 Stage2，:1106-1117）。
- 下游: DATA-P2-HIPS（§20，signal/support 逆归一化消费域，
  stage2.cpp:1227-1228/:1588-1589）；ACR 加速消费（acr_kernels.cpp，
  DATA_SEMANTICS 未设独立 ACR 节，消费合同以 §21 + ALG §6 为准）；
  TEST-P2-INT-001（MISSING，P2-INT-TEST 落地；设计冻结面=
  ALG-P2-INT-001 §11.4）。
- 同文档: §4（signal/support/invalid 基础语义）、§6（precision）、
  §20（Phase2 mosaic write 下游域）。

## 22. Phase2 rejection（lib/phase2）模块输入/输出数据（DATA-P2-REJ）

> ID: DATA-P2-REJ  状态: CONTRACT_READY（P2-REJ-DOC 冻结，2026-09-09）
> 模块: lib/phase2/src/rejection.cpp（2076 行）+ 唯一权威签名头
> lib/phase2/include/astro/phase2/rejection.h（329 行）
> （astrocs.p2.rejection；合同三件套 lib/phase2_rej/，迁移目标
> astrocs_p2_rejection.dll 为矩阵合同值，尚未存在，由 P2-REJ-IMPL
> 建立，禁止声明 IMPLEMENTED）。本节是 Phase2 候选栈排异内核
> in/out 单位/dtype/shape/invalid 的唯一权威；ALG: ALG-P2-REJ-001
> （docs/algorithms/PHASE2_REJECTION.md，逐符号锚与 §11 冻结容差）；
> SCI 上游: SCI-REJ-001（docs/science/REJECTION.md，FROZEN，零改动；
> descriptor 占位 SCI-P2-REJ-001⇒SCI-REJ-001 映射见 ALG §11.5）；
> API 面: API-P2-REJ-001（PUBLIC_API.md）+ 编排级 API-P2-001
> （FROZEN）。

### 22.1 输入（按消费路径分层；锚=rejection.h，未注文件者同）

**(1) eligibility 层**（P2EligibilityInput，rejection.h:199-205）:

| 字段 | dtype/shape | 单位/域 | 可空语义 | 约束（违规→rc=1/INVALID_INPUT） |
|---|---|---|---|---|
| values | const f64* `[count]` | ADU（UPM-calibrated） | 不可空（in/out null → rc=1，:1129-1140） | — |
| weights | const f64* `[count]` | 1/ADU²（数值域；策略在调用方） | 可空=等权（输出侧未写，调用方按等权处理，:209-211） | — |
| valid | const u8* `[count]` | 无量纲 0/1 | 可空=全部有效（core :1189-1191） | — |
| support | const f64* `[count]` | 无量纲 [0,1]（仅资格门禁作科学权重语义门，不进统计；§20.3 红线镜像） | 可空=不检查（core :1192-1198） | — |
| quality | const u32* `[count]` | 位集（现状 control 级数据模型，stage2 传 nullptr 并记录，:236-237） | 可空=不检查 | — |
| count | u32 标量 | 无量纲 | — | — |
| support_threshold | f64 标量 | 无量纲 | — | 严格大于语义（rejection.h:206；core :1108/:1192-1198） |
| quality_flags_required | u32 标量 | 位集 | — | 0=不要求 quality（:207；core :1199-1203） |

连续版入口 `p2_eligibility_filter` 声明 rejection.h:222（compat 路径
消费，ALG §3）；生产路径走 (2) gather。

**(2) gather 层**（P2EligibilityGatherInput :227-245 / Output :247-261；
strided frame-major 逐像素调用）:

Input: `values` const void*（frame-major，value_dtype 0=fp32/1=fp64，
:245）+ value_stride；`weights`/`support` 同 dtype 可空 + 各自 stride；
`valid` u8 可空 + stride；`quality` u32 可空 + stride；`frame_ids`
u64 可空（紧凑、帧序一一对应）；count/pixel；support_threshold 与
quality_flags_required 语义同 (1)。

Output → 紧凑 P2CandidateStack（kernel 输入，rejection.h:266-273）:

| 字段 | dtype/shape | 单位/域 | 可空语义 | 约束 |
|---|---|---|---|---|
| values | f64 `[n_eff]` | ADU（f32 源→f64 提升，:1164-1179） | 不可空 | gather n==0 → rc=0 空输出（:1152-1163） |
| weights | f64 `[n_eff]` | 1/ADU² | 可空=等权 1.0 | — |
| support | f64 `[n_eff]` | 无量纲 [0,1] | 可空 | — |
| frame_ids | u64 `[n_eff]` | 无量纲（稳定帧标识；ESD tie-break/确定性） | 可空 | — |
| count | u32 标量 | 无量纲 =n_eff（资格后候选数） | — | — |
| data_type | int 标量 | 无量纲 0=fp32,1=fp64（仅诊断，:272） | — | — |

+ `source_indices` u32 `[n_eff]`（Output 成员 :256）：eligible→
original slot 权威回映射——PHASE2_IVAR_WIRING 冻结注释（:252-255）：
compact 后禁止用 compact index 猜 original slot；ivar/quality/
variance/metadata 一律经此映射，权重构造方 stage2.cpp:1098 以此回
映射 ivar slot。

+ 诊断计数 u32 标量×5（Output :259-261）：`eligible_count`/
`invalid_finite`/`invalid_valid`/`invalid_support`/`invalid_quality`
（无量纲；连续版 P2EligibilityOutput :207-220 同名四诊断 +
eligible 位图 `[count]`，V15FilterAllPolicies :4337）。

**(3) kernel 层**（P2CandidateStack :266-273 + P2RejectionPlan
:151-169）:

- typed params 六组（禁止跨方法共享 low/high/max_iter，:99 注释）:
  P2SigmaParams :100-104（robust_mad/winsorized/averaged/
  median_sigma 共用结构）/ P2LinearFitParams :106-110 /
  P2EsdParams :112-115 / P2PercentileParams :117-120 /
  P2MinmaxParams :122-126 / P2RcrParams :128-130（technique=0
  SS_MEDIAN_DL 唯一支持）。
- P2LargeScaleParams :144-149: enabled 默认 0（:142 注释）/
  min_structure_pixels=8 / low·high_grow_radius_pixels=2。
- plan 归一化: P2RejectionNormalization :92-97（NONE=0/
  MEDIAN_CENTER=1/MEDIAN_SCALE=2），floor 默认 1e-12（:157）；
  minimum_n（:153）；underdetermined_n=2（:155）。
- AUTO 仅合法于 plan_resolve 请求（P2RejectionPlanRequest :171-180:
  request 允许 AUTO；nominal_contributors u32，wbpp_current=group
  active 一次解析、astrocs_adaptive=tile nominal depth，
  :174-177；profile 版本化 wbpp_2_9_1）；kernel 永不接收 AUTO
  （:285 注释，违规→INVALID_METHOD :1688-1701）。

### 22.2 输出（P2RejectionDecision，rejection.h:275-283）

| 字段 | dtype/shape | 单位/值域 | invalid/未定 |
|---|---|---|---|
| reasons | u8 `[count]` | 0..3（P2RejectReason :72-78: ACCEPTED=0/REJECTED_LOW=1/REJECTED_HIGH=2/UNDERDETERMINED=3）。判向冻结=低于 lower threshold→REJECTED_LOW、高于 upper→REJECTED_HIGH（:20-21 冻结注释，禁止原始值正负号判向） | UNDERDETERMINED reason=全接受语义（计入 accepted_count，:1820-1834）；INVALID_METHOD/UNDERDETERMINED 栈时 reasons 相应全 UNDERDETERMINED/全接受（:1688-1701/:1842-1849） |
| accepted_count | u32 标量 | 无量纲（含 UNDERDETERMINED 样本，全接受语义） | — |
| rejected_low | u32 标量 | 无量纲（仅显式拒绝） | — |
| rejected_high | u32 标量 | 无量纲（仅显式拒绝） | — |
| iterations | u32 标量 | 无量纲（kernel 外层迭代：ESD=k_out；RCR=3；percentile/minmax=1） | — |
| status | int 标量 | 无量纲 0..7（P2RejectStatus :80-90，§22.3） | — |

kernel 输出之外（本域其余产物）:

- eligibility 层输出: 合格值/掩码紧凑输出与诊断计数（§22.1(2)
  表与计数清单）。
- large_scale 原地修改: low/high u8 frame-major
  `[depth][height][width]`（每帧 width×height 字节），1=rejected；
  原地只增不减（p2_large_scale_apply 声明 :291-297/:292，语义
  :2043-2046；半径 0 → mask 不变 :1986）。

### 22.3 状态机（唯一权威=rejection.h:80-90；判据=ALG-P2-REJ-001 §4.1）

| status（实测枚举名） | 值 | 触发 | 锚（rejection.cpp） |
|---|---|---|---|
| P2_STATUS_OK | 0 | 判定完成且无 UNDERDETERMINED reason 且 accepted_count>0 | :1853-1855 |
| P2_STATUS_MIN_SAMPLES | 1 | count==0（ex :1706，rc=0；**非 NO_CANDIDATES**——DISP-P2REJ-002，NO_CANDIDATES 属积分域 P2IntegrateStatus integrate.h:46）∨ 资格数<min_samples（compat :1896-1907） | :1706/:1896-1907 |
| P2_STATUS_ALL_REJECTED | 2 | accepted_count==0 且 n>4（全拒非小栈） | :1851 |
| P2_STATUS_INVALID_INPUT | 3 | 任一候选 values 非 finite（compat 覆盖 :1971-1972） | :1709-1718 |
| P2_STATUS_UNDERDETERMINED | 4 | n≤underdetermined_n(2) ∨ n<minimum_n（:1739-1747）∨ 部分 UNDERDETERMINED reason（:1854）∨ 全拒 n≤4 容错 fallback（阈值冻结不变，:1842-1849） | :1739-1747/:1842-1855 |
| P2_STATUS_INVALID_CONFIGURATION | 5 | PERCENTILE×norm≠MEDIAN_CENTER（:1722-1728）∨ RCR×norm≠NONE（:1730-1736） | :1722-1736 |
| P2_STATUS_INVALID_METHOD | 6 | plan.method 出界（含 AUTO=10 进 kernel；AUTO 仅合法于 plan_resolve） | :1688-1701（status :1699） |
| P2_STATUS_INTERNAL_ERROR | 7 | kernel 内部不变量破坏（现状不可达，设计保留态） | h:89 |

八态互斥显式（V17InvalidMethodStatus :4763 等冻结断言；
ALG §11.4 F3）。per-sample reason 与 status 分离（SCI §7 状态分离
不变量；reason 4 值正交）。

rc（函数返回）: rc=0 语义由 status 承载（含科学态 INVALID_*）；
rc=1 参数 null/非法（ex: stack/plan/out null :1687、reasons/values
null 且 count>0 :1707；large_scale 参数非法 rc=1 :2054-2060；
plan_resolve :1031-1046；gather/eligibility :1129-1140/:1152-1163）。

### 22.4 单位/dtype/确定性

- 工作域全浮点 IEEE f64（gather f32 源→f64 提升，:1164-1179；无
  long double/复数）。MINMAX 判定用原始域值（:1812-1814 分派
  stack->values）；PERCENTILE scale=原始域 |median|（:1587）；σ 族/
  ESD/RCR 在工作域（MEDIAN_SCALE 下无量纲化
  work/max(|median|,1e-12)，:1763-1766）。weights（1/ADU²）与
  support（无量纲）量纲不同，分开消费禁止互换（§20.3 红线在本层
  镜像）；整数登记量（status/reasons/计数）bitwise 确定。
- 确定性=fixed_reduction_order: 逐样本独立判定（无跨样本浮点归约
  顺序问题）→ per-pixel 决策 bitwise 独立于 worker 数（像素间并行
  在调用方 stage2.cpp:1288/acr_kernels.cpp:218；per-thread 统计
  thread id 定序归并 stage2.cpp:1305-1313）。ESD tie-break=frame_id
  （1e-15 eps，:1515-1518）；linear_fit sort (value,orig_index)
  字典序稳定（:1417-1423）；minmax 比较器 value-only（std::sort
  非稳定，tie-break 未显式冻结=DISP-P2REJ-004，同输入同编译器
  确定）。验证锚: G6PermutationInvariance :2863/
  V15ExPermutationInvarianceTyped :4443（ALG §6 冻结容差）。

### 22.5 错误/边界

- rc 语义（§22.3 末）; 大栈 n>64 堆 fallback（n≤64 固定 scratch
  :944-986，无每像素堆分配）；accept 集 nc<2（σ 族）/:3（ESD）
  break、s≤1e-12 不除零（:1263/:1311/:1367/:1508/:1629）；
  linear_fit N<4 break（:1416）；minmax 删后<min_kept → 全栈
  UNDERDETERMINED, iterations=0（:1657-1664）。
- 兼容门: compat p2_reject_stack（:1863-1974）min_samples 换算
  :1891-1900 仅测试/旧调用（h:299 冻结注释"生产 Stage2 不再调用"；
  兼容 typed 换算 :1913-1942、non-finite 覆盖 :1971-1972）。
- 并发/重入: 无隐藏全局状态 reentrant=yes（无全局/静态可变状态，
  kRcrSS* 只读 const 表）；无取消检查点（ThreadLease 接线归
  P2-REJ-IMPL，与 DISP-COV-005 同构）。
- 缺陷登记（不改码）: DISP-P2REJ-001（h:118 percentile low_fraction
  注释"默认 0.1"漂移 vs 实现/SCI 0.2，整改归 P2-REJ-IMPL）；
  DISP-P2REJ-002（SCI §8 "无候选→NO_CANDIDATES" vs 实现
  MIN_SAMPLES，澄清归 SCI 修订流程）；DISP-P2REJ-003（SCI/
  REJECTION_ALGORITHMS 行号锚漂移，行号权威=ALG §3 实测）；
  DISP-P2REJ-004（minmax 等值 tie-break 未显式冻结，整改候选归
  P2-REJ-IMPL/TEST）。

### 22.6 交叉引用

- 上游: SCI-REJ-001（docs/science/REJECTION.md，FROZEN，零改动）；
  ALG-P2-REJ-001（docs/algorithms/PHASE2_REJECTION.md，本域算法
  权威；SCI-P2-REJ-001⇒SCI-REJ-001 映射=ALG §11.5）。
- 下游: API-P2-REJ-001（PUBLIC_API.md）+ API-P2-001（编排级，
  FROZEN）；TEST-P2-REJ-001（MISSING，P2-REJ-DOC 登记，P2-REJ-TEST
  落地+EVIDENCE）。
- 同文档: §20（编排域 mask 消费/二次积分，stage2.cpp:1544-1560）；
  §21（下游积分消费 accepted mask，DATA-P2-INT）；§19（本域与
  target_order 无关）。
- 端口词汇注记: registry descriptor p2_reject_descriptor
  （module_adapters.cpp:700-717，module_id=astrocs.phase2.reject
  占位）端口表为编排层词汇，由 P2-XX-INT 对齐 astrocs.p2.rejection，
  不得反向作为冻结依据。

## 23. Phase2 sampling（lib/phase2）模块输入/输出数据（DATA-P2-SMP）

> ID: DATA-P2-SMP  状态: CONTRACT_READY（P2-SAMP-DOC 冻结，2026-09-09）
> 模块: lib/phase2/src/sampler.cpp（1156 行）+ 唯一权威签名头
> lib/phase2/include/astro/phase2/sampler.h（136 行）
> （astrocs.p2.sampling；合同三件套 lib/phase2_samp/，迁移目标
> astrocs_p2_sampling.dll 为矩阵合同值，尚未存在，由 P2-SAMP-IMPL
> 建立，禁止声明 IMPLEMENTED）。本节是 Phase2 background-clean 控制
> 点采样 in/out 单位/dtype/shape/invalid 的唯一权威；ALG:
> ALG-P2-SMP-001（docs/algorithms/PHASE2_SAMPLER.md，逐符号锚与 §11
> 冻结容差）；SCI 上游: SCI-UPM-001（docs/science/PHASE2_UPM.md，
> FROZEN，零改动；页头明示模块 "phase2 (upm/sampler)"；descriptor
> 占位 SCI-P2-SMP-001⇒SCI-UPM-001 映射见 ALG §11.4）；API 面:
> API-P2-SMP-001（PUBLIC_API.md）+ 编排级 API-P2-001（FROZEN）。

### 23.1 输入（锚=sampler.h；未注文件者同）

**(1) coverage**（const P2CoverageResult*，上游 P2-COV 域 DATA-P2-COV）:
union_cells/tile 集合与 target_order 权威来源；null → rc=1
（sampler.cpp:476-479）；n_union 上限 1e6（:634-638）、
n_union×G² 上限 2e8（:644-648/:1071-1077）；首 tile ipix 越界 →
rc=1（:656-669）。

**(2) hips_paths / frame_ids**:

| 项 | dtype/shape | 单位/域 | 可空语义 | 约束（违规→rc=1） |
|---|---|---|---|---|
| hips_paths | const char* const* `[n_inputs]` | HiPS 根路径（AIO 打开 signal/support/snr/ivar :529-546） | 不可空 | — |
| frame_ids（cached 版） | const u64 `[n_inputs]` | 无量纲内容稳定帧标识（DATA-FRAME-ID-001，sampler.h:85-93 冻结） | 可空=实现内部 p2_frame_id 重算（:315-318）；0=非法哨兵（:512-523，sampler.h:117 冻结注释） | 与 hips_paths 同序（h:123） |

**(3) cfg**（P2SamplerConfig，sampler.h:32-57，15 字段；默认值
p2_sampler_default_config 单一来源 sampler.cpp:294-312；显式
cfg 覆盖；`<=0` 数值字段经 :485-502 修补回退默认——显式 0 被吞=
DISP-P2SMP-001；control_k_corr<=0 → 冻结默认 1.4 :497-498）:

| 字段 | dtype | 默认 | 单位/语义 |
|---|---|---|---|
| control_grid_per_tile | int | 8 | 每 union tile 控制网格边长 G（h:33） |
| patch_radius_leaf | int | 2 | SNR 邻域 leaf 半径（h:34） |
| min_samples | int | 5 | patch 有效样本下限（h:35；:793-800 显式拒绝） |
| snr_search_radius_deg | double | 0.05 | SNR 星点检索半径（h:36；:859-860） |
| background_patch_radius | int | 8 | 背景 patch 半径 → 17×17（h:38；:735-741） |
| background_clip_sigma | double | 3.0 | 亮端 clipping 阈值（MAD 单位，h:39；:815） |
| background_clip_iters | int | 3 | clipping 迭代次数（h:40；:812） |
| background_max_contamination | double | 0.20 | 亮像素占比上限（h:41；:1000） |
| background_contamination_sigma | double | 3.0 | 污染判定 sigma（h:42；:830-832） |
| background_min_retained_fraction | double | 0.60 | clipping 保留比例下限（h:43；:1005） |
| background_tolerance | double | 3.0 | 局部 tolerance gate（MAD 单位，h:44；:988-992） |
| background_neighbor_radius | int | 2 | 局部 baseline 邻域 cell 半径（h:45；:970-975） |
| background_catalog_veto | int | 1 | SNR catalogue veto 开关（h:46；:853-856） |
| control_k_corr | double | 1.4 | Drizzle 协方差方差放大因子（无量纲；h:47-53 冻结公式/实证 1.3883、保守 1.4；逐帧查表 :547-555 优先于该值） |
| cpu_workers | int | 1 | CON-004 worker 数，Runtime lease 唯一来源（h:54-56；stage2.cpp:273-274；0→1 :883） |

**(4) 输出缓冲四组**（probe/fill 协议，sampler.h:101-102/:118-119
冻结）: out_obs（P2ControlObservation 可空）/out_capacity、
out_n_obs、out_n_controls、out_stats（P2SampleStats 可空）、
out_controls（P2ControlNode 可空）/ctrl_capacity。容量不足**不报
错**：按 capacity 截断拷贝、out_n_* 返回真实需求量
（:1098-1117）。

### 23.2 输出

**(1) P2ControlObservation 13 字段**（upm.h:31-57；组装
sampler.cpp:1029-1059）:

| 字段 | dtype | 单位/域 | invalid/未定 |
|---|---|---|---|
| frame_id | u64 | 无量纲内容稳定帧标识（:1030，DATA-FRAME-ID-001） | — |
| control_id | u64 | 无量纲（=cells 索引，与 P2ControlNode.control_id 一致，:1031） | — |
| leaf_ipix | u64 | NESTED leaf pixel（控制拓扑位置，h:82 注；:1032） | — |
| ra_deg/dec_deg | f64×2 | deg（cell 中心，:1033-1034；F4 门 atol 1e-9） | — |
| value | f64 | ADU（UPM-calibrated 局部光度估计，**可负**，:1035；h 注冻结） | — |
| uncertainty | f64 | ADU（=sqrt(control_variance)，:843/:1036） | — |
| snr | f64 | 无量纲（局部 catalogue SNR 中位或回退整帧精确中位 ：860-861/:1037） | snr_available=0 时整帧回退值（不以 1.0 伪装，upm.h:51-55） |
| ivar | f64 | 1/ADU²（**弃用仅诊断**：单 leaf Phase1 ivar ≠ Var(control estimator)，:1039-1054 冻结注释；ivar 产品缺失/非 finite/≤0 → 0.0 如实降级 :1046，UPM 侧回退 1/uncertainty² :580 注） | 0.0=无 ivar 产品 |
| control_variance | f64 | ADU²（k_corr×(π/2)×σ_bg²/N_retained，:840-842 冻结；ALG-UPM-CONTROL-IVAR-001） | cvar≤0 不可达（σ floor 1e-12 :818/:829） |
| control_ivar | f64 | 1/ADU²（=1/cvar，cvar≤0 → 0 如实降级 :842；:1042） | 0.0=方差未定义 |
| snr_available | int | 0/1（1=局部邻域有 catalogue 星点 ：859；0=无，回退整帧中位；:1038） | — |
| support | f64 | 无量纲 [0,1]（patch 内有效支撑比，sup_sum/n_valid :844；:1057） | — |
| quality_flags | u32 | 位集（现状 control 级，:1058） | — |

**(2) P2SampleStats 10 字段 u64**（sampler.h:63-74；诊断计数，含
DISP-P2SMP-002 双计数现状口径——§23.3）: candidate_observations
（几何×覆盖帧）、accepted_observations（进入 UPM 的 clean 观测
≥2 clean 帧 :1013-1027）、rejected_insufficient_support、
rejected_insufficient_retained（**现状双计数** ：1006+:1022）、
rejected_bright_tolerance、rejected_high_contamination、
rejected_catalog_veto、rejected_lt_two_clean_frames、
accepted_controls（≥1 clean obs）、overlap_controls（≥2）。

**(3) P2ControlNode 7 字段**（sampler.h:77-83）: control_id u64 /
tile_ipix u64 / gx,gy int / ra_deg,dec_deg f64（deg）/ leaf_ipix
u64。out_n_controls = n_union×G² **全几何节点含空覆盖占位**
（sampler.h:118-119 冻结；与 accepted/overlap_controls 区分，
日志并列表述）。

### 23.3 观测 accept/reason 语义（本域无状态机；reason u8 权威值域）

| reason | 值 | 语义 | 锚（sampler.cpp） |
|---|---|---|---|
| 0 | accepted | 通过 Stage A-E 全部门（§5.4/§5.5） | :866 |
| 1 | insufficient | patch 有效样本 < min_samples / support·finite 不足 | :759/:768/:799 |
| 2 | insufficient_retained | clipping 后保留比例 < 0.60（第二遍 :1005；第三遍对同 reason 帧重复 ++rejected_insufficient_retained=DISP-P2SMP-002） | :1005/:1022 |
| 3 | bright_tolerance | 超局部 tolerance（Stage C） | :995-996 |
| 4 | high_contamination | 亮像素占比超限（Stage D） | :999-1000 |
| 5 | catalog_veto | Stage E SNR catalogue veto | :866 |

### 23.4 单位/dtype/确定性

- 全浮点 IEEE f64（f32 tile 源读取提升 :372-378/:1051；无 long
  double）。量纲分面: value/uncertainty=ADU、control_variance=ADU²、
  ivar/control_ivar=1/ADU²、snr/support/quality=无量纲、
  ra/dec=deg——ivar（1/ADU²）与 control_ivar（1/ADU²）数值域同、
  语义域不同（诊断 vs 科学权重），禁止互换（upm.h:38-41 冻结）；
  weights/supply 语义红线 §20.3 在本域镜像（control_ivar 是唯一
  科学权重源，value 为 patch median 非单像素）。
- 确定性=fixed_reduction_order: 固定槽位写回 cells[idx]（:870-872）
  + 第三遍单线程顺序扫描 → **输出 obs 序列 bitwise 与 worker 数
  无关**（1/N 等价；F8 门 sampler_parallel_consistency_test.cpp:29）；
  同输入同 cfg 同 fid → obs bitwise 确定（median_of 定序、
  clipping 收敛阈值确定、k_corr 逐帧查表确定）。

### 23.5 错误/边界

- rc 语义: rc=0 成功（含空 obs——空覆盖 union 合法）；rc=1 + err
  8KB 文本: bad args（:476-479）、frame_id 0（:512-523）、open
  failed（:536-546）、n_union>1e6（:634-638）、cells>2e8
  （:644-648/:1071-1077）、resize OOM（:649-654）、首 tile 越界
  （:656-669）、pairs resize（lambda :721；并行 err :909-912；串行
  err :919-923）、exception 兜底（:1089-1097；MSVC /EHa SEH
  :936-944）。容量不足不报错（probe/fill，§23.1(4)）。
- 降级路径（如实，不静默伪装）: ivar 产品缺失 → o.ivar=0.0
  （:1046）；catalogue 缺失 → snr_available=0 + snr=整帧精确中位
  （:860-861，:623 frame_snr_med_exact）；σ_bg=0 → 1e-12 floor
  （:818/:829）；空/全 NaN patch → reason=1 拒绝（:793-800）。
- 并发/重入: g_aio_mu 锁仅覆盖 read_tile_pair（:161/:166）；并行
  路径 per-worker 独立 AIO 句柄（:894）无共享可变全局态，
  reentrant yes；无取消检查点（ThreadLease 接线归 P2-SAMP-IMPL，
  与 DISP-COV-005 同构）。
- 缺陷登记（不改码）: DISP-P2SMP-001（cfg `<=0→默认` 吞显式 0，
  :485-502）；DISP-P2SMP-002（insufficient_retained 双计数
  :1006+:1022，统计面偏差）；DISP-P2SMP-003（17 处 stderr 诊断
  直写）；DISP-P2SMP-004（veto 阈值 10×frame_snr_med 与半径
  0.012° 硬编码 :849-850）；DISP-P2SMP-005（收敛阈值 1e-12 在
  m0≈0 退化全迭代 :818）。

### 23.6 交叉引用

- 上游: SCI-UPM-001（docs/science/PHASE2_UPM.md，FROZEN，零改动）；
  ALG-P2-SMP-001（docs/algorithms/PHASE2_SAMPLER.md，本域算法权威；
  SCI-P2-SMP-001⇒SCI-UPM-001 映射=ALG §11.4）；DATA-P2-COV
  （coverage union 输入，P2-COV 域）；DATA-FRAME-ID-001
  （frame_id 身份，§22 前文冻结）。
- 下游: API-P2-SMP-001（PUBLIC_API.md）+ API-P2-001（编排级，
  FROZEN）；P2-UPM 域消费 P2ControlObservation/control_variance
  （upm.h:31-57 同构，ALG-UPM-CONTROL-IVAR-001）；TEST-P2-SMP-001
  （MISSING，P2-SAMP-DOC 登记，P2-SAMP-TEST 落地+EVIDENCE）。
- 同文档: §20（编排域 stage2 编排消费 sccfg 透传）、§21（下游
  积分）、§22（rejection 域先行例）。
- 端口词汇注记: registry descriptor p2_sample_descriptor
  （module_adapters.cpp:642-654，module_id=astrocs.phase2.sample
  占位）端口表 coverage→samples 为编排层词汇，由 P2-XX-INT 对齐
  astrocs.p2.sampling，不得反向作为冻结依据。

## 24. Phase2 装配会话（lib/phase2_session）数据语义（DATA-P2-SESSION）

> ID: DATA-P2-SESSION  状态: CONTRACT_READY（P2-SESSION-DOC 冻结，
> 2026-09-10，SA-P2-X24）
> 模块: lib/phase2_session/p2_session.cpp（282 行）+ 唯一权威签名头
> lib/phase2_session/p2_session.h（39 行）（astrocs.p2.session；构建=
> 静态库 astrocs_phase2_session，根 CMakeLists.txt:454-458；迁移目标
> astrocs_p2_session.dll 为矩阵合同值，尚未存在——MISSING 如实登记，
> 由 P2-SESSION-IMPL 建立，禁止声明 IMPLEMENTED）。本节是 Phase2 装配
> 会话 config/manifest/错误码/各段数据面单位与透传口径的唯一权威；
> 本域为纯编排透传层（不实现科学公式，直调 lib/phase2 生产符号），
> SCI 上游零改动: SCI-UPM-001 / SCI-INT-001 / SCI-REJ-001
> （docs/science/，FROZEN）；ALG: ALG-P2-SESSION-001
> （docs/algorithms/PHASE2_SESSION.md，逐符号锚与调用序）；API 面:
> API-P2-SESSION-001（PUBLIC_API.md「Phase2 装配会话 C API」节）+
> 编排上游 API-P2-001（docs/api/PHASE2_API_V1.md，FROZEN，引用不改动）。

### 24.1 config JSON 键集（p2_session.cpp:69-98 validate 实测；run 消费 :100-248）

config 为单对象 JSON 文档（acs_span_u8 传入，非 object → rc（:80））。
validate 为纯读无 IO（p2_session.h:19），可幂等重复调用；run 不内嵌
validate 全套检查（仅 parse 兜底 :107-109，提示 "validate first"），
调用方契约=先 validate 后 run。

| 键 | 类型 | 必填 | 默认 | 校验规则（违规→ACS_ERR_PARAM，锚=p2_session.cpp） |
|---|---|---|---|---|
| hips_paths | array[string] | 是 | — | 缺失→"missing key 'hips_paths'"（:81-85）；非 array 或空 array→（:86-88）；元素非 string→（:91-92）；run 逐项 get\<string\>（:112）经 AIO 直读 HiPS 产品（§23.1(2) 同源） |
| output_dir | string | 是 | — | 缺失→"missing key 'output_dir'"（:81-85）；非 string→（:86-88）；**run 现状不消费该键**（validate 必填、run 无读取——诊断面词汇占位，如实登记不改码） |
| upm | object | 否 | 空对象语义（缺省=全冻结默认，§24.4(3)） | 非 object→（:93-96）；validate **不校验子键**——run 消费 max_iterations(int)/huber_delta(double)/smoothing_lambda(double)（:196-202），类型不符 get\<\> 抛异常的现状=未捕获路径（登记，见 ALG-P2-SESSION-001 DISP 清单） |
| persist_upm | bool | 否 | false（doc.value 默认） | validate 不校验；run :222 消费；类型不符 get\<bool\> 同上未捕获现状 |
| upm_save_path | string | 否 | — | validate 不校验；persist 触发=persist_upm 为真 **且** contains upm_save_path（:222）；run :229 get\<string\> |

未知键语义（权威=实现实测，非头注释）: **validate 现状不拒绝未知键**
——p2_session.h:19 注释声明"拒未知键/缺必需键"，实现 :69-98 仅拒缺
必需键 + 类型错（无未知键遍历）；注释-实现漂移，禁止按头注释宣称
unknown-key 拒绝语义，整改（补拒绝或改注释）归 P2-SESSION-IMPL
（编号见 ALG-P2-SESSION-001 DISP 清单）。缺必需键锚=:81-85（无
silent default，两键逐名报错）。

### 24.2 manifest JSON 字段（inspect 输出，p2_session.cpp:250-266 实测）

manifest 为 nlohmann::json 对象（create 初始化 :64），inspect 以
dump(2) 拷出（:258-265，经 host->allocator.alloc(n,16) 分配，释放
责任=同一 host allocator，common_abi_v1.h:74 合同头；alloc 失败→
ACS_ERR_NOMEM :261）。顶层字段:

| 字段 | 类型 | 值/语义 | 锚 |
|---|---|---|---|
| kind | string | 常量 "astrocs_phase2_session" | :64 |
| stages | array[object] | 段记录追加序 coverage→sample→upm_build→persist（persist 可选缺席）；每项 {name, status, ...extra}，status ∈ running/ok/fail/cancelled | :36-40/:121-147/:153-178/:182-219/:228-239 |
| status | string | created（未 run 且无错）→ complete（run 成功）/ failed（run 过但 last_error 非空，inspect 时补写） | :253-256/:245 |
| error | string | 仅 failed 时：last_error 脱敏摘要 | :256 |
| error_kind | string | input（生产 rc 映射，map_rc 统一标注 :46）/ output（persist 落盘失败 :234） | :46/:234 |
| n_inputs | int | coverage 输入 HiPS 数（run 成功后写入） | :243 |
| n_obs | u64 | 采样观测数 | :244 |
| artifacts | array[string] | persist 成功产物路径（upm_save_path） | :237-238 |

段内 extra 键（逐段实测）: coverage: rc/n_inputs/n_union_cells/
target_order（:127/:140/:145-147）；sample: rc/err/n_obs/n_controls/
accepted_obs/overlap_controls（:170/:174-176）；upm_build: rc/
control_count/observation_count/component_count/target_order/
model_hash（P2ModelInfo 可用时 :211-215，info 不可用仅 status=ok
:218）；persist: path（ok :239；fail 无 rc :232）。

manifest 状态机: created →（run 各段 running→ok/fail/cancelled）→
complete / failed。现状 run 重入不清空 manifest（stages 累积追加、
n_inputs/n_obs/status 覆盖写），幂等语义未冻结（如实登记）。

### 24.3 ACS_ERR_* 错误码语义表（map_rc p2_session.cpp:43-50 + 逐函数实测）

| 错误码 | 触发 | 锚（p2_session.cpp） |
|---|---|---|
| ACS_OK | run 全段 ok / validate 通过 / 生命周期原语成功 | :97/:247 |
| ACS_ERR_ABI_MISMATCH | create: host null ∨ struct_size≠sizeof(astrocs_host_services_v1) ∨ abi_version≠ACS_ABI_VERSION_V1 | :57-59 |
| ACS_ERR_PARAM | create out null（:60）；validate/run 句柄 null 或 config span null/空（:71/:102）；JSON parse 失败（:76-78/:107-109）；非 object（:80）；缺必需键/类型错（:81-96）；inspect out null（:252）；destroy 句柄 null（:270） | :60/:71-96/:102/:107-109/:252/:270 |
| ACS_ERR_STATE | 生产函数 rc=2（显式 build fail——生产路径缺 ivar 等，合同 §4 映射） | :48 |
| ACS_ERR_IO | persist 段 p2_upm_save 失败（error_kind=output，model 先 close 防泄漏） | :230-235 |
| ACS_ERR_INTERNAL | 生产函数其余 rc（map_rc 兜底；error_kind=input :46） | :49/:43-50 |
| ACS_ERR_NOMEM | create SessionState 分配失败（:62）；inspect manifest 拷贝分配失败（:261） | :62/:261 |
| ACS_ERR_CANCELLED | 各阶段边界取消检查命中（§24.5） | :120/:152/:181/:223-227 |

错误摘要通道: C++ 诊断面 astrocs::phase2::last_error（p2_session.h:35，
:278-281）返回最近一次 "what rc=N" 文本（:45）；C ABI 面错误语义仅经
manifest.error/error_kind 承载（:256/:46）。

### 24.4 各段数据面（透传口径；会话不复制科学结构，唯一权威=各域节）

**(1) coverage 段**（:119-148，两遍协议）: P2CoverageResult（coverage.h，
UNIT=ADU/tile 口径透传，唯一权威=DATA-COV-001 §19）——首遍 inputs=null
仅查询 union 容量（:124-128），回填 P2HipsInputInfo 后第二遍取
union_cells/target_order（:130-142）；cov_guard 经 p2_coverage_free
释放（:143-144）；产物仅以 manifest 计数/词汇登记（n_inputs/
n_union_cells/target_order），MOC 数据本体不出会话边界。

**(2) sample 段**（:150-178，probe/fill 两遍）: P2ControlObservation
13 字段（upm.h:31-57）——**引用 §23.2(1) 不复制**（value=ADU/
uncertainty=ADU/control_variance=ADU²/control_ivar=1/ADU² 等）；cfg=
p2_sampler_default_config() + sc.cpu_workers=host->budget.max_workers
（:154-155）；probe 遍 :158-163（obs/nodes 容量查询），fill 遍
:167-168；stats 诊断计数（accepted_obs/overlap_controls 入 manifest
:174-176）口径=§23.2(2)。

**(3) upm_build 段**（:180-219）: 输入 obs（§23.2(1) 结构）→
P2UpmBuildConfig 冻结默认 + upm 子键覆盖 → void* model（不透明句柄，
会话持有，p2_upm_close 唯一释放 :241）。冻结默认（:183-195）:

| P2UpmBuildConfig 字段 | 会话冻结值 | 锚 |
|---|---|---|
| robust_loss | 0（huber） | :184 |
| snr_weight_mode | 0（snr2_normalized） | :185 |
| huber_delta | 1.345（upm.huber_delta 可覆盖） | :186/:199-200 |
| max_iterations | 100（upm.max_iterations 可覆盖） | :187/:197-198 |
| tolerance | 1e-6（无 config 覆盖键） | :188 |
| target_order | =cov.target_order（coverage 实测值透传） | :189 |
| sigma_floor | 1e-3 | :191 |
| support_power | 1.0 | :192 |
| use_ivar_weight | 1 | :193 |
| control_reliability | 1.0 | :194 |
| cpu_workers | =host->budget.max_workers（blocks(budget)，禁硬编码） | :195 |

smoothing_lambda 仅经 config 覆盖（:201-202，缺省=P2UpmBuildConfig
零值现状）。模型信息仅以 P2ModelInfo 五字段入 manifest（:209-216），
模型本体/系数数组不出会话边界（唯一权威=SCI-UPM-001/ALG-UPM 域）。

**(4) persist 段**（:221-240，可选）: p2_upm_save(model, upm_save_path)
单文件直写（:230）；**单文件直写无原子发布边界——§12.5 语义（原子性/
tree hash/COMPLETE 状态）在本段不适用，如实现状态登记**（无校验和、
无临时文件+rename）；成功路径 artifacts 追加 :237-238。本段不产 HiPS
产品集（HiPS 写出唯一权威=DATA-P1-HIPS §12 / DATA-P2-HIPS §20 编排
域）。

### 24.5 取消语义与并发（p2_session.h:16/:22 注释锚 + 实测）

- 取消=**阶段边界检查点**（4 处，p2_session.h:22 注释冻结）: coverage
  段前（:120）/ sample 段前（:152）/ upm_build 段前（:181）/ persist
  段前（:223-227）。检查=宿主 cancel.is_cancelled 轮询（:32-35，
  common_abi_v1.h:91-97 单向置位原子读）。命中→该段 status=cancelled
  + 返回 ACS_ERR_CANCELLED；**upm 整模型不写半成品**——upm_build 段内
  无取消检查（取消只能整段前后），persist 取消先 p2_upm_close 防泄漏
  （:224）。
- 并发合同（p2_session.h:16 注释锚）: **threadsafe:no（handle 级）**
  ——同一 acs_handle 的并发调用未受保护，禁止；**reentrant:yes**——
  不同 handle 并发合法（SessionState 全实例态，无共享可变全局）。
- 内部并行**仅 UPM blocks**（预算驱动）: sample/upm 段 cpu_workers=
  host->budget.max_workers（:155/:195，Runtime lease 唯一来源，ARCH-004
  禁硬编码）；coverage/persist 段串行（persist 显式"串行 IO" :221）。
  预算绑定差异登记: p2_session.h:4 头注释 "sampler=1(串行 reference)"
  vs 实现 :155 sampler 同样透传 budget.max_workers（N-worker）——行号
  权威=cpp 实测，注释漂移整改归 P2-SESSION-IMPL（见 ALG-P2-SESSION-001
  DISP 清单）。
- host services 四通道（common_abi_v1.h:110-117 实测）: allocator
  （inspect manifest 分配 :260）/ logger（ACS_LOG_INFO "phase2" 预算与
  段日志 :28-31/:115-117/:148/:177-178）/ cancel（§24.5 取消）/
  budget（max_workers 注入 :155/:195 + available_cpus 日志 :116-117）。

### 24.6 交叉引用

- 上游: SCI-UPM-001 / SCI-INT-001 / SCI-REJ-001（docs/science/，共享
  FROZEN，零改动——本域纯透传不触碰科学公式）；ALG-P2-SESSION-001
  （docs/algorithms/PHASE2_SESSION.md，四段调用序逐源码行号锚）。
- 下游: API-P2-SESSION-001（PUBLIC_API.md，五符号展开冻结）+ 编排上游
  API-P2-001（docs/api/PHASE2_API_V1.md，FROZEN，引用不改动）；
  TEST-P2-SESSION-001（MISSING，P2-SESSION-DOC 登记，落地归
  P2-SESSION-TEST，设计冻结面=ALG-P2-SESSION-001 TEST-DESIGN）。
- 同文档: §16（P1 装配会话先例，DATA-P1-SESSION——created→complete/
  failed 状态机与 host services 四通道同构）；§19（DATA-COV-001，
  coverage 段输入唯一权威）；§23（DATA-P2-SMP，P2ControlObservation/
  P2SamplerConfig/P2SampleStats 唯一权威）；§12.5（发布/IO-003 对齐
  边界——persist 段不适用，§24.4(4)）；DATA-FRAME-ID-001（frame_id
  身份，sample 段经 §23 透传）。
- 端口词汇注记: registry descriptor 现无 astrocs.p2.session 占位——
  lib/core/src/module_adapters.cpp:23-26 仅为 RT-005 IModule 工厂声明
  五 C ABI（p2_session_create/validate/run/inspect/destroy）；词汇
  astrocs.p2.session 由 P2-XX-INT 对齐登记，不作冻结依据。

## 25. Phase2 UPM fit（lib/phase2）模块输入/输出数据（DATA-P2-UPM）

> ID: DATA-P2-UPM  状态: CONTRACT_READY（P2-UPM-DOC 冻结，2026-09-10）
> 模块: lib/phase2/src/upm.cpp（1565 行）+ 唯一权威签名头
> lib/phase2/include/astro/phase2/upm.h（184 行）（astrocs.p2.upm-fit；
> astrocs_phase2 静态库成员，根 CMakeLists.txt:337-346/:338；迁移目标
> astrocs_p2_upm.dll 为矩阵合同值，尚未存在，由 P2-UPM-IMPL 建立，
> 禁止声明 IMPLEMENTED）。本节是 Phase2 UPM fit（联合拟合/持久化/
> 求值底座）in/out 单位/dtype/shape/invalid 的唯一权威；SCI 上游:
> SCI-UPM-001（docs/science/PHASE2_UPM.md，FROZEN，零改动；单位权威=
> 其 §3）；ALG: ALG-P2-UPM-IMPL-001（docs/algorithms/PHASE2_UPM_IMPL.md，
> 逐符号锚）；API 面: API-P2-UPM-001（PUBLIC_API.md）；
> descriptor 占位 module_id=astrocs.phase2.upm-fit
> （module_adapters.cpp:661-678）由 P2-XX-INT 对齐。

### 25.1 输入（锚=upm.h/upm.cpp；未注文件者同）

**(1) P2ControlObservation[]**（const P2ControlObservation* obs,
n_obs；upm.h:31-57）: 上游 DATA-P2-SMP 产物（p2_upm_build 入口），
13 字段单位/dtype/invalid 唯一权威=§23.2(1)（**引用不复制**）。
本域科学权重唯一来源=control_ivar（=1/control_variance，production
use_ivar_weight=1，upm.h:43-50/:83-86 冻结；ALG-UPM-CONTROL-IVAR-001）:
control_ivar≤0/非有限 → p2_upm_raw_weight rc=2 显式失败（:1311，
h:126-133 "禁止静默回退 support/SNR"；权重语义红线 §23.4 镜像）。
value 可负（局部 patch 光度估计，§23.2(1)）；uncertainty=sqrt(
control_variance)（h:38）；ivar 字段弃用仅诊断（h:40-42），UPM 域
不消费（production 权重路径无 ivar 引用，:1292-1317）。

**(2) P2ControlNode[]**（仅 p2_upm_build_geo；upm.h:99-106）: coverage
union 全几何节点 7 字段（sampler.h:77-83，§23.2(3) 引用不复制；含
单帧区空覆盖占位，n_nodes=n_union×G² 口径）；obs 只含 ≥2 clean 帧
观测，单帧区节点无数据项，C 由全局平滑/Laplacian 延拓得到（harmonic
continuation，upm.h:99-101 冻结注释；SCI-UPM-001 §4）。stage2 生产
消费=lib/phase2/tools/stage2.cpp:432-434 p2_upm_build_geo。

**(3) cfg**（P2UpmBuildConfig，upm.h:71-92，15 字段；默认单一来源=
upm.cpp:222-236（cfg==nullptr 分支逐字段填充）；非法数值经 :237-245
修补回退冻结默认；**target_order=-1（auto）不可达——:246-249 显式
rc=1**（空间 UPM 必须知 control leaf 层级 order=target+9，:261/:380；
生产由 p2_session 取 coverage 实测值透传，p2_session.cpp:189））:

| 字段 | dtype | 默认 | 单位/语义 |
|---|---|---|---|
| robust_loss | int | 0 | 0=huber（首版冻结，h:72） |
| snr_weight_mode | int | 0 | 0=snr2_normalized（首版，h:73） |
| huber_delta | double | 1.345 | Huber delta（h:74；≤0→1.345 :237） |
| smoothing_lambda | double | 0.0 | 图平滑权重（默认关，h:75；<0→0.0 :245） |
| zero_anchor_weight | double | 1e-3 | 弱零校正锚权重（h:76；<0→1e-3 :244） |
| max_iterations | int | 100 | IRLS 最大迭代（h:77；≤0→100 :238） |
| tolerance | double | 1e-6 | 收敛容差（h:78） |
| target_order | int | -1(auto) | 模型目标 order；auto 必须显式，否则 rc=1（h:79；:246-249） |
| sigma_floor | double | 1e-3 | uncertainty 下限（h:80；≤0→1e-3 :239） |
| support_power | double | 1.0 | support 因子指数（h:81；<0→1.0 :240） |
| quality_mode | int | 0 | 0=flags 映射（h:82） |
| use_ivar_weight | int | 1 | 1=science weight 用 control_ivar（h:83-86；production control_ivar≤0/非有限→显式 INVALID，:1307-1311）；0=legacy 仅 ablation/诊断（SNR-015） |
| control_reliability | double | 1.0 | 默认 control reliability（h:87；≤0→1.0 :243） |
| input_manifest_hash | const char* | nullptr | 输入稳定 manifest（h:88；可空；非空参与模型 hash，:253-254） |
| cpu_workers | int | 1 | CON-005 worker 数，Runtime lease 唯一来源（h:89-91 注释漂移=DISP-P2UPM-002；生产=p2_session.cpp:195 budget.max_workers；0/负→1 :515/:615/:1478） |

**(4) 内存/规模**: C 校正矩阵=逐帧稀疏行（n_frame×n_control 结构
口径，规模 O(n_ctrl + n_frame·n_ctrl)——结构推断口径，代码/文档
无显式行级出处，如实登记）+ obs/nodes 数组线性 O(n_obs+n_nodes)；
dense 物化分块内存上界=kChunk(16)×kLeafPx×8 字节（:1386/:1407 实测
注释）。

### 25.2 输出/数据合同

**(1) UPM 模型对象**（void* 不透明句柄，p2_upm_build :929-932 /
p2_upm_build_geo :934-937 唯一产出，p2_upm_close :1559 唯一释放）:
模型本体不出 ABI 边界；对外溯源面=P2ModelInfo 七字段（upm.h:60-68，
经 p2_upm_info :1233 只读导出）:

| 字段 | dtype | 值/语义 | 锚 |
|---|---|---|---|
| version | u32 | 2（空间 UPM） | h:61；upm.cpp:255 |
| precision | u32 | 1=fp64（本域唯一权威 dtype） | h:62；upm.cpp:256 |
| target_order | u32 | 模型目标 order（=cfg 显式实测值） | h:63；:258 |
| control_count | u64 | 控制点数 | h:64 |
| observation_count | u64 | 观测数（=n_obs） | h:65；:257 |
| component_count | u32 | 连通分量数 | h:66 |
| model_hash | char[65] | 模型内容 SHA-256 hex（NUL 止） | h:67 |

**(2) upm_model 端口产物**（descriptor 词汇）: build/persist 链产物
=astrocs-upm-v2 单文件（§25.5(1)，唯一 AIO aio_upm_write_sparse）+
可选 dense cache（§25.5(2)）；runtime 消费面=calibrate_block/
evaluate_c/dense_read_block（apply 域=§26）。端口 samples→upm_model
为编排层词汇（module_adapters.cpp:669-670），由 P2-XX-INT 对齐，
不作冻结依据。

### 25.3 单位/dtype/确定性

- 唯一权威 dtype=FP64（precision=1，upm.cpp:256 "fp64 reference"）；
  dense cache 亦 fp64（:1402 写 1 /* fp64 缓存 */）；模型 JSON/AIO
  文本同值。
- 单位权威=SCI-UPM-001 §3（docs/science/PHASE2_UPM.md:29 实测
  "C, raw, calibrated, σ_bg: ADU"）: 校正场 C/参数 M/uncertainty/
  σ_bg=ADU、control_variance=ADU²、control_ivar=1/ADU²、
  snr/support/quality 无量纲、frame_id 无量纲 u64、M=latent unified
  reference（内部待求量，upm.cpp:56）。**控制包模板"校正=mag"表述
  与 SCI §3 实测不符——以 ADU 为冻结口径，mag 表述不作冻结依据**
  （偏差登记，口径修正归上游）。
- leaf_ipix 几何链: NESTED leaf 像素，tile=leaf_ipix>>18（tile_shift=9
  :261/:1254；leaf order=target+9，:380 nside=2^(target+9)）。
- 确定性=determinism class D1（:513-514 注释冻结"worker 数无关，
  同一 worker 数下位精确"）: worker-local tsums + 按 worker 顺序
  （与 OpenMP tid 升序语义一致）归并（:513-534）；raw weights 逐
  obs 独立写不相交（:613-633）；dense 物化=分批并行求值+块内
  (f,tile) 单调序串行写 → 稠密缓存 bit-identical（:1383-1386，
  kChunk=16 :1407）。全文件无 #pragma omp（std::thread 池实现；
  upm.h:89-91 注释 OpenMP 措辞漂移=DISP-P2UPM-002）。
- 并行 worker 数唯一来源=cfg.cpu_workers（Runtime lease；p2_session.cpp
  :195 预算驱动禁硬编码，§24.5 同构）；materialize_dense_n workers≤0
  →1（:1478；upm.h:176 注释 auto=omp_get_max_threads 与实现漂移，
  如实登记不改码）。

### 25.4 错误/边界

- rc 语义（16 符号逐条展开=API-P2-UPM-001 节）: rc=0 ok；rc=1=
  参数/IO/未知 frame_id/open format 校验失败（:1027-1028）；rc=2=
  production 缺 control_ivar（raw_weight :1311，build 链经
  raw_weight 传播，SCI-UPM-001 §4 "p2_upm_raw_weight rc=2 → build
  rc=2"）+ dense_read stale-cache（source hash 不匹配，
  aio_upm.cpp:469）。
- invalid/负向条款（如实，不静默伪装）: **未知 frame_id 双门**——
  calibrate_block rc=1 显式失败（:1250-1252 注释"未知 frame_id 必须
  显式失败，禁止回退 frame 0 参数（错误帧校准会静默制造错误科学
  结果）"）/ evaluate_c 返回 NaN 显式不可用（:1276-1279"禁止用
  frame 0 参数伪装有效结果"）——**禁回退红线，任何静默替换=违约**；
  NaN uncertainty → sigma_eff=max(|unc|, sigma_floor) 降权路径
  （:628/:646/:868；sigma_floor 冻结 1e-3）；control_ivar≤0/非有限
  → rc=2 显式 INVALID（production 禁静默回退，h:85）。
- save 绑定守卫: frame_id_by_index 与 C 行数不一致 → rc=1 拒写盘
  （:943-945，ALG-UPM-FRAME-BIND-001）。

### 25.5 持久化产物

**(1) sparse 模型文件 astrocs-upm-v2**（p2_upm_save :940-1006 单文件
JSON；唯一 AIO 出口 aio_upm_write_sparse :1003-1005，定义
aio_upm.cpp:66 原子写，ENG-IO-001；p2_upm_open :1008-…，format 校验
:1027-1028 非 astrocs-upm-v2 → rc=1）。JSON 顶层字段（:947-1002
实测）: format="astrocs-upm-v2"（:947）/version/target_order/precision/
robust_loss/snr_weight_mode/use_ivar_weight/huber_delta/
smoothing_lambda/zero_anchor_weight/max_iterations/tolerance/
sigma_floor/support_power/model_hash/input_manifest_hash/iterations/
objective/component_count/geometry_component_count/
unobserved_geometry_nodes/component_ref_frame[]/frame_component[]/
control_count/observation_count/frames[]/controls[]/cell_index[]/
C[]。controls 7 元组=[tile,gx,gy,ra,dec,M,leaf]（:980-987）；cell_index
4 元组（:988-992）；C 逐帧稀疏行 [k,v]（零值不落盘，:993-1002）。
save→close→open 保持 frame_id→θ 映射（SCI-UPM-PERSIST-001）。

**(2) dense cache astrocs-upm-dense-v2**（p2_upm_materialize_dense_n
:1390-1517 显式 workers / p2_upm_materialize_dense :1518-1521 委托
workers=0；**重复声明 upm.h:154-156 与 :173-175=DISP-P2UPM-001 登记
不改码**）: format 常量/校验=aio_upm.cpp:223（写）/:407-408（读）；
source_hash=model_hash 绑定，不匹配 → dense_read_block rc=2 stale
（:1542-1557；:1553 未知帧 rc=1）；p2_upm_dense_info :1523-1541
（稀疏=稠密 Gate 用）。materialize_dense 的目标 order 不可省
（:1394 取模型 info.target_order 兜底语义）。

### 25.6 交叉引用

- 上游: SCI-UPM-001（docs/science/PHASE2_UPM.md，FROZEN，零改动；
  单位权威=§3 :29，连续定义 §5）；ALG-P2-UPM-IMPL-001
  （docs/algorithms/PHASE2_UPM_IMPL.md，本域算法权威）；ALG-UPM-001
  （UPM_SOLVER.md，Huber IRLS 求解权威）；ALG-UPM-CONTROL-IVAR-001
  （control_variance/control_ivar 冻结公式）；DATA-P2-SMP（§23，
  P2ControlObservation/P2ControlNode 唯一权威）。
- 下游: API-P2-UPM-001（PUBLIC_API.md，16 导出符号展开冻结）；§26
  （DATA-P2-COR，apply 域消费 upm_model）；DATA-P2-SESSION（§24，
  upm_build/persist 段透传口径）；TEST-P2-UPM-001/002（MISSING，
  P2-UPM-DOC 登记，执行测试归 P2-UPM-TEST，设计冻结面=
  ALG-P2-UPM-IMPL-001 TEST-DESIGN）。
- 同文档: §23（输入观测上游）、§24（会话编排消费本域）、§22 前文
  权重语义红线（control_ivar 唯一科学权重源）。
- 端口词汇注记: registry descriptor p2_upm_fit_descriptor
  （module_adapters.cpp:661-678，module_id=astrocs.phase2.upm-fit
  占位）端口表 samples→upm_model 为编排层词汇，由 P2-XX-INT 对齐
  astrocs.p2.upm-fit，不得反向作为冻结依据。

## 26. Phase2 UPM apply（lib/phase2）模块输入/输出数据（DATA-P2-COR）

> ID: DATA-P2-COR  状态: CONTRACT_READY（P2-UPM-DOC 冻结，2026-09-10）
> 模块: lib/phase2/src/upm.cpp（1565 行）+ 唯一权威签名头
> lib/phase2/include/astro/phase2/upm.h（184 行，calibrate/evaluate/
> dense_read 面 =upm.h:112-123/:164-172）（astrocs.p2.upm-apply；
> astrocs_phase2 静态库成员，根 CMakeLists.txt:337-346/:338；迁移
> 目标 astrocs_p2_upm.dll 为矩阵合同值，尚未存在，由 P2-UPM-IMPL
> 建立，禁止声明 IMPLEMENTED）。本节是 Phase2 UPM apply（逐帧校正）
> in/out 单位/dtype/shape/invalid 的唯一权威；SCI 上游: SCI-UPM-001
> §5 连续定义（calibrated_f(p) = raw_f(p) − C_f(p)，FROZEN，零改动）；
> ALG: ALG-P2-UPM-IMPL-001；API 面: API-P2-UPM-001；descriptor 占位
> module_id=astrocs.phase2.upm-apply（module_adapters.cpp:680-696）
> 由 P2-XX-INT 对齐。

### 26.1 输入

**(1) upm_model**（const void* 不透明句柄）: 二源同构——内存态
（p2_upm_build/build_geo 产物，§25.2(1)）或 reload 态（p2_upm_open
:1008 读入的 astrocs-upm-v2 模型，§25.5(1)）；dense 加速路径=
p2_upm_dense_read_block 读 astrocs-upm-dense-v2 cache（§25.5(2)，
source_hash=model_hash stale 判定）。sparse 与 dense 同一科学语义
（upm.h:121/:164 冻结注释）；模型身份锚=model_hash（P2ModelInfo
char[65] SHA-256，h:67）。

**(2) calibrated_frames**（DATA-P2-CAL 域，descriptor 端口词汇
module_adapters.cpp:689）: 逐帧 signal f64 数组 input_signal[count]
+ frame_id（u64，模型 frames[] 绑定成员，DATA-FRAME-ID-001 身份）+
leaf_ipix[count]（NESTED leaf 像素，tile=leaf>>18，tile_shift=9，
:1254/:1258）。dtype=FP64；单位=ADU（§25.3 口径）。生产消费链=
lib/phase2/tools/stage2.cpp（p2_upm_build_geo :432-434 →
p2_upm_calibrate_block :927-930/:1272-1275 逐 chunk 校准，f32 tile
源读入提升 f64 → 校正 → f32 回写 :920-935）。

### 26.2 输出（corrected）

**corrected**（DATA-P2-COR）: output_signal[count]，逐点
corrected[i] = input_signal[i] − C(frame_id, leaf_ipix[i])
（p2_upm_calibrate_block :1240-1269 块协议，逐点写回 :1266）；C 由
8×8 control cell 内双线性插值求值（evaluate_c_field，:1263 注释
"双线性空间校正场求值（cell 内随位置连续）"；SCI-UPM-001 §5
冻结公式 C_f(p) = 双线性(8×8 control cell, θ_f)）。dtype=FP64 出入
一致；单位=ADU。等价面: p2_upm_evaluate_c 单点求值 C（sparse/dense
同一科学语义，h:121-123）；dense_read_block 与 sparse calibrate_block
数值等价（h:164 冻结注释）。

### 26.3 错误/边界

- rc 语义: rc=0 ok；rc=1=参数错误（model/leaf_ipix/input_signal/
  output_signal null，:1245-1248）或**未知 frame_id 显式失败**
  （:1250-1252，§25.4 禁回退红线镜像）；evaluate_c 面未知帧=NaN
  显式不可用（:1276-1279）；dense_read_block 未知帧 rc=1（:1553）。
- dense stale 语义: source_hash（=model_hash）不匹配 → rc=2
  stale-cache（aio_upm.cpp:469），禁止陈旧缓存静默出数；调用方处置
  =重新 materialize_dense_n 后重试（本域不自动切换 sparse）。
- 单位/帧绑定漂移禁止: save→close→open 保持 frame_id→θ 映射
  （SCI-UPM-PERSIST-001/ALG-UPM-FRAME-BIND-001，§25.5(1)）；save 前
  绑定守卫=§25.4。会话编排（p2_session）不直接消费 apply 面——
  四段编排止于 persist（§24.4），apply 消费链=stage2.cpp（生产）。
- 取消语义: calibrate/dense_read 面无取消检查点（取消=会话层阶段
  边界，§24.5 同构）。

### 26.4 交叉引用

- 上游: SCI-UPM-001 §5（加性校正连续定义，FROZEN 零改动）；
  ALG-P2-UPM-IMPL-001；DATA-P2-UPM（§25，模型对象/持久化/dtype
  唯一权威）；DATA-P2-CAL（calibrated_frames 输入域，descriptor
  词汇）；DATA-FRAME-ID-001（frame_id 身份）。
- 下游: API-P2-UPM-001（calibrate_block/evaluate_c/dense_read_block
  消费面冻结）；DATA-P2-SESSION（§24 编排透传）；TEST-P2-UPM-002
  （MISSING，apply 域验证，执行归 P2-UPM-TEST，设计冻结面=
  ALG-P2-UPM-IMPL-001 TEST-DESIGN）。
- 同文档: §23（观测上游）、§25（模型/持久化权威）、§24（会话域
  禁回退/取消语义同构）。
- 端口词汇注记: registry descriptor p2_upm_apply_descriptor
  （module_adapters.cpp:680-696，module_id=astrocs.phase2.upm-apply
  占位）端口表 upm_model→calibrated_frames→corrected 为编排层词汇
  （DISP-P2UPM-004 占位语义），由 P2-XX-INT 对齐
  astrocs.p2.upm-apply，不得反向作为冻结依据。

## 27. Phase3 FITS 写出（lib/phase3_fits）模块输入/输出数据（DATA-P3-FITS）

> ID: DATA-P3-FITS  状态: CONTRACT_READY（P3-FITS-DOC 冻结，2026-09-08）
> 模块: lib/phase3_session/p3_output.cpp（370 行）+ 唯一权威签名头
> lib/phase3_session/p3_output.h（64 行，write/verify 面
> =p3_output.h:45-60）（astrocs.p3.fits_writer；astrocs_phase3_session
> 静态库成员，根 CMakeLists.txt:460-465；迁移目标
> astrocs_p3_fits_writer.dll 为矩阵合同值，尚未存在，由 P3-FITS-IMPL
> 建立，禁止声明 IMPLEMENTED）。本节是 Phase3 FITS 写出域 in/out
> 单位/dtype/shape/invalid 的唯一权威；SCI 上游: SCI-P3-001 §9a-11
> G5 FITS 写公式 + §96 关键字冻结（docs/science/PHASE3_HIPS_TO_FITS.md，
> FROZEN V5 SCI-007 2026-08-28，零改动）；ALG:
> ALG-P3-FITS-IMPL-001（docs/algorithms/PHASE3_FITS_IMPL.md，实现级
> 合同，兼承接 ALG-P3-002/004 本域子面）；API 面: API-P3-FITS-001；
> descriptor 占位 module_id=astrocs.phase3.writer
> （module_adapters.cpp:445-460 p3_writer_descriptor）由 P3-FITS-INT
> 对齐 astrocs.p3.fits_writer。

### 27.1 输入

| 名称 | dtype | shape | 单位 | 语义/invalid |
|---|---|---|---|---|
| signal | float32 | [W·H]（行主序，W,H∈[1,20000]） | BUNIT（surface brightness，缺省 ADU） | 无覆盖像素=NaN（上游采样 c≠1 置 NaN，p3_session.cpp:238）；NaN 合法语义=无覆盖，禁 ±Inf 伪装 |
| coverage | float32 | [W·H] | DIMENSIONLESS（二值门 {0,1}） | covered ⇔ value>0.5f（p3_output.cpp:287/:346）；1=足迹内存在有限 tile 像素（ALG-P3-004 G5）；其它值按门归 0/1 |
| wcs | P3WcsDescriptor | 1 | deg/px（CD）、px（CRPIX） | crpix FITS 1-based pixel-center（p3_wcs.h:14）；cd FITS 顺序 CD[i][j]（:16）；projection="TAN"（:19）；abs(dec)≤85° 与四角同半球守卫（P3_WCS_PARAM/P3_WCS_HEMISPHERE，p3_wcs.h:24-26） |
| bunit | char* | 1 | — | 可空→缺省 "ADU"（p3_output.cpp:174-176） |
| prov | P3Provenance | 1 | — | 8 字段（p3_output.h:15-24）；manifest_hash 现状恒 nullptr（p3_session.cpp:270，P3-FITS-IMPL 接线，DISP 登记不改码） |
| bitpix | int | 1 | — | ∈{-32,-64}，其它值 P3_OUT_PARAM（p3_output.cpp:140-145）；session 默认 -32（:285） |
| output_path | char* | 1 | — | 发布路径；tmp 同目录（`<path>.<pid>.tmp`，:81；h:41-44 协议注形态偏差=DISP-P3FITS-002） |
| cancelled_at_row | int | 1 | — | -1=不取消（session 恒 -1 :292）；≥0 → 取消不落盘（:198-202） |

### 27.2 输出（output_path FITS 文件 + P3OutputResult）

| 名称 | dtype/形态 | 语义 |
|---|---|---|
| FITS 主 HDU | BITPIX=-32\|-64，NAXIS=2，[W,H] | signal；关键字=CTYPE1/2=RA---TAN/DEC--TAN、CUNIT1/2=deg、CRPIX1/2、CRVAL1/2、CD1_1..CD2_2（p3_output.cpp:148-169）、BSCALE=1/BZERO=0（:171-173）、BUNIT（:174-176）、HIPSID/RUNID/ORDERSEL/SAMPLER/SWVER+HISTORY（:178-189） |
| COVERAGE 扩展 HDU | 同 BITPIX，EXTNAME="COVERAGE" | coverage；DATASUM=32-bit fdatasum(signal)（:211-219；TINT 数值关键字，非 FITS 标准 ASCII CHECKSUM，如实冻结） |
| VARIANCE/IVAR 扩展 HDU（**目标态**，DATA-P3-UNC-001 §30.4） | 同 BITPIX，EXTNAME="VARIANCE"/"IVAR" | 输入 HiPS 含 variance/ivar 子产品时必写（禁静默丢弃，宪章 §7.3）；无则不写 HDU 且 manifest uncertainty_available=false；BUNIT=`<BUNIT>^2` / `1/(<BUNIT>^2)`；传播公式与 invalid 见 §30.4；实现归 P3-001，现状无此 HDU |
| result.sha256 | char[65] | 输出文件 SHA-256 hex 小写；仅完整读出后填写，失败不写空/前缀哈希（p3_output.cpp:92-114/:279-283） |
| result.coverage_ok / reopen_ok | int 0/1 | coverage 头/数据一致；独立 reader 重开回环一致（:350-354） |
| result.covered_px / total_px | long | #(coverage>0.5f)；W·H（:287-289） |

### 27.3 单位/dtype/确定性

- 单位唯一权威=本节：signal=BUNIT 串（SCI-P3 §96 按 properties，
  缺省 ADU）；coverage=二值门无量纲；WCS 角量=deg（CUNIT1/2=deg），
  CD 单位 deg/px；CRVAL=deg（ICRS）。
- dtype 唯一权威=本节：内存面 float32（TFLOAT 读写）；文件面
  BITPIX=-32/-64（调用方决定）；sha256 hex 字符；DATASUM u32 经
  TINT 写入。
- 确定性：写面单线程串行（cfitsio 进程锁 RT-008，p3_output.cpp:125），
  输出字节与 worker 数无关（1..N bitwise）；fits_write_pix 一次全帧
  行主序；sha256/fdatasum 纯函数定序。

### 27.4 错误/边界

- rc 语义（P3_OUT_OK=0/P3_OUT_PARAM=1/P3_OUT_IO=2/P3_OUT_CANCELLED=3，
  p3_output.h:34-39）与逐触发锚见 ALG-P3-FITS-IMPL-001 §10 表；
  失败/取消 → unlink(tmp/产物) 不留假文件、不发布无完整性锚输出
  （h:41-44 + IO_003 §6）。
- 发布协议冻结（R10-C）：fits_flush_file → close → fsync(fd) →
  rename（p3_output.cpp:221-273）；任何一步失败整体 IO。
- NaN 语义：signal 双方 NaN 视为一致（源无覆盖=NaN，:327-330）；
  禁止 0.0 伪装无覆盖。
- 取消：内核行粒度 cancelled_at_row；会话层取消在采样循环
  （p3_session.cpp:228-229），写面一旦进入发布序不可中断。

### 27.5 交叉引用

- 上游: SCI-P3-001（§9a-11 G5 + §96，FROZEN 零改动）；ALG-P3-002
  （G1/G2 WCS 构造，PHASE3_RESAMPLE.md）；ALG-P3-004（G5 FITS 写）；
  ALG-P3-FITS-IMPL-001（实现级合同）；DATA-P3-RES（resampled 输入
  域，descriptor 端口词汇）。
- 下游: API-P3-FITS-001（p3_output_write_atomic/p3_output_verify
  消费面冻结）；API-P3-001（会话五段编排面 FROZEN 镜像）；
  TEST-P3-WR-001（设计冻结面 TEST-P3-WR-DESIGN-001 VERIFIED=
  ALG-P3-FITS-IMPL-001 §12 + registry 手写页；可执行 MISSING 归
  P3-FITS-TEST）。
- 同文档: §3（FITS tile local-pixel 映射，读路径上游）、§4
  （signal/support/invalid 通用语义）、§26（UPM apply 域先例同构）。
- 端口词汇注记: registry descriptor p3_writer_descriptor
  （module_adapters.cpp:445-460，module_id=astrocs.phase3.writer
  占位）端口表 resampled(DATA-P3-RES 必)+fits(DATA-P3-FITS 可) 为
  编排层词汇，由 P3-FITS-INT 对齐 astrocs.p3.fits_writer，不得
  反向作为冻结依据。

## 29. Phase3 HiPS 重采样（lib/phase3_rsmp）模块输入/输出数据（DATA-P3-RES）

> ID: DATA-P3-RES  状态: CONTRACT_READY（P3-RSMP-DOC 冻结，2026-09-12）
> 模块: lib/phase3_session/p3_resample.cpp（239 行）+ 唯一权威签名头
> lib/phase3_session/p3_resample.h（58 行，本域十符号 =p3_resample.h
> 全部公共面）（astrocs.p3.resample；astrocs_phase3_session 静态库
> 成员，根 CMakeLists.txt:460-465；迁移目标 astrocs_p3_resample.dll
> 为矩阵合同值，尚未存在（DISP-P3RSMP-005），由 P3-RSMP-IMPL 建立，
> 禁止声明 IMPLEMENTED；合同落位 lib/phase3_rsmp/ 三件套）。本节是
> Phase3 重采样域 in/out 单位/dtype/shape/invalid 的唯一权威；SCI
> 上游: SCI-P3-001 §4 值语义 + §5 连续定义 + §9a-1 tile 冻结 +
> §9a-5 order 选择 + §9a-7 采样核 + §9a-8/10 输入拒绝（docs/science/
> PHASE3_HIPS_TO_FITS.md，FROZEN V5 SCI-007 2026-08-28，零改动）；
> ALG: ALG-P3-003（G3/G4 施工规格，PHASE3_RESAMPLE.md，公式零改动）
> + ALG-P3-RSMP-IMPL-001（docs/algorithms/PHASE3_RSMP_IMPL.md 实现
> 级合同）；API 面: API-P3-RSMP-001；descriptor 占位
> module_id=astrocs.phase3.resample2（module_adapters.cpp:425-439
> p3_resample2_descriptor）由 P3-RSMP-INT 对齐 astrocs.p3.resample。

### 29.1 输入

| 名称 | dtype | shape | 单位 | 语义/invalid |
|---|---|---|---|---|
| hips_dir | char* | 1 | — | HiPS 根目录；properties 严格校验（必需 keys hips_order/hips_tile_width/hips_frame/dataproduct_type，order∈[0,20]，tile_width 必须 512，NESTED 唯一，frame=ICRS——hips_properties.cpp:113-122） |
| max_order | int | 标量 | — | order 选择上限=输入实际 order（会话 clamp ≤20，p3_session.cpp:196-199；禁仅写 metadata 的 order）；<0 或 >20 → P3_RS_PARAM（p3_resample.cpp:84） |
| scale_deg_per_px | float64 | 标量 | deg/px | 必 >0（cpp:86）；G3 order 选择的唯一尺度输入 |
| input_mode（守卫面） | char* | 1 | — | `surface_brightness` 唯一合法（p3_resample_check_mode cpp:95-107）；flux/variance/weight/ivar → UNSUPPORTED（SCI §9a-8/10；会话接线缺口 DISP-P3RSMP-003）。**目标态合同（DATA-P3-UNC-001 §30.4，2026-09-09 冻结，supersession SCI-P3 §9a-10 variance/ivar 拒绝语义，上位=宪章 §7.1/§7.3）**：variance/ivar 子产品输入由显式拒绝改为显式消费传播（输出 VARIANCE/IVAR HDU），flux-per-pixel/weight 等其余拒绝项不变；实现归 P3-001，本节现状守卫在实现落地前保持有效 |
| max_tiles | int | 标量 | tile | 缓存容量；≤0 恢复默认 8（cpp:161-168）；会话层默认 min(1024, ceil(W·H/512²)+16)，请求超默认 → ACS_ERR_BUDGET（p3_session.cpp:179-194，可降不可升） |
| sampler 选择（会话面） | char* | 1 | — | `nearest`\|`bilinear`（缺省 bilinear；白名单 p3_session.cpp:116-119） |
| d（WCS 平面） | P3WcsDescriptor* | 1 | deg、px | 输出平面几何（DATA-P3-WCS §28.2）；逐输出像素中心 (x,y) 0-based int |
| HiPS tile（读路径） | float32 | 512×512/leaf | 面亮度 | tile=HEALPix cell @hips_order 的 W×W FITS float tile（SCI §9a-1）；local 映射 =DATA_SEMANTICS §3 CDS oracle 冻结（fits_index=(511-x)*512+y）；tile 值=面亮度（§9a-8），NaN=传播语义（§6.6）非 invalid；tile 缺失=无覆盖非错误 |

### 29.2 输出

| 名称 | dtype/形态 | 语义 |
|---|---|---|
| value（采样出参） | float32 标量 | 面亮度采样值（BUNIT 承载单位，缺省 'ADU' 绝不 Jy/beam——open_ex cpp:130-159 + SCI §9a-11）；tile 内 NaN → NaN（传播）；tile 缺失 → NaN |
| coverage（采样出参） | float32 标量 | **二值语义**（0/1，float 承载）；C=1 ⇔ 采样足迹内存在有限 tile 像素（SCI §5）；tile 缺失/未打开 → 0 |
| out_order（open_ex 出参） | int 标量 | 输入 survey 实际 order（properties 读出，非请求猜测） |
| out_bunit（open_ex 出参） | char* | tile BUNIT，缺省 'ADU'（绝不 Jy/beam） |
| order_sel（会话 provenance） | int 标量 | p3_order_select 结果；provenance.order_sel_used 填实际值（p3_session.cpp:265-277） |
| mask（会话 coverage_output） | float32 W×H 平面 | coverage_output 仅 `mask` 合法（p3_session.cpp:128-129）；由 coverage 平面生成 |
| S / C 输出平面（会话缓冲） | float32 | W×H 各一（S 初值 NaN、C 初值 0，p3_session.cpp:204-205）；逐像素独立填充 |

### 29.3 单位/dtype/确定性

- 单位唯一权威=本节：采样值=面亮度（surface brightness，HiPS image
  语义，SCI §9a-8），单位经 BUNIT 透传（缺省 'ADU'）；**禁止
  flux-per-pixel 解释与面积换算**（SCI §9a-8 显式拒）；order 无量纲；
  coverage 无量纲二值。
- dtype 唯一权威=本节：采样值/coverage=float32（tile 原生 float32，
  bilinear 权重 FP64 计算后回落 float32 输出）；order/max_tiles=int；
  目录/模式/单位=char 文本。
- 确定性：逐像素计算路径固定（邻域确定→最近中心确定→权重确定，
  ALG-P3-RSMP-IMPL-001 §7）⇒ 输出与 tile 装载顺序、worker 数、缓存
  容量无关；每 worker 独立 sampler+cache（无共享可变状态，HiPS tile
  只读共享）；禁 hardware_concurrency（worker=budget.max_workers）。

### 29.4 错误/边界

- 状态码（P3ResampleStatus，h:12-17）: OK=0/PARAM=1/UNSUPPORTED=2/
  IO=3；逐触发锚=ALG-P3-RSMP-IMPL-001 §9 表；会话映射 IO→ACS_ERR_IO、
  UNSUPPORTED→ACS_ERR_UNSUPPORTED、PARAM→ACS_ERR_PARAM（p3_session.cpp:
  169-172）。
- 边界: tile 缺失/读失败 = coverage=0 数据语义**非错误**（§29.2，
  SCI §9a-9）；tile 内 NaN = 值 NaN + coverage=1（传播非 invalid）；
  sampler 未打开时采样 = coverage=0（禁静默默认，test_p3_resample.py
  test_06 冻结）；order 上限=survey 实际 order（欠采样降级为原生
  分辨率输出，SCI §9a-5）；missing tile 聚合上报未接线
  （provenance.missing_tiles 恒 nullptr，DISP-P3RSMP-004）。

### 29.5 交叉引用

- 上游: SCI-P3-001（§4/§5/§9a-1/-5/-7/-8/-10，FROZEN 零改动）；
  ALG-P3-003（G3/G4 施工规格，PHASE3_RESAMPLE.md）；ALG-P3-RSMP-IMPL-001
  （实现级合同）；DATA-HIPS-001/DATA-TILE-001（HiPS properties/tile
  输入面）；DATA-P3-WCS（§28，输出平面几何）。
- 下游: API-P3-RSMP-001（p3_sampler_open/open_ex/p3_order_select/
  p3_resample_check_mode/p3_sampler_set_max_tiles/p3_sample_nearest/
  p3_sample_bilinear/p3_sampler_close 消费面冻结）；DATA-P3-FITS
  （§27，resampled 平面为其输入域，descriptor 端口词汇）；API-P3-001
  （会话五段编排面 FROZEN 镜像）；TEST-P3-RES-001（登记面=
  TEST-P3-RSMP-DESIGN-001 设计冻结 VERIFIED=ALG-P3-RSMP-IMPL-001
  §12 + registry 手写页 §9 双重陈述；可执行面升级归 P3-RSMP-TEST）。
- 同文档: §3（FITS tile local-pixel 映射，读路径权威）、§4
  （signal/support/invalid 通用语义）、§27（FITS 写出域，本域为其
  上游）、§28（WCS 域，输出平面几何上游）。
- 端口词汇注记: registry descriptor p3_resample2_descriptor
  （module_adapters.cpp:425-439，module_id=astrocs.phase3.resample2
  占位）端口表 wcs_plan(DATA-P3-WCS 必)+hips(DATA-HIPS-001 必)+
  resampled(DATA-P3-RES 可) 为编排层词汇，由 P3-RSMP-INT 对齐
  astrocs.p3.resample，不得反向作为冻结依据。

## 28. Phase3 投影/WCS（lib/phase3_proj）模块输入/输出数据（DATA-P3-WCS）

> ID: DATA-P3-WCS  状态: CONTRACT_READY（P3-PROJ-DOC 冻结，2026-09-11）
> 模块: lib/phase3_session/p3_wcs.cpp（165 行）+ 唯一权威签名头
> lib/phase3_session/p3_wcs.h（50 行，本域四函数 =p3_wcs.h:31-46）
> （astrocs.p3.projection；astrocs_phase3_session 静态库成员，根
> CMakeLists.txt:460-465；迁移目标 astrocs_p3_projection.dll 为矩阵
> 合同值，尚未存在，由 P3-PROJ-IMPL 建立，禁止声明 IMPLEMENTED；
> 合同落位 lib/phase3_proj/ 三件套）。本节是 Phase3 投影/WCS 域
> in/out 单位/dtype/shape/invalid 的唯一权威；SCI 上游:
> SCI-P3-001 §5 连续定义 + §9a-4 CRPIX/CD/parity 冻结 + §9a-6 极点/
> 半球（docs/science/PHASE3_HIPS_TO_FITS.md，FROZEN V5 SCI-007
> 2026-08-28，零改动）；ALG: ALG-P3-PROJ-IMPL-001
> （docs/algorithms/PHASE3_PROJ_IMPL.md，实现级合同，兼承接
> ALG-P3-002 G1/G2 本域子面）；API 面: API-P3-PROJ-001；descriptor
> 占位 module_id=astrocs.phase3.wcs（module_adapters.cpp:406-423
> p3_wcs_descriptor）由 P3-PROJ-INT 对齐 astrocs.p3.projection。

### 28.1 输入

| 名称 | dtype | shape | 单位 | 语义/invalid |
|---|---|---|---|---|
| centre_ra_deg / centre_dec_deg | float64 | 标量 | deg（ICRS） | 中心天球坐标；\|dec\|>85° → P3_WCS_PARAM（p3_wcs.cpp:40，kMaxAbsDec :15） |
| scale_deg_per_px | float64 | 标量 | deg/px | 必 >0（:41）；冻结进 CD 对角/旋转展开式 |
| width_px / height_px | int | 标量 | px | ∈[1,20000]（:42-43，kMaxSide 默认 20000，ASTROCS_P3_MAX_SIDE 编译期覆盖如实冻结） |
| parity | char* | 1 | — | "east_left"（默认，nullptr 归一，CD1_1<0）\|"east_right"（CD1_1>0）；其它 →PARAM（:39） |
| rotation_pa_deg | float64 | 标量 | deg | 天北相对 +y 位置角，逆时针为正；会话层现状恒 0.0（p3_session.cpp:160，PA 未接线=整改项不修码） |
| d（映射入口） | P3WcsDescriptor* | 1 | deg、px | 见 28.2 输出面；非空守卫（:95/:122） |
| x / y（pix2world） | float64 | 标量 | px | **0-based**（FITS 1-based=+1，:97-98 内部换算）；TAN 半球外 r≥π/2 → P3_WCS_HEMISPHERE（:104） |
| ra_deg / dec_deg（world2pix） | float64 | 标量 | deg（ICRS） | \|dec\|>85° →PARAM（:123）；背面 denom≤0 →HEMISPHERE（:130） |

### 28.2 输出

| 名称 | dtype/形态 | 语义 |
|---|---|---|
| P3WcsDescriptor | struct（p3_wcs.h:11-20） | crval_ra/dec_deg（deg, ICRS）；crpix_x/y（FITS 1-based pixel-center=(W+1)/2）；cd[2][2]（FITS 顺序 CD[i][j]，deg/px，CD-only，det=−s²<0 手性冻结）；width_px/height_px；projection="TAN"（硬编码，:36/:89） |
| ra_deg / dec_deg（pix2world 出参） | float64 标量 | deg（ICRS）；RA 归一 [0,360)（normalize_ra fmod+正化，:24-27/:113-114） |
| x / y（world2pix 出参） | float64 标量 | px（0-based；δ=CD⁻¹·(ξ,η)+CRPIX−1，:136-141） |
| fits_keywords 返回 | std::string | 每行 ≤80 字节 FITS 卡文本，"\n" 分隔：CTYPE1/2=RA---TAN/DEC--TAN、CUNIT1/2=deg、CRPIX1/2、CRVAL1/2（%.10f）、CD1_1..CD2_2（%.12e）（:145-163）；nullptr 入参→空串 |
| P3WcsStatus 返回 | enum（h:22-27） | OK=0/PARAM=1/UNSUPPORTED=2（无产生点）/HEMISPHERE=3；逐触发锚=ALG-P3-PROJ-IMPL-001 §9 表 |

### 28.3 单位/dtype/确定性

- 单位唯一权威=本节：天球角量=deg（ICRS，CUNIT1/2='deg' 冻结）；
  CD 单位 deg/px；像素量=px（crpix 1-based 约定、映射入参/出参
  0-based，两种约定并存如实冻结，实现内部换算）。
- dtype 唯一权威=本节：接口面 float64（FP64，roundtrip <1e-6 px
  冻结容差的精度前提）；尺寸 int；parity/关键词 char 文本。
- 确定性：纯函数无状态（0 处 thread/mutex/omp/全局可变量，:12-28
  匿名命名空间常量）——同入参跨线程/跨 worker bitwise 一致；
  并发安全（const-only 入口），无求和序问题。

### 28.4 错误/边界

- rc 语义（P3_WCS_OK=0/PARAM=1/UNSUPPORTED=2/HEMISPHERE=3）与逐
  触发锚见 ALG-P3-PROJ-IMPL-001 §9 表；make 失败 out 保持零初始化
  态（:35），不产出半成品 descriptor；四角同半球守卫失败
  （:80-88）按首次失败码透传。
- 极点邻域单一条件 \|dec\|≤85°（SCI/API/session 同一常数）；TAN
  半球界 r<π/2（正映射）与 denom>0（反映射）数学等价；FOV 适用
  上限 20°（SCI §9a-12 冻结，超限非错误、由畸变语义约束）。
- RA wrap：跨 0/360 经 atan2+fmod 归一 [0,360)，无接缝跳变
  （SCI §9a-6）。

### 28.5 交叉引用

- 上游: SCI-P3-001（§5 连续定义 + §9a-4/-6/-12 + §7 roundtrip
  <1e-6 px，FROZEN 零改动）；ALG-P3-002（G1/G2 施工规格，
  PHASE3_RESAMPLE.md，公式零改动）；ALG-P3-PROJ-IMPL-001（实现级
  合同）；DATA-P3-PROPS（descriptor 端口词汇，HiPS properties 面）。
- 下游: API-P3-PROJ-001（p3_wcs_make/p3_wcs_pix2world/
  p3_wcs_world2pix/p3_wcs_fits_keywords 消费面冻结）；
  DATA-P3-FITS（§27，WCS 关键词写路径 wcs 字段承载本 descriptor，
  p3_output.cpp:148-169）；API-P3-001（会话五段编排面 FROZEN
  镜像）；TEST-P3-WCS-001（登记面=TEST-P3-WCS-DESIGN-001 设计
  冻结 VERIFIED=ALG-P3-PROJ-IMPL-001 §12 + registry 手写页 §9
  双重陈述；可执行面升级归 P3-PROJ-TEST）。
- 同文档: §3（FITS tile local-pixel 映射，读路径上游域）、§4
  （signal/support/invalid 通用语义）、§27（FITS 写出域，WCS 为
  其输入面）。
- 端口词汇注记: registry descriptor p3_wcs_descriptor
  （module_adapters.cpp:406-423，module_id=astrocs.phase3.wcs
  占位）端口表 props(DATA-P3-PROPS 必)+wcs_plan(DATA-P3-WCS 可)
  为编排层词汇，由 P3-PROJ-INT 对齐 astrocs.p3.projection，不得
  反向作为冻结依据。

## 30. Phase2/Phase3 不确定度与 rejection/provenance 产品合同（DATA-UNC-001）

> ID: DATA-UNC-001  状态: FROZEN_TARGET_CONTRACT（DATA-001 冻结，控制包
> ASTROCS-CONSTITUTION-ALIGNMENT-V1，2026-09-09；前置 GOV-001 宪章 FROZEN）
> 上位约束: 宪章 ASTROCS-CONSTITUTION-001 §6.1（Phase2 输出
> variance/ivar/rejection 产品）、§6.3（integration 必须明确不确定度传播）、
> §7.1（Phase3 输出带不确定度传播能力）、§7.3（variance/ivar/coverage/NaN
> 传播规则必须由 SCI/ALG 明确；不支持的数据语义必须显式拒绝）、§16.2/§18.3
> （unavailable 显式登记模式）；supersession 生效。
> **目标态声明（实现不得先行）**：本节冻结的是产品合同目标态；现状代码不满足
> 本节（DISP-P2HIPS-001/002、§27.2、§29.1 守卫），差距整改归 P2-001/P3-001
> 及其 IMPL 子任务；本节不得被现状代码反向否证，现状描述节在实现落地前保持
> 有效。科学公式权威 = docs/science/UNCERTAINTY_AND_COVARIANCE.md（§30 引用
> 不重复定义，两处冲突以 docs/science/ 为准并回改本节，禁止反向）。

### 30.1 Phase2 马赛克 variance/ivar 产品（DATA-P2-VAR-001）

- **产品集注册（目标态）**: `aio_hips_product_begin` flags 增加
  `AIO_HIPS_PRODUCT_VARIANCE|AIO_HIPS_PRODUCT_IVAR`（位 8/16，aio_hips.h:39-40
  已冻结）；子产品目录 `<out_hips>/variance/`、`<out_hips>/ivar/`（NESTED
  512×512 tile，dtype 同 precision，f32/f64），写通道 = writer 既有
  `aio_hips_write_variance_tile`（aio_hips_writer.cpp:603；var_num_sum /
  covered_area² 归一合同 §12.3/§12.4 已冻结）。
- **合成公式**（weight_mode=2 科学默认，且 `ivar_product_missing==0`，即全部
  输入帧 ivar 产品可用、无 fallback；有效样本资格 = SCI-INT §5 valid ∧ W>0）:

```text
ivar_mosaic(p) = W(p) = Σ_i ivar_i(p)          # = SCI-INT §5 wsum（逐像素）
variance_mosaic(p) = 1 / W(p)
```

  一致性锚（一般式，权重非纯逆方差时适用，禁止换第二套公式）:
  `variance = Σ_i w_i²·v_i / W²`（SCI-DRZ-014 同构；w_i=实际参与积分权重，
  v_i=该样本输入方差=1/ivar_i）。UPM 控制权重（SCI-UPM-WEIGHT-001）与马赛克
  合成方差严格分离：control_variance 只进 w_UPM，不进本产品。

- **unavailable 规则（fail-closed，唯一出口）**: 下列任一 → **不写
  variance/ivar 子产品** + manifest `uncertainty_available=false` +
  diagnostics 标红计数，禁止用 support/snr²/常量 0 伪 variance：
  1. `weight_mode != 2`（mode 1 等权 / mode 0 legacy 诊断非科学方差面，ivar
     读端亦不强制打开，§20.1）；
  2. fallback 发生（`legacy_allow_weight_fallback=true` 且
     `ivar_product_missing>0`，§20.4 rc=7 门的显式降级路径）——混合帧集
     （部分帧 support 降级）同样整体 unavailable；
  3. 合成输入非有限被 `p2_validate_candidate_weights` 拒（INVALID_INPUT，
     现行 rc=6）达帧级阻断时整产品不发布（现行失败非原子语义不变，§20.4）。

- **invalid policy（per-pixel，输出面唯一权威）**:

| 条件 | variance | ivar | 说明 |
|---|---|---|---|
| 无有效样本（n_used=0：depth 0 tile / ALL_REJECTED / ZERO_VALID_WEIGHT，SCI-INT §5 状态机） | NaN | NaN | 与 signal=NaN 同态（writer 通道 §12.4 合同：covered_area≤0 → NaN）；禁 0/±Inf 伪装 |
| 正常合成（n_used≥1，W>0 有限） | 1/W | W | FP64 计算后按 precision 落盘 |
| 合成结果非有限（W=Inf 等病态） | NaN | NaN | + diagnostics 病态像素计数，禁静默 0 |
| 输入 ivar 非有限/负 | — | — | `p2_validate_candidate_weights` hard fail（现行 rc=6），不入合成 |

  注：Phase1 逐帧输入产品的 0/NaN 双值预存歧义见 §4a 消解注记（finding
  F-UNC-001）；读侧消费按 §20.1（ivar==0 合法零权重、nonfinite 拒），
  本表只冻结 Phase2 **输出**产品。

- **确定性**: 逐像素独立合成；求和顺序=帧输入索引序（与 SCI-INT §5 同序）；
  OMP 定序归并/固定 chunk 划分合同（§20.3）不变；1..N worker 与 repeat
  bitwise（实证基线 Phase2IvarWiring 同门槛）。
- **HIPS_VERIFY（目标态）**: 回读面由 signal 扩展为 signal/variance/ivar
  tile 数一致（任一回读失败 rc=7，现行 :1659-1676 语义同构扩展）。

### 30.2 Phase2 rejection 产品（DATA-P2-REJ-001）

- **子产品目录（目标态）**: `<out_hips>/nused/`、`<out_hips>/nrej/`，int32
  tile（BITPIX=32，NESTED 512×512，dtype 固定 int32 无 precision 开关）。
- **语义（已冻结 SCI 量的逐像素投影，无新科学定义）**:

```text
nused(p) = P2PixelResult.n_used        # SCI-INT §5 冻结量（参与积分样本数）
nrej(p)  = |{ s | reason_s ∉ {ACCEPTED, UNDERDETERMINED} }|
           # SCI-REJ §5 kernel 拒绝计数（eligibility 剔除不计入 nrej；
           # n_ineligible = depth − nused − nrej，depth=probe 覆盖帧数）
```

- **invalid**: 无覆盖 tile 像素（signal=NaN）→ nused=0、nrej=0（int 无 NaN，
  0 即"无"，禁 −1 哨兵）。
- **逐帧 reason 级产品为非目标**（本合同不冻结 per-frame rejection map；
  需要时须 SCI/owner 冻结变更，禁止实现自行扩展）。
- **AIO 子产品位分配（冻结）**: NREJ=32、NUSED=64（uint32 flags 域；
  1/2/4/8/16 已占用见 aio_hips.h:35-40，32/64 空闲）；AIO_ALL 掩码扩展由
  实现任务在 AIO 域合同登记，本节只冻结位值不冻结掩码。
- **exchange 面**: nused/nrej 定位为**诊断统计平面**（diagnostic planes），
  不进入 phase_product_exchange 的 science planes 枚举（signal/support/
  variance/ivar/mask，validator `_PLANE_ID_SET` 同步不变，零断链）；诊断
  平面由 artifact manifest content_role 与 diagnostics.json 描述。science
  planes 枚举扩展（若未来需要）必须与 runtime validator 同一提交修订
  （finding F-UNC-003 登记该联动约束）。

### 30.3 Phase2 provenance 产品合同（DATA-P2-PROV-001）

- **必写 provenance 键（目标态）**: HiPS properties 与 finalize manifest.json
  双写（writer provenance 通道 `aio_hips_set_drizzle_provenance`
  （aio_hips.h:144，ASTROCS_DRIZZLE_* 键先例）+ finalize manifest 面
  （aio_hips_writer.cpp:1086-1128））。键名冻结（ASTROCS_ 前缀同风格，
  properties 文本键，manifest JSON 键同名小写）:

| 键 | 值语义 | 来源（已冻结锚） |
|---|---|---|
| ASTROCS_INPUT_MANIFEST_HASH | 64hex sha256 | §20.3 input_manifest_hash 公式（stage2.cpp:230-245） |
| ASTROCS_MODEL_HASH | UPM model_hash | P2ModelInfo.model_hash（stage2.cpp:439-444） |
| ASTROCS_UNCERTAINTY_AVAILABLE | true/false | §30.1 unavailable 规则判定结果 |
| ASTROCS_WEIGHT_MODE | 0/1/2 | cfg.weight_mode（stage2_common.h:90） |
| ASTROCS_REJECT_PROFILE | 版本化 profile 串 | cfg.reject_profile（wbpp_2_9_1，§20.1） |

- **unavailable 显式登记**: uncertainty_available=false 不是失败态，是宪章
  §18.3 unavailable 模式在数据面的落位（禁命令占位/静默缺键/空输出冒充）。
- **原子发布边界不变**: 本键写入不改变 §20.3 原子发布归属（IO-003 编排层，
  DISP-P2HIPS-003 整改去向不变）。

### 30.4 Phase3 uncertainty 传播产品合同（DATA-P3-UNC-001）

- **输入消费面（读侧唯一权威；supersession SCI-P3 §9a-10 的 variance/ivar
  显式拒绝语义，上位=宪章 §7.1/§7.3，supersession 生效；SCI-P3 原文注记见
  docs/science/PHASE3_HIPS_TO_FITS.md 头部 DATA-UNC-001 更新块）**:
  1. 输入 HiPS 含 variance/ 子产品 → u_in = variance 平面；否则含 ivar/ →
     u_in = 1/ivar（ivar==0 像素 = u 无效）；两者并存 → variance 优先，
     provenance 记 uncertainty_source=variance（一致性数值校验为验证建议，
     不冻结容差）；
  2. 两者皆无 → **uncertainty unavailable**：输出不写 VARIANCE/IVAR HDU +
     manifest `uncertainty_available=false` + 命令 diagnostics 明示（宪章
     §18.3 模式；禁静默丢弃，禁占位 HDU）；
  3. u 值域: NaN = 传播态（见 invalid 表）；负/Inf（含 ivar==0 导出）=
     **产品损坏 → 显式错误**（run 拒绝，rc 由实现任务映射到现行 P3_RS_*/
     ACS_ERR_* 状态域登记，禁 clamp/补 0/静默跳过）；
  4. §29.1 input_mode 守卫扩展: surface_brightness 语义不变；variance/ivar
     从 UNSUPPORTED 拒绝项移除（转 uncertainty 子产品消费）；flux-per-pixel/
     weight 等其余拒绝项不变（SCI §9a-8 不动）。

- **传播公式**（科学权威 = docs/science/UNCERTAINTY_AND_COVARIANCE.md；
  c_k = ALG-P3-003 G4 冻结双线性权重，Σc_k=1）:

```text
nearest :  var_out = u_in
bilinear:  var_out = Σ_k c_k² · u_k     # 注意 Σc_k² ≠ 1：
           # 常数方差场经 bilinear 后 var_out < u 是正确物理（插值平均去相关），
           # 禁止误用 Σc_k=1 归一 variance（负向测试 W2 防错锚）
ivar_out = 1 / var_out   (var_out 有限且 >0)
ivar_out = var_out 同态  (var_out=0 → 0 显式不可用; NaN → NaN)
```

- **invalid policy（per-pixel，输出面唯一权威；与 §27.2 signal/coverage
  语义同构）**:

| 条件 | signal | variance | ivar | coverage |
|---|---|---|---|---|
| 无覆盖（tile 缺失/足迹无有限 leaf） | NaN | NaN | NaN | 0 |
| NaN 传播（足迹内 leaf signal 或 u 为 NaN） | NaN | NaN | NaN | 1 |
| 覆盖不一致（leaf signal 有限而 u 无效/缺失） | 按采样值 | NaN | NaN | 1（+ provenance uncertainty_missing_pixels 计数，不中断不补 0） |
| 正常传播（足迹 signal 与 u 全有效） | Σ c_k·s_k | Σ c_k²·u_k | 1/var_out | 1 |

  NaN 同态说明: 无覆盖 variance 用 NaN（§12.4 writer 通道合同与 signal NaN
  同态），不用 §4a 的 0（§4a 歧义消解见 §4a 注记与 §30.1）。

- **输出 FITS 表达（§27.2 目标态行）**: VARIANCE/IVAR 扩展 HDU，
  EXTNAME="VARIANCE"/"IVAR"，BITPIX 同主 HDU（用户 -32/-64 选择），
  BUNIT=<signal BUNIT>^2 / 1/(<signal BUNIT>^2)（缺省 ADU → "ADU^2" 与
  "1/(ADU^2)"），DATASUM 逐 HDU（COVERAGE HDU 模式同构，§27.2）；三 HDU 与
  主 HDU 同一原子发布序（§27.4 整文件单元不变，取消不落盘不变）。
- **exchange/manifest**: available 时 product_content.planes 声明
  variance/ivar（validator 枚举已合法，零 schema 改动）；unavailable 时不
  声明该两平面且 manifest uncertainty_available=false。
- **精度/确定性**: u 读入与传播 FP64；落盘 dtype=bitpix；逐输出像素独立、
  权重由几何唯一确定（§29.3 合同不变）；1..N worker bitwise（同 §27.3
  写面串行 + §29.3 采样确定性）。

### 30.5 验证门（TEST 设计冻结；可执行实现归 P2-001/P3-001，未运行不得 PASS）

- **TEST-P2-UNC-DESIGN-001**（Phase2，f64 oracle rtol=1e-12、1T/2T/repeat
  bitwise、索引/整数 bitwise）:
  - V1 正向: 3 帧合成 ivar 1:2:4，weight_mode=2 → ivar_mosaic==Σivar、
    variance_mosaic==1/W 逐像素；
  - V2 unavailable 负向: 缺 ivar 帧 + legacy_allow_weight_fallback=true →
    variance/ivar 子产品不存在 + uncertainty_available=false；缺 ivar 且未
    显式 fallback → rc=7（现行门不变）；
  - V3 invalid: 输入 ivar 非有限 → INVALID_INPUT（rc=6）；n_used=0 像素
    variance/ivar=NaN 且 signal=NaN 同态；
  - V4 确定性: 1T/2T/repeat bitwise + 帧置换不变；
  - V5 provenance: properties/manifest 双键实测值 == input_manifest_hash/
    model_hash 计算值；UNCERTAINTY_AVAILABLE 与实际子产品存在性一致。
- **TEST-P3-UNC-DESIGN-001**（Phase3，WCS roundtrip 1e-6 px 不变）:
  - W1 nearest 正向: var_out==u_in 逐像素；
  - W2 bilinear 正向+防错: 解析 4-leaf u 与冻结权重 → var_out==Σc_k²·u_k
    （FP64 rtol 1e-12）；常数 u 场断言 var_out==u·Σc_k²（≠u，防 Σc_k=1
    误归一回归）；
  - W3 unavailable: 无 variance/ivar 输入 → 无 VARIANCE/IVAR HDU +
    uncertainty_available=false + diagnostics 明示；
  - W4 invalid: u NaN → 输出 NaN/C=1；u<0/Inf → 显式错误非静默；
    无覆盖 → variance/ivar=NaN/C=0；
  - W5 确定性: 1/N worker bitwise；
  - W6 FITS 独立 reader 回读 EXTNAME/BUNIT/DATASUM（Oracle 独立性 §11
    SCI-P3 不变）。
- **故障注入有效性**: 上述负向项（V2/V3、W3/W4）在实现合入时必须先于实现
  以失败测试形式存在（红→绿），注入等价缺陷（静默 0 伪 variance、Σc_k=1
  误归一、静默缺 HDU）必须使其失败。

### 30.6 交叉引用与登记

- 上游 SCI: SCI-INT-001（§5 wsum/n_used）、SCI-REJ-001（§5 reason 值域）、
  SCI-DRZ-001（SCI-DRZ-014 方差传播同构）、SCI-UPM-001/ALG-UPM-CONTROL-IVAR-001
  （control_variance 与本产品分离）、SCI-P3-001（§5 采样核/§9a-10
  supersession 注记）、SCI-NOISE-001（缩放律同源）。
- 宪章: ASTROCS-CONSTITUTION-001 §6.1/§6.3/§7.1/§7.3/§16.2/§18.3（FROZEN，
  上位约束，本文不得放宽）。
- ALG（实现锚，现状如实）: ALG-P2-HIPS-001..004（PHASE2_MOSAIC_WRITE.md
  §11.3 DISP-P2HIPS-001/002 整改去向）；ALG-P3-003/ALG-P3-RSMP-IMPL-001
  （§29.1 守卫）、ALG-P3-004/ALG-P3-FITS-IMPL-001（§27.2 HDU 扩展）。
- DATA 同文档: §4a（0/NaN 歧义消解注记）、§12.3/§12.4（writer variance
  通道合同）、§20（P2-HIPS 现状）、§21/§22（n_used/reason 权威）、§27/§29
  （P3 现状注记）。
- 登记面: docs/contracts/DATA_ARTIFACTS.md §1 四行（DATA-P2-VAR-001/
  DATA-P2-REJ-001/DATA-P2-PROV-001/DATA-P3-UNC-001）；
  docs/contracts/INDEX.yaml DATA-UNC-001 四条目；消费任务 P2-001/P3-001
  （控制包 DAG，G-SCI 门组成）。
- findings 登记（域外预存问题，不在本任务修复）:
  - F-UNC-001: §4a（0=无覆盖）与 §12.4（无效=NaN）预存双值，Phase1
    域语义消歧归 Phase1 域任务；
  - F-UNC-002: 基线 check_data_artifacts 预存 FAIL（DATA-HIPS-001/
    DATA-TILE-001 声明未登记，HEAD 即失败，与本任务无关，须 P0/P1 清理时
    处置）；
  - F-UNC-003: exchange science planes 枚举（phase_product_exchange
    schema + runtime validator `_PLANE_ID_SET`）与诊断统计平面（nused/
    nrej）的联动扩展约束——任何扩展必须 schema 与 validator 同一提交，
    当前无扩展需求。
