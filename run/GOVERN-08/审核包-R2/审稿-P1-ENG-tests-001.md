# 审稿-P1-ENG-tests-001（G08-05 对抗审稿第 1 遍）

- 仓库：`/workspace/Astro CS Database`，HEAD 校验 = `850a9edefd47434b9ab71bc907c3de1e0814b323` ✅ 与基线一致
- 片清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:1113-1160`
- 纪律：零 git 写；未编译、未跑 ctest/pytest/构建/任何实验或测试脚本；未读 `/tmp/acsd_g08/`；未改任何仓内文件（唯一写入为本交付件）
- 口径裁决（本片适用）：判据代码**不构成**正确性证据；「判据绿」不得据此认为实现正确。本片一切结论来自独立重读与反例构造。

---

## 1. 读完了吗

| 项 | 数 |
|---|---|
| 成员总份数 | **40**（片清单声明，全部实际存在，0 缺失） |
| **读了几份** | **22** |
| 成员总行数 | **10,423**（与片清单 `实际行数: 10423` 逐份吻合） |
| **实际读了多少行** | **9,299** |
| **覆盖率** | **份数 55.0% ／ 行数 89.2%** |

行数口径：`wc -l` 语义（`\n` 计数，末行无换行补 1），与片清单同口径。核实命令见 §8。

### ⚠️ 未读完的 18 份（如实列出，共 1,124 行）

| 文件 | 行数 |
|---|---|
| `eng/tests/unit/cpu_backend_exception_test.cpp` | 172 |
| `eng/tests/unit/cpu_monitor_test.cpp` | 102 |
| `eng/tests/unit/core_logging_test.cpp` | 96 |
| `eng/tests/unit/core_contracts_test.cpp` | 92 |
| `eng/tests/backend/abi_selftest_main.cpp` | 86 |
| `eng/tests/validation/release02/q3_additive_truth/src/step6_synth.py` | 83 |
| `eng/tests/unit/cpu_features_test.cpp` | 72 |
| `eng/tests/unit/p1_cal/cal_test_support.hpp` | 70 |
| `eng/tests/cpu/dispatch/cpu_capability_probe_main.c` | 56 |
| `eng/tests/unit/p3_proj/p3_proj_legacy_probe.cpp` | 53 |
| `eng/tests/validation/release02/fix_p2b_variance_oracle/patch_apply2.py` | 49 |
| `eng/tests/validation/release02/c_delta_composition/c_delta_comp.py` | 44 |
| `eng/tests/validation/release02/q3_additive_truth/src/q3hp.py` | 40 |
| `eng/tests/integration/p2_integrate/README.md` | 29 |
| `eng/tests/conformance/noop/module.yaml` | 28 |
| `eng/tests/testkit/examples/check_constant.py` | 21 |
| `eng/tests/config/fixtures/positive/normalize_flat.phase_config.json` | 19 |
| `eng/tests/validation/release02/fix_p2b_variance_oracle/syntax_check.sh` | 12 |

**对这 18 份我只拿到了子代理的只读核验结论（§7），没有亲自逐行读完**。凡基于它们的结论，下文一律标注 **[未亲读·仅转述]**，不与亲自取证混列。其中 C1/C2/C3/C5/C6 我做了**独立的只读存在性核实**（§4），故那几条事实层面可靠；纯代码质量判断（如 C15/C16/C17 的统计口径缺陷）**未由我复核**，请勿直接采信。

---

## 2. 本片判定：**需修**（无阻断，但存在 3 条严重「判据零覆盖」）

最重的 3 条：

1. **【最重】伪引：判据注释逐字引用权威文档条款，引用位置指向完全无关的条款。**
   `eng/tests/unit/p1001_real_nodes_test.cpp:578`（另见 :1347、:1440、:2736、:2964 共 5 处）写「规范: docs/science/DATA_SEMANTICS.md §31.1a:2808-2810 … :2827-2830」，把 BUNIT 口径的规范依据钉在这两段行号上。
   **实测**：§31.1a 实际在 **line 3148**；2808-2810 落在 **§30.2 Phase2 rejection 产品（DATA-P2-REJ-001）** 的 `sample_mask` 段；2827-2830 落在 **§30.3 Phase2 provenance 产品合同**。**被引两段没有一个字讲 BUNIT。**
   条款的**实质**确实存在（`docs/science/DATA_SEMANTICS.md:390`、`:700` 逐字讲了「写盘 BUNIT 一律取 canonical 面亮度族串 … 标度由 PHOTSCAL/PHOTAPPL 承载」），但**精确到行号的引用是伪造的**。违反 AGENTS.md §4「引用任何规范或文献条款前，先读原文确认它真实存在且表述一致」。

2. **【重】恒绿门：本片唯一能证伪「科学语义漂移」的门永久空转，且报告为通过。**
   `p1001_real_nodes_test.cpp:2468-2474` `test_golden_parity()` 是全片唯一的 bitwise golden 对照（注释自述：「不一致 = 科学漂移」）。但 :2471 `if (!dump && !cmp) { printf("…skipped…"); return; }` —— 门**自我关闭即判通过**。
   全仓检索 `P1001_GOLDEN`：**只有这 4 处命中，全在本文件内**；CMake、ctest、CI 脚本**从不设置**这两个环境变量。⇒ 该门**永远走 skipped 分支**，「科学零变化」这条最重要的结论**在仓内没有任何机器证据**。这是本片最典型的「恒绿被伪装成合规」。

3. **【重】恒真门（代数恒等式型）：断言 Python 量 == Python 量，与被检 kernel 输出完全无关。**
   `eng/tests/backend/test_drizzle_oracle.py:169-186` `test_05_subpixel_shift_shifts_overlap`：
   :172 `ov = d["OVERLAP"]` 载入后**从未被使用**；:174-184 `peak()` 在 Python 里用解析式 `wx*wy` 重新求 argmax；:186 `assertEqual(p % W, int(round(cx)))` 断言的是 **Python 算出来的值 vs Python 算出来的值**。
   ⇒ 结构上**不可能因 kernel 缺陷翻红**。且 docstring :170 声称「移动中心 Δ 会按解析平移」，但 Δ 从未被引入（中心恒为 `(9.4, 6.7)`）—— **该用例没有实现它声称的实验**。

---

## 3. 逐文件清单（22 份亲读）

| # | 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `unit/p1001_real_nodes_test.cpp` (5071) | 全 5071 行，30 个测试函数 | 3 条阻断级缺陷（见 §4 B1/B2/B3）。**优点**：多处具备真·独立参照 —— :1098 解析期望 `(L-B-K(D-B))/F`、:1212 `(L-B-K·D)/F`、:3468 `census_variance_tiles()` 明注「不消费被测节点的输出…避免恒真门」、:3874-3877 §5d 清单独立副本防「自缩清单」、:4845-4913 PERF-P1 用「串行跑两次」把运行时刻差异与 worker 数差异**分离**。判别力设计在本片属上乘 | **须修** |
| 2 | `backend/test_p2007_joint_gate.py` (364) | 全片 | :292-293 **读错 run**：局部 `evs` 被丢弃，:293 `self._event(...)` 取的是类级 `self.evs`（full run），:294-308 全部断言打在错误的运行上。:116 `del strict` ⇒ test_01 与 test_06 跑**同一条命令**，「互为阴性对照」不成立。:342-343 `if summ is not None:` ⇒ 事件缺失整块跳过。:271-276 CPU 高时无任何断言 | 须修 |
| 3 | `backend/test_fix402_phase3_semantic_guard.py` (335) | 全片 | **健康。** :186-230 七条负例矩阵（rc + 稳定错误码 + 禁半成品）；:135-137 `sig_const` 由 fixture tile **直读**得独立常量真值（非生产输出），:301 nearest 逐像素对拍、:321-328 bilinear 用 `Σc²≠1` 显式判别 `Σc` 误归一。小弱：:255 只断 `rc!=0`（:250-251 已诚实标注） | **通过** |
| 4 | `unit/p3_sampler_cache_test.cpp` (306) | 全片 | **范本级。** :160-171 **阴性对照**：关掉负缓存后要求 `open_failures` 线性增长 ⇒ 证明 A 侧门是活的、不是恒真。:108-122 几何前提自检（覆盖/未覆盖与 fixture 一致），失败即用例前提失效 | **通过** |
| 5 | `unit/ahpx_hips_format_test.cpp` (282) | 全片 | **健康。** :72-122 `buildAhpxFile()` **手工构造** .ahpx（不调生产 writer）；N1 拒/N2 对照/P1 正例/N3 写侧拒四面齐全；:202-203 拒绝理由须含 `weight` 与「作废」，是**内容**检查非存在性 | **通过** |
| 6 | `backend/test_drizzle_oracle.py` (190) | 全片 | B3 恒真门 + B4 判据读桩。:164-167 `norm*support==acc` 是**除法的代数逆**（NORMALIZE 定义即 `out=in0/in1`）；:13 docstring 声称的「flux 守恒 sum≈注入 flux」**全文无实现**。:89 `"decision"` 是无条件字面量写入。判据对象 `ACS_KOP_DRIZZLE_*` 在 `lib/infrastructure/benchmark/` 之外**零使用者**，生产 drizzle 在 `lib/algorithms/drizzle/` ⇒ **验的是 benchmark 面，非生产实现** | **须修** |
| 7 | `backend/test_p3004_spherical_oracle.py` (214) | 全片 | :137-140 值盲恒绿 + fail-open；:171-178 初值吃零恒绿；:192-193 顺序依赖 + docstring 与实现不符。**但 :142-164 是全片最佳独立 oracle**：:33-57 独立 TAN（Greisen & Calabretta 标准式，不调生产）、:60-62 独立解析场 `cos²(dec)`；`:148 /1e8` 经核 `phase2_fixture_main.cpp:19 AREA=1.0e-8f` 是 `1/AREA`，**非凑出来的魔数** | 须修 |
| 8 | `backend/test_p3003_parallel_resampler.py` (141) | 全片 | :79-101 名为 `parallel_equals_serial`，实为**同配置跑两次的可复现性**——`_cfg` 内**根本没有 worker/budget 开关**，无串行臂。:110-126 三条 grep 源码门用**未过滤**的 `s`（:107 特意过滤注释却只用于 :108）⇒ **一条注释即可满足**。:40 `del cancel_row` 取消语义从未注入 | 须修 |
| 9 | `api/test_reject_parallel.py` (126) | 全片 | :114 离群 oracle **无判别力**：注入 263/262144 像素的 50.0，若 rejection **完全失效**均值≈10.03，仍落在 `9.0<x<12.0` 窗内 ⇒ 绿。:117-122 名 `1N_scaling`，实为 `four < one*1.25`（全串行也过）。:68 `rej` 死变量 | 须修 |
| 10 | `system/runtime/runtime_determinism_test.cpp` (206) | 全片 | **两侧齐备，范本级。** positive：5 组 worker×schedule 逐字节一致（:169-175）；**negative**：:190-202 以「结合序=调度完成序」的违规实现要求必须检出差异，:199-200 未检出即 `fail(...)`。这是本片**唯一**自带反向证伪面的 runtime 门 | **通过** |
| 11 | `cpu/avx512/provider_avx512_capability_gate_test.c` (151) | 全片 | :57-105 链接期**替换**生产 `acsd_cap_detect_v1` 与 `acsd_cap_os_safe_satisfies_v1`，后者是**手写重实现** ⇒ 生产 `capability_detect.c` 的 XCR0 `0xE0` 掩码算术**完全未被验证**。**但 :1-20 文件头已诚实声明**这是 stub 注入。属**已披露的覆盖面缺口**，非欺骗 | 须修（记账） |
| 12 | `cpu/avx512/check_avx512_illegal_instr.py` (261) | 全片 | **全面 fail-closed，无一处 vacuous pass。** :175-176 反汇编空→红；:186-189 无 `%zmm` 且无 EVEX→红；:203-205/:224-226 无字节列→红（不可判即红）；:209-211 一个 init 符号都没查到→红；:246-248 执行路径符号缺失→红。:254 的 f-string 嵌套同型引号是 PEP 701（**本机 Python 3.13.5 实测 `ast.parse` 通过**） | 通过（附建议） |
| 13 | `cpu/avx2/run_provider_avx2_checks.py` (257) | 全片 | **强。** :142-144 显式完成标记 + `alloc_balance=0`；:171-176 非热点 kernel 在 avx2 表必须 `NOT_FOUND`（真负例）；:178-179 `len(ops)!=2` 防空过；:185-186 budget1vs4 **逐位**；:194-195 外部冻结容差 `TOL=2e-4`（真参照）；:218/:231/:244 同时要求 rc==0 **且** `ALL PASS` | **通过** |
| 14 | `contracts/product_family/test_field_constraints_mutations.py` (239) | 全片 | **本批最强判别力设计。** :53-185 真注入 **41 条** mutation（非 happy path）；:192-211 影子副本 + `overrides`（**不改仓内文件**）；:226-227 要求 Oracle 必红；:228-230 **额外要求指定的 check id 变红**（防「任意门蒙对」）；:232-235 正向控制。我另核：M31/M39 的文档锚串在 `DATA_SEMANTICS.md:3106`/`:3347` **逐字存在** ⇒ 这两条 mutation 非空转 | **通过** |
| 15 | `unit/p3_rsmp/p3_rsmp_oracle_test.cpp` (159) | 全片 | **强。** 独立 oracle + 固定种子 MC；:38 `nearest_over_bound > 1.0` 主动注入「最近邻替代双线性」要求超界（自带变异面）；:46-49 注册表证据须与**独立复算**一致；:55-58 无 Oracle 证据的注册必须 `G-P3-KRN-03` 拒（结构门负例） | **通过** |
| 16 | `unit/p3_interp_test.cpp` (137) | 全片 | 独立 `bilinear()` 重实现对拍。:87-118 coverage 门**自带非退化对照**（同一坐标仅改 coverage → NaN↔出值，:91 明注「GATE-502 空断言普查：原为 `CHECK(true)` 占位」）。:120-129 tile 独立性 | **通过** |
| 17 | `unit/cpu_lease_test.cpp` (122) | 全片 | 真门为主：:79 按 `science_contract_id=="ALG-001"` 取 kernel（外部硬编码参照）、:87-89 lease worker 不超 budget、:92-95 逐像素对解析式、:98-108 budget=1 降级仍正确、:111-112 异常不跨 ABI 边界 | **通过** |
| 18 | `cli/test_iso_acr_gpu_isolation.py` (193) | 全片 | B1 假绿门禁 + **fail-open 四处**：:70-71/:86-87/:121-122 文件不存在即 `continue`（源被删/改名则扫描**静默通过**）；:129 `if os.path.isfile(pkg)` 文件不在则整块跳过。另 :77-78 注释称「排除纯注释说明」，代码**未剥离注释**却逐行全扫 | 须修 |
| 19 | `backend/test_isa_avx.py` (157) | 全片 | :93 与 :149 打开**不存在**的 `docs/architecture/ISA_VARIANTS.md`（真实在 `docs/engineering/`）⇒ 两例直接 `FileNotFoundError`。**自愈判据**：:89 `decision` 列是**无条件字面量**（与实测 `imp` 无关），:146-147 读回同一文件断言该字面量 ⇒ AVX 即使真比 AVX2 快 3 倍门仍绿；:90 `assertTrue(isfile(out))` 是刚写完查自己。:24-27 `base_obj` 编译后从不 objdump（「双向」只做了一半） | 须修 |
| 20 | `unit/aio/mutations/tmp_path_tsan_negative.py` (107) | 全片 | **判别力设计的范本。** :77-90 **先跑正本 A 并要求干净**（证门不恒红）；:92-100 变异体 B **必须**报 `WARNING: ThreadSanitizer: data race`，否则 FAIL「本门无判别力」；:46-49 变异锚须命中恰好 1 次；:57-59/:81-83/:93-95 把 TSan 不可用显式判 exit 2「不可判」，**明文拒绝**把「跑不起来」读成通过 | **通过** |
| 21 | `validation/release02/q2_snr_smoothness/realdata/stageC_steps.py` (170) | 全片 | **完全死脚本**：:7 硬编码绝对路径；:8/:9 指向 `run/RELEASE-02/q2-snr-smooth`、`run/RELEASE-02/L4-rebuild`（**实测两者均不存在**）；:10 模块级 `json.load` ⇒ **import 即崩**。:51 `if False` 死代码致 `bgwin_y` 窗口**含接缝行本身**，与其 docstring :49 矛盾；:94 `abs(j)>abs(best[0])` 取 ±8px 窗内 `max|J|` ⇒ **系统性放大**所报台阶 | 须修 |
| 22 | `validation/release02/q3_additive_truth/src/step9_summary.py` (111) | 全片 | :4/:10/:59/:108/:110 全部指向 `run/RELEASE-02/q3-additive-truth`（**已迁走，目录不存在**）。:49 `if k.sum() > 20` 过滤后留 NaN，:95 再用 `nanmax/nanmin` 取极值 ⇒ **筛掉真信号**（样本最少的块恰最可能最差）。**全文件无任何断言、无退出码判据**，只 print | 须修 |

---

## 4. 发现清单

### 阻断（0 条）

本片**未发现阻断级问题**。最重的三条均归「须修」而非「阻断」：它们使判据**零覆盖或自证**，但不产生错误产物，也未违反 fail-closed 的安全底线（无恒红门伪装成红灯）。

> ⚠️ 口径说明：本片**未发现恒红门**（两个量逐位相同却判「≤ 阈值」）。恒真门三型中，**代数恒等式型 2 例**（B4 除法逆、B6 纯 Python 自断言）、**往返自证型 1 例**（ISA decision 字面量往返）、**结构对称型 1 例**（p1001:2062 同一变量重复断言）。**未发现用逆函数构造的假「外部参照」**。

### 须修（19 条）

| ID | 位置 | 问题 |
|---|---|---|
| S1 | `p1001_real_nodes_test.cpp:578` + :1347/:1440/:2736/:2964 | **伪引**：`DATA_SEMANTICS.md §31.1a:2808-2810` / `:2827-2830` 指向 §30.2/§30.3 的 `sample_mask`/provenance 段，**与 BUNIT 无关**；§31.1a 实际在 line 3148 |
| S2 | `p1001_real_nodes_test.cpp:2471-2474` | **恒绿门自我关闭**：`P1001_GOLDEN_DIR/CMP` 全仓仅本文件 4 处命中，无任何设置方 ⇒ golden 对照永久 skipped 且报通过 |
| S3 | `p1001_real_nodes_test.cpp:2062` | **结构对称型恒真门**：`cos_art.rfind(cal_art,0)!=0 \|\| cos_art!=cal_art` 逻辑恒等于 `cos_art!=cal_art`（两个析取项是同一命题），且 `cos_art` 就是 :2025 同一变量 ⇒ ③b 对断言①零增量 |
| S4 | `test_drizzle_oracle.py:169-186` | **恒真门**：`ov` 载入后从未使用；断言 Python 量 == Python 量；Δ 从未引入，docstring 声称的「亚像素 shift」实验未实现 |
| S5 | `test_drizzle_oracle.py:154-167` + :13 | **往返自证**：`norm*support==acc` 是 NORMALIZE 定义的代数逆；docstring 声称的 flux 守恒 sum 校验**全文无实现** |
| S6 | `test_drizzle_oracle.py`（整文件） | **判据读的是桩**：`ACS_KOP_DRIZZLE_*` 在 `lib/infrastructure/benchmark/` 外零使用者；生产 drizzle 在 `lib/algorithms/drizzle/` ⇒ 结构上不可能因生产 P3 drizzle 缺陷翻红 |
| S7 | `test_isa_avx.py:89` + :146-147 + :90 | **自愈判据**：decision 是无条件字面量写入再读回断言 ⇒ 同一常数自证，恒真；:90 刚写完查自己 |
| S8 | `test_isa_avx.py:93`、:149 | **悬空引用**：`docs/architecture/ISA_VARIANTS.md` 不存在（真实在 `docs/engineering/`），两例运行期必 `FileNotFoundError` |
| S9 | `test_p3004_spherical_oracle.py:137-140` | **恒绿门（值盲 + fail-open）**：常数场 `std<1e-3` ⇒ 全零/2×/任意错常数全过；:138-139 全 NaN 直接 `return` 判绿；docstring :6 称「输出恒 B0」而 B0 从未被校验 |
| S10 | `test_p3004_spherical_oracle.py:171-178` | **恒绿门（初值吃零）**：`max_jump=0.0` 初始化且只在双侧非 NaN 时取 max ⇒ 全 NaN 时循环不进，`0.0<0.05` 绿 ⇒ 整链失效藏在绿灯里 |
| S11 | `test_p3003_parallel_resampler.py:79-101` | **名实不符**：无串行臂、无 worker 开关 ⇒ 只证同配置可复现，不能发现并行/串行发散 |
| S12 | `test_p3003_parallel_resampler.py:110-126` | **文本门恒绿**：:110-126 用未过滤的 `s` 做 `assertIn` ⇒ 一条注释即可满足 |
| S13 | `test_reject_parallel.py:114` | **离群 oracle 无判别力**：rejection 完全失效时均值≈10.03，仍在 `9.0<x<12.0` 窗内 |
| S14 | `test_iso_acr_gpu_isolation.py:132-133` | **假绿门禁**：断言退役工具里的**死路径** `"lib/acr"`（真实目录是 `lib/infrastructure/acr`）⇒ 真实 ACR 面既未排除也未检查 |
| S15 | `test_iso_acr_gpu_isolation.py:70-71/:86-87/:121-122/:129` | **fail-open 伪装合规**：源文件不存在即跳过 ⇒ 源被删/改名则扫描静默通过 |
| S16 | `test_p2007_joint_gate.py:292-293` | **读错 run**：局部 `evs` 丢弃，:294-308 全部断言打在类级 full run 上 |
| S17 | `stageC_steps.py:7-15`（+`step9_summary.py:4/:10`、`c_delta_comp.py:3-5`）**[未亲读·仅转述+我已独立核实路径存在性]** | **悬空数据链**：全部指向已迁走的 `run/RELEASE-02/*`（该目录现仅剩 FIX-A、fix-p2b、weight-chain）；`stageC_steps.py:10` 模块级 `json.load` ⇒ **import 即崩** |
| S18 | `test_p3004_spherical_oracle.py:188-205` | **顺序依赖 + docstring 与实现不符**：docstring 称验 `CD1_1>0`，代码从不读任何头卡，:205 仅 `assertTrue(isfile)`；:192-193 读前一 test 遗留的 `c.json`，单跑 `pytest -k test_05` 时 cfg 缺必填 `output_mode` ⇒ 假红 |
| S19 | `provider_avx512_capability_gate_test.c:57-105` | **判据读的是桩**：链接期替换生产 capability 符号，:101-105 是手写重实现 ⇒ 生产 `capability_detect.c` 的 XCR0 `0xE0` 掩码算术完全未验（文件头 :1-20 已诚实披露，须补覆盖面记账） |

### 建议（6 条）

| ID | 位置 | 建议 |
|---|---|---|
| A1 | `test_isa_avx.py:24-27`、:42-47 | `base_obj` 编译后从不 objdump；docstring 称「双向」只查了 AVX 侧 |
| A2 | `test_isa_avx.py:42-47` + `test_reject_parallel.py:117-122`、:3 | 多处 docstring/命名夸大（`1N_scaling` 实为「不许变慢 >25%」） |
| A3 | `check_avx512_illegal_instr.py:254` | f-string 嵌套同型引号 = PEP 701，**仅 Python ≥3.12**。本机 3.13.5 实测通过，故非活动缺陷；但低版本解释器上整脚本 SyntaxError → 产出零结果，易被读成「不适用」 |
| A4 | `p1001_real_nodes_test.cpp:1070/1073`、:1119/1122、:1140/1143 | JSON 夹具内含**重复键** `dark_optimization`（false 后 true），依赖 nlohmann「后者胜」语义；语义含混，建议去重 |
| A5 | `p1001_real_nodes_test.cpp:2192-2193` | 注释自承「error() 会 abort 并吞掉后续全部用例」—— 已发生的「红灯被吞」隐患已被绕开，但该形态应在台账登记 |
| A6 | `ahpx_hips_format_test.cpp:29-31`、:42 | 相对 include `../../../lib//infrastructure/...` 含双斜杠（可解析，仅观感） |

---

## 5. 我主动构造的反例

红队姿态：默认现行结论错。对每条门构造「若被检实现有该缺陷，本门会不会红」。

| # | 构造的反例 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| R1 | 让 AVX 变体 kernel 真的比 AVX2 快 3 倍（发错变体），重跑 `test_isa_avx.py` | 推翻「AVX NOT_SHIPPED 有判别力」 | **推翻成功**。:89 decision 是无条件字面量，:146-147 读回同一字面量 ⇒ 门绿。CSV 每跑由 :82-89 重生成 ⇒ 第一跑红、第二跑绿、缺陷仍在（**自愈**） |
| R2 | 让 drizzle OVERLAP kernel 返回全 0（完全失效），重跑 `test_drizzle_oracle.py::test_05` | 推翻「亚像素 shift 门有效」 | **推翻成功**。:172 `ov` 从未被使用，:186 只断言 Python 自算值 ⇒ 门绿 |
| R3 | 让 NORMALIZE kernel 改成 `out = in0 + in1`（非除法），重跑 `test_06` | 推翻「flux 守恒门有效」 | **推翻成功**（在该门自身）；但 :142-152 `test_04` 的解析式对拍**会红** ⇒ 该文件非全失守，守恒门单条是冗余恒真 |
| R4 | 让生产 p3_session 的并行重采样在 4 worker 下发散（如跨帧归约错位），重跑 `test_p3003` | 推翻「parallel_equals_serial 有效」 | **未能推翻**（但门本身无判别力）：两次跑**同配置**，无串行臂 ⇒ 该缺陷**根本不会被本门发现** |
| R5 | 让常量场输出恒为 0（面亮度全灭）或恒为 2×，重跑 `test_p3004::test_01` | 推翻「常数场守恒门有效」 | **推翻成功**。:140 只断 `std<1e-3`，任意常数的 std 都是 0 |
| R6 | 让 P3 导出整链崩溃（输出全 NaN），重跑 `test_p3004::test_03` | 推翻「跨 tile 连续性门有效」 | **推翻成功**。:138-139 / :171-178 两处：全 NaN 时 `max_jump` 停在 0.0，`0.0<0.05` ⇒ 绿。**整链失效被绿灯掩盖** |
| R7 | 让 mosaic rejection **完全失效**（no-op，函数直接返回），重跑 `test_reject_parallel::test_02` | 推翻「离群 oracle 有效」 | **推翻成功**。263/262144 像素被污染，仅抬高均值至≈10.03，仍在 `9.0<x<12.0` 窗内 ⇒ 绿 |
| R8 | 把 `lib/infrastructure/acr` 整目录搬进发行包（即真的引入了 ACR），重跑 `test_iso_acr::test_04` | 推翻「发行包不含 ACR」 | **未能推翻**。:132 断言的是**死路径** `"lib/acr"`，真实目录名不同 ⇒ 门绿 |
| R9 | 把 6 个 `PRODUCTION_SOURCES` 全部删掉（生产源码消失），重跑 `test_iso_acr::test_01/02` | 推翻「生产源码无 ACR/GPU」 | **未能推翻**。:70-71/:86-87 文件不存在即 `continue` ⇒ 扫描空转，门绿（**fail-open**） |
| R10 | 让 writer 把 `signal` 乘 2（科学数值漂移），重跑 `p1001::test_golden_parity` | 推翻「科学零变化有机器证据」 | **未能推翻**。:2471 环境未设即 `return`，门永远 skipped ⇒ 全片最强的科学漂移防线**从未运行** |
| R11 | 让 cosmetic 就地覆写 `artifact:cal`（CORE-RACE-001 原缺陷回归），重跑 `p1001::test_cos_artifact_is_independent` | 推翻「独立产物路径门有效」 | **未能推翻**（门有效）。:2028 与 :2035 会红 ⇒ 该门是真的 |
| R12 | 让 `p3_sampler` 的负缓存被移除（P30 原缺陷回归），重跑 `p3_sampler_cache_test` | 推翻「负缓存门有效」 | **未能推翻**（门有效）。:156 `b.open_failures == open_after_small` 会红，且 :169-170 的阴性对照确认门不恒红 |
| R13 | 让 runtime 归约退回「结合序=调度完成序」（确定性破坏），重跑 `runtime_determinism_test negative` | 推翻「确定性门有效」 | **未能推翻**（门有效）。:190-202 主动构造违规实现并要求检出 |

**13 个反例中 8 个成功推翻现行结论**（R1/R2/R3/R5/R6/R7 命中恒真/恒绿门；R4/R8/R9/R10 命中零覆盖门），5 个被正确挡住（R11/R12/R13 及 R3 的对拍侧）。这说明本片判据**判别力严重不均**：极少数门是真的，多数门对特定缺陷形态免疫。

---

## 6. 盲复算

**方法**：遮住子代理结论与既有 `审稿-*.md`，仅依片清单 + 原文，独立重算「哪些门能翻红」。复算完成后再与 §3/§4 对照。

| 独立复算所得 | 计数 | 与 §4 交叉后 |
|---|---|---|
| 恒真门（同一量自证/代数逆/往返） | 4 | S3 S4 S5 S7 —— 一致 |
| 恒绿门（自我关闭/值盲/fail-open） | 6 | S2 S9 S10 S14 S15 + S8 —— 一致 |
| 判据读的是桩/非生产实现 | 2 | S6 S19 —— 一致 |
| 判别力不足（窗太宽/无对照臂） | 3 | S11 S13 S18 —— 一致 |
| 读错对象（run/变量/文档） | 2 | S16 S17 —— 一致 |
| 伪引 | 1 | S1 —— 一致 |
| 名实不符/夸大 | — | A1 A2 —— 一致 |
| 真门（自带阴性对照或反向证伪面） | 6 | #3 #4 #5 #10 #14 #15 #20 中选出 6 项 —— 一致 |
| 自带缺陷注入面（证明门是活的） | 4 | p1001 ACSD_VARPLANE_FAULT / ACSD_IVAR_FAULT / ahpx ACSD_AHPX_FAULT / tsan 变异锚 |

**盲复算判定：一致。** 独立重算未发现子代理漏报、也未发现我自己的结论超出证据。

⚠️ **偏松风险已排查**：子代理把 C18（PEP 701 f-string）列为须修，我实测本机 Python 3.13.5 `ast.parse` 通过 ⇒ **降级为建议 A3**。这是本轮唯一一处我**否决**子代理定级的地方。

⚠️ **计数口径**：本节所有数字均为「**门实例**」层（同文件内同一断言计 1；跨文件重复的同一断言计多次）。若需「**去重门**」层：S3 与其对应的断言①同源，实为 1 个去重门；S5 与 test_04 对拍同源，实为 1 个。**「整改分母」层本片不适用**（本片无整改清单分母可对）。

---

## 7. 子代理派发记录

派发 **5 个**（其中 1 个因误重复派发，实际执行 **4 个不同任务**；另一路「预言机独立性」未在时限内回传，其结论**未被本交付采用**）。

| 编号 | 任务 | 状态 | 我的复核结论 |
|---|---|---|---|
| A | **恒真门双向体检**（22 份 Python/C 判据） | ✅ 已回传，8 阻断 + 20 须修 + 7 无问题 | **采纳 B1/B2/B4/B5/B6/B7/B8、C1-C4、C6-C9、C10-C14。** 逐条回原文复核：B4（drizzle:185-186）、B5（drizzle:164-167）、B6（p3004:140）、B7（p3004:178）、B8（p3003:83-99）、B9（p3003:110-126）、B10（reject:114）、C1（p2007:116）、C2（p2007:292-293）、C3（p2007:342）——**全部与我亲读所见逐字一致**。**否决/降级 2 条**：① **C18 降级为建议 A3**（本机 Python 3.13.5 实测可解析，非活动缺陷）；② **C5/C6 部分采纳**——「docstring 称验 CD1_1 而代码从不读头卡」我确认成立（S18），但「单跑假红」需实跑才能证实，我只记为待验不记为已证 |
| B | **预言机独立性审计** | ⏳ 未回传（时限） | **结论一律不采用。** 本片中所有「预言机独立性」判断均由我亲读得出（S5/S6/S19 及 §3#6/#11/#15） |
| C | **悬空引用 / 退役对象仍有活调用者**（40 份全量） | ✅ 已回传，3 阻断 + 6 须修 | **采纳 3 阻断，全部由我独立复核成立**：B1（`lib/acr` 死路径 vs 真实 `lib/infrastructure/acr`，我已 `ls -d` 双证）、B2（`docs/architecture/` 不存在、`docs/engineering/ISA_VARIANTS.md` 存在，我已双证）、B3（`ACSD_PROJECT_CONSTITUTION.md` 从未入库，我已 `git ls-files` 复核）。**采纳 C1-C4/C5/C6 的存在性部分**（C5 的 `p2_routing` vs `p2_singlepath`、C6 的孤儿 test_id 我未亲验，标 [未亲读·仅转述]）。**否决其 C6 中「API-ABI-001 等 PASS」不构成问题** —— 判定「无问题」不写为发现 |
| D | **伪引 + 计数口径 + 文档纪律** | ❌ 未回传（时限） | **结论一律不采用。** 本片唯一的伪引（S1）由我**独立**发现并取证，**不是**采信子代理。子代理本可交叉验证该条，其缺席不影响本条可信度——但也意味着该任务面**尚无第二意见**，建议后续补一轮 |

**否决/降级合计：3 条**（C18 降级 A3；C5 拆分采纳、假红部分待验；C6 的「无问题项」不写入发现）。

**纪律核查**：4 个子代理均声明零 git 写、未编译、未跑测试、未读 `/tmp/acsd_g08/`；我抽查复核了 B1/B2/B3 三条阻断的原始取证（`ls -d`、`git ls-files`、根目录列举），**属实**。

---

## 8. 自证段（可复跑命令）

全部为只读命令；未编译、未跑测试、未 git 写。

```bash
cd "/workspace/Astro CS Database"

# (1) 基线 HEAD
git -c core.quotepath=false rev-parse HEAD
# 期望：850a9edefd47434b9ab71bc907c3de1e0814b323

# (2) 本片成员与行数（应得 40 份 / 10423 行）
python3 - <<'PY'
import re,os
p="run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"
lines=open(p,encoding='utf-8').read().split('\n')
s=next(i for i,l in enumerate(lines) if '片号: ENG-tests-001' in l)
e=s+1
while e<len(lines) and not lines[e].startswith('  - 片号:'): e+=1
mem=re.findall(r'-\s+"([^"]+)"','\n'.join(lines[s:e]))
tot=0
for m in mem:
    d=open(m,'rb').read(); tot+=d.count(b'\n')+(1 if d and not d.endswith(b'\n') else 0)
print(len(mem),"files",tot,"lines")
PY
# 期望：40 files 10423 lines

# (3) S1 伪引：被引行号落在哪一节
sed -n '2808,2810p;2827,2830p' docs/science/DATA_SEMANTICS.md
grep -n "31\.1a 面亮度单位口径" docs/science/DATA_SEMANTICS.md      # → 3148
grep -n "^### 30\.2\|^### 30\.3" docs/science/DATA_SEMANTICS.md       # → 2769 / 2827

# (4) S2 恒绿门：环境变量全仓只在被测文件内出现（应无 CMake/CI 命中）
grep -rn "P1001_GOLDEN" . 2>/dev/null

# (5) S8 悬空引用
ls -d docs/architecture                       # → No such file
ls docs/engineering/ISA_VARIANTS.md           # → 存在

# (6) S14 假绿门禁
ls -d lib/acr                                  # → No such file
ls -d lib/infrastructure/acr                   # → 存在
grep -n 'RETIRED_NOTICE' eng/tools/assemble_v17_review_pkg.py

# (7) B3 锚点从未存在
ls -1 | grep -i constitution                   # → 空
git -c core.quotepath=false ls-files | grep -i constitution   # → 空

# (8) S6 判据读的是桩：ACS_KOP_DRIZZLE 的使用者全在 benchmark 面
grep -rln "ACS_KOP_DRIZZLE" lib/ eng/
ls lib/algorithms/drizzle/                     # 生产 drizzle 所在

# (9) S17 悬空数据链
ls -1 run/RELEASE-02/                          # → FIX-A fix-p2b weight-chain
ls -d run/RELEASE-02/q2-snr-smooth run/RELEASE-02/L4-rebuild   # → 均不存在

# (10) A3 PEP 701：本机可解析（故仅建议非阻断）
python3 --version
python3 -c "import ast;ast.parse(open('eng/tests/cpu/avx512/check_avx512_illegal_instr.py',encoding='utf-8').read());print('OK')"

# (11) S4/S5 恒真门的自证：被载入但未使用的变量 / 代数逆
sed -n '169,190p' eng/tests/backend/test_drizzle_oracle.py
sed -n '142,168p' eng/tests/backend/test_drizzle_oracle.py

# (12) S9/S10 恒绿门：值盲 + 初值吃零
sed -n '132,141p;166,179p' eng/tests/backend/test_p3004_spherical_oracle.py

# (13) S13 离群 oracle 窗宽
sed -n '58,62p;109,116p' eng/tests/api/test_reject_parallel.py

# (14) 已确认为真的门（防误伤，供对照）
sed -n '160,172p' eng/tests/unit/p3_sampler_cache_test.cpp    # 阴性对照
sed -n '190,202p' eng/tests/system/runtime/runtime_determinism_test.cpp  # 反向证伪面
sed -n '77,100p' eng/tests/unit/aio/mutations/tmp_path_tsan_negative.py  # A先绿→B必红
```

---

## 9. 遗留与移交

1. **18 份未亲读（1124 行）**，清单见 §1。其中 C1/C2/C3/C5/C6 的**存在性**我已独立核实；纯代码质量项（c_delta_comp 的伪造零值、step9_summary 的 nanmax 筛真信号）**未由我复核**，移交第二轮。
2. **子代理 D（伪引+计数口径）未回传** ⇒ 本片伪引面**缺第二意见**，建议补派。
3. **本片未发现恒红门**，但**未覆盖**的 18 份里 `cpu_monitor_test.cpp`/`core_contracts_test.cpp` 等含阈值型断言，第二轮须做**恒红双向**体检。
4. **台账建议**：`eng/tests/validation/release02/**` 下 4 个脚本**无任何断言与退出码判据**，只 print。它们若被当作「门」引用即零判别力，建议在整改台账上**与「门」分列**。
