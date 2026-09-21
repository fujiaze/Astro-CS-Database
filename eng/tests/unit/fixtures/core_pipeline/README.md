# core_pipeline 夹具（pipeline IR 示例）

`core_pipeline`（CORE-004/RT-004）单元测试的输入夹具。两个文件的**内容与 V6 控制包原夹具逐字节一致**：

```
git show a861d8f6^:engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/fixtures/<file>
```

| 文件 | 期望判据（测试内断言） |
|---|---|
| `valid_pipeline.json` | `PipelineIRParser::parse` 成功，且 `validate`（registry 见测试内 `make_registry()`）无 issue |
| `invalid_pipeline_serial_heavy.json` | `parse` 失败：`resources.class=cpu_heavy` 与 `parallel=false` 互斥 |

## 为什么在这里

- 原夹具随 `工程控制/CONTROL_V6/.../fixtures` 发布；该目录经 GOV-002（`b7b2dea7`）归档，
  再经 `a861d8f6`（以新设计文档集替换旧治理体系）整树删除 ⇒ 测试里指向旧控制包的所有候选
  路径在现行树中恒为空，`core_pipeline` 恒红（GAP-036）；
- 依 ENGINEERING_SPEC §8「锚存活」（硬编码引用必须存在）与 GAP-036 处置「把 fixtures 以新位置
  重建」，夹具重建在测试共址位置 `eng/tests/unit/fixtures/core_pipeline/`；测试经 `ASTROCS_REPO`
  （ctest 由 `eng/tests/unit/CMakeLists.txt` 注入仓库根）定位，判据未放宽、断言未删减。

重建溯源：QA-001（`run/PROJECT-GOVERNANCE-01/QA-001/logs/03-fixture-identity.txt`，
与原夹具 sha256 相同）。
