# Weighted Integration Reference

这些文件是未来Agent实现的接口与算法参考，不是绕过当前Dispatcher的独立替代框架。

落地要求：

- 使用当前`KernelRegistry / KernelInvocation / Dispatcher / ResidencyManager`；
- 不直接在Benchmark中手工按固定比例切CPU/GPU；
- OpenMP只作为独立基线；
- ACR CPU launcher不得嵌套OpenMP；
- CUDA输入必须resident；
- 输出按WorkToken拥有范围写入；
- 实际统计必须来自Dispatcher/Executor完成事件。

参考文件可按仓库API命名调整，但公式、数据布局、模式和验收不得更改。
