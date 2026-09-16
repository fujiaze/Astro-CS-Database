# D_COMMENT_P2 分片报告（ROOT-004 旧 bug 清单按最新权威订正）

- 分片名：D_COMMENT_P2（类别 D_COMMENT × 优先级 P2，分配 35 条：L28b-D-004 .. W1-N-13）
- 产物：reports/PROJECT-GOVERNANCE-01/root-scan/shards/D_COMMENT_P2.psv（表头 1 行 + 数据 35 行，10 列；列内无 | 无换行）
- 结论计数：OPEN 35 / RESOLVED 0 / VOID 0 / UNVERIFIABLE 0
- 判定口径：统一对照最新权威链（ASTROCS_DESIGN §0）；注释类一律以 ENGINEERING_SPEC §2（注释白名单 + 禁历史版本号/任务编号/审计流水/代码复述）承接旧宪章 §12.2，引用完整性以 ENGINEERING_SPEC §8 承接旧 §12.3-9；集合类数字全部本轮重算
- 归属计数：P1-001×17、P1-002×3、P2-001×2、P3-001×1、P3-002×2、CPU-001×1、DATA-001×1、DOC-001×2、OBS-001×1、RT-001×1、NEXT-PACK:NP-DC1×4
- NP-DC1 = 本片建议新增候选包「注释卫生 §2 一次性瘦身 + 机器门」（不属现有 30 任务）

## ID 覆盖自证（命令 + 输出）
- 命令：diff <(awk -F'|' 'NR>1{print $1}' reports/PROJECT-GOVERNANCE-01/root-scan/shards/D_COMMENT_P2.psv) <(awk -F'\t' 'NR>1{print $1}' reports/PROJECT-GOVERNANCE-01/root-scan/shards/_assign/D_COMMENT_P2.tsv)
- 输出：空 diff → IDS_IDENTICAL（35 个 ID 与分配表第 2..36 行逐字同序）
- 命令：wc -l D_COMMENT_P2.psv → 输出：36；命令：awk -F'|' '{print NF}' D_COMMENT_P2.psv | sort | uniq -c → 输出：36 行全部 10 列

## 异常
1. 基线漂移：简报写 HEAD=main=ecf6ad6f；实测 git rev-parse HEAD=2c328348304d033aecfa81faf79d1c6cd802b30a（main 同值，末次提交「docs(governance): 登记 GAP-024/025 与 CFG-001 裁决；消除 ROOT 卡二义」）。本片一律按当前树取证，零修复、零 git 写。
2. 行号漂移（finding 锚点 → 当前树，本片实测）：L28b-D-004 :369→370；M2a-D-4 module_entry.c :778-790→838-845；M6a-D-008 module_adapters.cpp :2142→2613；M6a-D-013 p3_resample.cpp :251→404；M3-D-006 star_matcher.cpp/pc_api.cpp 多处已变。
3. 子项失效或不可复现（仅影响子项，不改变 OPEN）：M6a-D-013「pix2ang 结果只被 (void) 压制」不成立（:331 结果在 add_pt 内当次消费，仅余 (void)y1）；M3-D-006 原报 pc_api.cpp:162「MAD 3σ 截断」文本本轮未复现；M1a-D-002 第④项 x00 日志单位未独立复现。
4. 集合类数字重算与原报不同：M6a-D-009 七目录命中 lib/phase2=103、lib/phase3_session=40、runtime=35、cli=64、lib/hips=14、providers=18、modules=0（原报 67/39/43/63/27/18/0；差异源于本轮正则不含 commit 短哈希分支且加 --include 限定）；L28b-D-004 空括号模式本轮命中 6 处，仅 1 处为溯源空壳（原报 5 处）。
5. 证据位于 run/（gitignored，仓内不可复核）：W1-N-08 的 run/perf-fix/P1-gaia/evidence/shard_magranges.tsv 本轮按盘上文件用 python 重算（36 片合并 high=[13.62,25.59]）。
6. 未 build/ctest（本任务只判定不修复），故无构建证据；全程未 read/打印 FATDUCK_ACCESS.md；命令均带 timeout 并排除 run/build/out/artifacts/evidence/reports/graph/worktrees/Testing/logs/third_party。

## UNVERIFIABLE 清单
（空）35 条全部取得当前树可复跑证据。

## 最重要 3 条 OPEN（供主控优先处置）
- M6a-D-007：同一公共头 SNR_QF_PSF_OK 注「status==0 或 3 有效」，而生产 SNR 通道一律 status!=0 剔除（snr_estimator.cpp:183/352/611/736/845），两套口径互斥且无适用面说明。
- M2a-D-1：HEALPIX_SCALE_PER_NSIDE_ARCSEC 注释与 ALG 常数表同写 ≈211034.6，python 复算为 211076.28514206142；同函数极区 delta 系数 1.25 与 1.15 两套并存。
- M6a-D-014：sampler.cpp:683 注「禁全局 critical(aio_read)」，同文件 :689-690 实现就是全局 mutex（CON-010 复归）；该锁删/留直接决定 cfitsio 并发读稳定性。
