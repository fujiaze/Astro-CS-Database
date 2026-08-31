# G2 Gate Checklist — 唯一 Runtime 与类型化数据链 (V6.1)

> 由 05_GATE_CHECKLISTS.md 复制；每项 PASS/FAIL + evidence path + SHA-256。
> RT-009 完成后本清单全部可填 PASS；此版本记录 RT-004..RT-009 达成状态。

| # | 检查项 | 结果 | Evidence |
|---|---|---|---|
| 1 | Module descriptor 的 SCI/ALG/DATA/API/TEST、端口、资源与并行模型完整 | PASS | RT-005 ModuleRegistry 校验；module.h ModuleDescriptor 含 sci/alg/data/api/test_id、ports、execution_class、parallel_ok。core_module_test + rt005_registry_test 全 PASS。evidence/v6_1_rework/tasks/RT-005/TASK_RESULT.json |
| 2 | ThreadBudget 是原子 reserve/release 租约；嵌套和并发总 worker 不超预算 | PASS | RT-001 冻结合同；context.h ThreadLease RAII。rt002_budget_test TSan PASS。evidence/v6_1_rework/tasks/RT-002/TASK_RESULT.json（G1 记录） |
| 3 | RunContext 的日志、指标、artifact、checkpoint、cancel 并发安全；TSan 定向通过 | PASS | core_context_test + core_checkpoint_test + rt003_context_test TSan PASS；core_logging_test。evidence/v6_1_rework/tasks/RT-003/TASK_RESULT.json（G1 记录） |
| 4 | PipelineIR 使用 schema 校验模块、端口、单位、坐标、producer、DAG 与资源 | PASS | RT-004 PipelineIRParser schema 驱动；UNKNOWN_MODULE/MISSING_PORT/DATA_MISMATCH/UNIT_MISMATCH/COORDINATE_MISMATCH/DUPLICATE_PRODUCER/CYCLE/SERIAL_HEAVY/UNPRODUCED_OUTPUT 各负例。evidence/v6_1_rework/tasks/RT-004/TASK_RESULT.json |
| 5 | Registry 能创建并执行真实模块，不只注册 metadata；重复/不完整 descriptor 拒绝 | PASS | RT-005 register_factory + create；rt005_registry_test。evidence/v6_1_rework/tasks/RT-005/TASK_RESULT.json |
| 6 | Scheduler 支持依赖并发、backpressure、失败传播、取消、checkpoint/recovery | PASS | RT-006 Scheduler run（依赖就绪/取消/失败传播/内存回压）；rt006_scheduler_test + core_scheduler_test；TSan rt006 0 race。evidence/v6_1_rework/tasks/RT-006/TASK_RESULT.json |
| 7 | ArtifactStore 校验 ID/hash/schema/unit/coordinate/provenance；不从路径猜语义 | PASS | RT-007 类型化 ArtifactStore role 绑定 + tamper 检测；rt007_artifact_store_test 全 PASS。evidence/v6_1_rework/tasks/RT-007/TASK_RESULT.json |
| 8 | CLI 只经 Runtime；无 session、CFITSIO、AIO 内部或科学内核直接调用 | PASS | RT-008 CLI 拆分（main.cpp 70 行入口壳）；CHK-001 REACH_PASS acr=0（cli/*.cpp 全扫）。evidence/v6_1_rework/tasks/RT-008/TASK_RESULT.json；CHK-001 PROD_REACHABILITY.json |
| 9 | Phase1 输出被 Phase2 按 Artifact 消费；Phase2 HiPS 被 Phase3 按 Artifact 消费 | PASS | RT-007 role 绑定 P1→P2→P3（T-P1-P2-BIND/T-P2-P3-BIND PASS）；RT-008 run --phases 1,2,3 真实链；observed trace 逐节点匹配。evidence/v6_1_rework/tasks/RT-007、RT-008 |
| 10 | 当前 commit 的静态图与现场 observed 图一致且非空 | PASS | RT-009 static_graph.json ↔ observed_trace.json：check_pipeline_graph.py PIPELINE_GRAPH_PASS（双向 node/edge/artifact/unit/resource 比较）；L0 简图显示 cal→res artifact:cal 传递。evidence/v6_1_rework/tasks/RT-009/logs |
| 11 | 每个正式 preset 有 static JSON/DOT/SVG；每次 run 有 observed JSON/DOT/SVG + sidecar（IR hash/source commit/profile ID/input manifest hash）；路径脱敏 | PASS | RT-009 `astrocs graph --preset 1,2,3` 生成 static JSON/DOT/SVG/L0；`run` 自动生成 observed_trace + observed DOT/SVG + graph_sidecar（ir_sha256/source_commit/profile_id/input_manifest_sha256）；路径 <root>/ 脱敏（无 /home/、绝对前缀）。evidence/v6_1_rework/tasks/RT-009/TASK_RESULT.json |

## 结论

G2 全部 11 项 PASS。唯一 Runtime 调度链（RT-004..008）、类型化 ArtifactStore 跨阶段绑定、
CLI 经 Runtime 唯一执行、静态/observed 运行图与 sidecar（脱敏）均已达成并带负例与 TSan 验证。

创建时间：2026-08-31T12:30:00Z
