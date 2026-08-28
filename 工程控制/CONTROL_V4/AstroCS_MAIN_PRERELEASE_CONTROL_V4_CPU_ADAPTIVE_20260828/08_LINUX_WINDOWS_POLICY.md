# Linux 控制节点与 Fatduck Windows 节点

## Linux（常在线、低资源）

负责：

- 文档、静态分析、合同/checker；
- 当前 main 的短 clean build；
- 合成小数据 Oracle；
- CPU feature/benchmark 框架开发；
- 资源监控开发；
- 短并行门禁、sanitizer分片；
- Windows任务调度、结果汇总和 Git main。

限制：

- 不跑历史版本；
- 不跑32R；
- 单项科学测试优先控制在60秒内；
- 2C内存紧张时降低合成规模，不降低科学覆盖；
- 长任务只有 Fatduck 长期离线且 Linux 无其他工作时才允许，仍必须监控和timeout。

## Fatduck Windows（间歇在线、高算力）

负责：

- 正式 Windows 全模块 clean build；
- 当前 CPU 的 ISA/worker benchmark；
- 全部合成 medium/large 测试；
- 少量真实数据 Stage1/Stage2；
- 当前候选32R一次；
- 接缝、HiPS、资源利用报告。

## 在线策略

- SSH探测timeout 30秒；
- 离线后继续 Linux 无依赖任务；
- 每30分钟最多重新探测一次；
- 在线后 `git pull --ff-only` 到已push的main；
- Windows发现缺陷：回到Linux主仓修改、commit、push，再由Windows pull；不得保留远端私有修补。

