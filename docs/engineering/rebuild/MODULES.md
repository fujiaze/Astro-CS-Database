# main → V2 的实际源码迁移清单（只迁已有功能）

本页只登记**生产代码所有权和施工动作**，不在这里复写科学公式或定义新算法。实施前沿 main `run_phase1_block/run_phase2_block/run_phase3_block` → `module_adapters.cpp` 真实consumer追到下表内核；存在但未被生产调用的旧源码仅在证明含有同一所需算法时取其有效计算部分。源分支 main 只读，迁入 V2 唯一生产计算路径。V2现有模块可能已有正确计算，必须先读当前实现**差异修补**，不是把整个旧库与V2并存。

下述V2文件名均是当前仓库的目标接收位置，而不是声称实际已连好数据。任何修改先保证相同物理估计量/单位/索引，随后将旧科学核接到同一Memory/Pipe数据面。

## M01｜FITS输入、Master和Native校准

- **main来源**：`lib/algorithms/calibration/src/calibrator.cpp`、`dark_optimizer.cpp`、`master_generator.cpp`、cosmetic correction；生产`p1_op_calibrate`与`p1_op_cosmetic`；旧AIO读FITS/XISF时只抽取正确格式处理、IO缓存。
- **V2接收**：`src/observation/{calibration_kernel,calibration_plan,calibrate,stream_map}.cpp` + 唯一 `src/runtime/storage.cpp`/FITS reader。检查有没有第二校准数学核或第二解码器。
- **施工**：按row/stripe真实读取raw/master；master只读共享、DQ/饱和/flat零除正确传递；校准运算在持有lease的NativeBlock上执行，块离开作用域释放；如需二次读取先由缓存/原始只读block提供，不落多个整帧临时FITS。按真正T2/T3 master尺寸、增益、曝光、暗包含bias核对；不引入新校准模型。
- **检查**：从一幅真实raw的实际某几像元手算到校准值、DQ与单位，追踪后续消费者仅见该值一次；计数实际解码/重复读盘/整图复制以及master常驻字节。

## M02｜本地Gaia索引、IPV盲解与WCS/SIP

- **main来源**：`lib/algorithms/platesolve/cpp/ipv/src/` 的solver/RANSAC/robust refine/SIP/itertrans，`lib/algorithms/drizzle/healpix_drizzle/wcs_sip.cpp`；真实`p1_op_wcs`。
- **V2接收**：`src/catalogue/{index,provider}.cpp`，`src/sources/astrometry_*.cpp`，`src/coordinates/{wcs,tan_sip}.cpp`。
- **施工**：保留可证正确的IPV检索/拟合内核和本地Gaia只读共享索引；统一一个WCS/SIP provider供天测、像元足迹、autoNSIDE、PSF和Export；改旧C/JSON adapter不重写盲解数学。Gaia目录按扇区查询/热cache，避免每帧重建；WCS逆变换先按正确SIP迭代，禁止未迭代的AP/BP初值立即精度拒绝。
- **检查**：真实无CD头M42帧尝试盲解，独立Gaia留出星与原图位置做统计，核CD手性、CRPIX一基/内部零基、强畸变、RAwrap。记录索引加载次数、候选量和CPU，在多帧时观察共享。

## M03｜Star Detection、PSF、源测量

- **main来源**：`lib/algorithms/star_detection/src/{sdet_image,nls_lm}.cpp`、`lib/algorithms/psf/src/{pixel_integrated_psf,spatial_psf}.cpp`，生产`p1_op_star_psf_impl`。
- **V2接收**：`src/sources/{psf,psf_fit,psf_model,measure}.cpp`。
- **施工**：抽取旧检测、像元积分模型、LM/空间变化有效内核；区分真正有消费者的生产分支与旧未启用的精密版本。窗口作为按位置/halo读入的NativeBlock切片，混叠邻星与M42星云背景按照已有模型处理，不能重复对同一frame全幅扫描找每颗星。全图检测可按tile流形成小型星列表/模型后再回读正式Drizzle块。
- **检查**：与原始frame的未饱和真实星窗口检查PSF形状/孔径flux/迭代收敛；旧强负亮星与2星错误不能继续被视为有效拟合。记录窗口读入数、模板重用、LM迭代与heap热点。

## M04｜真实仪器响应与相对光度

- **main来源**：`lib/algorithms/photometry/cpp/src/{spectrum_integrator,frame_photometry_fit,spatial_gain,image_corrector,star_matcher}.cpp`，生产`p1_op_photometry`。
- **V2接收**：`src/response/`、`src/photometry/{joint_fit,spatial_basis,apply_scale}.cpp`。
- **施工**：复用积分/相对标度/空间拟合；实际filter/QE与Gaia响应不得错配，T2/T3可比性靠真实数据。小型标度模型冻结后直接作用每个NativeBlock一次（可与Drizzle原始像元循环融合）；不得为了“显式photometry.apply”再写一整幅float64临时帧。
- **检查**：同一像元/恒星从校准→α→Drizzle的实值和单位/方差倍数，比较旧实际测光比例/未拟合留出源；统计是否有二次apply与额外全幅RAM/IO。

## M05｜AutoNSIDE、HEALPix与真球面Drizzle

- **main来源**：`lib/algorithms/drizzle/healpix_drizzle/{drizzle_engine,hp_drizzle_api,spherical_overlap,poly_clip,astro_sphere_sink}.cpp`、`lib/algorithms/shared/healpix/healpix_core.cpp`；真实`compute_auto_nside_ex`与P1正式Drizzle。
- **V2接收**：`src/coordinates/{resolution,healpix}.cpp`、`src/geometry/{drizzle_core,drizzle,overlap,refine}.cpp`、`src/workflows/normalize.cpp`。
- **施工**：自动NSIDE首轮已有order18 ROI实测，应保留；增加全参与帧WCS冻结共同分辨率计划，不能回到固定order13。旧Drizzle per-thread bounded geometry LRU、touched-only叶、条带归约/候选scratch共享迁进V2。正确的像元→连续球面映射和支持关系不允许由粗几何近似或用于加速的代理取代。**输出扁平有效leaf/TileFragment直接送Pipe或有界Tile累加器**，按触及分区刷新，CSR/贡献关系按需保存；不得先给每帧建一个全HEALPix空球产品。
- **检查**：追踪一真实原CCD星点经过WCS、足迹、R到输出的贡献、光度/PSF、support；同输入不同线程的归约差、order/tile order与真实读回。统计有效叶、nnz、candidate、geometry LRU命中与峰值临时字节；4096²不能靠降低科学order取得速度。

## M06｜Coverage：多面板并集、区域索引和空间邻接

- **main来源**：`lib/algorithms/coverage/src/{coverage,stage2_common}.cpp` 的球面集合/MOC/有序tile；正式`p2_op_coverage`。
- **V2接收**：`src/workflows/mosaic.cpp`中region规划与对应SpatialIndex/Provider。不可重新造一条重复全天球index实现。
- **施工**：把各帧已知touched leaf/tile和WCS覆盖合成**有序稀疏union/空间区域计划**；region只迭代真正覆盖它的frame。承接main有序并集与有效tile快速定位；目标order在早期同组计划确定，不在Mosaic暗降。多个不同M1–M6的面板足迹用真实球面交集确认，NESTED `ipix+1`不是2D天区相邻。
- **检查**：打开真实M2/M5的WCS和叶索引，手动追踪一块重叠、边缘、无覆盖区域是否正确归属；检查运行复杂度不按`allLeaves×allFrames`展开。

## M07｜Sampler与连续公共Sky/相对背景

- **main来源**：`lib/algorithms/coverage/src/{sampler,sky_plane}.cpp`，正式`p2_op_sample,p2_op_upm_fit,p2_op_upm_apply`的有效信号面核。旧`upm.cpp`的退役F1–F6核不作为生产实现移入。
- **V2接收**：`src/sky/{sky,sky_controls_process,sky_fit_process}.cpp` 与Mosaic小型屏障。
- **施工**：真实天空足迹选控点、联合拟合一张公共连续面和每帧加性差模；保留旧Schur/IRLS/连通组件/块Cholesky优化但不要并存两个sky solver。采样窗口使用可复用LeafBlock/halo，只记录全局有限个控制量与参数；冻结后按region读入并计算校正，不物化每帧整幅`corrected_signal`。**不开发新的乘性flat-screen求解器**；若现有平场校准无法解释差异，报告真实残余。
- **检查**：原始/校正后M2/M5重叠区背景、星云大尺度信号和共同面/帧差数学含义；同一组数据更改chunk不得造成不同sky基准。性能看采样器重复开tile、Schur维度、装配/求解scratch与线程。

## M08｜传统Rejection（核心：从main直接迁入，不重造LOO统计器）

- **main来源**：`lib/algorithms/coverage/src/rejection.cpp`与`include/astro/phase2/rejection.h`；正式`p2_reject_stack_ex`，包括RCR/ESD/传统sigma/trim或winsorized等代码中的真实方法。
- **V2接收**：现有`src/coadd/{coadd,coadd_select_process}.cpp`可以保留类名作工程适配，但**默认行为由main传统方法及正确性核对确定**，删除/隔离V2新造的`single_max_loo`与强制`Γ`求解作为主生产限制。核参数、sample weight、DQ与逐样本拒绝原因不应在外层重新实现。
- **施工**：抽纯`small valid stack + selected method/params → mask/reasons`核，每leaf只读该region覆盖它的真实帧样本，有限scratch反复使用。允许无排异、用户已有可达传统算法与自动选择；旧小N百分比带若非平移等变，应修正旧错误默认或用已有保守策略，说明实据，**不凭这次重构发明一个崭新的N=4/5检验法**。
- **检查**：实际多曝光中人为可识别宇宙线/卫星/坏点和正常星云的误删/漏删，数值位移对排异结果的影响，以及样本数1/2/3/4/很多时合理行为。检查输出值与mask来源，禁止`coadd`内部再跑第二次选择。

## M09｜传统Integration/Weights（非BLUE/QP必需）

- **main来源**：`lib/algorithms/coverage/src/integrate.cpp::p2_integrate_pixel`与`lib/algorithms/integration/phase2_integrate/`中有效非负权构造；正式`p2_op_integrate`。
- **V2接收**：`src/coadd/{weights,coadd_weights_process,coadd_integrate_process,coadd}.cpp`或者可维护的合并模块（代码层次不必等同旧22 Process）。
- **施工**：按当前region/leaf已有Selection和传统合法权值计算`Σwx/Σw`，零权/全拒/无输入/非法权/合法负值/支持都保持main真实六态；有合法噪声信息可求对应权，否则选择已有保守等权并标明未知。**无需Γ矩阵、BLUE/KKT/QP才允许产出有效平均图像**；高级求权如无旧生产消费者从默认流程移出。权只存当前region的稀疏有效`(leaf,frame,weight)`，不写全局`Nleaf×Nframe`表。
- **检查**：对真实两个/多个曝光同位置数值人工跟踪拒绝→权→均值的每项贡献和单位；查看support与signal是否混淆；检查运算量按有效贡献数而非全球矩阵规模增长。

## M10｜FramesProvider + Region执行（最先修的工程关键）

- **V2现有**：`src/workflows/frames_provider.cpp::read_leaf_block` **忽略 `FrameReadRequest.leaves/halo/fields`**，`src/workflows/mosaic.cpp`一次收集全局叶，`src/coadd/coadd.cpp`反复构造全帧`FrameView`/map/set，当前select/weights内存计划按groups×65536代理估。
- **施工**：使用现有`ArrayRef/ArrayView/Storage::read(BlockRequest)`和真实index，在一次请求里只读所需frame×region的signal/DQ/support，保持只读lease有效至消费者完成。与coverage共同提供有序`LeafRegion`分区并一次建立区域内完整样本；Sky apply→Rejection→Weights/Integrate能共享这个`RegionFrameView`，**不再三个Process重复读取一整幅**。先重用正确V2 `PipeCore/MemoryScope`实现，不另造Pipeline。去掉整图强制`allow_spill=false`与假整个leaf_count最小块要求。
- **检查**：在真实M2/M5 512/1024²追踪每个`read_leaf_block`实际请求边界与返回的bytes；看同region被读几次、什么时候free以及压力高时producer是否被消费者反压。小规模修复必须能自然扩到4096²/更多帧，而不是把4MiB改成8.5MiB后继续OOM。

## M11｜HiPS、重采样与FITS

- **main来源**：`lib/algorithms/projection/p3_{projection,wcs}.cpp`、`resample/p3_{resample,rsmp_operator}.cpp`、`fits_output/p3_output.cpp`、`scheduler/src/export_stream.cpp`与`drizzle/astro_sphere_sink.cpp`。
- **V2接收**：`src/projection/`、`src/workflows/{export_workflow,fits_writer,product,product_io}.cpp`、唯一Storage。
- **施工**：正确保留HiPS标准512 Tile/leaf order关系，高阶数据的目标WCS、nearest/bilinear的实际使用域；复用旧跨worker共享bounded Tile LRU、negative missing cache、ordered reader/compute/writer与FITS流式/原子发布，不留多个独立Writer。Output按行/矩形块顺序写入，不保留完整output平面在RAM；同进程Mosaic碎片若可直接服务Export避免不必要整幅中转，独立CLI仍可通过正式HiPS继续。
- **检查**：一个真实天区/星点从高阶leaf到目标FITS像元的真实值和单位，合法无覆盖NaN、长头与跨平台reader；统计tile独立打开数、共享缓存按worker数是否扩大、RSS和写盘流量。

## M12｜统一Runtime、性能与工程收口

- **main来源**：`lib/infrastructure/scheduler/src/{block_frame,block_flow,executor,scheduler,memory_budget,cpu_budget,export_stream}.cpp`及AIO性能结构；**不整套复制**。
- **V2接收**：`src/runtime/{host,pool,task,pipe,memory,storage}.cpp`，`src/workflows/`和实际公开`include/acsd/`。
- **施工**：一套CPU/IO预算，确定性stripe归并，帧间与帧内并行避免OpenMP N×N超售；master/Gaia/geometry/Tile的共享缓存只保留一种owner；在Pipe中持有真实Buffer lease；block结束释放；取消/失败会将整个DAG通知终止；受控spill与跨命令产品持久化唯一由Storage承担。
- **检查**：对应每个大buffer在实际内存中寻找所有者/生命周期；同输入worker1/2/4/8与旧main在同算法、同阶、同机器比较真实wall/CPU/RSS/IO及有效输出；查重复解码、全域map、未触及Tile发布与writer多个常驻大buffer。

## 迁移总约束

这12组是**代码组织/施工归属**，不是强制新增12个C++服务、22个Process或12份产品文件。子模块的真实数量由main已有算法依赖和V2最少耦合决定。优先合并能在同一region连续执行、但被原V2抽象拆成多次全域读写的kernel（特别SkyApply/Reject/Weight/Integrate），保留逻辑上的责任/小函数。**每迁一个算法就实际让它在内存块流水线上被消费；禁止先迁满一堆未连线模块、最后才发现没有科学数据流。**
