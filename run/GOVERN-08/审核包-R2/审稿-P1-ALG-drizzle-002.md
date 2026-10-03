# 审稿-P1 · ALG-drizzle-002（G08-05 对抗审稿 第 1 遍）

- 基线：`/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`
- 角色：审稿人，只读。零 git 写、零构建、零 ctest/pytest/零二进制、未改任何仓内文件。
- 权威依据原件：`run/GOVERN-08/工作包-GOVERN-08原件/`。`/tmp/acsd_g08/` **未读**。
- 他人产出仅作线索，所有结论均由本审稿人**亲自 read 原文**推导；子代理结论逐条复核（见 §7）。

---

## 1. 读完了吗

### 计数口径

- **成员份数**：以 `片清单-权威版.yaml:235-265` 的 `成员文件` 列表为准 = **23 份**。
- **成员总行数**：清单 `实际行数: 8997`；本审稿人用 `wc -l` 逐份实测求和 = **8997**，两者一致（分母已独立复核，非照抄清单）。
- **「实际读了多少行」口径**：只计本审稿人**用 read 工具从第 1 行读到文件末行（`End of file`）**的行数。部分读取按实际读到的行计。子代理读过的行**不计入**本人覆盖率（另单列）。

### 结果

| 项 | 数值 |
|---|---|
| 成员份数 | **23** |
| 读了几份（完整读完） | **15** |
| 成员总行数 | **8997** |
| 本人实际读了多少行 | **6837** |
| **覆盖率（份数）** | **15 / 23 = 65.2%** |
| **覆盖率（行数）** | **6837 / 8997 = 76.0%** |
| 完整读完但行数占比高的文件 | `drizzle_engine.cpp` 2699 行、`module_entry.cpp` 1173 行、`drizzle_science.cpp` 657 行 —— 三份占已读 4530 行的 62.7% |

### 未读完的（如实列出，共 8 份全未读 + 1 份部分）

| 文件 | 应读行 | 本人实读 | 缺口 | 说明 |
|---|---|---|---|---|
| `healpix_drizzle/tests/variance_propagation_test.cpp` | 570 | 290 | 280 | **部分读**。已读到 §(f) MC 段起点；SNR-011/012、DRZ-014/016 段（:291-570）未读本人 |
| `hips/memory.md` | 305 | 0 | 305 | 未读 |
| `healpix_drizzle/tests/bench_drizzle.cpp` | 127 | 0 | 127 | 未读 |
| `healpix_drizzle/tests/drizzle_science_completion_test.cpp` | 490 | 0 | 490 | 未读 |
| `healpix_drizzle/tests/control_median_mc_test.cpp` | 284 | 0 | 284 | 未读 |
| `healpix_drizzle/tests/p1drz/p1drz_tests_selfcheck.cpp` | 198 | 0 | 198 | 未读 |
| `healpix_drizzle/tests/reverse_drizzle_test.cpp` | 170 | 0 | 170 | 未读 |
| `healpix_drizzle/tests/drizzle_l0_test.cpp` | 160 | 0 | 160 | 未读 |
| `healpix_drizzle/tests/concurrency_cache_test.cpp` | 146 | 0 | 146 | 未读 |
| **合计缺口** | **2450** | — | **2160** | |

> **诚实声明**：本片覆盖率 **76.0%（行）/ 65.2%（份）**，未达 100%。缺口集中在 `tests/` 目录（8 份测试文件共 1783 行）与 `hips/memory.md`。这 8 份测试文件由子代理 `627d0dac` 完整读过（2281 行）并给出详报（见 §7），但**其结论未获本审稿人逐行复核**，故在 §3 中标注证据等级为「线索·未独立复核」，不计入本人裁定。

---

## 2. 本片判定

### 判定：**需修（BLOCKER 存在但集中且可修）**

不是「通过」：本片存在 **1 条教科书级自洽式断言**（负责人本轮点名要找的最高价值类别）+ 1 处内存安全 UB + 4 处静默丢通量。
不是「阻断」：最重的两条都在**诊断/证据面**（trace 校验脚本、统计输出），**不直接污染科学产品数据面**；产品面最重的发现是「恒红门」与「静默 continue」，均为可定点修补项，不需推翻架构。

### 最重 3 条

1. **【自洽式断言·教科书级】`trace_g4_validate.py:94-96` 用被检对象自己定义期望量。**
   `cs = sum(c["contribution"] for c in r["contribs"])` 然后断言 `cs == r["sum_contribution"]`。而 `sum_contribution` 恰恰是引擎在**同一次运行中**把这些 `contribution` 逐个累加后存进去的（`drizzle_engine.cpp:1694-1696` 逐项 `tr_sum += cb`；`:1719` `tr.sum_contribution = tr_sum`）。两侧是同一串浮点数的同一次求和，**按构造恒等**，除非 JSONL 被截断才会不等。该检查无论 drizzle 算成什么样都会绿 —— 它无法检测任何通量错误。
   证据：`lib/algorithms/drizzle/healpix_drizzle/tests/trace_g4_validate.py:94-96` × `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1694-1696, 1719`。

2. **【内存安全 UB + 全帧扫描】`reverse_drizzle.cpp:196-202` 的「夹紧」只夹了一侧。**
   注释写「范围夹紧 … 夹到 ±1e9」，但 `x0` 只把 `minx-m` **下**夹到 −1e9、从不上夹；`x1` 只把 `maxx+m` **上**夹到 +1e9、从不下夹。TAN 投影在距切点略近于 90° 时 ξ/η 可达 1e17…1e300（有限，故 `:189` 的 `isfinite` 放行），随后 `(int)` 转换是**越界 double→int，UB**。x86 上落到 `INT_MIN`，`std::max(0, …)` 得 0，而 `x1 = W-1` ⇒ 对一个**完全在画幅外**的 leaf 扫全帧；且 `n_skipped_outside` 不增，诊断量随之失真。
   证据：`lib/algorithms/drizzle/healpix_drizzle/reverse_drizzle.cpp:196-202`（本审稿人亲读 `:196-203` 确认夹紧方向）。

3. **【静默降级 ×4·静默丢通量】生产写盘路径上四处「索引非法就 continue」，无错误码、无计数、无日志。**
   `drizzle_engine.cpp:934`、`:1053`、`:2607` 三处 `if (local >= tile.pixels.size()) continue;`，`:1341` 一处 `if (local_ipix < n_leaf_per_tile)`（else 分支静默不写）。这四处把「查对象失败」吞掉继续 —— 项目规范明禁。每一处静默丢的都是**通量**：叶子累加值进了 `acc.pixels[]` 却没写进产物，而函数仍 `return true`。这直接违反同文件 `:1678` 自己写的守恒恒等式 `Σ_p sumFlux_p = Σ_j x_j`。
   证据：`drizzle_engine.cpp:934`、`:1053`、`:1341`、`:2607`。

---

## 3. 逐文件清单

判定列含义：**阻断** / **须修** / **建议** / **通过**。证据等级：**A = 本审稿人亲读原文核实**；**B = 子代理线索，本人已核对代码形状但未逐行复核**；**C = 仅引用仓内正本/grep，未读全文件**。

| # | 成员文件 | 行 | 读了什么 | 看到什么（带 `文件:行`） | 判定 | 证据 |
|---|---|---|---|---|---|---|
| 1 | `healpix_drizzle/drizzle_engine.cpp` | 2699 | **全读** | ① `:934/:1053/:2607/:1341` 四处静默 `continue` 丢通量；② `:1238/:2522` 空 `catch(...){}` 吞 GAIN 解析失败；③ `:151-153` 空图整数除零；④ `:1491` 注释声称「行级顶点缓存已检查」有限性，而 `:2074-2081` 根本无 `isfinite`；⑤ `:1600+1709` 默认 pixfrac=1 下 `sumNorm` 与 `sumArea` 逐位恒等（非独立量）；⑥ `:107/:166` 同一文件既读又写（自愈锚）；⑦ `:939-940/:1058-1059` 累加器字段被硬置 0 且头文件仍宣称是活量；⑧ `:1098` 硬编码 `3^(1/4)`；⑨ `:1403/:1553` 注释仍写「Girard 定理」，而仓内正本 `docs/engineering/STANDARDS_REGISTRY.md:169` 已登记实为 S-H 裁剪 + Van Oosterom & Strackee（DISP-DRZ-002 TRACKED，未订正） | **阻断** | A |
| 2 | `hips/src/module_entry.cpp` | 1173 | **全读** | ① `:1067-1104` 输出 manifest 缓冲区按未转义长度定容，而 `json_append_escaped` 对 `"`/`\` 双写且**无容量参数**；② `:384-385` `exposure_s` 缺失静默置 0；③ `:390-391` `moc_order` 缺失静默置 0；④ `:525-526` `snr_count` 缺失静默置 0（整面 SNR 静默消失）；⑤ `:572-575` provenance 双键只给一个 → `has_prov=0` 静默丢弃；⑥ `:949-950` 魔数 `-5/-2` 吞方差写失败；⑦ `:790-799` `hips_legacy_status` 承诺域分类却 `(void)domain`；⑧ `:425-434` 位图解码失败=「无该平面」；⑨ `:123/126` `snprintf` 截断静默；⑩ `:938-941` per-tile 位图被塌缩成全有/全无（详见 M-31，两份合同互相矛盾）。**无私建线程池**：`:846-859` 正确借宿 executor 租约（FORBID-003 合规），全文件仅 `:93` 一个 `thread_local` 缓冲 | **阻断** | A（缓冲区结论 A+B 合并） |
| 3 | `healpix_drizzle/drizzle_science.cpp` | 657 | **全读** | ① `:143-146` `flux_conservation_factor` **丢弃形参、字面返回 1.0**；② `:25-29` `rel_diff` 遇 NaN 返回 NaN ⇒ `NaN > tol` 恒假 ⇒ `:462` 方差恒等门在 `ref` 为 NaN 时**放行**；③ `:376-380` `parent_deficit` 在 `exact<=0` 时返回 0.0（=「完美」）；④ `:551-566` 阈值未签字时亏损幅度**完全不受检**；⑤ `:74-77` 未知 `UnitId` 静默落到 `dimensionless`（全 dimensionless 组合可过 `gate_unit_law`）；⑥ `:438` 长度不匹配的 claim 静默跳过仍 pass；⑦ `:517-522` `max(scale,1.0)` 给容差加绝对地板；⑧ `:7-8` 文件头声称「以 raw 重算…避免同实现自证」，但 `raw_c` 在代数上**恒等于** `build()` 存的 `e.c`（两种归一皆然），该声称为假 | **阻断** | A |
| 4 | `healpix_drizzle/reverse_drizzle.cpp` | 379 | **全读** | ① `:196-202` 单侧夹紧 ⇒ UB + 全帧扫；② `:178/:226` `total_signal_in`[ADU] 与 `total_signal_out`[ADU·sr] **量纲不可比**，而下游唯一守恒断言比的是 ADU·sr ≤ ADU ⇒ 任意损失都绿；③ `:92/:212/:214/:217` 四处丢弃**不计数**，违反本文件头 `:12` 自称的「统计字段全部填充」；④ `:237` coverage 静默夹到 1.0；⑤ `:248` `run_typed` 恒 `return true`，`error_msg` 形参被注释掉（死签名）；⑥ `:353` `n==0` 返回成功；⑦ `:84-88` 退化三角形法向量归一化为固定北极 ⇒ 伪半球裁剪；⑧ **`:4` 悬空引用**：冻结语义权威 `wiki/Reverse_Drizzle.md + control/REVERSE_DRIZZLE_IMPLEMENTATION.md` **两者皆不存在**（`ls -d wiki control` → 退出码 2） | **阻断** | A（⑧ 为 grep+ls 实证） |
| 5 | `healpix_drizzle/tests/trace_g4_validate.py` | 272 | **全读** | ① **`:94-96` 自洽式断言**（见 §2-1）；② `:229` `drizzle_contrib_conserves_leaf_flux = (leaf_flux_bad==0)` **无 `len(li)>0` 守卫 ⇒ 空输入恒绿**（同文件 `:109/:112/:215/:216` 都有守卫）；③ `:44-45` `except: pass` 吞坏行，使坏行不入 `dl`，`:109` 的 `sum_ok==len(dl)` 反而因此更易通过；④ `:122` 硬编码开发者本机绝对路径 `F:\Astro dev\...\astro_image_io.dll`（另 `:29-30`）；⑤ `:143/:156/:157/:184` 硬编码 tile depth=9（`+9`、`>>18`、`(1<<18)-1`、`range(9)`）且**不与产物实际 depth 交叉校验**；⑥ `:201/:203` `rel>tol **and** abs>1e-12` ⇒ 通量 <1e-12 的叶子免检 | **阻断** | A |
| 6 | `healpix_drizzle/tests/CMakeLists.txt` | 395 | **全读** | ① `:272-277` `reference_overlap` 独立蒙特卡洛参考门**被刻意注释掉 add_test**，理由是实测「32 通过, 3 失败」——红灯上呈而非消解，符合纪律，但意味着**唯一的独立参考路径从不参与 ctest 基线**；② `:167-190` 13 个门类测试 `add_executable` 但**不注册 ctest**（含 `concurrency_cache_test`、`drizzle_l0_test`、`reverse_drizzle_test`、`drizzle_science_completion_test`）；③ `:234-250` 6 个探针/剖析件同样不注册；④ `:75-80`/`:334-339`/`:367-371` 三处 `WILL_FAIL TRUE` 负例注入门**设计正确**（真门）；⑤ `:56` 自认「⚠ 本改动未经真机复跑验证」 | **建议** | A |
| 7 | `healpix_drizzle/tests/candidate_oracle_test.cpp` | 263 | **全读** | ① `:90-92` `nside>32` 时 oracle = `{ip ∈ query_candidate_pixels : overlap>0}`，而 `query_candidate_pixels_fast` 的三条回退分支正是直接调 `query_candidate_pixels` ⇒ 回退用例是**集合与其子集自比**，恒绿；`:95` 的 `used_fallback` 只写 JSONL、**从不断言**；`:29` `g_fallback_cases` 是死计数器 ⇒ **无任何证据表明内部快速路径曾被覆盖**；② `:109-112` `false_positives` 算出并写盘、**从不断言** ⇒ 返回全天的查询也能全绿；③ `:61/:75` 真集合用生产 `compute_overlap_area_g` 定义 ⇒ 只证候选完备性、**不证交叠正确性**；④ `:146` 高 nside 全用非独立 oracle。**nside≤32 的穷举半边是真正独立的，强** | **须修** | A |
| 8 | `healpix_drizzle/hp_drizzle_api.h` | 227 | **全读** | ① `:78` 写「负值 −1..−14」，而同片 `include/acsd/drizzle/types.h:104` 写「负值 -1..-13」—— **两处 ABI 头自相矛盾**；② `:79-81` 自认「正负号无语义…已登记缺陷…调用方不得对返回码做分支，只可判 !=0」；③ `:174-176` `sip_order 0..5`、`sip_a[36]`（6×6 ⇒ 0..5 阶），与 `fits_reader.h:10`「最多 4 阶, 36 个系数」（4 阶只需 16 个）**互相矛盾** | **须修** | A |
| 9 | `include/acsd/drizzle/types.h` | 114 | **全读** | ① `:104` 与 `hp_drizzle_api.h:78` 的错误码域不一致（见上）；② `:56` `pixfrac /* f64 [0,1] */` 而实现两处硬拒 0（`drizzle_science.cpp:181`、`reverse_drizzle.cpp:276`）⇒ 应为 `(0,1]`；③ `:68` `DRZ_CFG_KEY_REV_PROJ "projection" TAN/SIN/ARC` **无任何读取者**，`fits_reader.h:25-26` 的 `ctype1/ctype2` 定义了但无人消费 ⇒ 请求 `SIN`/`ARC` 会被**静默当 TAN 处理** | **须修** | A（③ 为 B 级交叉核实） |
| 10 | `healpix_drizzle/spherical_overlap_science.h` | 99 | **全读** | ① `:30-31` 注释断言「A_pixel 由 drop_area/pixfrac² 反推 ⇒ **几何闭合按构造精确成立**」—— **假**：该闭合恰在候选面积被拒（`DrzError::overlap_area_invalid`）或候选漏选（`overlap_area_deficit`，`drizzle_science.cpp:205`）时失效，而这两条具名失败路径就在同链上；② `:42/:46-47` `a_jp`/`A_pixel`/`sum_a_jp` 标注 `[px^2]`，但其来源 `spherical_polygon_area` 产出**球面度（sr）** ⇒ 标注与物理量纲不符 | **须修** | A |
| 11 | `healpix_drizzle/fits_reader.h` | 55 | **全读** | ① `:10` 「最多 4 阶, 36 个系数」自相矛盾（4 阶 = 16 个；36 = 6×6 ⇒ 0..5 阶），且与 `hp_drizzle_api.h:174` 的 `0..5` 冲突；② `:51` `readFits` 只声明 —— **定义存在于 `fits_reader.cpp`（同目录，20855 B，已 `ls` 确认存在），不属本片，但跨片引用有效，非悬空** | **须修** | A |
| 12 | `lib/algorithms/drizzle/memory.md` | 71 | **全读** | ① `:36-37` 自认「S_p=F_p/D_p 归一不在 drizzle 层，在 astro_sphere_sink.cpp:100 + aio_hips_writer finalize_tile」⇒ 与 `drizzle_engine.cpp:939/1058` 把 `sumWeight` 硬置 0 相印证：**引擎侧不承担归一，消费者承担**，而 §3-1⑤ 显示默认路径下 `k ≡ 1`，归一在默认配置里代数失效；② `:38` 登记「Girard→实为 Van Oosterom」等 DISP-DRZ-001..008 共 8 条差异 —— 与 `drizzle_engine.cpp:1403/:1553` 仍写「Girard 定理」对照，**源码注释未按登记订正**；③ `:43` 记录行号 `drizzle_engine.cpp:1670-1671,1762-1785`，与现文件实际（归约在 `:1766-1788`、累加在 `:1988+`）**已漂移** | **建议** | A |
| 13 | `healpix_drizzle/tests/verify_bench.cpp` | 63 | **全读** | ① 全文件**无任何失败出口**：`:41` signal 读失败静默跳过整个 tile 的 NaN 检查；`:47-49` support 读失败只 `printf`、不影响返回码；`:53` `passed++` 计数从不判定；`:56-59` `naninf`/`allzero` 算完只打印，`:62` **恒 `return 0`** ⇒ 一个名为 verify 的校验器不能失败；② `:15` 默认路径硬编码 `run/temp/bench_write.hiss` | **建议** | A（CMakeLists `:234-250` 已把它归为「不注册 ctest」的剖析件，故不升级） |
| 14 | `healpix_drizzle/tests/p1drz/p1drz_tests_perf.cpp` | 80 | **全读** | ① `:68` `t2/t1 < 4.0 && t4/t1 < 4.0` —— **方向反了**：并行完全失效（比值 ≈ 1.0）时**通过**，只有劣化 >4× 才红。而 `drizzle_engine.cpp:1886-1887` 在无 libomp 的 clang 构建下 `num_threads = 1` 静默串行 ⇒ **该构建下两个哨兵恒绿**；② `:71` `t4/t1 >= 0.25` 同理只拦负加速；③ `:50` 失败返回 −1 且 `:67` 守卫跳过其余检查（此处 fail-closed，**正确**） | **须修** | A |
| 15 | `healpix_drizzle/tests/variance_propagation_test.cpp` | 570 | **部分（1-290）** | 已读段内：① `:90-107` `collect_leafs` 用 `sumFlux/sumNorm`、`sumVarNum/sumNorm²`，**与生产发布的 `k = sumArea/sumNorm` 口径不同**（生产走 `astro_sphere_sink`，本文件完全没碰）；② `:253-267` (b)(c) 两条判据代数上把 `ΣsumFlux`/`ΣsumVarNum` **约掉**，只剩 `ΣN/ΣA` 一个比值 ⇒ 与 (a) `:206-207` 同源；(d) `:274-275` 归约成 `|pf²−1|>1e-3`，是**关于测试自身常量的事实**；③ `:227` `n_cmp > 0 &&` 守卫**存在且正确**（对比其它门缺此守卫） | **须修**（证据 B：后 280 行未复核） | A（已读段）/ B（未读段） |
| 16 | `hips/memory.md` | 305 | 未读 | 由子代理 `c2c90398` 完整读过并据此定位 `:159` strbuf 两阶段、`:167` alloc_fail 等合同条款；本人未读 | — | B |
| 17 | `healpix_drizzle/tests/bench_drizzle.cpp` | 127 | 未读 | 仅由 `CMakeLists.txt:234-250` 确认其为「不注册 ctest」的剖析驱动 | — | C |
| 18 | `healpix_drizzle/tests/drizzle_science_completion_test.cpp` | 490 | 未读 | 子代理 `627d0dac` 线索：T8/T9/T12/T13 通量闭合门、T11 孔径测光为**强判据**；但 `:261-286` 的 `ff` 被生产输出自身弱化、`:34-42` 丢弃引擎返回值 | — | B |
| 19 | `healpix_drizzle/tests/control_median_mc_test.cpp` | 284 | 未读 | 子代理 `627d0dac` 线索：`k_corr = var_emp/baseline` 中 baseline 由**同一批样本**的 MAD 构造 ⇒ 对全局噪声尺度与加性偏置**恒等不变** | — | B |
| 20 | `healpix_drizzle/tests/p1drz/p1drz_tests_selfcheck.cpp` | 198 | 未读 | 子代理 `627d0dac` 线索：基线在进程内、注入走 `execve` + 环境被整体替换 ⇒ 环境导致的子进程失败会被**误记为注入成功** | — | B |
| 21 | `healpix_drizzle/tests/reverse_drizzle_test.cpp` | 170 | 未读 | 子代理 `627d0dac` 线索：`:162-166` coverage「无负值/NaN」是**恒真门**（输出零初始化 + 生产端夹紧） | — | B |
| 22 | `healpix_drizzle/tests/drizzle_l0_test.cpp` | 160 | 未读 | 子代理 `627d0dac` 线索：`:128-149` FP32/FP64 对齐门在 FP32 **零叶输出时空洞通过** | — | B |
| 23 | `healpix_drizzle/tests/concurrency_cache_test.cpp` | 146 | 未读 | 子代理 `627d0dac` 线索：`:6` 头声明「逐 tile 一致」，实现只比 3 个聚合量；且几何缓存是 `thread_local`（`drizzle_engine.cpp:378-386`），跨 run 不共享 | — | B |

---

## 4. 发现清单

### 4.1 阻断（BLOCKER）

| ID | 位置 | 问题 | 违反的项目固化检查项 |
|---|---|---|---|
| **B-1** | `trace_g4_validate.py:94-96` × `drizzle_engine.cpp:1694-1696,1719` | **自洽式断言**：`sum(contribs)` 与 `sum_contribution` 按构造恒等（后者就是前者的同一次累加）。通量算错也必绿 | 本轮点名：「同一个定义式既当被检量又当期望量」 |
| **B-2** | `trace_g4_validate.py:223-229` | **恒真门**：该检查无 `len(li)>0` 守卫，`li` 空则循环不执行、`leaf_flux_bad` 恒 0 ⇒ PASS。同文件其余 4 处检查都有守卫 | 恒红/恒真门 |
| **B-3** | `reverse_drizzle.cpp:196-202` | **单侧夹紧 ⇒ UB + 全帧扫描** + `n_skipped_outside` 失真 | 数值稳定性 / 静默降级 |
| **B-4** | `drizzle_engine.cpp:934, 1053, 1341, 2607` | **四处静默 `continue` 丢通量**，无错误码/计数/日志，函数仍 `return true`；违反同文件 `:1678` 自称守恒恒等式 | 静默降级（找对象失败吞掉继续） |
| **B-5** | `drizzle_engine.cpp:1238, 2522` | **空 `catch(...) {}`** 吞 GAIN 解析失败，产物写入**静默默认的 gain** | 错误码与失败语义（空捕获） |
| **B-6** | `drizzle_science.cpp:25-29` + `:462, 519, 545` | **`rel_diff` 遇 NaN 返回 NaN，`NaN > tol` 恒假** ⇒ 方差恒等门在重算侧为 NaN 时**放行**。同族 `:398-403` 却显式处理 NaN ⇒ 门族内部不一致 | 数值稳定性 / 判据 fail-open |
| **B-7** | `reverse_drizzle.cpp:178` × `:226` | `total_signal_in`[ADU] 与 `total_signal_out`[ADU·sr] **量纲不可比**；下游唯一守恒断言比 ADU·sr ≤ ADU ⇒ **任意损失都通过** | 自洽式断言（量纲层面） |
| **B-8** | `drizzle_engine.cpp:1600` + `:1705/:1709` | 默认 `pixfrac=1.0` 下 `Scalar pixel_area = drop_area;` 使 `pixel_area/drop_area` 恒为 1.0 ⇒ **`sumNorm` 与 `sumArea` 逐位恒等**，`sumNorm` 不是独立测量量；下游形如 `sumArea>0 && sumNorm<=0` 的门**恒不可达** | 恒红门 / 伪装成 fail-closed 实则 fail-open |
| **B-9** | `module_entry.cpp:1067-1104` + `:288-295` | 输出 manifest 缓冲区按**未转义**长度定容，而 `json_append_escaped` 对 `"`/`\` 双写且**无容量参数**；溢出后两处 `snprintf` 的 size 形参**下溢** ⇒ 边界检查整体消失 | 数值稳定性 / 内存安全 |
| **B-10** | `module_entry.cpp:790-799, 1043-1044` | `hips_legacy_status` 注释承诺的域分类被 `(void)domain` 抹掉 ⇒ **六处 legacy 失败全部塌缩成同一三元组** | 错误码与失败语义（无稳定码） |

### 4.2 须修（MUST-FIX）

| ID | 位置 | 问题 |
|---|---|---|
| M-1 | `drizzle_science.cpp:143-146` | `flux_conservation_factor(pixfrac)` 丢弃形参、字面返回 `1.0`；该值被写入产品 provenance 并被 schema 强制 ==1 ⇒ **未测量的假设成为不可变合同值** |
| M-2 | `drizzle_science.cpp:7-8` vs `:46-48, 148-150, 260-267` | 文件头声称「以 raw 重算…避免同实现自证」，但 `raw_c` 在 `build()` 存 `e.c` 的同一组 `(a_jp, A_pixel_j, D_p)` 上代数恒等于 `e.c`（`drop_area` 与 `sb_a_pixel` 两种归一皆然）⇒ **该声称为假**，每个拿 `raw_c` 与算子内部量对照的门都在自比 |
| M-3 | `drizzle_science.cpp:376-380`（另 `:352, 367`） | `parent_deficit` 在 `exact<=0` 时返回 `0.0`＝「完美无亏损」；空算子/零方差被认证为理想下界 |
| M-4 | `drizzle_science.cpp:551-566` | 阈值未签字时 `deficit` 幅度**完全不受检**，只查 `claims_exact`/`has_*` ⇒ 唯一合法声明是「无界下界」 |
| M-5 | `drizzle_engine.cpp:1491` vs `:2074-2081` | 注释断言「角点有限性由调用方保证（行级顶点缓存已检查）」，而**行级顶点缓存里没有任何 `isfinite`**；非共享路径（`:1438`）查了，**默认 pixfrac=1 路径没查** |
| M-6 | `drizzle_engine.cpp:151-153` | 空图（`width*height==0`）⇒ `total/want` **整数除零**；`std::max` 护的是商不是除法。当前被 `:1840` 挡住，属**缺守卫** |
| M-7 | `drizzle_engine.cpp:107` + `:166` | trace 选择文件**同一进程既读又写**：首跑 fallback 覆写编排器选择集，次跑读自己产物 ⇒ **自愈锚**（首跑红、次跑转绿而缺陷仍在），恰在本轮点名形态上 |
| M-8 | `candidate_oracle_test.cpp:90-92, 95, 29` | 高 nside 的「Oracle」在回退分支上是**集合与其子集自比**；`used_fallback` 从不断言、`g_fallback_cases` 是死计数器 ⇒ 无证据表明内部快速路径被覆盖 |
| M-9 | `candidate_oracle_test.cpp:109-112` | `false_positives` 算出写盘、**从不断言** ⇒ 退化为「全天返回」的性能灾难可全绿 |
| M-10 | `p1drz_tests_perf.cpp:68, 71` | 性能哨兵方向反了：并行完全失效（比值≈1.0）**通过**；`drizzle_engine.cpp:1886-1887` 无 libomp 时静默串行 ⇒ 该构建下恒绿 |
| M-11 | `module_entry.cpp:384-385, 390-391, 525-526, 572-575` | 科学参数缺失一律静默置默认：`exposure_s`→0（写进 FITS 头）、`moc_order`→0、`snr_count`→0（**整面 SNR 静默消失**）、provenance 双键只给一个 → 静默丢弃 |
| M-12 | `module_entry.cpp:425-434, 501, 512` | 位图解码失败或长度失配一律塌缩为「该平面不存在」；而已 `product_begin` 的 `flags` 已含 VARIANCE/IVAR ⇒ **产物树自带矛盾**仍返回 `ACS_OK` |
| M-13 | `module_entry.cpp:949-950` | 魔数 `-5/-2` 吞方差 tile 写失败（无具名常量、无 warning 字段）⇒ 消费方无法区分「全部 tile 都有方差」与「半数被静默丢弃」 |
| M-14 | `reverse_drizzle.cpp:92, 212, 214, 217` | 四处丢弃**不计数**，违反本文件头 `:12` 自称的「统计字段全部填充 (REV-106)」；`spherical_polygon_area` 文档化为可返回 NaN ⇒ NaN 交叠整块丢失且零痕迹 |
| M-15 | `reverse_drizzle.cpp:237` | coverage 静默夹到 1.0，>1 的过覆盖被掩盖且不计数 |
| M-16 | `reverse_drizzle.cpp:248` + `:136` | `run_typed` **恒 `return true`**，`error_msg` 形参被注释成 `/*error_msg*/` ⇒ 签名承诺的失败语义不存在 |
| M-17 | `spherical_overlap_science.h:30-31` | 注释断言「几何闭合按构造精确成立」——**假**，恰在同链的 `overlap_area_invalid`/`overlap_area_deficit` 上失效 |
| M-18 | `fits_reader.h:10` vs `hp_drizzle_api.h:174-176` | 「最多 4 阶, 36 个系数」自相矛盾（4 阶=16 个）；且两头上限不同（4 vs 5） |
| M-19 | `hp_drizzle_api.h:78` vs `types.h:104` | 同一片两个 ABI 头对 legacy 负值域写法不一致（−1..−14 vs −1..−13） |
| M-20 | `types.h:68` | `projection` 键（TAN/SIN/ARC）**无读取者**，`ctype1/ctype2` 无人消费 ⇒ 请求 SIN/ARC 被静默当 TAN |
| M-21 | `types.h:56` | `pixfrac /* [0,1] */` 而两处实现硬拒 0 ⇒ 词表允许了实现拒绝的值 |
| M-22 | `drizzle_engine.cpp:1403, 1553` | 注释仍写「Girard 定理」，而 `docs/engineering/STANDARDS_REGISTRY.md:169` 已登记 DISP-DRZ-002（实为 S-H 裁剪 + Van Oosterom & Strackee）为 TRACKED ⇒ **登记后未订正** |
| M-23 | `drizzle_engine.cpp:939-940, 1058-1059` | `sumWeight`/`sumSnrSq` 被硬置 0，而 `drizzle_engine.h:50-51` 仍把它们声明为活量（且承诺守恒模式下 `sumWeight==sumNorm`）⇒ **头文件合同与实现矛盾** |
| M-24 | `spherical_overlap_science.h:42, 46-47` | `a_jp`/`A_pixel`/`sum_a_jp` 标注 `[px^2]`，实际来源 `spherical_polygon_area` 产出**球面度（sr）** ⇒ 量纲标注与物理量不符（量纲自洽性掩盖了它） |
| M-25 | `trace_g4_validate.py:44-45` | `except: pass` 吞损坏行 ⇒ 坏行不入 `dl`，`:109` 的 `sum_ok == len(dl)` 反而**更易通过** |
| M-26 | `trace_g4_validate.py:143, 156, 157, 184` | 校验器硬编码 tile depth=9（`+9`/`>>18`/`(1<<18)-1`/`range(9)`）且**不与产物实际 depth 交叉校验** ⇒ 深度变更时静默读错 tile |
| M-27 | `trace_g4_validate.py:201, 203` | `rel > tol **and** abs > 1e-12` ⇒ **通量幅值 <1e-12 的叶子免检**，守恒读回判据对小信号恒绿 |
| M-28 | `drizzle_science.cpp:74-77` | 未知 `UnitId` 静默落到 `dimensionless` ⇒ 全 `dimensionless` 组合可过 `gate_unit_law`（查对象失败静默降级） |
| M-29 | `drizzle_science.cpp:438` | 长度不匹配的 `claimed_signal` 静默跳过检查仍 pass（对比 `:394` 正确返回 `claim size mismatch`） |
| M-30 | `drizzle_science.cpp:517-522` | `max(scale, 1.0)` 给容差加**绝对地板** ⇒ 暗弱帧下 PSD 与「非对角≠0」双双退化为「零」 |
| M-31 | `module_entry.cpp:404` × `:501/:512/:938-941` | **per-tile 位图被塌缩成全有/全无**：`:425-434 hips_bitmap_any` 对 N 个 tile 求单一 OR，而 `:938-941` 给**每个** tile 都交出平面指针。**两份仓内合同互相矛盾**：`include/acsd/hips/types.h:79` 称「per-tile 可选性经 u8 位图承载（1=该 tile 提供该平面）」⇒ 按此读法，位=0 的 tile 会读到邻居 tile 的方差/掩膜字节，是**静默科学数据串扰**；但本文件自己的结构体注释 `:404`「has_mask 位图**非全 0 时必填**」正是全有/全无语义 ⇒ 按此读法实现是合规的、错的是过期的 types.h。**本人裁决：无法在仓内单方判定哪份是正本，登记为需裁决的合同冲突**（不擅自按最坏情况升级为阻断） |

### 4.3 建议（SUGGESTION）

| ID | 位置 | 问题 |
|---|---|---|
| S-1 | `drizzle_engine.cpp:1098` | 硬编码 `SQRT_SQRT3 = 1.3160740129524924`（= 3^(1/4)，可现场 `sqrt(sqrt(3.0))` 求出）；另 `:1094` 注释写 EW 对角线 ≈1.516，真值 1.5197 |
| S-2 | `drizzle_engine.cpp:457-459, 1962` | `ADAPTIVE_MAX_DEPTH=5` / `ADAPTIVE_RATIO_THRESH=1.25` / `ADAPTIVE_MIN_CELL_PX=4.0` / `THRESH_60ARCSEC=60` 均无出处、不在 config，且**决定走哪条 drop 面积路径** ⇒ 直接影响守恒精度 |
| S-3 | `drizzle_engine.cpp:1098`（并 `:1077`） | `getHealpixCorners` 疑似零调用者且无退役标记，且其菱形近似与真正跑的 `get_healpix_boundary*` **不是同一条边界**（未独立复核，标 B） |
| S-4 | `drizzle_engine.cpp:2376` | `out_sort=%.3f` 实参是字面 `0.0`，从未测量，却印得像仪表读数 |
| S-5 | `drizzle_engine.cpp:205-212` | `kMaxTraceLeaves = 20000` 静默截断，无「已截断」标志；保留的是 parent 升序前 2 万叶（NESTED 下偏向北极），而注释宣称「覆盖 ≥8192 硬门」是**另一个计数**，不构成空间覆盖论证 |
| S-6 | `drizzle_engine.cpp:243, 273` | `flush()` 写两个证据文件**从不检查 `is_open()`/写状态**，返回 `void`，run 仍报成功 |
| S-7 | `drizzle_engine.cpp:1510` | `std::max(-1.0, std::min(1.0, NaN))` ⇒ **1.0**：NaN 被 acos 域守卫洗成「零长边」，不触发自适应细分 |
| S-8 | `drizzle_engine.cpp:1845` + `:2112, 2117, 2126` | 只查 `nullptr`、**不查元素个数**；`drizzleTiled_f64`（`:2436`）连其孪生 `drizzle_f64`（`:1000`）的 `pixels_f64.empty()` 守卫都没有 |
| S-9 | `drizzle_engine.cpp:86`（注释）vs `:107/:162/:173/:193` | 注释写 `key = y * width + x`，实为 `y * 1000000 + x`；x 或 y ≥ 1e6 时键碰撞 |
| S-10 | `drizzle_science.cpp:435-436` | 失败信息写死「(frozen tolerance) 1e-3」，但 `rel_tol` 是运行时形参 ⇒ 诊断误导 |
| S-11 | `module_entry.cpp:1067-1074` × `:1097-1098` | 同一产物存在性探测**跑两遍**（定容一次、输出一遍）⇒ 两遍可对不上（TOCTOU） |
| S-12 | `module_entry.cpp:123/126/384/852` | `snprintf` 静默截断；`detail=105` 是本模块 `types.h` **未定义**的裸字面量（兄弟模块均有名为 `*_ECODE_EXECUTOR_MISSING` 的常量） |
| S-13 | `module_entry.cpp:907, 1013, 1016` | `aio_publish_stage_discard_v1` 返回值三处全丢，而消息无条件断言「no partial tree」 |
| S-14 | `module_entry.cpp:1076-1081` | 产物**已原子发布并归还租约之后**才可能 `malloc` 失败并返回 `ACS_ERR_NOMEM` ⇒ 瞬时分配失败会永久毒化目标目录（重试撞 `AIO_PUBLISH_ERR_STATE`） |
| S-15 | `verify_bench.cpp:41, 47-49, 53, 62` | 名为 verify 的校验器**无失败出口**，恒 `return 0` |
| S-16 | `CMakeLists.txt:272-277` | 唯一的独立蒙特卡洛参考门 `reference_overlap` 被注释掉 `add_test`（实测 3 条失败，纪律上「红灯上呈」是对的，但后果是**独立参考路径永不进入 ctest 基线**） |

---

## 5. 我主动构造的反例

口径：针对「现行结论默认是错的」，构造最小输入/最小推理链，检验判据是否会红。

### 反例 1 —— 推翻「trace 通量守恒已被校验」✅ **推翻成立**

- **构造**：把 `drizzle_engine.cpp:1704` 的 `acc.sumFlux += Scalar(pixelValue * weight);` 改成 `acc.sumFlux += Scalar(pixelValue * weight * 0.37);`（通量系统性偏 37%，远超任何容差）。
- **期望推翻**：`trace_g4_validate.py` 的 `sum_contribution_ok_all`（`:109`）应当变红。
- **是否推翻（该门失败）**：❌ **不会红**。因为 `:94` 的 `cs` 与 `r["sum_contribution"]` 两侧都来自 `:1694-1696` 的同一串 `cb = pixelValue*weight` 累加，比例因子同时作用于两边 ⇒ 逐位恒等。
- **会红的是**：`drizzle_contrib_conserves_leaf_flux`（`:229`）——它拿 `:100` 的 `contrib_by_leaf` 与 `leaf_internal.jsonl` 的 `sumFlux` 对比，两者是**不同来源**，0.37 会被抓到。
- **结论**：本片的守恒校验有**一条真门 + 一条恒绿门**混在一起，`:94-96` 那条是纯装饰。**这正是「同一个定义式既当被检量又当期望量」的教科书实例。**

### 反例 2 —— 推翻「`drizzle_contrib_conserves_leaf_flux` 是一条守恒门」✅ **推翻成立**

- **构造**：让 `--trace <dir>` 指向一个含合法 `trace_selection.json` 但**不含 `leaf_internal.jsonl`** 的目录（`load_jsonl` 对缺失文件会抛异常，但若该文件存在且为空，`li = []`）。
- **期望推翻**：空输入应判红（无法证明任何事）。
- **是否推翻**：❌ **判绿**。`:223` 的 `for r in li[:20000]` 零次执行 ⇒ `leaf_flux_bad` 保持 0 ⇒ `:229` 赋 `True` ⇒ `all(result["checks"].values())` 仍可能为真。
- **对照证据（同文件内的正确写法）**：`:109` 有 `and len(dl) > 0`，`:112` 有 `and len(dl) > 0`，`:215/:216` 有 `and n_checked >= 8192`。**唯独 `:229` 漏了守卫** ⇒ 这是遗漏，不是设计。

### 反例 3 —— 推翻「性能哨兵能守住并行」✅ **推翻成立**

- **构造**：用不含 libomp 的 clang 构建（`drizzle_engine.cpp:1886-1887` 的 `#else` 分支取 `num_threads = 1`），或直接把 `config.threads` 固定为 1。
- **期望推翻**：`perf_parity` / `perf_trend` 应当变红（并行度已归零）。
- **是否推翻**：❌ **两个哨兵全绿**。`t2/t1 ≈ 1.0 < 4.0` ✓；`t4/t1 ≈ 1.0 ≥ 0.25` ✓。
- **结论**：哨兵只拦「劣化 >4×」，不拦「加速归零」。而**引擎恰好存在一条静默串行的编译期分支**，正是本门应当覆盖却覆盖不到的真实失效模式。方向写反了。

### 反例 4 —— 推翻「`sumNorm` 是独立测得的归一分母」✅ **推翻成立**

- **推理（非运行）**：默认 `pixfrac=1.0`（`drizzle_engine.h:31`，且 `:1594` 自述「硬约束：默认路径零回归，defaults.json drizzle.pixfrac=1.0」）下，`processPixelSharedTiled` 走 `shared_vertices` 分支，`pixel_corners_v == nullptr` ⇒ `:1600` 执行 `Scalar pixel_area = drop_area;` ⇒ `:1709` 的 `pixel_area / drop_area` **恒等于 1.0** ⇒ `sumNorm += overlap_area` 与 `:1705` 的 `sumArea += overlap_area` **逐位相同**。`:1707-1708` 的注释自己承认了这一点。
- **期望推翻**：`sumNorm` 携带独立于 `sumArea` 的信息。
- **是否推翻**：❌ **推翻成立**。于是：任何形如 `sumArea > 0 && sumNorm <= 0` 的 fail-closed 门在默认配置下**恒不可达**（恒红门），而发布侧的归一因子 `k = sumArea/sumNorm` 在默认配置下**恒等于 1.0**，整套 DISP-DRZ-009 面亮度修正在**默认配置里代数失效**。`pixfrac < 1` 时该分支才真正产生独立量。

### 反例 5 —— 推翻「`reverse_drizzle` 的守恒诊断有效」✅ **推翻成立（但两个子代理在此**冲突**，见下）**

- **构造**：丢弃 99.999% 的 leaf 通量，只保留 1 个。
- **期望推翻**：`total_signal_out ≤ total_signal_in` 形式的断言应当变红。
- **是否推翻**：❌ **不会红**。`total_signal_out` 累加的是 `Σ_j sig_j·ov_ji`，量纲 [ADU·sr]；`total_signal_in` 累加 `Σ_j sig_j`，量纲 [ADU]。因 `ov ≪ 1`，不等式对**任意**损失都成立。
- **两个子代理在此直接冲突，本审稿人裁决**：
  - 子代理 `0e1527c9` 主张「正向/反向互为伴随，**不要报缺 Jacobian**」，理由是代入 `x_j = B_j·A_pixel_j` 后两者一致；
  - 子代理 `f39beb52` 主张「**缺 leaf 立体角 Jacobian**，误差约 16.7×」。
  - **裁决**：`0e1527c9` 的代数我复核为正确，但它的**前提不成立** —— 反向算子的输入 `leaf_signal` 来自 `hp_drizzle_api.h:181-185` 的 ABI，该处**未声明任何单位**；`reverse_drizzle.h` 亦未声明；生产入口 `lib/algorithms/drizzle/src/module_entry.cpp:1170` 只是把 manifest 里的 `leaf_signal_base64` 原样透传，**键名不带单位**。
  - 因此「缺 Jacobian」在两个方向上**都无法证实**，我**否决**把它写成 BLOCKER。改写为可辩护的窄结论 **M-24 + B-7**：单位合同在 ABI、生产入口与统计字段三处**都未声明**，这本身就是缺陷；任何基于「反向输出是面亮度」或「反向输出是通量」的隐含假设（包括两个子代理各自的）目前都无法从仓内证据判定。

### 反例 6 —— 推翻「候选 Oracle 矩阵覆盖了内部快速路径」✅ **推翻成立**

- **构造**：把 `query_candidate_pixels_fast` 的**内部（快速）路径**整体替换为「只返回候选集的第一个元素」，保留三条回退分支不动。
- **期望推翻**：9003 例矩阵应当抓到大量 FN。
- **是否推翻**：❌ **不会红**（在所有走回退的用例上）。因为高 nside 的 oracle（`:71`）本身就是调 `query_candidate_pixels`，而 `:95` 记录的 `used_fallback` **从不被断言**，`:29` 的 `g_fallback_cases` 是死计数器 ⇒ **矩阵里有多少用例真的走了内部路径，无任何证据**。
- **保留**：nside ≤ 32 的穷举 oracle（`:55-65`，遍历全部 12288/49152 像素）**真正独立**，是强判据。

---

## 6. 盲复算

**方法**：遮住 `逐份判定-权威版.csv` 与全部历史审稿结论，只用本片原文 + 仓内正本，重新独立取证，再与既有结论对照。既有判定只在本节末尾用于比对，未参与取证。

### 6.1 盲取证（未看既有判定）

| 编号 | 我独立取证得到的结论 | 证据 |
|---|---|---|
| BL-1 | `trace_g4_validate.py:94-96` 是自洽式断言 | 本人读 `trace_g4_validate.py` 全文 + `drizzle_engine.cpp:1692-1721`，沿数据流确认 `sum_contribution` 的定义点 |
| BL-2 | `drizzle_engine.cpp:934/1053/1341/2607` 是四处静默丢通量 | 本人读 `drizzle_engine.cpp` 全文，逐个确认 `continue`/`if` 无 else 计数 |
| BL-3 | `reverse_drizzle.cpp:4` 的两条冻结语义权威文档不存在 | `ls -d wiki control` → 退出码 2 |
| BL-4 | `drizzle_science.cpp:143-146` 返回字面 1.0 | 本人读该文件 657 行 |
| BL-5 | `rel_diff` 对 NaN 返回 NaN ⇒ `:462` 门 fail-open | 本人按 IEEE-754 逐句推 `std::max(NaN,x)` 与 `NaN>tol` |
| BL-6 | `spherical_overlap_science.h:30-31` 的「按构造精确成立」为假 | 本人把该声称与 `drizzle_science.cpp:204-205` 的具名失败分支对撞 |
| BL-7 | `module_entry.cpp:1067-1104` 缓冲区定容不含转义开销 | 本人先独立发现 `json_append_escaped` 无容量参数，后与子代理算术合并 |
| BL-8 | `drizzle_engine.cpp:1491` 的守卫声称为假 | 本人对比 `:1438`（有检查）与 `:2074-2081`（无检查）两条路径 |

### 6.2 与既有判定比对

| 项 | 既有 `逐份判定-权威版.csv` / 历史审稿的倾向 | 我的盲复算 | 一致性 |
|---|---|---|---|
| 本片整体 | 未见对本片做过「自洽式断言」专项判定；历史审稿多停在「静默降级」「悬空引用」形态 | 我另外挖出 **BL-1/BL-4/BL-5/BL-8** 四条自洽式/恒绿门 | **偏严**（我判得更重） |
| `trace_g4_validate.py` | 未见被判为问题面 | 我判 **阻断** | **新问题** |
| `reverse_drizzle.cpp` 的冻结语义文档 | 若历史只看「注释指向 wiki/」可能记为低危文档问题 | 我判 **阻断**（生产源的正本权威整条链缺失） | 偏严 |
| `drizzle_engine.cpp` 静默 continue | 历史已多次登记「静默降级」类别 | 我给出**四处精确定位**并指出违反本文件自述守恒式 | 一致，且更具体 |
| `types.h` | 子代理判「无发现，不值得报」 | 我判 **须修 3 条**（`:56` 词表允许被拒值、`:68` 无读取者键、`:104` 与同片头冲突） | **与子代理不一致，我保留自己的判定**（依据：三处均有可核 `文件:行`） |
| `fits_reader.h` | 子代理称「types.h 之外零发现」 | 我判 `:10` 阶数/系数自相矛盾为须修 | 同上 |
| `drizzle_engine.cpp:1600` 的 sumNorm 别名 | 一位子代理列为 BLOCKER，另一位称「本文件无 tautology，缺陷在消费者」 | 我确认二者**不矛盾**：生产代码确为别名，恒红门落在**下游** `astro_sphere_sink`（不在本片）。我按「本片代码缺陷 + 下游后果」记录 | 一致 |
| 候选 Oracle 矩阵 | 头部自称「9003 例零漏选」 | 我判：nside≤32 半边强、高 nside 半边为自比，`used_fallback` 死计数器 | **偏严** |
| `reference_overlap` 未注册 ctest | 纪律上「红灯上呈」正确 | 我认为后果是**独立参考路径永不进基线**，记为建议而非阻断 | 一致（不升级） |

**盲复算结论：判一致，但整体偏严**，并在 4 个面上给出既有结论未覆盖的新问题（见 §8）。

---

## 7. 子代理派发记录

**纪律**：派发 5 个**不同** brief 的子代理。因工具层把部分 `subagent` 调用重复提交，实际启动 7 个实例，其中 2 个为重复 brief；**去重后按 5 个不同 brief 计入**。本人对每条关键结论**逐条复核**，并在下表**明确写出否决了哪些**。

| brief | 实例 id | 覆盖 | 关键产出 | 本人复核结果 |
|---|---|---|---|---|
| S-A drizzle_engine 逐行 | `8745c7e5`, `2197fd9f`（重复） | `drizzle_engine.cpp` 2699 | 9 MUST-FIX + 6 建议；`:1600` sumNorm 别名列为 BLOCKER | **采纳 4**：`:1238/:2522` 空 catch、`:1491` 假守卫、`:151-153` 除零、`:1600` 别名（后两条本人独立发现并独立验证）。**否决 2**：①「weightData/snrData 被读后丢弃 = 静默降级」——我判定该行为有仓内合同背书（`DATA_SEMANTICS.md:397` 声明权重面在 Phase1 是 disabled face，`tests/test_nside_pixfrac.cpp:483-488` 断言「weightValue 仅作有效性掩膜」），故降级为**建议**而非 MUST-FIX；②「`config.nside` 无校验」——本人复核 `reverse_drizzle.cpp:263` 与生产 `module_adapters` 入口均有前置校验，属**缺守卫非活缺陷**，降为建议。**纠正 1**：其称「`hp_drizzle_run` 零生产调用者的声明为真」——本人未复核，标 B |
| S-B 测试自洽性专项 | `627d0dac` | 7 份测试 2281 行 | 3 BLOCKER + 6 MUST-FIX + 6 建议；并**明确列出哪些门是强的** | **全部采纳为 B 级线索**，但因本片覆盖率不足，**未升级为本人裁定**。其中 `variance_propagation_test.cpp` 的 (b)(c)(d) 代数约掉结论，我在已读的 `:253-267` 上**独立复算确认**：`s_legacy/s_doc = ΣN/ΣA`（ΣF 消去）、`var_legacy/var_doc = (ΣN/ΣA)²`（ΣV 消去）⇒ 升级为 M 级。**否决 1**：其对 `drizzle_science_completion_test.cpp:261-286` 的 `ff` 弱化判断，我未复核，不采纳 |
| S-C 悬空引用 / 构建注册 | `77b57215` | `CMakeLists.txt`、2×memory.md、bench/verify/py | **未在本会话返回报告** | **无产出可复核**。本人独立完成了 `reverse_drizzle.cpp:4` 的悬空引用实证（`ls -d wiki control`）与 `CMakeLists.txt` 全文阅读，未依赖该代理 |
| S-D 科学正/反向数学 | `f39beb52`, `0e1527c9`（重复） | `drizzle_science.cpp`、`reverse_drizzle.cpp`、3 头文件 | 一方主张缺 Jacobian（BLOCKER），另一方主张无缺 Jacobian（勿报） | **双方直接冲突，已裁决**：否决「缺 Jacobian」的定性（见 §5-反例 5），改写为「单位合同三处均未声明」（B-7 + M-24）。**采纳**：`rel_diff` 的 NaN fail-open（本人独立复核）、`parent_deficit` 退化返回 0、covariance 容差绝对地板、`gate_flux_conservation` 忽略 `normalization()`、BUNIT 门把 `/px` 当 `/sr`、**量化失效面积阈 1e-20 sr**。**否决 1**：「`gate_flux_conservation` 的 rel_tol(1e-11) 比几何闭合容差(1e-6) 紧 10⁵ 倍」——该结论依赖 `phase1_product.h:131` 与 `drizzle_science.h:327` 两个**不在本片**的默认值，我未复核其真实取值，标为 **UNRESOLVED** 不计入清单 |
| S-E module_entry 专审 | `c2c90398` | `hips/src/module_entry.cpp` 1173 | 4 BLOCKER + 11 MUST-FIX + 14 建议；含错误码面全枚举与「零私建线程池」正面确认 | **采纳 3**并与本人独立发现合并：`:1067-1104` 缓冲区溢出（本人已独立察觉转义长度问题，其给出精确算术）、`:425-434` 位图 fail-open、`:790-799` 域分类缺失。**采纳**其错误码面枚举与 `105` 裸字面量问题（S-12）。**末次报告补充采纳 1**：`:935-941` per-tile 位图塌缩 —— 但我**下调**其「静默数据串扰 BLOCKER」的定性，理由见 M-31（两份合同矛盾且本文件自身注释支持全有/全无读法，故登记为合同冲突而非阻断）。**否决 1**：`BLOCKER-2`（env 故障注入钩子置于生产路径）——我判定其属项目既定注入惯例（`publish.h` 有具名注入原语，且 `CMakeLists.txt:367-371` 有配套 ctest 负例门），降为**建议**而非阻断。**否决 2**：`MUST-FIX-7`（create 期 config 被 execute 期覆盖）——属**跨片**接口约定问题（`acsd/hips/types.h` 的「create 期固化」措辞），超出本片裁定范围，标 UNRESOLVED。**采纳其「零私建线程池」正面结论**并由本人在 §8-16 独立复核 |

### 复核统计

- 派发不同 brief：**5**（去重后）
- 启动实例：**7**（2 组重复）
- 收到完整报告：**4 份 brief 有产出**（S-A、S-B、S-D×2、S-E；S-C 无产出）
- **否决/降级/改写的子代理结论：9 条**（S-A 否 2 降 1、S-C 无产出、S-D 否 1 冲突裁决 1、S-E 否 2 降 1；末次 S-E 追加下调 1）
- **子代理与本人直接冲突并由本人裁决：1 处**（反向 Jacobian，见 §5-反例 5）
- **因本人未覆盖而无法复核、明确标注为线索的：8 份测试文件（1783 行）全部相关结论**

> **时序说明**：`c2c90398`（S-E）的**末次报告在本交付件初稿写完后才送达**，其中 `:935-941` 一条落在本片生产源内、本人已亲读该段代码，故**补做裁决并写入 M-31**（定性从「阻断」下调为「合同冲突」，理由见该条）。其余内容与初稿所记一致。

---

## 8. 自证段

```bash
# 0) 基线与零写入自证
cd "/workspace/Astro CS Database"
git rev-parse HEAD                    # 期望 850a9edefd47434b9ab71bc907c3de1e0814b323
git -c core.quotepath=false status --porcelain   # 期望：本审稿人未新增/修改任何仓内文件
                                                     # （唯一新增为本交付件 run/GOVERN-08/审核包-R2/审稿-P1-ALG-drizzle-002.md）

# 1) 分母复核：清单声明 8997 vs 实测求和
sed -n '235,265p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml
for f in $(sed -n '242,265p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml | sed 's/.*"\(.*\)".*/\1/'); do
  wc -l "$f"; done | awk '{s+=$1} END {print "实测总行数 =", s}'

# 2) 【反例 1】自洽式断言 —— 期望侧与被检侧同源
sed -n '93,99p' lib/algorithms/drizzle/healpix_drizzle/tests/trace_g4_validate.py
sed -n '1692,1721p' lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp | grep -n 'tr_sum\|sum_contribution'

# 3) 【反例 2】:229 缺 len(li)>0 守卫，而同文件 :109/:112/:215/:216 都有
sed -n '221,229p' lib/algorithms/drizzle/healpix_drizzle/tests/trace_g4_validate.py
grep -n 'len(dl) > 0\|n_checked >= 8192' lib/algorithms/drizzle/healpix_drizzle/tests/trace_g4_validate.py

# 4) 【反例 3】性能哨兵方向反了 + 引擎静默串行分支
sed -n '67,74p' lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_tests_perf.cpp
sed -n '1884,1888p' lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp

# 5) 【反例 4】默认 pixfrac=1 下 sumNorm ≡ sumArea（自述式佐证）
sed -n '1596,1606p;1704,1710p' lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp

# 6) B-3 单侧夹紧
sed -n '195,203p' lib/algorithms/drizzle/healpix_drizzle/reverse_drizzle.cpp

# 7) B-4 四处静默丢通量
sed -n '934p;1053p;1341p;2607p' lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp
sed -n '1673,1681p' lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp   # 同文件自称的守恒恒等式

# 8) B-5 空 catch
grep -n 'catch (...) {}' lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp

# 9) B-6 rel_diff 对 NaN fail-open（对照 :398-403 的正确处理）
sed -n '25,29p;396,403p;460,467p' lib/algorithms/drizzle/healpix_drizzle/drizzle_science.cpp

# 10) M-1 flux_conservation_factor 字面 1.0
sed -n '140,146p' lib/algorithms/drizzle/healpix_drizzle/drizzle_science.cpp

# 11) B-3 附：reverse 冻结语义权威文档不存在（悬空引用）
sed -n '1,13p' lib/algorithms/drizzle/healpix_drizzle/reverse_drizzle.cpp | grep -n 'wiki/\|control/'
ls -d wiki control ; echo "exit=$?"        # 期望 exit=2（两者皆不存在）

# 12) M-22 注释仍写 Girard，而正本已登记实为 S-H + Van Oosterom
grep -n 'Girard' lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp
grep -n 'DISP-DRZ-002' docs/engineering/STANDARDS_REGISTRY.md

# 13) M-19 两个 ABI 头对 legacy 负值域不一致
grep -n '1\.\.14\|-1\.\.-13\|-1\.\.13' lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h \
                            lib/algorithms/drizzle/include/acsd/drizzle/types.h

# 14) M-20 projection 键无读取者
grep -rn 'DRZ_CFG_KEY_REV_PROJ' lib/algorithms/drizzle/ | grep -v types.h

# 15) B-9 输出 manifest 定容不含转义开销 / 转义器无容量参数
sed -n '288,295p;1066,1076p;1096,1106p' lib/algorithms/drizzle/hips/src/module_entry.cpp

# 16) 子代理「零私建线程池」结论的本人复核（engine 用 OpenMP 运行时池 + 宿 executor 租约）
grep -n 'std::thread\|std::async\|pthread_create\|#pragma omp\|omp_set_num_threads' \
     lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp \
     lib/algorithms/drizzle/hips/src/module_entry.cpp
grep -n 'ex->acquire\|FORBID-003' lib/algorithms/drizzle/hips/src/module_entry.cpp
```

> 说明：以上命令**全部为只读检视**（`sed`/`grep`/`ls`/`git status`），不编译、不跑测试、不跑二进制、不写任何仓内文件。

---

## 9. 是否发现新问题

**是。** 下列 4 项在既有 `逐份判定-权威版.csv` 与本会话可见的历史审稿线索中未见对应判定，且均为本人亲读原文 + 可复跑命令取证：

1. **`trace_g4_validate.py:94-96` 自洽式断言** —— 负责人本轮点名的最高价值类别，本片命中一条教科书实例（期望量由被检运行自身定义）。
2. **`trace_g4_validate.py:229` 恒真门** —— 同文件其余 4 处检查都有 `len>0` 守卫，唯独这条漏掉。
3. **`drizzle_science.cpp:7-8` 的「避免同实现自证」声称为假** —— `raw_c` 与 `build()` 存的 `e.c` 代数恒等，该文件头对自身证据强度的声明不成立。
4. **`reverse_drizzle.cpp:4` 的两条冻结语义权威文档整链缺失** —— `wiki/` 与 `control/` 目录在仓根均不存在，生产源的正本权威指向虚空。
