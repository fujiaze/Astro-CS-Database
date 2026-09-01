# P3-002: properties/order/unit

任务 ID: P3-002
Gate: G6
依赖: P3-001; RT-007
平台: Linux
变更类别: algorithm

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P3-002：

> 解析实际 HiPS properties：order、tile width、frame、format、dataproduct、
> unit/creator/manifest。`order_sel` 上限来自输入实际 order，且实际 reader/resampler
> 使用该 order；禁止仅写 metadata。BITPIX/max memory 来自 request+NodePlan；
> 不支持输入明确错误。BUNIT 来源于输入合同，unknown 按 SCI 决定拒绝或原样
> UNKNOWN，绝不 Jy/beam 默认。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 解析实际 properties | HipsProperties 增加 bunit 字段(解析 BUNIT 键, 可缺) | c01 #2 |
| order_sel 上限=输入实际 order | p3_sampler_open_ex 暴露 order; session max_order=min(input_order,20) | c01 #1 |
| reader/resampler 用该 order | P3SamplerImpl.order=properties 实测(既有); leaf_nside=512·2^K | c01 #1 |
| BITPIX 来自 request | session bitpix -32|-64 校验(既有) | c01 #3 |
| BUNIT 来源合同 | session bunit.c_str(); 缺省 ADU; **移除硬编码 Jy/beam** | c01 #2 |
| 不支持输入显式错误 | variance/weight/ivar/flux-per-pixel 显式拒(既有) | c01 #4 |

## 实现文件

- `lib/phase3_session/hips_properties.h/.cpp`：HipsProperties.bunit + BUNIT 键解析
- `lib/phase3_session/p3_resample.h/.cpp`：p3_sampler_open_ex(暴露 order+bunit); open 委托
- `lib/phase3_session/p3_session.cpp`：order_sel 上限=输入实际 order; BUNIT 用输入合同(缺省 ADU)
- `tests/backend/p3_output_probe_main.cpp`：probe BUNIT "Jy/beam"→"ADU"(测试辅助与合同一致)
- `tests/backend/test_p3_output.py`：断言 ADU 非 Jy/beam(修复固化旧缺陷)
- `tests/backend/test_p3002_properties_order_unit.py`（新）：4 组审计断言

## 测试结果

- `test_p3002_properties_order_unit.py`: 4/4 PASS
- `test_p3_resample.py`/`test_p3_output.py`/`test_hips_properties.py`: 全 PASS(回归)
- `ctest`: 56/56 PASS

## 说明

- 缺陷修复: session 曾硬编码 BUNIT="Jy/beam"(违反面亮度 ADU 合同)与 order_sel 固定上限
  max_order=20(违反"上限来自输入实际 order"); 均已修复。
- test_p3_output.py 曾断言输出含 "Jy/beam"(固化旧缺陷), 更新为 ADU。
