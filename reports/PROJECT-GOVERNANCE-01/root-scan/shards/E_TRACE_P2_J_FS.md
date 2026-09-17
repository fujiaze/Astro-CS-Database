# E_TRACE_P2_J_FS 分片自检报告（ROOT-004 重派复核轮）

- 分片名：E_TRACE_P2_J_FS（27 条 = E_TRACE_BREAK P2 ×19 + J_FS_PUBLISH P1×2/P2×6）
- 产物：shards/E_TRACE_P2_J_FS.psv（1 表头 + 27 数据行）；日志 run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/E_TRACE_P2_J_FS.log
- 判定结果：OPEN 27 / RESOLVED 0 / VOID 0 / UNVERIFIABLE 0（全部为本轮当前树复现，未沿用 finding 或上轮自述）

## ID 覆盖自证
命令：python3 比对 psv 与 _assign/E_TRACE_P2_J_FS.tsv（命令与输出全文见日志 SELF-CHECK 段）
输出：HEADER_OK True；DATA_ROWS 27；NF10_OK True；STATES_OK True；STATE_DIST {'OPEN': 27}；ASSIGN_N 27；ID_SEQ_OK True；CAT_PRI_OK True

## 结论要点（与已入库上轮版本的关系）
- 27 条全部独立重跑取证：ID 序列与结论分布与上轮一致（均 OPEN），但行号锚、计数口径、站点存在性均按本轮树重新定位（如 M2a 的 tests/pipeline→tests/runtime、L28e-E-004 的 5 处→1 处、M8 的 stage2.cpp 1762→1782 行、W4-R2-04 的 commands.cpp 行号 :326-328）。
- 子项修正已写入对应行备注：M2a-E-5 第三名字 test_missing_required_each 已不在 DATA-002（子项不成立、口径收窄为两站）；M6b-E-006 的 $F.2 引用已从现文清除（不再计，核心错误引用仍在且锚对象今已整体出库，较时点恶化）；SA-N-03 影子树实例 module_entry.cpp 已入库转正（余量按档案口径保留）。

## UNVERIFIABLE 清单
- 无。

## 异常记录
1. 基线漂移：任务卡基线 HEAD=2c32834…，开工实测 HEAD=c44adc0…，取证收工时为 d414c3e…（并行提交持续推进，SHARD_BRIEF 的 ecf6ad6f/f96dff61 与 REBASE.md 抬头均早已过期）。本分片全部证据为工作树 d414c3e0 时点实测；cli/commands.cpp 正被 RUNTIME-CI-001 线改版，W4-R2-04 行号可能再漂。
2. 上轮同名片段（27 行）已在库（commit 46a1599a），本轮按任务指令整文件重写覆盖；全程零修复、零 git 写。
3. 多条 finding 的证据摘录含 markdown 表格行（内含竖线），PSV 纪律下按转述并在备注注明；虚构符号 build_fits_wcs_from_solution（M7-E-201 表内用词）已在 M7-E-201 与 SA-N-02 两行交叉标注。
4. GAP 关系：M6b-E-006 与 SA-N-04 标「与 GAP-019 重复」；W4-R2-03 标「与 GAP-021 重复」；W4-R2-06 标「与 GAP-017 重复」；其余 23 条「无」。
5. 归属分布：DOC-001 ×9、MOD-001 ×3、AIO-001 ×3、PKG-001 ×3、GOV-001 ×2、DATA-001/CI-001/OBS-001/P1-001/CLI-003/ROOT-002 各 ×1。
