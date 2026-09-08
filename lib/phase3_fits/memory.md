# memory.md — astrocs.p3.fits_writer（P3-FITS-DOC 冻结）

- 任务: P3-FITS-DOC（MODULE_MIGRATION_MATRIX P3-FITS 行，owner
  SA-P3-F27，2026-09-08）——合同冻结层，不改生产源码，不 commit。
  本目录 `lib/phase3_fits/` 三件套（README r1 + module.yaml +
  memory.md）由 P3-FITS-DOC 建立。
- 落位: `lib/phase3_fits/`（本目录）。实测生产源
  lib/phase3_session/p3_output.cpp + p3_output.h 位于
  lib/phase3_session/——该目录为 Phase3 会话编排域共享源
  （p3_session/p3_resample/p3_wcs/hips_properties 五源同库
  astrocs_phase3_session，根 CMakeLists.txt:460-465），非整目录
  归属本域（矩阵 legacy_paths="lib/phase3_session fits sources" 只
  圈 fits sources）；按 `lib/phase2_upm/`（P2-UPM-DOC）→
  `lib/phase2_samp/`（P2-SAMP-DOC）→ `lib/phase2_rej/`（P2-REJ-DOC）
  → `lib/phase2_int/`（P2-INT-DOC）→ `lib/hips_p2/`（P2-HIPS-DOC）
  先例新建迁移目标目录，仅合同文件、无源码、不与 legacy 目录重叠；
  legacy 生产源引用不搬家。
- 矩阵权威（P3-FITS 行，禁止编造）: owner=SA-P3-F27、
  module_id=astrocs.p3.fits_writer、target_dll=
  astrocs_p3_fits_writer.dll、legacy_paths="lib/phase3_session fits
  sources"、depends_on_int=**P3-RSMP-INT;IO-003**、
  science_specific_acceptance="block streaming;SCI/SUPPORT/MASK
  HDUs;BUNIT/WCS/checksum;atomic rename;overwrite policy;memory
  independent of full image size"。
- module_id 决策: 任务指派文字写 astrocs.p3.fits，与矩阵行
  astrocs.p3.fits_writer 冲突——MODULE_MIGRATION_MATRIX 为冻结权威
  依据（照 P2-SAMP 先例：指派文字与矩阵冲突以矩阵为准并显式记录），
  取 **astrocs.p3.fits_writer**。descriptor 占位
  module_id=astrocs.phase3.writer（module_adapters.cpp:383-398
  p3_writer_descriptor）为编排层词汇，由 P3-FITS-INT 对齐，不入合同。
- MOD ID 决策: registry 现有行 MOD-astrocs-phase3-writer（单
  descriptor 域），module.yaml id 与之一致；registry 页手写化
  （docs/modules/registry/astrocs.phase3.writer.md），模块总页新建
  docs/modules/phase3_fits.md。
- 本任务 ID 决策（唯一方案，避免与既有占位冲突）:
  SCI=SCI-P3-001（docs/science/PHASE3_HIPS_TO_FITS.md 共享 FROZEN
  V5 SCI-007 2026-08-28，集合 SCI-P3-001..020；descriptor 占位
  SCI-P3-WR-001 不入合同，映射声明于 PHASE3_FITS_IMPL.md §映射节，
  照 P2-REJ/P2-COV 先例）；
  ALG=ALG-P3-FITS-IMPL-001（docs/algorithms/PHASE3_FITS_IMPL.md
  新建，兼承接既有 ALG-P3-002（输出 WCS G1/G2）与 ALG-P3-004
  （FITS 写 G5）本域子面；ID 风格与 ALG-P2-UPM-IMPL-001 同族，
  取 -IMPL-1 避免与占位冲突；既有 ALG-P3-001..004 登记于
  docs/algorithms/PHASE3_RESAMPLE.md 头部范围声明，ALG-P3-002/004
  以独立条目入 INDEX.yaml）；
  DATA=DATA-P3-FITS（descriptor/matrix 现值沿用；承载=
  DATA_SEMANTICS.md §27 新建）；
  API=API-P3-FITS-001（PUBLIC_API.md 新节；descriptor 占位
  api_id=API-P3-001 为编排层 FROZEN 合同不变，镜像引用）；
  TEST=TEST-P3-WR-001（descriptor/matrix 现值沿用；登记面=
  TEST-P3-WR-DESIGN-001 设计冻结 VERIFIED 承载于 registry 手写页
  §独立 synthetic 验证节——照 P2-INT/P2-REJ/P2-SESSION registry
  页锚先例双重陈述；可执行测试 MISSING 归 P3-FITS-TEST，不冒认）。
- 生产源锚（grep/read 实测，2026-09-08）:
  lib/phase3_session/p3_output.h（64 行，唯一权威签名头）:
  P3Provenance :15-24（8 字段）、P3OutputResult :26-32
  （sha256[65]/coverage_ok/reopen_ok/covered_px/total_px）、
  P3OutputStatus :34-39（OK=0/PARAM=1/IO=2/CANCELLED=3）、原子写
  协议冻结注 :41-44、p3_output_write_atomic :45-53、verify 冻结注
  :55-56（sha256 失败→IO 不带假哈希）、p3_output_verify :57-60。
  lib/phase3_session/p3_output.cpp（370 行）:
  平台宏 unlink/fsync/close/open :19-31、g_last_err :56、
  fdatasum 32-bit checksum :58-67、make_temp_path :72-84
  （tmp=out+"."+pid+".tmp" :81，同目录保证 rename 原子 :82）、
  R10-C sha256 封装注 :85-91、sha256_file_checked 严格封装
  :92-114（fopen/ferror/fclose 全检查；ASTROCS_HASH_FAIL_INJECT
  仅测试注入 :105-108）、cfitsio 进程锁 :125（RT-008）、
  make_temp_path 调用+残留清理 :128-130、fits_create_file
  :135-137、bitpix 门 :140-145、fits_create_img :144-147、
  WCS 关键字 CTYPE1/2=CUSTYPE RA---TAN/DEC--TAN/CUNIT=deg
  :148-153、CRPIX/CRVAL/CD1_1..CD2_2 :154-170、BSCALE=1/BZERO=0/
  BUNIT（缺省 ADU）:171-177、provenance 关键字 HIPSID/RUNID/
  ORDERSEL/SAMPLER/SWVER :178-185+HISTORY :186-189、
  fits_write_pix TFLOAT 全帧 :193-195、取消不落盘 :198-202、
  COVERAGE 扩展 create_img :206-210+EXTNAME :211+write_pix :212、
  DATASUM(32-bit fdatasum(signal)) :214-219、R10-C 原子序注
  :221-224（fits_flush_file → close :231-239 → fsync(fd, POSIX
  O_RDONLY/Windows O_RDWR _commit) :240-267 → rename :269-273）、
  发布后 sha256+独立 verify :275-292（covered_px 统计 :287）、
  p3_output_verify :296-368（(void)wcs :305、READONLY 重开
  :312、逐 HDU 尺寸门 :318-348、signal 回环 fits_read_pix :326+
  NaN==NaN 语义 :327-330、coverage 二值门 >0.5f :344-346、
  sha256 重算 :351-364）。
  lib/phase3_session/p3_wcs.h（50 行）: P3WcsDescriptor :11-20
  （crpix FITS 1-based pixel-center :14、cd deg/px :16、
  projection="TAN" :19）、P3WcsStatus :22-27（OK/PARAM/
  UNSUPPORTED/HEMISPHERE）、p3_wcs_make :31-34（parity
  east_left=CD1_1<0 默认 :29）、p3_wcs_pix2world :38-39（0-based
  像素，FITS=+1 :36）、p3_wcs_world2pix :42-43、
  p3_wcs_fits_keywords :46。
  lib/phase3_session/p3_session.h（40 行）: 五段式 :16-28
  （create/validate/run/inspect/destroy，API-P3-001 冻结）、
  last_error 脱敏 :33-37。
  lib/phase3_session/p3_session.cpp（329 行）: parse 拒绝清单
  :97-129（projection≠TAN→UNSUPPORTED :97、frame≠icrs→
  UNSUPPORTED :102-103、abs(dec)>85°→PARAM :107、scale>0 :109、
  W/H∈[1,20000] :113-114、sampler :118-119、parity :123-124、
  bitpix :127、coverage_output=mask :129）、max_tiles 守卫
  :179-193（默认 min(1024, ceil(W·H/512²)+16)，请求可降不可升
  →ACS_ERR_BUDGET :188-190）、order select 上限=输入实际 order
  禁超 20 :196-199、行带并行 std::thread 池 :247-253（worker 数
  =host budget.max_workers :212-213，:209 注释禁
  hardware_concurrency，串行阈值 n_workers<2 :246）、取消点=行
  :228-229（cancelled_at atomic :216，open 失败 sentinel -2
  :224）、ACS_ERR_CANCELLED :258-262、provenance 填充 :265-277
  （manifest_hash 恒 nullptr :270——接线整改点）、写出调用
  :287-292（cancelled_at_row 恒 -1 :292、失败→ACS_ERR_IO
  :293-295）、inspect JSON :296-313（kind/run_id/exit_code/
  output_fits_path/sha256/order_sel_used/sampler_used/
  coverage_stats/provenance）。
- 构建挂载（实测）: 根 CMakeLists.txt:460-465
  astrocs_phase3_session STATIC（五源: p3_session/p3_wcs/
  hips_properties/p3_output/p3_resample）；astrocs_aio STATIC
  :273-296（aio_fits 等 6 源 + astrocs_cfitsio vendored + z）；
  cfitsio 进程锁单例 lib/astro_image_io/src/aio_cfitsio_mutex.h
  :9-15（aio_fits.cpp:529 读路径 + p3_output.cpp:125 写路径共用，
  RT-008）。tests/unit/CMakeLists.txt:442-447 p3_output_test
  （链 astrocs_phase3_session+astrocs_hips+astrocs_common）。
- 执行测试锚（tests/unit/p3_output_test.cpp，116 行，4 段）:
  ①原子写+mask（:62-88，64×48 渐变场+分段 mask、prov 全字段、
  BITPIX=-32、res.coverage_ok/reopen_ok/sha256 len==64）、
  ②独立 verify（:89-100）、③无 .tmp 残留（:101-113，
  filesystem 遍历 WIN-001 替代 popen）、④WCS roundtrip oracle
  （:114-131，pix→world→pix <1e-4 px + 采样值锚）。相邻证据
  引用不冒认：可执行 TEST-P3-WR-001 归 P3-FITS-TEST。
- lint 关键词实测（P3-FITS-DOC，2026-09-08）: 本域源码
  lib/phase3_session/*.cpp 0 处 hardware_concurrency（p3_session
  .cpp:209 仅注释命中）、0 处 #pragma omp、std::thread 池
  p3_session.cpp:247（worker 数=budget.max_workers :212-213）；
  lib/astro_image_io/src/aio_fits.cpp:1154 唯一
  `#pragma omp parallel for schedule(static)`（QA-001 -fopenmp
  编译处理）属 AIO 域非本域。
- 实测偏差登记（不改码）:
  - DISP-P3FITS-001: lib/astro_image_io/README.md 旧派生内容声称
    "零外部依赖、不依赖 cfitsio"，与现状 vendored
    third_party/cfitsio（astrocs_cfitsio 静态库 :273-296）矛盾；
    他域文件只登记不修。
  - DISP-P3FITS-002: tmp 命名冻结注与实现偏差——p3_output.h:41-44
    协议注写 `<dir>/.<base>.<pid>.tmp`（前置点隐藏文件形态），实测
    make_temp_path 生成 `out_path.<pid>.tmp`（p3_output.cpp:81，
    无前置点、保留 .fits 扩展名）；同目录保证 rename 原子性语义
    不变，但执行测试残留检查前缀 ".astrocs_p3_out_test."
    （tests/unit/p3_output_test.cpp:102-103）与实际命名恒不匹配
    → 残留检查弱匹配空转（不误报，也捕不到本实现形态的残留）。
    登记不改码，命名统一归 P3-FITS-IMPL。
  - 整改项（非缺陷）: manifest_hash 恒 nullptr（p3_session.cpp
    :270，HISTORY manifest 字段写空，SCI-P3 §96 接线归
    P3-FITS-IMPL）；p3_output_verify 忽略 wcs 参数（p3_output
    .cpp:299 (void)wcs，WCS 一致性由写路径单点保证，设计如此）。
- 五门基线与产出（本任务跑数见 selfcheck 输出）: traceability
  modules=30 errors=0 warns=4（基线 warns=4 均为存量 EVID/API
  authority WARN，本任务新增 WARN 计数须持平=4）；pytest
  tests/traceability 9 passed；contract graph 80→86（+6 条目）；
  doccheck 修复存量缺口 docs/algorithms/PHASE2_UPM_IMPL.md 未登
  DOCUMENT_INDEX（UPM-DOC 漏登，机械补登条目不改动文件内容——照
  P2-SAMP e654d4c2 补登先例）+ 本任务新文件登记 → verdict
  DOC_INDEX_PASS；selfcheck ALL PASS。
