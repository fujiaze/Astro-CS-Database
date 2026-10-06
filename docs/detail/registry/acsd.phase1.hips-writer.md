# 模块 acsd.p1.hips_writer

> 上游：`docs/ACSD_DESIGN.md`「模块与 ABI」一节

> 合同：ALG-HIPS-001（docs/science/algorithms/HIPS_WRITER.md）/
> DATA-HIPS-001（本页输入输出端口表）/ API-HIPS-001
> （docs/engineering/api/PUBLIC_API.md）。事实源：lib/algorithms/drizzle/hips/README.md
> （CONTRACT_READY）、module.yaml、lib/infrastructure/aio/src/hips/aio_hips_writer.cpp。

## 职责与明确非职责

生产实现=lib/infrastructure/aio/src/hips/aio_hips_writer.cpp（CMake
acsd_hips 静态库 CMakeLists.txt:599-610；registry descriptor 无本模块
页项——acsd_p1_hips_writer.dll 为迁移目标，entrypoint 未落地）。
职责：IVOa HiPS 1.4 产品集写入（signal/support/variance/ivar Image HiPS +
SNR Catalogue HiPS、叶级 tile FITS（NESTED→FITS 序映射、checksum）、低阶
hierarchy 聚合、Moc.fits UNIQ、properties/metadata/manifest 生成、Drizzle
provenance 键）。不做：tile 累加（P1-DRZ）；HiPS 读侧（aio_hips_reader）；
Phase2 原子发布语义层（IO-003）；hiss 编解码；编排 stage（orchestrator）。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `stacked`（drizzle 累加量 AstroSphereTileView） | `DATA-P1-DRZ` | 必 | ADU（flux_sum）/ sr（covered_area） | ICRS NESTED |
| `hips`（HiPS 产品集 out_dir） | `DATA-P1-HIPS` | 出 | ADU 面亮度 / 无量纲 support / 信号单位² variance | ICRS NESTED |

invalid：covered_area≤0 或非有限 → signal=NaN/support=0/variance=NaN；
variance tile 全无效 → rc=-5 显式失败；nside<512 / tile_width≠512 /
dtype 非法 / flags 越位 → begin NULL。

**负例与归零分支 N12（T05–T07 负向轮，shim 非法）**：负例输入构造 = 非法
`nside`（非 2 的幂 / `< 512`）、`tile_width ≠ 512`、非法 dtype、flags 越位。
预期行为 = `begin NULL` / 显式拒绝，不容忍、不钳制。落盘标记 = `last_error`
文本 + NULL 句柄；把 shim 非法输入静默 clamp 后继续写 ⇒ 判红；

## 公共 header、核心 symbol 与生命周期

现状 C ABI 九导出（aio_hips.h；API-HIPS-001，PUBLIC_API.md）：
aio_hips_product_begin / aio_hips_write_signal_support_tile /
aio_hips_write_variance_tile / aio_hips_write_snr_points /
aio_hips_set_drizzle_provenance / aio_hips_finalize / aio_hips_abort /
aio_hips_write（兼容）/ aio_hips_last_error。生命周期
begin→write_*→finalize/abort；迁移 create→validate→run→inspect→destroy
为迁移目标（未落地）。

## Registry descriptor 与配置 schema

module_id=`acsd.p1.hips_writer`；现状 registry 无本模块 descriptor
（module_adapters.cpp 无 hips_writer 项，全仓库无 acsd_p1_hips_writer
CMake 目标）；配置经 product_begin 参数固化（nside/tile_width/data_type/
flags/creator_did/obs_title/obs_filter/exposure_s/obs_date/moc_order）；
versioned config schema 待落地（窗口期以 product_begin 参数为冻结面）。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`（I/O 主导）；并行轴=无内部并行（单句柄串行写）；ThreadLease
零命中（现状调用方线程直连执行——迁移整改点）。确定性：tile 字节随输入
与写序可复现（FITS checksum 内嵌）；hierarchy 归约按 k 降序 + NESTED
索引确定性顺序；properties 的 UTC 时间戳（hips_creation_date 等）不跨
运行复现（真实时间合同，`docs/science/unified/DATA_SEMANTICS.md`「精度」一节）。

## 内存/cache/I-O/所有权

叶级 scratch 512×512 复用；hierarchy map<ipix,AncestorAcc> 随覆盖增长；
SNR 点全量内存缓存；I/O=CFITSIO 写 FITS + properties/manifest 文本 +
Moc.fits BINTABLE；handle 所有权=writer（finalize/abort 释放）；view
数据所有权=调用方（写调用期间有效）。

## 错误、日志、指标、取消和 checkpoint

错误码：product_begin NULL；write_signal -1..-5；write_variance -1..-7；
finalize -1..-8；provenance 1/2——无集中枚举（缺陷登记 = ALG-HIPS-001）；
last_error=thread_local 文本。日志=stderr [hips]/[sink]。指标=stderr
profile 计时（transform/fits_write/hierarchy/finalize 分段）。取消=无检查点；
abort 不清理已写文件，处置归调用方/IO-003 层（缺陷登记 = ALG-HIPS-001）。
- **负例与归零分支 N27（T05–T07 负向轮，本卡死值与静默 scale）**：负例输入构造甲 =
  `moc_order` 越界请求；负例输入构造乙 = abort 后已写文件。预期行为甲 = 显式拒绝，
  禁静默 clamp。落盘标记甲 = 拒绝码。预期行为乙 = 不清理 + 归调用方处置，不冒充
  原子发布（`remove → create` 非原子）。落盘标记乙 = abort 登记 + 残留清单；
  静默 clamp 或冒充原子 ⇒ 判红；

## 原子发布边界

最高设计「I/O 与原子产品」一节要求所有产品（含 HiPS tile）走「临时文件/目录 + 校验 + fsync + 原子
rename 提交」。本模块现状为 `remove → fits_create → write_chksum → close`
（lib/infrastructure/aio/src/hips/aio_hips_writer.cpp 的 `std::remove`），不构成原子发布，
属已登记的例外面（缺陷登记 = ALG-HIPS-001）。闭合判据 = tile 走
「临时区 → 校验 → 哈希 → 原子改名」且负例可红；在闭合判据通过前，
HiPS tile 的原子发布宣称不成立。

## 独立 synthetic 验证命令与容差

`TEST-HIPS-DESIGN-001`（HIPS_WRITER.md［A-1］）：解析 oracle（独立朴素实现）
逐像素校验 signal/support/variance/ivar 与 FITS 序映射（CDS Hipsgen 外部
点，`docs/science/unified/DATA_SEMANTICS.md`「坐标语义」一节）、MOC UNIQ/sky fraction、hierarchy 聚合闭合；
冻结容差=FP64 逐像素 bitwise、f32 存储 rtol=1e-7、MOC/properties 精确。
可执行 TEST-P1-HIPS-001 待建。

## 已知限制

ALG-HIPS-001 缺陷登记（abort 无 rollback、CFITSIO 裸调无 mutex、错误码混用、
hips_estsize 硬编码、moc_order 静默 clamp、无取消）+ HiPS 模块 README 的测试验证与已知限制一节；
全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
