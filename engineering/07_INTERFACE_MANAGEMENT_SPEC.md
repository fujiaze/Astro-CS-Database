# 07 接口管理 Spec

## 1. 接口范围

- DLL C ABI；
- 头文件结构体；
- PipelineFrame 块；
- HISS/HCSD 文件格式；
- JSON 配置；
- CLI 命令、退出码和 stdout JSON；
- Python ctypes 绑定；
- Qt 浏览器读取接口。

## 2. 每个 C ABI 必须定义

1. DLL/API 版本；
2. 函数签名；
3. 参数单位；
4. 数组形状与布局；
5. 坐标基准；
6. 指针可否为空；
7. 内存由谁分配、由谁释放；
8. 是否线程安全；
9. 全局线程数如何设置；
10. 返回码表；
11. 错误文本获取方式；
12. ABI 兼容规则。

## 3. 内存所有权规范

- 分配 DLL 必须提供对应 free API；
- 调用方不得用 `std::free` 释放未知 DLL 返回内存；
- 跨 DLL 传递 POD 结构体时必须固定 packing、大小和对齐；
- 不跨 ABI 传递 `std::string`、`std::vector`、异常；
- `add_block_move` 只接收与数据层释放策略兼容的内存；
- Python ctypes 必须显式声明 `argtypes/restype`。

## 4. 错误码规范

统一建议：

- `0`：成功；
- `>0`：成功但有受控退化/警告；
- `<0`：失败；
- 每个模块分配错误码区间；
- CLI 非 0 退出码必须映射到稳定类别；
- 详细错误信息写日志和结构化结果，不只写 stderr。

不得混用“1 表示成功”和“0 表示成功”而无适配层。

## 5. 配置接口

所有 JSON 配置必须有 schema，做到：

- 未知字段：警告或拒绝，策略固定；
- 缺必需字段：立即失败；
- 枚举白名单；
- 数值范围检查；
- 路径存在性检查；
- 最终解析结果导出；
- 默认值可见；
- 配置版本可迁移。

重点验证当前 `stage1_config.json` 与 `stage2_config.json` 的每个字段是否真正生效，不能只存在模板里。

## 6. CLI 契约

`orchestrator stage1` 和 `stage2` 必须：

- stdout 输出稳定 JSON；
- 日志默认写 stderr/日志文件，避免污染 JSON；
- 退出码稳定；
- `--help` 与实际参数一致；
- `--threads` 必须真正下发或明确拒绝；
- 支持 `--dry-run` 输出解析后的计划；
- 支持 `--version` 输出各 DLL 版本和格式版本。

## 7. ABI 契约测试

- 头文件结构体 `sizeof/offsetof` 快照；
- 导出符号清单；
- DLL 加载/卸载循环；
- 每个 free API 的内存检测；
- 空指针与边界参数；
- Python 绑定调用；
- 多线程并发能力；
- 不同编译优化级别；
- 旧调用方加载新 DLL 的兼容性。

## 8. 变更流程

接口变更必须：

1. 更新 `INTERFACE_REGISTER.csv`；
2. 写 ADR 或 Interface Change Record；
3. 先增加兼容测试；
4. 提供 deprecation 周期；
5. 更新调用方；
6. 更新绑定和文档；
7. 记录 ABI/数据格式版本变化。
