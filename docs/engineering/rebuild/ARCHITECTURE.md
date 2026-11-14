# V2 工程架构：沿 main 的生产算法重新组织

## 1. 工程层级与唯一所有权

V2 不以原22个Process数量衡量模块化质量。要分清模块**负责什么数据/数学能力**、谁消费、如何管理内存与并发。能保留的既有Process注册表作为适配入口使用；不为保留57/58个operation的表面数量而迫使旧算法分裂成几十个全局产物。科学函数由其模块拥有，工作流只组合，不重复实现。

```text
CLI / executable （Normalize | Mosaic | Export，允许同进程编排）
  └── Workflow orchestration（计划/模块依赖/小型屏障/产物发布）
        ├── Observation：原始FITS读取、校准、DQ、原生块
        ├── Astrometry/Catalogue/WCS：Gaia / IPV / TAN-SIP / Jacobian
        ├── Source/PSF/Photometry：点源、像元PSF、响应与相对尺度
        ├── Geometry/Drizzle：auto NSIDE、原生像元→球面leaf
        ├── Coverage/Sky：天区并集、控制点/联合背景与相对加性偏差
        ├── Rejection/Integration：传统逐位置样本筛选与归一加权叠加
        └── Projection/Export：高阶HiPS→目标WCS/FITS
                 ↑ 每个模块经 runtime 的 Buffer/Task/Pipe/Storage
    Runtime（唯一：CPU Task、内存配额、I/O、cancel、共享缓存、产品）
```

同一科学处理模块内部可以再细分 pure kernel，但**不在每层再挂一个Store、线程池、队列、JSON中间协议、另一个独立CLI**。实际 source header `include/acsd/model.hpp` 的 `ArrayRef`只是immutable数组描述，真实数据在Storage中；`ArrayView`含Lease，引用共享区间时必须保持owner存活。仅凭类型名叫“memory”不能视为已经按块交接数据。

## 2. main 复用边界和迁移代码原则

主分支科学入口：`lib/infrastructure/cli/commands.cpp::run_phase{1,2,3}_block` 调用 `lib/infrastructure/scheduler/src/module_adapters.cpp` 里的 `p1_op_* / p2_op_* / p3_op_*`。后者约1.86万行，混合参数解析、C ABI、文件名、调度与算法调用。**不移植其体量、全局变量和文件协议**；按真实调用追到 `lib/algorithms/` 下纯核，抽入本仓V2的一个模块，接口适配在外层。

- **保留算法主体**：原有Calibrator、IPV、像元积分PSF、测光、Drizzle自动NSIDE/候选和归约、Coverage/Sampler、Sky Plane、Rejection、Integration、P3 WCS/resample/FITS。
- **保留正确性能实现**：touched leaf、每线程bounded geometry cache、stripe固定归并、稀疏/分块Sky、动态microchunk、跨worker共享TileCache、负缓存、流式Writer。
- **替换工程组织**：旧Phase内部`p1_*.json` / 图像临时FITS / `p2_*.bin`文件消费者；多份解码器/像元数组；旧Scheduler/Executor/ThreadBudget和旧AIO长期状态；过度耦合的巨大adapter。
- **仅修证实的科学错误**：旧SIP反算、错误滤镜身份、Drizzle不真实覆盖、旧小N排异对背景平移不等变、单位/权重误用、假有效像元、FITS不合法等，沿真实生产数据流定位。拒绝另建高阶统计/超分/flat算法研发。

## 3. 模块间最小实际数据合同

`FrameSource`只负责raw引用与科学metadata；`NativeBlock`应是符合现有typed模型的校准像元、DQ、原像元Index、Lease（命名可由实际头决定）；`FrameModel`包含WCS/PSF/photometry/response的小型不可变对象；`LeafFragment`包含共同leaf order、touched leaf IDs、signal、支持、来源像元贡献；`MosaicBlock`包含真实空间叶域、每帧可用候选、排异原因、合法权、合成signal/support；`RasterBlock`包含输出矩形像元区间及WCS对应科学值。

这些是**数据责任/粒度说明，不是第二套可编译struct定义**。现有 `ArrayRef/ArrayView/BlockRequest/FramesProvider` 足够时直接适配；缺少“按被请求叶块供数/持Lease”的必要能力时扩展唯一公开头，并与所有消费者同时更新。不要同时支持两个几乎相同的provider协议。

**唯一索引和单位**：原CCD原生索引、HEALPix `(order,ipix)`、目标FITS像元，不能在消费者偷换；响应/曝光/PSF/标度应用身份和上游源hash沿数据对象传递。内部float/double工作精度按原算法与正确性要求选择，不能用变窄dtype节省内存并假装输出等价。全部大型buffer明确producer、consumer、最后借用者、回收点。

## 4. 最小屏障与真实内存连续性

有些操作科学上需要完整帧星表或多帧全局Sky解：可先读取若干块形成**小型统计摘要**，冻结解以后继续读同一原始内容的块做后续处理。数据必须根据需要**重读原始只读块或复用内存cache**，而不是整幅图的每个中间版本常驻或在临时文件目录层层传递。对复数帧可按空间region组织，Sky全局拟合只冻结小型参数而非所有输出叶。

默认科学中间块在有Lease的有界Pipe传输；只有跨不同CLI进程、重启恢复、无足够RAM要受控spill、最终HiPS/FITS产品时使用Storage。已经正常持有内存块时不因“有Storage接口”而强制序列化再读回。**这才是以内存管道代替旧文件驱动执行**。详见 [PIPELINE](PIPELINE.md)。

## 5. 生产错误与真实状态

绝不以`exit=0`或`publish`成功隐藏无WCS、空有效叶、坏master、Sky不可辨认、负权、通带不一致、NaN输出、资源耗尽、取消。分别由原生产模块解释并传递，不由Runtime根据后缀和文件名猜。科学条件不足的合法部分成品可以具名，但不能宣称全部M42已经完成或画质已合格。

本章定义工程归属，逐文件迁移见[MODULES](MODULES.md)，资源与重算见[PERFORMANCE](PERFORMANCE.md)，审查实际代码见[REVIEW](REVIEW.md)。
