# 测试与验收

## 1. 正确性

每个目标合成Operation必须比较：

- Scalar/可靠CPU基线；
- CPU多线程；
- GPU-only；
- AutoMixed。

覆盖：

- FP32；
- FP32输入+FP64累加；
- 多尺寸和非整块尾部；
- 多块Drizzle-like scatter；
- NaN/mask/边界样本（适用时）；
- 取消、设备错误和OOM注入。

## 2. 工作池

必须证明：

- 每块恰好一次；
- 无遗漏、重复和旧attempt提交；
- 动态逆序完成安全；
- 设备失败后只回收未完成块；
- report与真实completion一致。

## 3. Mixed能力

至少一个大规模目标合成工作负载必须出现：

- CPU完成量>0；
- GPU完成量>0；
- 无固定份额输入；
- 不同设备块大小可观察；
- coverage正确。

性能验收：

- `AutoMixed`在资格工作集上的中位耗时不得比CPU-only、GPU-only、Mixed三种实测最佳值差超过10%；
- 只有实测Mixed有边际收益时才允许路由选择Mixed；
- 若目标硬件上GPU-only确实最佳，允许Auto自然退化，但仍须用独立测试证明Mixed执行能力正确；
- 不以“CPU和GPU都非零”替代性能验收。

## 4. 传输与驻留

- resident与host输入的阈值必须不同；
- 连续两个GPU算子的chain测试只允许一次必要上传和一次最终下载；
- report中的传输字节与实际buffer操作一致；
- GPU块不得重复上传完整共享输入。

## 5. 内存

- RAM、pinned、每GPU VRAM独立记账；
- 缩块后重新估算；
- 等待与恢复有效；
- fallback重新评估成本与容量；
- OOM路径无泄漏、漏算和死锁。

## 6. 工程质量

- CPU-only构建通过；
- CUDA构建和真实设备测试通过；
- ASan/UBSan覆盖CPU ACR路径；
- compute-sanitizer memcheck/racecheck覆盖目标GPU内核；
- 所有外部进程、硬件等待和测试有明确超时；
- 0 failed、0 timeout，SKIPPED原因准确。

## 7. 范围门禁

- AstroCS现有算法、OpenMP、Pipeline和正常CLI零修改；
- 不把低负载模块包装进ACR；
- 不以扩充通用Benchmark数量代替目标性能证据。
