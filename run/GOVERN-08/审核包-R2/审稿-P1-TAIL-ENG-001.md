# 审稿-P1-TAIL-ENG-001（第 1 遍 · 对抗审稿）

- 片号：`TAIL-ENG-001`（尾域合并 · eng/ 下层行数 < 2000 的小层）
- 基线：`/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`
- ⚠️ **基线漂移记录（完整性声明）**：审稿期间 HEAD 自行前进到 `1fa477a7`（2 个新提交：`bf25c085` 文档权威层对齐、`1fa477a7` 移除实验域运行结果归档）。**该漂移非本审稿人造成** —— 本人全程只执行只读命令，未做任何 git 写（无 add/commit/checkout/reset/stash）。经复核 `git diff --stat 850a9ede..HEAD -- <本片 13 份>` **输出为空**，即本片全部成员文件在漂移前后**逐字节未变**，故本报告全部结论对两个 HEAD 均成立。
- 计数口径（涉及判据处）：本片**不含门实例**；下文凡称「门」均指**声明的不变量/判据契约**，其「整改分母」为**全仓判据注册面 `eng/ci/checks.json`（HEAD 已不存在）**，故本片任何「门缺失」结论均不含分母。

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数（权威清单） | **13** |
| 实际读了几份 | **13** |
| 成员总行数（权威清单声明） | **1012** |
| 实际读了多少行 | **1012** |
| **覆盖率** | **13/13 份，1012/1012 行 = 100%** |

**未读完的：无。** 13 份全部用 `read` 工具逐行读完全文（未抽样、未跳读、未以 grep 代替阅读）。

逐份行数实测与清单声明**逐份吻合**，无缺项、无多算：

| # | 文件 | 行数（实测 = 清单） |
|---|---|---|
| 1 | `eng/cmake/cfitsio_platform.cmake` | 224 |
| 2 | `eng/cmake/install_layout.cmake` | 215 |
| 3 | `eng/cmake/ARCH-001-migration-manifest.md` | 140 |
| 4 | `eng/cmake/win32_pthread_shim/pthread.h` | 95 |
| 5 | `eng/build/toolchain.ps1` | 91 |
| 6 | `eng/cmake/cfitsio_sources.cmake` | 64 |
| 7 | `eng/build/build.sh` | 54 |
| 8 | `eng/cmake/qa_coverage_report.sh` | 36 |
| 9 | `eng/cmake/acsd.product.windows.json.in` | 26 |
| 10 | `eng/README.md` | 23 |
| 11 | `eng/cmake/README.md` | 21 |
| 12 | `eng/build/README.md` | 17 |
| 13 | `eng/cmake/win32_pthread_shim/unistd.h` | 6 |
| | **合计** | **1012** |

复跑命令：
```bash
cd "/workspace/Astro CS Database"
for f in eng/cmake/cfitsio_platform.cmake eng/cmake/install_layout.cmake \
  eng/cmake/ARCH-001-migration-manifest.md eng/cmake/win32_pthread_shim/pthread.h \
  eng/build/toolchain.ps1 eng/cmake/cfitsio_sources.cmake eng/build/build.sh \
  eng/cmake/qa_coverage_report.sh eng/cmake/acsd.product.windows.json.in \
  eng/README.md eng/cmake/README.md eng/build/README.md \
  eng/cmake/win32_pthread_shim/unistd.h; do printf "%6s  %s\n" "$(wc -l < "$f")" "$f"; done
```

---

## 2. 本片判定

### **阻断**

最重的 3 条：

**① `eng/build/build.sh` 是完全死掉的构建入口 —— 源目录已迁走，且仓库根解析错一层。**
`eng/build/build.sh:26` `cmake -S "$REPO/lib/phase2"`；`lib/phase2` 不存在（`ls -d lib/phase2` → 没有那个文件或目录），且 `eng/cmake/ARCH-001-migration-manifest.md:41` 自己记载 `lib/phase2 → lib/algorithms/coverage` 状态 **DONE**，`:50` 记载「`lib/` 顶层旧目录已清零」。
叠加：`REPO="$(cd "$(dirname "$0")" && pwd)"`（`:14`）把「仓库根」解析成 `eng/build`，故 `:26` 实际 configure 的是 `<root>/eng/build/lib/phase2`（**双重不存在**），`:18-19` 日志落 `eng/build/run/logs/`、`:21` `rm -rf eng/build/build/…`、`:40` `cd eng/build`（而 `:7` 注释称须在仓库根运行）——对照 `eng/build/toolchain.ps1:21` 正确上跳两级。
⚠️ **一处必须精确的修正（见 §7 第 14 条）**：`:33-34`/`:42-43` 的 5 个目标 `phase2_synthetic_gate` / `phase2_ivar_wiring` / `phase2_execution_options` / `phase2_routing` / `phase2_async_io` **名字全是对的** —— 它们已随迁移移到 `lib/algorithms/coverage/CMakeLists.txt:111,114,128,130,146`，且该目录在活动构建图内（`CMakeLists.txt:1480 add_subdirectory(lib/algorithms/coverage)`）。**死因是 `-S` 路径与 `REPO` 根，不是目标不存在。**（另注 `lib/algorithms/coverage/CMakeLists.txt:1478` 载明 GTest 缺失时这些目标零注册。）
`eng/build/README.md:13` 仍把它宣传为「Linux 侧 configure/build/test 入口」，且其描述的 `build/linux-<type>`、日志落 `run/logs/`、「从仓库根运行测试」三项**逐条与代码矛盾**。

**② `eng/cmake/ARCH-001-migration-manifest.md:136` 用一条已被推翻的假断言，把现役构建文件豁免出验收门。**
原文：「**明确结论：该文件属「兼容图」，不参与任何构建目标**（根 CMakeLists.txt 无 `add_subdirectory(cli)`，cli 相关 target 由根 CMake 直接以显式源清单声明）」。
实测：`CMakeLists.txt:1317` **存在** `add_subdirectory(lib/infrastructure/cli)`；`lib/infrastructure/cli/CMakeLists.txt:21,28,33,38` 声明 4 个 INTERFACE target（`acsd_cli_subcommands` 及 normalize/mosaic/export），并被 `CMakeLists.txt:1358` 链接进 `acsd`。两条断言**双双为假**。
后果：一条以「不参与构建」为由的豁免，正保护着一个**现役**文件的旧路径残留；而豁免所依据的「43 处旧路径命中」现已实测为 **0 处**，即豁免理由已完全失效却在继续生效。
这是「退役对象自证无消费者」的**镜像形态** —— 对象已复活，消费豁免未撤销。

**③ `eng/cmake/cfitsio_platform.cmake` 声明的三条不变量（I1/I2/I3）核查器已被物理删除，本片无任何门禁执行面。**
`:23` 把 `eng/ci/check_cfitsio_platform_surface.py` 登记为 I1/I2 的静态核查器；`:28-30` 声称「两条都是静态可判定的」、`:34` 给出统计命令。
实测：`eng/ci/` **整个目录不存在**，`.github/` 亦不存在。根因可复核：`e5f589a6`「G08-01 物理删除旧门禁与 CI（344 件 / -136676 行）」，`git merge-base --is-ancestor e5f589a6 HEAD` → YES；该 commit 删 `eng/ci/` 下 **141** 个文件、`.github/` 下 **4** 个文件，含 `check_cfitsio_platform_surface.py` 本身。
⇒ 这是**已声明不变量 + 零强制点**：按 AGENTS.md §8「判据须在正确实现下绿、注入缺陷时红」，现在新增一个漏调 `acsd_cfitsio_apply_platform_shim` 的目标，**不会红**。
追加雷：`:20` 与 `:120` 把 `.github/workflows/ci-windows.yml:49` 当作「两条注入布局的**合同声明**」，该文件同样不存在 ⇒ zlib 布局合同锚在空处。

---

## 3. 逐文件清单

### 3.1 `eng/cmake/cfitsio_platform.cmake`（224 行）— **判：阻断**

- **读了什么**：全文。四个函数 `acsd_cfitsio_isolate_warnings`(64-78)、`acsd_openmp_link_if_unix`(80-93)、`acsd_cfitsio_apply_platform_shim`(103-116)、`acsd_cfitsio_apply_third_party_deps`(136-224)，含 22-34 行的判据抬头。
- **看到什么**：
  - `:23` 判据抬头点名核查器 → **核查器与 `eng/ci/` 全删**（阻断③）。
  - `:33-34` 给出的「统计值」命令**原样跑不通**：`rg: --glob: No such file or directory (os error 2)` × 8，EXIT=2，**stdout 为空**。根因：`--` 终止选项解析，其后 `--glob`/`-g`/glob 模式全部退化为路径操作数。⚠️ 空 stdout 意味着任何 `| wc -l` 式复核会得到「0 命中」⇒ **假绿**。修好后（选项前置）实测 13 命中 / 3 文件（`CMakeLists.txt:214,642,1369`；本文件 `:33,41,42,44,47,52,86,88,89`；`lib/algorithms/drizzle/CMakeLists.txt:133`），**全部在注释里，0 条真实 `-fopenmp` 旗标**。故 invariant「只应命中本文件」按字面为**假**（实为 3 文件），按实质（真实旗标）为**真**。
  - `:53-54` 声称「**7 个调用点**」并列举 `CMakeLists.txt` 三处 + `lib/algorithms/{calibration,cosmetic,drizzle}` 三处。实测调用点共 **6**：`CMakeLists.txt:643,685,780` + `lib/algorithms/calibration/CMakeLists.txt:75` + `lib/algorithms/cosmetic/CMakeLists.txt:96` + `lib/algorithms/drizzle/CMakeLists.txt:141`。**数字 7 与其自身列举（3+3=6）自相矛盾。**
  - `:184-194` 的 fail-fast 兜底 c) 条件是 `elseif(_acs_zroot)` —— **fail-fast 的触发条件与它要防的故障模式共用同一个变量**。`ACS_ZLIB_ROOT` 未设时 `_acs_zroot` 为空 ⇒ 既不进 `:184` 也不进 `:186` ⇒ **静默全空返回**，正落回 `:12` 自称已消灭的 `C1083 "zlib.h"`（详见 §5 反例 1）。
  - `:206-207` `find_library(... HINTS "${_acs_zroot}/lib" NO_DEFAULT_PATH)`，`_acs_zroot` 为空时展开为真实绝对路径 `HINTS "/lib"`。
  - `:114` `target_link_libraries(${tgt} PUBLIC kernel32)` 与 `:110` `target_include_directories(${tgt} PRIVATE …)` **无 INTERFACE_LIBRARY 分支**，而同文件 `:213-215` 的 B-2 注释刚写明该形态对 INTERFACE_LIBRARY 会「configure 期硬失败」—— 同一坑在同一文件留了一半。
  - `:96-97` `ACSD_WIN32_PTHREAD_SHIM_DIR` 用 `CACHE INTERNAL`（不覆盖既有项），`:110` 使用点无 `if(NOT EXISTS …)` 存在性校验。
  - `:216-217` 声称命中 `lib/infrastructure/gaia_xpsd_client/CMakeLists.txt:106`；`:106` 确为 `add_library(acsd_gaia_zlib_include INTERFACE)` 声明行，实际**调用**在 `:122`。引用指向声明而非调用（轻微）。
- **判定**：阻断（判据无门 + fail-fast 失效 + 多处计数/引用错误）。

### 3.2 `eng/cmake/install_layout.cmake`（215 行）— **判：须修**

- **读了什么**：全文，含 install 规则全集（64-213）与 1-41 行契约抬头。
- **看到什么**：
  - `:39`「本文件是唯一 install() 集合; 其余文件绝不 install」。实测另有 3 个文件含 `install(`：`lib/infrastructure/aio/third_party/cfitsio/CMakeLists.txt:301,308,328,336,348,368,426,428`、`lib/infrastructure/pipeline/orchestrator/cpp/third_party/json-schema-validator/CMakeLists.txt:57`、`lib/infrastructure/aio/third_party/cfitsio/cmake/portfile.cmake:42`。**三者在活动构建图中均不可达**（无任何 `include()`/`add_subdirectory` 指向）⇒ 该 claim **实质成立**（仅措辞绝对化）。但 vendored `cfitsio/CMakeLists.txt:15` 仍有 `PROJECT(CFITSIO)` / `CMAKE_MINIMUM_REQUIRED`，是一枚休眠地雷。
  - `:41`「机器校验: `eng/packaging/verify_install_tree.py`」→ **MISSING**（`git ls-files` 空，`e5f589a6` 删除；仅余 `eng/packaging/__pycache__/check_packaging_consistency.cpython-313.pyc` 残骸）。
  - `:146` 同一缺失脚本；`:146` 的 `eng/tests/abi/mod001_install_load_check.py` **存在**，但其 `:269` 与 `:350` 断言 `len(munits) == 10`，而两份 manifest 实为 **17** unit ⇒ 该「加载验证」运行必红。
  - `:168`「由 `eng/packaging/check_packaging_consistency.py` C2/C6 机器判红」→ **MISSING**（同 commit 删除）。
  - `:201`「供 `eng/tools/quality/check_module_map.py` 消费」→ **MISSING**（同 commit 删除）。
  - `:3` 与 `acsd.product.windows.json.in:6` 引作契约正本的 `03_TARGET_PRODUCT_AND_ARCHITECTURE.md` **仓内不存在**（来自仓外控制包 V7）。
  - `:148` 注释含**日期**「owner 裁决 2026-09-11」与历史叙事「V7 残留断链解除」⇒ 违反 AGENTS.md §5「正文无日期…无历史叙事」。
  - `:149` 理由错误：称 `acsd_p1_noise` 摘出因「该子图 CMakeLists 未入库」，实测该子图**已入库**（`lib/algorithms/noise_snr/CMakeLists.txt:39` 有 `add_library(acsd_p1_noise SHARED …)`），真因是 `CMakeLists.txt:383` 的 `add_subdirectory` 被注释。
  - `:206-209` `string(REGEX REPLACE …)` 静默替换，无命中判据。
  - `:73-161` 11 个 SHARED target 只给 `LIBRARY`+`RUNTIME`，全仓无 `ARCHIVE DESTINATION`（**本轮禁止编译，未实测**）。
- **判定**：须修（白名单闭合的机器强制点全灭 + 注释含日期/理由错误/多项悬空）。

### 3.3 `eng/cmake/ARCH-001-migration-manifest.md`（140 行）— **判：阻断**

- **读了什么**：全文 5 节（状态总览 35+3 行、同步义务核对 35 行、等价性证据 6 行、INT-001 待办 6 行、锚点同步 4 行、越界登记 4 行）。
- **看到什么**：
  - `:5`「当前 HEAD：`8f0a4c6b…`」—— 实测 HEAD `850a9ede…`，**落后 844 个提交**（`git rev-list --count 8f0a4c6b..HEAD`）。且违反 AGENTS.md §5「正文无 commit」。
  - `:4`「生成：`run/PROJECT-GOVERNANCE-01/ARCH-001/gen_final_manifest.py`（**可复跑**）；状态由 moves.json 现场计算，禁止手改」—— `run/PROJECT-GOVERNANCE-01/` **整个目录不存在**，`gen_final_manifest.py` 与 `moves.json` 均无。**「可复跑」为假**。
  - `:3`「权威：docs/ACSD_DESIGN.md **7.1（顶层结构唯一）**/7.3（模块与 DLL-SO 边界）」—— 实测 `docs/ACSD_DESIGN.md:397` 是 `### 7.1 命令树`（正文为 `normalize --json` 等命令，**全文不含任何 `algorithms/` 路径**）；`:427` 的 §7.3 是「错误传播与日志」；顶层结构在 `:483 ### 8.4`，模块边界在 `:520 ### 8.5`。⇒ **两条权威引用全错，35 个「依据」格的 `7.1 algorithms/<x>` 全部指错节**（典型裸从句伪引：主项完全没有引号，只有 `:3` 冒号后的裸从句）。
  - `:136` cli 豁免前提被推翻（详见 §2 阻断②）。
  - `:116`「infrastructure/observability 为空（仅 PENDING.md）」—— 实测含 `logging/`、`monitoring/`、`probes/`、`README.md`，HEAD 有 14 个 tracked 文件。
  - `:119`「lib/infrastructure/cli/**（9 文件）」—— 实测 **33** 个 tracked 文件。
  - `:101-106` 等价性证据 7 项（`logs/00_baseline_build.log`、`60_final_build.log`、`61_ctest.log`、`71_renames.txt`、`9*_driver*.log`、`verify_pathonly2.py`）**逐项 MISSING**，仓根无 `logs/` 目录 ⇒ `:108`「等价性由…支撑」**不可证伪**。
  - `:128` `python3 docs/algorithms/anchors/check_doc_line_anchors.py` —— `docs/algorithms/` 不存在，`docs/science/algorithms/anchors/` 亦不存在，`git ls-files | grep check_doc_line_anchors` = 0。
  - `:93`「docs/**：已同步 2136 处（120 文件）**`docs/science/algorithms/**` 未动**」与 §4b `:125`「23 文件 128 行」口径并列，未说明 120 是否含这 23。
  - **属实无需改的部分**：`:46-48` 的 3 个 DEFERRED Session 目录实测仍存在（`lib/phase1_session`、`lib/phase2_session`、`lib/phase3_session`），与 `:50` 一致；DEFERRED 判定本身未被推翻。
- **判定**：阻断（权威链指错节 + 不可复跑 + 证据全灭 + 用假断言豁免现役文件）。

### 3.4 `eng/cmake/win32_pthread_shim/pthread.h`（95 行）— **判：须修**

- **读了什么**：全文，含 `pthread_mutex_t`(14-16)、attr(18-20)、宏(22-23)、`_InterlockedExchange` 声明(25-33)、五个 mutex API(35-58)、`strtok_r`(62-95)。
- **看到什么**：
  - `:35-39` `pthread_mutex_init` 首行 `(void)a;` —— **属性被静默丢弃**；`:14-16` 结构体只有 `volatile long locked`，**无 owner、无递归计数** ⇒ 结构上**不可能**是递归锁。`:23` 注释自认「shim 近似非递归」，`:41` 自认「无递归」。
  - 上游 `lib/infrastructure/aio/third_party/cfitsio/cfileio.c:83` 原文注释 `/* Init the main fitsio lock here since we need a a recursive lock */` —— **上游明确要求递归锁**（含其 `a a` 双写笔误）。契约被违背。
  - `:40-46` 自旋体为空，无 `SwitchToThread`、无 `_mm_pause`、无超时、**永远 `return 0`** ⇒ 违约即**永久 100% CPU 挂死**，而非返回错误码。
  - `:23` 只定义 `PTHREAD_MUTEX_RECURSIVE`，**未定义** `PTHREAD_MUTEX_RECURSIVE_NP`。
  - `:5` 注释称「仅用于 cfitsio 编译 (include 路径最先注入)」；实测 `_REENTRANT` 在 4 个重编译点间**不一致**（见 §3.5）。
- **判定**：须修（静默丢弃契约 + 失败模式为挂死）。

### 3.5 `eng/cmake/cfitsio_sources.cmake`（64 行）— **判：建议（本片最干净的一份）**

- **读了什么**：全文，`set(ACSD_CFITSIO_SOURCES …)` 共 60 条目。
- **看到什么**：
  - `:2`「证据源: lib/infrastructure/aio/third_party/cfitsio/*.c（命中 **60**）」—— 实测该目录 `.c` 共 **71** 个；60 是生成器 `eng/tools/gen_cfitsio_list.py` 的 `EXCL` 表**排除 11 个之后**的数。措辞误导：读者无法从 `.cmake` 自查该排除表。
  - 清单条目数 = **60**，与 `:2` 一致；**幽灵条目（列了但盘上无）= 0** ⇒ 无链接错误风险；无重复；条目已排序。
  - 生成器 `eng/tools/gen_cfitsio_list.py` **存在**（tracked）—— `:1` 的引用为真。
  - 该目录残留 **61 个 `.o`**，`git ls-files | grep -c '\.o$'` = **0** ⇒ 未入库，仅脏工作树，非仓内缺陷。
- **判定**：建议（仅措辞与可自查性；数据面正确）。

### 3.6 `eng/build/build.sh`（54 行）— **判：阻断**

见 §2 阻断①。要点：`:26` 源目录 `lib/phase2` 已迁至 `lib/algorithms/coverage` 故不存在（**双重不存在**，因 `:14` 把根解析成 `eng/build`）；`:14` 仓库根解析错一层；`:5` 文档写 `build/linux-<type>` 而 `:17` 实为 `build/run-${BUILD_TYPE,,}`；`:9-10` 引 `REAUDIT_V3/v3_exec/G5_linux_prebuild_baseline.md`（**仓内不存在**，且子代理追出其历史真实路径为 `reports/REAUDIT_V3/v3_exec/…` —— 即脚本连历史上都写错，漏了 `reports/` 前缀；自认「已退役」⇒ 违反 AGENTS.md §5）；`:53` `BLD-001_RESULT` 与 `:54` 退出码口径自洽；失败方向是 **fail-closed**（`:12 set -uo pipefail` + `:26 if ! cmake` ⇒ exit 1），**不假绿**。
⚠️ **目标的准确性**：`:33-34`/`:42-43` 的 5 个 target **名字正确**，已迁至 `lib/algorithms/coverage/CMakeLists.txt:111,114,128,130,146`。**我原稿称其「全仓零定义」是错的**（首次 grep 用了 `--include` 形式误报空），已更正，见 §7 第 14 条。
`:27` 传的 `-DP2_ENABLE_OPENMP=ON` 有对应 `option(P2_ENABLE_OPENMP ... OFF)`（`lib/algorithms/coverage/CMakeLists.txt:28`）⇒ **此项不是陈旧项**（否决子代理，见 §7 第 15 条）。

### 3.7 `eng/build/toolchain.ps1`（91 行）— **判：建议**

- **读了什么**：全文，`param`(14-16)、`$ACSD_ROOT`(21)、preset 名(22-23)、`Test-ACSDToolchain`(25-58)、`Build-ACSDAll`(60-71)、`Test-ACSDAll`(73-81)、分派(83-91)。
- **看到什么**：
  - `:21` 正确上跳两级得仓库根（与 `build.sh:14` 形成对照）。
  - `:22-23` preset 名 `win-msvc-17.14.39-x64` / `win-rel` —— **实测全部存在**：`configurePresets` 含前者，`buildPresets` 与 `testPresets` 均含后者。`:76` `ctest --preset $ACSD_BUILD` 有对应 testPreset。**此项无问题（我先前的怀疑被自己的取证推翻）**。
  - `:43-46` 4 条 vendored 路径 + `:86` 的 `eng/packaging/windows/README.md` —— **实测 5 条全部 EXISTS**。
  - `:6`「ENGINEERING_SPEC §7/§10」→ `ENGINEERING_SPEC.md` **全仓不存在**（悬空权威）。
  - `Test-ACSDToolchain`(25-58) 校验工具与 4 条 vendored 路径，**不校验它自己依赖的两个 preset 名** ⇒ preset 一旦改名，`check` 仍全绿而 `build` 失败（覆盖面错位）。
- **判定**：建议（脚本功能面正确；仅权威悬空 + 自检覆盖面错位）。

### 3.8 `eng/cmake/qa_coverage_report.sh`（36 行）— **判：须修**

- **读了什么**：全文。
- **看到什么**：
  - `:6` 文档称 `$2 source dir（覆盖统计只统计 lib/ 与 **lib/infrastructure/cli/**）`；`:32`/`:34` 实参为 `"$SRC_DIR/lib"` 与 `"$SRC_DIR/cli"`。实测 CLI 实际在 `lib/infrastructure/cli`，**仓根无 `cli/` 目录** ⇒ 文档口径与代码口径不一致，第二个 source 参数指错位置。
  - `:5` 文档称 build dir 含 `eng/tests/unit/*_test`；`:20` 实为 `"$BUILD_DIR"/tests/unit/*_test` ⇒ 层级不一致。
  - `:8` `set -eu` + `:17` `"$COV_DIR"/*.profraw`：无 profraw 时 glob 不展开，字面量传给 `llvm-profdata` ⇒ 报错退出（fail-loud，非 fail-open）。
  - `:24-27` 无 `*_test` 时显式 `exit 1`（fail-loud）。
  - 全仓无任何调用者（CI 目录已删、无 CMake/CI 引用）⇒ 疑为死脚本。
- **判定**：须修（文档与代码口径不符 + 无消费者）。

### 3.9 `eng/cmake/acsd.product.windows.json.in`（26 行）— **判：须修**

- **读了什么**：全文，17 个 unit（`:8-24`）。
- **看到什么**：
  - `:3-4` `@ACSD_BASE_VERSION@` / `@ACSD_GIT_COMMIT@` 变量**确有定义**（`CMakeLists.txt:69-70`、`:83`），`configure_file(@ONLY)` 由 `install_layout.cmake:193-196` 调用 ⇒ 注入链完整。
  - 与 `eng/packaging/acsd.product.json` 逐项比对：unit_id 集合相同、顺序相同；字段差异**仅 `rel_path` 平台后缀**；`module_id`/`status`/`kind`/`abi_version`/`sha256` 零差异 ⇒ `:6` 的「与 Linux 侧一一对应」「units 5→10 与 Linux 骨架同步」**属实**。
  - **17 个 unit 的 `sha256` 全为 `null`**。schema `eng/packaging/schemas/acsd-product.schema.json:24` 允许 null、`:17` required 不含它 ⇒ 合法。但**无任何活代码校验 sha256**；唯一相关的 `eng/tests/abi/mod001_install_load_check.py:280` 反而**接受 null 也接受 key 缺失**，`:316` 探针**自己重算** sha256，从不回读 manifest ⇒ 填了也不比对，不填照样绿。
  - **两份 manifest 都通不过自己随包安装的 schema**：`acsd-product.schema.json:22` `"module_id": {"type": "string"}`（不含 null），而两份 manifest 各有 **11** 个 unit 显式写 `"module_id": null`；`:20` `kind` enum `["exe","runtime","io","module","provider"]` **无 `"manifest"`**，而 `MANIFEST-BACKENDS`/`MANIFEST-PROVIDERS` 两个 unit 用 `"kind": "manifest"`（而 `install-tree-contract.schema.json:25` 的 enum **有** `"manifest"` ⇒ 两份 schema 不同步）。
  - `:6` note 含**日期**「owner 裁决 2026-09-11」⇒ 违反 AGENTS.md §5。
  - `:6` 引的 `03 §4` 指向仓外不存在的 `03_TARGET_PRODUCT_AND_ARCHITECTURE.md`。
- **判定**：须修（对自带 schema 不合规 + sha256 校验为空转）。

### 3.10 `eng/README.md`（23 行）— **判：须修**

- **读了什么**：全文。
- **看到什么**：
  - `:7` 把 `run/` 列入「放」，`:8` 又把 `run/` 列入「不放：CI 与验收的产物和证据（artifacts/、run/）」—— **同文件内自相矛盾**。
  - `:7,13` 把 `ci/` 描述为「机器门注册表 checks.json 与确定性执行器 run_checks.py，及各检查器脚本」⇒ **`eng/ci/` 整个目录不存在**，文档把已删除的门禁层写成现有内容。
  - `:20` 称 `run/` 为「eng 侧本地运行工作区目录骨架」⇒ `eng/run/` 存在但 `find eng/run -type f | wc -l` = **0**（空骨架）。
  - `:24`「ENGINEERING_SPEC.md §7、§10」→ 该文件**全仓不存在**。
  - `:9`「CMake 入口本体…留在仓库根」—— 属实（`CMakePresets.json` 在根）。
- **判定**：须修（自相矛盾 + 把已删门禁层写成现有）。

### 3.11 `eng/cmake/README.md`（21 行）— **判：须修**

- **读了什么**：全文。
- **看到什么**：
  - `:15` 列「`toolchain/` —— 平台工具链 CMake 片段」⇒ **`eng/cmake/toolchain/` 不存在**。实测 `eng/cmake/` 实际内容仅 8 项：`acsd.product.windows.json.in`、`ARCH-001-migration-manifest.md`、`cfitsio_platform.cmake`、`cfitsio_sources.cmake`、`install_layout.cmake`、`qa_coverage_report.sh`、`README.md`、`win32_pthread_shim/`。⇒ 悬空目录项。
  - `:12-18` 内容清单**漏列 `cfitsio_platform.cmake`** —— 而它恰是本目录体量最大、被 `:1` 自称「**唯一实现**」、被 4 个 CMakeLists 引用的核心文件。
  - `:22`「ENGINEERING_SPEC.md §1、§7」→ 该文件**全仓不存在**。同行的「docs/ACSD_DESIGN.md §8.4（顶层结构·eng/cmake）」**属实**（`:483-490` 确含 `eng/cmake/`）。
- **判定**：须修（悬空目录项 + 漏列核心文件 + 悬空权威）。

### 3.12 `eng/build/README.md`（17 行）— **判：须修**

- **读了什么**：全文。
- **看到什么**：
  - `:13` 把 `build.sh` 宣传为「Linux 侧 configure/build/test 入口：在 build/ 下配置与构建，从仓库根运行测试，日志落 run/logs/」⇒ 全部三项与代码矛盾（§3.6）。
  - `:18`「ENGINEERING_SPEC.md §7（仓库根固定条目登记 eng/build/build.sh 与 eng/build/toolchain.ps1…）」→ 该文件**全仓不存在**；且其宣称的登记事实已失效（`build.sh` 指向已删目录）。
  - `:9`「脚本…不引用机器绝对路径」—— 属实（`:21` 动态求根）。
- **判定**：须修（广告的入口已死 + 悬空权威）。

### 3.13 `eng/cmake/win32_pthread_shim/unistd.h`（6 行）— **判：须修**

- **读了什么**：全文（仅 6 行：`#pragma once` + `_WIN32` 守卫 `#error`）。
- **看到什么**：
  - `:2` 注释「空 shim — cfitsio 仅用它声明 unlink/access 等, **实际未调用这些分支**」⇒ **被证伪**。`eval_l.c:930-935` 在 `#ifndef YY_NO_UNISTD_H` 下 include `<unistd.h>`（MSVC 下无条件成立），而同一 TU 在 `:2355` 调用 `isatty( fileno(file) )`；`group.c:28` include、`:5674` 调用 `getcwd`，而其 `<direct.h>` 门控 `group.c:23` 是 `#if defined(WIN32) || defined(__WIN32__)` —— **MSVC 只定义 `_WIN32`，两个宏均不成立** ⇒ Windows 上连 `direct.h` 都拿不到。
  - 仓内**已存在一份完整正确的映射**：`lib/infrastructure/aio/third_party/cfitsio/win_compat/unistd.h`（948 B，13 条 define，覆盖 `getcwd`/`isatty`/`close`/`read`/`write`/`unlink`/`access`/`ftruncate`/`fsync`）⇒ 实测**不在任何 include 路径上**（`rg -l win_compat -- '*.cmake' CMakeLists.txt'` 零命中）。**正确的那份永不生效，接线的是空的那份。**
  - ⚠️ **本轮不断言 MSVC 链接期已失败**：仓内唯一 MSVC 冷构建日志（`run/FINAL-07/审核包/工程/Windows腿/A-coldbuild-raw.log`）编译通过但**未走到链接**。按 `eng/tests/support/acsd_test_posix_compat.h:19,252` 的仓内自定口径，空 shim 把 `isatty` 静默提升为「隐式声明 + 寄望符号存在」，属该头明令禁止的**伪等价物**。登记为须修 + 待 Windows 线实测。
  - `:1` 称「个别 .c (eval_l.c) include」—— 这一句**在 MSVC 下是对的**（另 3 个 include 点均被 unix 门控挡掉），应予承认。
- **判定**：须修（注释断言被证伪 + 空 shim 让正确实现失效）。

---

## 4. 发现清单

### 阻断（4）

| # | 发现 | 位置 |
|---|---|---|
| B1 | 声明的 I1/I2/I3 不变量**核查器与注册表已被物理删除**，零强制点；恢复旧核查器还会因 `astrocs_*` vs `acsd_*` 函数名不匹配而**恒真变绿** | `eng/cmake/cfitsio_platform.cmake:23-34`；`lib/algorithms/drizzle/CMakeLists.txt:77` |
| B2 | 用「不参与任何构建目标」这一**已被推翻的断言**，把现役文件 `cli/CMakeLists.txt` 豁免出「旧路径零命中」验收门 | `eng/cmake/ARCH-001-migration-manifest.md:136` vs `CMakeLists.txt:1317` |
| B3 | Linux 构建入口**完全死掉**：`-S` 指向已迁走的 `lib/phase2`（叠加 `:14` 把仓库根解析成 `eng/build`，实为**双重不存在**）；文档描述的三项逐条与代码矛盾。失败方向 fail-closed，**不假绿，但一个文件都编不出来** | `eng/build/build.sh:5,14,17-21,26,40` |
| B4 | 权威引用**指错节**：§7.1 实为「命令树」、§7.3 实为「错误传播与日志」；35 个依据格全部指错；生成器与 7 项等价性证据全灭 ⇒ 整份 manifest 不可复跑、不可证伪 | `eng/cmake/ARCH-001-migration-manifest.md:3,4,5,101-106,128` |

### 须修（13）

| # | 发现 | 位置 |
|---|---|---|
| R1 | fail-fast 条件 `elseif(_acs_zroot)` 与它要防的故障模式**共用同一变量** ⇒ `ACS_ZLIB_ROOT` 缺省时静默全空返回，落回抬头自称已消灭的 `C1083 zlib.h` | `eng/cmake/cfitsio_platform.cmake:184-194,206-207` |
| R2 | 空 `unistd.h` 让 `isatty`/`getcwd` 无声明，而**仓内已有一份完整的 `win_compat/unistd.h` 却在任何 include 路径外** | `eng/cmake/win32_pthread_shim/unistd.h:2` |
| R3 | `pthread_mutex_init` 静默丢弃 attr，结构上不可能递归；上游 `cfileio.c:83` 明写「need a recursive lock」；违约失败模式是**永久挂死**而非报错 | `eng/cmake/win32_pthread_shim/pthread.h:14-16,23,35-39,40-46` |
| R4 | `_REENTRANT` 跨 4 个 cfitsio 重编译点**不一致**：`acsd_p1_drizzle`、`acsd_p1_hips_writer` 编入全部 60 个 TU 却**不定义** `_REENTRANT` ⇒ 其 `FFLOCK` 退化为空、`ffstrtok` 退化为非线程安全 `strtok`，产品二进制内并存一份**无锁** cfitsio 副本；而 `hips/CMakeLists.txt:64` 注释谎称「源集只编 fitsio.h 消费面」，与其上 8 行的 `:56-59` 无条件全量循环**直接矛盾** | `lib/algorithms/drizzle/CMakeLists.txt:68-71`（`_REENTRANT` 0 处）；`lib/algorithms/drizzle/hips/CMakeLists.txt:56-59` vs `:64` |
| R5 | 白名单闭合的机器强制点全灭：`verify_install_tree.py`、`check_packaging_consistency.py`、`check_module_map.py` 三脚本均已删除，注释仍称「机器校验」「机器判红」 | `eng/cmake/install_layout.cmake:41,146,168,201` |
| R6 | 两份产品 manifest 都**通不过自己随包安装的 schema**，三类违规：① 顶层 `"note"` 是 `additionalProperties: false`(:31) 下的**未声明属性**；② `module_id: null` ×11 违反 `:22` `type: string`；③ `kind:"manifest"` ×2 不在 `:20` enum 内（而 `install-tree-contract.schema.json:25` **有** `"manifest"` ⇒ 两份 schema 互不同步）。因无任何活代码加载该 schema，违规永远不会浮现 | `eng/packaging/schemas/acsd-product.schema.json:20,22,31` |
| R7 | sha256 全 null **且强制点自相矛盾**：生产 loader `lib/infrastructure/pipeline/module_loader/secure_loader.c:499` `if (u->expected_sha256.size > 0)` —— 期望值为空即**整段 hash 比对被跳过**；而唯一在册验证器 `mod001_install_load_check.py:280` **主动要求** `sha256 in (None,"missing")`。⇒ 交付产品的模块**零产物完整性约束**，且 `install_layout.cmake:147` 的「签名清单」语义仅是名义 | `secure_loader.c:499`；`mod001_install_load_check.py:280`；`install_layout.cmake:147` |
| R8 | `mod001_install_load_check.py` 断言 `units == 10`，实际 17 ⇒ 运行必红；它正是 `install_layout.cmake:146` 的唯一落地验证 | `eng/tests/abi/mod001_install_load_check.py:269,350` |
| R9 | `ENGINEERING_SPEC.md` **全仓不存在**，却被本片 **6 处**当权威上游引用 | `cfitsio_platform.cmake:18`；`ARCH-001:3`；`toolchain.ps1:6`；`eng/README.md:24`；`eng/cmake/README.md:22`；`eng/build/README.md:18` |
| R10 | 三份 README 描述已删除/不存在的内容：`eng/README.md:7,13` 把 `eng/ci/` 写成现有；`eng/cmake/README.md:15` 列不存在的 `toolchain/` 且**漏列核心文件** `cfitsio_platform.cmake`；`eng/build/README.md:13` 广告已死的 `build.sh` | 三份 README |
| R11 | `qa_coverage_report.sh` 文档口径（`lib/infrastructure/cli`）与代码实参（`$SRC_DIR/cli`）不符 —— 实测 `$2` 为仓库根，故实参指向不存在的 `<root>/cli` | `eng/cmake/qa_coverage_report.sh:6,20,32,34` |
| R12 | `qa_coverage_report.sh:20` 的对象 glob **只匹配顶层** `"$BUILD_DIR"/tests/unit/*_test`，而真实构建树中大量测试二进制位于子目录（`aio/`、`p1_cal/`、`p2_rej/` 等）；`:24-27` 的守卫**只在零对象时**报错 ⇒ **部分丢失完全不可见**，覆盖基线数字静默失真 | `eng/cmake/qa_coverage_report.sh:20,24-27` |
| R13 | `qa_coverage_report.sh:32/:34` 的 `--sources "$SRC_DIR/lib"` 把 vendored 第三方扫进统计（`lib/` 下 `.c/.cpp` 中约 14% 是 `lib/infrastructure/aio/third_party` 的 cfitsio），与「lib+cli」的声明口径冲突 | `eng/cmake/qa_coverage_report.sh:32,34` |

### 建议（9）

| # | 发现 | 位置 |
|---|---|---|
| S1 | `:53-54` 称「7 个调用点」，实为 **6**（且其自身列举也只有 6） | `eng/cmake/cfitsio_platform.cmake:40,53-54` |
| S2 | `:33-34` 的「统计值」命令**逐字执行 exit 2、stdout 空**（`--` 吞掉后续全部选项）⇒ 空输出可致假绿；修好后 invariant「只应命中本文件」按字面为假（实为 3 文件 13 处，全在注释，0 条真实旗标） | `eng/cmake/cfitsio_platform.cmake:33-34` |
| S3 | `:20`/`:120` 的 zlib 布局「合同声明」`.github/workflows/ci-windows.yml:49` 不存在 | `eng/cmake/cfitsio_platform.cmake:20,120` |
| S4 | `:114` `PUBLIC kernel32` 与 `:110` 缺 INTERFACE_LIBRARY 分支 —— 同文件 `:213-215` 已自证该形态会 configure 硬失败（同一坑修一半） | `eng/cmake/cfitsio_platform.cmake:110,114` |
| S5 | `:216` 引用 `:106`（声明行）而非实际调用行 `:122` | `eng/cmake/cfitsio_platform.cmake:216` |
| S6 | `:2`「命中 60」措辞误导（原始 glob 命中 71，60 是排除 11 之后）；排除表只存在于生成器，`.cmake` 无任何线索 | `eng/cmake/cfitsio_sources.cmake:2` |
| S7 | `:148` 与 `windows.in:6` 注释含日期「2026-09-11」与历史叙事，违反 AGENTS.md §5；`install_layout.cmake:149` 理由错误（子图已入库，真因是 `CMakeLists.txt:383` 注释掉 add_subdirectory） | `eng/cmake/install_layout.cmake:148-149`；`acsd.product.windows.json.in:6` |
| S8 | `eng/README.md:7` 把 `run/` 列入「放」、`:8` 又列入「不放」，同文件自相矛盾 | `eng/README.md:7-8` |
| S9 | `build.sh:9-10` 引 `REAUDIT_V3/…`（不存在，且自认「已退役」）；`:5` 文档写 `build/linux-<type>` 而 `:17` 实为 `build/run-<type>` | `eng/build/build.sh:5,9-10,17` |

---

## 5. 你主动构造的反例

### 反例 1（最强）：`ACS_ZLIB_ROOT` 缺省 ⇒ 函数静默全空返回，而 cfitsio 仍需 `zlib.h`

- **构造什么**：取「MSVC + 全新 checkout + `ACS_ZLIB_ROOT` 未设（env 空、无 `-D`）」这一配置，沿 `cfitsio_platform.cmake:136-224` 逐行走查：
  1. `:142` `if(NOT MSVC) return()` → 不返回；
  2. `:146-149` `_acs_zroot = ""`；
  3. `:155` `if(_acs_zroot)` 假 ⇒ 跳过注入点探测；
  4. `:164`/`:167` `ZLIB_INCLUDE_DIR(S)` 未设 ⇒ 跳过回退②；
  5. `:184` `if(_acs_inc)` 假 → `:186` `elseif(_acs_zroot)` **也假** ⇒ **FATAL_ERROR 不触发**；
  6. `:203` 走 else → `:206` `find_library(... HINTS "/lib" NO_DEFAULT_PATH)` → NOTFOUND；
  7. `:212` 假 ⇒ **什么都没注入，零诊断**。
- **期望推翻什么**：推翻 `:12`（「同一目标自编 zcompress.c/zuncompress.c 却没有 zlib 包含面 ⇒ C1083 "zlib.h"」已解决）与 `:134`（「fail-fast：不允许把错误布局静默传给编译期变成 C1083」）两条断言。`zcompress.c`/`zuncompress.c` 确在 `cfitsio_sources.cmake:62-63` 清单内。
- **是否推翻**：**推翻成功。** 该函数在**最可能的默认路径**上静默返回，精确复现它声称防住的缺陷。
- **为何今天没炸**（诚实记录）：纯属**排序侥幸** —— `CMakeLists.txt:350` 先 `add_subdirectory(lib/infrastructure/gaia_xpsd_client)`，其 `find_package(ZLIB REQUIRED)` 抢先把 `ZLIB_INCLUDE_DIR` 灌进 cache，回退②才碰巧活着；根 `CMakeLists.txt` **自己从不调用 `find_package(ZLIB)`**。一个与 cfitsio 毫无关系的模块的 add_subdirectory 顺序，在替 cfitsio 的不变量兜底。

### 反例 2：cfitsio init 失败 → 非递归自旋锁 → 永久挂死（而非返回错误码）

- **构造什么**：追 `cfileio.c:4404` `FFLOCK;` 持锁 → 错误出口先调 `ffpmsg` 再 `FFUNLOCK` → `fitscore.c:685 ffpmsg` → `:690 ffxmsg` → **`fitscore.c:766 FFLOCK;`**（同线程再取同一把非递归锁）。
- **期望推翻什么**：推翻 `pthread.h:23,41` 的「近似非递归无害」定性 —— 若上游声明的递归契约被需要，失败模式应为报错而非挂死。
- **是否推翻**：**推翻成功，且我已独立复核。**
  - 自查：`sed -n '4404,5327p' cfileio.c | grep -c "ffpmsg("` = **31**；样本 `cfileio.c:4466-4469` 确为 `if (status) { ffpmsg(...); FFUNLOCK; return(status); }`。
  - `fitscore.c:766` 确为 `FFLOCK;`；`fitscore.c:685 ffpmsg` → `:690 ffxmsg` 链确认。
- **范围诚实收窄**：31 处全部落在 `fits_init_cfitsio` 一个函数内；`need_to_initialize` 仅 `cfileio.c:49`（初值 1）与 `:5325`（置 0）两处赋值，**永不重置回 1** ⇒ **不存在稳态（post-init）递归锁路径**。危害是：一次本该返回错误码的 init 失败，在 shim 下变成永久挂死（CI 表现为无输出超时而非红灯）。**不断言「运行中会随机挂死」。**

### 反例 3：`ARCH-001` 的验收门豁免可否被推翻

- **构造什么**：假设 `cli/CMakeLists.txt` 真是「兼容图、不参与构建」，则 `ARCH-001:136` 的豁免成立；去构建图里找它。
- **期望推翻什么**：推翻该豁免的**前提**。
- **是否推翻**：**推翻成功。** `CMakeLists.txt:1317 add_subdirectory(lib/infrastructure/cli)` 存在；`cli/CMakeLists.txt:21,28,33,38` 定义 4 个 INTERFACE target；`CMakeLists.txt:1358` 链接之。且豁免所依据的「43 处旧路径命中」实测已为 **0 处** —— 前提与理由**双双失效**，豁免却仍在生效。

### 反例 4：`cfitsio_sources.cmake` 是否漏了源（会致链接错误）

- **构造什么**：比对清单 60 条与磁盘 `.c` 集合。
- **期望推翻什么**：若清单漏源 ⇒ 链接错误。
- **是否推翻**：**未能推翻 —— 清单是干净的。** 磁盘 71 个 `.c`，清单 60 条，**幽灵条目 0**；11 个差额是生成器 `EXCL` 表刻意排除的 Fortran 包装/VMS 桩/Windows dump 测试/需特殊宏的驱动，与上游自带 `cfitsio/CMakeLists.txt:230,252-254` 的取舍一致。仅 `:2` 的「命中 60」措辞误导（原始 glob 命中 71）。

### 反例 5：`-fopenmp` 唯一性判据能否被证伪

- **构造什么**：逐字执行 `cfitsio_platform.cmake:33-34` 给出的命令。
- **期望推翻什么**：该命令是文件自证的「可复跑统计值」。
- **是否推翻**：**推翻成功。** EXIT=2、stdout 空、8 条 `No such file or directory`。**空 stdout 是最危险的形态**：任何 `| wc -l` 式复核得「0 命中」⇒ 假绿。修正后实测 13 命中 / 3 文件，其中 **0 条非注释** ⇒ invariant 按字面为假（3 文件）、按实质为真（0 条真实旗标）。该命令**无法区分注释与真实旗标**，故即便修好也不足以充当判据。

---

## 6. 盲复算

**方法**：先写下本片「若只看机器信号会得到什么结论」，再去掉既有判定、仅凭原文重新取证。

| 项 | 既有/权威判定 | 我的盲复算 | 结论 |
|---|---|---|---|
| `逐份判定-权威版.csv` 对本片 13 份 | 全部 `HUMAN` / 理由「**默认保留（非产出面或非数据形态）**」 | 逐份读完后，13 份中 **4 份阻断、6 份须修** | **既有判定偏松（严重）** |
| 「判据代码绿 ⇒ 实现正确」 | — | 本片**零个判据被执行面**（`eng/ci/` 已删），故「绿」在此**不是证据而是空洞**；正确性只能靠重算 | **偏松** |
| `install_layout.cmake:39`「唯一 install() 集合」 | — | 另 3 文件含 `install()`，但活动图中不可达 | **判一致**（实质成立） |
| `acsd.product.windows.json.in:6`「与 Linux 侧一一对应」 | — | 17↔17，差异仅 `rel_path` 后缀 | **判一致**（成立） |
| `toolchain.ps1:22-23` preset 名 | 我**起初怀疑**版本串漂移会静默失效 | 实测 configure/build/test 三类 preset 均存在 | **我推翻了自己的怀疑 ⇒ 判一致** |
| `cfitsio_sources.cmake:2`「命中 60」 | — | 原始 glob 命中 71 | **偏严的部分要收回**：清单本身正确，仅措辞误导 |

**净判定：既有机器判定对本片「偏松」**，且其宽松是**结构性的** —— `逐份判定-权威版.csv` 用「非产出面或非数据形态」这一与内容无关的理由把整片默认保留，恰好放过了本片最重的一类问题（**已删除的门禁层留下的悬空引用**）。这与负责人「判据不可信、不必核实为通过」的裁定方向一致，但成因不同：不是判据假绿，而是**分片判定层从未读过这些文件**。

---

## 7. 子代理派发记录

**派发 6 次 / 5 个独立片核验范围**（其中 2 次因我在同一消息内重复投递而重复，非有意）。全部为只读，均未被授予任何写权限。

| 子代理 | 范围 | 状态 |
|---|---|---|
| `3c2112a0` / `bee12211` | packaging 引用面 + 两份 manifest 三方比对 | ✅ 报告（1 份） |
| `d3f963cd` / `ce8a05fc` | cfitsio 平台门 I1/I2/I3 + 反例构造 | ✅ 报告（2 份，内容重叠） |
| `28b91112` | build.sh / toolchain.ps1 / qa_coverage_report.sh | ✅ 报告（**推翻了本报告初稿**） |
| `a437c5be` | pthread.h / unistd.h / ARCH-001 红队复审 | ✅ 报告 |
| `bee12211` | packaging 引用面 + 两份 manifest 三方比对（`3c2112a0` 的重复投递） | ✅ 报告（结论与 `3c2112a0` 一致，另补 2 类新违规） |

### 逐条复核与**否决**记录

| # | 子代理结论 | 我的裁决 | 依据 |
|---|---|---|---|
| 1 | `3c2112a0`：两份 manifest 有 **12** 个 unit 写 `module_id: null` | ❌ **否决（数字错）** | 实测 `grep -c '"module_id": null' eng/cmake/acsd.product.windows.json.in` = **11**；其列举的行号（8,9,10,17,18,19,20,21,22,23,24）本身也是 11 个 |
| 2 | `d3f963cd`：`:199-201` 早退会跳过 include 目录 | ❌ **否决** | `:185 target_include_directories` 在 `:198-201` **之前**已执行，早退只跳链接面，设计合理 |
| 3 | `d3f963cd`：`:222` 的 `_acs_inc_scope` 复用是缺陷 | ❌ **否决** | `:178-183` 无条件赋值，到 `:222` 必然可见；INTERFACE 库用 INTERFACE 关键字语义正确 |
| 4 | `ce8a05fc`：`--` 之后 token 全部退化为路径 ⇒ 8 条 os error、EXIT=2、stdout 空 | ✅ **采纳并独立复核** | 我自己执行得同一组报错；根因正确 |
| 5 | `a437c5be`：**证伪我方 CLAIM 2** —— 缺 `PTHREAD_MUTEX_RECURSIVE_NP` **不构成编译错误**，MSVC 走 `#else` 分支用已定义的 `PTHREAD_MUTEX_RECURSIVE` | ✅ **采纳 —— 这是对我自己假设的否决** | 我独立确认：`cfileio.c:91 #ifdef __GLIBC__` / `:95 #else`；MSVC 只定义 `_WIN32` 不定义 `__GLIBC__`；`fitsio2.h` 零引用 `_NP`。**我原稿把它列入"可能阻断"是过度声称，已撤回** |
| 6 | `a437c5be`：CLAIM 3 存在可达触发点（31 处 `ffpmsg` 错误出口 → `ffxmsg` → `FFLOCK` 自死锁），但**无稳态嵌套** | ✅ **采纳并独立复核** | 我自查 `sed -n '4404,5327p' cfileio.c \| grep -c ffpmsg(` = **31**；`fitscore.c:766` 确为 `FFLOCK;`。**并据此把本片最强反例从"稳态可能挂死"收窄为"init 失败路径挂死"** —— 这是我主动接受的降级 |
| 7 | `a437c5be`：unistd.h「实际未调用」被证伪；仓内 `win_compat/unistd.h` 完整但未接线 | ✅ **采纳并独立复核** | `eval_l.c:935` include + `:2355` `isatty`；`group.c:23` 门控用 `WIN32`/`__WIN32__`（MSVC 不定义）+ `:5674` `getcwd`；`win_compat/unistd.h` 存在且 `rg -l win_compat` 零命中 |
| 8 | `a437c5be`：同上报告称 `eval_l.c` 的 include 归属**正确**（另 3 处被 unix 门控挡掉） | ✅ **采纳（对子方的修正性让步）** | 我接受这一局部：`:8` 关于 `eval_l.c` 的那半句为真，不应一并推翻 |
| 9 | `a437c5be`：`acsd_p1_drizzle` / `acsd_p1_hips_writer` 编入全部 60 TU 却未定义 `_REENTRANT` | ✅ **采纳，但我把它加强了** | 我独立复核到更强的一层：`hips/CMakeLists.txt:64` 注释自称「源集只编 fitsio.h 消费面」，与其上 8 行 `:56-59` 的**无条件全量 `foreach`+`target_sources`** 直接矛盾 ⇒ 该注释本身为假 |
| 10 | `3c2112a0`：install() 单一源「**成立**」 | ✅ **采纳（附条件）** | 我独立枚举出另 3 个含 `install(` 的文件；同意其「活动图中不可达」判定，故判**实质成立**，但措辞绝对化 |
| 11 | `3c2112a0`：`03_TARGET…` 悬空属**已知 OPEN 项**（`FIX_LEDGER.csv:671` M8a-I-005） | ⚠️ **降级采纳** | 属实则为已知项，但 `install_layout.cmake:3` / `windows.in:6` **仍在拿它当契约正本**，故我仍记为须修，只标注「非本轮新发现」 |
| 12 | `3c2112a0`：`ARCHIVE DESTINATION` 缺失「Windows 必需」 | ⚠️ **保留但标注未实测** | 本轮禁止编译，无法验证 MSVC generate 阶段行为；我按纪律**不断言**，仅登记待前台实测 |
| 13 | `28b91112`（build.sh / toolchain.ps1 / qa_coverage_report.sh） | ✅ 报告 —— **并推翻了本报告的初稿** | 见下第 14/15 条 |
| 14 | **本报告自纠**：初稿称 `build.sh:33-34` 的 5 个测试目标「**全仓零定义**」 | ❌ **我自己错，已更正** | 子代理指出后我复核：`lib/algorithms/coverage/CMakeLists.txt:111,114,128,130,146` 确实定义了这 5 个目标，且 `CMakeLists.txt:1480 add_subdirectory(lib/algorithms/coverage)` 使其在活动图内。**我初稿的 grep 用了 `--include` 形式，误报为空。** 已把 §2 阻断①、§3.6、§4 B3 三处改为准确表述：死因是 `-S` 路径 + `REPO` 根解析，不是目标不存在 |
| 15 | `28b91112`：`build.sh:27` 的 `-DP2_ENABLE_OPENMP` 是第三处陈旧项，「全树无任何 `option(P2_ENABLE_OPENMP)`」 | ❌ **否决** | 我复核到 `lib/algorithms/coverage/CMakeLists.txt:28 option(P2_ENABLE_OPENMP "Enable OpenMP parallel sampler (default OFF, hotfix revert)" OFF)`，另有 `docs/engineering/BUILD_GRAPH.md:111` 佐证。**该项不成立** |
| 16 | `28b91112`：`qa_coverage_report.sh:20` 的对象 glob 只匹配顶层，静默漏掉约 38% 的测试二进制 | ✅ **采纳为 R12** | 与其「守卫只在零对象时报错 ⇒ 部分丢失不可见」的机制描述一并采纳 |
| 17 | `bee12211`：两份 manifest 另有一类违规 —— 顶层 `"note"` 在 `additionalProperties: false` 下未声明 | ✅ **采纳并独立复核** | `acsd-product.schema.json:31 additionalProperties: false`；`:7 required` 不含 `note`；两份 manifest 各含 1 处 `"note"` |
| 18 | `bee12211`：`secure_loader.c:499` 在期望 hash 为空时**整段跳过比对** | ✅ **采纳并独立复核** | `sed -n '495,505p'` 见 `if (u->expected_sha256.size > 0) {` ⇒ 与全 null manifest 合成**零产物完整性约束** |
| 19 | `28b91112`：`REAUDIT_V3/…` 的历史真实路径是 `reports/REAUDIT_V3/v3_exec/…`，脚本连历史上都写错 | ✅ **采纳（增强 S9）** | 与我独立得到的「仓内不存在」一致，并补出「历史路径也错」这一层 |
| 20 | `28b91112`：Windows `toolchain.ps1` 是目前**唯一还能用**的文档化入口；`check` 全绿但不校验 preset 名 | ✅ **采纳** | 与我独立复核的 preset 三类均存在一致；「check 绿 / build 红」是诊断体验问题（`:64` 捕获 exit code，fail-closed），非静默放行 |
| 21 | `28b91111`：「`cmakelists.txt` 是活动构建图的一部分」类表述 | ✅ 归入 R2 的 `ACSD_PLATFORM_SHARED_TARGETS` 观察 | 该全局属性 `install_layout.cmake:45-47` 只列 6 个目标、漏 8 个已交付 SHARED 库；且全仓零读者 |

**否决统计**：共复核 **21 条**，**否决 5 条**（第 1、2、3、15 条为子代理结论错误；**第 14 条是我自己的错误，由子代理推翻**），**采纳 11 条**，**降级/条件采纳 3 条**，**自我更正 1 处**。其中第 5 条是子代理反过来否决了我自己的假设（`PTHREAD_MUTEX_RECURSIVE_NP`），第 14 条是子代理推翻了我对 `build.sh` 的初稿定性 —— **两处我均已在正文中显式标注撤回**，不做静默修补。

**方法学教训（自记）**：本次唯一一处**我自己**的实质错误，根因是**用 grep 的形式正确性冒充了 grep 的结论正确性** —— `rg --include=CMakeLists.txt <显式路径>` 在该调用形态下静默返回空，我未察觉「空输出」这一信号本身可疑（与我在 S2 里批评被判据犯的正是同一个错）。凡 grep 返回空，一律须用第二种写法交叉验证后才可写入结论。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
git -c core.quotepath=false rev-parse HEAD      # 期望 850a9edefd47434b9ab71bc907c3de1e0814b303

# ── 覆盖率：13 份 / 1012 行 ──
for f in eng/cmake/cfitsio_platform.cmake eng/cmake/install_layout.cmake \
  eng/cmake/ARCH-001-migration-manifest.md eng/cmake/win32_pthread_shim/pthread.h \
  eng/build/toolchain.ps1 eng/cmake/cfitsio_sources.cmake eng/build/build.sh \
  eng/cmake/qa_coverage_report.sh eng/cmake/acsd.product.windows.json.in \
  eng/README.md eng/cmake/README.md eng/build/README.md \
  eng/cmake/win32_pthread_shim/unistd.h; do
  printf "%6s  %s\n" "$(wc -l < "$f")" "$f"; done | tee /tmp/t.txt
awk '{s+=$1} END{print "TOTAL="s}' /tmp/t.txt              # 期望 TOTAL=1012

# ── B1 判据无门 ──
ls eng/ci/ 2>&1                                            # 期望：没有那个文件或目录
ls .github/ 2>&1                                           # 期望：没有那个文件或目录
ls eng/ci/check_cfitsio_platform_surface.py 2>&1
git merge-base --is-ancestor e5f589a6 HEAD && echo "e5f589a6 IS ancestor"
git -c core.quotepath=false show --name-status --diff-filter=D e5f589a6 -- eng/ci | grep -c '^D'   # 141
git -c core.quotepath=false log --diff-filter=D --oneline -- eng/ci/check_cfitsio_platform_surface.py

# ── B2 验收门豁免前提被推翻 ──
rg -n "add_subdirectory\(lib/infrastructure/cli\)" CMakeLists.txt
rg -n "add_library\(acsd_cli_subcommand" lib/infrastructure/cli/CMakeLists.txt
grep -cE "lib/(phase2_rej|phase2_samp|phase2_upm|phase3_fits|phase2_int|phase3_proj|phase3_rsmp|common|drizzle|hips|healpix_db|astro_image_io|io|orchestrator|backend_host|gaia_xpsd_client|calibration|cosmetic|star_detector|dynamic_psf|plate_solve|photometric_calib|snr_estimator|acr|hips_p2|core)" lib/infrastructure/cli/CMakeLists.txt   # 期望 0
ls -d lib/phase2 2>&1                                      # 期望：没有那个文件或目录

# ── B4 权威指错节 ──
rg -n "^### 7\.1|^### 7\.3|^### 8\.4|^### 8\.5" docs/ACSD_DESIGN.md
sed -n '397,400p' docs/ACSD_DESIGN.md                      # §7.1 = 命令树
sed -n '397,436p' docs/ACSD_DESIGN.md | grep -c algorithms   # 期望 0
ls run/PROJECT-GOVERNANCE-01 2>&1                          # 期望：没有那个文件或目录

# ── R9 悬空权威 ──
find . -name "ENGINEERING_SPEC.md" -not -path "./build/*" -not -path "./run/*"   # 期望无输出

# ── 反例 1：zlib fail-fast 条件与故障模式共用同一变量 ──
sed -n '184,194p;203,212p' eng/cmake/cfitsio_platform.cmake

# ── 反例 2：31 处 ffpmsg 在 FFLOCK..FFUNLOCK 区间内 → 自死锁 ──
D=lib/infrastructure/aio/third_party/cfitsio
sed -n '4404,5327p' $D/cfileio.c | grep -c "ffpmsg("        # 期望 31
sed -n '4466,4469p' $D/cfileio.c
sed -n '760,767p' $D/fitscore.c
grep -n "need_to_init" $D/cfileio.c                        # 仅 :49 与 :5325，永不重置

# ── R3：pthread shim 丢弃递归契约 ──
sed -n '14,16p;23p;35,46p' eng/cmake/win32_pthread_shim/pthread.h
sed -n '83p' $D/cfileio.c                                   # 上游原文 "need a a recursive lock"

# ── R2：空 unistd.h vs 未接线的完整映射 ──
sed -n '930,935p;2355p' $D/eval_l.c
sed -n '23,28p' $D/group.c
rg -l "win_compat" -g '*.cmake' -g 'CMakeLists.txt' .      # 期望无输出（未接线）

# ── R4：_REENTRANT 跨重编译点不一致 ──
grep -c "_REENTRANT" CMakeLists.txt lib/algorithms/integration/phase2_integrate/CMakeLists.txt lib/algorithms/drizzle/CMakeLists.txt lib/algorithms/drizzle/hips/CMakeLists.txt
sed -n '56,64p' lib/algorithms/drizzle/hips/CMakeLists.txt

# ── R6/R7/R8：manifest vs 自带 schema ──
sed -n '17,26p' eng/packaging/schemas/acsd-product.schema.json
grep -c '"module_id": null' eng/cmake/acsd.product.windows.json.in eng/packaging/acsd.product.json   # 11 / 11
grep -c '"kind": "manifest"' eng/cmake/acsd.product.windows.json.in                                  # 2
grep -c '"unit_id"' eng/cmake/acsd.product.windows.json.in                                           # 17
sed -n '269p;350p' eng/tests/abi/mod001_install_load_check.py

# ── R5：已删脚本 ──
for p in eng/packaging/verify_install_tree.py eng/packaging/check_packaging_consistency.py eng/tools/quality/check_module_map.py; do
  echo "$p tracked=$(git -c core.quotepath=false ls-files "$p" | wc -l) ondisk=$([ -f "$p" ] && echo YES || echo NO)"; done

# ── S2：逐字执行被引的 rg 命令（期望 EXIT=2、stdout 空）──
rg -n -- '-fopenmp' --glob '**/CMakeLists.txt' --glob '**/*.cmake' -g '!build/**' -g '!run/**'; echo "EXIT=$?"
# 修正后（选项前置）：
rg -n -e '-fopenmp' --glob '**/CMakeLists.txt' --glob '**/*.cmake' -g '!build/**' -g '!run/**' . | grep -vE '^\S+:[0-9]+:\s*#' ; echo "NONCOMMENT_EXIT=$?"   # 期望空

# ── S1：调用点 7 vs 6 ──
rg -n 'acsd_openmp_link_if_unix\s*\(' -g '!build/**' -g '!run/**' . | grep -v '^./eng/cmake/cfitsio_platform.cmake'

# ── B3：build.sh 全死（路径 + 根解析；target 名本身正确）──
ls -d lib/phase2 2>&1                                        # 期望：没有那个文件或目录
ls -d eng/build/lib/phase2 2>&1                              # 期望：没有那个文件或目录（:14 REPO 解析错层）
for t in phase2_synthetic_gate phase2_ivar_wiring phase2_execution_options phase2_routing phase2_async_io; do
  printf "%-28s " "$t"; rg -n --no-ignore-vcs "add_executable\($t\b" -g '!build/**' -g '!run/**' . | head -1 || echo "UNDEFINED"; done
#   ⚠️ 不要用 `rg --include=CMakeLists.txt CMakeLists.txt lib eng` 的形式 —— 该形态会静默返回空
rg -n "option\(P2_ENABLE_OPENMP" -g '!run/**' .            # 期望命中 coverage/CMakeLists.txt:28（即 build.sh:27 的 -D 是有效项）
rg -n "add_subdirectory\(lib/algorithms/coverage\)" -B3 CMakeLists.txt
ls -d REAUDIT_V3 reports/REAUDIT_V3 2>&1
ls -d eng/build/run eng/build/build eng/build/lib 2>&1      # 期望全部不存在（零运行痕迹）

# ── R12/R13：qa_coverage_report 覆盖率统计失真 ──
sed -n '20,27p' eng/cmake/qa_coverage_report.sh
ls -d build/eng/tests/unit 2>/dev/null && { echo "top-level:"; ls build/eng/tests/unit/*_test 2>/dev/null | wc -l;
  echo "subdir:"; find build/eng/tests/unit -mindepth 2 -name '*_test' 2>/dev/null | wc -l; }

# ── 恒真/恒红双向体检（本片无判据执行面，故只能查声明）──
rg -n "check_|判据|核查器|机器校验|机器判红" eng/cmake/*.cmake eng/build/*.sh eng/build/*.ps1 eng/cmake/*.md eng/*/README.md
```

---

## 附：本片与「已固化检查项」的对账

| 检查项 | 本片是否命中 | 位置 |
|---|---|---|
| **恒真门三型** | 命中「往返自证型」变体：`:33-34` 自证命令产出**空 stdout**，任何 `wc -l` 复核得 0 ⇒ 假绿。**未发现代数恒等式型或结构对称型**（本片无判据代码可审） | `cfitsio_platform.cmake:33-34` |
| **判据体检双向（恒红）** | 本片无门执行面，**双向体检无从做起** —— 这本身就是结论 | `cfitsio_platform.cmake:22-34` |
| **自愈判据** | 未命中（本片无判据执行面） | — |
| **恒红/恒绿伪装成合规（fail-closed 伪装）** | 命中：`:184-194` 的 FATAL_ERROR 条件 `elseif(_acs_zroot)` 只在**变量已声明**时触发，而真正出错的**未声明**情形完全无诊断 ⇒ 「fail-fast」被伪装成合规 | `cfitsio_platform.cmake:186` |
| **伪引** | 命中**两型**：① 成对引号型 —— `cfitsio_platform.cmake:33-34` 被引命令原样跑不通；② **裸从句型（主项，扫成对引号会漏）** —— `ARCH-001:3` 冒号后裸从句「docs/ACSD_DESIGN.md 7.1（顶层结构唯一）」及 35 个依据格「7.1 algorithms/\<x\>」，被引节实为「命令树」 | `ARCH-001-migration-manifest.md:3` 及 §1 全部 35 行 |
| **判据读的是桩** | 命中：`INSTALL` 白名单的「机器校验」实为三个**已删除**的脚本；`sha256` 校验实为探针**自算**（从不回读 manifest） | `install_layout.cmake:41,146,168`；`mod001_install_load_check.py:316` |
| **筛掉真信号** | 命中：`:33-34` 的统计命令**先按 glob 筛文件再取命中数**，无法区分注释与真实旗标 ⇒ 被筛掉的恰是「0 条真实旗标」这条真信号 | `cfitsio_platform.cmake:33-34` |
| **代码改了、归档没重跑** | 命中（本片最集中）：`ARCH-001` 落后 **844** 个提交，`observability` 由空变 14 文件、`cli` 由 9 变 33、`add_subdirectory(cli)` 由无变有 | `ARCH-001-migration-manifest.md:5,116,119,136` |
| **悬空引用** | 命中（面最大）：`ENGINEERING_SPEC.md`（6 处）、`03_TARGET…`（2 处）、`eng/ci/*`（3 脚本 + 注册表）、`.github/workflows/ci-windows.yml`（2 处）、`REAUDIT_V3/…`（1 处）、`run/PROJECT-GOVERNANCE-01/…`（1 处）、`docs/algorithms/anchors/…`（1 处）、7 项 `logs/*` 等价性证据、`eng/cmake/toolchain/`（1 处） | 见 §4 |
| **退役对象仍有活调用者** | 命中**镜像形态**：对象已复活（`cli/CMakeLists.txt` 进入构建图），而「不参与构建」的**消费豁免未撤销**，仍在保护一个现役文件 | `ARCH-001-migration-manifest.md:136` vs `CMakeLists.txt:1317` |