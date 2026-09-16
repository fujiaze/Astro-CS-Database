# 分片 G_GOV_GATE_P0（ROOT-004 分片执行）

- 分片名：G_GOV_GATE_P0；分配 28 条（M3b-G-01 .. V9-N-17）；PSV 行数 28（文件 29 行 = 表头 1 + 数据 28）。
- 四态计数：OPEN 24 / RESOLVED 4（M5a-G-001、M5a-G-002、V2-N-01、V2-N-08）/ VOID 0 / UNVERIFIABLE 0。
- 结论列均为本轮在当前树重跑的命令证据（第 6 列：单行命令 + 逐字输出，未使用「同上/见原文/推断」）。

## ID 覆盖自证（命令 + 输出）

```
$ wc -l reports/PROJECT-GOVERNANCE-01/root-scan/shards/G_GOV_GATE_P0.psv
29 reports/PROJECT-GOVERNANCE-01/root-scan/shards/G_GOV_GATE_P0.psv
$ diff <(awk -F'|' 'NR>1{print $1}' .../shards/G_GOV_GATE_P0.psv) <(awk -F'\t' 'NR>1{print $1}' .../shards/_assign/G_GOV_GATE_P0.tsv) && echo ID_COVERAGE_IDENTICAL
ID_COVERAGE_IDENTICAL
$ awk -F'|' 'NF!=10{print "BADCOLS line " NR ": " NF}' .../shards/G_GOV_GATE_P0.psv; echo FIELD_CHECK_DONE
FIELD_CHECK_DONE
```

（行序 = 分配表顺序；28 个 ID 逐字一致，每行恰 10 列，列内无管道符与换行。）

## 异常

1. 基线与简报不符：简报写 HEAD=main=ecf6ad6f，实测 `git rev-parse HEAD` = `2c328348304d033aecfa81faf79d1c6cd802b30a`（2c328348，位于 ecf6ad6f 之后 2 个提交），且工作树含其它并行线的已修改文件（artifacts/**、evidence/**、reports/v19r2/**、问题扫描/_recheck|_verify/**）。本轮一律按**磁盘当前树**取证。
2. M6b-G-001 数值漂移（不改变结论）：finding 记 evidence_status VERIFIED 3/30、test_path 以 docs/ 开头 18/22；本轮实测 VERIFIED 6/30、docs/ 前缀 15/22 —— EVIDENCE 层缺口扩大，判定仍 OPEN。
3. M6b-G-002 子项变化：AGENTS.md 已不含 `REVIEW_PENDING`/`治理要素映射表`（ecf6ad6f 版本同样为 0 命中），而 tools/check_agents_gov.py:15 仍把 `REVIEW_PENDING` 列为强制 needle；四份 RELEASE_STATUS 与 PASS 口径仍在，主判词不变，判 OPEN。
4. M5b-G-01 证据中 CI 步骤输出行尾的单反斜杠为脚本原文续行符，PSV 内以 \\ 保留。
5. M7-G-001 为家族判词：本轮复核门文本与 §15「§11 Oracle 全过」声明仍在位、docs/science+docs/algorithms 全库无「判决域/折算因子」声明、并复现 FWHM 解析值 1.2303076525901024 vs 冻结 1.230310（差 2.35e-6）；**未**逐成员复算七处门的完整数学（M7-T-101/M7-A-118/M1a-C-001 等分属他域）。

## UNVERIFIABLE 清单

- 无（0 条）。28 条全部给出可复跑命令证据。
