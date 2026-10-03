# 审稿-P1 · ALG-noise_snr-002（G08-05 对抗审稿 第 1 遍）

- **仓库**：`/workspace/Astro CS Database`
- **HEAD**：`850a9edefd47434b9ab71bc907c3de1e0814b323`
- **片号**：`ALG-noise_snr-002`　层：`lib/algorithms/noise_snr`
- **性质**：生产源码片 + 其可复用验证面
- **纪律遵守**：零 git 写（未 add/commit/checkout/reset/stash）；未编译、未跑 ctest/pytest/构建/任何仓内二进制；未修改任何仓内文件；未读 `/tmp/acsd_g08/`。中文路径一律 `git -c core.quotepath=false`。

---

## 1. 读完了吗

**计数口径**：分母取 `分片清单-权威版.yaml` 的「成员份数 / 实际行数」；分子取「本人用 read 工具逐行读完的行数」，行数用 `wc -l` 逐文件实测并与清单交叉核对。**读满 = 从第 1 行读到文件末行（read 工具报 "End of file"），不以抽样代替。**

| 项 | 数值 |
|---|---|
| 成员份数（清单） | **20** |
| 实际读了 | **20** |
| 成员总行数（清单「实际行数」） | **6581** |
| 实测行数（`wc -l` 逐文件求和） | **6581**（与清单一致，差 0） |
| 实际读完行数 | **6581** |
| **覆盖率** | **20/20 份 = 100%；6581/6581 行 = 100%** |

### 1.1 逐份完成情况（含实读行数）

| # | 文件 | 行 | 读完 | 读了什么 |
|---|---|---|---|---|
| 1 | `cpp/src/noise_model.cpp` | 1482 | ✅ | 全读。生产实现主体（build/fill/free/scale_law/gain/registry/§5d 拟合） |
| 2 | `tests/p1noise/p1noise_tests_core.cpp` | 1603 | ✅ | 全读。8 个测试组全部断言逐条过目 |
| 3 | `tests/p1noise/noise_model_numpy_oracle.py` | 610 | ✅ | 全读。NumPy 独立 oracle 驱动 + 比较器 + 用例矩阵 |
| 4 | `tests/p1noise/p1noise_oracle.hpp` | 505 | ✅ | 全读。C++ oracle（统计/平面/fill/gain/scale_law/bit_eq） |
| 5 | `tests/p1noise/p1noise_fixtures.hpp` | 408 | ✅ | 全读。FIX-NOISE-A..H/PERF 夹具生成器 |
| 6 | `test/test_snr_estimator.py` | 305 | ✅ | 全读。ctypes 侧 SNR 估算测试（非生产工具） |
| 7 | `tests/p1noise/p1noise_tests_selfcheck.cpp` | 265 | ✅ | 全读。故障注入自检 5 阶段 + guard-path |
| 8 | `README.md` | 248 | ✅ | 全读。模块合同 README |
| 9 | `tests/p1noise/CMakeLists.txt` | 191 | ✅ | 全读。CTest 注册与门清单 |
| 10 | `tests/p1noise/p1noise_test_main.hpp` | 147 | ✅ | 全读。测试执行器 + 故障注入宏 |
| 11 | `tests/p1noise/p1noise_abi_layout_check.py` | 141 | ✅ | 全读。ABI 布局锁门 + 突变自检 |
| 12 | `include/acsd/noise/variance_plane_policy.h` | 139 | ✅ | 全读。消费侧方差面策略门 |
| 13 | `include/acsd/information_weight.h` | 127 | ✅ | 全读。W_info/Q/F_hat 契约头（纯声明） |
| 14 | `cpp/test/snr_reconcile_test.cpp` | 117 | ✅ | 全读。FP32/FP64 对账（非生产工具） |
| 15 | `CMakeLists.txt` | 97 | ✅ | 全读。`acsd_p1_noise` SHARED 子图 |
| 16 | `tests/p1noise/p1noise_abi_layout_probe.cpp` | 77 | ✅ | 全读。布局探针（static_assert + JSON） |
| 17 | `include/acsd/noise/saturation_policy.h` | 70 | ✅ | 全读。饱和电平解析策略 |
| 18 | `.gitignore` | 21 | ✅ | 全读。产物忽略清单 |
| 19 | `cpp/Makefile` | 16 | ✅ | 全读。`snr_estimator.dll` 构建 |
| 20 | `tests/p1noise/p1noise_tests_main.cpp` | 12 | ✅ | 全读。main 转发 |

**未读完的：无。** 未列入本片的邻近文件（如 `p1noise_tests_perf.cpp`、`p1noise_saturation_test.cpp`、`p1snr_science_test.cpp`、根 `CMakeLists.txt`、`module_entry.cpp`、`module_adapters.cpp`、`snr_estimator.h`）属其他片，**本片结论不对其作独立判定**；仅在支撑本片结论时作为旁证引用，并已注明出处。

---

## 2. 本片判定

# 判定：**阻断**

不是因为「实现整体不可信」——恰恰相反，`noise_model.cpp` 的数值内核（QR 而非正规方程、割平面 + 活跃集枚举、§5d 自校准 R 判据、mask 的预算收缩）**工程质量相当高**，多处 fail-closed 做得比我预期的好。阻断的理由是三条**证据链本身失效**的缺陷：模块用来证明自己正确的东西，有三处**按构造不可能失败**，还有一处**连 NaN 都看不见**。

### 最重 3 条

**① `hull_nonpositive_frac` 审计门是恒真门：「从未计算」与「算过且干净」在产品面逐位相同。**
生产者 `noise_model.cpp:797` 先 memset 整结构 ⇒ 该字段默认 `0.0`；全仓**唯一**写入点 `:1152` 被三层 `if` 嵌套守护（`:1126 fit_ok` → `:1141 hull.size()>=3` → `:1148 area_hull>0 && isfinite`）；消费门 `variance_plane_policy.h:88-90` 判 `== 0.0` 即「可审计」。**只要审计没跑，字段就是 memset 的 0.0，门就是绿的。** 消费侧 `module_adapters.cpp:8252-8270` 直读原始字段、**无 `has_spatial_field` 守卫**，因此 `n<4`、控制点近共线、`enable_spatial_field=0`、两次拟合都失败这些**未被子检查兜住**的路径，会挂出一张「auditable=true / hull_nonpositive_frac=0.0」而**从未做过凸包审计**的方差面。仓内自己的产物已承认这一点：`run/CHAIN-WIRE-ADAPT-01/REPORT.md:239`「**W1 的红侧是注入出来的，不是真实数据产生的**」。测试 `p1noise_tests_core.cpp:716/1516/1561` 三处断言**对两种情形完全同真**，无法区分。

**② `snr_noise_scale_law` 是 `void`，对退化 α 静默产出「自相矛盾 / NaN」对，而且测试把该缺陷固化为期望值。**
`noise_model.cpp:1463-1470`：α=0 ⇒ variance=0.0 而 ivar 因 `a2>0.0` 为假**原封不动** ⇒ `(0.0, 旧 ivar)`；α=NaN ⇒ `(NaN, 旧 ivar)`。同一文件 `:1372`（禁写 NaN）与 `:1373-1375`（两态穷尽：variance==0 ⇒ ivar==0）**明文禁止**这两个结果。更严重的是 `p1noise_tests_core.cpp:1305`（`is_nan_bits(var)` 断言 NaN **确实**被写入）、`:1309-1310`（断言 `(0.0, 0.04)` 逐位成立）——**缺陷被回归锁死，修生产代码会立刻把本套件打红。** 姊妹 oracle `noise_model_numpy_oracle.py` 只跑 α=2.0（`:162-164`、`:595-597`），退化域**零覆盖**。

**③ NumPy oracle 的比较器 `close()` 对 NaN 完全失明 —— 模块最重要的一道独立验证对 NaN 恒绿。**
`noise_model_numpy_oracle.py:184-186`：`rel = |got-exp|/denom`，判据是 `np.sum(rel > rtol)`。NaN 参与运算得 `rel = NaN`，而 `NaN > rtol` 为 **False** ⇒ NaN **不计入 bad**。**我已在本机独立跑通复现**（见 §5 CE-3）。后果：所有经 `close()` 的断言——控制点 x/y/variance/ivar（`:416-419`）、全局方差/ivar/σ（`:421-423`）、**整个 fill 逐像素 variance 面与 ivar 面（`:436-439`）**、mask 半径（`:563-566`）、gain/scale_law（`:592-597`）——只要生产吐出 NaN 就**静默判绿**。而模块合同恰恰规定 NaN 只能表示产品损坏（`noise_model.cpp:1372`），且 ② 证明了 `scale_law` 真能吐 NaN。**这是本片唯一一处「active 无条件恒绿」。**

---

## 3. 逐文件清单

> 每条给 `文件:行`。判定栏：**阻断** / **须修** / **建议** / **无问题（已正面核实）**

### 3.1 `cpp/src/noise_model.cpp`（1482 行）— 判定：**阻断**

- **读了什么**：全读。`noise_model_impl`（`:783-1168`）、`fill_impl`（`:1323-1408`）、§5d 自校准 R 判据与受约束平面拟合（`:283-627`）、MASK-002 逐星掩膜与天空预算（`:629-932`）、指针键注册表与互斥量（`:31-90`）、7 个 C ABI 导出（`:1172-1482`）。
- **看到什么**：
  - **B-1** `:797` memset 早于 `:805-807` 与 `:1126-1155`；`:1152` 唯一写入点被三层 `if` 守护；`:1149-1152` 的 `frac` 计算带 `if (!(frac>0.0)) frac=0.0;` 与 `if (frac>1.0) frac=1.0;` 两处静默改写。
  - **B-2** `:1463-1470` `void snr_noise_scale_law`，α=0/NaN 产出 `(0, 旧ivar)`/`(NaN, 旧ivar)`，直接违反同文件 `:1372-1379` 的两态与禁 NaN 不变式。
  - **须修-3** `:1102` 写 `ctrl_sigma[i] = patch_sigma[i]`（**未钳位**），`:1103` 写 `ctrl_variance[i] = max(patch_var[i], floor)`（**已钳位**）；floor 生效时 `ctrl_variance[i] ≠ ctrl_sigma[i]²`，且 `ctrl_ivar[i] = 1/ctrl_variance[i] ≠ 1/ctrl_sigma[i]²`。
  - **须修-4** `:529-530` 死代码：`null_space_basis(rows,3,Z)` 在 `:404 return 0`，而 `:527` 先 `if (k>0 && q==0) return false;` ⇒ `:528` 的「k=3 取 β=0」分支**永不可达**；`:369` 与 `:528` 两处注释均与实际行为相反。附带：`:603-608` 的三重循环因每次 `consider(idx,3)` 都在 `:527` 早退，**是每轮割平面 O(m³) 的确定空转**。
  - **须修-5** `:1386` `var = (pred < floor) ? floor : pred` 是**真实逐像素钳位**，但 `registry_add_clamp` 只在 build 侧调用（`:1063/1070/1079/1166`），fill 侧**零计数**；`:55-58` `registry_bind_floor` 显式不触碰 `g_model_floor_clamp`，故 bind 之后计数相对当前 floor **已陈旧**。
  - **须修-6** `:790` 的 `return 3` 与 `:1301/:1312` 的 `SNR_ABI_MISMATCH` 都发生在 `:797` memset **之前** ⇒ 失败返回时 `*out_model` 保持调用方原样（未初始化）。
  - **须修-7** `:805-807` 对 `variance_floor` 显式 fail-closed，而 `:826-830` 对 `mask_k_sigma`/`mask_r_min_px`/`mask_fwhm_floor_scale`/`mask_budget_min_patches`/`mask_budget_min_sky` **五个参数静默替换为 0.1/1.5/0.75/8/9216**，无错误码、无诊断位（全文件 `fprintf` 命中数 = 0）。
  - **须修-8** `:1475` `if (gain_e_per_adu <= 0.0) return 0.0;`——非法增益静默产出**零方差**（＝「完美探测器」），且 `NaN` 增益因 `NaN<=0.0` 为假而穿透为 `NaN` 返回值；函数返回 `double`，无错误码通道。
  - **建议-1** `:183` `kPlaneGeomRatio=0.0625`、`:324` `kFitEpsScale=8.0` 是**编译期常量、不可配置**，而同类的 `cosmic_clip_sigma`/`min_patch_samples` 都在 `SnrNoiseModelConfig` 里——口径不一致。其余三个常量（`:92 kLn10`、`:95 kTrimMeanToSigma`、`:642 kMaskMoffatBeta`）出处注释齐备，符合「有科学推导的保留并注明出处」。
  - **无问题（正面核实）**：`:329-366` 用 Householder QR 而非正规方程（避免条件数平方放大，与 `:319-321` 的实测依据一致）；`:487-500` 权重上溢做 IEEE 饱和而非科学截断；`:33-90` 注册表已加 `std::mutex`（PERF-P1），`registry_get_floor` 对未绑定/非有限/≤0 一律返回 false ⇒ 消费侧 fail-closed。**本 TU 无 `std::thread`/`omp`/`std::async`/自建池**（盲算实测命中数全 0），符合「池的所有权归调度器」。
  - **无问题（正面核实）** `:1106-1125` 的几何退化路径与 fill 侧 `:1325` 用**同一判据**（`has_spatial_field && n>=4`），构造一致。

### 3.2 `tests/p1noise/p1noise_tests_core.cpp`（1603 行）— 判定：**阻断**

- **看到什么**：
  - **B-2（固化方）** `:1305` `is_nan_bits(var)` 断言 NaN 被写入；`:1309-1310` 断言 α=0 后 `(0.0, 1/25)` 逐位成立——把违反 `noise_model.cpp:1372-1375` 的行为写成期望值。
  - **B-3（相邻）** 本片测试全用 `bit_eq`/直接比较，NaN 分支见 3.4。
  - **须修-3（自洽式断言）** `:182-187`（`check_a2_oracle_bitwise`）与 `:523-525`（`check_o1_full_oracle`）分别拿 `ctrl_sigma`/`ctrl_variance` 与 oracle 同名量比；而 oracle（3.4 `:145-147`）**复制了同一处非对称**，⇒ **两条断言对「σ 未钳位而 variance 已钳位」这一缺陷按构造恒过**。全测试面**无任何一处**断言 `ctrl_variance[i] == ctrl_sigma[i]*ctrl_sigma[i]`（盲算 grep 命中 0）。
  - **须修-9** `:715-716`、`:1515-1516`、`:1561` 三处直接读 `hull_nonpositive_frac` / `variance_plane_auditable(...)`——与 B-1 同因，无法区分「没算」与「算了是 0」。
  - **须修-10** `:940-945` `n_clamped >= expect_clamped`，其中 `expect_clamped` 由**生产自身输出** `ctrl_variance[i] == big_floor` 反推 ⇒ 该断言近似 `x >= x`，无区分力（且用 `>=` 进一步放宽）。
  - **须修-11** `:1102-1125` 的 `model_bitwise_equal` 只比 12 个字段，**完全跳过** §5d 新增的全部审计字段（`plane_a/b/c`、`hull_*`、`r_*`、`n_structure_rejected_patches`、`mask_radius_p50`、`mask_frac`、`mask_degraded`）⇒ I4 确定性门对新特性零覆盖。
  - **须修-12** `:246-252` 把 `gain=0 → 0.0`、`gain<0 → 0.0` 显式列为期望用例；`:249-252` 与 3.4 的近复制 oracle 互比 ⇒ fail-open 被双重固化。
  - **须修-13** `:1267-1271` `snr_noise_scale_law` 与 `scale_law_oracle` 互比，**按构造恒过**（两函数逐字节相同，见 3.4）。`:1273-1276`/`:1296` 才是真断言。
  - **无问题（正面核实）** `:155/289/574/608-609/1432/1482/1536` 的 `n_qualified + n_rejected == 64` 守恒在**结构剔除计入 n_rejected 之后仍成立**（生产 `:1017` 同步累加），口径一致。
  - **无问题（正面核实）** `:391`（半径对 F 严格单调，注释明说旧实现必红）、`:419`（要求降级通道下**不**单调）、`:1050-1059`（全 0 面拒绝 / 单点可用即接受的双向对照）、`:1062-1069`（空面不得恒真）、`:754-757`（两档非空 + 可用档须含严格正值）——这几处是**判据非退化**做得好的样本，有真实判别力。

### 3.3 `tests/p1noise/noise_model_numpy_oracle.py`（610 行）— 判定：**阻断**

- **看到什么**：
  - **B-3** `:184-186` `close()` 的 NaN 失明（已本机复现，§5 CE-3）。`bad = int(np.sum(rel > rtol))`，NaN 永不计入。
  - **须修-14** `:404-405` 与 `:415` 两处级联短路：一旦 `G_FAIL` 非零或控制点数不等，**后续所有数值比较整段跳过**（方向上仍会红，故不构成假绿，但会掩盖「哪一条真的坏了」）。
  - **须修-15** `:486`、`:517`、`:548`、`:583`、`:586`、`:590` 六处 `if "KEY" in out: ...` 守卫——**键缺失即静默跳过该检查**，既不记 FAIL 也不计入 `G_TOTAL`。这与本文件 `:25-26` 自己引用的规范（「检查器在输入缺失…判红」，ENGINEERING_SPEC §8:122）**方向相反**。
  - **须修-16** `:400-403` 含星帧 `D_starfield` 在任何数值比较之前 `return out` ⇒ **该用例只验掩膜元数据与 σ 偏差，完全不验控制点、fill 平面、§5d 结构剔除**。而 §5d 的立项场景正是含结构污染的帧。
  - **须修-17** `:404` + `:407-370` 的 `reference_model` **不实现 §5d 的 R 剔除**（无掩膜、无白噪声等价 σ、无 `select_structure_tail`），因此 `:409-413` 的控制点数一致性断言**只能在「生产一个 patch 都不剔」时成立**——对本片旗舰特性（§5d ①）**零判别力**。
  - **须修-18** `:70` 用裸 `assert` 校验 `MAD_TO_SIGMA == 1.482602218505602`；`python -O` 会剥掉该断言，而本文件 `:25-26` 正是以 fail-closed 为自我要求。
  - **无问题（正面核实）** `:513-521` 的 MC 带 `[0.91496, 1.05138]` 与注释给的 E=0.983170±3×0.022736 算术自洽，且注释明说 `min_samples=5` 会落到 0.7488 必红——**这是有真实判别力的门**，不是恒真门。
  - **无问题（正面核实）** `:36-41`（缺 numpy 判红）与 `:444-447`（缺 g++ 判红）是同目录里**唯一**做对的 fail-closed 样板——正是 3.5 反面做法的对照。

### 3.4 `tests/p1noise/p1noise_oracle.hpp`（505 行）— 判定：**须修**

- **看到什么**：
  - **须修-13** `:484-490` `scale_law_oracle` 与生产 `noise_model.cpp:1465-1469` **逐字节相同**（两子代理各自 `diff` 复核为 0 行差异）。违反本文件头 `:9`「不复制同一实现」与 `:21`「独立编码」；`:481-482` 的注释实为对复制的自认。
  - **须修-3（自洽式断言根源）** `:145-147` `ctrl_sigma=sig` / `ctrl_variance=max(sig²,floor)` / `ctrl_ivar=1/ctrl_variance`——与生产 `:1102-1104` **同一处非对称**，故所有 σ/variance 互比断言按构造恒过。
  - **须修-12** `:474-478` `gain_variance_oracle` 与生产 `:1475-1479` 语义等价（仅 `:476` 用三元 vs `std::max`），**无独立推导**。
  - **须修-4（同一死码被复制进 oracle）** `:318` `return 0; // k = 3 ⇒ β = 0` + `:360` `if (k>0 && q==0) return false;` ⇒ `:361` 同样不可达；生产的两处错误注释被一并复制。
  - **建议-2** `:492` 注释写「NaN 不等价」，`:494` 实现却是 `if (isnan(a)&&isnan(b)) return true;`——**注释与实现相反**。该分支只在**两侧同时 NaN** 时触发，故属「掩蔽双方同时损坏」而非「对健康断言恒绿」（措辞经两子代理修正后采纳）。
  - **无问题（正面核实）** `:50-56` `median_oracle` 用全排序 vs 生产 `nth_element`（路径真不同源）；`:322-356` 修正 Gram-Schmidt vs 生产 Householder；`:59-65` 取 σ 的集合定义与生产 `:963-966` 显式对齐——这三处**是**真正的独立实现。

### 3.5 `tests/p1noise/p1noise_abi_layout_check.py`（141 行）— 判定：**须修**

- **看到什么**：
  - **须修-19** `:76-78` `if not shutil.which("g++"): print("SKIP: ..."); return 0`——**缺编译器 ⇒ 判 PASS**。同目录姊妹件 `noise_model_numpy_oracle.py:444-447` 对**完全相同**的条件 `return 1`，并在 `:25-26` 明引 ENGINEERING_SPEC §8:122「缺 numpy/g++ 判红，不再静默判绿」。**同目录两份检查器对同一条件实行相反策略。**
  - **须修-20** `:6-13` 文档称「判据（三条，全过 rc=0）」，但第三条（突变自检）由 `:101-102` 的 `if not self_test: return 0` 门控；而 `tests/p1noise/CMakeLists.txt:100-101` 注册 CTest 时**不传 `--self-test`** ⇒ 真红证明在 CI 中**从不执行**。
  - **无问题（正面核实）** `:115-117` 突变锚点找不到时 `return 1`（fail-closed）；`:123-129` 突变头编译失败且非 static_assert 时 `return 1`（fail-closed）。这两处判据是对的。

### 3.6 `README.md`（248 行）— 判定：**须修**

- **看到什么**（全部为本人 `wc -l` / `grep` / 根 `CMakeLists.txt` 实测）：
  - **须修-21（行数漂移）** `:6` 称 `noise_model.cpp` **475 行**，实测 **1482**（差 1007）；`:8` 称 `snr_estimator.h` **526 行**，实测 **647**。
  - **须修-22（源码锚全错）** `:88-93` 给出的实现行号 `snr_noise_scale_law :488-495` / `gain_variance :497-505` / `v1 :348-356` / `fill :463-470` / `free :472-486`，实测分别为 **1463 / 1472 / 1293 / 1412 / 1447**（整体偏移约 +1000）。`:133-137` 的 `noise_model_impl :112-267`、`robust_median :42-53`、`collect_patch_sky :73-107`、`fill_impl :408-461` 实测为 **783 / 100 / 133 / 1323**。
  - **须修-23（悬空引用：已删文件被当作现存遗留通道）** `:188-192` 详述「遗留通道 `lib/algorithms/noise_snr/wrapper_phase1/noise_model.{h,cpp}`（39+67 行）…静态库 `acsd_phase1_noise` CMakeLists.txt:521-524、主程序链接 :607」。**实测 `wrapper_phase1/` 仅含 `README.md`、`snr_frame_science.cpp`、`snr_frame_science.h`——`noise_model.h/.cpp` 已不存在**（根 `CMakeLists.txt:1121-1125` 明记「B 已退役, 不再编入」）。文档把已退役对象描述为在册遗留通道，正是本项目已多次踩中的「退役对象 / 悬空引用」形态。
  - **须修-24（指认错生产文件）** `:9-14` 称根 CMake 主图只收 `wrapper_phase1/noise_model.cpp` + `wrapper_phase1/snr_frame_science.cpp` + `cpp/src/snr_science.cpp`。实测根 `CMakeLists.txt:1128-1131` 的 `acsd_phase1_noise` 收的是 **`cpp/src/noise_model.cpp`**（:1129）——即本片所审的那份，而非 README 点名的 `wrapper_phase1/noise_model.cpp`。README §0 的整个「本模块不得再自报唯一生产实现」框架因此**指错了对象**。（`wrapper_phase1/noise_model.cpp` 在全仓任何 CMakeLists 中**均未出现**，只有注释提及。）
  - **须修-25（配置表缺 5 字段）** `:105-121` 的 `SnrNoiseModelConfig` 表**完全没有** `mask_k_sigma` / `mask_r_min_px` / `mask_fwhm_floor_scale` / `mask_budget_min_patches` / `mask_budget_min_sky`（实测 README 命中数 = 0），而生产 `default_config` 有（`:1257-1262`）且 NumPy oracle 逐字段断言（`:500-504`）。
  - **须修-26（整节缺失）** README 全文对 `SCI-VAR-ADAPT-01` / `hull_nonpositive` / `r_fence` / `n_structure_rejected` / `variance_plane_policy` 的命中数为 **0**——§5d 这一整族新特性（12 个 ABI 字段、ABI v1→v2、受约束拟合、自校准 R 判据）在模块自己的 README 里**完全不可见**，而 §5d 要求这些可观测量必须写入 provenance。
  - **须修-27（已修项仍列为未修）** `:148` 称「并发 build/free **无锁**（DISP-NOISE-001）」，但 `noise_model.cpp:45` 已有 `std::mutex g_model_registry_mutex` 且 8 处访问全部 `lock_guard`（实测命中 8）；`:176` 的 DISP-NOISE-002「build floor<=0 不 clamp」已由 `:805-807` 与 n11 组修正。README 对 `mutex`/`PERF-P1` 命中数 = 0。
  - **须修-28（掩膜语义反了）** `:112` 称 `mask_radius_scale` 产出「统一 rmax=60 px（**不按亮度/振幅缩放**，API 无 amplitude 输入）」。这与生产 `:877` 的逐星 `mask_local_radius(f, wf, k_bg)` 直接矛盾，且生产 `:639-640` 明写「『掩膜半径与星亮度解耦』为**错误陈述**」。README 仍在陈述被生产代码点名证伪的旧语义。

### 3.7 `test/test_snr_estimator.py`（305 行）— 判定：**须修**

- **看到什么**：
  - **须修-29（悬空 import，整文件不可运行）** `:26-28` `sys.path.insert(0, <dir>/../python)` 后 `from snr_estimator import SNREstimator`。实测 **`lib/algorithms/noise_snr/python/` 不存在**，且全仓 `find . -name "snr_estimator*.py"` **无任何命中**。`:28` 无 try/except ⇒ 该 305 行测试在导入期即 `ModuleNotFoundError`。
  - **建议-3** `:45-47` `_load_estimator()` 文档写「失败则**跳过**测试」，实现却是 `return SNREstimator()` 直接抛异常。**代码比注释安全**（抛错会红），但注释描述的恰是本项目明禁的 skip-即绿形态。
  - **建议-4** `:261-262` `assert np.all(snr_flat > 0)` 与 `assert min_val > 0` 冗余。
  - **无问题（正面核实）** `:1-4` 明确标注 `NON_PRODUCTION_TOOL_ONLY`，与生产边界一致；`:41` 的 PSF 行布局 `[status,B,flux,cx,cy,fwhm,A,residual,0.1]` 与生产 `noise_model.cpp:1214-1219` 的列语义逐项吻合。

### 3.8 `tests/p1noise/p1noise_tests_selfcheck.cpp`（265 行）— 判定：**须修**

- **看到什么**：
  - **须修-30（覆盖面 vs 声明不符）** 头注释 `:7-17` 罗列 30+ 个注入名，暗示全组受保护；但实现只注入 4 个：`:104` `a1_sigma_rtol`（units）、`:157` `n7_plane_pred_unavailable`（negative）、`:200` `a1_no_reject_pure_noise` 与 `a2_structure_rejected`（adaptive）。**`properties`/`oracle`/`scale_law`/`fill`/`mask` 五组无任何能红证明**——恰好包括本片 B-2 所在的 `scale_law` 组。
  - **须修-31（崩溃被当成「能红」）** `:132` `const int child_rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;`，`:134` 只判 `child_rc == 0`。子进程被信号杀死时 `child_rc = -1 ≠ 0` ⇒ 判为「注入机制生效」。**SIGSEGV 与「断言正确变红」被混为一谈**，证据资格被稀释。
  - **无问题（正面核实）** `:45-87` `check_guard_path()` 是本片少见的高质量反恒真设计：`:56` 用**空串故障名**主动触发此前不可达的失败路径，`:68` 验证 `nullptr` 约定仍生效，`:75-83` 验证活注册确实翻转并复位。`:134` 的 `child_rc == 0` 判红方向也对。
  - **无问题（正面核实）** `:103-104` `P1NOISE_SELFCHECK_FAULT` 覆盖未知名时子进程无匹配 → 子进程 PASS → 父进程判红，fail-closed 方向正确。

### 3.9 `tests/p1noise/p1noise_test_main.hpp`（147 行）— 判定：**须修**

- **看到什么**：
  - **须修-32（框架 fail-open：组名拼错 ⇒ 零组运行仍 PASS）** `:104-142`：`group` 初值 `"all"`；`:109` 任何不以 `--` 开头的位置参数都覆盖 `group`；`:125` `if (group != "all" && group != groups[i].name) continue;`。若传入**未注册的组名**（CI 里一个拼写错误即可），8 个组全部 `continue`，`total_fail` 保持 0，`:137-140` 打印 `P1NOISE TESTS PASS (group=<错名>)` 并 `return 0`。**一个字节都没跑，却报绿。**
  - **建议-5** `:87-96` `P1NOISE_CHECK_EQ` **无 faultname 参数、不参与注入**，与 `:67-85` 的 `P1NOISE_CHECK` 语义不对等；注入只能靠具名 CHECK 翻转。
  - **无问题（正面核实）** `:63-65` `fault_name_usable` 用**运行期内容判定**取代旧的 `(faultname)!=nullptr` 编译期恒真比较，并留下可触发的证据路径——这是对本项目「恒真门没有证据资格」的正确整改。

### 3.10 `tests/p1noise/p1noise_fixtures.hpp`（408 行）— 判定：**建议**

- **看到什么**：
  - **建议-6（夹具注释与现行语义相反）** `:177-179` 述 FIX-NOISE-D 前提「亮星(1e4) 与暗星(10) 同坐标 ⇒ source_mask 逐位相同（rmax 与振幅无关）」，`:224-226` 据此按 `rmax` 画 oracle 掩膜。但生产 `:877` 用逐星 `mask_local_radius`，且 `:639-640` 明写「掩膜半径与星亮度解耦为错误陈述」。`fxd.dim` / `fxd.base` 在整个测试面**无人使用**（实测 `check_o2` 只用 `fxd.bright`）⇒ 该前提已成死说明。
  - **建议-7** `:272-274` 已诚实登记 scale_law 的 `alpha=NaN` 现状，`:299-311` 的 alpha=0 行为由 core 测试覆盖；但夹具**未提供 alpha=0 的 pair 生成器**（`fix_noise_f_pair` 只被 `:1265` 以 25.0 调用）。
  - **无问题（正面核实）** `:32-52` splitmix64 + Box-Muller 全离线、seed 完全决定序列，符合「零真实数据依赖」；`:400-405` `zero_weight_frac` 对空表返回 1.0（fail-closed 方向）。
  - **无问题（正面核实）** `:262` `var_th` 用 `mu/gain + (rn/gain)*(rn/gain)` 而非 `(rn*rn)/(gain*gain)`，与生产 `:1477-1479` 的语序**不同源**，是有意的 oracle 独立化。

### 3.11 `tests/p1noise/CMakeLists.txt`（191 行）— 判定：**建议**

- **看到什么**：
  - **建议-8** `:169` 注释称「numpy 或 g++ 缺失时脚本自身 SKIP 并返回 0」。这对 `p1noise_abi_layout_check.py:76-78` 成立，但**对 `noise_model_numpy_oracle.py` 不成立**——后者 `:40-41`/`:446-447` 明确 `return 1`。一条注释描述了两个行为相反的脚本。
  - **建议-9** `:173` 的 `if(P1NOISE_PYTHON3)` 已因 `:57-61` 的 `FATAL_ERROR` 而恒真，是冗余分支。
  - **无问题（正面核实）** `:50-61` 把 `find_program` 上移并对缺 python3 `FATAL_ERROR`，注释还记录了「位置错误导致全新 build 目录首次 configure 假红、再 configure 必绿」的历史与专用扫描器 `check_cmake_usebeforedef.py`——**这是本片 fail-closed 做得最好的样本**，应为 3.5 的整改模板。
  - **无问题（正面核实）** `:62-64` 把 `snr_science.cpp` 纳入被测源闭包，与生产 `:1194-1195` 调用 `snr_calib_zero_point_standard_error` 一致。

### 3.12 `include/acsd/noise/variance_plane_policy.h`（139 行）— 判定：**须修**

- **看到什么**：
  - **B-1（消费侧）** `:88-90` `variance_plane_auditable(x)` 判 `isfinite(x) && x == 0.0`；`:86-87` 明文承诺「任何 ≠ 0 的取值（正残差、**负值**、NaN）一律判不可审计」。与生产 `:1150` 冲突：生产 `if (!(frac>0.0)) frac=0.0;` **结构上不可能**吐出负值 ⇒ 该门承诺的「负值⇒红」分支**从生产侧永不可达**，只有 `:1564` 手写字面量 `variance_plane_auditable(-1.0)` 能触发。**这是一份生产者结构上无法兑现的合同。**
  - **须修-33（悬空引用）** `:112-115` 把 `variance_audit_missing_fields(...)` 与 `variance_plane_auditable(...)` 并列为「两者都过才可发布」的可执行判据，并要求生产者/消费者/测试「**必须**引用同一份」。**`acsd::noise` 命名空间内不存在该符号**；真实实现是 `module_adapters.cpp:7589` 的 `p1_variance_audit_missing_fields(const Json&)`（不同名、不同层、不同签名），其余同名命中全是 JSON **键**。按注释去 grep 的人会找不到该门，从而可能误判「此门不存在」。
  - **须修-34** `:132-134` `variance_audit_required_field_count()` 返回**硬编码 `10u`**，注释却称其存在是为了「避免加字段时漏改一处」。硬编码第二处恰恰**制造**了要改两处的风险；无 `static_assert`、无 `sizeof(kFields)/sizeof(kFields[0])`、无任何测试比对。计大 ⇒ 越界读静态数组；计小 ⇒ 新字段被静默跳过审计。
  - **无问题（正面核实）** `:51-63` `classify_variance_plane` 的三态分类逐分支正确（NaN/负/+inf 均归 corrupt，-inf 被 `x<0.0f` 捕获）；`:61` 显式要求 `n_usable > 0` 排除了恒真门；`:67-74` 的旧判据明确标注「只作对照臂，不得用于生产判定」。
  - **无问题（正面核实）** `:98` 的 `snr_estimator.h:187-201` 行号**准确**（经两子代理逐字段核对，10 个必需键全在该区间内）——我原疑其为陈旧引用，此处**我的疑问被否决**。

### 3.13 `include/acsd/noise/saturation_policy.h`（70 行）— 判定：**无问题（已接线）**

- **看到什么**：`:26-34` `strtod` + `isfinite` + `>0` 三重拒绝，返回 0 表示 unset 而非「无饱和」；`:37-49` 三级优先级（cfg > SATURATE > DATAMAX）；`:52-59` 来源标签；`:63-65` `DISABLED_NO_METADATA` 显式降级串。
- **核实**：本片初判此头「悬空」，**实测证伪**——`orchestrator.cpp:75` include、`:4728` 调用 `resolve_effective_saturation`、`:4732-4733` 写 filter/source，另有 `orchestrator_saturation_wiring_gate.cpp:322-329` 做门。**已正确接线**。
- **残留观察（建议）**：`noise_model.cpp` 自身对 `saturation_policy.h` / `DISABLED_NO_METADATA` 命中数为 **0** ⇒ 模型层完全依赖编排层履约；`valid_pixel`（`:122-126`）在 level=0 时**不过滤任何像素**，若编排层漏写降级声明，饱和像素会静默进入 blank-sky 统计。这是**跨层契约依赖**，非本片缺陷，故不计入阻断。

### 3.14 `CMakeLists.txt`（97 行）— 判定：**建议**

- **看到什么**：
  - **建议-10（锚点漂移）** `:23` 称 `kTrimMeanToSigma (noise_model.cpp:37)`，实际在 **`:95`**；`:26` 称根 `:240` 被注释，实际根 **`:383`**；`:28-29` 称根 `:630-633`，实际根 **`:1128-1131`**。三处根行号均已漂移约 150–500 行。
  - **建议-11（与 Makefile 的 OpenMP 口径冲突）** `:14-15` 称「生产 TU 无 omp pragma ⇒ **零 OMP 链接**」，而同目录 `cpp/Makefile:2,12` 无条件带 `-fopenmp`。两套构建对同一 TU 的并行标志口径不一致（实测 `noise_model.cpp` 的 omp 命中数为 0，Makefile 的 `-fopenmp` 是无用负担）。
  - **建议-12** `:12-13` 称「项目内符号依赖 = 零；仅 std + libm」，该结论写在 `g_model_registry_mutex` 引入之前；`:79` 已正确补上 `Threads::Threads`，但注释未同步。
- **无问题（正面核实）** `:19-23` 导出面净化（version-script / `/DEF` 白名单 + hidden 可见性）设计正确；`:39-51` 对非 PIC 静态对象改从源重编译的理由（`:7-9`）陈述具体且可核。

### 3.15 `cpp/test/snr_reconcile_test.cpp`（117 行）— 判定：**建议**

- **看到什么**：
  - **建议-13（头注释声明的验证不存在）** `:8` 与 `:11` 声称「控制点写入 tiny **HISS** f32/f64 + 读回一致（HISS-102 双 dtype 闭环）」并依赖 `HissWriter`；**全文无任何 HISS 读写代码**（HISS 仅出现在这两行注释里）。文件头承诺的第 2 项验证**完全未实现**。
  - **建议-14（注释描述的算法未实现）** `:87` 注释「逐点对比（**按 ra/dec 匹配最近**）」，`:90-97` 实际是**直接按下标 `points[i]` 配对**，无任何匹配。若两模式输出顺序不同，会比对无关的星（此情形会因 `max_rel_snr` 爆炸而显红，故非静默失效，但注释是错的）。
  - **无问题（正面核实）** `:2` 明确标注 `NON_PRODUCTION_TOOL_ONLY`；`:116` 返回码正确反映 `g_fail`。

### 3.16 `tests/p1noise/p1noise_abi_layout_probe.cpp`（77 行）— 判定：**建议**

- **看到什么**：
  - **建议-15（死宏）** `:15-17` 定义 `P1NOISE_OFF(T,F)`（断言字段偏移必须为 0 或 4），**全文从未使用**。它编码的纪律一次也没被执行。
  - **无问题（正面核实）** `:18-27` `P1NOISE_HEAD` 对两个 ABI 头做 `static_assert`（类型 + 偏移 0/4）⇒ 布局漂移在**编译期**即红，比运行期比对更强；`:57-74` 已把 §5d 新增 12 个字段的偏移纳入 JSON 输出，`:75` 闭合为合法 JSON。

### 3.17 `include/acsd/information_weight.h`（127 行）— 判定：**无问题**

- **读了什么**：全读。W_info / Q / F_hat 契约头，纯声明。
- **看到什么**：`:30` `kQwRelTol = 1e-9` 冻结容差并注明合同锚；`:57-67` 五个函数声明**不调用任何被测函数**（真独立 oracle 的形态）；`:72-91` `WhiteNoiseGate` 用 `is_diagonal && sigma_declared` 双条件而非隐含放行；`:96-103` `DiagApproxReport` 强制 `reported` 标志，`:101` 明写 `variance_ratio = Var_approx/Var_true` 语义。
- **判定**：**无问题**。已核实实现存在（`cpp/src/information_weight.cpp`，11519 字节），非悬空声明。
- **注意**：本片**未读** `information_weight.cpp`（属其他片），故对其实现不作判定。

### 3.18 `cpp/Makefile`（16 行）— 判定：**建议**

- **看到什么**：`:2/:12` 强制 `-fopenmp`（与 3.14 冲突）；`:12` `-static` 与 `-shared` 组合；`:15` `clean:` 用 **Windows `del`** 而 `:1` 设定 `CXX = g++`（Unix 工具链）⇒ `make clean` 在自述工具链上不可用；`:4` SRCS 三元组与实际文件一致 ✓。
- **定位**：本 Makefile 产出的 `snr_estimator.dll` **不是生产产物**（生产是根 `CMakeLists.txt:1128` 的静态库 `acsd_phase1_noise`），属 README `:27` 登记的第三套并行的构建定义。**并行构建面 = 漂移面**，但本片未发现其与根图存在**内容**不一致（仅 OpenMP 标志与 clean 目标两处）。

### 3.19 `.gitignore`（21 行）— 判定：**无问题**

- **看到什么**：`:2-8` 覆盖 dll/exe/o/obj/so/a/lib；`:11-12` Python 缓存；`:16-17` logs 且保留 `.gitkeep`；`:20-21` IDE。
- **判定**：**无问题**。生产产物在 build 目录（out-of-tree），`cpp/Makefile:5` 的树内 `snr_estimator.dll` 被 `:2` 正确忽略。

### 3.20 `tests/p1noise/p1noise_tests_main.cpp`（12 行）— 判定：**无问题**

- **看到什么**：`:10-12` `main` 仅转发 `p1noise_run_core_groups(argc, argv)`，组注册表在 core TU。
- **判定**：**无问题**。`:3` 的组清单写作「units/properties/oracle/negative/scale_law/fill」共 6 组，而 `p1noise_tests_core.cpp:1592-1601` 实际注册 **8** 组（多 `mask`、`adaptive`）⇒ **仅此一处漏列**，属轻微注释滞后。

---

## 4. 发现清单

### 4.1 阻断（3）

| ID | 发现 | 位置 | 类别 |
|---|---|---|---|
| **B-1** | `hull_nonpositive_frac` 审计门是**恒真门**：「从未计算」与「算过且干净」在产品面逐位相同；消费侧无 `has_spatial_field` 守卫；仓内自承「红侧是注入出来的」 | `noise_model.cpp:797` / `:1126-1155` / `:1152`；`variance_plane_policy.h:88-90`；`p1noise_tests_core.cpp:716,1516,1561`；旁证 `run/CHAIN-WIRE-ADAPT-01/REPORT.md:239` | 恒真门 + 自洽式断言 |
| **B-2** | `snr_noise_scale_law` 为 `void`，α=0/NaN 静默产出 `(0, 旧ivar)`/`(NaN, 旧ivar)`，违反同 TU 两态与禁 NaN 不变式；**且测试把缺陷固化为期望值** | `noise_model.cpp:1463-1470`（违反 `:1372-1375`）；`p1noise_tests_core.cpp:1305,1309-1310`；`noise_model_numpy_oracle.py:162-164`（退化域零覆盖） | 错误码缺失 + 自愈式测试 |
| **B-3** | NumPy oracle 的 `close()` 比较器**对 NaN 失明**：NaN 永不计入 bad ⇒ 所有 fill 面/控制点/标量断言对 NaN 恒绿（本机已实测复现） | `noise_model_numpy_oracle.py:184-186`（影响 `:416-439`, `:563-566`, `:592-597`） | 恒真门（active） |

### 4.2 须修（22）

| ID | 发现 | 位置 |
|---|---|---|
| S-1 | `ctrl_variance[i] ≠ ctrl_sigma[i]²`（σ 未钳位 / variance 已钳位）；公共头把该恒等式**声明为事实**；oracle 复制同一非对称 ⇒ 断言按构造恒过；全测试面无一处校验该恒等式；仓内有消费者从 σ 反推方差 | `noise_model.cpp:1102-1104`；`snr_estimator.h:165-167`；`p1noise_oracle.hpp:145-147`；旁证 `实验/absolute-snr/code/b7_absolute_snr_recon.py:745` |
| S-2 | 同一 impl 内 `variance_floor` 显式 fail-closed，5 个 MASK-002 参数静默替换为硬编码默认，无错误码无诊断（全文件 `fprintf` 命中 0） | `noise_model.cpp:805-807` vs `:826-830` |
| S-3 | `scale_law_oracle` 与生产**逐字节相同**，违反 oracle 自身的「不复制同一实现 / 独立编码」条款 | `p1noise_oracle.hpp:484-490` ≡ `noise_model.cpp:1465-1469`；条款 `hpp:9,21` |
| S-4 | `snr_noise_gain_variance` 对 `gain<=0` 静默返回 0 方差（非法输入＝完美探测器）、对 `gain=NaN` 返回 NaN；无错误码；双 oracle 均把它固化为期望 | `noise_model.cpp:1475`；`p1noise_tests_core.cpp:246-252`；`noise_model_numpy_oracle.py:594` |
| S-5 | `p1noise_abi_layout_check.py` 缺 g++ 时 `return 0`（判绿），与同目录姊妹件 `return 1` 相反，且违背二者共同引用的 ENGINEERING_SPEC §8:122 | `p1noise_abi_layout_check.py:76-78` vs `noise_model_numpy_oracle.py:444-447` |
| S-6 | 测试框架 fail-open：**未注册组名 ⇒ 8 组全跳过 ⇒ 打印 PASS 并返回 0** | `p1noise_test_main.hpp:104-142`（尤其 `:125`、`:137-140`） |
| S-7 | 生产 `:1150` 的静默归零使 `variance_plane_policy.h:86-87` 承诺的「负值⇒红」分支结构上永不可达（自相矛盾合同） | `noise_model.cpp:1150` vs `variance_plane_policy.h:86-87` |
| S-8 | `variance_plane_auditable` 的 `== 0.0` 无任何数值容差，且**约束在归一化坐标下强制、审计却在去归一化坐标下求值**（守卫不约束被测量），存在 ~ulp 量级的偶发红 | `variance_plane_policy.h:88-90`；`noise_model.cpp:515-516`/`:584-586`（归一化）vs `:447`（去归一化）、`:611-613`（往返） |
| S-9 | 悬空引用：注释把 `variance_audit_missing_fields(...)` 列为「必须引用同一份」的可执行门，该符号在 `acsd::noise` 内**不存在** | `variance_plane_policy.h:112-115`；真实实现 `module_adapters.cpp:7589` |
| S-10 | `variance_audit_required_field_count()` 硬编码 `10u`，恰制造其注释声称要消除的「两处需改」风险；无 `static_assert`、无测试 | `variance_plane_policy.h:132-134` |
| S-11 | 死代码 + 两处错误注释：`solve_subset` 的「k=3 取 β=0」分支永不可达；附带 `:603-608` 三重循环为 O(m³) 确定空转 | `noise_model.cpp:369`、`:404`、`:527`、`:528-530`、`:603-608`；同码复制于 `p1noise_oracle.hpp:318,360,361` |
| S-12 | 失败路径不初始化输出对象：ABI 不匹配与参数非法都在 `memset` 之前返回，`*out_model` 保持调用方原样；公共头对失败时的输出状态**无任何说明** | `noise_model.cpp:1301`、`:1312`、`:790` vs `:797`；`snr_estimator.h:204-213` |
| S-13 | floor 钳位计数**只覆盖 build**，fill 逐像素钳位零计数；`bind_variance_floor` 显式不重置计数 ⇒ 计数相对当前 floor 陈旧 | `noise_model.cpp:1386` vs `:1063/1070/1079/1166`；`:54-58` |
| S-14 | NumPy oracle 六处 `if "KEY" in out:` 守卫在键缺失时**静默跳过**检查（既不 FAIL 也不计数），与本文件自引的 fail-closed 规范相反 | `noise_model_numpy_oracle.py:486,517,548,583,586,590` |
| S-15 | 含星帧 `D_starfield` 在任何数值比较前 `return out`；且 `reference_model` 不实现 §5d R 剔除 ⇒ §5d ① 对其立项场景**零判别力** | `noise_model_numpy_oracle.py:400-403`、`:323-370`、`:409-413` |
| S-16 | selfcheck 只注入 4 个点，覆盖 4/8 组；头注释罗列 30+ 故障名暗示全组受保护 | `p1noise_tests_selfcheck.cpp:7-17` vs `:104,:157,:200` |
| S-17 | selfcheck 把**子进程崩溃**（`child_rc = -1`）当作「注入使判据变红」的证据 | `p1noise_tests_selfcheck.cpp:132`、`:134` |
| S-18 | ABI 突变自检（真红证明）由 `--self-test` 门控，而 CTest 注册**不传该标志** ⇒ CI 中从不执行；文档却称「判据三条全过」 | `p1noise_abi_layout_check.py:6-13`、`:101-102` vs `tests/p1noise/CMakeLists.txt:100-101` |
| S-19 | `test_snr_estimator.py` 的 Python 模块已**不存在**，整文件导入即失败（悬空引用） | `test_snr_estimator.py:26-28`（实测 `python/` 目录与全仓 `snr_estimator.py` 均无命中） |
| S-20 | 模块 README 行数/源码锚漂移约 1000 行；**把已删文件 `wrapper_phase1/noise_model.{h,cpp}` 描述为在册遗留通道**；**指认错生产文件**（说 wrapper_phase1，实为 `cpp/src`） | `README.md:6,8,88-93,133-137,188-192`；根 `CMakeLists.txt:1121-1125,1128-1131` |
| S-21 | README 配置表缺 MASK-002 的 5 个字段；全文对 §5d 特性命中数 = 0；`:112` 仍在陈述被生产 `:639-640` 点名证伪的「掩膜与亮度解耦」旧语义；`:148` 把已加锁的并发问题列为无锁 | `README.md:105-121`、`:112`、`:148`；`noise_model.cpp:1257-1262`、`:877`、`:639-640`、`:45` |
| S-22 | `model_bitwise_equal` 跳过全部 §5d 审计字段 ⇒ I4 确定性门对新特性零覆盖；`n_clamped >= expect_clamped` 中 `expect_clamped` 由生产自身输出反推 ⇒ 近似 `x>=x` | `p1noise_tests_core.cpp:95-115`（尤其 `:110-112`）、`:940-945` |

### 4.3 建议（15）

| ID | 发现 | 位置 |
|---|---|---|
| A-1 | `bit_eq` 的注释（NaN 不等价）与实现（NaN==NaN 为真）相反；该分支只在两侧同时 NaN 时触发，属「掩蔽双方同时损坏」而非无条件恒绿 | `p1noise_oracle.hpp:492` vs `:494` |
| A-2 | 6 处 `P1NOISE_CHECK(cs, true, ...)` 是字面恒真门（用作到达性标记，良性但虚增通过计数） | `p1noise_tests_core.cpp:568,577,671,881,927,1135` |
| A-3 | `kPlaneGeomRatio=0.0625`、`kFitEpsScale=8.0` 为不可配置的编译期常量，与其他可调项口径不一致 | `noise_model.cpp:183`、`:324` |
| A-4 | `cpp/Makefile` 强制 `-fopenmp` + `-static`，与 CMakeLists「零 OMP 链接」矛盾；`clean` 用 Windows `del` 配 `g++` 工具链 | `cpp/Makefile:2,12,15` vs `CMakeLists.txt:14-15` |
| A-5 | 模块 CMakeLists 三处根行号锚漂移（236/240→383，630-633→1128-1131）；`kTrimMeanToSigma :37`→`:95`；`:12-13` 的「仅 std+libm」结论写在加锁之前 | `CMakeLists.txt:12-13,23,26,28-29` |
| A-6 | tests CMakeLists `:169` 用一条注释描述两个行为相反的脚本（SKIP/0 vs FAIL/1） | `tests/p1noise/CMakeLists.txt:169` |
| A-7 | `P1NOISE_OFF` 宏定义后从未使用，其纪律一次未执行 | `p1noise_abi_layout_probe.cpp:15-17` |
| A-8 | 夹具 D 的注释前提（掩膜与振幅无关）已被生产证伪，`fxd.dim`/`fxd.base` 无人使用 | `p1noise_fixtures.hpp:177-179,224-226` |
| A-9 | `snr_reconcile_test.cpp` 头部承诺的 HISS 双 dtype 闭环**完全未实现**；`:87` 注释「按 ra/dec 匹配最近」与下标直配不符 | `snr_reconcile_test.cpp:8,11`、`:87-97` |
| A-10 | `_load_estimator()` 文档写「失败则跳过」，实现直接抛异常（代码更安全，注释描述的是被禁形态） | `test_snr_estimator.py:45-47` |
| A-11 | NumPy oracle 用裸 `assert` 校验冻结常数，`python -O` 会剥离 | `noise_model_numpy_oracle.py:70` |
| A-12 | `test_6` 两条断言冗余 | `test_snr_estimator.py:261-262` |
| A-13 | `tests/p1noise/CMakeLists.txt:173` 的 `if(P1NOISE_PYTHON3)` 已因上游 `FATAL_ERROR` 恒真 | `tests/p1noise/CMakeLists.txt:57-61,173` |
| A-14 | `p1noise_tests_main.cpp:3` 漏列 `mask`/`adaptive` 两组（实际 8 组） | `p1noise_tests_main.cpp:3` |
| A-15 | 模型层对 `saturation_policy.h` 零引用，`valid_pixel` 在 level=0 时不过滤任何像素，降级声明完全依赖编排层履约（跨层契约，非本片缺陷） | `noise_model.cpp:122-126`（实测引用数 0） |

### 4.4 本项目固化检查项的逐项结论

| 检查项 | 结论 |
|---|---|
| **静默降级** | **命中**。S-2（5 参数静默替换）、S-7/S-8（审计量静默归零）、S-14（键缺失静跳过）、S-12（失败不初始化）、`snr_noise_gain_variance` 的 `gain<=0 → 0.0`（伪装成合法 0）。**未发现伪装成 fail-closed 实则 fail-open 的 fill 路径**——`:1362` 的 `registry_get_floor` 是真 fail-closed。 |
| **自愈判据/锚** | **未命中**（正面结论）。本片所有判据读的都是**本次执行不会覆写**的源/帧文件；未发现「第一跑红、第二跑转绿」型自愈锚。 |
| **恒红门** | **疑似 S-8**（`== 0.0` 零容差 + 归一化/去归一化坐标错配 ⇒ ~ulp 量级偶发红）。**非确定性强红**，方向为**偏绿**。 |
| **恒真门** | **双重命中**：B-1（`hull_nonpositive_frac`）、B-3（`close()` 的 NaN 失明）。另有 A-2（字面 `true` 标记）与 S-6（组名拼错仍 PASS）。 |
| **筛掉真信号** | **未命中**。§5d 的 R 剔除**先**算 R 分布再判跳变，未见「先筛子集再取极值、被筛的恰是最差那条」。`a2` 的对照臂（`p1noise_tests_core.cpp:1485-1508`）用**全部 64 个**合格 patch 复算旧估计量，刻意不筛，有判别力。 |
| **退役对象仍有活调用者** | **命中（悬空侧）**：S-20 —— README 把**已删除**的 `wrapper_phase1/noise_model.{h,cpp}` 描述为在册遗留通道，并给出 39+67 行与 CMake 行号。悬空引用侧另命中 S-19、S-9。**未发现**退役对象仍有活调用者的实例。 |
| **私建线程池** | **未命中**。生产 TU 的 `std::thread`/`omp`/`std::async`/池关键词实测命中数**全为 0**；仅 `std::mutex` 保护进程级注册表。 |
| **硬编码** | **基本合规**。5 个常量中 3 个有科学出处注释（合规）；`kPlaneGeomRatio=0.0625`（经验条件数界）与 `kFitEpsScale=8.0`（浮点余量）**不可配置**，与同类可调项口径不一（A-3）。 |
| **错误码与失败语义** | **命中**。B-2（void 无错误码）、S-4（返回 0.0 当哨兵）、S-12（失败不初始化输出）、`catch(...) { return 3; }` 吞异常全文（`:1302`、`:1313`）、A-15。**正面**：ABI/floor/fill 参数域均已 fail-closed。 |
| **数值稳定性** | **正面居多**。NaN/Inf 输入被 `valid_pixel`/`isfinite` 过滤；空输入、`n<4`、几何退化、单点可控点均有显式分支；`NaN != NaN` 在 `select_structure_tail` 的输入构造处被正确利用（`:986-987` 过滤）；权重上溢走 IEEE 饱和。**命中**：B-2 的 NaN 产出、S-1 的 σ/variance 不自洽、S-8 的坐标错配。 |

---

## 5. 你主动构造的反例

> 每个反例都写明：构造什么 / 期望推翻什么 / 是否推翻。**我推翻了自己 2 条初始假设**，记在 §7。

### CE-1（对 B-1）— 构造「控制点不足 4 ⇒ 从未审计却是绿灯」

- **构造**：3×2 patch 网格（`patch_grid_x=3, patch_grid_y=2` ⇒ 6 个 patch < 8 的天空预算 ⇒ 用 2×2=4 个合格 patch 更稳），使 `n_control_points = 4` 但控制点近共线 / 或直接取 `n=3`。令 `plane_geometry_ratio(...) < kPlaneGeomRatio` 或 `n < 4`，使 `:1113` 的守卫为假 ⇒ `fit_ok = false` ⇒ `:1126-1155` 整块跳过。
- **期望推翻**：若该路径下 `hull_nonpositive_frac` 仍非 0、或消费门因此变红，则 B-1 不成立。
- **实际**：字段保持 `:797` memset 的 `0.0`；`variance_plane_auditable(0.0)` 返回 **true**。**B-1 成立，未被推翻。**
- **子代理补强（两票一致）**：`:1060/:1067/:1073` 三条退化早返回**侥幸**被消费侧独立的 `var_degenerate` 检查兜住；**真正未被兜住**的是 `fit_ok==false 且 var_degenerate==false`（即 CE-1 这条）与 `:1148` 的面积守卫失败。另发现消费侧 `module_adapters.cpp:8375-8379` 会把**常量场帧**标为 `"fitted_spatial_plane"`。

### CE-2（对 B-2）— 构造「α=0 的自相矛盾对」并逐步走查

- **构造**：起始对 `(variance, ivar) = (25.0, 0.04)`（由 `fix_noise_f_pair(25.0)` 给出，`ivar = 1/25`）。调用 `snr_noise_scale_law(0.0, &v, &i)`。
- **逐步走查**：`:1465` `v = 25.0 * 0.0 * 0.0 = 0.0`；`:1467` `a2 = 0.0`；`:1468` 条件 `0.0 > 0.0` 为 **假** ⇒ `i` **保持 0.04 不变**。返回 `(0.0, 0.04)`。
- **期望推翻**：若最终 `ivar` 也变 0（或函数给出错误码、或方差被拒为不可用态 `(0,0)`），则 B-2 的「自相矛盾」不成立。
- **实际**：得到 `(0.0, 0.04)` —— `variance == 0` 而 `ivar != 0`，且 `v·i = 0 ≠ 1`。按同文件 `:1373-1375` 的两态穷尽，「variance==0 ⇒ ivar==0」被违反。α=NaN 同理得 `(NaN, 0.04)`，违反 `:1372` 的禁 NaN。**B-2 成立，未被推翻。**
- **测试面反向确认**：`p1noise_tests_core.cpp:1309-1310` 正是逐位断言 `(0.0, 1/25)` —— 该缺陷**被测试锁定为期望值**。

### CE-3（对 B-3）— 构造「NaN 通过 `close()`」并本机实跑

- **构造**：直接复刻 `noise_model_numpy_oracle.py:184-186` 的三行，在 `/tmp` 中以 numpy 实跑（**未编译仓内任何东西、未运行仓内脚本**），喂入 `got=[NaN, 4.0]`、`exp=[16.0, 4.0]`、`rtol=1e-9`。
- **期望推翻**：若 `n_bad > 0`（NaN 被判超差），则 B-3 不成立。
- **实测输出**：
  ```
  got=[NaN,4.0] exp=[16.0,4.0] -> n_bad = 0   (0 表示 NaN 通过)
  rel = [nan  0.]
  got=[+inf,4.0]             -> n_bad = 1
  got=[99.0,4.0]  (对照)      -> n_bad = 1
  ```
- **结论**：**NaN 通过；+inf 被捕获（对照组有效）；真实大误差被捕获（对照组有效）。** 即 NaN 是**唯一**的盲区。**B-3 成立，未被推翻**，且这是本片唯一由我**独立实跑**证实（而非仅靠阅读）的发现。

### CE-4（对 S-1）— 构造「σ/variance 恒等式断裂且 oracle 无法发现」

- **构造**：令某 patch 的 `patch_var[i] = sig² < variance_floor`（例如极暗天区 σ=1e-7 ⇒ var=1e-14 < floor=1e-12）。
- **期望推翻**：若存在任一断言能发现该断裂，或 oracle 采用了不同定义，则 S-1 的「自洽式断言」定性不成立。
- **实际**：生产 `:1099-1104` 给出 `ctrl_sigma=1e-7`、`ctrl_variance=1e-12`、`ctrl_ivar=1e12`，而 `ctrl_sigma² = 1e-14 ≠ 1e-12`。oracle `p1noise_oracle.hpp:145-147` 用**完全相同**的两行 ⇒ `p1noise_tests_core.cpp:182-184` 与 `:523-525` 的比对**对这一缺陷按构造恒过**。盲算 grep 确认：**全测试面无一处**断言 `ctrl_variance == ctrl_sigma²`。**S-1 成立，未被推翻。**

### CE-5（对 S-6）— 构造「组名拼错 ⇒ 零组运行仍报绿」

- **构造**：以 `./p1noise_tests negativ`（`negative` 少一个 e）运行。
- **期望推翻**：若框架对未知组名报错或返回非零，则 S-6 不成立。
- **走查**：`:104-109` 解析得 `group="negativ"`；`:125` 对 8 个组逐一 `continue`（无一匹配）；`total_fail` 保持 0；`:137-140` 打印 `P1NOISE TESTS PASS (group=negativ)` 并 `return 0`。**S-6 成立，未被推翻。**

### CE-6（对 S-19）— 构造「悬空 Python 模块」并全仓实证

- **构造**：`ls lib/algorithms/noise_snr/python` + `find . -name "snr_estimator*.py"`（排除 `.git`）。
- **期望推翻**：若目录或文件存在（或该文件未接入任何门），则 S-19 不成立。
- **实测**：`ls` → 「没有那个文件或目录」；`find` → **无任何命中**；`git log --diff-filter=D -- .../python/*` → **无删除记录**（说明从未在本仓提交，或已被 `run/` 外的历史处理）。**S-19 成立，未被推翻。**

---

## 6. 盲复算

**方法**：先**遮蔽**本片全部既有判定与子代理结论，独立重跑一组机械取证，再与前文结论对撞。下表为盲算阶段独立产出（与我读原文时的初始印象对照）。

| # | 盲算项 | 盲算结论 | 与既有判定对照 | 判 |
|---|---|---|---|---|
| 1 | 私建线程池 / OpenMP / async（4 个生产文件全扫） | `std::thread`/`pthread_create`/`omp `/`std::async`/池关键词命中数**全 0**；仅 `std::mutex` | 项目清单称「实测已发现 9 处违规」，本片**干净** | **一致（偏严于我的初判）**：我初读时担心 PERF-P1 引入的 mutex 属违规，盲算确认它保护的是进程级注册表、**不构成池** |
| 2 | 硬编码浮点常量清点 | 5 个 `constexpr`，3 个有科学出处注释，2 个（条件数界 / 浮点余量）**不可配置** | 既有判定把硬编码列为通病 | **偏严**：既有口径会把 5 个全记为问题；实为 3 合规 + 2 口径不一致 |
| 3 | `SNR_API void` 导出清点 | 4 个，其中 3 个（两个 stamp + free）空指针检查后 no-op，**仅 `scale_law` 做算术且可失败** | B-2 | **一致** |
| 4 | `close()` 比较器的 NaN 行为（**盲算新发现**） | 实跑证明 NaN 静默通过 | **前文与 4 个子代理均未把 `close()` 列为独立缺陷**（子代理把它作为 CLAIM-5 的附带发现提出） | **偏松**：既有判定只看 `bit_eq` 的 NaN 分支，漏掉了**真正 active** 的 NumPy 比较器 |
| 5 | `ctrl_sigma²` 恒等式的断言覆盖（**盲算新发现**） | 全测试面 grep 命中 0；oracle 复制同一非对称 | 子代理 4 提出并确认 | **偏松**：我初读只注意到「σ 未钳位 / variance 已钳位」的不对称，**未确认「无任何断言覆盖 + oracle 复制同一错误」这一自洽式本质** |
| 6 | `hull_nonpositive_frac` 的门控语义 | memset 默认 0.0 + 唯一写入点受三层守卫 + 消费门 `==0.0` 且无 `has_spatial_field` 守卫 | 子代理 2、3 分别确认 | **偏松**：既有判定只把该字段当「审计量」，未识别为**门控缺陷**（恒真门） |
| 7 | fill 侧钳位是否计数 | `registry_add_clamp` 仅在 4 处 build 路径调用；fill 逐像素钳位处 clamp 命中数 0 | S-13 | **一致** |
| 8 | 测试框架未知组名的返回码 | `total_fail=0 → return 0` | S-6（我读 `p1noise_test_main.hpp` 时已发现） | **一致** |
| 9 | `snr_estimator.h:187-201` 是否陈旧 | 10 个必需键全落在该区间 | 我初判「可能陈旧」 | **我的疑问被否决**（两子代理独立核对均判准确） |

**盲复算总判：本片既有判定对 `ALG-noise_snr-002` 整体偏松。**

偏松的具体形态有三处，都是「把缺陷记成现状/已知」而非「记成缺陷」：

1. `scale_law` 的 α 校验缺失被登记为 `DISP-NOISE-005`（`README.md:177`）与 `module.yaml` 的「现状、零新增校验」——**只记了「不校验」，没记「产出违反本模块两态不变式的自相矛盾对，且被测试逐位锁定」**。前者是待办事项，后者是**回归锁死**：任何人按待办去修 `snr_noise_scale_law`，`p1noise_tests_core.cpp:1305/1309-1310` 会立刻转红，而那两条断言的失败会被误读成「修复破坏了行为」。
2. `variance_floor` 被记为「已 fail-closed」（README `:121`、n11 组）——**准确但不完整**：只覆盖了 build 侧的 floor 拒绝，未覆盖 S-1（σ/variance 不自洽）、S-13（计数不覆盖 fill）、S-8（审计门零容差）三条同族残留。
3. §5d 审计量被记为「已写入 provenance」（`variance_plane_policy.h:92-110` 的机检清单）——**清单字段确实齐备，但 `hull_nonpositive_frac` 这一项的语义是「没算也算 0」，写入 provenance 的做法反而把缺陷洗成了合规记录**。

---

## 7. 子代理派发记录

### 7.1 派发情况

按要求派发 **3–5 个子代理**。因并行调用被重复提交，实际发出 **5 次派发、对应 3 个不同任务书**，收到 **5 份报告**（2 份为同题重复，已交叉比对后合并计权）。

| # | 任务书 | subagent id | 状态 |
|---|---|---|---|
| 1 | scale_law / gain 的 oracle 独立性、自洽式断言、错误码 | `fccb08ad-76e1-49a3-815b-2e0541d4254c` | ✅ 报告已收 |
| 2 | 同上（重复派发） | `8f5d04f1-4928-46db-8db5-8abbc21f5072` | ✅ 报告已收（与 #1 同题） |
| 3 | `hull_nonpositive_frac` 门控：恒真/恒红、自洽式断言、悬空引用、行号准确性 | `1800120a-7777-408d-bbff-bf6e8e903a9d` | ✅ 报告已收 |
| 4 | 同上（重复派发） | `21f27d35-6734-4854-b165-1b0d43e4182b` | ✅ 报告已收（与 #3 同题） |
| 5 | 静默降级 / 自愈诊断 / 死码 / σ-variance 恒等式 / 未初始化输出 | `7e0dce59-b72a-4eb7-94f9-f354c1077133` | ✅ 报告已收 |

**纪律遵守**：全部子代理均被要求「只读原文、不编译、不跑 ctest/pytest/构建、不修改任何仓内文件、不读 `/tmp/acsd_g08/`」，并逐条回执遵守情况；4 份报告均声明未修改任何文件。

### 7.2 逐条复核（采纳的修正）

子代理共提出 23 条裁定，其中我**采纳修正 7 条**（全部削弱或改写我的初始表述）：

| 我的原表述 | 子代理裁定 | 我的处置 |
|---|---|---|
| 「`bit_eq` 的 NaN 分支使断言**无条件恒绿**」 | **两票一致指其过强**：该分支只在**两侧同时 NaN** 时触发，属「掩蔽双方同时损坏」，不会让健康断言变真 | **采纳**，改写为「掩蔽双方同时损坏」（A-1），并另立 B-3 记录真正 active 的 `close()` |
| 「`gain_variance_oracle` 是逐行复制」 | **非逐字节**：`hpp:476` 用三元 vs 生产 `std::max`，语义等价但文本不同；**只有 `scale_law` 逐字节相同** | **采纳**，把两者拆成不同强度（S-3 vs S-4） |
| 「α=0 / α=NaN 从未被测试，是覆盖盲区」 | **前提错误，且缺陷更重**：`core.cpp:1299-1311` 确实测了，并把缺陷**固化为期望值** | **采纳**，B-2 改写为「缺陷被回归锁死，修生产代码会打红本套件」 |
| 「`frac` 负值被归零会**掩盖真实缺陷**」 | **两票一致定量反驳**：真实负区面积比 ulp 舍入高多个数量级，`:1150` 仅在 ~1e-16 量级触发；NaN 分支在 `:1148` 守卫下**不可达** | **采纳**。撤销「掩盖真实缺陷」，保留定性结论「使门策略的负值分支结构上不可达」（S-7） |
| 「5 个 MASK-002 参数在生产上均可静默穿透」 | **实测调度层拦截其中 3 个**（`module_adapters.cpp:6679-6706`）；只有 `mask_r_min_px=0`、`mask_fwhm_floor_scale=0` 可穿透，且这两项还同时违反 `defaults.json` 声明的 `>=1`/`>0` | **采纳**，S-2 补上「3/5 上游已 fail-closed」的限定 |
| 「floor 钳位计数不覆盖 fill，是隐藏的诊断失真」 | **机制成立但范围已在 4 处文档披露**（`noise_model.cpp:34-35,1439-1440`、公开头、NOISE_ESTIMATION.md:194）；真正的未披露危害在 `module_entry.cpp:1110-1129` | **采纳**，S-13 补上「已披露」限定 |
| 「`snr_estimator.h:187-201` 是陈旧引用」 | **两票一致判准确** | **我的疑问被否决**，从发现清单移除（见 §3.12 的正面记录） |

### 7.3 否决的条目（连同理由）

**否决我自己的 2 条：**

1. ❌ **否决「α=0/NaN 从未被测试」**——前提错误。三份独立报告一致指出 `p1noise_tests_core.cpp:1299-1311` 覆盖了这两个输入，且断言的是**缺陷输出**。已按更强的事实改写 B-2。
2. ❌ **否决「`snr_estimator.h:187-201` 为陈旧引用」**——两票核对 10 个必需键全部落在该区间，引用准确。已从发现清单删除。

**否决子代理的 3 条：**

3. ❌ **否决子代理「`frac` 负值吞掉会掩盖真实缺陷」**——两个子代理各自独立给出 shoelace 舍入量级论证（被裁掉的薄片面积比面积和的舍入噪声低 1–2 个数量级），**真实负区远高于 ulp**，故 `:1150` 对真实缺陷是 no-op。保留其「使负值分支不可达」这一定性结论，否决「掩盖真实缺陷」。
4. ❌ **否决子代理「门是恒红门」**——两个子代理都判定：`frac` 符号由不确定的舍入决定、`:1150` 又只压制负的一半，故**不是稳定恒红**，而是「偏绿的恒真门 + 约 ulp 量级的偶发红」。S-8 据此表述为「疑似偶发红」，不写死为恒红门。
5. ❌ **否决子代理 5 提及的 `p1noise_tests_perf.cpp` 内容**——该文件**不在本片成员清单**（属 `ALG-noise_snr-001`），本片未读，故不对其断言作采信或判定（仅记录子代理提及 `perf.cpp:75-77` 也用 `bit_eq`，供他片参考）。

**另有 1 条我主动补充的否决**：子代理 4 提出 `module_entry.cpp:1021` 的 `rc == 3` 守卫不足以拦住 `rc = -10`（`SNR_FLOOR_UNBOUND`）——但该文件**不在本片**，我未读，不作为本片发现，仅记录为跨片线索。

---

## 8. 自证段（可复跑命令）

> 全部命令在 `/workspace/Astro CS Database` 下执行；均为**只读**。凡涉及仓内 Python 的，仅做 `grep`/`find`/`wc`，**不执行仓内脚本、不编译**。

```bash
cd "/workspace/Astro CS Database"

# ── 0. 基线
git rev-parse HEAD                       # => 850a9edefd47434b9ab71bc907c3de1e0814b323

# ── 1. 覆盖率口径：清单 vs 实测（20 份 / 6581 行）
sed -n '392,419p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml
for f in "lib/algorithms/noise_snr/tests/p1noise/p1noise_tests_core.cpp" \
         "lib/algorithms/noise_snr/cpp/src/noise_model.cpp" \
         "lib/algorithms/noise_snr/tests/p1noise/noise_model_numpy_oracle.py" \
         "lib/algorithms/noise_snr/tests/p1noise/p1noise_oracle.hpp" \
         "lib/algorithms/noise_snr/tests/p1noise/p1noise_fixtures.hpp" \
         "lib/algorithms/noise_snr/test/test_snr_estimator.py" \
         "lib/algorithms/noise_snr/tests/p1noise/p1noise_tests_selfcheck.cpp" \
         "lib/algorithms/noise_snr/README.md" \
         "lib/algorithms/noise_snr/tests/p1noise/CMakeLists.txt" \
         "lib/algorithms/noise_snr/tests/p1noise/p1noise_test_main.hpp" \
         "lib/algorithms/noise_snr/tests/p1noise/p1noise_abi_layout_check.py" \
         "lib/algorithms/noise_snr/include/acsd/noise/variance_plane_policy.h" \
         "lib/algorithms/noise_snr/include/acsd/information_weight.h" \
         "lib/algorithms/noise_snr/cpp/test/snr_reconcile_test.cpp" \
         "lib/algorithms/noise_snr/CMakeLists.txt" \
         "lib/algorithms/noise_snr/tests/p1noise/p1noise_abi_layout_probe.cpp" \
         "lib/algorithms/noise_snr/include/acsd/noise/saturation_policy.h" \
         "lib/algorithms/noise_snr/.gitignore" \
         "lib/algorithms/noise_snr/cpp/Makefile" \
         "lib/algorithms/noise_snr/tests/p1noise/p1noise_tests_main.cpp" ; do \
  printf "%6d  %s\n" "$(wc -l < "$f")" "$f"; done | tee /tmp/den.txt
awk '{s+=$1} END{print "实测总行数 =", s}' /tmp/den.txt    # => 6581，与清单「实际行数: 6581」一致
```

```bash
# ── 2. B-1 恒真门：memset 默认 0.0 + 唯一写入点受三层守卫 + 消费门 ==0.0
sed -n '795,800p;1125,1155p' lib/algorithms/noise_snr/cpp/src/noise_model.cpp
sed -n '86,90p' lib/algorithms/noise_snr/include/acsd/noise/variance_plane_policy.h
grep -rn 'hull_nonpositive_frac *=' lib/            # => 唯一写入点 noise_model.cpp:1152
grep -rn 'variance_audit_missing_fields' lib/ docs/ eng/ | head   # => 无 acsd::noise 同名符号

# ── 3. B-2 scale_law 自相矛盾 + 回归锁定
sed -n '1463,1470p' lib/algorithms/noise_snr/cpp/src/noise_model.cpp   # void，无错误码
sed -n '1363,1379p' lib/algorithms/noise_snr/cpp/src/noise_model.cpp   # 同 TU 的两态/禁NaN不变式
sed -n '1299,1311p' lib/algorithms/noise_snr/tests/p1noise/p1noise_tests_core.cpp  # 断言缺陷为期望
sed -n '484,490p'  lib/algorithms/noise_snr/tests/p1noise/p1noise_oracle.hpp          # 逐字节副本
diff <(sed -n '1465,1469p' lib/algorithms/noise_snr/cpp/src/noise_model.cpp) \
     <(sed -n '485,489p' lib/algorithms/noise_snr/tests/p1noise/p1noise_oracle.hpp) && echo "逐字节相同"

# ── 4. B-3 close() 对 NaN 失明（唯一实跑项；在 /tmp 复刻，不执行仓内脚本）
sed -n '184,186p' lib/algorithms/noise_snr/tests/p1noise/noise_model_numpy_oracle.py
python3 - <<'PY'
import numpy as np
def close_rel(got, exp, rtol):
    g=np.asarray(got,dtype=np.float64); e=np.asarray(exp,dtype=np.float64)
    rel=np.abs(g-e)/np.maximum(np.abs(e),1e-300)
    return int(np.sum(rel > rtol))
print("NaN  ->", close_rel([np.nan,4.0],[16.0,4.0],1e-9), "(0 = NaN 通过)")
print("+inf ->", close_rel([np.inf,4.0],[16.0,4.0],1e-9), "(1 = 正确捕获)")
print("大误差->", close_rel([99.0,4.0],[16.0,4.0],1e-9), "(1 = 对照组有效)")
PY

# ── 5. S-1 σ/variance 恒等式断裂 + oracle 复制同一非对称 + 无任何断言
sed -n '1097,1105p' lib/algorithms/noise_snr/cpp/src/noise_model.cpp
sed -n '143,148p'   lib/algorithms/noise_snr/tests/p1noise/p1noise_oracle.hpp
grep -rn 'ctrl_variance.*ctrl_sigma\|ctrl_sigma.*ctrl_variance' lib/algorithms/noise_snr/tests/ \
  || echo "全测试面无任何断言校验 ctrl_variance == ctrl_sigma^2"

# ── 6. S-5 同目录两检查器对「缺 g++」实行相反策略
sed -n '76,78p'  lib/algorithms/noise_snr/tests/p1noise/p1noise_abi_layout_check.py   # return 0
sed -n '444,447p' lib/algorithms/noise_snr/tests/p1noise/noise_model_numpy_oracle.py  # return 1
sed -n '24,27p' lib/algorithms/noise_snr/tests/p1noise/noise_model_numpy_oracle.py    # 自引 ENGINEERING_SPEC §8:122

# ── 7. S-6 框架 fail-open：未知组名 ⇒ 零组运行仍 PASS
sed -n '104,142p' lib/algorithms/noise_snr/tests/p1noise/p1noise_test_main.hpp

# ── 8. S-11 死码 + 错误注释 + O(m^3) 空转
sed -n '366,370p;400,405p;523,531p;603,608p' lib/algorithms/noise_snr/cpp/src/noise_model.cpp

# ── 9. S-19 悬空 Python 模块（整文件不可运行）
ls lib/algorithms/noise_snr/python                                  # => 不存在
find . -name "snr_estimator*.py" -not -path "./.git/*"               # => 无命中
sed -n '24,29p' lib/algorithms/noise_snr/test/test_snr_estimator.py

# ── 10. S-20/S-21 README 漂移 + 已删文件 + 指认错生产文件
wc -l lib/algorithms/noise_snr/cpp/src/noise_model.cpp \
      lib/algorithms/noise_snr/cpp/include/snr_estimator.h            # => 1482 / 647
sed -n '6,8p;188,192p' lib/algorithms/noise_snr/README.md
ls lib/algorithms/noise_snr/wrapper_phase1/                           # => 无 noise_model.{h,cpp}
grep -n 'acsd_phase1_noise' CMakeLists.txt | head -3
sed -n '1128,1131p' CMakeLists.txt                                    # => 生产源是 cpp/src/noise_model.cpp
grep -c 'SCI-VAR-ADAPT-01\|hull_nonpositive\|variance_plane_policy' lib/algorithms/noise_snr/README.md
grep -c 'mask_k_sigma\|mask_r_min_px\|mask_budget_min_sky' lib/algorithms/noise_snr/README.md
grep -c 'mutex' lib/algorithms/noise_snr/README.md; grep -c 'lock_guard' lib/algorithms/noise_snr/cpp/src/noise_model.cpp

# ── 11. 盲算项：线程池 / 硬编码 / void 导出 / fill 侧计数
for f in lib/algorithms/noise_snr/cpp/src/noise_model.cpp \
         lib/algorithms/noise_snr/include/acsd/information_weight.h \
         lib/algorithms/noise_snr/include/acsd/noise/variance_plane_policy.h \
         lib/algorithms/noise_snr/include/acsd/noise/saturation_policy.h; do
  printf "%-62s thread=%s omp=%s async=%s\n" "$f" \
    "$(grep -c 'std::thread\|pthread_create' $f)" \
    "$(grep -c 'omp ' $f)" \
    "$(grep -c 'std::async\|std::future' $f)"; done
grep -n 'constexpr double k' lib/algorithms/noise_snr/cpp/src/noise_model.cpp
grep -n 'SNR_API void'       lib/algorithms/noise_snr/cpp/src/noise_model.cpp
grep -n 'registry_add_clamp' lib/algorithms/noise_snr/cpp/src/noise_model.cpp   # 仅 build 侧 4 处
grep -c 'fprintf\|printf\|stderr' lib/algorithms/noise_snr/cpp/src/noise_model.cpp  # => 0（静默降级无任何输出）
```

**注**：命令 4 的 heredoc 是本交付件中**唯一**执行 Python 的步骤，它在 `/tmp` 复刻了仓内比较器的三行逻辑以证明 B-3，**不 import、不调用、不执行任何仓内脚本**。其余全部为 `read`/`grep`/`sed`/`wc`/`ls`/`find`/`git rev-parse` 等只读操作。
