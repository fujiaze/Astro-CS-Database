# Independent Review Protocol

复核者不得只阅读实现者总结。必须：

1. 查看原始 diff、契约和任务验收；
2. 从干净构建或独立构建目录运行关键测试；
3. 检查是否存在跳过、回退、门限放宽和未记录接口变化；
4. 对真实数据证据抽样复现；
5. 检查资源释放、错误路径和兼容性；
6. 对 P02 专项确认每帧检测一次、同一 star_det hash、PSF float32。

报告最后一行只能是：

`VERDICT: PASS`、`VERDICT: FAIL` 或 `VERDICT: BLOCKED`。
