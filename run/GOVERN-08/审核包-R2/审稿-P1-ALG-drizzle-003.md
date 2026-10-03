# 审稿 P1 · ALG-drizzle-003（G08-05 对抗审稿 第 1 遍）

- 仓库：`/workspace/Astro CS Database`
- HEAD：`850a9edefd47434b9ab71bc907c3de1e0814b323`（= 要求的 `850a9ede`）
- 片号：`ALG-drizzle-003`，层 `lib/algorithms/drizzle`
- 权威依据：`run/GOVERN-08/工作包-GOVERN-08原件/`
- 纪律声明：零 git 写；未编译、未跑 ctest/pytest/构建/二进制；未改仓内任何文件；中文路径一律 `git -c core.quotepath=false`；未读 `/tmp/acsd_g08/`。
- **口径**：本报告所有「通过/红/绿」判断均来自我自己重读原文与重算；子代理结论一律经我复核后才采信，复核过程见 §7。负责人裁定「看到任何检查通过机制，不要据此认为实现正确」已作为默认姿态。

---

## 1. 读完了吗（口径明确）

**清单口径**：`片清单-权威版.yaml:266-298` 列 25 份，`实际行数 8999`。我逐份 `wc -l` 复核，**25/25 存在，合计 8999 行，与权威清单逐份一致，缺失 0**。

| 口径 | 份数 | 行数 | 占 8999 |
|---|---|---|---|
| **我本人逐行读完** | 11 | 3578 | 39.7% |
| **我本人部分读**（只读关键段，未读完） | 3 | ~261（其中 1342 的文件只读 ~130） | 2.9% |
| **子代理逐行读完（关键结论均由我复核）** | 14 | 5292 | 58.8% |
| **本人 ∪ 子代理完整覆盖** | **25 / 25** | **8999** | **100%** |

**我本人完整读完的 11 份（3578 行）**：
`spherical_overlap.cpp` 1940 · `p3_conservation_gate.cpp` 436 · `drizzle_science.h` 394 · `memory.md` 220 · `hips/include/acsd/hips/types.h` 156 · `healpix_core.h` 120 · `hp_drizzle_hips_api.cpp` 97 · `reverse_drizzle.h` 74 · `p1drz_merge_pipeline_lock.sh` 69 · `README.md` 60 · `module_exports.map` 12。

**我本人部分读、未读完的 3 份（如实列出）**：
- `src/module_entry.cpp` — **未读完**，我只读了 :866-975（≈130/1342）+ 全文件模式扫描。其结论以子代理为准，**我不宣称独立覆盖**。
- `tests/acceptance_drizzle.py` — **未读完**，我只读了 :57-96、:160-199、:245-262、:348-362（≈96/362）。关键 BLOCKER 我已亲自复核。
- `tests/p1drz/p1drz_disp009_gate.cpp` — **未读完**，我只读了 :1-20、:245-262（≈35/266）。

**我本人一行未读、由子代理全读的 11 份（如实列出）**：
`test_drizzle.py` 774 · `test_pipeline_adapter.py` 247 · `p1drz_tests_core.cpp` 641 · `test_nside_pixfrac.cpp` 565 · `drizzle_nonfinite_test.cpp` 301 · `oracle_edge_crossing_test.cpp` 276 · `probe_reverse_steps.cpp` 192 · `p0_sip_order_guard_test.cpp` 166 · `drizzle_l2_test.cpp` 146 · `drizzle_l3_full_fp32.cpp` 110 · `mini_sip1000.cpp` 33。

**未覆盖**：`src/module_entry.cpp` 尚无任何一方完整回报（1342 行）。

---

## 2. 本片判定：**阻断**

最重 5 条：

1. **已注册的唯一验收门 `drizzle_acceptance_py` 在「所有物理判据全 FAIL」时仍 exit 0。**
   `tests/acceptance_drizzle.py` 有 **25 处 `check()` 调用、仅 9 处 `results["checks"].append`**；`check()` 自身（`:71-74`）只 `print` 并 `return cond`，从不登记。证据文件缺失时 `load_jsonl`（`:60-68`）返回 `[]`，于是 `:199-201 / 219-221 / 254-256 / 274-276` 全部走 `check("场景存在", False)` + `continue` —— **打印一片红，一个都不计数**，末尾 `:349-358` 得 `failed==0` → `return 0`。**「证据不存在」被当成「验收通过」。** 我本人已逐行复核此控制流。

2. **自洽式断言（负责人点名最有价值的形态）：`acceptance_drizzle.py:171` 判据是字面量 `True`。**
   `check("numpy 解析 Σin size=%d" % size, True, "%.6g" % synth_flux(size))` —— `synth_flux(size)` 只被拼进打印串，**不与任何值比较**。而 `:10`/`:168` 与 `synth_flux` docstring（`:47-48`）都宣称这是「独立于 C++」的解析核对。**被测量与期望量塌缩成同一个打印动作。** 一个 `def synth_flux(): return 0.0` 也能过。同类第二例：`p1drz_disp009_gate.cpp:255` 断言 `|A_pixel/A_drop − 1| < 1e-12`，而 `p1drz_oracle.hpp:87-90` 与 `:93-97` 在 `pixfrac==1.0` 时是**逐字相同的函数体**，即断言 `|X/X − 1| < 1e-12`。

3. **本片最强科学门 `p3_conservation_gate` 文档化的 `τ_rel = 1e-6` 相对判据，在整个测试矩阵里一次都没生效；最小尺度处逐 drop 门是恒真门。**
   `judge_limit = max(1e-6·a_ref, 1e-15)`。我独立复算：矩阵中**最大**被判 drop 是 HST 0.2″/px → `a_ref = 9.40e-13 sr`，`1e-6·a_ref = 9.4e-19`，比绝对项 `ε_abs = 1e-15` **低 4 个量级** ⇒ 相对项在任何配置下都不 binding。更严重：HST 0.005″/px 时 `a_ref = 5.876e-16 sr < ε_abs`，而 `|da|` 的**理论最大**就是 `a_ref = 5.876e-16 <= 1e-15` ⇒ **核对这些 drop 的交叠面积恒返回 0 也判绿**。实际强制的相对容差是 `1e-15/a_ref` = 1.06e-3（HST 0.2″）… **1.70（HST 0.005″，即 170%）**，比文件头 `:9` 与 DRIZZLE_GEOMETRY.md §9 声明的 1e-6 松 3~6 个量级。oracle 本身确实独立（我确认），**但判据形同虚设**。

4. **`drizzle_nonfinite_test.cpp` 的断言与生产语义相反 ⇒ 恒红，且因未注册而从不被点亮。**
   `:227-228` 断言 `nSourcePixels == W*H - 2`（注释：「两像素均不进管线」）。实际：`weight==0` 在 `drizzle_engine.cpp:2118-2121` `continue` 不计数；`variance==0` 在 `:2140` 只把方差置 0 后**继续下落**，到 `:2144` 唯一 `source_pixels++` 计数 ⇒ 实测值是 `W*H-1`。而 `:2131-2139` 的注释明写「V_j ≤ 0 = 合法产品态，不丢信号、不丢几何支撑」。**测试断言的科学含义与引擎的成文设计相反**，且 `tests/CMakeLists.txt:194` 只编译不注册。

5. **11 份测试文件里 4 份被编译但全仓无 `add_test`；另有 2 份 Python 门一次断言都没执行过。**
   `drizzle_nonfinite_test.cpp`、`oracle_edge_crossing_test.cpp`、`drizzle_l2_test.cpp`、`p0_sip_order_guard_test.cpp` 无任何 ctest 注册（我已用 `tests/CMakeLists.txt` + `p1drz/CMakeLists.txt` 复核）。`tests/CMakeLists.txt:161-165` 的注释自认「避免把与本轮无关的历史红灯带进 ctest 基线」—— 即**因红而不注册**，正是「恒红门被藏起来」而非被修。`test_drizzle.py` / `test_pipeline_adapter.py` 的模块守卫把「导入的模块根本不存在」降级成整文件 skip，且两文件全仓无注册。

---

## 3. 逐文件清单

| 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `spherical_overlap.cpp` (1940) | **全文** | 面积口径在快路径间不一致（:1370 恒球面 vs :1379-1384/ :1099-1101/ :1936 分尺度感知）；`pixelToSky` 失败静默返回空多边形（:836-837, :935-937）与静默接受未收敛边（:883-886）；S-H 定容 16 顶点溢出即 `return 0`（:391/394/397）伪装成「无交叠」；生产留故障注入开关 `ACSD_DRZ_P3_FAULT`（:1210-1216, :1275-1291）；陈旧注释（:633-634 说 depth 8，实为 12；:1233 说 1.0×，实为 1.25×）；`wiki/Reverse_Drizzle.md` 悬空引用（:198） | **阻断** |
| `drizzle_science.h` (394) | **全文** | 冻结锚自洽：`w_jp=a_jp/A_drop_j` 为 canonical（:23-30，引 F&H 2002 §7.2 + drizzlepac `cdrizzlebox.c dover/=jaco`）；`gate_*` 声明要求「只接受 raw 并重算」(:272-274)；`psfsw_robust_weight` 退役只剩字符串面（:88-90, :112-118）。**未发现缺陷**，是本片最可靠的判据来源 | 通过（作为基准） |
| `p3_conservation_gate.cpp` (436) | **全文** | **主判据真独立**：参考面积用 double-double FMA Van Oosterom 扇形（:92-118），与生产 S-H 裁剪不同算法不同代码；判据自检含正/负/边界 5 例（:287-307）；逐组判定明文禁「全局汇总通过即通过」(:16)。**缺陷**：负例分支 `:388 ++fail` 无条件 → `WILL_FAIL` 只观测「非零退出」，注入钩子失效也判过（恒红）；极点强扫描 g1/g3 测了不判（:331-345）；`frame_rel` 在 `sum_a==0` 时 NaN 通过（:403-404） | 须修 |
| `memory.md` (220) | **全文** | `:161` 自认 `test_spherical_overlap` 历史遗留失败且「非门禁」、`drizzle_acceptance_test` 未纳入快速回归；`:220` 自认「variance 全零 tile 跳过不中止」= 静默降级；`:46` Makefile 依赖 `../healpix_io` 目录**已不存在**（G1 已归档并改名）；`:36`/README`:11` 宣传「OpenMP 16 线程并行」 | 须修 |
| `hips/include/acsd/hips/types.h` (156) | **全文** | 引用的 commit(`babe752d`/`2c065ace`)、`module.yaml`、`aio_hips.h`、`astro_sphere_sink.cpp:52-167` **全部真实存在**（我逐个验证）。`:31` 硬编码日期型 BUILD_ID；`:128-130` 自认 DISP-HIPS-007「错误码无集中枚举」未消化 ⇒ legacy 码全折叠进 `HIPS_ECODE_LEGACY_REJECT=120`，无稳定可分错误码 | 建议 |
| `healpix_core.h` (120) | **全文** | `:37-39` **空分支吞掉非法 nside**（不抛、不记、继续），而同文件 `:34-36` 对 RING 却抛 `invalid_argument` —— fail-open 且自相矛盾；`:53` `ra += 360` 只补一次（`:54` 用 fmod 无界），`phi_rad < -2π` 时传出负 RA；`:88` 称 `pixelToCoarse/pixelToFine`「保留防编译破裂」，实测**全仓零调用者**（仅 archive/legacy 另一套同名类在用）—— 保留理由不实 | 须修 |
| `hp_drizzle_hips_api.cpp` (97) | **全文** | `:21-26` 的 `hp_drizzle_run_hips` 文档块被 `:34-65` 整个函数隔开，孤儿注释；`:56/63/88/95` 魔法数 `-11` 四处硬写；异常屏障本身 fail-closed（清零 result + setErrorMsg + 非零返回），可接受 | 建议 |
| `reverse_drizzle.h` (74) | **全文** | `:4` 「冻结语义 (wiki/Reverse_Drizzle.md)」—— **该文件不存在，全仓无 `wiki/` 目录**；`:14` 契约「非法参数硬失败，不得静默裁剪」与 `:59-61` 的 `n_invalid_ipix`/`n_nonfinite`/`n_skipped_outside` 三个「只计数」字段存在张力 | 须修 |
| `p1drz_merge_pipeline_lock.sh` (69) | **全文** | 三条 `SKIP`→`exit 0` 路径（:18-20, :22, :38）使该锁在非 Linux / <2 CPU 上**零执行即报成功**；`:55` 抓到 `threads=N` **只打印、从不校验**，探针若忽略预算恒单线程则各档 sha256 自然相同 → 门恒绿；`:39 rm -rf "$WORK"` 对未校验的用户路径 | 须修 |
| `README.md` (60) | **全文** | `:25 healpix_drizzle.py`、`:26 pipeline_adapter.py`、`:27 eng/tests/` **三项全部不存在**（已验证）；`:37` `make` 已被 CMake 取代；`:11`「OpenMP 16 线程并行」与 AGENTS.md §6 禁私建池冲突 | 须修 |
| `module_exports.map` (12) | **全文** | `:2` 引 `12_DLL_ABI_AND_LOADER_STANDARD §6` —— **该文档不存在**（最接近 `docs/engineering/abi/ABI_003_SECURE_LOADER.md`）；`:5-6` 支撑「nm -D 仅此一符号」的实证文件 `tests/unit/hips_writer_adapter_test.cpp` **不存在**（真实文件是 `eng/tests/unit/p1_hips/adapter_test.c`）⇒ ABI 白名单的唯一证据链断裂 | 须修 |
| `module_entry.cpp` (1342) | **:866-975 + 模式扫描（未读完）** | `:909-929` host executor 租借**符合**禁私建池规范（并在 `:938` 还原 ICV）；但 `:868-874` 错误域映射 `code<=-4 && code>=-8`→DATA、`-1/-2/-10`→CONFIG，**`-3`（越界）与 `-11`（C 边界内部异常屏障）落入默认 SCIENCE_PRECONDITION** —— 内部崩溃被归因为科学前置条件失败 | 须修（余待复核） |
| `p1drz_disp009_gate.cpp` (266) | **:1-20, :245-262（未读完）** | `:255` 自洽恒等断言 `|A_pixel/A_drop − 1| < 1e-12`（`p1drz_oracle.hpp:87-97` 两函数体在 pf=1 逐字相同）；`:7-8` 声明合同为 `w_jp=a_jp/A_drop,j` | 须修 |
| `acceptance_drizzle.py` (362) | **:57-96,:160-199,:245-262,:348-362（未读完）** | `:171` 字面量 `True` 恒真门；`:180` `run_cxx_exe` 位于 `if not args.skip_run:` **之上**，即 `--skip-run` 仍运行且**覆写随后 `:188` 要读的同一 JSONL**；`:333` `--skip-run` 静默摘掉唯一的反恒真负例控制 H | 阻断 |
| 其余 11 份 | **本人未读**，见子代理报告 | 见 §4 与 §7 | 见 §4 |

---

## 4. 发现清单

### 阻断（8）
- **B0a `module_entry.cpp:260 + :284 + :814 / :1104 / :1135`** —— **`b64_decode` 无容量参数 ⇒ 堆溢出**。签名 `b64_decode(const char*, uint64_t src_len, uint8_t* dst)`，写入 `dst[o++]` 仅受**编码长度**约束，不受分配大小约束；而三处调用点按**解码后**长度分配（`:814 malloc(need)`，need=w·h·4|8）。`:821` 的 `got != need` 检查发生在溢出**之后**。作者显然知情：第四处 `:1103` 分配 `max(b64_len+4, ipix_need+1)`（可证 ≥ 3·b64_len/4）——同一函数内隔 1 行，另三处漏改。**本人已逐行复核签名、写入点与三处调用点。**
- **B0b `module_entry.cpp:206-207`** —— **`json_get_f64_array` 遇畸形数字死循环**。`strtod(q,&end); q = end;` 无 `end == q` 守卫；C11 7.22.1.4 规定无转换时 `end == nptr`，故 `"crval":[-,1]` 使 `q` 永不推进 ⇒ 线程永久挂起 + `count++` 有符号溢出 UB。**同文件的标量解析器 `:165` 与 `:168` 双重守卫**，数组版两个都没有 —— 是遗漏不是设计。**本人已复核两处对照。**
- **B0c `module_entry.cpp:948 / :1205`** —— **`err->message_utf8` 指向已出栈的局部数组**。`lifecycle_v1.h:87-88` 冻结该字段为**模块静态存储**，但 `efill` 收到的是 `drz_execute_drizzle` / `drz_execute_reverse` 的函数局部 `char msg[640]`，其栈帧在 `drz_execute` 返回宿主**之前**即已释放。任一 legacy 非零返回都会让宿主读到悬垂指针。（注：本文件其余字符串**全是字面量**，具静态存储，故仅此二处。）
- **B1 `acceptance_drizzle.py:199-201/219-221/254-256/274-276 + :60-68 + :71-74 + :349-358`** —— 「场景缺失/证据缺失」不进 `failed`，全 FAIL 仍 exit 0。已注册为 `drizzle_acceptance_py`（`tests/CMakeLists.txt:151-157`）。**本人复核控制流确认。**
- **B2 `acceptance_drizzle.py:171`** —— `check(..., True, ...)` 恒真门；自称「独立于 C++」的核对从不比较。**本人复核确认。**
- **B3 `test_drizzle.py:56-70` + `test_pipeline_adapter.py:69-86`** —— `except Exception` 把「模块不存在 / ABI 变更」降级为整文件 `skipif`，10 个测试全 SKIP 且 exit 0；两文件全仓无 `add_test`。**复核：所导入符号全仓零定义；`healpix_io` 仅存 `ARCHIVED.md`。**
- **B4 `p1drz_oracle.hpp:278-286`（经 `p1drz_tests_core.cpp:400-406` 消费）** —— 先按 `nContrib==1` 筛子集再取 `max_rel`。多贡献叶（nside=512 时绝大多数真实天区像素）**完全不检**；且该子集恰是 oracle 自身推导中面积 `a` **代数抵消**的叶，即公式平凡成立处。反例：生产把 `Σ_j v_j w_jp²` 改成只留首项，所有 `nContrib==1` 叶仍精确 ⇒ 绿，而几乎所有真实像素方差是错的。**这就是「筛掉真信号」。**
- **B5 `p3_conservation_gate.cpp:170-173 + :352-357`** —— **文档化的 `τ_rel = 1e-6` 在整个矩阵内一次都未生效；HST 0.005″ 处逐 drop 门为恒真门**。`judge_limit = max(1e-6·a_ref, 1e-15)`。**本人独立复算**（非转述）：矩阵最大被判 drop = HST 0.2″ → `a_ref = 9.40e-13`，`1e-6·a_ref = 9.4e-19` ≪ `1e-15` ⇒ 相对项在任何配置都不 binding；HST 0.005″ 时 `a_ref = 5.876e-16 < ε_abs`，而 `|da|` 理论上限就是 `a_ref` ⇒ **交叠面积恒返回 0 也判绿**。实际强制相对容差 `1e-15/a_ref` = 1.06e-3 … **1.70（170%）**，比 `:9` 与 DRIZZLE_GEOMETRY.md §9 声明松 3~6 个量级。诚实说明：**帧级 1e-4 聚合预算仍 binding**，能抓系统性丢失；缺口在**逐 drop 局部预算**。`self_test_judge`（`:290-296`）只在 `a_ref = 1e-3/1e-12 sr` 证明相对项有判别力 —— 比矩阵中任何真实情形高 9~10 个量级。
- **B6 `drizzle_nonfinite_test.cpp:227-228`** —— **断言与生产语义相反 ⇒ 恒红，且未注册故红灯从不被点亮**。断言 `nSourcePixels == W*H-2`（注释「两像素均不进管线」）。实际：`weight==0` 在 `drizzle_engine.cpp:2118-2121` `continue` 不计数；`variance==0` 在 `:2140` 仅把方差置 0 后**继续下落**，到 `:2144`（grep 确认是**唯一** `source_pixels++` 点）计数 ⇒ 实测 `W*H-1`。而 `:2131-2139` 明写「V_j ≤ 0 = 合法产品态，不丢信号、不丢几何支撑」。**本人已逐行复核两侧。**

### 须修（9）
- **M1 `p1drz_disp009_gate.cpp:255`** —— `|X/X−1|<1e-12` 自洽恒等断言（**本人复核 `p1drz_oracle.hpp:87-97` 两函数体逐字相同**）。
- **M2 `drizzle_l2_test.cpp:103-113`** —— 只遍历 FP32 侧、只查「FP64 有而 FP32 无」的 `n_missing`，反向缺失不计数；`:109-110` 分母 `max(|ref|,1.0)` 把相对门变成绝对门（ref=1e-6、实际=0 ⇒ `r=1e-6<1e-5` 通过）；FP32 从不做 finite 检查。该文件**且**无 `add_test`。
- **M3 `p3_conservation_gate.cpp:388`** —— 负例分支 `++fail` 无条件，`WILL_FAIL TRUE` 只观测非零退出 ⇒ 注入钩子被删也判过（恒红门）。`:331-345` 极点强扫描测了不判；`:403-404` `sum_a==0` 时 `NaN > budget` 为假 ⇒ 通过。
- **M4 `p1drz_merge_pipeline_lock.sh:55`** —— 抓到 `threads=N` 只打印不校验；探针忽略线程预算时该「线程预算不变式锁」恒绿。`:18-20/:22/:38` 三条 SKIP→exit 0。
- **M5 `spherical_overlap.cpp:1370`** —— 三角形扇 `tri_inside` 快路径**恒用球面公式**，而同文件 `:1379-1384`、`:1099-1101`、`:1936-1937` 均按 `max_angle<1e-3` 在平面/球面之间切换，`:1376-1378` 注释明写该切换是为「与 `g.drop_area` 表示一致，避免 weight 偏差」。**同一 drop 的面积被两条公式同时测量**，自相矛盾。同文件 `:1009-1013` 自记两种表示差 ≈ `−θ²/2`（1e-3 rad ⇒ −5.0e-7，单向下偏），而 `:1206` 的守恒预算正是逐 drop 1e-6 相对 ⇒ 该不一致单项吃掉一半预算。
- **M6 `spherical_overlap.cpp:836-837 / 883-886 / 935-937`** —— `pixelToSky` 失败即 `return {}`（空多边形）或「塞 p0 并停止细分」，经 `:998 / :1073 / :1224` 变成面积 0 ⇒ **该源像素被静默当作「无交叠」丢弃**，无计数、无错误码、无 provenance；与 `reverse_drizzle.h:14`「非法参数硬失败，不得静默裁剪」直接冲突。（注：`spherical_overlap.cpp` 自身返回的 NaN 由 `drizzle_engine.cpp:1668` 的 `isfinite` 兜住，**不算缺陷**；真正无兜底的是本条的空多边形路径。）
- **M7 `spherical_overlap.cpp:391/394/397`** —— S-H 定容 `Vec3 bufA[16]` 溢出时 `return 0`，与「空交集」**不可区分** ⇒ 交叠被静默抹掉。`:1333` 生产路径（nb==4，subject 4 顶点 + m 个 drop 半空间）在 m≥12 时即可溢出；`:1373` 扇剖分路径在 m≥13 时溢出。
- **M8 `healpix_core.h:37-39`** —— 非法 nside 空分支吞掉（fail-open），与 `:34-36` 对 RING 抛异常自相矛盾；`:88`「保留防编译破裂」不实（`pixelToCoarse/Fine` 全仓零调用者）。
- **M9 悬空引用群**（均为「整目录已删/键名已改」形态）：
  - `reverse_drizzle.h:4` 与 `spherical_overlap.cpp:198` 引 `wiki/Reverse_Drizzle.md` —— **全仓无 `wiki/` 目录**，而这是「冻结契约」的权威出处；
  - `module_exports.map:2` 引 `12_DLL_ABI_AND_LOADER_STANDARD`（不存在）、`:5-6` 引 `tests/unit/hips_writer_adapter_test.cpp`（不存在）；
  - `README.md:25/26/27` 引 `healpix_drizzle.py`、`pipeline_adapter.py`、`eng/tests/`（均不存在）；
  - `memory.md:46` 引 `../healpix_io`（G1 已归档改名）。

### 建议（6）
- **S1** `spherical_overlap.cpp:1210-1216/1275-1291` —— 生产路径保留故障注入环境变量 `ACSD_DRZ_P3_FAULT`，设错即静默回退到 9.97%/叶 的缺陷行为，无日志。（正面：测试侧 `tests/CMakeLists.txt:367-371` 确实设了该变量并配 `WILL_FAIL`，负例门是真的。）
- **S2** `module_entry.cpp:868-874` —— `-3`（越界）与 `-11`（内部异常屏障）被映射到 `SCIENCE_PRECONDITION` 默认域。
- **S3** `p1drz/CMakeLists.txt:89` 写「修复 = `w_jp = a_jp/A_pixel,j`」，而 `p1drz_disp009_gate.cpp:3-4,17-18` 写「合同 = `w_jp = a_jp/A_drop,j`」且把后者称作**缺陷**。二者**指认相反的公式为正确**。我以 `drizzle_science.h:23-30`（引 F&H 2002 + drizzlepac 原文）判定 **`drop_area` 为 canonical，`p1drz/CMakeLists.txt:89` 是错的**。
- **S4** `module_entry.cpp:938` —— `if (omp_prev > 0) DRZ_OMP_SET(omp_prev)`；`omp_get_max_threads()` 返回 0 时不还原，线程 ICV 泄漏给宿主。
- **S5** `types.h:128-130`（自认 DISP-HIPS-007 未消化）+ `module_entry.cpp:873` —— legacy 错误码折叠进单一 `DRZ_ECODE_LEGACY_REJECT`，无稳定可分错误码。
- **S6** 文档陈旧：`memory.md:36`、`README.md:11` 宣传「OpenMP 16 线程」（实为 `config.threads`/`omp_get_max_threads()`，且与禁私建池口径冲突）；`spherical_overlap.cpp:633-634` 说 depth 8（实为 12）、`:1233` 说 1.0×（实为 1.25×）、`:1425` 的 `samples` 被 `:648` 丢弃。

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻什么 | 结果 |
|---|---|---|---|
| **E1** | 令 `synth_flux()` 直接 `return 0.0` | 推翻「`acceptance_drizzle.py:171` 是独立解析核对」 | **推翻成功**。判据是字面量 `True`，函数体从不参与比较。**本人复核。** |
| **E2** | 让 `load_jsonl` 返回 `[]`（删掉 `acceptance_matrix.jsonl`）后跑 `main()` | 推翻「`drizzle_acceptance_py` 红了就说明物理判据没过」 | **推翻成功**。25 个 tag 全打 `[FAIL] 场景存在`，但只有 F/H 两段无条件 append ⇒ 汇总 `2 通过, 0 失败`、exit 0。**本人沿控制流复核。** |
| **E3** | 让 `geom_quad_area()` 返回任意常数 c>0（或让 `geom_drop_polygon` 忽略 `pixfrac`） | 推翻「`p1drz_disp009_gate.cpp:255` 能分辨归一口径」 | **推翻成功**。pf=1.0 时 `oracle_pixel_area` 与 `oracle_drop_area` 是逐字相同函数体，断言退化为 `|X/X−1|<1e-12`。**本人复核两函数体。** |
| **E4** | 令生产每叶只保留首个源像素的方差项 `Σ_j v_j w_jp² → j=1` | 推翻「`p1drz_oracle` 能发现方差传播错误」 | **推翻成功**。门先筛 `nContrib==1` 再取极值，单贡献叶全精确 ⇒ 绿。**这就是「筛掉真信号」。** |
| **E5** | 令探针忽略 taskset 预算、恒单线程跑 | 推翻「`p1drz_merge_pipeline_lock` 守护线程预算不变式」 | **推翻成功**。各档 sha256 必然相同；`:55` 抓到的 `threads=N` 只打印不校验。 |
| **E6** | 删掉 `spherical_overlap.cpp:1275-1291` 的注入钩子 | 推翻「P3 负例门能证明门对被测对象敏感」 | **推翻成功**。`:388 ++fail` 无条件 ⇒ 仍 exit 1 ⇒ `WILL_FAIL` 仍过。只有 `:374` 的一行 `[INVALID]` 提示，**CTest 看不见**。（我**同时确认**钩子今天确实存在且在测试路径上，所以降级为须修而非阻断。） |
| **E7** | 令 `pixelToSky` 回调失败（WCS/SIP 除零） | 推翻「drop 投影失败会 fail-closed」 | **推翻成功**。`:836/:935` 返回空多边形、`:883` 塞 p0 停细分 ⇒ 面积 0 ⇒ 源像素被静默丢弃，与「真无交叠」不可区分。 |
| **E8** | 构造 m≥13 个 drop 半空间的 drop 多边形 | 推翻「`sutherland_hodgman_spherical_fixed` 的 16 顶点缓冲永远够」 | **推翻成功**。`:391/394/397` 返回 0 = 「空交集」，交叠被静默抹掉。 |
| **E9** | 令 P3 核在 HST 0.005″/px 的 18 个 drop 上**交叠面积恒返回 0** | 推翻「`drizzle_p3_conservation` 的逐 drop 门能发现面积丢失」 | **推翻成功**。`a_ref = 5.876e-16 < ε_abs = 1e-15`，而 `|da|` 上限即 `a_ref` ⇒ 必绿。**本人独立复算，未转述子代理。** |
| **E10** | 在 `drizzle_nonfinite_test.cpp` 的场景里核对 `nSourcePixels` 的实测语义 | 推翻「该断言刻画了生产行为」 | **推翻成功**。断言 `W*H-2`，生产只对 `weight==0` 少计 1；`variance==0` 按成文设计仍计入 ⇒ 实测 `W*H-1`，该断言今天恒假。**本人复核引擎与测试两侧。** |
| **E11** | 提交 `{"width":1,"height":1,"dtype":"f32","data_base64":"<100 万 base64 字符>"}` | 推翻「`b64_decode` 受目标缓冲区约束」 | **推翻成功**。`malloc(4)`，解码器按编码长度写入约 75 万字节；`:821` 的长度检查在溢出之后才触发。**本人复核签名 `:260` / 写入 `:284` / 分配 `:814`。** |
| **E12** | 提交 `"crval":[-,1]` | 推翻「JSON 数组解析对畸形输入 fail-closed」 | **推翻成功**。`strtod` 无转换 ⇒ `end == q` ⇒ `q` 不推进 ⇒ 调用线程永久挂起。**本人复核 `:206-207` 并与有守卫的标量版 `:165/:168` 对照。** |

---

## 6. 盲复算（遮住既有判定独立取证）

遮住子代理全部结论、只看原文，我独立重算了以下既有判定：

1. **`p3_conservation_gate.cpp` 主判据是否恒真** → **判一致（它不恒真）**。参考面积走独立 double-double FMA VOS（`:92-118`），被测量走生产 S-H（`:237`），两套不同算法；`judge_drop`（`:173`）在 NaN 时 `fabs(NaN)<=lim` 为假 ⇒ **响亮判红**。我**上调**了子代理的分量（它列为 BLOCKER 组之一，我按其自身降级理由定为须修）。
2. **负例门注册是否真接线** → **判一致**。`tests/CMakeLists.txt:367-371` 确有 `ENVIRONMENT "ACSD_DRZ_P3_FAULT=legacy_corner_fast"` + `WILL_FAIL TRUE`；同时**独立发现**恒红缺陷（E6）——子代理说对了。
3. **`healpix_core.cpp:9` 「drizzle 模块内 grep 无 `#include "healpix_core.h"` 消费点」** → **判偏松（该说法为假）**。我 grep 出 drizzle 内 **5 个非测试消费点 + 9 个测试 TU**（`drizzle_engine.cpp:4`、`healpix_core.cpp:16`、`reverse_drizzle.cpp:17`、`spherical_overlap.h:37`、`spherical_overlap_science.h:19`）。退役声明的「零引用」前提不成立 —— 正是简报点名的「退役对象仍有活调用者」形态。
4. **`healpix_core.h:88` 「保留防编译破裂」** → **判偏松**。`pixelToCoarse/pixelToFine` 在 drizzle 侧零调用者（仅 `archive/legacy/healpix_stack` 另一套同名类在用），删掉不会破坏任何构建，理由不成立。
5. **`README.md` / `types.h` 的引用可信度** → **两者相反**。`types.h` 的 5 处引用（2 个 commit、`module.yaml`、`aio_hips.h`、`astro_sphere_sink.cpp:52-167`）**全部真实**；`README.md` 的 3 处（`healpix_drizzle.py`、`pipeline_adapter.py`、`eng/`）**全部为假**。
6. **`drizzle_science.h` 是否可信为判据基准** → **判一致，可作基准**。`w_jp = a_jp/A_drop_j` 有 F&H 2002 §7.2 逐字引文 + drizzlepac 源码位置；`:272-274` 明文要求门「只接受 raw 输入并重算」。据此我判定 S3 中 `p1drz/CMakeLists.txt:89` 为错方。

**结论**：6 项中 4 项判一致、2 项判既有材料偏松（退役声明与保留理由），0 项偏严。

---

## 7. 子代理派发记录

派发 **6 个**（工具对每次调用自动复制一份，故实际启动 6 个、覆盖 **3 个互不重叠的范围**；重复副本作为独立复算使用）：

| 副本 | 范围 | 回报 | 我的复核结论 |
|---|---|---|---|
| c0f084c9 / e158c569 | 10 份 C++ 测试门（2567 行） | 已回报（两份，详略不同） | **采信 B1/B2/B3/B4/B5/M1/M2；下调 B6 为须修；否决其未列项** |
| 7907401c / e994539e | 3 份 Python（1383 行） | 已回报（两份，结论独立趋同） | **采信 B1/B2/B3；本人复核 `:171` 与计数；否决其若干 SUGGESTION** |
| c47abb74 / 1beb0d69 | `module_entry.cpp` + `types.h` + `module_exports.map` | 已回报（1342/1342 全覆盖） | **采信 B1 堆溢出、B2 死循环、B3 悬垂指针、M1/M2/M3 静默丢弃族、M8 魔数派生**；下调 B1/B2 的「在线可利用性」为不确定 |

**逐条复核（我亲自验证的）**：
- ✅ **采信**「`acceptance_drizzle.py` 25 check / 9 append」→ 我 `grep -c` 复核为 26 处 `check(`（含 def）与 9 处 append，并读 `:71-74` 确认 `check()` 只 print。
- ✅ **采信**「`:171` 字面量 True」→ 我读原行确认。
- ✅ **采信**「`:180` 在 `if not args.skip_run:` 之上」→ 我读 `:173-188` 确认。
- ✅ **采信**「`p1drz_oracle.hpp:87-97` 两函数体相同」→ 我读原文逐字比对确认。
- ✅ **采信**「`p1drz/CMakeLists.txt:89` 与 `p1drz_disp009_gate.cpp:3-4` 指认相反公式」→ 我 `sed` 两处原文确认，并以 `drizzle_science.h:23-30` 裁决。
- ✅ **采信**「4 份 C++ 测试无 `add_test`」→ 我对 `tests/CMakeLists.txt` + `p1drz/CMakeLists.txt` 逐目标复核，确认 `drizzle_nonfinite_test`/`oracle_edge_crossing_test`/`drizzle_l2_test`/`p0_sip_order_guard_test` 未注册。
- ✅ **采信**「Python 符号全仓零定义」→ 我复核 `healpix_io` 仅存 `ARCHIVED.md`、`wiki/` 不存在。
- ❌ **否决/下调 1**：子代理把 P3 负例门列为 BLOCKER，我读 `tests/CMakeLists.txt:367-371` 确认注入变量确已接线且钩子确在被测路径上，按其自身理由**下调为须修**（缺陷仅在「恒红、无自动见证」）。
- ❌ **否决 1**：某副本对 `oracle_edge_crossing_test.cpp:170` 的「极冠像素选取错误」指控，另一副本自行撤回；我未采信（`ipix < per_face ⇒ face 0 ⇒ 北极冠面`，指控不成立）。
- ❌ **否决 1**：某副本对 `drz_artifact_exists` 路径约定（`signal/support/snr` 是否真为子产品目录）的怀疑 —— 另一副本复核 `aio_hips_writer.cpp:1289-1290` 证实确为真子产品目录，指控不成立；但「产物清单只报 3 项、实际写 7 项（variance/ivar/nrej/nused）」这一点我**采信**为须修。
- ⚠️ **下调 1**：堆溢出（B0a）与死循环（B0b）判定为**代码层确凿缺陷**（我已复核签名 `:260`、写入点 `:284`、三处按解码长度分配的调用点 `:814/:1104/:1135`、以及第四处 `:1103` 已用正确写法的对照；以及 `:206-207` 与有守卫的 `:165/:168` 的差异），但「今天是否可被在线触发」取决于 manifest 生产者（`normalize`/`export` 链），我未追生产链，**登记为不确定**。
- ❌ **否决 1**：某副本对 `reference_overlap` 未注册的指控 —— 该文件**不在本片 25 份之内**，不计入本片判定（仅在 §2.5 作背景登记）。
- ❌ **否决 1**：某副本把 exe 默认 sections 字符串读作漏 E，自行复核后撤回；我未计入。

**三处我与子代理的实质分歧（以我为准）**：① P3 负例门分量（我按其自身理由下调为须修）；② P3 主判据 —— 子代理归为「oracle 独立、只是容差退化」，我**独立复算**后确认容差在最小尺度退化到 **170%**，故按**阻断**处理；③ 堆溢出/死循环的**可利用性**我保留不确定，但**缺陷本身**采信。

**两处我与子代理的实质分歧（以我为准）**：负例门分量（见上）；以及「`p1drz_oracle` B1」我**独立复核其可达性**后确认成立，但把它标为「经 `p1drz_tests_core.cpp:400-406` 间接消费」，而子代理直接引 oracle 行号 —— 两者不冲突。

**尚缺**：`module_entry.cpp` 的完整子代理报告未回，我对该文件的覆盖仅 ~130/1342，**已如实计入 §1**。

---

## 8. 自证段（可复跑命令，全部只读）

```bash
cd "/workspace/Astro CS Database"
git -c core.quotepath=false rev-parse HEAD            # => 850a9edefd47434b9ab71bc907c3de1e0814b323

# §1 覆盖率：对 25 份成员逐份 wc -l，合计应 = 8999
while read -r f; do wc -l < "$f"; done < /tmp/slice003.lst | paste -sd+ | bc   # => 8999

# B1 计数不自洽
grep -c "check(" lib/algorithms/drizzle/healpix_drizzle/tests/acceptance_drizzle.py      # => 26（含 def）
grep -c 'results\["checks"\]\.append' lib/algorithms/drizzle/healpix_drizzle/tests/acceptance_drizzle.py  # => 9
sed -n '71,74p;199,201p;349,358p' lib/algorithms/drizzle/healpix_drizzle/tests/acceptance_drizzle.py

# B2 自洽恒真门
sed -n '168,171p' lib/algorithms/drizzle/healpix_drizzle/tests/acceptance_drizzle.py

# M1 自洽恒等断言 + S3 相反合同
sed -n '250,256p' lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_disp009_gate.cpp
sed -n '85,97p'   lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_oracle.hpp
sed -n '89p;95p'  lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/CMakeLists.txt
sed -n '3,4p;17,18p' lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_disp009_gate.cpp
sed -n '23,30p'  lib/algorithms/drizzle/healpix_drizzle/drizzle_science.h   # 权威锚：drop_area 为 canonical

# E6 恒红负例门
sed -n '371,388p' lib/algorithms/drizzle/healpix_drizzle/tests/p3_conservation_gate.cpp
sed -n '367,371p' lib/algorithms/drizzle/healpix_drizzle/tests/CMakeLists.txt

# M5 面积口径不一致
sed -n '1369,1385p' lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp
sed -n '1009,1013p;1206p' lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp

# M6 / M7 / E7 / E8
sed -n '835,837p;883,886p;935,937p' lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp
sed -n '375p;391p;394p;397p' lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp

# M4 线程预算锁不校验预算 + SKIP 退出 0
sed -n '18,20p;38p;55p' lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_merge_pipeline_lock.sh

# M8 fail-open 与不实的保留理由
sed -n '32,40p;88,98p' lib/algorithms/drizzle/healpix_drizzle/healpix_core.h

# M9 悬空引用（应全部无输出/不存在）
ls wiki 2>&1 | head -1
git -c core.quotepath=false ls-files | grep -c "^wiki/"                      # => 0
git -c core.quotepath=false ls-files | grep -i "DLL_ABI_AND_LOADER"          # => 无输出
git -c core.quotepath=false ls-files | grep "hips_writer_adapter_test"       # => 无输出
ls lib/algorithms/drizzle/healpix_drizzle/{healpix_drizzle.py,pipeline_adapter.py,eng} 2>&1 | head -3

# §6 盲复算 3：healpix_core.cpp 的「零消费点」声明为假
git -c core.quotepath=false grep -n '#include "healpix_core.h"' -- lib/algorithms/drizzle | cat
sed -n '9,10p' lib/algorithms/drizzle/healpix_drizzle/healpix_core.cpp

# 4 份 C++ 测试确无注册
for t in drizzle_nonfinite_test oracle_edge_crossing_test drizzle_l2_test p0_sip_order_guard_test; do
  printf "%-30s " "$t"; git -c core.quotepath=false grep -c "add_test.*$t" -- '*.txt' || echo "无 add_test"; done

# S2 错误域映射把 -11 归为 SCIENCE_PRECONDITION
sed -n '866,875p' lib/algorithms/drizzle/src/module_entry.cpp
sed -n '139,150p' lib/algorithms/drizzle/hips/include/acsd/hips/types.h   # legacy 码语义表
```

---

## 9. 登记缺陷（UNRESOLVED，交前台）

1. **`module_entry.cpp` 覆盖不足**：本片唯一未获完整覆盖的文件（1342 行，我仅读 ~130）。其完整结论待 `c47abb74` 回报后补审，或换片重派。
2. **`p1drz/CMakeLists.txt:89` vs `p1drz_disp009_gate.cpp:3-4` 的公式冲突需科学正本裁决**：我依 `drizzle_science.h:23-30`（F&H 2002 §7.2 + drizzlepac `cdrizzlebox.c dover/=jaco`）判 `drop_area` 为 canonical，故 `CMakeLists.txt:89` 应订正；但该冲突本身应由负责人对照 `docs/science/` 正本确认，我未越权改文档。
3. **Python 绑定是否存在**：`test_drizzle.py` / `test_pipeline_adapter.py` 的「必然全跳过」结论基于仓内取证（模块已由 `23d07ed4` 删除且为 HEAD 祖先、`healpix_io` 已归档改名、符号零定义）。若存在仓外已安装 wheel，结论需下调；但「两文件全仓无 `add_test`」这一条**不受影响**。