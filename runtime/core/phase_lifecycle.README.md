# RT-002 Phase-isolated Runtime 生命周期（设计权威）

> owner: SA-RT-05 · 权威文档形态（本文）+ 执行语义形态
> (`runtime/core/phase_lifecycle.py`) + 测试 (`tests/runtime/test_phase_lifecycle.py`)。
> 三形态必须同步修改；机器一致性由 tests/runtime/test_phase_lifecycle.py 校验。

## 1. 目标（tasks/03_RUNTIME_DATA_IO_TASKS.md RT-002）

> 每条 phase CLI 创建独立 Runtime/Store/RunContext；只加载本 Phase registry；
> 跨 Phase manifest 在进程外读取。
> 验收：进程内全局状态 spy；phase1 DLL 缺失不影响 phase3 外部 fixture；禁止 `--phases` 调用图。

约束来源：`AstroCS_ENGINEERING_CONSTRAINTS.md` A.3/A.4/A.6（三 Phase 是隔离产品命令；
禁止同进程 `--phases 1,2,3`；阶段间只通过原子发布、哈希与 provenance 完整的磁盘
产品/manifest 交换）；`03_TARGET_PRODUCT_AND_ARCHITECTURE.md` §5（每个 Phase 启动独立
Runtime 实例）；DATA-002（Phase 产品 manifest 交换合同，跨 Phase 仅磁盘交换）。

## 2. 生命周期隔离结构

```text
phase1 CLI ── 进程 A ── Runtime_A ── registry_A(仅 phase1 模块)
                         ├─ Store_A      （run 私有 ArtifactStore）
                         └─ RunContext_A （run 私有）
   │ 磁盘发布: phase1_product_v1 交换对象（manifest+content, 原子+hash）
   ▼
phase2 CLI ── 进程 B ── Runtime_B ── registry_B(仅 phase2 模块)
                         ├─ Store_B
                         └─ RunContext_B
   │ 进程外读取: DATA-002 交换对象（不共享进程 A 任何对象）
   ▼
phase3 CLI ── 进程 C ── Runtime_C ── registry_C(仅 phase3 模块)
```

不变量：

1. **Phase registry 隔离**：`phase_registry_view(phase)` 只含该 Phase 模块记录。
   运行时 Registry 只注册本 Phase 工厂；其他 Phase 模块不注册、不可创建。
2. **生命周期 run 私有**：每次 phase run 新建 Registry 视图 / ArtifactStore /
   RunContext；运行结束释放。生产执行路径不存在进程内全局共享注册表/Store/
   RunContext 单例贯穿多个 run（global state spy 验证）。
3. **跨 Phase 仅进程外读取**：下游读取上游产物清单 = 读取已发布磁盘交换对象
   （`PhaseManifestReader` 读取 DATA-002 phase-product-exchange 文档形态）；
   绝不引用上游进程内 Registry/Store/RunContext，不要求 run_id/artifact_id 匹配。
4. **禁止 `--phases` 调用图**：一个运行图只允许一个 Phase 的模块。把多 Phase 模块
   混入同一图 → 拒绝（图内跨 Phase 数据边由 RT-001 compiler 以 PHASE_SCOPE /
   CROSS_PHASE 拒绝；生命周期层以 `assert_no_cross_phase_graph` 拒绝）。

## 3. 验收映射

| 验收 | 语义实现 | 测试 |
|---|---|---|
| 进程内全局状态 spy | `PhaseIsolationGuard` spy 记录每次 run 生命周期；registry 视图按 phase 新建；生产源无全局单例注册表 | `test_global_state_spy.py`（静态扫描 + 生命周期 spy） |
| phase1 DLL 缺失不影响 phase3 外部 fixture | phase3 registry 视图不含任何 phase1/2 模块；phase3 输入可来自 DATA-002 `origin=external_fixture` 交换对象；phase1 模块不可用不影响 phase3 生命周期创建 | `test_phase3_external_fixture_independent.py` |
| 禁止 `--phases` 调用图 | 单一图 module phase 集合必须 size==1；phase1+phase3 混图 → 拒绝 | `test_no_cross_phase_graph.py` |
| 每条 phase CLI 独立 Runtime/Store/RunContext | `new_lifecycle(phase, run_id)` 每次返回新对象；registry 只含本 phase | 生命周期创建测试 |
| 跨 Phase manifest 进程外读取 | `PhaseManifestReader` 只读磁盘交换对象；不共享进程内对象 | manifest 读取测试 |

## 4. 边界（非目标）

- 本任务不改科学公式/常数；纯静态/结构语义。
- `--phases` 命令面删除/拒绝属 CLI-002（SA-CLI-04）；本模块在运行时/生命周期层
  禁止跨 Phase 调用图。
- 真实 scheduler/executor/DLL 接线属 RT-003..007；本任务交付合同 + 语义骨架
  （与 RT-001 typed_dag.py 同为 Python 语义执行形态 + 权威文档 + pytest）。
- 全局 ThreadBudget 属 RT-003；本任务不实现线程预算。

## 5. 一致性自检

`python3 runtime/core/phase_lifecycle.py --registry-isolation-check` →
`PHASE_REGISTRY_ISOLATION PASS`（三个 phase 视图均无泄漏）。
