# HardwareProfile Qualification Benchmark 规范

## 1. 目标

Benchmark 生成本机多维硬件能力画像，不生成总分、业务 kernel 路由或固定 CPU/GPU 百分比。

独立工具：

```text
acr-benchmark --profile quick|standard|full
acr-status
acr-report
acr-invalidate
```

## 2. 启动提示

```text
即将执行本机计算性能标定。请停止其他CPU/GPU密集任务并保持系统空载。
标定期间不要同时运行AstroCS计算任务。程序不会检查或关闭其他应用。
```

提示后直接继续。不得替用户判断空载，也不得强制执行。

## 3. 测试框架

优先使用 Google Benchmark 或经 ADR 批准的成熟等价框架。ACR 自研部分只负责：

- GPU event计时；
- 设备和驻留参数化；
- 持续负载；
- 原始结果聚合；
- Profile拟合、指纹和验证；
- 中断/超时和结果完整性。

不再维护只能测少量 kernel 的孤立自写 BenchmarkDriver，除非其被证明提供框架缺失能力并有清晰边界。

## 4. Profile级别

### quick

CI/开发冒烟，不生成正式可用画像。

### standard

默认正式标定，有限经典矩阵，生成可用 HardwareProfile。

### full

增加大尺寸、持续负载、NUMA、多 GPU、更多卷积/稀疏点和专用库。仍不做业务算法穷举。

## 5. CPU画像

### ISA与线程

仅执行硬件、OS和构建都支持的：scalar/baseline、SSE4、AVX、AVX2、FMA、具体 AVX-512 子集。

线程粗点：

```text
1, 2, 4, 约25%, 约50%, 约75%, 约95%, 100%
```

这是画像采样，不代表正式运行少开线程。在交叉点和前两名 ISA 附近有限补点。

### 算术

FP32/FP64：add、mul、FMA、div、sqrt；必要时 sin/cos。分别测独立链吞吐和依赖链延迟，防止只测一种流水线特性。

### 内存

STREAM 思路：Copy、Scale、Add、Triad；尺寸覆盖 L1/L2/L3 和明显超出 LLC 的主存区域。记录线程、ISA、NUMA本地/远端和持续带宽。

### Reduction

sum、dot、min/max、sum of squares、mean/variance：

- FP32输入+FP32累加；
- FP32输入+FP64累加；
- FP64输入+FP64累加。

## 6. GPU画像

### 显存

BabelStream 思路：device-resident Copy、Mul/Scale、Add、Triad、Dot。显存带宽不得混入 PCIe 时间。

### 传输

普通和 pinned host memory：

- H2D；
- D2H；
- 双向并发；
- 4KiB到大块对数尺寸；
- 传输与计算重叠；
- 多 GPU；
- NUMA本地/远端 host buffer。

### 算术与不规则能力

FP32/FP64算术、reduction、atomic、gather/scatter、热点 histogram、分支发散和工作量不均。

## 7. 卷积与Stencil

至少：

- direct 2D：3×3、5×5、7×7、15×15、31×31；
- separable：5、15、31、63；
- Sobel、Gaussian、box和固定随机不可分离核；
- FP32/FP64；
- contiguous/stride；
- clamp/mirror；
- kernel-only、device-resident和host端到端；
- Tile/halo候选；
- 大核可选成熟 FFT卷积 adapter。

不能只记录 GOp/s；还要记录 transfer、workspace、halo重复和总完成时间。

## 8. 不规则、原子和分支

- Gather：identity、reverse、prime stride、random；
- Scatter：无冲突、均匀原子、局部热点、单热点；
- active fraction：1/5/10/25/50/100%；
- Histogram：uniform和高热点；
- SpMV风格稀疏读取；
- Mandelbrot或等价工作量不均实验。

## 9. 固定开销

- oneTBB提交；
- ACR dispatcher提交；
- GPU launch；
- event创建/等待；
- queue/stream同步；
- host/device/pinned allocation和复用；
- merge；
- 首次初始化与温态执行。

## 10. 采样方法

- 尺寸使用对数序列；
- 粗扫→淘汰错误/明显劣势→交叉点/缓存边界补点→持续验证；
- 数据构造不计时；
- 至少3次预热；
- 至少9次正式样本或累计1秒；
- 关键路径持续至少30秒；
- 保存全部原始样本；
- 输出 median、p95、MAD/CV；
- 所有外部进程和硬件等待有明确超时。

## 11. 正确性门禁

错误实现不得进入画像。保存 seed、输入、设备、ISA、后端、误差和日志。其他候选可继续测，但画像必须标记 partial/invalid。

## 12. 模型拟合

- 按 log2 尺寸分段线性或单调插值；
- 使用留出点验证；
- 关键能力族 median相对预测误差目标建议 ≤15%；
- 交叉点附近可更宽，但必须报告；
- 低置信度曲线必须标记并在 CostEstimator 中加惩罚；
- 正式运行不得用实际任务时间更新模型。

## 13. HardwareProfile

正式数据结构见 `schemas/hardware_profile.schema.json`。至少包含：

```text
arithmetic / memory / transfer / reduction / convolution /
irregular / branch / overhead / library / confidence / fingerprint
```

指纹至少包含 CPU stepping、OS ISA状态、cache/NUMA、GPU UUID/PCI、driver/runtime、编译器、flags、ACR commit、依赖锁和 backend实现hash。

## 14. 输出

```text
compute_profiles/<fingerprint>/
  hardware.json
  dependency-lock.json
  qualification.raw.json
  validation.json
  hardware-profile.json
  model-fit-report.json
  summary.md
  logs/
```

正式运行只读。
