---
id: MOD-astrocs-phase3-properties
version: 1.0.0
status: NOT_VERIFIED
owner: astrocs-core
source_commit: 822b9c5391a14cc36979a7c550984f6ce363c713
upstream: [SCI-P3-PROPS-001, ALG-P3-001, API-P3-001]
downstream: [TEST-P3-PROPS-001]
---

<!-- GENERATED-ANCHOR tool=eng/tools/quality/gen_module_readmes.py tool-sha256=4fb971add4d7449dd4b58ca4d2af74c7b8eaeca5924ec940b488dcda0f864402 source=lib/infrastructure/scheduler/src/module_adapters.cpp source-sha256=db3a8ed2fc7e26f232145d7e1e8b020f17c5a555f156a10290aec2a630c29979 ownership=docs/DOCUMENT_INDEX.yaml status=GENERATED evidence=eng/ci/ledgers/module_page_evidence.json evidence-sha256=- regenerate="python3 eng/tools/quality/gen_module_readmes.py" not-verified=status,determinism,verification,tolerance body-sha256=e8efdb5f9c29f322b2a8334155658703573712d7be9d512c763fbf349db2eef3 -->

# 模块 astrocs.phase3.properties

> 上游：ASTROCS_DESIGN.md §8.4（模块与 ABI）

## 职责与明确非职责

Registry production 模块(唯一源=module_adapters.cpp descriptor)。职责由
SCI/ALG 合同定义(见链接); 不做 SCI/ALG 之外的扩展。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `hips` | `DATA-HIPS-001` | 必 | `UnitId::SURFACE_BRIGHTNESS` | `CoordinateFrame::HEALPIX` |
| `props` | `DATA-P3-PROPS` | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::HEALPIX` |

invalid = NaN/coverage=0(按 DATA 合同)。

## 公共 header、核心 symbol 与生命周期

由 `API-P3-001` 公共 API 定义(phase session extern "C"); 生命周期 create→validate→
run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase3.properties`; execution_class=`cpu_heavy`;
parallel_ok=True; 配置=phase config JSON(按 PHASE API 文档)。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是(资源门拒绝 heavy+serial 组合); worker 数=ThreadBudget.max_workers(唯一取值源);
确定性=NOT_VERIFIED（证据源 eng/ci/ledgers/module_page_evidence.json(absent) 无 astrocs.phase3.properties.determinism 条目）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源=lib/infrastructure/cli/exit_codes.h（本页不复制数值表）；取消=协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成 → exit 9，最高设计 §6.3；接线以实测为准）；模块内无 checkpoint（无断点续算）。

## 独立 synthetic 验证命令与容差

测试标识=`TEST-P3-PROPS-001`（registry descriptor 单源）；执行证据=NOT_VERIFIED（证据源 eng/ci/ledgers/module_page_evidence.json(absent) 无 astrocs.phase3.properties.verification 条目）；容差=NOT_VERIFIED（证据源 eng/ci/ledgers/module_page_evidence.json(absent) 无 astrocs.phase3.properties.tolerance 条目）。

## 已知限制

见 docs/KNOWN_LIMITATIONS.md 与 `ALG-P3-001` 合同边界。
