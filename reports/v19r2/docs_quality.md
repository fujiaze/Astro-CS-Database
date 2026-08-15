# Docs Quality (V19R2)

## 文档体系

- L0：README / DEVELOPER_GUIDE / RELEASE_STATUS / KNOWN_LIMITATIONS /
  CHANGELOG（指针已更新到新分层）；
- L1 science：docs/science/* 11 份（目的/定义/公式/变量/单位/假设/域/
  误差/精度/参考文献/ID）；
- L2 algorithms：docs/algorithms/* 12 份（输入/输出/前后置/不变量/伪代码/
  复杂度/并行/数值风险/fast+reference+oracle）；
- L3 architecture：docs/architecture/* 12 份 + docs/diagnostics/；
- L4 standards：docs/standards/* 13 份；
- L5 modules：docs/modules/* 13 份（固定模板）；
- 历史：docs/history/（V19 扁平文档迁移，非 current authority）。

## 机器一致性

`tools/docs_machine_consistency.py` 6/6 PASS（config/exit-code/stage/SNR
常数/产品契约/Drizzle 公式 ↔ 代码）。

## 缺口修复

- F-V19R2-DOCS-001：DATA_SEMANTICS.md 补齐 variance/ivar 产品语义
  （DATA-HIPS-VAR-001/IVAR-001）——S8 修复；
- ERROR_MODEL.md 补齐 AstroCsExitCode 表；
- TRACEABILITY.csv 30 行，broken=0。

## 故障定位演练（§20）

10 个故障场景全部可经 docs/diagnostics/TROUBLESHOOTING.md 定位到
stage → error/metric → contract → module doc → source/test
（docs/diagnostics/TROUBLESHOOTING.md 表格）。

```text
DEVELOPER_DOCS=PASS
```
