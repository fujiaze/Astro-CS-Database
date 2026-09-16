# 分片 G_GOV_GATE_P1b（ROOT-004）交付说明

- 分片名：G_GOV_GATE_P1b ｜ 类别 G_GOV_GATE ｜ 原优先级 P1 ｜ 分配 31 条
- 产物：reports/PROJECT-GOVERNANCE-01/root-scan/shards/G_GOV_GATE_P1b.psv（表头 1 + 数据 31 = 32 行，UTF-8）
- 四态计数：OPEN 31 ｜ RESOLVED 0 ｜ VOID 0 ｜ UNVERIFIABLE 0
- 归属分布：CI-001 8、ARCH-001 6、DOC-001 3、MOD-001 3、AIO-001 3、NEXT-PACK:NP-GAIA-01 3、PKG-001 2、OBS-001 1、GOV-001 1、P3-001 1
- GAP 关系：5 条标重复（与 GAP-008 三条、与 GAP-011 一条、与 GAP-002 一条），其余 26 条为「无」

## ID 覆盖自证（命令 + 逐字输出）

```bash
awk -F'|' 'NR>1{print $1}' reports/PROJECT-GOVERNANCE-01/root-scan/shards/G_GOV_GATE_P1b.psv > /tmp/got_ids.txt
awk -F'\t' 'NR>1{print $1}' reports/PROJECT-GOVERNANCE-01/root-scan/shards/_assign/G_GOV_GATE_P1b.tsv > /tmp/want_ids.txt
diff /tmp/want_ids.txt /tmp/got_ids.txt && echo COVERAGE_IDENTICAL
wc -l < /tmp/want_ids.txt; wc -l < /tmp/got_ids.txt
awk -F'|' 'NR>1{print $7}' reports/PROJECT-GOVERNANCE-01/root-scan/shards/G_GOV_GATE_P1b.psv | sort | uniq -c
```

```text
COVERAGE_IDENTICAL
31        （分配表行数）
31        （PSV 数据行数）
     31 OPEN   （结论列计数）
```

## 异常

- 基线漂移：任务书称 HEAD=main=ecf6ad6f，实测 HEAD=main=2c328348、origin/main=5f891080（本地领先旧基线两个提交）。本轮一律按当前树取证。
- 路径/布局漂移（已在对应行按当前树重定位）：include/ 公共头由 V11 普查时点 96 个降至 23 个（lib/*/include 现有 74 个）；gen_hips_browser_test.py 迁至 lib/healpix_db/healpix_browser_qt/tests/；orchestrator.cpp 迁至 lib/orchestrator/cpp/src/；json_get_f64_array 现于 module_entry.c:179；gaia_client.c 已重写（load_xpsd_file :1193、魔数校验 :1219）。
- 集合数字漂移（均按当前树重算并写入第 6 列）：ci/checks.json 75→145 项；ci/known_failures.json failures 2→1；DOCUMENT_INDEX 条目 236→305、ACTIVE_NORMATIVE 123→190（不在内容级门内 82→50）；lib/*/README.md 7→27；no_checks_selected 0→3 命中；V11 S-3 漂移仍为 7 处/6 文件/3 结构。
- 无法定位的 ID：无（31 条全部读到最后一级正文并重新取证）。
- 找不到原文的条款：无（第 5 列全部在 ASTROCS_DESIGN.md 与 ENGINEERING_SPEC.md 中逐节核对原文）。

## UNVERIFIABLE 清单

- 无（0 条）。
