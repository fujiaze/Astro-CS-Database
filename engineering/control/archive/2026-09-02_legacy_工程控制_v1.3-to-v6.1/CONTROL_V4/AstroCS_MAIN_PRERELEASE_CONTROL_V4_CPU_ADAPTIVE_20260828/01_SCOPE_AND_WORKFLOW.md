# 范围与连续工作流

## 取消

- 历史 A/B/C build、2R/32R 和差分；
- 旧图像作为数值 Oracle；
- ACR GPU/Mixed 接入；
- CP0–CP8 逐次等待人工批准；
- Linux 上32R和大规模重复基准；
- 用“环境性失败”“影响不大”把 FAIL 改 PASS。

## 保留

- 当前 main 的有效提交；
- 合成 Oracle、build/test、文档和 checker 框架；
- Fatduck 的真实数据和 Windows 远程执行能力；
- 原子 commit/push；
- 最终一次外部发布审核。

## 连续流程

1. **当前状态收敛**：冻结最新 main；重审 V3 未闭合项。
2. **科学与算法**：定义先冻结，再写 Oracle 和实现要求。
3. **CPU 自适应基础设施**：拓扑、ISA、安全注册、benchmark、cache、dispatcher。
4. **资源监控**：所有重计算统一包装并按 stage 判定利用率。
5. **生产纯 CPU 并行**：AIO 管线、Sampler、UPM、Integration、Drizzle、Stage1重核。
6. **Linux 验证**：静态、合成、小型真实数据；严格限制资源和运行时间。
7. **Fatduck 验证**：Windows clean build、autotune、全部合成、少量真实、当前候选32R一次。
8. **最终发布**：HiPS、接缝、资源报告、traceability、审核包。

中间任务通过机器门禁后自动继续。Windows 离线时将 Windows 任务置 `BLOCKED`，继续所有无依赖的 Linux 工作；每30分钟最多探测一次，不忙轮询。

## 异步外部审核

- 每个 commit 都生成本地审查胶囊，不要求立即停工上传。
- SCI/ALG/ARCH/API 完成后生成 `REV-001` 完整文档审查包；外部审核人负责公式推导、科学性和主文献核实。
- 等待 REV-001 时继续 CPU/MON/构建等独立任务；科学依赖实现可起草，但不能进入发布闭合。
- 并行/ISA/监控核心代码生成 `REV-002`，合成测试生成 `REV-003`，Windows/32R生成 `REV-004`。
- 只在科学定义必须由用户选择时请求一次决定；普通审查意见形成后续原子修复任务。
