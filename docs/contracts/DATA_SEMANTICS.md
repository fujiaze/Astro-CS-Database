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
> （module_adapters.cpp:450-464）为编排层词汇，由 P1-WCS-INT 对齐，不得
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
> （module_adapters.cpp:561-576）为编排层词汇（端口坐标 PIXEL 登记与
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
输出合成 ivar/variance）。几何: NESTED（§2 唯一 ordering），
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
    产品（DISP-P2HIPS-001）；weight_mode 语义: 2=逐样本 ivar
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
  （lib/core/src/module_adapters.cpp:677-694，module_id=
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
