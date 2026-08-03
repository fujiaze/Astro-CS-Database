# 测试与验证矩阵

详细实验见 `17_CLASSIC_EXPERIMENT_SUITE.md`。

## 1. 构建

- Windows/MSVC CPU-only；
- Linux/GCC、Clang CPU-only；
- 无GPU SDK；
- 至少一个真实GPU backend；
- Debug/Release；
- ASan/UBSan实际构建；
- TSan适用CPU路径。

工具链不支持的组合明确SKIPPED，不能虚报。

## 2. 架构迁移

- 旧 `routes.json` 不再生成；
- 无 `preferred_backend` per-kernel路由；
- 无CPU/GPU share API/schema；
- HardwareProfile schema有效；
- TaskTraits真正进入CostEstimator；
- Public API真正进入Dispatcher/backend；
- 无画像CPU fallback明确。

## 3. HardwareProfile

- CPU ISA/线程/尺寸；
- FP32/FP64算术；
- STREAM CPU内存；
- BabelStream GPU显存；
- H2D/D2H/pinned；
- reduction；
- direct/separable/FFT卷积；
- gather/scatter/atomic/histogram；
- branch/work variance；
- submit/launch/event/alloc/merge；
- missing/valid/stale/partial/corrupt；
- 指纹；
- 模型留出验证；
- profile只读。

## 4. CostEstimator

- 不同任务类别映射不同能力族；
- 尺寸、驻留和固定开销影响选择；
- 小任务CPU、大device-resident任务GPU的合理性；
- 低置信度惩罚；
- RAM/VRAM约束；
- 预测误差报告。

## 5. 动态Mixed

- CPU和真实GPU同时完成不同唯一块；
- 无GPU时SKIPPED；
- 多GPU可用时独立领取；
- coverage每块恰好一次；
- GPU预忙时CPU继续；
- CPU预忙时GPU继续；
- 尾部收缩；
- 故障回收未开始块；
- profile hash前后不变。

## 6. 资源控制

- 所有CPU线程可参与；
- 50/80/95/100是利用率目标；
- 真实利用率或明确估算；
- GPU队列水位；
- RAM/VRAM上限；
- 状态和取消响应；
- 控制器不修改画像。

## 7. 数值

- FP32/FP64/FP64 accumulator；
- NaN/Inf/signed zero/subnormal基础；
- CPU/GPU末位差异允许；
- integer histogram/scan exact；
- deterministic merge；
- fast-math gate。

## 8. 主线

- 算法目录零diff；
- 现有测试通过；
- CPU-only默认构建；
- 普通启动不初始化ACR、不探测GPU、不发警告；
- 合并后重复验证。
