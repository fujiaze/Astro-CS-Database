# 第四批任务卡（W4，2026-09-16 前台立卡）

来源：`OPEN_ITEMS.md` A1 / A3 / A6 / A9。四张卡共用一个文件以免碎片化；执行行按自己的 ID 段落作业。

---

## W4-A3　门禁收口：判据 rebind + 退出码一致性（最高优先）

**问题（CI-003 实测，28 个执行单元 FAIL，全量 ctest 之外的机器面）**：
- **A 类 路径漂移**（判据对、路径旧）：`CTEST-REGISTRATION`（baseline_only 仍记迁移前路径）、`UT-VERSION`（与 `ci/check_version.py` 是两套不同步判据）、`V6-RUNTIME-CLOSURE`/`V6-CLI-MODE-ROUTING`/`V6-NEGATIVE-MUTATION`（oracle 驱动按旧 `cli/**` 调用）、`TRACEABILITY-MATRIX`/`CON-TRACEABILITY`/`UT-TRACEABILITY`（旧模块路径）。
- **B 类 判据过时/历史债**：`CON-DOC-SYMBOLS`（只登记函数签名，科学量符号天然不在面内）、`CON-API-CONTRACTS`（契约表登记类、抽取器只抽函数）、`CON-COMMENTS`、`UT-CONTRACTS`、`UT-GAIA-ZLIB`（rc=5 target 名失配）、`CON-FULL-INTEGRATION`（**未捕获 Traceback**）。
- **C 类 真缺陷（判据与退出码不一致 ⇒ 恒红或假绿）**：`WARNING-SUPPRESSION` 打印 `QA-001_PASS 生产警告=0` 却 **rc=2**；`DOC-LINE-ANCHORS` 打印 `DOC_LINE_ANCHORS_PASS 842 anchors` 却 **rc=2**（且 `EXEMPT:8` 未点名豁免对象）；`SERIAL-HEAVY-SELFTEST` flake（已由前台修为 `6ac16214`，验证后关闭）。
- 另：`ci/tests/test_impact_map.py` 4 条既有红（根因：`impact_map.checks` 引用 step id 而该测试只索引顶层 id）⇒ 立 `CHK-IMPACT-MAP`。

**要求**：逐条二选一——**改绑现行路径/判据**或**显式退役并登记能力去向**；C 类必须让「打印的结论」与「退出码」一致（并给负例注入证明能红）；所有新增/修改的门必须满足 `ENGINEERING_SPEC §8` 五条（可执行负例面 / fail-closed / 锚存活 / 注册表双向一致 / 裁决 named-ID）。**不得**用豁免或放宽断言。改动面：`ci/**`、`tools/**`、`docs/ci/**`、`tests/quality/**`。

**复现基线**：`run/PROJECT-GOVERNANCE-01/CI-003/logs/existing_reds_repro.txt`；双向验证工具 `ci/polarity_probe.py --gate <ID>`。

---

## W4-A1　像素中心契约口径（PSF 块 → 测光/SNR）

**问题**：`psf` 块输出的是 **index-is-center**（dpsf 原始）坐标，却直喂 PHOTOMETRIC/SNR，而 WCS 与其它消费者是 **FITS 1-based** ⇒ 该域存在独立的中心约定问题（SCI-WCS-001 §5a 口径边界 / DISP-WCS-006 家族）。

**要求**：① 先取证：列出每条消费链（psf→photometry、psf→snr、psf→导出）实际的坐标约定与差 1 的位置，给可复跑探针；② 判定唯一正确约定并给依据（FITS 标准 + 仓内既有契约 `star_coord_contract.h` 的先例）；③ 若需改实现，给**产品面影响评估**（坐标变化量、受影响测试/合同/工件、版本递增建议）与重锚清单；④ 新增/收紧失败即红的门；⑤ 不得以「两处都对」收尾——必须给唯一结论。

---

## W4-A6　同常数残留清理（科学常数只能有一种全精度写法）

**问题（SCI-FIX-NOISE 清单）**：`0.6745`（应 `0.6744897501960817`）在 photometry 9 文件 + ACR + 4 份文档；`1.4826`（应 `1.482602218505602`）在 8 个算法域 + 12 份文档 + tools/tests；`0.7316728`（应 `0.7316727929211932`）在 PSF 域 5 处；`docs/ARCHITECTURE.md:124` 与 `tools/docs_machine_consistency.py`（后者**源码路径已被迁移改坏**，需其 owner 修）。

**要求**：① **先判定是否同一科学口径**（部分落点是独立实现——benchmark baseline provider——或纯展示工具 `frame_qc_grid.py`；不同口径者**不得**强改，列出并说明）；② 同一口径者统一为全精度，截断值只允许出现在「≈」语境并标精度；③ 加机器门：`tools/docs_machine_consistency.py` 的检查路径修复 + 新增「同常数多写法」检测（能在注入截断值时判红）；④ 分域提交清单交前台。

---

## W4-A9　INT-001 剩余范围（架构收口）

**范围**：① 删除 3 个 `*_session` 目录（其能力已由流水线/调度器承接，逐条给能力去向）；② `lib/infrastructure/cli/**` 接线三步——`add_subdirectory`、include 4 目录、`astrocs_cli_subcommands` 链接（ROOT-008 交接清单 §6）；③ `cli/CMakeLists.txt` 最终处置（当前唯一保留项：非构建图 + GOV-001 的 VERSION 依据 + 3 条 CI 判据输入 ⇒ 若退役必须同步 `ci/check_version.py` 的 `CLI_CMAKE_REL` 两条 chain 判据与 `ci/root_manifest.json` 的 VERSION 保留理由）；④ 确认唯一 entrypoint = `lib/infrastructure/cli/main.cpp`。

**要求**：遵守 `DISPATCH 附录 F` 迁移纪律（**分批、每批可构建**）；每步给「改前 → 改后 → 依据」；`ninja 0 FAILED` 与 `ctest` 不低于 **426/425**（唯一既有红 `core_pipeline`）。
