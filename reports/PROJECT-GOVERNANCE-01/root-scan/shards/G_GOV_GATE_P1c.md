# 分片 G_GOV_GATE_P1c —— ROOT-004 旧 bug 清单按最新权威订正

- 分片名：`G_GOV_GATE_P1c`（31 条：V13-N-01..V19-N-07，类别 G_GOV_GATE，优先级 P1）
- 产物：`reports/PROJECT-GOVERNANCE-01/root-scan/shards/G_GOV_GATE_P1c.psv`（UTF-8，1 表头 + 31 数据行 = 32 行，列数 31/31 校验为 10）
- 四态计数：**OPEN 31 / RESOLVED 0 / VOID 0 / UNVERIFIABLE 0**
- 归属分布：CI-001 13、QA-001 11、MOD-001 3、PKG-001 2、CPU-001 1、RT-001 1；GAP 关系 31/31 记「无」

## ID 覆盖自证（命令+输出）
```
$ cd "/workspace/Astro CS Database" && timeout 60 python3 -c "<比对 _assign/G_GOV_GATE_P1c.tsv 与 G_GOV_GATE_P1c.psv>"
lines 32 bad []                      # 每行竖线数 +1 == 10，无行内竖线、无换行
assign 31 psv 31 order_ok True       # 行序与分配表逐字一致
```

## 异常（无法定位的 ID / 找不到原文的条款 / 时点问题）
1. **基线不符**：任务说明 HEAD=main=`ecf6ad6f`，实测 `2c328348304d033aecfa81faf79d1c6cd802b30a`（subj: `docs(governance): 登记 GAP-024/025 与 CFG-001 裁决；消除 ROOT 卡二义`）。全部行号按当前树重定位。
2. **工作树扫描期并发改动**：`ci/checks.json` 145→147 项（`CHK-*` 0→2）、`docs/contracts/INDEX.yaml`、`工程控制/…/GAP_AUDIT.md`、`.gitignore`、`artifacts/prerelease_v5/ISA-00{1,2,3}/MEASUREMENTS.csv` 处 M 状态，`设计大纲/**` 大批 D。故 V19-N-02/V19-N-05/V19-N-06 等集合类数字按本行重算口径给出，不沿用 finding 原数。
3. **账本 clause 列大面积为空**：31 条中 27 条在 `问题扫描/账本/FIX_LEDGER.csv` 的 clause 列为空；仅 V14-N-01（宪章 §16.2/§16.3、§17.2、§17.12）、V14-N-04（§17.2、§12.3）、V14-N-05（§17.6）、V14-N-06（§10.2）、V17-N-04（§10.5）、V17-N-06（§15.4）有 finding 内明文引用。空者第 4 列按 SHARD_BRIEF 写「无（原判据即现行权威，账本 clause 列为空）」。
4. **路径/行号漂移（已重定位）**：`check_traceability_matrix.py` 现行于 `tools/traceability/`（finding 写 tools/quality）；`test_p1003_drizzle_path.py` 现行于 `tests/cli/`（finding 写 tests/ 下）；V18 各站行号普遍漂移 2-6 行（如 p1hips 注入支 64→69）。
5. **原 finding 集合数字与重算不一致（已在备注注明口径）**：selfcheck 家族 10 TU/16 门 → 现 **11 TU / 17 处 `child_rc == 0`**（`==127` 分类仅 p1drz 1 处）；V19 profiles census fast63/linux-main122/windows-main68 → 现 **68/137/74**（fatduck 仍 0）。
6. **分配表与 finding 自标优先级不一致**：V18-N-13 分配表原优先级 `P1`、finding 自标 `(P2)`；按「逐字复制分配表」写 P1，差异已写入备注。
7. **找不到原文的条款**：V13 四条与 V14-N-02/N-03/N-08、V17、V18、V19 多数 finding 未给旧节号（账本 clause 亦空），第 4 列按要求写「无（原判据即现行权威…）」并保留判据对象路径。
8. **无法定位的 ID：无**（31/31 均定位到正文与当前树锚点）。
9. 取证分工：V13/V14/V17/V18 由分片 Agent 与分片内子代理完成并抽样复算通过；V19 子代理超时被中断，V19 六行由分片 Agent 亲自取证（V19-N-03 的「实消费 9 道」与 V19-N-05 的「21 道门」两项**未逐门复算**，已在第 10 列声明，不影响 OPEN 判据）。
10. 纪律：全程**零修复**（只写本 .psv 与本 .md 两个交付文件 + `run/…/G_GOV_GATE_P1c.log` 日志）、**零 git 写**（仅 git rev-parse/ls-files/log/rev-list/status 只读查询）、未 read/打印 `FATDUCK_ACCESS.md`、未触碰 `lib/** cli/** tests/** contracts/** ci/** tools/** docs/**` 及仓库根条目。

## UNVERIFIABLE 清单
- 无（0 条）。
