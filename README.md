# ACSD v2：固定基建与科学process

从[设计入口](docs/engineering/rebuild/README.md)开始：[一份基建骨架](docs/engineering/rebuild/SYSTEM.md)维护注册、生命周期、调度和服务；[各科学process](docs/engineering/rebuild/processes/README.md)独立维护算法与模块边界；公共API/对象/执行/验收标准统一复用。

基建以VirtualComputePool统一CPU/内存/I/O资源，提供Task、有界内存管道和异步I/O；算法通过[封装好的计算模块](docs/engineering/rebuild/standards/COMPUTE_MODULES.md)与强类型函数组合，实现逐模块维护和后续接入。

内部开发只维护一套当前格式，允许破坏式重构，不兼容v1或早期v2产品/接口，也不实现旧产品转换。Git历史及原始科学证据保留。GPU只设计KernelExecutor/ArrayBacking/Completion扩展，本期CPU，不实现GPU后端。

本轮只改文档。分支已含部分科学实现，新的标准注册和基建仍待接线，不能把设计当运行能力。新增/修改算法按模块process文档与唯一typed descriptor推进，不需要向框架加入算法分支。

```text
python -m pip install -r validation/requirements.txt
cmake --preset design
cmake --build --preset design
ctest --preset design
python tools/check_design.py
```

上述是现有设计检查入口，实际科学构建/运行状态依CMakePresets与冻结验证记录。[本次更正与复核](docs/audit/process-framework.md)记录文档范围和验证边界。继续在rebuild/acsd-v2追加提交，完整保留已有历史。
