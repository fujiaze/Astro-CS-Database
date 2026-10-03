# 审稿-P1 · ENG-tests-012（第 1 遍 对抗审稿）

- 仓库：`/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`
- 依据：片清单-权威版.yaml `ENG-tests-012`，层 `eng/tests`，成员 48 份 / 10 430 行
- 裁定口径：**一遍 = 对该片材料的一次完整重读**。判据代码不作为正确性证据，只作为被质疑对象。
- 本人全程：**零 git 写**（未 add/commit/checkout/reset/stash）、**零仓内文件改动**、**未编译/未跑 ctest/pytest/未跑任何实验脚本**、**未读 `/tmp/acsd_g08/`**。

---

## 1. 读完了吗（口径如实说明）

**本人逐行完整读完：10 份 / 5 176 行。** 另有 1 份读到 150–217 行、1 份 20 行全文（build.sh）。

| 项 | 数值 |
|---|---|
| 成员份数（权威版） | 48 |
| **本人逐行完整读完** | **10 份 / 5 176 行** |
| 本人部分读（p3_proj_legacy_deviation.py） | 1 份 / 68 行（150–217） |
| 本人单行定点核验（含子代理引用） | 约 22 份，见 §3 |
| 成员总行数 | 10 430 |
| **本人完整行覆盖率** | **5 176 / 10 430 = 49.8 %** |

**⚠️ 未由本人逐行读完的 37 份（如实列出，未声称读完）**

`unit/p1wcs/p1wcs_fixtures.hpp`、`unit/p3_rsmp/p3_rsmp_core_test.cpp`、`unit/cpu007_profile_store_test.cpp`、`unit/mon002_alloc_test.cpp`、`cpu/avx512/provider_avx512_oracle_main.cpp`、`support/acsd_test_posix_compat.h`、`unit/p1wcs/p1wcs_tests_units.cpp`、`unit/xpsd_spectrum_count_bounds_test.c`、`io/make_hips_fixture.py`、`backend/phase1_fixture_main.cpp`、`backend/test_p3006_production_pipeline.py`、`validation/release02/q1_photometry_gradient/src/synth_core.py`、`validation/release02/fix_p1_photometry_apply/p1_apply_oracle.cpp`、`backend/test_isa_variants.py`、`backend/kernel_bench_main.cpp`、`backend/test_p2006_canonical_pipeline.py`、`io/hips_output_fixture.py`、`unit/p1_psfw/p1psfw_tests_gates.cpp`、`testkit/examples/registry.example.json`、`backend/p3_output_fsync_probe.cpp`、`unit/cpu_bench_test.cpp`、`validation/release02/q1_photometry_gradient/src/real_data2.py`、`validation/release02/q2_snr_smoothness/realdata/stageF_assemble.py`、`validation/release02/q2_snr_smoothness/realdata/stageG_patch.py`、`validation/release02/c_delta_composition/convergence.py`、`validation/release02/q3_additive_truth/src/step4b_wcs.py`、`api/test_p2_api.py`、`validation/release02/c_delta_composition/counterfactual_gk.py`、`unit/p1_drz/CMakeLists.txt`、`validation/release02/c_delta_composition/Ck_vs_M.py`、`validation/release02/q2_snr_smoothness/realdata/stageC2_profile.py`、`conformance/noop/include/acsd/noop/types.h`、`config/fixtures/positive/mosaic_blocks.phase_config.json`、`validation/release02/phot_verify/debug2.py`、`unit/p2_sky_kappa/CMakeLists.txt`、`unit/p2_rej/p2_rej_test.cpp` 的 `oracle_expected.inc`（94 502 B，非本片成员）。

> 其中 21 份由子代理逐行读完并交叉复核（§7），17 份**既未本人读完、也未子代理逐行覆盖**，只对被引用的具体行做了定点核验 —— 这是本片最明确的覆盖缺口，已登记为 **S-COVER-01（建议）**。

---

## 2. 本片判定

### **判定：阻断（BLOCK）**

> 理由不是「发现了缺陷」，而是**本片至少 6 份判据在结构上不可能提供正确性证据，其中 4 份根本跑不起来**；在「判据不可信、不必核实为通过」的裁定下，本片**不能为 P1-002 / RT-005 / RT-006 / RT-007 / P2-API / ISA 任何一项背书**。

### 最重的 3 条

**① 自指断言（本轮最高价值产出）：`test_p1002_gaps.py:451-477` 的「独立解析解」用被测函数自己的 CD/CRPIX 构造期望值，且唯一能验证 scale 的变量是死变量。**
`crpix_x, crpix_y = make[1], make[2]`、`cd11..cd22 = make[3]` 全部取自 `p3_wcs_make` 的输出（driver `:132-133`），期望值 `exp_x = CD⁻¹·(ξ,η) + (crpix-1)` 因而与被测量同源。**决定性证据：`:457` 取出的 `scale = 0.0011` 从头到尾未被引用**（`grep -n scale` 只命中 `:18/:129/:457/:510/:515`，`:457` 之外无一进入断言）。⇒ scale / PA / handedness 写错仍全绿。**见 §5 反例 C1。**

**② 恒真门（名实不符）：`test_rt006_trace.py:785-789`**
`test_no_session_duplicate_module_registry_typo` 的 docstring 承诺「生产注册表…7 个 P2 module 保持唯一 entry 绑定」，函数体只有 `self.assertTrue(hasattr(py_detect_violations, "__call__"))` —— 对任意 Python 函数恒真，且**不读任何注册表**（`module_adapters.cpp` 从未被该测试打开）。

**③ 恒绿门 + 已违反的不变式：`test_rt006_trace.py:762-767` 只检查 4 处中的第 1 处，而第 4 处确实违反该不变式。**
`idx = ma.find('ctx.set_provider("baseline")')` 只取首个下标。实测 `module_adapters.cpp` 有 **4 处** `ctx.set_provider("baseline")`（619 / 13877 / 14088 / **16001**），而 `hs.init(cap)` 只有 3 处（612 / 13866 / 14078）。**本人实读 `module_adapters.cpp:15988-16001`：该 P3 export 节点 `execute()` 内 `acquire_lease` → `release()` 之后直接 `ctx.set_provider("baseline")`，全程无任何 host init。** 不变式在生产里已被违反，判据结构上看不见另外三处。**见 §5 反例 C2。**

---

## 3. 逐文件清单（读了什么 / 看到什么 / 判定）

### 本人完整读完的 10 份

| 文件 | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---|---|---|---|
| `eng/tests/unit/p2_rej/p2_rej_test.cpp`（931） | 6 个 mode 的全部断言；独立 Oracle `oracle_expected.inc` 的 6 个校准用例元数据 | **质量最高的文件**：`ref_decide`(`:147-186`) 是不调被测函数的独立转写；16 条负向注入逐条「先验基线绿→注入→断言红」；`:350` 自认 `PENDING_OWNER_SIGNOFF SO-07` | **通过（须修 1）** |
| ↳ 校准门 BINMIN 疑点 | 我最初怀疑 `rejection.cpp:2931` 的 `nb >= BINMIN(50)` × `bin_count(10)` ⇒ 需 ≥500 样本，否则 RELIABILITY/BSS 分支永不触发 | **自我推翻**：`oracle_expected.inc:91` `oracle_calib_n[6] = {2000,600,600,60,600,2000}`，4 个状态（OK/BSS_FAIL/RELIABILITY_FAIL/INSUFFICIENT）全被覆盖 | 疑点不成立，已否决 |
| ↳ `mode_units` 阈值钉死 | `:351-353` | `CHECK(P2_REJ_CALIB_BINMIN == 50u)` 等 —— 与 `rejection.h:496-498` 逐字相同，**「用定义式当期望量」的常量钉死**，且注释自认未签署 | **须修 S1** |
| ↳ `mode_reference` recall | `:540` | `CHECK_NEAR(buf.out.recall, (lo+hi)/n, 1e-12)` 与 `rejection.cpp:2864-2865` 的定义式逐字相同 ⇒ 该处自指；`mode_oracle:315` 另有 Python oracle 值覆盖 | **建议 S2** |
| `eng/tests/runtime/integration/test_rt006_trace.py`（927） | 5 个 driver 场景 + 7 个静态/编译闭包判据 | 见 §2 最重②③；另 `:777-783` 只断言 `trace_replay.py` **含 4 个子串**（其中 4 个同时出现在其 docstring `:7/:9/:10/:113` ⇒ 删掉实现只留注释仍绿）；`:23`/`:872` 宣称的「双实现互证」**无任何测试把同一输入同时喂给 C++ 与 Python**；`:473` `_SRC_TRACE=CORE/"trace.cpp"` 该文件不存在 | **阻断** |
| `eng/tests/unit/p1_cal/cal_covariance_test.cpp`（755） | 8 条负向注入、MC oracle、dense oracle | 注入机制**落在生产代码里**：`lib/algorithms/coverage/...` 实为 `lib/algorithms/calibration/src/calibration_covariance.cpp:26,30,71,148,216,298,301,395,546,616,657` —— 8 处 `fault_is(...)` 分支由**环境变量 `ACSD_V6_CAL_FAULT`**（`:71`）激活，无「仅测试二进制」护栏 | **须修 M1** |
| `eng/tests/backend/test_p1002_gaps.py`（596） | 3 个 driver、19 个用例 | 见 §2①；`:199-200` 两个已删 TU；`:368-384` 名为饱和拒绝却从不调用 `Photometer`；`:105-106` `plat` 由**测试自己合成的 u16 数组**统计，与生产无关 | **阻断** |
| `eng/tests/runtime/test_rt005_plan_estimator.py`（555） | 7 组 C++ 场景 + 6 个静态判据 | `:475-487` 恒真门（见 §5 C3）；`:206-230`+`:245` `CHECK(lo2==lo && hi2==hi)` 断言「独立 oracle」**等于**生产 API 的输出界，且 oracle 的输入 `write_bytes/read_bytes/max_useful_workers` 全取自 `estimate_plan` 返回 ⇒ `:9-11`「双向独立推导…错误同源即双 FAIL」的因果是反的（同源错误产生 GREEN） | **阻断** |
| `eng/tests/runtime/integration/test_rt007_cancel_resume.py`（436） | 5 场景 + 5 个子测试 | 覆盖真实、负向充分。`:73-80` `status_of` 对缺失节点回落 `PLANNED`，配合 `:141` 的 `!= RUNNING` ⇒ 调度器**完全不报 B 状态也通过**；`:162` 「run 外 cancel」场景故意传 `nullptr` 关掉状态通道；`:14-15` 承诺的「临时产物清理」全文件无对应断言（`make_hips` 的 path 是假的 `/run/rt007/fake.hips`） | **须修 M2** |
| `eng/tests/unit/p1wcs/p1wcs_tests_oracle.cpp`（424） | oracle 组 + F2 对拍段 | **`:4` 文件头合同写明「\|Δ\|≤1e-4 px 于中心 90% 区域, 冻结不放宽」，`:390` 实现为 `max_rt < 50.0`** —— 同文件内 1e-4 放宽 5×10⁵ 倍；注释 `:353-354` 自认实测 ~7.64/10.88 px；`:268/288/396` 三处 `in_center90` 把 `:356` 自认的「边缘畸变 ~117px」整域排除 ⇒ 全域无门；`:355` 引用的 `run/tmp_wcs002` 实测不存在；`:349-353` 注释内嵌日期/工单号（违 AGENTS §5） | **阻断** |
| `eng/tests/integration/p1_integrate/EVIDENCE.md`（105） | 交付表 / 冻结节点表 / 实测块 / 负向表 / 未决项 | 见 §4 F1–F3 | **阻断** |
| `eng/tests/contracts/check_data_semantics_anchors.py`（189） | 锚核证器全流程 | `:36` `DOC=docs/contracts/DATA_SEMANTICS.md`，**`docs/contracts/` 整个目录不存在**（实为 `docs/science/DATA_SEMANTICS.md`）⇒ `:98 open(DOC)` **每次调用必抛 FileNotFoundError**；`:185 main()` 恒 `return 0`，即便能跑也永不判红；`:26` 的「修复单」`data_semantics_anchor_fixlist.json` 文件存在但**代码从不读取** | **阻断** |
| `eng/tests/validation/release02/q2_snr_smoothness/synth_q2_v2.py`（258） | 合成场设计、4 种规范、5 个 scenario | 本人读后**未发现恒真门**：`joint_ref_fit`/`gauss_seidel`/`stack_field` 互不相同源，`step()`(`:149-151`) 用两侧 `nanmedian` 差分（不是自我比较）。但 `:59` 噪声 `rng.normal` 与 `:28` 真值 `:49` 的 `B_TRUE` 分离、`:174` 的 gauge 用 `B_TRUE`（生成真值）算 `gscal` —— **属「期望量用生成量」**，作为对照研究可接受，作为验收判据则需 owner 确认 | **建议 S3** |
| `eng/tests/validation/release02/fix_p2a_seam_oracle/build.sh`（20） | 全文 | `:7` `ROOT=.../../../..` —— 本文件在 `eng/tests/validation/release02/fix_p2a_seam_oracle/`，上溯 3 级 = **`<repo>/eng/tests`**，需 4 级；`:13` `-Ithird_party` 顶层不存在；`:15` `run/RELEASE-02/fix-p2a/` 不存在；`:3` 文档用法路径亦不存在 ⇒ **恒红** | **阻断** |

### 本人定点核验但未逐行读完的成员文件（据实标注）

| 文件 | 核验方式 | 结果 |
|---|---|---|
| `unit/p3_proj/p3_proj_legacy_deviation.py`（217） | 本人读 150–217 + 子代理全文 | `:206 reproduced = value >= threshold`；`:210 ok = ok and reproduced`；`:213 return 0 if ok else 1` ⇒ **rc=0 当且仅当 4 个已登记缺陷仍全部复现**；`:11` 声称「任何表外新偏差 → rc=1」但 `main` 只遍历表内 4 项，**表外偏差无论多严重都 rc=0** | **阻断 F4** |
| `api/test_p2_api.py`（52） | `test -d docs/api` | `docs/api/` **不存在**（实为 `docs/engineering/PHASE2_API_V1.md`）⇒ `:19` `setUpClass` 抛错，6 个用例全 ERROR | **阻断 F5** |
| `backend/test_isa_variants.py`（157） | `test -d docs/architecture` | `docs/architecture/` **不存在**（实为 `docs/engineering/ISA_VARIANTS.md`）⇒ `:118` FileNotFoundError | **阻断 F5** |
| `backend/test_p3006_production_pipeline.py`（172） | 本人比对枚举 + 子代理全文 | `:140-142` 白名单 **21 项与 `resource_gate.h:44-74` 的 `enum class GateDiag` 21 个枚举一一对应**（`CpuP↔cpu_p50_low`、`CpuMeanLow↔cpu_mean_low`、`FastFailFirst↔fast_fail_first_10s`、`UtilizationP↔utilization_p75_low`）⇒ `assertIn(verdict, 白名单)` 对门能吐出的**任何** verdict 恒真；函数名/docstring 写「verdict ok」 | **阻断 F6** |
| `backend/test_p2006_canonical_pipeline.py`（141） | 子代理全文 + 本人比对 | `:112-114` 同一份 21 值白名单，同一恒真 | **阻断 F6** |
| `unit/p1_drz/CMakeLists.txt`（44） / `unit/p2_sky_kappa/CMakeLists.txt`（9） | 本人 `grep add_subdirectory` | **子代理「从未被 configure」的结论我否决**：实际注册在**根** `CMakeLists.txt:1488/1489/1495/1506`，不在 `eng/tests/unit/CMakeLists.txt`（该文件计数 0）。但 `p1_drz/CMakeLists.txt:7` 自称「由 eng/tests/unit/CMakeLists.txt 以 add_subdirectory 引入」——**该声称为假**；`:42` 提到的 `CHK-DRZ-PF-AREA-S1` 在 `eng/` 下只存在于该注释本身 | **须修 M3** |
| `validation/release02/fix_p1_photometry_apply/p1_apply_oracle.cpp`（160） | 子代理全文 | `:94-103`「负例」整块**不调用任何生产代码**：`:101` `check(fabs(worst - fabs(g[3]/g[ref]-1.0)) < 1e-12)` 两侧同源于写死数组 `g[4]={0.80,1.00,1.25,2.82}`，恒真；`:112` `k_photo = 1.0/g[k]` 恰是 `:88` 构造输入所用因子的**逆**，期望值用测试自己的逆因子重算 ⇒ **k 与 1/k 的方向约定不可判** | **阻断 F7**（本人采信：逻辑自洽且可逐行验证） |
| `unit/p1_psfw/p1psfw_tests_gates.cpp`（128） | 子代理全文 | `:100-102` 用生产 `quantile_of` 算期望值，而被检门内部也调同一函数 ⇒ quantile 缺陷永不可见；`:33` `s.w_psfsw={0.5,1.0,2.0}` 在 5 个扫描点**完全相同** ⇒ `max_rel_dev` 结构性为 0 | **须修 M4**（本人采信逻辑，未逐行复核行号） |
| `unit/cpu_bench_test.cpp`（98） | 子代理全文 | `:63-70` 对**本地字面量** `std::sort` 后断言 `med==3.0`，不调任何生产函数；`:11` `using ...bench_kernel` 导入后**全文件未使用** | **须修 M5** |
| `backend/test_p3006` / `test_p2006` 的「孤儿函数」声称 | 本人 grep | `test_p3006:14-16,164` 与 `test_p2006:13-14` 称 `write_run_graphs` 无调用点 —— **实测为假**：`lib/infrastructure/cli/commands.cpp:1886` 与 `:2319` 是两处活调用（`:1883` 注释「**此前**…无任何调用点」表明已被重新接线） | **须修 M6** |
| `io/make_hips_fixture.py`（235） | 子代理全文 | `:177-193` astropy 缺失时 `afits=None` → **静默不写 `Moc.fits`** 而 `build_fixture` 照常成功（fail-closed 伪装） | **须修 M7** |

---

## 4. 发现清单

### 阻断（BLOCKER）— 8

| 编号 | 位置 | 缺陷型 | 结论 |
|---|---|---|---|
| **F1** | `EVIDENCE.md:46-59` + `:26` | 悬空引用 + 归档失效 | §3 实测：`run/v6/P1-INTEGRATE-001` **不存在**、`tests/integration/v6_p1` **不存在**、`run/v6/P1-INTEGRATE-001/logs/` **不存在**。`:50` 的 `cmake -S ../../../tests/integration/v6_p1` **既跑不通、也根本不是交付路径**（交付在 `eng/tests/integration/p1_integrate/`，见 `:22-25`）。⇒ 5/5 PASS、sha256、0 warning 全部**不可复现**；`:4-7` 的 HEAD（95703e6 / a689eff2）也远早于当前 850a9ede |
| **F2** | `EVIDENCE.md:70` | **伪引（无引号裸从句）** | 称「`docs/ACSD_DESIGN.md` §3.1 **禁止 PSF 质量代理进入科学叠加权重**」。本人实读 `docs/ACSD_DESIGN.md:177-184`（§3.1 全节）：**无此禁止条款**；全文 `grep "质量代理"` **零命中**。最接近的真条款在 `:131`（§2.2 区）「可作科学叠加权重的对象只有两个（§3.1）…」，既不在 §3.1、也无「PSF 质量代理」字样。`consume_phase1_group_for_psfsw` 的整体退役正是建立在这条伪造引用上 |
| **F3** | `p1wcs_tests_oracle.cpp:4` vs `:390` | 恒绿门 + 声称与实现不符 | 文件头合同「1e-4 px，冻结不放宽」↔ 实现 `max_rt < 50.0`。`:349-360` 自认 1e-4「数学不可达」、实测 ~7.64/10.88 px。⇒ 真实误差可再劣化 4.6–6.5× 仍绿 |
| **F4** | `p3_proj_legacy_deviation.py:206-213`（+`:11`） | **反向门 + 伪引** | `return 0 if ok else 1`，`ok` 要求 4 个缺陷**全部仍复现** ⇒ **把 `p3_projection.cpp` 修对会让这条 ctest 变红**。`:11` 的「任何表外新偏差 → rc=1」在结构上不可实现 |
| **F5** | `test_p2_api.py:6,19` + `test_isa_variants.py:118` | 悬空引用 | `docs/api/`、`docs/architecture/` **两个目录都已不存在**（正本迁到 `docs/engineering/`）⇒ 各自 `setUpClass` 抛 FileNotFoundError，用例**恒红且恒无信息** |
| **F6** | `test_p3006:140-142` + `test_p2006:112-114` | **恒真门** | 白名单 21 项 = `enum class GateDiag` 全集（本人逐项比对），`assertIn` 对门能吐出的任何 verdict 恒真；函数名/docstring 却写「verdict **ok**」⇒ 20 个失败判据全部放行 |
| **F7** | `p1_apply_oracle.cpp:94-103`、`:112-127` | **自指断言 + 恒真门** | 「负例」整块零生产调用、`:101` 两侧同源恒真；`:112` 用构造输入的**逆因子**当期望 ⇒ 方向约定不可判 |
| **F8** | `check_data_semantics_anchors.py:36`（+`:185`,`:26`） | 悬空引用 + 永不判红 | `docs/contracts/` 整目录不存在 ⇒ 每次调用必抛；即便修好，`main()` 恒 `return 0`；`:26` 的「修复单」文件存在但**从不被读取** |

### 须修（MUST-FIX）— 12

| 编号 | 位置 | 缺陷 |
|---|---|---|
| M1 | `cal_covariance_test.cpp:602/621/634/651/664/687/706/718` + `lib/algorithms/calibration/src/calibration_covariance.cpp:26,30,71,148,216,298,301,395,546,616,657` | 8 条「负向注入」依赖**生产代码内**的 `fault_is()` 分支，由**环境变量 `ACSD_V6_CAL_FAULT`** 激活，无测试二进制护栏。⇒ 生产可被环境变量静默置于缺陷态；「绿」只证明「设了环境变量时门会红」 |
| M2 | `test_rt007:73-80`+`:141`；`:162`；`:14-15` | `status_of` 缺失回落 `PLANNED` ⇒ 不报 B 状态也过；「run 外 cancel」关掉状态通道；「临时产物清理」无断言 |
| M3 | `p1_drz/CMakeLists.txt:7`、`:42` | 自称由 `eng/tests/unit/CMakeLists.txt` 引入 —— 实为根 `CMakeLists.txt:1488`（该文件计数 0）；`:42` 的 `CHK-DRZ-PF-AREA-S1` 只存在于注释 |
| M4 | `p1psfw_tests_gates.cpp:100-102`、`:33` | 用生产 `quantile_of` 当期望值；`w_psfsw` 5 点全同 ⇒ `max_rel_dev` 结构性 0 |
| M5 | `cpu_bench_test.cpp:63-70`、`:11` | 断言 `std::sort` 字面量结果；`bench_kernel` 导入未用 |
| M6 | `test_p3006:14-16,164` + `test_p2006:13-14` | 「`write_run_graphs` 零调用点/孤儿」**实测为假**（`commands.cpp:1886`、`:2319`）—— 且被删的门断言很可能正由它产出 |
| M7 | `make_hips_fixture.py:177-193` | astropy 缺失 ⇒ 静默少写 `Moc.fits`，仍报成功 |
| M8 | `test_p1002_gaps.py:199-200` | `sdet_detector.cpp` / `sdet_background.cpp` 已删（`build/` 下只剩陈旧 `.o`）⇒ 8 个用例恒 ERROR |
| M9 | `test_p1002_gaps.py:368-384` | 「饱和拒绝」测试从不调用 `Photometer`；`TestPhotometryOracle` 只有 `test_01/02/04`，**无 `test_03`** |
| M10 | `test_p1002_gaps.py:105-106,356,372` | `plat` 由测试自己合成的 u16 数组统计 ⇒ 与生产无关，永不可能红 |
| M11 | `test_rt006_trace.py:777-783` | 「双实现同构」只查 4 个子串**存在性**（4 个同时出现在 docstring `:7/:9/:10/:113`）；C++ 与 Python 从未在同一输入上对比 ⇒ `:23`/`:872` 的「双实现互证」不成立 |
| M12 | `test_rt005_plan_estimator.py:206-230,245` | 「独立 oracle」与生产同源（输入取自 `estimate_plan`，共用 `kHipsTileF32Bytes`），`:245` 断言两者**相等** ⇒ `:9-11`「错误同源即双 FAIL」的因果是反的 |

### 建议（SUGGESTION）— 8

S1 `p2_rej_test.cpp:351-353` 常量钉死与定义式同字面、且自认 `PENDING_OWNER_SIGNOFF`｜S2 `p2_rej_test.cpp:540` recall 用定义式当期望（`mode_oracle:315` 另有覆盖）｜S3 `synth_q2_v2.py:174` gauge 用生成量 `B_TRUE` 算期望｜S4 `p1wcs_tests_oracle.cpp:268/288/396` `in_center90` 排除 `:356` 自认的 ~117px 边缘畸变 ⇒ 全域无门｜S5 `p1wcs_tests_oracle.cpp:355` 引用的 `run/tmp_wcs002` 不存在｜S6 `p1wcs_tests_oracle.cpp:349-353` 注释内嵌日期/工单号（违 AGENTS §5）｜S7 `test_rt006_trace.py:473` `_SRC_TRACE` 指向不存在的 `trace.cpp`；`:338` 死循环体｜**S-COVER-01** 本人完整行覆盖率仅 49.8%，17 份既未本人读完也未子代理逐行覆盖（清单见 §1）。

---

## 5. 我主动构造的反例

### C1 — 推翻「WCS 独立解析解」（**成功推翻**）

- **构造**：令 `p3_wcs_make` 把请求的 `scale=0.0011 deg/px` 写错（例如乘 10，或 PA 写成 3°，或 east/left 约定反转），但保持 `world2pix`/`pix2world` **自洽**。
- **期望推翻**：`test_01_known_field_analytic`（`:451-477`）应当变红。
- **结果**：**未能推翻，判据仍绿。** 理由：`cd11..cd22`/`crpix` 取自 `make`（`:455-456`），期望值 `exp_x = CD⁻¹·(ξ,η) + (crpix-1)`（`:470-473`）与被测量同源；`:457` 的 `scale` 变量取出来后**从未被使用**，唯一能证伪 scale/PA 的自然检查缺席。
- **证据**：`grep -n scale eng/tests/backend/test_p1002_gaps.py` → 仅 `:18/:129/:457/:510/:515`，`:457` 之外无一进入断言。
- **结论**：这是**往返自证 + 自指期望**，且是本片最干净的一处。

### C2 — 推翻「baseline 只在 host init 成功后置位」（**未能推翻，判据恒绿**）

- **构造**：在 `module_adapters.cpp` 的第 4 个 `set_provider("baseline")` 处（`:16001`）保留/制造「无任何 host init 即置位」。
- **期望推翻**：`test_no_config_pretending_observation`（`:752-767`）应判红。
- **结果**：**未能推翻。** 本人实读 `module_adapters.cpp:15988-16001`，该 P3 export `execute()` 内确无 host init；判据只检查 `ma[:ma.find(...)]` 这一**前缀**，而前缀里的 `:612 hs.init(cap)` 满足断言 ⇒ 全绿。
- **佐证**：全文件 4 处 `ctx.set_provider("baseline")`（619/13877/14088/16001）对 3 处 `hs.init(cap)`（612/13866/14078）；`:16001` 无 init。

### C3 — 推翻「kernel id 必须来自权威清单」（**未能推翻，判据恒绿**）

- **构造**：把 `lib/infrastructure/scheduler/src/plan_estimator.cpp` 里的 `"integration-accumulate"` 改成拼错的 `"integration-accumlate"`。
- **期望推翻**：`test_kernel_ids_authoritative`（`:475-487`）应判红（「实现中出现的 kernel id 必须都在 backend_table.inc 权威清单内」）。
- **结果**：**未能推翻。** `:483` `alt` 由 `_KNOWN_KERNELS` 自身构造 ⇒ 拼错的 id **不会被正则匹配到**、静默不进 `hits`；`:486-487` 的 `assertIn(k, _KNOWN_KERNELS)` 对由该集合构造的匹配结果**结构性恒真**；`:482` 全程只读 `_SRC`，**从不打开 `_BACKEND_TABLE`**。
- **旁证**：`:479-480` 的 docstring 声称 V15-N-06 已「改为先由 .inc 实测清单构造交替式」——**该声称为假**，`:483` 仍用硬编码集合。

### C4 — 推翻「RT-006 注册表 entry 唯一」（**未能推翻，判据恒真**）

- **构造**：让生产注册表出现重复 entry / 拼错的 module_id（docstring 承诺守护的正是这个）。
- **结果**：**未能推翻。** 函数体只有 `hasattr(py_detect_violations, "__call__")`，对任意 Python 函数恒真；不读 `module_adapters.cpp`。

### C5 — 推翻「P2 拒绝分类正确」（**未能推翻**）

- **构造**：把 `rejection.cpp` 的 `p2_reject_calibration` 里 `P2_REJ_CALIB_BSS_MIN` 的比较方向写反。
- **结果**：**理论上可被 `mode_calibration` 抓到**（6 个 oracle 用例含 BSS_FAIL/RELIABILITY_FAIL）——**此项我判现有判据有效，不作为发现**。列出以说明我并非一律唱反调。

---

## 6. 盲复算（遮住既有判定，独立取证）

**方法**：对 6 个文件先只读源码形成独立判定，再对照子代理给出的判定，逐条判「一致 / 偏松 / 偏严」。

| 文件 | 本人盲判定 | 与子代理对照 | 结论 |
|---|---|---|---|
| `test_p1002_gaps.py` | 阻断（自指期望 + 已删 TU） | 两份报告均判阻断 | **一致** |
| `test_rt006_trace.py` | 阻断（恒真门 + 只查 4 处之 1） | 报告判 MUST-FIX（M4/M5） | **本人偏严**——`:789` 我按 BLOCKER 计（docstring 承诺的生产不变量完全无检查），子代理按 MUST-FIX |
| `test_rt005_plan_estimator.py` | 阻断（`:245` 自指 + `:487` 恒真） | 报告 M3/M12 判 MUST-FIX | **本人偏严** |
| `test_rt007_cancel_resume.py` | 须修 | 报告 M18 判 MUST-FIX + 新增 fail-open | **偏严但方向一致**；报告发现的「场景名拼错即全绿」（`:342-357` + Python 侧 `:404-406` 只断言 rc==0、从不解析 `checks=%d`）是**本人盲读漏掉的真缺陷，采信并补入 M2** |
| `unit/p1_drz` + `p2_sky_kappa` 注册 | 可疑 | 报告 788aa7d3 判「从未被 configure」（BLOCKER） | **我否决该条**：`CMakeLists.txt:1488/1489/1495/1506` 确有注册；该子代理查错了文件。其余部分采信 |
| `check_data_semantics_anchors.py` | 阻断 | **两份报告均未覆盖此文件** | **本人独立发现，无交叉背书** |
| `EVIDENCE.md` | 阻断 | **两份报告均未覆盖此文件** | **本人独立发现**（F1/F2） |
| `p1wcs_tests_oracle.cpp` | 阻断（1e-4→50px） | **两份报告均未覆盖此文件** | **本人独立发现** |
| `build.sh` | 阻断（ROOT 少一级） | 两报告一致 | **一致** |
| `p3_proj_legacy_deviation.py` | 阻断（反向门） | 报告一致 | **一致** |

**总体**：本人盲判定 **偏严 2 处**（rt006、rt005 各高 1 级），**无偏松**；**否决子代理 1 条**（788aa7d3 的 B5「6 个 ctest 从未注册」）；**采信并补入 1 条本人漏检**（rt007 场景名 fail-open）；**本人独立发现 3 处无交叉背书的阻断项**。

---

## 7. 子代理派发记录

派发 **5 个**（`subagent`，只读、禁编译、禁 git 写、禁改文件）。

| # | 范围 | 回报 | 复核结果 |
|---|---|---|---|
| 1 | 11 份 C++ 单测 | 未在本轮单独回报 | 已被 #2/#3 覆盖 |
| 2 | 21 份 runtime/backend/io | 12 BLOCKER / 18 MUST-FIX / 13 SUGGESTION / 8 UNRESOLVED | **逐条复核**：采信 B1/B2/B3/B4/B5/B10/B11/B12、M3/M4/M6/M8/M10/M13/M14。**本人独立复验** `docs/api`、`docs/architecture`、`write_run_graphs`、`GateDiag` 枚举、build.sh ROOT —— 全部**实测为真**。**否决 0 条** |
| 3 | 13 份 validation oracle + EVIDENCE + contracts | 未单独回报（内容并入 #2 组） | 覆盖 `p1_apply_oracle.cpp`、`p1psfw_tests_gates.cpp`、`make_hips_fixture.py` 等 |
| 4 | 21 份 runtime/backend/io（重复派发） | 6 BLOCKER / 14 MUST-FIX / 12 SUGGESTION / 8 UNRESOLVED | **逐条复核**：采信 B1/B2/B3/B4、M1/M2/M6/M8/M9/M12/M13/M16/M18、S4/S5/S12。**否决 2 条**（详见下） |
| 5 | 3 份小文件 + 全片悬空引用横扫 | 未单独回报 | 其结论并入 #2/#4 |

**否决的子代理结论（2 条，必须记录）**

1. **否决 #4 的 B5**：「`eng/tests/unit/CMakeLists.txt` 无 `p1_drz`/`p2_sky_kappa`/`p1_psfw`/`p3_proj` 的 `add_subdirectory` ⇒ 6 个 ctest 从未被 configure」。
   **理由：查错了文件。** 本人实测 `grep -n "add_subdirectory" CMakeLists.txt`（根）得 `:1488 add_subdirectory(eng/tests/unit/p1_drz p1_drz)`、`:1489 p1_psfw`、`:1495 p2_sky_kappa`、`:1506 p3_proj` —— 四者**均已注册**。子代理 #2 在同一问题上给出了正确结论（注册点在根而非 `eng/tests/unit/`）。子代理只查了 `eng/tests/unit/CMakeLists.txt`（计数 0）便推断「死注册」，属**证据不足的过度外推**。
2. **否决 #4 的 B6**：「`test_rt005_plan_estimator.py:62-66` 的 `approx_less` 是未被调用的 static 函数，`-Werror` 下必定编译失败」。
   **理由：该函数自递归**（`:65` 调用自身），GCC 的 `-Wunused-function` 不对自引用函数告警；子代理自己也标注「基于 GCC 语义推断，建议前台实跑确认」。在「不得编译」的纪律下无法证实，**不能作为阻断项入账**。降级为未决（记入 S2 一并复核）。

**采信但降级（1 条）**：#2 的 M14「`test_replay_py_matches_cpp_semantics` 只查子串」—— 本人复核后**采信并升为本片 M11**（因 4 个子串同时出现在被查文件自身的 docstring `:7/:9/:10/:113`，删实现留注释仍绿）。

**采信但标注为「未逐行复核」**：#2/#4 关于 `p1_apply_oracle.cpp`、`p1psfw_tests_gates.cpp`、`cpu_bench_test.cpp` 的行号级结论，本人**采信其逻辑、但未逐行复核行号**（见 §3 表末「未逐行复核行号」标注）。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# 0) 基线
git log -1 --format='%H'            # 期望 850a9edefd47434b9ab71bc907c3de1e0814b323

# 1) 本片成员与行数（期望 48 / 10430）
python3 - <<'PY'
import re,os
p='run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml'
L=open(p,encoding='utf-8').read().splitlines()
s=next(i for i,l in enumerate(L) if l.strip().startswith('- 片号: ENG-tests-012'))
e=next((i for i in range(s+1,len(L)) if L[i].strip().startswith('- 片号:')),len(L))
fs=[l.strip()[3:].strip().strip('"') for l in L[s:e] if l.strip().startswith('- "')]
miss=[f for f in fs if not os.path.exists(f)]
print('份数',len(fs),'行数',sum(sum(1 for _ in open(f,encoding='utf-8')) for f in fs if os.path.exists(f)),'缺',miss)
PY

# 2) F6 恒真门：GateDiag 枚举 vs 白名单（期望 21 vs 21，一一对应）
sed -n '44,74p' lib/infrastructure/cli/resource_gate.h | grep -oE "^\s+[A-Z][A-Za-z]+" | tr -d ' '
sed -n '140,142p' eng/tests/backend/test_p3006_production_pipeline.py
sed -n '112,114p' eng/tests/backend/test_p2006_canonical_pipeline.py

# 3) C1 自指期望：scale 死变量
sed -n '451,477p' eng/tests/backend/test_p1002_gaps.py
grep -n "scale" eng/tests/backend/test_p1002_gaps.py

# 4) C2 只查 4 处之 1
grep -n 'ctx.set_provider("baseline")' lib/infrastructure/scheduler/src/module_adapters.cpp   # 4 处
grep -n "hs.init(cap)"              lib/infrastructure/scheduler/src/module_adapters.cpp   # 3 处
sed -n '762,767p' eng/tests/runtime/integration/test_rt006_trace.py
sed -n '15988,16001p' lib/infrastructure/scheduler/src/module_adapters.cpp                    # 第 4 处无 host init

# 5) C3 kernel-id 恒真门
sed -n '475,487p' eng/tests/runtime/test_rt005_plan_estimator.py
grep -n "_BACKEND_TABLE" eng/tests/runtime/test_rt005_plan_estimator.py                        # :482 只读 _SRC

# 6) C4 rt006 注册表恒真门
sed -n '785,789p' eng/tests/runtime/integration/test_rt006_trace.py

# 7) F8 悬空引用：docs/contracts 整目录不存在
ls docs/contracts 2>&1                       # 期望 No such file
ls docs/science/DATA_SEMANTICS.md
grep -n "fixlist" eng/tests/contracts/check_data_semantics_anchors.py   # 仅 docstring :26

# 8) F1/F2 EVIDENCE.md 归档与伪引
ls run/v6/P1-INTEGRATE-001 2>&1              # 期望 No such file
ls tests/integration/v6_p1 2>&1              # 期望 No such file
ls eng/tests/integration/p1_integrate        # 真实交付路径
sed -n '177,184p' docs/ACSD_DESIGN.md         # §3.1 全文：无「禁止 PSF 质量代理」
grep -n "质量代理" docs/ACSD_DESIGN.md        # 期望零命中
grep -n "可作科学叠加权重的对象只有两个" docs/ACSD_DESIGN.md   # 真条款在 :131（§2.2 区）

# 9) F3 1e-4 → 50px
sed -n '4p;349,360p;390p' eng/tests/unit/p1wcs/p1wcs_tests_oracle.cpp
ls run/tmp_wcs002 2>&1                       # 期望 No such file

# 10) F4 反向门
sed -n '11p;206,213p' eng/tests/unit/p3_proj/p3_proj_legacy_deviation.py

# 11) F5 已删文档目录
ls docs/api 2>&1 ; ls docs/engineering/PHASE2_API_V1.md
ls docs/architecture 2>&1 ; ls docs/engineering/ISA_VARIANTS.md

# 12) M8 已删 TU
ls lib/algorithms/star_detection/src/        # 无 sdet_detector.cpp / sdet_background.cpp
sed -n '199,200p' eng/tests/backend/test_p1002_gaps.py

# 13) build.sh 恒红（ROOT 少一级）
sed -n '3,20p' eng/tests/validation/release02/fix_p2a_seam_oracle/build.sh
ls third_party 2>&1                          # 顶层不存在（实为 lib/third_party）

# 14) M1 故障注入落在生产代码
grep -rn "fault_is\|ACSD_V6_CAL_FAULT" lib/algorithms/calibration/src/calibration_covariance.cpp

# 15) 被否决的子代理 B5（本人复核证据）
grep -n "add_subdirectory.*p1_drz\|add_subdirectory.*p2_sky_kappa\|add_subdirectory.*p1_psfw\|add_subdirectory.*p3_proj" CMakeLists.txt

# 16) M6 「孤儿函数」声称为假
grep -n "write_run_graphs" lib/infrastructure/cli/commands.cpp
```

**纪律自证**：本轮未执行 `ctest` / `pytest` / `cmake` / 任何实验脚本；未产生任何仓内改动（唯一写入为本交付件 `run/GOVERN-08/审核包-R2/审稿-P1-ENG-tests-012.md`）；未读取 `/tmp/acsd_g08/`。

---

## 9. 计数口径声明

本片涉及的门计数按三层分别记，互不混用：

- **门实例层**：本片 48 份文件中，含 `assert`/`CHECK` 的文件 41 份；可执行的顶层判据条目约 210 条（本人完整读到的 10 份内为 96 条）。
- **去重门层**：按「独立不变量」去重后，本片覆盖的独立不变量约 74 条。
- **整改分母层**：本片**阻断 8 + 须修 12 + 建议 8 = 28** 条；其中**本人逐行独立取证 19 条**（62 %），**采信子代理 9 条**（其中 1 条采信后降级、1 条采信后升级）。

> ⚠️ 上述「约」数为人工估算口径，**不作为整改分母的正式依据**；正式分母以负责人裁决为准。

---

## 10. 给负责人的结论

1. **本片判为「阻断」。** 不是因为发现了新缺陷类型，而是因为**本片 6 份判据在结构上不可能提供正确性证据，其中 4 份（`test_p2_api.py`、`test_isa_variants.py`、`test_p1002_gaps.py`、`build.sh`）根本跑不起来**。在「判据不可信」的裁定下，本片不能为 P1-002 / RT-005 / RT-006 / RT-007 / P2-API / ISA 任何一项背书。
2. **最值得推广的三条方法论**（均为本轮首次在本片确认）：
   - **死变量即自指的铁证**：C1 里 `scale` 取出来却从不使用 —— 当「期望量」里出现一个只被赋值、从不参与断言的量，几乎可以直接判定该期望是自指的。
   - **`find()` 只查首处的恒绿门**：C2 里 4 处声明点只查第 1 处，第 4 处已实际违反不变式。**任何 `str.find`/`grep | head -1` 式断言都应被判为不可信。**
   - **恒红与恒绿同源**：F3 的 1e-4 → 50 px 与 F4 的反向门是同一病的两面 —— 前者为了「能绿」而放宽到失去鉴别力，后者为了「锁行为」而要求缺陷**必须继续存在**。两者都把判据从「检验实现」变成了「检验历史」。
3. **子代理的价值与风险**：5 个子代理产出密集，但**有 1 条 BLOCKER 级结论是错的**（查错文件导致把 4 个已注册的 ctest 判成「从未注册」），**有 1 条依赖未验证的编译器语义推断**。⇒ 建议：子代理的「文件不存在 / 注册缺失」类结论必须由 Lead 用**同一条命令**复跑确认，不得转述。
4. **本片诚实缺口**：本人完整行覆盖率 49.8 %，17 份文件既未本人逐行读完、也未被子代理逐行覆盖（清单见 §1）。若负责人要求「一遍 = 全片逐行」，本片需**再跑一遍**补齐这 17 份。