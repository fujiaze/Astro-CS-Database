# H_NUMERIC_ALL 分片报告（ROOT-004）

- 分片名：H_NUMERIC_ALL；行数：PSV 表头 1 行 + 29 条 = 30 行（ID 覆盖 M2a-H-1 .. V2-N-07）
- 基线：HEAD=2c328348（ecf6ad6f 为其祖先；任务书写 ecf6ad6f），本轮取证全部在该 SHA 的现树上完成
- 纪律：零修复、零 git 写；只写 PSV/MD 与本日志；未读取 FATDUCK_ACCESS.md
- 产物：reports/PROJECT-GOVERNANCE-01/root-scan/shards/H_NUMERIC_ALL.psv；日志 run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/H_NUMERIC_ALL.log

## ID 覆盖自证（命令 + 逐字输出）
命令：timeout 60 python3 -c "ids=[l.split(chr(9))[0] for l in open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/_assign/H_NUMERIC_ALL.tsv',encoding='utf-8').read().splitlines()[1:] if l.strip()]; ps=[l.split(chr(124))[0] for l in open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/H_NUMERIC_ALL.psv',encoding='utf-8').read().splitlines()[1:] if l.strip()]; hdr=open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/H_NUMERIC_ALL.psv',encoding='utf-8').read().splitlines()[0]; print('assign=%d psv=%d order_match=%s cols=%d'%(len(ids),len(ps),ids==ps,hdr.count(chr(124))+1))"
输出：assign=29 psv=29 order_match=True cols=10

## 四态计数
OPEN=23 RESOLVED=5 VOID=1 UNVERIFIABLE=0（合计 29）
- RESOLVED：M2a-H-1、M3b-H-01、M9-H-1、M9-H-2、V5-N-03（树内已修复，均附复跑证据与替代条款）
- VOID：M7-H-104（docs/science/NOISE_MODEL.md §7/§9 明文要求任意 variance 经 max(...,1e-12)、平面预测负值 clamp 至 floor，原判据与最新权威相反，改判须走 SCI 变更）

## UNVERIFIABLE 清单
无（29 条全部给出本轮真跑的单行命令 + 逐字输出，无「同上/见原文/推断」）

## 异常
1. HEAD 已由 ecf6ad6f 前进到 2c328348（前者为其祖先），本轮判定以 2c328348 现树为准。
2. 悬空/错引条款：M2b-H-02 引「宪章 §13.5」——旧宪章无 §13.5（悬空节号）；M4-H-01 引「宪章 §7.3（显式错误禁止静默）」——旧宪章 §7.3 实为 Phase3 投影架构。
3. 路径漂移：V2-N-06 的 p1star_mad_check.cpp 实在 lib/star_detector/tests/p1star/；V2-N-07(b) 的 ipv_wcs.cpp 实在 lib/plate_solve/cpp/ipv/src/。
4. V2-N-07 部分消解：(b) 的失败分支现已 result->success=false 并 return（:345），本轮不可复现；(a) 文档锚失真与 (c) 常数自述两半仍成立，故整体判 OPEN。
5. M8-H-001：账本 verified_state=NOT_A_DEFECT，与本轮读码（共享栈对象上非原子 ++/+=，omp parallel 内以 &trace 传入）不一致，已按证据判 OPEN 并留待复核。
6. M2a-H-1/M3b-H-01/M9-H-1/M9-H-2/V5-N-03 的修复非本轮所为，仅复跑取证；5 条 P0 均给了单行命令 + 逐字输出。
7. grep -c 无命中时输出 0 且退出码 1（M9-H-4、M2a-H-3、M7-H-101 第二/三段），非执行失败；相关行已在 PSV 备注注明。
8. 未改任何代码/文档/测试/CI，未触碰仓库根条目与 FATDUCK_ACCESS.md。
