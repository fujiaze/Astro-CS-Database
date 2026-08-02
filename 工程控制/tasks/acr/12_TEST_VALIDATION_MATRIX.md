# 测试与验证矩阵

详细数据见`17_CLASSIC_EXPERIMENT_SUITE.md`。

## 1. 构建

- Windows/MSVC CPU-only；
- Linux/GCC和Clang CPU-only；
- 无GPU SDK；
- 实际可用CUDA/HIP/SYCL后端；
- Debug/Release；
- shared/static依仓库支持。

无真实硬件只允许编译验证，不宣称运行通过。

## 2. API和连接性

- 空/单元素/非整Tile/大范围；
- shape/stride/access；
- Event、取消、异常；
- TaskTraits进入CostEstimator；
- public API真正调用dispatcher和backend；
- 禁止忽略OperationId/traits；
- 禁止公开CPU/GPU share参数。

## 3. Hardware Profile

- CPU ISA/线程/尺寸曲线；
- STREAM式CPU内存；
- BabelStream式GPU显存；
- H2D/D2H和pinned；
- FP32/FP64算术；
- reduction；
- direct/separable/FFT卷积；
- gather/scatter/atomic/histogram；
- branch/work variance；
- submit/launch/event/alloc/merge；
- missing/valid/stale/partial/corrupt；
- 指纹变化；
- profile只读且无在线更新。

## 4. 动态混合调度

不测试固定百分比。测试：

- CPU和GPU均从共享池领取；
- 多GPU；
- coverage每项恰好一次；
- 画像预测决定块大小；
- GPU预忙时CPU继续领取；
- CPU预忙时GPU继续领取；
- 尾部块收缩；
- 数据驻留影响决策；
- 小任务因启动/传输成本保留CPU；
- 设备失败时未开始块回收；
- 不为填满设备执行负收益迁移。

## 5. 资源

- CPU所有线程可参与；
- 50/80/95/100是利用率目标测试，不是任务比例；
- GPU队列水位；
- RAM/VRAM上限；
- 控制器不修改画像；
- 系统可响应取消和状态查询。

## 6. 数值

- FP32/FP64/FP64 accumulator；
- NaN/Inf/signed zero/subnormal基础行为；
- CPU/GPU末位差异允许；
- integer histogram/scan exact；
- deterministic merge；
- fast-math gate。

## 7. 可靠性

- ASan/UBSan实际开启；
- TSan适用CPU路径；
- 进程重启、profile重载、持续运行；
- 内存泄漏、race、deadlock；
- 所有外部进程明确超时。

## 8. 主线无回归

- 算法目录零diff；
- 现有测试全过；
- CPU-only默认构建；
- 普通启动不初始化ACR、不发警告；
- Pipeline和文件输出行为不变；
- 合并后再次通过。
