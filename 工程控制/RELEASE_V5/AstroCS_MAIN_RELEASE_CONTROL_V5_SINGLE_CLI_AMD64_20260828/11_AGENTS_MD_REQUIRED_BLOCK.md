# 必须合并到根 AGENTS.md 的短块

```md
## AstroCS 开发与节点
中文分析、开发、提交和汇报。只在 main 原子提交并立即 push，禁止分支及破坏性 Git。仅支持 amd64。vm-bj Linux 负责静态、文档、合成小测和调度；Fatduck 在线时负责 Windows 编译、benchmark、真实数据和重计算，离线不阻塞 Linux 任务。发布每个平台仅一个 astrocs CLI，Phase1/2/3 由 CLI 调用；未来 Windows GUI 只控制 CLI。ACR 暂不接入，生产仅纯 CPU 自适应 backend。重计算自动监控；低利用率或异常内存增长为失败。ISA、workers、block 由逐内核 benchmark 选择，禁止硬编码。未经最终外部审核不得宣称发布。
```

若现有 AGENTS 已有等价条目，合并去重；不得增加 Windows PowerShell/Git Bash 作为 Linux 默认环境的旧规则。

