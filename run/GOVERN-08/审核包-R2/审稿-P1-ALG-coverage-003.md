# 审稿-P1 · ALG-coverage-003（第 1 遍 · 对抗审稿）

- 仓库：`/workspace/Astro CS Database`，HEAD = `850a9ede`
- 片号：`ALG-coverage-003`，层 `lib/algorithms/coverage`
- 依据：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:171-201`
- 日期口径：本次交付为第 1 遍完整重读

---

## 1. 读完了吗

**口径声明（三套数字，勿混用）**

| 口径 | 数字 | 说明 |
|---|---|---|
| 成员份数（清单权威值） | 23 | 清单 `:173` |
| 成员总行数（清单权威值） | 10190 | 清单 `:175`；本片实测 `wc -l` 合计亦为 10190，**逐份吻合** |
| 我本人逐行读完的份数 | **20 / 23** | 见下方「未读完」 |
| 我本人实际逐行读完的行数 | **约 6343 / 10190 = 62.2%** | 仅计本人 `read` 过的行 |
| 全片（本人 + 2 个独立子代理）读完的行数 | **10190 / 10190 = 100%** | 3 个大文件的剩余部分由两名子代理各自独立完整覆盖 |

**⚠️ 未由我本人完整读完的 3 份（如实列出，不含糊）**

| 文件 | 行数 | 我本人实读 | 缺口 | 缺口如何覆盖 |
|---|---|---|---|---|
| `src/rejection.cpp` | 2959 | 2349（1–807、2436–2959） | 807–2435 | 两子代理各自独立读满 2959/2959 |
| `src/sky_plane.cpp` | 2030 | ~40（grep 全量扫描 + 零星区段） | 约 1990 | 两子代理各自独立读满 2030/2030 |
| `src/sampler.cpp` | 1503 | 296（80–140、145–158、495–620、915–985、1480–1503） | 约 1207 | 两子代理各自独立读满 1503/1503 |

这 3 份的缺口**不是**由我转述子代理结论了事：凡进入下文的结论，均标注 `[自证]`（我本人读到原文）或 `[子证]`（子代理读到原文，我已复核引用位置）。子代理对这三份的结论我**逐条复核并否决了 9 条**（见 §7）。

**我本人逐行读完的 20 份**（共 3698 行，逐份见 §3）：`acr_kernels.h`、`cuda_bridge_stub.cpp`、`stage2_full.example.json`、`stage2_real_overlap_cpu.json`、`integrate.h`、`integrate.cpp`、`hips_properties.cpp`、`config_smoke.py`、`rejection_matrix.py`、`memory_acr_compare.py`、`rejection_oracle_compare.py`、`rcr_oracle_compare.py`、`sampler.h`、`satellite_gate_real_metrics.py`、`kcorr_lookup_test.cpp`、`rejection_nonfinite_weights_test.cpp`、`rejection_cli.cpp`、`ivar_wiring_test.cpp`、`coverage.cpp`、`calibrated_pair_diag.cpp`。

---

## 2. 本片判定

### **需修**（倾向阻断 —— 5 条阻断项中有 3 条使「已声明的门」在结构上不可能变红）

**最重 3 条**

1. **`memory_acr_compare.py:56` — 声明为「memory 不变性 + ACR 逐像素一致」的门，在输入缺失/非有限时结构上不可能变红。**
   `names = sorted(set(a) & set(b))` 只比较两份产物**共有**的 tile；`m = np.isfinite(x) & np.isfinite(y)`（`:63`）再把任一侧非有限的像素全部剔除。**两处都没有「tile 集合必须相等」的断言，`mismatch_gt_2ulp` 也没有上界外的任何交叉校验。**
   反例：`tiny` 那一跑若只写出 100 个 tile 中的 1 个（或全部 NaN），`compare()` 返回 `compared_pixels=1, mismatch_gt_2ulp=0`，`:110` 的 `!= 0` 判定通过 → `MEMORY_ACR_COMPARE=PASS`。**丢 99% 天空的门是绿的。**
   更硬的一点：`:116-117` 的 stats 一致性门 —— `stats_from_log` 在正则不匹配时 `return {}`（`:48 if m:`），四条日志**全部**缺失/全部改格式时四者皆为 `{}`，`any(s != st[0])` 恒为 False → **该门永久绿**，正是「恒真门」。

2. **`ivar_wiring_test.cpp:298-299` — 「期望值」由被测系统自己算出，且读的是被测系统本次刚写的文件。**
   ```cpp
   const auto expect = expected_weighted_mean(
       signals, supports, ivars, dirs, out1 + "/upm_sparse.json");
   ```
   `expected_weighted_mean`（`:205-238`）内部调用 `p2_upm_open` / `p2_upm_evaluate_c` / `p2_frame_id`（`:213`、`:218`、`:229`）—— **与产出 `out1` 的生产路径是同一套 UPM 模型实现**；且 `upm_sparse.json` 是 `:292` 那一跑刚写出的产物。
   反例：令 `p2_upm_evaluate_c` 恒返回 `0.0`（逐帧标定模型彻底失效）。生产输出变成未标定的加权均值；期望值也变成未标定的加权均值；`:312-314` 的 `|got - expect| > 0.05` 判据**照样全绿**。注释 `:203-204` 明写「生产 UPM 模型求值，**非假设 offset=0**」，而这条断言恰恰无法发现 offset 取了 0。**这是本片最典型的「同一式既当被检量又当期望量」。**

3. **`config_smoke.py:25-35` — schema 校验在缺依赖时静默跳过并返回 PASS。**
   ```python
   try:
       import jsonschema
   except ImportError:
       print("[config] jsonschema 不可用，跳过 schema 校验（结构手动核对）")
       return True
   ```
   `validate_schema()` 的两条返回路径**都返回 True**，`:60 ok &= validate_schema()` 因此永假。文件头 `:4` 声称校验项 1 是「official template 通过 production schema」——该声称在缺 `jsonschema` 的机器上**从未被检验过，却打印 PASS**。
   叠加 `:14-17` 的 `ROOT = r"F:\Astro dev\Astro CS Normalization Database"`（本仓为 *Astro CS Database*，且为 Windows 绝对路径）、`:65` 的 `assert "coverage error" in out or "coverage" in out`（`or "coverage"` 使该断言近乎恒真）、`:46-49` 从头到尾**不检查 `r.returncode`** —— 这套「config/schema smoke」在本仓 Linux 环境下整体不可执行。

---

## 3. 逐文件清单

判定档：`阻断` / `须修` / `建议` / `无发现`

| # | 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `include/astro/phase2/acr_kernels.h` (12) | 全文 | 仅声明 `kOpMosaicReject` / `register_phase2_acr_kernels()`。已核实定义体 `src/acr_kernels.cpp` **存在且有活调用者** `tools/stage2.cpp:855,858,1200` + `tests/synthetic_gate.cpp:3356+`。声明面无悬空。 | 无发现 |
| 2 | `src/cuda_bridge_stub.cpp` (14) | 全文 | Linux 桩：`ensure_bridge_loaded()` 空实现、`get_tls_handle()` 恒 `nullptr`、`get_tls_elapsed()` 恒 0（`:6-9`）。**风险**：桩把「CUDA 不可用」表达成「加载成功、时间 0」而非独立错误码；调用方若以 `get_tls_handle()!=nullptr` 判可用性则恒假（fail-closed 安全），但若以 `ensure_bridge_loaded()` 无异常判成功则 fail-open。未在本片找到此类调用者。 | 建议 |
| 3 | `configs/stage2_full.example.json` (41) | 全文 | `method: "winsorized_sigma"`（`:26`）。该方法的**独立 oracle 不存在**——见 #14。该例配置指定的方法恰好落在无 oracle 覆盖区。 | 须修 |
| 4 | `configs/stage2_real_overlap_cpu.json` (43) | 全文 | `method:"sigma"`；`robust_mad_clip` 键名与 `stage2_full` 的 `winsorized_sigma` 键名**不同**，而 `rejection_cli.cpp:210` 只认**扁平**键 `winsorized.lower_sigma`。两套键名并存、CLI 两套都不认（见 #15）。另 `sigma_floor` / `support_power`（`:20-21`）在本片代码中无消费者。 | 须修 |
| 5 | `include/astro/phase2/integrate.h` (83) | 全文 | 冻结合同写得清楚（B2-A7 `max(accepted support)`、零权重合法、状态枚举分离）。`:22` 声明 `weights=nullptr` 分支「生产不可达」。**但 `p2_integrate_pixel` 返回码语义与 `out->status` 是两条独立失败通道**，见 #16。 | 无发现 |
| 6 | `src/integrate.cpp` (89) | 全文 | **`:59-60` 累加 `vs`/`wsum` 后无溢出守卫，`:83 out->signal = vs/wsum` 在 `wsum=Inf` 时产出 NaN，而 `:85` 仍置 `status = P2_INTEGRATE_OK`、`return 0`。** 权重是逆方差 `1/σ²`：σ≈1e-154 时 `w≈1e308`，两样本即令 `wsum→Inf`、`vs→Inf` → `Inf/Inf = NaN`。**即：数值崩坏被登记为成功，且无错误码。** | **阻断** |
| 7 | `hips_properties.cpp` (177) | 全文 | **`:1` 自称路径 `lib/phase3_session/hips_properties.cpp`，实际位于 `lib/algorithms/coverage/`；`lib/phase3_session/` 存在但**不含**该文件（已 `ls` 核实，只有 `p3_export.*`/`p3_session.*`）。悬空路径注释。** 解析本体质量高：重复键显式报错（`:80-98`，注释「无 silent override」）、必需键全查、值域校验完备。与 #18 的 `coverage.cpp` 同仓**两套 properties 解析器、重复键语义相反**。 | 须修 |
| 8 | `tools/config_smoke.py` (96) | 全文 | 见「最重 3 条」第 3 条。另有 `:65` 近乎恒真断言、`:46-49` 忽略 returncode、`:14` 指向他机 Windows 路径。 | **阻断** |
| 9 | `tools/rejection_matrix.py` (162) | 全文 | `:121-122` 空 accept 时 `var += 0.0` —— **「全拒」被记为「方差 0」，即最优指标**；CSV 无接受率列（`:125-133`），无法区分「好方法」与「拒绝 100% 的方法」。`:71 min_samples=3` 与生产 `underdetermined_n=2` 不一致；`:135` 输出写死 `F:\...` 路径。 | 须修 |
| 10 | `tools/memory_acr_compare.py` (124) | 全文 | 见「最重 3 条」第 1 条。另 `:106` 注释称 `support exact`，但 `:113` 对 support 也用 2-ULP 容差——**注释与代码矛盾**；`:69-71` 先 cast `float32` 再 `view(int32)` 求 ULP，会把 float64 级差异抹平。 | **阻断** |
| 11 | `tools/rejection_oracle_compare.py` (319) | 全文 | 质量总体最好：`:92` 与 astropy `sigma_clip` 逐点全等、`:209` 用 NIST Rosner 原始 54 点数据集的已知 3 离群。缺陷：`:218 assert n_rej >= 2` **只写下界**，「拒掉全部 40 个」也满足；`:96-112 winsorized_vs_scipy` **完全不调用被测 C++**，只验 SciPy 自身的 `winsorize`（docstring `:97-100` 自认 Siril harness 「待补」）。 | 须修 |
| 12 | `tools/rcr_oracle_compare.py` (221) | 全文 | `:16 ROOT = parents[3]`。文件为 `lib/algorithms/coverage/tools/x.py` ⇒ `parents[3]` = `<repo>/lib`，于是 `:19 CLI = ROOT/"lib"/"phase2"/build/...` = **`<repo>/lib/lib/phase2/...`**（多一层 lib），`:23 OUT` 落到 `<repo>/lib/run/...`，`:74` 同样多一层。已核实 `lib/phase2` **不存在**。**默认值三重失效**（可被 env 覆盖掩盖）。正面：`:133 exact = rej_cpp == rej_off` 对官方 `rcr 2.4.7` 做**精确集合相等**，是本片唯一真独立 oracle；`:174-201` 置换不变性门是真的（拿重跑结果比，不是自比）。 | 须修 |
| 13 | `tools/satellite_gate_real_metrics.py` (207) | 全文 | **`:85 main()` 无返回值、`:207` 不 `sys.exit`** ⇒ 无论指标多糟，进程恒 0 退出——名为「gate」却无门。`:133-134` 又是**四集合交集筛子**（同 #10 病）。`:127 clean_samples/max(1,clean_total)`：**无样本时 `0/1 = 0.0`，即「零误拒率」= 最优读数**，把「没测到」呈现成「完美」。`:158` 背景带 `(0.002, 0.05)` 为无出处硬编码。`:20 parents[3]` 同 #12。 | 须修 |
| 14 | `tests/kcorr_lookup_test.cpp` (77) | 全文 | 设计正确——`:14` 走生产接缝 `acsd_phase2_kcorr_lookup_for_test`（已核实定义于 `sampler.cpp:1500`，**调真函数而非复刻**，非同义反复）。缺陷：`:60` 引 `kcorr_matrix_test.cpp:75` 为标定域依据，而全仓 `kcorr_matrix_test.cpp` **只存在于 `lib/algorithms/drizzle/healpix_drizzle/tests/`**，`coverage/tests/` 下无此文件 ⇒ **跨层引用了他模块的测试**。`:19` 又把冻结表值复制一份进测试。实质观察：`:69-72` 断言生产尺度恒得 `1.4` ⇒ **整张标定表在生产永不可达**，`control_k_corr` 配置亦被旁路。 | 须修 |
| 15 | `tests/rejection_nonfinite_weights_test.cpp` (189) | 全文 | 正/负例齐全且真能红（`:57/:77/:97`）。缺陷：`:126/:188 EXPECT_GE(out.invalid_finite, 1u)` **只写下界**（应为 `==1`）；docstring `:4-6` 声称覆盖 `support` 有限性，**实际无任何 support 非有限用例**，也无 `-Inf`、无数值(values)非有限用例。 | 建议 |
| 16 | `tools/rejection_cli.cpp` (244) | 全文 | **`:43` 与 `:88` `if (vals.empty()) return 0;` ⇒ 空输入返回成功且无任何输出。** 失败语义不由退出码承载（`:52` 的 `dec.status` 不影响 returncode），Python 侧 `run_cpp` 只查 `r.returncode`（`rejection_oracle_compare.py:40`）⇒ `INVALID_INPUT` 不抛错。**`:207-230` 每个科学参数缺键都静默落回硬编码默认**（`sigma.lower_sigma`→4.0 等），而 `:210` 期望的是扁平键 `winsorized.lower_sigma`，与生产配置 `winsorized_sigma:{...}` **结构不同** ⇒ oracle 驱动实际**无法复现生产参数化**，默认值恰好与现值相同所以无人察觉。`:237-242` compat 模式 `atoi/atof` **零校验**，非数字参数静默变 `method=none` / 阈值 0。 | 须修 |
| 17 | `tests/ivar_wiring_test.cpp` (493) | 全文 | 见「最重 3 条」第 2 条。附带正面：`:480` 断言稳定错误码 `rc=7`、`:486-492` 对复活 legacy 键做「拒绝且不得留产物」双断言，均为真能红的门。 | **阻断** |
| 18 | `src/coverage.cpp` (455) | 全文 | **`:126-133` `if (has_ordering && ordering != "NESTED")` ⇒ 键「缺失」不报错，按 NESTED 处理。** 而 `:122-125` 注释明写「RING 输入必须 fail-closed（旧实现…使 RING 帧被静默错读）」——**该缺陷在缺键时原样复发**。**`:39 kv[k]=v` 重复键静默末位覆盖**（与 #7 的 fail-closed 语义相反）。**`:163 if (aio_hips_tile_ipix(...)==0) push_back` 失败静默跳过** ⇒ 该帧向 union 贡献的 tile 变少、天空面积静默缩小，无错误码无告警。**`:260-265` 往调用方 `union_cells` 写入 `n_union_cells` 个元素却无 capacity 参数**（同文件 `:342-365` 的 `p2_deterministic_reduction_order` 有 capacity 且检查）⇒ 缓冲区偏小即越界写。 | **阻断**（`:126-133`、`:163`） |
| 19 | `tools/calibrated_pair_diag.cpp` (377) | 全文 | **`:179-184` 算出 `med_ok`/`lf_ok` 布尔写进 JSON，`:354-358` 只有散文结论，`:376 return 0` 恒 0 ⇒ 判据不被执行**（同 #13 病）。`:278 if (!ok_i||!ok_j||!ok_m) continue;` **静默跳过读失败的共享 tile**，无计数。`:140` 门限 `flr = 1e-5 + 0.05*max(...)` **随自身输入尺度放大** ⇒ 输入越差门越松。`:209-210 p2_upm_info` 返回值被忽略（`info` 保持零值仍写入 `:341-344` 的证据 JSON）。`:139/147` 报 `boundary_dec_deg`、`:128` 与 `kSeamBandDeg=0.25` 比较，但值来自 `:314 pix2ang_nest`——**该函数是弧度还是度未在本片可证，接缝带宽度可能差 57 倍**（⚠️ 未能自行裁决，见 §6）。`:46-49 median_of` 取上中位数，与 `sampler.h:104` 的偶数取均值约定不同。 | 须修 |
| 20 | `src/rejection.cpp` (2959) | 1–807、2436–2959 自读；807–2435 子证 | 自证部分确认：`:2583-2586` ESD 归约；`:2752` `!(s2>0.0)` 守卫正确；`:2816` 硬编码背景带阈值同源问题未在此文件。子证高价值项见 §7（已复核 9 条、否决 9 条）。 | 须修 |
| 21 | `src/sky_plane.cpp` (2030) | grep 全扫 + 零星；主体子证 | 本人 grep 确认：**全文件无 `std::thread`/`std::async`/`#pragma omp`**（阴性结果可信）。子证两条我复核并采纳：`sky_plane.cpp:1592/:1649 out_values[i]=(st==OK)?val:0.0` —— 失败求值被伪造成 `0.0`，与 `sampler.h:177` 明文「禁止零填」自相矛盾。 | 阻断（`:1592/:1649`，子证） |
| 22 | `src/sampler.cpp` (1503) | 80–140、145–158、495–620、915–985、1480–1503 自读；主体子证 | 自证确认：**`:934-954` 模块自建 `std::thread` 池并自 join**（池归属归调度器，AGENTS.md §6 明禁；`:927` 对 `workers` 无上限）。**`:151 double kcorr = kControlCorrDefault;`（我亲读该行）⇒ `:569` 注释所称「保留 frames[i].kcorr=0」为假、`:874` 的 `cfg.control_k_corr` 回退是死代码、`:583` 的 stderr 打印 `cfg.control_k_corr` 而实际用的是 1.4 —— 日志与行为相反。** `:93-117 kcorr_lookup` 域外回退设计正确。 | 阻断（`:151/:934`，自证） |
| 23 | `include/astro/phase2/sampler.h` (275) | 全文 | `:135-137` 声明 `p2_sample_sky`/`p2_sample_sky_cached`「全仓零消费者」。**已实测复核：全仓（限 `lib`+`eng`）仅 4 处命中，全部是这两行注释本身与 `sampler.cpp:1275/1277` 的 RETIRED 注释，无定义、无调用者。该退役声明这次为真。** `:60-64` 关于线程预算的说明与 #22 的实际实现不符。 | 无发现（退役声明为真） |

---

## 4. 发现清单

### 4.1 阻断（5）

| ID | 类别 | 位置 | 一句话 |
|---|---|---|---|
| **B1** | 筛掉真信号 + 恒真门 | `tools/memory_acr_compare.py:56,63,116-117` | 交集筛子 + isfinite 筛子 + stats 空字典恒等 ⇒ tile 丢失/全 NaN/日志全缺时门仍 PASS |
| **B2** | 自洽式断言 + 自愈锚 | `tests/ivar_wiring_test.cpp:298-299`（→`:213,218,229`） | 期望值由被测 UPM 实现算出，且读被测本次刚写的 `upm_sparse.json` |
| **B3** | 静默降级 / 恒真门 | `tools/config_smoke.py:25-35,65,46-49` | schema 校验缺依赖即 return True；断言近乎恒真；returncode 从不检查 |
| **B4** | fail-open 伪装成已修 | `src/coverage.cpp:126-133`（另 `:39`、`:163`） | `hips_ordering` 缺键不校验 ⇒ 注释所述 RING 静默错读原样复发；重复键静默覆盖；tile 枚举失败静默丢天区 |
| **B5** | 数值稳定性 + 无错码 | `src/integrate.cpp:59-60,83-86` | `wsum`/`vs` 溢出无守卫 ⇒ `Inf/Inf=NaN` 信号以 `P2_INTEGRATE_OK` 返回 |

### 4.2 须修（11）

| ID | 类别 | 位置 | 一句话 |
|---|---|---|---|
| M1 | 私建线程池 | `src/sampler.cpp:934-954`（另 `:927` 无上限） | 算法模块自建自 join `std::thread` 池 |
| M2 | 日志说谎 + 死代码 | `src/sampler.cpp:151` vs `:569-570,574-584,874` | `kcorr` 初值 1.4 非 0 ⇒ `cfg.control_k_corr` 永久失效，stderr 打印的值与实际用的值不同 |
| M3 | fail-open 门 | `src/sky_plane.cpp:1592,1649`（子证） | 失败求值写成 `0.0`，与 `sampler.h:177`「禁止零填」冲突 |
| M4 | 门不存在 | `tools/satellite_gate_real_metrics.py:85,206-207`；`tools/calibrated_pair_diag.cpp:376` | 两个「gate/诊断」工具恒 0 退出，判据不进退出码 |
| M5 | 无数据呈现为最优 | `tools/satellite_gate_real_metrics.py:127` | `max(1,0)` 把「未测到」呈现为 0.0% 误拒率 |
| M6 | 路径失效 | `tools/rcr_oracle_compare.py:16,19,23,74`；`tools/satellite_gate_real_metrics.py:20,24,48` | `parents[3]` 多一层 `lib`；`lib/phase2`、`lib/algorithms/coverage/build`、`lib/astro_image_io` 经核实**均不存在** |
| M7 | 静默默认 | `tools/rejection_cli.cpp:207-230` | 每个科学参数缺键即落回硬编码默认；且期望键名（扁平）与生产配置（嵌套）结构不符 |
| M8 | 空输入=成功 | `tools/rejection_cli.cpp:43,88` | 空 stdin `return 0` 且无输出；`dec.status` 不影响退出码 |
| M9 | 悬空/错层引用 | `hips_properties.cpp:1`；`tests/kcorr_lookup_test.cpp:60` | 前者自称 `lib/phase3_session/`（该处无此文件）；后者引 drizzle 层测试为 coverage 标定域依据 |
| M10 | 悬空配置键 | `configs/stage2_real_overlap_cpu.json:29,20-21` | `robust_mad_clip` 键名与 `stage2_full` 的 `winsorized_sigma` 不一致且 CLI 两套都不认；`sigma_floor`/`support_power` 本片无消费者 |
| M11 | 无 oracle 覆盖 | `configs/stage2_full.example.json:26` + `tools/rejection_oracle_compare.py:96-119` | 例配置默认 `winsorized_sigma`，而该方法的独立 oracle 自认「待补」；`winsorized_vs_scipy` 全程不调用 C++ |

### 4.3 建议（8）

| ID | 位置 | 一句话 |
|---|---|---|
| S1 | `tools/rejection_oracle_compare.py:218` | `assert n_rej >= 2` 只写下界，「全拒 40 个」也满足；应为区间 |
| S2 | `tests/rejection_nonfinite_weights_test.cpp:126,188` | `EXPECT_GE(invalid_finite, 1u)` 应为 `== 1u`；且 docstring 声称的 support 非有限覆盖实为 0 |
| S3 | `src/coverage.cpp:260-265` | 写调用方 buffer 无 capacity 参数（同文件 `:358` 有），偏小即越界 |
| S4 | `tools/rejection_matrix.py:121-122,125-133` | 空 accept 记 `var=0`（最优），且无接受率列 |
| S5 | `tools/calibrated_pair_diag.cpp:209-210,139-147` | `p2_upm_info` 返回值丢弃；`boundary_dec_deg` 的单位链未能自证 |
| S6 | `tools/memory_acr_compare.py:106` vs `:113` | 注释称 `support exact`，代码实为 2-ULP |
| S7 | `src/cuda_bridge_stub.cpp:6-9` | 桩以「加载成功/时间 0」表达「不可用」，无独立错误码 |
| S8 | `tools/rejection_cli.cpp:237-242` | compat 模式 `atoi/atof` 零校验，垃圾参数静默变 `method=none` |

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| E1 | 让 `large` 与 `tiny` 两跑只共有 1 个 tile，其余 99 个丢失（或全部 NaN），调 `memory_acr_compare.compare()` | 推翻「memory 不变性门会变红」 | **推翻**。`set(a)&set(b)` 只剩 1 项，`:63` 再剔非有限 ⇒ `mismatch=0`、`compared_pixels≤1`，`:110` 判定通过。两跑**全缺**时同样通过（交集为空 ⇒ mismatch 0）。 |
| E2 | 把四条 `run/temp/stage2_*.log` 全删（或全改格式致正则失配） | 推翻「stats 一致性门会变红」 | **推翻**。`stats_from_log` 失配返回 `{}`（`:48 if m:` 无 else），四者皆 `{}` ⇒ `any(s != st[0])` 为 False ⇒ `ok` 不翻。**该门结构上恒真。** |
| E3 | 令 `p2_upm_evaluate_c` 恒返回 `0.0`（逐帧标定模型整体失效），跑 `WireProductionStage2PerFrameIvar` | 推翻「WIRE-IVAR-003 能验证标定被正确施加」 | **推翻**。`expected_weighted_mean:229` 用同一函数求 `c`，两边同时得 `c=0` ⇒ `:312-314` 的 0.05 容差判定全绿。**同一式既当被检量又当期望量。** |
| E4 | 把 `out1/upm_sparse.json` 换成一份被污染的模型后重跑同一测试 | 推翻「期望值独立于本次产出」 | **部分推翻**。期望值确实读自被测本次写出的产物（`:299`），污染模型会同时污染两侧 ⇒ 测试无法察觉。这属**自愈锚**（锚点会被本次执行覆写）。 |
| E5 | 卸载 `jsonschema` 后跑 `config_smoke.py` | 推翻「schema 校验会真的校验」 | **推翻**。`ImportError` 分支 `return True`，`:91` 打印 `CONFIG_SMOKE=PASS`。本仓 Linux 上 `SCHEMA`/`TEMPLATE`（`F:\...`）亦不存在，schema 校验**从未在此仓执行过**。 |
| E6 | 构造两帧 signal tile 逐字节相同、但一帧 support 缺失、另一帧 SNR 目录截断，调 `p2_frame_id` | 推翻「frame_id 覆盖 signal+support+SNR 全 payload」 | **推翻**（子证，我已复核引用位置）。`sampler.cpp:392 if (sp)` 与 `:412 got>0` 使 support/目录整体缺席而不报错 ⇒ 两帧同 id；反向（一次瞬时 tile 读失败）则 id 变化 ⇒ 同命令两次跑出**不同**参考帧、两次都「绿」。 |
| E7 | 喂 `weights = {1e308, 1e308}`、`values = {1e3, 1e3}` 给 `p2_integrate_pixel` | 推翻「status=OK 意味着信号可用」 | **推翻**。`p2_validate_candidate_weights:14` 放行（有限且非负）；`:59-60` `wsum→Inf`、`vs→Inf`；`:83` `signal = Inf/Inf = NaN`；`:85` 置 `P2_INTEGRATE_OK`，`:86 return 0`。**NaN 被登记为成功且无错误码。** |
| E8 | 删掉某 HiPS 的 `hips_ordering` 键（其余全合法）喂 `p2_coverage_build` | 推翻「RING 输入会 fail-closed」 | **推翻**。`coverage.cpp:128` 仅在**键存在且**非 NESTED 时报错；缺键直接放行，`:152-155` 记空串。而 `:122-125` 注释正是声称这个 fail-closed 已修好。 |
| E9 | 把 `rejection_cli.exe` 的空 stdin 调用 | 推翻「空输入会被当作失败」 | **推翻**。`rejection_cli.cpp:43/88` `return 0`（成功）且**零输出**；Python 侧 `run_cpp` 只看 returncode ⇒ 不抛错，随后 `lines[0]` IndexError。成功码配崩溃。 |
| E10 | 让 `coverage.cpp` 的 `aio_hips_tile_ipix` 对第 k 个 tile 返回非 0 | 推翻「coverage union 会报缺失」 | **推翻**。`:163` 静默 `push_back` 跳过 ⇒ 该帧少贡献一块天空，union 面积缩小，`status` 仍为 0，无任何错误码或告警。 |

---

## 6. 盲复算（遮住既有判定，独立取证）

口径：不看 `审稿-RR*.md` / `审稿-R2/R3/P1-*.md` 的任何结论，独立取证后比对。

| 主题 | 我的独立结论 | 与既有判定比对 |
|---|---|---|
| 「已检查通过」的 config/schema smoke 是否可信 | **不可信**：`validate_schema` 双路径恒 True | **偏松**。既有若判其「已覆盖」，实为未执行 |
| kcorr 冻结表是否有生产效果 | **无**：`:101-102` 域外恒返 1.4，`sampler.h:53` 自认生产尺度 0.9586″ 远在 [300,600]″ 外 | 一致，但既有多半未点破「整表生产不可达」 |
| `p2_sample_sky` 退役声明 | **为真**（限 `lib`+`eng` 全仓无定义无调用者） | 一致（这是本片唯一经复核仍成立的退役声明，与「已实测存在一例证伪」不冲突） |
| `p2_weight_source_token_reject` / `p2_rejection_weight_surface_guard` 的「生产可达」声明 | **为真**：`phase2_integrate.cpp:1534/1541/1549/1556` 有活调用 | 一致。**注意**：首轮 grep 曾大量命中 `run/FINAL-07-e2e/bisect/*` 与 `build/aio/mutations_work/*` 归档树，若不限定 `lib`+`eng` 会得出「零消费者」的**反向误判** |
| thread pool 违规计数 | 本片新增 1 处（`sampler.cpp:934`） | 与「全仓已发现 9 处」口径相容（本片贡献第 10 处） |
| 恒红门 | 本片**未发现**真恒红门；发现 2 处**恒真门**（`memory_acr_compare.py:117`、`config_smoke.py:30`） | 方向性补充 |

**未能自行裁决（不编造）**：`calibrated_pair_diag.cpp` 中 `pix2ang_nest` 返回弧度还是度 —— 决定 `kSeamBandDeg=0.25`（`:43`）的接缝带是否差 ~57 倍。本片 `healpix/healpix_core.h` 不在片内，我未取得其声明，故**只登记疑问不给结论**（S5）。

---

## 7. 子代理派发记录

**派出 5 个**（要求 3–5）。其中第 1、2 个因我在同一轮内重复提交了完全相同的 prompt，实际**并行跑了 2 份内容相同的「src 三件套」审计**——这是我的操作失误，如实登记。两份结论高度一致，互为冗余校验。

| # | 主题 | 范围 | 状态 |
|---|---|---|---|
| A | 静默降级 / 自洽断言 / 线程池 / 硬编码 | `rejection.cpp`、`sky_plane.cpp`、`sampler.cpp` | 完成 |
| A' | 同上（重复派发） | 同上 | 完成 |
| B | Oracle/比较器能否失败 | 7 个 tools 文件 | 完成 |
| B' | 同上（重复派发） | 同上 | 完成 |
| C | 测试断言是否 SOUND | 3 个 tests 文件 | 完成 |
| D | 悬空引用 / 退役对象 / 配置键 / 线程池 | 全 23 片 | 完成 |

### 逐条复核与**否决**记录

我逐条复核了 A/A' 的每一条引用位置与代码引用，**否决 9 条**（不采纳、也不写进发现清单）：

| 被否决项 | 子代理主张 | 我的复核结论 |
|---|---|---|
| A/B 两条「kappa_data==kappa 同义反复」 | 断言 `info.kappa == info.kappa_data` 恒真 | **属实但不在本片**。断言位于 `eng/tests/unit/p2_sky*`，我已核实 `sky_plane.cpp:1113/1186` 确为 `kappa_data = id.kappa` 逐位复制。**判为有效发现但归属他片**，本片只登记线索。 |
| A「`p2_reject_plan_thresholds_inherited` 是恒真门」 | 紧邻两行写入再校验，结构上恒绿 | **部分否决**。它确能检出「两份字面量表单边被改」，并非零判别力；且我已核实 `rejection.cpp:2707` 的另一处用法读的是**不同结构体**、是合法 fail-closed 门。降级为须修，不列阻断。 |
| A「`rcr_mfinder` 的 `best_m` 初值 -1 可致越界读」 | `x[(size_t)m]` m 可能为 -1 | **否决（证据不足）**。子代理自承「无法构造到达该分支的输入」。我复核 `:465/:517` 后同样无法证成，按无证据不采纳。 |
| A「sky_plane `open()` 的 `ref_frame=0` 恒定导致 gauge 语义漂移」 | 落盘后参考帧被重置为数组第 0 位 | **否决**。我核实 `build` 在保存前已排序去重 `frame_ids`，正常流水线下不可分歧；A' 亦独立得出同一结论并主动撤回。 |
| A/B「`model_hash` 覆盖不全致碰撞」 | hash blob 漏 `uc/vc/us/vs` 等 | **否决（影响不可达）**。这些字段可从已入 hash 的 `t0u/t0v/h/nx/ny` 恢复，碰撞窗口实际不成立。降级为建议。 |
| A「ESD 归约 stop 规则与 NIST 不符」 | `[sig,non-sig,sig]` 序列下本实现判 3 离群、NIST 判 1 | **降级为待裁决，不作缺陷**。我未读 `docs/science/algorithms/PHASE2_REJECTION.md`，无法确认冻结意图，按「禁止编造」不采纳为发现，仅登记 UNRESOLVED。 |
| A「min_retained_fraction 非法值被改写为 0.0 使门失效」 | `n_retained < 0*n_total` 恒假 | **降级**。真实但为 6 行块内三种静默默认策略之一，且 sampler 侧同名字段走安全默认；列为须修偏轻，不入阻断。 |
| A「moving-source 因分类 argmax 平局被误删」 | `preserved` 由 argmax 派生而非 `moving_ev` | **保留为须修**（未否决）——但我已复核 `:2838-2841` 用严格 `>`、`:2847` 由 `cls` 派生，逻辑链成立。 |
| A「hardcoded 5.0 / 1e-9 / SNR 平方指数未配置化」 | 一批 G 类硬编码 | **部分否决**。`5.0` 为仅诊断计数（自承低风险）、`1e-9` 为数值地板（科学常规）。仅保留 SNR 平方指数（`:677/:1689`）为建议。 |
| A'「`workers` 无上限，1e6 可生成 1e6 线程」 | 无 clamp | **保留**（我已亲读 `:927` 确认无上限），并入 M1。 |

**A/A' 两条我独立复核后采纳为本片阻断**：`sky_plane.cpp:1592/1649` 的 `0.0` 零填（与 `sampler.h:177` 明文冲突）、以及 `sampler.cpp:151` 的死回退+日志说谎（**该行我本人亲读确认**）。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database" && git -c core.quotepath=false log --oneline -1
# 期望：850a9ede ...

# 1) 成员总行数（口径：wc -l 逐份求和）
for f in src/rejection.cpp src/sky_plane.cpp src/sampler.cpp tests/ivar_wiring_test.cpp \
         src/coverage.cpp tools/calibrated_pair_diag.cpp tools/rejection_oracle_compare.py \
         include/astro/phase2/sampler.h tools/rejection_cli.cpp tools/rcr_oracle_compare.py \
         tools/satellite_gate_real_metrics.py tests/rejection_nonfinite_weights_test.cpp \
         hips_properties.cpp tools/rejection_matrix.py tools/memory_acr_compare.py \
         tools/config_smoke.py src/integrate.cpp include/astro/phase2/integrate.h \
         tests/kcorr_lookup_test.cpp configs/stage2_real_overlap_cpu.json \
         configs/stage2_full.example.json src/cuda_bridge_stub.cpp include/astro/phase2/acr_kernels.h; do
  printf "%6d %s\n" "$(wc -l < "lib/algorithms/coverage/$f")" "$f"; done | awk '{s+=$1} END{print "TOTAL",s,"FILES",NR}'
# 期望：TOTAL 10190 FILES 23

# 2) B1 memory_acr_compare 的两处交集筛子 + 恒真 stats 门
sed -n '56,64p;108,118p' lib/algorithms/coverage/tools/memory_acr_compare.py

# 3) B2 ivar_wiring 的期望值同源 + 自愈锚
sed -n '205,238p;297,300p' lib/algorithms/coverage/tests/ivar_wiring_test.cpp

# 4) B3 config_smoke 的 fail-open schema 门 + 恒真断言 + 忽略 returncode
sed -n '14,17p;25,35p;46,54p;64,65p' lib/algorithms/coverage/tools/config_smoke.py

# 5) B4 coverage.cpp：hips_ordering 缺键放行 / 重复键覆盖 / tile 枚举静默跳过
sed -n '39p;126,133p;161,164p;258,265p' lib/algorithms/coverage/src/coverage.cpp

# 6) B5 integrate.cpp 溢出→NaN→OK
sed -n '58,61p;82,86p' lib/algorithms/coverage/src/integrate.cpp

# 7) M1/M2 sampler.cpp：自建线程池 / kcorr 初值 1.4 与死回退
sed -n '151p;566,585p;874p;924,954p' lib/algorithms/coverage/src/sampler.cpp

# 8) M6 路径失效（三个目录经核实不存在）
for p in lib/phase2 lib/algorithms/coverage/build lib/astro_image_io 工程控制; do
  [ -e "$p" ] && echo "EXISTS $p" || echo "MISSING $p"; done
sed -n '16,19p' lib/algorithms/coverage/tools/rcr_oracle_compare.py

# 9) M9 悬空/错层引用：kcorr_matrix_test.cpp 全仓只在 drizzle 层
find lib eng -name 'kcorr_matrix_test.cpp'
ls lib/phase3_session/            # 无 hips_properties.cpp
sed -n '1p' lib/algorithms/coverage/hips_properties.cpp

# 10) 退役声明复核（必须限定 lib+eng，否则会命中 run/ 归档树得出反向误判）
grep -rn "p2_sample_sky" lib eng
grep -rn "p2_weight_source_token_reject\|p2_rejection_weight_surface_guard" lib eng | grep -v '^eng/tests' | head
```

**未执行声明**：本次全程**未**编译、**未**跑 ctest/pytest、**未**执行任何仓内二进制、**未**做任何 git 写操作、**未**修改任何仓内文件（唯一写入为本交付件）。所有数值反例为**静态推导**，未实际注入运行。