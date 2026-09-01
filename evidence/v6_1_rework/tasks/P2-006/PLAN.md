# P2-006: Canonical Phase2 Pipeline

任务 ID: P2-006
Gate: G5
依赖: P2-001..005
平台: Linux
变更类别: pipeline

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P2-006：

> IR 至少含 coverage→sample→upm_fit→upm_apply→reject→integrate→write，各端口
> DATA/单位/Artifact ID 完整。ACR 不注册不链接。Observed trace 必须逐节点匹配
> static graph，且输出 mosaic/signal/support/ivar/variance/UPM/rejection 诊断的命名
> 无模糊 weight。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| IR 7 节点链 | build_pipeline_ir phase2 → coverage/sample/upm_fit/upm_apply/reject/integrate/write 7 节点, 端口串接 | c01 #1 |
| 各端口 DATA/单位/Artifact ID 完整 | core module_adapters 7 个 descriptor(端口/UnitId/Artifact ID/sci/alg/data/api/test id) | 实现 |
| ACR 不注册不链接 | 纯 CPU 自适应 backend; ACR(cuda_bridge_stub)不注册 | 实现 |
| Observed trace 逐节点匹配 | 7 static = 7 observed COMPLETED; PIPELINE_GRAPH_PASS | c01 #2/#3 |
| 输出命名无模糊 | mosaic 在 write 节点输出(artifact:write); graph 产物齐全 | c01 #4 |

## 实现文件

- `lib/core/src/module_adapters.cpp`：7 个 phase2 子模块 descriptor(p2_coverage/sample/upm_fit/upm_apply/reject/integrate/write) + 注册(工厂委托 P2Api)
- `cli/runtime_client.cpp`：phase2 IR 从单节点 `res` → 7 节点链(端口名与 descriptor 一致)
- `tests/backend/test_p2006_canonical_pipeline.py`（新）：4 组断言(7 节点 IR/trace 匹配/graph 双向/输出命名)
- `tests/unit/rt005_registry_test.cpp`、`rt008_runtime_client_test.cpp`：断言更新(10→17 模块; res→7 节点链)

## 测试结果

- `test_p2006_canonical_pipeline.py`: 4/4 PASS
- `test_phase123_pipeline.py`: 7/7 PASS; `test_phase2_inprocess.py`: OK
- `ctest`: 56/56 PASS

## 说明

- 7 节点工厂委托 P2Api session adapter(与 phase2.resample 同一调度, 无第二调度顺序);
  每个 IR 节点 execute 委托完整 phase2 session(幂等, 输出一致)。
- 端口单位: coverage/mask 用 DIMENSIONLESS(UnitId 无专用枚举), 其余 ADU。
