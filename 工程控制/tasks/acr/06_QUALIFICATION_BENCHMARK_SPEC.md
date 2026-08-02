# Qualification / 硬件画像 Benchmark规范

## 1. 工具形态

本支线不修改现有AstroCS CLI，提供独立工具：

```text
acr-benchmark --profile quick|standard|full
acr-status
acr-report
acr-invalidate
```

## 2. 开始提示

```text
即将执行本机计算性能标定。请停止其他CPU/GPU密集任务并保持系统空载。
标定期间不要同时运行AstroCS计算任务。程序不会自动检查或关闭其他应用。
```

提示后继续，不替用户判断。

## 3. Benchmark目标

生成“多维硬件能力画像”，不是总分、业务kernel排名或固定CPU/GPU比例。画像必须能回答：

- 不同ISA和线程规模的FP32/FP64吞吐；
- CPU主存、缓存、NUMA和GPU显存带宽；
- H2D/D2H延迟和带宽随尺寸变化；
- 归约、卷积、Stencil、随机访问、原子和分支的设备差异；
- 任务提交、kernel启动、event、同步、分配和merge固定成本；
- 数据已驻留与需要迁移时的差别；
- 不同问题规模的CPU/GPU交叉点和推荐块大小。

## 4. Profile

### quick

CI和开发冒烟；小/中尺寸；不生成正式画像。

### standard

默认正式标定；经典有限组合；生成可用画像。目标时间可由Agent根据测试规模设计，但不得为追求穷举无限增长。

### full

增加大尺寸、持续负载、NUMA、多GPU、专用库和更多卷积/稀疏点。仍不做业务算法比例穷举。

## 5. CPU ISA和线程曲线

只测试构建提供且硬件/OS安全支持的baseline、SSE4、AVX、AVX2、FMA和具体AVX-512子集。

线程点先测试：

```text
1, 2, 4, 约25%, 约50%, 约75%, 约95%, 100%
```

去重后运行。95%在这里只是性能曲线采样点，不是正式运行少开线程。对曲线拐点和前两名ISA有限补点。

每个ISA至少测：

- FP32/FP64 add、mul、FMA；
- div、sqrt；
- 必要的sin/cos或坐标数学函数；
- scalar与vector reduction；
- 持续负载下的频率回落。

## 6. 内存和互连

### CPU内存

按STREAM思想测Copy、Scale、Add、Triad。尺寸必须覆盖L1/L2/L3和明显超出LLC的主存区间，单独报告缓存曲线和持续主存带宽。

### GPU显存

按BabelStream思想测device-resident Copy、Mul/Scale、Add、Triad、Dot。显存测试不混入PCIe。

### 主机与设备传输

对普通和pinned host memory分别测：

- H2D；
- D2H；
- 双向并发；
- 4KiB至大块的延迟/带宽曲线；
- 传输与计算重叠；
- 多GPU并发传输；
- NUMA本地和远端host buffer。

## 7. 归约画像

操作：sum、dot、min/max、sum of squares、mean/variance。

精度：

- FP32输入+FP32累加；
- FP32输入+FP64累加；
- FP64输入+FP64累加。

尺寸采用对数序列，不只S/M/L三个点，例如从2^10递增至资源预算允许的大尺寸。记录启动成本、元素/秒、有效带宽和误差。

## 8. 卷积与Stencil画像

必须至少包含：

- direct 2D：3×3、5×5、7×7、15×15、31×31；
- separable：长度5、15、31、63；
- Sobel、Gaussian、box和不可分离固定随机核；
- 512²、2048²、4096²、8192²或预算降级；
- FP32、FP64；
- contiguous和带stride；
- clamp/mirror边界；
- host端到端、device-resident kernel-only；
- Tile与halo候选；
- 大核可选FFT卷积adapter。

报告不仅是GOp/s，还要记录输入、输出、halo重复、传输、workspace和总时间。

## 9. 不规则与并发写

- Gather：identity、reverse、prime stride、随机；
- Scatter：无冲突和原子冲突；
- Histogram：均匀和高热点；
- 稀疏active fraction：1%、5%、10%、25%、50%、100%；
- 原子冲突程度：均匀、局部热点、单热点；
- SpMV风格稀疏读取；
- 不同工作量Mandelbrot/迭代实验。

## 10. 固定开销

测量：

- oneTBB任务提交；
- ACR dispatcher提交；
- GPU kernel launch；
- event创建/等待；
- queue/stream同步；
- host/device分配与复用；
- pinned memory分配；
- 局部结果merge；
- backend首次初始化与温态执行，分别记录。

## 11. 测试尺寸与采样

尺寸使用对数序列和关键拐点，不把所有维度做笛卡尔积。推荐流程：

1. 粗扫所有候选；
2. 淘汰错误或明显劣势实现；
3. 在设备交叉点、缓存边界和吞吐拐点附近补测；
4. 对最终画像关键点做持续负载验证。

每case：

- 数据构造不计时；
- 至少3次预热；
- 至少9次正式测量或累计1秒；
- GPU用device event测kernel-only，并另测端到端；
- 保存全部原始值，不删除异常值；
- 输出median、p95、MAD/CV；
- 关键路线持续至少30秒；
- 外部进程和硬件等待必须设置超时。

## 12. 正确性门禁

性能前先做正确性。失败实现：

- 不进入画像；
- 保存seed、输入、设备、后端、误差和日志；
- 允许继续测其他候选；
- 画像标记partial/invalid，不静默忽略。

## 13. 画像数据模型

按设备和能力族保存分段曲线：

```text
DeviceProfile
  arithmetic[precision][operation][isa/thread or gpu]
  memory[level/residency][size]
  transfer[direction][memory_type][size]
  reduction[operation][precision][size]
  convolution[method][kernel_shape][precision][size/stride]
  irregular[pattern][sparsity/contention][size]
  branch[uniformity][size]
  overhead[submit/launch/event/alloc/merge]
  library[fft/gemm/...]
```

插值优先采用log2尺寸上的分段线性或单调插值，禁止用一个全局常数代表全部尺寸。

## 14. 指纹

至少包含CPU型号/stepping、ISA/OS状态、core/cache/NUMA、GPU UUID/PCI、driver/runtime、编译器、build flags、ACR commit、依赖锁、后端实现hash、操作系统和benchmark profile。

## 15. 输出

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

正式运行只读，不在线更新。
