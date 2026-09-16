# 分片 A_SCI_DEF_P2（ROOT-004 旧 bug 清单按最新权威订正）

- 分片名：A_SCI_DEF_P2（类别 A_SCI_DEF，优先级 P2）｜分配 36 条（M3-A-007 … W2-N-15）
- 产物：reports/PROJECT-GOVERNANCE-01/root-scan/shards/A_SCI_DEF_P2.psv（表头 1 行 + 36 行，10 列 PSV）
- 四态计数：OPEN 32｜RESOLVED 2（M7-A-204、V12-N-15）｜VOID 1（V12-N-14）｜UNVERIFIABLE 1（V12-N-17）
- 判定方式：全部条目本轮在当前树重新取证（grep/sed/python3/git 只读），零修复、零 git 写；未读 FATDUCK_ACCESS.md。

## ID 覆盖自证（命令 + 输出）

    $ cd reports/PROJECT-GOVERNANCE-01/root-scan/shards
    $ wc -l A_SCI_DEF_P2.psv
    37 A_SCI_DEF_P2.psv
    $ tail -n +2 _assign/A_SCI_DEF_P2.tsv | wc -l
    36
    $ diff <(cut -f1 _assign/A_SCI_DEF_P2.tsv | tail -n +2) <(cut -d'|' -f1 A_SCI_DEF_P2.psv | tail -n +2) && echo IDENTICAL_36_IDS
    IDENTICAL_36_IDS
    $ cut -d'|' -f7 A_SCI_DEF_P2.psv | tail -n +2 | sort | uniq -c
         32 OPEN / 2 RESOLVED / 1 UNVERIFIABLE / 1 VOID
    $ python3 -c "…按 | 计数校验…"  → rows with wrong pipe count: []（每行恰好 9 个 "|"，10 列，列内无 "|"、无换行）

## UNVERIFIABLE 清单

- V12-N-17（光度学零点常数四套 + 两处兜底）：宿主指针「V12.md §2.17」不存在（p1/V12.md 该小节只有 V12-N-01/03/05/06），22.5/20.6/20.48 在 lib/docs/cli/tools 无零点语境命中。缺宿主 ⇒ 无法定性；须 V12 原轴补交宿主后重派（问题扫描/_recheck/RC2_g2.md:65 复核结论一致）。

## 异常（锚漂移 / 原文条款缺失 / 基线）

1. 基线漂移：任务书写 HEAD=main=ecf6ad6f，实测 HEAD=main=2c328348、origin/main=5f891080；ecf6ad6f..HEAD 两提交仅改 工程控制/**，未触及 lib/docs/tests/tools，故现行树取证有效（GAP-030 已登记同类现象）。
2. 路径改名（已按当前树重定位）：tools/monitoring/run_provider_oracle_checks.py → tests/cpu/baseline/（M8-A-001）；app/browser_cli.cpp → lib/healpix_db/healpix_browser_qt/app/，M9-A-3 消费面重锚 core/browser_backend.cpp:387。
3. 引用文档已废止/不存在：docs/algorithms/PSF_FITTING_VARIANTS.md（V12-N-14）、docs/algorithms/PHASE1_HIPS_DRIZZLE.md（V12-N-15）；docs/architecture/PHASE3_MODULE_ARCH.md 仍在但全文无 M6a-A-001 所引 order/叶数条款（grep 0 命中）。
4. 原判据文本在现树不存在：M7-A-204 的「§10 声明 scale=1.0」（PHOTOMETRY.md grep 0 命中、git log -S 0 提交）；M7-A-206 的「§11 含 flux 相对误差」（PSF.md 0 命中，现门为 :98 max_abs==0 解析自洽）。
5. V12-N-16 原描述两腿（sqrt(10) 两处、MAG_PER_LN10 正例）三向 0 命中，本轮收窄到 kLn10 双写（snr_science.cpp:33 / noise_model.cpp:34，位数不一致）。
6. V12-N-14 台账 verified_state=REJECT-NEVER-EXISTED；本轮独立复核一致（kMoffatBeta 与 2.5531628 在 HEAD 零载体），按 VOID 记。
7. 本分片未构建、未跑 CI（判定对象为文档文本/常量定义，静态取证足够）；M8-A-001 的判定器不在任何注册执行面，其死分支为静态判定。
