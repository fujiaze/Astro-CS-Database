# 任务执行协议

1. 阅读任务文件的依赖、目标、修改面和门限。
2. 记录仓库 commit、工作树、编译器、依赖版本、数据路径和硬件信息。
3. 用原版程序复现问题并保存原始证据。
4. 先添加能失败的自动测试，再做最小实现。
5. 测试层级：contract → unit → component → pipeline → representative real data → full TestData（任务要求时）。
6. 外部命令必须有超时；不得无限等待 Gaia、构建、批处理或 GUI。
7. 生成任务报告并执行隔离复核。
8. PASS 后更新控制文件并选择下一任务。
