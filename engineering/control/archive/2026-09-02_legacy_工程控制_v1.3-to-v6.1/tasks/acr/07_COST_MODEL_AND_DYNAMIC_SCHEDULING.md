# CostEstimator 与动态异构调度

## 1. 基本原则

HardwareProfile 保存分设备、分精度、分任务能力族的性能曲线，不保存固定 CPU/GPU 份额，不保存业务 kernel 的 `preferred_backend`。

CostEstimator只产生预测：

```text
Estimate(device, chunk) = queue + launch + transfer + compute + merge + penalty
```

预测不是执行事实。`preferred_device`、`predicted_primary_device` 等字段不得写入 `actual_*` 字段。

## 2. 必须接通的真实链路

```text
API
→ TaskDescriptor
→ ProfileStore
→ CostEstimator::estimate_all_candidates()
→ Dispatcher::claim_next(device_id)
→ Backend::submit()
→ Event/Completion
→ Coverage/ExecutionStatistics
```

每个设备每次领取下一块时重新读取：

- 剩余 pending 块；
- 当前队列等待；
- 数据驻留；
- RAM/VRAM容量；
- 设备健康；
- 固定 HardwareProfile；
- 当前利用率控制器给出的提交许可。

运行时不修改 HardwareProfile。

## 3. Shared Pending Pool

每个工作块必须有唯一 ID，并具备状态机：

```text
PENDING → CLAIMED(device, attempt) → DONE
                         └────────→ FAILED
FAILED 可按策略重新进入 PENDING；已成功 DONE 的块不可再次执行。
```

要求：

- claim 使用原子状态转换；
- 已开始块不跨设备迁移；
- 设备失败只回收未开始或明确失败块；
- coverage由实际完成事件驱动；
- 禁止执行结束后无条件批量 `mark_done`；
- 最终验证每块恰好一次成功完成。

## 4. 候选块与设备选择

设备领取时，对有限候选块计算预计完成时间：

```text
T_finish(device, chunk) =
    queue_wait
  + submit_or_launch
  + required_transfer
  + profile_compute(task_traits, size, precision, residence)
  + merge_or_sync
  + confidence_penalty
```

选择预计最早完成且资源控制器允许提交的候选。禁止先选一个全局设备字符串，再让所有工作继续走 CPU runner。

## 5. 动态 guided scheduling

禁止固定 `70%正常块 + 30%半块` 作为最终 guided 实现。下一块大小必须依据：

- remaining_items；
- active_device_count；
- 各设备画像吞吐；
- 当前队列长度；
- 最近尚未完成块的预计拖尾；
- launch/submit固定开销；
- 内存预算。

建议形式：

```text
chunk_work_time_target = clamp(k × submit_overhead, min_ms, max_ms)
chunk_items = throughput_estimate × chunk_work_time_target
chunk_items = shrink_when_remaining_small(chunk_items)
```

运行时实际耗时只用于本次任务的队列状态和拖尾判断，不允许写回长期画像。

## 6. 实际执行报告

正式结果必须从 Backend Completion 和 coverage统计生成，至少包括：

- predicted_first_choice；
- actual_devices_used；
- 每设备 claimed/done/failed块数；
- 每设备处理元素和字节数；
- H2D/D2H/P2P字节；
- 实际开始/结束时间；
- fallback原因；
- coverage总计；
- profile hash before/after。

`actual_primary_backend` 只能由真实完成工作量最大者生成；若只执行CPU，就必须报告CPU，即使模型曾推荐GPU。

schema见 `schemas/runtime_execution_report.schema.json`。

## 7. 数据驻留

- device-resident数据优先留在设备；
- CPU参与前必须计入迁移成本；
- 写入后使其他副本失效；
- 多GPU优先P2P，不可用时比较host staging；
- 不为表面利用率执行负收益迁移。

## 8. Reduction

各设备生成局部结果，按 NumericPolicy 合并。FP32允许正常末位差异；FP64 accumulator、deterministic merge和fast-math限制由任务声明。

## 9. 验收

必须证明：

- CostEstimate改变了真实领取设备或块大小；
- 推荐设备与实际设备分别报告；
- GPU预忙时CPU继续领取；
- CPU预忙时GPU继续领取；
- 无GPU时Mixed为SKIPPED；
- profile前后hash不变；
- 故障块不被错误标DONE；
- 固定70%尾段实验只能作为单元实验，不作为最终调度结论。
