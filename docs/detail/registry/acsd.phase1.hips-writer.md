# 共用 HiPS 写入库

本页登记统一 AIO 的 HiPS 写入能力，由 Normalize Drizzle/sink 和 Mosaic writer 复用。它是格式/层级写库，不是额外 Normalize 调度节点；阶段产品范围由[最高设计](../../ACSD_DESIGN.md)、[Drizzle 正本](../../science/drizzle/DRIZZLE.md)及 [Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)决定。

## API 与调用者

公开头为 [aio_hips.h](../../../lib/infrastructure/aio/include/aio_hips.h)，实现在 [aio_hips_writer.cpp](../../../lib/infrastructure/aio/src/hips/aio_hips_writer.cpp)。

| 入口 | 作用 |
|---|---|
| `aio_hips_product_begin` | out_dir、叶 nside、tile_width、dtype、flags、标识/响应/曝光等 → opaque 产品句柄；失败 NULL |
| `aio_hips_write_signal_support_tile` | 消费 tile 累加视图，按 flags 写相应图像 |
| `aio_hips_write_variance_tile` | 消费真实方差分子，按已声明 flags 写 variance/ivar |
| `aio_hips_write_snr_points` | 缓存/写 Catalogue 浏览点；不自行生成参考物理量 |
| `aio_hips_set_provenance`、`_set_drizzle_provenance`、`_set_frame_snr`、`_set_geometry` | 登记真实来源/几何/兼容参考字段；不证明上游科学定义完整 |
| `aio_hips_finalize` / `aio_hips_abort` | 收尾层级、properties、MOC、manifest 并释放，或中止释放 |
| `aio_hips_verify_product_set`、`aio_hips_last_error` | 产品集一致性检查与线程局部诊断 |

另有 `aio_hips_write_diag_tile` 和兼容 `aio_hips_write`；能力存在不表示当前命令发布其所有通道。Normalize 可以发布必要辅助对象；Mosaic 的 `p2_op_write` 仅选择 SIGNAL。

## 数值视图、单位和坐标

`AstroSphereTileView` 包括 parent_ipix、leaf_order、width、dtype、flux_sum、covered_area、valid_mask、var_num_sum。view 数组为 NESTED local；写库使用共享 HEALPix 转换落 FITS row-major，调用方不能再预翻一次。width=512，tile order=leaf_order−9。

写库信号为 flux_sum/covered_area。因此输入 flux_sum 是写视图中间量，covered_area 是该视图声明的分母；它不自动等于天区覆盖并集。variance 由对应分子除分母平方。上游必须将 Drizzle 的 N/D 归一变换一致地适配到这些字段，不能按字段名字把 D、N、Ω 互换。非正/非有限分母产生无覆盖状态，真实零与负信号保留。

跨 ABI 结构须以 `*_abi_init` 初始化 struct_size/abi_version；每元素都需匹配。稀疏 sky 为 ICRS degree；Catalogue SNR 继承上游估计器身份，不能反写 canonical sidecar。

## 所有权、并行与发布

输入视图由调用方持有，至少覆盖同步写调用；句柄由 begin 创建，并由 finalize/abort 结束，不重复释放。单句柄按调用方串行写，tile 转换可在外部预算下并行；ancestor 聚合和 Catalogue 缓存随覆盖/点数增长，不能写成全库固定常数内存。

非法 nside/width/dtype/flags、ABI 不匹配和 I/O 失败通过 NULL、非零码及 last_error 传递。库 finalize 与文件写完整不等于整帧科学事务完成。最终 signal、模型、sidecar 与完成清单的统一发布责任在[阶段一 writer](acsd.phase1.writer.md)；当前完整性边界见[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 验证入口

从上述 C API 与 `aio_hips_verify_product_set` 进入检查；[阶段一管线测试](../../../eng/tests/pipeline/test_stage_normalize.py)核对调用路径。应独立读回索引手性、父层、MOC、真实单位、无覆盖、flags、ABI 拒绝及失败残留，不能用同库读写往返单独证明天球方向或原子性。本页未执行验证。
