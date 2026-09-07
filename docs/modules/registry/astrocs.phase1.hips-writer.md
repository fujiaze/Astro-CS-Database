---
id: MOD-astrocs-phase1-hips-writer
version: 1.0.0
status: ACTIVE
owner: astrocs-core
source_commit: 860639be8eb0c07a89383c3975bc14ba2f97ba19
upstream: [SCI-DRZ-001, ALG-HIPS-001, DATA-P1-DRZ, DATA-P1-HIPS, API-HIPS-001]
downstream: [TEST-HIPS-DESIGN-001]
---

# 模块 astrocs.p1.hips_writer

> P1-HIPS-DOC（2026-09-07）新建：本页为 HiPS writer 模块合同登记页（此前
> registry 无占位页）。事实源：lib/hips/README.md、module.yaml、
> docs/algorithms/HIPS_WRITER.md、docs/contracts/DATA_SEMANTICS.md §12、
> docs/contracts/PUBLIC_API.md API-HIPS-001、
> docs/traceability/TRACEABILITY_MATRIX.json MOD-astrocs-phase1-hips-writer 行。

## 职责与明确非职责

生产实现=lib/astro_image_io/src/hips/aio_hips_writer.cpp（CMake
astrocs_hips 静态库 CMakeLists.txt:298-309；registry descriptor 无本模块
页项——astrocs_p1_hips_writer.dll 由 P1-HIPS-IMPL 建立，entrypoint=MISSING）。
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

## 公共 header、核心 symbol 与生命周期

现状 C ABI 九导出（aio_hips.h；API-HIPS-001，PUBLIC_API.md）：
aio_hips_product_begin / aio_hips_write_signal_support_tile /
aio_hips_write_variance_tile / aio_hips_write_snr_points /
aio_hips_set_drizzle_provenance / aio_hips_finalize / aio_hips_abort /
aio_hips_write（兼容）/ aio_hips_last_error。生命周期
begin→write_*→finalize/abort；迁移 create→validate→run→inspect→destroy
由 P1-HIPS-IMPL 接线。

## Registry descriptor 与配置 schema

module_id=`astrocs.p1.hips_writer`；现状 registry 无本模块 descriptor
（module_adapters.cpp 无 hips_writer 项，全仓库无 astrocs_p1_hips_writer
CMake 目标）；配置经 product_begin 参数固化（nside/tile_width/data_type/
flags/creator_did/obs_title/obs_filter/exposure_s/obs_date/moc_order）；
versioned config schema=MISSING（P1-HIPS-IMPL 冻结）。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`（I/O 主导）；并行轴=无内部并行（单句柄串行写）；ThreadLease
零命中（现状调用方线程直连执行——迁移整改点）。确定性：tile 字节随输入
与写序可复现（FITS checksum 内嵌）；hierarchy 归约按 k 降序 + NESTED
索引确定性顺序；properties 的 UTC 时间戳（hips_creation_date 等）不跨
运行复现（真实时间合同，DATA_SEMANTICS §5）。

## 内存/cache/I-O/所有权

叶级 scratch 512×512 复用；hierarchy map<ipix,AncestorAcc> 随覆盖增长；
SNR 点全量内存缓存；I/O=CFITSIO 写 FITS + properties/manifest 文本 +
Moc.fits BINTABLE；handle 所有权=writer（finalize/abort 释放）；view
数据所有权=调用方（写调用期间有效）。

## 错误、日志、指标、取消和 checkpoint

错误码：product_begin NULL；write_signal -1..-5；write_variance -1..-7；
finalize -1..-8；provenance 1/2——无集中枚举（DISP-HIPS-007）；
last_error=thread_local 文本。日志=stderr [hips]/[sink]。指标=stderr
profile 计时（transform/fits_write/hierarchy/finalize 分段）。取消=无
检查点（登记限制）；checkpoint 无（abort 不清理已写文件，
DISP-HIPS-001，处置归调用方/IO-003 层）。

## 独立 synthetic 验证命令与容差

`TEST-HIPS-DESIGN-001`（HIPS_WRITER.md §9）：解析 oracle（独立朴素实现）
逐像素校验 signal/support/variance/ivar 与 FITS 序映射（CDS Hipsgen 外部
点，DATA_SEMANTICS §3）、MOC UNIQ/sky fraction、hierarchy 聚合闭合；
冻结容差=FP64 逐像素 bitwise、f32 存储 rtol=1e-7、MOC/properties 精确。
可执行 TEST-P1-HIPS-001 由 P1-HIPS-TEST 建立。

## 已知限制

ALG-HIPS-001 §10（DISP-HIPS-001..012）+ README §9（abort 无 rollback、
CFITSIO 裸调无 mutex、错误码混用、hips_estsize 硬编码、moc_order 静默
clamp、无取消）；docs/KNOWN_LIMITATIONS.md。
