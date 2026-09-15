# 复验期取证纪律（由 RC1/RC7 反噬我方条目的事故归纳，全轴共用）

- **F-1｜判缺失（absence）必须双宿主覆盖**：断言「X 不存在」前，须同时检索代码面、`contracts/**/*.json`（含 `contracts/data/*`）、`docs/**` 与 `INDEX.yaml`。我方三条「不存在」结论因漏检 `contracts/data/*.json` 被推翻（`M6b-E-007` 实测比原报**更重**：四件全缺 vs 原报三件）。
- **F-2｜并发判据要过三套标准**：C11 `_Atomic`、OpenMP `#pragma omp atomic`、平台锁。`M8-H-001` 只查前者 ⇒ 误报「裸 ++ 竞态」，实测 HEAD 与 BASE 均为写点 18／受原子保护 18／裸写 0，**在原报自指锚点上同样成立**。
- **F-3｜短数值码检索必须带词界**：`V3-N-01` 的「23 命中 `-14`」实为日期误匹配，按 `rc=` 词界后为 0。
- **F-4｜复算前先做空白归一**：`docs/TRACEABILITY.csv` 的「脏改」经 `--stat 188/188` 且忽略空白后为空，是**纯行尾差异**，不是内容改动。
- **F-5｜FIXED 须双证**：机制复算 **+ 载体存在性**（回归锁是否真注册、门是否真有载体）。RC7 组 B 交出三处「不存在的工件」型伪证据（`CTEST-PHASE2-UPM-GATE`、`astrocs_p2_upm_synthetic_test`、`tools/quality/check_ci_registration.py`）与一处自相矛盾（`M4-F-03` 先后 FIXED/STILL），全部被片长实测推翻。
- **F-6｜全史无载体者按 `REJECT` 处理，不得记 FIXED**：`V12-N-14` 首例（`git log -S` 唯一命中是**该 finding 自身的提交**）。
- **F-7｜「有判据无载体」是一门之族**：`ci/validate_registry.py`(R11)、`ci/tests` 其余 19 个 `test_*.py`、`tools/quality/gen_module_readmes.py` 均无门载体 ⇒ 与 `M8-G-001`/`V4-N-04`/`V9-N-14` 同族并档；**这道防复发的门自己不执行**。
- **F-8｜时点须区分 HEAD 与工作树**：`ci/checks.json` HEAD 面 **136** 项、工作树 **135** 项——未提交删除 `CTEST-MON004-ENFORCEMENT`、`CTEST-RESOURCE-MONITOR-QUALITY`、`CTEST-COMPARE-PRODUCTS-QUALITY` 三道**非豁免** linux-main 门并新增两道 IPV 门。四件套删除在工作树内自洽（测试源/CMake/门/基线同步）⇒ **暂不入账**；若最终提交时面不同步，按 `W1-N-08` 形制立条。
- **F-9｜「文件被碰过」≠「缺陷已修」**：RC7 片区 61 条中 55 条宿主 diff 为空，6 条 FIXED 里 **5 条的修复提交经 `merge-base --is-ancestor` 实测为基线祖先** ⇒ 属我方定稿锚误而非整改；**本窗 63 个提交对该片净闭合数 = 1**。
- **F-10（我自己的事故）｜写共享文件必须原子**：标记脚本以 `"w"` 打开账本后才因缺列抛异常 ⇒ 文件被截断为部分行。已从 HEAD 恢复，并把所有账本改写改为**写临时文件 + 行数下限断言 + `os.replace`**。任何后续标记脚本沿用此形制。
