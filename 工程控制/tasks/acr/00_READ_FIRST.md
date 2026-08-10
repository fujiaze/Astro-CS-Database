# AstroCS ACR 底层支线开发控制包

更新时间：2026-08-04  
唯一开发分支：`feature/astrocompute-runtime`  
最终归宿：全部底层验收通过后合并到 `main` 备用。  
发布状态：未发布；本包无版本号，后续只覆盖更新这一份权威控制包。

## 0. 工程启动词

```text
继续在现有feature/astrocompute-runtime分支，读取00_READ_FIRST和22_FIX_REVIEW_CORRECTION_PLAN；修复真实设备执行、并发工作池与证据，不改算法。
```

同样内容见 `00_AGENT_START_PROMPT.txt`。分支已存在时必须继续增量修改；禁止新建仓库、日期分支、`-v2` 分支或第二套 ACR。

## 1. 当前审计状态

最新审计对象：`AstroCS_ACR_Fix_Review_2026-08-03.zip`。

最新审计结论见`audits/FIX_REVIEW_AUDIT.md`，当前最高优先级执行入口为`22_FIX_REVIEW_CORRECTION_PLAN.md`。

Commit F 可以保留为以下中间基础：

- CPU 利用率采样器；
- MemoryBudget 接口及 `Fail/ShrinkBlock` 初步动作；
- CPU 尾段缩块实验；
- CostEstimate、CurrentState 和 Dispatcher 的数据结构；
- 现有 CPU-only 单元测试。

但下列能力**尚未完成，提交说明和测试名称不得继续宣称完成**：

1. `CostEstimate` 尚未驱动真实设备执行，只影响块大小和报告标签；
2. `actual_primary_backend` 可能来自推荐值，而不是实际执行统计；
3. 当前后 30% 固定缩块不是动态 guided scheduling；
4. 95% 目前只有采样，没有可执行闭环控制；
5. MemoryBudget 多数动作尚未进入执行链，正式配置也未完整注入；
6. coverage 可能把失败或未执行块错误标记为完成；
7. path guard 报告为 `ABORT`；
8. Evidence 中 HEAD、git log、证据提交不一致；
9. 真实 GPU、CPU+GPU Mixed 和 Sanitizer 仍为 SKIPPED。

完整审计见 `audits/COMMIT_F_AUDIT.md`，执行顺序见 `21_COMMIT_F_CORRECTION_PLAN.md`。

## 2. 冻结后的目标调用链

```text
Public API
   ↓
TaskDescriptor / NumericPolicy / DataResidence
   ↓
HardwareProfile（正式运行只读）
   ↓
CostEstimator：为每个设备和候选块计算预计完成时间
   ↓
Dispatcher Shared Pending Pool
   ├── CPU workers 动态领取
   ├── GPU 0 worker 动态领取
   └── GPU N worker 动态领取
   ↓
Backend真实执行 + Event/coverage/错误回传
   ↓
ExecutionReport从实际统计生成，不从推荐值伪造
```

CostEstimator 的职责是估算；Dispatcher 的职责是执行。任何“推荐 GPU、实际 CPU”情况都必须在报告中如实体现，不能把推荐设备写成 `actual_*`。

## 3. 本分支唯一范围

允许开发：

- ACR API、Buffer、Event、TaskDescriptor、HardwareProfile；
- CPU/GPU backend；
- CostEstimator、共享工作池、驻留和故障回退；
- 资源采样、95%软目标闭环、RAM/VRAM反压；
- 经典实验、Benchmark、CI、文档和 Evidence。

严禁修改或接入：

- Drizzle、批量积分、HISS、校准、测光、PSF、重采样等真实算法；
- PipelineFrame、Orchestrator、Stage 1/2、正常 CLI；
- 现有 OpenMP；
- 任何业务语义。

## 4. 当前优先级

1. 修复 path guard 与 Evidence 单一 HEAD；
2. 修复 coverage 和实际执行报告，禁止推荐值冒充执行值；
3. 接通 `CostEstimator → Dispatcher → Backend`；
4. 实现真正共享未开始工作池和动态 guided 领取；
5. 实现可执行的 CPU/GPU 利用率控制动作；
6. 完整执行 MemoryBudget 动作；
7. 使用受支持工具链完成真实 GPU 与 Mixed；
8. 实际开启 ASan/UBSan；
9. 重新生成同一干净 HEAD 的完整交付证据。

## 5. 阅读顺序

1. `01_FROZEN_REQUIREMENTS.md`
2. `22_FIX_REVIEW_CORRECTION_PLAN.md`
3. `21_COMMIT_F_CORRECTION_PLAN.md`
3. `19_CURRENT_BRANCH_CORRECTION_TASKS.md`
4. `07_COST_MODEL_AND_DYNAMIC_SCHEDULING.md`
5. `08_RESOURCE_CONTROL_SPEC.md`
6. `10_PHASES_TASKS_ACCEPTANCE.md`
7. `12_TEST_VALIDATION_MATRIX.md`
8. `13_DELIVERY_PACKAGE_RULES.md`
9. `16_AGENT_MASTER_INSTRUCTION.md`
10. `CHECKLIST.md`

`09_FUTURE_ALGORITHM_INTEGRATION_GUIDE.md` 仅供未来参考，本支线不得执行。

## 6. 当前合并结论

Commit F 后不得合并 `main`。只有以下全部通过才允许合并：

- 实际执行设备、块数、字节数和耗时可审计；
- CostEstimator 真正影响每次块领取；
- CPU 和至少一张真实 GPU 同时从共享池领取；
- 95%控制有真实采样、执行动作和持续负载报告；
- MemoryBudget配置和动作全部接通；
- coverage状态严格为 pending/claimed/done/failed；
- path guard PASS；
- Evidence 单一干净 HEAD；
- Sanitizer实际开启；
- 现有算法目录零修改；
- 合并后普通 AstroCS 无 ACR 副作用。
