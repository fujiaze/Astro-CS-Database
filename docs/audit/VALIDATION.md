# 实际工程验证记录

最终设计树，2026-10-22。范围为声明和设计检查，不是科学算法验收。

| 实际执行 | 结果/边界 |
|---|---|
| Windows CMake design 配置与构建、GCC 16.1 C++20声明编译 | 通过；仅INTERFACE+object声明检查，没有科学链接实现 |
| Windows CTest design_integrity | 1/1通过 |
| Ubuntu WSL GCC 15.2 -std=c++20 -fsyntax-only | 通过；Linux声明可编译，未声称Linux完整生产构建/数值库联调 |
| tools/generate_contract_schema.py --check | 302定义、77操作变体与唯一声明同步 |
| tools/check_design.py | 六份工程所有权、实际链接、全部JSON schema、四份请求字段/Ref/Index/Array shape、真实块size/hash、4295删除记录通过 |
| 独立小矩阵/块真值 | RCRᵀ及t真值277/64由审核者独立复算；24byte小端输入(2,4,8)→R预期(3,7)，不是API实跑 |
| Git whitespace/变更审查 | diff --check通过；新增示例不是旧测试拷贝 |

科学算法未实现，acsd及acsd-contract尚不存在；examples/*.expected-model.json为独立期望模型，非伪造运行输出。性能数值仅在VERIFICATION规定未来测量方法，未报告未测吞吐或内存达标。产品恢复、跨平台科学数值一致性和真实数据需后续独立执行。

科学构建开关当前明确停止，等待负责人接受设计。没有创建PR、修改main或强制推送。
