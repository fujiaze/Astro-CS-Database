# 14 版本与发布

## 1. 版本对象

分别版本化：CLI 协议、PipelineFrame block schema、模块 ABI、HISS、HCSD、有效配置 schema 和发布包。

## 2. 兼容策略

- 增加字段：次版本；读取器忽略未知字段。
- 改变布局/单位/语义：主版本；必须提供迁移或明确拒绝。
- 旧 API 可保留一个迁移周期，但生产路径只使用目标版本。

## 3. 发布包

包含 CLI、所有必需 DLL、第三方运行库、默认配置、response curves、协议 schema、license、版本清单、SHA-256、smoke 数据定位说明和验证命令。不得要求普通用户安装 Python/PowerShell。
