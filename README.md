# ACSD v2 design branch

从[唯一架构入口](docs/engineering/rebuild/README.md)开始。基线 c5fee09fe；分支 rebuild/acsd-v2。不兼容旧系统；科学算法尚未实现。

```text
cmake --preset design
cmake --build --preset design
ctest --preset design
python tools/check_design.py
g++ -std=c++20 -fsyntax-only -Iinclude validation/contracts_compile.cpp
```

当前仅验证规范/schema/声明，不能证明科学算法可用。Windows 无CMake时可先用Python校验和已有C++20编译器检查声明。模块目录和平台构建定案见SYSTEM；负责人接受设计后才开始算法实现。

目标程序：acsd normalize|mosaic|export --json config.json；独立接口驱动acsd-contract --request request.json --response result.json。两者尚未实现。[示例](examples/README.md)是有效合同输入与期望模型，不冒称实跑。

[清理](docs/audit/CLEANUP.md)记录全部删除路径和基线blob，[审核](docs/audit/REVIEW.md)记录问题与处理。不创建PR、不改main、不强推。
