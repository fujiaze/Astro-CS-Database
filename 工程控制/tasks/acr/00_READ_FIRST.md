# AstroCS ACR 底层支线开发控制包

更新时间：2026-08-03  
唯一开发分支：`feature/astrocompute-runtime`  
最终归宿：全部底层验收通过后合并到 `main` 备用。  
发布状态：未发布；本包无版本号，后续只覆盖更新这一份权威控制包。

## 0. 工程启动词

```text
继续在现有AstroCS的feature/astrocompute-runtime分支修改，读取本包00_READ_FIRST；只纠正并完善ACR底层，不改算法，不新建仓库或版本分支。
```

同样内容见 `00_AGENT_START_PROMPT.txt`。分支已存在时必须继续在原分支增量修改；只有分支确实不存在时，才允许从最新 `main` 创建同名分支。禁止建立 `-v2`、日期分支、新仓库或第二套 ACR。

## 1. 当前审计结论

上传的 Phase I AuditPack 证明以下内容可保留：

- oneTBB CPU runtime；
- hwloc 与 cpu_features 基础；
- CPU ISA 实现框架；
- Buffer/Event、单测和经典实验基础；
- CUDA backend 已有代码骨架；
- 算法目录隔离原则。

但当前实现尚未达到完成门槛，主要差距是：

1. 仍存在以业务 `kernel` 为键的 `routes.json` / `preferred_backend` 思路；
2. 公共 API 仍可能忽略 `OperationId/TaskTraits` 并直接走 CPU；
3. Benchmark 目前主要是 Copy/AXPY/Triad，不能形成完整硬件画像；
4. 缺少正式 `DeviceProfile`、`CostEstimator` 和画像留出验证；
5. Mixed 测试若 `enable_gpu=false`，只能算 CPU coverage 测试，不能算 CPU+GPU；
6. 95% 控制若只输入人工利用率数字，不算真实资源控制；
7. Sanitizer 若构建未真正开启 ASan/UBSan，不得宣称已验证；
8. Evidence 必须从同一干净 HEAD 一次生成。

详细任务见 `19_CURRENT_BRANCH_CORRECTION_TASKS.md` 和 `20_PHASE_I_AUDIT_ACTION_PLAN.md`。

## 2. 冻结后的正确架构

```text
离线经典 Benchmark
        ↓
生成多维 HardwareProfile：CPU ISA / GPU / RAM / VRAM / PCIe / 算术 / 归约 / 卷积 / 不规则访问 / 固定开销
        ↓
任务只提交 TaskDescriptor：类别、规模、精度、访存、驻留、可拆性
        ↓
CostEstimator 估算每个设备与候选块的预计完成时间
        ↓
Work-conserving Dispatcher 让 CPU 与可用 GPU 动态领取未开始工作块
        ↓
Utilization Controller 将各设备利用率控制在用户目标附近，默认约 95%
```

关键点：

- 不保存固定 `CPU 18% / GPU 82%`；
- 不让用户配置任务份额；
- 不为每个 AstroCS 业务算法做比例穷举；
- Benchmark 建立能力曲线，运行时根据任务和队列推算；
- 运行时动态派发不等于在线学习，HardwareProfile 正式运行期间只读。

## 3. 本分支唯一范围

允许开发：

- ACR 公共 API、Buffer、Event、TaskDescriptor；
- CPU、GPU 和成熟数学库 backend；
- HardwareProfile Benchmark 与 schema；
- CostEstimator、Dispatcher、数据驻留和故障回退；
- 95%资源利用率控制；
- 独立工具、经典实验、CI、文档与 Evidence。

严禁修改或接入：

- Drizzle、批量积分/叠加、HISS、校准、测光、PSF、重采样等真实算法；
- PipelineFrame、Orchestrator、Stage 1/2、正常 CLI；
- 现有 OpenMP；
- 任何业务结果语义。

底层合并 `main` 后保持 dormant。用户完成其他算法逻辑验证后，才另开算法集成分支。

## 4. 阅读顺序

1. `01_FROZEN_REQUIREMENTS.md`
2. `20_PHASE_I_AUDIT_ACTION_PLAN.md`
3. `19_CURRENT_BRANCH_CORRECTION_TASKS.md`
4. `02_SYSTEM_ARCHITECTURE.md`
5. `03_PUBLIC_API_SPEC.md`
6. `05_OPEN_SOURCE_REUSE_PLAN.md`
7. `06_HARDWARE_PROFILE_BENCHMARK_SPEC.md`
8. `07_COST_MODEL_AND_DYNAMIC_SCHEDULING.md`
9. `08_RESOURCE_CONTROL_SPEC.md`
10. `10_PHASES_TASKS_ACCEPTANCE.md`
11. `17_CLASSIC_EXPERIMENT_SUITE.md`
12. `12_TEST_VALIDATION_MATRIX.md`
13. `18_MAIN_MERGE_AND_DORMANT_INTEGRATION.md`
14. `13_DELIVERY_PACKAGE_RULES.md`
15. `16_AGENT_MASTER_INSTRUCTION.md`

`09_FUTURE_ALGORITHM_INTEGRATION_GUIDE.md` 只供未来接入参考，本支线不得执行。

## 5. 完成定义

只有全部满足才允许合并 `main`：

- `RouteProfile/preferred_backend/routes.json` 已删除或完成不可逆迁移；
- 公共 API 真正进入 `TaskDescriptor → CostEstimator → Dispatcher → Backend`；
- 无画像时纯 CPU 多线程并非阻断警告；
- CPU baseline 和 ISA 变体有真实计时证据；
- 至少一个真实 GPU backend 完成构建和运行验证；
- HardwareProfile 覆盖算术、内存、传输、归约、卷积、不规则访问、原子、分支和固定开销；
- 模型使用留出点验证，不以一个总分路由；
- CPU/GPU Mixed 是真实同时执行，无 GPU 时明确 SKIPPED；
- 95% 是真实利用率闭环或可审计平台估算，不是人工喂值；
- ASan/UBSan 等声明的工具实际开启；
- 现有算法源码零修改；
- Evidence、源码快照、日志、摘要和 manifest 来自同一干净 HEAD；
- 合并后普通 AstroCS 不初始化 ACR、不探测 GPU、不发警告。
