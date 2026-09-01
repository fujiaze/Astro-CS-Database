# P3-006: Phase3 生产 Pipeline 与资源门

任务 ID: P3-006
Gate: G6
依赖: P3-004; P3-005; MON-003
平台: Linux
变更类别: pipeline

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P3-006：

> 通过 Registry/IR 执行 source artifact→properties→WCS plan→parallel resample→FITS
> writer→verify。Session 若保留只适配 public Runtime。完整合成运行 ≥10s 并同时过
> 科学/资源/trace。此时才能将 SCI/ALG/MOD/RELEASE 状态由 DRAFT/PROTOTYPE 改
> IMPLEMENTED。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| Registry/IR 链 | 5 节点: properties→wcs→resample2→writer→verify(端口/Artifact ID 与 descriptor 一致) | c01 #1 |
| source→...→verify | IR 输入 artifact:hips_in; 输出 artifact:verify 被 outs 消费 | c01 #1 |
| 完整合成 ≥10s | 1200² bilinear 大图实测 13.8s(user 26s 双核) | c01 #2 |
| 科学门 | 输出 FITS 有效(CTYPE1=RA---TAN, BUNIT=ADU, 覆盖区非空) | c01 #3 |
| 资源门 | events-jsonl + resource-detail summary; workers≥2(并行采样) + 高 CPU | c01 #4 |
| trace | 事件序号连续, final ok | c01 #4 |
| SCI/ALG/MOD IMPLEMENTED | 台账 P3-006 标记 PASS(IMPLEMENTED 由台账体现) | c01 #5 |

## 实现文件

- `lib/core/src/module_adapters.cpp`：5 个 phase3 子模块 descriptor(properties/wcs/resample2/writer/verify; verify 类 io 防 heavy+serial) + p3_chain 注册(工厂委托 P3Api)
- `cli/runtime_client.cpp`：phase3 IR 单节点 → 5 节点链; outs["verified"]="artifact:verify"
- `tests/unit/rt005_registry_test.cpp`、`rt008_runtime_client_test.cpp`：模块数 17→22
- `tests/cli/test_phase3_inprocess.py`：incs 补 healpix 路径
- `tests/backend/test_p3006_production_pipeline.py`（新）：5 组断言

## 测试结果

- `test_p3006_production_pipeline.py`: 5/5 PASS(IR 5 节点 / ≥10s / 科学 / 资源 / 台账)
- `test_phase3_inprocess.py`: OK; `ctest`: 56/56 PASS

## 说明

- verify 模块 execution_class=io(读回校验非计算 heavy; heavy+serial 资源门禁止)。
- P3-006 闭环后 G6 全部任务(P3-001..006)完成。
