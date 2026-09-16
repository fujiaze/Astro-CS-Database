# F_TEST_GAP_P1 分片判定（ROOT-004）

- 分片名：F_TEST_GAP_P1（类别 F_TEST_GAP／优先级 P1／分配 46 条：FD-F-001..V15-N-16）
- 产物：reports/PROJECT-GOVERNANCE-01/root-scan/shards/F_TEST_GAP_P1.psv（表头 1 行 + 判定 46 行，10 列 PSV，UTF-8）
- 行数：46 条 finding；行序 = _assign/F_TEST_GAP_P1.tsv 顺序，逐条一行
- 四态计数：OPEN 46 ｜ RESOLVED 0 ｜ VOID 0 ｜ UNVERIFIABLE 0
- 基线实况：git rev-parse HEAD = 2c328348304d033aecfa81faf79d1c6cd802b30a = main（与简报所写 ecf6ad6f 不一致，见异常 1）；本轮按实测树取证
- ID 覆盖自证（命令+输出）：
  python3 -c "比对 _assign/F_TEST_GAP_P1.tsv 第 1 列与 PSV 第 1 列"
  输出：assign 46 psv 46 / missing_in_psv [] / extra_in_psv [] / order_identical True
- PSV 结构自证：python3 -c "逐行 split('|') 计字段" → nonempty lines 47 / bad field counts [] / header ok True / 结论分布 Counter({'OPEN': 46})
- 最重要 3 条 OPEN：
  1. FD-F-002：cli/resource_recorder.h:287 硬写 normalized_cpu_100pct_all_allocated_cores:false，tests/unit/mon001_recorder_test.cpp:100 只做 find(子串) 守卫 ⇒ 字段不表达事实、无可证伪性。
  2. M3-F-004：docs/TRACEABILITY.csv 的 SCI-NOISE 15 行 VERIFIED 全锚 lib/snr_estimator/cpp/test/noise_model_science_test.cpp，而该文件在 tests/ 与 ci/ 零编译目标（仅 redteam 脚本手工编译）。
  3. M8-F-005：tools/quality/check_ctest_registration.py 只从 add_test(NAME …) 出发；磁盘存在但从未注册的测试源对其永久隐形（口径：29 个 drizzle cpp 中 28 个在构建/CI 配置面 0 命中）。
- 异常：
  1. 简报基线 HEAD=main=ecf6ad6f，实测 2c328348（HEAD=main 仍一致）。
  2. 工作树脏（设计大纲/** 删除，artifacts/**、evidence/**、reports/v19r2/** 修改），属另一条线根清洁作业；本分片未触碰。
  3. 路径/引文漂移：tests/unit/gaia_xpsd_fixture_sp_gen.c 已不存在；GAIA_QUERY.md 已无「深树」字样；EVIDENCE_MANIFEST.json 全仓不存在（V1-N-09 原锚失效）；p1snr_science_test.cpp 实际位于 lib/snr_estimator/tests/p1noise/；u5_snr 段现为 u4_votable（M2b-F-03）；0.1431 仅命中 0.14312 子串（M1a-F-003）。
  4. 运行期未定性：无 build/astrocs、无 build/linux-openmp-on ⇒ M4-F-06 与 V15-N-16 的 CI 红绿终态未复跑（已在备注列写明）。
  5. 树内已有部分整改：M8-F-009 子事实①改 0.85/1.7；M4-F-04 的 seam 门改 fail-closed；FD-F-002 字面量改 false；M3-F-004 的 p1noise 已入库（EXISTS 门卫激活）——各自剩余判据仍成立，故结论仍 OPEN 并在备注列写明。
- UNVERIFIABLE 清单：无（0 条）。
- 零修复／零 git 写：仅写本 PSV、本 MD 与 run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/F_TEST_GAP_P1.log，未执行任何 git 写操作。
