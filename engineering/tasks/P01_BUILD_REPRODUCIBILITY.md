# P01 可复现构建

## 目标

在干净 clone 中通过统一入口构建所有模块并运行 smoke test。

## 执行顺序

1. ADR 确定根级构建策略；
2. 锁定工具链和依赖；
3. bootstrap 检查/安装；
4. 统一产物目录；
5. 逐模块 clean build；
6. DLL 依赖与导出符号检查；
7. 根级 smoke test；
8. 第二目录重建。

## 关键验收

- 构建不依赖源码目录残留 DLL；
- Makefile/PowerShell 产物名称一致；
- 运行时 DLL 搜索路径可解释；
- Qt 与非 Qt 产物分开；
- manifest 包含编译器、flags、依赖、哈希；
- 失败时返回非 0 并保存日志；
- 所有 Python 启动外部命令设置超时。
