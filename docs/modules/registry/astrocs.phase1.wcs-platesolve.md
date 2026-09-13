---
id: MOD-astrocs-phase1-wcs-platesolve
version: 1.0.0
status: ACTIVE
owner: astrocs-core
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-WCS-001, ALG-WCS-001, API-P1-004]
downstream: [DATA-P1-WCS, API-WCS-001, TEST-P1-WCS-001]
---

# 模块 astrocs.phase1.wcs-platesolve

> P1-WCS-DOC 事实修订（2026-09-07）：本页 registry descriptor 占位 ID
> （SCI-P1-WCS-001/ALG-002/TEST-P1-WCS-001）保留为编排层词汇，由
> P1-WCS-INT 对齐；冻结合同 = SCI-WCS-001（docs/science/ASTROMETRY.md，
> FROZEN T102 2026-08-23，共享引用不改动）/ ALG-WCS-001（PLATESOLVE.md
> §11 逐符号锚）/ DATA-P1-WCS（DATA_SEMANTICS §18）/ API-WCS-001
> （PUBLIC_API WCS 节）；模块级事实以 lib/plate_solve/README.md（r1，
> CONTRACT_READY）+ lib/plate_solve/module.yaml（astrocs.p1.wcs，迁移
> 目标 astrocs_p1_wcs.dll，entrypoint=MISSING）为准；现状构建
> cpp/ipv/build.ps1:27 / Makefile:6 → ipv_solver.dll，未编入根 CMake
> 主构建。测试设计 TEST-WCS-DESIGN-001（PLATESOLVE.md §11.4）已冻结，
> 由 P1-WCS-TEST 执行落 TEST-P1-WCS-001 + EVIDENCE。现状缺陷登记
> DISP-WCS-001..006（§11.3，登记不改码，整改归 P1-WCS-IMPL/INT；
> CD 退化静默坍缩=DISP-WCS-001，失败-置信度语义冻结：退化必须
> success=0 禁止冒充解）。

## 职责与明确非职责

Registry production 模块(唯一源=module_adapters.cpp:512-526 descriptor)。
职责由 SCI/ALG 合同定义(见链接); 不做 SCI/ALG 之外的扩展（禁重检测、
禁重采样、禁星表缓存管理——API-WCS-001 范围界定）。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `sources` | `DATA-P1-SOURCES`（编排词汇；模块权威输入=star_measurements [N,≥15] 权威块 + star_det fallback，DATA_SEMANTICS §18.1） | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::PIXEL`（统一契约 index-is-center，+0.5 桥接至 IPV 接口契约） |
| `wcs` | `DATA-P1-WCS`（DATA_SEMANTICS §18） | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS`（RADESYS=ICRS/EQUINOX=2000 写回） |

invalid = NaN/coverage=0(按 DATA 合同)；求解失败 → PLATESOLVE_FAILED
不写半成品 WCS 头（orchestrator.cpp:1980/:2003）。

## 公共 header、核心 symbol 与生命周期

由 `API-P1-004` 公共 API 定义(phase session extern "C"); 现状 C ABI
12 导出见 API-WCS-001（生产入口 ipv_solve_from_detections_v1，
ipv_entry.cpp:524）; 生命周期 create→validate→run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase1.wcs-platesolve`; execution_class=`cpu_heavy`;
parallel_ok=True; 配置=phase config JSON（initial_ra/initial_dec/
focal_length/pixel_size 覆盖，orchestrator.cpp:1922-1944）。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是(heavy+serial 资源门禁止); worker 数=ThreadBudget.max_workers(禁 hardware_concurrency);
现状 OpenMP 仅三角形投票/选星（整数归并，bitwise 与线程数无关），
ThreadLease 未接线（DISP-WCS-005）; 确定性=固定顺序输出
（determinism=fixed_reduction_order，ALG-WCS-001 §11.4 F5 冻结断言）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; IpvWcsResult 调用方分配
POD（DLL 仅写不 malloc，无堆所有权转移）；inlier 缓冲调用方预分配
（ipv_get_last_inliers）。

## 错误、日志、指标、取消和 checkpoint

错误码=ACS_ERR_*(API 合同)；现状 ret 0/1 + error_msg[256] +
BLOCK_MISSING/PLATESOLVE_FAILED 编排码（API-WCS-001 返回码节）；
取消=host cancel 回调（现状缺失，DISP-WCS-005）; 无 checkpoint
(Phase3 原子写)。

## 独立 synthetic 验证命令与容差

`TEST-P1-WCS-001` 对应测试(逐任务 TASK_RESULT 证据); 容差=验收冻结。
测试设计=TEST-WCS-DESIGN-001（PLATESOLVE.md §11.4：合成线性场
rms≤0.5″/astropy SIP oracle ≤1e-4 px/CRPIX 不变量/失败语义负例含
CD 退化注入/bitwise 确定性/WcsTan roundtrip <1e-6 deg + 独立前向交叉
≤1e-9 deg（B2-A1）），fixture 生成器注记容差来源。

## 已知限制

见 docs/KNOWN_LIMITATIONS.md、`ALG-WCS-001` 合同边界与 DISP-WCS-001..006
（PLATESOLVE.md §11.3；CD 退化静默坍缩=DISP-WCS-001，AP/BP 半静默=
DISP-WCS-004，取消点缺失=DISP-WCS-005，三套 TAN 并存=DISP-WCS-006）。
