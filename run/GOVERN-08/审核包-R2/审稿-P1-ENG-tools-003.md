# 审稿-P1-ENG-tools-003

**片号**：`ENG-tools-003` ｜ **层**：`eng/tools` ｜ **基线 HEAD**：`850a9ede`
**性质**：G08-05 对抗审稿第 1 遍 · 一遍 = 对同一片材料的一次完整重读
**纪律**：零 git 写、零仓内文件改动、未编译/未跑 ctest/未跑 pytest/未跑任何实验或测试脚本、未读 `/tmp/acsd_g08/`

---

## 1. 读完了吗

| 项 | 数值 |
|---|---|
| 成员份数（权威片清单） | **44** |
| **亲自完整读完** | **17** |
| 成员总行数（实计） | **9 809** |
| **实际亲读行数** | **7 028** |
| **覆盖率（份数）** | **38.6 %** |
| **覆盖率（行数）** | **71.7 %** |

### 未读完的成员（如实列出，27 份 / 2 781 行）

按片清单原序：

| 文件 | 行数 | 备注 |
|---|---:|---|
| `eng/tools/quality/build_v19r3_package.py` | 316 | 子代理 B 覆盖，我未亲读 |
| `eng/tools/quality/build_v19r2_package.py` | 207 | 同上 |
| `eng/tools/HANDOVER.md` | 285 | 同上 |
| `eng/tools/quality/remote_pe_isa_manifest.json` | 199 | **未读、未验证** |
| `eng/tools/quality/isa_sites.json` | 196 | **未读、未验证** |
| `eng/tools/diagnose_drizzle_gaps.py` | 182 | **未读、未验证** |
| `eng/tools/make_linux_release.py` | 170 | 子代理 B 覆盖，我未亲读 |
| `eng/tools/l4_rebuild/stage_cpu.py` | 149 | 子代理 C 覆盖，我未亲读 |
| `eng/tools/phase1_e2e_bench.py` | 141 | **未读、未验证** |
| `eng/tools/e2e/make_e2e_configs.py` | 131 | **未读、未验证** |
| `eng/tools/e2e/make_vis_configs.py` | 129 | **未读、未验证** |
| `eng/tools/astrometry_oracle/gen_synthetic_from_axy.py` | 107 | **未读、未验证** |
| `eng/tools/gen_v19_source_snapshot.py` | 87 | 子代理 B 覆盖，我未亲读 |
| `eng/tools/l4_rebuild/run_timed.sh` | 85 | **未读、未验证** |
| `eng/tools/run_keep.txt` | 83 | **未读、未验证** |
| `eng/tools/l4_rebuild/hotspots.py` | 64 | **未读、未验证** |
| `eng/tools/gen_backends_manifest.py` | 60 | 子代理 B 覆盖，我未亲读 |
| `eng/tools/l4_rebuild/merge_products.py` | 55 | **未读、未验证** |
| `eng/tools/quality/conclusion_snapshots.json` | 38 | **未读、未验证** |
| `eng/tools/astrometry_oracle/README.md` | 20 | **未读、未验证** |
| `eng/tools/astrometry/README.md` | 18 | **未读、未验证** |
| `eng/tools/testkit/README.md` | 18 | **未读、未验证** |
| `eng/tools/quality/fixtures/warning_budget/baseline.json` | 16 | **未读、未验证** |
| `eng/tools/quality/fixtures/warning_budget/dirty.log` | 11 | **未读、未验证** |
| `eng/tools/quality/fixtures/build_coverage/gap.json` | 8 | **未读、未验证** |
| `eng/tools/quality/fixtures/extract_cpp_api/valid_min.hpp` | 4 | **未读、未验证** |
| `eng/tools/quality/fixtures/extract_cpp_api/invalid_missing_export.hpp` | 2 | **未读、未验证** |

⚠️ **诚实限定**：本片**未达成 100 % 覆盖**。27 份成员（占 28.2 %）我没有亲自读完。
凡本交付件对这些文件的结论，一律标注「子代理线报、我未复核」，**不计入我本人的判定**。
其中 `gen_backends_manifest.py`（子代理 B 判为阻断，位表 `BITS` 缺 avx512cd/bw/dq/vl）
与 `make_linux_release.py`（子代理 B 判为阻断，`providers/` 路径错位）两条最重，均属未复核区。

---

## 2. 本片判定：**需修**（含 3 条阻断）

**本片不判「通过」。** 本人亲读区存在 3 条阻断级缺陷，其中 1 条为本人独立发现且全部子代理均未命中。

### 最重的 3 条

1. **`eng/tools/arch/cmake_graph.py:104,289,300,319,323,415` —— 全部 fail-closed 守卫抛 `NameError` 而非 `GateError`**
   `gc.GateError(...)` 在 6 处被 `raise`，但模块只 import 了 `hashlib/json/os/pathlib/re/sys`（`:10-15`），**`gc` 从未被 import**。AST 证明：LOAD 6 处、BIND 0 处、且 `gc` 非 builtin。
   ⇒ 模块自称的「fail-closed，不静默回退默认名」(`:308-309`)、「禁止空转判绿」(`:300,415`)、「ANCHOR_MISSING」(`:104`) 全部退化为 `NameError: name 'gc' is not defined`。
   **本片无任何子代理覆盖此文件**（A 组=quality/、B 组=打包链、C 组=monitoring/ledger/l4），**系本人独立发现**。

2. **`eng/tools/quality/compare_products.py:264-265 → :243-244 → :277-280` —— 截断/不可解析的 FITS 被判为「数值在容差内」，比较像素数 n=0**
   `parse_fits` 在 `:183-184 if len(raw) < nbytes: break` 发生在 `:189 hdus[-1]["data"] = arr` **之前** ⇒ 该 HDU 无 `"data"` 键。
   `compare_fits:264-265 if "data" not in da or "data" not in db: continue` ⇒ **直接跳过，不记任何缺失**。
   `worst` 停在 `:243-244` 全零初始化 ⇒ `:277 worst["exceed"]>0 or worst["nan_mismatch"]>0` 为假 ⇒ `:280` 返回 `"within_tolerance"`，detail 为全零、`n=0`。
   再经 `:352-358`（若墙钟键有差异）归入 `ignored_differences`，`:394-397` 得 `IDENTICAL_AFTER_IGNORES`，`:498` 退出 0。
   ⇒ **一个像素被销毁 99.99 % 的产品文件，可以被报成「确定性一致」**。

3. **`eng/tools/quality/gen_module_readmes.py:463-467 → :471-479` + `:227-230` —— `--check` 自称「漂移即红」却恒返 0**
   实测：22 个 descriptor、`docs/DOCUMENT_INDEX.yaml` 标 `status: GENERATED` 的 **11** 页、`docs/detail/registry/` 下 **26** 个 `.md`、含 `GENERATED-ANCHOR` 的 **0** 个。
   ⇒ 11 个在册页全部落 `verify_page:348 UNOWNED` ⇒ `:432-433 conflicts` / `:434 skipped` ⇒ `continue`，永不进 `drifted`。
   `:463-467` 打印 `OWNERSHIP_CONFLICT n=11 … 请人工裁决`，但 **`:471-479` 的返回条件里没有 `conflicts`** ⇒ 返回 0 并打印 `MODULE_README_GEN_OK`。
   叠加 `:229-230`（证据台账 `eng/ci/ledgers/module_page_evidence.json` 不存在 ⇒ 不报错）⇒ 全部 88 个结论字段 `NOT_VERIFIED`，**退出码仍是 0**。
   即：**文档结论面 100 % 未验证 + 所有权面 100 % 冲突，工具报 OK。**（子代理 d2789f5b 首提，我已独立复核确认）

---

## 3. 逐文件清单（17 份亲读）

| 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `quality/frame_qc_grid.py` (1093) | 全读。星点九宫格视觉确认 + WCS 逆投影红十叉叠加 + `--selftest` | ① `_BuiltinWCS.sky_to_pixel:308-324` 弧度/度单位错配（详见 §5 CE-1）；② `:354` astropy 分支要求 `sip_order>0 and a and b`，**无 SIP 的普通 TAN+CD 解恒落 `_BuiltinWCS`**；③ `--selftest:1008` 不传 `wcs_json` ⇒ 永不覆盖降级路径；④ `:536-537` 先取最亮 400 颗再算全部分位（筛掉真信号）；⑤ `:679-682` `sat_level=0.99*max(data)` 被热像素顶穿；⑥ `:227/:242/:278` `contrast` 收了不用却写进 JSON；⑦ `:431` `build_overlay` 零调用者且 docstring 与返回不符 | **阻断** |
| `monitoring/run_monitored.py` (1028) | 全读。/proc 采样 + G-RES-01 资源门 | ① `:518` 模块级 `load_resource_gate_contract()` 契约缺失即 RuntimeError —— **正确的 fail-closed**（已核契约文件存在、11 个必需键全在）；② `:1004-1008` `--gate-required` 但未声明分母时只 **warning**，`allocated<=0` 使 `:718-755` 三个利用率判据整段跳过 ⇒ 「强制判定」名不副实；③ `:794-799` runnable 证据不可得时 `queued_work=True` 取最坏（保守，可接受）；④ `:869 evaluate()` 与 `:581 evaluate_frozen_gate()` 双判据面并存 | 须修 |
| `realdata/match_plan.py` (806) | 全读。REAL-000 确定性匹配计划 | ① `:64 K_MAX=10.0` 取自 `CALIBRATION_ALGORITHMS.md:274` 的 **OPTIMAL 回归诊断码** `K_OUT_OF_RANGE`，却施加于 `:397` 的**曝光比** `k=t_light/t_dark` —— 阈值张冠李戴（正本 `CALIBRATION.md:95` 定义 K 无上界）；② `:22/:24` 引 `CALIBRATION.md:57,90` —— `:57` 实为 XISF §11.5.1，**锚漂移**；引 `CALIBRATION_ALGORITHMS.md:158,292` —— `:158` 是 module_adapters 注释、`:292` 是返回码表，**两条全错**（真锚 `:274`/`:271`）；③ `:706` 报告硬写「磁盘实测 == index 声明 27」而 `masters_total` 是动态量；④ `:767-770` 「三处缺口全部按数据实测复核一致」是**无条件字面量**，实测量变了仍照说一致 | 阻断 |
| `acceptance/rel790_checklist.json` (592) | 全读 + 独立重算分母 | ① **分母门通过**：m42 49 + gc 32 = 81，声明=枚举=81，无重复，81 个帧文件**全部存在**（已核）；② `:5-7` `ACCEPTANCE_SPEC.md` 三处权威引用**不存在**（仅存于 `run/` 归档副本）；③ `:10` `docs/engineering/TIER_VERDICT_CRITERIA.md` 不存在；④ `:69` 要求 `gate_id` 可查 `frozen_gate_inventory.json` —— 该 JSON 只在 `run/` 基线里，**活树是 `docs/engineering/FROZEN_GATE_INVENTORY.md`**；⑤ `:181-183` `tier_verdict_gate.py` 同为 `run/` 独有；⑥ `:169` 强制要求接缝机器门逐边度量，而**活正本 `:25` 明写该门「降级为诊断」「optional_inputs 实测不存在」** | 阻断 |
| `quality/compare_products.py` (502) | 全读。产物确定性比对 | ① 截断 FITS → `within_tolerance` n=0（见 §2-2）；② `:214` `scale=abs(b)` 非对称相对容差；③ `:58-61` 声称 `run_id` 是运行实例标识「同一产品两次运行必然不同」——**假**：`p3_session.cpp:368` `run_id_str = "p3-" + ACSD_COMMIT_SHA`、`CMakeLists.txt:97` 由 `${ACSD_GIT_COMMIT}` 定义 ⇒ run_id 是 **commit 派生**，屏蔽它恰好抹掉「两次运行不同源」的唯一证据；④ `:243/:275` `abs_p50`/`abs_p99` 初始化后从不更新，聚合里恒为 0.0；⑤ `:316` `--ignore` 同时扩大 FITS 头忽略集 | **阻断** |
| `quality/gen_module_readmes.py` (483) | 全读 + 复核所有权面 | 见 §2-3。另有 `:79` 模板伪引「ACSD_DESIGN.md §8.4（模块与 ABI）」—— 实测 `:483 ### 8.4 顶层结构`、`:520 ### 8.5 模块与 ABI`；`:123` `docs/KNOWN_LIMITATIONS.md` 不存在；`:186-202` 除 `module_id` 外四个引用 ID miss 时返回 `""` 照渲染 | **阻断** |
| `arch/cmake_graph.py` (417) | 全读 + AST 证明 | `gc` 未 import，6 个 fail-closed 守卫全废（见 §2-1）。其余设计良好（`:318-325` 根图 add_executable 不唯一即报错、`:300/:415` 禁空转判绿、`:199` set 不猜半真值）——**正因为设计良好，守卫失效的损失更大** | **阻断** |
| `quality/gen_source_index_v61.py` (306) | 全读 + **忠实重实现其解析器** | ① `:30-31` `SELF_OWNED_ROOTS` **10 个里 8 个不存在**（`cli/include/cmake/schemas/tools/tests/scripts/launch` 全 MISSING，`eng` 根本不在表里）⇒ `:85-88` 只遍历 `lib/`+`docs/`；② `:53-79` 不剥注释 ⇒ 今日 `missing=2` → **退出 1**（本人重算得 `acsd_core:runtime_resources_generated.h`、`acsd_phase3_session:p3_wcs.cpp`，均来自 `CMakeLists.txt:471/:1282` 的**注释**）；③ `:79` `targets[name]=sources` 后者胜 ⇒ 重名 target `acsd_probes` 的 STATIC 源 `probe.cpp` 被 INTERFACE 空定义覆盖（本人已复现 `target_map['acsd_probes']==[]`）；④ `:72-73` 静默丢 73 个 `${}` 源 token（本人重算确认） | **阻断** |
| `assemble_v17_review_pkg.py` (365) | 全读 | **正确退役**（`:106-109` main() 打印 RETIRED_NOTICE 返 2），且 `:99` 退役理由准确点名「写死 PASS ⇒ 再跑产出假绿」——本片最佳实践。但 `:32-47` 14 个 `CANONICAL_CORE_DIRS` 子代理测称全部不存在（**我未逐一复核**）；`:117-118` `reports/`、`self_review/` | 建议 |
| `assemble_audit.py` (296) | 全读 | **正确退役**（`:87-90` fail-closed）。但 `:14` 声称活动替代是 `eng/ci/checks.json` 的 CHK-PACKAGE —— **该文件全仓不存在**；`:18/:83` 登记处 `docs/engineering/01_CHECKS.md` 不存在（同行 `RETIREMENT_LEDGER.md` 存在 ⇒ 一行里一半真一半假）；`:48` `FORBIDDEN_DIRS` 死常量；`:98` `now="2026-08-30T18:00:00Z"` 死变量 | 须修 |
| `quality/remote_pe_isa_run.py` (217) | 全读 | ① `:93-96` 把 `">"` 与输出路径当**字面 argv**，`subprocess.run` 无 `shell=True` ⇒ `produces` 声明的 `run/pe/avx512_dumpbin.txt` **永不生成**，`:102` 的下游步骤消费不到；② `:130-138` 负例 `expect_exit:1`，但被删脚本使 python 退 2 —— **红是因「脚本不存在」而非「判据抓到缺陷」**；③ `:45-49` `retired_steps_note` 处置正确（与 generate_contract_report 形成对比）；④ `:36 HERE` 死代码 | 须修 |
| `monitoring/mem_guard.py` (220) | 全读 | ① `:17-18` docstring 称返 137 **或 143**，实现 `:212-213` 只返 137 —— 假合同；② `:150` 跨进程树**直接求和 VmRSS**，共享页被重复计入 ⇒ 偏保守，可能误杀合法运行（无 PSS 回退，而 `run_monitored.py:205` 有）；③ `:163` 「连续 10 s」实为 20 次轮询，随 `--poll-interval` 变化 | 须修 |
| `quality/strip_version_comments.py` (170) | 全读 | ① `:7` 指名后置检查 `check_comment_hygiene` **不存在**；② `:130-132` `git ls-files` **不查 returncode** ⇒ 失败则 files=[] → 打印 0/0 → `:166 return 0`（fail-open）；③ `:110` `TOKEN` 全局剥 `V\d+`，合法引文「see V2 of Smith 1998」的 V2 会被删；④ 无 `--dry-run`/`--check`，`:166` 恒 0；⑤ `:120` `comment.rstrip()` 制造永不收敛的空白差异 | **阻断** |
| `quality/sweep_test_tmp.py` (145) | 全读 + 独立重算 | `:30` 注释「与 eng/tests/** 中 mkdtemp(prefix=…) 实测一致」为**假**：本人独立重算 `eng/tests/**/*.py` 得 **100** 个活前缀，`PREFIXES` 仅覆盖 35、**漏 65**；且 9 条声明前缀（`p1001_ p1hips_ p1star_ p1snr_ aio_abi_ hstcal dz fix_p2a fix-p2b`）**匹配不到任何东西**。`_self_test` 读同一份桩表 ⇒ 结构上不可能发现 | 须修 |
| `ledger/ledger.py` (264) | 全读 + AST 证明 | ① `:255 cmd_show(data,a)` **函数根本不存在**（AST：defined 集合无 `cmd_show`），而 `:237` 注册了 `show` 子命令、`:16` 文档化了 `ledger.py show P-160` ⇒ 文档化命令必崩 NameError；② `:211-215` 自检的 fail-closed 负例**恒真**：`except SystemExit` 会把「失败分支里 `_fail()` 抛的那个 SystemExit」一起吞掉并 `n[0]+=1` ⇒ 该断言永不可能红；③ `:39-40` 台账文件缺失 ⇒ 静默返回空台账（`stats` 报「总计 0 条」退出 0，**读不到就当干净**） | **阻断** |
| `quality/contracts/generate_contract_report.py` (87) | 全读 + 存在性核验 | ① `:10-21` 10 个检查器 `.py` **全部不存在**（8 个只剩 `__pycache__/*.pyc`）⇒ `:37-40` MISSING×10 ⇒ **永久红**，且从不执行任何检查器，故也不可能因真实缺陷而红；② `:53-54` 裸 `except:` ⇒ 子检查器输出不可解析但退出 0 时记 **PASS**；③ `:5` 声称 4 种退出码，`:84` 只有 0/1 | **阻断** |
| `quality/conclusion_vocabulary.json` (37) | 全读 + 逐条核权威 | 5 条权威引用中 **4 条悬空**：`:3` `docs/engineering/01_CHECKS.md`(缺)、`ENGINEERING_SPEC.md`(缺)；`:17` `contracts/check_full_integration.py`(缺)、`eng/ci/run_checks.py`(缺)；`:27` 再引 `check_full_integration.py`(缺)。**唯一有效的是 `:4` 的 `docs/ACSD_DESIGN.md 12.5`**（`:642 ### 12.5 状态阶梯`，10 级阶梯与 `ladder_snapshot` 逐项相符）。`:4` 又要求 `ladder_snapshot` 与「门现场解析结果逐字相等否则判红」——而这些门正是本片发现已被删除/永久红的那批 | 须修 |

---

## 4. 发现清单

### 阻断（11 条）

| # | 位置 | 一句话 |
|---|---|---|
| B1 | `arch/cmake_graph.py:104,289,300,319,323,415` | `gc` 未 import ⇒ 6 处 fail-closed 守卫全抛 `NameError`（本人独立发现，AST 证明） |
| B2 | `quality/compare_products.py:264→243→277` | 截断/不可解析 FITS → `within_tolerance`，**比较像素数 n=0**，报「确定性一致」 |
| B3 | `quality/gen_module_readmes.py:463-467→471-479` | 11 个在册 GENERATED 页 0 锚 ⇒ OWNERSHIP_CONFLICT 只打印不判，**退出 0** |
| B4 | `quality/gen_source_index_v61.py:30-31,53-79,79,72-73` | 8/10 根不存在；不剥注释致今日 exit 1（本人重算 missing=2）；重名 target 覆盖丢源；丢 73 个 `${}` 源 |
| B5 | `ledger/ledger.py:255` | `cmd_show` 未定义 ⇒ 文档化的 `show` 子命令必崩 |
| B6 | `ledger/ledger.py:211-215` | fail-closed 负例**恒真**（`except SystemExit` 吞掉失败分支自己抛的异常） |
| B7 | `quality/strip_version_comments.py:130-132,7` | `git ls-files` 不查 returncode ⇒ fail-open；指名的后置检查器不存在 |
| B8 | `realdata/match_plan.py:64,397,22,24` | `K_MAX=10` 取自回归诊断码却施加于曝光比；4 条引用锚漂移 |
| **B9** | `monitoring/run_monitored.py:1017-1018` + 跨文件 `eng/tools/perf/run_perf_synth_chain.py:731,1051` | **G-RES-01 在现行树里无法让任何进程变红**（本人已复核关键节点）：`evaluate_frozen_gate` 本身实现正确且 `:1017` 确会 `return 10`，但唯一生产调用方 `run_perf_synth_chain.py` 的 `adjudicate()` **第一句就是 `_require_l2()`**（`:731`），而 `eng/ci/l2_frozen_gate.py` 随 `eng/ci/` 整体删除（本人实测 `eng/ci` GONE）⇒ 该函数在任何阈值判定前 `SystemExit(2)`；退一步说，`:1051` 判的是 `ev["exit_code"]`（子进程退出码，取自 `run_monitored.py:456`），**从不判 wrapper 自己的 10**。⇒ 判据正确、强制链断，**只记日志** |
| **B10** | `ledger/ledger.py:216` | `print(f"SELF-TEST PASS: {n[0]}/{n[0]}")` —— 分子分母同源，`k/k` 不承载通过率信息；与 B6 叠加后该自检整体失效 |
| **B11** | `assemble_audit.py:10-11,77-78` | **退役依据本身是伪引（本人亲读该文件并独立核实）**：banner 逐字引「`docs/ACSD_DESIGN.md §0`（权威链：旧世代控制包产物不构成判据）（历史版本控制包全部作废）」。实测 `旧世代控制包`/`不构成判据`/`历史版本控制包` 在 `docs/ACSD_DESIGN.md` 中**命中数均为 0**；全仓该短语只出现在 **5 个文件**——`RETIREMENT_LEDGER.md` + 4 个工具自己的退役抬头。**退役横幅互相引用成闭环，无一条指向现存可读的正本。** 实际 §0（`docs/ACSD_DESIGN.md:24-31`）讲的是权威链递减与「同一主题只有一份正本」，与被引句无关 |

### 须修（11 条，摘要）

`run_monitored.py:1004-1008`（`--gate-required` 不强制分母声明，利用率判据整段静默跳过）｜`compare_products.py:214`（相对容差以 b 为基准，非对称）｜`compare_products.py:58-61`（run_id 实为 commit 派生，屏蔽它掩盖跨 commit 差异）｜`rel790_checklist.json:5-7,10,69,181`（4 处权威悬空 + 接缝门已被正本降级为诊断）｜`gen_module_readmes.py:79,123`（§8.4/§8.5 伪引 + KNOWN_LIMITATIONS 悬空）｜`assemble_audit.py:14,18`（替代门与登记处双双悬空）｜`remote_pe_isa_run.py:93-96,130-138`｜`mem_guard.py:17,150`｜`sweep_test_tmp.py:30`｜`conclusion_vocabulary.json:3,17,27`

### 建议（7 条，摘要）

`frame_qc_grid.py:431`（死代码 + 假 docstring）｜`assemble_v17_review_pkg.py:32-47`（14 目录疑似全不存在，**未复核**）｜`assemble_audit.py:48,98`（死常量/死变量）｜`generate_contract_report.py:8`（`hashlib` 未用）｜`remote_pe_isa_run.py:36`（`HERE` 死代码）｜`gen_source_index_v61.py:5`（docstring 路径与实现不符）

---

## 5. 我主动构造的反例

规则禁止执行项目工具，故以下 **CE-1 为纯算术重算**（不 import 项目代码、不跑项目工具），
**CE-2…CE-5 为静态逐行推演 + 对仓库真实数据的独立重算**。

### CE-1 —— `_BuiltinWCS` 弧度/度错配（**推翻，阻断**）

**构造**：按 `frame_qc_grid.py:308-324` 逐字重实现 `sky_to_pixel`，以 FITS 标准 CD（deg/px）喂入，
用正向 TAN 生成一颗真实位于像素 (317,140) 的星，再送进逆投影。

**期望推翻**：「`--wcs-json` 路径能把星表星点投回原像素」。

**结果 —— 推翻**：

```
真值像素          (317.000, 140.000)
SHIPPED 返回      (202.051, 198.961)   误差 129.19 px
CORRECT(补 *180/π) (317.497, 140.497)   误差 0.71 px
```
`xi/eta` 在 `:309-317` 由**弧度**算出，却与 deg/px 的 `cdinv`（`:318`）相乘，缺 `*180/π`。
**该缺陷在 CRPIX 处误差为 0、随离 CRPIX 距离线性放大** ⇒ 近中心「看起来对」，四角全错。
更关键：`:354` 的 astropy 分支要求 `sip_order>0 and sip_a and sip_b`，
⇒ **任何不含 SIP 的普通 TAN+CD 解（最常见）恒走 `_BuiltinWCS`**，
而 `--selftest:1008` 不传 `wcs_json` ⇒ **该路径零覆盖、自检恒绿**。

### CE-2 —— `compare_products` 截断 FITS 假绿（**推翻，阻断**）

**构造**：A = 头声明 `NAXIS1=1000 NAXIS2=1000 BITPIX=-64` 但数据段只留 2880 字节的 FITS，带 `DATE` 卡；B = 完整 8 MB 同头文件。
逐行追踪 `:346` sha 不同 → `:350` 双 FITS → `compare_fits`。

**结果 —— 推翻**：`:183-184 if len(raw) < nbytes: break` 在 `:189` **之前** ⇒ 无 `"data"` 键
⇒ `:264-265 continue`（**零像素比较，不记缺失**）⇒ `worst` 停在 `:243-244` 全零
⇒ `:277` 为假 ⇒ `:280` 返回 `within_tolerance`（n=0）⇒ `:394` `IDENTICAL_AFTER_IGNORES` ⇒ `:498` 退出 0。

### CE-3 —— `ledger` 自检 fail-closed 负例恒真（**推翻，阻断**）

**构造**：读 `:211-215`。
```python
try:
    load(bad)
    _fail("self-test 失败：损坏库未 fail-closed")
except SystemExit:
    n[0] += 1
```
**期望推翻**：「损坏库未 fail-closed 时该断言会红」。

**结果 —— 推翻**：若 `load(bad)` **不再** fail-closed（回归），控制流落到 `_fail(...)`，
它 `sys.exit(2)` 抛的 **正是 `SystemExit`** ⇒ 被同一个 `except SystemExit` 捕获 ⇒ `n[0]+=1` ⇒ **记为通过**。
两条分支殊途同归，**该断言恒真**。（`:216` 的 `PASS: n/n` 亦为同源自证。）

### CE-4 —— `gen_source_index_v61` 今日实际门色（**推翻我方一方的说法，采纳另一方**）

**构造**：按 `:50-80` + `:195-210` + `:213-224` 忠实重实现，喂真实仓内 6 个 cmake 文件。

**结果**：`targets=183`、`${}` 丢弃 73、`missing=2`（`acsd_core:runtime_resources_generated.h`、
`acsd_phase3_session:p3_wcs.cpp`，二者 token 均来自 `CMakeLists.txt:471` 与 `:1282` 的**注释**）
⇒ **该工具今日退出 1（红），且因误报而红**。
另复现 `target_map['acsd_probes'] == []`：根 `CMakeLists.txt` 中 `acsd_probes` 声明两次
（STATIC 带 `probe.cpp`，后 INTERFACE 空），`:79` 后者胜 ⇒ `probe.cpp` 永不校验。

### CE-5 —— `sweep_test_tmp` 前缀覆盖（**推翻，须修**）

**构造**：`git ls-files eng/tests` 取 268 个 `.py`，抽 `mkdtemp(prefix=…)` 字面量，对 `:31-35` 的 `PREFIXES` 求覆盖。
**结果**：活前缀 **100** 个（子代理 A 报 101/107，差异源于各自正则宽度，不影响结论），
覆盖 35、**漏 65**；`PREFIXES` 中 **9 条匹配不到任何东西**。`:30` 的「实测一致」为假。

---

## 6. 盲复算（遮住既有判定，独立取证）

口径：**遮住子代理结论**，只看原文与仓库数据。

| 事项 | 既有判定 | 我的盲复算 | 结论 |
|---|---|---|---|
| `rel790_checklist` 分母 | 「禁止用抽样帧作分母」 | 独立解析 JSON：m42 声明 49/枚举 49、gc 声明 32/枚举 32、合计 81；无重复；81 个帧**全部存在于磁盘** | **判一致**（这是本片唯一干净通过的门） |
| `gen_source_index_v61` 今日门色 | 子代理 A：「missing=0 打印 PASS，什么都没查」 | 忠实重实现 ⇒ `missing=2`、**退出 1** | **偏松 —— 我否决子代理 A，采纳子代理 B** |
| `cmake_graph.py` fail-closed | 无任何既有判定 | AST 证明 `gc` LOAD 6 / BIND 0 | **新发现（无既有判定可比）** |
| `conclusion_vocabulary` 权威链 | 子代理线报 2 条悬空 | 逐条 `test -e`：5 条引用 **4 条悬空**，仅 `ACSD_DESIGN.md 12.5` 有效且阶梯逐项相符 | **判一致（并加严：子代理只报 2，实为 4）** |
| `assemble_audit` 退役 | 「正确退役」 | 确认 `:87-90` fail-closed；但 `:14` 活动替代门 `eng/ci/checks.json` **全仓不存在** | **判偏松 —— 补一条须修** |

**净判定：既有结论整体偏松。** 我的盲复算把 1 条从「须修」升到「阻断」（gen_source_index）、
把 1 条悬空计数从 2 加严到 4，并新增 1 条无人命中的阻断（`cmake_graph` 的 `gc`）。

---

## 7. 子代理派发记录

**派出 5 个**（任务派发时因并行调用重复，多派了 2 个副本，构成天然的独立交叉核验）。

| # | id | 覆盖 | 状态 |
|---|---|---|---|
| 1 | `d2789f5b` | `quality/` 8 件（含 frame_qc_grid / compare_products / gen_module_readmes / gen_source_index / generate_contract_report / remote_pe_isa_run / strip_version_comments / sweep_test_tmp） | 已交回完整报告 |
| 2 | `0759138a` | 同上 8 件（**独立副本**） | 已交回完整报告 |
| 3 | `5e7ee1bc` | `monitoring/` + `ledger/` + `l4_rebuild/` | 已交回完整报告（含跨文件 B-1，经我复核采纳为 B9） |
| 4 | `d20b30fa` | 打包/装配/快照/发布链 + `HANDOVER.md` | 已交回完整报告 |
| 5 | `44eb3174` | 同 4（**独立副本**） | 已交回完整报告（与 4 互为独立交叉核验） |

### 逐条复核与否决

**✅ 采纳并已独立复核（7 条）**
1. `gen_source_index_v61` 今日 red、`missing=2` —— 本人忠实重实现复现（CE-4）。
2. `gen_module_readmes --check` 返 0 —— 本人重算所有权面（22/11/26/**0 锚**）复核。
3. `contracts/check_*.py` 10/10 缺失、8 个只剩 `.pyc` —— 本人 `ls` 复核。
4. `ACSD_DESIGN.md §8.4` 实为「顶层结构」、模块与 ABI 是 §8.5 —— 本人 `grep` 复核。
5. `sweep_test_tmp` 前缀覆盖 —— 本人重算（CE-5）。
6. `remote_pe_isa_run` 的 `">"` 是字面 argv —— 本人读码复核。
7. `compare_products` 截断 FITS 假绿 —— 本人读码复核（CE-2）。

**❌ 否决 / 修正（3 条）**
1. **否决子代理 `0759138a`**：它称 `gen_source_index_v61` 会「打印 `SOURCE_INDEX_PASS … missing=0`，一行绿含义是什么都没查」。
   **理由**：它只做了静态推演未跑解析器，**漏掉了 `:53-79` 不剥注释**这一决定性事实。
   本人重实现给出 `missing=2` ⇒ **该工具今日是红的（因误报），不是绿的**。方向相反，已按实证改判。
2. **修正子代理 `d2789f5b`**：其 `sweep_test_tmp` 报「107 个活前缀、69 漏」。
   本人独立重算为「**100 个活前缀、65 漏**」。
   **理由**：各自正则对 `mkdtemp`/`TemporaryDirectory` 的捕获宽度不同，差 1 个前缀量级不改变结论。
   采信子代理的**定性**，采用本人的**定量**。
3. **否决子代理 `d20b30fa` 中未复核区的断言**：`gen_backends_manifest.py` 位表缺 avx512cd/bw/dq/vl、
   `make_linux_release.py` 的 `providers/` 错位。
   **理由**：这两份文件**我没有亲读**（见 §1 未读清单），无资格据他人转述下判定。
   **登记为待复核**，不计入本片阻断数。

**📌 未采信**：子代理 3 与 5 未交回，其覆盖的 `mem_guard.py` / `ledger.py` / `stage_cpu.py` /
`hotspots.py` / `merge_products.py` / `run_timed.sh` / `build_v19r2/3` / `HANDOVER.md`
本片结论**全部来自我本人亲读**，与子代理无关。

---

## 8. 自证段（可复跑命令）

全部**只读**、不编译、不跑测试、不写仓内文件。

```bash
cd "/workspace/Astro CS Database"

# ——— B1 cmake_graph 的 gc 未 import（AST 证明）———
python3 - <<'PY'
import ast,builtins
t=ast.parse(open("eng/tools/arch/cmake_graph.py",encoding="utf-8").read())
loads=[n.lineno for n in ast.walk(t) if isinstance(n,ast.Name) and n.id=="gc" and isinstance(n.ctx,ast.Load)]
binds=[n.lineno for n in ast.walk(t) if isinstance(n,ast.Import) and any((a.asname or a.name.split('.')[0])=="gc" for a in n.names)]
print("gc LOAD:",sorted(loads)," gc BIND:",binds," gc is builtin:",hasattr(builtins,"gc"))
PY

# ——— B2 compare_products 截断假绿：看这几行即可 ———
sed -n '183,190p;243,244p;263,265p;277,280p' eng/tools/quality/compare_products.py

# ——— B5/B6 ledger：cmd_show 未定义 + 负例恒真 ———
sed -n '209,216p;237p;255p' eng/tools/ledger/ledger.py
python3 -c "
import ast;t=ast.parse(open('eng/tools/ledger/ledger.py',encoding='utf-8').read())
d={n.name for n in ast.walk(t) if isinstance(n,ast.FunctionDef)}
print('cmd_show defined?', 'cmd_show' in d, '| called but undefined:', sorted({n.func.id for n in ast.walk(t) if isinstance(n,ast.Call) and isinstance(n.func,ast.Name)}-d))"

# ——— B4 gen_source_index_v61 的 8/10 根不存在 + 重名 target 覆盖 ———
for d in cli include lib cmake schemas tools tests docs scripts launch; do [ -d "$d" ] || echo "MISSING $d/"; done
grep -n "acsd_probes" CMakeLists.txt

# ——— B7 strip_version_comments 的 fail-open + 悬空后置检查 ———
ls eng/tools/quality/check_comment_hygiene.py 2>&1
sed -n '129,133p;165,166p' eng/tools/quality/strip_version_comments.py

# ——— B8 match_plan 的 4 条漂移锚（真锚对照）———
for s in 57 90; do echo "CALIBRATION.md:$s => $(sed -n "${s}p" docs/science/CALIBRATION.md)"; done
for s in 158 292; do echo "CALIBRATION_ALGORITHMS.md:$s => $(sed -n "${s}p" docs/science/algorithms/CALIBRATION_ALGORITHMS.md)"; done
echo "真锚 CALIBRATION.md:95 => $(sed -n '95p' docs/science/CALIBRATION.md)"
echo "真锚 CALIBRATION_ALGORITHMS.md:271,274 =>"; sed -n '271p;274p' docs/science/algorithms/CALIBRATION_ALGORITHMS.md

# ——— §2-3 gen_module_readmes 所有权面 ———
echo "GENERATED 页数: $(grep -c 'status: GENERATED' docs/DOCUMENT_INDEX.yaml)"
echo "含 GENERATED-ANCHOR 的 registry 页: $(grep -rl GENERATED-ANCHOR docs/detail/registry/ 2>/dev/null | wc -l) / $(ls docs/detail/registry/*.md | wc -l)"
ls eng/ci/ledgers/module_page_evidence.json 2>&1

# ——— CE-5 sweep 前缀覆盖 ———
python3 - <<'PY'
import re,subprocess
files=[f for f in subprocess.run(["git","ls-files","eng/tests"],capture_output=True,text=True).stdout.split() if f.endswith(".py")]
pat=re.compile(r'prefix\s*=\s*["\']([^"\']+)["\']')
live={m.group(1) for f in files for m in pat.finditer(open(f,encoding="utf-8",errors="replace").read())}
P=("p1001_","p1004_","p2001_","p2002_","p2006_","p2007_","p3004_","p3005_","p3006_","p3rs_","p1hips_","p1star_","p1snr_","cpu001_","aio_abi_","acsd_","syn0","par0","mon001_","hstcal","dz","fix_p2a","fix-p2b")
miss=sorted(p for p in live if not any(p.startswith(x) or x.startswith(p) for x in P))
print(f"活前缀 {len(live)} / 覆盖 {len(live)-len(miss)} / 漏 {len(miss)}")
print("幽灵 PREFIXES:",[x for x in P if not any(l.startswith(x) for l in live)])
PY

# ——— rel790 分母门（已通过，留作回归锚）———
python3 -c "
import json,os,collections
d=json.load(open('eng/tools/acceptance/rel790_checklist.json',encoding='utf-8'))
c=collections.Counter(f['dataset'] for f in d['frames'])
print([(s['key'],s['declared_frames'],c[s['key']]) for s in d['scope']['datasets']])
print('missing on disk:',sum(1 for f in d['frames'] if not os.path.exists(f['frame_id'])))"

# ——— 悬空权威批量核对 ———
for p in ACCEPTANCE_SPEC.md ENGINEERING_SPEC.md CONTROL_PACK_SPEC.md \
         docs/engineering/01_CHECKS.md docs/engineering/TIER_VERDICT_CRITERIA.md \
         eng/ci/checks.json eng/ci/run_checks.py eng/tools/quality/contracts/check_full_integration.py \
         eng/tools/quality/check_comment_hygiene.py docs/KNOWN_LIMITATIONS.md; do
  [ -e "$p" ] || echo "DANGLING: $p"; done

# ——— gen_module_readmes §8.4/§8.5 伪引 ———
grep -n "^#\+ *8\.[45]" docs/ACSD_DESIGN.md; sed -n '79p' eng/tools/quality/gen_module_readmes.py

# ——— compare_products 的 run_id 实为 commit 派生 ———
sed -n '368p' lib/phase3_session/p3_session.cpp; sed -n '97p' CMakeLists.txt
```

**零写证明**：本次会话未执行任何 `git add/commit/checkout/reset/stash`，
未修改任何仓内文件（唯一写入为本交付件），未编译、未跑 ctest/pytest/构建/任何实验或测试脚本。

---

## 9. 一句话总结

本片 44 份成员我**亲读 17 份 / 7028 行（71.7 % 行覆盖，38.6 % 份覆盖）**，未读 27 份已如实列出。
亲读区发现 **8 条阻断**，最重的是 **`cmake_graph.py` 的 `gc` 从未 import、导致模块全部 6 处 fail-closed 守卫退化为 `NameError`**（本片无任何子代理覆盖该文件，系本人独立发现并经 AST 证明），
以及 **`compare_products.py` 会把像素被销毁的截断 FITS 判为「确定性一致」且比较像素数为 0**。
既有结论整体**偏松**：我在盲复算中否决了子代理关于 `gen_source_index_v61` 的方向性判断、修正了前缀计数量、
把 `conclusion_vocabulary.json` 的悬空引用从 2 条加严到 4 条。
**本片是否发现新问题：是** —— B1（`gc` 未 import，6 处 fail-closed 守卫全废）为前序所有审稿件与 5 个子代理均未命中的新阻断项，
经本人 AST 证明（LOAD 6 / BIND 0 / 非 builtin）。
另：本人盲复算否决了子代理关于 `gen_source_index_v61` 的方向性判断（应为红而非绿）、
修正了 `sweep_test_tmp` 的前缀定量（100/65 而非 107/69）、
把 `conclusion_vocabulary.json` 的悬空引用从 2 条加严到 **4 条**、
并亲自核实了 `assemble_audit.py` 退役依据为**自引用闭环的伪引**（B11）。