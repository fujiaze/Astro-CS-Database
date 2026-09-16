# 分片 F_TEST_GAP_P2 — ROOT-004 旧 bug 清单订正（F_TEST_GAP · P2 · 17 条）

- 分片名：F_TEST_GAP_P2
- 行数：17 条 finding；PSV 共 18 行（1 表头 + 17 行），每条 finding 恰好一行、10 列
- 四态计数：OPEN 17 ｜ RESOLVED 0 ｜ VOID 0 ｜ UNVERIFIABLE 0
- 归属分布：QA-001 3、CPU-001 3、P3-002 2、P2-001 2、P2-002 1、CLI-003 1、CI-001 1、AIO-001 1、DOC-001 1、OBS-001 1、RT-001 1
- GAP 关系：17 条全部「无」（已与 工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md 的 GAP-001..GAP-025 逐条比对，无重复项）
- 取证基线：实测 HEAD=main=2c328348；17 条证据均在本轮当前树重跑，命令与逐字输出见 PSV 第 6 列

## ID 覆盖自证
命令：
```
python3 -c "a=[l.split(chr(9))[0] for l in open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/_assign/F_TEST_GAP_P2.tsv',encoding='utf-8').read().splitlines()[1:] if l.strip()]; p=[l.split('|')[0] for l in open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/F_TEST_GAP_P2.psv',encoding='utf-8').read().splitlines()[1:] if l.strip()]; print('assign_n',len(a),'psv_n',len(p)); print('missing',[x for x in a if x not in p]); print('extra',[x for x in p if x not in a]); print('order_equal',a==p)"
```
输出：
```
assign_n 17 psv_n 17
missing []
extra []
order_equal True
```
命令：`wc -l reports/PROJECT-GOVERNANCE-01/root-scan/shards/F_TEST_GAP_P2.psv`
输出：`18 reports/PROJECT-GOVERNANCE-01/root-scan/shards/F_TEST_GAP_P2.psv`
（列数自证：17 行 split("|") 后均为 10 字段，表头逐字等于 SHARD_BRIEF §3 规定串）

## 异常
1. 基线漂移：简报称 HEAD=main=ecf6ad6f，实测 2c328348（其后两个 governance 提交 5f891080、2c328348）；本轮一律按实测树取证。
2. 锚点路径错误（M2a-F-5）：原 finding 写 tests/synth/test_drizzle_oracle.py，git 全历史无该路径（git log --all --diff-filter=A 只见 tests/backend/test_drizzle_oracle.py），实体在 tests/backend/ 同内容；球面链零命中故结论不变。
3. 行号漂移：M5b-F-02 :230-233 → :242-243（类 :245）；M2a-F-6 断言落在 :284（原 :280-287 段一致）；V15-N-13 :234-250 → :249-253、:281-284 → :323-324；M8-F-015 五处位置逐字一致。
4. 集合数字订正（本轮重算口径）：M8a-F-001 版本串 14 命中 / 10 源文件（原记 13 / 11，另有 1 个 .pyc）；V15-N-10 同族 reason+find+npos 共 44 行（tests/**/*.cpp + lib/backend_host/*.cpp，原记 45 处）；M8-F-015 CHECK(true) 5 文件 5 处、CHECK(0 == 0) 0 处（与原判一致）。
5. 证据部分削弱（仍判 OPEN）：V15-N-13 现树 :260-262 新增 assertNotIn(cpu_p50_low/cpu_mean_low/compute_io_mem_all_low)，原证据「含 cpu_p50_low 本身故该条不判失败」已不成立；弱半边仍在（verdict 非等值 ok、:323-324 产物仅断 os.path.isfile）。
6. 数量扩大：W1-N-12 原记双实现，实测 3 处同式重推导（drizzle_engine.cpp:684、module_adapters.cpp:3146、orchestrator.cpp:188），仍无单一承载层。
7. 未触碰：只读访问 lib/tests/ci/docs 等取证对象；仓库根条目、FATDUCK_ACCESS.md 未读；零修复、零 git 写。
8. 日志：run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/F_TEST_GAP_P2.log

## UNVERIFIABLE 清单
无（0 条）。17 条均在当前树取到可复跑证据，无「缺构建/缺 Windows 节点/缺真实数据/缺外网原文/缺负责人裁决/缺 23 模块映射表」类阻塞。
