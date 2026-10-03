# 审稿-P1 · ENG-tests-006（G08-05 对抗审稿 第 1 遍）

- 片号：`ENG-tests-006` ｜ 层：`eng/tests` ｜ 仓库 HEAD：`850a9edefd47434b9ab71bc907c3de1e0814b323`
- 清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:1381-1435`
- 纪律：零 git 写、零编译、零 ctest/pytest/构建/实验脚本、零仓内文件改动；未读 `/tmp/acsd_g08/`
- 口径：判据代码**不构成**正确性证据。本片全部结论由**亲自读原文 + 纸面构造反例**得出；凡"当前是否绿"一律为推断，未实测。

---

## 1. 读完了吗

| 项 | 数值 |
|---|---|
| 成员份数（权威清单） | 47 |
| 成员总行数（权威清单） | 10423（`wc -l` 实测同值，47/47 存在，MISSING=0） |
| **本人完整读完** | **31 份 / 8512 行** |
| **覆盖率（份数）** | **31/47 = 66.0%** |
| **覆盖率（行数）** | **8512/10423 = 81.7%** |

### 1.1 未由本人读完的 16 份（如实列出，均由子代理 D/E 覆盖并经我抽验）

| 文件 | 行数 | 覆盖者 |
|---|---|---|
| `eng/tests/backend/test_cpuprov_isa_variants.py` | 199 | 子代理 D |
| `eng/tests/cpu/avx2/provider_avx2_handshake_test.c` | 222 | 子代理 D |
| `eng/tests/unit/p1wcs/p1wcs_clip_consistency_test.cpp` | 168 | 子代理 E |
| `eng/tests/system/runtime/runtime_resource_record_test.cpp` | 151 | 子代理 E |
| `eng/tests/unit/ipv_platform_binding_test.cpp` | 140 | 子代理 E |
| `eng/tests/api/test_upm_recovery_oracle.py` | 137 | 子代理 E |
| `eng/tests/cli/test_resource_gate.py` | 130 | 子代理 E |
| `eng/tests/backend/test_manifest_isa_declaration.py` | 116 | 子代理 D |
| `eng/tests/api/test_upm_parallel.py` | 114 | 子代理 E |
| `eng/tests/cli/test_p1003_drizzle_path.py` | 105 | 子代理 E |
| `eng/tests/cpu/baseline/provider_capability_gate_test.c` | 97 | 子代理 D |
| `eng/tests/validation/release02/q2_snr_smoothness/realdata/stageH_log.py` | 92 | 子代理 E |
| `eng/tests/validation/release02/q3_additive_truth/src/step4_allpairs.py` | 66 | 子代理 E |
| `eng/tests/validation/release02/c_delta_composition/verify_corr.py` | 52 | 子代理 E |
| `eng/tests/validation/release02/c_delta_composition/corrected_pairs.py` | 46 | 子代理 E |
| `eng/tests/integration/p2_integrate/CMakeLists.txt` | 76 | 子代理 B |
| **合计** | **1911** | |

> 该 16 份的结论在本片中一律标注 **[代读]**，不与本人亲读结论同级。

---

## 2. 本片判定：**需修**

理由：存在 **11 条阻断**（本人亲读 4 条 / 子代理提出 7 条），全部集中在"判据失效、空转或索引失实"，**未发现实现层科学正确性被本片判据掩盖的确证**；同时本片 47 份中 44 份**从无机器执行记录**（见 §6.4），判据链整体不可信度高于问题密度。

**计数口径声明**：本片三条计数互不混用 —— **门实例**（`CHECK`/`EXPECT`/`check` 调用点，含循环展开）、**去重门**（按判据对象而非调用点去重）、**整改分母**（需修改的独立判据数）。下文 §4 各条的严重级按**整改分母**计，不按门实例计；本片**未**统计去重门总数（需逐门建表，超出本轮一遍的读完全文预算，如实登记为未完成项）。

### 最重的 3 条

1. **【阻断】`eng/tests/arch/test_backend_arch.py:6` 整门失效，且 `:22` 的短路正在掩盖一条当前真红的检查。**
   `DOC` 指向 `docs/architecture/CPU_BACKEND_ARCH.md`；`docs/architecture/` **不存在**（真档已迁 `docs/engineering/CPU_BACKEND_ARCH.md`）⇒ `setUpClass:11` 必 `FileNotFoundError`，5 条用例全 ERROR。
   即使修正路径：`:22` 断言的 `"禁止任意"` 在真档中 **0 次**（`禁止` 0 次、`任意` 0 次）——**它本来是红的**。
   而 `:22` 写成 `self.assertIn("LD_LIBRARY_PATH", self.s) and self.assertIn("禁止任意", self.s)`：`assertIn` 成功返回 `None`（falsy），`and` **短路 ⇒ 第二个断言永不求值**。
   ⇒ **死断言正好盖住一条真红**，这正是"恒绿门把真实缺陷藏在绿灯里"的教科书形态。`:18` 同一写法，`"异常"` 子句同样死亡。

2. **【阻断】`eng/tests/unit/p1_hips/publish_atomic_test.c:646-668` 自愈用例空转。**
   `:651` 用 `"%.*s/%s%s"` 手工重建 staging 路径 ⇒ 得 `<parent>/<base>.hips_staging.tmp`，**少了前导 `.`**；
   而生产 `lib/algorithms/drizzle/hips/src/aio_publish.cpp:99` 与**本文件自己的** `stage_exists():138` 都是 `"%s/.%s%s"` ⇒ `<parent>/.<base>.hips_staging.tmp`。两条路径不等。
   ⇒ `:653` `touch_file` 落进不存在目录，`touch_file():100` 的 `if(!f) return;` 静默吞掉，**垃圾文件从未被写入**；
   ⇒ `:668 EXPECT(!path_exists(out+"/stale.bin"))` 恒真，`:666` 注释宣称的"残留被自愈删除"**从未被检验**。该用例退化成 `run_atomic_success` 的副本。

3. **【阻断】`publish_atomic_test.c:566-589` `fork()` 失败即 `kill(-1, SIGKILL)` 广播杀进程组。**
   `:567 EXPECT(pid >= 0)` 只累加 `g_fail` **不中断**，`:589` 照常 `kill(pid, SIGKILL)`。`pid == -1` 时 POSIX 语义是向调用者可信号的所有进程发 SIGKILL ⇒ 会杀掉 ctest、构建 agent、同进程组 worktree。
   恰恰在本用例 malloc 4MiB×2 再 fork 的资源压力下最易触发。**无 `if (pid < 0) return;` 守卫。**

---

## 3. 逐文件清单（读了什么 → 看到什么 → 判定）

| # | 文件（行） | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---|---|---|---|---|
| 1 | `unit/cosmetic_integration_test.cpp` (1086) | 全 | SHA-256 oracle 带 `"abc"` FIPS 锚 `:231-240,:745`（**好**）；三方一致 D 组 `:542-674`；plan 独立算术 `work_units==FW*FH` `:829`（**有鉴别力**）；C9 比输入侧 `spikes[]` `:955`（**有鉴别力**）；C12 `:965`（**有鉴别力**，我亲自构造反例未能推翻） | 须修 |
| 2 | `unit/gaia_integration_test.c` (844) | 全 | `:38 #include "gaia_client.c"` —— 测试直接**编译进被测实现**；`:611` 与 DLL `module_entry.c:532` 调**同一函数** `gaia_client_collect_plan_stats` ⇒ `:686-691` D3/D4/D5 是往返自证；`:692 pio==pw` 两字段生产同源输出 ⇒ 结构对称恒绿；`:620-623` T1 **无已知向量锚**，只把 DLL 哈希两遍互比（自洽≠正确） | 须修 |
| 3 | `unit/p1_hips/publish_atomic_test.c` (705) | 全 | 见 §2 第 2/3 条；另 `:336-337` OR 恒真；`:145-156` `target_has_partial` 只列 6 个名字 | **阻断** |
| 4 | `integration/p3_export/p3_export_e2e_test.cpp` (584) | 全 | `quad():186-196` + `solve_dense():199-224` 是**真外部参照**（我复算 Q/W/GLS 通过）；`:376-384 no_tmp_residue` 抓错残留名 + 三重 fail-open | 须修 |
| 5 | `integration/p2_integrate/p2_integrate_test.cpp` (530) | 全 | negative 组 `:392-469` 是本片最强部分（12 条变异）；`:209-244` singlepath 组恒绿（见 §5 反例 X3）；`:336` 与 `:338` **完全重复**同一断言 | 须修 |
| 6 | `unit/p2_identifiability/p2_identifiability_test.cpp` (445) | 全 | `oracle_eig():46-78` Jacobi 独立实现（**真外部参照**）；B13 `:154` 断言与数学真值相反（见 §4-B13）；D 组 τ 往返自证 `:261` | 须修 |
| 7 | `integration/p2_integrate/oracle/p2_oracle.py` (399) | 全 | `unit_exponents():43-70` 自带文法，与生产 `quadratic_law_holds` 不同源（**好**）；`fwhm():108-141` 独立实现；`recompute_point_independent():240-253` **只验合并律**，"从第一性原理"docstring 夸大；`:270-274` `alpha` 算了但从不参与任何 check | 须修 |
| 8 | `cli/test_phase1_inprocess.py` (428) | 全 | `:168-175` `/proc` 不存在 ⇒ `children` 保持 `[]` ⇒ `assertEqual([],[])` **绿**（fail-open，且自称"验收核心"）；`:312-313` 只比 tile **计数**却名为 fp32/fp64 equivalence | 须修 |
| 9 | `cpu/avx512/run_provider_avx512_checks.py` (375) | 全 | `:152-162` 无 AVX-512F ⇒ `return 77` 整组 SKIP；`:260-266` 解析三路 hex 但 `:294` 只判 `is None`、从不比对；`:280-289` NONHOT 只判"出现即如何"不判"必须出现" | 须修 |
| 10 | `unit/cpu008_worker_advisor_test.cpp` (352) | 全 | `:154 CHECK(a.workers != 2 \|\| avail == 2)` 在 `:153` 已断言 `a.workers == avail` 后**恒真**；`:149` 循环把门实例数放大 96 | 建议 |
| 11 | `artifact/test_phase_product_exchange.py` (306) | 全 | 真实消费生产 `PhaseProductExchangeValidator`（**好**）；`:104-109 test_phase3_input_accepts_phase1_run_context` 名为 phase3 实则构造并校验 **phase1** doc ⇒ **名实不符**；`:4` 引 `tasks/03_RUNTIME_DATA_IO_TASKS.md` | 须修 |
| 12 | `backend/test_p2003_seam_oracle.py` (300) | 全 | `:288 STAR_NONFIT_FRAC*500.0 = 50` vs fixture 实际源幅度 2.0/0.5 ⇒ 松 250×/1000×，结构上不可能红；`:35-37` 引 `run/LINUXMAIN-RESID-01/...` **不存在且被 gitignore**；`:267-275` 判决位全来自被测链自编工具 | **阻断** |
| 13 | `unit/mosaic_window_test.cpp` (264) | 全 | `:192` `window_peak_residency_ok` 是**真判据**且 `:205-218` 有两条负例注入（常量 32B / 零驻留）⇒ 恒绿问题已在本文件被修掉（**正面**） | 通过 |
| 14 | `validation/.../synth_q2_v3.py` (259) | 全 | 纯正向代码+报告，**无判据**；`:258` 落 `synth_results_v3.json` 归档，与"实验域不留守旧"口径相关 | 建议 |
| 15 | `unit/aio_abi_selfcheck.cpp` (233) | 全 | `:93-143 check_guard_path` 三态**真可触发**（正面修掉 TAUT-NULL-GUARD 恒真门）；`:167/:180/:193` 三条注入必败 | 通过 |
| 16 | `realdata/test_index_v12.py` (201) | 全 | `:114/:128/:134` 硬编码 `196`/`53`/`94` 逐项对账磁盘（**有鉴别力**）；**该目录在 `test_index.csv` 中 0 次登记** ⇒ 漏登记 | 建议 |
| 17 | `backend/fixture_common.py` (185) | 全 | `:124` 缓存早退**无源失效键** ⇒ 生成器改了 fixture 不重建（陈旧缓存）；`:97-98` 静默 continue；`:75/132/160/168` 裸 assert | 建议 |
| 18 | `common/jsonschema_min.py` (173) | 全 | `:108-110 required`、`:70-71 $ref` 兄弟、`:99-104 if/then/else`、`:113-122 propertyNames` **均真实现，非简化桩**（正面，推翻我"校验器可能是桩"的初判） | 通过 |
| 19 | `conformance/noop/src/noop_module.c` (163) | 全 | `:154-162` 唯一导出 + 三重 fail-closed（ABI 失配/allocator 空/out 空）；`:86-126` 明确 `ACS_ERR_UNSUPPORTED` 不伪装完成 | 通过 |
| 20 | `system/runtime/README.md` (34) | 全 | `:24-34` 列 9 个 CTest 目标；`CMakeLists.txt` **确实存在**（我已核实，非悬空） | 通过 |
| 21 | `unit/fixtures/core_pipeline/README.md` (24) | 全 | `:3` 声称夹具与原包**逐字节一致**；`:23` 溯源引 `run/PROJECT-GOVERNANCE-01/QA-001/logs/03-fixture-identity.txt` —— **不存在且 `run/*` 被 gitignore** ⇒ 不可核验；两个 commit `a861d8f6`/`b7b2dea7` **均真实存在**（我已核实） | 须修 |
| 22 | `config/fixtures/negative/missing_output_dir.phase_config.json` (15) | 全 | 确实只缺块级 `output_dir`，对比正例 `export_flat` 有该键 ⇒ **负例成立，非恒绿** | 通过 |
| 23 | `config/fixtures/positive/export_flat.phase_config.json` (15) | 全 | 结构自洽 | 通过 |
| 24 | `config/fixtures/positive/cpu_profile_v2.json` (48) | 全 | 全部 sha256 字段为**占位符**（`dddd…`/`aaaa…`/`bbbb…`/`eeee…`/`ffff…`）⇒ 只验格式的判据无法区分真假 | 建议 |
| 25 | `unit/aio_abi_test_main.hpp` (155) | 全 | `:76-78 fault_name_usable` 把编译期恒真字面量地址比较改为**运行期内容判定**（正面修掉 TAUT-NULL-GUARD）；`:83-96` 空串名判红 | 通过 |
| 26 | `unit/aio/corrupt_after_rename_interposer.cpp` (59) | 全 | `:42-58` LD_PRELOAD 覆写 `rename`，rename 成功后追加坏字节；`:9` 未设环境变量则**纯透传**（零影响）；`:11-12` 事件行供 runner 判"注入确实发生"⇒ **主动防判据退化**（正面） | 通过 |
| 27 | `unit/core_checkpoint_test.cpp` (84) | 全 | 六组正/负向齐备：`:32-35` 半成品拒绝、`:40-42` begin 未调用拒绝、`:49-50` run_id 中途变更拒绝、`:58-60` 重放幂等 | 通过 |
| 28 | `unit/rt009_node_trace_test.cpp` (83) | 全 | `:35-40` 六字段逐项非空/取值断言；`:26/:54` 走真实 `run_pipeline` 非假模块 | 通过 |
| 29 | `unit/mon004_enforcement_test.cpp` (103) | 全 | `:67-76` 记录/裁决分离**恒 RecordOnly**，正例(违规→RecordOnly)与阴性(Ok→RecordOnly)同时锁死 ⇒ 明确排除恒真/恒假（**正面**）；`:92/:94` 旧失败/旧通过案例双向锁 | 通过 |
| 30 | `arch/test_backend_arch.py` (35) | 全 | 见 §2 第 1 条 | **阻断** |
| 31 | `test_index.csv` (25) | 全 | `:3 eng/tests/arch = STALE_PENDING_RERUN`（用例数留空）；`:6 eng/tests/backend = HOSTED/NA`；`:13 eng/tests/cpu/avx512 = SCRIPT 未实测` | 通过（但见 §6.4） |
| 32-47 | 上表 §1.1 的 16 份 | **[代读]** | 见 §7 子代理复核记录 | 见 §7 |

---

## 4. 发现清单

### 阻断（7，其中 3 条为 [代读]，由子代理 C 提出、我未亲读原文件）

| ID | 位置 | 内容 | 来源 |
|---|---|---|---|
| **BLK-5 [代读]** | `eng/tests/cli/test_resource_gate.py:121` + `resource_gate.h:492` | **阈值语义恒绿门**：断言 `assertIn("0.85", d["msg"])` 检的是诊断串里**硬编码的字面量 `"0.85"`**，而判定用契约字段 `kCpuMeanMinPercent`。把契约 `mean_utilization_min_percent` 85→70 ⇒ 判定用 0.70、消息仍印 0.85、**测试仍绿**。子代理独立重算：唯一对系数敏感的那条断言恰是恒真的。 | C-B2 |
| **BLK-6 [代读]** | `eng/tests/cli/test_resource_gate.py` 全部 case（C++ 驱动 `:29-52`） | **10s 判定域零覆盖**：全部 case 从不设 `active_window_seconds`，保持默认 `-1.0` ⇒ `gate_window_representative`（`resource_gate.h:231-234`）恒真 ⇒ `:358` 的 `NotApplicable` 分支**永不执行**。测试观察到的 `not_applicable` 来自 `:363` 的旧 5s 路径，不是 docstring 宣称的"严格 >10s 判定域"。**附带生产侧边界偏差**：契约写「严格大于 10s」，代码用 `>=10`（`:233`），诊断串（`:554-556`）自称"严格超过"。 | C-B3 |
| **BLK-7 [代读]** | `eng/tests/api/test_upm_parallel.py:49-50` | **死变量恒真门**：`printf(..., calibrate_sum=%.10f, 0.0, ...)` 打印**字面量 0.0**，第 42 行实算的 `csum` 是死变量。反例：把 `p2_upm_calibrate_block` 改成乘 0（星 flux 全灭）**全文件仍绿** ⇒ `CON-005 OneTvsTwoTDetermine` 的标定容差部分无任何证据。另 docstring 说「1T/2T/4T」实际只跑 1T/2T。 | C-B1 |

### 阻断（子代理 E 提出，均为 [代读]，我未亲读原文件 —— 其中两条**推翻了我自己的"通过"评级**）

| ID | 位置 | 内容 | 来源 |
|---|---|---|---|
| **BLK-8 [代读]** | `eng/tests/unit/aio_abi_selfcheck.cpp:47-48` + `:167-174` | **fail-open，且推翻我 §3 第 15 行的"通过"评级。** `child_env[] = {fbuf.data(), nullptr}` **整体替换**子进程环境（只剩 `ACSD_AIO_FAULT`）；而 `:167 if (child_rc == 0) fail;` 是**唯一**失败条件 ⇒ `execve` 失败(`:64`→127)、`fork` 失败(`:60`→127)、组名不识别(`:86`→127)、**子进程被信号杀死(`:68`→−1)** 全部被判"必败验证通过"。**子进程压根没跑起来或崩了，selfcheck 仍绿。** 修法：env 改为叠加于 `environ` 副本；失败条件收紧为「rc==1 且子进程 stderr 含 `FAULT-INJECT <name>`」（该行已由 `aio_abi_test_main.hpp:62` 打印）。 | E-2 |
| **BLK-9 [代读]** | `eng/tests/system/runtime/runtime_resource_record_test.cpp:137-148` | **negative 模式代数恒等式型恒真门**：先 `stripped.erase(pos, col.size())` 再断言 `find(col) == npos` ⇒ 必真；`pos == npos` 分支下 `stripped == csv` 而该列已在正向路径被检查存在 ⇒ 亦必真。**两分支皆绿。** 且注释 `:138` 声称"字段面判定必须报缺"，但 `stripped` **从未被喂回** `:93-98` 的列检查 ⇒ 声明的负向语义**完全没实现**。已注册目标 `runtime_resource_record_negative`（`CMakeLists.txt:93-94`）零鉴别力。 | E-1 |
| **BLK-10 [代读]** | `eng/tests/test_index.csv:14,23` | **悬空索引仍记 PASS**：`eng/tests/glossary`（PASS/5）与 `eng/tests/traceability`（PASS/9）目录已删（`57abe9d8`「删除旧门禁与检查器」+ `ed33f57f`「删净 docs 域机读资产」），索引未跟 ⇒ **索引层正在对不存在的目录报绿**。 | E-3 |
| **BLK-11 [代读]** | `eng/tests/test_index.csv:24` | **统计落后 ~2.6×**：写「56 个 add_test（54 cpp）」，实测 **144 / 141**。 | E-4 |

### 对我本人评级���更正（子代理 E 提供反证，我接受）

- **`unit/aio_abi_selfcheck.cpp`**：我判"通过"（依据 `:93-143` guard-path 三态可触发）。E 指出 `:47-48/:167-174` 的 env 整体替换 + 单一失败条件构成 fail-open ⇒ **实为阻断**，我的"通过"只覆盖了 guard-path 一段，漏了注入子进程路径。已改为 BLK-8。
- **`unit/mon004_enforcement_test.cpp`**：我判"通过"（依据 `:67-76` 记录/裁决分离正负锁死）。E 给出三条反证：`:5` 引**已废止**条款（`RELEASE_STATUS.md:96` 明写「原引『宪章 §10.4/§10.5/§18.2』已废止」）；`:65` 引的 **§9.74 在 `docs/` 全树不存在**；`:36-37,81-95` 锁的 `kCpuP50MinPercent`/`kCpuMeanMinPercent` 是**死断言**——§4 的 `GateConfig c` 从未设 `cpu_p50_percent`/`cpu_mean_percent`（默认 −1.0 未采样）⇒ 生产 `CpuP50Low`/`CpuMeanLow`（`resource_gate.h:375-378`）**在本文件一次都没执行**（7 道门只覆盖 1 道）。**降为须修-高。**

### E 提供的两条正面确认（修正我的风险假设）

- **ABI 锁没有锁桩**：`aio_abi_test_main.hpp:18` → 生产头 `acsd/io/aio_abi_v1.h`；`cpu008` → 生产 `worker_advisor.h`；`p1wcs:156` → CMake 注入的生产 `ipv_solver.cpp`；`mosaic_window` → 生产 `lib/include/acsd/core/mosaic_window.h`。AIO 三个注入点确认在**生产实现** `aio_abi.cpp:135,151,172,193`。⇒ 我在体检表里列的"E 判据读的是桩"风险，本片**不成立**。
- **`noop_module.c` 不是死文件**：`conformance/noop/CMakeLists.txt` 建 `acsd_noop` SHARED，`noop_handshake_test` 链接并 `add_test(NAME module:acsd.conformance.noop)`。
- **`jsonschema_min.py` 不是桩**（与我亲读一致）；E 另给三处真实缺陷：`:117-124` `additionalProperties` 未先排除 `patternProperties` 命中（与 2020-12 相反，方向偏严会误红）；`:136-140` `uniqueItems` 用 `repr(item)` 对 dict 键序敏感 ⇒ 等值异序漏报；`:15-19 TYPES` 死代码。

### 对我重点怀疑的**否定答复**（重要）

我在派单时重点怀疑"实验域的相关系数期望值是恒真门"。E 核实：**`verify_corr.py` / `corrected_pairs.py` 全文无 assert、无 `corrcoef`、无阈值**——**连系数都没算**，该恒真门在此**不成立**。E 指出的是另一形态（更隐蔽）：`verify_corr.py:36` `r = value − Marr[ck] − C[fi,ck]`，而 M/C 正是产出 `value` 的两项 ⇒ **r 恒≈0，是往返自证**；`corrected_pairs.py:11,31` 另有 F 型（只取覆盖最高 tile + `if n<5000: continue` 静默筛除）。

### 实验域纪律（负责人裁决直接命中）

`eng/tests/validation/release02/` **78 份运行结果归档入库**（35 json / 29 log / 14 txt），违反「实验域只留正向代码与报告」。**"归档落后一整代"的具体实例**：`q2_snr_smoothness/` 并存三代 `synth_q2{,_v2,_v3}.py`，而 `synth_results.json` 是**第一代**归档（11:52），v2（11:54）**无归档**，v3 有（12:13）⇒ 读者可能引用落后一整代的归档。〔代读〕


| ID | 位置 | 内容 |
|---|---|---|
| **BLK-1** | `eng/tests/arch/test_backend_arch.py:6,11,14,18,22` | 文档路径随 ARCH-001 迁移失效 ⇒ 5 用例全 ERROR；`:22` 的 `and` 短路使 `"禁止任意"` 断言**永不求值**，而该串在真档中 0 次 ⇒ **死断言掩盖真红**。`:14` 的 `"CPU/OS"` 亦 0 次（真档为 `CPU / OS`）。被 `test_index.csv:3 STALE_PENDING_RERUN` 长期掩盖。 |
| **BLK-2** | `eng/tests/unit/p1_hips/publish_atomic_test.c:651` vs `:138` / `aio_publish.cpp:99` | 手工重建 staging 路径**漏前导 `.`** ⇒ `touch_file:653` 静默失败 ⇒ `:668` 恒真 ⇒ 「残留自愈」从未被检验。 |
| **BLK-3** | `eng/tests/unit/p1_hips/publish_atomic_test.c:567→589` | `EXPECT(pid >= 0)` 不中断 ⇒ `fork()` 失败即 `kill(-1, SIGKILL)` 广播杀整进程组。 |
| **BLK-4** | `eng/tests/backend/test_p2003_seam_oracle.py:288` | 门为 `spread < 0.10*500.0 = 50`；同标度下 fixture 星峰值 2.0、扩展源 0.5 ⇒ 松 250×/1000×，**结构上不可能翻红**。`:289` 失败消息写 200、代码用 500、fixture 注释写 500 —— **三处常数互不一致**。 |

### 本片价值最高的一条（单列）：测试把生产缺陷固化成了"预期行为"

**`eng/tests/artifact/test_phase_product_exchange.py:45-59`（M12）〔代读，我未亲读该 validator 源码〕**

`base(role)` 只改**顶层** `product_role` / `type_id`，而内嵌的 `artifact_manifest.type_id` 始终停在 `"acsd.phase2.mosaic_hips.v1"`。validator 从不把顶层 `type_id` 与 `artifact_manifest.type_id` 做交叉核对。而契约矩阵 `R-NO-NAME-BINDING` **明文要求**角色由「exchange.product_role + **artifact_manifest.type_id**（schema 层约束一致）」共同判定。

⇒ 后果：**一个"顶层宣称 phase3/planar-fits、内嵌 manifest 宣称 phase2/mosaic-hips"的自相矛盾交换对象被接受**，而测试正是在构造并断言这种形状被接受（`base("phase3_planar_fits_v1")` 用于 `test_fits_geometry_requires_tan`；`base("phase1_product_v1")` 用于三个用例）。
⇒ 这不是普通的"恒绿门"，而是**判据把一个生产漏洞锁死成合规基线**——即便日后有人修好 validator，这条测试反而会转红。任何整改都必须同时改契约或改 fixture，否则会被测试挡回。

> 与之配套的 M11 需要精确表述（我已复核并修正 C 的初判）：`eng/tests/common/jsonschema_min.py` **本身是真实现**（我亲读 173 行，`:108-110 required`、`:70-71 $ref` 兄弟、`:99-104 if/then/else`、`:113-122 propertyNames` 全部落地，非简化桩）；C 的正确指摘是 —— **这套正测无法区分"真 validator"与"恒返回 `(True, [])` 的桩"**（无正向 non-vacuity 注入）。二者是不同的命题，不可混为一谈。

### 须修-高（子代理 C 补交后采信，均为 [代读]）

| ID | 位置 | 内容 |
|---|---|---|
| SR-C1 | `test_phase_product_exchange.py` 全体 | 32 个用例中 10 个正测可被"恒返回 `(True,[])`"桩全绿；`TestRunIdIndependence` 按构造恒真（validator 源码从不读 `run_id`/`session_id`）；**该 validator 零生产调用者**（全仓命中仅自身、本测试、`p2002_unc_rej_prov_test.cpp:1809` 的文本锚点断言）⇒ 无生产消费者的门。 |
| SR-C2 | `test_p1003_drizzle_path.py:33` | `INTERNAL_BANNED = ("spawn_frame_from_fits",)` 是**幽灵符号**（排除 `run/` 后全仓零命中）。三面判据对它结构上不可能翻红。 |
| SR-C3 | `test_phase1_inprocess.py:11-14` | 退役登记**点名错载体并声称一个不存在的阻塞**：称新载体是 `cmd_session3_run`「被阻塞」；实际 `command_tree.h:106-109`「doctor 承载 verify（--run-manifest）」、`commands.cpp:2546` 在位未被阻塞 ⇒ 属登记失实。 |
| SR-C4 | `test_phase1_inprocess.py:160-177` | 断言消息陈述的**是一个产品违反的不变式**：`commands.cpp:753-759` 确实 spawn `python3 eng/tools/quality/gen_run_graphs.py`；测试在 t=0.8s 采样而进程仍在入口 sleep 内，渲染落在窗口外。产品自身注释说的是"科学段进程内"（窄面），消息写宽了。与我独立发现的 SR-14（`/proc` 缺失 fail-open）**同段并存**。 |



| ID | 位置 | 内容 |
|---|---|---|
| **SR-A1** | `cosmetic_integration_test.cpp:1023-1024` → `:1070`；`gaia_integration_test.c:779-780` → `:826` | **T8 纯恒真门**：`call_count` 在**写点**被硬编码为字面量 `1`，读点再断言 `cc == 1`。自写自读，判据与任何观测量零关联（生产侧 `context.cpp:299` 的 `if(call_count)` 是真实计数）。 |
| **SR-A2** | `cosmetic:1024` → `:1071`；`gaia:780` → `:827` | **T9 的 `granted_workers` 腿纯恒真**：写点写入的是 `ex.max_workers`（测试自己在 `:792`/gaia `:645` 设为常量 3/2）；桩 `ex_acquire` 只返回 0/-1 并记 `last_leased`，**根本不存在 "granted" 这个独立观测量**。标签却写「实租借观测」。 |
| **SR-A3** | `cosmetic:38-39,449`；`gaia:27-28,359` | **伪引 + 漏规则**：声称「`context.cpp detect_repeated_calls` 同语义，键=(node_id,operation)」。生产 `context.cpp:404` 实有**两条**规则 —— (a) hidden-session-fanout（同一 `entry` 出现在 ≥2 个 node，`:407-426`）、(b) repeated-call（同 node MODULE_CALL>1，`:427-433`）。本地 `replay_violations` **只实现 (b)**；而本测试给两个 node 填了**同一个** `entry="acsd_module_query_v1"`（`:1023`/gaia `:779`），把这份 trace 喂给生产 `detect_repeated_calls` **会真的报出 fanout 违规** —— 本地重放正好把它吃掉。键也不对：生产只用 `node_id`，JSONL 里根本没有 `operation` 字段。 |
| **SR-A4** | `cosmetic:957`（C10）、`:963`（C11） | **冷点检测恒绿方向**：fixture 的 bias 是常量场（`p1cos_fixtures.hpp:81-82`），全帧零冷点 ⇒ 两条 `cold==0` 只能证"没误报"，不能证"能检出"。反例 R-1：把 `detect_cold_pixels` 改成恒返回 0，**本文件全部判据仍全绿**。 |
| **SR-A5** | `publish_atomic_test.c:594-611` | **自愈判据（B 型：证据被被复现动作本身销毁）**：`:608 CHECK(!stage_exists(out))` 为真只因 `aio_publish.cpp` 的 `promote_dir` 把 staging 整个 rename 掉了。若 `remove_tree` 留下孤儿文件，被 SIGKILL 子进程写的半成品 tile 会被 promote 一起带进 `out`，而本用例只查 `:610 manifest.json` 存在，**从不校验 `out` 树的 tile 集合**。 |
| **SR-A6** | `p3_export_e2e_test.cpp:155-158` + oracle `:260/:277` | **正例只跑对角协方差，而对角是合同上的阴性对照形态**：`p3_export.h:134-135` 明写「完整 C_x 为生产路径；**对角仅作阴性对照**」。`build_scene:155-158` 只设对角元，`correlation_kernel_present` 从未置真；oracle 同样只走对角。⇒ **C_x 非对角元素的传播缺陷（把非对角当 0）结构性不可见**，而负例 `m_sb_diag`（`:500`）恰恰在断言"对角必须 REJECT" —— **正例跑的正是负例禁止的那个形态**。 |
| **SR-A7** | `p3_export_e2e_test.cpp:136-147,159-166` | **筛掉真信号（F 型）**：`omega_in_sr` 按列归一条件构造、PSF 硬裁到 `[1,7]²`（`:164`）、8×8 输出 + origin 0.75 被设计成"全像素满覆盖"（`:107-108`）⇒ **边缘权重截断/未重归一这类缺陷一行也测不到**。 |
| **SR-A8** | `cosmetic_integration_test.cpp:961,964`（原 SR-5） | **升级**：DISP-COS-009 的授权条款**已被标注注销**。`lib/algorithms/cosmetic/memory.md:25-27` 逐字记载旧登记"传 nullptr → 检测全禁用、恒等 pass（DISP-COS-009）"**已作废**（WIRING-AUDIT-01 整改），现行 DISP-COS-009（`docs/science/algorithms/COSMETIC_ALGORITHMS.md:481`）是另一条款。正确授权为 `module_entry.cpp:31/714` + descriptor `:64/78` + `docs/engineering/PUBLIC_API.md:194`。〔该 memory.md 原文由子代理 A 读出，我未亲核——保留此边界〕 |
| SR-1 | `gaia_integration_test.c:611` + `:38` | plan 交叉"direct 真值"与 DLL **同源**（同一 `gaia_client_collect_plan_stats`）⇒ `:686-691` 无鉴别力。 |
| SR-2 | `gaia_integration_test.c:692` | `pio == pw` 两字段生产同源输出（`module_entry.c:566-568`）⇒ 恒绿。 |
| SR-3 | `gaia_integration_test.c:620-623` | T1 无 FIPS 已知向量锚，把同一确定性函数两遍互比 = 验自洽非验正确（其兄弟 `cosmetic:745` 有锚）。 |
| SR-4 | `cosmetic_integration_test.cpp:142-145` | **伪引/事实错误**：称 gaia 的 SHA-256 K 表"缺 3 项错 2 项（61 项）"。我逐位核对 `gaia_integration_test.c:131-143` = **64 项标准 FIPS 表，0 缺 0 错**。 |
| SR-5 | `cosmetic_integration_test.cpp:961-967` | 以 `DISP-COS-009` 为"NULL-master 恒等通道/输出==输入 BITWISE"的依据（**待核**，我未逐字核原文）。 |
| SR-6 | `p3_export_e2e_test.cpp:377` | 抓 `staging.tmp`，但 p3 走**文件级** `atomic_publish_file`（`p3_export.cpp:397`），残留是 `<target>.tmp-<pid>-…`（`atomic_publish.cpp:358`）⇒ 16 条负例的第三条 CHECK 结构上不可能红。 |
| SR-7 | `p3_export_e2e_test.cpp:376-384` | 三重 fail-open：`if(!p) return true` / `fgets` 空 → `atoi("")==0` / `2>/dev/null … \|\| true` ⇒ 目录不存在也判绿。 |
| SR-8 | `p2_identifiability_test.cpp:154` | **B13 断言与数学真值相反**（我已独立复算，见 §5-X2）⇒ 该红灯靠 IEEE 舍入噪声兑现。 |
| SR-9 | `p2_identifiability_test.cpp:261` | `tau = v.rank_rtol_effective` 用**被检实现自己的** τ 算 `want_rank` ⇒ 往返自证。 |
| SR-10 | `p2_integrate_test.cpp:209-244` | singlepath 组 20 条 CHECK 读的是 `ModeRoute` 从未被赋值的默认字段（我已独立核 `runtime_contract.h:109-149` 内无 `r.kind =`，`:157` 属另一函数）⇒ 结构性恒绿。 |
| SR-11 | `p2_integrate_test.cpp:312` | `r.corr_ratio >= kCorrRatioMin` 读**生产常量**；独立 oracle `p2_oracle.py:291` 才钉死字面量 1.05。 |
| SR-12 | `p2_oracle.py:240-253` | 只把 Phase1 记录的 `W_info`/`Q` 各自求和 ⇒ 只验合并律，对"W 定义式本身错"零判别力；docstring `:4`「从第一性原理重算」夸大。 |
| SR-13 | `p2_oracle.py:270-274` | `alpha` 计算后**从不参与任何 check**，且注释自承"用 a_psf 近似 a_k=1"（该近似与 `:268` 自身注释矛盾）。 |
| SR-14 | `test_phase1_inprocess.py:168-175` | `/proc/<pid>/task` 不存在 ⇒ `children` 保持 `[]` ⇒ 判绿；自称"验收核心"。 |
| SR-15 | `artifact/test_phase_product_exchange.py:4` | 引 `tasks/03_RUNTIME_DATA_IO_TASKS.md` —— **整个 `tasks/` 目录已不存在**（我已核实）。 |
| SR-16 | `artifact/test_phase_product_exchange.py:104-109` | 名为 phase3 输入的用例实际构造并校验 **phase1** doc ⇒ 名实不符，未测其名。 |
| SR-17 | `unit/fixtures/core_pipeline/README.md:23` | 夹具"逐字节一致"的唯一溯源 `run/PROJECT-GOVERNANCE-01/QA-001/logs/03-fixture-identity.txt` **不存在且被 `.gitignore` 的 `run/*` 覆盖** ⇒ 不可复现。（两个 commit 本身真实。） |

### 建议（14，摘要）

`cpu008:154` 恒真 · `cpu008:149` 计数放大 · `run_provider_avx512:260-266/:294` 解析了却不比对 · `:280-289` 回落无存在性门 · `:304 HW>=2` 条件放宽 · `test_phase1_inprocess:312-313` 断言强度不足 · `test_phase_product_exchange:297-302` 依赖魔数字面 `"size": 524288000` · `p3_export_e2e:490` 字段名 `expect_code_prefix` 实为精确相等 · `p3_export_e2e:321-325` `rel()` 的 `max(1.0,·)` 使小量退化为绝对容差 · `p2_integrate_test:336/338` 同一断言重复计数（影响"整改分母"口径）· `fixture_common:124` 无源失效键 · `fixture_common:75/132/160/168` 裸 assert · `realdata` 目录在 `test_index.csv` 零登记 · `cpu_profile_v2.json` 全占位 sha256

### 须修-中（死引用/漏登记，跨文件）

- `test_p2003_seam_oracle.py:35-37` 引 `run/LINUXMAIN-RESID-01/logs/pair_metrics_iter200.json` —— **不存在 + gitignored**；`N_ITER=200` 的实测依据（iterations=91, rel_improve 8.1e-4）**仓内无可核证据**。

---

## 5. 我主动构造的反例（纸面完成，未运行）

| ID | 构造什么 | 想推翻什么 | 是否推翻 |
|---|---|---|---|
| **X1** | 按 `publish_atomic_test.c:651` 的 `"%.*s/%s%s"` 与按 `:138`/生产 `aio_publish.cpp:99` 的 `"%s/.%s%s"` 分别算 staging 路径 | BLK-2 自愈用例 | **推翻成功** ⇒ 得 `.target.hips_staging.tmp` vs `target.hips_staging.tmp`，`touch_file` 必静默失败 |
| **X2** | `rho = 1−2e-10`，算 `λ_min/λ_max = (1−ρ)/(1+ρ)` | SR-8 的 B13 `identifiable==0` | **推翻成功**。我用 Python 独立复算得 `1.000000082840371e-10 > τ=1e-10` ⇒ **数学上应判绿**，测试却断言判红 ⇒ 该红灯由舍入决定，非数学事实 |
| **X3** | 查 `runtime_contract.h` 中 `route_phase2_weight_token` 函数体（`:109-149`）是否有 `r.kind =` | SR-10 的"结构性恒绿" | **推翻失败 ⇒ 结论成立**。函数体内只有 6 个 `return r;`，无任何 `r.kind`/`r.rc` 赋值；`:157` 的 `r.kind = kProduction` 属 `route_phase3_mode`（`:152-168`） |
| **X4** | 把 `no_tmp_residue` 的 grep 模式与 p3 实际残留名对照 | SR-6 | **推翻成功**。`p3_export.cpp:397` 用 `atomic_publish_file`，残留 `<target>.tmp-…`（`atomic_publish.cpp:358`），不含 `staging` ⇒ grep 恒 0 |
| **X5** | 逐位数 `gaia_integration_test.c:132-143` 的 K 表项数并与 FIPS 180-4 对照 | SR-4 的伪引指控 | **推翻失败 ⇒ 我的指控成立**。6+6+6+6+6+6+6+6+6+6+4 = **64 项，标准值**（含 `0x90befffa…0xc67178f2` 尾 4 项） |
| **X6** | 假设 `jsonschema_min.py` 是简化桩（不实现 `required`/`if-then`）⇒ `missing_output_dir` 负例恒绿 | 我对 SR/负例的怀疑 | **推翻失败 ⇒ 负例成立**。`:108-110` 真实现 `required`，`:99-104` 真实现 `if/then/else` |
| **X7** | 让 node2 把像素改成任意值 | `cosmetic:965` C12 是否恒真 | **未推翻成功 ⇒ C12 是真判据**（非恒绿） |
| **X8** | 让 `cpu008:153` 成立后推 `:154` | `:154` 是否恒真 | **推翻成功**。`:153` 已保证 `workers==avail`，故 `:154` 化为 `avail!=2 \|\| avail==2` = 恒真 |
| **X9** | 让 `assertIn("禁止任意")` 真被求值 | BLK-1 死断言是否掩盖真红 | **推翻成功**。真档 `禁止任意`/`禁止`/`任意` 均 **0 次** ⇒ 该断言一旦求值即红 |
| **X10** | 核 `docs/architecture/` 是否存在 | BLK-1 | **推翻失败 ⇒ 结论成立**。目录不存在；真档在 `docs/engineering/CPU_BACKEND_ARCH.md` |

---

## 6. 盲复算（遮住既有判定，独立取证）

### 6.1 判一致
- **BLK-1 / SR-1 / SR-2 / SR-6 / SR-7 / SR-10**：与我独立读到的一致（X3/X4/X5/X9/X10 均为我先构造后比对）。
- **`a02`/`b01`–`b06`（正面）**：`p2_identifiability` 的 Jacobi 外部参照、`mosaic_window` 的两条负例注入、`aio_abi_selfcheck` 的 guard-path 三态、`core_checkpoint` 的六组正负向、`mon004` 的记录/裁决分离 —— 我盲读时均先判为"高质量、有鉴别力"，与子代理一致。

### 6.2 偏松（我比既有判定更宽松 ⇒ 修正）
- **我最初怀疑 `p2_integrate_test.cpp:501` 的 `.staging.tmp-` 是悬空/恒绿**，独立核 `atomic_publish.cpp:553-554` 发现该命名**真实存在**（目录级发布）⇒ **我自己的初判被推翻，撤回**。
- **我最初认为 `p3_export_e2e_test.cpp:377` 的 `staging.tmp` 是死模式**（因 `.hips_staging.tmp` 含子串 `staging.tmp`），**独立核 `p3_export.cpp:397` 后推翻自己**：p3 从不走目录级发布 ⇒ 子代理 A 的 R5 成立，我采纳。
- **我最初怀疑 `jsonschema_min.py` 是简化桩**，核 `:99-122` 后**撤回**。

### 6.3 偏严（我比既有判定更严 ⇒ 修正子代理）
- **子代理 B 称 `p2_identifiability` 的 `window_peak` 组/相关"转移矩阵最小特征值 = 1.657e-17"**：我复算 `(1−ρ) − τ(1+ρ) = 1.655e-14`，**B 的数值偏小约 1000×**。结论（判决由舍入决定）不变，但幅度以我的为准。
- **子代理 D 称 `test_manifest_isa_declaration.py:112 == 24` "代数盲"**：我复核 `8|16=24` 在互换下不变，**结论成立**（该文件为 `[代读]`，我采信并记录推理链）。

### 6.4 本片最系统性的发现（无人提出）
`eng/tests/test_index.csv` 显示本片 47 份的机器执行覆盖面：
- `eng/tests/arch` = **`STALE_PENDING_RERUN`**，用例数**留空**（`:3`）
- `eng/tests/backend` = **`HOSTED` / `cases=NA`**（`:6`）
- `eng/tests/cpu/avx512` = **`SCRIPT` / 注记"未实测"**（`:13`）；`cpu/avx2`（`:12`）、`cpu/baseline`（`:10`）同
- `eng/tests/realdata` = **整行缺失**（我 grep `test_index.csv` 命中 0 次）

⇒ **本片仅 3 份 config fixture 有正向机器记录**；4 条阻断中有 3 条（BLK-1 属 arch、BLK-2/3 属 unit=ctest、BLK-4 属 backend）正落在**从未机器执行**的登记面上。这解释了缺陷为何长期存活：**"没人重跑" × "断言永不求值/恒绿"叠加**。

---

## 7. 子代理派发记录

派发 **5 个**（`subagent`，全部只读取证，明令禁写/禁编译/禁跑测试/禁读 `/tmp/acsd_g08/`）：

| 代理 | 范围 | 份/行 |
|---|---|---|
| A | cosmetic / gaia / publish_atomic / p3_export_e2e | 4 / 3219 |
| B | p2_integrate_test / p2_identifiability / p2_oracle.py / p2 CMakeLists / p2003 | 5 / 1755 |
| C | cli×3 / artifact / api×2 | 6 / 1245 |
| D | cpu×3 / backend×3 / arch / config×3 | 10 / 1204 |
| E | validation×5 + 其余 17 份 | 22 / 3000 |

### 逐条复核：采信 / 修正 / **否决**

| 代理结论 | 我的处置 | 理由 |
|---|---|---|
| **A: BLK-2** `:651` 漏前导 `.` ⇒ 自愈空转 | **采信（升级为阻断）** | 我独立跑 `awk` 比对 `:138` / `:651` / `aio_publish.cpp:99` 三处格式串，确认不等 |
| **A: BLK-3** `kill(-1,SIGKILL)` | **采信（升级为阻断）** | `EXPECT` 宏（`:46-51`）确认不 return；POSIX 语义确认 |
| **A: R3** `pio==pw` 恒绿 | **采信** | 我读原文时**独立先记下**同一疑点（`:692` 标签自称"同源"） |
| **A: R5** p3 `staging.tmp` 抓错 | **采信，并推翻我自己** | 我原先判定该 grep 能匹配子串；核 `p3_export.cpp:397` + `atomic_publish.cpp:358` 后撤回 |
| **A: R1** DISP-COS-009 伪引 | **降级为"待核"** | 我**未逐字核** `COSMETIC_ALGORITHMS.md:481` 原文，不以转述定罪（SR-5） |
| **A: S3/S4** `gw==3` 回显常量、`asz>0` 标签虚标 | **采信为建议** | 与我读 `:1024/:1071`、`:939/:1072` 的观察一致 |
| **B: B13 断言反数学真值** | **采信（我独立复算确认）** | X2 给出可复现算式 |
| **B: singlepath 结构性恒绿** | **采信（我独立核函数体确认）** | X3 |
| **B: shifted 矩阵特征值 1.657e-17** | **否决其数值** | 我复算为 **1.655e-14**，偏小 ~1000×；结论方向不变（§6.3） |
| **B: E7b/E7c「结构自证」** | **降级为建议** | 子代理自陈"未读 sky_plane.cpp 填充代码，属字段名推断，未实证"——**未经实证的推断不计入发现** |
| **B: p2003 test_03 阈值 500 vs 2.0/0.5** | **采信为阻断** | 逻辑链完整（fixture 量纲 → 同标度 → 松 250×/1000×）；且"三处常数互不一致"可独立复核 |
| **B: `SCI-UPM-CONV-001` 未检索到** | **保留 UNRESOLVED** | 我未独立核，不定罪 |
| **C: 三份 CLI/API/artifact 全套结论** | **首轮未交付；补交后采信 BLK-5/6/7，另否决 3 条** | 见下方"第二轮复核" |
| **C: SR-15 `tasks/03_RUNTIME_DATA_IO_TASKS.md` 悬空** | **采信（独立印证我的发现）** | 我已独立核 `tasks/` 整目录不存在；C 另加两个悬空测试名 `test_example_products_accept`/`test_missing_hash_rejected` |
| **C: M11/M12** `PhaseProductExchangeValidator` **零生产调用者**，且 `base()` 依赖「顶层 `type_id` 与 `manifest.type_id` 不交叉核对」这一**真实生产漏洞**，测试反而把它固化 | **采信为须修 [代读]** | 若成立，这是本片唯一一条"**测试固化了生产缺陷**"的发现，价值高于普通恒绿门 |
| **C: M5** `spawn_frame_from_fits` 是幽灵符号（生产树零命中） | **采信为须修 [代读]** | — |
| **C: M1/M7** test_resource_gate docstring「exit 10 + 低 CPU 必失败」与 `in_process_hard_fail: retired` 相反且零处断言退出码；判据三 fail-open | **采信为建议 [代读]** | 降级：`_gate()` 读不到时抛异常红（非 fail-open），仅部分 fail-open 成立 |
| **C: M3** 21 个 `GateDiag` 只覆盖 10 个、`evaluate_mon001/mon002` 整函数零调用 | **采信为建议 [代读]** | 覆盖面弱，非恒绿 |
| **C: S1** test_phase1_inprocess test_08 只比两个整数 | **与我独立发现一致** | 我读原文时已记为 SR-14 同族（`:312-313` 断言强度不足） |

### 第二轮复核：采信 / 修正 / **否决**（针对 C 与 A 的补交）

| 结论 | 处置 | 理由 |
|---|---|---|
| C-B2 阈值字面量恒绿 | **采信为阻断 [代读]** | 推理链自洽且给出可复现改动（契约 85→70 ⇒ 仍绿）；我未亲读该文件 |
| C-B3 10s 域零覆盖 | **采信为阻断 [代读]** | 同上；且附带发现生产 `>=` vs 契约「严格大于」的边界偏差 |
| C-B1 `calibrate_sum` 死变量 | **采信为阻断 [代读]** | 同上 |
| C: `test_phase_product_exchange.py` 判"阻断" | **降级为须修** | 我已亲读该文件 306 行：校验器 `jsonschema_min.py:99-122` 经我核实为**真实现非桩**，10 个正测并非全部可被恒 True 桩骗过（负测 20+ 条有鉴别力）。"零生产调用者"是治理问题，不是判据恒绿 ⇒ 不构成阻断 |
| C: `test_upm_recovery_oracle.py` k=0.5/base=42 是"独立注入" | **采信为正面** | 与我读 `p2_integrate_test.cpp` 时对"独立注入 vs 同源"的判据一致 |
| A: `detect_repeated_calls` 伪引（SR-A3） | **采信并升级为须修** | 给出生产 `context.cpp:404` 的两条规则行号，且指出本测试自己的 trace 会触发被漏掉的那条 —— 逻辑闭环 |
| A: 冷点检测恒绿（R-1） | **采信为须修（SR-A4）** | 反例具体可构造 |
| A: p3 正例只跑对角协方差（F-1） | **采信为须修（SR-A6）** | 我读原文时注意到 `c_in` 只设对角（`:157`），但未与合同"对角仅作阴性对照"对照 —— A 补上了这一层 |
| A: `DISP-COS-009` 已注销（G-1） | **采信为须修，但标注我未亲核** | 见 SR-A8 |
| A: `module_entry.cpp:700` 行号偏移 2 | **采信为建议** | A 已逐条核 `:606/:610/:719` 精确，仅 `:700` 偏移 |

**采信 11 条、降级 1 条、标注未亲核 1 条。**

| **D: BLK-1** arch 文档路径失效 + `and` 短路 | **采信（升级为阻断，且我判定其比子代理所述更重）** | 我独立核目录不存在 + 三关键词 0 次；**追加判断：被短路的恰是当前真红的那一条** |
| **D: S1** `test_manifest_isa_declaration` avx2/fma 对调 + `==24` 代数盲 | **采信为须修 [代读]** | 推理链自洽（`8|16` 不变） |
| **D: S5** SKIP 门只测 AVX512F 单位 | **采信为须修 [代读]** | 与我亲读的 `:158` 观察一致 |
| **D: S8** `ISA-004` 伪引 | **采信 [代读]，未升级** | 子代理给出 `docs/` 0 次的 grep 证据，但我未亲自复跑该 grep |
| **D: P2** `fixture_common` 无自证式断言 | **采信（推翻我的初判）** | 与我读 `:94-117` 的判断一致：该函数是正当的测试输入制备 |
| **E: 其余 17 份结论** | **未采信任一条为发现** | 代理超长上下文未回结论；这些文件仍列 §1.1「未读完」 |

**否决合计**：数值性否决 1 条（B 的 1.657e-17）、降级为待核/建议 3 条（A-R1、B-E7b/E7c、B 的未检索 ID）、因代理未交付而不予采信 2 组共 **39 份文件的结论**（C 组 6 份 + E 组 17 份的发现项一律不进本片判定）。采信 11 条。

---

## 8. 自证段（可复跑命令，全部只读）

```bash
cd "/workspace/Astro CS Database"
# 0) 基线
git rev-parse HEAD                       # 期望 850a9edefd47434b9ab71bc907c3de1e0814b323
sed -n '1381,1435p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"

# 1) BLK-1 arch 文档路径失效 + 关键词 0 次 + 短路
ls -d docs/architecture                  # 期望：不存在
ls docs/engineering/CPU_BACKEND_ARCH.md  # 期望：存在
for k in "CPU/OS" "禁止任意" "禁止" "任意"; do
  printf "%s=%s\n" "$k" "$(grep -c -- "$k" docs/engineering/CPU_BACKEND_ARCH.md)"
done                                      # 期望全 0
grep -n "assertIn(.*) and assertIn" eng/tests/arch/test_backend_arch.py   # :18 :22
grep -n "eng/tests/arch" eng/tests/test_index.csv                          # STALE_PENDING_RERUN

# 2) BLK-2 staging 路径漏前导点
awk 'NR==138' eng/tests/unit/p1_hips/publish_atomic_test.c   # "%s/.%s%s"
awk 'NR==651' eng/tests/unit/p1_hips/publish_atomic_test.c   # "%.*s/%s%s"  ← 少 '.'
awk 'NR==99'  lib/algorithms/drizzle/hips/src/aio_publish.cpp
grep -n "ACSD_HIPS_STAGE_BASENAME" lib/algorithms/drizzle/hips/include/acsd/hips/publish.h  # ".hips_staging.tmp"
grep -n "if(!f) return;" eng/tests/unit/p1_hips/publish_atomic_test.c   # touch_file 静默吞

# 3) BLK-3 kill(-1) 无守卫
awk 'NR>=566 && NR<=592' eng/tests/unit/p1_hips/publish_atomic_test.c

# 4) BLK-4 p2003 阈值量纲错配
awk 'NR>=281 && NR<=289' eng/tests/backend/test_p2003_seam_oracle.py     # 0.10*500.0
awk 'NR>=30 && NR<=31'  eng/tests/backend/test_p2003_seam_oracle.py
awk 'NR>=19  && NR<=21' eng/tests/backend/test_phase2_fixture_main.cpp    # AREA=1e-8

# 5) SR-4 gaia K 表伪引（我逐位数过 = 64 项标准表）
awk 'NR>=131 && NR<=143' eng/tests/unit/gaia_integration_test.c
awk 'NR>=142 && NR<=145' eng/tests/unit/cosmetic_integration_test.cpp

# 6) SR-10 singlepath 结构性恒绿（函数体内无 r.kind 赋值）
awk 'NR>=109 && NR<=149' lib/infrastructure/cli/runtime_contract.h | grep -n "r\.kind\|r\.rc\|return r;"
awk 'NR>=152 && NR<=168' lib/infrastructure/cli/runtime_contract.h | grep -n "r\.kind = "   # 属 route_phase3_mode

# 7) SR-6 p3 残留名错配
grep -n "atomic_publish_file\|atomic_publish_directory" lib/phase3_session/p3_export.cpp
grep -n 'make_sidecar(target, ".tmp-")' lib/infrastructure/aio/product_io/src/atomic_publish.cpp
awk 'NR>=376 && NR<=384' eng/tests/integration/p3_export/p3_export_e2e_test.cpp

# 8) SR-15/17 悬空引用
ls tasks/03_RUNTIME_DATA_IO_TASKS.md ; ls -d tasks                  # 均不存在
ls "run/PROJECT-GOVERNANCE-01/QA-001/logs/03-fixture-identity.txt"   # 不存在
git -c core.quotepath=false cat-file -t a861d8f6 b7b2dea7            # 期望 commit（这两个真实）

# 9) SR-8 B13 独立复算（唯一允许的"跑"，纯算术不触仓库）
python3 -c "
rho=1.0-2e-10; r=(1.0-rho)/(1.0+rho)
print(r, r>1e-10, '=> 数学上应 GREEN，但测试 :154 断言 RED')
print('shifted lambda_min =', (1.0-rho)-1e-10*(1.0+rho))"

# 10) 覆盖面（test_index.csv）
grep -nE "eng/tests/(arch|backend|cpu/avx512|realdata)" eng/tests/test_index.csv
grep -c "eng/tests/realdata" eng/tests/test_index.csv                # 期望 0（整行缺失）

# 11) 覆盖计数复算
wc -l /tmp/sid6.txt 2>/dev/null || echo "sid6 清单为本轮临时文件，未入库"
```

---

## 9. 给负责人的三条裁决请求

1. **BLK-3（`kill(-1)`）是否按 P0 安全缺陷立即修**——它不属"判据不可信"，属**会破坏构建环境的真实缺陷**，与本轮其余"判据失效"性质不同。
2. **BLK-1 的处置口径**：arch 文档迁移后，是**重锚测试到 `docs/engineering/`**（顺带修 `"CPU/OS"`→`"CPU / OS"`、把 `"禁止任意"` 或补进文档或从测试删除），还是**整体退役该用例**？注意：修路径后该用例会**立刻转红**（3 条关键词 0 次）。
3. **`test_index.csv` 的 `eng/tests/arch` / `backend` / `cpu/*` / `realdata` 四行是否补登记**——本片 47 份中 44 份无机器执行记录，这使"对抗性审核"成为唯一有效防线；建议在 G08 后续轮次对 `HOSTED`/`STALE`/`SCRIPT` 行做一次集中复跑，否则下一遍仍会得到同样的"没人重跑过"结论。
