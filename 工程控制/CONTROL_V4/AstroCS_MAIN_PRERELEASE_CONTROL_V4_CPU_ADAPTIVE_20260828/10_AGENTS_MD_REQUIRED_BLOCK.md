# AGENTS.md 必须加入的精简块

仅加入以下内容，避免占用常驻上下文：

```md
## AstroCS节点与性能
仅在main原子提交并立即push。vm-bj Linux常在线：静态、文档、合成小测和调度；Fatduck在线时：Windows编译、真实数据和重计算，离线不阻塞Linux任务。ACR暂不接入，生产仅纯CPU自适应并行。重计算必须经资源监控；CPU低利用率、异常内存增长或无解释等待视为失败。核心数与ISA由运行时检测和benchmark选择，禁止硬编码。
```

