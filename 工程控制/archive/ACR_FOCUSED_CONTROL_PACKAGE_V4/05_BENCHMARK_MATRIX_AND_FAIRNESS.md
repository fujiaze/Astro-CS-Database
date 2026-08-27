# Benchmark矩阵与公平性

## 1. 预设矩阵

### quick（默认CTest）

|尺寸|帧数|
|---:|---:|
|512×512|8|
|1024×1024|8|
|2048×2048|8|

### standard（正式Evidence）

|尺寸|帧数|
|---:|---:|
|512×512|16|
|1024×1024|16|
|2048×2048|16|
|4096×4096|8|

### full（人工资格测试）

|尺寸|帧数|
|---:|---:|
|1024×1024|32|
|2048×2048|32|
|4096×4096|16|
|8192×8192|8，容量允许时|

若预测使用超过可用RAM或VRAM的70%，必须明确`SKIPPED_CAPACITY`，不得交换到swap继续跑。

## 2. 重复与统计

- Release构建；
- 每模式2次warmup；
- 7次有效重复；
- 报告median、min、p90；
- case执行顺序可固定轮转，避免总是让某模式先跑；
- GPU context/runtime初始化单独记录，不计入steady测量；
- cold端到端必须明确包含哪些传输；
- resident测量必须明确prefetch是否在计时外；
- 每case硬超时120秒；standard整体900秒；full整体1800秒；
- 所有启动外部进程或等待GPU的脚本必须强制超时并在超时后终止子进程。

## 3. 公平比较

- 所有模式使用完全相同输入、权重和输出语义；
- OpenMP与ACR使用相同编译优化级别；
- OpenMP线程数与ACR CPU worker上限同时记录；
- 不把数据生成、文件I/O或首次依赖下载计入算法耗时；
- 不把GPU context初始化隐藏进某一模式；
- 不用ForcedMixed结果代表Auto；
- 不因小尺寸GPU较慢而判定实现失败；必须报告交叉点。

## 4. 单次与复用场景

### single-shot

每个输入帧栈只执行一次加权积分。比较OpenMP与Auto的真实端到端收益。

### resident-reuse

同一帧栈连续执行4组不同权重：

- frames只上传一次；
- 每次只更新weights；
- 每次输出按需要物化；
- 报告总耗时与平均每次耗时；
- 验证input H2D count为1。

该场景用于证明显存利用与传输复用，不取代single-shot结果。

## 5. GPU通道候选

- standard：1、2 stream；
- full：可测试1、2、3 stream；
- OperationProfile保存实测最优stream count；
- 若2/3 stream无收益，选择1并如实报告。

## 6. 性能结论

性能不是所有尺寸都必须加速。必须回答：

- 从什么`pixels × frames`规模开始GPU有收益；
- Auto是否正确使用CPU、GPU或自然退化；
- Mixed是否比OpenMP有提升；
- Mixed是否接近当前实测最佳合理模式；
- 传输和驻留分别贡献多少。

不得预先写死“必然提升”。
