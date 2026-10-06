# 模块 acsd.phase3.writer

> 上游：`docs/ACSD_DESIGN.md`「模块与 ABI」一节与「流程」一节（投影到平面、
> WCS直接计算生成）、「输出模式显式声明」一节与「I/O 与原子产品」一节（输出不带权重、
> 原子提交）
> 科学正本：docs/science/PHASE3_HIPS_TO_FITS.md（SCI-P3-001，FROZEN，零改动；
> 相关章节（面、真值阈与判据三节）［S-1］
> 算法正本：docs/science/algorithms/PHASE3_FITS_IMPL.md（ALG-P3-FITS-IMPL-001；
> 错误触发锚、T1–T7与合同边界三节）；承接ALG-P3-002 / ALG-P3-004本域子面
> 数据正本：docs/detail/registry/acsd.phase3.writer.md（DATA-P3-FITS 输出行与 HDU 合同，本页输入输出端口表）；
> 单位定义与 BUNIT 语义 = `docs/science/unified/DATA_SEMANTICS.md`「面亮度单位的推导」与「单位与量纲表」两节，规则项见同文件「判据与误差」一节
> API 正本：docs/engineering/api/PUBLIC_API.md（API-P3-FITS-001，Phase3 FITS 写出
> 公共消费面节）、docs/engineering/api/PUBLIC_API.md「分阶段 API 面」（API-P3-001，p3_session 五段
> FROZEN 镜像）
> 原子发布：docs/engineering/contracts/ATOMIC_PUBLISH.md（IO_003，章节「错误语义」）
> 落地设计：`docs/detail/export/pipeline.md`「FITS 产品」一节
> 引用文献：见文末「参考文献」（角标用全角 `［N］`，因本文正文的半角 `[...]` 已被
> 数值域区间占用）

## 1 身份与合同落位

- 模块: acsd.p3.fits_writer（module_id 合同值；dll_target=
  acsd_p3_fits_writer.dll 为迁移合同值，未落地，IMPLEMENTED 只由验收签发；
  现状构建 = acsd_phase3_session 静态库成员）。registry 行 ID =
  `MOD-acsd-phase3-writer`。
- **module_status = CONTRACT_READY 语义**：实现存在（生产源实测 + 编排消费方
  接线）且合同已冻结；模块化迁移（独立 dll / adapter / ThreadLease 接线）为迁移
  目标（未落地）。
- 合同落位: lib/algorithms/fits_output/ 三件套（README/module.yaml/memory.md，
  CONTRACT_READY；schema `acsd.module-manifest/v1`）。legacy 生产源在
  `lib/phase3_session/`（acsd_phase3_session 五源同库，其中 fits 写出源归属
  本模块）。
- 生产源: lib/algorithms/fits_output/p3_output.cpp + 签名头正本 p3_output.h
  + WCS 关键字源 p3_wcs.h。
- 合同链: SCI-P3-001（docs/science/PHASE3_HIPS_TO_FITS.md，FROZEN）→
  ALG-P3-FITS-IMPL-001（docs/science/algorithms/PHASE3_FITS_IMPL.md，兼承接
  ALG-P3-002/004 本域子面）→ DATA-P3-FITS（本页输入输出端口表）+
  API-P3-FITS-001（PUBLIC_API Phase3 FITS 写出公共消费面节）→ TEST-P3-WR-001
  （设计冻结 = TEST-P3-WR-DESIGN-001，见本页「独立 synthetic 验证命令与容差」）；
  编排面 API-P3-001（p3_session
  五段 FROZEN）镜像不变。
- 上游依赖: acsd_phase3_session（采样/重采样编排域）+
  acsd_aio（aio_fits + vendored third_party/cfitsio）；depends_on_int=
  P3-RSMP;IO-003。

## 2 职责与明确非职责

- 职责: 把上游重采样结果（signal+coverage）写成 FITS 单文件
  （主 HDU signal + COVERAGE 扩展 HDU）——WCS/BUNIT/provenance
  关键字全量（SCI-P3 面）、原子发布序（tmp→`fits_flush_file`→close→
  `fsync(fd)`→rename，
  `lib/algorithms/fits_output/p3_output.cpp`）、失败/取消清理不发布（签名面见
  `lib/algorithms/fits_output/p3_output.h`）、发布后 sha256 完整性锚（同上两文件，
  严格封装）与独立重开验证 `p3_output_verify`（同上 `p3_output.cpp`）。
- 非职责: 不做重采样/tile 读取（P3-RSMP/采样域）、不做请求解析
  与参数拒绝清单（p3_session run 段）、不做读路径 FITS/HiPS 解析
  （AIO/hips 域）、不改 vendored cfitsio、不改 SCI 公式（SCI-P3
  FROZEN 零改动）。

## 3 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标/dtype |
|---|---|---|---|---|
| `resampled` | `DATA-P3-RES`（descriptor 词汇；实际承载=DATA-P3-FITS in 面） | 必 | UnitId::SURFACE_BRIGHTNESS（BUNIT，缺省 ADU） | PIXEL 行主序 f32 [W·H]，W,H∈[1,20000] |
| `fits` | `DATA-P3-FITS` | 可 | UnitId::SURFACE_BRIGHTNESS | FITS 文件 BITPIX=-32/-64 + COVERAGE 扩展 + sha256 |

- invalid 权威源=DATA-P3-FITS：signal 无覆盖=NaN（禁 ±Inf
  伪装；NaN==NaN 回环一致 `lib/algorithms/fits_output/p3_output.cpp`）；coverage 二值门
  >0.5f（同上文件）；bitpix∉{-32,-64}→PARAM（同上文件）；WCS 守卫
  abs(dec)≤85°+四角同半球（`lib/algorithms/projection/p3_wcs.h`）。
- 端口词汇（resampled/fits、DATA-P3-RES、UnitId/CoordinateFrame）为 descriptor
  派生（p3_writer_descriptor），其与 DATA-P3-FITS 的对齐属迁移目标（未落地），
  不作冻结依据。
- **输出形态**：交付物为**裸 FITS，不压缩、不套壳**；Phase3 不产出 HiPS，不使用
  `.hips` / `.hips.zst` 命名。输入 HiPS 产品的落盘形态由落盘名判定（裸/归档同义），
  输入合同**不设** `storage_form` 键，出现即 REJECT。
- **输出不需要带权重** —— 上游已完成叠加，这里只投影到平面并直接计算生成对应
  WCS。PRIMARY = 所选科学 signal / flux / statistic；扩展 HDU = COVERAGE、
  VARIANCE / IVAR（**语义择一且一致**）；其余候选面（VALIDITY、SUPPORT、
  REJECTION、POINT_INFORMATION/W、PSF 表/图与 correlation 描述）为待实现项，
  落盘前须先在本页「输入输出端口、DATA、单位、坐标、invalid」立输出行与 HDU 合同。
  标准 WCS 为**直接计算生成**；DATASUM / CHECKSUM 的口径与整改登记见本页
  「已知限制与缺陷登记（登记不改码）」。
- **BUNIT 语义**：主 HDU 的 `BUNIT` = 输入 HiPS `signal/properties#BUNIT` 声明的
  canonical 串（canonical 值 `ADU/sr`；写端口单位 `UnitId::SURFACE_BRIGHTNESS`，
  落盘值 = 通量和 / 覆盖面积 = 面亮度）。缺声明时按`docs/science/unified/DATA_SEMANTICS.md`「面亮度单位的推导」一节的量纲可判条件
  处理 —— `BUNIT = "ADU"` 要求 provenance 声明
  `pixel_semantics = "surface_brightness"`；`VARIANCE` / `IVAR` 扩展 HDU 的
  `BUNIT` = 主 HDU BUNIT 的平方 / 倒数（`FZ-P3-BUNIT-QUADRATIC`）。单位口径唯一
  权威 = `docs/science/unified/DATA_SEMANTICS.md`「面亮度单位的推导」与「单位与量纲表」两节。
- **provenance**：源 product / hash、软件完整 SHA、配置、投影、核、order、近似、
  生成时间。
- **节点产物**：`output_phase3.fits`、`p3_writer.json`（写侧自述）、`p3_verify.json`
  （独立复核面）。所有 HDU shape / WCS 对齐。
- **out 面细节**（DATA-P3-FITS）：FITS 文件 BITPIX = -32 / -64、
  CTYPE = `RA---TAN` / `DEC--TAN`、CUNIT = deg、BSCALE = 1 / BZERO = 0、
  HIPSID / RUNID / ORDERSEL / SAMPLER / SWVER + HISTORY、DATASUM（32-bit）；
  上述头卡与数据模型的依据 = FITS 标准［1］［2］。`P3OutputResult` = `sha256[65]` /
  `coverage_ok` / `reopen_ok` / `covered_px` /
  `total_px`。
- **不确定度可得性（fail-closed，唯一出口）**：输入 HiPS 不含 variance / ivar
  子产品（或权重非纯逆方差、发生fallback等规则项）时 → **不写**
  VARIANCE / IVAR 扩展 HDU（禁静默丢弃、禁用常量 0 冒充）+ manifest 写
  `uncertainty_available=false` + diagnostics 标红计数；**该键不是失败态**，是
  unavailable 显式登记模式。正本 = `docs/science/unified/DATA_SEMANTICS.md`「状态与失败语义」一节
  （禁占位 / 静默缺键 / 空输出冒充）。

## 4 公共 header、核心 symbol 与生命周期

- 内核消费面=API-P3-FITS-001（p3_output.h 签名头正本）:
  `p3_output_write_atomic`（声明与实现分别在 `lib/algorithms/fits_output/p3_output.h`
  与 `lib/algorithms/fits_output/p3_output.cpp`）、`p3_output_verify`（同上两文件）
  + 数据结构 P3Provenance/P3OutputResult/P3OutputStatus（均定义于
  `lib/algorithms/fits_output/p3_output.h`，OK=0/PARAM=1/IO=2/CANCELLED=3）
  + WCS 符号 p3_wcs_make/p3_wcs_pix2world/p3_wcs_world2pix/p3_wcs_fits_keywords
  （`lib/algorithms/projection/p3_wcs.h`，ALG-P3-002 承接）。
- 会话编排面=API-P3-001 FROZEN（`lib/phase3_session/p3_session.h` 五段
  create/validate/run/inspect/destroy + last_error 脱敏，同一签名头）；
  生命周期 create→validate→run→inspect→destroy，run 内经
  p3_output_write_atomic 落盘（`lib/phase3_session/p3_session.cpp`）。
- 不新增/不修改任何 C 头/C ABI（本页为既有符号展开冻结）。

## 5 Registry descriptor 与配置 schema

- descriptor（p3_writer_descriptor，编排层口径）: module_id=`acsd.phase3.writer`；
  execution_class=`io`；parallel_ok=False；ports resampled(必)+fits(可)；
  sci_id=SCI-P3-WR-001、alg_id=ALG-P3-004、data_id=DATA-P3-FITS、
  api_id=API-P3-001、test_id=TEST-P3-WR-001。descriptor 派生的占位合同 ID 与
  module_id 合同值 acsd.p3.fits_writer 的对齐属迁移目标（未落地），
  不作冻结依据。
- 配置=phase config JSON（键集 = API-P3-001）；无独立 schema 文件。
- 模块注册 = `lib/infrastructure/pipeline/module_ports.registry.json` 的
  `acsd.phase3.writer`；生产接线 =
  lib/infrastructure/scheduler/src/module_adapters.cpp 的 `p3_op_writer`。

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `band_height` | —— | px | 输出行带高度（内存预算） |
| `tile_cache_mb` | —— | MB | tile 缓存上限 |
| `compression` | `none` | —— | 压缩选项（无 / 无损；交付为裸 FITS，不套壳） |
| `mode` | `surface_brightness` | —— | surface_brightness / point_source_flux / visualization |

写入序 = 写临时文件 → flush/close/fsync → 标准 checksum → 原子 rename → 重开
独立验证；复用 infrastructure/aio 的原子提交设施（临时文件隔离 + 校验 + fsync +
原子 rename；**单写者前提**：同一输出路径同一时刻只有一个写者，**跨进程不取文件
锁**）。流式：按行带 / 块执行，常驻内存以「输出宽度 × 行带高度 + tile 缓存」为
上界；cache 只缓存，**不改变 order / 核 / 科学值**；线程预算来自 Runtime，
**并行输出与单线程科学结果一致**。

## 6 Execution class、并行轴、ThreadBudget lease、确定性

- execution_class=`io`；写面无并行——cfitsio 进程级互斥 RT-008
  （`lib/algorithms/fits_output/p3_output.cpp` 全程持 aio::cfitsio_io_mutex，与读路径
  `lib/infrastructure/aio/src/aio_fits.cpp` 同锁），fits_write_pix 一次全帧行主序，输出
  字节与 worker 数无关（1..N bitwise）。
- 并行仅上游采样（`lib/phase3_session/p3_session.cpp` 的 std::thread 池，worker
  数=budget.max_workers，同文件注释禁 hardware_concurrency；
  本域源码 0 处 #pragma omp——`lib/infrastructure/aio/src/aio_fits.cpp` 的唯一 omp 循环属
  AIO 域非本域）。ThreadLease/取消检查点无接线（迁移整改点）。
- 确定性: 固定顺序输出（fits_write_pix 定序 + sha256/fdatasum
  纯函数）；取消点=行（kernel cancelled_at_row，签名见 `lib/algorithms/fits_output/p3_output.h`；session 层
  取消在采样循环，同 `p3_session.cpp`），写面发布序不可中断（IO_003 的
  「发布流水线（原子语义）」）。

## 7 内存、cache、I-O、所有权

- 内存: 调用方分配 sig/cov 缓冲（O(W·H)×2×4B），本域不复制
  （verify 内逐 HDU 临时 vector 除外）；内存不依赖 tile 数
  （tile 缓冲在上游，max_tiles 守卫在 `lib/phase3_session/p3_session.cpp`）。
- I-O: 单 writer 串行（cfitsio 锁内）；磁盘临时文件
  `<path>.<pid>.tmp` 同目录（`lib/algorithms/fits_output/p3_output.cpp` 的 make_temp_path；协议注与实测命名的差异见本页「已知限制与缺陷登记（登记不改码）」）；
  发布后无 tmp 残留（失败/取消 unlink）。
- 所有权: 输出文件归调用方；sha256 结果归 result 出参
  （P3OutputResult，定义见 `lib/algorithms/fits_output/p3_output.h`）。

## 8 错误、日志、指标、取消和 checkpoint

- 错误: rc 语义 P3_OUT_OK=0/P3_OUT_PARAM=1/P3_OUT_IO=2/
  P3_OUT_CANCELLED=3（`lib/algorithms/fits_output/p3_output.h`，逐触发锚=ALG-P3-FITS-
  IMPL-001［A-1］表）；会话层映射 ACS_OK/ACS_ERR_PARAM/ACS_ERR_IO/
  ACS_ERR_CANCELLED；g_last_err→last_error 脱敏出口（`lib/phase3_session/p3_session.h`）。
- 失败不变量: 任一步失败 unlink(tmp)/产物，不产生完整假文件、
  不发布无完整性锚输出；sha256 失败→IO 且删产物（`lib/algorithms/fits_output/p3_output.cpp`）。
  取消 → 无可见半成品（临时文件隔离 + 原子 rename）。
- 输出模式缺所需科学层 → 拒绝或明确 unavailable；BUNIT 与实际量纲不一致
  （`bunit="ADU"` vs 面亮度口径）⇒ 必须显式失败或标注 unavailable，**声明值以
  实际量纲为准**（`"ADU"` 只在量纲一致时使用）；>2 GiB、长 UTF-8 路径、
  Windows CFITSIO、取消、缺 tile 必须正确处理。
- 日志/指标: inspect JSON（`lib/phase3_session/p3_session.cpp` 的 kind/run_id/
  exit_code/output_fits_path/sha256/order_sel_used/sampler_used/
  coverage_stats/provenance）；无独立 metrics 通道。
- 取消: kernel 行粒度 cancelled_at_row（session 恒 -1，见 `lib/phase3_session/p3_session.cpp`）；
  无 checkpoint（原子写整文件单元，SCI-P3/ALG-P3-001）。

## 9 独立 synthetic 验证命令与容差

- 本节承载 TEST-P3-WR-001 登记面：设计冻结 = TEST-P3-WR-DESIGN-001
  （ALG-P3-FITS-IMPL-001 T1-T7）；可执行测试待建，验收证据待补。
  已取证的相邻读数为 4 段写出测试，引用不冒认。
- T1 原子写+mask: 64×48 渐变场+分段 mask、BITPIX=-32、prov 全字段 → rc=0、
  coverage_ok=1、reopen_ok=1、sha256 64hex。
- T2 独立 verify: 重开 dims/像素回环（NaN==NaN）/coverage 二值门/
  sha256 重算一致。
- T3 原子性: 无 .tmp 残留（filesystem 遍历替代 popen；前缀弱匹配差异
  见本页「已知限制与缺陷登记（登记不改码）」，不误报）。
- T4 WCS roundtrip oracle: pix→world→pix ≤1e-4 px（SCI-P3
  真值阈 ≤1e-6 px）+ 采样值锚 100.0+0.5·32（≤1e-3）。
- T5-T7 设计面（现状未覆盖，可执行测试待建）: 取消不落盘、
  sha256 注入失败不产假哈希、bitpix=-64 全链。
- **可复现轮 T05–T10 诚实化（T-EXP-01..03 对称锚）**：输出测试组 1a/1b、3b、4c 四段载体已撤，本页 T1–T4 登记为设计冻结面（TEST-P3-WR-DESIGN-001），**不可验收（载体已撤）**；完整三项划分见 ALG-P3-FITS-IMPL-001 [S-6]「Oracle」一节 T-EXP-01..03。WCS roundtrip oracle 面有效（合同紧门 1e-8 px），执行证据待补。
- **容差登记**：WCS roundtrip ≤ 1e-8 px（SCI-P3 真值；适用域与门限由
  `p3_wcs_applicability()` 单一事实源给出，执行测试观测阈 1e-4 px）；采样值锚
  ≤ 1e-3；sha256 64hex；逐值精确回环（NaN == NaN 一致）。
- 命令面（落地后冻结）: 构建产物侧的用例接入（验收证据域）。
- Oracle 面补充：标准 FITS 验证器（checksum / 结构 / WCS）；重开独立验证内容与
  写入一致；不同 block / cache / worker 输出科学值一致；取消 / 失败无半成品；
  >2 GiB 与长路径测试。

## 10 已知限制与缺陷登记（登记不改码）

- lib/infrastructure/aio/README.md 的既有表述声称「零外部依赖、不依赖 cfitsio」，
  与现状 vendored third_party/cfitsio（acsd_cfitsio 静态库，acsd_aio 链接）
  矛盾；他域文件只登记不修。
- tmp 命名：p3_output.h 协议注写 `<dir>/.<base>.<pid>.tmp`（前置点隐藏形态），
  实测 make_temp_path 生成 `out_path.<pid>.tmp`（`lib/algorithms/fits_output/p3_output.cpp`）；同目录
  rename 原子性语义不变，但残留检查所用前缀与实际命名不匹配（残留检查空转）；
  命名统一属迁移目标（未落地，含用例修正）。
- 整改项（非缺陷）: prov.manifest_hash 恒 nullptr（`lib/phase3_session/p3_session.cpp`，HISTORY
  manifest 字段写空，SCI-P3 接线属迁移目标（未落地））；p3_output_verify
  忽略 wcs 参数（`lib/algorithms/fits_output/p3_output.cpp` 内 `(void)wcs`，设计如此）；DATASUM 为 32-bit
  数值校验和，不是 FITS 标准的 ASCII CHECKSUM 约定［1］（如实冻结）。
- 其余: 见 ALG-P3-FITS-IMPL-001 合同边界；全局限制登记 =
  artifacts/evidence/known-limitations-ledger/LIMITATIONS.md；
  SCI-P3 FROZEN 零改动声明（本页不承载公式）。

## NaN 与写端口

- NaN规则（权威 = `ACSD_DESIGN.md`「逐像素排异」一节）：**样本级掩膜 + 重归一 + 覆盖级NaN + 强制计数**；
  剔除项逐条进场级计数。无覆盖/无数据 = NaN；0 与 ±Inf 不作有效值。
- signal 语义 = **面亮度**，写端口 `UnitId::SURFACE_BRIGHTNESS`；输出模式显式声明
  （`surface_brightness` / `point_source_flux` / `visualization`，最高设计「输出模式显式声明」一节）。

## 参考文献

- ［1］ IAU FITS Working Group. (2016). *FITS Standard*, Version 4.0.
  永久链接 [fits.gsfc.nasa.gov/fits_standard.html](https://fits.gsfc.nasa.gov/fits_standard.html)
- ［2］ Pence, W. D.; Chiappetti, L.; Page, C. G.; Shaw, R. A.; Stobie, E. (2010).
  "Definition of the Flexible Image Transport System (FITS), Version 3.0".
  *Astronomy and Astrophysics* 524, A42.
  DOI [10.1051/0004-6361/201015362](https://doi.org/10.1051/0004-6361/201015362)
