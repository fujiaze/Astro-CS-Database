# 模块 astrocs.phase3.writer

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 合同：SCI-P3-001（docs/science/PHASE3_HIPS_TO_FITS.md，FROZEN）/
> ALG-P3-FITS-IMPL-001（docs/science/algorithms/PHASE3_FITS_IMPL.md，承接
> ALG-P3-002/004 本域子面）/ DATA-P3-FITS（DATA_SEMANTICS §27）/ API-P3-FITS-001
> （PUBLIC_API Phase3 FITS 写出节）。descriptor 词汇（module_id=astrocs.phase3.writer、
> SCI-P3-WR-001/ALG-P3-004/API-P3-001/TEST-P3-WR-001）与 module_id 合同值
> `astrocs.p3.fits_writer` 的对齐属迁移目标（未落地）。

## 1 身份与合同落位

- 模块: astrocs.p3.fits_writer（module_id 合同值；dll_target=
  astrocs_p3_fits_writer.dll 为迁移合同值，未落地，IMPLEMENTED 只由验收签发；
  现状构建 = astrocs_phase3_session 静态库成员）。
- 合同落位: lib/algorithms/fits_output/ 三件套（CONTRACT_READY；落位规则见
  docs/detail/README.md）。
- 生产源: lib/algorithms/fits_output/p3_output.cpp + 签名头正本 p3_output.h
  + WCS 关键字源 p3_wcs.h。
- 合同链: SCI-P3-001（docs/science/PHASE3_HIPS_TO_FITS.md，FROZEN）→
  ALG-P3-FITS-IMPL-001（docs/science/algorithms/PHASE3_FITS_IMPL.md，兼承接
  ALG-P3-002/004 本域子面）→ DATA-P3-FITS（DATA_SEMANTICS §27）+
  API-P3-FITS-001（PUBLIC_API Phase3 FITS 写出公共消费面节）→ TEST-P3-WR-001
  （设计冻结 = TEST-P3-WR-DESIGN-001，见 §9）；编排面 API-P3-001（p3_session
  五段 FROZEN）镜像不变。
- 上游依赖: astrocs_phase3_session（采样/重采样编排域）+
  astrocs_aio（aio_fits + vendored third_party/cfitsio）；depends_on_int=
  P3-RSMP;IO-003。

## 2 职责与明确非职责

- 职责: 把上游重采样结果（signal+coverage）写成 FITS 单文件
  （主 HDU signal + COVERAGE 扩展 HDU）——WCS/BUNIT/provenance
  关键字全量（SCI-P3 §96 面）、原子发布序（tmp→flush→fsync→rename，
  p3_output.cpp:235-287）、失败/取消清理不发布（h:41-44）、
  发布后 sha256 完整性锚（:92-114 严格封装）与独立重开验证
  （p3_output_verify :296-368）。
- 非职责: 不做重采样/tile 读取（P3-RSMP/采样域）、不做请求解析
  与参数拒绝清单（p3_session run 段）、不做读路径 FITS/HiPS 解析
  （AIO/hips 域）、不改 vendored cfitsio、不改 SCI 公式（SCI-P3
  FROZEN 零改动）。

## 3 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标/dtype |
|---|---|---|---|---|
| `resampled` | `DATA-P3-RES`（descriptor 词汇；实际承载=DATA-P3-FITS §27.1 in 面） | 必 | UnitId::SURFACE_BRIGHTNESS（BUNIT，缺省 ADU） | PIXEL 行主序 f32 [W·H]，W,H∈[1,20000] |
| `fits` | `DATA-P3-FITS` | 可 | UnitId::SURFACE_BRIGHTNESS | FITS 文件 BITPIX=-32/-64 + COVERAGE 扩展 + sha256 |

- invalid 权威源=DATA-P3-FITS §27：signal 无覆盖=NaN（禁 ±Inf
  伪装；NaN==NaN 回环一致 p3_output.cpp:341-344）；coverage 二值门
  >0.5f（:346）；bitpix∉{-32,-64}→PARAM（:140-145）；WCS 守卫
  abs(dec)≤85°+四角同半球（p3_wcs.h:25-26）。
- 端口词汇（resampled/fits、DATA-P3-RES、UnitId/CoordinateFrame）为 descriptor
  派生（p3_writer_descriptor），其与 DATA-P3-FITS 的对齐属迁移目标（未落地），
  不作冻结依据。

## 4 公共 header、核心 symbol 与生命周期

- 内核消费面=API-P3-FITS-001（p3_output.h 签名头正本）:
  `p3_output_write_atomic`（h:45-53，实现 :117-321）、
  `p3_output_verify`（h:57-60，:296-368）+ 数据结构 P3Provenance
  （h:15-24）/P3OutputResult（h:26-32）/P3OutputStatus（h:34-39，
  OK=0/PARAM=1/IO=2/CANCELLED=3）+ WCS 符号 p3_wcs_make/
  p3_wcs_pix2world/p3_wcs_world2pix/p3_wcs_fits_keywords（p3_wcs.h
  :31-46，ALG-P3-002 承接）。
- 会话编排面=API-P3-001 FROZEN（p3_session.h:16-28 五段
  create/validate/run/inspect/destroy + last_error h:33-37 脱敏）；
  生命周期 create→validate→run→inspect→destroy，run 内经
  p3_output_write_atomic 落盘（p3_session.cpp:287-292）。
- 不新增/不修改任何 C 头/C ABI（本页为既有符号展开冻结）。

## 5 Registry descriptor 与配置 schema

- descriptor（p3_writer_descriptor，编排层口径）: module_id=`astrocs.phase3.writer`；
  execution_class=`io`；parallel_ok=False；ports resampled(必)+fits(可)；
  sci_id=SCI-P3-WR-001、alg_id=ALG-P3-004、data_id=DATA-P3-FITS、
  api_id=API-P3-001、test_id=TEST-P3-WR-001。descriptor 派生的占位合同 ID 与
  module_id 合同值 astrocs.p3.fits_writer 的对齐属迁移目标（未落地），
  不作冻结依据。
- 配置=phase config JSON（键集 = API-P3-001）；无独立 schema 文件。

## 6 Execution class、并行轴、ThreadBudget lease、确定性

- execution_class=`io`；写面无并行——cfitsio 进程级互斥 RT-008
  （p3_output.cpp:125 aio::cfitsio_io_mutex 全程，与读路径
  aio_fits.cpp:529 同锁），fits_write_pix 一次全帧行主序，输出
  字节与 worker 数无关（1..N bitwise）。
- 并行仅上游采样（p3_session.cpp:247-253 std::thread 池，worker
  数=budget.max_workers，:209 注释禁 hardware_concurrency；
  本域源码 0 处 #pragma omp——lib/infrastructure/aio/src/aio_fits.cpp:1154 唯一 omp 循环属
  AIO 域非本域）。ThreadLease/取消检查点无接线（迁移整改点）。
- 确定性: 固定顺序输出（fits_write_pix 定序 + sha256/fdatasum
  纯函数）；取消点=行（kernel cancelled_at_row h:52；session 层
  取消在采样循环 :228-229），写面发布序不可中断（IO_003 §6）。

## 7 内存、cache、I-O、所有权

- 内存: 调用方分配 sig/cov 缓冲（O(W·H)×2×4B），本域不复制
  （verify 内逐 HDU 临时 vector 除外）；内存不依赖 tile 数
  （tile 缓冲在上游，max_tiles 守卫 p3_session.cpp:179-193）。
- I-O: 单 writer 串行（cfitsio 锁内）；磁盘临时文件
  `<path>.<pid>.tmp` 同目录（:81；协议注与实测命名的差异见 §10）；
  发布后无 tmp 残留（失败/取消 unlink）。
- 所有权: 输出文件归调用方；sha256 结果归 result 出参
  （P3OutputResult h:26-32）。

## 8 错误、日志、指标、取消和 checkpoint

- 错误: rc 语义 P3_OUT_OK=0/P3_OUT_PARAM=1/P3_OUT_IO=2/
  P3_OUT_CANCELLED=3（p3_output.h:35-39，逐触发锚=ALG-P3-FITS-
  IMPL-001 §10 表）；会话层映射 ACS_OK/ACS_ERR_PARAM/ACS_ERR_IO/
  ACS_ERR_CANCELLED；g_last_err→last_error 脱敏出口（p3_session.h
  :33-37）。
- 失败不变量: 任一步失败 unlink(tmp)/产物，不产生完整假文件、
  不发布无完整性锚输出；sha256 失败→IO 且删产物（:279-283）。
- 日志/指标: inspect JSON（p3_session.cpp:296-313 kind/run_id/
  exit_code/output_fits_path/sha256/order_sel_used/sampler_used/
  coverage_stats/provenance）；无独立 metrics 通道。
- 取消: kernel 行粒度 cancelled_at_row（session 恒 -1 :292）；
  无 checkpoint（原子写整文件单元，SCI-P3 §5c/ALG-P3-001）。

## 9 独立 synthetic 验证命令与容差

- 本节承载 TEST-P3-WR-001 登记面：设计冻结 = TEST-P3-WR-DESIGN-001
  （ALG-P3-FITS-IMPL-001 §12 T1-T7）；可执行测试待建，验收证据待补。
  现状执行测试 eng/tests/unit/p3_output_test.cpp（4 段，
  eng/tests/unit/CMakeLists.txt 注册）= 相邻证据，引用不冒认。
- T1 原子写+mask: 64×48 渐变场+分段 mask、BITPIX=-32、prov 全字段 → rc=0、
  coverage_ok=1、reopen_ok=1、sha256 64hex。
- T2 独立 verify: 重开 dims/像素回环（NaN==NaN）/coverage 二值门/
  sha256 重算一致。
- T3 原子性: 无 .tmp 残留（filesystem 遍历替代 popen；前缀弱匹配差异
  见 §10，不误报）。
- T4 WCS roundtrip oracle: pix→world→pix ≤1e-4 px（SCI-P3 §7
  真值阈 ≤1e-6 px）+ 采样值锚 100.0+0.5·32（≤1e-3）。
- T5-T7 设计面（现状未覆盖，可执行测试待建）: 取消不落盘、
  sha256 注入失败不产假哈希、bitpix=-64 全链。
- 命令面（落地后冻结）: ctest / pytest 接入（验收证据域）。

## 10 已知限制与缺陷登记（登记不改码）

- lib/infrastructure/aio/README.md 的既有表述声称「零外部依赖、不依赖 cfitsio」，
  与现状 vendored third_party/cfitsio（astrocs_cfitsio 静态库，astrocs_aio 链接）
  矛盾；他域文件只登记不修。
- tmp 命名：p3_output.h 协议注写 `<dir>/.<base>.<pid>.tmp`（前置点隐藏形态），
  实测 make_temp_path 生成 `out_path.<pid>.tmp`（p3_output.cpp:81）；同目录
  rename 原子性语义不变，但执行测试残留检查前缀（eng/tests/unit/p3_output_test.cpp:102-103）
  与实际命名不匹配（残留检查空转）；命名统一属迁移目标（未落地，含测试修正）。
- 整改项（非缺陷）: prov.manifest_hash 恒 nullptr（p3_session.cpp:270，HISTORY
  manifest 字段写空，SCI-P3 §96 接线属迁移目标（未落地））；p3_output_verify
  忽略 wcs 参数（p3_output.cpp:319 (void)wcs，设计如此）；DATASUM 为 32-bit
  数值校验和非 FITS 标准 ASCII CHECKSUM（如实冻结）。
- 其余: 见 docs/KNOWN_LIMITATIONS.md 与 ALG-P3-FITS-IMPL-001
  §14 合同边界；SCI-P3 FROZEN 零改动声明（本页不承载公式）。

## NaN 与写端口

- NaN 规则（权威 = `ASTROCS_DESIGN.md` §5.5）：**样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数**；
  剔除项逐条进场级计数。无覆盖/无数据 = NaN；0 与 ±Inf 不作有效值。
- signal 语义 = **面亮度**，写端口 `UnitId::SURFACE_BRIGHTNESS`；输出模式显式声明
  （`surface_brightness` / `point_source_flux` / `visualization`，最高设计 §6.3）。
