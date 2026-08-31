# P1-001: Phase1 模块迁移

任务 ID: P1-001
Gate: G4
依赖: RT-005; RT-008
平台: Linux
变更类别: architecture

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P1-001：

> 注册 calibration、cosmetic、star/PSF、WCS/platesolve、photometry、noise/SNR、
> drizzle/writer 模块。每个节点声明标准 DATA 端口和执行类。Session 如暂保留只能
> 是兼容 adapter，内部必须委托 Runtime，不得拥有第二调度顺序。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 8 类模块注册 | calibration(已有)+cosmetic/star-psf/wcs-platesolve/photometry/noise-snr/drizzle/writer 7 个新增 | c01 |
| 标准 DATA 端口 | 每模块 ports 含输入(是 is_input=true)与输出, data_schema_id 均 DATA- 前缀 | c01 |
| 执行类声明 | cpu_heavy(7 个计算类) / io(writer) | c01 |
| Session 兼容 adapter 委托 Runtime | 工厂经 make_session_module<P1Api> 委托 p1_session(唯一 Runtime 执行路径, 无第二调度) | 实现 |
| 合同引用 | 每模块 sci_id/alg_id/data_id/api_id/test_id 前缀合规; alg_id 映射 backend_table kernel(ALG-002/004/005) | c01 |

## 实现文件

- `lib/core/src/module_adapters.cpp`：新增 7 个模块描述函数 + register_phase_modules 注册(工厂委托 P1Api)
- `tests/unit/p1001_modules_test.cpp`（新）：8 类模块注册/端口/执行类/合同/Runtime 委托断言
- `tests/unit/rt005_registry_test.cpp`、`tests/unit/rt008_runtime_client_test.cpp`：size==3 → 10 同步

## 测试结果

- `ctest`: 56/56 PASS
- `p1001_modules_test`: P1-001 TESTS PASS(8 类模块全注册, 端口/执行类/合同合规)
- `rt005_registry_test`: RT-005_PASS; `rt008_runtime_client_test`: RT-008_CLIENT_PASS
- `tests/cli/test_cli_protocol.py`: 10/10 PASS

## 遗留说明

- 各模块的科学执行语义(Oracle)在 P1-002 验证; 本任务交付注册/端口/执行类/委托链。
