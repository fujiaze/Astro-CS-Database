# R 层复验派单规格（已就绪，**等触发条件满足即发**）

- **触发三条件**：① `git rev-parse HEAD` 连续两轮采样不变；②真源白名单内工作树脏 = 0；③全部代理已交档（**已满足**：27 轴 + 14 域 + 四补轴 + E1–E4 + FD）。
- **本轮采样**：docs=295 refs=10913 exact=8160 mismatch=340 absent=409 sym_elsewhere=31 sym_absent=31 oob=8 ｜ changed=89 referenced=79 unreferenced=10 ｜ CMakeLists.txt=471 ｜ ci/checks.json=323 ｜ H1=b8d1eb04 ｜ dirty=15 ｜ findings件=234 ｜ FD条目=4

## 一、为什么需要 R 层（负责人原话拆解）
- 「对记录中有改动的文件，如果行变化了修订行，如果 bug 变化了也需要修订」⇒ 两件事：**行锚漂移修复** + **条目现状四态重判**。
- 「最好在另一个工作全部结束后，排一些 agent 重新审阅一遍有改动的文件」⇒ 工作清单已由 `_merge/CHANGED_FILES_WATCH.md` 生成（表 A 必复验 / 表 B 未点名=潜在漏报 / 表 C 按档案分布的工作量）。

## 二、R 分片（按改动面热度切，互斥不重叠）
| 片 | 负责改动文件（Top） | 复验主体档案 | 规模 |
|---|---|---|---|
| R1 | `CMakeLists.txt`(408 引用)、`cmake/**`、`tests/unit/CMakeLists.txt`(94) | M5a M5b M8a + L28c | 高 |
| R2 | `ci/checks.json`(301)、`ci/**`、`.github/**` | M5b M8 M9 + FD-G 系列 | 高 |
| R3 | `docs/contracts/DATA_SEMANTICS.md`(149)、`docs/standards/**`、`docs/contracts/**` | M2b M3 M6b M7 | 高 |
| R4 | `lib/core/src/module_adapters.cpp`(135)、`cli/**`、`runtime/**` | M1a M3b M5a + FD-F 系列 | 中 |
| R5 | `lib/astro_image_io/**`、`lib/hips/**`、`lib/drizzle/**` | M2a M4 M2b | 中 |
| R6 | `lib/gaia_xpsd_client/**`、`lib/phase2/**`、`lib/plate_solve/**`、`providers/**`、`lib/backend_host/**` | M9 M3 M8 + L28d/e | 中 |
| R7 | `tests/**`（含表 B 未点名的 3 个 backend 件）+ `docs/science/**` + `docs/algorithms/**` | M1a M6a M7 | 中 |
| R8 | **审计自身复验**：`问题扫描/**` 的 214 条 PATH_MISMATCH + 31 条 SYMBOL_ELSEWHERE 改锚，及 364+ ABSENT 三级复核 | 前台 + 本片代理 | 高 |

## 三、四条硬规则（逐字下发，不得省略）
1. **PATH_MISMATCH 与 SYMBOL_ELSEWHERE 两档不是撤条理由**（分别 214 / 31 条）：前者只改锚，后者补宿主文件锚。只有 ABSENT（存在性检索确认）或"缺陷已被修掉"才可撤条/改四态。
2. **机器文件一律用解释器解析**（python3/jq），**禁止用 read 的行文本解析结构化文件** —— `read` 对 >2000 字符单行截断，本会话已两次因此出错（M6b 的 CSV 一次、前台自己 JSON.parse 一次）。
3. **判"缺失 / 0 命中"必须走三级复核**（整句 grep → 放宽关键词 → 定点 read），且以**存在性检索**为准：字面串 0 命中只说明用词不同。**排除前缀必须按 `./x` 写**（前台自己漏点号导致排除全失效、扫进 12.4 万文件）。
4. **裸数字一律带口径号 + 时点**；"各域报数不一致"默认按并发漂移加注、**不判谁错**（checks.json 111→112→113→114 已证是同一曲线不同时点）。

## 四、四态判据与升级条件
- **仍成立 / 已被修复 / 部分修复（只报残余）/ 无法判定**。凡条目引用代码当前行为（注释摘录、常量、阈值、默认值、返回值、分支）**必须逐字重读重抄**，禁止只改行号不改判断。
- 已被修复者：从 `findings/` 移出并记入该域 `_merge`「已修复」表；**"修复但无回归锁"另立 F_TEST_GAP**（簇 9 的主力）。
- **P0 判据沿用 M8a 被我采纳的三条**：断言可为假 + 条文在位 + 后果达交付面；三缺一即降。
- **免重报**：SUMMARY 的「跨域主题 → 唯一总述」表（INDEX §三）列了 15 个主题的总述归属，**R 层不得另立同义总述**。

## 五、R 层的执行权限（沿用 E 层边界，含本轮两条新增禁令）
- 允许：白名单只读命令 + python3 解析 + `git --no-optional-locks` 系查询。
- **新增禁令 A**：对 `build/`、`build/asan/`、`build/linux-control/` 等**任何非自建构建树禁止一切 `ctest` 形态（含 `-N`）**；用例清单改静态解析 `CTestTestfile.cmake`/`build.ninja`/`add_test(NAME …)`。
- **新增禁令 B**：**不得跑 `ci/run.py` 的非 `--plan-only` 路径**（`:227` 的 `git()` 无 `--no-optional-locks`、`:460` 跑 `git status --porcelain` ⇒ 会刷新 `.git/index`，与并发 agent 冲突）。
- 判"只读"以**是否触碰共享树状态**为准，不以命令名义为准（D-06 的教训，尽管其归因已被撤回）。

## 六、需要 R 层顺手核实的 8 个悬项（我复验时留下的）
1. **L27-004 的前提**：其"测试与源用 `file(GLOB)` 同步"—— E1/E2 均实测**全仓 `file(GLOB` = 0** ⇒ 该条很可能整条不成立，须重判。
2. **M1a-D-002 重锚丢信息**：`ipv_select.cpp:56` 的 `206.265` 错 1000 倍子事实未落进位置列，补挂（L28d 报告）。
3. **L28e-E-002 枚数名单**：我实测零承载 **10 枚**（`docs/contracts/DATA_ARTIFACTS.md` 全文 distinct 29），须按"§1 schema 表行集"定最终名单；`DATA-REG-001`(97 承载) 与 `DATA-IMG-CAL-001`(145) **必须剔除**。
4. **`evidence/reports` 救回的 51 枚与范围锚救回的 51 枚的重合度**（L28e §4.2-3 未比对）。
5. **`ahpx/DEPRECATED.md`（tracked、自述废弃不入生产）** ⇒ M9 的 L24-009/010 影响面须按死代码面复核（E3）。
6. **`ci/known_failures.json` 键名是 `failures`**（不是 `known_failures`）——前台与 E1 各错一次，R 层脚本直接用 `d["failures"]`。
7. **`runtime/io/hips_core.c` 不属任何构建目标**（E4）⇒ 其 :122/:168/:415 三站点属"不可达"，与 B4-22 修复本体在位（`drizzle_engine.cpp:1661-1663`）一并复核可达性。
8. **FD-F-003 的 09-07 断点**：最后一次真跑 Windows C++ 用例的运行清单内**无 publish 用例** ⇒ 若 publish 面是 09-07 后新加，Windows 从未测过它（定档时须区分"退化"与"从未覆盖"）。
