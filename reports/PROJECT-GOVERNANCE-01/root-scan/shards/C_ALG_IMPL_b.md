# 分片 C_ALG_IMPL_b · 自证与异常（ROOT-004）
- 分片名：`C_ALG_IMPL_b`（类别 C_ALG_IMPL；原优先级 P2/P?；分配 34 条 V10-N-09 .. V7-N-10）
- 产物：`reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_ALG_IMPL_b.psv`；行数 **35 行**（1 表头 + 34 finding 行），行序 = 分配表顺序
- 四态计数：**OPEN 32 / RESOLVED 0 / VOID 2 / UNVERIFIABLE 0**
- VOID 两条：`V10-N-09`（权威链无「环境变量严格解析」条款；0 级即 AIO_LOG_INFO=默认值，finding 的「日志静默」不成立）、`V7-N-09`（判否负清单/非缺陷条目，无待整改对象）
- 命令日志：`run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/C_ALG_IMPL_b.log`

## ID 覆盖自证（命令 + 输出）
```bash
python3 -c "import csv;from collections import Counter;a=list(csv.DictReader(open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/_assign/C_ALG_IMPL_b.tsv',encoding='utf-8'),delimiter='\t'));psv=[l.rstrip().split('|') for l in open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_ALG_IMPL_b.psv',encoding='utf-8') if l.strip()];rows=psv[1:];ia=[x['ID'] for x in a];ip=[r[0] for r in rows];print('assign',len(ia),'psv',len(rows),'header cols',len(psv[0]));print('order identical',ia==ip,'missing',sorted(set(ia)-set(ip)),'extra',sorted(set(ip)-set(ia)));print('verdicts',dict(Counter(r[6] for r in rows)));print('cols!=10',[r[0] for r in rows if len(r)!=10],'stray pipe',[r[0] for r in rows if any('|' in c for c in r)])"
```
```text
assign 34 psv 34 header cols 10
order identical True missing [] extra []
verdicts {'VOID': 2, 'OPEN': 32}
cols!=10 [] stray pipe []
```

## 异常
1. **基线漂移（非本分片所为）**：派发声明 `HEAD=main=ecf6ad6f`；开工实测 `HEAD=main=2c328348`，收工实测 `HEAD=939d3f6c`（`git merge-base --is-ancestor ecf6ad6f HEAD` 为真）。全部取证在 2c328348→939d3f6c 窗口内完成，每条「当前证据」命令可复跑。
2. **并发现场污染（非本分片所为）**：取证期间 `ci/checks.json` 由 145 项变 **147** 项且为 `M`（`git diff --stat` = +61 行），另有未跟踪 `ci/root_manifest.json`；本分片零写入该域，W6-N-06 集合数字按实测 147 记录并附命令。
3. **两条判定口径分歧（PSV 备注已写理由）**：V10-N-09 判 VOID（三处 `*_log.cpp` 的 atoi 越界回退 INFO，且 `AIO_LOG_INFO=0` 即默认值；strtol 站点全在 `ASTROCS_TEST_*` 测试钩子，V7-N-09 第 9 条已判非缺陷）；V7-N-09 判 VOID（旧账 `verified_state=NOT_A_DEFECT`，九条判否在最新权威下仍非偏差，抽核第 1、3 条成立）。
4. **抽核未逐处重算（PSV 备注已标注）**：V20-N-09 的 6 结构体/18 字段只复现 3 结构体/6 字段；V20-N-07 的「41 处 (void)」未逐处重算（cal `(void)self`=3 已复核）；W6-N-07 的 ipv 两个头按旧路径已不存在（现存 `lib/plate_solve/cpp/ipv/ipv_types.h`），需重定位。
5. **行号漂移普遍**：finding 行号与当前树不符（W3-R2-004 162-165→259-263、V20-N-04 657→659、W6-N-09 1159-1166→1176 等），PSV 第 6 列一律给当前行号与逐字输出。
6. **写入范围**：仅本 `.psv`、本 `.md` 与 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/C_ALG_IMPL_b.log`；零修复、零 git 写、未触碰 FATDUCK_ACCESS.md。

## UNVERIFIABLE 清单
- 无（0 条）：34 条均在当前树复现核心判据；第 4 项的未重算子计数已按行标注为口径限制，不改变结论。
