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
> lib/core/src/module_adapters.cpp:508-525 p1_drizzle_descriptor）将
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
> module_adapters.cpp:469-486）为编排层词汇，由 P1-PHOT-INT 对齐，
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
> module_adapters.cpp:437-441）与本节一致。**双 [N,9] 布局并存**（15.2 A/B）
> 为冻结事实，禁止混淆。

### 15.1 输入（dpsf_fit_batch_f / _d 生产通道，orchestrator.cpp:2314/:2339）

| 参数/字段 | dtype/shape | 单位/域 | invalid / NULL 语义 |
|---|---|---|---|
| image（cleaned 块） | float32（f 通道）/ float64（d 通道）`[h·w]` 行主序 | ADU | NULL/h≤0/w≤0 → rc=−1；FP64 通道不降级（PREC-105 同族约束） |
| cx_arr / cy_arr（来自 sources 块 star_det v1 [N,6] 列 [0]/[1]，dynamic_psf.h:79-104） | double `[n_stars]` | pixel（0-based） | NULL/count≤0 → −1；越界星 fitRadius 裁窗 clamp（dpsf_psf.cpp:533-536）；空 rect → 该星 INVALID_PARAMS |
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

**布局 B：批 API `out_psf_params`（dpsf_fit_batch_f32/f64，dynamic_psf.h:84-105）**：

| 列 | dtype/单位 | 语义 / invalid |
|---|---|---|
| [0]=B / [1]=A | double ADU | 背景/振幅；失败星 9 字段全 NaN（:729-732/:857-860） |
| [2]=cx / [3]=cy | double pixel | 全图坐标（局部回移） |
| [4]=sx / [5]=sy | double pixel | σ 分量 |
| [6]=theta | double 弧度 | 旋转角（x 轴起边，4 候选消歧后） |
| [7]=fwhm_x / [8]=fwhm_y | double pixel | 1.230310·sx/sy |

`out_n_valid` 仅计 DPSF_FIT_OK（:804/:925）。两布局列序不同（A=状态/派生量序，
B=参数序），跨层传递禁止直接复用同一缓冲。

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
- 确定性：逐星独立拟合，OpenMP dynamic 调度仅影响线程分配，输出按索引写，
  bitwise 与线程数无关（fixed_reduction_order，README §7）。
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
  （module_adapters.cpp:254-544）与本节冲突时以本节+各冻结 DATA 节为准。

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
> 权威；descriptor astrocs.phase1.star-psf（module_adapters.cpp:430-448）为
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
