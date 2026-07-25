# 11 测试体系

## 1. 层次

- Contract：数据块、ABI、JSON schema、格式 round-trip。
- Unit：单算法与数学函数。
- Component：模块 DLL + 真实/合成输入。
- Pipeline：Stage1/Stage2 单命令。
- Scientific regression：WCS、PSF、测光、SNR、Drizzle、Stack 数值。
- Fault injection：缺依赖、损坏文件、超时、取消、磁盘失败。
- Performance/soak：内存、线程、长批次。

## 2. PlateSolve 路径决策测试

- 在候选代码运行前冻结全量 TestData manifest 和比较门限；
- 旧路径每案例至少运行 3 次以测量重复性；
- 路径 A 对全量 TestData 逐例 A/B；
- 任一案例退化即选择路径 B，不得以总体均值覆盖；
- 路径 B 验证 detection sink 开关不改变 PlateSolve 输出；
- 最终路径必须写入 capability、结果 JSON 和 HISS provenance。

## 3. 单次检测与 PSF 测试

- 在 star_detector 测试 shim 中统计调用次数；
- 路径 A 验证 PlateSolve 不再加载 detector；
- 路径 B 验证 detector 只在 PlateSolve 内调用一次；
- 给 `star_det` 加 hash，PlateSolve/PSF 记录同一 hash；
- float32 PSF 对高动态范围样本验证无 65535 截断。

## 4. 测试不可篡改原则

不得通过删除测试、放宽门限、屏蔽异常或用占位输出使 Gate 通过。门限变化必须有 ADR、旧新分布和科学理由。全量 TestData 清单冻结后不可因候选失败而移除案例。
