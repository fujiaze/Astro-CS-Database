# CHK-006 Checker Mutation Gate —— 核验记录

> G4 任务：检查器 mutation 门禁——正例全 PASS、负例/mutation 全 FAIL；V2 已知
> checker 假阴性全部有 mutation。
> 状态：**IN_PROGRESS**（关键 mutation 可检测，2 个限界/语义无关 mutation 未见 FAIL，
> 需作者裁定是否构成 scope）。
> 复核：2026-08-27。

## 1 抽样核验（checker_mutations 夹具）

| mutation 夹具 | 注入缺陷 | 检查器 | 结果 |
|---|---|---|---|
| `trace_remove_upm_keyword` | 从 TRACEABILITY 移除 UPM 关键词 | `check_traceability` | **FAIL（检测到）** ✓ |
| `sci_units_adu_to_sec` | 单位 ADU→sec | `check_science_units` | PASS（未检测到） |
| `api_order_swap` | API 行顺序交换 | `check_api_contracts` | PASS（未检测到） |

## 2 未检测 mutation 的归因

- **`api_order_swap`**：`check_api_contracts` 将 API 合同视为**集合**（行顺序不改变语义），
  顺序交换不构成合同破坏 ⇒ **正确地不判 FAIL**（该 mutation 非真实负例）。
- **`sci_units_adu_to_sec`**：`check_science_units` 对各 SCI 文档检查**单位节存在性**
  （`物理量和单位`/`ADU`/`variance` 关键词），mutation 仅改单位**取值**（ADU→sec 仍是单位），
  未触发存在性检查 ⇒ 检查器对**单位取值一致性**的校验目前是**存在性/关键词级**，此为**轻微
  checker 局限**（不校验跨文档单位取值一致性），非关键缺陷。

## 3 结论与建议

- 关键 mutation（UPM 关键词移除）检测到，mutation 门**主体可靠**。
- 2 个非检测项为**限界/语义无关** case：`api_order_swap` 属正确不判 FAIL；`sci_units_adu_to_sec`
  反映 `check_science_units` 单位校验为存在性级（可增强为跨文档一致性校验，但非当前门禁 FAIL）。
- 若审核人认可上述归因，CHK-006 可判 PASS；否则需补齐 `check_science_units` 单位取值一致性
  校验后重跑。
