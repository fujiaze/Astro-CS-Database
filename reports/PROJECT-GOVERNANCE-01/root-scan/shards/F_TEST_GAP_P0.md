# 分片 F_TEST_GAP_P0（ROOT-004 分片执行层）

- 分片名：F_TEST_GAP_P0（原类别 F_TEST_GAP，原优先级 P0，分配 15 条：FD-F-003 .. M9-F-1）
- 产物：F_TEST_GAP_P0.psv ＝ 表头 1 行 + 15 条 = 16 行；逐行 10 列解析通过
- 四态计数：OPEN 12 / RESOLVED 3 / VOID 0 / UNVERIFIABLE 0
- 命令日志：run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/F_TEST_GAP_P0.log
- 判定基线：执行时点 HEAD=2c328348304d033aecfa81faf79d1c6cd802b30a（brief 基线 ecf6ad6f 为其祖先，树已前移，见异常 1）

## ID 覆盖自证（命令 + 逐字输出）

命令：timeout 60 python3 -c "a=[l.split(chr(9))[0] for l in open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/_assign/F_TEST_GAP_P0.tsv',encoding='utf-8').read().splitlines()[1:] if l.strip()]；p=[l.split('|')[0] for l in open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/F_TEST_GAP_P0.psv',encoding='utf-8').read().splitlines()[1:] if l.strip()]；print('assign=',len(a),'psv=',len(p),'order_identical=',a==p)；print('missing=',[x for x in a if x not in p],'extra=',[x for x in p if x not in a])"

逐字输出：
assign= 15 psv= 15 order_identical= True
missing= [] extra= []

PSV 结构自检同批输出：lines= 16 / header_ok= True / bad_col_rows= [] / ids 顺序＝分配表顺序。
（注：上面"命令"行内的分号在实跑时为换行，PSV 列内禁换行故此处折为单行；两行 print 逐字一致。）

## RESOLVED 3 条的证据要点
- M8-F-001：tests/abi 四个脚本已包装 TestCase（断言 main()==0），同一 discover 命令采集 22 例（原 0）；ci/validate_registry.py R11 已加"采集用例数>0"判定。
- M8-F-002：io_ownership_test.cpp 的 main 现按 failures 返回 0/1（:131-137），并有 --inject-failure 子进程回归锁（:80-83/:123-128）。
- M8-F-003：ci/steps/linux_build_root_graph.sh:33 产出根 build/libastrocs_runtime.so，缺前置产物在 CI 下 raise AssertionError(:37-39)，install 失败改 self.fail。

## UNVERIFIABLE 清单
无（0 条）。

## 异常
1. 基线前移：brief 写 HEAD=main=ecf6ad6f，实测 HEAD=2c328348（ecf6ad6f 为其祖先）；本片一律按执行时点当前树取证。
2. 锚点漂移：FD-F-003 的 ci_windows_driver.py 与 deep_ci_driver.py 现位于 tools/quality/（git ls-files 可证），仓库根与 ci/ 下均无，行号亦漂移。
3. 旧条款编号：旧宪章 §17.1/§17.10/§17.11/§12.3-4/-12 系列表项编号；现行 ASTROCS_PROJECT_CONSTITUTION.md 的 §17 与 §12.3 无子节标题，第 4 列照原 finding 口径抄录，未改其编号。
4. 部分条目在本轮前已被并发整改，本片按当前树改判并保留残项：M3b-F-01（验收门已改挂生产路径 0.3px 但仍零 CI 登记、证据内生产门=false）；M4-F-01（(a) 类已入图并被采集，残余 (b) 类 MC/kcorr 仍孤儿）；M3b-F-02（TRACEABILITY.csv:65 失真行随该 CSV 重构消失，其余残项仍在）。
5. 过期构建产物：run/scif3001/build/tests/unit/io_ownership_test 为修复前旧二进制（--inject-failure 仍 rc=0），不作为当前树证据；M8-F-002 以真源代码为准，并已在 PSV 备注标注。
6. 无 VOID：本片 15 条原判据均为现行权威链上条款（旧宪章条目经 §2 映射表可落位），未见"旧路径废止且新设计不要求"的情形。
