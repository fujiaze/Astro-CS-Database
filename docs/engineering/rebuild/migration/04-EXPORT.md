# 04｜Export、HiPS与FITS：主分支高效的真实标准输出迁入V2

**对应**：projection.export、WORKFLOWS Export、唯一Storage/Product IO。**旧生产链**：`lib/infrastructure/scheduler/src/module_adapters.cpp::p3_op_{properties,wcs,resample,writer,verify}`；主算法 `lib/algorithms/projection/p3_{projection,wcs}.cpp`、`lib/algorithms/resample/p3_{resample,rsmp_operator,rsmp_units}.cpp`、`lib/algorithms/fits_output/p3_output.cpp`；Runtime `lib/infrastructure/scheduler/src/export_stream.cpp`。**V2**：`src/projection/{export_process,export_projection}.cpp`、`src/workflows/{export_workflow,fits_writer,product,product_io}.cpp`、`src/runtime/storage.cpp`。

## E-01 真正的高阶HiPS→目标FITS，不允许伪分辨率

1. Export读取真实Mosaic高阶leaf order、tile order、支持、正确BUNIT与响应身份；HiPS 512 tile的NorderK对应leaf order K+9。**main曾有p3_order_select**依据目标输出角尺度选适当输入层级；把其科学层级选择与缓存结构移植到V2的唯一`build_export`调用，不是固定读取任意粗层。
2. 目标WCS由真实参与帧球面足迹全域规划：中心、CD/PA/像素角尺度、TAN投影、宽高、margin与角点；同时确保共同M1–M6区域，按最终各通道相同ICRS目标网格。不得为省内存硬裁到中心4096²，也不得从已严重粗化的order13导出0.967″图。
3. 两次采样的真实定义分别检查：源CCD→球面R、HiPS细→粗下采样、球面→FITS Q。旧main支持nearest与局部bilinear并且设计了area-overlap，但旧文档明确**精确area overlap未全面接入P3生产**；V2若使用nearest会有块状alias，应以科学可重建分辨率和真实PSF/MTF比较选择/实现可证的面积核。不能仅把菱形插值平滑后称恢复细节。
4. 保留极区、RAwrap、SIP变换、一基CRPIX、裁剪后CRPIX调整与WCS外域拒绝；旧main的PA/不同投影配置与实际可用性不同，V2不能只因enum存在就标已支持。
5. 对于混合通带、响应随天区变化、未知gain/read，FITS需保留正确“信号估计量”和状态，不凭单signal新造variance、SNR或声称绝对flux。合法负像素/零像素保留，无支持写NaN，已知饱和holes不能用插值假值默补。

## E-02 必须继承的性能优势：共享LRU、负缓存与分段流

**main: `lib/algorithms/resample/p3_resample.cpp`** 源码明确：
- 跨所有worker共享一个**有界LRU**，每张512² float32 tile约1MiB，缓存最大长度`max_tiles`，**不是每worker一份1GiB池**，因此缓存主要上界不随CPU核数增长。
- 失败/缺失tile有**negative cache**，一个tile故障不重复open和产生几十万个同样的IO操作；故障是数据完整性还是合法空覆盖需区分。
- 局部2–4tile热工作集减少共享锁请求；LRU互斥只护元数据，实际tileI/O在锁外；重复只读tile采用不可变共享持有，避免N次拷贝。
- 数值路径与缓存命中/逐出解耦（不允许由于缓存策略改变像素值）；线程数不同缓存统计可变，但同样科学输入与算子数值要等价。

**main: `lib/infrastructure/scheduler/src/export_stream.cpp`** 已有reader/compute/writer子块流式队列与限深、取消、worker异常保护。**main: `lib/algorithms/fits_output/p3_output.cpp`** 实现`P3FitsStream`，FITS写卡/数据DATASUM/CHECKSUM、flush/close/fsync、rename及重开校验等。迁移为V2唯一Storage/Task/Pipe机制，不复制旧第二Executor：
1. 每输出sub_block在途reader/compute/writer容量受一个MemoryScope账本约束，队列深度和热Tile缓存按`per_block_bytes×K + max_tiles×tile_bytes + WCS scratch + FITS writer buffers`计划。
2. 可复用tile位置上的多worker只持`shared_ptr<const TileData>`式租约，writer按输出块顺序且异步I/O/CPU分开计费，避免整幅float64输出数组和整幅临时coverage同时常驻。
3. 独立新进程重开文件并严格读头：BUNIT、CTYPE/CD/CRPIX/CRVAL、SIP HIERARCH/长键、长度、padding、checksum/data；**非法ACSUNIT长卡必须拒绝发布**。失败时不得留exit0/complete=true。
4. 发布不是“先rename再校验”，应在staging的完整字节、依赖闭包和最终科学内容均验过后原子指向final；坏tile/坏head/不完整引用的故障注入需验证旧成功final不被破坏。
5. 产品本体支持signal与受支持区域辅助sidecar；ProductGraph/LinearAction/NoiseRoot/CovarianceAction等对象必须能在**空缓存新进程**中按相同身份重开。若只发signal且没有noise模型，必须显式unknown而非伪造variance。

## E-03 源码级施工任务

- E01：审阅main P3的`p3_order_select`与V2`export_projection`，重构唯一“输入层级真实性+目标WCS/投影”计划，不重复计算WCS与HiPS索引。证明order与有效角尺度的关系，而非只校验输出shape。
- E02：审阅`p3_resample.cpp::SharedTileCache`及所有reader调用，把cache/hot set/negative cache移到V2 Storage级别，同一run仅保留一套，限制缓存上界并记录hit/miss/read_failure/lock_wait；保留科学像元值精确同算子一致。
- E03：复用main resampler里经过验证的合法nearest/bilinear计算核、NaN边界和位运算，V2通过`projection.export`调用；科学上需要area-overlap时利用V2严格几何核而不是重新写第二套相交器。
- E04：旧`P3FitsStream`的可证写法改为V2`fits_writer.cpp` +唯一Storage的原子事务；注释/证据迁到本模块，**不能整个旧writer连AIO全局状态复制进来**。
- E05：把响应分布、标准BUNIT、PRODUCT/INPUT hash、order/targetWCS、SATURATE/DQ/coverage与nonfinite解释纳入产品闭包，导出与Mosaic manifest约束源输入；每个Hα/R/G/B有独立identity。
- E06：要复用 main旧的`eng/tools/e2e/render_vis.py`成品对照能力，可改成独立观察工具，但它的自动视觉阈值不能代替用户最终PixInsight验收；同时必须自行做FWHM/PSF/星云低频/真实空间接缝统计。

## E-04 外部读回与真实科学判别

验证二维测试目标：TAN+SIP旋转、窄band、分块交界、RAwrap、反向/镜像CD、角点、裁剪、饱和holes、输入缺tile、长FITS关键字、临时目录不足、取消、fflush/fsync或rename失败、坏LinearAction闭包。独立Astropy/WCSLIB+healpy/astropy-healpix同时检验WCS与天球空间真实值；不能用生产代码自己逆映射自己当oracle。

质量门：同一真实Sky模型生成的CCD真值/PSF与完整R→Q输出比较，点源通量/FWHM/ellipticity/MTF/高频块纹，扩展源常值/低频/相对背景/面积误差；与main匹配同物理/NSIDE/目标WCS/重采样核时对拍，并注明旧近似。任何明显菱形假星点/不合理黑洞/有覆盖NaN的科学说明不闭合均不能交付最终FITS。

**性能门**：至少固定1、2、4、8worker并观察共享cache容量近似不随线程数增倍（临时scratch可随W增长），对每种科学采样核记录wall、CPU、RSS、I/O总字节、unique_tile_read、cache hit/miss、输出像元/秒、平均锁等待与队列利用率。错误更多/像元少了/算法变nearest不是性能优化。详见[06](06-PERFORMANCE.md)。
