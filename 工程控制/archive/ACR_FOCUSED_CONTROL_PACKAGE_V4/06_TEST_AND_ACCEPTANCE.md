# 测试与验收

## 1. 正确性

必须覆盖：

- SerialReference、OpenMP、AcrCpuOnly、AcrGpuOnly、ForcedMixed、AutoMixed；
- 不同尺寸、帧数和非整除尾块；
- CPU/GPU输出范围无重叠、无漏算；
- WorkToken retry/attempt不会重复写；
- frames/weights generation变化触发正确重新上传；
- 1和2 stream结果一致；
- 无GPU时Gpu/Mixed准确SKIPPED，CPU/OpenMP仍通过。

容差（合成值域约0.25～1.0）：

- 所有输出有限；
- `max_abs_error <= 2e-5`；
- `relative_l2_error <= 2e-6`；
- coverage必须等于pixel_count。

如当前编译器/FMA产生更大但合理末位差异，只能基于原始数据提出并记录新的明确容差，禁止直接放宽到掩盖算法错误。

## 2. 路由与分块

必须验证：

- 无固定CPU/GPU比例；
- CPU/GPU使用独立推荐块；
- GPU收益阈值不覆盖推荐块；
- Auto在worker启动前筛选GPU；
- 至少一个ForcedMixed case双方items均非零；
- Auto的实际设备集合与makespan判断一致；
- 尾段慢设备停止claim且无拖尾死锁；
- 实际chunk序列进入报告。

## 3. 驻留与通道

必须验证：

- frames在GPU执行前真实上传；
- single-shot每caseframes H2D最多1次；
- resident-reuse四次调用frames H2D仍为1；
- weights按调用更新，不能重复上传整帧；
- launcher使用device view，不创建每token全量host临时输入；
- GPU输出只物化GPU拥有范围或经过明确合并；
- 1/2 stream的in-flight、event和传输统计真实；
- cache释放/generation变化使resident状态失效。

## 4. 性能资格

### 硬门禁

- correctness全部通过；
- standard全部case完成或因容量明确跳过；
- 报告OpenMP、GpuOnly、AutoMixed中位耗时；
- Auto不得比实测最佳合理模式慢超过10%，除非报告可复现的明确原因并阻止业务接入；
- 至少一个中/大case的Auto相对OpenMP有正加速，建议资格线`>=1.05x`；若没有，样例仍可证明正确性，但状态必须为`PERFORMANCE_NOT_QUALIFIED`，不得开始真实业务改造。

### Mixed证据

- ForcedMixed只证明机制正确；
- 应争取至少一个standard/full case中Auto实际CPU/GPU均非零；
- 如果最优路由实测为单设备，允许自然退化，但必须显示为什么加入另一设备不能缩短makespan；
- 禁止为了满足Mixed表象而牺牲性能。

## 5. CTest与独立Benchmark

至少新增：

- `acr_weighted_integration_correctness_quick`
- `acr_weighted_integration_forced_mixed`
- `acr_weighted_integration_resident_reuse`
- `acr_weighted_integration_stream_consistency`

quick CTest单项TIMEOUT不超过180秒。standard/full作为独立Evidence命令运行，必须有整体超时。

## 6. 工具验证

- CPU-only和CUDA构建；
- CTest 0 failed、0 timeout；
- compute-sanitizer memcheck/racecheck覆盖加权积分CUDA路径；
- CPU sanitizer真实开启或准确说明工具链限制；
- path guard PASS；
- 最终单一干净HEAD；
- JSON schema和SHA可复核。

## 7. “允许修改业务代码”门

只有以下全部成立，才可输出`READY_FOR_BUSINESS_ADAPTER=true`：

1. 架构冻结文档与实现一致；
2. 加权积分所有正确性门通过；
3. Dispatcher真实resident与多token复用通过；
4. Auto路由性能已被standard矩阵证明；
5. 内存预算和故障回退通过；
6. Evidence来自最终单一干净HEAD。
