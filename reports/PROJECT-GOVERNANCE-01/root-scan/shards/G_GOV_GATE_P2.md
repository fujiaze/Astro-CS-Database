# 分片 G_GOV_GATE_P2 执行报告（ROOT-004）

- 分片名：G_GOV_GATE_P2（类别 G_GOV_GATE，优先级 P2，分配 29 条：FD-G-003 至 V19-N-12）
- 产物：`reports/PROJECT-GOVERNANCE-01/root-scan/shards/G_GOV_GATE_P2.psv`（表头 1 行 + 数据 29 行，10 列）
- 行数：**29**；四态计数：**OPEN 27 / RESOLVED 0 / VOID 1 / UNVERIFIABLE 1**
- 判定时点：2026-09-16T07:28:06Z。**基线漂移**：任务书写 HEAD=main=ecf6ad6f，实测本轮作业期间 HEAD 由 2c328348 继续前移到 c44adc08，且 `ci/checks.json` 正被另一条线并发改写（145→147 门）。为可复跑，本轮把校验对象快照钉在 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/_snap/checks.json`（sha256=`f89af2c483dc44af1a27271d54ee4b4bfb6e673c19381e8590cdc68164861eaa`）。

## ID 覆盖自证（命令 + 输出）

```
$ python3 -c "<csv 解析 PSV 与 _assign/G_GOV_GATE_P2.tsv 并逐位比对>"
data_rows= 29 cols_ok= True order_match= True
verdicts= {'VOID': 1, 'OPEN': 27, 'UNVERIFIABLE': 1}
header_ok= True
```

ID 列表（逐字，序 = 分配表序）：FD-G-003, M1a-G-003, M2a-G-2, M2a-G-3, M4-G-02, M6b-G-007, M6b-G-008, M8a-G-009, M8a-G-010, V11-N-03, V13-N-05, V13-N-07, V13-N-08, V14-N-07, V16-N-01, V19-N-09, V19-N-10, V19-N-11, V19-N-12, V5-N-02, V5-N-04, V9-N-11, V9-N-13, W5-N-10, W5-N-11, W5-N-12, W5-N-13, W5-N-14, W5-N-15。

分配表 ID 与 PSV ID 差集：missing=[] extra=[]。

## UNVERIFIABLE 清单

- `V19-N-12`（1 条）：正文逐字自述「负结果 · 不立条，供邻站定性与防误报」，作者未立为条目。缺的是「该负结果是否升为独立偏差条目」的**负责人裁决**；其根事实本轮已用解释器复算逐字成立（`ci/checks.schema.json` 为 `additionalProperties=false`，无 `inputs`/`depends_on`/`needs`/`produced_by`）。

## 异常与需上级注意的事实

1. **原 finding 措辞过宽，本轮订正 3 条（均保留 OPEN）**：`V19-N-10`「既不能提交也不被忽略」不成立（`git status` 显示二者为 `??` 未跟踪、可提交），真实偏差是 `ci/run.py` 的 `ignore_exact=set(check[outputs])` 对未登记产物自豁免 dirty 检测；`W5-N-10`「rc=124/127 不在冻结码集」不成立（局部计数器，失败路径统一 `emit_final(INTERNAL)`/`return INTERNAL`，且 `cli/protocol.h:32-38` 硬闸拒绝域外值），真实偏差是 `include/astrocs/core/contracts.h:41-46` 重复数值表 + 门只扫 `cli/`；`V5-N-04` 新增 `CTEST-GAIA-MAGNITUDE-RANGE-BOUNDS` 已覆盖畸形声明解析边界，但仍未覆盖剪枝谓词方向。
2. **集合类数字重算全部高于原报**：M2a-G-2 29/42（原「约半数」）；V19-N-09 13/98 dead（原 7 道门 + 5 条 rule）；V9-N-13 102/147 与 133/147（原 99/130、124/130）；V13-N-08 5 个补丁（原 4 个）；M1a-G-003 6 个文件（原 3 处）。
3. **行锚漂移（E_TRACE_BREAK 邻域）**：M1a-G-003 的 `p3_wcs_validate_descriptor` 全仓 0 命中；V11-N-03 由 :108-111 漂到 :78-81；V5-N-13、W5-N-12/13 路径迁移到 `src/ahpx/`、`tools/quality/contracts/`；M6b-G-008 原引 README.md:33 已移走。
4. **发现一条不在本分片、也不在 23 条 GAP 的偏差**（未在本分片产物中单独立条，仅登记备查）：`docs/ci/01_CHECKS.md §1` 写「豁免显式登记 `ci/exemptions.json`，只减不增」，但 **`ci/exemptions.json` 不存在**，实际豁免分散在 `ci/checks.json` 的 `waivable`（7 项）与 `ci/run.py:103 EMPTY_OUTPUT_SILENCE_EXEMPT`（硬编码 2 项）。建议主控另立条目（候选 NEXT-PACK:NP-09）。
5. **未越界/未修复**：本分片只写 `.psv`/`.md` 两个产物 + `run/` 下日志与快照，未改任何代码/文档/CI，未执行任何 git 写操作；`FATDUCK_ACCESS.md` 未 read/打印。
