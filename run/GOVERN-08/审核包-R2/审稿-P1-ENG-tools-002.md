# 审稿-P1-ENG-tools-002（G08-05 对抗审稿 第 1 遍）

- 片号：`ENG-tools-002`
- 层：`eng/tools`
- 基线：`/workspace/Astro CS Database` @ HEAD `850a9edefd47434b9ab71bc907c3de1e0814b323`
- 清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:1832-1884`
- 裁定口径：**一遍 = 对同一片材料的一次完整重读**。判据代码**不构成**正确性证据；本片一切结论由本人重读原文 + 独立构造反例得出，判据「绿」不作为任何结论的依据。

---

## 1. 读完了吗

| 口径 | 数值 |
|---|---|
| 成员份数（清单声明） | 45 |
| 清单声明总行数 | 9804 |
| **实测总行数（`wc -l`，逐份）** | **9804（与清单逐份吻合，MISSING=0）** |
| **本人亲自完整读完** | **26 份 / 4668 行** |
| 本人部分读（读到但未覆盖全文） | 3 份 / 870 行（`concurrency_sweep.py` 330/545、`resource_monitor.py` 400/974、`run_gc.py` 140/370） |
| **本人直接覆盖率** | **5538 / 9804 = 56.5%** |
| 子代理完整读完 | 45/45 份并集 = **9804 / 9804 = 100%（团队覆盖率）** |
| 派发子代理 | 5 个（分组 A×1、B×2、C/D/E×1；A、B 两组各被派两次，第二次为重复派发，我保留其一结果并交叉比对） |

### 1.1 本人未亲自读完的部分（如实列出，共 16 份 / 3248 行 + 3 份局部缺口 1020 行）

仅由子代理覆盖、本人**未亲读**原文者（其结论我已逐条复核并在 §7 记录采纳/否决）：

| 文件 | 行数 | 覆盖者 |
|---|---|---|
| `eng/tools/e2e/seam_footprint.py` | 1118 | 子代理 A×2 |
| `eng/tools/l4_rebuild/sysmon.py` | 490 | 子代理 B×2 |
| `eng/tools/monitoring/node_waterfall.py` | 323 | 子代理 B×2 |
| `eng/tools/quality/sparse_punch_probe.cpp` | 245 | 子代理 C/D/E |
| `eng/tools/quality/contracts/symbol_dimension_registry.json` | 207 | 子代理 C/D/E |
| `eng/tools/make_rev2_capsule.py` | 193 | 子代理 C/D/E |
| `eng/tools/make_windows_release.py` | 184 | 子代理 C/D/E |
| `eng/tools/acceptance/fixtures/tier_verdict_real_bad.json` | 146 | 子代理 C/D/E（我另行逐字段 diff） |
| `eng/tools/acceptance/fixtures/tier_verdict_real_good.json` | 144 | 子代理 C/D/E（我另行逐字段 diff） |
| `eng/tools/gen_cfitsio_list.py` | 95 | 子代理 C/D/E |
| `eng/tools/migrate_stage2_config.py` | 71 | 子代理 C/D/E |
| `eng/tools/quality/fixtures/build_coverage/complete.json` | 13 | 子代理 C/D/E |
| `eng/tools/fatduck_ps.sh` | 8 | 子代理 C/D/E |
| `eng/tools/quality/fixtures/warning_budget/clean.log` | 5 | 子代理 C/D/E |
| `eng/tools/quality/fixtures/extract_cpp_api/invalid_sig_mismatch.hpp` | 3 | 子代理 C/D/E |
| `eng/tools/quality/fixtures/extract_cpp_api/lib/test/include/invalid_sig_mismatch.hpp` | 3 | 子代理 C/D/E |

局部缺口（本人只读了部分行段）：`concurrency_sweep.py` 缺 1-39 与 470-545；`resource_monitor.py` 缺 1-574；`run_gc.py` 缺 141-370。

**诚实声明**：§4 中标注 **[亲读]** 的条目由我本人逐行读过并独立复核；标注 **[子代理]** 的条目我**未亲读原文**，其结论依赖子代理取证，我已尽可能用独立命令（grep/wc/find/ls/diff）交叉验证，但在交付件中明确区分二者。

---

## 2. 本片判定

### 判定：**阻断（BLOCKING）**

本片同时命中负责人点名的三类高价值形态，且**均为本轮新发现**：

**最重的 3 条**

1. **【自证式判据 + 自证式白名单】`concurrency_sweep.py:62,85,326,360,429`**
   `p1_final.json`（逐帧科学终判）被放进 `TELEM_EXACT`，而 `classify()` **先查白名单**、后查 `MASKED_JSON`。文件自身 `:64-66` 白纸黑字写着「含科学量 ⇒ 不能整体降级为遥测（**会静默容忍科学差异**）」——它写下了正确规则，然后对这一个文件执行了相反的规则。`cmd_compare:365` 只在 `n_must_diff` 非零时判红，而该文件的差异被 `:326` 路由进 `telem_diff`。**更严重**：`:429` 的自检 `("f/p1_final.json", "telem")` 把这个缺陷**断言为正确行为**——谁按 `:64-66` 去修，自检就转红。这正是本轮要找的「用同一个定义式既当被检量又当期望量」。
   亲读 + 两条独立子代理报告三方一致。

2. **【恒绿门 / 自证式覆盖率】`file_audit.py:69-73,140,157`**
   模块头 `:23-24` 声称分子 = 「**标准扫描器真的会读**的文件」；实现 `:140` 只做 `os.path.splitext(p)[1].lower() in SCANNED_EXT` 的**扩展名白名单匹配**，不打开任何文件、不校验任何扫描器存在。`coverage = len(scanned)/len(shipping)` 因此是「扩展名在自备名单里的比例」——**期望量由被检量自己的一张查找表定义**，是教科书式自证。该工具的存在理由（`:3-17`）恰恰是消灭「分子分母不同源」，却把分子换成了另一张自造表。另 `:64-68` `SHIPPING_EXACT` 11 项中 5 项（`ENGINEERING_SPEC.md` / `ACCEPTANCE_SPEC.md` / `CONTROL_PACK_SPEC.md` / `DEPENDENCIES.md` / `memory.md`）**在 HEAD 不存在**，分母定义本身已腐。

3. **【伪引 + 悬空权威，全片系统性】** `ENGINEERING_SPEC.md`、`ACCEPTANCE_SPEC.md`、`eng/ci/checks.json`、`docs/engineering/01_CHECKS.md`、`docs/engineering/03_GATES.md` **在 HEAD 均不存在**（仅存于 `run/` 历史工作树），却被本片 **10 处**当作现行权威逐字引用。其中两条是**伪造归属**：
   - `README.md:89-90` 引「`ENGINEERING_SPEC.md §7`「新产物落位到对应目录，不散落根目录」与 §9「仓库只保留最新生产代码、自解释文档集、合同与测试」」。被引句**逐字存在**，但在 `docs/engineering/DOCUMENT_GOVERNANCE.md:205`（实为 **§9.1**）与 `docs/engineering/EXECUTION_MODEL.md:215`（实为 **§11.7**）；且第二句被**截断**——原文续有「、证据目录，以及当前在执行的工作包」，截断后改变了含义。**文档名错、节号错、原文被削**。
   - `README.md:101` 称 `eng/packaging/verify_install_tree.py`「在役」——**该文件不存在**；`:103` 用「在 `eng/ci/checks.json` 的注册 step 面引用数为 0」作证——**该文件不存在**，这是一次对不存在文件的空验证。
   其余 8 处见 §4-BLOCKING。

---

## 3. 逐文件清单

格式：`文件 → 读了什么 → 看到什么 → 判定`。行数为我实测。

| # | 文件 | 行 | 读了什么 / 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `e2e/seam_footprint.py` | 1118 | **[子代理]** L4 接缝门：`median(seam)/bg`，门 `max\|rel_step\|<=1e-2`。核心统计量对「覆盖边长 <50% 的接缝」盲（`seam_p90` 算了但 `gate_decision` 不消费）；`:829-834` S5 是 `ndarray.copy` 恒真往返；`:5-11` 整个「依据（逐条可核）」块在 HEAD 全部不可核 | 须修 |
| 2 | `quality/resource_monitor.py` | 974 | **[亲读 575-974]** 外部 /proc 采样 + `--judge`。`:749` `checks=[]` 且全文件 `grep -c "checks.append"` = **0**（我亲验）⇒ 每份 `judge.json` 的机器可读证据数组恒空；`:770-772` capacity=2 时要求 100% 饱和，实质恒红；`:780` 判红用 `io_wait_ms`（组长口径，注释自称低估），`:684/728` 算出的 `io_wait_all_ms` 不参与判定；`:839/891` 的 `min_busy_threads` 在 `facts()` 体（629-740）出现 **0** 次（我亲验）⇒ 契约阈值死参数；`:922/969` `--judge-exit-code` 丢弃被测子进程退出码；`:750` `wall<1.0` ⇒ INSUFFICIENT ⇒ `main` 返回 0 | 阻断 |
| 3 | `canonical_product_hash.py` | 844 | **[亲读 全文]** 可复现性判据唯一实现，分层设计扎实、`self_test` 有真负例（`:718-752`）。但 `:635` 两份 `artifacts:[]` 的 manifest ⇒ `verdict:"PASS"`、exit 0（**空集判绿**）；`:516-517` `canonical_raw` 完全忽略 `subs`，`:517` 却把归一化写进 `normalizations` ⇒ **报告为一次没发生的归一化背书**；`:27` 依据指向不存在的 `run/PROJECT-GOVERNANCE-01/DET-001/自证摘要.md`；`:76` 引 `ENGINEERING_SPEC §8`；`:216,222` 引 `p3_writer/p3_verify`（全仓不存在）；`:471` 丢弃无 `=` 行，违反自述「任何未列名差异都改变哈希」 | 须修 |
| 4 | `monitoring/concurrency_sweep.py` | 545 | **[亲读 40-189, 290-469]** 见 §2-①。`:309-315` 的 fail-closed 恒真门**确已真修**（`vacuous`/`set_mismatch`），`:326` 按当前判据重算 class（不信 manifest）是好实践；但 `:62` 一条白名单把正确规则架空，`:429` 自检断言错误行为正确 | **阻断** |
| 5 | `astro_toolkit.py` | 517 | **[亲读 全文]** JSON 编排批量工具。`:1-4` `# NON_PRODUCTION_TOOL_ONLY` 压在 `:6` shebang **之前** ⇒ shebang 失效；`:4`「production pipeline uses orchestrator.exe exclusively」与 AGENTS.md §1（三命令 `build/acsd`）及本片 `run_e2e_chain.py:44 BINARY=build/acsd` 矛盾；`:192` 在**任何平台**把 `C:\msys64\mingw64\bin` 注入 PATH；`:253-263` `delete_file` 吞 `FileNotFoundError` 且**不接 `ctx["cwd"]`**；`:124` sha256 返回**大写**，与本片 `canonical_product_hash.py:247` 小写不兼容 | 须修 |
| 6 | `l4_rebuild/sysmon.py` | 490 | **[子代理]** 纯记录器，**无任何裁决**（本组最干净：无门 ⇒ 无假绿）。`:41` 硬编码 `CLK_TCK=100`、`:331,332,377` 硬编码 4 KiB 页，而同片 `resource_monitor.py:47` 用 `os.sysconf("SC_CLK_TCK")` ⇒ 同量两个来源；首样本写结构性 0；`:361,486` 目标死亡后全 0 列仍 exit 0 | 须修 |
| 7 | `monitoring/resource_probe.py` | 422 | **[亲读 全文]** 诚实探针，字段/source 成对，`:292-322` Windows 回退（`GATE-TRIAGE-01` 恒绿修复）是真的。`:237` cgroup v2 候选为 `sys_root/"cpu.max"` ⇒ 默认 `/sys/cpu.max`（真实挂载点是 `/sys/fs/cgroup/cpu.max`），且全片无人传非默认 `sys_root` ⇒ **结构不可达**；`:25` 引 `eng/ci/tests/test_resource_probe.py`（`eng/ci/` 不存在） | 须修 |
| 8 | `run_gc.py` | 370 | **[亲读 1-140]** run/ 回收器，**真 fail-closed**：`:96-113` 清单缺失/为空 ⇒ `KeepListUnavailable`（dry-run 与 apply 一律拒），`:56-61` 根哨兵，`:120-128` git-tracked 拒绝。`:132` 引 `ENGINEERING_SPEC §10`（该文件不存在）。**[子代理]** 另指出 `:306,347` 两处自称「硬护栏」实为恒真 | 须修 |
| 9 | `monitoring/node_waterfall.py` | 323 | **[子代理]** 正则经与 C++ 格式串逐条核对，无伪引（该文件清白）。`:146` `io_wait_all_ms/10.0` 把 `dt` 丢掉（`:141` 读了 `dt`，`:147-148` 用了，`:146` 没用）⇒ 误差恰为 `dt` 倍；`:168` 对齐误差上限硬编码 0.5 s 而源是 1 Hz | 须修 |
| 10 | `quality/v19r3_audit.py` | 302 | **[亲读 全文]** **已退役**（`:197-200` `main()` 打印告示 return 2，行为正确）。但 `:149` `WHITELIST.search(raw)` 对**整行**豁免，而白名单含裸词 `format`、`T\d`、`G\d`、`F0[1-9]`、`B0[1-9]`（`re.IGNORECASE`）⇒ 任何含这些子串的行**整行免检**；`:143` 只认 `//`，`CODE_EXT` 却含 `.py/.f/.f90`（其唯一注释语法是 `#`）⇒ 对三分之二注释语法恒盲；`:242-243` `review_status="V19R3-FRESH-VERIFIED"` **仅由 `os.path.exists` 推出**（存在即已验证）；`:187` 引 `ENGINEERING_SPEC §8`；`:191`「活动替代」指向 `known_failures_baseline.py` + `eng/ci/` ——**两者皆不存在** | 阻断（文档面） |
| 11 | `quality/v19r3_static.py` | 297 | **[亲读 全文]** `:229,235` 对**所有** `.cu` 直接 `cuda_exception()`，该函数**从不调用任何分析器**（`:200-208`），而 `:178-185` 真正的 CUDA 判定被 `:230` 架空 ⇒ 死代码 + 无条件豁免；`:236-239` 所有 header 记 `HEADER_VIA_TU` 空 findings，`:293` exit 0 不受影响，而 `:3` 宣称「100% shipping units」；`:110-113` 读 `reports/v19r3/.../shipping_units.csv`，其唯一生产者 `v19r3_audit` 已退役 ⇒ 未捕获 `FileNotFoundError`；`:48,79-86` 全部 include 路径硬编码 Windows | 须修 |
| 12 | `e2e/run_e2e_chain.py` | 290 | **[亲读 全文]** `:219-246` `--self-test` 用**硬编码 dict 字面量**跑 `:172-176` 的条件副本，**从不调用 `verify_chain`**（我亲验：`awk 'NR>=219&&NR<=246' … \| grep verify_chain` 无输出）⇒ 结构上不可能转红；`:199-205` E2E-E5 两个候选路径**都从 `d` 构造**（`glob(d+…)` 与 `os.path.join(d,…)`）⇒ `realpath` 比较恒真，而 `:19` docstring 承诺的「扫 output_dir **之外**」从未实现；`:213` `--verify-only` 我亲验 `grep -c verify_only` = **1**（仅 add_argument 自身）⇒ **死开关**；`:166-192` 产品判据全读 `p3_verify.json` 自报值（`:188-192` 用 FITS 自己的 NAXIS 去对同一文件自己的 `total_px`） | 阻断 |
| 13 | `quality/sparse_punch_probe.cpp` | 245 | **[子代理]** 「无字节变化」的判据唯一测量是 `sha_before == sha_after`，两个摘要**都由被测单元内的 `aio_file::sha256_hex` 产出**；探针自称独立的 `metrics_of` 调**同一个函数** ⇒ 哈希辅助函数一坏，对刚被破坏的文件发 `verified:true`。非恒真（`:583` 可达红），但**测试单元外无任何 oracle** | 阻断（自证） |
| 14 | `gen_v19_evidence.py` | 237 | **[亲读 全文]** **已退役**（`:174-177` return 2，行为正确）。`:37-51` `MODS` 13 条全是**改名前**的 `lib/astro_image_io` / `lib/phase2` / `lib/acr` 等路径，HEAD **全不存在** ⇒ `shipping_units()` 经 `:68-69 continue` 返回 `[]`；`:212` 字面量 `"result": "WARNINGS=PASS (first-party zero)"`、`:216-229` `KNOWN_P0: 0` 等**写死结论**（`:166-167` 退役告示已诚实承认）；`:164` 引 `ENGINEERING_SPEC §8` | 须修 |
| 15 | `quality/update_audit_status.py` | 217 | **[亲读 全文]** **已退役**（`:90-93` return 2，行为正确）。`:84` 退役告示称「活动替代 = `eng/tools/quality/check_module_map.py` 的口径」——**该文件不存在**（`check_*.py` 已全族清退）⇒ **退役告示指向的接班人没有接**；`:112-130` 旧实现按文件类别**直接赋** `PASS`/`VERIFIED`、P0-P3 全 0、不读任何证据；`:80` 引 `ENGINEERING_SPEC §8` | 阻断（文档面） |
| 16 | `quality/contracts/symbol_dimension_registry.json` | 207 | **[子代理]** `:201` 排除 `check_symbol_dimension_uniqueness.py`（不存在）⇒ 自我排除空转；`:12` 自陈只判「定义站点」，而该模式在 `lib/**/*.cpp` 几乎不命中 ⇒ 相对自述目的近零覆盖；21 个符号带量纲但仅 2 个进 `symbols[]` | 须修 |
| 17 | `file_audit.py` | 197 | **[亲读 全文]** 见 §2-②。另 `:151` 快照取自 `git ls-files`（索引），`:135` 复算取自 `git ls-tree <rev>`（提交树），`:58-60` 自陈二者相等**「索引干净时」**成立，而索引**从不被检查** ⇒ 复算等式不可靠 | 阻断 |
| 18 | `make_rev2_capsule.py` | 193 | **[子代理]** `:11,141` 伪引：「docs/ACSD_DESIGN.md §0（权威链: 旧世代控制包产物不构成判据）」——§0 存在（`:7`）但 `控制包|旧世代` 在 §0 内零命中；裸从句置于冒号后，正是「只扫成对引号会漏」的主项形态 | 须修 |
| 19 | `make_windows_release.py` | 184 | **[子代理·在役]** `:90` `"generated_at_utc": "2026-08-30T16:00:00Z"`、`:115` SPDX `"created"` 同值——**硬编码日期被盖进每一份出货 SBOM**；`:66` `VERSION` 缺失 ⇒ 静默发 `0.10.0-alpha.2` 且 exit 0（实际 VERSION 是 `0.1.0-alpha.1`，回退值本身也错），违反 AGENTS.md §11 | 阻断（在役工具造假） |
| 20 | `quality/v19r3_strip_comments.py` | 173 | **[亲读 全文]** **在役的整树改写器**：`:145` 遍历全部 `lib/**` 四种扩展名，`:165` 原子覆写，`:169` **无条件 return 0**。无 `--dry-run`、无「工作树干净」守卫、无构建/测试门。`:15` 指定的复扫器 `v19r3_audit.py` **已退役（恒 exit 2）** ⇒ **活着的改写器，配一个永远不动的验证器**。`:131` `rstrip()+"\n"` 静默把 CRLF 转 LF | 阻断 |
| 21 | `quality/gen_commits_csv.py` | 169 | **[亲读 全文]** 两条 docstring 规则**都未实现**：`:13`「commit-required 任务缺 commit ⇒ 失败」——`commit_required` 从未被读；`:115-116` `status != "PASS"` 直接 `continue`，**被筛掉的恰是唯一会暴露未提交工作的那批**（失败模式「筛掉真信号」）；`:124-126` 父提交不匹配时回落 `candidates[0]`，却把**声明的** `parent_commit` 写进产物，而 `:14` 声称校验脚本能抓伪造父 SHA | 须修 |
| 22 | `acceptance/fixtures/tier_verdict_real_bad.json` | 146 | **[亲读（逐字段 diff）+ 子代理]** 我亲跑 diff：与 good 仅 7 个顶层键不同，**两者算术均自洽**（good 33+1+15=49、33/34；bad 33+16+0=49、33/49）⇒ **任何算术检查都分不开二者**。`frames[]` 在**两者中都不齐构**（`frames[0]` 有 `frame_id`，`frames[1..2]` 是 `frame_ids`+`frame_count`）。`declared.planned_frame_list_sha256` 在**两者**都是占位非摘要 | 须修 |
| 23 | `acceptance/fixtures/tier_verdict_real_good.json` | 144 | **[亲读（逐字段 diff）]** `classification:"defect"` + `block_outcome:"partial"` + `aborted_at:null`，却声称块在第 1 帧中止 ⇒ **「好」样本自身内部矛盾**；检查「`block_outcome != full` ⇒ `aborted_at != null`」会**误伤 good**。我亲验：`grep -rn "tier_verdict_real" eng/ lib/` = **0 命中 ⇒ 这对夹具无任何消费者** | 须修 |
| 24 | `isa_feature_bits.py` | 142 | **[亲读 全文]** 单一口径源，fail-closed（`UnknownFeature`），组宏递归展开、续行合并（`:60`，注释自陈踩过），设计正确。`:56` 头文件缺失抛 `FileNotFoundError` 而非文档承诺的 `UnknownFeature`；我亲验 `cpu_features.h` **存在**。**清白** | 通过 |
| 25 | `quality/plane_stretch.py` | 134 | **[亲读 全文]** `:4` 引 `ACCEPTANCE_SPEC.md §5.1`（**该文件不存在**）；`:37-38` `if not (hi>lo): hi=lo+1.0` ⇒ 平坦/退化图出**全黑 PNG 且 exit 0**；`:48` MAD 为 0 时静默换口径 | 须修 |
| 26 | `README.md` | 115 | **[亲读 全文]** 见 §2-③。另 `:83` 引 `vq-commit.ps1`（**全仓不存在**）；`:94` 引 `eng/ci/root_manifest.json`（不存在）；`:87-114` 整段历史叙事（两个日期、三个任务号、两个完整 commit SHA），违反 AGENTS.md §5 | 阻断（文档面） |
| 27 | `quality/plane_chunks.py` | 114 | **[亲读 全文]** `:4` 同引不存在的 `ACCEPTANCE_SPEC.md §5.1`；`:84-85` 静默跳过空块 ⇒ `n_tiles < grid²` 仍打印 `PASS`；`:88-91` 只降不升，与「每块 tile×tile」矛盾 | 须修 |
| 28 | `gen_cfitsio_list.py` | 95 | **[子代理]** `:9` 称「CMakeLists.txt:**176** include」—— CMakeLists 共 1580 行，实际在 **202** 行；`:48` EXCL 用**子串**而非前缀匹配（今天 11 处删除恰好都对，但 `writer_a.c` 含 `iter_a` 会被误删）；`:49` 只检查 ≥1 存活 | 建议 |
| 29 | `round_start.sh` | 85 | **[亲读 全文]** **本片写得最好的文件，零发现**：`:13` `set -euo pipefail`；`:28-33` 未知选项点名退出 2，并把「`--dry-run` 被静默丢弃而 `--apply` 仍生效、证据真被删」的事故写进注释；`:51-53` 三处锚断言；`:59` vs `:68` dry-run 正确地不带 `--apply`。我亲验 `run_gc.py` **确有** `--keep-file`(`:251`) 与 `--prune-products`(`:255`) ⇒ `:59/:68` 调用合法（我一度怀疑此处断链，核实后**否决**该怀疑）。仅 `:48` 引 `ENGINEERING_SPEC §10`（不存在） | 建议 |
| 30 | `isa_sites.py` | 83 | **[亲读 全文]** 口径表读取器，`:72-73` 未登记旗标 ⇒ `UnknownSite`（真 fail-closed），`:82` 按头文件序归一与 `:56` docstring 一致。`:4` 引 `eng/tools/quality/check_isa_same_source.py`——**该文件不存在**（`check_*.py` 已全族清退）⇒ 它自称的强制对齐机制已无实现 | 须修 |
| 31 | `migration/rewrite_refs.py` | 81 | **[亲读 全文]** `:36` `re.sub(pat, "docs/ACSD_DESIGN", t2)` 与 `:45` `re.sub("docs/science/algorithms/(?!anchors)", "docs/science/algorithms/", t)` —— **两处都是用文本替换它自己**（恒等替换）；`:73-74` 处数统计**硬编码 `'docs/ACSD_DESIGN'` 这个键**，故 `--batch algorithms-science` 对它改过的每个文件都报 `+0`，而 `migration/README.md:12` 声称会打印「样例」；`:76` `open(p,"w")` 截断后写，**非原子**；`:9 SKIP_DIRS` 漏 `third_party`；`:8` REPO 由 `__file__` 三级上溯（`eng/tools/migration`→根）正确。**在役全仓改写器，未退役** | 阻断 |
| 32 | `migrate_stage2_config.py` | 71 | **[子代理]** `:26-27` 默认 `low=4.0 > high=3.0`，`:44-46` 静默写出**倒置的** `low_fraction:4.0, high_fraction:3.0`；`:42` `alpha:0.05` 硬编码且丢弃 lo/hi（违反 AGENTS.md §6）；自称「一次性」(`:2`) 却无 RETIRED 告示 | 须修 |
| 33 | `gen_visual_views.py` | 59 | **[亲读 全文]** `:57` `print("VIEWS_OK: 6 视图生成于 …")` —— 全文件 `write_pgm` 只在 `:30,37,43,49,54` 调用 **5 次**，**第 6 视图从未生成**，且无任何计数断言 ⇒ 硬编码的「6」恒绿。另 `:9-10` `OUT` 为 **CWD 相对**且 `mkdir` 在**模块导入期**执行 ⇒ 任何 import 都会在当前目录落文件 | 阻断（恒绿文案） |
| 34 | `CONTINUE.md` | 46 | **[亲读 全文]** `:6` 称「`run/` 不入库」——我亲验 `git ls-files run/` = **2**（大体成立，少数例外）；`:46` 指向 `python3 eng/ci/run_checks.py --all` —— **`eng/ci/` 整个目录不存在**，即「机器门现状」的查询入口不可执行；`:20` 依赖 `monitoring/mem_guard.py`（在 ENG-tools-003，存）；`:32` 引 `run/MEMGOV-01`（不存在）。`:23,26,38,40` 保留具体运行结果（`KILLED RSS 14.00 GiB` 等），**违反本轮「实验域运行结果归档不留」与 AGENTS.md §5** | 须修 |
| 35 | `source_scan.py` | 43 | **[亲读 全文]** `:5-6` 立论正确（「判据读被检对象而不是文档文字」）。但 `:16-17` 正则**无字符串字面量感知**：`const char* u="http://x";` 会从 `//` 起全删，含 `/*` 的字符串会吞到下一个 `*/` ⇒ 与其自述目的相悖；`:41` 用**绝对**路径分量判定 `skip_parts` ⇒ `--root` 落在任何含 `build`/`tests`/`archive` 的目录下 ⇒ **产出零文件** | 须修 |
| 36 | `quality/README.md` | 25 | **[亲读 全文]** `:12` 称「`check_*.py`（**34 个**）」—— 我亲验 `ls eng/tools/quality/check_*.py \| wc -l` = **0**（`check_module_map.py`/`check_ctest_registration.py`/`check_conclusion_truth.py`/`known_failures_baseline.py` 全已删）；`:18` 称 `contracts/` 13 个检查器 —— 实存 **1**；`:8` 指 `eng/ci/checks.json` 与 `eng/tools/doccheck/`（**均不存在**）；`:23-24` 整段按 `eng/ci/checks.json` 登记；`:25` 引 `01_CHECKS.md`（不存在）与 `03_GATES.md`（不存在）。**本目录自己的清单在描述一个已不存在的目录状态** | 阻断 |
| 37 | `e2e/README.md` | 22 | **[亲读 全文]** `:8` 指 `ACCEPTANCE_SPEC.md`（不存在）与 `eng/ci/checks.json`（不存在）；`:21` 列 4 个 CHK-ID + 5 个 step-ID **挂在不存在的注册面上**；`:22` 指 `01_CHECKS.md`（不存在）。`:17` 称 `__pycache__` 不入库 —— 我亲验 `.gitignore:83-84` 覆盖且 `git ls-files` 中 `.pyc` 计数 = **0** ⇒ 此条**成立** | 须修 |
| 38 | `arch/README.md` | 19 | **[亲读 全文]** `:12` 描述 `check_thread_budget.py`「未登记线程创建…扫描面由权威来源派生」—— **该文件不存在**（只剩 `__pycache__/check_thread_budget.cpython-313.pyc`，未入库）；`:13` `check_p3_export_stream_prod.py` 同样不存在；`:8` 指 `eng/ci/checks.json` | 须修 |
| 39 | `graph/README.md` | 18 | **[亲读 全文]** `:12` `render_run_graph.py` **存在**（我亲验）；`:8,17` 指 `eng/ci/checks.json`（不存在）；`:18` 指 `04_ARTIFACTS.md`（存在）。`:17`「未注册于」一句是对不存在注册面的陈述 | 须修 |
| 40 | `migration/README.md` | 17 | **[亲读 全文]** `:8` 指 `eng/tools/doccheck/`（不存在）与 `eng/ci/checks.json`（不存在）；`:12` 称打印「样例」—— 实际 `:73-74` 只打印文件与硬编码计数，**无样例**（我亲读 `:57-77` 确认）；`:17` 引 `01_CHECKS.md`（不存在） | 须修 |
| 41 | `quality/fixtures/build_coverage/complete.json` | 13 | **[子代理]** 我亲验：`git grep 'build_coverage\|complete.json'` 在 eng/docs/lib **无消费者** ⇒ 孤儿夹具 | 建议 |
| 42 | `fatduck_ps.sh` | 8 | **[子代理]** `set -euo pipefail` ✓；`:6` `PS_CMD="$1"` 在无参时抛裸 `unbound variable`；`:8` 硬编码 `root@100.73.70.16`、`fujia@100.104.10.71` 且 `StrictHostKeyChecking=no`；`base64 -w0`/`timeout` 为 GNU-only | 建议 |
| 43 | `quality/fixtures/warning_budget/clean.log` | 5 | **[子代理]** 名为 clean 却含 **3 条告警**（`:2` first-party `lib/algorithms/calibration/src/cal_core.cpp`）。旁证 `regressed.log`/`dirty.log` 存在 ⇒ 应为**负例**夹具，但无消费者 | 建议 |
| 44 | `quality/fixtures/extract_cpp_api/invalid_sig_mismatch.hpp` | 3 | **[子代理]** 与 #45 **逐字节相同**的一对深度路径夹具（`lib/test/include/`），意图应是验证 `test/` 路径被排除 —— 意图自洽，非偶然 | 通过 |
| 45 | `quality/fixtures/extract_cpp_api/lib/test/include/invalid_sig_mismatch.hpp` | 3 | **[子代理]** 同上；`extract_cpp_api.py` 从不引用 `fixtures/` ⇒ 夹具无消费者 | 建议 |

---

## 4. 发现清单

计数口径见 §4.4。

### 4.1 阻断（BLOCKING）— 11 条

| ID | 位置 | 问题 | 可复现命令 |
|---|---|---|---|
| **BLK-1** | `concurrency_sweep.py:62,85,326,360,429` | 逐帧科学终判 `p1_final.json` 被白名单降级为遥测，与本文件 `:64-66` 自述规则相反；`:429` 自检把该缺陷断言为正确 | `sed -n '55,93p;316,331p;422,434p' eng/tools/monitoring/concurrency_sweep.py` |
| **BLK-2** | `file_audit.py:23-24,69-73,140,157` | 「标准扫描器真的会读」被替换为自备扩展名白名单 ⇒ `coverage` 自证；`SHIPPING_EXACT` 11 项中 5 项不存在 | `sed -n '63,80p;128,142p' eng/tools/file_audit.py`；`for f in ENGINEERING_SPEC.md ACCEPTANCE_SPEC.md CONTROL_PACK_SPEC.md DEPENDENCIES.md memory.md; do [ -e "$f" ] && echo "EXISTS $f" || echo "MISSING $f"; done` |
| **BLK-3** | `run_e2e_chain.py:219-246` | `--self-test` 用硬编码 dict 跑判据条件副本，**从不调用 `verify_chain`** ⇒ 恒绿 | `awk 'NR>=219 && NR<=246' eng/tools/e2e/run_e2e_chain.py \| grep -c "verify_chain\|newest_run_manifest\|fits_structure"` → `0` |
| **BLK-4** | `run_e2e_chain.py:199-205` vs `:19` | E2E-E5 两候选路径均由 `d` 构造 ⇒ `realpath` 比较恒真；docstring 承诺的「扫 output_dir 之外」从未实现 | `sed -n '103,106p;198,207p' eng/tools/e2e/run_e2e_chain.py` |
| **BLK-5** | `README.md:89-90` | 伪引：被引句逐字存在但在 `DOCUMENT_GOVERNANCE.md:205`（§9.1）与 `EXECUTION_MODEL.md:215`（§11.7）；`ENGINEERING_SPEC.md` 不存在；且第二句**截断**掉「、证据目录，以及当前在执行的工作包」 | `grep -n "新产物落位" docs/engineering/DOCUMENT_GOVERNANCE.md`；`grep -n "仓库只保留最新生产代码" docs/engineering/EXECUTION_MODEL.md`；`find . -name ENGINEERING_SPEC.md -not -path "./run/*"` → 空 |
| **BLK-6** | `README.md:101,103` | 「在役」文件与「注册 step 面引用数为 0」两条论据**双双指向不存在的文件** | `[ -e eng/packaging/verify_install_tree.py ] || echo MISSING`；`[ -e eng/ci/checks.json ] || echo MISSING` |
| **BLK-7** | `quality/README.md:12,18,8,23-25` | 目录清单整体失真：34→**0** 个 `check_*.py`，13→**1** 个 contracts 检查器，注册面与 `doccheck/` 不存在 | `ls eng/tools/quality/check_*.py 2>/dev/null \| wc -l` → `0` |
| **BLK-8** | `rewrite_refs.py:36,45` | 两处**恒等替换**（文本替换为自身）；`:73-74` 计数键硬编码致另一批次恒报 0；`:76` 非原子写 | `sed -n '33,38p;43,46p;70,77p' eng/tools/migration/rewrite_refs.py` |
| **BLK-9** | `v19r3_strip_comments.py`（在役） vs `v19r3_audit.py:197-200`（已退役） | **活的整树改写器配一个恒 exit 2 的验证器**；改写器 `:169` 无条件 return 0，无 dry-run、无守卫 | `sed -n '140,170p' eng/tools/quality/v19r3_strip_comments.py`；`sed -n '196,201p' eng/tools/quality/v19r3_audit.py` |
| **BLK-10** | `make_windows_release.py:90,115`（在役） | 硬编码日期 `2026-08-30T16:00:00Z` 被写入每一份出货 SBOM 与 SPDX `created` | 见 §8 命令 |
| **BLK-11** | `gen_visual_views.py:57` | 宣称「6 视图」而只生成 5（`write_pgm` 仅 5 次调用），无计数断言 ⇒ 硬编码恒绿 | `grep -c "write_pgm(" eng/tools/gen_visual_views.py` → 5（不含定义） |

### 4.2 须修（NEEDS-FIX）— 26 条（摘要，逐条依据见 §3 表与 §8）

`resource_monitor.py`（`:332` 死 import `from tools.monitoring import …` ⇒ 容量口径退化为机器核，与 `eng/contracts/resource_gate_v1.json` 的 `denominator.forbid` 正面冲突；`:770-772` capacity=2 时恒红；`:780` 用自称低估的字段判红；`:749/839` `checks`/`min_busy_threads` 死代码；`:922/969` 丢弃子进程退出码；`:750` INSUFFICIENT ⇒ exit 0）｜`v19r3_audit.py:149` 整行白名单豁免 + `:143` 只认 `//` + `:242` 存在即已验证 + `:191` 接班人不存在 ｜`update_audit_status.py:84` 接班人 `check_module_map.py` 不存在 ｜`concurrency_sweep.py:429` 自检锁死缺陷 ｜`seam_footprint.py` 接缝统计对 <50% 覆盖盲 + `依据` 块不可核 [子代理] ｜`sysmon.py:41` 硬编码 `CLK_TCK`/页大小 ｜`resource_probe.py:237` cgroup v2 不可达 + `:25` 悬空 ｜`node_waterfall.py:146` 单位错误 + `:168` 误差上限硬编码 [子代理] ｜`run_gc.py:132` 悬空权威 + `:306,347` 恒真护栏 [子代理] ｜`canonical_product_hash.py:635` 空集判绿 + `:516-517` 报告为未发生的归一化背书 + `:27/:76/:216/:222` 悬空 ｜`astro_toolkit.py:1-4/:192/:253-263/:124` ｜`v19r3_static.py:229,235/:236-239/:110-113/:48,79-86` ｜`gen_commits_csv.py:13,115-116,124-126` docstring 两条规则均未实现 ｜`plane_stretch.py:4/:37-38` ｜`plane_chunks.py:4/:84-85` ｜`isa_sites.py:4` 强制对齐器不存在 ｜`gen_v19_evidence.py:37-51` MODS 全为改名前路径 ｜`migrate_stage2_config.py:26-27,44-46` 倒置分数对 ｜`source_scan.py:16-17,41` ｜`CONTINUE.md:46/:32/:23,26,38,40` ｜`e2e/README.md`、`arch/README.md`、`graph/README.md`、`migration/README.md`、`quality/README.md` 悬空上游 ｜`canonical_product_hash.py` 与本片 10 处 `ENGINEERING_SPEC`/`ACCEPTANCE_SPEC` 伪引 ｜`seam_footprint.py:8-9` 伪引（`SCI-C` 在 `docs/ACSD_DESIGN.md` 零命中；R5 行号 `:225`→实为 `:245`）[子代理]

### 4.3 建议（SUGGESTION）— 8 条

`gen_cfitsio_list.py:9` 行号错（176→202）、`:48` 子串匹配 ｜`fatduck_ps.sh:6,8` 裸 `unbound variable` + 硬编码两台内网主机 ｜`file_audit.py:151,135` 索引/提交树快照不一致且索引从不被检查 ｜`round_start.sh:48` 悬空 §10 ｜`canonical_product_hash.py:357-368` 排除键按**裸名任意深度**匹配，可静默吞掉同名科学字段 ｜孤儿夹具 3 组（`build_coverage/complete.json`、`warning_budget/clean.log`、`extract_cpp_api/` 全树）零消费者 ｜`gen_visual_views.py:9-10` CWD 相对 + 导入期建目录 ｜`CONTINUE.md` 属 harness 机器状态文档，混入产品仓

### 4.4 计数口径说明

- 本片**无注册门实例可数**——`eng/ci/checks.json` 整个目录不存在，README 里列出的 CHK-ID（`CHK-E2E-CHAIN`、`CHK-E2E-CHAIN-SELFTEST`、`CHK-L4-SEAM-FOOTPRINT`、`CHK-CONTRACT-TEST`、`CHK-UNIT`、`CHK-ORACLE`、`CHK-SYNTH-P2`、`CHK-NWORKER`、`CHK-NWORKER-TOLERANCE`、`CHK-RESOURCE` 等）**全部无注册承载**。
- 因此本片所有计数均为 **「整改分母 = 45 份成员文件」** 与 **「发现条目数」**，不使用「门实例」「去重门」口径。
- 伪引计数：同一句伪引（`ENGINEERING_SPEC §8/§10/§7`）跨文件出现属**同一缺陷模式**，我在须修项中按出现处列出，但**合并为 1 个整改项**（BLK-5/BLK-6 与须修列表末条同源）。

---

## 5. 你主动构造的反例

口径：全部为**静态重推 + /tmp 内独立重实现**，**未运行仓内任何脚本、未编译、未跑 ctest/pytest**。

| CE | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| **CE-1（本轮最强）** | 取 `concurrency_sweep.classify()` 与 `compare_manifests()` 的差异路由逻辑，在 /tmp 内独立重写；输入两份仅 `p1_final.json` 的 `var_plane_auditable: true→false`、`plane_used: "spatial"→"none"` 不同的 manifest | 「`:64-66` 声明的科学字段严格比对对 `p1_final.json` 生效」 | **推翻。** `classify("frame/M1/signal/p1_final.json")=="telem"` ⇒ 差异进 `telem_diff` ⇒ `n_must_diff=0` ⇒ `cmd_compare:365` 返回 **0（绿）**。对照同一输入走 `p1_phot.json`（不在白名单）返回红。**且 `:429` 会把该行为判为「正确」。** |
| **CE-2** | `file_audit.is_shipping()` + 扩展名匹配：构造一个 `.dat` 的 `lib/x/y.dat`（真含科学数据但无扫描器）与一个 `.py` 的空壳 | 「`coverage` 度量的是标准扫描器实际覆盖」 | **推翻（口径层面）。** 两者都不被计入 `standard_scanned`，因为 `.dat ∉ SCANNED_EXT`；把 `.dat` 改名 `.json` 即计入。⇒ 分子是扩展名表的函数，与内容、与扫描器存在性无关。 |
| **CE-3** | `canonical_product_hash.cmd_compare`：`{"artifacts": []}` × 2 | 「可复现性判据在无可复现对象时不会误报绿」 | **推翻。** `read_manifest_artifacts` 接受空列表 ⇒ `pa=pb=[]` ⇒ `_default_base([])→None` ⇒ `index()→{}` ⇒ `common=∅`、`only_a=only_b=[]`、`bad=err=0` ⇒ `verdict:"PASS"`、`artifacts_compared:0`、**exit 0**。 |
| **CE-4** | `canonical_product_hash.hash_file(path, base, subs=[("/abs/run/A","<OUT>")])`，对象为 `.txt`/`.log`（落 `canonical_raw`） | 「报告的 `normalizations` 字段如实反映施加过的归一化」 | **推翻。** `:516` `canonical_raw(path)` **不接收 `subs`**，`raw` 字节原样入哈希；`:517` 却无条件写 `res["normalizations"]=["/abs/run/A -> <OUT>"]`。⇒ **报告为一次结构上没发生的归一化背书**；`--normalize-output-dir` 下 raw 类产物必假阴性。 |
| **CE-5** | `run_e2e_chain` `--self-test` 分支：把 `:172` 的 `covered_px` 判据或 `:175` 的 FITS 存在判据整段删掉，重读 `:219-246` | 「自检在被检判据被破坏时会红」 | **推翻（结构性）。** 自检只对 `:222-223` 的**硬编码 dict** 求值；`awk 'NR>=219&&NR<=246' … \| grep -E "verify_chain\|fits_structure\|newest_run_manifest\|p3_input_manifest_hash"` → **0 命中**。被测函数一次都没被调用。 |
| **CE-6** | `run_e2e_chain` E2E-E5：`man = newest_run_manifest(d)` 恒在 `d` 内、`pth2 = os.path.join(d,"alloc_report.json")` 恒在 `d` 内 | 「E2E-E5 能发现落在 output_dir 之外的本阶段产物」 | **推翻（构造层面），部分保留。** 两候选都由 `d` 构造 ⇒ `realpath(pth).startswith(realpath(d)+sep)` 代数上恒真。**我修正了子代理的过度断言**：用「指向 `d` 外的符号链接」仍能触发红，故它不是字面恒真，而是**空转且与 docstring 承诺的检查不是同一件事**。 |
| **CE-7** | `gen_visual_views.py`：`grep -c "write_pgm("` 与 `:57` 宣称数对照 | 「『6 视图生成』有计数支撑」 | **推翻。** `write_pgm(` 实际调用 5 次（`:30,37,43,49,54`），`:57` 硬编码打印 `6`，无任何断言。 |
| **CE-8** | `rewrite_refs.rule_design_root` 第 36 行正则：`"见 docs/ACSD_DESIGN §9 的分层"` | 「该重写把裸名加上前缀」 | **推翻。** 模式 `(?<!docs/)(?<![A-Za-z_])docs/ACSD_DESIGN(?=\s*[§\s:,)（(、])(?!\.md)` 会匹配，但**替换串就是匹配串本身**（`"docs/ACSD_DESIGN"`）⇒ 文本不变、`changed=False`。`:45` 同型恒等。 |
| **CE-9** | `tier_verdict` 两夹具逐字段 diff + 算术 | 「存在某个算术量能分开 good/bad」 | **我推翻了自己的假设。** 两者算术均自洽（good `33+1+15=49`、`33/34=0.9706`；bad `33+16+0=49`、`33/49=0.6735`），**算术分不开任何东西**。真正的判别面只有 `aborted_at` 的一致性，而**good 自身不一致**（`block_outcome:"partial"` + `aborted_at:null` 却称第 1 帧中止）。另亲验两份夹具**零消费者**。 |
| **CE-10** | `round_start.sh:59,68` 调用的 `run_gc.py` 参数是否存在 | 「`--keep-file`/`--prune-products` 是断链调用」 | **未推翻 ⇒ 我否决了这条怀疑。** `run_gc.py:251` 有 `--keep-file`、`:255` 有 `--prune-products` ⇒ 调用合法。**记此一笔以免后人重犯同一误判。** |
| **CE-11** | `eng/tools/**/__pycache__` 下 73 个 `.pyc` 是否有源 | 「存在只剩 `.pyc` 的退役判据」 | **未推翻 ⇒ 我否决了这条发现。** 73 个孤儿 `.pyc` 中确含 `check_thread_budget`/`check_conclusion_truth` 等退役判据字节码，但 `git -c core.quotepath=false ls-files eng/tools \| grep -c '\.pyc$'` = **0**，且 `.gitignore:83-84` 覆盖 ⇒ **是本地未跟踪构建碎屑，不是仓内内容缺陷**。已从发现清单剔除。 |
| **CE-12** | `v19r3_strip_comments.py` + `v19r3_audit.py` 配对 | 「二者构成自愈判据（复现动作覆写被审文件）」 | **我推翻了自己的假设。** `v19r3_audit.main()` 恒 `return 2` ⇒ 复扫永不执行。真实形态**不是**自愈判据，而是**更孤立的问题**：活改写器 `:169` 无条件 return 0，配一个永远不动的验证器（已升级为 BLK-9）。 |

---

## 6. 盲复算

方法：先只读 `eng/tools/` 内文件并形成我的独立结论（§3、§4 前半、CE-1~CE-9、CE-12），**之后**才读子代理报告，逐条比对。

| 判定点 | 我的盲判 | 盲判依据 | 与既有结论比 | 结论 |
|---|---|---|---|---|
| `concurrency_sweep.py` 严重度 | **阻断** | `:62` 白名单 + `:85` 判定顺序 + `:326` 路由 + `:365` 判红条件，四点闭合；`:429` 锁死 | 子代理 B×2 一致列为各自 B1/最重项 | **判一致** |
| `run_e2e_chain.py --self-test` | **阻断（自证）** | `:219-246` 无 `verify_chain` 调用，我亲跑 awk grep 得 0 | 子代理 A×2 一致（B1/B3） | **判一致** |
| `e2e/README.md` | **须修** | 上游 3/4 不可解 | 子代理 A×2 判「阻断」 | **我偏松**，已上调理由：注册面 `eng/ci/` 缺失属本片系统性问题，但单看这一份 README 不构成独立阻断，阻断归 BLK-6/7 |
| `seam_footprint.py` 严重度 | **不单独判**（我未亲读） | — | 子代理 A：一方「须修（science 未能推翻，证伪层不能）」、另一方「阻断（median 对 <50% 覆盖盲，已构造反例）」 | **我采纳更重的一方进须修档，但在阻断清单中不单列**——因为该门按本轮裁定本就不构成正确性证据，「判据不可信」已是既定前提，其「失效」不构成新的阻断增量。**此项为本轮口径判断，非技术判断。** |
| `resource_monitor.py --judge` | **须修（非阻断）** | 容量口径退化（`:332` 死 import）确实严重，但 `--judge` 的 docstring `:744` 自陈「**按可配置阈值给出建议**」、`:876` 亦为「建议」，且 `--judge-exit-code` 默认关闭 ⇒ 默认不参与退出码 | 子代理 B×2 均判**阻断** | **我偏松。** 理由记录：默认不带 `--judge-exit-code` 时 `main:970` 返回子进程退出码，判据不劫持流水线。**但这是默认值层面的缓解，不是否认缺陷**，已把容量口径问题（契约 `denominator.forbid` 明文禁止）写进须修首条。 |
| `gen_visual_views.py` | **阻断** | 宣称 6 实做 5 且无断言 | 子代理 C/D/E 判阻断 | **判一致** |
| `make_windows_release.py` | 归为「在役工具造假」 | 硬编码日期进 SBOM | 子代理 C/D/E 判阻断 | **判一致**（我未亲读，采信并标注） |
| `v19r3_audit.py` WHITELIST 整行豁免 | **新发现（子代理未报）** | `:149` `WHITELIST.search(raw)` 作用在**整行**；白名单含裸词 `format` 与 `T\d`/`G\d`/`F0[1-9]`（`IGNORECASE`）⇒ 含这些子串的行整行免检 | 子代理 C/D/E 报的是 `:142-143` 只认 `//`（另一条） | **我新增一条判据，未否决任何一条；两条并存** |
| `eng/ci/` 全域消失 | **阻断（文档面）** | `ls eng/ci` 不存在；eng/tools 下 **12 份** README、共 **28 处** 引用它；`conclusion_snapshots.json:4` 自己承认「policy 所依赖的登记面 eng/ci/checks.json 亦已不在树内」 | 子代理 A（4 处）、C/D/E（8 处）分别独立发现 | **判一致，且我把它提升为跨文件系统性问题** |
| `run_gc.py:306,347` 恒真护栏 | 不列 | 我未读到该段 | 子代理 B×2 均报 | **我采信但降为须修**（进入 `run/` 的条目已蕴含包含性，故当前无害，属潜伏退化） |
| `canonical_product_hash` 空集判绿 / 归一化背书 | **新发现（子代理未报）** | CE-3、CE-4 | 无子代理覆盖此文件 | **纯本轮新增** |

**盲复算小结**：判一致 6 项；我偏松 2 项（已记录理由，其中 1 项上调、1 项保留）；我偏严 0 项；**本轮新增 3 条阻断级发现**（BLK-2 的自证式 coverage、CE-3 空集判绿、CE-4 归一化背书、`v19r3_audit` 整行白名单）。**无既有结论被我推翻为错误**；我推翻的是 3 条**我自己或子代理的过度假设**（CE-6 的「字面恒真」、CE-10 的「断链调用」、CE-11 的「只剩 .pyc」、CE-12 的「自愈判据」）。

---

## 7. 子代理派发记录

**派发 5 个**（要求 3-5）：

| 子代理 | 覆盖 | 声明覆盖 | 状态 |
|---|---|---|---|
| A（`cfa2b802`） | `e2e/seam_footprint.py`、`e2e/run_e2e_chain.py`、`e2e/README.md` | 1430/1430 | 已交付 |
| A'（`2690353b`，**重复派发**） | 同上 | 1430/1430 | 已交付 |
| B（`18e8370f`） | `resource_monitor.py`、`sysmon.py`、`concurrency_sweep.py`、`resource_probe.py`、`node_waterfall.py`、`run_gc.py` | 3124/3124 | 已交付 |
| B'（`7f463b88`，**重复派发**） | 同上 | 3124/3124 | 已交付 |
| C/D/E（`6754ab6b`） | 余 33 份（`canonical_product_hash.py` 起至 5 个夹具） | 5254/5254 | 已交付 |

> 诚实记录：A 与 B 各被派发两次（harness 重复了我的调用）。我未因此丢弃任一结果，而是把两组独立报告当作**交叉验证**：A×2 与 B×2 在各自组的最重项上**结论一致且行号一致**，构成互证。

### 7.1 逐条复核：采纳 / 否决 / 降级

**采纳（复核后仍成立）**

| 来源结论 | 我的复核动作 | 结果 |
|---|---|---|
| B1 `concurrency_sweep.py:62` 白名单自证 | **亲读** `:55-93, 316-331, 422-434`，确认判定顺序与 `:429` 自检 | **采纳，升为 BLK-1** |
| B2 `resource_monitor.py:332` 死 import | 亲验仓根无 `tools/` 包；亲读 `:330-338` | **采纳，进须修首条**（严重度我偏松，理由见 §6） |
| B3 `min_busy_threads` 死参数 | **亲跑** `sed -n '629,740p' … \| grep -c min_busy_threads` → **0** | **采纳** |
| B2' `checks` 恒空 | **亲跑** `grep -c "checks.append" resource_monitor.py` → **0** | **采纳** |
| A B3 `run_e2e_chain --self-test` 恒绿 | **亲跑** awk\|grep → 0 命中 | **采纳，升为 BLK-3** |
| A B2 E2E-E5 恒真 | 亲读 `:103-106, 198-207` | **部分采纳**：见下「降级」 |
| A N4 `--verify-only` 死开关 | **亲跑** `grep -c verify_only` → **1** | **采纳** |
| C/D/E B2 `rewrite_refs.py:36,45` 恒等替换 | **亲读全文**确认 | **采纳，升为 BLK-8** |
| C/D/E B2 `gen_visual_views.py` 6≠5 | **亲跑** `grep -c "write_pgm("` → 5 | **采纳，升为 BLK-11** |
| C/D/E B4 README 清单整体失真 | **亲跑** `ls eng/tools/quality/check_*.py \| wc -l` → **0** | **采纳，升为 BLK-7** |
| C/D/E B5 tier_verdict 无消费者 | **亲跑** `grep -rn "tier_verdict_real" eng/ lib/` → **0**；并亲做逐字段 diff | **采纳** |
| C/D/E B6 `make_windows_release` 硬编码日期 | 未亲读该文件 | **采纳但标注 [子代理]** |
| C/D/E 最重项 `sparse_punch_probe.cpp` 自证 | 未亲读 | **采纳为 BLK（自证类）并标注 [子代理]** |
| A B4 `seam_footprint.py:5-11` 伪引块 | 未亲读该文件 | **采纳，标注 [子代理]** |

**否决 / 降级（我复核后不成立或需改写）**

| 来源结论 | 否决理由 |
|---|---|
| A CE-2「E2E-E5 **字面**恒真」 | **降级为「空转 + 与 docstring 不符」**。我自己用「指向 `d` 外的符号链接」构造后，该检查**确实能转红**（`os.path.realpath` 会解引用）。原表述过强，已在 BLK-4 改写。 |
| A B2/C B2「`seam_footprint.py` 阻断」 | **降级为须修、不单列阻断**。理由：本轮裁定已定「判据不可信、不必核实为通过」，一个被证明失效的判据不构成新的阻断增量；其价值在于**它曾被当作证据使用过**，这一点已写入须修档。 |
| B/C/D/E「`resource_monitor.py` 阻断」 | **降级为须修**。理由：`--judge` 默认不劫持退出码（`main:970` 透传子进程码），且 docstring 自陈为「建议」。属**偏松**，已在 §6 留痕。 |
| B「`run_gc.py:306,347` 恒真护栏」 | **降级为须修/潜伏**。进入 `entries` 已蕴含包含性，当前无害；删掉上游符号链接预检才会真正失效。 |
| B N6「`run_keep.txt` 漏保 `run/ci`」 | **不采纳为主项**，降为提示。我未能独立复现其 fnmatch 推演（未读 `run_keep.txt` 全文），按「禁止编造」不予转正。 |
| C/D/E S21「`canonical_product_hash` 无消费者」 | **不采纳为阻断**。我未独立复核 `UNRESOLVED_REGISTER.md:3127` 的原文，只作提示。 |
| C/D/E B5「good 夹具的 `aborted_at` 矛盾」 | **采纳并加强**：我亲做 diff 确认 `aborted_at: null` + `block_outcome:"partial"` 同时出现。 |
| C/D/E CE-C「两夹具算术自洽，算术分不开」 | **采纳**——这是否决了子代理自己前一版假设，属自我纠正，我原样保留。 |

---

## 8. 自证段（可复跑命令）

全部为只读命令；**未编译、未跑 ctest/pytest、未运行任何被审脚本、零 git 写**。中文路径均用 `git -c core.quotepath=false`。

```bash
cd "/workspace/Astro CS Database"

# 0) 基线与清单口径
git rev-parse HEAD                       # 850a9edefd47434b9ab71bc907c3de1e0814b323
sed -n '1832,1884p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"
while IFS= read -r f; do printf "%6s  %s\n" "$(wc -l < "$f")" "$f"; done < /tmp/eng_tools_002.txt   # 合计 9804

# BLK-1 自证白名单：判定顺序 + 自检锁死
sed -n '55,93p;316,331p;360,366p;422,434p' eng/tools/monitoring/concurrency_sweep.py

# BLK-2 自证式 coverage：分子只是扩展名白名单
sed -n '23,24p;63,80p;128,142p;153,158p' eng/tools/file_audit.py

# BLK-3 自检从不调用被检函数（预期输出 0）
awk 'NR>=219 && NR<=246' eng/tools/e2e/run_e2e_chain.py \
  | grep -c "verify_chain\|fits_structure\|newest_run_manifest\|p3_input_manifest_hash"

# BLK-4 E2E-E5 候选路径均由 d 构造
sed -n '103,106p;19,19p;198,207p' eng/tools/e2e/run_e2e_chain.py

# BLK-5 伪引：被引句的真实出处与截断
grep -n "新产物落位" docs/engineering/DOCUMENT_GOVERNANCE.md          # :205（§9.1，非 §7）
grep -n "仓库只保留最新生产代码" docs/engineering/EXECUTION_MODEL.md    # :215（§11.7，非 §9；原文续「、证据目录…」被削）
find . -name ENGINEERING_SPEC.md -not -path "./run/*"                  # 空

# BLK-6 悬空「在役」与空验证
for p in eng/packaging/verify_install_tree.py eng/ci/checks.json eng/ci/root_manifest.json \
         docs/engineering/01_CHECKS.md docs/engineering/03_GATES.md ACCEPTANCE_SPEC.md; do
  [ -e "$p" ] && echo "EXISTS  $p" || echo "MISSING $p"; done
grep -rln "eng/ci/" eng/tools --include=*.md | wc -l                  # 12 份 README
grep -rn "eng/ci/" eng/tools --include=*.md | wc -l                    # 28 处

# BLK-7 目录清单失真
ls eng/tools/quality/check_*.py 2>/dev/null | wc -l                     # 0（README 称 34）
ls eng/tools/quality/contracts/*.py 2>/dev/null | wc -l                  # 1（README 称 13）

# BLK-8 恒等替换 + 硬编码计数键 + 非原子写
sed -n '33,38p;43,46p;57,78p' eng/tools/migration/rewrite_refs.py

# BLK-9 活改写器 × 死验证器
sed -n '135,170p' eng/tools/quality/v19r3_strip_comments.py
sed -n '196,201p' eng/tools/quality/v19r3_audit.py                       # main() 恒 return 2

# BLK-10 在役发布工具硬编码日期
sed -n '88,92p;112,118p;64,68p' eng/tools/make_windows_release.py
cat VERSION

# BLK-11 宣称 6 实做 5
grep -c "write_pgm(" eng/tools/gen_visual_views.py                       # 5（不含定义）
grep -n "VIEWS_OK" eng/tools/gen_visual_views.py

# 须修：resource_monitor 死代码（两条亲验计数均为 0）
grep -c "checks.append" eng/tools/quality/resource_monitor.py            # 0
sed -n '629,740p' eng/tools/quality/resource_monitor.py | grep -c min_busy_threads   # 0
grep -n "from tools.monitoring" eng/tools/quality/resource_monitor.py    # :332 仓根无 tools 包
[ -d tools ] || echo "MISSING tools/（:332 import 必失败）"

# 须修：v19r3_audit 整行白名单 + 只认 // + 存在即已验证
sed -n '79,91p;142,155p;166,178p;242,243p' eng/tools/quality/v19r3_audit.py

# 须修：canonical_product_hash 空集判绿 + 归一化背书 + 悬空依据
sed -n '581,600p;628,645p;493,500p;503,527p' eng/tools/canonical_product_hash.py
[ -e "run/PROJECT-GOVERNANCE-01/DET-001/自证摘要.md" ] || echo "MISSING :27 依据"
find . \( -name "p3_writer*" -o -name "p3_verify*" \) | wc -l           # 0（:216,:222 引用）

# 须修：夹具零消费者 + good 夹具自身矛盾
grep -rn "tier_verdict_real" eng/ lib/ --include=*.py --include=*.json | wc -l   # 0
diff <(python3 -c "import json;print(json.dumps(json.load(open('eng/tools/acceptance/fixtures/tier_verdict_real_good.json')),indent=1,sort_keys=True,ensure_ascii=False))") \
     <(python3 -c "import json;print(json.dumps(json.load(open('eng/tools/acceptance/fixtures/tier_verdict_real_bad.json')),indent=1,sort_keys=True,ensure_ascii=False))")

# 须修：CONTINUE.md 指向不存在的 eng/ci/；run/ 入库实况
grep -n "eng/ci/" eng/tools/CONTINUE.md
git -c core.quotepath=false ls-files run/ | wc -l                        # 2（:6「不入库」大体成立）

# CE-10 / CE-11 两条**已否决**怀疑的复跑（防止后人重犯）
grep -n "keep-file\|prune-products" eng/tools/run_gc.py                  # :251 / :255 —— 调用合法
git -c core.quotepath=false ls-files eng/tools | grep -c '\.pyc$'        # 0 —— .pyc 是未跟踪碎屑
grep -n '__pycache__\|\.pyc' .gitignore                                  # 83,84
```

---

## 9. 交回前台的三点建议（不自行施工）

1. **`eng/ci/` 与 `ENGINEERING_SPEC.md` / `ACCEPTANCE_SPEC.md` 的消失，需要一次全仓裁决**，而不是逐文件补洞。本片 12 份 README、28 处引用、10 处逐字伪引全部指向同一批已注销权威；`eng/tools/quality/conclusion_snapshots.json:4` 已经自己承认「policy 所依赖的登记面 eng/ci/checks.json 亦已不在树内，故该 policy 现无人执行」——**项目内部已知，但没有任何一处文档把这个已知状态写成结论**。
2. **`check_*.py` 全族（34 个）与 `eng/ci/` 同批清退后，在役改写器失去了验证器**：`v19r3_strip_comments.py`（在役、整树覆写、无条件 exit 0）↔ `v19r3_audit.py`（恒 exit 2）。这是本片唯一一条「活体破坏 + 死体验证」的组合，风险高于任何单点恒真门。
3. **本片两条自证式指标应从证据链中撤下**：`file_audit.py` 的 `coverage`（扩展名白名单自证）与 `concurrency_sweep.py` 对 `p1_final.json` 的遥测豁免（自检反锁）。按本轮裁定它们本就不构成正确性证据，但它们仍在产出机器可读的绿字段，**会被下游继续引用**。

---

*审稿人：G08-05 对抗审稿第 1 遍 · 片号 ENG-tools-002 · 基线 850a9ede · 零 git 写 · 未编译未跑测试 · 未读 `/tmp/acsd_g08/`*