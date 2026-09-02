# AstroCS ACR Commit F 审计报告

审计对象：`AstroCS_ACR_CommitF_Review_2026-08-03(1).zip`  
审计时间：2026-08-03  
依据：当前唯一权威 `AstroCS_ACR_Control_Package`，重点对照 HardwareProfile、CostEstimator、工作保持调度、95%利用率控制、Evidence 单一 HEAD 规则。

## 1. 结论

Commit F 可以保留为“CPU 调度器接入资源采样与尾段分块实验”的中间提交，但不能认定为以下能力已经完成：

- CostEstimator 驱动的设备路由；
- 真实 CPU+GPU 混合执行；
- 95% CPU/GPU 利用率闭环；
- RAM/VRAM 完整反压；
- guided 动态尾部收缩；
- 可合并 main 的完整 Evidence。

当前建议：继续留在 `feature/astrocompute-runtime`，不得合并 `main`。

## 2. 阻断问题

### 2.1 CostEstimate 没有真正控制执行设备

`dispatch_range_cost_aware()` 只从 `CostEstimate` 读取推荐块大小，随后仍直接调用：

```cpp
impl_->runner.run_range(begin, end, chunk_size, fn, user_data);
```

`preferred_device` 仅被用于：

```cpp
result.actual_primary_backend = impl_->pick_backend_from_estimate(estimate);
```

它没有传入 `MixedRunner`，各设备成本也没有进入领取决策。因此：

- CostEstimator 尚未连接实际执行链；
- `actual_primary_backend` 可能只是报告标签；
- 即使报告显示 `cuda:0`，执行仍可能全部发生在 CPU；
- 当前不能宣称“Cost-aware 路由已经完成”。

正确要求：每个设备领取下一块时，都必须依据 `CostEstimate + CurrentState + 数据驻留 + 队列等待`计算预计完成时间，不能只在执行前选一个字符串。

### 2.2 95% 利用率没有形成闭环

当前代码会调用 `CpuController::sample_and_decide()`，但控制决策基本只用于：

```cpp
if (actual_ratio > target + 0.05) {
    guided_chunk /= 2;
}
```

缩小 chunk 通常只增加提交次数，并不会自动降低 oneTBB 的可运行线程数量或 CPU 占用率。代码没有体现：

- 根据实时利用率暂停/恢复提交；
- 对 worker 做错峰让步；
- 调整 arena 并发、任务占空比或提交窗口；
- 验证 CPU 利用率稳定在目标附近；
- GPU 利用率采样与控制。

因此这只能称为“接入 CPU 利用率采样”，不能称为“95% 利用率控制”。

### 2.3 guided 尾部收缩是固定两段切分，不是动态 guided scheduling

当前实现预先在：

```text
guided_tail_threshold = 0.7
```

对应位置把范围切成两段，后 30% 使用一半 chunk。它并不是根据：

- 实际剩余工作量；
- CPU/GPU 当前吞吐；
- 队列状态；
- 空闲设备数量；
- 预计拖尾时间；

动态生成下一批块。

所以更准确的名称是“固定尾段缩块实验”，不能作为 guided 工作保持调度的最终实现。

### 2.4 MemoryBudget 动作只实现了一小部分

当前完整处理的主要是：

- `Fail`：提前返回；
- `ShrinkBlock`：缩小 chunk。

以下动作虽然枚举并报告，但执行链没有对应行为：

- `StopNewSubmit`；
- `ReleaseCache`；
- `LowMemoryPath`；
- `FallbackOtherDevice`。

同时，`MemoryBudgetConfig` 在 `Dispatcher::configure()` 内部使用默认值重新创建，没有看到来自正式配置文件的 RAM/VRAM 限额注入。中途重新采样后的动作也没有同步更新最终 `result.mem_action`。

### 2.5 CurrentState coverage 可能错误报告完成

CPU 路径执行后，无论 `run_result` 是否存在失败，代码都会：

```cpp
for (...) {
    current_state.coverage().mark_done(i);
}
```

这会把全部 chunk 标记完成。正确做法应从 MixedRunner 的真实 coverage / chunk 状态导入，失败或未开始块不得标成 done。

## 3. Evidence 问题

### 3.1 Path guard 明确失败

`evidence/path_guard_report.txt` 内容是：

```text
[path_guard] ABORT. If already committed, run 'git reset HEAD~1'.
```

但证据提交说明写成“path_guard 验证通过”。这是直接矛盾，按控制包属于硬门禁失败。

必须重新运行 path guard，并附完整命令、退出码和允许/禁止路径清单。

### 3.2 HEAD、git_log 与证据提交不一致

包内同时出现：

- `git_head.txt`：`f0f86f...`；
- `git_log.txt`：只记录 `49ea5c1...`；
- `commit_log.txt`：后续还有证据提交 `8b6dd25...`。

至少 `git_log.txt` 没有对应 `git_head.txt`。这不满足“同一干净 HEAD 一次生成”的要求。

建议 Evidence 不再提交到实现分支后再引用旧 HEAD；应使用独立打包脚本，从目标 HEAD 的干净 worktree 一次生成 manifest、diff、测试日志和源码快照。

### 3.3 GPU SKIPPED 可以接受，但不得等同于完成

评审包正确记录了 CUDA 工具链不可用，真实 GPU 测试应标为 `SKIPPED`。但“无可用 GPU 编译工具链”不应表述为与“无 GPU”完全等效。

控制包已经规定：至少一个真实 GPU backend 完成构建、运行、画像和 Mixed 验证，才允许最终合并。因此 Commit G 跳过后，分支只能继续开发，不能进入完成/合并阶段。

### 3.4 Sanitizer 没有实际完成

`acr_test_sanitizer_actual` 为 7 项全部跳过，当前只能证明普通生命周期测试通过，不能证明 ASan/UBSan 已开启。

### 3.5 测试日志过度压缩

大多数日志只有一行“PASSED N tests”，无法审计：

- 实际执行命令；
- 二进制路径；
- 构建选项；
- 测试耗时；
- 完整跳过原因；
- 退出码；
- 测试环境。

正式 Evidence 应保留完整原始日志，摘要文件只能作为索引。

## 4. 测试覆盖评价

新增的 5 个 Commit F 测试主要验证：

- CPU 路径可执行；
- 固定尾段会缩块；
- 禁用开关有效；
- 正常内存条件下返回 `none`；
- CurrentState JSON 非空。

没有验证：

- CostEstimator 是否改变真实设备领取；
- CPU/GPU 同时执行；
- 95%目标是否被真实维持；
- 超目标后的控制动作是否有效；
- 内存超限各动作；
- VRAM控制；
- 设备忙闲时的工作保持；
- 失败块是否正确回收；
- profile hash运行前后不变。

因此 545 passed 只能说明已有单元测试通过，不能替代关键能力的验收。

## 5. 建议的下一步提交

### Commit F-fix 1：修正 Evidence 和状态报告

- 修复 path guard；
- Evidence 使用单一干净 HEAD；
- 保存完整命令、退出码和日志；
- coverage 从真实执行状态导入；
- `actual_primary_backend` 根据实际执行统计生成，不能从推荐值直接填写。

### Commit F-fix 2：把 CostEstimator 接入共享工作池

- Dispatcher 保存未开始块；
- CPU 和每张 GPU 在领取时计算候选完成时间；
- `CostEstimate.per_device` 真正影响设备领取和 chunk 大小；
- 不保存固定份额；
- 已开始块不迁移，失败只回收未开始块。

### Commit F-fix 3：实现真实资源控制

- CPU/GPU利用率由平台采样器读取；
- 控制器输出可执行的 throttle/submit decision；
- 使用提交节奏、并发窗口、队列深度或worker让步控制占用；
- 所有CPU线程仍可参与；
- 95%为软目标并报告时间窗、平均值、p95和容差；
- MemoryBudget各动作有实际处理路径。

### Commit G：真实 GPU

- 使用受支持的 MSVC + CUDA 工具链，或其他经 ADR 批准的后端；
- 构建并运行真实 GPU kernel；
- 完成 device-resident、H2D/D2H、launch和至少一组 CPU+GPU Shared Pool 验证；
- 无真实证据不得宣称 Mixed 完成。

## 6. 当前验收状态

| 项目 | 状态 |
|---|---|
| CPU-only 基础执行 | 通过 |
| CostEstimate 数据结构 | 已有 |
| CostEstimator 驱动真实执行 | 未通过 |
| guided 动态调度 | 未通过 |
| CPU 95%闭环 | 未通过 |
| GPU 95%闭环 | 未实现 |
| RAM/VRAM完整反压 | 部分实现 |
| 真实 CPU+GPU Mixed | SKIPPED/未通过 |
| Sanitizer实际开启 | SKIPPED |
| path guard | 失败 |
| Evidence单一HEAD | 失败 |
| 合并 main | 禁止 |

## 7. 最终判断

Commit F 不需要推倒重来，利用率采样、MemoryBudget接口和尾段缩块实验可以保留。但必须把提交描述从“95%控制和Cost-aware混合调度已接入”降级为“资源采样与CPU尾段分块基础”，随后继续完成真实执行链。

原评审包 SHA-256：

```text
e2b7125bc61aa1b8fba86d5ca40fde7a50ebbc20831042c2b8e4f7b2c8781a50
```
