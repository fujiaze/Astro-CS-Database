# 数据驻留、传输与内存预算

## 1. 驻留状态

Buffer至少跟踪：

- Host valid；
- Device valid；
- Both valid；
- Host dirty；
- Device dirty；
- 对应device id和字节数。

只有消费方需要更新副本时才传输。

## 2. 传输优化

- 相同输入不得为每个GPU块重复整帧上传；
- 优先上传一次并以tile/view复用；
- 连续ACR算子允许中间结果留在显存；
- 只有CPU或外部模块需要结果时才D2H；
- 支持pinned staging与双缓冲，以重叠传输和计算；
- 小任务不得为使用GPU而强制复制。

## 3. 积分建议分块

未来接入时优先按输出像素tile分块：

- 每个tile遍历相关输入帧；
- CPU与GPU负责不同输出tile；
- 输出区域独立，避免跨设备原子和大规模合并；
- 若输入帧可批量驻留显存，应复用而不是每tile重复上传。

## 4. Drizzle建议分块

优先研究输出tile所有权：

- 将落入同一输出tile的样本批量交给一个设备；
- 或使用设备私有累计区，再执行明确的最终合并；
- 禁止CPU/GPU无协议地同时写同一累计数组；
- 分块应兼顾投影覆盖密度和显存workspace，而不是只按输入像素数平均切割。

这些是未来接入契约，本轮只通过drizzle-like合成内核验证底层能力。

## 5. 内存预算

分别管理：

- 全局RAM；
- pinned staging；
- 每张GPU VRAM。

有效上限取：

```text
min(total × ratio_limit, total - fixed_reserve)
```

默认`ratio_limit=0.95`，必须允许配置固定保留量。

## 6. Claim前峰值估算

至少包含：

- 输入和输出；
- device/host workspace；
- 私有partial与merge buffer；
- pinned staging；
- 双缓冲；
- 已驻留可复用数据不能重复计费。

## 7. 超预算处理顺序

1. 减小当前设备块；
2. 复用或释放可重建缓存；
3. 等待该设备已提交工作释放内存；
4. 将未开始块交给其他合格设备；
5. 所有路径都不可行时明确失败。

不得等待操作系统OOM。MemoryBudget与CPU/GPU利用率诊断完全独立。

Schema见`schemas/memory_budget_report.schema.json`。
