# lib/hips — astrocs.p1.hips_writer（P1-HIPS）

> 状态: CONTRACT_READY（P1-HIPS-DOC 冻结，2026-09-07）｜doc revision: r1
> 本 README 由源码逐函数核对后新建（P1-HIPS-DOC）：函数、单位、坐标、dtype、
> shape、invalid、错误、并发、内存、I/O 均以现行唯一生产实现
> `lib/astro_image_io/src/hips/aio_hips_writer.cpp`（合同头
> `lib/astro_image_io/include/aio_hips.h`；根 CMakeLists.txt:298-309 编入静态库
> `astrocs_hips`，C ABI 导出 aio_hips.h:104,121,130,135,144,149,152,163,177
> 的 9 符号）为准；`lib/hips/` 是 P1-HIPS 迁移目标目录（astrocs_p1_hips_writer.dll
> 落码由 P1-HIPS-IMPL 建立，当前本目录仅合同文件、无源码；落位依据
> MODULE_MIGRATION_MATRIX.csv P1-HIPS 行 target=astrocs_p1_hips_writer.dll、
> legacy_paths="lib/healpix_db;lib/phase1_session"——实测生产 writer 在
> lib/astro_image_io/src/hips/，lib/healpix_db 侧参与生产的为 drizzle 写通道
> astro_sphere_sink.cpp，healpix_stack 系列函数全仓零调用已死代码化，
> lib/phase1_session 对 HiPS 零引用（grep 退出码 1），与 lib/drizzle/、
> lib/cosmetic/、lib/calibration/ 先例同构，本目录不与 legacy 目录重叠）。权威合同：
> SCI-DRZ-001（共享引用）→ ALG-HIPS-001..005 → DATA-P1-HIPS / API-HIPS-001
> （链接见 §4）。

## 1. 身份

| 字段 | 当前值 |
|---|---|
| MOD ID / DLL target | `MOD-astrocs-phase1-hips-writer` / 现状实现编入 CMake 静态库 `astrocs_hips`（CMakeLists.txt:298-309，无独立 DLL 产物，全仓库无 astrocs_p1_hips_writer 目标）；迁移目标 `astrocs_p1_hips_writer.dll`（P1-HIPS-IMPL 建立，尚未存在） |
| module / ABI / doc revision | `astrocs.p1.hips_writer` / C ABI（aio_hips.h extern "C" AIO_HIPS_EXPORT，无版本化 query 入口，迁移缺口）/ r1 |
| owner / phase scope | SA-P1-D18 / phase1（matrix P1-HIPS；depends_on_int=P1-DRZ-INT;IO-003） |
| 文档状态 | CONTRACT_READY（实现存在于 lib/astro_image_io/src/hips，模块化迁移未开始；不声明 IMPLEMENTED） |
| 构建 | 现状随 CMakeLists.txt:298-309 `astrocs_hips`（STATIC，AIO_ENABLE_FITS/AIO_ENABLE_HEALPIX :317，链接 astrocs_aio/astrocs_common :318；astrocs_drizzle :378 与主程序 :493-497 均链 astrocs_hips）；独立目标由 P1-HIPS-IMPL 建立 |

## 2. 负责范围

负责：IVOa HiPS 1.4 产品集写入（aio_hips.h:6-9 标准：NESTED、tile_width=512、
NorderK/DirD/NpixN.fits 万进制分片、CFITSIO 4.6.4 写 FITS 含 checksum）——
signal/support/variance/ivar 四个 Image HiPS 子产品与 SNR Catalogue HiPS
（TSV tile + VOTable metadata.xml）、叶级 tile 写（AstroSphereTileView 直写
视图）、surface brightness/coverage 归一（signal=flux_sum/covered_area、
support=covered_area/A_cell）、方差产品（variance=var_num_sum/covered_area²、
ivar=1/variance）、低阶 hierarchy tiles 从磁盘聚合、Moc.fits（BINTABLE
UNIQ）、properties（IVOa 关键字 + ASTROCS 扩展键）、metadata.fits、
manifest.json、Drizzle provenance 键（ASTROCS_DRIZZLE_PIXFRAC/SCALE_ARCSEC，
Phase2 k_corr 选择输入）。数据语义 DATA-P1-HIPS（DATA_SEMANTICS §12）。

不负责：tile 累加本身（sumFlux/sumArea/sumVarNum 生成=P1-DRZ drizzle 引擎，
本模块仅消费 AstroSphereTileView；tile_depth!=9/nside<512 在 sink 层拒绝，
astro_sphere_sink.cpp:34-44）；FITS 读/通用图像 IO（aio_fits；本模块只写）；
HiPS 读侧（aio_hips_reader.cpp，HIPS_VERIFY 后端，orchestrator:3794）；Phase2
输入原子发布语义层（IO-003 docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md，
Python 侧发布器，不改本 C++ writer——边界引用不越权）；HISS 中间容器链路
（hiss_codec/hiss_stream_writer/.hiss，独立通道，仅 legacy_hiss_compare=true
时写出，HISS_VERIFY 由 CFG-002 关闭 orchestrator:3404-3407）；编排 stage
序列与 DLL 加载（orchestrator）；整 Phase 行为（禁止）。

## 3. 输入与输出（DATA-P1-HIPS，DATA_SEMANTICS §12）

输入（流式，不要求 tiles 全在 RAM，aio_hips.h:84）：`out_dir` 根路径；
`nside`（叶级，2 的幂且 ≥512，=2^leaf_order）；`tile_width` 恒 512；
`data_type` AIO_HIPS_FLOAT32(0)/FLOAT64(1)；`flags` 子产品位或
SIGNAL=1/SUPPORT=2/SNR=4/VARIANCE=8/IVAR=16/ALL=7/ALL_V19=31（aio_hips.h:34-43）；
properties 元数据入参 creator_did（缺省 ivo://astrocs/phase1）/
obs_title（缺省 "AstroCS Phase1"，:414-415）/obs_filter(可 NULL)/exposure_s(s)/
obs_date(可 NULL)/moc_order(0=auto=tile_order，>0 取 min 与 tile_order，
:419)。逐 tile 输入 AstroSphereTileView（aio_hips.h:59-71）：parent_ipix
（Norder K=leaf_order−9 的 NESTED ipix，uint64）、leaf_order、width=512、
dtype、flux_sum/covered_area `[512×512]` NESTED local 行主序（float32 或
float64，与 data_type 一致）、valid_mask `[512×512]` uint8（可 NULL=全有效，
:466/:606）、var_num_sum `[512×512]`（可 NULL）。SNR 点 AioHipsSnrPoint
（ra/dec deg double、snr double、star_id int64、quality_flags 位 1=PSF_OK/
2=saturated/4=has_saturated/8=photo_matched/16=photo_rejected、
photometric_status 0=unmatched/1=used/2=rejected，aio_hips.h:74-82）。
单位：flux_sum ADU（面亮度累加语义）、covered_area sr、exposure_s s、
ra/dec deg。所有权：view 数据调用方持有、调用期间有效（writer 同步消费）。

输出（单位/dtype/shape/invalid 全表见 DATA_SEMANTICS §12.2）：
`<out_dir>/signal|support|variance|ivar/NorderK/DirD/NpixN.fits`（512×512，
bitpix −32/−64 随 data_type，PIXTYPE=HEALPIX/ORDERING=NESTED/COORDSYS=C/
NSIDE/FIRSTPIX/LASTPIX cards + DATASUM/CHECKSUM，write_fits_image :171-238）；
`<out_dir>/snr/` Catalogue HiPS：逐 cell TSV tile（star_id/ra/dec/snr/
quality_flags/photometric_status，SNR-PREC-001 FP32 %.9g/FP64 %.17g
round-trip 精度，:904-924）+ properties（dataproduct_type=catalog、
hips_cat_nrows、hips_initial_ra/dec=源位置中位数）+ metadata.xml（VOTable
1.3，:977-1000）+ Moc.fits；每子产品 `properties`（key=value 逐行，
write_properties :285-293）+ `Moc.fits`（UNIQ=4·4^m+(c>>2(K−m))，BINTABLE，
write_moc_fits :243-283；空 MOC 不写 :246）+ `metadata.fits`（:776-786）；
根级 `manifest.json`（format_version/hips_version="1.4"/nside/tile_width/
data_type/products/n_leaf_tiles/moc_sky_fraction/
astrocs_covered_sky_fraction/signal_dtype，:1086-1128）。invalid：
covered_area≤0 或非有限 → signal=NaN、support=0（:476-485）；variance/ivar
同 tile 域 NaN（:615-621）；无有效样本 tile 的 variance 写请求 rc=−5 显式
失败（:629-632），不静默产空 tile。

## 4. 合同链接

| 层 | ID | 权威文档 |
|---|---|---|
| SCI | SCI-DRZ-001（共享引用，不改 SCI） | docs/science/DRIZZLE.md（FROZEN T105 2026-08-23；:130 实现锚 aio_hips_writer finalize_tile；:145 support=D_p 归一语义）；产品语义另见 docs/science/SCIENCE_SCOPE.md:9-10 与读侧消费合同 SCI-P3-001 |
| ALG | ALG-HIPS-001..005 | docs/algorithms/HIPS_WRITER.md（逐公式源码锚定 + DISP-HIPS-001..012） |
| DATA | DATA-P1-HIPS | docs/contracts/DATA_SEMANTICS.md §12（上游 DATA-P1-DRZ §11；产品位/ivar 语义 §4a DATA-HIPS-VAR-001/DATA-HIPS-IVAR-001；帧身份 §5） |
| API | API-HIPS-001 | docs/contracts/PUBLIC_API.md（aio_hips.h 9 符号现状 C API；编排级经 API-P1-007 hp_drizzle_run_hips 间接到达） |
| ARCH/MOD/SRC | ARCH-001 / MOD-astrocs-phase1-hips-writer / SRC-HIPS-001 | docs/traceability/TRACEABILITY_MATRIX.json；SRC 锚 aio_hips.h::9 符号 |

## 5. 公共入口与符号（API-HIPS-001）

现状 C ABI 九导出（lib/astro_image_io/include/aio_hips.h；实现行号
aio_hips_writer.cpp）：`aio_hips_product_begin`（头 :104，实现 :386-422）→
`aio_hips_write_signal_support_tile`（:121/:424-562）→
`aio_hips_write_variance_tile`（:130/:571-692）→ `aio_hips_write_snr_points`
（:135/:684-692，finalize 前累计缓存）→ `aio_hips_set_drizzle_provenance`
（:144/:1007-1016）→ `aio_hips_finalize`（:149/:1018-1131）或
`aio_hips_abort`（:152/:1133-1137）；兼容批量 `aio_hips_write`（:163/:1149-，
旧 support uint8 0..255 → covered_area=su/255·A_cell、flux_sum=signal·su/255
语义映射，flags 固定 ALL）与 `aio_hips_last_error`（:177/:1139-1141，
thread_local g_hips_error，每次入口 clear）。调用协议=begin → 零或多次
write_* → finalize/abort（成功路径 finalize 内 delete ps；abort 仅释放句柄，
:1133-1137）。失败返回负错误码，文本经 last_error 获取（无跨边界异常）。
所有权：ps 由调用方 finalize/abort 释放。

## 6. 配置 schema 与错误码

配置经 product_begin 参数固化（无独立 config 文件/JSON schema——11 号标准
"未知写 MISSING"：versioned config schema=MISSING，迁移时由 P1-HIPS-IMPL
冻结）。现状错误码（无集中枚举，登记缺陷 DISP-HIPS-007）：
product_begin 返回 NULL + last_error（nside<512/tile_width≠512/dtype∉{0,1}/
flags 越位，:399-404）；write_signal_support_tile −1 null、−2 视图不匹配
（width/leaf_order/dtype，:428-431）、−3 parent_ipix 越界（:433-437）、
−4 signal FITS 失败（:513）、−5 support FITS 失败（:521）；
write_variance_tile −1 null、−2 var_num 缺失（:575-576）、−3 不匹配（:579）、
−4 越界（:584）、−5 全无效（:632）、−6/−7 FITS（:648/:658）；
write_snr_points −1（:686）；finalize −1 null、−2 重复（:1022-1023）、
−3/−4/−7/−8 signal/support/variance/ivar 子产品失败（:1036/:1042/:1049/
:1055）、−5 hierarchy（:1065）、−6 snr（:1072）；set_drizzle_provenance
1 null、2 值域（pixfrac∈(0,1]，scale≥0，:1007-1016）；legacy
aio_hips_write −1..−6。

## 7. 线程、并行轴、内存与 I/O

线程安全：单一 ProductSet 句柄**非线程安全**（scratch_sigF/sigD/supF/supD/
varF/varD/sig_n/sup_n/var_n 等成员缓冲跨 tile 复用，:442-453/:590-600；
moc_cells/hier/leaf_ipix_list 无锁累积）；不同句柄并发未同步——实现**未
包装**进程级 aio::cfitsio_mutex（同库 aio_fits.cpp:502 与
aio_hips_reader.cpp:91/:140/:293 均用），CFITSIO 调用直接裸调（DISP-HIPS-006
登记）。内部并行：无（顺序写，单句柄串行；上游 drizzle 为 OpenMP 行级并行
+线程私有累加+线程 0 串行合并后单线程调 writer，drizzle_engine.cpp:1670、
astro_sphere_sink.cpp:97）；threading_model=host_executor_lease 为迁移合同
值，现状由调用方线程直接执行；OpenMP ICV 不受 writer 影响。内存：叶级
scratch 512×512×多数组复用零再分配（:442-453）；hierarchy 累加器
hier[k]=map<ipix,AncestorAcc>（512×512/acc，:297-333,:359-361）随产品覆盖
增长（K=14 时约 13 层×活跃 cell×~9MB f32——DISP-HIPS-011 缓冲重分配）；
SNR 点全量内存缓存（:684-692）；leaf_ipix_list 逐 tile 追加。I/O：CFITSIO
写 FITS（先 remove 后 create 允许覆盖，:185-186）；properties 直写文本；
hierarchy 落盘读回已写 leaf tiles（finalize_hierarchy :791-885）。stdout
无日志；stderr [hips]/[sink] 前缀；六段 profile 计时（:368-373，
:1030-1076 打印）；取消/进度无检查点（长 finalize 不可中断）。

## 8. 确定性与 prov

tile 写顺序由调用方决定（drizzle sink 按 tiles 向量序）；同输入同序则科学
tile 字节逐字节可复现（FITS DATASUM/CHECKSUM 内嵌；properties 的
hips_creation_date/hips_release_date=真实 UTC（utc_now_date/utc_now_iso
:93-/:80-，gmtime_r 固定格式不依赖时区，META-001 禁伪造）→ **properties/
manifest 字节不跨运行复现**）。hierarchy 归约按 k 降序 + NESTED 索引确定性
顺序（:536-557）无浮点求和顺序漂移（每父 cell 单线程顺序累加；f32 产品
AncestorAcc float 累加精度见 DISP-HIPS-009 与 TEST-HIPS-DESIGN-001 容差）。
provenance：creator_did/obs_title/obs_filter/exposure/obs_date 直写入
properties；ASTROCS_DRIZZLE_PIXFRAC/SCALE_ARCSEC 由 sink 经
set_drizzle_provenance 传入（astro_sphere_sink.cpp:65-76）；prov_progenitor
=ivo://astrocs/phase1/drizzle（:729）；hips_builder 含 CFITSIO 版本。整树
哈希无（writer 层）——sha256 清单在 IO-003 发布层，边界=DATA-P1-HIPS §12.5。

## 9. 测试、验证与已知限制

现有验证：tests/unit/p1_hips_writer_test.cpp（product_begin→写→finalize 全
链）；drizzle 侧 tests/hiss_write_probe.cpp、bench_write.cpp；独立复检生态
gate7_hips_validate.py（astropy 对拍 support∈[0,1]/F=signal×support×A_cell/
MOC↔叶级对应）与 HIPS_VERIFY（读侧验证）；IO-003 合同测
tests/io/test_hips_output_contract.py（Python 语义层）。冻结测试设计
TEST-HIPS-DESIGN-001 见 ALG-HIPS-001 §9（fixture/oracle/不变量 I1-I12/
负面/容差），可执行 TEST-P1-HIPS-001 由 P1-HIPS-TEST 建立。

已知限制（完整清单 = ALG-HIPS-001 §10 DISP-HIPS-001..012）：abort 不删除
已写文件（:1133-1137 仅 delete，头注释 aio_hips.h:151 "清理已写部分(尽力)"
与实现不符——部分失败产品残留由调用方/IO-003 层处置）；无原子发布
（remove+create 直写 :185-186；对照 IO-003）；props "hips_estsize"=
"1000000"/hips_initial_fov="60" 硬编码（:721/:745/:954）；hips_status 恒
"private master"（:718）；CFITSIO 裸调无 mutex 包装（DISP-HIPS-006）；
错误码无集中枚举且 1/2 正负混用（DISP-HIPS-007）；moc_order 入参静默
clamp 且低阶 MOC 对自家 reader 无效（:419；DISP-HIPS-005）；f32 产品
hierarchy float 累加漂移风险（DISP-HIPS-009）；fits_str 68 字符静默截断
（DISP-HIPS-010）；properties 无转义（key=value 裸写，值域受控）；hierarchy
空父 cell 照写全 NaN tile（DISP-HIPS-011）；FIRSTPIX/LASTPIX 声明性头卡
（DISP-HIPS-012）。

## 10. 迁移（P1-HIPS-IMPL 目标，不声明完成）

本目录（lib/hips/）为迁移落点：astrocs_p1_hips_writer.dll、module.yaml
（同目录，已冻结 manifest：entrypoint=MISSING——registry 无 descriptor）、
C ABI adapter、plan/execute/cancel/inspect、ThreadLease 接线、DISP-HIPS
清单消化见 ALG-HIPS-001 §0/§10 与 module.yaml 注释。迁移不得改变
ALG-HIPS-001..005 公式语义与 DATA-P1-HIPS 数据语义（SCI-DRZ-001 未变更前）；
IO-003 发布合同对齐（临时写→fsync→原子 rename→manifest COMPLETE）属
P1-HIPS-INT 集成范围，不在 DOC 层改动 C++ writer。
