# astrocs.p3.fits_writer — Phase3 FITS 写出域（P3-FITS）

> P3-FITS-DOC（2026-09-08，SA-P3-F27）合同冻结任务新建模块页。合同三件套
> 落位 `lib/phase3_fits/`（README r1 + module.yaml + memory.md，
> CONTRACT_READY，entrypoint=MISSING）——迁移目标目录按 `lib/phase2_int/`
> → `lib/phase2_rej/` → `lib/phase2_samp/` → `lib/phase2_upm/` 先例新建，
> 仅合同文件、无源码、不与 legacy 目录重叠；生产源引用不搬家。
> 生产源 `lib/phase3_session/p3_output.cpp`（370 行，根 CMakeLists.txt
> astrocs_phase3_session STATIC 目标 :460-465 五源文件之一）+ 唯一权威
> 签名头 `lib/phase3_session/p3_output.h`（64 行）；进程内编排消费方
> `lib/phase3_session/p3_session.cpp`（329 行，run 段 :287-289 调
> p3_output_write_atomic）；执行测试 `tests/unit/p3_output_test.cpp`
> （116 行，tests/unit/CMakeLists.txt:442-447 注册）。

## 身份与合同

- MOD ID：`MOD-astrocs-phase3-writer`（registry 行 ID 沿用，手写合同页
  `docs/modules/registry/astrocs.phase3.writer.md`）；module_id：
  `astrocs.p3.fits_writer`（MODULE_MIGRATION_MATRIX P3-FITS 行权威值；
  任务指派文字 astrocs.p3.fits 与矩阵冲突——矩阵为权威依据，落位决策
  记录于 memory.md 与本 README）；dll_target：
  `astrocs_p3_fits_writer.dll`（矩阵合同值，尚未存在——MISSING 语义，
  迁移归 P3-FITS-IMPL，本页不声明 IMPLEMENTED）。
- owner SA-P3-F27；depends_on_int=**P3-RSMP-INT;IO-003**；
  legacy_paths="lib/phase3_session fits sources"（均以
  MODULE_MIGRATION_MATRIX.csv P3-FITS 行为权威）。
- 合同链：SCI-P3-001（docs/science/PHASE3_HIPS_TO_FITS.md，FROZEN
  V5 SCI-007 2026-08-28，集合 SCI-P3-001..020；descriptor 占位
  SCI-P3-WR-001 不入合同，映射声明 SCI-P3-WR-001⇒SCI-P3-001 于
  docs/algorithms/PHASE3_FITS_IMPL.md §映射节）
  → ALG-P3-FITS-IMPL-001（docs/algorithms/PHASE3_FITS_IMPL.md，
  P3-FITS-DOC 新建；兼承接既有 ALG-P3-002（输出 WCS，PHASE3_RESAMPLE
  G1/G2）与 ALG-P3-004（FITS 写，G5）的施工规格本域子面）
  → DATA-P3-FITS（DATA_SEMANTICS §27）/ API-P3-FITS-001（PUBLIC_API
  Phase3 FITS 写出公共消费面节）→ TEST-P3-WR-001（登记面=设计冻结
  TEST-P3-WR-DESIGN-001 VERIFIED，承载于
  docs/modules/registry/astrocs.phase3.writer.md §独立 synthetic
  验证节 + ALG 文档 §TEST-DESIGN 容差；可执行测试 MISSING 归
  P3-FITS-TEST，不冒认）。

## 职责（摘要，权威=ALG-P3-FITS-IMPL-001）

- FITS 输出原子写（ALG-P3-004）：signal 主 HDU + COVERAGE 扩展 HDU
  合成单文件；写 `<dir>/.<base>.<pid>.tmp` → cfitsio 缓冲全量 flush
  （fits_flush_file）→ fsync(fd) → 原子 rename（IO_003 §4 顺序冻结；
  R10-C 修正注释 p3_output.cpp:245-248 实测锚）；任何失败/取消 → 删
  tmp/产物，不留完整假文件，不发布无完整性锚的输出（h:41-44 冻结注）。
- 输出关键字全量冻结（SCI-P3 §96）：BITPIX=-32|-64（调用方参数，
  :131 显式拒其它）、BSCALE=1/BZERO=0、BUNIT（缺省 "ADU"）、
  WCS=CRPIX/CRVAL/CD1_1..CD2_2/CTYPE=RA---TAN|DEC--TAN/CUNIT=deg
  （:143-166）、HISTORY+provenance 关键字 HIPSID/RUNID/ORDERSEL/
  SAMPLER/SWVER（:170-185）；COVERAGE 扩展 EXTNAME+DATASUM
  （32-bit fdatasum，:210-217）。
- 独立重开验证（p3_output_verify）：READONLY 重开 → 逐 HDU 尺寸/像素
  回环（NaN==NaN 语义 :311-313）→ coverage 二值门（>0.5f）回环 →
  sha256 重算；哈希失败 = 完整性锚缺失 → P3_OUT_IO，result 不携带
  假哈希（h:55-56 冻结注）。
- 非职责：不做重采样/瓦片读（P3-RSMP 域 p3_resample/p3_sampler）、
  不做 order 选择（p3_order_select，会话编排域）、不做请求解析/参数
  拒绝（p3_session run 段）、不做读路径 FITS/HiPS 解析（AIO/hips 域）。
  FITS WCS 科学算法（ALG-P3-002 TAN 正反变换）权威在 p3_wcs.cpp，
  本域仅承载其输出关键字落盘面。

## 关键合同事实

- cfitsio 进程级串行化（RT-008）：写全程持 `aio::cfitsio_io_mutex()`
  （p3_output.cpp:125；lib/astro_image_io/src/aio_cfitsio_mutex.h:9-15
  单例；与 aio_fits.cpp:529 读路径共用），覆盖内部 verify 重开。
- rc 枚举冻结（p3_output.h:34-39）：P3_OUT_OK=0 / P3_OUT_PARAM=1 /
  P3_OUT_IO=2 / P3_OUT_CANCELLED=3；CANCELLED 仅由 cancelled_at_row≥0
  触发（:198-202），session 层恒传 -1（:292）。
- sha256_file_checked 严格封装：fopen/ferror/fclose 全检查，失败不写
  空/前缀哈希（R10-C；ASTROCS_HASH_FAIL_INJECT 仅测试注入）。
- 参数门：signal/coverage/wcs/output_path 非空、W/H∈[1,20000]（会话
  层 :113-114 先行拒绝；内核再拒 width<1||height<1 :117）。
- determinism=fixed_reduction_order：像素写序=行主序单线程 cfitsio
  调用（fits_write_pix 一次全帧），与 worker 数无关；并行仅发生在
  上游采样（p3_session.cpp:247-253 std::thread 池），写面串行。
- 编排面镜像：API-P3-001（p3_session.h 五段 C ABI FROZEN）不变，
  本域内核消费面=API-P3-FITS-001，二者由 API 节显式区分；
  ARCH-001 承载域边界（矩阵行 arch 权威）。

## 已知缺陷（登记不改码，P3-FITS-IMPL/TEST 整改）

- DISP-P3FITS-001：lib/astro_image_io/README.md 旧派生内容声称
  "零外部依赖、不依赖 cfitsio"，与现状 vendored third_party/cfitsio
  （astrocs_cfitsio 静态库，根 CMakeLists.txt:273-296 astrocs_aio
  链接）矛盾——P3-FITS-DOC 只登记不修他域文件。
- DISP-P3FITS-002：tmp 命名冻结注与实现偏差——p3_output.h:41-44
  协议注写 `<dir>/.<base>.<pid>.tmp`（前置点隐藏文件形态），实测
  make_temp_path 生成 `out_path.<pid>.tmp`（p3_output.cpp:81，无
  前置点、保留 .fits 扩展名）；同目录 rename 原子性语义不变，但
  执行测试残留检查前缀（tests/unit/p3_output_test.cpp:102-103）
  与实际命名恒不匹配 → 残留检查弱匹配空转。登记不改码，命名统一
  归 P3-FITS-IMPL。
- 整改项（非缺陷，登记）：provenance.manifest_hash 现状恒 nullptr
  （p3_session.cpp:270），HISTORY 的 manifest 字段写空——SCI-P3 §96
  要求 manifest hash 必写，接线归 P3-FITS-IMPL；session 内核
  p3_output_verify 忽略 wcs 参数（p3_output.cpp:305 (void)wcs，
  WCS 一致性由写路径单点保证）——设计如此，注释如实。

## 关联文档

- 合同权威：docs/algorithms/PHASE3_FITS_IMPL.md（ALG-P3-FITS-IMPL-001）
- 数据语义：docs/contracts/DATA_SEMANTICS.md §27（DATA-P3-FITS）
- API 面：docs/contracts/PUBLIC_API.md「Phase3 FITS 写出公共消费面
  （API-P3-FITS-001）」
- SCI：docs/science/PHASE3_HIPS_TO_FITS.md（FROZEN，零改动）
- 模块页：docs/modules/phase3_fits.md；registry 手写页：
  docs/modules/registry/astrocs.phase3.writer.md
- 元数据：module.yaml（29 键，CONTRACT_READY，entrypoint=MISSING）、
  memory.md（任务决策与实测锚）
