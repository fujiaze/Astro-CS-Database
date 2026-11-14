# 实际性能与资源：保留main的优化而非自造性能指标

**性能是工程重构的一部分**，不独立排队到“所有功能完成后”。用户关心真实4096²、多帧、多面板，尤其12GiB进程内存上限和有限磁盘。只有在同输入、同模型、同NSIDE、同硬件、相同有效输出下性能才可直接对比；修正旧数学错误增加的计算必须单独解释来源，不用降低正确性来“优化”。

## 1. main已存在的性能资产及具体迁入点

| 来源代码 | main中已有的有价值优化 | V2必须保持的工程行为 |
|---|---|---|
| `drizzle_engine.cpp` | 唯一作者stripe累加、固定归并；每线程 bounded target-ipix geometry LRU 和复用scratch；touched叶与有效Tile合并 | V2一条R路径仍按实际touched域工作；排序归并稳定；geometry cache含有效WCS/NSIDE身份，线程私有scratch及时释放 |
| `astro_sphere_sink.cpp` | 只发布实际触及的球面tile | 不写大量空NaN tile；内部块大小与标准HiPS tile层级不混 |
| `coverage/src/{coverage,sampler,stage2_common,block}.cpp` | 稀疏MOC/有序并集、局部sample与按budget动态microchunk | Mosaic按真实region×局部frames工作，不构造allLeaves×allFrames；每tile读一次可覆盖多control |
| `coverage/src/sky_plane.cpp` | 连通图、Schur/IRLS、固定分片装配、block Cholesky | 一个全局小模型解、冻结再按块应用；不重新发明/复制第二求解器与独立线程池 |
| `coverage/src/rejection.cpp` | 多种已有稳健small-stack排异内核 | per-leaf栈scratch重用，同一region可复用排序/参数，不每像元堆分配 |
| `resample/p3_resample.cpp` | 跨worker**共享一个bounded tile LRU**与negative cache，tile IO在全局锁外、局部热缓存 | 不能每worker持独立大cache；不存在tile/坏tile不同状态；重复目标像元不反复开同tile |
| `scheduler/src/export_stream.cpp`及`fits_output/p3_output.cpp` | reader/compute/writer分段、限深队列、流式FITS写出 | 有效I/O-CPU重叠，按序写块，不双份全幅输出，失败不污染正式文件 |
| `scheduler/src/cpu_budget.cpp`/旧OpenMP修复 | 限外部帧并行与内部kernel并行，总线程可控，旧ICV遗留不影响后续工作 | 一套V2 pool/lease；`frames×OMP_threads`不能暗超售，真实活跃线程数可观察 |

## 2. 每类对象的真实资源模型

要从实际profile看：
```
RSS_peak ≈ fixed(master/Gaia/models)
         + active_frame_input_blocks + active_calibration_blocks
         + active_geometry_scratch + active_sparse_contrib/leaf
         + active_region_stack/selection/weights/output
         + bounded_shared_tile_cache + pipe_items
         + per_worker_scratch + third_party_buffers + safety_margin
```
4096² float32约64MiB，但同时复制raw、flat、clean、scaled、R、fullSky、全帧mask、全幅output等可以放大远超这个值；同样高leaf order若枚举全球HEALPix也会灾难性增长。真实单位是**触及的叶与非零贡献**而非`12×4^order`全球数组。预算不能按固定组数×65536估全域后让真实容器提前分配。所有长期对象有实际持有人、分配上限、回收点。

重点查：
- FrameReadRequest被忽略导致每次读整帧；Mosaic三次set/map合并和重复read_f64；
- `Nleaf×Nframe`密集Selection/WeightSolution；改成对当前region有效`(leaf,frame)`样本；
- Copy/Storage/ArrayView之间是否为本来同一个块做不必要的编码→落盘→解码；
- 高阶R的CSR/COO块、几何候选、星点工作区是否在下一region开始后还活着；
- Export是否为每个worker复制整个tile-cache，是否有重复open和多份大Output；
- Sky/Photometry是否重复为每帧重拟同一全局模型。

## 3. 检查方法：实际运行时解释性能因果

运行前先用一个真实源帧的有效触及叶/nnz、实际plan、当前活跃frame数预测一个region峰值。用编译器符号和真实profile/heap trace记录分配者、最长存活对象、CPU热点、I/O调用、Pipe等待与Cache命中。沿512→1024→4096真实输入逐步观察规模，不要靠假随机数据“证明性能”。大全量运行耗时高可先使用真实小天区查清瓶颈，修好并复用已有产品再放大全幅；没有根据不反复重算196帧。

比较旧main与V2先冻结共同算法/WCS/NSIDE/实际帧数/校准/输出单位；同时看`CPU_seconds, wall, active_threads, peakRSS, actual_read/write_bytes, touched_tiles, nnz, legitimate_output`。如果模型和支持不同，解释数学差异，不能拿秒数直接给“退化百分比”。显著增加的开销一定要追到某条生产源码、某种额外计算或I/O，不以抽象“框架开销”掩盖。

**内存上限仍为12GiB**（含第三方/缓存/临时，不只框架账本）。内存压力先缩区/降低同时在途的块/减少并行/复用immutable views/受控spill；不降低NSIDE、不跳步骤、不用nearest假替更正确采样，不将超限当作成功。剩余磁盘要能存正式产品和必要可恢复spill，但不能为每个调试版本保留重复全量输出。

## 4. 性能与数值之间的取舍

main有已知科学局限时，必须保留V2真正修正的数学，但不应该因为“更科学”就保留一个性能明显更差的基础实现。先用源代码推理证明是否可用main的候选搜索、缓存、归并与分块优化服务同一个严格科学计算，再以真实输入验证数值/物理结果。**没有性能自动评分门、没有“20%超过就自动失败”的人为指标**，只有可以解释的真实慢点、真实资源预算和工程裁决。
