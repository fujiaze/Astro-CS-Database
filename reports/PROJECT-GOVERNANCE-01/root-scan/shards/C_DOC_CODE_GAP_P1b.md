# 分片交付说明 · C_DOC_CODE_GAP_P1b（ROOT-004 分片执行层）

- 分片名：`C_DOC_CODE_GAP_P1b`（原类别 C_DOC_CODE_GAP，原优先级 P1）
- 条目数：**32**（分配表 `_assign/C_DOC_CODE_GAP_P1b.tsv` 数据行 M4-C-06 .. V2-N-04，逐条判定，无抽样）
- 四态计数：**OPEN 30 / RESOLVED 1 / VOID 1 / UNVERIFIABLE 0**
- 产物：`reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_DOC_CODE_GAP_P1b.psv`（表头 1 行 + 32 行 × 10 列，逐行 10 列校验通过）
- 命令日志：`run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/C_DOC_CODE_GAP_P1b.log`（295 行，含 32 条 ID 的实跑命令与输出）
- **基线异常**：任务书写 HEAD=main=`ecf6ad6f`；取证时实测 HEAD=main=`2c328348`，落日志时实测 `939d3f6c`。新增提交（5f891080/c5392daa/939d3f6c）只改 `工程控制/PROJECT-GOVERNANCE-01/` 治理文档（git diff --stat 已核），未触及本片任何证据文件；全部判定按**当前树**重新取证，行号按当前树重定位。

## ID 覆盖自证（命令 + 输出）
实际命令（把 | 写作 chr(124) 以避开 PSV 分隔符）：

`python3 -c "import csv;lines=open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_DOC_CODE_GAP_P1b.psv',encoding='utf-8').read().splitlines();rows=[l.split('|') for l in lines[1:]];tsv=list(csv.reader(open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/_assign/C_DOC_CODE_GAP_P1b.tsv',encoding='utf-8'),delimiter=chr(9)));a=[r[0] for r in tsv[1:] if r];b=[r[0] for r in rows];print(len(rows),all(len(r)==10 for r in rows),a==b,set(a)==set(b))"`

输出：

```text
psv_rows 32 cols_ok True
assign 32 psv 32 order_match True set_match True
verdicts {'OPEN': 30, 'VOID': 1, 'RESOLVED': 1}
unverifiable []
```

## 异常
1. 基线漂移（见上）：简报基线 ecf6ad6f 已不是当前 HEAD，本片以取证时点树为准并记录 SHA。
2. `M5b-C-07` 判 **VOID**：原判据引的 `ASTROCS_PROJECT_CONSTITUTION.md §18.1`（四投影、ZEA 不在内）已被 `ASTROCS_DESIGN.md §5.3`（首批冻结八投影，含 ZEA）相反替换；残余真实缺口（registry 仅 4 投影、缺 STG/MOL/CEA/ZEA）已登记为 GAP-011。
3. `M6a-C-002` 判 **RESOLVED**：实现已随 P30 改为跨 worker 共享有界 LRU（`SharedTileCache` + std::mutex + 负缓存），原「每 worker 无锁 FIFO / tile 读 N 倍 / 内存上界少算」不再成立；残余仅两处注释与 §3 一行措辞。
4. 三处原 finding 锚点在本轮复算中失效或需改写（不改变结论，已写入备注列）：`lib/star_detector/README.md:132` 已无 `RMSE=mad·1.4826/A` 等式（1.4826 命中 0）；`M9-C-2` 的 API_CONTRACTS.csv 现为 382 行（原记 102 行），units 去重仍为 1；`lib/astro_image_io/src/healpix/aio_healpix_io.h` 已移至 `lib/astro_image_io/include/aio_healpix_io.h`。
5. 集合类数字一律本轮重算并写入第 6 列：module_id 交集 0/21（21 份 manifest vs 22 个 descriptor）；vendored cfitsio 60 源；`.gitignore` healpix 命中 0；provider 字面量 baseline 4 处；`ci/checks.json` 中 docs_machine_consistency 命中 0。
6. 归属与 GAP 关系：全部 32 行填 30 任务之一（无 NEXT-PACK）；13 行与 GAP_AUDIT 既有条目重复（GAP-002/005/008/009/011/014/015/019），按纪律不合并、保留原 ID。

## UNVERIFIABLE 清单
无（0 条）。全部 32 条均能在当前树给出可复跑命令证据。
