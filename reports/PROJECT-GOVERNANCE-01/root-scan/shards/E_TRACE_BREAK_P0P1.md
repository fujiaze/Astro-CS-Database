# 分片 E_TRACE_BREAK_P0P1 — ROOT-004 旧 bug 清单按最新权威订正

- 分片名：E_TRACE_BREAK_P0P1（分配 32 条：M3-E-001、M6b-E-001、L28c-E-001/002、L28e-E-001/002/003、M1a-E-001/002/003、M2a-E-1/2/3/4、M2b-E-01、M3-E-002、M3b-E-01、M4-E-01/02、M5a-E-001、M5b-E-01..E-05、M6a-E-001、M6b-E-002/003/004、M8a-E-001/002、M9-E-1）
- 产物：`reports/PROJECT-GOVERNANCE-01/root-scan/shards/E_TRACE_BREAK_P0P1.psv`（表头 1 行 + 32 数据行，列序=分配文件顺序）
- 四态计数：OPEN 32 / RESOLVED 0 / VOID 0 / UNVERIFIABLE 0

## ID 覆盖自证
命令：python3 -c "import csv;a=[r[0] for r in list(csv.reader(open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/_assign/E_TRACE_BREAK_P0P1.tsv',encoding='utf-8'),delimiter=chr(9)))[1:]];p=[r[0] for r in list(csv.reader(open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/E_TRACE_BREAK_P0P1.psv',encoding='utf-8'),delimiter=chr(124)))[1:]];print('assign=',len(a),'psv=',len(p),'same_order=',a==p,'unique=',len(set(p)))"
输出：assign= 32 psv= 32 same_order= True unique= 32

## UNVERIFIABLE 清单
无。32 条均在本轮用当前树命令重新取证（read/grep/git ls-files/解释器解析），无需外部数据的条目也能给出仓内可复跑判据。

## 异常
1. 基线漂移：任务书称 HEAD=main=ecf6ad6f；本片开工时实测 2c328348304d033aecfa81faf79d1c6cd802b30a，收工时已变为 c44adc085593345602c30e2081fd1ecfbc1224eb（并行线在本片作业期间提交；docs/contracts/INDEX.yaml、ci/checks.json、docs/contracts/DATA_ARTIFACTS.md 被并行线改动，contracts/schemas/unified/ 新增——均非本片所为）。本片 32 条判定与 PSV 第 6 列命令已在收工 HEAD 上**整体复跑一次，32/32 输出逐字一致**；判定按当前树，未按任务书基线回退。
2. 行锚相对 finding 记录继续漂移（本轮实测）：module_adapters.cpp p1_nodes :5471→:6282、p3_nodes :5523→:6334；aio_hips_writer.cpp aio_hips_product_begin :420→:466；coverage.cpp 280→454 行、coverage.h 65→168；p3_resample.cpp 239→519；gaia_client.c 2441→2700。集合/数字类判据均按本轮口径重算，未抄 finding 数字。
3. 与 finding 数字不一致处（PSV 采本轮重算值）：L28e-E-001 该两族 CSV 行 16→17；M6b-E-003 矩阵非占位 ID 152→158、contracts/INDEX.yaml id 100→114、不在册 85→91（INDEX.yaml 作业期间被并行线改动，100 为开工读数、114 为收工复核读数，PSV 取 114）；M3-E-002 复算仍 13/14 组不命中（命中组由第 5/9 组的子锚变为第 11 组，已改指 §8 且 §8:77 在位）。
4. 整条 OPEN 但子项已消失（已在 PSV 备注写明）：L28c-E-002 的 `lib/photometric_calib/docs/algorithm.md` 本轮已存在；M3b-E-01 的 `docs/TRACEABILITY.csv:65` 行已删除（CSV 现 0 处 PSF）；M5b-E-05 的 `ARCHITECTURE_OVERVIEW.md:18` 原行已漂移（同型引用移至 RELEASE_STATUS.md:11/:70）。
5. P0 两条（M3-E-001、M6b-E-001）第 6 列均为本轮单行命令 + 逐字输出，无"同上/见原文/推断"。
6. 格式：列内无 `|`、无换行；需要保留含 `|` 原文的命令均用 chr(124)/awk gsub 在命令内替换为 `!`，命令与输出逐字对应。
7. 未读取 FATDUCK_ACCESS.md；未改任何代码/文档/测试/CI；未做任何 git 写操作；仅写本分片 psv、md 与命令日志。
