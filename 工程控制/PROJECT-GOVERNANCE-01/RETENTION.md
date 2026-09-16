# ROOT-003 run/ 临时产物保留策略与清运（RETENTION）

任务：`ROOT-003 run/ 临时产物保留策略与清运`　执行者：执行 Agent（零 git 写权限）
生成时间：2026-09-16　权威：ENGINEERING_SPEC §7（`run/` = 临时产物/日志，gitignore）、CONTROL_PACK_SPEC §6.2/§9、ASTROCS_DESIGN §6.3

> **裁决记录（2026-09-16，提交 `5f891080`）**：`tasks/ROOT-003.md` 新增负责人裁决「`run/` 全部可清理，不再设 >1G 禁删线；判据 = 无活动引用 + 非本控制包执行期产物」，并追加放行前置条件。派发直令（「>1G 只出方案」）由调度员于同日明示作废。

## 1. 保留规则（按控制包/任务定义，不按目录名）

| 规则 | 适用对象 | 保留期 | 依据 |
|---|---|---|---|
| R1 引用即保留（硬约束） | 被 `reports/`、`evidence/`、`docs/`、`ci/`、`tests/`、`contracts/`、`tools/` 以 `run/<dir>/` 形式引用的路径 | 直到引用它的报告被归档或删除（本次 27 个目录全部保留） | 本轮 ROOT-003 裁决「保留清单是硬约束」 |
| R2 本控制包执行期产物 | `run/PROJECT-GOVERNANCE-01/**` | 保留到本控制包 FINAL-001 通过验收 | ROOT-003 禁碰项 |
| R3 tracked 文件 | `git ls-files run`（实测仅 `run/.gitkeep`） | 永不清运 | 任务禁碰项 |
| R4 未引用且 >7 天 | 未被 R1 引用、最后修改 ≥7 天前的一次性运行产物 | 保留期满即清运 | ROOT-003 禁碰项「删除最近 7 天内修改过的目录」的补集；建议默认保留期 14 天 |
| R5 未引用且 <7 天但可确证为一次性临时数据 | ① 所属控制包已不存在于 `工程控制/` 且不在当前任务表；② 合成测试夹具目录（`t_*` / `tmp_wcs*`）；③ 自述 stale 的临时目录 | 立即可清运（逐项留证） | ROOT-003 禁碰项例外条款「除非证明其为一次性临时数据且无引用」 |
| R6 未引用且 <7 天，无法确证 | 见 §6 | 保留待观察（下一轮复评） | 保守优先，避免误删 |
| R7 用户/测试数据区 | `GaiaDR3/`、`GaiaDR3SP/`、`BASS DR3/`、`testdata/` | 不属本任务，原位不动 | ROOT-003 裁决段 |

## 2. 被引用 → 必须保留清单（R1；逐条 `test -e` 证明）

| run/ 目录 | `test -e` | 引用者（tracked 文件 : 行） |
|---|---|---|
| `run/arch_audit_p1` | EXISTS | reports/review-package-20260915/03_缺陷账本/DEFECT_LEDGER.json:10031:  "position": ":正式运行只有 orchestrator.exe`（207 行、UNTRACKED）；同文本第二份 `run/reaudit_v3/run0 |
| `run/arch_tb_001` | EXISTS | tools/arch/check_thread_budget.py:34:    # ARCH-TB-001 (裁决 R-10 先实证后登记; 证据 run/arch_tb_001/logs/provenance_snippets.txt): |
| `run/baselines` | EXISTS | reports/review-package-20260915/code-baseline/README.md:5:    run/baselines/astrocs-src-1f7c9e81.tar.gz |
| `run/ci` | EXISTS | reports/review-package-20260915/03_缺陷账本/DEFECT_LEDGER.md:881:\| V16-N-01 \| G_GOV_GATE \| P2 \| V5 \| 已登记待修 \| 无 _merge 处置表（V/W/SA/补轴 验证波 \| OPEN \| —<br>reports/review-package-20260915/03_缺陷账本/DEFECT_LEDGER.json:18013:  "position": "66`（`\"run/ci/ci-baseline-001/ctest_full_pre.log（当前门卫关闭：168 目标中无 p1_no<br>evidence/v8_1_ci_control/tasks/V8-CI-010/TASK_RESULT.json:565:     "BUILD-GCC-RELEASE": "prerequisite 未满足（waivable）：run/ci/build-gcc-release 前置产物不存在" |
| `run/ci_repair` | EXISTS | ci/tests/test_ci_repair_round.py:311:                self.assertTrue(rc.startswith(("ci/", "tools/quality/", "run/ci_repair/")),<br>ci/ci_repair_round.py:46:WHITELIST_PREFIXES = ("ci/", "tools/quality/", "run/ci_repair/") |
| `run/configs` | EXISTS | reports/archive/stage2_fix_report.md:76::: 5) 回归 32 帧（如有 run/configs/stage2_gc_32red.json）<br>reports/archive/stage2_fix_report.md:77:build/phase2/astrocs-stage2.exe run/configs/stage2_gc_32red.json<br>reports/archive/stage2_fix_report.md:130:- 2-hips `run/configs/stage2_gc_2red_test.json` (gc_R_panel1_f01/02) → `run/phase2/v7/gc_2red.mosaic.hips` ** |
| `run/docconv001` | EXISTS | docs/owner/RELEASE_STATUS.md:10:> 命令日志见 `run/docconv001/logs/`）。<br>docs/owner/RELEASE_STATUS.md:83:> `run/docconv001/logs/{focused_rebuild_test.log,mod001_install_check.log,cli001_vpi.log}`；<br>docs/owner/CHANGE_REVIEW.md:115:本提交内的执行级证据（日志 `run/docconv001/logs/`）： |
| `run/graph` | EXISTS | tools/v6/v6_runtime_oracle.py:11:  R3  §3.2 三 Phase 隔离：CLI 命令面不得存在聚合式 run/graph/pipeline 入口。<br>tools/v6/v6_runtime_oracle.py:164:    """CLI 命令面不得有聚合 run/graph/pipeline 入口。""" |
| `run/local` | EXISTS | reports/CAT-GAIA-TEST-result.md:29:- run/local/agent_catgaia_test/：gaia_cat_test、gaia_cat_polar_ref、gaia_xpsd_fixture_gen、inc/、cat/{clean,truncdb,work<br>reports/CAT-GAIA-TEST-result.md:30:- 构建验证目录：run/local/catgaia-build（ctest -R gaia_cat → 9/9 Passed）<br>reports/bughunt_p1_batchA/B4_P1_FINAL_REPORT.md:42:## 产物 (run/local/bughunt_p1_batchA/) |
| `run/logs` | EXISTS | reports/archive/stage2_fix_report.md:74::: 预期：stdout 首行 [stage2] + run/logs/phase2/<YYYYMMDD>/stage2.log 非 3 字节，sampler progress 正常，落盘 mosaic + hips_v<br>reports/archive/stage2_fix_report.md:141:**限制与超时**: 全程 `timeout 600s` 分段轮询 (cmake/build 各 60/300s, stage2 各 300/600s)，日志落 `F:\F_final_*_out/err/exit.t<br>reports/archive/stage2_fix_report.md:151:**失败**: 紧接 `upm_fit` 后 `C:\Users\fujia\run32b.log` 末行即 `EC:1`（`stage2.cpp:350 p2_upm_materialize_dense` 返回 1  |
| `run/p1wcs_wcs003` | EXISTS | tests/unit/p1wcs/p1wcs_tests_apbp.cpp:7:// 六类验收覆盖 (本组全部落断言, 数值证据导出 run/p1wcs_wcs003/):<br>tests/unit/p1wcs/p1wcs_tests_apbp.cpp:431:    // 数值证据 + Astropy 交叉验证输入导出 (run/p1wcs_wcs003/)<br>tests/unit/p1wcs/p1wcs_tests_apbp.cpp:441:                                     : std::string("run/p1wcs_wcs003/" |
| `run/perf-fix` | EXISTS | reports/review-package-20260915/08_算法推导_drizzle.md:6:> 依据来源：P34（`run/perf-fix/P34-algo-research/`）、P35（`run/perf-fix/P35-algo-land/`）、<br>reports/review-package-20260915/08_算法推导_drizzle.md:7:> P22（`run/perf-fix/P22-drizzle-par/`）、P15a（`run/perf-fix/P15a-drizzle-det/`）、P17（`run/perf-fix/P<br>reports/review-package-20260915/08_算法推导_drizzle.md:30:\| 行顶点 WCS \| 7.099 \| 0.96% \| `run/perf-fix/P34-algo-research/logs/A2_fullframe.txt`; `drizzle |
| `run/phase2` | EXISTS | reports/archive/stage2_fix_report.md:130:- 2-hips `run/configs/stage2_gc_2red_test.json` (gc_R_panel1_f01/02) → `run/phase2/v7/gc_2red.mosaic.hips` **<br>reports/archive/stage2_fix_report.md:149:**控制采样与 UPM**: `control sampling (V13): controls=45696 observations=407535 candidates=566208 accepted=407535 <br>reports/archive/stage2_fix_report.md:151:**失败**: 紧接 `upm_fit` 后 `C:\Users\fujia\run32b.log` 末行即 `EC:1`（`stage2.cpp:350 p2_upm_materialize_dense` 返回 1  |
| `run/realdata` | EXISTS | tools/realdata/README.md:5:**只读 testdata/ 与 GaiaDR3*/GaiaDR3SP/**，产物一律写 `run/realdata/`。<br>tools/realdata/README.md:19:产物（`run/realdata/`，gitignore）： |
| `run/reaudit_v3` | EXISTS | reports/REAUDIT_V3/v3_exec/RUN002_2frame_matrix.md:66:  `run/reaudit_v3/run002/diag/*_diagnostics.json`（5 份）+ 各 run 日志。<br>reports/REAUDIT_V3/v3_exec/RUN001_anchor_build.md:15:  - Linux 主仓：`run/reaudit_v3/run001/{A,B}`（`git archive \| tar -x`）。<br>reports/REAUDIT_V3/v3_exec/REVIEWPACK_20260828T1126Z.md:8:- **路径**：`run/reaudit_v3/AstroCS_REAUDIT_V3_REVIEWPACK_20260828T1126Z.zip`（1,810,676 字节） |
| `run/release-rescue` | EXISTS | reports/review-package-20260915/01_工作总览_V3至今.md:4:> 口径：本文件由前台自撰，只写**可核对的产物与提交**；判词以 `run/release-rescue/rqs-fix/EXECUTOR_RULINGS.md`（四十二节）与各批 `REPORT.<br>reports/review-package-20260915/01_工作总览_V3至今.md:117:- 前台裁决记录 **42 节**（`run/release-rescue/rqs-fix/EXECUTOR_RULINGS.md`）；<br>reports/review-package-20260915/05_验收与证据索引.md:9:\| `a6082b4e` / `2a82bdd2` \| V3 抢救期基线修复提交（见 `run/release-rescue/rqs-fix/`） \| — \| |
| `run/repair_round3` | EXISTS | evidence/v8_1_ci_control/tasks/V8-CI-012/TASK_RESULT.json:1104:      "method": "本机无 pytest 且无 pip/ensurepip → python3 -m pytest --collect-only 不可执行；以 <br>evidence/v8_1_ci_control/tasks/V8-CI-012/logs/repair_round3.log:26:   以 run/repair_round3/f9b_collect_probe.py stdlib 等价复刻 pytest collection<br>evidence/v8_1_ci_control/tasks/V8-CI-012/logs/repair_round3.log:79:新增（本轮）：evidence/v8_1_ci_control/tasks/V8-CI-012/logs/repair_round3.log、logs/repair_ |
| `run/resource` | EXISTS | ci/ci_repair_round.py:121:        "minimal_patch": "cli 侧把运行产物默认落 run/resource/（AGENTS.md 目录规范），或 tests/cli 显式传 --out-dir run/...；禁以 dirty_ignore 登记掩盖<br>tools/quality/compare_products.py:455:    ap.add_argument("--out", default="run/resource/compare", help="报告输出目录")<br>tools/quality/resource_monitor.py:16:  python3 tools/quality/resource_monitor.py --out run/resource/mon -- <cmd> [args...] |
| `run/review-package` | EXISTS | reports/review-package-20260915/04_设计大纲综述/METHOD.md:6:- **只写自己的输出目录**：`run/review-package/digest-outline/`（本次新建）。该路径由仓库 `.gitignore` 第 19 行 `run/*` 排除<br>reports/review-package-20260915/04_设计大纲综述/METHOD.md:37:git check-ignore -v run/review-package/digest-outline/x<br>reports/review-package-20260915/08_算法推导_drizzle.md:3:> 文件：`run/review-package/derive-drizzle/DRIZZLE_DERIVATION.md` |
| `run/rqs-fix` | EXISTS | tests/unit/mon002_gate_test.cpp:297:        const std::string dir = "run/rqs-fix/B7-memgate/tmp_mon002_gate";<br>tests/unit/mon002_gate_test.cpp:398:        const std::string dir = "run/rqs-fix/B7-memgate/tmp_mon002_gate_t4"; |
| `run/rt001` | EXISTS | reports/review-package-20260915/03_缺陷账本/DEFECT_LEDGER.json:10031:  "position": ":正式运行只有 orchestrator.exe`（207 行、UNTRACKED）；同文本第二份 `run/reaudit_v3/run0 |
| `run/scif2001` | EXISTS | reports/review-package-20260915/03_缺陷账本/DEFECT_LEDGER.json:10031:  "position": ":正式运行只有 orchestrator.exe`（207 行、UNTRACKED）；同文本第二份 `run/reaudit_v3/run0 |
| `run/std_f1_adj` | EXISTS | docs/science/ASTROMETRY.md:87:**实测证据（2026-09-12，STD-F1-ADJ；证据目录 `run/std_f1_adj/`）**<br>docs/science/ASTROMETRY.md:174:  正向达机器精度且三向负向注入全部检出（证据 `run/std_f1_adj/std_f1_bridge_cross.json`）；<br>tests/unit/p3_wcs_test.cpp:53:// STD_F1_P3_EXPORT 覆盖, 默认 run/std_f1_adj/p3_nine_grid_export.json |
| `run/temp` | EXISTS | reports/v19r3/evidence/quality/healpix_drizzle.log:2:  [WARN] 无法写 JSONL: run/temp/precise_hardening/candidate_matrix.jsonl<br>reports/v19r2/warnings.md:26:- run/temp/phase2_warn_build2.log（0 warning 构建日志）<br>reports/v19r2/warnings.md:27:- run/temp/aio_force_build.log（0 warning） |
| `run/test_cli_cwd` | EXISTS | tests/cli/cli_test_hygiene.py:21:本模块只提供**测试侧**的 cwd/env 落点：把子进程 cwd 指到 `run/test_cli_cwd/`（AGENTS.md |
| `run/v6` | EXISTS | reports/v6/owner-package/OWNER-PACK-001_REVIEW_PACKAGE.md:77:\| 1 \| BASE-OWN-001 \| 0 \| PASS \| `4b508f28` \| `run/v6/base/` \|<br>reports/v6/science-adjudication/SUMMARY.md:86:复现（单一 rc）：`python3 run/v6/adjudication/tools/run_all.py`<br>reports/v6/science-adjudication/SUMMARY.md:119:- 本任务只写 `docs/science/v6/adjudication/`、`reports/v6/science-adjudication/`、`run/v6/adjudication/`（run/* |
| `run/v81adopt004_neg` | EXISTS | evidence/v8_1_ci_control/tasks/V81-ADOPT-004/test_summary.json:12:      {"name": "placeholder_version", "file": "run/v81adopt004_neg/lock_bad_placehol<br>evidence/v8_1_ci_control/tasks/V81-ADOPT-004/test_summary.json:13:      {"name": "inconsistent_present_flag", "file": "run/v81adopt004_neg/lock_bad_in |

> 原始结果：`run/PROJECT-GOVERNANCE-01/ROOT-003/logs/refs-by-target.txt`。扫描口径：对每个 `run/<dir>` 用 `grep -rIn -F 'run/<dir>/'` 扫 `reports/ evidence/ docs/ ci/ tests/ contracts/ tools/`，**排除** `run/` 自身与 `reports/PROJECT-GOVERNANCE-01/**`（本任务新产物，首轮扫描曾被其污染，已修正）。

**关键结论**：体量最大的两个目录 `run/perf-fix`（61.76 GB / 274,457 文件）与 `run/release-rescue`（29.60 GB / 252,156 文件）**均被 tracked 报告引用**（`reports/review-package-20260915/08_算法推导_drizzle.md` 以 `run/perf-fix/P34-algo-research/logs/A2_fullframe.txt` 等为数值来源；`reports/review-package-20260915/01/05` 以 `run/release-rescue/rqs-fix/EXECUTOR_RULINGS.md` 为裁决记录事实源）→ 按 R1 **必须保留**，本次未删除。

## 3. 清运执行记录（2026-09-16）

执行器：`run/PROJECT-GOVERNANCE-01/ROOT-003/logs/cleanup-exec.sh`；日志：`run/PROJECT-GOVERNANCE-01/ROOT-003/logs/cleanup-execution.log`。

| 指标 | 清运前 | 清运后 |
|---|---|---|
| `run/` 体积（`du -sh run`） | **102G** | **97G** |
| `run/` 表观字节（`du -sb run`） | 106,692,246,060 B（99.365 GiB） | — |
| `df -h /workspace` 已用 | 287G（60%） | **281G（59%）** |
| 删除目录数 | — | **70**（run/ 内，全部 untracked，failed=0） |
| 删除字节（run/，表观） | — | **5,430,815,754 B（5.058 GiB）** |

### 3.1 逐条删除清单（路径 / 大小 / 文件数 / 最后修改 / 规则 / 无引用证据）

| # | 路径 | 字节 | GiB | 文件数 | 最后修改 | 规则 | 无引用证据 |
|---|---|---|---|---|---|---|---|
| 1 | `run/BASE-UTIL-001-attempt1` | 2,217,674,035 | 2.065 | 873 | 2026-09-12 | R5_closed_pack_absent | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 2 | `run/BASE-UTIL-001-r4` | 1,754,999,376 | 1.634 | 1430 | 2026-09-12 | R5_closed_pack_absent | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 3 | `run/BASE-UTIL-001-r3` | 753,393,402 | 0.702 | 600 | 2026-09-12 | R5_closed_pack_absent | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 4 | `run/temp_stale_20260914_054344` | 257,319,631 | 0.240 | 743 | 2026-09-14 | R5_selfdeclared_stale_temp | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 5 | `run/t_hips_publish_idem` | 25,372,399 | 0.024 | 48 | 2026-09-11 | R5_synthetic_test_fixture | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 6 | `run/t_fi_asan` | 25,372,391 | 0.024 | 46 | 2026-09-11 | R5_synthetic_test_fixture | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 7 | `run/t_ct1` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 8 | `run/t_ct2` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 9 | `run/t_ct3` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 10 | `run/t_hd4` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 11 | `run/t_hd5` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 12 | `run/t_he1` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 13 | `run/t_he2` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 14 | `run/t_he3` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 15 | `run/t_hf1` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 16 | `run/t_hf2` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 17 | `run/t_hg1` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 18 | `run/t_hh1` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 19 | `run/t_hh2` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 20 | `run/t_hr7` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 21 | `run/t_hr8` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 22 | `run/t_hr9` | 16,938,688 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 23 | `run/t_asan_ref` | 16,938,683 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 24 | `run/t_hr6` | 16,938,683 | 0.016 | 40 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 25 | `run/t_fi_p1_fsync_fail` | 16,914,930 | 0.016 | 31 | 2026-09-11 | R5_synthetic_test_fixture | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 26 | `run/t_fi_p1_promote_fail` | 16,914,930 | 0.016 | 31 | 2026-09-11 | R5_synthetic_test_fixture | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 27 | `run/t_fi_p1_stage_create_fail` | 16,914,930 | 0.016 | 31 | 2026-09-11 | R5_synthetic_test_fixture | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 28 | `run/t_hr1` | 8,506,043 | 0.008 | 32 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 29 | `run/t_hr2` | 8,506,043 | 0.008 | 32 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 30 | `run/t_hr5` | 8,506,043 | 0.008 | 32 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 31 | `run/tmp_wcs002` | 4,320,245 | 0.004 | 10 | 2026-09-10 | R5_synthetic_test_fixture | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 32 | `run/t_hr3` | 4,289,723 | 0.004 | 28 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 33 | `run/t_hr4` | 4,289,723 | 0.004 | 28 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 34 | `run/BASE-UTIL-001` | 902,554 | 0.001 | 37 | 2026-09-11 | R5_closed_pack_absent | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 35 | `run/tmp_wcs002_reval` | 444,900 | 0.000 | 15 | 2026-09-11 | R5_synthetic_test_fixture | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 36 | `run/tmp_wcs003` | 432,489 | 0.000 | 8 | 2026-09-11 | R5_synthetic_test_fixture | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 37 | `run/tmp_wcs001_reval` | 286,264 | 0.000 | 17 | 2026-09-11 | R5_synthetic_test_fixture | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 38 | `run/mon003` | 157,050 | 0.000 | 19 | 2026-09-01 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 39 | `run/v6_p003` | 132,461 | 0.000 | 1 | 2026-08-30 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 40 | `run/cli_runs` | 59,401 | 0.000 | 96 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 41 | `run/toolchain_install` | 49,772 | 0.000 | 3 | 2026-09-06 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 42 | `run/v8ci001` | 44,342 | 0.000 | 11 | 2026-09-05 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 43 | `run/commits_regen` | 42,162 | 0.000 | 2 | 2026-09-02 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 44 | `run/reaudit_v4` | 33,407 | 0.000 | 1 | 2026-08-28 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 45 | `run/tmp_wcs001` | 29,160 | 0.000 | 3 | 2026-09-10 | R5_synthetic_test_fixture | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 46 | `run/p2upmdoc` | 11,441 | 0.000 | 7 | 2026-09-08 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 47 | `run/Testing_archive` | 125 | 0.000 | 2 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 48 | `run/t_fi_discard` | 8 | 0.000 | 2 | 2026-09-11 | R5_synthetic_test_fixture | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 49 | `run/t_asan1` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 50 | `run/t_ha1` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 51 | `run/t_ha2` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 52 | `run/t_ha3` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 53 | `run/t_ha4` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 54 | `run/t_ha5` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 55 | `run/t_ha6` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 56 | `run/t_ha8` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 57 | `run/t_ha9` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 58 | `run/t_hb1` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 59 | `run/t_hb2` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 60 | `run/t_hb3` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 61 | `run/t_hb4` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 62 | `run/t_hb5` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 63 | `run/t_hb6` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 64 | `run/t_hb7` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 65 | `run/t_hb8` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 66 | `run/t_hb9` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 67 | `run/t_hc1` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 68 | `run/t_hd1` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 69 | `run/t_hd2` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |
| 70 | `run/t_hd3` | 0 | 0.000 | 0 | 2026-09-09 | R4_>7d_unreferenced | `refs-precise.txt` 判 UNREFERENCED；`refs-by-target.txt` 无该目标条目 |

合计：5,430,815,754 B = 5.058 GiB（70 个目录）。每目录文件级清单落 `reports/PROJECT-GOVERNANCE-01/root/deleted-manifest/<run_dir>.files-by-size.txt`。

### 3.2 附带删除（ROOT-001 任务卡负责人授权项）

| 路径 | 字节 | GiB | tracked 文件数 | 依据 |
|---|---|---|---|---|
| `设计大纲` | 86,346,995 | 0.080 | 344 | ROOT-001 任务卡负责人裁决段「已确认无用，删除」；指纹见 `reports/PROJECT-GOVERNANCE-01/root/deleted-manifest/设计大纲.sha256` 与 `设计大纲.tracked-files-344.txt` |

## 4. 例外登记

| 例外 | 内容 |
|---|---|
| E1 | `run/PROJECT-GOVERNANCE-01/**` 在本轮扫描中判为 UNREFERENCED，仍按 R2 保留 |
| E2 | `run/perf-fix`、`run/release-rescue` 为 >1G 目录但被 tracked 报告引用 → 按 R1 保留，**未删除**（与派发直令「只出方案」结论一致；若与放行条冲突，以 R1 硬约束为准） |
| E3 | `run/.gitkeep` 是 `run/` 下唯一 tracked 文件，清运前后 `git ls-files run` 均只输出它 |

## 5. 关联：同批次 ROOT-001 清运

ROOT-001 删除 72 个仓库根顶层条目 / 6,365,080,946 B（5.928 GiB），其中 `build/` 6,364,625,713 B（5.927 GiB）；根条目 133 → 61，叠加本节删除 `设计大纲/` 后为 **60**。详见 `工程控制/PROJECT-GOVERNANCE-01/ROOT_LEDGER.md` §8。

## 6. 未清运的未引用目录（R6，保留待观察）

| 路径 | 字节 | 文件数 | 说明 |
|---|---|---|---|
| `run/ut_backend_fix_001` | 348,159,187 | 307 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/sci_f3_001` | 279,807,557 | 3019 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/scif3001` | 265,810,342 | 3295 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/mon_fix_001` | 72,047,349 | 273 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/release-success` | 47,464,204 | 1726 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/mod001_install_load` | 23,142,671 | 45 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/mod001` | 19,587,829 | 6409 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/sci_anchor_001` | 3,555,249 | 57 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/ci_domain_merge` | 1,740,205 | 652 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/da001_neg` | 1,482,700 | 53 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/wcs003_evidence` | 1,057,497 | 9 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/ci002` | 692,346 | 432 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/PSF-001` | 653,838 | 12 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/cp_runs` | 594,363 | 9 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/ci001` | 464,150 | 22 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/p1_001_reval` | 392,776 | 9 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/审计执行层` | 384,081 | 56 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/p2003_fix` | 342,837 | 12 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/stdreg_evidence` | 318,640 | 101 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/run` | 311,616 | 1 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/interrupted_work_20260912` | 238,607 | 6 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/rqs-triage` | 219,121 | 8 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/target-v5` | 197,776 | 9 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/con_comment_001` | 196,602 | 40 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/incoming` | 126,462 | 34 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/pack_state` | 104,309 | 15 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/dirty_backup` | 95,765 | 8 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/p1001_e2e` | 91,261 | 21 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/ci_data_reg_001` | 77,483 | 58 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/p3proj_p3001` | 66,438 | 18 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/cprun` | 48,569 | 10 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/cli001_evidence` | 42,654 | 11 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/p1hips_digest_001` | 36,178 | 32 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/p1_001_wk` | 34,824 | 1 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/psf-001` | 30,568 | 18 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/gov_agents_001` | 25,596 | 9 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/cli_input_gate_001` | 17,433 | 49 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/p3002` | 10,974 | 2 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/p2002_evidence` | 8,756 | 2 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/p2002_rev70` | 8,746 | 8 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/Rmtvuwx3i0c5cb8` | 7,225 | 9 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/aio-001` | 2,266 | 1 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/front_desk_verify` | 599 | 1 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/sci_f2_001` | 232 | 1 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |
| `run/ut_cli_maint` | 0 | 0 | 未被 R1 引用，但最后修改 <7 天且无法确证为一次性临时数据 → 按 R6 保留，下一轮复评 |

## 7. 复跑方式

```bash
cd "/workspace/Astro CS Database"
bash run/PROJECT-GOVERNANCE-01/ROOT-003/logs/refs-precise.sh    # R1 引用扫描
bash run/PROJECT-GOVERNANCE-01/ROOT-003/logs/unref-analysis.sh  # 未引用清单 + 年龄/规则判定
git ls-files run; du -sh run; df -h /workspace
```

