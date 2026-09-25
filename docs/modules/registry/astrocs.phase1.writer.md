---
id: MOD-astrocs-phase1-writer
version: 1.0.0
status: ACTIVE
owner: astrocs-core
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-P1-WR-001, ALG-P1-WR-001, API-P1-008]
downstream: [TEST-P1-WR-001]
---

> 人工内容（MANUAL）：本页无 GENERATED-ANCHOR，非生成器所有；重跑 eng/tools/quality/gen_module_readmes.py 不会覆盖本页。

# 模块 astrocs.phase1.writer

> 上游：ASTROCS_DESIGN.md §8.4（模块与 ABI）

## 职责与明确非职责

Registry production 模块(唯一源=module_adapters.cpp descriptor)。职责由
SCI/ALG 合同定义(见链接); 不做 SCI/ALG 之外的扩展。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `stacked` | `DATA-P1-STACK` | 必 | `UnitId::ADU` | `CoordinateFrame::ICRS` |
| `fits` | `DATA-P1-FITS` | 可 | `UnitId::ADU` | `CoordinateFrame::ICRS` |

invalid = NaN/coverage=0(按 DATA 合同)。

## 公共 header、核心 symbol 与生命周期

由 `API-P1-008` 公共 API 定义(phase session extern "C"); 生命周期 create→validate→
run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase1.writer`; execution_class=`io`;
parallel_ok=False; 配置=phase config JSON(按 PHASE API 文档)。

## Execution class、并行轴、ThreadBudget lease、确定性

`io`; parallel=否(资源门拒绝 heavy+serial 组合); worker 数=ThreadBudget.max_workers(唯一取值源);
确定性=NOT_VERIFIED（证据源 eng/ci/ledgers/module_page_evidence.json 无 astrocs.phase1.writer.determinism 条目；未读到的结论不写成 PASS/FAIL）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源=lib/infrastructure/cli/exit_codes.h（本页不复制数值表）；取消=协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成 → exit 9，最高设计 §6.3；接线以实测为准）；模块内无 checkpoint（无断点续算）。

## 独立 synthetic 验证命令与容差

测试标识=`TEST-P1-WR-001`（registry descriptor 单源）；执行证据=NOT_VERIFIED（证据源 eng/ci/ledgers/module_page_evidence.json 无 astrocs.phase1.writer.verification 条目）；容差=NOT_VERIFIED（同上）。

## 已知限制

见 docs/KNOWN_LIMITATIONS.md 与 `ALG-P1-WR-001` 合同边界。
