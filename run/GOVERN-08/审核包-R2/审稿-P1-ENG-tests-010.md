# 审稿-P1-ENG-tests-010 — 对抗审稿第 1 遍

- **片号**：`ENG-tests-010`
- **层**：`eng/tests`
- **基线**：仓库 `/workspace/Astro CS Database`，`HEAD = 850a9ede`（`git -c core.quotepath=false log --oneline -1` 实测）
- **口径**：一遍 = 对同一片 47 份材料的一次完整重读；判据代码**不作为**正确性证据；一切结论来自自己读完原文 + 独立重算 + 构造反例
- **纪律遵守**：未读 `/tmp/acsd_g08/`；未 add/commit/checkout/reset/stash/`git rm --cached`；未编译、未跑 ctest/pytest/构建/任何实验或测试脚本；未修改任何仓内文件（唯一写入为本交付件）；中文路径全程 `git -c core.quotepath=false`

---

## 1. 读完了吗

| 项 | 数值 |
|---|---|
| 成员份数（权威清单） | **47** |
| 实际读完份数 | **47 / 47（100%）** |
| 成员总行数（`wc -l` 实测） | **10422**（片清单声明 `实际行数: 10422`，一致） |
| 实际读入行数 | **10422** |
| **覆盖率** | **47/47 份 = 100%；10422/10422 行 = 100%** |
| 未读完的 | **无。** 47 份成员文件全部由本人用 `read` 工具从头到尾读入（不是机器词表、不是子代理转述） |

补充说明（诚实口径）：

1. 本片 47 份成员文件我**全部亲自读入**。此外为核实结论，另读了仓内生产实现与旁证文件（`lib/algorithms/noise_snr/cpp/src/noise_model.cpp`、`lib/algorithms/noise_snr/cpp/src/information_weight.cpp`、`lib/algorithms/integration/phase1_product/src/phase1_product.cpp`、`lib/algorithms/resample/p3_rsmp_kernel_registry.cpp`、`lib/algorithms/projection/p3_projection.h`、`eng/tests/unit/p1_noise/adapter_entry_impl.cpp` 等），这些**不计入**覆盖率分母。
2. 我同时派了 **5 个子代理**做独立分片核验（见 §7）。子代理结论一律只作**线索**；凡进入本交付件的每一条，我都独立复核过（`文件:行` 或可复现命令）。我**否决**了其中 6 条（见 §7.2）。子代理读过但我未采信、也不构成我覆盖率的文件，同样如实说明（§1.1）。

### 1.1 本片成员文件存在性核查

47 份全部存在，无 MISSING（`while read; do [ -f "$f" ] && wc -l < "$f"; done`，退出码 0）。**注意 1 处路径易错**：`eng/tests/cpu/avx2/provider_avx2_so_load_test.c` 在 `eng/tests/cpu/`，**不在** `eng/tests/unit/cpu/`；按后者读会得到「文件不存在」。这是路径误用，不是文件缺失。

---

## 2. 本片判定

### **判定：阻断（BLOCKING）**

**最重的 3 条：**

**① `p3_rsmp_oracle.h:227-257` 的 `mc_var_Fhat` 是对任意 S/C_d 恒成立的代数恒等式 —— 独立 Oracle 的头条门在结构上不可证伪。**
Oracle 逐字计算 `rel_err_out = |var·a·denom − 1|`（`p3_rsmp_oracle.h:255`），其中 `denom = a·qᵀπ`（`:232`），`q = C_y⁻¹π` 由高斯消元解出（`:231`），`a·denom = a²πᵀC_y⁻¹π = W`。我独立推导：`Var(F̂) = qᵀC_y·q / denom²`，而 `C_y q = π` **由构造成立**，故 `qᵀC_y q = qᵀπ`，于是 `Var·W = (qᵀπ/(a·qᵀπ)²)·(a²·qᵀπ) = 1` —— **对任意 `build_S`、`build_R`、`cholesky_lower`、`solve` 的返回值恒成立**。把 `build_S` 的面积归一整个写错（漏掉 `area/omega` 之类），该门仍为 0。它只度量 Monte-Carlo 采样噪声与三套自写数值方法互相吻合，**不可能发现 S、C_d、q 或公式本身的任何错误**。

**② `eng/tests/unit/p1_noise/adapter_test.cpp:723-729` 的 D11 两条断言是自洽式断言（用同一个定义式既当被检量又当期望量）。**
生产 `noise_model.cpp:1041-1042` 定义 `sigma_bg_global = sqrt(variance_bg_global)`、`ivar_bg_global = 1.0 / variance_bg_global`；测试却断言 `|ivar_bg − 1.0/var_bg| ≤ 1e-15·|ivar_bg|` 与 `|sigma_bg² − var_bg| ≤ 1e-15·var_bg`。这是 `|1/V − 1/V|` 与 `|√V·√V − V|`：**对任意 `V` 逐位精确成立**。`variance_bg_global` 可以是 0.25×、100×、用错分位数、甚至取到帧方差而非天光方差 —— D11 全绿。文件头 `adapter_test.cpp:28-30` 自称「科学纪律：期望值 = direct 通道现场直调生产实现（同一权威 noise_model.cpp），无 golden 抄写」——这句话恰恰承认**「期望」就是被测实现本身**。D11 的其余两条是 `floor_echo == 1e-12`（config 回显）与 `source == "0"`，**D11 因此没有任何独立的科学内容**。

**③ `eng/tests/validation/release02/README.md:88` 的头条证据（FIX-P1 判别力）是两个恒真式的复合。**
`p1_apply_oracle.cpp:88` 用 `g_k` 造数据 → `:112` **测试自己**喂入 `k_photo = 1.0/g[k]` → `:114` 调 `apply_photometry` → `:126` 取 `out[p]/ref_applied` → 取中位。于是 `ratio ≡ (scene·g)·(1/g)/scene ≡ 1`，`|中位−1| ≡ 0`。归档 `oracle_output.txt:9` 印着位精确的 `|中位-1| = 0.000e+00` —— **位精确的零正是恒等式的指纹，不是测量的结果**。负例同样空转：`:101` 断言 `fabs(worst − fabs(g[3]/g[ref]−1.0)) < 1e-12`，而 `worst` 因 `g[3]=2.82` 最大而**恒等于**右项，该行展开为 `check(0 < 1e-12)` —— **而这行注释恰写着「判别力：若此步为 0 则测试恒真」**。README 把这条「红 1.82 → 绿 1.000000000000」当作 FIX-P1 的判别力写进结论。

---

## 3. 逐文件清单

47 份全读。`→` 后为读到的主要事实与判定依据。

| # | 成员文件（行） | 读了什么 | 看到什么（含 `文件:行`） | 判定 |
|---|---|---|---|---|
| 1 | `eng/tests/unit/p1_noise/adapter_test.cpp` (979) | 全 979 行 + 旁证 `adapter_entry_impl.cpp`、`noise_model.cpp:1030-1079` | `:28-30` 自承「期望值=direct 通道现场直调生产实现」；`:323-324` direct-vs-plugin 逐位对拍，而 `adapter_entry_impl.cpp:6` 直接 `#include .../module_entry.cpp` ⇒ **同源同 TU 编两遍，X vs X**；`:723-729` D11 自洽式断言（见 §2②）；`:325` 失败诊断 `if (od != op && g_fail == 0)` 在 `od!=op` 时 `g_fail` 必已 ≥1 ⇒ **永不触发**；`:911-912` `exec_count==0` 检的是从未 execute 过的实例 | **阻断** |
| 2 | `eng/tests/unit/p3002_uncertainty_test.cpp` (881) | 全 881 行 | 真故障注入面（`:569-636` 断言缺陷行为 + `:867-872` 注入无效则 rc=1）设计正确；`:317-320` 如实登记并修好了旧的 `status==0 \|\| true` 恒真门（正面）；`:373-374` `if (man.contains(...))` 才 CHECK，键缺失即整段静默通过（fail-open），而同文件 `:560-562` W4c 又强制要求该键 ⇒ 自相矛盾；`:366`/`:393`/`:397`/`:627-628` 注释说「恒 3 HDU」「5 HDU」，实断言是 `:371 hi.hdus==2`、`:397 ==4` ⇒ **注释与活断言相反**；`ACSD_P3002_FAULT` 在 `lib/phase3_session/` 下**零 getenv 读者**（实测），注入面是空壳 | **须修** |
| 3 | `eng/tests/unit/p1wcs/p1wcs_tests_apbp.cpp` (738) | 全 738 行 | 本片质量最高之一：`:367-505` 三条真负例（零畸变归零、τ 收紧 1000×、APx/BPx 清零必红）；`:421-447` 用 τ 收缩证明「度量非恒真」；`:327-338` 用 `n == pts.size()` 反向兜住「筛掉真信号」；`:7` 自承 1e-4 px 冻结门「判据力弱」。缺陷：`:629-642` 导出的 `run/p1wcs_wcs003/` **既不在 git 中也不在磁盘**（实测 `ls` 不存在、`git ls-files` 零命中），而 `p1wcs_astropy_cross` 读的正是同一路径（`CMakeLists.txt:146-150`）⇒ **自愈式归档**；`:579-599` 的「1/N worker」并行是**测试自己的 `#pragma omp`**，不碰任何生产并行路径 | 需修 |
| 4 | `eng/tests/integration/p1_integrate/p1_integrate_test.cpp` (647) | 全 647 行 + `phase1_product.cpp:387-410,687-690,1205-1211`、`information_weight.cpp:305-316` | `:344-346` `flux_conservation_factor == 1.0` 配负例 `:477-481`（0.64 必红）是**真判别门**（正面范例）；`:582-597` 26 条负例 + `:428-437` 未篡改正控制，真双向。缺陷：`:261` `CHECK_NEAR(v.flux_variance * v.w_info, 1.0, 1e-9)` 是**自洽式断言** —— `information_weight.cpp:313` `out.var_f = 1.0/out.sum_w`，故对任意 `sum_w` 恒真；同一恒等式在生产消费端 `phase2_integrate.cpp:346` 重复出现 ⇒ 两层都不可证伪 | 需修 |
| 5 | `eng/tests/unit/mem_wire_test.cpp` (481) | 全 481 行 | 真双向负例（W1b 预算=0 `:355-372`、W1c 估算硬写 0 `:444-467`），`:279` `peak0 > peak` 与 `:381 max_free >= 2` 是正确的非退化守卫。缺陷：`:142-148` Stub 输出是 `ctx.current_node()` 的 FNV 哈希，**按构造与并发/worker 数无关** ⇒ `:378 throttled_out == unlimited_out`、`:407 got == ref` 两条「逐位一致」在构造上不可能翻红；`:435-436` `big.limit_bytes <= ~0ull` 对 unsigned 恒真；`:163` `flush_ledger(&err) \|\| err.empty()` 弱析取 | 需修 |
| 6 | `eng/tests/unit/p2_rejection_test.cpp` (469) | 全 469 行 | `:198-244` FIX-204 路由表是真门（`:227` 钉死 `band_min==4` 锚住派生值，`:216-218` 明断 16/20 不得为 linear_fit）；`:348-423` 真 8 线程 + 三种置换逐位一致，`:420-422` 断言被排除子集非空。缺陷：`:157-166` `int reason_codes[4]={0,0,0,0}` 相加后 `CHECK(sum==0)` = `ASSERT_EQ(0+0+0+0,0)`，注释宣称的 `accepted+low+high+underdetermined == n_samples` **从未执行**（`n=100` 被 `(void)n` 丢弃）；`:143-155` 造了 20 样本 `vals` 却零使用，断言退化为枚举 `2 > 1`；`:2-4`/`:9-10` 文件头称「n>15 → linear fit」，`:212-218` 断言相反 | **阻断** |
| 7 | `eng/tests/backend/test_p3_output.py` (424) | 全 424 行 | `:297-337` test_12 发布序（fsync→校验→sha256→rename）用真 LD_PRELOAD 逐事件断言，`:332-333`「RENAME 后不得读已发布路径」在旧序下必红 —— 真负例；`:339-365` test_13 只改 DATASUM 一位数字即必红，真负例。缺陷：`:407-408` `if os.geteuid()==0: self.skipTest` 无 CI 分支，Docker 常以 root 跑 ⇒ 恒 skip（同仓 `test_cli_single_install.py:40-42` 是正确范式）；`:132-142` `_fill_ref` 从未被调用（真 fill 在 C++ 探针），注释 `:10-11` 暗示 Python 侧有独立参照，易误导；`:154` 注释「= 1536」实为 768；`:178` 关键字表含 `"HIS"`，是 `HISTORY` 的子串 ⇒ 恒真命中 | 需修 |
| 8 | `eng/tests/unit/memory_pressure_test.cpp` (413) | 全 413 行 | 本片质量最高之一：①–⑥ 六组真负例；`:375-383` 红绿配对（`stops>=1`、`max_governed==1`、`max_governed < max_free`、`max_free >= 2`）**四个断言同时保证行为差异确实来自本机制**；`:281-295` fail-open 路径显式留痕。轻微：`:163` 弱析取（后由 `:164-172` 落盘+内容兜住）；`:3` 写「四条」实列六条 | **通过** |
| 9 | `eng/tests/unit/p3_rsmp/p3_rsmp_gate_test.cpp` (365) | 全 365 行 + `p3_rsmp_kernel_registry.cpp:193-226` | `:51-59` `expect_ok`/`expect_code` 是统一入口，`:235`/`:336` 有绿侧反证，结构规范。缺陷：`:283-284`/`:347` `P3_CHECK(k.oracle.ok && k.oracle.independent)` 读的是**被测冻结注册表自己的自述布尔**（`registry:217-220` 同文件内写死），属「判据读自声明元数据」；`:202-204` 用 `kNaN` 打开 fail-closed 通道 | 建议 |
| 10 | `eng/tests/backend/test_p3_projection_oracle.py` (345) | 全 345 行 + `p3_projection.h:1-12,30-33` | 数学侧**确属第一性独立**（`:124-190` 纯 math 3D 单位向量/透视除法/Paper II；生产走 `p3_projection.cpp:93-94` 球面三角旋转核，路径确实不同）；`:249-250,322` fail-closed 做对。缺陷：`:105` 编译 `lib/algorithms/projection/p3_projection.cpp`，而该文件在 `p3_projection.h:4-11` 明文 **RETIRED**、`:33 kP3ProjectionRegistryRetired = true` ⇒ **门禁的是已退役 registry，在役 `p3_proj.h/.cpp` 零覆盖**；`:303-305` oracle 输入坐标由生产 `desc[0..5]` 算出 ⇒ **CD/CRPIX 面往返自证**；`:244` 解析出的 `min_scale`/`c_env` 从未被任何断言使用 | **阻断** |
| 11 | `eng/tests/unit/p1_stars_test.cpp` (319) | 全 319 行 | `:253-271` 真负例（注入 2% 热像素，未裁剪全局 std 必超 2%）；`:287-301` NaN fail-closed 回归锚。缺陷：`:147-150`、`:177-180`、`:127-128` 三段全在 `if (... && !sources.empty())` / `size()==2` 守卫内 ⇒ 探测器返 0 源或 1 源即**整段静默通过**；`:191` `sources[0]` 未查非空（UB）；`:212` 段头「≤2%」与 `:231 rel<=0.06` 不符 | 须修 |
| 12 | `eng/tests/unit/mon001_gate_test.cpp` (297) | 全 297 行 | `:94-104`/`:107-118` 边界真门（0.70 vs 0.6999、12.0 vs 9.9 vs −1.0 哨兵）；`:216-227` `monitoring_effective` 三要素；`:239-256` 18a 钉死「配置不得冒充观测」。缺陷：`:276` `peak_active >= peak0` 是进程级单调高水位，构造上恒真（有效的是 `:277 >= 4`）；`:279-281` 注释称「净值回零」但**无任何断言**观测净值回零；`:3-4` 头注称分母 = `min(selected,available)`，被 `:244-255` 推翻 | 通过（建议级） |
| 13 | `eng/tests/unit/p3_rsmp/p3_rsmp_oracle.h` (261) | 全 261 行 + 消费方 `p3_rsmp_oracle_test.cpp:44-55` + 生产 `p3_rsmp_kernel_registry.cpp:193-226` | **独立性声明属实**（`:15-19` 只含标准库；`:106-146` 高斯消元 vs 生产 Cholesky，确为异算法）—— 此点须如实肯定。缺陷：`:227-257` Var·W≡1（见 §2①）；`:200-204` `w00..w11` 硬编码 0.25 ⇒ `max_weight_sum_dev ≡ 0`，而 `registry:195` 亦硬编码 `0.0`，`:49` 交叉核对的是 `0 == 0`；`:180` `boundary_fail_closed = true` 在 `run_kernel_oracle()` 内**从未赋值**；`:64-69` `diagonal()` 全仓无调用者；`:168` `ALG-P3-001_KERNEL_REGISTRY` 全树零命中 | **阻断** |
| 14 | `eng/tests/unit/export_stream_test.cpp` (256) | 全 256 行 | `:101-122` 与整幅参考逐位一致是真门（`ref_pixel` 由测试经 `set_pixel_fn` 注入，调度器只负责行序，X 与 Y 确为不同计算）；`:141-156` 磁盘满真负例。缺陷：`:251` `check(!o.published || o.ok, ...)` 左支按设计恒真 ⇒ 取消门无判别力；`:183` `max_queue_depth`、`206-208` `peak_resident_bytes`、`:222 p256>p64` 全部读**生产自报指标**，若生产恒返 0 则 E1 局部绿、F1 `0==0` 假绿 | 须修 |
| 15 | `eng/tests/unit/p1wcs/p1wcs_test_main.hpp` (245) | 全 245 行 | `:154-165` 明确把旧写法 `(faultname) != nullptr`（编译期恒真）改成运行期内容判定，是**本仓对恒真门的正面处置**；`:167-185` 注入后「后续具名 CHECK 全部计失败」设计正确。潜伏：`:222-223` + `:235` 未知组名 → 循环全 `continue` → `total_fail==0` → 返回 0 并打印 PASS（当前 `p1wcs_tests_main.cpp:18` 确有注册 `"apbp"`，故不触发） | 通过（建议级） |
| 16 | `eng/tests/unit/p1wcs/CMakeLists.txt` (214) | 全 214 行 + `p1wcs_astropy_cross.py` 存在性 | `:185-190` 明记 `p1wcs_closure_metric_gate` 的 selftest 注册「随 G08-01 一并移除」；实测 `grep -rn p1wcs_closure_metric_gate` 仅 3 命中、全在 `docs/` 与 `lib/`、**零 CMake**，而其中 `docs/science/algorithms/GATES_AND_TOLERANCES.md:67` 把它登记为可执行门且末列状态标 **Y**、`docs/science/ASTROMETRY.md:281` 写「过」⇒ **门禁正本表指向一个不存在的 ctest**。另 `:155-158` `P1WCS_LABELLED_TESTS` 漏掉 `:210` 注册的 `p1wcs_clip_consistency`（其 `:211-214` 无 LABELS）⇒ `ctest -L p1wcs` 静默跳过；`:36-37` 头注 CTest 清单列 6 个，漏了本文件自己注册的 4 个 | **阻断** |
| 17 | `eng/tests/cli/test_bench_cli.py` (200) | 全 200 行 | `:99-143` verdict 单源复算是真门（独立重算 `all_pass` 再比 `d["verdict"]` 与 rc）；`:190-196` 用 `hashlib.sha256` 独立复算二进制 hash，是真外部参照；`:146-163` 退役面逐条钉 `rc==2`（正面）。缺陷：`:87 assertGreaterEqual(len(d["kernels"]), 1)` 用下限代替等值，而 docstring `:9-13` 自称保留「kernel 规格」断言；`:60-61` `parts[0] if len(parts)==2 else parts[0]` 两分支相同，死条件 | 须修 |
| 18 | `eng/tests/unit/p1_psfw/p1psfw_tests_record.cpp` (194) | 全 194 行 | m01–m26 + G01/G17/G19/G23 逐门 `has_gate` 负向 mutation，覆盖面广；`:70-74` m09b 有绿侧反证（合法 fail-closed 记录 ACCEPT）；`:177-187` 退役 token 识别 + 迁移提示四要素齐。缺陷：`:2` 引用 `docs/engineering/v6/NEGATIVE_MUTATION_CATALOG.md`（路径有误且该件已删）；`:112` `has_gate(v,"G06") \|\| has_gate(v,"G22")` 是全文件唯一析取，弱化 m18 门定位 | 通过（建议级） |
| 19 | `eng/tests/cpu/avx2/provider_avx2_so_load_test.c` (190) | 全 190 行 | `:169-174` `out[i] != (float)i` 是**测试侧独立期望**（`in0v[0..3] = 1,2,3,4`，`k=0` → `out = in0 − 1`），非自证，真门；`:118-119` 唯一导出面；`:176-179` 越表索引必 `ACS_ERR_UNSUPPORTED`。缺陷：`:116-117` `CHECK(q != NULL)` 只 `++g_failures` 不中止，`:123` 随即无条件调 `q(...)` ⇒ **dlsym 失败即空函数指针调用（UB，吞诊断）**；`:4-8`/`:16-17` 覆盖声明里的「8. dlclose 后句柄失效」实为进程退出保证，非可断言事实 | 须修（UB） |
| 20 | `eng/tests/unit/p3_assembly_test.cpp` (171) | 全 171 行 | `:131-163` 负例对照 `:159-161` 非法尺寸必 `fail-closed`，并自陈「证明上面的 OK 不是恒真」（正面）；`:150-153` sha256 恰 64 位小写 hex。缺陷：`:71-73` `CHECK(ps != nullptr)` 后**无提前返回**即调 `aio_hips_write_signal_support_tile`；`:58` 注入的 500.0 局部峰值**全程未被验证**（`:93` 只判 `val >= 99.5f`）；`:93` 注释「采样值 > 100」与断言 `>= 99.5f` 不符 | 建议 |
| 21 | `eng/tests/validation/release02/README.md` (171) | 全 171 行 | `:140` `N=8 高估 1.2857×` 算术自洽（(1+1/8)/(1−1/8)=9/7）；`:146-149` 权重序与 `synth_results_v3.json` 逐字段吻合。缺陷：`:39-41`「`k_photo` 是单一全局标量：无空间项」已被 `photometry_apply.h:43-45,110` 的 `m(x,y)` 与 `module_adapters.cpp:6281-6283` 的活调用推翻；`:82`「`apply_photometry` 零生产调用者」与 `docs/science/algorithms/CALIBRATION_ALGORITHMS.md:296`「有生产调用方」冲突；`:34-36` 三行定义式（`10^(−location)`、Tukey-IRLS c=4.685、`F_syn=∫…`）在 `photometry_apply.h` 全 115 行**无逐字存在**（c=4.685 实存于 `star_matcher.cpp:23`，归属错）；`:40` 行号漂移（`:63` 是注释，`for` 在 `:67`）；`:163` `run/RELEASE-02/L4-rebuild/` 与 `:170`「`run/` 未被删除」**均与事实相反**（实测 `run/RELEASE-02/` 只剩 `FIX-A/ fix-p2b/ weight-chain/`）；`:88` 见 §2③ | **阻断** |
| 22 | `eng/tests/validation/release02/fix_p1_photometry_apply/p1_qf_oracle.cpp` (161) | 全 161 行 | `:150-156` 三条真门：两次 `rc==0`、`da.rejected_quality==0` vs `db.rejected_quality==1`、`|loc_a−loc_b| > 1e-3` ⇒ 判别力来自**同一 fixture 下两次调用的差**，不是与自身比对，是**本片唯一完全通过的科学判据**。缺陷：`:14` 引用 `build_qf_oracle.sh` —— **全仓零命中**（实测）；`:131` `flux[i] = 10^{r_true}·fsyn[i]` 的 `fsyn` 取自被测 pass1 输出（本门只验 qf 接线、不验 `F_syn` 绝对正确性，与其自述目的 `:4-6` 一致，可接受）；`:153` 比的是 `|loc_b| < |loc_a|`（借符号，仅对本 fixture 成立） | 需修（轻） |
| 23 | `eng/tests/backend/test_p2001_parallel_sampler.py` (157) | 全 157 行 | `:106-112` test_02 断言 `returncode==0` + stderr 含 `sample ok`，真门。缺陷：`:128`/`:139` 写的 `cfg["sampler"]={"cpu_workers":1|4}` **被生产显式覆盖**（`module_adapters.cpp:9608-9612`「lease 是唯一权威…恒最后赋值」，实测在位），两次跑 worker 数完全相同 ⇒ `test_03` 不是「N vs 1」对照；`:153` 只比 `o1[0]/oN[0]`（`n_obs`），`overlap_controls`（P2-001 的科学量）**解析自 `:114-120` 却从不比较**；`:108-109`/`:124-125` 无 CLI 二进制即 `skipTest`（`build/` 被 gitignore，`build/acsd` 未跟踪）；`:104 assertIn("budget.max_workers", sess)` 是纯文本 grep，改成字面量 1 仍绿 | **阻断** |
| 24 | `eng/tests/unit/master_flat_median_test.cpp` (148) | 全 148 行 | 本片最佳之一：`:33-50`/`:112-131` 拒绝路径用哨兵 `out[0]==7.0 && out[NPIX/2]==7.0` 证明**未被回拷**；`:62-68` 正输入下 median→1.0、floor≥0.1、**非常数场**（`sorted.back()−sorted.front() > 1e-4`）三条独立科学门；`:75-92` NaN 跳过的前后行为差异有具体错值描述。轻微：哨兵只查 2/64 个槽 | **通过** |
| 25 | `eng/tests/unit/cpu004_routing_test.cpp` (147) | 全 147 行 | `:61-80` 三类 stale 判定（quota_signature / logical_available / commit）真门；`:93-103` kernel 不在 profile → 保守 baseline + **保留多线程**（`workers==4`）真门；`:120-130` unsupported provider 同。缺陷：`:101`/`:128` 引「08 §4-7/§4-8」、`:3` 引「08」—— 该文档 ID 仓内不可解析（UNRESOLVED） | 通过（建议级） |
| 26 | `eng/tests/unit/p1_wcs_phot_test.cpp` (132) | 全 132 行 | `:62-79` 已知 PSF 解析总通量 `flux·2πσ²` 是**独立物理参照**，`:75` 判 20% 容差、`:76` 判背景、`:77` 判 SNR，真门；`:81-90`/`:109-118` 越界必 `valid=false` + 非空 reason。缺陷：`:42-54` `pix2sky`→`sky2pix` 往返自证（同对互逆且可能共错）；`:58-59` `pix2sky(crpix)≈(150,2)` 因 `cd11==cd22` 测不出轴交换；`:191`（另见 `p1_stars_test.cpp:191`）空容器越界 | 通过（建议级） |
| 27 | `eng/tests/validation/release02/fix_p2b_variance_oracle/patch_integrate.py` (129) | 全 129 行 + `module_adapters.cpp` 实测 | **它不是判据**，是改生产源码的一次性补丁施加器，被 `README.md:22` 归类为「Oracle」属分类错误。`:121-125` 锚点计数 `n != 1 → sys.exit(2)`，属**正确的 fail-closed**（正面）。实测 7 个 anchor 命中计数 `0,0,0,1,0,0,1` ⇒ 补丁**已应用过**，脚本已成死补丁且永远不可能成功；`:91-98` 非有限/非正方差 fail-closed 且明确「no clamp」 | 须修 |
| 28 | `eng/tests/validation/release02/q3_additive_truth/src/step8_alpha_final.py` (118) | 全 118 行 | `:107-111` `except Exception: print('FAIL',tag,e); continue` 把失败帧对**静默丢弃**，`:117` 仍无条件写 `alpha_final.json`、`:118` 仍 `print('WROTE')`、退出码恒 0 ⇒ **9 个 case 可掉到 0 个而产物与全量测量无法区分**（无计数、无完整性标记）；`:9`/`:14` 指向 `run/RELEASE-02/q3-additive-truth/`，实测该目录**已不存在**（真实模块在 `eng/tests/validation/release02/q3_additive_truth/src/`）⇒ 无法就地复跑 | 须修 |
| 29 | `eng/tests/cli/test_cli_single_install.py` (112) | 全 112 行 | `:40-43` 是**全仓最佳 fail-closed 范式**（CI 判红、仅本地 SkipTest），`:64-79` 扫全树只认 basename `acsd`、`:81-88` 退役 exe 正则、`:100-108` 禁源码/第三方（正面）。缺陷：`:37` 指向 `eng/ci/steps/linux_build_root_graph.sh` —— **实测 `ls eng/ci` 不存在**（已被 `e5f589a6` 物理删除），即该门唯一记载的产出方式已失效；`:95` 注释自陈「.sh/**.py** 可穿透执行旧 exe」但 `:97` 只查 `.sh/.bat/.cmd`，`.py` 漏网 | 须修 |
| 30 | `eng/tests/unit/p2_block_plan_test.cpp` (108) | 全 108 行 | `:18-35` 预算内全量、`:38-53`/`:85-100` 超预算缩块 + 峰值 ≤ 预算是真门；`:67-83` 重复调用确定性。缺陷：`:55` 注释承诺「零像素拒绝」，代码只实现零内存（`:61`），零像素用例不存在；`:34`/`:52`/`:99` 读生产自报 `estimated_peak_bytes`（恒 0 即假绿） | 通过（建议级） |
| 31 | `eng/tests/api/test_seam_metric_gate.py` (100) | 全 100 行 + 被驱 `syn008_seam_main.cpp:113-150` | `:33-45` `_require_prerequisites()` **显式 fail-closed**（`raise AssertionError` 而非 skip），并明写这是修「缺库 = 接缝已消除不可区分」的 P0（正面，全仓正确范式）；`:24 SIGMA=0.05` 与夹具 `kNoiseRms` 一致，门限标定是真的。缺陷在被驱（见下条与 §5 CE-4/CE-5） | 需修 |
| 32 | `eng/tests/unit/p1_psfw/p1psfw_tests_anea.cpp` (99) | 全 99 行 | **独立物理参照的样板**：`:19`/`:31-32` 用 `oracle::o_anea` + long double 显式 `ΣP²` 独立复算 A_NEA；`:66-75` 逐点独立复算 `P_eff = (ΣαaP)/(ΣαaP(centre))`；`:76-82` integral 归一 Σ=1；`:83-90` FWHM 夹逼 + 包含关系 + ee_r1<1；`:92-98` 三个 fail-closed 负例 | **通过** |
| 33 | `eng/tests/unit/core_module_test.cpp` (90) | 全 90 行 | `:42-49` duplicate 拒绝 + `size()==1`；`:51-64` 三类非法描述符（非命名空间 / `abi=rust` / 只有 input 端口）真负例；`:66-77` find/export_index 真门。简洁且无冗余 | **通过** |
| 34 | `eng/tests/unit/p3_rsmp/write_evidence.py` (88) | 全 88 行 | **fail-closed 做对**（`:26-28` 二进制缺失记 `rc=None` → `:81` 判假 → return 1；`:33` 归档缺失 → `:82` 判假），**不是**恒绿。缺陷：`:31` 读 `run/v6/p3-rsmp/mutation_summary.json`，而产出方 `run_mutations.py:16` 注释明写该路径已被 GC 掉、真实位置是 `run/FINAL-07/p3-mutation-open/p3-rsmp/` ⇒ **产出方搬了、读取方没搬**，变异这一半永久为 `None`；`:44` `standalone/CMakeLists.txt` 不存在；`:2` docstring 的 `run/quality/…` 与 `:11`/`:77` 的 `run/v6/…` **自相矛盾**（实测 `run/quality/` 下无 `p3-rsmp/`）；`:51 "does_not_include_production": True` 是**硬编码自证常量**，脚本从不校验；`:73-74` `checks_total` 格式敏感，摘要行一变即静默少计 | 须修 |
| 35 | `eng/tests/backend/test_mon003_synthetic.py` (81) | 全 81 行 | `:38-40` 缺构建产物即点名判红，**不把编译失败当 skip**（正面，`:36-37` 明引 AGENTS §5/§9）；`:66 assertIn("workers=2")` 的 `workers=%u` 来自夹具实测 `p.workers_used`，是真测量非硬编码。缺陷：`:30` `@unittest.skipUnless(shutil.which("g++"))` 整类跳过，与 `:38` 的 fail-closed 立场**自相矛盾**；`:67 assertNotIn("MULTI_FAIL", l)` 是真空断言（`MULTI_FAIL` 是独立行，被 `:63` 的 `startswith("MULTI ")` 过滤掉，结构上不可能出现）；`:77` 硬要求 `gate=single_threaded` 而夹具只要求 `d1 != Ok`，两端口径不一致，会产生「夹具过、门红」的假红 | 须修 |
| 36 | `eng/tests/validation/release02/c_delta_composition/mult_test1.py` (75) | 全 75 行 | `:31-49` 两两最小二乘 + `rms`/`medratio` 是真计算。缺陷：`:5` `run/RELEASE-02/L4-rebuild/upmfix_out` **不存在** ⇒ `:6` 必抛 IOError；`:74` 写 `run/RELEASE-02/c-delta/`（不存在且不 makedirs）⇒ 崩在最后一步，前面的分析全白做；`:35 if n<30: continue` 先筛子集，被丢弃的 pair 数**无任何输出**；`:23-26 tag()` 靠文件名 `split('_')` 取 `parts[1],parts[2]`，`:27` 的兜底 `'?'` 会在下游 `parts[1]` 处越界 | 须修 |
| 37 | `eng/tests/validation/release02/c_delta_composition/seam_vs_bg.py` (65) | 全 65 行 | `:10-27` `Jx/Jy` 用 `pinv` 去线性趋势是有方法学依据的。缺陷三处：`:40`/`:57` `bg=localbg(D['OLD'], ...)` —— 循环遍历 OLD 与 NEW，但**背景分母恒取 OLD**，NEW 的 `step/bg%` 是拿新图阶跃除旧图底噪，OLD/NEW 百分比**不可比**；`:36-39`/`:54-56` 在 ±20 px 内搜 `|阶跃|` 最大的位置再取该点，**报出的是 41 个候选上的 max**，非无偏估计；`:41`/`:58` `if np.isfinite(bg) and abs(bg)>1e11:` —— 这是把 C_k 场的无修正哨兵阈值误植到背景量上，背景是 ADU 量级（10²–10³），`|bg|>1e11` **永不成立** ⇒ `rows` 恒空 ⇒ `:43` 恒走 `print(lbl,'too few'); continue` ⇒ **脚本对任何真实数据都静默空转且退出 0** | **阻断** |
| 38 | `eng/tests/unit/p2_sky/CMakeLists.txt` (63) | 全 63 行 + `p2_sky_test.cpp:576-592` 模式 | `:4`/`:25`/`:49` 三个可执行都 `target_link_libraries ... acsd_phase2`，被测面是真生产库（非桩）；`:14`/`:34`/`:57` 三处 `foreach(case ...)` + `COMMAND <exe> ${case}`。缺陷：`p2_sky_test.cpp:576-592` 的 argv 分发是 12 个裸 `if`、**无 `else`、无 `unknown-case` 默认分支** ⇒ 任何拼错的 case 名都会注册出一个**跑零断言却返回 0 的绿测试**。现状核查：`:14` 的 12 个名与源文件 12 个函数**逐一对应，无当前假绿** ⇒ 潜伏隐患而非现网红灯 | 须修（潜伏） |
| 39 | `eng/tests/unit/p2_upm/CMakeLists.txt` (50) | 全 50 行 | `:18-31` 五个用例名与 `p2_upm_ma_test.cpp` 的组名一致；`:40-50` 四个 geo 诊断用例同样一致。结构简单无伏笔，但**依赖 argv 分发同样无默认分支**（与 #38 同源） | 通过 |
| 40 | `eng/tests/validation/release02/c_delta_composition/seam_fixed.py` (47) | 全 47 行 | `:20-32` 固定坐标复现。缺陷三处：`:33-35` `bgx/bgy1/bgy2` **只从 `D['OLD']` 算一次**，`:37-43` 却对 `('OLD','NEW')` 两者都除以它 ⇒ 口径错配；`:24`/`:30` `l=np.median(d[y,pos-w:pos])`、`r=np.median(d[y,pos:pos+w])` **左右两窗共享边界像素 `pos`**，真接缝跳变的一半被同一像素同时计入两侧，系统性压低所测 step —— 这是对 §2 接缝结论的**直接测量偏置**；`:46` 把覆盖度**二值化** `covref>0.5`，丢弃分数覆盖；`:44-46` `os.path.join(...) + '/'` 拼串且**无 makedirs** ⇒ 目录不存在即 FileNotFoundError | **阻断** |
| 41 | `eng/tests/backend/hips_properties_probe_main.cpp` (45) | 全 45 行 | **纯探针、零判据零阈值**：`:19`/`:31`/`:40` 三个符号全部解析到生产 `lib/algorithms/coverage/hips_properties.h:29/38/43`，**不存在本地 stub**；`:29` open 失败是显式 `FAIL` + `rc=1`，非静默通过；有活消费者 `test_hips_properties.py`。**本片应作正例参照** | **通过** |
| 42 | `eng/tests/validation/release02/c_delta_composition/map_test2.py` (39) | 全 39 行 | `:10-12` 打印「model frames == corrected frames order?」与 same 位置数。缺陷：`:3` `sys.path.insert(0,'run/RELEASE-02/trail')` → **该目录不存在**，`layout` 模块全仓无同名文件 ⇒ 连 import 都过不去；`:10-12` **帧序不一致只打印不失败**（正是它要防的缺陷），`:31-33` 仍按 `FI`/`CI` 索引取数，错序会静默产出错误对照表；`:34` `if not np.isfinite(cp): continue` 静默跳过且不计数 | 须修 |
| 43 | `eng/tests/validation/release02/c_delta_composition/map_test.py` (30) | 全 30 行 | 三种索引方案并排打印。缺陷：`:3` 同样悬空 `run/RELEASE-02/trail`；`:27-29` 三方案交叉验算**完全无断言**（纯打印式排查脚本），若三者本应相等而实际不等，脚本仍「成功」；`:24 obs[:4000:137]` 静默子集采样，无种子、无样本量声明 | 须修 |
| 44 | `eng/tests/validation/release02/phot_verify/probe_pairs.py` (28) | 全 28 行 | 依赖 `pair_ratio.py` **确实存在**（同目录）。缺陷：`:16-17` 无匹配即 `continue` 静默丢 pair，`:28` 仍写出 `pair_probe.json`，**无计数、无退出码**（4 个 pair 掉到 0 也照写）；`:28` 输出写 **CWD** 而非脚本目录 ⇒ 从别处调用会写错位置，并可能**覆盖同目录归档** | 建议 |
| 45 | `eng/tests/validation/release02/phot_verify/make_summary.py` (21) | 全 21 行 | **本片唯一完全干净的证据写出器**：`:3-4` 直接 `json.load` 两个**别的脚本**产出的 JSON，本脚本只做纯量纲换算（`-2.5*log10` / `2.5*x`）；**无 try/except、无 `if not exists`、无 `return` 提前退出** ⇒ 任何输入缺失即抛异常硬失败。**不自算自己，也不是恒绿** | **通过** |
| 46 | `eng/tests/config/fixtures/negative/normalize_perframe_inputs.phase_config.json` (16) | 全 16 行 + 全部消费者 | 形如已退役的三段式（`:2` `phase_name` + `:3` `config` + `:7-15` 逐帧 `inputs[]`），作为**守门夹具**正确（证明退役形态被拒，非活调用者）。消费方 `gate_assertions.py:27`、`test_cfg003_multiblock.py:76`、`test_cfg001_negative.py:176` 均存在，且 `:78-91` 既要求错误非空、又要求错误消息**指名** `phase_name` 与 `inputs`，并反查 schema 顶层不得再声明这些键 ⇒ **双向锁、非退化** | **通过** |
| 47 | `eng/tests/config/fixtures/negative/normalize_block_unknown_key.phase_config.json` (14) | 全 14 行 + 全部消费者 | `:11` 唯一未知键 `unknown_knob` 承担全部触发责任，其余键（`name`/`input_lights`/`output_dir`/`master_bias`）合法 ⇒ 归因无歧义。消费方 `test_cfg003_multiblock.py:70-72` 要求错误非空**且**消息**指名** `unknown_knob`，不是「有错即可」 | **通过** |

**小计**：阻断 7 份、需修 19 份、建议/通过 21 份。

---

## 4. 发现清单

### 4.1 阻断（7 条）

> 计数口径说明：本节「条」是**独立缺陷条目数**。涉及「门」的数量时，我在各条内分别写明它属于**门实例 / 去重门 / 整改分母**哪一层。**本片不产出「门实例数」口径**（本片是测试文件，不承载分母台账）；凡我说「X 条门」，指的是本片成员文件内的**去重门条目**，不含跨片重复。

**BLK-1｜`p3_rsmp_oracle.h:227-257` 往返自证：`Var·W ≡ 1` 对任意 S/C_d 恒成立**
`rel_err_out = |var·a·denom − 1|`（`:255`），`denom = a·qᵀπ`（`:232`），`q = C_y⁻¹π`（`:231`）。因 `C_y q = π` 由构造成立 ⇒ `qᵀC_y q = qᵀπ` ⇒ `Var·W = 1` 对**任意** `build_S`/`build_R`/`solve`/`cholesky_lower` 恒成立。消费方 `p3_rsmp_oracle_test.cpp:119-121` 判 `rel < 0.01`，只度量 MC 噪声。
复现：`sed -n '227,257p' eng/tests/unit/p3_rsmp/p3_rsmp_oracle.h`；`sed -n '117,122p' eng/tests/unit/p3_rsmp/p3_rsmp_oracle_test.cpp`
**去重门**：1（该 oracle 的 Var/W 交叉校验门）。

**BLK-2｜`adapter_test.cpp:723-729` 代数恒真式：D11 用生产定义式当期望量**
生产 `noise_model.cpp:1041-1042` `sigma=sqrt(var)`、`ivar=1.0/var`；测试断言 `|ivar−1/var|≤1e-15|ivar|` 与 `|sigma²−var|≤1e-15·var`。对任意 `var` 精确成立。叠加 `adapter_entry_impl.cpp:6` 直接 include 生产 TU ⇒ D1–D8 的 direct-vs-plugin 也是**同实现对拍**。
复现：`sed -n '720,732p' eng/tests/unit/p1_noise/adapter_test.cpp`；`sed -n '1039,1043p' lib/algorithms/noise_snr/cpp/src/noise_model.cpp`；`sed -n '6p' eng/tests/unit/p1_noise/adapter_entry_impl.cpp`
**去重门**：3（D11 的 ivar、sigma² 两条 + D1–D8 逐位对拍组，合并计一条根因）。

**BLK-3｜`p1_integrate_test.cpp:261` 同一恒等式的第二处**
`CHECK_NEAR(v.flux_variance * v.w_info, 1.0, 1e-9)`，而 `information_weight.cpp:313` 定义 `out.var_f = 1.0/out.sum_w`。对任意 `W_info` 恒真；生产消费端 `phase2_integrate.cpp:346` 重复同一恒等式。
复现：`sed -n '261p' eng/tests/integration/p1_integrate/p1_integrate_test.cpp`；`sed -n '313p' lib/algorithms/noise_snr/cpp/src/information_weight.cpp`；`sed -n '346p' lib/algorithms/integration/phase2_integrate/src/phase2_integrate.cpp`
**去重门**：1。

**BLK-4｜`p3_rsmp_oracle.h:200-204` + `registry:195` 的 `max_weight_sum_dev` 恒真链**
Oracle 把 `w00..w11` 硬编码为 0.25 再算 `|(0.25×4)−1| ≡ 0`；生产 `registry:195` 同字段也是硬编码 `0.0`；`p3_rsmp_oracle_test.cpp:49` 交叉核对的是 `0 == 0`。**两侧都是结构常数，一个真实权重都没算。** 另 `p3_rsmp_oracle.h:180` `boundary_fail_closed = true` 在 `run_kernel_oracle()` 内从未赋值，`oracle_test.cpp:51` 断言其为真 ⇒ 另一条恒真。
复现：`sed -n '200,204p' eng/tests/unit/p3_rsmp/p3_rsmp_oracle.h`；`sed -n '195p' lib/algorithms/resample/p3_rsmp_kernel_registry.cpp`；`sed -n '49p;51p' eng/tests/unit/p3_rsmp/p3_rsmp_oracle_test.cpp`
**去重门**：2。

**BLK-5｜`release02/README.md:88` 头条证据是恒真式（往返自证 + 逆函数构造）**
`p1_apply_oracle.cpp:112` 测试自己喂 `k_photo = 1.0/g[k]` ⇒ 比值恒 1；`:101` 负例断言展开为 `check(0 < 1e-12)`。归档 `oracle_output.txt:9` 的 `|中位-1| = 0.000e+00` 是位精确零 = 恒等式指纹。
复现：`sed -n '88p;101p;112p;126p' eng/tests/validation/release02/fix_p1_photometry_apply/p1_apply_oracle.cpp`；`sed -n '88p' eng/tests/validation/release02/README.md`
**去重门**：1。

**BLK-6｜`seam_vs_bg.py` 三重失效（分母错配 / 极值偏倚 / 哨兵致静默空转）**
`:40`/`:57` 分母恒取 `D['OLD']`；`:36-39`/`:54-56` 取 |阶跃| 最大点；`:41`/`:58` `abs(bg)>1e11` 对 ADU 背景**永不成立** ⇒ `rows` 恒空 ⇒ `:43` 恒 `too few` ⇒ **退出 0 且无任何输出**。`README.md §2.1` 的表格依赖本目录脚本却未加任何限定。
复现：`sed -n '40,43p;57,58p' eng/tests/validation/release02/c_delta_composition/seam_vs_bg.py`
**去重门**：1（该脚本的接缝统计门）。

**BLK-7｜`seam_fixed.py:24/30/33-35` 测量偏倚 + 分母错配**
左右窗**共享边界像素 `pos`**，真阶跃的一半被同一像素计入两侧；`bgx/bgy1/bgy2` 只从 OLD 算一次却对 NEW 也用。
复现：`sed -n '24p;30p;33,35p;37,43p' eng/tests/validation/release02/c_delta_composition/seam_fixed.py`
**去重门**：1。

> 另有两条在本片成员文件内、但更接近「阻断」边界而我归入须修，见 §4.2 FIX-1（`test_p2001_parallel_sampler.py:153` 漏比 P2-001 科学量）与 FIX-2（`p1wcs/CMakeLists.txt:185-190` 退役门仍被正本表标 Y）。**若负责人认为「正本表指向不存在的 ctest」应升级为阻断，FIX-2 可直接上调**——我按「文档失真」而非「判据说谎」归须修。

### 4.2 须修（12 条）

**FIX-1｜`test_p2001_parallel_sampler.py:153` 漏比 `overlap_controls`（P2-001 的科学量本身）**
`_parse_sample:114-120` 抓到 `obs` 与 `overlap_controls` 两个数，`:153` 只比 `o1[0]/oN[0]`。仓内 `lib/algorithms/coverage/tests/sampler_parallel_consistency_test.cpp:256,339` 早已在比后者。且 `:128`/`:139` 的 `cpu_workers` 被生产显式覆盖（`module_adapters.cpp:9608-9612`），两次跑 worker 数相同 ⇒ `test_03` 名不副实。
复现：`sed -n '114,120p;128p;139p;153p' eng/tests/backend/test_p2001_parallel_sampler.py`

**FIX-2｜`p1wcs/CMakeLists.txt:185-190` 退役门仍被三处文档宣称在跑且已过**
`p1wcs_closure_metric_gate` 注册已随 G08-01 移除（实测 `grep` 零 CMake 命中），但 `docs/science/algorithms/GATES_AND_TOLERANCES.md:67` 在**门禁正本表**把它登记为可执行门且末列状态 **Y**，`docs/science/ASTROMETRY.md:281` 写「过」，`lib/algorithms/platesolve/memory.md:110-111` 写「可执行门」。
复现：`grep -rn p1wcs_closure_metric_gate docs/ lib/ eng/ 2>/dev/null`

**FIX-3｜`p2_rejection_test.cpp:157-166` 纯代数恒真 + `:143-155` 死 fixture + `:2-4` 头注与断言矛盾**
见 §3 #6。复现：`sed -n '143,166p' eng/tests/unit/p2_rejection_test.cpp`

**FIX-4｜`p3_rsmp_oracle.h:168` 与 `write_evidence.py:2/44` 的悬空引用；`p1psfw_tests_record.cpp:2`、`p1_qf_oracle.cpp:14` 的悬空引用**
`ALG-P3-001_KERNEL_REGISTRY` 全树零命中（生产实际用 `ALG-P3-001-KERNEL-ORACLE/...`）；`docs/engineering/v6/` 整目录已删；`eng/tests/unit/p3_rsmp/standalone/CMakeLists.txt` 不存在；`eng/tests/validation/release02/fix_p1_photometry_apply/build_qf_oracle.sh` 全仓零命中。
复现：`grep -rln "ALG-P3-001_KERNEL_REGISTRY" docs run/GOVERN-08 2>/dev/null`（无输出）；`ls eng/tests/unit/p3_rsmp/standalone`（No such file）

**FIX-5｜`write_evidence.py:31` 变异归档路径失效（产出方搬走、读取方没搬）**
`run_mutations.py:16` 注释明写旧路径 `run/v6/p3-rsmp/` 已被 `eng/tools/run_gc.py` GC 掉、新位置 `run/FINAL-07/p3-mutation-open/p3-rsmp/`。`write_evidence.py` 仍读旧路径 ⇒ `ev["mutations"] = null`、`:82` 恒假 ⇒ 该门**恒红**（方向正确但恒红本身把真缺陷藏进红灯）。
复现：`sed -n '14,20p' eng/tests/unit/p3_rsmp/run_mutations.py`；`sed -n '11p;31p' eng/tests/unit/p3_rsmp/write_evidence.py`

**FIX-6｜恒绿伪装四处**
- `test_p3_output.py:407-408` root 下裸 `skipTest`，无 CI 分支（对照 `test_cli_single_install.py:40-42` 正例）
- `test_mon003_synthetic.py:30` 整类 `skipUnless(g++)`
- `test_p2001_parallel_sampler.py:108-109,124-125` 无 CLI 二进制即 `skipTest`
- `step8_alpha_final.py:107-111` 吞异常后仍写残缺产物、`:118` 仍报「WROTE」、退出 0
复现：`grep -n "skipTest\|skipUnless" eng/tests/backend/test_p3_output.py eng/tests/backend/test_mon003_synthetic.py eng/tests/backend/test_p2001_parallel_sampler.py`

**FIX-7｜悬空 `eng/ci` 引用（唯一记载的产出方式已不存在）**
`test_cli_single_install.py:37` 指向 `eng/ci/steps/linux_build_root_graph.sh`；`eng/ci` 已被 `e5f589a6`「G08-01 物理删除旧门禁与 CI」整体删除。`cli_test_hygiene.py:4` 同样引用。
复现：`ls eng/ci`（No such file）；`git -c core.quotepath=false log --oneline -1 e5f589a6`

**FIX-8｜`mem_wire_test.cpp:142-148` 桩使「1/N worker 逐位一致」构造上不可翻红；`:435-436`/`:163` 弱门**
桩输出是 `ctx.current_node()` 的 FNV 哈希，按构造与并发度/worker 数无关。文件头 `:16-18` 的措辞「数值结果**逐位一致**」高于实际证明力。
复现：`sed -n '142,148p;378p;407p' eng/tests/unit/mem_wire_test.cpp`

**FIX-9｜`p1_stars_test.cpp:147-150/177-180/127-128` 三段在守卫内整段跳过**
探测返 0 源或 1 源时，边缘星位、tie-breaker、高本底不过标三段全部静默通过。
复现：`sed -n '147,150p;177,180p' eng/tests/unit/p1_stars_test.cpp`

**FIX-10｜`export_stream_test.cpp:251` 取消门左支恒真 + `:183/206/222` 读生产自报指标**
复现：`sed -n '251p' eng/tests/unit/export_stream_test.cpp`

**FIX-11｜`test_bench_cli.py:87` 用下限代替等值**
`assertGreaterEqual(len(d["kernels"]), 1)`，而生产 `profile_gen_v2.cpp:68-81` 是固定 12 条、CLI `commands.cpp:2639` 硬编码 `"full"`。docstring `:9-13` 自称保留「kernel 规格」断言。
复现：`sed -n '87p' eng/tests/cli/test_bench_cli.py`；`sed -n '67,81p' lib/infrastructure/benchmark/backend_host/profile_gen_v2.cpp`

**FIX-12｜`p1wcs/CMakeLists.txt:155-158` 标签派生漏 `p1wcs_clip_consistency`**
`:210` 注册该测试，`:211-214` 无 `LABELS` ⇒ 任何 `ctest -L p1wcs` 派生门都跳过它。同文件 `:36-37` 头注 CTest 清单也漏了本文件自己注册的 4 个。
复现：`sed -n '155,162p;210,214p' eng/tests/unit/p1wcs/CMakeLists.txt`

### 4.3 建议（15 条）

1. `adapter_test.cpp:325` `if (od != op && g_fail == 0)` 永不触发（`CHECK:324` 已把 `g_fail` 递增）⇒ **主对拍门失败时的 diff 诊断被永久抑制**。复现：`sed -n '323,328p' eng/tests/unit/p1_noise/adapter_test.cpp`
2. `p3wcs_tests_apbp.cpp:629-642` 导出到 `run/p1wcs_wcs003/`，该目录**既不在 git 也不在磁盘**（实测），而 `p1wcs_astropy_cross` 读同一路径 ⇒ 自愈式归档；任何引用该路径为证据的结论不可复现。
3. `p3002_uncertainty_test.cpp:373-374` `if (man.contains(...))` 才 CHECK，键缺失整段静默通过（fail-open），与同文件 `:560-562` 强制要求该键自相矛盾。
4. `p3002_uncertainty_test.cpp:366/393/397/627-628` 注释的 HDU 数（3 / 5）与活断言（2 / 4）相反；`:317-319` 提到的行号 `:367/:393` 已陈旧。
5. `p2_sky/CMakeLists.txt:14/34/57` + `p2_sky_test.cpp:576-592` argv 分发无 default 分支 ⇒ 拼错 case 名 = 零断言却返回 0 的绿测试（**当前 12/12 名对上，无现网红灯**，属潜伏）。`p2_upm/CMakeLists.txt` 同源。
6. `provider_avx2_so_load_test.c:116-123` `CHECK(q != NULL)` 不中止，`:123` 随即无条件调 `q(...)` ⇒ dlsym 失败即空函数指针调用（UB，吞诊断）。
7. `p3_assembly_test.cpp:71-73` `CHECK(ps != nullptr)` 后无提前返回；`:93` 注释「> 100」实为 `>= 99.5f`，且 `:58` 注入的 500.0 峰值全程未验证。
8. `test_p3_output.py:132-142` `_fill_ref` 从未被调用（死代码），但 `:10-11` 暗示 Python 侧有独立参照，易误导读者；`:154` 注释「= 1536」实为 768；`:178` `"HIS"` 是 `HISTORY` 的子串 ⇒ 恒真命中。
9. `test_p3_projection_oracle.py:303-305` oracle 输入坐标由生产 `desc[0..5]` 算出 ⇒ CD/CRPIX 面往返自证（**全仓另有 `p3_projection_test.cpp:370-392` 独立钉 CD 手性，故非全仓洞**）；`:244` 解析出的 `min_scale`/`c_env` 从未使用。
10. `test_mon003_synthetic.py:67` `assertNotIn("MULTI_FAIL", l)` 是真空断言（该串是独立行，被 `:63` 的过滤器排除）；`:77` 硬要求 `gate=single_threaded` 而夹具只要求 `d1 != Ok` ⇒ 会产生「夹具过、门红」的假红。
11. `mon001_gate_test.cpp:276` `peak_active >= peak0` 是进程级单调高水位，构造上恒真（有效的是 `:277 >= 4`）；`:279-281` 注释称「净值回零」但无断言；`:3-4` 头注的分母口径被 `:244-255` 推翻。
12. `cpu004_routing_test.cpp:101/128` 引「08 §4-7/§4-8」、`:3` 引「08」，该文档 ID 仓内不可解析；`mem_wire_test.cpp:417` 引 `runtime_resources.json` 的 95 我未核到取值（均列 UNRESOLVED）。
13. `write_evidence.py:51` `"does_not_include_production": True` 是硬编码自证常量（人工核实该断言为真，但脚本不校验）；`:40-41` 硬编码 commit + head_note 违反 AGENTS §5「无版本号/历史叙事」。
14. 整棵 `release02` 的复跑路径已断：`run/RELEASE-02/L4-rebuild`、`trail`、`q3-additive-truth`、`c-delta` **均不存在**（实测 `run/RELEASE-02/` 只剩 `FIX-A/ fix-p2b/ weight-chain/`），而 `README.md:161-170` 给的是**相反**的保证。
15. `map_test2.py:10-12` 帧序不一致只打印不失败；`map_test.py:27-29` 三方案交叉验算无断言；`mult_test1.py:35` 先筛子集不输出丢弃数；`probe_pairs.py:28` 输出写 CWD 且可能覆盖归档。

### 4.4 伪引核查（独立成节，因为主项常无引号）

按「成对引号」**和**「裸从句摆在冒号后」两种形态都扫了。逐条给「引用文本 → 被引位置 → 是否逐字存在 → 判定」。

| # | 引用文本（出处） | 被引位置 | 实际是否存在 | 判定 |
|---|---|---|---|---|
| 1 | `I_photo = k_photo · I_cal`（`README.md:33`，标注出自 `photometry_apply.h`） | `photometry_apply.h` | **逐字存在**（`:11`/`:22`） | **通过** |
| 2 | `k_photo = scale = 10^(−location)`（`README.md:34`，同上） | `photometry_apply.h` | **不存在**（全 115 行无 `10^(-location)`/无 `scale`/`location` 语义） | **伪引** |
| 3 | `location = Tukey-IRLS(c=4.685) …`（`README.md:35`，同上） | `photometry_apply.h` | **归属错**：该头无 `4.685`/`Tukey`/`IRLS`/`F_syn`；`4.685` 实际在 `star_matcher.cpp:23` | **伪引（归属错）** |
| 4 | `F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ`（`README.md:36`，同上） | `photometry_apply.h` | **不存在** | **伪引** |
| 5 | 「`k_photo` 是单一全局标量：无空间项、无颜色项」（`README.md:39`，**裸从句**） | 生产 | **被证伪**：`photometry_apply.h:43-45,110` 实现 `m(x,y)` 空间场，`module_adapters.cpp:6281-6283` 活调用 | **伪引（结论级）** |
| 6 | 「`apply_photometry` 零生产调用者」（`README.md:82`，**裸从句**） | 生产树 | **被证伪**：`module_adapters.cpp:6283` 活调用；`CALIBRATION_ALGORITHMS.md:296`「有生产调用方」 | **伪引（结论级）** |
| 7 | 「全帧统一相乘（`photometry_apply.cpp:63` 单一 `for` 循环）」（`README.md:40`，**裸从句**） | `photometry_apply.cpp` | **行号漂移**：`:63` 是注释行，`for` 在 **`:67`** | **须修** |
| 8 | 「因 `run/` 未被删除，绝对路径迁入后仍有效」（`README.md:170`，**裸从句**） | 文件系统 | **被证伪**：`run/RELEASE-02/` 只剩 3 个目录，被引 4 个全无 | **伪引** |
| 9 | 「确认 `run/RELEASE-02/L4-rebuild/` 存在（由 `run_l4.sh` 生成）」（`README.md:163`） | 文件系统 | **不存在**；`run_l4.sh` 亦未见 | **伪引** |
| 10 | `ALG-P3-001_KERNEL_REGISTRY §3`（`p3_rsmp_oracle.h:168`，**裸从句**） | `docs/` + `run/GOVERN-08/` | **全树零命中**；生产实际用 `ALG-P3-001-KERNEL-ORACLE/...`（`registry:167,191,219`） | **伪引（ID 不存在）** |
| 11 | 「原对齐 `docs/engineering/v6/ORACLE_AND_ZERO_CASE_POLICY.md §1`」（`p3_rsmp_oracle.h:4`、`p1psfw_tests_record.cpp:2`） | 文件系统 | 确认已删（`docs/engineering/v6/` 整个目录不存在）。**自陈属实**，但按规范仍须换活指针 | **自陈属实，仍属悬空引用** |
| 12 | `standalone_cmake: eng/tests/unit/p3_rsmp/standalone/CMakeLists.txt`（`write_evidence.py:44`，**裸从句**） | 文件系统 | **不存在** | **伪引** |
| 13 | 「→ `run/quality/p3-rsmp/evidence.json`」（`write_evidence.py:2` docstring，**裸从句**） | 文件系统 | **不存在**，且与同文件 `:11`/`:77` 实际写入的 `run/v6/p3-rsmp/evidence.json` **自相矛盾** | **伪引（自相矛盾）** |
| 14 | 「编译：见 `build_qf_oracle.sh`」（`p1_qf_oracle.cpp:14`，**裸从句**） | 文件系统 | **全仓零命中** | **伪引** |
| 15 | `docs/engineering/v6/NEGATIVE_MUTATION_CATALOG.md`（`p1psfw_tests_record.cpp:2`） | 文件系统 | 目录已删，且历史真实路径是 `docs/validation/v6/NEGATIVE_MUTATION_CATALOG.md` | **路径错 + 悬空** |
| 16 | 「08 §4-7/§4-8」「08 §4」（`cpu004_routing_test.cpp:3,101,128`；`test_bench_cli.py:189`） | `docs/` | **仓内不可解析**（我未核到同名正本） | **UNRESOLVED，倾向伪引** |
| 17 | 「真实 L4 控制级朴素/正确中位 **1.2591**、最大 **31.99**」（`README.md:150`） | `fix_p2b_variance_oracle/realdata_evidence.txt` | **逐字存在**（`:13 median=1.2591 … max=31.9870`，README 的 31.99 是四舍五入）。**但**该文件 `:22-23` 自陈 L4 数据早于修复提交，README 未带此 staleness 限定 | **通过（需补限定）** |
| 18 | 「N=8 高估 **1.2857×**」（`README.md:140`） | 算术 | `(1+1/8)/(1−1/8) = 9/7 = 1.28571…` | **通过** |
| 19 | 「权重序 A>C>B>D → A>B>C>D；Kendall τ=0.667；6.76→1.69」（`README.md:146-149`） | `q2_snr_smoothness/synth_results_v3.json` | **逐字段吻合** | **通过** |
| 20 | 「（2026-09-09）：owner 裁决 1 选 B」（`p1wcs_tests_apbp.cpp:4`） | — | 含日期，违反 AGENTS §5「正文无日期」 | **建议** |

---

## 5. 你主动构造的反例

> 全部为**纸面代数/代码追踪**反例（不改仓内文件、不编译、不跑测试）。凡结论由子代理提出而我独立复核的，标注「复核」。

| # | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| **CE-1** | 令 `noise_model.cpp:1040` 的 `variance_bg_global = V`，其中 `V` 取**任意**值（0.25×、100×、用错分位数、误取帧方差）。由 `:1041` `sigma=√V`、`:1042` `ivar=1/V`。代入 `adapter_test.cpp:723-728`：`\|ivar−1/var\| = \|1/V−1/V\| = 0`，`\|sigma²−var\| = \|√V·√V−V\| = 0` | 推翻「D11 能检出背景方差估计错误」 | **推翻成功。** 且 V 可取任意正数 ⇒ 该门对 `variance_bg_global` 的**全部可能取值**都恒绿，**无反例能使其变红** |
| **CE-2** | 令 `p3_rsmp_oracle.h` 的 `build_S` 完全写错（例如 `area/omega_in` 漏成 `area`），其余一字不改。因 `C_y = S·C_d·Sᵀ` 与 `f = S·d` 用的是**同一个错 S**，而 `q` 由 `C_y q = π` 求解 ⇒ `qᵀC_y q = qᵀπ` 仍成立 ⇒ `Var·W ≡ 1` | 推翻「oracle 的 Var/W 交叉校验能发现算子算错」 | **推翻成功。** 该恒等式对**任意 S、任意 C_d、任意 a** 成立，结构上不可能变红 |
| **CE-3** | 令 `information_weight.cpp:313` 的 `out.sum_w` 取任意值（把 `Σ P²/σ²` 误写成 `N`、忘乘 `a`、等）。由 `:313` `var_f = 1/sum_w`。代入 `p1_integrate_test.cpp:261`：`var_f·W = (1/w)·w = 1` | 推翻「`Var(F̂)=1/W_info` 能检出 `W_info` 算错」 | **推翻成功。** 同一恒等式在 `phase2_integrate.cpp:346` 再重复一次，两层都不可证伪 |
| **CE-4（复核）** | 把 `seam_vs_bg.py:6` 的 `P['NEW']` 换成一条亮度差 2× 的渲染。`:40`/`:57` 的 `bg` 恒取 `D['OLD']` ⇒ NEW 的 `step/bg%` 随 **OLD 的电平**反向缩放，而 NEW 自身 step 未变 | 推翻「OLD/NEW 的 `step/bg%` 可比」 | **推翻成功** |
| **CE-5（本人独立发现，修正子代理判词）** | 取任何真实背景（ADU 量级 10²–10³）。`seam_vs_bg.py:41`/`:58` 的条件是 `if np.isfinite(bg) and abs(bg)>1e11: rows.append(...)` ⇒ `\|bg\|>1e11` **永不成立** ⇒ `rows` 恒空 ⇒ `:43` 恒 `print(lbl,'too few'); continue` | 推翻「该脚本能产出 §2.1 的接缝统计」 | **推翻成功，且比子代理判词更重。** 子代理称该条件「恒真、空操作门」；**我核对源码后判定方向相反** —— 它不是空操作，而是**拒绝一切样本的过滤器**，使脚本对任何真实数据静默空转、退出码 0、只打印 `too few`。§2.1 的百分比因此在结构上无法由本脚本产出 |
| **CE-6（复核）** | 令 `p3_rsmp_oracle.h:180` `boundary_fail_closed` 保持默认 `true`（`run_kernel_oracle():183-220` 内**无任何赋值**）。消费方 `p3_rsmp_oracle_test.cpp:51` `P3_CHECK(ko.boundary_fail_closed)` 展开为 `P3_CHECK(true)` | 推翻「该断言有判别力」 | **推翻成功。** 它不读生产、不读输入、不读时间，改代码永远不会红 |
| **CE-7（复核）** | `p2_rejection_test.cpp:160-164`：`int reason_codes[4] = {0,0,0,0};` 相加后 `CHECK(sum==0)` ⇒ 编译期即 `CHECK(0+0+0+0 == 0)` | 推翻「计数守恒不变量被验证」 | **推翻成功。** 注释宣称的 `accepted+low+high+underdetermined == n_samples` 从未执行（`n=100` 被 `(void)n` 丢弃） |
| **CE-8（复核）** | 把 `p3_projection.cpp` 的 CD 手性做 180° 翻转（`east_left` 写成 `east_right`），其余一字不改；再按 `test_p3_projection_oracle.py:303-305` 的原样把该错 CD 喂给 oracle | 推翻「`:310-313` 以 <1e-9 deg 独立锁死生产投影」 | **推翻成功（在本文件内）。** 根因：oracle 输入坐标由生产 `desc[0..5]` 算出，正向/反向/往返三处共用同一块「底片」，CD 误差完全对消。**但全仓未推翻** —— `p3_projection_test.cpp:370-392` 独立断言 CD 符号与手性、`test_phase3_reproject_oracle.py:187-188` 断言 CD1_1/CD2_2 ⇒ 降级为「本文件覆盖洞」，非全仓洞 |
| **CE-9（复核）** | 检索 `p1wcs_closure_metric_gate` 全仓并逐条核 `eng/tests/unit/p1wcs/CMakeLists.txt` 的 10 个 `add_test` | 推翻「该 ctest 是仓内在跑的门」 | **推翻成功。** 零 CMake 命中，3 处 `docs/`+`lib/` 文档宣称，其中门禁正本表状态列为 **Y** |
| **CE-10（复核）** | 检索 `ACSD_P3002_FAULT` 的读者（`lib/phase3_session/` 下 `getenv` 零命中）+ 核 `eng/tests/unit/CMakeLists.txt` 的 fault 变体注册 | 推翻「`p3002_uncertainty_test.cpp:569-636` 的三个 `test_fault_*` 会真的注入缺陷」 | **推翻成功。** 生产零读者、fault ctest 零注册。更危险的是：若有人照 CMakeLists 注释补一个 `WILL_FAIL` 的 fault ctest，则**正确实现必然失败**（`var_out=0.25≠0`、`hdus=4≠2`），harness 会把「失败」读成「注入有效」⇒ **把绿灯洗成绿灯的假绿**。当前未注册，属埋雷 |
| **CE-11** | 检索 `run/v6/p3-rsmp/mutation_summary.json` 是否存在，并核 `run_mutations.py:16` 的搬迁注释 | 推翻「`write_evidence.py` 的变异这一半在跑」 | **推翻成功（复核）。** 产出方已搬至 `run/FINAL-07/p3-mutation-open/p3-rsmp/`，读取方没搬 ⇒ `ev["mutations"]=null`、`:82` 恒假 ⇒ 该门**恒红**（恒红把真缺陷藏进红灯） |
| **CE-12** | **我没能打成的一条**：把 `p2_sky/CMakeLists.txt:14` 的 12 个 case 名逐一比对 `p2_sky_test.cpp:578-589` 的 12 个函数 | 推翻「每个 `p2_sky_<case>` 都真跑了断言」 | **推翻失败。** 12/12 全部对应，**无当前假绿**。harness 确无 unknown-case 默认分支，故降级为潜伏项报「须修」而非「阻断」。**如实记录这一条我没打成。** |

**额外算术复核（子代理提出，我独立验算）**：
`p3_rsmp_oracle.h:218` 定义 `zero_fill_error = 4.0·0.25·F(70,70)`，`F = exp(−r²/(2s²))`，`s=3`、峰在 `(70.5,70.5)` ⇒ `F(70,70) = exp(−0.5/18)`。
我实测：`python3 -c "import math;print(4*0.25*math.exp(-0.5/18))"` → **`0.9726044771163483`**。
生产归档实测：`p3_rsmp_kernel_registry.cpp:197` `k.oracle.zero_fill_error = 1.3223;`（我亲眼读到该行）。
**差 −0.3497，比值 1.3595 ⇒ 36% 偏差。**
为何没被抓住：`p3_rsmp_oracle_test.cpp:53` 只写 `P3_CHECK(bil->oracle.zero_fill_error > 0.5)` —— **只判下界，从不与 oracle 复算值比对**；而同文件 `:48`/`:49` 对 `max_interp_err`/`analytic_bound`/`err_over_bound`/`max_weight_sum_dev` **全都做了等值比对** ⇒ `zero_fill_error` 是六个字段里**唯一漏掉等值比对的那个**，36% 错误在 oracle 端与生产端同时存活。

**另核实一条恒绿（生产默认核）**：`p3_rsmp_kernel_registry.cpp:218-226` 为 `bilinear_area_overlap_exact`（`:226 production_science_default = true`，**生产科学默认**）声明 `max_interp_err = 0.0; analytic_bound = 0.0; err_over_bound = 1.0;`（注释自陈「0/0 → 约定为 ≤ 上界」）。注册门 `registry:99` 判 `!(max_interp_err <= analytic_bound)` 即 `!(0.0 <= 0.0)` ⇒ **两个逐位相同的量比大小，永远绿**，该核的「误差界证据」结构上不可能承载任何信息。

---

## 6. 盲复算

**方法**：对既有判定（片清单的 `逐份判定-权威版.csv`、以及子代理的三份报告）先**遮住**，只凭原文 + 权威原件独立取证，得出结论后再比对。**口径**：判「一致 / 偏松 / 偏严」——「偏松」= 我判得更严重（既有判定放过了），「偏严」= 我判得更轻（既有判定过严）。

| 复算对象 | 盲取证结论 | 与既有判定比 | 判定 |
|---|---|---|---|
| `adapter_test.cpp` | D1–D8 是同 TU 对拍（`adapter_entry_impl.cpp:6`）；D11 两条是自洽式断言 | 子代理 07eca2b4 判「通过，仅建议级」，并称 `:723-729` 的 `ivar=1/var` 是「测试侧手算的独立物理参照，非自证」 | **我判更重（偏松）**。`noise_model.cpp:1042` 就是 `ivar := 1/var`，断言写的是 `\|ivar−1/var\|`，这是恒等式不是参照。**否决子代理这一条。** |
| `p1_integrate_test.cpp:261` | `information_weight.cpp:313` 定义 `var_f = 1/sum_w` ⇒ 断言恒真 | 子代理 07eca2b4 判「自洽但非恒真」 | **我判更重（偏松）**。对任意 `sum_w` 恒成立就是恒真。**否决。** |
| `p2_rejection_test.cpp:157-166` | 纯局部数组求和 = 0 | 子代理 07eca2b4 判「阻断」 | **一致。** |
| `p1wcs/CMakeLists.txt` 退役门 | 3 文档命中、0 CMake | 子代理 07eca2b4 判「阻断」 | **内容一致，我的归档降一级为须修**（文档失真 vs 判据说谎）。**部分否决其定级，保留其发现。** |
| `p3_rsmp_oracle.h:227-257` | Var·W≡1 对任意 S 成立 | 两个子代理独立判为恒真 | **一致。** 我补上了完整推导（`C_y q = π` ⇒ `qᵀC_y q = qᵀπ`）。 |
| `seam_vs_bg.py:41,58` | `\|bg\|>1e11` 对 ADU 背景恒**假** ⇒ 静默空转 | 子代理 0764741b 判「恒真、空操作门」 | **我判更重且方向不同**。**否决其判词，保留其发现点。** 见 CE-5。 |
| `seam_vs_bg.py:40,57` | 分母恒取 `D['OLD']` | 两个子代理一致 | **一致。** |
| `mem_wire_test.cpp:142-148` | 桩输出 = FNV(node_id)，与并发无关 | 子代理 07eca2b4 判 M6 须修 | **一致。** 我读码独立确认。 |
| `p3_stars_test.cpp` 守卫跳过 | 三段在 `if` 内 | 子代理 07eca2b4 判 M4 | **一致。** |
| `test_p2001:153` 漏比 overlap_controls | 确认 | 两个子代理独立判 | **一致。** |
| `README.md:88` | 恒真式 | 两个子代理独立判 | **一致。** |
| `make_summary.py` | 纯汇总、不自算自己、缺输入即硬失败 | 三个子代理一致判「通过」 | **一致。** |
| `p1_qf_oracle.cpp` | 真红绿门 | 两个子代理一致判「通过」 | **一致。** |
| `p3_rsmp_oracle.h` 独立性声明 | 属实（只含标准库、异算法） | 子代理 25e21db0 明写「独立性主体通过，不得夸大」 | **一致。** 我同样如实肯定这一条。 |

**盲复算小结**：既有判定整体**偏松**。最松的三处是本片成员文件 `adapter_test.cpp`、`p1_integrate_test.cpp`、`p3_rsmp_oracle.h` 的恒真式断言，以及 `seam_vs_bg.py` 的失效门。我判严于既有判定的 5 处、判轻于既有判定的 1 处（`p1wcs/CMakeLists.txt` 退役门降级）、其余一致。

---

## 7. 子代理派发记录

### 7.1 派发

共派 **5 个**子代理（全部 `subagent`、红队姿态、只读、明确禁止编译/跑测试/写文件/读 `/tmp/acsd_g08/`），按本片成员文件切成 4 个互斥分片 + 1 个交叉核验：

| # | 目标 | 范围 | 份数 | 状态 |
|---|---|---|---|---|
| A1 | C++ 单元/集成测试判据体检 | `p1_noise/adapter_test.cpp`、`p3002`、`p1wcs_tests_apbp`、`p1_integrate_test`、`mem_wire`、`p2_rejection`、`memory_pressure`、`p3_rsmp_gate_test`、`p1_stars`、`mon001_gate_test`、`export_stream`、`p1wcs_test_main.hpp`、`p1wcs/CMakeLists.txt`、`p1psfw_tests_record`、`provider_avx2`、`p3_assembly`、`master_flat_median`、`cpu004_routing`、`p1_wcs_phot`、`p2_block_plan`、`p1psfw_tests_anea`、`core_module_test`、`p2_sky/CMakeLists`、`p2_upm/CMakeLists` | 24 | 已交回 |
| A2 | Python 后端/CLI/API 测试判据体检 | `test_p3_output`、`test_p3_projection_oracle`、`test_bench_cli`、`test_p2001`、`test_cli_single_install`、`test_seam_metric_gate`、`test_mon003`、`hips_properties_probe_main`、两个 config 夹具 | 10 | 已交回（两份额外报告，结论一致互为印证） |
| A3 | validation/release02 实验脚本 + p3_rsmp oracle | `release02/README.md`、`p1_qf_oracle`、`patch_integrate`、`step8_alpha_final`、5 个 `c_delta_composition`、`phot_verify` 两件、`p3_rsmp_oracle.h`、`write_evidence.py` | 13 | 已交回（两份额外报告，结论高度一致） |

（去重后覆盖本片 47/47 份成员文件。）

### 7.2 逐条复核与**否决**记录

我对每份报告的**头条结论**都独立复跑取证（`文件:行` / `sed -n` / `grep` / `python3 -c`）。结果：

**已独立复现、采信的（举例，附我的取证）**：
- `ACSD_P3002_FAULT` 生产零读者 → 我跑 `grep -rn getenv lib/phase3_session/`，**无输出**，复现成功（CE-10）
- `p1wcs_closure_metric_gate` 零 CMake 命中 → 我跑 `grep -rn`，得 3 命中全在 `docs/`+`lib/`；`grep -c add_test` 得 10，复现成功（CE-9）
- `seam_vs_bg.py` 分母恒取 `D['OLD']` → 我 `sed -n '40p;57p'` 亲眼读到 `localbg(D['OLD'], ...)`，复现成功（CE-4）
- `zero_fill_error` 36% 偏差 → 我 `sed -n '197p'` 读到 `1.3223`，`python3 -c` 算出 `0.9726044771163483`，复现成功
- `p3_rsmp_oracle.h:200-204` 权重硬编码 0.25、`registry:195` 硬编码 0.0 → 两条都亲眼读到，复现成功
- `p1wcs_tests_apbp.cpp` 导出目录不存在 → `ls run/p1wcs_wcs003/` 得「没有那个文件或目录」，复现成功
- `eng/ci` 已删 → `ls eng/ci` 得「没有那个文件或目录」，复现成功

**我否决 / 修正的 6 条**：

1. **否决 A1 对 `adapter_test.cpp:723-729` 的判词**（「测试侧手算的独立物理参照，非自证」）。理由：`noise_model.cpp:1042` 生产写的就是 `ivar_bg_global = 1.0/variance_bg_global`，断言写的是 `|ivar_bg − 1.0/var_bg|`。这是**用同一个定义式既当被检量又当期望量**，不是独立参照。→ 升为 **BLK-2**。
2. **否决 A1 对 `p1_integrate_test.cpp:261` 的判词**（「自洽但非恒真」）。理由：`information_weight.cpp:313` 定义 `var_f = 1/sum_w`，断言 `var_f·W == 1` 对任意 `sum_w` 成立 ⇒ 是恒真。→ 升为 **BLK-3**。
3. **否决 A2（0764741b）对 `seam_vs_bg.py:41,58` 的判词方向**（「该条件恒真，等于没筛」）。理由：源码是 `if ...: rows.append(...)`，即**只有 `|bg|>1e11` 才保留样本**。ADU 量级背景永不满足 ⇒ `rows` 恒空 ⇒ 走 `too few` 分支。→ 不是空操作，是**静默空转**；改写为 **CE-5**，并入 **BLK-6**。
4. **改 A1 的定级**：`p1wcs/CMakeLists.txt:185-190` 退役门，A1 判「阻断」。我认为该文件本身行为正确（它如实登记了移除），**失真在 `docs/` 三处**，故本片侧降为 **须修 FIX-2**，同时在 §4.1 末注明「负责人可上调为阻断」。**保留发现，改定级。**
5. **否决 A1 的 CE3 结论外推**：A1 诚实报告「p2_sky argv 分发这条我**没打成**（12/12 名对上，无现网红灯）」。我采纳其诚实标注，并在 §5 CE-12 明确记为「推翻失败」，**不把潜伏隐患冒充现网缺陷**。
6. **否决 A3（0b3b41ad）关于 `p3_rsmp_oracle.h:227-257` 的「缺陷只是恒绿冗余、不是黑洞」定性**（该定性出现在 25e21db0 的报告中）。理由：`:49` 的等值交叉核对**只覆盖 `max_weight_sum_dev`**，而 `boundary_fail_closed`（`:51`）与 `zero_fill_error`（`:53`）**均未做等值比对**，故 `max_weight_sum_dev` 一条恒真**并不能**为该 oracle 整体背书；36% 偏差正是在漏比的那个字段上存活。→ 定性改为**黑洞**（BLK-1 + BLK-4 + §5 算术复核）。

**另如实记录 1 条子代理 UNRESOLVED（我未能结案，列出交负责人裁决）**：
- A1/A2 指出 `test_seam_metric_gate.py:22-23` 的前置产物（`build/linux-openmp-on/libphase2.a`、`lib/infrastructure/aio/astro_image_io.dll`）在 `eng/ci` 被删后**无法证实仍有构建步产出**。若无人产出，该 fail-closed 门会**恒红**。`eng/ci` 已删导致该问题在仓内不可自证。**我不替负责人下结论。**

---

## 8. 自证段（可复跑命令）

全部命令**只读**，不编译、不跑测试、不写仓内文件。中文路径一律带 `git -c core.quotepath=false`。

```bash
# ── 0. 基线与片清单 ──────────────────────────────────────────────
cd "/workspace/Astro CS Database"
git -c core.quotepath=false log --oneline -1                 # 期望 850a9ede
sed -n '1612,1666p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml

# ── 1. 覆盖率自证：47 份全部存在、行数合计 ────────────────────────
sed -n '1620,1666p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml \
  | sed 's/^ *- "//; s/"$//' > /tmp/eng010_list.txt
wc -l /tmp/eng010_list.txt                                    # 期望 47
while IFS= read -r f; do [ -f "$f" ] && wc -l < "$f" || echo "MISSING $f"; done < /tmp/eng010_list.txt
while IFS= read -r f; do [ -f "$f" ] && wc -l < "$f"; done < /tmp/eng010_list.txt \
  | awk '{s+=$1} END {print "TOTAL LINES =", s}'            # 期望 10422

# ── 2. BLK-1 / BLK-2：恒真式断言（用生产定义式当期望量）──────────
echo "--- 测试侧断言 ---"
sed -n '720,732p' eng/tests/unit/p1_noise/adapter_test.cpp
sed -n '259,263p' eng/tests/integration/p1_integrate/p1_integrate_test.cpp
sed -n '227,257p' eng/tests/unit/p3_rsmp/p3_rsmp_oracle.h
echo "--- 生产侧定义式（被检量即期望量）---"
sed -n '1039,1043p' lib/algorithms/noise_snr/cpp/src/noise_model.cpp
sed -n '1073,1077p' lib/algorithms/noise_snr/cpp/src/noise_model.cpp
sed -n '305,316p' lib/algorithms/noise_snr/cpp/src/information_weight.cpp
sed -n '6p' eng/tests/unit/p1_noise/adapter_entry_impl.cpp   # 直接 include 生产 TU
sed -n '344,348p' lib/algorithms/integration/phase2_integrate/src/phase2_integrate.cpp

# ── 3. CE-1 的纸面重算（不编译）：对任意 V，D11 恒绿 ─────────────
python3 -c "
V=0.3721                      # 任取的（错误也无妨的）背景方差
var=V; ivar=1.0/V; sig=V**0.5
print('ivar-1/var =', abs(ivar-1.0/var))       # 0.0
print('sigma^2-var=', abs(sig*sig-var))        # 0.0
"

# ── 4. CE-2 的纸面重算：Var·W ≡ 1 对任意 S 成立 ────────────────
python3 -c "
import numpy as np
rng=np.random.default_rng(0)
for S in [np.eye(4), rng.normal(size=(4,4)), np.ones((4,4)), 0.5*rng.normal(size=(4,4))]:
    Cd=np.eye(4); pi=rng.normal(size=4); a=1.7
    Cy=S@Cd@S.T
    q=np.linalg.solve(Cy,pi); denom=a*(q@pi); W=a*a*(pi@np.linalg.solve(Cy,pi))
    var=(q@q)/denom**2
    print('cond(Cy)=%.2e  Var*W-1 = %.3e' % (np.linalg.cond(Cy), abs(var*W-1)))
"                                                    # 即使 S 完全错，Var*W-1 仍 ≈ 0

# ── 5. CE-3 的纸面重算：var_f·W ≡ 1 对任意 sum_w 成立 ───────────
python3 -c "
for sw in [1.0, 4.0, 1e-6, 1e9]:
    print('sum_w=%g  var_f*W = %g' % (sw, (1.0/sw)*sw))"

# ── 6. BLK-4：max_weight_sum_dev 恒真链 ────────────────────────
sed -n '200,204p'   eng/tests/unit/p3_rsmp/p3_rsmp_oracle.h
sed -n '180p'       eng/tests/unit/p3_rsmp/p3_rsmp_oracle.h
sed -n '195p'       lib/algorithms/resample/p3_rsmp_kernel_registry.cpp
sed -n '44,55p'     eng/tests/unit/p3_rsmp/p3_rsmp_oracle_test.cpp
echo "--- 生产默认核的 0.0<=0.0 恒绿 ---"
sed -n '216,226p'   lib/algorithms/resample/p3_rsmp_kernel_registry.cpp
sed -n '99p'        lib/algorithms/resample/p3_rsmp_kernel_registry.cpp

# ── 7. zero_fill_error 36% 偏差（§5 算术复核）───────────────────
sed -n '218p' lib/algorithms/resample/p3_rsmp_kernel_registry.cpp   # 期望 1.3223
sed -n '53p'  eng/tests/unit/p3_rsmp/p3_rsmp_oracle_test.cpp          # 期望只判 >0.5
python3 -c "import math; print(4*0.25*math.exp(-0.5/18))"              # 期望 0.9726044771163483

# ── 8. BLK-5：FIX-P1 头条证据是 1/g[k] 往返自证 ──────────────────
sed -n '86,90p;99,103p;110,114p;124,130p' \
  eng/tests/validation/release02/fix_p1_photometry_apply/p1_apply_oracle.cpp
sed -n '88p' eng/tests/validation/release02/README.md
grep -n "中位-1" eng/tests/validation/release02/fix_p1_photometry_apply/oracle_output.txt

# ── 9. BLK-6 / CE-5：seam_vs_bg.py 三重失效 ─────────────────────
sed -n '33,47p;49,63p' eng/tests/validation/release02/c_delta_composition/seam_vs_bg.py
echo "--- 背景量级 vs 1e11 哨兵 ---"
python3 -c "print('ADU 背景 ~1e2..1e3 ；|bg|>1e11 成立? ', 1e3>1e11)"

# ── 10. BLK-7：seam_fixed.py 共享边界像素 + OLD 分母 ────────────
sed -n '21,35p;37,47p' eng/tests/validation/release02/c_delta_composition/seam_fixed.py

# ── 11. FIX-3：p2_rejection 恒真 + 死 fixture ───────────────────
sed -n '143,166p'   eng/tests/unit/p2_rejection_test.cpp
sed -n '1,10p;210,218p' eng/tests/unit/p2_rejection_test.cpp

# ── 12. FIX-1：p2001 漏比 overlap_controls + cpu_workers 被覆盖 ──
sed -n '106,125p;145,153p' eng/tests/backend/test_p2001_parallel_sampler.py
sed -n '9606,9613p' lib/infrastructure/scheduler/src/module_adapters.cpp
sed -n '254,258p;337,341p' lib/algorithms/coverage/tests/sampler_parallel_consistency_test.cpp

# ── 13. FIX-2：退役门仍被正本表标 Y（CE-9）──────────────────────
grep -rn p1wcs_closure_metric_gate docs/ lib/ eng/ 2>/dev/null
grep -c add_test eng/tests/unit/p1wcs/CMakeLists.txt          # 期望 10，其中无 closure
git -c core.quotepath=false log --oneline -1 e5f589a6

# ── 14. FIX-4 / FIX-5 / FIX-7：悬空引用 ─────────────────────────
grep -rln "ALG-P3-001_KERNEL_REGISTRY" docs run/GOVERN-08 2>/dev/null   # 期望无输出
ls docs/engineering/v6 2>&1                                       # 期望 No such file
ls eng/tests/unit/p3_rsmp/standalone 2>&1                         # 期望 No such file
ls eng/tests/validation/release02/fix_p1_photometry_apply/build_qf_oracle.sh 2>&1
ls eng/ci 2>&1                                                    # 期望 No such file
sed -n '11,20p' eng/tests/unit/p3_rsmp/run_mutations.py          # 搬迁注释
ls run/v6/p3-rsmp/ 2>&1 ; ls run/FINAL-07/p3-mutation-open/p3-rsmp/ 2>&1
ls run/RELEASE-02/ 2>&1                                          # 期望只剩 3 个目录

# ── 15. FIX-6：恒绿伪装四处 ─────────────────────────────────────
grep -n "skipTest\|skipUnless" eng/tests/backend/test_p3_output.py \
  eng/tests/backend/test_mon003_synthetic.py \
  eng/tests/backend/test_p2001_parallel_sampler.py
sed -n '36,44p' eng/tests/cli/test_cli_single_install.py        # 正例范式（对照）
sed -n '105,118p' eng/tests/validation/release02/q3_additive_truth/src/step8_alpha_final.py

# ── 16. FIX-8 / FIX-9 / FIX-10：桩与守卫 ───────────────────────
sed -n '137,151p;378p;407p' eng/tests/unit/mem_wire_test.cpp
sed -n '145,150p;175,180p'   eng/tests/unit/p1_stars_test.cpp
sed -n '249,252p'            eng/tests/unit/export_stream_test.cpp

# ── 17. 伪引：README 全部逐条复核 ─────────────────────────────
sed -n '30,41p;82,88p;124,125p;161,170p' eng/tests/validation/release02/README.md
sed -n '1,12p;40,50p;105,115p'  lib/algorithms/calibration/src/photometry_apply.h
sed -n '6279,6285p' lib/infrastructure/scheduler/src/module_adapters.cpp
sed -n '294,300p' docs/science/algorithms/CALIBRATION_ALGORITHMS.md
sed -n '20,25p' lib/algorithms/photometry/cpp/src/star_matcher.cpp   # 4.685 的真实出处

# ── 18. 误路径反例（成员文件确实在 eng/tests/cpu/ 而非 unit/cpu/）──
ls eng/tests/unit/cpu/avx2/provider_avx2_so_load_test.c 2>&1   # 期望 No such file
ls eng/tests/cpu/avx2/provider_avx2_so_load_test.c               # 期望存在
```

---

## 9. 给负责人的裁决项（UNRESOLVED，不替负责人下结论）

1. `test_seam_metric_gate.py:22-23` 的前置产物（`build/linux-openmp-on/libphase2.a`、`lib/infrastructure/aio/astro_image_io.dll`）在 `eng/ci` 被 `e5f589a6` 物理删除后，**是否仍有构建步产出**？仓内已不可自证。若无人产出 ⇒ 该 fail-closed 门**恒红**，会把真实接缝缺陷藏进红灯。
2. `cpu004_routing_test.cpp:3,101,128` 与 `test_bench_cli.py:189` 引用的「08 §4-7/§4-8」「08 §4」，该文档 ID 仓内不可解析 —— 是悬空引用还是有效引用？
3. `mem_wire_test.cpp:417-418` 引 `eng/packaging/config/runtime_resources.json` 的 `95`，我未核到该文件取值。
4. `p1psfw` 的 `PSFSW-G01..G25` 是否应覆盖 G16/G20？`lib/` 下 `PSFSW-G16|PSFSW-G20` 零命中，编号非连续。**我未判为缺陷**（`p1psfw_tests_record.cpp` 的断言均按实际存在的门号写），但若正本确应有此二门，则记录门有缺口。
5. `p1wcs_closure_metric_gate`（FIX-2）：恢复注册，还是把 `GATES_AND_TOLERANCES.md:67` 的 **Y** 与 `ASTROMETRY.md:281` 的「过」改为「未注册/待重建」？二者必居其一 —— 现状是门禁正本表指向一个不存在的 ctest。

---

**审稿人**：ENG-tests-010 车道 · 对抗审稿第 1 遍
**基线**：HEAD = 850a9ede · **覆盖**：47/47 份 / 10422/10422 行 = 100%
**零 git 写、零编译、零测试执行、零仓内文件修改（除本交付件）**
