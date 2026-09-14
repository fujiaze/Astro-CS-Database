# V6 片3 · P1 两条（证据可复现性与自述一致性）

### V6-N-04 冻结 SCI 文档的推导权威指向 gitignored 目录 ⇒ 干净检出无法复核任何一条新科学锚
- **ID** V6-N-04 ｜ **类别** G_GOV_GATE ｜ **优先级** **P1** ｜ **状态** OPEN
- **锚点证据**：`docs/science/CONTROL_WEIGHT_SNR.md:51` 现行写「推导与文献锚见 `run/release-rescue/science-phot/PHOTOMETRY_LITERATURE_REVIEW.md` §C.3.1」，而 `git ls-files run/` 只有 `run/.gitkeep`、`.gitignore:19` 是 `run/*`；`git log -S PHOTOMETRY_LITERATURE_REVIEW ccc7a933..HEAD` 命中 `b0353303`（即**授权订正 FROZEN SCI 的那一次**），基线版本文案 0 命中 ⇒ **该行是本批新写**。
- **量化面（不是孤例）**：V6 对 `ccc7a933..HEAD` 逐文件 added-line 扫 `run/[A-Za-z0-9_./-]+` ⇒ **50 处新增引用、14 个被改文件**，含：P4 割线迭代常数的「实测依据」（`ipv_select.h/.cpp`、`ipv_types.h`）、P5/P8 两个新锁的 **oracle 锚值来源**（`snr_frame_science.cpp:11`、`p1snr_science_test.cpp:7`、`tests/unit/p1snr/p1snr_linux_test.cpp:12-14`、`tests/unit/CMakeLists.txt`、`lib/snr_estimator/README.md:234`）、P1 gaia `REPORT`（`gaia_client.c:2053`）。
- **问题说明**：这与我原判 `V1-N-09`（oracle 脚本失踪、清单仍指向它）和簇 8「证据不可复现」**同一根因的规模化表现**——**锚有宿主之名、无宿主之实**。V6 特别申明其判据是**「锚必须有宿主」**，不是「与旧文档不一致」，我确认这点成立。**要害在于新测试的期望值本身来自不可复核文件**：锁能跑绿，但"绿"的含义在干净检出上不可验证（§12.3-1、§17.2、§16.2）。
- **依据条款**：宪章 §12.3（注释/锚须可机器核验）、§17.2 追溯完整、§16.2 清单可判别；AGENTS.md 目录规范（`run/*` 是工作区，**不是权威源**）。
- **建议处置**：①把这批被引用的 `run/**` 文档**入库**到 `docs/`（推导/文献评审归 `docs/science/` 或 `docs/algorithms/`，A/B 报告归 `reports/`）；②或把锚改写为**仓内可复算形式**（闭式常数 + 推导步骤，如 V1/V2 已给的那些）；③新增机器门：**任何 tracked 文件引用 `run/**` 作为权威依据 ⇒ 红**（与我建议的 C-12 证据新鲜度门合并实现，成本一次性）。**这条应并入 C-12 一并裁**。
- **取证口径** V6 逐文件 added-line 正则扫描（可复跑）+ 我复核 `git ls-files run/` ｜ **置信度** 高 ｜ **related** **V1-N-09**、`C-12`、`A-35`、簇 8、`V6-N-05`

### V6-N-05 README 自述与**同一提交**的 diff 直接矛盾：声明「未改仓库树 lib/core」，实际该提交给 `module_adapters.cpp` 加了 162 行
- **ID** V6-N-05 ｜ **类别** G_GOV_GATE ｜ **优先级** **P1** ｜ **状态** OPEN
- **证据**：`git show --stat 35c85f53` → `lib/core/src/module_adapters.cpp | 162 +++++++++++-`；而被**同一提交新增**的 `lib/phase1/noise/README.md:54-58` 写「`lib/core/src/module_adapters.cpp`（P7 独占）。本批**未改仓库树中的 lib/core**，改动以补丁交付：`run/perf-fix/P8-snr-linux/patches/out-of-scope/lib-core-snr-wiring.patch` …（仓库文件逐字节未变，见 REPORT §6）」。**HEAD 至今未订正**（`grep :54/:58` 仍在）。
- **问题说明**：这是 R-6a「自报量可复算 ⇒ **复算为反**」的最干净实例——不是数字过时、不是行号漂移，而是**同一提交内两处互斥**：一个说没改、diff 说改了 162 行。危害在于**审查者据此判断"交付闭合依赖 P8"的边界**（见 `A-40`）：若按 README，`lib/core` 侧 SNR 接线"不在本批"；实际已在。叠加 V6-N-04（引用不存在的 patch 路径与 REPORT），等于**交付说明本身不可信**。
- **依据条款**：宪章 §12.3、§16.2/§16.3（交付说明与实际改动一致）、§14.5（一次任务一个可独立验证的提交）。
- **建议处置**：①删/改 `README.md:54-58` 四行为实况（"lib/core 的 SNR 接线由本批 P8 完成，`+162` 行"）；②若确有"以补丁交付"的历史意图，须在提交说明里区分"已入库部分"与"待入库部分"，不得以未入库补丁的路径充当现状说明；③把「README 声称未改 X ⇒ 该提交 `--stat` 不得含 X」写成一条机器门（一行断言，纳入 `ci/checks.json` 非豁免项，与 `check_commit_stat` 同族）。
- **取证口径** V6 双命令对置（同提交 stat 与新增文本）｜ **置信度** 高 ｜ **related** **V6-N-04**、`A-40`（P5 单独不在根交付图 ⇒ 闭合依赖 P8，而 P8 的自述失实）、簇 10、`M3-E-001`
