# 分片 G_GOV_GATE_P2 执行报告（ROOT-004 · 本轮复验版）

- 分片名：G_GOV_GATE_P2（类别 G_GOV_GATE，优先级 P2，29 条：FD-G-003 … W5-N-15）
- 产物：reports/PROJECT-GOVERNANCE-01/root-scan/shards/G_GOV_GATE_P2.psv（表头 1 行 + 数据 29 行 × 10 列）
- 四态计数：**OPEN 22 / RESOLVED 0 / VOID 7 / UNVERIFIABLE 0**
- 证据日志：run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/G_GOV_GATE_P2.log（全部命令本轮真跑、逐字输出）
- 纪律：零修复、零 git 写（仅只读查询）；未读取 FATDUCK_ACCESS.md；集合类判据全部重算未抄旧数。

## ID 覆盖自证
- 命令：python3 -c（逐行 split 校验：行数=29；每行 NF==10；结论∈四态；类别/优先级与账本一致；ID 序列与 _assign/G_GOV_GATE_P2.tsv 第 1 列逐字比对）
- 输出：data_lines= 29；nf_bad= []；conclusion_bad= []；ids_seq_equal_assign= True；cat_pri_bad= []；verdicts= {'VOID': 7, 'OPEN': 22}
- 锚点覆盖：29/29 在 _gen/idmap.csv 有锚文件与锚行（python csv 解析），0 条落空。

## UNVERIFIABLE 清单
- 无（29 条均可在当前树取证判定）。

## VOID 清单（7）
- M2a-G-3（§16.1 分离条款下链→§12 零版本替代，GAP-017 承接）；M6b-G-007（L0/Wiki 制度下链→§0 权威链替代，GAP-002 同族）；
- M8a-G-009（审核包/许可可寻址旧制→§12 发布候选门+ARCH-001 清运对象）；V19-N-12（负结果·不立条，旧 schema 面换代）；
- V5-N-02（夸大声明宿主=已降历史证据的旧账本 fix_note；代码点静态有界）；W5-N-11（旧 standards source 条文下链→§9+21_observability）；FD-G-003（防误读注非交付面偏差，红灯处理口径以 CP§7.3/docs ci 为准）。

## 异常记录
1. 基线漂移：任务卡记 HEAD=main=origin/main=2c328348；开工实测 HEAD=main=c44adc08；证据复跑时 main 已前进至 d414c3e0（origin=b4afc135）——控制包执行期内 main 仍在被提交（GAP-030 同现象）。全部行号锚按取证时点当前树重定位。
2. 本分片在 16:07 已存在一版旧产物（疑似早前派发未收尾版）：其 MD 自述 ID 范围与计数和自身 PSV 不符，且将 V19-N-12（负结果·不立条）判 UNVERIFIABLE、W5-N-11 判 OPEN、GAP 关系引用超范围的 GAP-027。本轮以逐项真跑复验覆盖重写，以本版为准。
3. W5-N-10 复验期间再漂移：原 commands.cpp:306/309 的 rc=124/127 已随 0d8e9e8f1（RUNTIME-CI-001）移出，124 消失、126/127 约定改驻 cli/process.cpp:173/175；「非冻结码当退出码+contracts.h 重表+§6.3 唯一源文件缺失」仍活，维持 OPEN 并已在行内写明重锚。
4. V19-N-10：artifacts/KNOWN_FAILURES_BASELINE.json 取证时点文件已不存在（并行清运），但注册机制面（checks.json:703 outputs + 不入库 + 不被忽略 + run.py outputs 自豁免 dirty）复现，按机制判 OPEN。
5. 数字口径更新：ci/checks.json 由 130→147 项；V19-N-09 本轮点名死模式门=6-7、V9-N-13=137/147 与 101/130、M2a-G-2=30/43、V13-N-05=63/63 全 VERIFIED——均本轮重算，与 finding 原数方向一致、幅度因注册表演进而变。
6. V13-N-08 原 finding 正文自述类别 I_DOC_HYGIENE，账本/分配表为 G_GOV_GATE——第 2/3 列按纪律逐字沿用分配表（G_GOV_GATE/P2）。
7. 账本 fix_state：29 条全部 OPEN（无 FIXED/PARTIAL），不触发「FIXED 复跑」路径；V13-N-05 另有 verified_state=STILL。
8. M8a-G-010 上游现势（外部）：本轮 web_fetch https://heasarc.gsfc.nasa.gov/fitsio/ status=200「latest 4.7.0」，非仅沿用 finding 抓取记录。
