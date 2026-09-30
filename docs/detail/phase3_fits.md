# 模块 astrocs.p3.fits_writer

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 合同：SCI-P3-001（docs/science/PHASE3_HIPS_TO_FITS.md，FROZEN）/ ALG-P3-001..004
> （docs/science/algorithms/PHASE3_RESAMPLE.md 施工规格，公式零改动）/
> ALG-P3-FITS-IMPL-001（docs/science/algorithms/PHASE3_FITS_IMPL.md，实现级合同，
> SCI-P3-WR-001⇒SCI-P3-001 映射声明在其 §5）。合同三件套落位
> `lib/algorithms/fits_output/`（README/module.yaml/memory.md，CONTRACT_READY；
> 落位规则见 docs/detail/README.md）；legacy 生产源在 `lib/phase3_session/`
> （astrocs_phase3_session 五源同库，其中 fits 写出源归属本模块）。descriptor 词汇
> module_id=`astrocs.phase3.writer`（p3_writer_descriptor）为编排层口径，其对齐属
> 迁移目标（未落地）；冻结依据 = ALG-P3-FITS-IMPL-001。

## 身份与合同落位

- MOD ID：`MOD-astrocs-phase3-writer`；module_id = `astrocs.p3.fits_writer`；
  dll_target = `astrocs_p3_fits_writer.dll`（迁移合同值，未落地，IMPLEMENTED 只由
  验收签发）；现状构建 = astrocs_phase3_session 静态库成员。
- 合同落位：`lib/algorithms/fits_output/` 三件套（README + module.yaml
  CONTRACT_READY，schema astrocs.module-manifest/v1 + memory.md）。
- module_status=CONTRACT_READY 语义：实现存在（生产源实测+编排
  消费方接线）且合同已冻结，模块化迁移（独立 dll/adapter/
  ThreadLease 接线）为迁移目标（未落地）。

## 职责与明确非职责

- 职责：signal 主 HDU + COVERAGE 扩展 HDU 的 FITS 单文件写出——
  WCS/BUNIT/provenance 关键字全量（SCI-P3 §96 面）、原子发布序
  （tmp→fits_flush_file→close→fsync(fd)→rename，
  p3_output.cpp）、失败/取消清理不发布（p3_output.h）、
  sha256 严格封装完整性锚（p3_output.h）与独立重开验证
  （p3_output_verify，p3_output.cpp）。
- 非职责：重采样/tile 读取（P3-RSMP 域）、请求解析与参数拒绝
  清单（p3_session run 段）、读路径 FITS/HiPS 解析
  （AIO/hips 域）、vendored cfitsio 修改、SCI 公式改动。

## 输入输出端口、DATA、单位、坐标、invalid

- in：signal f32[W·H]（BUNIT 缺省 ADU，无覆盖=NaN）+ coverage
  f32[W·H]（二值门 >0.5f）+ P3WcsDescriptor（CRPIX FITS 1-based、
  CD deg/px、TAN、abs(dec)≤85° 四角同半球）+ bunit/prov/bitpix
  （-32|-64）/output_path/cancelled_at_row——权威源=DATA-P3-FITS
  §27.1。
- out：FITS 文件（BITPIX=-32/-64，CTYPE=RA---TAN/DEC--TAN，
  CUNIT=deg，BSCALE=1/BZERO=0，HIPSID/RUNID/ORDERSEL/SAMPLER/SWVER
  +HISTORY，DATASUM 32-bit）+ P3OutputResult（sha256[65]/
  coverage_ok/reopen_ok/covered_px/total_px）——§27.2。
- rc：P3_OUT_OK=0/P3_OUT_PARAM=1/P3_OUT_IO=2/P3_OUT_CANCELLED=3
  （p3_output.h）；端口词汇（resampled/fits、DATA-P3-RES）为 descriptor
  派生（其对齐属迁移目标，未落地）。

## 公共 header、核心 symbol 与生命周期

- 内核消费面=API-P3-FITS-001（p3_output.h 176 行签名头正本）：
  p3_output_write_atomic（p3_output.h 声明，p3_output.cpp 实现）/p3_output_verify
  （p3_output.h 声明，p3_output.cpp 实现）+ P3Provenance/P3OutputResult/P3OutputStatus
  + p3_wcs_make/p3_wcs_pix2world/p3_wcs_world2pix/
  p3_wcs_fits_keywords（p3_wcs.h）。
- 编排面=API-P3-001 FROZEN（p3_session.h 五段 C ABI +
  last_error，见 p3_session.h）；生命周期 create→validate→run→inspect→
  destroy；不新增/不修改任何 C 头/C ABI。

## Registry descriptor 与配置 schema

- descriptor（p3_writer_descriptor）：
  module_id=astrocs.phase3.writer、execution_class=io、
  parallel_ok=false、ports resampled(必)+fits(可)、sci_id=
  SCI-P3-WR-001、alg_id=ALG-P3-004、test_id=TEST-P3-WR-001——descriptor 与本 manifest/README 的对齐属迁移目标（未落地），不作冻结依据。
- 配置=phase config JSON（PHASE API 文档）；无独立 schema 文件。

## Execution class、并行轴、ThreadBudget lease、确定性

- io 类；写面单线程串行（cfitsio 进程锁 RT-008，p3_output.cpp
  与 aio_fits.cpp 共锁）；并行仅上游采样 std::thread 池
  （p3_session.cpp，worker=budget.max_workers，其注释禁
  hardware_concurrency；本域 0 处 #pragma omp）；ThreadLease/取消
  检查点无接线（整改项）。
- 确定性：输出与 worker 数无关（1..N bitwise）；fits_write_pix
  定序 + sha256/fdatasum 纯函数；取消点=行（kernel cancelled_at_row，
  session 恒 -1），发布序不可中断。

## 内存、cache、I-O、所有权

- 内存 O(W·H)×2×4B（调用方缓冲，本域不复制），不依赖 tile 数
  （max_tiles 守卫 p3_session.cpp）；I-O 单 writer 串行，
  tmp=`<path>.<pid>.tmp` 同目录（命名差异见「已知限制」）；
  所有权=输出文件与 result 归调用方。

## 错误、日志、指标、取消和 checkpoint

- rc 语义+会话层 ACS_ERR_* 映射（ALG-P3-FITS-IMPL-001 §10 表）；
  失败不变量 unlink(tmp/产物) 不留假文件；inspect JSON 摘要
  （p3_session.cpp）；无 checkpoint（整文件原子单元）。

## 独立 synthetic 验证命令与容差

- 登记面 = TEST-P3-WR-DESIGN-001 设计冻结（ALG-P3-FITS-IMPL-001 §12 T1-T7；
  registry 登记页 §9 同源）；可执行 TEST-P3-WR-001 待建；现状执行测试
  eng/tests/unit/p3_output_test.cpp（4 段）= 相邻证据，引用不冒认。
- 容差：WCS roundtrip ≤1e-8 px（SCI-P3 §7 真值；适用域与门限由
  `p3_wcs_applicability()` 单一事实源给出，执行测试观测阈 1e-4 px）；
  采样值锚 ≤1e-3；sha256 64hex；逐值精确回环
  （NaN==NaN 一致）。

## 已知限制

- AIO README 的 cfitsio 表述与 vendored 现状矛盾（他域文件只登记不修）；
  tmp 命名协议注（p3_output.h）与实测（p3_output.cpp）差异 + 测试残留检查前缀
  弱匹配（登记不改码）。
- 整改项：manifest_hash 恒 nullptr（p3_session.cpp，SCI-P3 §96 接线
  未落地）、verify 忽略 wcs（(void)wcs 设计如此）、DATASUM 非 FITS 标准
  ASCII CHECKSUM（如实冻结）。
- 其余见 docs/KNOWN_LIMITATIONS.md 与 ALG-P3-FITS-IMPL-001 §14。

## 链接

- SCI: docs/science/PHASE3_HIPS_TO_FITS.md（SCI-P3-001，FROZEN）
- ALG: docs/science/algorithms/PHASE3_FITS_IMPL.md（ALG-P3-FITS-IMPL-001）
  + docs/science/algorithms/PHASE3_RESAMPLE.md（ALG-P3-001..004，零改动）
- DATA: docs/science/DATA_SEMANTICS.md §27（DATA-P3-FITS）
- API: docs/engineering/PUBLIC_API.md API-P3-FITS-001 节 +
  API-P3-001（FROZEN 镜像）
- TEST: TEST-P3-WR-DESIGN-001（ALG-P3-FITS-IMPL-001 §12）/
  TEST-P3-WR-001（待建）
- Registry 登记页: docs/detail/registry/astrocs.phase3.writer.md
