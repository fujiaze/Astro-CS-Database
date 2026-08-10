# ACR经典实验与硬件画像套件

## 1. 总则

只使用独立、确定性、可复现的经典实验，不引用AstroCS真实算法。默认随机seed：`0xA57C5AC20260802`。每项分Correctness、Profile、Dynamic和Fault（适用时）。

FP32通用比较：

```text
abs(a-b) <= 1e-5 + 5e-5*max(abs(a),abs(b))
```

FP64：

```text
abs(a-b) <= 1e-12 + 1e-11*max(abs(a),abs(b))
```

大量累加使用操作特定容差和FP64参考；整数必须exact。

## 2. E01 STREAM式CPU内存

Copy、Scale、Add、Triad；FP32/FP64；尺寸从小于L1到明显大于LLC，最后至少覆盖可用内存预算内的大数组。记录单线程、线程曲线、各ISA、NUMA本地/远端、GB/s。数组构造不计时。

验收：逐元素正确；大数组持续带宽稳定；能识别缓存和主存区域。

## 3. E02 BabelStream式GPU显存

Device-resident Copy、Mul、Add、Triad、Dot；FP32/FP64；多尺寸。不得包含PCIe传输。记录kernel-only、显存GB/s和不同queue配置。

验收：正确；与H2D/D2H结果分开。

## 4. E03 Host-Device传输

普通/pinned host memory；H2D、D2H、双向；4KiB到大块对数尺寸；计算重叠；多GPU；NUMA本地/远端。

模型输出：固定延迟、尺寸带宽曲线、重叠效率。

## 5. E04 算术吞吐

FP32/FP64 add、mul、FMA、div、sqrt；CPU各ISA/线程和GPU。使用足够寄存器独立链避免只测单一依赖延迟，并同时提供依赖链latency实验。关键数学函数可测sin/cos。

验收：防止编译器消除；保存汇编/编译报告抽样；持续负载反映降频。

## 6. E05 AXPY与融合逐元素

`Y=alpha*X+Y`以及多输入融合表达式；覆盖memory-bound与balanced elementwise。FP32/FP64，host/device resident。

用途：校验通用elementwise成本模型，不作为总分。

## 7. E06 Reduction Family

sum、dot、min/max、sum of squares、mean/variance；FP32/FP64/FP32+FP64 accumulator；尺寸2^10逐级增加至预算上限；正值、正负交替和动态范围输入。

参考：scalar FP64或更稳定算法。Mixed时各设备局部结果后合并。

## 8. E07 Matrix Transpose

511×509、2048²、8191×4093；uint32或精确FP32；Tile 16/32/64；CPU/GPU。逐元素exact，覆盖边缘、stride和局部内存。

## 9. E08 Direct 2D Convolution

核：3×3 Sobel、5×5 Gaussian、7×7固定核、15×15 Gaussian/box、31×31不可分离固定随机核。图像：512²、2048²、4096²、8192²；脉冲、梯度和固定随机图；FP32/FP64；clamp/mirror；连续和stride。

参考：CPU scalar FP64 accumulation。分别测kernel-only、host end-to-end、Tile/halo。输出max error、RMSE、吞吐和总时间。

## 10. E09 Separable与FFT卷积

可分离长度5/15/31/63；大核FFT adapter。比较direct、separable、FFT在不同尺寸的交叉点。FFT必须使用成熟库，不自写生产实现。

## 11. E10 Bilinear Affine Resampling

固定平移、旋转和非均匀scale；棋盘、斜坡、随机图；513×509、2048²、4096²；FP32/FP64；zero/clamp边界。参考FP64坐标和插值。

## 12. E11 Histogram与原子

uint8 256 bins；uniform、90%热点、ramp、随机；尺寸2^16至2^26。实现全局atomic、block/local histogram+merge。整数exact。记录冲突程度曲线。

## 13. E12 Prefix Scan

inclusive/exclusive；uint32输入、uint64输出；N=1、3、1025、2^20+17。使用oneTBB/CUB/rocPRIM/oneDPL等成熟primitive adapter。逐元素exact。证明依赖任务不能当普通parallel_for。

## 14. E13 Gather/Scatter/SpMV风格

index：identity、reverse、prime stride、随机permutation；active fraction 1/5/10/25/50/100%；无冲突scatter和整数atomic scatter；可选固定稀疏矩阵SpMV。exact或FP64参考。

## 15. E14 Branch与不均匀工作

Mandelbrot整数逃逸计数，区域覆盖快速逃逸、边界高迭代和混合；1024²、4096²。输出exact。画像记录uniformity与设备差异。

## 16. E15 GEMM/FFT成熟库adapter

GEMM：256、1024、4096及非方阵；BLAS/cuBLAS/rocBLAS/oneMKL。FFT：1D 1024/65536/1000，2D 512²/2048²；FFTW/pocketfft/cuFFT/rocFFT/oneMKL。不可用标记SKIPPED。

这些只建立library-call能力曲线，不代表通用任务总分。

## 17. E16 固定开销

测oneTBB任务、ACR dispatcher、GPU launch、event、同步、allocation、pinned allocation、merge、首次初始化和温态执行。使用空kernel/最小工作并防止优化消除。

## 18. E17 Hardware Profile拟合验证

将原始数据拟合为按log2尺寸的分段曲线。使用留出测试点验证预测误差：

- median相对预测误差目标建议<=15%；
- 交叉点附近允许更宽但必须报告；
- 不满足时增加采样点或标记低置信度；
- 禁止通过在线运行改写模型。

## 19. E18 动态CPU+GPU工作池

不设置固定比例。对elementwise、convolution、reduction、histogram分别：

- 创建大量带唯一ID的chunk/Tile；
- CPU和真实GPU并发从共享池领取；
- 画像决定初始块大小；
- 预先让GPU忙、CPU忙和多GPU不均；
- 检查空闲设备继续领取；
- 检查尾部块缩小；
- coverage bitmap每项恰好一次；
- 输出正确；
- profile文件hash运行前后不变。

报告实际完成工作量比例，但它只是运行结果，不作为配置或固化路由。

## 20. E19 资源利用率控制

持续workload测试50/80/95/100%CPU/GPU利用率目标。所有CPU线程均可参与。报告实际平均、p95、控制误差、队列和系统响应。百分比仅是资源目标。

## 21. E20 故障和回退

插件缺失、无画像、画像损坏/过期、显存不足、launch失败、设备lost模拟、分配失败、取消、异常、profile只读。未开始块回收，已完成块不重复。

## 22. E21 持续与并发可靠性

30秒及更长持续关键实验；重复进程启动；多线程并发提交；Event生命周期；取消；ASan/UBSan实际开启；TSan适用路径；内存/句柄泄漏；所有命令超时。

## 23. 结果要求

每个case保存设备、后端、ISA、线程、尺寸、精度、驻留、seed、原始计时、正确性、错误、SKIPPED原因和commit。不得只写“全部通过”。
