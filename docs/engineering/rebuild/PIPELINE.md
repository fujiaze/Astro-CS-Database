# 一条真正运行在内存里的生产—消费流水线

## 1. 先明确现状：V2有Pipe，但生产代码仍绕开它

`src/runtime/pipe.cpp`已有队列容量 `max_items/max_queued_bytes`、有序传递和关闭/等待路径；`src/runtime/memory.cpp`有内存/线程/IO资源分账；`src/runtime/storage.cpp`有块/哈希与持久化。**这些对象已存在不说明科学流已接线。**

当前 `src/workflows/frames_provider.cpp::MemoryFramesProvider::read_leaf_block` 直接返回整帧signal/dq，未解释 `FrameReadRequest` 的 `leaves/halo/fields`。当前 `src/workflows/mosaic.cpp` 先收集全局 `std::set<pair<order,ipix>>` 与各帧整幅ArrayRef，再构成provider；`src/coadd/coadd.cpp` 对每帧做全域read/map/set和 `Nleaf×Nframe` 密集决策/权，且 `coadd.integrate` 需要一次完整目标叶域。mosaic接缝诊断仍包含三帧与线性NESTED邻接假设。这些不符合真正按块流动的要求。

**迁移不是新写个Pipe类**，是把真实算法每次的输入输出改到这条有界数据路径。

## 2. 流程结构

```text
Read Raw FITS block
    │  [原始像元 + 头/有效区域] 只读，受控预取
    ▼
Calibration / DQ block ───► Detection/PSF/Photometry small summaries
    │                               │
    │                               ▼ 模型冻结（只缓存小对象）
    ├──► Scale once (可与下游块循环融合)
    ▼
WCS footprint / Drizzle stripe (auto ResolutionPlan)
    │  touched leaf fragments + support + provenance
    ▼
bounded leaf-fragment Pipe ──► region/Tile accumulator ──► optional frame product cache
                                           │
                  ┌────────────────────────┴────────────────────┐
                  │     multi-frame region reader/provider       │
                  ▼                                              ▼
       Coverage / spatial controls  ───────────────► global Sky fit barrier
                  │                                              │
                  └─────────────── frozen Sky model ◄────────────┘
                                           │
                    region Pipe ► Sky apply ► Rejection ► weighted mean
                                           │
                                 Mosaic leaf/tile fragment
                                           │
                                region Pipe ► Target WCS Export
                                           │
                                streamed FITS writer + publish
```

实际图像可分次执行，因此独立`normalize`结束必须保留发布的frame科学数据以供另一个进程`mosaic`读取；独立`mosaic`/ `export`同理。**不可能跨已经退出的三个CLI进程只传内存**。统一Workflow内部组合运行时，则使用相同producer/consumer的内存provider减少临时磁盘往返；增加同进程编排入口属于工程调度而不是新科学功能，不能复制另一套Normalize/Mosaic/Export实现。

## 3. 生命周期与背压：必须能用代码走通

对每类大对象要能在实际源码指出：
- producer创建块时从唯一 `Resources`/MemoryScope申请其**真实字节**，不能以代理 `groups×65536` 估“全部叶”后再整幅申请；
- block带 `ArrayView/Lease` 或等价的SharedRef，只有消费者完成释放引用才归还容量，不能由上游`publish`立即回收仍被下游异步读取的buffer；
- Pipe同时限制`queue_items/bytes`；producer遇下游慢必须阻塞/挂起而不是自旋或先把整帧重复解码；有序块依据真实key归并，不因线程工作顺序改变归约；
- FrameReadRequest需要指定frame、叶域或有序范围、halo（用于需要邻居的采样/排异）、字段；provider必须**只返回请求范围的实际slice**，并且成功时证明附带索引与signal/DQ/support严格对齐。若当前`ArrayRef`只能整数组引用，则Storage `BlockRequest`与Provider适配为可按块读的视图/只读lease；
- 读到的每个Tile和frame块应只服务当前union-region，不能对每个目标叶重新打开一次完整frame；输入frame不覆盖本region时不创建空全域数组；
- 取消/错误/缺盘时上游停止新块，所有等待者有终止通知，剩余Lease与staging释放；既有最后成功产品不被破坏。

**禁止**在每帧保存4套raw/calibrate/scaled/drizzled dense full images；禁止在Mosaic构建`Nleaf×Nframe`密集工作表（当每叶只有少数实际覆盖帧）；禁止`read_leaf_block`按需请求却返回整个Frame；禁止对输出未触达天区预生成NaN HiPS目录。

## 4. 屏障可以有，持久化不能冒充屏障

- **WCS/PSF/Photometry**：源测量可能需要遍历完整帧；先边读块边归纳星列表/小模型，冻结WCS/PSF/标度，再对原始/校准块顺序遍历做Drizzle。既有calibration/master读取可走有界缓存，不必保留全部像元。
- **autoNSIDE**：在参与帧 WCS 有效后对全组冻结leaf order；这个metadata级工作在真正分配全分辨率R前完成。无法估计划时说明未知，不能随意低order。
- **Sky**：Coverage/Sampler在真实面板重叠区域形成全局足够统计量，先解并冻结共同天空面及帧差，再按各个region消费实际帧块作Sky apply→传统Reject/Mean。不能每region重新拟一个无共同gauge的sky，也不能因此保留每帧整幅校正数组。
- **Export**：消费Mosaic Tile/Leaf块构成目标WCS行/矩形块；共享bounded tile cache与写线程顺序一致，不生成同时存在的完整Output+Coverage+Variance数组。

## 5. 数据粒度和索引

目标图像科学最小单位是一个原始CCD像元的真实天区映射和一个HEALPix叶/目标FITS像元的有效贡献；**计算块大小可调**（行、stripe、leaf分区或512 tile），**不能改数学估计量/完整足迹**。给定空间region，仅取与其交叠的帧和该帧真实样本。各row按真实(leaforder,ipix)/frame ID排序；Sky/排异的二维邻域使用真正HEALPix几何邻接或WCS投影，不用ipix+1。

区域策略需由实际touched leaf、CSR条目、同region同时活跃帧、PSF/背景halo和可用RAM动态选择；避免“从512²试验推一个常数，乘全196帧固定分配”。处理完region即归还缓冲与临时缓存，所有持久数据都由已发布引用身份唯一拥有。

## 6. 缓存/落盘和文件所有权

在进程内每个算子可重用只读模型、master、Gaia索引、几何Lru、source block与tile cache；不同独立CLI或断点继续必须有最小科学产品/Manifest提供恢复。**缓存不是正式科学数据的另一份副本**：按source内容、有效WCS、PSF/response/scale、order/pixfrac、Sky参数/目标WCS建立真实依赖链，下游变化只使依赖它的块失效。省盘时首先回收已无依赖的临时spill，不覆盖原始数据，不将大FITS进Git。

每种整图中间数组必须回答“它为什么不能按块流过？是否是必要的全局统计量？”缺少可证明理由就拆块；真的需要不可局部化的耦合量只保存**小型可重开的全局模型**或有界外存spill，不能默默把所有像素全量物化。

## 7. 真实工程完成的含义

直接从源码证明阶段间通过有界Pipe/ArrayView/Lease或等价链传块，进程峰值按当前块/worker/缓存而不是总196帧×全幅大小增长。实际运行M42 4096²和多个不同面板，观察过数据是否经过模块、是否重复read/alloc、是否完成FITS输出。独立命令跨进程使用持久产品是合理例外。只具备接口、演示一个tiny buffer或内部数学自洽**不是内存流水线完成**。
