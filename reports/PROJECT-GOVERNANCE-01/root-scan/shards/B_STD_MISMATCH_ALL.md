# 分片 B_STD_MISMATCH_ALL（ROOT-004 旧 bug 清单按最新权威订正）

- 分片名：`B_STD_MISMATCH_ALL`　类别：B_STD_MISMATCH　分配 25 条（M1a-B-001 .. M8a-B-001）
- 产物：`reports/PROJECT-GOVERNANCE-01/root-scan/shards/B_STD_MISMATCH_ALL.psv`（表头 1 行 + 25 行，10 列）
- 本轮基线（实测，与派发提示词不同，见「异常」）：开工时 `HEAD = main = 2c328348304d033aecfa81faf79d1c6cd802b30a`；取证期间并行线提交持续推进 HEAD（`2c328348` 到 `c44adc08` 到 `4511712b345a2548aad6230ad61cc705b44ef7bd`）；25 条命令已在最新树 `4511712b` 上全量重放，输出逐字一致（REPLAY_DIFF_COUNT 0）
- 四态计数：**OPEN 25 / RESOLVED 0 / VOID 0 / UNVERIFIABLE 0**
- 优先级分布：P0 10 条（M1a-B-001/002、M2a-B-1、M2b-B-01/02/03/04/07/09、M9-B-1）、P1 13 条、P2 2 条
- 归属分布：GOV-001 11、P1-001 9、CFG-001 1、DATA-001 1、P2-002 1、P3-001 1、NEXT-PACK 0
- GAP 关系：全部 `无`（逐条比对 GAP_AUDIT.md；仅 M2a-B-3 与 GAP-024 部分相关，题面不同，未标重复）

## ID 覆盖自证（命令 + 输出）

```
python3 -c "import csv,io;raw=open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/B_STD_MISMATCH_ALL.psv',encoding='utf-8').read();rows=list(csv.reader(io.StringIO(raw),delimiter='|',quoting=csv.QUOTE_NONE));asg=[l.split(chr(9))[0] for l in open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/_assign/B_STD_MISMATCH_ALL.tsv',encoding='utf-8').read().rstrip(chr(10)).split(chr(10))[1:]];print(len(rows),all(len(r)==10 for r in rows),[r[0] for r in rows[1:]]==asg)"
输出：26 True True
```

- 26 行（表头 + 25）逐行 10 列；第 1 列 ID 序列与分配表逐行一致（同序、无缺、无多）。
- 第 6 列口径：每行均为本轮**真跑过**的单行命令 + 逐字输出（≤3 行）；全部命令不含 `|`（避免与 PSV 分隔符冲突），输出中出现的 `|` 一律改写为 `¦`（仅限第 6 列，共 12 行受影响）。
- 命令原始取回件：`run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/B_STD_MISMATCH_ALL.log`（含每行命令、原始输出、`¦` 替换对照）。

## 异常

1. **基线漂移（不属环境掩盖，属事实登记）**：派发提示词与 SHARD_BRIEF 均声明基线 `ecf6ad6f`，本轮实测 `git rev-parse HEAD = git rev-parse main = 2c328348304d033aecfa81faf79d1c6cd802b30a`，二者相差 3 个提交（最新提交 `docs(governance): 登记 GAP-024/025 与 CFG-001 裁决；消除 ROOT 卡二义`）。本轮所有判定以 `2c328348` 树为准。
2. **原 finding 行号漂移（不改变结论）**：M1a-B-001 的 `NB_GRID` 由 :402-403 迁至 :411-412；M1a-B-003 的 ipv 输出块由 :869-872 迁至 :936-938；M9-B-1 的 `p1_sip_write_header_frame` 由 :1678 迁至 :1933；M9-B-2 的复制站点 2 由 :564-571 迁至 :436-441；M8-B-001 的 `artifact.h` 由 `lib/core/src/artifact.h` 迁至 `include/astrocs/core/artifact.h`（旧路径已不存在）。
3. **原 finding 内一处条款锚纠正**：M1a-B-004 所引 `docs/algorithms/PHASE3_PROJ_IMPL.md §5（:107-116 头面示例）` 在现树**已不存在**（该文 §5 现为「SCI/ALG 映射声明」，关键词合同迁到 §8 且只列 `CTYPE1= 'RA---TAN'` 形态、无列位断言）；代码缺陷（等号落第 7 列）仍在，故仍判 OPEN，但「合同示例与代码相反」这一半在现树已无对象。
4. **M8a-B-001 原报两项不成立**（现树复核）：`2.4.1` 与 `2.4.7` 互斥不存在（`lib/`+`docs/` 内 `2.4.1` 仅命中 vendored `third_party/cfitsio/m4/libtool.m4` 的 "Rational C++ 2.4.1"）；`rcr_oracle_compare.py` 直接 `from rcr import`，无 astropy 冒名。本条按残留的「语义权威外挂软件版本 + 无推导位」成立。
5. **集合类数字重算与原报差异（已按本轮口径写入）**：M2b-B-08 本轮得 `checklist_rows 37 / unique_ids 27 / orphan 8 / CONFORMANT-行-挂开放偏差 7 行（62,94,97,98,99,160,163）/ CLOSED 0`（原报口径为 27 条 §3 索引行、32 条六域清单行，差异源于行数与表头识别口径）；M2b-B-10 的 `fits §6` 行本轮实测为注册表 :223（原报未给行号）。
6. **命令沙箱**：本轮未触发任何沙箱拒绝；`FATDUCK_ACCESS.md` 未被 read/打印；未执行任何 git 写操作；仅写了本任务允许的 2 个报告文件 + 1 个日志文件。

## UNVERIFIABLE 清单

- **无**。25 条全部在本轮以当前树命令证据定档为 `OPEN`（每条均满足「当前树仍违反最新权威条款」并给出可复跑命令与逐字输出）。
- 唯一受限维度（不改变四态）：M2b-B-01/02/03/04/05/06/07 的**国际标准原文**为原 finding 已取回件（IVOA HiPS 1.0 §4.1/§4.4.1/§4.4.2、MOC 2.0 §6 Table 3、FITS 4.0 §4.4.2+附录 J、Górski 2005 §5），本轮**未重新联网取回**，仅核对仓内条款锚与实现事实；结论不依赖重新取回。
