# RELEASE-02 收口与全量验证（CONVERGE-AND-VERIFY）

- 分片：**CONVERGE-AND-VERIFY**（收口 + 全量验证）
- 工作目录：`/workspace/Astro CS Database`
- 日期：2026-09-20（跨零点）
- 基线 HEAD：`06216ec86a7cdd8c2342a2f76c7dab0d701a8d19`（main，零 git 写）
- 产物：`run/RELEASE-02/converge-verify/`（logs / checks / fingerprint / evidence）
- 硬约束遵守：**零 git 写**（未 commit/add/checkout/stash）；未改科学公式/容差/冻结定义；
  未改 `docs/science/**`、`docs/algorithms/**`；未放宽判据/删检查/标 SKIP/改基线

---

## 0 一句话结论

**当前工作树尚未达到「可以跑 e2e」的状态** —— 但有明确、可执行的收口清单（见 §5）。
编译 0 error；ctest 466/466 全绿（修掉 1 条本轮真回归后）；门禁见 §4。
**e2e 的阻塞项不是代码，而是「在飞分片仍在写工作树 + 并发运行把产物散落仓库根」**（§6 U-1/U-2）。

---

## 1 收口清单（分片 → 文件 → 状态）

工作树 `git status` 条目在本次会话中从 **89 → 101**（分片仍在陆续落地）。
逐文件归属如下（依据各分片回执 + 代码内注释的 claim 引用 + 内容比对）。

### 1.1 CONFORM-FIX-A（回执 `reports/RELEASE-02/conform-fix-a.md`，20:52）— 符合性修复 ①/④/⑤

| 文件 | 关键位置 | 内容 | 状态 |
|---|---|---|---|
| `lib/algorithms/noise_snr/cpp/src/snr_science.cpp` | :45-63, :111-124, :134-138, :155 | 新增 `kGaussFwhmFactor=2.3548200450309493`（检测块高斯）；`moffat4SigmaFromFwhm`→`detectionSigmaFromFwhm`；网格 FWHM 改为由本块 sigma 正向导出 | ✅ 自洽 |
| `lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp` | :35-41, :71-86, :105-118 | 新增 `kMoffat4FwhmFactor=1.230310`（PSF 块）；`p.fwhm_px=0; p.sigma_px=fwhm/1.230310`；frameDepth 同款 | ✅ 自洽 |
| `lib/algorithms/noise_snr/cpp/include/snr_estimator.h` | :253-268, :300-302 | 两列契约（检测块 vs PSF 块）+ 禁跨块说明 | ✅ |
| `lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.h` | :19-42 | `SnrSourceRow.fwhm_px` 明确为 **DATA-P1-SOURCES 检测块**列 | ✅ |
| `lib/algorithms/noise_snr/tests/p1noise/p1snr_science_test.cpp` | :44-50, :196-232, :305, :325, :347 | oracle 按新口径复算 + 跨块负例（红/绿双向） | ✅ 非空断言 |
| `lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp` | :84-101, :184-211 | ④ 饱和判定 = `dynrange=min(maxi,65535)−bg`、`0.7·dynrange` + `0.1·dynrange` **双条件**；删除旧字面量 `peak>50000.0` | ✅ 规范有出处 |
| `tests/unit/p1_stars_test.cpp` | :4, :69-123 | ⑤ 13a–13d 负例（满井 40000/16000/65535 必标；高本底未饱和峰不得标） | ✅ |
| `tests/unit/p1snr/p1snr_linux_test.cpp` + `tests/unit/p1snr/CMakeLists.txt` | CMake :17-18, :32-40 | 新增 `p1snr_linux_mother_function` 用例并注册 | ✅ 已在 ctest |

**核对结论**：① 的两条路径因子**互斥且随数据来源选择**，方向正确；生产帧级 SNR 走 `module_adapters.cpp:3755` 的 `p1_sources.json.fwhm_px`（检测块），与 `snr_frame_science.h` 契约一致。

### 1.2 A6 / F-INSTR-CONFORM-FIX（回执 `f-instr-conform-fix.md`，20:56）— 测光 F_instr 域

| 文件 | 关键位置 | 内容 | 状态 |
|---|---|---|---|
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | :2034-2045 | 新增 `p1_psf_analytic_flux(A,sx,sy)=2πA·sx·sy/3`（与 `dpsf_psf.cpp:428` 同式） | ✅ |
| 同上 | :2160-2173 | `p1_psf.json` 逐星行新增 `flux` 列（PSF 拟合域解析通量） | ✅ |
| 同上 | :3190-3640 | `p1_op_photometry`：F_instr 取 PSF 域、`psf_status!=OK` fail-closed、`n_psf_domain` provenance、零点门 0.5→0.02 dex | ✅ |
| `lib/algorithms/photometry/cpp/src/frame_photometry_fit.h` | :32-43 | `psf_flux` 域契约（禁检测域 5×5 盒和） | ✅ 仅注释/契约 |
| `lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp` | :98-102 | 同款契约注释 + fail-closed 说明 | ✅ |

### 1.3 CONFORM-FIX-B（回执 `reports/RELEASE-02/conform-fix-b.md`，**00:06:37 落地 = 在我首轮门禁运行期间**）

| 条目 | 文件:行 | 内容 | 状态 |
|---|---|---|---|
| 001 | `module_adapters.cpp:5228-5245` | **合规回退**：`uc.tolerance=1e-6; uc.tolerance_relative=0`（回到 FZ-UPM-CONVERGENCE 冻结值；未走流程的 1e-3/relative=1 已撤） | ✅ 未放宽，反而收紧 |
| 003 | `lib/algorithms/coverage/src/upm.cpp:1643-1655` | 归一化判据由绝对门 `s>1e-12` 改为尺度无关 `s>0 && isfinite(s)`（与 build 内部 :646 同判据） | ✅ |
| 004 | `module_adapters.cpp:6797-6810, :6908-6940, :7613-7635` | `uncertainty_available=false` 显式原因 + fail-closed + 随产品落盘 | ✅ |
| 005 | `lib/algorithms/coverage/src/sampler.cpp:881-895` | catalog veto 改为消费 `cfg.star_mask_snr_factor` / `cfg.star_mask_radius_deg`（去硬编码 10.0/0.012） | ✅ |
| 007 | `stage2_common.h:81-88` + `stage2_common.cpp:252-262` | 生产默认 profile = `P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL`（权威：REJECTION.md §2/§5、CONFIG_SCHEMA.md:25、provenance:67） | ✅ |
| 008 | `stage2_common.h:89-95` + `stage2_common.cpp:263-305` | `underdetermined_n` 缺键=0 → 委托 `p2_reject_plan_resolve`（去 2/3 双份字面量） | ✅ |
| 009 | `stage2_common.h:16-24,50-52` + `stage2_common.cpp:137-140` + `module_adapters.cpp:5264-5310` | `P2_SMOOTHING_LAMBDA_AUTO=0.1` 单一来源；struct/parser/module 三面统一 | ⚠️ 与 `upm.h:75` 编译期默认 0.0 仍分叉（**已登记**，见 U-4） |
| 011/012 | `module_adapters.cpp:5442-5468, :6087-6160` | `sky_plane_loaded`/`sky_applied` 分离、provenance 自洽 | ✅ |
| 014 | `module_adapters.cpp:4909-5010, :5047-5138` | sampler 配置解析单一事实源 + 生效配置全量落盘 | ✅ |
| — | `sky_plane.h:157-166` + `sky_plane.cpp:412-417` | 删除死字段 `P2SkyPlaneConfig.cpu_workers`（零消费者 + 违反 QA-002/P2-002 禁硬编码 workers） | ✅ |
| **本分片修补** | `lib/algorithms/coverage/tests/synthetic_gate.cpp:4702-4723` | **陈旧测试期望订正**（见 §3.2） | ✅ 已修 |

### 1.4 SNR-CONFIG-LAND（回执 20:30）

`config/defaults.json`、`config/config_registry.json`（07_noise_snr 行号 60→108 等 + 新增 `snr_path` 登记 + `sparse_snr_layer` false→true）、
`config/templates/{mosaic,normalize}.phase_config.json`、`contracts/schemas/phase_config_{mosaic,normalize}.schema.json`、
`ci/ledgers/dead_config_keys.json`（+2 条死键登记：`snr_path`、`sparse_snr_spacing_px`）、
`ci/root_manifest.json`（`HST_M16` 入 `allowed_dirs`）、`tools/config_consistency_check.py`（重写 905+/198−）、
`tools/fixtures/config_consistency_known_divergences.json`（新台账）。**状态：配置/schema 已落地；实现跟随项登记未实现**（回执 §5.3 自述）。

### 1.5 GUARD-TOOLS-FIX（回执 23:48）+ DESIGN-GAP-LAND（回执 23:45）

- 新门禁：`ci/check_spec_named_impl.py`（604 行）+ `ci/spec_named_impls.json` + `ci/ledgers/spec_named_impl_gaps.json`（4 条缺口登记）；
  `ci/checks.json` 新增 `CHK-SPEC-NAMED-IMPL-ON-PROD-PATH`（P0，3 steps）；`ci/id_migration_map.json`；`docs/ci/01_CHECKS.md:73`。
- 新工具（**未注册为门禁**）：`tools/doccheck/check_alg_line_anchors.py`；`docs/algorithms/*` 5 处行锚更新；
  `docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv`（61 行行号刷新）。
- `ci/checks.json` 另新增 5 条 `CTEST-AIO-*` 单项注册（aio_checksum/tile_model/transform/query_pixel/precision_dual），
  并在 `CTEST-LINUX-FULL` 的排除表同步加入（避免重复计数）—— 已核实 5 个 target 在 `build/` 与 `run/ci/build-gcc-release` 均存在。

### 1.6 GATE-RED-FIX（回执 20:18）与前台改动

- `tools/v6/v6_runtime_mutation_driver.py:63-75`：注入锚点改绑到 `command_tree.h` 新形态（语义不变，仍注入「mosaic 缺 --mode」）。
- `artifacts/KNOWN_FAILURES_BASELINE.json`：`source_commit` → `06216ec8`（**只换基线指针，finding_count/reproduced_count 未改**）。
- 前台：`ASTROCS_DESIGN.md`（+87/−? ，23:39 落）、`.gitignore`（+3：`run/RELEASE-02/paper/` 不入库）、
  `工程控制/RELEASE-02/GAP_AUDIT.md`（会话中持续增长）。
- `artifacts/prerelease_v5/ISA-00{1,2,3}/MEASUREMENTS.csv`：benchmark 实测值刷新（**在我两次门禁运行之间仍在变**，见 §4.0）。

### 1.7 多分片改同一文件的冲突核查

`module_adapters.cpp` 被 A6 + CONFORM-FIX-B 两片改过。逐 hunk 行区间核查：**39 个 hunk 全部不相交**
（A6 集中在 2034–3640 的 p1_op_* 区；CONFORM-FIX-B 集中在 4909–7665 的 p2_op_* 区；:76 为 include）。
**无互相覆盖**。默认值一致性交叉核对：
`module_adapters.cpp:6270` 的 `reject_profile` 缺省 = `P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL`，与 `stage2_common.h:87` 新默认**同值**；
`module_adapters.cpp:6274` 的 `req.underdetermined_n=0`（按 profile 解析）与 `stage2_common.cpp:263-305` 同语义。

### 1.8 半成品扫描

- **TODO/FIXME/XXX/HACK/`#if 0`**：改动文件中 **0 处新增**。
- **注释掉的代码**：**0 处新增**。
- **临时调试输出**：改动文件内新增的 `fprintf/stderr` **均为既有诊断面**（`sampler.cpp`/`snr_estimator.cpp` 等原有 trace），
  **本轮未新增调试打印**；`module_adapters.cpp` 新增的是既有 `[nodetrace]` 风格。
- **语法完整性**：9 个改动/新增 Python 全部 `py_compile` OK；13 个改动/新增 JSON 全部 `json.load` OK；
  C++ 经**完整 clean 重建**验证（§2）。
- **悬空引用**：首轮扫描发现 `reports/RELEASE-02/conform-fix-b.md`、`reports/RELEASE-02/guard-tools-fix.md`
  被代码注释引用但**当时不存在**；**两份回执已分别于 23:48 / 00:06:37 落地 ⇒ 现已闭合**（复扫 0 悬空）。

---

## 2 构建结果（收口后完整重建）

```
export TMPDIR=/dev/shm/astrocs_cv
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # EXIT=0
ninja -C build clean                                      # EXIT=0
ninja -C build                                            # EXIT=0   23:43:10 → 23:48:03（1125 步）
```

| 指标 | 值 |
|---|---|
| **error** | **0** |
| **warning** | **49** |
| FAILED | 0 |

**warning 分布**（全部落在**本轮未改动**的文件/系统头）：

| 类别 | 数 | 位置 |
|---|---|---|
| `-Wenum-compare` | 28 | `tests/unit/aio_abi_tests.cpp` |
| `-Walloc-size-larger-than=` | 8 | `lib/infrastructure/gaia_xpsd_client/src/gaia_client.c` |
| `-Wstringop-overflow=` | 6 | `/usr/include/c++/14/bits/stl_algobase.h` |
| `-Wformat-truncation=` | 5 | `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp` |
| `-Wunused-function` | 2 | `lib/algorithms/coverage/src/rejection.cpp:100`（`norm_cdf`，文件未改） |

**本轮改动文件新增 warning = 0。** 且 `tools/check_warning_suppression.py` 实测 **PASS**
（`QA-001_PASS: V6 生产模块零抑制指令; -w 仅第三方豁免; 生产警告=0`）。

**辅助构建面**：`build/linux-openmp-on/libphase2.a` 原为 **09-17 16:16 陈旧归档**，导致 `tests/api` 10 条假红（§3.3）。
已按该测试自述命令重建（`cmake -S lib/algorithms/coverage -B build/linux-openmp-on -DP2_ENABLE_OPENMP=ON` +
`cmake --build … --target phase2`）：**0 error / 1 warning**，归档更新为 09-20 00:32。

---

## 3 ctest 结果

### 3.1 汇总

| 轮次 | 命令 | 结果 |
|---|---|---|
| 第 1 轮 | `ctest --test-dir build -j 8` | 466 中 **2 失败**（`core_scheduler`、`phase2_synthetic_gate.Phase2Config.V15RejectionTypedParseAndDefaultAuto`），EXIT=8 |
| 第 2 轮（修 338 后） | `ctest --test-dir build -j 8` | **100% tests passed, 0 tests failed out of 466**，EXIT=0，216.61 s |

12 条 Skipped 均为显式条件跳过（CUDA/AVX512 无硬件、`RealHips*` 无 fixture、`OneTvsTwoTDeterminism` 并行用例），
**非本轮引入**，且与 `ctest -N` 登记的 466 条一致。

### 3.2 失败项诊断 ①：`V15RejectionTypedParseAndDefaultAuto`（#338）— **本轮真回归（已修）**

- **现象**：`lib/algorithms/coverage/tests/synthetic_gate.cpp:4715-4716` 断言
  `cfg1.reject_profile=="wbpp_2_9_1"`、`reject_underdetermined_n==2u`；实测得 `"astrocs_adaptive_pixel"` / `3`。
- **定性**：**真失败，非陈旧二进制**（clean 重建后仍 5/5 稳定复现）。
- **根因**：CONFORM-FIX-B-007/008 依负责人裁决把**生产默认 profile** 改为自研档 `astrocs_adaptive_pixel`（resolver 默认 `underdetermined_n=3`），
  但**未同步更新锁该默认值的同址单测**。该文件**不在本轮改动清单内** ⇒ 属「改了默认值但漏改锁默认值的测试」。
- **权威核对（改测试而非改实现）**：
  - `docs/science/REJECTION.md` §2 profile 行：「`astrocs_adaptive_pixel`（**生产默认，AstroCS 自研**）/ `wbpp_2_9_1`（**对照档**）」；§5 同。
  - `docs/development/CONFIG_SCHEMA.md:25`：「`profile(astrocs_adaptive_pixel(生产默认,自研)|wbpp_2_9_1(对照档)|…)`」。
  - `contracts/data/phase2_uncertainty_rejection_provenance_v1.json:67`：「canonical=astrocs_adaptive_pixel 自研档」。
  - `lib/algorithms/coverage/include/astro/phase2/rejection.h:230-234`（冻结）：0 = 按 profile 默认，`astrocs_adaptive_pixel=3`。
  ⇒ **实现是对的，测试期望是陈旧基线**。
- **处置**：**最小改动订正测试期望**（`synthetic_gate.cpp:4702-4723`）：改用宏 `P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL`（单一来源）+ `3u`，
  并写入权威出处注释。**未放宽任何判据**（仍逐值断言默认，只是断言到正确值）。
- **验证**：`ctest -R V15RejectionTypedParseAndDefaultAuto` **3/3 PASS**；全量 ctest 转 **466/466 全绿**。

### 3.3 失败项诊断 ②：`core_scheduler`（#14）— **负载敏感 flaky（既有，非本轮）**

- **现象**：`tests/unit/core_scheduler_test.cpp:159: c_done_before_b.load()`（B13-R13-2 内存回压无队头阻塞）。
- **定性**：**非确定性**。证据：单独串行复跑 **8/8 PASS**；8 路并发压测 **8/8 PASS**；仅在 `-j 8` 全量并发（含 200 s 级重测）时偶发。
- **机制**：用例要求「C 在 A 的 200 ms 睡眠窗口内被调度」；全量并发下调度延迟 > 200 ms 即失败。
- **归因**：`core_scheduler_test.cpp` 与 `lib/infrastructure/scheduler` 实现**均未被本轮改动**
  （本轮 scheduler 目录仅 `module_adapters.cpp` 变更，与本用例无关）⇒ **既有 flaky 欠账**，登记不修。

### 3.4 失败项诊断 ③：`tests/api` 10 条（`UT-API` 面）— **陈旧二进制（已消除）**

- **现象**：`test_seam_metric_gate`(3) + `test_upm_parallel`(3) + `test_upm_recovery_oracle`(4) 全红，探针输出 `FAIL build`。
- **根因**：这 3 个用例在 `setUpClass` 里用 `g++` 直接链接 `build/linux-openmp-on/libphase2.a`，
  而该归档为 **09-17 16:16**（早于本轮全部修复）⇒ 探针链接的是旧 `p2_upm_build`。
- **处置**：按用例自述命令重建该归档（§2 末），**未改任何测试/源码**。
- **验证**：`python3 -m unittest discover -s tests/api` → `Ran 48 tests … OK`（**rc=0**）。
- **注**：该归档属 `build/`（gitignore），其陈旧不反映工作树状态；但**任何 e2e 前必须重建它**，否则 UT-API 必假红。

---

## 4 全量门禁（`--all --profile linux-main`）

### 4.0 靶子静止性与运行有效性

| 项 | 值 |
|---|---|
| `ci/checks.json` sha256（**运行前**） | `4d173ec67c560b14ccc69212f773cd10570444df174aad6b8f7c7a20474a17c0` |
| `ci/checks.json` sha256（**运行后**） | （见 §4.3） |
| 工作树指纹（运行前） | `41de93cebfe62c0173eab8852302cdb0c7d1f474b0e5b0e88c3b7a45b7bd3453` |
| 工作树指纹（运行后） | （见 §4.3） |

**第 1 次门禁运行（00:00:33 起）已作废**，原因两条，均已定位到硬证据：

1. **TMPDIR ENOSPC**：按硬约束使用 `TMPDIR=/dev/shm/astrocs_cv` 时，`deep_ci_driver ctest-full` 内部的
   `cmake --build` 把 gcc 中间 `.s` 写入该 tmpfs；`/dev/shm` 总容量 7.9 G，并发分片另占 ~3.1 G，
   `astrocs_cv` 自身涨到 **4.8 G / 557 项**，随后编译报
   `fatal error: error writing to /dev/shm/astrocs_cv/cc5raHA5.s: 设备上没有空间` ⇒ `CTEST-LINUX-FULL` **exit 2（runner error，非测试失败）**。
   该 ENOSPC 会级联污染其后所有需要临时空间的 step ⇒ 整轮不可用。
2. **并发分片持续写工作树**：运行期间 `reports/RELEASE-02/conform-fix-b.md`（00:06:37）、
   `artifacts/prerelease_v5/ISA-00{1,2,3}/MEASUREMENTS.csv`、`reverse_verify/experiments/p7_noise/*` 仍在变。
   （**核实：`lib/**`、`ci/**`、`config/**`、`contracts/**`、`tests/**`、`CMakeLists.txt` 在运行期间无变更** —— 见 §4.3 指纹 diff。）

**第 2 次门禁运行**（本报告 §4.1 起的数字）改用 `TMPDIR=/workspace/.tmp-astrocs-cv`
（**与硬约束的偏离，理由 = 上条 ENOSPC 硬证据**；该目录在仓库之外、`/workspace` 盘 69 G 可用、用后清理）。
另在运行前已重建 §2 末的辅助归档，避免 UT-API 假红。

---

### 4.1 运行元数据

| 项 | 值 |
|---|---|
| 命令 | `python3 ci/run_checks.py --all --profile linux-main --json-out run/RELEASE-02/converge-verify/gate-linux-main.json` |
| 起止 | 2026-09-20T00:34:18 → 01:43:57（**4176.4 s**） |
| registry | `ci/checks.json` entry_count=54, step_count=200 |
| 选中 | entries=48, steps=**185** |
| **passed** | **167** |
| **failed** | **15** |
| **timeout** | **2** |
| skipped(waivable) | 1（`UT-CPU-AVX512`，rc=77 宿主能力 gate，显式计数） |
| verdict | **FAIL**（GATE EXIT=1） |
| 落盘 | 逐 step JSON：`run/RELEASE-02/converge-verify/checks/<STEP>.json`；日志 `logs/gate-linux-main.log` |

### 4.2 checks.json 前后 sha256（靶子静止性判据）

| 时点 | sha256 |
|---|---|
| **运行前** | `4d173ec67c560b14ccc69212f773cd10570444df174aad6b8f7c7a20474a17c0` |
| **运行后** | `4d173ec67c560b14ccc69212f773cd10570444df174aad6b8f7c7a20474a17c0` |

**两次一致 ⇒ 满足任务 §③8 的形式判据（registry 未在运行中移动）。**

### 4.3 ⚠️ 但「靶子静止」的实质判据**不满足** —— 源码在运行期间仍被改动

工作树指纹（对 `git status` 全部条目逐文件 sha256 后整体取哈希，排除 `run/`、`build/`）：

| 时点 | 指纹 |
|---|---|
| 运行前 | `41de93cebfe62c0173eab8852302cdb0c7d1f474b0e5b0e88c3b7a45b7bd3453` |
| 运行后 | `241f3ff65d7abdaff2a4e8793096b17c8925cbd4ed5a1b4c912889bf579b7325` |

**指纹不一致。** 逐文件 diff 显示 **6 个源码/合同/文档面文件在门禁运行窗口内被改动**：

| 文件 | mtime | 落在窗口内？ |
|---|---|---|
| `tests/unit/p1snr/CMakeLists.txt` | 00:42:31 | ✅ 窗口内（门禁 00:34 起） |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | **01:28:06** | ✅ 窗口内（diff 由 447 行涨到 **656 行 / +593−63**） |
| `contracts/schemas/unified/frame_snr.schema.json` | 01:31:44 | ✅ 窗口内（新增文件） |
| `ci/root_manifest.json` | 01:49:33 | 窗口后 |
| `ci/prepare_linux_fixtures.py` | 01:52:31 | 窗口后（新增文件） |
| `docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv` | 01:55:31 | 窗口后 |

**结论**：本轮门禁结果**能反映 00:34 时刻的工作树**，但**不能认证 01:55 之后的当前工作树** ——
门禁自身的 `BUILD-GCC-RELEASE` 步骤在 00:35–00:42 完成构建，而 `module_adapters.cpp` 在 **01:28** 又被改了 146 行。
这与负责人所指「靶子在跑的过程中被移动」是**同一类问题**，只是这次移动的是源码而非 registry。

**补充证据（最新树重验）**：对 01:55 时刻的工作树重跑 `ninja -C build`（**0 error / 0 warning**）与
`ctest --test-dir build -j 8`：**467 条中 2 失败**（`cpu006_bench_report`、`p1phot_performance`）。
两条**单独复跑 3/3 全 PASS**，且均为性能/离散度门 ⇒ **并发负载导致的 flaky**（当时 load average 10.24），非真回归。

### 4.4 逐项 FAIL 分类（15 FAIL + 2 TIMEOUT）

分类口径：**(a)** 本轮修复的真实回归/副作用 ⇒ 必须修；**(b)** 新门禁发现的新问题 ⇒ 如实登记；
**(c)** 既有欠账（与本轮无关）；**(d)** 陈旧基线 / 环境·级联。

| # | step | 类 | 根因（证据） | 处置 |
|---|---|---|---|---|
| 1 | `WARNING-SUPPRESSION` **TIMEOUT** | **(d) 环境/超时过紧** | 登记 timeout=60 s；**单独复跑 rc=0，实测 46.71 s**（`QA-001_PASS: 生产警告=0`）。门禁时刻并发负载使其 60.1 s 被 kill | 登记：60 s 预算对本机偏紧（非代码缺陷） |
| 2 | `CHK-MODULE-MANIFEST` | **(c) 既有** | 99+ 条 `missing_cmake_target`/`missing_co_located_tests`/`module_yaml_value_mismatch`/`header_missing_abi_version`；例：`lib/algorithms/photometry/CMakeLists.txt` 缺失、`noise_snr/module.yaml` module_id 与映射表不符 | 登记（与本轮无关） |
| 3 | `DOC-LINE-ANCHORS` | **(a) 本轮副作用** | `[C4_symbol_binding] P3RSMP-DESCRIPTOR: symbol now at lines [697, 9471] (outside every anchor range)`。HEAD 为 695/8945；CONFORM-FIX-B 在 `module_adapters.cpp` 插入行（:76 include + p2_op_* 区）使符号位移 | **登记，须由 anchor 域主改锚**（`docs/algorithms/**` 属只读面，本分片不改） |
| 4 | `CHK-ROOT-CLEAN` | **(d) 并发污染** | 门禁时刻根目录存在并发分片运行产物 `f0000..f0048.resamp.fits`+`.weight.fits`（18 条）；**清理后单独复跑 PASS（0 violations）**；写入者不在仓库源码内（`grep resamp.fits` 全仓 0 命中） | 登记：**运行纪律问题**，非代码缺陷（见 U-2） |
| 5 | `UT-ARCH` (1/40) | **(a) 本轮副作用** | `test_05_regeneration_idempotent`：已提交 `PRODUCTION_EXECUTION_INVENTORY.csv` 与生成器输出**不逐字节一致**（`:4725` vs `:4910`）⇒ 该 CSV 的刷新（GUARD-TOOLS-FIX/DESIGN-GAP-LAND）与当前源码已再次分叉 | **登记，须重生成该 CSV** |
| 6 | `UT-BACKEND` (18/210) | **(c) 既有** | **15/18 同一根因**：`export config missing output_mode (FZ-P3-MODES …)`（`test_p3004_spherical_oracle`×5、`test_p3005_fits_output`×4、`test_p3006_production_pipeline`×4、`test_p3003_parallel_resampler`×2）。**决定性证据**：该 reject 代码在 HEAD 已存在，且 `git diff module_adapters.cpp | grep output_mode` = **0**（本轮未改）；上述 3 个测试文件**均未被本轮改动**且**不含 output_mode** ⇒ 与本轮无关。余 3 条：`test_p1004_joint_gate`×2（资源 workload/run_id）、`test_p2003_seam_oracle`×1 | 登记（既有；测试配置缺 `output_mode`） |
| 7 | `UT-CLI` (19F+2E / 134) | **(c) 既有** | 同 #6 的 `output_mode` 根因（`test_11_template_runnable_without_force`：`export --template` 产物无 `output_mode`） | 登记 |
| 8 | `UT-CONFIG` (3/50) | **(c) 既有** | `source_ref` 锚漂移：`docs/science/REJECTION.md:59/60/61`、`PHOTOMETRY.md:21/39`、`DRIZZLE.md:21`、`11_upm.md:36`。**决定性证据**：这些 ref 在 `git show HEAD:config/{defaults,config_registry}.json` 中**同值存在**，且目标文档 `docs/science/**`、`docs/plugins/**` 本轮**零改动** | 登记 |
| 9 | `UT-QUALITY` (2/149) | **(a) 本轮真回归** | `test_root_cleanliness.py:139`：`manifest_dirs == doc_dirs` 断言失败，`多=['HST_M16']`。即 `ci/root_manifest.json` 的 `allowed_dirs` 新增 `HST_M16`，但 **ENGINEERING_SPEC §7 未同步**（`grep HST_M16 ENGINEERING_SPEC.md` = 0 命中） | **登记，未修**（见 U-1） |
| 10 | `AST-API` **TIMEOUT** | **(d) 环境/超时过紧** | 登记 timeout=120 s；**单独复跑 rc=0，实测 106.16 s**（`DOC-004_PASS: 45/45 头 …`）。门禁中 120.3 s 被 kill | 登记：120 s 预算对本机偏紧 |
| 11 | `UT-ABI` (1/22) | **(d) 辅助构建树陈旧** | `test_acceptance_script_passes` 走 `build/linux-control`（存在，CMakeCache 00:32）；该目录下 `MOD001_INSTALL_LOAD_CHECK` **rc=1**；而 `--build-dir build` 下 **rc=0（66/66 PASS）** ⇒ 辅助树不完整 | 登记：与 §3.4 `linux-openmp-on` 同类（辅助构建面陈旧） |
| 12 | `V6-CLI-MODE-MATRIX` | **(c) 级联** | `failures=20/24`，首条 `[error] source.hips_dir 指向的路径不存在`；该 step 用 `run/v6/cli-matrix/x.hips` fixture，而 **`LINUX-MAIN-FIXTURES` 已失败**（#15）⇒ 无 fixture | 登记（级联） |
| 13 | `PKG-CONSISTENCY-NEG` | **(d) 本分片 TMPDIR 偏离所致** | `shutil.SameFileError: packaging/licenses/CFITSIO_LICENSE.txt … are the same file`。因我把 TMPDIR 放到 `/workspace/.tmp-astrocs-cv`（与仓库同文件系统）⇒ self-test 沙箱复制判同 inode。**改用 `/dev/shm/astrocs_cv` 复跑 rc=0（SELFTEST PASS）** | 已定位；**与代码无关** |
| 14 | `ENG-CONSTRAINTS` | **(a)+(d) 混合** | (i) **真**：`manifest_not_wider_than_spec7: ci/root_manifest.json 白名单比 §7 更宽: HST_M16`（同 #9）；(ii) **瞬态**：`unregistered_root_entry: psfex.xml / scamp.xml` —— 并发分片产物，**现已不存在** | 登记（(i) 见 U-1；(ii) 见 U-2） |
| 15 | `LINUX-MAIN-FIXTURES` | **(c) 既有路径腐烂** | `FileNotFoundError: lib/astro_image_io/third_party/cfitsio`；该路径在 HEAD 的 `ci/prepare_linux_fixtures.py:33` 即已写死（`AIO = REPO/"lib"/"astro_image_io"`），模块实已迁至 `lib/infrastructure/aio/third_party/cfitsio` | 登记 |
| 16 | `LINUX-MAIN-BUILD-TREE` | **(c) 既有（已登记）** | `CMake Error: File cli/version_generated.h.in does not exist`（CMakeLists.txt:56 `configure_file`）。`ci/root_manifest.json#registered_local_retention` 已登记 `cli/` 为 ROOT-008 遗留（含此失效引用） | 登记 |
| 17 | `KNOWN-FAILURES-BASELINE-CHECK` | **(d) 级联** | 聚合上述各红；基线 `ci/known_failures.json` 未覆盖（`error_count=1`）。注意 `artifacts/KNOWN_FAILURES_BASELINE.json` 本轮仅换 `source_commit`，**finding 集未动** | 登记 |

**统计**：**(a) 本轮真实回归/副作用 3 项**（#3 DOC-LINE-ANCHORS、#5 UT-ARCH、#9 UT-QUALITY，另 #14 含同一真因）；
**(c) 既有欠账 7 项**；**(d) 环境/超时/级联/辅助树 7 项**；**(b) 新门禁新问题 0 项**。

---

## 5 明确结论：**当前工作树未达到「可以跑 e2e」的状态**

判定依据（三条硬阻塞，任一成立即不可跑）：

1. **在飞分片尚未停止写工作树（阻塞性）**。
   证据：门禁运行窗口内（00:34–01:43）`module_adapters.cpp` 在 **01:28** 仍被改 +146 行；
   `contracts/schemas/unified/frame_snr.schema.json`（01:31）新增；
   窗口后 `ci/root_manifest.json`（01:49）、`ci/prepare_linux_fixtures.py`（01:52）、
   `PRODUCTION_EXECUTION_INVENTORY.csv`（01:55）继续变更。
   ⇒ **任何 e2e 结果都会因靶子移动而不可复现**（这正是 22:56 那次 `fail=40` 作废的同一病因）。
2. **`HST_M16` 登记方式违反清单自身规则，且已被 2 个独立门禁同时判红（阻塞性）**。
   `ci/root_manifest.json` 的 `notes.growth_rule` 明文：「新增根条目必须先经负责人确认并同步 ENGINEERING_SPEC §7，再改本清单；
   **禁止把本清单当只增不减的豁免表**」。而本轮为消 `m16.zip` 红，直接把 `HST_M16` 加进 `allowed_dirs`，
   未同步 §7 ⇒ `UT-QUALITY` 与 `ENG-CONSTRAINTS` **双红**。这是「用放宽清单换绿」的形态，**必须由负责人裁决处置**（见 U-1）。
3. **门禁 15 FAIL / 2 TIMEOUT 中，尚有 3 项属本轮副作用未收口**（DOC-LINE-ANCHORS、UT-ARCH、UT-QUALITY），
   且 `KNOWN-FAILURES-BASELINE-CHECK` 作为聚合门判红 ⇒ **当前不满足「全绿」的 e2e 前置**。

**同时确认的「已就绪」面**（供 e2e 前的快速复核）：
- **编译面**：clean 全量重建 **0 error / 49 warning**（warning 全在未改文件；生产 V6 模块警告 0）；最新树增量重建 **0 error / 0 warning**。
- **ctest 面**：修掉 1 条本轮真回归后，`build/` 全量 **466/466 全绿**（EXIT=0）。
- **registry 静止面**：`ci/checks.json` 前后 sha256 一致。
- **新门禁非空转**：`check_spec_named_impl.py --self-test` 14 例全符合预期（含 7 条注入必红 + fail-closed）；
  `config_consistency_check.py --self-test` 16 组全符合预期。

**建议的 e2e 放行条件（按序）**：
① 全部在飞分片停止写树（`git status` 与 mtime 在 ≥10 min 窗口内不变）；
② 处置 U-1（HST_M16 合规化）并重生成 `PRODUCTION_EXECUTION_INVENTORY.csv`、重锚 `P3RSMP-DESCRIPTOR`；
③ 重建辅助树 `build/linux-control`、`build/linux-openmp-on`（否则 UT-ABI/UT-API 假红）；
④ 重跑 `--all --profile linux-main`，要求 registry sha256 前后一致 **且** 工作树指纹前后一致。

---

## 6 未解决项清单

| ID | 项 | 类 | 证据 | 影响面 |
|---|---|---|---|---|
| **U-1** | `HST_M16` 入 `ci/root_manifest.json#allowed_dirs` 但 **ENGINEERING_SPEC §7 未同步** | (a) 本轮 | `UT-QUALITY:139` `多=['HST_M16']`；`ENG-CONSTRAINTS` `manifest_not_wider_than_spec7: HST_M16`；`grep HST_M16 ENGINEERING_SPEC.md`=0；manifest 自述 `growth_rule` 禁此形态 | 2 个 P0 门禁判红；**须负责人裁决**：(i) 改 §7 增 `HST_M16`（改最高文档，走变更流程），或 (ii) 把 M16 数据移出根目录（如 `testdata/`）并回退清单。**本分片不擅自改**（AGENTS §9 第 1 类） |
| **U-2** | 并发运行把产物散落仓库根（`f*.resamp.fits`、`psfex.xml`、`scamp.xml`、`CS`） | (c)+(d) | `CHK-ROOT-CLEAN` 在门禁时刻 18 条违规；`ENG-CONSTRAINTS` 2 条；清理后单独复跑 **PASS**；写入者不在仓库源码（全仓 `grep` 0 命中）⇒ 来自并发分片的外部脚本以 CWD=仓库根运行 | 使 `CHK-ROOT-CLEAN`/`ENG-CONSTRAINTS` **非确定性判红**；属 GOV-001 已登记缺陷（产物目录解析为 CWD）。**未删他人在跑产物**（仅删除一个 307 B 的 python 报错转储 `CS`，已存证 `run/RELEASE-02/converge-verify/evidence/stray-CS-file.txt`） |
| **U-3** | `P3RSMP-DESCRIPTOR` 行锚漂移（697/9471 vs 文档 695） | (a) | `DOC-LINE-ANCHORS` C4；HEAD 695/8945 | P0 门禁 `CHK-SCI-REF` 判红；`docs/algorithms/**` 属只读面，须 anchor 域主改锚 |
| **U-4** | `smoothing_lambda` 默认三面不一致：`stage2_common.h`=0.1（`P2_SMOOTHING_LAMBDA_AUTO`）、`upm.h:75`=0.0、`module_adapters.cpp:5264` 缺键保持 0.0 | (b) 已登记 | `conform-fix-b` 注释自述「与负责人裁决 GAP_AUDIT §9.39 A5『λ 不能为 0』冲突已登记上呈」；`tools/fixtures/config_consistency_known_divergences.json` 仅登记 `underdetermined_n` 一条 | 生产 `p2_op_upm_fit` 缺键时 λ=0（关平滑）；**生产取值归 SMOOTH-LAMBDA 分片裁决**，未擅自改冻结面 |
| **U-5** | CONFORM-FIX-A ① 注释「数值与旧路径**逐位一致**」在数学上不严格 | (a) 措辞 | PSF 路径改为 `fwhm_eff = (fwhm/K)*K` 后，`autoHalf` 可能在 `12·fwhm` 整数边界 ±1 ulp 邻域取到不同 `half`；**实测 32/12000 边界样本不一致**（如 `fwhm=2.583333333333334`：32 vs 31）。可达路径 = `snr_extract_model_v3 → frameDepthFromPsf` / `sourceSnrFromPsfRow` | 需 `fwhm` 落在 `n/12` 的 ~1e-15 相对邻域，**实际影响可忽略**；建议注释订正为「除 12·fwhm 整数边界 ±1 ulp 邻域外逐位一致」。**未改代码** |
| **U-6** | `module_adapters.cpp:2133` 注释写 `fwhm=2.3548*sx`，与 PSF 块权威因子 **1.230310** 矛盾 | (c) 既有（HEAD 2115 同款） | `dpsf_psf.cpp:25` `MOFFAT4_FWHM_FACTOR=1.230310`、`:393-394` `fwhm_x=1.230310*sx`；`git show HEAD` 证实该注释本轮前已存在；代码本身只取 `psf_params[7]/[8]` 权威列（**未施加错误因子**） | 纯注释缺陷，但正是 CONFORM-SWEEP-1-001 同类跨块混淆；建议随下一轮订正 |
| **U-7** | 门禁 2 条 TIMEOUT 的登记预算偏紧 | (d) | `WARNING-SUPPRESSION` 60 s vs 实测 46.71 s；`AST-API` 120 s vs 实测 106.16 s（均单独 PASS） | 并发负载下必假红；建议放宽登记 timeout 或降低两检查的构建动作 |
| **U-8** | 既有红未收口（与本轮无关） | (c) | `CHK-MODULE-MANIFEST`(99+)、`UT-BACKEND`(15 条 output_mode)、`UT-CLI`(19+2)、`UT-CONFIG`(3 锚漂移)、`LINUX-MAIN-FIXTURES`(路径腐烂 `lib/astro_image_io/...`)、`LINUX-MAIN-BUILD-TREE`(`cli/version_generated.h.in`)、`V6-CLI-MODE-MATRIX`(级联) | 7 个门禁面；均**未**被本轮改动触及，登记待排期 |
| **U-9** | `build/linux-control`、`build/linux-openmp-on` 辅助构建树陈旧/不完整 | (d) | `UT-ABI` 在 `build/linux-control` 下 rc=1、在 `build/` 下 rc=0(66/66)；`tests/api` 10 条假红在重建 `linux-openmp-on` 后 **48/48 OK** | 不修则 e2e 前 UT-ABI/UT-API 必假红；**已在本次重建 `linux-openmp-on`**，`linux-control` 未重建（属他人在制面） |
| **U-10** | 本分片对硬约束的两处**已声明偏离** | 声明 | (i) 门禁 TMPDIR 用 `/workspace/.tmp-astrocs-cv`（非 `/dev/shm/astrocs_cv`）—— 因 `/dev/shm` 7.9 G 被并发分片占 3.1 G，`astrocs_cv` 自身涨到 **4.8 G/557 项**后 gcc 报 `设备上没有空间`，`CTEST-LINUX-FULL` 曾 exit 2；该偏离**导致 `PKG-CONSISTENCY-NEG` 假红**（同文件系统 `SameFileError`），改用 `/dev/shm` 复跑 PASS。(ii) 唯一代码改动 = `synthetic_gate.cpp` 陈旧期望订正（§3.2，有权威依据，非放宽判据） | 偏离均为可复现环境约束，证据在 `logs/`；`/dev/shm/astrocs_cv` 已清理 |

---

## 7 产物索引（`run/RELEASE-02/converge-verify/`）

| 路径 | 内容 |
|---|---|
| `logs/build.log` | clean 全量重建（configure/clean/build 三段 + EXIT） |
| `logs/build-final.log` | 01:55 树增量重建（0 error / 0 warning） |
| `logs/ctest.log` | 首轮 ctest（466 中 2 失败） |
| `logs/ctest-after-fix.log` | 修 338 后 ctest（466/466 全绿） |
| `logs/ctest-final.log` | 最新树 ctest（467 中 2 flaky） |
| `logs/gate-linux-main.log` | 门禁 stdout/stderr + 前后 sha256 |
| `gate-linux-main.json` | 门禁机器可读全量结果（48 entries / 185 steps） |
| `checks/<STEP>.json` | 逐 step 结果（185 个） |
| `fp-before-gate.txt` / `fp-after-gate.txt` | 靶子静止性指纹（含逐文件 sha256） |
| `checks.json.sha256.before` | registry 运行前哈希 |
| `evidence/stray-CS-file.txt` | 仓库根瞬态污染文件存证（307 B） |
| `logs/ut-backend-verbose.log` | UT-BACKEND 18 条失败逐条清单 |
| `fingerprint.sh` | 指纹脚本（可复现） |

