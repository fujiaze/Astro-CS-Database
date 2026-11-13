# 06｜不可严重退化的性能合同：main的优化、实测对照、资源模型

本页**只负责性能基线、测量方法和性能完成标准**；不复制科学公式或模块迁移任务。旧main的CPU/IO优化是重要资产，但旧运行报告的机器/算法/数据不同，不可把历史秒数当本次可直接复用的速度真值。没有跑过同质量、同硬件的对照时统一记 BASELINE_PENDING，不得声称“快/等价”。

## P-00 必须区分两种性能对照

**A：等价算法开销（工程性能回归）**——main和V2使用同一科学假设、input raw/master、WCS/SIP、response/PSF、pixfrac、leaf order、rejection、Sky选择、输出WCS/核和数学容差；控制`main`已知科学缺陷的影响并把实现差异限定到数值核/线程/存储。如果main旧算子不能满足严格科学定义，**不得为了追求速度让V2退回旧错误**。

**B：提高数学正确性的新增成本（增量科学能力）**——相同数据分别用main近似与V2真曲边/协方差/严格认证核评估，把额外成本拆为`candidate search, fine overlap/refinement, C action, certification, spill/I/O`，同时测它比独立真值改善多少。不能用A“wall≤旧”要求B牺牲精度；应在固定科学目标下对 V2同物理算法的不同优化版本比较，证明新增开销必要且可控。

### 性能红线（须据实测放行）

- **工程纯改造包**：同输入同math条件下，目标`wall≤1.20×main`、`CPU_seconds≤1.20×main`、`peak_RSS≤1.25×main`、`logical_read/write_bytes≤1.20×main`，并且不突破用户12GiB进程上限、磁盘空间安全余量。这里的20%/25%是**工程回归警戒线/优化任务触发线，不是科学或硬件定理**；对旧main本来会OOM/隐藏cache的例子不得按此胡比。遇阈值红，先画像/修复，若数值正确性确实迫使更高成本，记录严格数学增量和可复现实验供负责人裁决，不可悄悄标PASS。
- **数学更严格的包**：不能直接比总wall判退化；必须复用旧高性能策略、报告`ops/valid_leaf, nnz/valid_source, bytes/valid_leaf, sec/overlap, sec/sky_control`，对同一严格数值核优化前后至少无明显示性能劣化。独立科学Reviewer判断是否可以降低求解工作量而不降低信息。
- **资源与正确性硬线**：已配置12GiB/有效CPU/IO上限不得超售，OOM、全空HiPS tile、未完全产品exit0、损坏的缓存或降低NSIDE/参与帧/质量以提高速度都视为失败。不以牺牲真实原生空间分辨率换main的旧速度。
- **性能统计必须包括**：加载/解码、Gaia/PSF/天测、几何、Sky、Coadd、write/flush、new-process reopen、FITS/export、缓存命中、清理；不能只取高性能小内核时间，隐藏慢的typed封装/Storage写盘。

## P-01 固定运行控制变量、计量来源

同一机器、同一 NUMA/内存/文件系统/CPU有效亲和/cgroup与OS电源档，设定线程数1、2、4、8（若资源不足自动取有效档）；同一原始文件和校准；每档先预热cache再测冷/暖两类，至少运行3次（高成本全量例外按单次实测+多个可重复ROI对照，并说明不确定度）。配对顺序交错而非先所有main后所有V2，记录系统其它负载。报告median/p95的wall、CPU时间、peak RSS、磁盘bytes/峰、cache hits/miss/eviction、open/read calls、GC/heap或allocator采样/主要火焰图stack。测CPU有效使用率`CPU_seconds/wall`和实际in_flight，不用配置workers冒充活跃线程。

必须记录：repo SHA、binary SHA、编译器/O3/AVX2/FMA/OMP配置、依赖(CFITSIO/fftw/astropy版本等)、CPU型号/核频/内存/IO介质、raw/calibration/response/WCS/resolution recipe哈希、目标WCS/输出形状、执行命令、输入/输出/最终支持像元数与有效科学模型。算法改变而未更新recipe的缓存自动判无效。

至少输出三张可审计剖析表：按Process的`wall/CPU/RSS/bytes/ops`、按分配对象的`count×bytes/lifetime`、按具体性能热点的`源码行/调用次数/CPU占比/等待时间/改动收益`。I/O类小模块不必构造繁琐统计真值，简单核对生产字节/流量和错误传播即可。

## P-02 main必须保留的性能机制、其失效条件

| 旧源码与机制 | 保留的硬不变量 | V2测量与失效负例 |
|---|---|---|
| drizzle_engine：按stripe一线程写累加/固定顺序归并 | 决定性、无floating atomic冲突，扩并行不改数学、scratch受控 | 同帧1/2/4/8核值、CPU占比、每线程scratch，几何区域不同时空失效 |
| target_ipix per-thread bounded geometry LRU；run generation | 同一R/NSIDE/WCS重复叶边界不重复算，旧run不污染新run | cache hit/miss vs unique leaves、切换WCS/order负例，缓存容量/峰RSS |
| touched leaf TileAccumulator / astro_sphere_sink | 只写真实支持Tile，无全Npix NaN填充 | `written_tiles/touched_tiles≈1`（需扣除合法全空已禁止情形）、bytes/tile，S1旧6.1GiB空tile复发测试 |
| coverage MOC union/ordered tile + sampler tile pair | 扫真实覆盖并集，避免逐叶全帧嵌套/重复open | union cells、control重用、read_calls/unique input tiles；跨M1–M6空间邻接正确 |
| sky_plane Schur/IRLS/blocked Cholesky | 稀疏/低秩，归约顺序固定，小矩阵无需开大量线程 | 内存与矩阵fill、秩条件、装配/分解/IRLS各占比；旧OMP oversubscription反例 |
| rejection小stack稳健选择核 | 每像元栈不重复分配长向量，科学方法条件不退化 | N2/3/4/8/16/64分层性能、临界值/失败率，科学TPR/FPR独立 |
| block planner microchunk | 固定内存开销不能盖过预算，按块自然缩容 | 输入超过budget返回resource_exhausted而非malloc/OOM；small/full/boundary |
| export SharedTileCache | across worker one bounded LRU；negative cache防重复失败tile读取 | RSS随workers的增量仅scratch、tile cache≈cap×1MiB、IO失败读次数≤1/identity |
| ExportStreamScheduler read→compute→write | bounded queue及真正CPU/IO overlap，取消/故障不死锁 | queue_depth/blocked time/throughput、IOPS、writer待写峰值和panic injection |
| 旧scheduler ThreadBudget及OMP并行轴修复 | F×I不超售、ICP线程局部恢复、单节点内核有真实并行度 | real active CPU/OMP team counts/并发高水位、不同worker worker-pool API、缓存复制次数 |
| P3FitsStream原子发布 | 不整幅常驻；新进程独立可读，失败不污染上版 | writer buffered bytes / checksum / fsync / rename / reopen，故障注入 |

上述表不要求**复制**旧类实现，只要求迁入V2唯一Host/Storage后不丢其运行优化。旧数值近似不合新科学模型时保留其候选/缓存/调度策略，而不是其错误面积数值。

## P-03 物理工作量与内存方程：高NSIDE不能靠扩大内存

- 一张4096² float32原生图约64MiB；但是实际峰值包含原始FITS解码、masters、calibrated、DQ、PSF/匹配、几何候选、源像元WCS/footprint、R COO/CSR、signal/support、sky/weight/selected、产品编码、FITS缓冲、线程scratch、cache、第三方分配。不能只拿64MiB估RSS，也不能因256²变出6.1GiB就断言所有字节都是“合理高阶”。
- 高阶_HEALPix全集_叶数`12×4^order`，order18全球空间远不可枚举/存储；**只按M42受支持角面积/原始像元足迹估当前触及叶、CSR nnz和tile bytes**。从order13到18等面积叶数最多扩1024倍，但实际有效叶与重叠规模取决于源像元数/footprint；不可把1024作为任何真实帧文件量的直接预测。
- 单执行块静态预算建议显式分解：
  `M_live = M_immutable_masters + M_source_buffers + M_geometry_candidates + M_R_nonzero + M_output_leaf + M_cov_actions + M_sky_controls + M_tile_cache_shared + F*M_worker_scratch + M_staging + M_external_library + M_margin`。
  并给`M_R≈(nnz×(value+col_index))+(nrow+1)×row_ptr`、`M_tile_shared≈min(cap,active_tiles)×512²×dtype`、`M_pipe≤K×block_bytes`的真实变量/单位和零溢出检查；全部对象不可双算也不可漏算。
- 由同一MemoryScope为bytes/CPU/IO/spill计费。可优先减少“重复拥有的常驻对象”、touched-only、block CSR、on-demand covariance_apply、Sky小型global屏障、按需Export Q、共享Tile LRU与顺序Spill；严禁默认把N×N大矩阵或上亿叶密集double平面塞进RAM。
- 资源运行用安全余量（目标RSS≤10GiB，在12GiB process限额下保留给OS/第三方 allocator）；目标值不是全应用固定物理常数，可按机器实际和合同说明，但正式申请不能突破用户资源上界。文件系统剩余必须预留≥50GiB或20%取较大值（资源不足先安全重用/清理无引用staging/压缩，不删原始、已引用缓存与他项目）。
- 高Nside工作瓶颈可能是interval精化/MPFR、CSR序列化与大量tile metadata；在没有画像之前不得擅自改物理pixfrac/降低order/少跑帧/禁用噪声来规避。

## P-04 分阶段固定性能实验，不无限跑全量

| 阶段/输入 | 算法与质量固定条件 | 需要的性能证据 | 不应重复什么 |
|---|---|---|---|
| P0 tiny oracle32² | 两分支相同WCS/pixfrac/nside，同等合法几何 | 函数调用数、cache/CSR、工作内存、线程值 | 不跑真实4096² |
| P1 128²、256²、512²真ROI | 同一局部WCS，冻结几何/模型 | 每一规模有效叶、nnz、RSS/bytes/CPU曲线 | 修writer不重算Gaia/master |
| P2 一帧4096²高阶 | 真autoPlan、完整校准/WCS/PSF/R | 每模块peak/wall/存储、full FITS质量 | 不用order13基线冒充高阶等价 |
| P3 两个真实不同面板ROI与4096² | 真Overlap + 全局Sky + coadd + Export | tile可用性/最坏重叠、高频/sky控制等效工作量 | 不为接缝参数变化重跑Normalize |
| P4 六面板逐通道、196帧 | 同一共同WCS/合法响应，已缓存正确上游 | 每帧有效处理时间、吞吐估计、实际进度/峰盘 | 不重新SHA196源/复制FITS/重复已成功帧 |

正式跑一个大阶段之前必须先给时间/内存/盘容量的合理外推式；外推失准则停在最小有判别力的输入修优化，再执行剩余块，不能重复启动196帧。每个性能优化必须附“改前同输入/改后同输入/冷暖cache/代码SHA/科学恒定”证据。

## P-05 性能FAIL自主处理次序

1. 先分清有效输入变大、数学更精确、模型状态不同、代码路径变慢、CPU限额/磁盘缓存不同，禁止拿不等价数据硬比；
2. 看profile/火焰图/heap按CPU%、allocation bytes、读盘重复次数排序，修最大真实热点，而不是盲加缓存或用更粗NSIDE；
3. 优先消除双持有、重复decode、无界全域Tile、全局锁持IO、N×N nested OpenMP、全局dense C、重复标度/天测；然后再考虑SIMD/批处理/更合理数学近似（若经过科学认证）；
4. 改动后同质量/同硬件复测，确认数值/支持/孔径/PSF/天空和故障行为不退化；若目标资源不可实现具名BLOCKED_RESOURCE并记录正确可用缓存，而不是以conditional或降低质量关闭任务。
