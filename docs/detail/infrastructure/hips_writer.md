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

非法 nside/width/dtype/flags、ABI 不匹配和 I/O 失败通过 NULL、非零码及 last_error 传递。库 finalize 与文件写完整不等于整帧科学事务完成。最终 signal、模型、sidecar 与完成清单的统一发布责任在[阶段一 writer](../normalize/modules.md#writer)；当前完整性边界见[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 验证入口

从上述 C API 与 `aio_hips_verify_product_set` 进入检查；[阶段一管线测试](../../../eng/tests/pipeline/test_stage_normalize.py)核对调用路径。应独立读回索引手性、父层、MOC、真实单位、无覆盖、flags、ABI 拒绝及失败残留，不能用同库读写往返单独证明天球方向或原子性。本页未执行验证。


## D7-C｜来源算法与当前工程、数据接口（2026-10-17）

本页作为[ALG-HIPS正式长文](../../science/algorithms/export/HIPS_WRITER.md)的函数级应用说明：其历史完整公式、测试设计、修正分级和正反oracle已在正文，不另行再复制一个治理台账。**CURRENT** C ABI是`aio_hips.h`(405行)声明、`aio_hips_writer.cpp`(3138行)实现，且并非第三个单独CLI命令；Normalize入口为`astro_sphere_sink.cpp::write_hips_phase1`、后由`p1_op_writer`补正式frame-science sidecar，Mosaic入口为`module_adapters.cpp::p2_op_write`。后者只要求SIGNAL最终图像，不能用AIO可写VARIANCE/IVAR的接口证明Mosaic发布了它们。

**公共接口顺序**：`aio_hips_product_begin(out_dir,nside,tile_width,data_type,flags,creator_did,obs_title,obs_filter,exposure_s,obs_date,moc_order)`创建opaque `AioHipsProductSet*`；`aio_hips_write_signal_support_tile(ps,view)`消费`AstroSphereTileView`的NESTED-local `flux_sum`与`covered_area`，按`view.valid_mask`发布signal/support；有经证明的方差分子时可调用`aio_hips_write_variance_tile`，并可选`aio_hips_write_diag_tile`、`aio_hips_write_snr_points`及set_geometry/set_drizzle_provenance/set_frame_snr/set_provenance；最终`aio_hips_finalize`或故障时`aio_hips_abort`。flags为SIGNAL=1、SUPPORT=2、SNR=4、VARIANCE=8、IVAR=16、NREJ=32、NUSED=64。nside=2^K∈[512,2^29]且width=512、`tile_order=K-9`，view要`aio_hips_tile_view_abi_init`，首两字段struct_size/abi_version不符返回`AIO_HIPS_ABI_MISMATCH=-9`；view指针缓冲由调用方持有至同步API返回，单opaque writer句柄不支持多线程无锁并写。

**归一不是文件字段重命名**：P1实际Drizzle累计`F_p=sum w_jp*x_j`、`N_p=sum w_jp*Omega_j`，sink以`k=D_p/N_p`把`view.flux_sum=F_p*k`、`view.covered_area=D_p`交给AIO，再得`signal=F_p/N_p`；方差分子乘`k²`保持二次律。Signal单位`L/sr`随真实native/applied线性计量，不是恒ADU/sr（#9）；covered_area是累计权重面积，可>真实leaf面积，support写出`min(1,D_p/Omega_leaf)`只为coverage fraction，不是随机精度也不是无重叠几何并集。leaf有效面积>0但flux非有限时AIO保留support>0、signal NaN；父层现行逐signal过滤同时跳过area是本批静态确认的条件性跨层覆盖误差风险。高nside #15曲边问题发生在上游Drizzle，AIO正确NESTED序列化不修正交叠几何。

**variance编码**：有面积且var_num>0时`V=var_num/area²,ivar=1/V`，有面积但var_num=0时FITS两数值0/0表示精度不可用、**不是零随机噪声或可用无限ivar**，无覆盖NaN/NaN；有覆盖但var_num<0/NaN/Inf或area非有限视为损坏而硬失败。缺失canonical球面精度与数据损坏不是同态：#10真实leaf_order赋值已修正但正式v2写读与机器schema仍是TARGET；#13 Stage2仍走legacy sparse layer，未消费经v2审计的precision；#19混阶帧需重映射或临时fail-closed；#74低nside实测停滞根因未知，#75增量构建源码SHA可能陈旧。

**层级与发布**：现行Hierarchy是稀疏double祖先累加、仅使用未钳制area，完备即写出并释放，浮点累加历史缺陷已修；每个HiPS tile采用`write_fits_atomic`的tmp/checksum/fsync/rename及父目录尽力sync，properties由`aio_atomic::write_file_atomic`、finalize两次排空异步publish pool后**最后原子写manifest.json**。AIO读侧以完成清单为最小前提；abort只排空并释放，不承诺清理所有已落盘的orphan文件。P1 HiPS正式manifest与随后发布的`frame_science.json`当前没有共享整个revision commit pointer；P2另在private staging生成并发布产品根，必须区分单文件原子与多文件产品完整性。源头oracle、资源/故障测试、schema v2生产均未在D7-C运行。

