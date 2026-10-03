# 审稿-P1-ALG-psf-001 — G08-05 对抗审稿 第 1 遍

- 仓库：`/workspace/Astro CS Database`
- 基线 HEAD：`850a9edefd47434b9ab71bc907c3de1e0814b323`
- 片清单：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml`（权威版，非 G1/GEN-2）
- 车道：`lib/`（生产源码）
- 纪律：零 git 写、零编译、零 ctest/pytest、零二进制执行、零仓内文件改动、未读 `/tmp/acsd_g08/`

---

## 1. 读完了吗

**口径说明**：成员清单取自权威版 YAML 的 `ALG-psf-001.成员文件`（29 条）；行数用 `wc -l` 逐文件实测，与 YAML 的 `实际行数: 6969` 逐位相符。「读完」= 用 read 工具取得该文件**每一行的正文**（不以 grep 摘要替代）。

| 项 | 数值 |
|---|---|
| 成员份数（权威清单） | 29 |
| 实际读了几分 | **29 / 29** |
| 成员总行数（`wc -l` 实测 = YAML 声称） | **6969** |
| 实际读了多少行 | **6969** |
| 覆盖率 | **100%（29/29 份，6969/6969 行）** |
| 未读完的成员 | **无** |

**为判定结论额外读了片外文件（非成员，不计入覆盖率）**：`star_coord_contract.h`(61)、根 `CMakeLists.txt` 定点、`eng/tests/unit/CMakeLists.txt` 定点、`module_adapters.cpp` 定点、`docs/science/PSF.md` 定点、`noise_model.cpp` 定点、`FIX_LEDGER.csv` 定点。

---

## 2. 本片判定：**阻断**

最重 3 条：

1. **自洽式断言簇**（`p1psf_tests_core.cpp:313,316,318,321`，重复于 `:584,585,:895`）：期望量与被检量用**同一定义式**从同一批生产输出算出，误差逐位为 0，对 LM 拟合器零鉴别力。配套 `p1psf_oracle.hpp:8-12` 自称「绝不调用被测函数 / 与生产无共享代码路径」，而 `oracle_flux:49-51` 与生产 `dpsf_psf.cpp:653` **逐字符相同**且**零调用**（死代码），`kMoffat4FwhmFactor:54` 与生产 `:174` 是**同一字面量**。
2. **正例门恒真**（`CMakeLists.txt:79-87` 无编译定义 + `p1psf_stall_equiv.cpp:22`）：文件头声称「默认阈值 60」，全仓不存在 60；正例 target 无 `DPSF_BATCH_STALL_ITERS` ⇒ 源默认 0 ⇒ 批路径与单星路径**传同一参数给同一模板**，被点名的「提前退出」根本没编进行为。
3. **负例门 fail-open**（`p1psf_stall_equiv.cpp:246-248,273-282` + `CMakeLists.txt:101-103`）：批 API 只要因任何原因返回非 0，`diffs=1` 被当成「检出不等价」⇒ 打印 `NEGATIVE INJECTION DETECTED` ⇒ PASS；`PASS_REGULAR_EXPRESSION` 再让 CTest 忽略返回码。**批接口整体坏掉时该负例门反而绿。**

---

## 3. 逐文件清单（29 份，全部读完）

| # | 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `src/dpsf_psf.cpp` (1433) | LM 求解、残差、批入口、诊断面全 | 数值守卫链完整；`MOFFAT4_FWHM_FACTOR=1.230310` 截断；4 处 OpenMP parallel for | 须修 |
| 2 | `tests/p1psf/p1psf_tests_core.cpp` (1126) | units/properties/oracle/negative/boundary 五组 | 自洽式断言簇；P4/U6/B4 缺 rc 守卫 | **阻断** |
| 3 | `src/dpsf_image.cpp` (431) | 17 个图像工具函数 | **整文件零调用者**；`<100` 魔数、`range<=0` 清零、radius=0 退化、dw==1 除零 | 须修 |
| 4 | `tests/p1psf/p1psf_centroid_gate.cpp` (327) | sdet+dpsf 真实源门 | `[P1]` 恒真；筛掉真信号 25/120 | 须修 |
| 5 | `tests/p1psf/p1psf_stall_equiv.cpp` (305) | 等价面 | 正例恒真 + 负例 fail-open + 生存者偏差 | **阻断** |
| 6 | `tests/p1psf/p1psf_center_contract_gate.cpp` (297) | 原点口径门 | 负例任意失败即算检出；`--export` 缺值静默 | 须修 |
| 7 | `tests/p1psf/p1psf_fitcost_probe.cpp` (296) | 测量探针 | 三处输出 `fopen` 失败静默（输入读却 fail-loud） | 建议 |
| 8 | `tests/p1psf/CMakeLists.txt` (260) | 9 target / 15 add_test | 头注列 7 个；引 `eng/ci/*`（已删）；「根构建无 dynamic_psf target」与事实矛盾 | 须修 |
| 9 | `README.md` (248) | 冻结合同 | **§5 全部 17 个行号锚失效**且自称已复测；`eng/tests/p1psf/` 不存在；「零消费者」被证伪 | **阻断** |
| 10 | `tests/p1psf/p1psf_fixtures.hpp` (239) | FIX-PSF-A..E/PERF | 夹具独立（用 oracle 采样器）；背景恒 ≥10 | 建议 |
| 11 | `include/dynamic_psf.h` (215) | 7 导出 + 状态码 | 公开头仍宣称消费 `maxIter/tolerance`（实为死参数） | 须修 |
| 12 | `tests/p1psf/p1psf_test_main.hpp` (196) | 执行器框架 | **未知组名 ⇒ 真空绿**；两个死全局 | **阻断** |
| 13 | `src/psf_information.cpp` (189) | A_NEA / effective PSF | 权重无符号校验；`fwhm_from_half` 单侧镜像伪造 | 须修 |
| 14 | `module.yaml` (174) | 模块清单 | 判据 `eng/ci/check_prod_wiring.py` 整目录已删；「零消费者」被证伪；行号锚同 README | 须修 |
| 15 | `tests/p1psf/p1psf_oracle.hpp` (158) | 独立 oracle | `oracle_flux` 死代码且抄生产；独立部分（`moffat4_eval`/`oracle_m_matrix`/`oracle_sigma_axes`）确实独立 | **阻断** |
| 16 | `tests/p1psf/p1psf_tests_selfcheck.cpp` (156) | 注入自检 | 只注入 1/9 名、只跑 1/5 组；受组名真空绿传染 | 须修 |
| 17 | `tests/p1psf/p1psf_prodpath_check.cpp` (146) | 质心回归锁 | **全套最佳**：独立 oracle 真值 + `rc==0&&nv==1` 守卫 + T3 非停驻对照 | 通过（正面样板） |
| 18 | `tests/p1psf/p1psf_tests_perf.cpp` (142) | 性能基线 | 缺 OpenMP 时结构性恒红（条件性） | 建议 |
| 19 | `test/test_dpsf_nan_sort.cpp` (136) | NaN/Inf UB 回归 | 无科学断言（诚实声明）；已由 `eng/tests/unit/CMakeLists.txt:1641-1648` 注册为 `w34_dpsf_nan_sort` | 建议 |
| 20 | `tests/p1psf/p1psf_center_contract_astropy.py` (114) | astropy 第三方交叉 | **全套最强门，逐路径 fail-closed 落实**；`px_ref`/`kind` 死代码 | 通过（正面样板） |
| 21 | `include/acsd/psf_information.h` (95) | 合同头 | 规范输出非负但实现不校验（见 §4） | 建议 |
| 22 | `src/dpsf_log.cpp` (94) | 日志 | Windows 路径 `lib\dynamic_psf` 已不存在；`atoi` 容错 fail-open | 须修 |
| 23 | `memory.md` (74) | 归档记忆 | 已标 ARCHIVED_NON_NORMATIVE；行号锚 `:528,635,738,866` 失效 | 建议 |
| 24 | `src/dpsf_psf.h` (37) | 数值契约锚 | `kTrimMeanToSigma` 断言值与其引用的 PSF.md:179 闭式矛盾 | 须修 |
| 25 | `src/dpsf_image.h` (28) | 17 声明 | 声明与定义一一对应（无悬空声明） | 通过 |
| 26 | `Makefile` (23) | 独立构建 | 漏 `psf_information.cpp`（3/4 TU） | 建议 |
| 27 | `tests/p1psf/p1psf_tests_main.cpp` (14) | core 入口 | 与 `core.cpp:1117` 声明一致 | 通过 |
| 28 | `src/dpsf_log.h` (10) | 日志级别枚举 | 与 `dpsf_psf.cpp` 用法一致 | 通过 |
| 29 | `.gitignore` (6) | 忽略规则 | 覆盖 `logs/`、`*.dll`、`*.o` | 通过 |

---

## 4. 发现清单

### 4.1 阻断（4）

**【B1】自洽式断言：期望量 = 被检量同式重算** — `tests/p1psf/p1psf_tests_core.cpp:313,316,318,321`（重复于 `:584,585,:895`）

| 断言 | 被检（生产） | 期望（测试） |
|---|---|---|
| `:313` `rel_err(r.fwhm_x, kMoffat4FwhmFactor*r.sx)` | `dpsf_psf.cpp:617` `MOFFAT4_FWHM_FACTOR*sx`，`:174` 同值 `1.230310` | `oracle.hpp:54` `kMoffat4FwhmFactor = 1.230310` **同一字面量** |
| `:316` `fwhm_y` | `:618` | 同上 |
| `:318` `rel_err(r.flux, 2.0*M_PI*r.A*r.sx*r.sy/3.0)` | `dpsf_psf.cpp:653` `double flux = 2.0 * M_PI * A * sx * sy / 3.0;` | **表达式逐字符相同** |
| `:321` `oracle_eccentricity(r.sx,r.sy)` | `dpsf_psf.cpp:655-656` `sqrt(1-(smin/smax)²)` | `oracle.hpp:58-62` **同式** |

四条全部只用被测**自己返回的** `r.sx/r.sy/r.A`，对 LM 求解器**零鉴别力**。容差 `oracle.hpp:130-131` 的注释自认「同一 double 乘法」「oracle 同式重算」。

配套：`oracle.hpp:49-51 oracle_flux` 与生产逐字符相同且**全仓零调用**（死代码）；本该用它的地方（`core.cpp:318/:585`）是把公式**就地内联重写**了一遍。`oracle.hpp:8-12` 的独立性声明（"绝不调用被测函数 / 与生产无共享代码路径"）对这三条不成立。

**反例**：把 `dpsf_psf.cpp:604-605` 的 LM 求解整个短路、直接返回初值 `sx/sy/A`，`:313/:316/:318/:321` 仍全绿 —— 它们只是拿错值重算同一公式。

仓内已有同型裁决先例：`artifacts/evidence/governance-01/research/R-7_AIO_HIPS_ABI与精度.md:418`（"oracle 逐字复刻了生产公式 ⇒ 与实现同源，不能发现归约式本身的错误"）。

**【B2】正例门恒真：被点名的机制根本没有编进构建** — `tests/p1psf/p1psf_stall_equiv.cpp:22` + `tests/p1psf/CMakeLists.txt:79-87`

- `:22` 声称「`p1psf_stall_equiv` : 默认阈值 60 ⇒ 断言等价成立 ⇒ 绿」。全仓 `DPSF_BATCH_STALL_ITERS` 只出现在 `CMakeLists.txt:77`（注释）与 `:99`（**仅 inject 目标** `=2`）。正例 target `:79-87` **无 `target_compile_definitions`**。
- ⇒ `dpsf_psf.cpp:288-291` 取源默认 **0**；`:760-766` 批路径传 `kDpsfBatchStallIters`=0；`:740-746` 单星路径不传 ⇒ 用 `:472` 默认 0。**两条路径传同一参数给同一模板**。
- `dpsf_psf.cpp:353,395,399` 全部 stall 逻辑被 `if (stall_iters > 0)` 包住 ⇒ 0 时整段惰性。
- 该门对 PSF-PERF-001 **零鉴别力**；且顺带证明**生产构建中该性能优化也是关闭的**。

**【B3】负例门 fail-open：批 API 整体坏掉时反而绿** — `p1psf_stall_equiv.cpp:246-248,273-282` + `CMakeLists.txt:101-103`

```
:247  P1PSF_CHECK_EQ(cs, rc, 0);      // rc!=0 ⇒ cs.failures++
:248  if (rc != 0) return cs.failures == 0 ? 0 : 1;   // ⇒ 返回 1
:273  const int diffs = run_equiv_group(cs, "equiv");  // ⇒ diffs = 1
:276  if (diffs > 0) { printf("NEGATIVE INJECTION DETECTED..."); return 0; }  // ⇒ PASS
```
`CMakeLists.txt:102-103` 设 `PASS_REGULAR_EXPRESSION "NEGATIVE INJECTION DETECTED"` ⇒ CTest 只看正则、**忽略返回码**。注入失效与"接口坏了"不可区分。

**【B4】执行器真空绿，并传染给"排除恒绿"的自门** — `p1psf_test_main.hpp:176-191`

`run_all_groups` 不校验请求组名是否存在：组名拼错 ⇒ `:177` 全部 `continue` ⇒ `total_fail=0` ⇒ `:191 return 0`（PASS），而 `:189-190` 打印 **"5 通过, 0 失败"**（谎报）。

传染：`p1psf_tests_selfcheck.cpp:87,:104` 都以 `{"...","units"}` 调 `p1psf_run_core_groups`。若 `core.cpp:1119` 的组名漂移，**基线相与注入相同时真空绿** ⇒ 这道专门用来"排除恒 PASS 侧"的自门自身恒绿。

### 4.2 须修（16）

**【S1】README §5 全部 17 个行号锚失效，而 README 自称已复测** — `README.md:5-7,60,78-105`

`README.md:5` 称「行号锚**全量复测刷新**」，`:60` 称「本 README 全部行号锚」，`:72` 标题写「**实测行号**」。实测（`grep -n`，见 §8）：

| 符号 | README 声称 | 实际 | 偏差 |
|---|---|---|---|
| `dpsf_fit` | 459 | **768** | +309 |
| `dpsf_fit_batch` | 534 | **843** | +309 |
| `dpsf_fit_batch_f` | 661 | **970** | +309 |
| `dpsf_free_results` | 681 | **990** | +309 |
| `dpsf_fit_batch_f32` | 792 | **1101** | +309 |
| `dpsf_fit_batch_d` | 694 | **1003** | +309 |
| `dpsf_batch_dims_check` | 514 | **823** | +309 |
| `fit_batch_float_image` | 575 | **884** | +309 |
| `gauss_solve` | 34 | **228** | +194 |
| `lm_solve` | 105 | **293** | +188 |
| `compute_trimmed_mad` | 203 | **427** | +224 |
| `moffat4_fit_tmpl` | 238 | **688** | +450 |
| `MOFFAT4_FWHM_FACTOR` | 25 | **174** | +149 |
| `NPARAMS` | 26 | **175** | +149 |

偏差从 +149 到 +450 不等 ⇒ 这些锚**从未对本版文件正确过**，不是单纯插队漂移。`module.yaml:1-3,128-161` 复刻同一批锚，同样失效。`oracle.hpp:67` 引 `dpsf_psf.cpp:19-30` 实为 `DpsfDiagStage` 枚举。`memory.md:32` 引 `:528,635,738,866` 亦失效。

**【S2】"psf_params 零消费者"被证伪** — `README.md:214`、`module.yaml:169`、`module_adapters.cpp:3325,4364`

四处声称「`psf` 输出端口为死边、**`psf_params` 在仓库内零消费者**」。实测存在**活的生产消费者**：
`lib/infrastructure/scheduler/src/module_adapters.cpp:5785-5791`
```cpp
std::map<std::string, const Json*> psf_row_by_id;
if (cat[i].contains("psf_params") && cat[i]["psf_params"].is_array()) {
  for (const auto& r : cat[i]["psf_params"]) { ... psf_row_by_id.emplace(...); }
}
```
其上文 `:5778` 明写「修复: (1) 按 star_id 关联 p1_psf.json 的 psf_params 行**取 PSF 域解析通量**」⇒ 该字段驱动 P1-PHOT 的 `F_instr`。另有 `lib/infrastructure/pipeline/module_ports.registry.json:325`（「下游消费者：photometry、noise-snr、drizzle」）、`lib/infrastructure/cli/runtime_client.cpp:206`、`eng/tests/unit/p1001_real_nodes_test.cpp:2328-2353`。

仓内台账 `实验/engineering-evidence/audit-2026-01/FIX_LEDGER.csv:91`（V2-N-08）已记「FAST 模式的裁决前提**已被同批另一提交推翻**」，但 README `:214` 与 `module.yaml:169` 仍以该前提为现行依据；README `:217-222`（P14-N-08 订正）与它在 4 行内自相矛盾且未划除旧句。

**【S3】判定依据整目录已删，只剩归档与 .pyc** — `module.yaml:89,93`、`tests/p1psf/CMakeLists.txt:31,194`

`module.yaml:89` 以「判据 = `eng/ci/check_prod_wiring.py` W1」为据收缩 `source_symbols`；`:93` 引 `dormant_algorithms.json` 台账。实测 `eng/ci/` **在活树中不存在**；`check_prod_wiring.py` 只存活于 `run/FINAL-07*/**` 归档，外加一个孤儿 `run/FINAL-07/doc-migration/selftest/eng/ci/__pycache__/check_prod_wiring.cpython-313.pyc`。`CMakeLists.txt:31` 的 `eng/ci/ledgers`、`:194` 的 `eng/ci/check_platform_syslib_links.py` 同样不存在。

附带：`W1` 是**名字级调用图**判据，而本模块经 `dll_loader.cpp` 动态加载（README `:183`），名字级调用图**结构上看不到 dlsym 边** ⇒ "不可达"在此等价于"静态不可见"，不等于"未被调用"。这是判据本身不可信。

**【S4】文档指向的测试树整目录不存在** — `README.md:187-190,144`、`module.yaml:143`

README §10 声称共址测试面 `eng/tests/p1psf/`、并以 `eng/tests/p1psf/p1psf_tests_core.cpp:717` 作为 DISP-PSF-007 闭环证据。实测 `eng/tests/p1psf/`、`eng/tests/unit/p1psf/` **均不存在**；真实路径是 `lib/algorithms/psf/tests/p1psf`（挂载点 `eng/tests/unit/CMakeLists.txt:1298`）。README 列出的 ctest 名（`performance/selfcheck`）也与 `docs/engineering/TRACEABILITY_SPEC.md:201` 登记的（`prodpath_centroid/centroid_gate/centroid_gate_neg`）不符。

**【S5】容差元数据的"11 号标准"全仓不存在** — `oracle.hpp:6,110`、`README.md:176`

三处把 `11_MODULE_SOURCE_TEST_STANDARD.md`「§5」当作容差来源引用。实测 `find -name "*MODULE_SOURCE_TEST_STANDARD*"` **零命中**。另 `oracle.hpp:5` 引「README §5（容差元数据）」，实测 README §5 是 source symbols 表，容差在 **§9** ⇒ 章节锚指错。

**【S6】`dpsf_image.cpp` 全文件零调用者，却随 `acsd_p1_dpsf` 出货** — `src/dpsf_image.cpp`(431)、`src/dpsf_image.h`(28)

逐函数全仓核（排除 `sdt_`/`sdet_` 前缀、排除 `run/`、`build/`、`out/`、排除本模块）：

```
gaussian_filter_separable 0 | median_filter_3x3 0 | median_filter_5x5 0 | median_filter 0
dilate_box 0 | erode_box 0 | dilate_circle 0 | erode_circle 0
truncate_and_rescale 0 | local_maxima_map 0 | robust_mad 0 | downsample 0
upsample_bilinear 0 | atrous_b3v_filter 0 | iterative_sigma_clip 0 | extract_lowfreq_atrous 0
robust_median 9  ← 全部落在 noise_snr/cpp/src/noise_model.cpp:100 的另一个重载
                  (std::vector<double>&)，与 dpsf_image.h:18 的 (const float*, int) 非同一符号
```

它与 `lib/algorithms/star_detection/src/sdet_image.{h,cpp}:3-35` 的 17 个 `sdet_*` 函数**一一对应且签名相同**，即 psf 侧是一份**死复刻**；`sdet_*` 那份确有生产调用（`sdet_api.cpp:740,824`）。历史先例：`artifacts/evidence/sweep-01/ledger-raw.json:1612` 已记「`sdet_gaussian_filter_separable` 零调用…已登记」。

**该死文件内的具体缺陷（当前不可达，但随出货静态库携带）**：
- `:379` `if ((int)clipped.size() < 100)` — **绝对计数魔数**，`iterative_sigma_clip` 对 `n < 111` 的输入**永远在第 1 轮返回 round-0 的中位/MAD，即截尾从未发生**，且无任何告警。规模不变性被破坏。
- `:194-196` `if (range <= 0.0f) { for(...) data[i]=0.0f; return; }` — 常量图（`max_val==min_val`）被**整块清零**而非报错，与"全黑帧"不可区分（静默数据销毁）。
- `:222` `local_maxima_map` 的 `se` 排除 (0,0)，故 **radius=0 ⇒ `se` 空 ⇒ 每个像素 `val=-1e30` ⇒ 所有 `< limit` 的像素被判为局部极大**（整幅图退化）。
- `:293` `upsample_bilinear` `x_ratio=(sw-1)/(dw-1)`，**dw==1 ⇒ 除零 ⇒ `sx=0*inf=NaN` ⇒ `(int)NaN` ⇒ 越界读**。`extract_lowfreq_atrous:415` 在 `w<downsample_factor` 时可产生 `dw==1`。
- `:152,210,116` `radius*radius` / `ksize*ksize` 为 int 乘法，radius > 46340 溢出 UB。

**【S7】背景约束在近零/负背景域退化，且全部夹具 B≥10 从不覆盖** — `src/dpsf_psf.cpp:627-628`

判据 `|B − bkg0| / max(bkg0, 0.01) > 0.5 ⇒ NO_CONVERGENCE`。实测算术：

| bkg0 | B | 分母 | 比值 | 结果 |
|---|---|---|---|---|
| 100 | 120 | 100 | 0.20 | accept |
| 10 | 12 | 10 | 0.20 | accept |
| 0.5 | 0.7 | 0.5 | 0.40 | accept |
| **0.05** | 0.1 | 0.05 | **1.00** | **REJECT** |
| **0.001** | 0.05 | 0.01 | **4.90** | **REJECT** |
| **−0.002** | 0.05 | 0.01 | **5.20** | **REJECT** |

即 `bkg0` 降到 0.05 counts 以下，分母塌到 0.01，容差变成 **|B−bkg0| ≤ 0.005 counts**；`bkg0` 为负时同理。而本项目的 `cleaned` 块（`README.md:44`，DATA-P1-COSMETIC）正是**扣天光后**的产物，背景≈0 属正常域。

**全部夹具的背景值**：`fixtures.hpp:79` 默认 B=50、`:229` bkg_mean=80、`core.cpp:406` 80、`:545` 100、`:870` 100、`:1048` 100、`nan_sort:37,55` 10/20。**最小 10，无一例接近 0 或为负** ⇒ 该退化域**零覆盖**。

口径声明：判据本身在 `README.md:122` 与 `module.yaml:147-148` 被登记为冻结语义，故这不是"实现跑偏"，而是**冻结的设计在生产域不成立且无测试**。我未执行流水线，不能断言线上已发生大面积拒拟合；但**算术与覆盖缺口两项均为确定事实**。

**【S8】effective PSF 的权重无符号校验，可产出 `ok=true` 的负值算子** — `src/psf_information.cpp:158,166-171,181-183`

`:158` `const double w = alpha_k[k] * a_k[k];` **不校验 `w > 0`**；`:153` 只对**输入** profile 套 `psf_profile_stats`（含 P≥0 与归一），**输出 `num[]` 不复检**。头 `:5` 把 `P_k(u,v) ≥ 0`（FZ-COND-WHITENOISE）列为管辖条件，`phase1_product.cpp:701` 更把 `a_nea_ref` 写进产品 provenance。

反例（构造）：取 `alpha_k=[2,-1,0.5]`、`a_k=[1,1,1]`，`P_1=P_3=δ(索引0)`（各自 sum=1），`P_2=flat`（n=5，各 0.2）。则 `total = 2-1+0.5 = 1.5 > 0` 通过 `:168`；`num[0]=2.3`、`num[i>0]=-0.2` ⇒ `out.ok=true` 且剖面**含负值**。下游 `fwhm_from_half` 在其上照常返回看似合理的 0.92 px。

活消费者已核实存在：`lib/algorithms/integration/phase1_product/src/phase1_product.cpp:840`、`phase2_integrate/src/phase2_integrate.cpp:846`。

**【S9】`fwhm_from_half` 单侧缺失时镜像伪造另一半** — `src/psf_information.cpp:97-99`

```
if (!have_left && !have_right) return 0.0;
if (!have_left)  left  = right;      // 剖面未降到半高 ⇒ 凭空造出对称值
if (!have_right) right = left;
```
PSF cutout 被图边截断时单侧不下降是**常态**，此处不报"测不到"而**镜像另一半**并以 `ok=true` 上报。伪造值继续流入 `:179-180` 的 `ee_r1`/`ee_r2`。头 `:85` 只授权"线性插值半高点"，未授权镜像。

**【S10】日志：Windows 路径指向已不存在目录；`atoi` 容错 fail-open** — `src/dpsf_log.cpp:36,41,22`

- `:41` Windows 日志路径 `lib\dynamic_psf\logs\dynamic_psf.log` —— `lib/dynamic_psf` 实测**不存在**（模块早已 `dynamic_psf` → `psf`）。POSIX 侧 `:36` 是新路径 ⇒ Windows 分支是改名残留。
- `:22` `g_dpsf_log_level = (v >= 0 && v <= 3) ? v : LOG_INFO;`：`atoi("abc") == 0` ⇒ 通过 `v>=0` ⇒ 落到 **LOG_INFO（最啰嗦档）**。环境变量拼错 ⇒ 日志量最大化。且 `:4-7` 把 INFO=0 排在 DEBUG=1 之下，使 0 与 1 两档过滤效果等价，枚举分级失效。
- `:19` 环境变量名 `DYNAMIC_PSF_LOG_LEVEL` 仍是改名前缀，与诊断面 `DPSF_DIAG_PATH` 不一致。
- 日志写入**仓内**相对路径 `lib/algorithms/psf/logs/`（依赖 CWD），与 AGENTS.md §6「不写未声明文件」相悖。

**【S11】负例门：任意无关失败都算"注入被检出"** — `p1psf_center_contract_gate.cpp:288-292`

`g_failures` 汇总 **全部** `[C1]..[C5]`（`:164,:191,:221,:253,:260`），任一红即打印 `NEGATIVE INJECTION DETECTED` 并 rc=0；`CMakeLists.txt:250-251` 再以正则判绿。反例：让 `snr_extract_model_v3` 返回非 0 ⇒ `[C3]` 红 ⇒ 该负例门 PASS ⇒ 此后对原点平移永久失能。

**正确写法就在同片内**：`p1psf_centroid_gate.cpp:300-305` 只聚合 `sep_legacy` 这一条具体量。可直接照抄。

**【S12】三处位级相等断言缺 `rc` 守卫 ⇒ 恒真门** — `core.cpp:414-417`(P4)、`:275-278`(U6)、`:1103-1106`(B4)

三处都调批 API 两次、**不检查返回码**，只断言两次输出互相相等；缓冲区分别预填 `-1.0`、`nv` 预填 `-5`。若被测函数**一致地**提前返回且不触碰输出，两个条件同时成立 ⇒ 绿。

对照（证明是局部漏写而非风格）：同文件 P3 `:383`、`U5 :239`、`prodpath_check.cpp:77` 都有 `rc == 0 && ...` 前置守卫。

**【S13】筛掉真信号：丢弃失败星 + 25/120 弱守卫** — `p1psf_centroid_gate.cpp:189,195,311`

`:189`（配对失败）与 `:195`（`r2 != 0 || n_valid != 1 || !isfinite`）静默 `continue`；`:311` 门槛 `total_matched >= 25`，而 `total_truth = 3×40 = 120`。被丢掉的恰是 **dpsf 拟合失败/输出非有限**的那批。统计量只在"dpsf 成功的那批"上求中位数/p95。

反例：注入一个让 dpsf 在低 SNR 域返回 `n_valid=0` 的背景估计缺陷 ⇒ 仅 SNR=300 那组可匹配 ⇒ `matched≈40 ≥ 25` ⇒ `[P0]` 绿，`[P2]..[P5]` 在容易的星上统计 ⇒ 全绿。门在 dpsf 2/3 目标域失效时判绿。

**【S14】`kTrimMeanToSigma` 在生产契约头里断言了一个其引用的权威文档自身否定的值** — `src/dpsf_psf.h:11-12`

```
robust_residual_sigma=residual_scale/0.7316727929211932 …（E[trimmed mean |r|]=0.7316727929211932·σ, docs/science/PSF.md §9 / §14）
```
被引用的 `docs/science/PSF.md:179` 自己写：闭式 = **`2(φ(Φ⁻¹(0.55))−φ(Φ⁻¹(0.95)))/0.8 = 0.7316730952806134`**，「本仓复算…与实现常量相对差 **4.13e-7**」；`:127` 复述同一差。按 AGENTS.md §3（文档高于代码），此处**生产头断言的科学恒等式与仓内一级正本矛盾**。仓内台账 `FIX_LEDGER.csv:693`（V12-N-04）已登记该问题且状态 **OPEN**。

口径：该常量本体定义在 `lib/algorithms/noise_snr/`（他片），本条只针对 **psf 片内生产契约头作出错误断言**。

**【S15】构建注释与事实矛盾；被测面窄于出货产物** — `tests/p1psf/CMakeLists.txt:24-25`

注释称「根构建**无** dynamic_psf 生产 target，生产源独立编入测试目标」。实测根 `CMakeLists.txt:1205-1208` 有 `add_library(acsd_p1_dpsf STATIC dpsf_psf.cpp dpsf_image.cpp dpsf_log.cpp)`。两后果：
(a) 9 个 p1psf target **重编**生产源而非链接 `acsd_p1_dpsf` ⇒ 门测的不是出货产物（同仓 `eng/tests/unit/CMakeLists.txt:1647` 的 `w34_dpsf_nan_sort` 反而链接 `acsd_p1_dpsf`，两种策略并存）；
(b) `dpsf_image.cpp` **不在任何 p1psf target** ⇒ S6 的死代码与其中的 4 个缺陷完全无测试。

**【S16】`MOFFAT4_FWHM_FACTOR` 圆整偏差 +1.908e-6，且 4 处副本** — `src/dpsf_psf.cpp:174`

实测 `1.230310` vs 闭式 `2√2·√(2^{1/4}−1) = 1.2303076525901024`，**相对差 +1.9079861e-6**。该常量按定义可现场导出（AGENTS.md §6「可由几何或物理导出的应现场计算」），且正确闭式就在同片测试 oracle `oracle.hpp:146 oracle_fwhm_factor_exact()` 里。字面量副本散落 5 处：`dpsf_psf.cpp:174`、`oracle.hpp:54`、`fixtures.hpp:50`、`centroid_gate.cpp:59`、`fitcost_probe.cpp:113`；生产常量改动不会被任何一处报错。

**公平登记**：该偏差**已被仓内科学正本如实披露**（`docs/science/PSF.md:104` 「实现常量 1.230310 与精确值相对差 +1.91e-6」、`:230` 「残差 1.2e-6 = 实现常量圆整量，判绿」），且 `tests_core.cpp:619-631` 的 `O-σ` 组用闭式常量独立校验它（门限 3.0e-6，实测 1.908e-6，**余量仅 1.57×**）。故这是**已登记未修**，不是隐藏缺陷；严重度定为须修。

### 4.3 建议（8）

| # | 位置 | 问题 |
|---|---|---|
| A1 | `oracle.hpp:49-51` | `oracle_flux` 死代码（零调用），却给人"已用独立 oracle"的错觉 |
| A2 | `test_main.hpp:27-28` | `g_p1psf_pass` / `g_p1psf_fail` 定义后从未读写 |
| A3 | `CMakeLists.txt:19-21` | 头注宣称注册 7 个 ctest，实测 **15 个**（`grep -c '^add_test'`）；未列出的 8 个含**全部合同门**（prodpath_centroid / centroid_gate(+_neg) / center_contract_gate(+_neg,_astropy) / stall_equiv(+_inject)）。按头注审计会漏 53% |
| A4 | `stall_equiv.cpp:288-293` | `group_junk` 与 `group_equiv` 同函数同语料同断言，仅 tag 字符串不同 ⇒ 零信息增量 |
| A5 | `stall_equiv.cpp:213-216,231` | 最差误差在**成功子集**上取极值，且 `:231` 容忍 12 颗中失败 1 颗 ⇒ 筛掉真信号 |
| A6 | `Makefile:12` | `SOURCES` 只有 3 个 TU，漏 `psf_information.cpp`（该文件由 `phase1_product` / `phase2_integrate` / `p1_psfw` 三个 target 编译，**非死码**，本假设已被证伪） |
| A7 | `center_contract_gate.cpp:138` | `--export` 若为最后一个参数，整块静默跳过，无提示 |
| A8 | `astropy.py:74-75,76-81` | `px_ref` 算出未用；`kind` 解构未用；`:69 pt0=(10.0,12.0)` 与 `center_contract_gate.cpp:127` 跨语言重复魔数 |

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻 | 是否推翻 |
|---|---|---|---|
| C1 | 把 `dpsf_psf.cpp:604-605` 的 LM 求解整体短路、直接返回初值 `sx/sy/A` | `core.cpp:313/316/318/321` 应判红 | **推翻成立**：四条只拿错值重算同式，仍全绿 ⇒ 确认 B1 零鉴别力 |
| C2 | 把 `dpsf_psf.cpp:353` 的 `if (stall_iters>0 && ++stall>=stall_iters)` 改成无条件提前退出（真实缺陷：优化泄漏到不该截断的星） | `p1psf_stall_equiv` 正例门应红 | **推翻成立**：正例跑在阈值 0，走不到该段 ⇒ 仍全绿 ⇒ 确认 B2 |
| C3 | 让 `dpsf_fit_batch_f64` 对任何输入返回 −1 | `p1psf_stall_equiv_inject` 应判红（判据失效） | **推翻成立**：`diffs=1` 被当成检出 ⇒ PASS ⇒ 确认 B3 fail-open |
| C4 | 把 `core.cpp:1119` 组名 `"units"` 改成 `"unit"` | selfcheck 基线相应失败 | **推翻成立**：两相都跳过全部组 ⇒ `return 0` ⇒ 真空绿 ⇒ 确认 B4 |
| C5 | 构造 `alpha_k=[2,-1,0.5]`、`P_1=P_3=δ_0`、`P_2=flat` | `conventional_effective_psf` 应因负权重拒 | **推翻成立**：`total=1.5>0` 通过，`num[i>0]=-0.2`，`ok=true` ⇒ 确认 S8 |
| C6 | 令 `iterative_sigma_clip` 收 `n=50` 的样本 | 应至少告警阈值不适用 | **推导成立**：第 1 轮 `clipped.size()≤50<100` ⇒ 返回 round-0 值，**截尾从未发生**且无告警 ⇒ S6 |
| C7 | `local_maxima_map(..., radius=0, ...)` | 应返回空或报错 | **推导成立**：`se` 空 ⇒ `val=-1e30` ⇒ 所有 `<limit` 像素判为极大 ⇒ S6 |
| C8 | `upsample_bilinear` 收 `dw=1`（`extract_lowfreq_atrous` 在 `w<downsample_factor` 时可达） | 应报错 | **推导成立**：`(sw-1)/0 = inf` ⇒ `0*inf = NaN` ⇒ `(int)NaN` ⇒ 越界读 ⇒ S6 |
| C9 | 断言「`psf_information.cpp` 未编入任何 target，是死码」 | — | **假设被证伪（我自己的）**：`phase1_product/CMakeLists.txt:27`、`phase2_integrate/CMakeLists.txt:45`、`eng/tests/unit/p1_psfw/CMakeLists.txt:26` 三处均编译它，且有活消费者 |
| C10 | 断言「`test/test_dpsf_nan_sort.cpp` 是从不构建的孤儿测试」 | — | **假设被证伪（我自己的）**：`eng/tests/unit/CMakeLists.txt:1641-1648` 注册为 `w34_dpsf_nan_sort` |
| C11 | 断言「`dpsf_image.cpp` 的 `robust_median` 有模块外消费者（`noise_snr`）」 | — | **假设被证伪（我自己的）**：`noise_model.cpp:100` 是 `std::vector<double>&` 重载，与 `dpsf_image.h:18` 的 `(const float*, int)` 非同一符号 ⇒ S6 零调用者结论**反而更稳固** |
| C12 | 断言「`[P5]` 通过蕴含 `[P3]` 通过（恒真门）」 | — | **假设被证伪（子代理 A 的发现 6，见 §7）** |

---

## 6. 盲复算

遮住既有判定，独立重取证据后的结论：**判一致，但严重度偏松**（我上调 2 条、下调 0 条）。

独立重算要点：

1. **正例门是否真的没编宏** — 不看子代理结论，直接 `sed -n '79,88p' CMakeLists.txt` 与 `sed -n '286,292p' dpsf_psf.cpp`：正例 target 无 `target_compile_definitions`；源 `#ifndef … #define … 0`。⇒ B2 成立。
2. **负例门是否 fail-open** — 直接读 `:246-248` 与 `:273-282`，沿 `run_equiv_group` 返回值追到 `diffs`。⇒ B3 成立。
3. **自洽式断言** — 直接比对 `dpsf_psf.cpp:653` 与 `core.cpp:318` 的字符，以及 `:174` 与 `oracle.hpp:54` 的字面量；再用 `grep -c oracle_flux` 得 1（仅定义）。⇒ B1 成立。
4. **README 锚** — 逐条 `grep -n` 实测定义行，17 条无一命中。⇒ S1 成立。
5. **"零消费者"** — 先按字面 grep（命中 `module_ports.registry.json:325` 等声明），再读 `module_adapters.cpp:5778-5791` 的实际取值循环。⇒ S2 成立。
6. **判据文件是否还在** — `ls eng/ci` → 不存在；`find -name check_prod_wiring*` → 只在 `run/**`。⇒ S3 成立。

**我比既有登记更严的两处**：
- `dpsf_image.cpp` 从「零调用者」进一步量化到**逐函数 17/17 为 0**，并补上「与 `sdet_image.cpp` 一一对应的死复刻」这一结构性判断（S6）。
- 背景约束从「记一条」升级为**给出 6 行算术表 + 证明全部夹具 B≥10**（S7），从"可能有问题"变成"覆盖缺口确定"。

**我比既有登记更松的一处**：README 锚失效（S1）虽属"文档自称已复测而实际全错"，但它不影响任何门的行为，故定为**须修**而非阻断；阻断名额留给 3 条真正制造假绿的机制（B1/B2/B3/B4）。

---

## 7. 子代理派发记录

派发 **5 个**（要求 3-5），全部只读、禁编译、禁 git 写、禁读 `/tmp/acsd_g08/`。

| 代号 | 范围 | 份数/行 | 状态 |
|---|---|---|---|
| A | `p1psf_tests_core.cpp` / `centroid_gate.cpp` / `stall_equiv.cpp` / `center_contract_gate.cpp` | 4 / 2055 | 已回，3 阻断 + 若干须修 |
| B | `fitcost_probe.cpp` / `prodpath_check.cpp` / `tests_selfcheck.cpp` / `tests_perf.cpp` / `test_dpsf_nan_sort.cpp` | 5 / 876 | 截止交付未回 |
| C | `oracle.hpp` / `fixtures.hpp` / `test_main.hpp` / `astropy.py` / `CMakeLists.txt` / `tests_main.cpp` | 6 / 981 | 已回，12 阻断/须修 + 27 建议 |
| D | `README.md` / `module.yaml` / `memory.md` / `Makefile` / `.gitignore` / `dpsf_image.h` / `dpsf_log.h` / `dpsf_log.cpp` | 8 / 657 | 截止交付未回 |

> A、C 的覆盖范围**已由我本人逐字重读全部 29 份**（§1），故 B、D 未回**不构成覆盖缺口**；其范围内的问题（探针 `fopen` 静默、日志路径、`eng/ci` 悬空、`psf_params` 消费者数）我已独立取证并写入 S2/S3/S10 与 A7。

### 逐条复核与否决

**采纳（复核后确认成立）**

| 子代理结论 | 我的独立复核 |
|---|---|
| A-阻断1 正例门恒真 | ✅ 采纳 → **B2**（亲自读 `CMakeLists.txt:79-87` + `dpsf_psf.cpp:288-291,472,740-766`） |
| A-阻断2 负例门 fail-open | ✅ 采纳 → **B3**（亲自追 `run_equiv_group` 返回路径） |
| A-阻断3 自洽式断言 | ✅ 采纳 → **B1**（亲自比对 `:653` 与 `:318` 字符） |
| A-须修8 P4/U6/B4 缺 rc 守卫 | ✅ 采纳 → **S12**（亲自核对 `:414/:275/:1103` 与 P3/U5/prodpath 的对照） |
| A-须修7 负例任意失败即检出 | ✅ 采纳 → **S11** |
| A-须修4 筛掉真信号 | ✅ 采纳并**上调**为 **S13** |
| A-须修5 `[P1]` 恒真 | ✅ 采纳（我另补严格性：`star_coord_contract.h:42-57` 的 `(v−0.5)+0.5` 对所有指数 ≤51 的 double **精确**，故恒为 0.0） |
| C-B1/B2/B3 oracle 抄实现 | ✅ 采纳 → 并入 **B1** |
| C-B4 runner 真空绿 + 传染 selfcheck | ✅ 采纳 → **B4**（我亲自读 `test_main.hpp:176-191` 确认） |
| C-B7 构建注释与事实矛盾 | ✅ 采纳 → **S15** |
| C-B9/B10 悬空标准与错章节锚 | ✅ 采纳 → **S5** |
| C-B11 头注列 7 实测 15 | ✅ 采纳 → **A3**（`grep -c '^add_test'` = 15） |
| C-B12 `G-P1-CENTER-CONTRACT-1` 未登记 | ✅ 采纳（`grep -rn docs/` 零命中；门头 `center_contract_gate.cpp:23` 自陈"待登记"） |

**否决 / 降级（明确记录）**

| 子代理结论 | 否决理由 |
|---|---|
| **A-须修6「`[P3]` 被 `[P5]` 蕴含 ⇒ 恒真门」** | **否决（数学方向反了）**。`star_coord_contract.h:49-51` 确实使 `psf` 与 `dpsf_frame` 为同一向量，但 `median_abs` = median(\|d\|) 而 `[P5]` 用 \|median(d)\|，恒有 **median\|d\| ≥ \|median d\|**。该不等式给 `[P3]` 的是**下界**，不是上界，故 `[P5]` 通过**不能**推出 `[P3]` 通过。反例 d=[−0.5,+0.5]：`[P5]`=0 ≤ 0.05 ✓，`[P3]`=0.5 > 0.1 ✗。⇒ `[P3]` 是**更强**判据，非恒真。**记入 C12。** |
| A-建议17「OpenMP parallel for 是否属私建线程池，未读合同不下结论」 | 认可其保留。我同样**不签**该结论：`dpsf_psf.cpp:902,1027,1160,1334` 的 `#pragma omp parallel for` 是否违反 AGENTS.md §6「池所有权归调度器」，须以 `docs/engineering/SCHEDULER_CONTRACT.md` 判定，超出本片。**登记为 UNRESOLVED，不计入发现。** |
| C-B24「`oracle_sigma_axes` 恒返回 {max,min}，不提供宣称的独立谱路径」 | **否决**。`oracle.hpp:101-107` 是 2×2 对称阵的**解析特征值**，对 `M=R·diag(sx²,sy²)·Rᵀ` 精确给出 {sx,sy}；`core.cpp:570-573` 对**拟合侧与真值侧**同时施加，系统误差相消而比较仍有效。这是正确数学，非缺陷。 |
| C-B22「Box-Muller 只取 cos ⇒ 样本非独立」 | **否决**。`fixtures.hpp:43-48` 每次调用消耗**新的一对** uniform ⇒ 各次抽样独立；被丢弃的是同对的 sin 分支（标准浪费变体），不影响独立性。`u1==0 ⇒ log(0)` 概率 2⁻⁵³，且 `core.cpp` 对噪声统计零断言 ⇒ 即使成立亦无影响。 |
| C-B6「perf 门在缺 OpenMP 时结构性恒红」 | **存疑不签**。源码推理成立（`perf.cpp:34-40` 的 `set_threads` 在无 `_OPENMP` 时是空操作 ⇒ `med1==med4` ⇒ `ratio=1.0 < 1.60`），但我无证据判定当前 CI 是否真缺 OpenMP，且我禁止执行 ⇒ 不计入发现。 |
| C-B25「负例门用 `PASS_REGULAR_EXPRESSION` 忽略退出码」 | **降级为机制说明**，已由更具体的 **B3**（stall_equiv）与 **S11**（center_contract）承载，不单列。 |
| A-建议12「`g_p1psf_pass/g_p1psf_fail` 死代码」 | ✅ 采纳 → **A2**（但仅为建议级）。 |

**我否决的自己的假设**：C9（psf_information 未编译）、C10（nan_sort 孤儿）、C11（robust_median 有外部消费者）—— 三条均在复核中被证据推翻，且**推翻方向都使 S6 更稳固**。见 §5。

---

## 8. 自证段（可复跑命令）

> 全部只读。中文路径已用 `git -c core.quotepath=false`。未编译、未执行任何项目二进制；`python3 -c` 仅做纯算术复算，不加载项目代码。

```bash
cd "/workspace/Astro CS Database"
git -c core.quotepath=false log -1 --format='%H'   # 期望 850a9ede…

# ── 成员清单与行数（覆盖率口径）──
awk '/^  - 片号: ALG-psf-001$/,/^  - 片号: ALG-resample-001$/' \
  run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml \
  | grep -oP '(?<=^      - ")[^"]+' > /tmp/psf_members.txt
wc -l < /tmp/psf_members.txt                       # 期望 29
while IFS= read -r f; do wc -l < "$f"; done < /tmp/psf_members.txt | paste -sd+ | bc   # 期望 6969

# ── B2 正例门恒真 ──
grep -n "DPSF_BATCH_STALL_ITERS" lib/algorithms/psf/tests/p1psf/CMakeLists.txt lib/algorithms/psf/src/dpsf_psf.cpp
sed -n '79,88p'   lib/algorithms/psf/tests/p1psf/CMakeLists.txt   # 正例 target 无 target_compile_definitions
sed -n '20,26p'   lib/algorithms/psf/tests/p1psf/p1psf_stall_equiv.cpp  # 谎称"默认阈值 60"
sed -n '286,292p' lib/algorithms/psf/src/dpsf_psf.cpp               # #define DPSF_BATCH_STALL_ITERS 0
sed -n '470,474p' lib/algorithms/psf/src/dpsf_psf.cpp               # 默认 stall_iters = 0
sed -n '740,746p;760,766p' lib/algorithms/psf/src/dpsf_psf.cpp     # 两条路径都拿到 0
sed -n '351,360p;393,406p' lib/algorithms/psf/src/dpsf_psf.cpp     # stall 逻辑被 if(stall_iters>0) 包住

# ── B3 负例门 fail-open ──
sed -n '243,252p;271,286p' lib/algorithms/psf/tests/p1psf/p1psf_stall_equiv.cpp
sed -n '101,103p' lib/algorithms/psf/tests/p1psf/CMakeLists.txt    # PASS_REGULAR_EXPRESSION

# ── B1 自洽式断言 ──
sed -n '311,323p;582,586p;893,896p' lib/algorithms/psf/tests/p1psf/p1psf_tests_core.cpp
sed -n '174p;617,618p;653,656p'      lib/algorithms/psf/src/dpsf_psf.cpp
sed -n '49,62p' lib/algorithms/psf/tests/p1psf/p1psf_oracle.hpp
grep -c "oracle_flux" -r lib/algorithms/psf     # 期望 1（仅定义处，零调用）
sed -n '418p' "artifacts/evidence/governance-01/research/R-7_AIO_HIPS_ABI与精度.md"

# ── B4 runner 真空绿 ──
sed -n '175,192p' lib/algorithms/psf/tests/p1psf/p1psf_test_main.hpp
sed -n '84,92p;102,108p' lib/algorithms/psf/tests/p1psf/p1psf_tests_selfcheck.cpp

# ── S1 README 行号锚全失效 ──
F=lib/algorithms/psf/src/dpsf_psf.cpp
grep -n "^DPSF_EXPORT int dpsf_fit\b\|^DPSF_EXPORT void dpsf_free\|^DPSF_EXPORT int dpsf_fit_batch\|^static int dpsf_batch_dims_check\|^static int lm_solve\|^static bool gauss_solve\b\|^static double compute_trimmed_mad\|^static int moffat4_fit_tmpl\b\|MOFFAT4_FWHM_FACTOR =" $F
# 对照 README.md:78-92 声称的 34/105/203/238/459/514/534/575/661/681/694/792/937 → 全部不符
sed -n '5,7p;60p;72p' lib/algorithms/psf/README.md     # 自称"全量复测刷新""实测行号"

# ── S2 "零消费者"被证伪 ──
sed -n '5774,5795p' lib/infrastructure/scheduler/src/module_adapters.cpp
grep -n "零消费者" lib/algorithms/psf/README.md lib/algorithms/psf/module.yaml \
  lib/infrastructure/scheduler/src/module_adapters.cpp
sed -n '325p' lib/infrastructure/pipeline/module_ports.registry.json
sed -n '91p' "实验/engineering-evidence/audit-2026-01/FIX_LEDGER.csv"

# ── S3 判据整目录已删（只剩 run/ 归档与 .pyc）──
ls -d eng/ci                                    # 期望: No such file
find . -name "check_prod_wiring*" | grep -v '^./run/'   # 期望: 空
find . -name "check_prod_wiring*" -path '*__pycache__*'  # 期望: 孤儿 .pyc
ls -d eng/ci/ledgers                            # 期望: No such file

# ── S4 文档指向的测试树不存在 ──
ls -d eng/tests/p1psf eng/tests/unit/p1psf      # 期望: 均 No such file
sed -n '187,192p' lib/algorithms/psf/README.md
sed -n '1298p' eng/tests/unit/CMakeLists.txt     # 真实挂载点

# ── S5 容差标准不存在 ──
find . -name "*MODULE_SOURCE_TEST_STANDARD*"   # 期望: 空
sed -n '72,80p;161,178p' lib/algorithms/psf/README.md   # §5 是 symbols 表，容差在 §9

# ── S6 dpsf_image 全死 + 内部缺陷 ──
F2=lib/algorithms/psf/src/dpsf_image.cpp
for f in truncate_and_rescale local_maxima_map iterative_sigma_clip upsample_bilinear robust_mad extract_lowfreq_atrous; do
  grep -rn "\b$f\b" --include=*.cpp --include=*.h --include=*.hpp . 2>/dev/null \
   | grep -v '^./run/' | grep -v '^./build/' | grep -v '^./out/' \
   | grep -v "sdet_$f" | grep -v '^./lib/algorithms/psf/' | grep -v '^./lib/algorithms/star_detection/'
done                                              # 期望: robust_mad 无输出
sed -n '379p;194,196p;222p;293p' $F2
grep -n "sdet_truncate_and_rescale\|sdet_local_maxima_map\|sdet_iterative_sigma_clip" \
  lib/algorithms/star_detection/src/sdet_image.h   # 一一对应的死复刻

# ── S7 背景约束在近零/负背景退化；夹具 B≥10 ──
sed -n '627,628p' lib/algorithms/psf/src/dpsf_psf.cpp
python3 -c "
for bkg0,B in [(100.,120.),(10.,12.),(.5,.7),(.05,.1),(.001,.05),(-.002,.05)]:
    r=abs(B-bkg0)/max(bkg0,0.01); print(f'bkg0={bkg0:>7} B={B:>6} ratio={r:>8.4f} {\"REJECT\" if r>0.5 else \"accept\"}')"
grep -n "fix_psf_a(double B = \|bkg_mean\|, 80.0, 3.0\|, 100.0, 3.0" \
  lib/algorithms/psf/tests/p1psf/p1psf_fixtures.hpp lib/algorithms/psf/tests/p1psf/p1psf_tests_core.cpp
# 期望: 最小 B=10，无接近 0 或负值

# ── S8 effective PSF 负权重 ──
sed -n '148,171p;178,184p' lib/algorithms/psf/src/psf_information.cpp
sed -n '5p' lib/algorithms/psf/include/acsd/psf_information.h
grep -n "conventional_effective_psf" \
  lib/algorithms/integration/phase1_product/src/phase1_product.cpp \
  lib/algorithms/integration/phase2_integrate/src/phase2_integrate.cpp   # 活消费者

# ── S9 fwhm 镜像伪造 ──
sed -n '95,101p' lib/algorithms/psf/src/psf_information.cpp

# ── S10 日志 ──
ls -d lib/dynamic_psf                            # 期望: No such file（Windows 分支指向它）
sed -n '36,45p;19,27p' lib/algorithms/psf/src/dpsf_log.cpp

# ── S11/S12 负例门与 rc 守卫 ──
sed -n '286,297p' lib/algorithms/psf/tests/p1psf/p1psf_center_contract_gate.cpp
sed -n '298,309p' lib/algorithms/psf/tests/p1psf/p1psf_centroid_gate.cpp   # 正确写法对照
sed -n '414,417p;275,278p;1103,1106p' lib/algorithms/psf/tests/p1psf/p1psf_tests_core.cpp
sed -n '383p;239p' lib/algorithms/psf/tests/p1psf/p1psf_tests_core.cpp  # 有守卫的对照
sed -n '77p' lib/algorithms/psf/tests/p1psf/p1psf_prodpath_check.cpp

# ── S13 筛掉真信号 ──
sed -n '186,199p;309,312p' lib/algorithms/psf/tests/p1psf/p1psf_centroid_gate.cpp   # 3×40=120 vs 门槛 25

# ── S14 kTrimMeanToSigma 断言与正本矛盾 ──
sed -n '11,12p' lib/algorithms/psf/src/dpsf_psf.h
grep -n "0.7316730952806134\|4.13e-7" docs/science/PSF.md
sed -n '693p' "实验/engineering-evidence/audit-2026-01/FIX_LEDGER.csv"   # V12-N-04 OPEN

# ── S15 构建注释矛盾 ──
sed -n '23,25p' lib/algorithms/psf/tests/p1psf/CMakeLists.txt
sed -n '1205,1208p' CMakeLists.txt                 # 期望: add_library(acsd_p1_dpsf STATIC …)
sed -n '1647p' eng/tests/unit/CMakeLists.txt       # w34 链 acsd_p1_dpsf（策略对比）

# ── S16 FWHM 常量偏差 + 副本 ──
python3 -c "
import math
h=math.sqrt(2.0**0.25-1.0); e=2.0*math.sqrt(2.0)*h; p=1.230310
print('rel diff=',(p-e)/e, ' margin x',3.0e-6/abs((p-e)/e))"
grep -rn "1\.230310" lib/algorithms/psf/

# ── A3 头注 7 vs 实测 15 ──
grep -c "^add_test" lib/algorithms/psf/tests/p1psf/CMakeLists.txt   # 期望 15
sed -n '19,21p' lib/algorithms/psf/tests/p1psf/CMakeLists.txt        # 头注列 7

# ── C12 否决 A-须修6：median|d| ≥ |median d|，[P5] 不能蕴含 [P3] ──
# centroid_gate.cpp:79-86 percentile 为线性插值分位数
python3 -c "
def pct(v,p):
    v=sorted(v); i=p*(len(v)-1); lo=int(i); hi=min(lo+1,len(v)-1)
    return v[lo]+(v[hi]-v[lo])*(i-lo)
d=[-0.5,0.0,0.5]                      # psf 样本 = dpsf_frame 样本 (star_coord_contract.h:49-51)
p5=abs(pct(d,0.5)); p3=pct([abs(x) for x in d],0.5)
print(f'P5 |median(d)|={p5}  (门限 0.05) -> {\"PASS\" if p5<=0.05 else \"FAIL\"}')
print(f'P3 median|d|={p3}   (门限 0.10) -> {\"PASS\" if p3<=0.10 else \"FAIL\"}')
print('=> P5 绿而 P3 红 ⇒ [P5] 不能蕴含 [P3], A-须修6 否决')"
sed -n '41,57p' lib/infrastructure/pipeline/orchestrator/cpp/include/star_coord_contract.h

# ── C9/C10/C11 我自己的假设被证伪 ──
grep -n "psf_information.cpp" lib/algorithms/integration/phase1_product/CMakeLists.txt \
  lib/algorithms/integration/phase2_integrate/CMakeLists.txt eng/tests/unit/p1_psfw/CMakeLists.txt
sed -n '1641,1648p' eng/tests/unit/CMakeLists.txt   # w34_dpsf_nan_sort 已注册
sed -n '100p' lib/algorithms/noise_snr/cpp/src/noise_model.cpp; sed -n '18p' lib/algorithms/psf/src/dpsf_image.h
```

---

## 9. 登记为 UNRESOLVED（不计入发现，需负责人裁定）

1. **`dpsf_psf.cpp` 的 `#pragma omp parallel for`（`:902,1027,1160,1334`）是否构成 AGENTS.md §6「私建线程池」违规**：须以 `docs/engineering/SCHEDULER_CONTRACT.md` 判定。本片无该合同正文，不签结论。
2. **S7 的线上影响面**：判据在近零/负背景域退化已证，但生产 `cleaned` 块的实际背景分布需跑一次真实帧才能定量。我禁止执行，移交前台。
3. **oracle.hpp:111-112 与 fixtures.hpp:159-164 的"probe 实证"容差/状态码数字**：均为仓外实验，仓内无 probe 脚本或日志路径，不满足 AGENTS.md §4「仓内实验＝复现命令＋实测输出」，证据不可复核。

---

*本文件为 G08-05 对抗审稿第 1 遍交付件。审稿人未改任何仓内文件、未执行任何 git 写、未编译、未跑测试。*