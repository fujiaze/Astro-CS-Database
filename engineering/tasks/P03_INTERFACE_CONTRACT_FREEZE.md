# P03 接口契约冻结

## 目标

让 C ABI、Python 绑定、CLI 和配置都有可测试契约。

## 必做

- 导出符号清单；
- `sizeof/offsetof` 布局快照；
- 分配/free 配对；
- 错误码注册；
- 线程安全和线程数控制；
- DLL/version 查询；
- JSON schema 与字段生效测试；
- CLI stdout JSON 与退出码；
- 兼容和弃用规则。
