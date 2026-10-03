# 审稿 P1 — ALG-coverage-001（对抗审稿第 1 遍）

- 片号：**ALG-coverage-001**
- 层：`lib/algorithms/coverage`
- 基线：仓库 `/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`（本人 `git rev-parse HEAD` 核实）
- 权威分母：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:109-138`（成员 22 份 / 实际 10184 行）
- 姿态：红队。默认现行结论为错；一切正确性结论来自**本人亲自 read 原文**或**本人亲自跑的只读命令**。未读 `/tmp/acsd_g08/`；未读他人审稿报告；零 git 写；未编译、未跑 ctest/pytest/任何二进制；未修改任何仓内文件。

---

## 1. 读完了吗（口径必须写明）

**口径定义**：
- 「读」= 本人用 read 工具**逐行**打开并通读全文（不以 grep 摘要代替阅读）。
- 行数分母 = `wc -l`（权威清单 10184 行即此口径）。注意 `coverage.h`/`block.h` 的 read 工具行数（173/49）比 `wc -l`（172/48）多 1，因文件末行无换行符；下文统一以 `wc -l` 计入分母。

| 项 | 数 |
|---|---|
| 成员总份数 | **22** |
| 逐行读完的份数 | **16** |
| 成员总行数（`wc -l`） | **10184** |
| 实际逐行读完的行数 | **3180** |
| 部分精读（`synthetic_gate.cpp`） | **446** 行（3259–3704），另做全文件结构普查 |
| 实际触及行数合计 | **3626** |
| **覆盖率（逐行读完 / 总行数）** | **31.2%** |
| **覆盖率（逐行读完 + 精读 / 总行数）** | **35.6%** |
| 完全未读份数 | **5** |

### 1.1 未读完的（如实列出，不得以「已派子代理」充抵）

| 文件 | 行数 | 未读原因 |
|---|---|---|
| `tests/synthetic_gate.cpp` | 6078 | **未读完**。逐行读了 3259–3704（ACR + G5WeightTruthGate 区，即本片风险最高区）；对全文件做了结构普查（104 个 TEST 名与行号、12 处 GTEST_SKIP、全部 catch、断言阈值频次、include 面、零生产符号核验）。**1–3258 与 3705–6078 的逐行阅读未完成**。 |
| `memory.md` | 302 | 未逐行读（仅由子代理覆盖线索） |
| `hips_p2/README.md` | 185 | 未逐行读 |
| `hips_p2/memory.md` | 96 | 未逐行读 |
| `CMakeLists.txt` | 252 | 未逐行读（关键行 `P2_ENABLE_OPENMP` 由子代理取证并经本人复核） |
| `module.yaml` | 93 | 未逐行读 |

**这是一份覆盖不完整的交付。** 本片 60% 的行数集中在 `synthetic_gate.cpp`，该文件只逐行读了 7.3%。第 4 节中该文件的判定仅覆盖已读区段，未读区段不得视为已审。

---

## 2. 本片判定

**判定：需修**（无「阻断」级生产正确性缺陷；有 1 项须立即处置的门禁失效 + 2 项新发现的机制缺陷）

最重 3 条：

1. **【新·须修】`satellite_gate_build.py:133-149` —— 卫星线门的「干净对照臂」实际含有注入的卫星线。**
   `out[trail] += TRAIL_AMP`（`:137`）发生在 `clean_sig[ip] = out.copy()`（`:149`）**之前**，`:143-147` 也已把含 trail 的 `out` 写盘。`:105` 与 `:154` 的注释都自称 `clean_sig` 是「无注入版本（bias 对照）」。⇒ `frame10_clean.hips` 与 `frame10.hips` 逐字节相同，`stage2_satellite_clean.json`（`:181`）的对照臂失效。
   **两难**：若 trail 真的注入（`:210` 打印 trail 像素数即为证），对照臂无效；若 trail 一个像素都没注入（`n_trail==0`，`:136`），则整扇门是空门。**两种状态都不构成有效门。**

2. **【新·须修】`synthetic_gate.cpp:3259-3352` `G5WeightTruthGate` —— 名字叫「权重真值门」，实际不调用任何生产代码。**
   本人实测该区段对 `p2_*` / `upm` / `integrate.` 的引用数为 **0**。七个权重函数（`:3288-3299`）与积分器（`:3274-3287`）全是本文件内的局部 lambda，随后 `:3323-3325`、`:3342`、`:3345` 断言的是**这些 lambda 自己算出来的数之间的关系**。
   ⇒ 典型的**自洽式断言**：同一个定义式既当被检量又当期望量。`upm.cpp`/`integrate.cpp` 的权重实现即使全错，本门仍恒绿。
   附带：`:3335` `const double v1c = v1[p] - 1.5;` 把「UPM 校准」在测试里**手工假定为完美**，从未运行 UPM。

3. **【新·须修】`README.md:51-52` 声明的冻结权重式，与 `stage2_common.cpp:545-546` 的实际赋值不符。**
   README 冻结式：`w_UPM = quality_factor × geometric_reliability × control_ivar`（三因子）。
   代码实际：`mcfg.quality_mode = 0;`（`:545`）与 `mcfg.control_reliability = 1.0;`（`:546`）——两因子被**硬编码**，既不读 `cfg` 也不从几何/物理导出（本人 grep 确认这两个字段在 `stage2_common.cpp` 内**只出现这两次**，均为字面量赋值）。
   ⇒ 三个因子中两个在生产恒为常数，README 的「唯一冻结式」描述的是一条**生产不执行的公式**。

---

## 3. 逐文件清单

| # | 文件 | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---|---|---|---|---|
| 1 | `src/stage2_common.cpp` 554 | **全文逐行** | ① `version` 键**读取 0 次**（实测 `sed -n '7,512p' \| grep -c version` = 0），但三份仓内配置都写 `"version": 1`；② 未知键从不拒绝——只对少数退役键 `contains` 后 return false，**任何拼写错误的键（如 `integretion`）被静默忽略并取默认值**（`:507` 只有 `catch` 兜底）；③ `:545-546` 权重两因子硬编码；④ `:514-531` `p2_acr_block_eligible` 无条件 `return false`（4 参数全 `(void)` 丢弃） | 需修 |
| 2 | `src/acr_kernels.cpp` 368 | **全文逐行** | ① `:160-166` `n_valid==0` 时静默写 `dst[p]=0.0f` + `out_valid=0` 并 `return`，**无错误码、无告警**，与本文件其它失败一律 throw 的风格相反；② `:199-200` integrate 非 OK 时静默写 `0.0f`；③ `:141` `if (wmode && *wmode == 2)` —— **相等判定非值域判定**，且 `wmode &&` 使「槽缺失/越界」（`read_scalar` 返回 `nullopt`）一并放行；④ `:169`/`:191` `weights = (sup != nullptr) ? … : nullptr`，buffer 2 未绑即静默降级为等权；⑤ `:362-363` 声明 `numeric.compute = fp64`，而 `:105-106` buffer 实为 `float*`、`:122` `value_dtype = 0 // fp32`；⑥ 全文件 `std::thread` 零命中 | 需修 |
| 3 | `src/async_io.cpp` 3 | **全文逐行** | 全文 3 行 = 2 行注释 + 1 个 `#include`。零实现、零线程。是合法的预留 TU | 通过 |
| 4 | `include/astro/phase2/coverage.h` 172 | **全文逐行** | ① `:142` 契约「shift 过大（`2*leaf_shift>=64`）」在 `leaf_shift` 极大时 `2*leaf_shift` 溢出 `int`，契约本身不可安全求值；② `:86` 语义源 `eng/contracts/data/clause_registry.json` **存在**（已核）；③ `:90` 指向 `coverage.cpp` 的 `RETIRED-OBJECT-REJECT` 注释块（coverage.cpp 属 ALG-coverage-003，未审）；④ `:5-6` 引 `docs/detail/algorithms_phase2/10_sampling.md` —— **该目录不存在**（悬空引用，见 §4-D3）；⑤ `:98-106` `P2_CELL_UNAVAILABLE` 明禁零填，契约表述正确 | 需修（悬空引用） |
| 5 | `include/astro/phase2/async_io.h` 116 | **全文逐行** | 纯容器 + 条件变量：成员仅 `:107-113` mutex/2×condvar/deque/标量，**无 thread 成员、无启动点**，include 面无 `<thread>`。`:27-32` `bounded_queue_capacity` 对 `item_bytes==0` 与 `memory_budget_bytes==0` 均返回 1（fail-safe）。`:61` 出错时 `pop()` 返回 `nullopt`——**错误与「已关闭且排空」共用 `nullopt`，调用方无法区分二者** | 通过（附建议） |
| 6 | `include/astro/phase2/block.h` 48 | **全文逐行** | ① `:31` `double safety_factor; // 默认 0.75` —— **无出处、无推导的经验值**，且是影响内存峰值估算的安全系数；② `:29` `int precision` 未声明取值域与非法值语义；③ `:41` 错误码仅 `0=ok,1=unfeasible` 两值，无稳定错误码族；④ `:5` 同引不存在的 `docs/detail/algorithms_phase2/10_sampling.md` | 须修 |
| 7 | `tests/weight_mode_retire_negative_test.cpp` 144 | **全文逐行** | 4 个 TEST 全属 `PsfswRetire`，测 `p2_weight_source_token_reject` 与 `route_phase2_weight_token`/`route_legacy_weight_mode_int`。`:130` 注释称「legacy 整数 weight_mode：纯拒绝面，**全值域拒绝**」，但 `:131` 只测 `{0,1,2,7}` —— 3/4/5/6/-1/99 全域未验。**与本片 `acr_kernels.cpp` 的 `wmode` 标量零关联** | 须修（门覆盖不足） |
| 8 | `tests/execution_options_test.cpp` 87 | **全文逐行** | ① `:11-14` `hw()` 是 `execution_options.h:23-26 default_cpu_workers()` 的**逐行复刻**，故 `:28` `EXPECT_EQ(e.cpu_workers, hw())` 与 `:34` `EXPECT_EQ(effective_cpu_workers(e), hw())` 是**自洽式断言**（同一表达式既当被检量又当期望量）；② `:29` 是 `:28` 的**字面重复**（`hw()` 自身已含 `h>0?h:1` 保护，`std::max(1,hw()) ≡ hw()`），零信息；③ `:30` `EXPECT_GE(e.io_workers,1)` 未钉住 README/合同声明的 `io = cpu/2` | 须修 |
| 9 | `tests/sampler_parallel_consistency_test.cpp` 462 | **全文逐行** | 本片**质量最高**的文件：`:249`/`:325`/`:444` 三处显式**非退化自证**；`:450-453` 期望值由几何独立推导（`n_union_cells × grid²`）而非复刻被测量；`:380` 明写「负例注入自证」。**未发现筛掉真信号**：`:253`/`:362` 遍历前已 `ASSERT_EQ(o1.size(), o2.size())`，无子集过滤。缺陷：`:113-116` `remove_all(dir, ec)` 静默吞错；`:53` 缺 `ASSERT_GT(cov.n_union_cells,0u)` 前置守卫（另两处有，`:216`/`:294`）；`:45` 探测 `t4_crop_v3` 但 `:44` 另用 `t4_full_v3_final`，后者存在性未探 | 通过（附建议） |
| 10 | `tools/linear_fit_oracle.py` 194 | **全文逐行** | ① `:138-139` **非退化检查只 `print("WARN")`、不置 `ok=False`**；而 `:114` 的 `no_outlier` 用例按设计「Siril 无拒绝」⇒ 该例 `agree == len(vals)` **恒真**，`:189` 仍可输出 `LINEAR_FIT_ORACLE=PASS`；② `:24` 默认 CLI 路径 `lib/phase2/build/rejection_cli.exe` —— **`lib/phase2` 不存在**（已 `ls` 核实），真实源在 `lib/algorithms/coverage/tools/rejection_cli.cpp`；③ `:28-29` `run/temp/p2_v4_evidence/` **不存在**（已核实）；④ `:10` 引 `evidence/oracle/linear_fit_provenance.json` —— **全仓无此文件**（已 `find` 核实）；⑤ `:55-57` `PROVENANCE["verify"]` 声称 `rej[0]=3 rej[1]=2 mean=0.476394 accepted=35`「已验证 harness」—— **全文无任何一处断言这些数字**；⑥ `:73-78`/`:92-97` 硬编码 `C:\msys64\mingw64\bin` + `.exe`，本环境（Linux）**完全不可运行** | 须修 |
| 11 | `tools/hips_compare.py` 452 | **全文逐行** | ① **`:246` 与 `:172-173` 自相矛盾**：逐像素逻辑把 `NaN==NaN` 判为一致并 `continue`，但聚合判决 `if d["nan_baseline"] or d["nan_candidate"]: ok = False` 只要**存在 NaN 就判 FAIL**，理由写「NaN domain drift」。本项目 HiPS 本就合法携带 NaN（`satellite_gate_build.py:117` 显式保留 NaN 结构）⇒ **该工具对任何含 NaN 的产品恒红，且掩盖真实差异**；② `:166-167` 两个 NaN 计数器只在 `:157` `if db != dc:` 分支内累加（`:154` 字节相同则跳过）⇒ **同一对目录能 PASS 或 FAIL 取决于是否走了快路径**；③ `:191` `if worst is None or ma > worst[1]`，而 `:180` 可把 `worst` 置为 `(rel, nan)`，`ma > nan` 恒 False ⇒ **worst 被 NaN 毒死，永不更新**；④ `:372-376` 硬编码 `ra_deg=266.4168, dec_deg=-29.0078` 无出处；⑤ `:318` manifest 硬写 `nside:65536, tile_width:512`。优点：`:390-435` `--perturb` 注入扰动且 `if pert_diff == 0: ok = False`，是**正确的非退化自证** | 须修 |
| 12 | `tools/satellite_gate_build.py` 216 | **全文逐行** | ① 见 §2 第 1 条（对照臂含注入）；② `:36` `TRAIL_WIDTH_DEG = 1.5 * (3.220769/3600.0)` —— `3.220769`（pixel scale，角秒/像素）**无出处、无配置、无注释来源**，属需标定却硬编码；③ `:9` 注释「≈17×背景 median」中的 17× **全文从未计算背景 median**，是散文式自证；④ `:125` 注释称「cos 因子并入阈值」，但 `:125-132` 全程用裸 (lon,lat) 度数算距离，**代码里没有任何 cos 修正**，高纬时 trail 宽度被高估 1/cos(lat)；⑤ `:34` 注释「order 7 → nside 65536」把 tile order 与 leaf order 混写（重推：leaf = 7+9=16，nside=2^16 ✓，**数值正确、表述误导**） | 须修 |
| 13 | `configs/stage2_real_3frame.json` 43 | **全文逐行** | 写 `"version":1`，而解析器**从不读 version**（见文件 1）。`underdetermined_n:2` + `method:"sigma"` + 3 输入，显式合法。输入指向 `run/temp/phase1_freeze/`（临时区，非归档产物） | 须修（依赖文件 1） |
| 14 | `configs/stage2_t4_true_overlap.json` 43 | **全文逐行** | 同上。`:35` `"acr_route":"cpu"` 在 `p2_acr_block_eligible` 恒 false 下是**无效开关**（配置暗示 ACR 可路由，实际不可达） | 须修（误导） |
| 15 | `configs/stage2_overlap.example.json` 40 | **全文逐行** | 同上。`method:"winsorized_sigma"` + `normalization` 未设 ⇒ 走默认 `acsd_median_center_v1`，满足 `:358-362` 的组合约束，合规。缺 `sigma_floor`/`support_power`（取默认值，非缺陷） | 通过 |
| 16 | `README.md` 236 | **全文逐行** | **行锚全线漂移**（本人实测）：① `:10-11` 称 `coverage.h` 共 **59 行**，实际 **172**；② `:93` `p2_coverage_build` 在 `coverage.h:52`，实际 **:59**；③ `:94` `p2_coverage_free` 在 `:56`，实际 **:63**；④ `:98` `P2MocCell` 在 `:26-29`，实际 **:28-31**；⑤ `:98` `P2HipsInputInfo` 在 `:31-38`，实际 **:33-45**；⑥ `:99` `P2CoverageResult` 在 `:40-48`，实际 **:47-55**。该节标题自称「**AST/行号实测，非手抄**」—— **精度声明为假**。⑦ `:145-147` 称 `RealHipsUnion` 在 `synthetic_gate.cpp:3374`、`FilterMismatchRejected` 在 `:3410`、GTEST_SKIP 在 `:3376/:3413`，实测分别在 **:3731 / :3768 / :3735 / :3772**（漂移 ~357 行，该位置现在是 ACR 区）。⑧ §13「原 42 行全文保留，**事实未变**」—— **事实已变**：`:203` `cd lib\phase2\build`、`:208` `lib\astro_image_io\...`、`:232` `lib/include/astro/phase2/`、`:235` `eng/tools/stage2.cpp`、`:236` `eng/tests/synthetic_gate.cpp`、`:218` `run/phase2/stage2_t4_overlap.json` —— **6 条路径实测全部不存在**。⑨ `:157`/`:227` 称合成门「**18/18**」，实测 `synthetic_gate.cpp` 有 **104 个 TEST**。⑩ `:89` `docs/detail/phase2.md` **不存在** | 须修 |
| 17 | `tests/synthetic_gate.cpp` 6078 | **部分精读**（3259–3704）+ 全文件结构普查 | 见下 | 需修 |
| 18 | `memory.md` 302 | 未逐行读 | — | 未覆盖 |
| 19 | `hips_p2/README.md` 185 | 未逐行读 | — | 未覆盖 |
| 20 | `hips_p2/memory.md` 96 | 未逐行读 | — | 未覆盖 |
| 21 | `CMakeLists.txt` 252 | 未逐行读 | 子代理取证 `:28 option(P2_ENABLE_OPENMP … OFF)`，本人复核 | 未覆盖（关键行已核） |
| 22 | `module.yaml` 93 | 未逐行读 | — | 未覆盖 |

### 3.1 `synthetic_gate.cpp` 已读区段细目

| 行号 | 读到什么 | 判定 |
|---|---|---|
| `:3259-3352` `G5WeightTruthGate` | **零生产符号调用**（本人实测 grep `p2_\|upm\|integrate\.` 命中 0）。7 个权重 lambda（`:3288-3299`）+ 本地积分器（`:3274-3287`），断言 `:3323-3325`/`:3342`/`:3345` 全是自比较。`:3335` 手工减 1.5 假装 UPM 校准完美 | **须修**（§2 第 2 条） |
| `:3355-3391` `LegacyLauncherEquivalent` | **正面样本**：`:3382-3387` 显式非退化自检（`ASSERT_EQ(scalars.bytes.size(), reg->args.scalar_bytes)` + 三个 `read_scalar(...).has_value()`） | 通过 |
| `:3395-3463` `LegacyCpuOneVsTwoTDetermine` | **正面样本**：`:3434-3441` 判别力前置自检；`:3442-3446` 在 `!P2_ENABLE_OPENMP` 时 `GTEST_SKIP` 并明写「本用例未行使」——**主动承认默认构建下本用例不构成证据** | 通过 |
| `:3466-3510` `CudaEquivalent` | `:3492-3499` 只 append **8** 个标量 = 48 字节，注释却标为「CON-007 workers」；注册声明 `scalar_bytes=60`。⇒ `p0`(读 8B@44，52>48)→`nullopt`、`wmode`(@52)→`nullopt` **绕过 wmode 门**、`workers`(@56)→`nullopt`→缺省 1 静默串行化。**且无 `:3382` 那样的非退化自检** | **须修** |
| `:3513-3587` `CudaWeightedSupportEquivalent` | 同 `:3559-3566`（48 字节）。另 `:3582-3585` 的期望值建立在「等权」之上，而等权正是被 retire 的口径 | 须修 |
| `:3591-3645` `G9CompactFrameSubset` | 同 `:3626-3633`（48 字节） | 须修 |
| `:3648-3687` `G9WinsorizedCpuRoute` | 同 `:3670-3677`（48 字节）。`:3678-3685` 用 try/catch 断言 CUDA 对 Winsorized 抛 CPU_ROUTE —— **catch 只捕获 `runtime_error` 且只认一个子串**，若抛别的异常类型会逃逸 | 须修 |
| `:3691-3704` `NanInputRejected` | NaN 输入被拒（`acc[1]==0`, `status==3`）——负例有效 | 通过 |

**为何 48 字节未炸**：这些用例直接调 `reg->legacy_parallel(inv, nullptr)` 函数指针，**绕过 `validate_invocation`**（其唯一调用点在 executor / cuda_bridge_loader / cuda_executor）。⇒ 布局契约在测试路径上**无机器强制**。

---

## 4. 发现清单

### 阻断
无。（生产科学路径未发现会直接产出错误数值的缺陷；且 ACR 块在生产不可达，见 D5。）

### 须修

| ID | 位置 | 问题 |
|---|---|---|
| **S1** | `satellite_gate_build.py:133-149` | 干净对照帧含注入卫星线 ⇒ 对照臂失效；若 trail 未注入则门为空门。无有效状态 |
| **S2** | `synthetic_gate.cpp:3259-3352` | `G5WeightTruthGate` 零生产调用，纯自洽式断言；`:3335` 手工假定 UPM 校准完美 |
| **S3** | `README.md:51-52` vs `stage2_common.cpp:545-546` | 冻结权重式三因子，生产硬编码两因子；`:546 control_reliability=1.0` 属「可由几何导出却硬编码」 |
| **S4** | `linear_fit_oracle.py:138-139` | 非退化检查只 WARN 不 fail；`no_outlier` 例恒真而仍输出 PASS |
| **S5** | `linear_fit_oracle.py:24,28-29,10` | 3 条悬空路径（`lib/phase2` 不存在、`p2_v4_evidence` 不存在、`linear_fit_provenance.json` 不存在） |
| **S6** | `hips_compare.py:246` vs `:172-173` | 判决「存在 NaN 即 FAIL」与逐像素「NaN==NaN 一致」自相矛盾 ⇒ 含 NaN 产品恒红；`:166-167` 计数只在慢路径累加，结果依赖是否走快路径 |
| **S7** | `synthetic_gate.cpp:3492-3499/3559-3566/3626-3633/3670-3677` | 4 处 48 字节标量 vs 注册 60 字节；`p0/wmode` 越界→`nullopt` 绕过门；绕过 `validate_invocation` 故无强制 |
| **S8** | `stage2_common.cpp:7-512` | `version` 键零读取 + 未知键零拒绝 ⇒ 拼写错误静默降级为默认配置 |
| **S9** | `README.md:10-11,93-99,145-147,157,227,203,208,232,235,236,218,89` | 行锚全线漂移 + 6 条路径不存在 + 「事实未变」为假 + 「18/18」实为 104 TEST + 「AST 实测非手抄」精度声明为假 |
| **S10** | `execution_options_test.cpp:11-14,28,29,34` | `hw()` 是 `default_cpu_workers()` 的逐行复刻 ⇒ `:28`/`:34` 自洽式断言；`:29` 为 `:28` 字面重复 |
| **S11** | `weight_mode_retire_negative_test.cpp:130-137` | 注释称「全值域拒绝」但只测 `{0,1,2,7}`；对 kernel `wmode` 标量零覆盖（子代理 grep 证实全仓无一处断言 `acr_kernels.cpp:141` 那条 throw） |
| **S12** | `satellite_gate_build.py:36` | `3.220779`→`3.220769`（角秒/像素）无出处，属需标定却硬编码 |
| **S13** | `coverage.h:5-6` + `block.h:5` | 引用 `docs/detail/algorithms_phase2/10_sampling.md`，**该目录不存在**（两处悬空） |

### 建议

| ID | 位置 | 问题 |
|---|---|---|
| R1 | `acr_kernels.cpp:160-166` | `n_valid==0` 静默写 0.0f，无错误码；与本文件 throw 风格不一致。下游无法区分「无贡献者」与「测得 0」 |
| R2 | `acr_kernels.cpp:199-200` | integrate 非 OK 静默写 0.0f |
| R3 | `acr_kernels.cpp:141` | `if (wmode && *wmode == 2)` 是相等判定非值域判定；`wmode &&` 连「槽缺失」也放行 |
| R4 | `block.h:31` | `safety_factor = 0.75` 无出处，且是内存峰值估算的安全系数 |
| R5 | `coverage.h:142` | 契约 `2*leaf_shift>=64` 在 `leaf_shift` 极大时溢出 `int`，契约本身不可安全求值 |
| R6 | `async_io.h:61-62` | `pop()` 用同一个 `nullopt` 表示「出错」与「已关闭且排空」，调用方无法区分 |
| R7 | `hips_compare.py:180,191` | `worst` 被 `(rel, nan)` 毒死后永不更新（`ma > nan` 恒 False） |
| R8 | `hips_compare.py:372-376` | 硬编码 `ra_deg/dec_deg` 无出处，应入配置 |
| R9 | `satellite_gate_build.py:125-132` | 注释称「cos 因子并入阈值」但代码无任何 cos 修正 |
| R10 | `satellite_gate_build.py:9` | 「≈17×背景 median」从未计算，纯散文自证 |
| R11 | `sampler_parallel_consistency_test.cpp:113-116` | `remove_all(dir, ec)` 静默吞错 |
| R12 | `synthetic_gate.cpp:3678-3685` | catch 只认 `runtime_error` + 单子串，其它异常类型逃逸 |

### 已登记但**不判为违规**（红队自我订正）

| ID | 位置 | 裁定 |
|---|---|---|
| N1 | `acr_kernels.cpp:218` `#pragma omp parallel num_threads(cpu_workers)` | **不构成「私建线程池」**。`CONCURRENCY_STANDARD.md:32` 正面许可 OpenMP 显式并行区；`:34` 禁的是「硬编码 worker 数」与「**长期**线程池」，本处 worker 数由 invocation 标量注入（`:76-79`，CON-007）。且 `P2_ENABLE_OPENMP` 默认 OFF（`CMakeLists.txt:28`），该分支默认构建**不参与编译**、生产不可达。**我原先的假设被两个独立子代理以规范原文推翻，予以撤回。** |

---

## 5. 我主动构造的反例

### 反例 1：注入权重缺陷，`G5WeightTruthGate` 会转红吗？
- **构造**：把 `upm.cpp` / `integrate.cpp` 的权重实现整体改成常数 1.0（等价于全部等权，且是最严重的科学错误）。
- **期望推翻**：`G5WeightTruthGate`（`:3259`）应转红。
- **结果**：❌ **未能推翻**。该用例对 `p2_*`/`upm`/`integrate.` 的符号引用实测为 0，权重函数全是本文件 lambda ⇒ **注入缺陷后本门仍绿**。这是本片最硬的自洽式断言证据。

### 反例 2：`no_outlier` 例能否让 `linear_fit_oracle.py` 判红？
- **构造**：让 Siril oracle 与生产对 `no_outlier` 产生分歧。
- **期望推翻**：`LINEAR_FIT_ORACLE=FAIL`。
- **结果**：✅ 能推翻——`:152` `if agree != len(vals): ok = False` 是真门。
- **但**：若缺陷是「两端都不过滤」而非「两端分歧」，该例 `agree==len` 恒成立 ⇒ `:138-139` 的 WARN 是**唯一预警，而它不参与判决**。故 **反例只在分歧型缺陷下成立，非分歧型缺陷下失效**。

### 反例 3：`execution_options_test.cpp:81,83` 的 `EXPECT_EQ(..., 3)` 是否绑定机器核数？
- **构造**：假设这是「恒红门」，即只在 6 核机器上通过。
- **期望推翻**：改成显式 `cpu_workers=6` 后 `effective_io_workers` 应与硬件无关。
- **结果**：✅ **我推翻了自己的假设**。重推：`effective_io_workers`（`execution_options.h:32-36`）在 `io_workers==0` 时取 `effective_cpu_workers(e)`，而 `:79` 已把 `cpu_workers` **显式**设为 6 ⇒ 结果恒为 3，与 `hardware_concurrency` 无关。**该处不是恒红门。**（真正的问题是它不是自洽式而是**同义重复**，见 S10。）

### 反例 4：`worst` 报告能否自愈？
- **构造**：让某 tile 的 `worst` 先被置为含 NaN 的元组（`:180`），再看后续更大 `ma` 能否替换它。
- **期望推翻**：能替换。
- **结果**：❌ **未能推翻**。IEEE 下 `ma > float("nan")` 恒 False ⇒ `worst` 永久冻结在第一个 NaN tile。

### 反例 5：卫星线门的对照臂是否真的干净？
- **构造**：读 `:133-149`，检查 `clean_sig` 是否在 trail 注入**之前**快照。
- **期望推翻**：能推翻。
- **结果**：❌ **未能推翻**。`out[trail] += TRAIL_AMP`（`:137`）→ 写盘（`:143-147`）→ `clean_sig[ip] = out.copy()`（`:149`），快照取在注入之后。**对照臂与注入臂逐字节相同。**

---

## 6. 盲复算（遮住既有判定独立取证）

遮住一切既有审稿结论，只依据 `HEAD=850a9ede` 原文重新取证，结论与本片既有/我先前判定比对：

| 项 | 盲复算独立结论 | 与本片判定 | 判 |
|---|---|---|---|
| `G5WeightTruthGate` 是否调生产码 | 符号引用数 = 0 | 同（须修） | **一致** |
| `satellite_gate_build.py` 对照臂是否干净 | 快照在注入后 → 不干净 | 同（须修） | **一致** |
| `linear_fit_oracle.py` 非退化检查是否参与判决 | 只 `print`，不置 `ok` | 同（须修） | **一致** |
| `README.md` 行锚 | 5 处 coverage.h 锚全漂、2 处 synthetic_gate 锚漂 ~357 行、6 条路径不存在 | 同（须修） | **一致** |
| `acr_kernels.cpp:218` 是否私建线程池 | OpenMP 并行区，规范正面许可 | 同（**不判违规**） | **一致** |
| `execution_options_test.cpp:81` 是否恒红 | 显式 `cpu_workers=6` ⇒ 与核数无关 | 同（非恒红） | **一致** |
| `parse_config` 是否校验 `version` | `grep -c version` = 0 | 同（须修） | **一致** |

**结论：判「一致」，未见偏松或偏严。** 唯一一次偏严（把 `acr_kernels.cpp:218` 判为私建线程池）已由子代理以规范原文推翻并主动撤回，记入 §4「已登记但不判违规」。

---

## 7. 子代理派发记录

**派发 7 个 / 5 个独立分片**（工具在单次调用中重复投递了 2 个同题代理，二者作为**独立复本**保留，用于交叉验证；有效分片数 = 5，符合 3–5 口径）。

| # | 分片 | 结论 | 本人复核结果 |
|---|---|---|---|
| 1 | 线程池合规（`acr_kernels.cpp` / `BoundedAsyncQueue` / 「9 处」数字） | **不构成违规**；`P2_ENABLE_OPENMP` 默认 OFF；「9 处」不含 `:218` | ✅ **采纳**。本人已据规范原文撤回原假设 |
| 1′ | 同题独立复本 | 结论与 #1 一致，额外给出「生产 `std::thread` 创建点约 25 处/14 文件」 | ✅ 一致，互证 |
| 2 | 退役 `weight_mode` 域（`stage2_common.cpp:435-470` vs `acr_kernels.cpp:141`） | 配置面 fail-closed 成立；kernel 面「只拒 2」成立；**该负向测试对 kernel 零覆盖**；`p2_acr_block_eligible` **有生产调用者**非死代码；`weights==nullptr` 静默降级成立 | ✅ 全盘采纳，并据此修正我原「p2_acr_block_eligible 是死代码」的表述 |
| 2′ | 同题独立复本 | 同上，补充「`wmode &&` 连槽缺失也放行」 | ✅ 一致，互证 |
| 3 | 标量 ABI 布局（`scalar_bytes` / `read_scalar` / fp32-fp64） | **未对齐不成立**（`read_scalar` 用 `memcpy`）；**行锚 `kernel_registry.cpp:21` 不悬空**；**fp32/fp64 声明矛盾成立**；并独立发现 4 处 48/56 字节夹具 | ✅ 采纳。本人已独立逐行复核 `synthetic_gate.cpp:3492-3499` 等 4 处，确认 8 槽 48 字节 |
| 4 | 门与预言机自洽式审计 | 未返回（见 §7.1） | ⏳ |
| 5 | 悬空引用普查 | 未返回（见 §7.1） | ⏳ |

### 7.1 否决/未采纳的子代理意见

| 提案来源 | 提案 | 处置 |
|---|---|---|
| 子代理 2 | 「`weight_mode_retire_negative_test.cpp` 只测 `wmode==2`」（我给假设中的措辞） | **否决并订正**：该文件**完全不碰** ACR kernel，grep `mosaic_reject\|acr\|wmode` 零命中。准确表述是「kernel 的 `wmode` 门在全仓零测试，连 `==2` 自身也无测试」 |
| 子代理 1/1′ | 「pragma 由 `rejection.h` 引入」（我给任务描述中的前提） | **否决**：rejection.h 内 `omp` 命中全是 `compact` 子串误报，pragma 是 `acr_kernels.cpp:218` 字面写死 |
| 我本人原假设 | 「`execution_options_test.cpp:81` 是绑定机器核数的恒红门」 | **自我否决**：见 §5 反例 3 |
| 我本人原假设 | 「`acr_kernels.cpp:218` 私建线程池」 | **自我否决**：见 §4 N1 |
| 子代理 3 | 「`stage2.cpp:1249` fallback CPU 重调同一函数致二次 throw」 | **记入但不定级**：`stage2.cpp` 属 ALG-coverage-002，非本片，仅登记为跨片线索 |

### 7.2 派发未完成说明（不得隐去）
分片 4（门与预言机自洽式审计）与分片 5（悬空引用普查）在本次交付窗口内**未返回结果**。本人对 §2/§3/§4 中相应类别的判定**均系亲自读原文与亲自跑命令得出**，不依赖这两个代理；但**门禁自洽式的普查不完整**（仅覆盖已读区段），**悬空引用普查亦不完整**（仅覆盖 §3/§4 已列条目）。这两类须在补派后复核。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
git -c core.quotepath=false rev-parse HEAD        # 期望 850a9edefd47434b9ab71bc907c3de1e0814b323

# --- 分母核对（22 份 / 10184 行）---
wc -l lib/algorithms/coverage/tests/synthetic_gate.cpp \
      lib/algorithms/coverage/src/stage2_common.cpp \
      lib/algorithms/coverage/tests/sampler_parallel_consistency_test.cpp \
      lib/algorithms/coverage/tools/hips_compare.py \
      lib/algorithms/coverage/src/acr_kernels.cpp \
      lib/algorithms/coverage/memory.md \
      lib/algorithms/coverage/CMakeLists.txt \
      lib/algorithms/coverage/README.md \
      lib/algorithms/coverage/tools/satellite_gate_build.py \
      lib/algorithms/coverage/tools/linear_fit_oracle.py \
      lib/algorithms/coverage/hips_p2/README.md \
      lib/algorithms/coverage/include/astro/phase2/coverage.h \
      lib/algorithms/coverage/tests/weight_mode_retire_negative_test.cpp \
      lib/algorithms/coverage/include/astro/phase2/async_io.h \
      lib/algorithms/coverage/hips_p2/memory.md \
      lib/algorithms/coverage/module.yaml \
      lib/algorithms/coverage/tests/execution_options_test.cpp \
      lib/algorithms/coverage/include/astro/phase2/block.h \
      lib/algorithms/coverage/configs/stage2_real_3frame.json \
      lib/algorithms/coverage/configs/stage2_t4_true_overlap.json \
      lib/algorithms/coverage/configs/stage2_overlap.example.json \
      lib/algorithms/coverage/src/async_io.cpp | tail -1   # 期望 10184

# --- S2：G5WeightTruthGate 零生产调用（期望：无输出）---
sed -n '3259,3352p' lib/algorithms/coverage/tests/synthetic_gate.cpp \
  | grep -nE "p2_[a-z_]*|upm|integrate\.|Upm"

# --- S1：卫星线对照臂快照在注入之后 ---
sed -n '133,149p' lib/algorithms/coverage/tools/satellite_gate_build.py

# --- S4：非退化检查只 WARN ---
sed -n '136,140p;186,190p' lib/algorithms/coverage/tools/linear_fit_oracle.py

# --- S5：3 条悬空路径 ---
ls -d lib/phase2; ls -d run/temp/p2_v4_evidence
find . -name 'linear_fit_provenance.json' -not -path './.git/*'    # 期望无输出

# --- S6：NaN 判决与逐像素判定自相矛盾 ---
sed -n '170,180p;243,249p' lib/algorithms/coverage/tools/hips_compare.py

# --- S7：4 处 48 字节标量 vs 注册 60 字节 ---
sed -n '348,358p' lib/algorithms/coverage/src/acr_kernels.cpp
sed -n '3492,3499p;3559,3566p;3626,3633p;3670,3677p' \
  lib/algorithms/coverage/tests/synthetic_gate.cpp
grep -n "read_scalar<int>(inv.scalars" \
  lib/algorithms/coverage/include/astro/compute/kernel_registry.hpp 2>/dev/null || \
grep -rn "template<class T>" -A6 lib/infrastructure/acr/include/astro/compute/kernel_registry.hpp | head -12

# --- S8：version 零读取 + 未知键零拒绝 ---
sed -n '7,512p' lib/algorithms/coverage/src/stage2_common.cpp | grep -c version   # 期望 0
sed -n '7,512p' lib/algorithms/coverage/src/stage2_common.cpp \
  | grep -nE "unknown|unexpected|未识别|未知" || echo "(无未知键处理)"

# --- S9：README 行锚漂移与路径悬空 ---
grep -n "coverage.h（59 行）\|coverage.h:52\|coverage.h:56\|synthetic_gate.cpp:$" \
  lib/algorithms/coverage/README.md
grep -n "TEST(Phase2Coverage, RealHipsUnion)\|TEST(Phase2Coverage, FilterMismatchRejected)" \
  lib/algorithms/coverage/tests/synthetic_gate.cpp          # 期望 3731 / 3768
for p in lib/phase2 lib/astro_image_io lib/include/astro/phase2 \
         eng/tools/stage2.cpp eng/tests/synthetic_gate.cpp \
         run/phase2/stage2_t4_overlap.json docs/detail/phase2.md \
         docs/detail/algorithms_phase2; do
  [ -e "$p" ] && echo "存在: $p" || echo "缺失: $p"; done
grep -c "^TEST" lib/algorithms/coverage/tests/synthetic_gate.cpp   # 期望 104（README 称 18）

# --- S3：权重式两因子硬编码 ---
sed -n '51,52p' lib/algorithms/coverage/README.md
sed -n '544,547p' lib/algorithms/coverage/src/stage2_common.cpp
grep -n "quality_mode\|control_reliability" lib/algorithms/coverage/src/stage2_common.cpp

# --- S10：hw() 是 default_cpu_workers() 的复刻 ---
sed -n '11,14p;28,29p;34p' lib/algorithms/coverage/tests/execution_options_test.cpp
sed -n '23,26p;32,36p' lib/algorithms/coverage/include/astro/phase2/execution_options.h

# --- N1：OpenMP 合规性（本人原假设被推翻的依据）---
sed -n '28p' lib/algorithms/coverage/CMakeLists.txt
sed -n '32,34p' docs/engineering/CONCURRENCY_STANDARD.md
grep -n "std::thread" lib/algorithms/coverage/src/acr_kernels.cpp || echo "(零命中)"
```

---

## 9. 未决事项（UNRESOLVED）

| ID | 事项 | 所需输入 |
|---|---|---|
| U1 | `synthetic_gate.cpp` 未读区段（1–3258、3705–6078，共 5618 行）可能含更多自洽式断言 | 续派一遍完整重读 |
| U2 | 分片子代理 4/5 未返回；门禁自洽式与悬空引用的**全片普查**不完整 | 补派并复核 |
| U3 | `memory.md` / `hips_p2/*.md` / `module.yaml` 未逐行读（676 行），退役声明与「零消费者」类声明**未验证** | 本项目已实测存在「自称零消费者实有调用者」的先例，此项须补 |
| U4 | `satellite_gate_build.py` 的 `TRAIL_AMP=0.05`「≈17×背景 median」中背景 median 真实值未知 | 需实跑（留前台） |
| U5 | `acr_kernels.cpp:362` `numeric.compute=fp64` 与全库 `fp32/fp64` 约定的冲突，定级依赖 `NumericPolicy::compute` 字段是否有权威定义 | 需补 `task_traits.hpp` 字段级规范 |