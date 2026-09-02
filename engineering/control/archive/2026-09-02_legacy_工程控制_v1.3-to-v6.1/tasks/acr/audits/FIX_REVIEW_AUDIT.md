# AstroCS ACR Fix Review 审计报告

审计对象：`AstroCS_ACR_Fix_Review_2026-08-03.zip`  
审计日期：2026-08-04  
审计依据：当前无版本号 ACR 控制包、源码快照、Fix Evidence 与 Merge Report。

## 结论

本次修复改善了预测/实际字段分离、coverage统计和动态块接口，但**仍未接通真实异构执行链，也未形成可接受的闭环资源控制和单一HEAD证据**。当前分支不得合并到 `main`。

可保留的中间成果：

- `predicted_primary_backend` 与 `actual_primary_backend` 已分字段；
- CPU动态分块和基本coverage测试有所增加；
- CPU利用率、内存预算采样接口可保留；
- GPU、Sanitizer未验证时没有宣称可合并。

## 阻断问题

### 1. CostEstimate仍未驱动真实设备执行

`Dispatcher::dispatch_range_cost_aware()` 在检测到GPU可用后，最终仍调用 `execute_via_pool_dynamic()`。该函数内部所有工作块都通过：

```cpp
pool.claim_next_dynamic("cpu", n_active_devices)
```

领取，并通过CPU `parallel_batch`执行。`n_active_devices`只是影响块大小，并没有创建GPU worker或调用CUDA backend。

因此：

- `CostEstimate.per_device`没有参与每个设备的实际claim；
- `preferred_device`最多影响统一的`max_chunk`；
- GPU即使可用，也不会真正领取工作块；
- 当前仍是CPU执行器，不是CPU+GPU动态共享池。

### 2. MixedRunner的GPU路径仍是占位回退

`MixedRunner`在`enable_gpu=true`时仍然全部走CPU，并把任务记为fallback。CUDA backend没有进入Dispatcher的正式执行链。当前代码即使换到兼容CUDA工具链，也不能自动完成真实Mixed。

### 3. 动态SharedWorkPool存在并发正确性风险

动态模式在取得`block_id`后才进入互斥区`push_back`。不同线程可能按相反顺序插入，导致`block.id`与`blocks_[id]`索引不一致。随后：

```cpp
mark_done(block->id)
```

可能标记错误块、标记不存在的索引，或留下永久`Claimed`块。

此外，`blocks_`在一个线程`push_back`时，其他线程会无锁读取`size()`、按索引访问或遍历，容器本身存在数据竞争。预留容量只能避免重分配，不能让并发修改`std::vector`合法。

动态工作池必须改成稳定槽位或值令牌设计，禁止返回指向并发增长vector的裸指针。

### 4. 资源控制仍不是闭环

当前实现存在以下问题：

- `cached_batch_size`被计算但没有用于任务提交；
- `ShrinkBlock`只增加计数，不改变后续动态claim参数；
- `ReleaseCache`、`LowMemoryPath`、`FallbackOtherDevice`仍是注释或记录动作；
- `StopNewSubmit`一旦触发就永久停止，本次调用没有等待、恢复和迟滞机制，可能直接留下未完成工作；
- 只在CPU执行链中采样，GPU controller没有接入实际GPU队列；
- 单元测试只检查“采样非空”，没有验证50/80/95/100%持续负载下实际利用率收敛。

因此不能称为“95%资源控制闭环”。

### 5. 实际执行报告仍不支持多GPU真实性

`actual_backend_from_stats()`只有CPU/GPU计数，并把GPU硬编码为`cuda:0`。它无法区分GPU 0、GPU 1或其他后端，也无法从真实设备完成事件生成设备列表。

正式报告必须按`device_id`统计真实完成工作量、失败、重试和执行时间。

### 6. Fix Evidence不是单一干净HEAD

`evidence_manifest.json`声明HEAD一致且工作树干净，但证据中的`git_status.txt`显示：

```text
?? 工程控制/evidence/acr/commit_f_fix/
```

Merge Report又同时给出：

- Result HEAD：`024c87c...`
- Evidence HEAD：`bc665a4`

根`package_manifest.json`仍声称`head_consistent: true`。这些信息互相矛盾。

### 7. 新旧证据混装

根`EVIDENCE_INDEX.md`仍以旧HEAD `84e60e9...`为主，记录573项测试；Fix目录则以`024c87c...`为主，只记录279项通过和3个超时。不能把两套结果合并成一个“统一证据包”。

### 8. 三个TIMEOUT不能视为通过

Fix测试中以下可执行文件超时：

- `acr_test_classic`
- `acr_test_persistence`
- `acr_test_qualification`

超时不是PASS。应为每个外部构建/测试命令设置明确且合理的超时，超时后定位原因并重新运行，最终Evidence不得包含未解释的超时结果。

### 9. SHA-256清单不可跨平台复核

根`SHA256SUMS.txt`中的中文路径在当前环境被写成`????`，导致数百文件无法验证。交付包应使用UTF-8安全的Python清单生成与验证工具，不能只依赖平台编码敏感的`sha256sum`文本。

### 10. GPU与Sanitizer仍是合并阻断项

当前构建仍为：

- `ACR_BUILD_CUDA=OFF`
- `ACR_ENABLE_SANITIZER=OFF`

真实GPU执行、Mixed、GPU利用率闭环和ASan/UBSan均未完成。明确标为阻断项是正确的。

## 当前判定

| 能力 | 判定 |
|---|---|
| CPU基础运行时 | 可保留 |
| 预测/实际字段分离 | 部分完成 |
| CPU动态分块 | 有实现但需并发重构 |
| HardwareProfile/CostEstimator | 数据结构可保留 |
| CostEstimator驱动真实设备 | 未完成 |
| 真实CPU+GPU共享工作池 | 未完成 |
| 95%资源闭环 | 未完成 |
| RAM/VRAM动作执行 | 部分/占位 |
| 真实GPU验证 | 未完成 |
| Sanitizer | 未完成 |
| Evidence单一HEAD | 失败 |
| 合并main | 禁止 |

## 后续执行顺序

1. 先重构动态SharedWorkPool，消除ID/索引错位和容器数据竞争；
2. 建立统一`DeviceExecutor`接口，让CPU和每个GPU各自成为真实worker；
3. 让每个worker按自身CostEstimate、驻留、队列和容量动态claim；
4. 接通CUDA backend和真实完成事件；
5. 重做资源控制为可恢复的提交门、队列深度和批次控制；
6. 完成真实持续负载、GPU/Mixed和Sanitizer测试；
7. 从同一干净HEAD重新生成唯一Evidence，删除所有旧证据混装。
