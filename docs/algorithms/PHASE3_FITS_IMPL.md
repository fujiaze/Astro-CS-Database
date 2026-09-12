# Phase3 FITS Write-out Algorithms（P3-FITS / astrocs.p3.fits_writer）

> ID: ALG-P3-FITS-IMPL-001  状态: CONTRACT_READY（P3-FITS-DOC 冻结，2026-09-08，
> owner SA-P3-F27）。本文件是 Phase3 HiPS→FITS 写出域的**实现级算法合同**：
> 逐符号源码行号锚定 + 冻结容差 + 现状缺陷登记。科学语义权威=SCI-P3-001
> （docs/science/PHASE3_HIPS_TO_FITS.md，FROZEN V5 SCI-007 2026-08-28，
> 集合 SCI-P3-001..020，零改动）；推导级算法权威=ALG-P3-001..004
> （docs/algorithms/PHASE3_RESAMPLE.md，DERIVED 施工规格，本任务不改动其
> 公式；G1/G2 WCS 构造与 G5 FITS 写公式的本域实现子面由本文档承接）。
> 模块: lib/phase3_session/p3_output.cpp（370 行）+ 唯一权威签名头
> lib/phase3_session/p3_output.h（64 行）+ WCS 关键字源
> lib/phase3_session/p3_wcs.h（50 行，实测 2026-09-08）；
> API: API-P3-FITS-001（PUBLIC_API.md「Phase3 FITS 写出公共消费面」节）；
> DATA: DATA-P3-FITS（DATA_SEMANTICS §27）；MOD: astrocs.p3.fits_writer
> （MODULE_MIGRATION_MATRIX P3-FITS 行）；TEST: TEST-P3-WR-001
> （设计冻结面=本文档 §12 + registry 手写页，可执行 MISSING 归
> P3-FITS-TEST）。

## 1 目的与非目标

- 目的：冻结 HiPS 重采样结果的 FITS 落盘合同——signal 主 HDU +
  COVERAGE 扩展 HDU 合成单文件、WCS/BUNIT/provenance 关键字全量、
  原子写协议（tmp → flush → fsync → rename）、失败/取消清理、
  独立重开验证与 sha256 完整性锚。
- 非目标：不做重采样/tile 读取（ALG-P3-001/003，p3_resample/
  p3_sampler 域）、不做请求解析与参数拒绝清单（p3_session run 段
  职责，§15 引用）、不做读路径 FITS/HiPS 解析（AIO/hips 域）、
  不改 vendored cfitsio（third_party/cfitsio 隔离，astrocs_cfitsio
  静态库，根 CMakeLists.txt:273-296）、不改 SCI 公式。

## 2 符号与单位（权威=本表 + SCI-P3-001 §9a/§96）

| 符号 | 类型 | 单位/值域 | 锚 |
|---|---|---|---|
| signal | f32 [W·H] | surface brightness（BUNIT，缺省 ADU） | p3_output.cpp:193-195 |
| coverage | f32 [W·H] | 二值门 {0,1}（>0.5f=covered） | p3_output.cpp:212/:287/:346 |
| width,height | int px | [1,20000]（会话层 :113-114；内核 width<1 拒 :132） | p3_output.h:46 |
| bitpix | int | -32 \| -64（真实决定 buffer，h:51） | p3_output.cpp:140-145 |
| BSCALE/BZERO | int | 1 / 0（恒定） | p3_output.cpp:189-191 |
| BUNIT | string | properties 缺省 "ADU"（h 缺省 :175） | p3_output.cpp:192-193 |
| CRPIX1/2 | f64 px | FITS 1-based pixel-center | p3_wcs.h:14 / p3_output.cpp:179-180 |
| CRVAL1/2 | f64 deg | ICRS 中心 | p3_wcs.h:12-13 / p3_output.cpp:181-182 |
| CD1_1..CD2_2 | f64 deg/px | FITS 顺序 CD[i][j] | p3_wcs.h:16 / p3_output.cpp:183-186 |
| CTYPE1/2 | string | RA---TAN / DEC--TAN | p3_output.cpp:169-170 |
| CUNIT1/2 | string | deg | p3_output.cpp:171-172 |
| HIPSID/RUNID/ORDERSEL/SAMPLER/SWVER | string | provenance 八字段子集 | p3_output.h:15-24 / p3_output.cpp:196-202 |
| DATASUM | u32 | signal 32-bit fdatasum | p3_output.cpp:59-67/:214-219 |
| sha256 | char[65] | 输出文件 SHA-256 hex 小写（完整读出才填） | p3_output.h:27 / :92-114 |

## 3 逐符号锚（p3_output.cpp 370 行 / p3_output.h 64 行 / p3_wcs.h 50 行，2026-09-08 实测）

- 平台宏（Windows _unlink/_commit/_close/_open 映射）:19-31；
  头包含 aio_fits.h/fitsio.h/aio_cfitsio_mutex.h/sha256.h :47-54。
- `fdatasum`（:59-67）: 32-bit 字节和 checksum（LE 4 字节字累加）。
- `make_temp_path`（:72-84）: tmp = `out + "." + pid + ".tmp"`（:81），
  同目录保证 rename 原子（:82 注释）——**与 h:41-44 协议注
  `<dir>/.<base>.<pid>.tmp` 形态偏差**（DISP-P3FITS-002）。
- `sha256_file_checked`（:92-114）: R10-C 封装（:85-91 注释冻结）——
  lib/common/crypto::sha256_file 对 fopen 失败返回空串、fread 中途
  错误静默返回前缀哈希，均为无意义完整性锚；本封装 fopen/ferror/
  fclose 全检查，仅完整读出产出 64hex，失败返回 false 由调用方
  整体失败（禁止空串/前缀哈希入 provenance）。ASTROCS_HASH_FAIL_INJECT
  仅测试构建注入（:105-108）。
- `p3_output_write_atomic`（h:45-53 声明，实现 :117-321）:
  1. 参数门 :132-134（signal/coverage/wcs/output_path 非空、
     width<1||height<1 → P3_OUT_PARAM）；result 清零 :135-136。
  2. cfitsio 进程锁 :125（RT-008：`std::lock_guard<std::mutex>
     cfitsio_guard(aio::cfitsio_io_mutex())`，全程持锁覆盖内部
     verify 重开；锁单例 aio_cfitsio_mutex.h:9-15，与读路径
     aio_fits.cpp:529 共用）。
  3. tmp 路径 + 历史残留清理 :128-130；fits_create_file :135-137。
  4. bitpix 门 :140-145（≠-32/≠-64 → P3_OUT_PARAM + 删 tmp）；
     fits_create_img :144-147（2 维 naxes={width,height}）。
  5. WCS+基础关键字 :148-177（CTYPE1/2 :149-150、CUNIT1/2=deg
     :151-152、CRPIX1/2 :160-162、CRVAL1/2 :163-164、CD1_1..CD2_2
     :165-169、BSCALE=1/BZERO=0 :171-173、BUNIT 缺省 ADU :174-176）。
  6. provenance 关键字 :178-190（HIPSID/RUNID/ORDERSEL/SAMPLER/SWVER
     :179-185 + HISTORY "source=<hips_id> manifest=<hash>" :186-189）。
  7. signal 像素 :193-195（fits_write_pix TFLOAT 一次全帧行主序）。
  8. 取消门 :198-202（cancelled_at_row≥0 → close+unlink+返回
     P3_OUT_CANCELLED，输出不落盘）。
  9. COVERAGE 扩展 HDU :206-212（fits_create_img 同 bitpix + EXTNAME=
     "COVERAGE" :211 + fits_write_pix :212）。
  10. DATASUM :214-219（fdatasum(signal) :215 → fits_write_key TINT）。
  11. R10-C 原子发布序 :221-273（注释 :221-224 冻结：cfitsio 内部
      缓冲 flush → fsync(fd) → 原子 rename；原实现在 close 前 fsync
      只能落已入内核页缓存前缀，崩溃可丢数据或留半成品，违反
      IO_003 §4/§6）——fits_flush_file :231-236（失败即无成功对象）
      → fits_close_file :237-239 → open+fsync+close :240-267
      （POSIX O_RDONLY / Windows O_RDWR _commit，:247-249 注释
      R18 34201181796 errno=9 实证）→ rename :269-273。
  12. 发布后完整性 :275-292（sha256_file_checked :279，失败 → 删
      产物+P3_OUT_IO :279-283；total_px/covered_px（coverage>0.5f
      计数 :287）/coverage_ok=1/reopen_ok 经独立 verify :290-292）。
- `p3_output_verify`（h:57-60 声明，实现 :296-368）:
  参数门 :297-299；(void)wcs :305（WCS 一致性由写路径单点保证，
  :301-302 注释）；READONLY 重开 :312；fits_get_num_hdus :314；
  HDU1 signal 回环 :316-331（尺寸门 :318-321、fits_read_pix :326、
  逐值精确比对 + **NaN==NaN 视为一致** :327-330——源无覆盖=NaN）；
  HDU2 coverage 回环 :333-348（二值门 `(cov>0.5f)!=(coverage>0.5f)`
  :346）；reopen_ok/coverage_ok/covered_px/total_px :350-354；
  sha256 重算 :356-366（失败 → P3_OUT_IO 不带假哈希 :357-360）。
- `p3_wcs_make/p3_wcs_pix2world/p3_wcs_world2pix/p3_wcs_fits_keywords`
  （p3_wcs.h:31-46 声明）: TAN 正反变换实现 p3_wcs.cpp（ALG-P3-002
  G1/G2 承接；0-based 像素入参，FITS=+1 :36 注释；parity
  east_left=CD1_1<0 默认 :29；abs(dec)≤85° 与四角同半球守卫
  P3_WCS_HEMISPHERE :26）。

## 4 伪代码（write_atomic 与 verify 主流程）

```text
function p3_output_write_atomic(signal, coverage, W, H, wcs, bunit, path, prov, bitpix, cancelled_at_row, out result):
  if signal/coverage/wcs/path null or W<1 or H<1: return P3_OUT_PARAM        # :132-134
  lock(aio::cfitsio_io_mutex())                                              # :125 RT-008 全程
  tmp = path + "." + getpid() + ".tmp"; unlink(tmp)                          # :128-130 DISP-P3FITS-002
  f = fits_create_file(tmp) else IO                                          # :135-137
  if bitpix not in {-32,-64}: unlink; return P3_OUT_PARAM                     # :140-145
  fits_create_img(f, bitpix, [W,H]) else IO                                   # :144-147
  write CTYPE1=RA---TAN, CTYPE2=DEC--TAN, CUNIT1/2=deg                        # :149-152
  write CRPIX1/2, CRVAL1/2, CD1_1..CD2_2                                      # :160-169
  write BSCALE=1, BZERO=0, BUNIT=bunit or "ADU"                               # :171-176
  if prov: write HIPSID/RUNID/ORDERSEL/SAMPLER/SWVER + HISTORY(source,manifest)  # :178-189
  fits_write_pix(f, TFLOAT, signal)                                           # :193-195
  if cancelled_at_row >= 0: close; unlink; return P3_OUT_CANCELLED             # :198-202 不落盘
  fits_create_img(f, bitpix, [W,H]); write EXTNAME="COVERAGE"                  # :206-211
  fits_write_pix(f, TFLOAT, coverage)                                          # :212
  write DATASUM = fdatasum(signal)                                             # :214-219
  fits_flush_file(f) else IO            # ① cfitsio 缓冲全部到 OS(R10-C)        # :231-236
  fits_close_file(f) else IO                                                   # :237-239
  fsync(open(tmp))  else IO             # ② fd 级落盘(POSIX O_RDONLY/WIN O_RDWR) # :240-267
  rename(tmp, path) else IO             # ③ 原子发布                            # :269-273
  result.sha256 = sha256_file_checked(path) else { unlink(path); IO }          # :275-283
  result.total_px = W*H; result.covered_px = #(coverage>0.5f); coverage_ok=1   # :284-289
  result.reopen_ok = p3_output_verify(path, ...).reopen_ok                     # :290-292
  return P3_OUT_OK

function p3_output_verify(path, wcs, signal, coverage, W, H, out result):
  if path/result null or W<1 or H<1: return P3_OUT_PARAM                       # :297-299
  f = fits_open_file(path, READONLY) else IO                                   # :312
  hdus = fits_get_num_hdus(f)                                                  # :314
  movabs HDU1: nax==[W,H]? fits_read_pix(TFLOAT) : ok=0                        # :316-331
    sig[i]==signal[i] 或 双方 NaN → 一致 else ok=0                              # :327-330
  if hdus>=2: movabs HDU2: nax==[W,H]? fits_read_pix : covok=0                 # :333-345
    (cov[i]>0.5f)!=(coverage[i]>0.5f) → covok=0                                # :346
  close; result.reopen_ok = ok && covok; coverage_ok=covok                     # :350-354
  result.sha256 = sha256_file_checked(path) else IO                            # :356-366
  return P3_OUT_OK
```

## 5 上游 SCI 与映射声明（本任务零 SCI 改动）

- 语义权威已有 FROZEN SCI：**SCI-P3-001**（docs/science/
  PHASE3_HIPS_TO_FITS.md；冻结集合 SCI-P3-001..020，V5 SCI-007
  2026-08-28）。**不因本任务改动**（共享 SCI 引用不改动；P2-SAMP/
  P2-REJ/P2-UPM 先例同构）。FITS 关键字面权威=SCI-P3 §96（BITPIX
  -32/-64、BSCALE=1/BZERO=0、BUNIT 按 properties 缺省 ADU、WCS
  CRPIX/CRVAL/CD/CTYPE=TAN/CUNIT=deg、HISTORY+provenance 必写）。
- **descriptor 占位映射声明**（占位 ID 是矩阵/descriptor 词汇，不注册
  INDEX、不入合同）：
  - `SCI-P3-WR-001`（writer descriptor sci_id，
    lib/core/src/module_adapters.cpp:383-398 p3_writer_descriptor）⇒
    **SCI-P3-001**（docs/science/PHASE3_HIPS_TO_FITS.md 共享 FROZEN；
    §9a-11 G5 FITS 写 + §96 关键字冻结为科学语义来源）；
  - `ALG-P3-004`（writer descriptor alg_id）⇒ **ALG-P3-004**
    （PHASE3_RESAMPLE.md G5 施工规格）+ **ALG-P3-FITS-IMPL-001**
    （本文件，实现级合同；ALG-P3-002 G1/G2 WCS 构造子面同承接）；
  - `astrocs.phase3.writer`（descriptor module_id）⇒
    **astrocs.p3.fits_writer**（MODULE_MIGRATION_MATRIX P3-FITS 行
    权威值）。
  - SCI/公式语义不在此重复定义，两处冲突时以 docs/science/ 为准并
    回改本文档（禁止反向）；descriptor 词汇由 P3-FITS-INT 对齐，
    不作冻结依据。

## 6 实现级合同 F1-F4（非 SCI 新公式；G5/G1/G2 引用 PHASE3_RESAMPLE.md 零改动）

- **F1 原子发布序**（IO_003 §4/§6 实现，R10-C 修正 :221-224）:
  tmp 建写 → fits_flush_file（cfitsio dirty buffer 全量到 OS，失败
  即无成功对象 :231-236）→ close :237-239 → fsync(fd)（POSIX O_RDONLY
  /Windows O_RDWR _commit :240-267）→ rename :269-273。任何一步失败
  → unlink(tmp) 不发布；发布后 sha256 失败 → unlink(产物) 不留无锚
  输出（:279-283）。取消（cancelled_at_row≥0）→ close+unlink+不落盘
  （:198-202）。**fdatasum 于 COVERAGE HDU 写入前计算自内存 signal**
  （:215），非 FITS 标准 ASCII CHECKSUM 关键字——DATASUM 为 TINT
  数值关键字（:216-218），与 cfitsio 内建 CHECKSUM 校验和非同一
  语义（如实冻结，不冒称 FITS 标准校验和）。
- **F2 完整性锚**（:85-91/:92-114）: sha256 仅在文件完整读出后产出
  64hex；空串/前缀哈希禁止入 result/provenance；测试注入开关
  ASTROCS_HASH_FAIL_INJECT 仅测试构建。
- **F3 coverage 二值门**（:287/:346）: covered ⇔ value>0.5f；回环
  比对在二值化后进行（浮点 0.7 与 0.9 等价 covered），与 DATA-P3-FITS
  §27 二值语义同源。
- **F4 NaN 回环语义**（:327-330）: 双方 NaN 视为一致（源无覆盖=NaN
  传播），否则逐值精确相等（float bitwise 经 fits_write_pix/read_pix
  TFLOAT 往返无损，bitpix=-32 时 dtype 不变；-64 时 f32 上游已定，
  写入仍 TFLOAT 请求按文件 bitpix 缩放）。
- 依赖声明：F1-F4 为实现级协议合同，不含新科学公式；G1/G2（WCS
  构造/反变换）与 G5（FITS 写公式面）推导权威=PHASE3_RESAMPLE.md
  §2（ALG-P3-002/004），冲突时以 SCI-P3-001 为准（§5 红线）。

## 7 确定性与归约

- 写面单线程串行（cfitsio 进程锁内单次 fits_write_pix 全帧行主序），
  **输出字节与 worker 数无关（1..N bitwise）**——并行仅上游采样
  （p3_session.cpp:247-253），采样结果行主序汇入 sig/cov 后才进入
  写面；sig/cov 逐像素独立写不相交（p3_session.cpp:236-241）。
- sha256/DATASUM 为纯函数（fdatasum 定长 LE 字序累加 :59-67，
  与平台字节序无关的显式构造）；verify 回环逐值精确。
- provenance 字符串（run_id="p3-"+ASTROCS_COMMIT_SHA、
  order_sel 十进制串）确定性生成（p3_session.cpp:266-277）。

## 8 并行语义、cfitsio 串行化与 lint 关键词

- **cfitsio 进程级互斥（RT-008）**：cfitsio 全局表非线程安全 →
  本域写全程持 `aio::cfitsio_io_mutex()`（:125，lock_guard 全帧
  作用域），覆盖发布后独立 verify 重开（:312 同锁保护）；读路径
  aio_fits.cpp:529 同锁。vendored cfitsio 编译定义 _REENTRANT
  （third_party 隔离），不改第三方源。
- **写面无并行**：本域源码（p3_output.cpp/p3_output.h）无
  std::thread、无 OpenMP；并行仅上游采样 p3_session.cpp:247-253
  std::thread 池（worker 数=host budget.max_workers :212-213，
  :209 注释禁 hardware_concurrency；每 worker 独立 sampler+值拷贝
  WCS :217-235；取消点=行 :228-229）。
- **lint 关键词实测清单**（2026-09-08 grep 实测，唯一权威=本表）:
  | 关键词 | 命中 | 处置 |
  |---|---|---|
  | `#pragma omp` | lib/phase3_session/*.cpp **0 处** | 无 |
  | `hardware_concurrency` | p3_session.cpp:238（仅注释，禁用声明） | 合规 |
  | `std::thread` | p3_session.cpp:327（采样池，非写面） | §8 声明面 |
  | `#pragma omp`（AIO 域） | aio_fits.cpp:1154 唯一 `parallel for schedule(static)` | 属 AIO 域非本域（QA-001 -fopenmp 编译处理），登记不改 |
  | `cfitsio_io_mutex` | p3_output.cpp:143 / aio_fits.cpp:529 / aio_cfitsio_mutex.h:11 | RT-008 合规 |
- **取消点**：内核级 cancelled_at_row 参数（h:52，行粒度，session
  层恒 -1 :292）；会话级取消在采样循环 :228-229；写面一旦进入
  R10-C 发布序不可中断（半成品不可见，符合 IO_003 §6）。

## 9 复杂度

- 时间 O(W·H)（fits_write_pix 两次全帧 + sha256 全文件 + fdatasum
  O(4·W·H) 字节）；verify O(W·H)（两次 read_pix 回环 + sha256）。
- 空间 O(W·H)×2×4B（sig/cov 调用方缓冲，本域不复制；verify 内
  逐 HDU 临时 vector W·H×4B）；磁盘 O(W·H)×(bitpix/8)×2 HDU+
  header 块（2880B 对齐）。内存不依赖 tile 数（tile 缓冲在上游
  P3-RSMP 域，max_tiles 守卫 p3_session.cpp:179-193）。

## 10 边界与错误（rc 语义表，2026-09-08 实测锚）

| rc | 枚举 | 触发（p3_output.cpp 锚） | 会话层映射（p3_session.cpp） |
|---|---|---|---|
| 0 | P3_OUT_OK | 发布+验证全成功（:320） | ACS_OK |
| 1 | P3_OUT_PARAM | signal/coverage/wcs/path null、W/H<1（:132-134）、bitpix∉{-32,-64}（:140-145） | ACS_ERR_PARAM（请求级拒绝清单 :97-129 先行） |
| 2 | P3_OUT_IO | cfitsio 调用失败（:135-147/:206-210/:231-239）、flush/fsync/rename 失败（:240-273）、sha256 失败（:279-283/:357-360）、verify 重开失败（:312） | ACS_ERR_IO（:293-295） |
| 3 | P3_OUT_CANCELLED | cancelled_at_row≥0（:198-202）；session 层恒传 -1（:292），取消在采样层 ACS_ERR_CANCELLED（:258-262） | （采样层）ACS_ERR_CANCELLED |

- 失败清理不变量：任一步失败 → unlink(tmp)（或发布后失败 →
  unlink(产物)），**不产生完整假文件、不发布无完整性锚输出**
  （h:41-44 冻结注；IO_003 §6）。g_last_err 承载最近错误摘要
  （:56，last_error 脱敏出口 p3_session.h:33-37）。
- 边界值：W/H∈[1,20000]（会话 :113-114）；abs(dec)≤85° TAN 极点
  守卫（p3_session.cpp:107）；输出四角同半球 P3_WCS_HEMISPHERE
  （p3_wcs.h:26）；max_tiles 请求可降不可升 → ACS_ERR_BUDGET
  （p3_session.cpp:179-193）。

## 11 Oracle

- 独立 FITS/WCS 读取器重开（verify 即进程内 oracle：独立 fits_open_file
  READONLY + 逐 HDU 回环 + sha256 重算，不复用写缓冲状态）；
  SCI-P3 §11 全集为真值面（Oracle 不调用本模块——独立小规模球面
  reference + 独立 FITS/WCS 读取器，PHASE3_RESAMPLE.md §8 同源）。
- 执行测试现状锚（相邻证据，引用不冒认）：tests/unit/
  p3_output_test.cpp 4 段（§12）；WCS oracle=p3_wcs roundtrip
  （:114-131）。

## 12 TEST-DESIGN（TEST-P3-WR-DESIGN-001 冻结，2026-09-08）

可执行 TEST-P3-WR-001 MISSING（P3-FITS-TEST 建立，不冒认）；设计
冻结面如下（承载于 registry 手写页 §独立 synthetic 验证节 + 本节；
双重陈述照 P2-INT/P2-REJ 先例）：

- T1 原子写+mask：64×48 渐变场+分段 mask（x<40），BITPIX=-32，
  prov 全字段 → rc=0、coverage_ok=1、reopen_ok=1、sha256 64hex
  （tests/unit/p3_output_test.cpp:62-88）。
- T2 独立 verify：重开 dims/WCS/BUNIT/checksum/mask 一致 →
  reopen_ok=1、coverage_ok=1、sha256 64hex（:89-100）。
- T3 原子性：无 .tmp 残留（filesystem 目录遍历，WIN-001 替代
  popen；前缀匹配弱匹配偏差 DISP-P3FITS-002 如实，不误报）
  （:101-113）。
- T4 WCS roundtrip oracle：pix→world→pix <1e-4 px + 采样值锚
  sig[24,32]=100.0+0.5·32（:114-131）。
- T5（设计面，现状未覆盖）取消不落盘：cancelled_at_row≥0 → rc=3
  且产物不存在、无 tmp 残留（:198-202 语义；归 P3-FITS-TEST）。
- T6（设计面，现状未覆盖）sha256 注入失败：ASTROCS_HASH_FAIL_INJECT
  → rc=2 且产物被删、result 无哈希（:279-283/:105-108 语义；归
  P3-FITS-TEST）。
- T7（设计面，现状未覆盖）bitpix=-64 全链：写入/回环/校验和
  dtype 语义（归 P3-FITS-TEST）。

## 13 容差与冻结清单（2026-09-08 实测）

- BITPIX ∈ {-32,-64}（kernel :140-145；session 默认 -32 :285）；
  BSCALE=1/BZERO=0 恒定；BUNIT 缺省 "ADU"；CTYPE=TAN/CUNIT=deg。
- W/H ∈ [1,20000]；abs(dec) ≤ 85°；sampler ∈ {nearest,bilinear}；
  parity ∈ {east_left,east_right}（east_left 默认，CD1_1<0）；
  coverage_output = mask（单一合法值）；bitpix/coverage_output 外
  值显式拒（session :127/:129）。
- max_tiles 默认 min(1024, ceil(W·H/512²)+16)，请求可降不可升
  （:179-193）；order_sel ≤ min(20, 输入实际 order)（:196-199）。
- 容差：WCS roundtrip ≤1e-6 px（SCI-P3 §7；执行测试取 1e-4 px
  观测阈 :128-129）；常数场 0（bilinear 权重和=1 构造保证）；
  回环逐值精确（F4 NaN 语义）；sha256 64hex 小写。
- R10-C 发布序（F1）与 sha256 严格封装（F2）为冻结协议，整改归
  P3-FITS-IMPL 时不得放宽（禁前缀哈希/禁半成品发布）。

## 14 现状缺陷登记（DISP-P3FITS-001..002，登记不改码）

- **DISP-P3FITS-001**：lib/astro_image_io/README.md 旧派生内容声称
  "零外部依赖、不依赖 cfitsio"，与现状 vendored third_party/cfitsio
  （astrocs_cfitsio 静态库，根 CMakeLists.txt:273-296 astrocs_aio
  链接；astrocs_aio 含 aio_fits.cpp 等 6 源）矛盾。他域文件只登记
  不修（本任务边界）；事实以本文件 §8 + memory.md 为准。
- **DISP-P3FITS-002**：tmp 命名冻结注与实现偏差——p3_output.h:41-44
  协议注写 `<dir>/.<base>.<pid>.tmp`（前置点隐藏文件形态），实测
  make_temp_path 生成 `out_path.<pid>.tmp`（:81，无前置点、保留
  .fits 扩展名）；同目录 rename 原子性语义不变；执行测试残留检查
  前缀 ".astrocs_p3_out_test."（tests/unit/p3_output_test.cpp:102-103）
  与实际命名恒不匹配 → 残留检查弱匹配空转（不误报亦捕不到本实现
  形态残留）。命名统一归 P3-FITS-IMPL（含测试修正）。
- 整改项（非缺陷，登记不改码）：prov.manifest_hash 恒 nullptr
  （p3_session.cpp:270），HISTORY manifest 字段写空——SCI-P3 §96
  manifest hash 必写，接线归 P3-FITS-IMPL；p3_output_verify 忽略
  wcs 参数（:305 (void)wcs，WCS 一致性由写路径单点保证，:301-302
  注释如实）；DATASUM 为 32-bit 数值校验和非 FITS 标准 ASCII
  CHECKSUM（§6 F1 如实冻结）。

## 15 消费链（生产编排面）

- 唯一编排消费方：lib/phase3_session/p3_session.cpp run 段——
  parse 拒绝清单 :97-129 → p3_wcs_make :161 → p3_sampler_open_ex
  :172-176 → max_tiles 守卫 :179-193 → order_select :196-199 →
  行带采样（std::thread 池 :247-253，取消 :228-229）→ provenance
  填充 :265-277 → **p3_output_write_atomic :287-292**（cancelled_at_row
  恒 -1）→ inspect JSON :296-313（kind/run_id/exit_code/
  output_fits_path/sha256/order_sel_used/sampler_used/
  coverage_stats/provenance）。
- 会话五段式（create/validate/run/inspect/destroy）=API-P3-001
  FROZEN（p3_session.h:16-28）；本域内核消费面=API-P3-FITS-001
  （PUBLIC_API.md 新节，§16）。
- CLI 直调会话不 shell-out（p3_session.h:3 注释）；输出路径默认
  `<hips_dir>/../output_phase3.fits`，请求 output_dir 可覆盖
  （p3_session.cpp:278-284）。

## 16 关联 ID 映射（本文件承接）

- ALG-P3-FITS-IMPL-001 = 本文件（实现级合同；upstream SCI-P3-001 +
  ALG-P3-001 + ALG-P3-002 + ALG-P3-004，PHASE3_RESAMPLE.md §1-§10
  推导权威不重复）。
- DATA-P3-FITS = DATA_SEMANTICS.md §27（in signal f32[W·H] ADU /
  coverage f32{0,1} / out FITS BITPIX/WCS/BUNIT/checksum 唯一权威）。
- API-P3-FITS-001 = PUBLIC_API.md「Phase3 FITS 写出公共消费面」
  （p3_output_write_atomic/p3_output_verify 符号级冻结 + 会话编排
  p3_session 五段镜像 API-P3-001 不变）。
- TEST-P3-WR-001 = 登记面 TEST-P3-WR-DESIGN-001 设计冻结 VERIFIED
  （§12 + docs/modules/registry/astrocs.phase3.writer.md）；可执行
  MISSING 归 P3-FITS-TEST。
- MOD-astrocs-phase3-writer / astrocs.p3.fits_writer /
  astrocs_p3_fits_writer.dll（合同值未建，P3-FITS-IMPL）；
  三件套 lib/phase3_fits/（README r1 + module.yaml CONTRACT_READY
  entrypoint=MISSING + memory.md）。

## 17 追溯

- MATRIX 行：MOD-astrocs-phase3-writer（TRACEABILITY_MATRIX.json/
  csv :20 P3-FITS 行）；合同落位=lib/phase3_fits/ 三件套 + 本文件
  + DATA_SEMANTICS §27 + PUBLIC_API API-P3-FITS-001 节 + registry
  手写页 + docs/modules/phase3_fits.md。
- 零改动声明：docs/science/（SCI-P3 FROZEN）、docs/algorithms/
  PHASE3_RESAMPLE.md（公式/容差零改动）、lib/ 生产源、third_party/
  cfitsio、ci/、tools/、tests/ 本任务零触碰；发现的实现偏差全部
  登记（§14）不反向修改 SCI（模板红线）。
