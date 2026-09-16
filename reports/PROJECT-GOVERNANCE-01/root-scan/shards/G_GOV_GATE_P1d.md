# G_GOV_GATE_P1d 分片报告（ROOT-004 旧 bug 清单按最新权威订正）

- 分片名：`G_GOV_GATE_P1d`｜类别 G_GOV_GATE｜优先级 P1｜分配 31 条（V19-N-08 .. W5-N-09）
- 产物：`reports/PROJECT-GOVERNANCE-01/root-scan/shards/G_GOV_GATE_P1d.psv`
- 行数：**32**（表头 1 + 数据 31）｜四态计数：OPEN **27** / RESOLVED **4** / VOID **0** / UNVERIFIABLE **0**
- RESOLVED 4 条：`V2-N-09` `V2-N-10` `V6-N-05` `V9-N-20`

## ID 覆盖自证（命令 + 逐字输出）

```
$ cd "/workspace/Astro CS Database" && timeout 30 wc -l reports/PROJECT-GOVERNANCE-01/root-scan/shards/G_GOV_GATE_P1d.psv
32 reports/PROJECT-GOVERNANCE-01/root-scan/shards/G_GOV_GATE_P1d.psv
$ timeout 60 python3 /tmp/psv_proof.py     # 读 PSV 与 _assign/G_GOV_GATE_P1d.tsv 逐行比对
header_ok True
data_rows 31
cols_ok True          # 每行恰 10 列，列内无 | 无换行
no_cr True
assign_rows 31
order_and_set_equal True
missing_from_psv []
extra_in_psv []
verdicts {'OPEN': 27, 'RESOLVED': 4}
unverifiable []
```

行序 = 分配文件顺序，ID 集与分配表逐字相等。

## UNVERIFIABLE 清单

**无**（0 条）。31 条全部取得当前树可复跑证据；无一条以「环境/工具问题」结案。

## 异常

1. **基线两次漂移（并行线施工）**：任务书给定 `HEAD=main=ecf6ad6f`，本轮取证期实测 `2c328348304d033aecfa81faf79d1c6cd802b30a`（`git diff --stat ecf6ad6f..HEAD` 只动 `reports/PROJECT-GOVERNANCE-01/**` 9 文件）；收尾时 HEAD 又变 `939d3f6c57bb603e98e11e4ee2f4f5ab4c88c876`，且 `ci/checks.json` 工作树有 +61 行未提交改动（非本分片所写）。收尾复核确认全部承重结论未变（clean `CON-API-CONTRACTS` 仍 FAIL；`validate_registry` 仍 0；UT-BACKEND/UT-CLI/CTEST-PHASE2-GATES/CI-BINDING-TESTS 登记项逐字未变）。
2. **门数由 130 → 145 → 147**：第 6 列记录的是**取证当时**的 145（不可达 89）；收尾复核 147（不可达 91）。数字口径已在各行注明，未抄原 finding 数字。
3. **本机脏树使部分门假绿**：`run/**` 影子树（`run/rqs-fix/**`、`run/perf-fix/**`）让 `CON-API-CONTRACTS` 在工作树 PASS、干净检出（`git archive HEAD` 解包 /tmp）FAIL。凡涉「干净检出必红」的条目（V2-N-02/V9-N-04/V6-N-01/V9-N-01/V6-N-02/V9-N-02/V9-N-03/V9-N-05/V9-N-08/V9-N-20）一律在干净检出内取证。
4. **V9-N-09 真机直证已消失**：finding 引的 `artifacts/ci/**/win-test-summary.json`（exit_code 0 + No tests were found）在本树 `find artifacts -name "*test-summary*.json"` **0 命中**；本轮改以 `deep_ci_driver.py::_ctest_argv` 静态 argv 取证（结论仍 OPEN），已在第 10 列注明。
5. **原 finding 行号普遍漂移**：V2-N-09 的 `:1737/:1750` 现为 `:1614/:1793`；W1-N-03 的 `lib/snr_estimator/CMakeLists.txt:28` 现为 `:25`；V9-N-08 的 `synthetic_gate.cpp:5488` 未复现同一行（改以「1 条注释吞 222679 字符」机制取证）。行号漂移计为 E_TRACE_BREAK 类证据，不改结论。
6. **GAP 关系**（第 9 列）：仅 2 条判重复 —— `W1-N-03` 与 `GAP-009`（同一 lib/snr_estimator 注释与事实冲突 + 无独立 target）、`V9-N-15` 与 `GAP-021`（根目录运行产物污染）；其余 29 条记 `无`。`GAP_AUDIT.md` 实含 GAP-001..GAP-022（22 条），与简报「23 条」差 1，未寻得第 23 条。
7. **未触碰**：零修复、零 git 写；只写本分片 `.psv`、`.md` 与 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/G_GOV_GATE_P1d.log`；`FATDUCK_ACCESS.md` 未 read/打印。
