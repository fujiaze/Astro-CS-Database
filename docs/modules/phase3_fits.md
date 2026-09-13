---
id: MOD-astrocs-phase3-writer
version: 1.0.0
status: ACTIVE
owner: SA-P3-F27
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-P3-001, ALG-P3-FITS-IMPL-001, ALG-P3-002, ALG-P3-004, API-P3-001]
downstream: [DATA-P3-FITS, API-P3-FITS-001, TEST-P3-WR-001]
---

# 模块 astrocs.p3.fits_writer（P3-FITS-DOC 新建，2026-09-08）

> P3-FITS-DOC（SA-P3-F27，MODULE_MIGRATION_MATRIX P3-FITS 行）新建
> 模块页，冻结合同 `astrocs.p3.fits_writer`（registry 行 ID 沿用
> `MOD-astrocs-phase3-writer`；任务指派文字 astrocs.p3.fits 与矩阵
> astrocs.p3.fits_writer 冲突——矩阵权威，决策记录 lib/phase3_fits/
> memory.md）。合同三件套落位 `lib/phase3_fits/`（README/module.yaml/
> memory.md，按 `lib/phase2_upm/`→`phase2_samp/`→`phase2_rej/`→
> `phase2_int/`→`hips_p2/` 先例新建；`lib/phase3_session/` 为会话
> 编排域共享源 p3_session/p3_wcs/hips_properties/p3_output/
> p3_resample 五源同库 astrocs_phase3_session（根 CMakeLists.txt
> :460-465），不整目录归属，矩阵 legacy_paths="lib/phase3_session
> fits sources" 只圈 fits sources）。合同权威=三件套 +
> docs/science/PHASE3_HIPS_TO_FITS.md（SCI-P3-001，共享 FROZEN V5
> SCI-007 2026-08-28）+ docs/algorithms/PHASE3_RESAMPLE.md
> （ALG-P3-001..004 施工规格，公式零改动）+
> docs/algorithms/PHASE3_FITS_IMPL.md（ALG-P3-FITS-IMPL-001，实现级
> 合同，SCI-P3-WR-001⇒SCI-P3-001 映射声明在其 §5）。descriptor 词汇
> module_id=`astrocs.phase3.writer`（module_adapters.cpp:445-460
> p3_writer_descriptor）为编排层占位，由 P3-FITS-INT 对齐
> `astrocs.p3.fits_writer`，不得反向作为冻结依据。

## 身份与合同落位

- MOD ID：`MOD-astrocs-phase3-writer`（registry 行 ID 沿用）；
  module_id=`astrocs.p3.fits_writer`（矩阵权威）；
  dll_target=`astrocs_p3_fits_writer.dll`（迁移合同值，尚未存在——
  MISSING 语义，由 P3-FITS-IMPL 建立，禁止声明 IMPLEMENTED）；
  现状构建=astrocs_phase3_session 静态库成员。
- 合同落位：`lib/phase3_fits/` 三件套——README r1（合同页）+
  module.yaml（CONTRACT_READY，entrypoint=MISSING，31 键 schema
  astrocs.module-manifest/v1）+ memory.md。
- module_status=CONTRACT_READY 语义：实现存在（生产源实测+编排
  消费方接线）且合同已冻结，模块化迁移（独立 dll/adapter/
  ThreadLease 接线）由 P3-FITS-IMPL 执行。

## 职责与明确非职责

- 职责：signal 主 HDU + COVERAGE 扩展 HDU 的 FITS 单文件写出——
  WCS/BUNIT/provenance 关键字全量（SCI-P3 §96 面）、R10-C 原子
  发布序（tmp→fits_flush_file→close→fsync(fd)→rename，
  p3_output.cpp:221-273）、失败/取消清理不发布（p3_output.h:41-44）、
  sha256 严格封装完整性锚（:92-114）与独立重开验证
  （p3_output_verify :296-368）。
- 非职责：重采样/tile 读取（P3-RSMP 域）、请求解析与参数拒绝
  清单（p3_session run 段 :97-129）、读路径 FITS/HiPS 解析
  （AIO/hips 域）、vendored cfitsio 修改、SCI 公式改动。

## 输入输出端口、DATA、单位、坐标、invalid

- in：signal f32[W·H]（BUNIT 缺省 ADU，无覆盖=NaN）+ coverage
  f32[W·H]（二值门 >0.5f）+ P3WcsDescriptor（CRPIX FITS 1-based、
  CD deg/px、TAN、abs(dec)≤85° 四角同半球）+ bunit/prov/bitpix
  （-32|-64）/output_path/cancelled_at_row——唯一权威=DATA-P3-FITS
  §27.1。
- out：FITS 文件（BITPIX=-32/-64，CTYPE=RA---TAN/DEC--TAN，
  CUNIT=deg，BSCALE=1/BZERO=0，HIPSID/RUNID/ORDERSEL/SAMPLER/SWVER
  +HISTORY，DATASUM 32-bit）+ P3OutputResult（sha256[65]/
  coverage_ok/reopen_ok/covered_px/total_px）——§27.2。
- rc：P3_OUT_OK=0/P3_OUT_PARAM=1/P3_OUT_IO=2/P3_OUT_CANCELLED=3
  （h:34-39）；端口词汇（resampled/fits、DATA-P3-RES）为 descriptor
  派生由 P3-FITS-INT 对齐。

## 公共 header、核心 symbol 与生命周期

- 内核消费面=API-P3-FITS-001（p3_output.h 64 行唯一权威签名头）：
  p3_output_write_atomic（h:45-53，实现 :117-321）/p3_output_verify
  （h:57-60，:296-368）+ P3Provenance/P3OutputResult/P3OutputStatus
  + p3_wcs_make/p3_wcs_pix2world/p3_wcs_world2pix/
  p3_wcs_fits_keywords（p3_wcs.h:31-46）。
- 编排面=API-P3-001 FROZEN（p3_session.h:16-28 五段 C ABI +
  last_error h:33-37）；生命周期 create→validate→run→inspect→
  destroy；不新增/不修改任何 C 头/C ABI。

## Registry descriptor 与配置 schema

- descriptor（编排层占位，module_adapters.cpp:445-460）：
  module_id=astrocs.phase3.writer、execution_class=io、
  parallel_ok=false、ports resampled(必)+fits(可)、sci_id=
  SCI-P3-WR-001、alg_id=ALG-P3-004、test_id=TEST-P3-WR-001——由
  P3-FITS-INT 对齐本 manifest 与 README r1，不作冻结依据。
- 配置=phase config JSON（PHASE API 文档）；无独立 schema 文件。

## Execution class、并行轴、ThreadBudget lease、确定性

- io 类；写面单线程串行（cfitsio 进程锁 RT-008，p3_output.cpp:125
  与 aio_fits.cpp:529 共锁）；并行仅上游采样 std::thread 池
  （p3_session.cpp:247-253，worker=budget.max_workers，:209 注释禁
  hardware_concurrency；本域 0 处 #pragma omp）；ThreadLease/取消
  检查点无接线（P3-FITS-IMPL 整改项）。
- 确定性：输出与 worker 数无关（1..N bitwise）；fits_write_pix
  定序 + sha256/fdatasum 纯函数；取消点=行（kernel cancelled_at_row，
  session 恒 -1），发布序不可中断。

## 内存、cache、I-O、所有权

- 内存 O(W·H)×2×4B（调用方缓冲，本域不复制），不依赖 tile 数
  （max_tiles 守卫 p3_session.cpp:179-193）；I-O 单 writer 串行，
  tmp=`<path>.<pid>.tmp` 同目录（DISP-P3FITS-002 命名偏差登记）；
  所有权=输出文件与 result 归调用方。

## 错误、日志、指标、取消和 checkpoint

- rc 语义+会话层 ACS_ERR_* 映射（ALG-P3-FITS-IMPL-001 §10 表）；
  失败不变量 unlink(tmp/产物) 不留假文件；inspect JSON 摘要
  （p3_session.cpp:296-313）；无 checkpoint（整文件原子单元）。

## 独立 synthetic 验证命令与容差

- 双重陈述：登记面=TEST-P3-WR-DESIGN-001 设计冻结 VERIFIED
  （ALG-P3-FITS-IMPL-001 §12 T1-T7 + registry 手写页
  astrocs.phase3.writer.md §9 锚）；可执行 TEST-P3-WR-001 MISSING
  归 P3-FITS-TEST；现状执行测试 tests/unit/p3_output_test.cpp
  （116 行 4 段）=相邻证据引用不冒认。
- 容差：WCS roundtrip ≤1e-6 px（SCI-P3 §7 真值，执行测试观测阈
  1e-4 px）；采样值锚 ≤1e-3；sha256 64hex；逐值精确回环
  （NaN==NaN 一致）。

## 已知限制

- DISP-P3FITS-001（AIO README cfitsio 表述矛盾）/DISP-P3FITS-002
  （tmp 命名 h:41-44 vs cpp:81+测试残留检查弱匹配）登记不改码；
  整改项：manifest_hash 恒 nullptr（p3_session.cpp:270，SCI-P3
  §96 归 P3-FITS-IMPL）、verify 忽略 wcs（(void)wcs 设计如此）、
  DATASUM 非 FITS 标准 ASCII CHECKSUM（如实冻结）。
- 其余见 docs/KNOWN_LIMITATIONS.md 与 ALG-P3-FITS-IMPL-001 §14。

## 链接

- SCI: docs/science/PHASE3_HIPS_TO_FITS.md（SCI-P3-001，FROZEN）
- ALG: docs/algorithms/PHASE3_FITS_IMPL.md（ALG-P3-FITS-IMPL-001）
  + docs/algorithms/PHASE3_RESAMPLE.md（ALG-P3-001..004，零改动）
- DATA: docs/contracts/DATA_SEMANTICS.md §27（DATA-P3-FITS）
- API: docs/contracts/PUBLIC_API.md API-P3-FITS-001 节 +
  API-P3-001（FROZEN 镜像）
- TEST: TEST-P3-WR-DESIGN-001（ALG-P3-FITS-IMPL-001 §12）/
  TEST-P3-WR-001 MISSING（P3-FITS-TEST）
- Registry 手写页: docs/modules/registry/astrocs.phase3.writer.md
