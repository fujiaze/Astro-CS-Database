# ROOT-001 仓库根全条目账本（ROOT_LEDGER）

任务：`ROOT-001 仓库根清洁：全条目账本、处置分类与执行`　执行者：执行 Agent（零 git 写权限）

生成时间：2026-09-16　基线：`HEAD = main = ecf6ad6fe55102c08a156564cc0dd30cea3a7cdc`（`origin/main = f96dff61e6ae9cc052a925713f9864f5d6bac3bb`，三 SHA 不一致见 GAP_AUDIT §5.1 / U-08）

判定口径：**物理根目录干净**（不是 `git status` 干净）。权威：ENGINEERING_SPEC §7（根固定条目白名单 + 禁止散落根目录）、ASTROCS_DESIGN §6.3（运行产物只落 `output_dir`，不得以进程 CWD 作隐式缺省）。

分类定义：**A** 一次性运行产物→删；**B** 构建/测试缓存→删；**C** 有价值历史资料→归档；**D** 用户数据→原地保留登记；**E** 目标结构内合法条目→保留；**F** 无法确证或本任务无权处置→**绝不删除**，登记待裁决（F1 = 来源/用途无法确证；F2 = 已确证但处置权在 GOV-001/DOC-001/ARCH-001 等）。

机器可读副本：`reports/PROJECT-GOVERNANCE-01/root/ROOT_LEDGER.tsv`、`reports/PROJECT-GOVERNANCE-01/root/ledger-stats.json`；原始盘点：`run/PROJECT-GOVERNANCE-01/ROOT-001/logs/root-inventory2.txt`；前后对照：`run/PROJECT-GOVERNANCE-01/ROOT-001/logs/root-before.txt` / `root-after.txt`。

## 1. 计数断言

| 项 | 值 |
|---|---|
| 执行前顶层条目 | **133**（`ls -A . | wc -l`） |
| 账本条目数 | **133**（= 执行前 133，无遗漏） |
| A 类 | 68 |
| B 类 | 4 |
| C 类 | 0 |
| D 类 | 4 |
| E 类 | 34 |
| F 类 | 23 |
| 待删条目（A+B） | **72** |
| 待删总字节（表观） | **6,365,080,946** B（6.365 GB） |

## 2. A/B 清运清单（先写入本账本，后执行）

> 每项含 路径 + 指纹 + 判定依据，满足「删除动作可追溯」。指纹：文件 = sha256；目录 = 文件数 + 总量（目录内小文件清单见 §7）。

| # | 路径 | 类 | 字节 | 文件数 | 最后修改 | 指纹 | 依据 |
|---|---|---|---|---|---|---|---|
| 1 | `alloc_report.json` | A | 819 | 1 | 2026-09-16 02:54:18 | `064e5511fc6062c292a80fc42fd045b23dd217c3de4c63bf4e0b7786064ceaf1` | MON-002/资源监控一次性运行产物（CWD 落盘）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷；ci/ci_repair_round.py:118 明文点名 alloc_report.json/alloc_samples.csv 为 CWD 误落 |
| 2 | `alloc_samples.csv` | A | 151 | 1 | 2026-09-16 02:54:18 | `30a82c5d24d77a30104d9976ed70746f959c9180575d1ab26bdc9fa0ac389185` | MON-002/资源监控一次性运行产物（CWD 落盘）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷；ci/ci_repair_round.py:118 明文点名 alloc_report.json/alloc_samples.csv 为 CWD 误落 |
| 3 | `astrocs_p1sess_neg` | A | 5,280 | 1 | 2026-09-12T17:16:33 | `files=1, sum=5,280 B` | p1sess 测试在 CWD 落盘的测试输出目录（lib/phase1_session/tests/p1sess/p1sess_tests_{units,perf,negative,properties}.cpp 以 tmp_root=CWD 建同名目录）；全部 untracked，重跑测试即再生 |
| 4 | `astrocs_p1sess_perf` | A | 134,400 | 16 | 2026-09-12T17:16:33 | `files=16, sum=134,400 B` | p1sess 测试在 CWD 落盘的测试输出目录（lib/phase1_session/tests/p1sess/p1sess_tests_{units,perf,negative,properties}.cpp 以 tmp_root=CWD 建同名目录）；全部 untracked，重跑测试即再生 |
| 5 | `astrocs_p1sess_props` | A | 37,920 | 7 | 2026-09-12T17:16:33 | `files=7, sum=37,920 B` | p1sess 测试在 CWD 落盘的测试输出目录（lib/phase1_session/tests/p1sess/p1sess_tests_{units,perf,negative,properties}.cpp 以 tmp_root=CWD 建同名目录）；全部 untracked，重跑测试即再生 |
| 6 | `astrocs_p1sess_test` | A | 55,288 | 12 | 2026-09-12T17:16:33 | `files=12, sum=55,288 B` | p1sess 测试在 CWD 落盘的测试输出目录（lib/phase1_session/tests/p1sess/p1sess_tests_{units,perf,negative,properties}.cpp 以 tmp_root=CWD 建同名目录）；全部 untracked，重跑测试即再生 |
| 7 | `astrocs_run_37bc24d27490.json` | A | 594 | 1 | 2026-09-12 18:49:33 | `0c4bea311a2c2f30a3067df7643d37e43d7ce969c7d0b4d8296e82a8e2767ae5` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 8 | `astrocs_run_37e2e0b1a467.json` | A | 600 | 1 | 2026-09-12 18:52:20 | `bf8be805d6503187307b13f093801ccd6d1dd7c5da2d347d3d3fb04e072f1487` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 9 | `astrocs_run_37e30359908d.json` | A | 610 | 1 | 2026-09-12 18:52:20 | `d70ffcdfc94c4cee57128e3313f0637a9bc31cfc779823761e6e7a2df69ab71c` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 10 | `astrocs_run_37e3658244f9.json` | A | 595 | 1 | 2026-09-12 18:52:22 | `b45f0b2763c5a37a4fb52ff6830e25536be8b3839e34abae379ca9799c4fa312` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 11 | `astrocs_run_747dd4685289.json` | A | 750 | 1 | 2026-09-13 13:22:56 | `e8b389bd68dd3a20ce6f1460a9ff921b530579d8b17a2e520905f7c68cccfb6d` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 12 | `astrocs_run_7710c5d75718.json` | A | 612 | 1 | 2026-09-13 14:10:06 | `34f263f09d1c4f498ba4bcfa4dcad19a316dd9102b67a530f12c3edb0b480c80` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 13 | `astrocs_run_7727be85ca3b.json` | A | 600 | 1 | 2026-09-13 14:11:45 | `54e61ca97072c58720aa51a64a32988271fb1f54c2897199397fa5f78adec62a` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 14 | `astrocs_run_773fc5abbdb2.json` | A | 616 | 1 | 2026-09-13 14:13:28 | `b4362ead22d03c06739d96e18783db3a479a099dd3519d7eeb744b140ca10a8b` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 15 | `astrocs_run_7764cfc42284.json` | A | 1,059 | 1 | 2026-09-13 14:16:07 | `c667aec1e44050483eb8af3be71046feec8bcbc106d83c0f1aa89b7e98f74cd6` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 16 | `astrocs_run_7764edd68939.json` | A | 632 | 1 | 2026-09-13 14:16:07 | `3616a374cf3f51448328c815dbb1bc21586a7c0f53080fa95278e30225157977` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 17 | `astrocs_run_776551ed3162.json` | A | 617 | 1 | 2026-09-13 14:16:09 | `acbccec57f0bf042f11509649477eb829dc2cfc3cfe41dcc9290014d9f21f652` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 18 | `astrocs_run_7775fc748ba0.json` | A | 623 | 1 | 2026-09-13 14:17:21 | `1129229b9ab122e0d58d3b7e0e8785f7737d2cae45959a8d2733c3bd758885af` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 19 | `astrocs_run_777ed0a3fc79.json` | A | 612 | 1 | 2026-09-13 14:17:59 | `96c75e07a9b211f0e44e2e1c8f6e41e20fe5cc86fb6b77d880ac8239c190d3cf` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 20 | `astrocs_run_778a3f855cbc.json` | A | 612 | 1 | 2026-09-13 14:18:48 | `5a4997713ec214b3208ccf6e5ddac04744407b445397f614b41b49eaa91a68ea` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 21 | `astrocs_run_77aa2e202ef7.json` | A | 616 | 1 | 2026-09-13 14:21:05 | `7578220b6f4d9aa2035d43980c9af0aef8569487a541f6c9ce6818afac754cb1` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 22 | `astrocs_run_77cf2b3ad159.json` | A | 622 | 1 | 2026-09-13 14:23:44 | `1b913cf2f13e1363a38dce992a9650d2fb15a47ad892d9e9c6ae1a0cf25d9f10` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 23 | `astrocs_run_77cf4df6054b.json` | A | 632 | 1 | 2026-09-13 14:23:44 | `88889942791dad051d5c2204ccb76b92804289b72f916c2b712ae74bebfbc2eb` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 24 | `astrocs_run_77cfb00d72f5.json` | A | 617 | 1 | 2026-09-13 14:23:46 | `ffc1dce92e0eff9c15535ce9dea34399f7f4689c28b602c28855c6ba5bfbab7c` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 25 | `astrocs_run_77e065d9a558.json` | A | 623 | 1 | 2026-09-13 14:24:58 | `dd066d883fb2398fa96b97327cf7ae380f6a3edcf6036535092557bdd981b2c5` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 26 | `astrocs_run_77e9148413a2.json` | A | 612 | 1 | 2026-09-13 14:25:35 | `5e6131d71928d043ae46e332d66a56e963a1c5f5a9b7823e1d9324717de68733` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 27 | `astrocs_run_77f43b9efd54.json` | A | 612 | 1 | 2026-09-13 14:26:23 | `fc1714904e88d824bfbf123998d6199d31b440ae78e0bffa50e5f1a39ffe65b2` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 28 | `astrocs_run_78ca3c837398.json` | A | 683 | 1 | 2026-09-13 14:41:42 | `3b8c120d5bf275fcb50d9d4a1418941e03fe6b62ba9e1fb2d2ff5e56cad35e8c` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 29 | `astrocs_run_78ca5b10633d.json` | A | 632 | 1 | 2026-09-13 14:41:43 | `e1ec0a1af74b75fcf69e73ce1d55de4ed5631d97678cc86d72a8c70daad38c14` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 30 | `astrocs_run_7a0e52cce96d.json` | A | 683 | 1 | 2026-09-13 15:04:54 | `33b295e2491204d69908ae6d9e66b5beb4d4eeef93af10a6c3d0c3b080099a01` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 31 | `astrocs_run_7a0e716c3e34.json` | A | 632 | 1 | 2026-09-13 15:04:55 | `25d782f6c5e484b111b0bf32370a282c7b8474610c12e943b62248cc881d4a24` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 32 | `astrocs_run_7aa043edacca.json` | A | 683 | 1 | 2026-09-13 15:15:21 | `d6a32721f5939f0a444ebd2ba03599b3ea22063e7087648060c780100d52e875` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 33 | `astrocs_run_7aa06280a22e.json` | A | 632 | 1 | 2026-09-13 15:15:21 | `3e8a7569b2939647661c13e195fad138400c383ad668f38caf5b10e7d59f08fa` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 34 | `astrocs_run_7b9b6d815bea.json` | A | 683 | 1 | 2026-09-13 15:33:20 | `3502637e6c736eab2d68b9eb6e6543266aed9bc282f1ee865f7dc6aeb8950c5e` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 35 | `astrocs_run_7b9b8c00f246.json` | A | 632 | 1 | 2026-09-13 15:33:20 | `b82ddbf82e20dd59b6628c323c04af199f9a56c8ad7c5e12ffdbc88064e78bc9` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 36 | `astrocs_run_7c2d25c2a8c7.json` | A | 661 | 1 | 2026-09-13 15:43:46 | `1652058bc9c0c2e87f184b1724321302c4b78152485fa833ea0c52e9c2bfa927` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 37 | `astrocs_run_7c2d4452f907.json` | A | 610 | 1 | 2026-09-13 15:43:46 | `d411b274331c32489577396b7814526dc5113645047be71949fd5929730ce6f9` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 38 | `astrocs_run_7c92e964454a.json` | A | 594 | 1 | 2026-09-13 15:51:03 | `7c7653cc6b230720068508030e707741805192ab67fb53c41b0f2c52aa0dc91d` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 39 | `astrocs_run_7cb926de8cb6.json` | A | 600 | 1 | 2026-09-13 15:53:47 | `a5c37280980ddc58538400ef182b42849fbc132fbd31807ec9ffc39cef96a471` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 40 | `astrocs_run_7cb94a8107ba.json` | A | 610 | 1 | 2026-09-13 15:53:47 | `c96adf974e00d35429280b097cfa51640753d42b1f623e246ecf2cf02ce15e67` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 41 | `astrocs_run_7cb9ae372be0.json` | A | 595 | 1 | 2026-09-13 15:53:49 | `4a4fb6f0df0038824d95d6e05c9de8f51bfa10cf7930e9b4947258e010f95799` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 42 | `astrocs_run_7cd992a70cb6.json` | A | 600 | 1 | 2026-09-13 15:56:07 | `c90fc95f08fad221ccee6ae33f20bce49c99d0fc83af5a01b16f134470d94cbc` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 43 | `astrocs_run_7ce3113498e3.json` | A | 590 | 1 | 2026-09-13 15:56:47 | `9535cd119bf7dc5368d3ce7d32f81fce1b5537130df62452b047366016df54d7` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 44 | `astrocs_run_7cf141fb3858.json` | A | 590 | 1 | 2026-09-13 15:57:48 | `6f5f9896d78d42d11dfd7e8ea94e80f789e070b548ee9a26e02eb556c2060a32` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 45 | `astrocs_run_7d268fd82da5.json` | A | 594 | 1 | 2026-09-13 16:01:37 | `4d1dcb1162b37c0330f9275ae8fd40a616857b54bfbab700df661885a0576539` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 46 | `astrocs_run_7d4cd130056e.json` | A | 600 | 1 | 2026-09-13 16:04:21 | `2f4fd8aec87e2d5a7139eb67f7a3e6efc26bb69387c627574baf360c8dc07e14` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 47 | `astrocs_run_7d4cf3fb7d58.json` | A | 610 | 1 | 2026-09-13 16:04:22 | `d4852ceb597634d9c9db7cba528bacdcc6dead9829cc87c0443fc292e08c534c` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 48 | `astrocs_run_7d4d5a19bdd9.json` | A | 595 | 1 | 2026-09-13 16:04:23 | `c071de9c3f7f67cf55700c98fc1c863067ea0149cf3a638486bcee5a56e086f5` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 49 | `astrocs_run_7d868d76fc59.json` | A | 590 | 1 | 2026-09-13 16:08:29 | `368c2864ca21b90134c08a8696b533b4bd54a9295d141c1c9e80ead02fe65bfe` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 50 | `astrocs_run_7dbd576dd925.json` | A | 594 | 1 | 2026-09-13 16:12:24 | `b0c486b4b162ca96c9c02a23b93008dfe055fd16c1afb40aa706f2684b2e8169` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 51 | `astrocs_run_7de3c1dd4ebc.json` | A | 600 | 1 | 2026-09-13 16:15:09 | `f996162eaecd7823537474eb49fd18a8ed8242d2228b655bba5ad13d33080bdc` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 52 | `astrocs_run_7de3e6278900.json` | A | 610 | 1 | 2026-09-13 16:15:10 | `c13b35034fa0a56e4dd212ea72716cede19898fe264f448bdd44b6e1538c5811` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 53 | `astrocs_run_7de4477e6a0c.json` | A | 595 | 1 | 2026-09-13 16:15:12 | `1825baf73599ef989e1293348c2c02cf4c12fce76037f5c31aa0b66b2b580c6c` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 54 | `astrocs_run_7e0290c68a05.json` | A | 600 | 1 | 2026-09-13 16:17:22 | `5289a6715d09b0c019523b3c9d6fdd3e0da3e3397bc65cc37e84f0784cadff68` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 55 | `astrocs_run_7e0b6c51dacc.json` | A | 590 | 1 | 2026-09-13 16:18:00 | `746134e0d532a479e1a3bfbfd8b07d1fa4802ba73f998f5ad122552c1d357180` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 56 | `astrocs_run_7e1909b8a2bf.json` | A | 590 | 1 | 2026-09-13 16:18:58 | `afc5a5f8bc807b974d485446e07f7a8e16d43b63a882b5aa0dea599257a6ea3f` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 57 | `astrocs_run_7e65e9789939.json` | A | 683 | 1 | 2026-09-13 16:24:28 | `c3dab58c85cdb16656738af7e1b2388d0d093fe81ac20e2e20df6730e1e831ba` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 58 | `astrocs_run_7e6607ca717c.json` | A | 632 | 1 | 2026-09-13 16:24:29 | `6a540ad1bf9d61a70981cea62a2ba595528cb3145ee7bc1f75e9df497b021a40` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 59 | `astrocs_run_9154bb000abc.json` | A | 1,115 | 1 | 2026-09-13 22:11:25 | `6ac7fb49ad29dd2150d876ed8ff75bb66a6fa4a0d4bd762f2a4cfbed46c71f22` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 60 | `astrocs_run_9154d913321c.json` | A | 610 | 1 | 2026-09-13 22:11:26 | `3dbb5157e3b764c237c7bb0a3d541b85b542242fe914ff0a8373f8bc125e7b28` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 61 | `astrocs_run_a92907fa9737.json` | A | 1,115 | 1 | 2026-09-14 05:28:06 | `d40e8f32ef636716616733952ad9ea5c9c10a0688a328bf6775596bb5de418e8` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 62 | `astrocs_run_a929266a85f1.json` | A | 610 | 1 | 2026-09-14 05:28:06 | `f53967f62b196dccde4cb24797dc078d045cbdf5e2d9e143bf840917ed934a2d` | 一次性运行清单（run manifest）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷 |
| 63 | `build` | B | 6,364,625,713 | 7080 | 2026-09-16T02:32:59 | `files=7080, sum=6,364,625,713 B` | CMake/ninja 构建树（可重建）。bin/obj 生成物；CMakeLists.txt + ci/steps/linux_build_root_graph.sh 可重建；GAP-022 列为构建/缓存残留 |
| 64 | `Database` | A | 48,146 | 26 | 2026-09-15T01:57:13 | `files=26, sum=48,146 B` | 未加引号路径 '/workspace/Astro CS Database/run/...' 被 shell 分词后误建；实测与 run/ 下同名文件 sha256 逐字节相同（fc99741a…），run/ 为唯一事实源 |
| 65 | `graph` | B | 5,459 | 9 | 2026-09-13T16:15:09 | `files=9, sum=5,459 B` | tools/graph 生成的运行图产物（graph/*.dot\|json\|svg）；.gitignore:/graph/ 已兜底；ci/ci_repair_round.py:118 点名 'astrocs graph 落 graph/*.json' 为 CWD 误落 |
| 66 | `out` | A | 661 | 1 | 2026-09-08T07:38:49 | `files=1, sum=661 B` | CLI 缺省 output_dir 兜底产物（含 1 个 astrocs_run_*.json）；ASTROCS_DESIGN §6.3 禁止以 CWD 作隐式缺省 |
| 67 | `.pytest_cache` | B | 128,974 | 6 | 2026-09-16T02:19:41 | `files=6, sum=128,974 B` | pytest 缓存（可重建）；tools/quality/ci_coverage_runner.py:130 明文要求禁止其落在工作区根 |
| 68 | `resource_samples.csv` | A | 351 | 1 | 2026-09-16 02:54:18 | `a82b434cdeeb21fcd95cc78816a7071740ce2f07dbbba6e226899d6d4d52d54c` | MON-002/资源监控一次性运行产物（CWD 落盘）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷；ci/ci_repair_round.py:118 明文点名 alloc_report.json/alloc_samples.csv 为 CWD 误落 |
| 69 | `resource_summary.json` | A | 1,237 | 1 | 2026-09-16 02:54:18 | `495d1f475e8f94caabe45faff5c157c6a1c0073d7cb57f3297c3a105dc0f6daf` | MON-002/资源监控一次性运行产物（CWD 落盘）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷；ci/ci_repair_round.py:118 明文点名 alloc_report.json/alloc_samples.csv 为 CWD 误落 |
| 70 | `run_context.json` | A | 231 | 1 | 2026-09-16 02:54:17 | `0ff783873e4eb640909f1f254eb26d7bb87db9dcb5348913e8147684d72cfd82` | CLI 一次性运行上下文（CWD 落盘）。生成=lib/core/src/module_adapters.cpp:6198-6214；消费方只读 output_dir 内副本（tests/unit/p3002_real_nodes_test.cpp:234 读 fx.out 下文件，非仓库根） |
| 71 | `Testing` | B | 125 | 2 | 2026-09-11T17:21:41 | `files=2, sum=125 B` | ctest 残留（Testing/Temporary）；ENGINEERING_SPEC §7 规定 ctest 残留归 run/Testing_archive/，本任务禁改 run/ 内部，故按 B 类删除并登记该口径差 |
| 72 | `worker_balance.csv` | A | 82 | 1 | 2026-09-16 02:54:18 | `d7a2b6d9113fadccc457f7f4d243308e8071438ea56f0e695bd3f95c3290d044` | MON-002/资源监控一次性运行产物（CWD 落盘）。cli/commands.cpp:446 out_dir/astrocs_run_<run_id>.json + :343 write_run_context；ASTROCS_DESIGN §6.3 只许落 output_dir；ci/ci_repair_round.py:118 明文认定「目标目录解析为 CWD(=仓库根)」为缺陷；ci/ci_repair_round.py:118 明文点名 alloc_report.json/alloc_samples.csv 为 CWD 误落 |

## 3. C 类归档

**本次 C 类 = 0 条**。理由：根级唯一具历史价值且非 tracked 的资料只有构建/修复日志 `logs/`（§7 白名单内 → E）与图形产物 `graph/`（可重生运行产物 → B）；其余有保留价值的资料（`设计大纲/`、`问题扫描/`、`evidence/`）均为 **tracked**，按任务规则只登记建议、由 GOV-001/DOC-001 处置，本任务不移动、不删除。`docs/archive/` 已存在（5 条目），本次无新增归档。

## 4. D 类用户数据（原地保留登记，不删不移）

| 路径 | 字节 | 文件数 | 最后修改 | 依据 |
|---|---|---|---|---|
| `BASS DR3` | 59,820,332 | 367 | 2026-08-21T21:43:26 | 用户数据区（.gitignore 已覆盖：BASS DR3/ :7、GaiaDR3/ :3、GaiaDR3SP/ :4）；硬规则禁止移动/删除 |
| `GaiaDR3` | 43,936,859,210 | 16 | 2026-09-09T16:38:27 | 用户数据区（.gitignore 已覆盖：BASS DR3/ :7、GaiaDR3/ :3、GaiaDR3SP/ :4）；硬规则禁止移动/删除 |
| `GaiaDR3SP` | 67,834,893,663 | 20 | 2026-09-09T16:41:24 | 用户数据区（.gitignore 已覆盖：BASS DR3/ :7、GaiaDR3/ :3、GaiaDR3SP/ :4）；硬规则禁止移动/删除 |
| `testdata` | 31,758,220,035 | 944 | 2026-09-11T21:34:17 | §7 白名单内且属数据区（30G，944 文件）；按任务硬规则只登记、不删不移 |

## 5. E 类（§7 白名单内，保留）

| 路径 | tracked 文件数 | 字节 | 依据 |
|---|---|---|---|
| `工程控制` | 36 | 203,031 | §7 白名单（控制包家；PROJECT-GOVERNANCE-01 已 tracked 36 文件） |
| `AGENTS.md` | 1 | 4,652 | §7 白名单 |
| `artifacts` | 268 | 76,304,189 | §7 白名单（证据 artifacts/） |
| `ASTROCS_DESIGN.md` | 1 | 26,073 | §7 白名单（最高权威） |
| `build.sh` | 1 | 2,011 | §7 白名单 |
| `ci` | 56 | 2,216,048 | §7 白名单 |
| `.clang-format` | 1 | 436 | §7 白名单 |
| `cmake` | 7 | 34,754 | §7 白名单 |
| `CMakeLists.txt` | 1 | 43,614 | §7 白名单（唯一根 CMake） |
| `CMakePresets.json` | 1 | 4,833 | §7 白名单 |
| `contracts` | 62 | 462,017 | §7 白名单 |
| `CONTROL_PACK_SPEC.md` | 1 | 8,195 | §7 白名单 |
| `DEPENDENCIES.md` | 1 | 4,783 | §7 白名单 |
| `docs` | 333 | 3,614,069 | §7 白名单 |
| `.editorconfig` | 1 | 264 | §7 白名单 |
| `engineering` | 0 | 0 | §7 白名单（当前为空目录，0 条目） |
| `ENGINEERING_SPEC.md` | 1 | 7,416 | §7 白名单 |
| `.git` | 0 | 162,184,393 | 版本库本体；硬规则禁止触碰 |
| `.gitattributes` | 1 | 409 | §7 白名单 |
| `.github` | 4 | 30,301 | §7 白名单（.github/） |
| `.gitignore` | 1 | 2,516 | §7 白名单 |
| `include` | 23 | 148,620 | §7 白名单 |
| `lib` | 1221 | 212,345,576 | §7 白名单（目标源码根，GAP-003 待 ARCH-001 迁移） |
| `logs` | 0 | 5,798 | §7 白名单（logs/（gitignore）） |
| `memory.md` | 1 | 15,373 | §7 白名单 |
| `packaging` | 17 | 62,137 | §7 白名单 |
| `README.md` | 1 | 8,831 | §7 白名单 |
| `reports` | 686 | 7,572,484 | §7 白名单 |
| `run` | 1 | 106,692,246,060 | §7 白名单（临时产物/日志；内部清运归 ROOT-003） |
| `scripts` | 2 | 4,087 | §7 白名单 |
| `tests` | 397 | 8,102,307 | §7 白名单 |
| `third_party` | 1 | 1,040,613 | §7 白名单 |
| `toolchain.ps1` | 1 | 9,334 | §7 白名单 |
| `tools` | 169 | 2,672,834 | §7 白名单 |

## 6. 全条目账本（133 条）

| # | 路径 | 类型 | tracked | 字节 | 文件数 | 最后修改 | 指纹（sha256 / 目录计数） | git 最近相关提交 | 类 | 处置 |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `.clang-format` | FILE | 是 | 436 | 1 | 2026-08-21 21:43:26 | `e87deb303a93078ba9d99b4a99b45cfd47eaf6e2b5eb8fe434d19c139a6f9358` | a0c92280 2026-08-13 18:34:01 +0800 | E | 保留 |
| 2 | `.editorconfig` | FILE | 是 | 264 | 1 | 2026-08-21 21:43:26 | `51db027592986ac39e1c498419429d1c8dba4c62b45ab5797d3b2b10687257c5` | a0c92280 2026-08-13 18:34:01 +0800 | E | 保留 |
| 3 | `.git` | DIR | 否 | 162,184,393 | 3331 | 2026-09-16T14:08:25 | `files=3331, sum=162,184,393B` | —（untracked） | E | 保留 |
| 4 | `.gitattributes` | FILE | 是 | 409 | 1 | 2026-08-21 21:43:26 | `dfee8a722364cd168c34fdcab6e34f35eb867f9881a920aab9bba82420f366b5` | d459bb4f 2026-07-24 17:30:57 +0800 | E | 保留 |
| 5 | `.github` | DIR | 否 | 30,301 | 4 | 2026-09-13T22:00:31 | `files=4, sum=30,301B` | c21eec50 2026-09-13 22:47:58 +0800 | E | 保留 |
| 6 | `.gitignore` | FILE | 是 | 2,516 | 1 | 2026-09-13 15:26:56 | `dbf78b5740a0837ab1fac8624bf81cf7b0edad81d54e6013626e20c9440bfac2` | 3848fc61 2026-09-13 16:30:21 +0800 | E | 保留 |
| 7 | `.pytest_cache` | DIR | 否 | 128,974 | 6 | 2026-09-16T02:19:41 | `files=6, sum=128,974B` | —（untracked） | B | 删除 |
| 8 | `AGENTS.md` | FILE | 是 | 4,652 | 1 | 2026-09-16 13:19:13 | `e171a67a7cb0b115608077a610f3e6c5cdbf464cea9959388626c22f443be6f2` | a861d8f6 2026-09-16 13:19:42 +0800 | E | 保留 |
| 9 | `ASTROCS_DESIGN.md` | FILE | 是 | 26,073 | 1 | 2026-09-16 13:19:13 | `d9345874ea822e4d0a62ad257ff10b893210db64b5c96fd4c3060e17a3202aff` | a861d8f6 2026-09-16 13:19:42 +0800 | E | 保留 |
| 10 | `ASTROCS_PROJECT_CONSTITUTION.md` | FILE | 是 | 39,903 | 1 | 2026-09-10 01:38:23 | `0f25544eeea26904c1836a7899ac0327bb7cc168bdb1db52aaf0dce47c3c66db` | d8c821db 2026-09-10 01:58:38 +0800 | F | 保留（登记建议，交由 GOV-001/DOC-001） |
| 11 | `AstroCS.wiki` | DIR | 否 | 0 | 0 | 2026-08-21T21:43:26 | `files=0, sum=0B` | —（untracked） | F | 保留（待负责人裁决） |
| 12 | `AstroCS_ENGINEERING_CONSTRAINTS.md` | FILE | 是 | 10,061 | 1 | 2026-09-10 01:40:24 | `e6547a0bdd77b26304e8a52fc23e916a7caf57db402c1b30d732d81ebdbbc4c7` | d8c821db 2026-09-10 01:58:38 +0800 | F | 保留（登记建议，交由 GOV-001/DOC-001） |
| 13 | `BASS DR3` | DIR | 否 | 59,820,332 | 367 | 2026-08-21T21:43:26 | `files=367, sum=59,820,332B` | —（untracked） | D | 原地保留登记 |
| 14 | `CHANGELOG.md` | FILE | 是 | 16,478 | 1 | 2026-09-16 03:55:57 | `fa15b1a510a303660213b6f6c45fc925aec2229e843685916215711e0e211deb` | 46ca7573 2026-09-16 03:57:13 +0800 | F | 保留（登记建议，交由 GOV-001/DOC-001） |
| 15 | `CMakeLists.txt` | FILE | 是 | 43,614 | 1 | 2026-09-16 02:25:25 | `4719432c072c025c64fcee652ca6b8937a7846a4cd15dbd878d4e14b29440826` | 3e7fbc44 2026-09-16 02:30:54 +0800 | E | 保留 |
| 16 | `CMakePresets.json` | FILE | 是 | 4,833 | 1 | 2026-09-06 21:24:43 | `230daadc21b6cfe3052baa1c6a10104d937a0cc8f7e99e88c7101c32adc72839` | 63721d70 2026-09-06 21:29:31 +0800 | E | 保留 |
| 17 | `CONTROL_PACK_SPEC.md` | FILE | 是 | 8,195 | 1 | 2026-09-16 13:19:13 | `f46e85834477ee8484ec0eaf2fde38a2a64a80717f4c48d3c12ccec64787765c` | a861d8f6 2026-09-16 13:19:42 +0800 | E | 保留 |
| 18 | `CS` | DIR | 否 | 0 | 0 | 2026-09-13T14:55:36 | `files=0, sum=0B` | —（untracked） | F | 保留（待负责人裁决） |
| 19 | `DEPENDENCIES.md` | FILE | 是 | 4,783 | 1 | 2026-09-03 03:41:44 | `d20d48a8a6131dc53a3a1e00ae78b5eb40b3100e4902914a6e62fe452a2e6663` | 64a2f996 2026-09-03 03:41:55 +0800 | E | 保留 |
| 20 | `Database` | DIR | 否 | 48,146 | 26 | 2026-09-15T01:57:13 | `files=26, sum=48,146B` | —（untracked） | A | 删除 |
| 21 | `ENGINEERING_SPEC.md` | FILE | 是 | 7,416 | 1 | 2026-09-16 13:19:13 | `8664f18088cbcb845431922bf93eba8ac401e01712344ee8ae323a084d5df432` | a861d8f6 2026-09-16 13:19:42 +0800 | E | 保留 |
| 22 | `FATDUCK_ACCESS.md` | FILE | 是 | 2,153 | 1 | 2026-08-26 21:37:03 | `4e1d653a46143e478a5b0f9d1d2701d7e2930352936c2b4b9336ddecc17e373a` | 2f20a99d 2026-08-29 00:24:51 +0800 | F | 保留（登记建议，交由 GOV-001/DOC-001） |
| 23 | `GaiaDR3` | DIR | 否 | 43,936,859,210 | 16 | 2026-09-09T16:38:27 | `files=16, sum=43,936,859,210B` | —（untracked） | D | 原地保留登记 |
| 24 | `GaiaDR3SP` | DIR | 否 | 67,834,893,663 | 20 | 2026-09-09T16:41:24 | `files=20, sum=67,834,893,663B` | —（untracked） | D | 原地保留登记 |
| 25 | `HANDOVER.md` | FILE | 是 | 9,216 | 1 | 2026-09-11 02:12:11 | `bc7aa32c164b4c93a94bb0c1400405245f42b4bc9aac79efa758bc142cfa8d4b` | eb315b20 2026-09-11 02:12:25 +0800 | F | 保留（登记建议，交由 GOV-001/DOC-001） |
| 26 | `README.md` | FILE | 是 | 8,831 | 1 | 2026-09-16 03:46:06 | `dfe13c5154dee66968676e1f7ed73a3977e8e340e6aaa4d5d4a162722a6540ef` | 46ca7573 2026-09-16 03:57:13 +0800 | E | 保留 |
| 27 | `REVIEW.md` | FILE | 是 | 20,936 | 1 | 2026-09-16 03:46:53 | `eea9c8400262a6d3ec152b6ec8bf214c3e47c12deff08da049f5d7e431862c51` | 46ca7573 2026-09-16 03:57:13 +0800 | F | 保留（登记建议，交由 GOV-001/DOC-001） |
| 28 | `Testing` | DIR | 否 | 125 | 2 | 2026-09-11T17:21:41 | `files=2, sum=125B` | —（untracked） | B | 删除 |
| 29 | `VERSION` | FILE | 是 | 15 | 1 | 2026-09-05 19:38:12 | `c42c9d58f54f29085af17d9f96fb3caf922a9abdac2f02dd10654174a5309a55` | b4f923cc 2026-09-05 20:06:55 +0800 | F | 保留（登记建议，交由 GOV-001/DOC-001） |
| 30 | `VISUAL_CHECK_README.md` | FILE | 是 | 2,271 | 1 | 2026-08-21 21:43:26 | `a121b3f9fbee6a67952a39b3c405f5df03d6acade51e0d7778bad213ac3a7377` | 42c801de 2026-08-12 18:21:39 +0800 | F | 保留（登记建议，交由 GOV-001/DOC-001） |
| 31 | `alloc_report.json` | FILE | 否 | 819 | 1 | 2026-09-16 02:54:18 | `064e5511fc6062c292a80fc42fd045b23dd217c3de4c63bf4e0b7786064ceaf1` | —（untracked） | A | 删除 |
| 32 | `alloc_samples.csv` | FILE | 否 | 151 | 1 | 2026-09-16 02:54:18 | `30a82c5d24d77a30104d9976ed70746f959c9180575d1ab26bdc9fa0ac389185` | —（untracked） | A | 删除 |
| 33 | `artifacts` | DIR | 否 | 76,304,189 | 1415 | 2026-09-16T04:27:29 | `files=1415, sum=76,304,189B` | 3ec798d3 2026-09-16 04:28:28 +0800 | E | 保留 |
| 34 | `astrocs_p1sess_neg` | DIR | 否 | 5,280 | 1 | 2026-09-12T17:16:33 | `files=1, sum=5,280B` | —（untracked） | A | 删除 |
| 35 | `astrocs_p1sess_perf` | DIR | 否 | 134,400 | 16 | 2026-09-12T17:16:33 | `files=16, sum=134,400B` | —（untracked） | A | 删除 |
| 36 | `astrocs_p1sess_props` | DIR | 否 | 37,920 | 7 | 2026-09-12T17:16:33 | `files=7, sum=37,920B` | —（untracked） | A | 删除 |
| 37 | `astrocs_p1sess_test` | DIR | 否 | 55,288 | 12 | 2026-09-12T17:16:33 | `files=12, sum=55,288B` | —（untracked） | A | 删除 |
| 38 | `astrocs_run_37bc24d27490.json` | FILE | 否 | 594 | 1 | 2026-09-12 18:49:33 | `0c4bea311a2c2f30a3067df7643d37e43d7ce969c7d0b4d8296e82a8e2767ae5` | —（untracked） | A | 删除 |
| 39 | `astrocs_run_37e2e0b1a467.json` | FILE | 否 | 600 | 1 | 2026-09-12 18:52:20 | `bf8be805d6503187307b13f093801ccd6d1dd7c5da2d347d3d3fb04e072f1487` | —（untracked） | A | 删除 |
| 40 | `astrocs_run_37e30359908d.json` | FILE | 否 | 610 | 1 | 2026-09-12 18:52:20 | `d70ffcdfc94c4cee57128e3313f0637a9bc31cfc779823761e6e7a2df69ab71c` | —（untracked） | A | 删除 |
| 41 | `astrocs_run_37e3658244f9.json` | FILE | 否 | 595 | 1 | 2026-09-12 18:52:22 | `b45f0b2763c5a37a4fb52ff6830e25536be8b3839e34abae379ca9799c4fa312` | —（untracked） | A | 删除 |
| 42 | `astrocs_run_747dd4685289.json` | FILE | 否 | 750 | 1 | 2026-09-13 13:22:56 | `e8b389bd68dd3a20ce6f1460a9ff921b530579d8b17a2e520905f7c68cccfb6d` | —（untracked） | A | 删除 |
| 43 | `astrocs_run_7710c5d75718.json` | FILE | 否 | 612 | 1 | 2026-09-13 14:10:06 | `34f263f09d1c4f498ba4bcfa4dcad19a316dd9102b67a530f12c3edb0b480c80` | —（untracked） | A | 删除 |
| 44 | `astrocs_run_7727be85ca3b.json` | FILE | 否 | 600 | 1 | 2026-09-13 14:11:45 | `54e61ca97072c58720aa51a64a32988271fb1f54c2897199397fa5f78adec62a` | —（untracked） | A | 删除 |
| 45 | `astrocs_run_773fc5abbdb2.json` | FILE | 否 | 616 | 1 | 2026-09-13 14:13:28 | `b4362ead22d03c06739d96e18783db3a479a099dd3519d7eeb744b140ca10a8b` | —（untracked） | A | 删除 |
| 46 | `astrocs_run_7764cfc42284.json` | FILE | 否 | 1,059 | 1 | 2026-09-13 14:16:07 | `c667aec1e44050483eb8af3be71046feec8bcbc106d83c0f1aa89b7e98f74cd6` | —（untracked） | A | 删除 |
| 47 | `astrocs_run_7764edd68939.json` | FILE | 否 | 632 | 1 | 2026-09-13 14:16:07 | `3616a374cf3f51448328c815dbb1bc21586a7c0f53080fa95278e30225157977` | —（untracked） | A | 删除 |
| 48 | `astrocs_run_776551ed3162.json` | FILE | 否 | 617 | 1 | 2026-09-13 14:16:09 | `acbccec57f0bf042f11509649477eb829dc2cfc3cfe41dcc9290014d9f21f652` | —（untracked） | A | 删除 |
| 49 | `astrocs_run_7775fc748ba0.json` | FILE | 否 | 623 | 1 | 2026-09-13 14:17:21 | `1129229b9ab122e0d58d3b7e0e8785f7737d2cae45959a8d2733c3bd758885af` | —（untracked） | A | 删除 |
| 50 | `astrocs_run_777ed0a3fc79.json` | FILE | 否 | 612 | 1 | 2026-09-13 14:17:59 | `96c75e07a9b211f0e44e2e1c8f6e41e20fe5cc86fb6b77d880ac8239c190d3cf` | —（untracked） | A | 删除 |
| 51 | `astrocs_run_778a3f855cbc.json` | FILE | 否 | 612 | 1 | 2026-09-13 14:18:48 | `5a4997713ec214b3208ccf6e5ddac04744407b445397f614b41b49eaa91a68ea` | —（untracked） | A | 删除 |
| 52 | `astrocs_run_77aa2e202ef7.json` | FILE | 否 | 616 | 1 | 2026-09-13 14:21:05 | `7578220b6f4d9aa2035d43980c9af0aef8569487a541f6c9ce6818afac754cb1` | —（untracked） | A | 删除 |
| 53 | `astrocs_run_77cf2b3ad159.json` | FILE | 否 | 622 | 1 | 2026-09-13 14:23:44 | `1b913cf2f13e1363a38dce992a9650d2fb15a47ad892d9e9c6ae1a0cf25d9f10` | —（untracked） | A | 删除 |
| 54 | `astrocs_run_77cf4df6054b.json` | FILE | 否 | 632 | 1 | 2026-09-13 14:23:44 | `88889942791dad051d5c2204ccb76b92804289b72f916c2b712ae74bebfbc2eb` | —（untracked） | A | 删除 |
| 55 | `astrocs_run_77cfb00d72f5.json` | FILE | 否 | 617 | 1 | 2026-09-13 14:23:46 | `ffc1dce92e0eff9c15535ce9dea34399f7f4689c28b602c28855c6ba5bfbab7c` | —（untracked） | A | 删除 |
| 56 | `astrocs_run_77e065d9a558.json` | FILE | 否 | 623 | 1 | 2026-09-13 14:24:58 | `dd066d883fb2398fa96b97327cf7ae380f6a3edcf6036535092557bdd981b2c5` | —（untracked） | A | 删除 |
| 57 | `astrocs_run_77e9148413a2.json` | FILE | 否 | 612 | 1 | 2026-09-13 14:25:35 | `5e6131d71928d043ae46e332d66a56e963a1c5f5a9b7823e1d9324717de68733` | —（untracked） | A | 删除 |
| 58 | `astrocs_run_77f43b9efd54.json` | FILE | 否 | 612 | 1 | 2026-09-13 14:26:23 | `fc1714904e88d824bfbf123998d6199d31b440ae78e0bffa50e5f1a39ffe65b2` | —（untracked） | A | 删除 |
| 59 | `astrocs_run_78ca3c837398.json` | FILE | 否 | 683 | 1 | 2026-09-13 14:41:42 | `3b8c120d5bf275fcb50d9d4a1418941e03fe6b62ba9e1fb2d2ff5e56cad35e8c` | —（untracked） | A | 删除 |
| 60 | `astrocs_run_78ca5b10633d.json` | FILE | 否 | 632 | 1 | 2026-09-13 14:41:43 | `e1ec0a1af74b75fcf69e73ce1d55de4ed5631d97678cc86d72a8c70daad38c14` | —（untracked） | A | 删除 |
| 61 | `astrocs_run_7a0e52cce96d.json` | FILE | 否 | 683 | 1 | 2026-09-13 15:04:54 | `33b295e2491204d69908ae6d9e66b5beb4d4eeef93af10a6c3d0c3b080099a01` | —（untracked） | A | 删除 |
| 62 | `astrocs_run_7a0e716c3e34.json` | FILE | 否 | 632 | 1 | 2026-09-13 15:04:55 | `25d782f6c5e484b111b0bf32370a282c7b8474610c12e943b62248cc881d4a24` | —（untracked） | A | 删除 |
| 63 | `astrocs_run_7aa043edacca.json` | FILE | 否 | 683 | 1 | 2026-09-13 15:15:21 | `d6a32721f5939f0a444ebd2ba03599b3ea22063e7087648060c780100d52e875` | —（untracked） | A | 删除 |
| 64 | `astrocs_run_7aa06280a22e.json` | FILE | 否 | 632 | 1 | 2026-09-13 15:15:21 | `3e8a7569b2939647661c13e195fad138400c383ad668f38caf5b10e7d59f08fa` | —（untracked） | A | 删除 |
| 65 | `astrocs_run_7b9b6d815bea.json` | FILE | 否 | 683 | 1 | 2026-09-13 15:33:20 | `3502637e6c736eab2d68b9eb6e6543266aed9bc282f1ee865f7dc6aeb8950c5e` | —（untracked） | A | 删除 |
| 66 | `astrocs_run_7b9b8c00f246.json` | FILE | 否 | 632 | 1 | 2026-09-13 15:33:20 | `b82ddbf82e20dd59b6628c323c04af199f9a56c8ad7c5e12ffdbc88064e78bc9` | —（untracked） | A | 删除 |
| 67 | `astrocs_run_7c2d25c2a8c7.json` | FILE | 否 | 661 | 1 | 2026-09-13 15:43:46 | `1652058bc9c0c2e87f184b1724321302c4b78152485fa833ea0c52e9c2bfa927` | —（untracked） | A | 删除 |
| 68 | `astrocs_run_7c2d4452f907.json` | FILE | 否 | 610 | 1 | 2026-09-13 15:43:46 | `d411b274331c32489577396b7814526dc5113645047be71949fd5929730ce6f9` | —（untracked） | A | 删除 |
| 69 | `astrocs_run_7c92e964454a.json` | FILE | 否 | 594 | 1 | 2026-09-13 15:51:03 | `7c7653cc6b230720068508030e707741805192ab67fb53c41b0f2c52aa0dc91d` | —（untracked） | A | 删除 |
| 70 | `astrocs_run_7cb926de8cb6.json` | FILE | 否 | 600 | 1 | 2026-09-13 15:53:47 | `a5c37280980ddc58538400ef182b42849fbc132fbd31807ec9ffc39cef96a471` | —（untracked） | A | 删除 |
| 71 | `astrocs_run_7cb94a8107ba.json` | FILE | 否 | 610 | 1 | 2026-09-13 15:53:47 | `c96adf974e00d35429280b097cfa51640753d42b1f623e246ecf2cf02ce15e67` | —（untracked） | A | 删除 |
| 72 | `astrocs_run_7cb9ae372be0.json` | FILE | 否 | 595 | 1 | 2026-09-13 15:53:49 | `4a4fb6f0df0038824d95d6e05c9de8f51bfa10cf7930e9b4947258e010f95799` | —（untracked） | A | 删除 |
| 73 | `astrocs_run_7cd992a70cb6.json` | FILE | 否 | 600 | 1 | 2026-09-13 15:56:07 | `c90fc95f08fad221ccee6ae33f20bce49c99d0fc83af5a01b16f134470d94cbc` | —（untracked） | A | 删除 |
| 74 | `astrocs_run_7ce3113498e3.json` | FILE | 否 | 590 | 1 | 2026-09-13 15:56:47 | `9535cd119bf7dc5368d3ce7d32f81fce1b5537130df62452b047366016df54d7` | —（untracked） | A | 删除 |
| 75 | `astrocs_run_7cf141fb3858.json` | FILE | 否 | 590 | 1 | 2026-09-13 15:57:48 | `6f5f9896d78d42d11dfd7e8ea94e80f789e070b548ee9a26e02eb556c2060a32` | —（untracked） | A | 删除 |
| 76 | `astrocs_run_7d268fd82da5.json` | FILE | 否 | 594 | 1 | 2026-09-13 16:01:37 | `4d1dcb1162b37c0330f9275ae8fd40a616857b54bfbab700df661885a0576539` | —（untracked） | A | 删除 |
| 77 | `astrocs_run_7d4cd130056e.json` | FILE | 否 | 600 | 1 | 2026-09-13 16:04:21 | `2f4fd8aec87e2d5a7139eb67f7a3e6efc26bb69387c627574baf360c8dc07e14` | —（untracked） | A | 删除 |
| 78 | `astrocs_run_7d4cf3fb7d58.json` | FILE | 否 | 610 | 1 | 2026-09-13 16:04:22 | `d4852ceb597634d9c9db7cba528bacdcc6dead9829cc87c0443fc292e08c534c` | —（untracked） | A | 删除 |
| 79 | `astrocs_run_7d4d5a19bdd9.json` | FILE | 否 | 595 | 1 | 2026-09-13 16:04:23 | `c071de9c3f7f67cf55700c98fc1c863067ea0149cf3a638486bcee5a56e086f5` | —（untracked） | A | 删除 |
| 80 | `astrocs_run_7d868d76fc59.json` | FILE | 否 | 590 | 1 | 2026-09-13 16:08:29 | `368c2864ca21b90134c08a8696b533b4bd54a9295d141c1c9e80ead02fe65bfe` | —（untracked） | A | 删除 |
| 81 | `astrocs_run_7dbd576dd925.json` | FILE | 否 | 594 | 1 | 2026-09-13 16:12:24 | `b0c486b4b162ca96c9c02a23b93008dfe055fd16c1afb40aa706f2684b2e8169` | —（untracked） | A | 删除 |
| 82 | `astrocs_run_7de3c1dd4ebc.json` | FILE | 否 | 600 | 1 | 2026-09-13 16:15:09 | `f996162eaecd7823537474eb49fd18a8ed8242d2228b655bba5ad13d33080bdc` | —（untracked） | A | 删除 |
| 83 | `astrocs_run_7de3e6278900.json` | FILE | 否 | 610 | 1 | 2026-09-13 16:15:10 | `c13b35034fa0a56e4dd212ea72716cede19898fe264f448bdd44b6e1538c5811` | —（untracked） | A | 删除 |
| 84 | `astrocs_run_7de4477e6a0c.json` | FILE | 否 | 595 | 1 | 2026-09-13 16:15:12 | `1825baf73599ef989e1293348c2c02cf4c12fce76037f5c31aa0b66b2b580c6c` | —（untracked） | A | 删除 |
| 85 | `astrocs_run_7e0290c68a05.json` | FILE | 否 | 600 | 1 | 2026-09-13 16:17:22 | `5289a6715d09b0c019523b3c9d6fdd3e0da3e3397bc65cc37e84f0784cadff68` | —（untracked） | A | 删除 |
| 86 | `astrocs_run_7e0b6c51dacc.json` | FILE | 否 | 590 | 1 | 2026-09-13 16:18:00 | `746134e0d532a479e1a3bfbfd8b07d1fa4802ba73f998f5ad122552c1d357180` | —（untracked） | A | 删除 |
| 87 | `astrocs_run_7e1909b8a2bf.json` | FILE | 否 | 590 | 1 | 2026-09-13 16:18:58 | `afc5a5f8bc807b974d485446e07f7a8e16d43b63a882b5aa0dea599257a6ea3f` | —（untracked） | A | 删除 |
| 88 | `astrocs_run_7e65e9789939.json` | FILE | 否 | 683 | 1 | 2026-09-13 16:24:28 | `c3dab58c85cdb16656738af7e1b2388d0d093fe81ac20e2e20df6730e1e831ba` | —（untracked） | A | 删除 |
| 89 | `astrocs_run_7e6607ca717c.json` | FILE | 否 | 632 | 1 | 2026-09-13 16:24:29 | `6a540ad1bf9d61a70981cea62a2ba595528cb3145ee7bc1f75e9df497b021a40` | —（untracked） | A | 删除 |
| 90 | `astrocs_run_9154bb000abc.json` | FILE | 否 | 1,115 | 1 | 2026-09-13 22:11:25 | `6ac7fb49ad29dd2150d876ed8ff75bb66a6fa4a0d4bd762f2a4cfbed46c71f22` | —（untracked） | A | 删除 |
| 91 | `astrocs_run_9154d913321c.json` | FILE | 否 | 610 | 1 | 2026-09-13 22:11:26 | `3dbb5157e3b764c237c7bb0a3d541b85b542242fe914ff0a8373f8bc125e7b28` | —（untracked） | A | 删除 |
| 92 | `astrocs_run_a92907fa9737.json` | FILE | 否 | 1,115 | 1 | 2026-09-14 05:28:06 | `d40e8f32ef636716616733952ad9ea5c9c10a0688a328bf6775596bb5de418e8` | —（untracked） | A | 删除 |
| 93 | `astrocs_run_a929266a85f1.json` | FILE | 否 | 610 | 1 | 2026-09-14 05:28:06 | `f53967f62b196dccde4cb24797dc078d045cbdf5e2d9e143bf840917ed934a2d` | —（untracked） | A | 删除 |
| 94 | `build` | DIR | 否 | 6,364,625,713 | 7080 | 2026-09-16T02:32:59 | `files=7080, sum=6,364,625,713B` | —（untracked） | B | 删除 |
| 95 | `build.sh` | FILE | 是 | 2,011 | 1 | 2026-08-28 13:25:52 | `1d4a591730cbe23e0c42dbaa7eac47a2a16e7cc18460c6a3f5e792f80246e137` | ef19a5e0 2026-08-28 13:28:19 +0800 | E | 保留 |
| 96 | `ci` | DIR | 否 | 2,216,048 | 115 | 2026-09-16T02:16:04 | `files=115, sum=2,216,048B` | 0d8e98f1 2026-09-16 02:23:08 +0800 | E | 保留 |
| 97 | `cli` | DIR | 否 | 367,299 | 22 | 2026-09-16T02:11:13 | `files=22, sum=367,299B` | 0d8e98f1 2026-09-16 02:23:08 +0800 | F | 保留（登记建议，交由 GOV-001/DOC-001） |
| 98 | `cmake` | DIR | 否 | 34,754 | 7 | 2026-09-11T23:39:11 | `files=7, sum=34,754B` | e6254d4d 2026-09-12 00:12:18 +0800 | E | 保留 |
| 99 | `contracts` | DIR | 否 | 462,017 | 62 | 2026-09-16T00:48:15 | `files=62, sum=462,017B` | 2ac6b758 2026-09-16 00:51:00 +0800 | E | 保留 |
| 100 | `docs` | DIR | 否 | 3,614,069 | 334 | 2026-09-16T13:19:13 | `files=334, sum=3,614,069B` | a861d8f6 2026-09-16 13:19:42 +0800 | E | 保留 |
| 101 | `engineering` | DIR | 否 | 0 | 0 | 2026-09-16T13:19:13 | `files=0, sum=0B` | —（untracked） | E | 保留 |
| 102 | `evidence` | DIR | 否 | 19,680,121 | 2799 | 2026-09-14T05:29:29 | `files=2799, sum=19,680,121B` | 9690c1da 2026-09-09 19:23:00 +0800 | F | 保留（登记建议，交由 GOV-001/DOC-001） |
| 103 | `graph` | DIR | 否 | 5,459 | 9 | 2026-09-13T16:15:09 | `files=9, sum=5,459B` | —（untracked） | B | 删除 |
| 104 | `include` | DIR | 否 | 148,620 | 23 | 2026-09-14T22:18:28 | `files=23, sum=148,620B` | 57c04256 2026-09-14 22:26:27 +0800 | E | 保留 |
| 105 | `lib` | DIR | 否 | 212,345,576 | 1327 | 2026-09-16T03:10:38 | `files=1327, sum=212,345,576B` | 393db3fb 2026-09-16 01:48:06 +0800 | E | 保留 |
| 106 | `logs` | DIR | 否 | 5,798 | 5 | 2026-09-14T20:31:03 | `files=5, sum=5,798B` | —（untracked） | E | 保留 |
| 107 | `memory.md` | FILE | 是 | 15,373 | 1 | 2026-09-12 15:15:54 | `3bea1a8b2e511012536bb061a786d861398d005eafd18266674b75331fb6fdf0` | 327b6c30 2026-09-12 15:19:31 +0800 | E | 保留 |
| 108 | `modules` | DIR | 否 | 144,993 | 16 | 2026-09-06T23:31:01 | `files=16, sum=144,993B` | f896d1bb 2026-09-06 23:35:10 +0800 | F | 保留（登记建议，交由 GOV-001/DOC-001） |
| 109 | `out` | DIR | 否 | 661 | 1 | 2026-09-08T07:38:49 | `files=1, sum=661B` | —（untracked） | A | 删除 |
| 110 | `p10-files.patch` | FILE | 否 | 21,329 | 1 | 2026-09-14 22:18:28 | `ba1df961e836dabecfeca9619991171265581ba85e81b39a79c2d0232277a172` | —（untracked） | F | 保留（建议 .gitignore 登记） |
| 111 | `p11-files.patch` | FILE | 否 | 13,704 | 1 | 2026-09-14 21:54:10 | `061ed1d301b3050da2786d2e92adf2fc45b151b73e5657a84a3458611107061d` | —（untracked） | F | 保留（建议 .gitignore 登记） |
| 112 | `p15a-files.patch` | FILE | 否 | 26,485 | 1 | 2026-09-14 23:36:24 | `c97bd9c5a51c0df19ae94135207594f79727bc2e2bad1407f3b38343317cfeb8` | —（untracked） | F | 保留（建议 .gitignore 登记） |
| 113 | `p8-files.patch` | FILE | 否 | 23,533 | 1 | 2026-09-14 20:39:10 | `88707acacdb656df135897391134b033fd9c68cd7520f19ef2c8ed6967f656e4` | —（untracked） | F | 保留（建议 .gitignore 登记） |
| 114 | `p9-files.patch` | FILE | 否 | 19,878 | 1 | 2026-09-14 22:27:22 | `7e0bae40f501ffb18842e2a70be44426bac3a1bc25b2b82e1bd29c0699d261de` | —（untracked） | F | 保留（建议 .gitignore 登记） |
| 115 | `packaging` | DIR | 否 | 62,137 | 19 | 2026-09-11T23:47:24 | `files=19, sum=62,137B` | e6254d4d 2026-09-12 00:12:18 +0800 | E | 保留 |
| 116 | `providers` | DIR | 否 | 127,412 | 10 | 2026-09-07T00:58:21 | `files=10, sum=127,412B` | 1938b7aa 2026-09-07 01:02:25 +0800 | F | 保留（登记建议，交由 GOV-001/DOC-001） |
| 117 | `reports` | DIR | 否 | 7,572,484 | 734 | 2026-09-16T14:09:54 | `files=734, sum=7,572,484B` | 3ec798d3 2026-09-16 04:28:28 +0800 | E | 保留 |
| 118 | `resource_samples.csv` | FILE | 否 | 351 | 1 | 2026-09-16 02:54:18 | `a82b434cdeeb21fcd95cc78816a7071740ce2f07dbbba6e226899d6d4d52d54c` | —（untracked） | A | 删除 |
| 119 | `resource_summary.json` | FILE | 否 | 1,237 | 1 | 2026-09-16 02:54:18 | `495d1f475e8f94caabe45faff5c157c6a1c0073d7cb57f3297c3a105dc0f6daf` | —（untracked） | A | 删除 |
| 120 | `run` | DIR | 是 | 106,692,246,060 | 635019 | 2026-09-16T14:10:53 | `files=635019, sum=106,692,246,060B` | 896755de 2026-07-31 19:10:28 +0800 | E | 保留 |
| 121 | `run_context.json` | FILE | 否 | 231 | 1 | 2026-09-16 02:54:17 | `0ff783873e4eb640909f1f254eb26d7bb87db9dcb5348913e8147684d72cfd82` | —（untracked） | A | 删除 |
| 122 | `runtime` | DIR | 否 | 985,683 | 50 | 2026-09-16T03:14:22 | `files=50, sum=985,683B` | 0d8e98f1 2026-09-16 02:23:08 +0800 | F | 保留（登记建议，交由 GOV-001/DOC-001） |
| 123 | `scripts` | DIR | 否 | 4,087 | 2 | 2026-08-31T01:58:47 | `files=2, sum=4,087B` | b840ed64 2026-08-31 01:59:23 +0800 | E | 保留 |
| 124 | `testdata` | DIR | 是 | 31,758,220,035 | 944 | 2026-09-11T21:34:17 | `files=944, sum=31,758,220,035B` | 9f6b72b5 2026-09-11 21:43:09 +0800 | D | 原地保留登记 |
| 125 | `tests` | DIR | 否 | 8,102,307 | 663 | 2026-09-16T14:03:27 | `files=663, sum=8,102,307B` | b7c4f35e 2026-09-16 03:38:07 +0800 | E | 保留 |
| 126 | `third_party` | DIR | 是 | 1,040,613 | 1 | 2026-08-28T23:01:31 | `files=1, sum=1,040,613B` | d484f7c5 2026-08-28 23:13:30 +0800 | E | 保留 |
| 127 | `toolchain.ps1` | FILE | 是 | 9,334 | 1 | 2026-08-21 21:43:26 | `ac106fa00db90cd4de008cfca6b9b67c90ad245b08794a7bd7aac58d343f24a1` | a53c97de 2026-08-04 18:46:37 +0800 | E | 保留 |
| 128 | `tools` | DIR | 否 | 2,672,834 | 229 | 2026-09-16T14:03:27 | `files=229, sum=2,672,834B` | 0d8e98f1 2026-09-16 02:23:08 +0800 | E | 保留 |
| 129 | `worker_balance.csv` | FILE | 否 | 82 | 1 | 2026-09-16 02:54:18 | `d7a2b6d9113fadccc457f7f4d243308e8071438ea56f0e695bd3f95c3290d044` | —（untracked） | A | 删除 |
| 130 | `worktrees` | DIR | 否 | 0 | 0 | 2026-09-04T18:53:05 | `files=0, sum=0B` | —（untracked） | F | 保留（待负责人裁决） |
| 131 | `工程控制` | DIR | 否 | 203,031 | 37 | 2026-09-16T14:10:01 | `files=37, sum=203,031B` | 5f891080 2026-09-16 14:11:39 +0800 | E | 保留 |
| 132 | `设计大纲` | DIR | 否 | 86,346,995 | 6406 | 2026-09-15T12:27:50 | `files=6406, sum=86,346,995B` | 08898978 2026-09-15 12:28:17 +0800 | F | 保留（登记建议，交由 GOV-001/DOC-001） |
| 133 | `问题扫描` | DIR | 否 | 17,651,027 | 1095 | 2026-09-15T13:59:22 | `files=1095, sum=17,651,027B` | f17f28c4 2026-09-15 13:59:22 +0800 | F | 保留（登记建议，交由 GOV-001/DOC-001） |

## 7. 目录内小文件指纹清单（A/B 待删目录，删除可追溯）

完整清单落盘在 `reports/PROJECT-GOVERNANCE-01/root/deleted-manifest/`（每目录一份 `*.sha256`，格式 `<sha256>  <相对路径>`；`build/` 因可重建且 7080 文件，只记文件数/总量/顶层清单）。

## 10. 收口修订（2026-09-16，本节数字为最终口径，覆盖 §1 的「执行前」计数）

### 10.1 最终计数

| 项 | 值 |
|---|---|
| 执行前顶层条目 | 133 |
| **执行后顶层条目** | **60** |
| 删除条目总数 | **74** = ROOT-001 A/B 72 + `设计大纲/` 1 + `timeout` 1 |
| 删除总字节 | **6,451,428,033 B = 6.008 GiB** |
| 其中 ROOT-001 A/B | 72 项 / 6,365,080,946 B（5.928 GiB） |
| 其中 `设计大纲/` | 1 项 / 86,346,995 B（0.080 GiB）/ 6,406 文件 / 344 tracked |
| 其中 `timeout` | 1 项 / 92 B（并发线 ROOT-004 的意外 stderr 重定向产物） |
| 另：ROOT-003 清运 `run/` | 70 目录 / 5,430,815,754 B（5.058 GiB），见 `RETENTION.md` §3 |
| `/workspace` 已用（`df -B1`） | 313,646,256,128 B（292.1 GiB）→ **301,904,625,664 B（281.2 GiB）**，回收 **11,741,630,464 B ≈ 10.935 GiB** |
| `git ls-tree -r HEAD` 行数 | 清运前后 5,367 一致；开工时为 5,366，因前台提交 `5f891080` 增至 5,367（非本任务所致） |
| `git ls-files run` | 始终仅 `run/.gitkeep` |

### 10.2 追加删除项（指纹与依据）

| 路径 | 类 | 字节 | 文件数 | 指纹 / 清单 | 依据 |
|---|---|---|---|---|---|
| `设计大纲/` | A（负责人授权） | 86,346,995 | 6,406（tracked 344） | `reports/PROJECT-GOVERNANCE-01/root/deleted-manifest/设计大纲.sha256`（344 条 sha256，由 `git cat-file blob HEAD:<path>` 反算）+ `设计大纲.tracked-files-344.txt` | `tasks/ROOT-001.md` 负责人裁决段「已确认无用，删除」；调度员 2026-09-16 明示按任务卡执行（本任务只删工作区文件，删除 commit 由前台提交） |
| `timeout` | A | 92 | 1 | sha256 `48d4d97cd0181c4b3d6a0400e4711606fa66efd22a9abe7ebe49cf74c785c417`，内容留档 `timeout.content.txt` | 内容为 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/p0_verify_A.sh: 行 5: 11: 错误的文件描述符` —— 并发线 ROOT-004 脚本重定向失误产生的 stderr 转储，非本任务产物；已同步告知调度员 |

### 10.3 §9 待裁决表修订

- `设计大纲/` 已从 F2 移出并删除（见 §10.2），**最终 F 类 = 22 条**：F1 8 条（5 个 `p*-files.patch` + `CS/` + `AstroCS.wiki/` + `worktrees/`）；F2 14 条（`VERSION`、`CHANGELOG.md`、`FATDUCK_ACCESS.md`、`VISUAL_CHECK_README.md`、`REVIEW.md`、`HANDOVER.md`、`ASTROCS_PROJECT_CONSTITUTION.md`、`AstroCS_ENGINEERING_CONSTRAINTS.md`、`问题扫描/`、`evidence/`、`cli/`、`runtime/`、`providers/`、`modules/`）。
- `run/perf-fix`、`run/release-rescue` **未删除**（被 tracked 报告引用，按 `RETENTION.md` R1 保留）；其余 27 个被引用目录同样保留。

### 10.4 单位口径（统一 GiB + 字节双写）

按调度员要求，本账本与 `RETENTION.md` 的容量数字统一「字节（表观）+ GiB」双写；1 GiB = 1,073,741,824 B。`du -sh` 的 `G` 后缀是磁盘块占用口径，与表观字节可能差数个百分点；引用时以字节为准并注明来源命令。

## 11. 未执行但建议做的事（交付后续任务）

| # | 事项 | 建议归属 | 理由 |
|---|---|---|---|
| 1 | `设计大纲/` 的 344 个删除需由前台以独立 commit 提交 | 前台 | 本任务零 git 写权限；`git status` 现显示 344 个 `D` |
| 2 | `docs/ci/01_CHECKS.md` 登记 `CHK-ROOT-CLEAN` | CI-001 | 本次未改 `docs/**`（派发直令禁碰），注册表已入库但检查项文档未同步 |
| 3 | `tests/quality` 4 个预存失败（`test_docchk002_mutation`、`test_doc_machine_check`、`test_doc_line_anchors`、`test_known_failures_baseline_ci::test_t13b`） | DOC-001 / CI-001 | 与本次改动无关（移除本任务测试文件后同样 4 失败，94 vs 99 用例）；`test_t13b` 即 `ci/validate_registry.py` R10 的既有红灯 |
| 4 | `ci/checks.json` 其余 145 项旧 ID 尚未迁移为 `CHK-*`（本次仅新增 1 项 `CHK-ROOT-CLEAN`） | CI-001 | GAP-016 原样保留，本任务不改既有条目 |
| 5 | `.gitignore` 新增项的「清理条件」需在 CLI/测试修复后回收 | CLI-003 / QA-001 | `/run_context.json`、`/astrocs_p1sess_*/` 是 ASTROCS_DESIGN §6.3 违规的兜底，不是终态 |


---

## 附录 R1　根条目用途台账（2026-09-16 清理后 · 回答「这目录是干什么的」）

### R1.1 本次清理（已执行，根条目 54 → 47）

| 条目 | 性质 | 为何无意义 | 处置 |
|---|---|---|---|
| `p8/p9/p10/p11/p15a-files.patch`（5 个） | 未跟踪文件（09-14） | **旧世代补丁**：diff 打的是迁移前路径（`lib/plate_solve/**`、`lib/healpix_db/**`、`docs/contracts/DATA_SEMANTICS.md`），ARCH-001 迁移后全部失效 | **已删除**（恢复走 git 历史） |
| `.pytest_cache/` | 未跟踪缓存 | pytest 缓存（`.gitignore:91` 已忽略但仍物理存在） | **已删除** |
| `AstroCS.wiki/` | 空目录（0 文件） | 旧 Wiki 归档，内容已不存在 ⇒ 空壳 | **已删除** |
| `evidence/` | 未跟踪（1 文件） | `evidence/v6_1_rework/...` —— **v6.1 世代**残留；§7 规定证据落 `artifacts/` ⇒ 根 `evidence/` 属未登记条目 | **已删除**；后续证据一律落 `artifacts/` |

### R1.2 保留条目及用途

**文件（§7 点名）**：`ASTROCS_DESIGN.md`（最高权威）、`AGENTS.md`（机器手册）、`ENGINEERING_SPEC.md`（工程约束+§7 根规范）、`CONTROL_PACK_SPEC.md`、`README.md`/`memory.md`/`DEPENDENCIES.md`、`CMakeLists.txt`/`CMakePresets.json`/`build.sh`/`toolchain.ps1`、`.clang-format`/`.editorconfig`/`.gitignore`/`.gitattributes`/`.github/`、**`VERSION`**（内部助记符，`cli/CMakeLists.txt:27` 读取，删除破构建）、**`FATDUCK_ACCESS.md`**（运营必需，禁删，无凭据）。

**目录**：

| 条目 | 用途 |
|---|---|
| `lib/` | 产品源码：`algorithms/`（16 模块 + shared）+ `infrastructure/`（9 模块），ARCH-001 迁移后结构 |
| `include/` `modules/` `providers/` `runtime/` `cli/` `packaging/` | 头文件 / 模块 / 提供者 / 运行时 / **CLI 兼容层**（不在根构建图内）/ 打包 |
| `contracts/` `cmake/` `config/` `docs/` `tests/` `tools/` `ci/` `scripts/` `testdata/` `third_party/` | §7 固定目录（合同/构建/全局配置/文档/测试/工具/CI/脚本/测试数据/第三方） |
| `工程控制/` | 控制包（现役 `PROJECT-GOVERNANCE-01/`） |
| `reports/` | 正式报告与证据（含 `research/**` 研究线交付） |
| `artifacts/` | §7 的**证据/产物**：`KNOWN_FAILURES_BASELINE.json`（**在用**，`source_commit` 指向当前 HEAD）、`ci/<sha>/`（每次 CI 运行产物）；`prerelease_v5/` 为旧世代但仍被 `ci/checks.json`/`impact_map` 引用 ⇒ **待 CI-003 处理** |
| `engineering/` | §7 要求存在，**当前为空**（见 R1.4） |
| `logs/` | §7 明示 gitignore 的运行日志目录，**当前为空**（日志实际落 `run/<task>/logs/`） |
| `run/` | gitignore 临时产物/日志（**94G / 63 万文件**，见 R1.4） |
| `问题扫描/` | 隔壁挖掘线的**在用台账**（1097 文件），登记禁删 |

**数据目录（gitignore；代码按相对路径读取 ⇒ 位置即契约）**：`GaiaDR3/` 41G、`GaiaDR3SP/` 63G（`tools/realdata/match_plan.py:666/667` 直读相对路径）、`BASS DR3/` 57M、`testdata/` 30G（§7 固定，944 文件中仅 1 个入库）、`build/` 590M。

### R1.3 检查器为什么没报？

`tools/quality/check_root_cleanliness.py` 只校验「§7 白名单 + 未登记条目」，**不校验**：① 未跟踪文件是否为旧世代残留；② §7 要求的目录是否为空壳；③ gitignore 目录的膨胀。⇒ 已登记为检查器缺口，**归 CI-003**（新增「根目录不得存在未跟踪的非白名单文件」与「白名单目录不得长期为空」两条门）。

### R1.4 需裁定（各一句）

1. **空壳 §7 目录**（`engineering/` 空、`logs/` 空）：① 删除并从 §7 移除；② 保留但要求有内容（如 `engineering/` 放工程决策记录、`logs/` 落运行日志）；③ 维持现状。
2. **`run/` 94G / 63 万文件**：是否按「保留最近 N 天 + 每任务自证摘要」裁剪一次？
3. **104G 参考数据**（`GaiaDR3/` `GaiaDR3SP/` `BASS DR3/`）：是否移出仓库根到 `/workspace/astrocs_data/` 并留**同名符号链接**（代码无需改动）？

