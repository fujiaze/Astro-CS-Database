# P11-006 任务报告

| 字段 | 值 |
|------|-----|
| 任务 ID | P11-006 |
| 日期 | 2026-07-28 |
| 任务 | 更新坐标契约、CLI capabilities、provenance |
| 状态 | DONE |

## 1. 完成内容

### 1.1 移除像素偏移检查
- 文件：lib/plate_solve/python/siril_compare/run_ipv_baseline.py
- 修改：validate_wcs 移除 offset_px < 250 检查
- 原因：望远镜指向偏差（抖动）是正常现象，不应作为 WCS 验证条件
- 验证：710帧重跑 709/710 pass

### 1.2 修复 ipv_wcs.cpp CRPIX 冲突
- 文件：lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp (L165-167)
- 修改：crpix[0] = cx + 1.0 → crpix[0] = cx + 0.5（统一为 width/2.0 + 0.5）
- 原因：与 L287 和 P11-001 冻结值一致
- 验证：编译成功，710帧回归 709/710 无回归

### 1.3 更新 CLI capabilities
- 文件：lib/orchestrator/cpp/src/cli_command.cpp (L1696, L1715)
- 修改：
  - ipv_solver capabilities 新增 export_authoritative_pairs, wcs_sip_serialization
  - schema_versions 新增 wcs_authoritative_pairs:"1.0", wcs_closure_report:"1.0", coordinate_convention:"2"
- 验证：编译成功，orchestrator.exe capabilities 输出正确

### 1.4 坐标契约 v2
- 文件：engineering_v1.3/evidence/P11-006/COORDINATE_CONVENTION_V2.md
- 内容：CRPIX统一、WCS+SIP管线内存块传递、A/B/C三层验证架构、B层硬Gate阈值、SIP序列化要求

### 1.5 provenance schema 扩展
- 文件：engineering_v1.3/contracts/wcs_authoritative_pairs.schema.json
- 修改：新增可选 provenance 对象（solver_version, gaia_catalog_version, wcs_closure_summary 等）

## 2. 710帧回归验证

| 指标 | 值 |
|------|-----|
| 总帧数 | 710 |
| success=true | 709 |
| status=pass | 709 |
| 失败帧 | 1 (Galaxy_Center_mosaic1 OIII, solve_failed) |
| RMS中位 | 0.285" |
| 耗时 | 15.9min |

与 P11-005 一致，无回归。

## 3. 证据索引

- 坐标契约v2：engineering_v1.3/evidence/P11-006/COORDINATE_CONVENTION_V2.md
- 回归结果：lib/plate_solve/logs/siril_compare/ipv_p11_006_crpix_fix/
- 编译产物：build/artifacts/ipv_solver.dll, build/artifacts/orchestrator.exe
- schema：engineering_v1.3/contracts/wcs_authoritative_pairs.schema.json
