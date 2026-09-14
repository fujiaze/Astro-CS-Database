# V9 附表：ci/checks.json 逐门五判据表（机器生成，只读复算）

* 口径：python 解析 ci/checks.json；终版时点 HEAD f5c790bd，UTC 2026-09-14T17:31:53Z（北京 2026-09-15 01:31）。门数 130（本会话开始时 121；期间前台连续新增 9 门，HEAD 移动 6 次）。**登记面是活动靶**：本表与 V9.md §3 结论均按终版时点复算；邻站再增门后须重跑 问题扫描/_verify/scripts_v9_table.py 与本表。
* 生成脚本 问题扫描/_verify/scripts_v9_table.py（只读）。「paths覆盖checker」= 该门自身 checker 路径是否落在其 changed_paths 内（N：改坏 checker 不触发该门）；「paths覆盖checks.json」= 登记面改动是否触发该门。
* 「静态判定」只填有复算直证或实测直证的行；其余留「—」。profiles 缩写 P=fast L=linux-main W=windows-main D=linux-deep。

| 门 id | profiles | plat | 可豁免 | 在基线 | paths覆盖checker | paths覆盖checks.json | 静态判定 | 失效机制/备注 |
|---|---|---|---|---|---|---|---|---|
| VERSION-CONSISTENCY | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| TRACEABILITY | l/w | any | N | N | N | N | 永不红 | A1 checker 无条件 return 0 (tools/quality/check_traceability.py:159) |
| TRACEABILITY-CODE | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| PRODUCTION-GRAPH | f/l/w | any | N | N | Y | N | 永不红 | A1 只跑 --selftest；真实 IR/trace 比对在登记面零载体 |
| NO-SERIAL-HEAVY | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| ACR-DORMANT | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| ABI-BOUNDARY | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| AGENTS-GOV | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| AIO-OWNERSHIP | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| API-DOCS | f/l/w | any | N | N | N | N | 永不红 | E5 run.py EMPTY_OUTPUT_SILENCE_EXEMPT：0 输出记 PASS |
| CLI-COMMAND-LAYER | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| CLI-RUN-PRESET | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| CONTRACT-GRAPH | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| DATA-ARTIFACTS | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| DUPLICATION | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| DOC-L0 | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| GLOSSARY-DOCS | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| MODULE-READMES | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| P3-STATUS | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| PIPELINE-TRACE | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| SERIAL-HARDCODE | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| UNIT-CLOSURE | f/l/w | any | N | N | N | N | 永不红 | E5 run.py EMPTY_OUTPUT_SILENCE_EXEMPT：0 输出记 PASS |
| WARNING-SUPPRESSION | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| THREAD-BUDGET | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| AST-API | l | linux | N | N | N | N | — | 静态可红（未逐一复算） |
| TASK-RESULT-SCHEMA | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| ISA-LEAK-SELFTEST | f/l/w | any | N | N | Y | N | 永不红 | A1 只跑 --selftest；真实模式需 --binary，CI 从不扫交付二进制 |
| SERIAL-HEAVY-SELFTEST | f/l/w | any | N | N | Y | N | 永不红 | A1 只跑 --selftest（真面由 NO-SERIAL-HEAVY 承担） |
| PROD-REACH-SELFTEST | f/l/w | any | N | N | Y | N | 永不红 | A1 只跑 --selftest；真实模式需 --binary/--compile_commands，零载体 |
| KNOWN-FAILURES-BASELINE | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| CON-API-CONTRACTS | f/l/w | any | N | N | N | N | 必红：API_CONTRACTS.csv:371 estimate_mag_lim_by_density 头文件已无声明 | 静态可红（未逐一复算） |
| CON-BUILD-GRAPH | f/l/w | any | N | N | N | N | 永不红 | E4 字面量子串断言（add_library(phase2 等） |
| CON-COMMENTS | f/l/w | any | N | N | N | N | 必红：synthetic_gate.cpp:5488 注释含 V19R2 且无「冻结」 | 静态可红（未逐一复算） |
| CON-CONFIG-CONTRACTS | f/l/w | any | N | N | N | N | 永不红 | E4 字面量子串断言（weight_mode = 2 / 错误消息原文） |
| CON-DOC-SYMBOLS | f/l/w | any | N | N | N | N | 永不红 | E4 符号三向 substring + 白名单/启发式跳过；known_files=repo.rglob 含未跟踪影子树 |
| CON-EXECUTION-CONTRACTS | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| CON-FORBIDDEN-PATTERNS | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| CON-FULL-INTEGRATION | f/l/w | any | N | N | N | N | 连带必红：聚合 10 个 checker，trace/api/comments 已 FAIL | 静态可红（未逐一复算） |
| CON-SCIENCE-UNITS | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| CON-TEST-CONTRACTS | f/l/w | any | N | N | N | N | 永不红 | E4 synthetic_gate 兜底 + TST 数>=5 阈值 |
| CON-TRACEABILITY | f/l/w | any | N | N | N | N | 必红：PSF/REJ 关键词零命中（2 findings） | E4 关键词 substring 断言（keyword in 拼接 requirement_id 串） |
| DOC-INDEX | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| VERSION-NAMESPACES | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| ENG-CONSTRAINTS | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| LOG-CONTRACT-SELFCHECK | f/l/w | any | N | N | N | N | 永不红 | A1 只跑 --selfcheck（无样本即 PASS 语义） |
| TRACEABILITY-MATRIX | f/l/w | any | N | N | N | N | 必红：CSV 带 BOM→表头不等；剥 BOM 后仍 5 行与 JSON 发散 | 静态可红（未逐一复算） |
| TESTKIT-LIST | f/l/w | any | Y | N | N | N | — | A1 只跑自检/清单模式 |
| WORKSPACE-ADOPTION | f/l/w | any | Y | N | N | N | — | A2 红被 waiver 吸收 |
| RECONCILE-STATE | f/l/w | any | Y | N | N | N | — | A2 红被 waiver 吸收 |
| TOOLCHAIN-VERIFY | f/l/w | any | N | N | N | N | — | 静态可红（未逐一复算） |
| LINUX-MAIN-FIXTURES | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| LINUX-MAIN-BUILD-TREE | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| UT-API | l | any | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-ARCH | l | any | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-BACKEND | l | any | N | N | Y | N | 实测红（48ceee59 exit1）且基线已无条目；现状需运行期确认 | 需运行期（未静态复算） |
| UT-CLI | l | any | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-VERSION | f/l/w | any | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-GLOSSARY | f/l/w | any | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-MONITORING | f/l/w | any | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-PIPELINE | f/l/w | any | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-RUNTIME | f/l/w | any | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-SCIENCELINT | f/l/w | any | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-TRACEABILITY | f/l/w | any | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-ABI | l | any | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-ARTIFACT | f/l/w | any | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-CONTRACTS | f/l/w | any | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-IO | l | any | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-QUALITY | l | linux | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-CPU-BASELINE | l | linux | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-CPU-DISPATCH | l | linux | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-CPU-AVX2 | l | linux | N | N | Y | N | — | 需运行期（未静态复算） |
| UT-CPU-AVX512 | l | linux | N | N | Y | N | 永不红 | A2 waivable + 缺 AVX-512F 即 exit 77 → SKIPPED(waivable) |
| BUILD-GCC-RELEASE | l/l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| DEEP-CLANG-BUILD | l | linux | Y | N | N | N | — | A2 红被 waiver 吸收 |
| DEEP-SAN-ASAN | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| DEEP-SAN-TSAN | l | linux | Y | N | N | N | — | A2 红被 waiver 吸收 |
| DEEP-COV-CPP | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| DEEP-COV-PY | l | linux | Y | N | N | N | — | A2 红被 waiver 吸收 |
| DEEP-COMPLEXITY | l | any | Y | N | N | N | — | A2 红被 waiver 吸收 |
| WIN-BUILD-RELEASE | w | windows | N | N | N | N | — | 需运行期（未静态复算） |
| WIN-TEST-UNIT | w | windows | N | N | N | N | — | 需运行期（未静态复算） |
| WIN-PACKAGE-CANDIDATE | w | windows | N | N | N | N | — | 需运行期（未静态复算） |
| WIN-CANDIDATE-VALIDATE | w | windows | N | N | Y | Y | — | 需运行期（未静态复算） |
| WORKFLOW-REGISTRY-BINDING | f/l/w | any | N | N | Y | Y | — | 静态可红（未逐一复算） |
| CI-BINDING-TESTS | f/l/w | any | N | N | N | Y | — | 静态可红（未逐一复算） |
| CTEST-REGISTRATION | f/l/w | any | N | N | Y | Y | — | 需运行期（未静态复算） |
| CTEST-LINUX-FULL | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-PHASE2-GATES | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-DRIZZLE-PRECISION-DEFAULT | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P1001-REAL-NODES | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P2001-REAL-NODES | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P2002-UNC-REJ-PROV | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P3002-REAL-NODES | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P3002-UNCERTAINTY | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P3-PROJECTION-UNITS | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P3-PROJECTION-FAULT | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P1WCS-APBP | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P1WCS-ASTROPY-CROSS | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-AIO-ABI-UNITS | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-AIO-ABI-NEGATIVE | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-AIO-ABI-SELFCHECK | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-AIO-HIPS-PUBLISH-ATOMIC-UNITS | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-AIO-HIPS-PUBLISH-ATOMIC | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-RT001-UNIQUE-EXECUTOR | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| KNOWN-FAILURES-BASELINE-VERIFY | f/l/w | any | N | N | Y | Y | — | 静态可红（未逐一复算） |
| UT-GAIA-ZLIB | f/l/w | any | N | N | Y | N | — | 需运行期（未静态复算） |
| CTEST-P1WCS-STD-F1-BRIDGE | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P2HIPS-UNITS | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P2HIPS-DETERMINISM | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P2HIPS-NEGATIVE | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P2HIPS-SELFCHECK | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| DOC-LINE-ANCHORS | f/l/w | any | N | N | Y | N | 必红：C4 13 条 BINDING_VIOLATION（本机 48 errors／干净检出 13） | 静态可红（未逐一复算） |
| IPV-PLATFORM-BINDING | l | linux | N | N | N | N | — | 静态可红（未逐一复算） |
| CTEST-GAIA-MANIFEST-BOUNDS | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-XPSD-SPECTRUM-COUNT-BOUNDS | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P1PHOT-FIXGATES | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P1STAR-MAD | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P1PSF-PRODPATH-CENTROID | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-IPV-EXTRACT-WCS-SIP-FAILCLOSED | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-IPV-MAG-ITER | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P1SNR-SCIENCE-ALL | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P1SNR-LINUX-ALL | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P1DRZ-TASKSET-INVARIANCE | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-P1STAR-ANGLE-GUARD | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-IPV-TRIANGLE-BUDGET | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-IPV-ABI-LAYOUT-LOCK | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-IPV-ABI-LAYOUT-LOCK-SELFCHECK | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-IPV-PARAMS-ABI-FAILCLOSED | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| CTEST-GAIA-MAGNITUDE-RANGE-BOUNDS | l | linux | N | N | N | N | — | 需运行期（未静态复算） |
| KNOWN-FAILURES-BASELINE-CHECK | l | linux | N | N | Y | Y | 连带必红：上述红门不在基线→fail-closed 新增失败 | 静态可红（未逐一复算） |
