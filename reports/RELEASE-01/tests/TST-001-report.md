# TST-001 测试审查与补充报告

- 任务：`工程控制/RELEASE-01/tasks/TST-001.md`（测试审查与补充）
- 执行：只读审查 + 少量轻量单测；未执行任何 git 写操作；未修改 `lib/`、`contracts/`、`config/`、`ci/checks.json` 或产品代码
- 输入证据：BLD-001 前台独立复跑（`run/RELEASE-01/logs/BLD-001/build_ctest.log`）
- 权威：`ACCEPTANCE_SPEC.md` §2、`ENGINEERING_SPEC.md` §5/§8、`docs/plugins/**` §8「测试与 Oracle」、`docs/ci/01_CHECKS.md` §1
- 产出日期：本会话；工作目录 `/workspace/Astro CS Database`

---

## 0. 证据基线与审查口径

### 0.1 BLD-001 基线（复核）

| 项 | 值 | 证据 |
|---|---|---|
| ctest 总数 | 442 | `build_ctest.log:946` `100% tests passed, 0 tests failed out of 442` |
| 通过/跳过/失败 | 430 / 12 / 0 | `build_ctest.log:946,965,968-979` |
| 总耗时 | 404.30 s | `build_ctest.log:965` |

### 0.2 关键口径：442 ctest ≠ 全测试面

442 只是 ctest 面。`ci/checks.json` 中：

- `CHK-UNIT` 有 19 个 step，含 `UT-MONITORING`（`unittest discover -s tests/monitoring`，实跑 60 用例）、`UT-QUALITY`、`UT-API`、`UT-RUNTIME` 等 Python unittest 面；
- `CHK-ORACLE` 11 个 step（显式点名 Oracle ctest target）；`CHK-ABI` 14；`CHK-CONTRACT-TEST` 13；`CHK-ISA-EQ` 4。

因此：**「442 用例覆盖」不等于「全部测试覆盖」**；Python 域（monitoring/quality/version/api/backend/cli/runtime…）不在 BLD-001 的 442 内，需单独评估。本报告对两者都给出结论。

### 0.3 审查命令（可复现）

```bash
# 442 用例清单与跳过
ctest --test-dir build -N | grep -oE 'Test +#[0-9]+: [^ ]+' | sed 's/.*: //' > /tmp/ctestlist.txt
grep -nE 'Skipped|tests passed' run/RELEASE-01/logs/BLD-001/build_ctest.log
# 注册表↔文档一致性（当前判红）
python3 ci/check_registry_doc_sync.py; echo "rc=$?"
# 负例入口自查
python3 ci/polarity_probe.py --plan   # 只打印，不写 ledger
# 逐模块测试源/断言抽检（见 §3）
```

---

## 1. 测试缺口矩阵（模块 × 测试类型）

图例：● 有独立用例；○ 部分/间接/仅真实数据门；× 空白。证据为代表性 target/测试名（file:line 见 §3 与附录）。

| 模块 | 合同 | 负例 | Oracle | 不变量 | 边界 | 并行确定性(1vN) | 性能 |
|---|---|---|---|---|---|---|---|
| calibration | ● p1cal_units / v6_p1_cal_covariance / p1cal_bias_influence_gate | ● p1cal_negative（core:397-556，NULL/0维/负中位数/全 NaN） | ● p1cal_oracle.hpp（core:157/180/254 独立复算） | ● p1cal_properties I1/I3/I5 | ● 负例组含 NaN/Inf/空/极端 + FP64 位级往返（core:222-242） | ● I4 1/2/4 线程 bitwise（core:358-389） | ● p1cal_performance（仅 ratio∈[0.4,4.0]，未断 D.7≥1.60） |
| cosmetic | ● p1cos_units / cosmetic_adapter | ● p1cos_negative（core:412-420，ABI/NaN/Inf/max_size≤0） | ● p1cos_oracle.hpp（core:426-481 correct_oracle） | ● p1cos_properties I2-I6 | ● NaN dark/NaN/Inf data + 2×2 小帧（core:357-377/483-498） | ● I4 1/2/4 + worker 1-4（core:212-228/500-523） | ● p1cos_performance（仅 ratio∈[0.25,4]） |
| star_detection | ● p1star_units / p1_stars | ● p1star_negative（core:237-273 + OOM 注入 :607-683） | ● p1star_oracle.hpp（core:413-512，0.05px/1e-3） | ● p1star_properties（mag 全序/NaN 末尾/maxStars 保最亮） | ● NaN 注入场 + u16 量化 ≤0.5px（core:328-347/383-401） | ● F3 OMP 1/2/4 bitwise（core:289-311） | ● p1star_performance |
| psf | ● p1psf_units / p1psf_center_contract_gate / v6_p1_psfw_* | ● p1psf_negative N1-N8（:625-978）/ _centroid_gate_neg / _center_contract_gate_neg / v6_p1_psfw_negative | ● p1psf_oracle O1-O2（:533-618）/ p1psf_center_contract_astropy / v6_p1_psfw_oracle | ● p1psf_properties P1/P2 / v6_p1_psfw_gates | ● p1psf_boundary（四角/满量程/stride :984-1093） | ● P3 1/2/4/8 worker bitwise（:365-405） | ● p1psf_performance（**唯一**断 E.7≥1.60） |
| platesolve | ● p1wcs_units / ipv_abi_layout_lock / ipv_params_abi_failclosed | ● p1wcs_negative / ipv_extract_wcs_sip_failclosed / ipv_dead_params_lock | ○ p1wcs_oracle / p1wcs_astropy_cross；**ipv 独立 Oracle 仅在未注册的 test_kvector.cpp/test_synthetic.cpp** | ● p1wcs_properties / p1wcs_closure_metric_gate / ipv_mag_iter | ● ipv_triangle_budget 越界（n≥66 :63-79） | × | ● p1wcs_performance；ipv 性能 × |
| photometry | ● p1phot_units / p1phot_fixgates | ● p1phot_negative N1-N9 | ● p1phot_oracle O1-O5 + units U6/U7/U10 | ● p1phot_properties P1-P6 | ● N6 NaN/Inf flux、N7 像素 NaN/Inf、N1/N2 空指针 | ● P1 线程 1/4/max bitwise | ● p1phot_performance（仅 parity<4.0/trend≥0.25） |
| noise_snr | ● p1noise_units / p1snr_science_units / p1snr_linux_units / p1noise_abi_layout | ● p1noise_negative（:529-818）/ p1snr_science_negative / p1snr_linux_negative | ● p1noise_oracle / p1snr_science_oracle / p1noise_numpy_oracle / p1snr_linux_oracle | ● p1noise_properties I1-I6 / p1snr_frame_parity / p1noise_scale_law | ● n4 NaN/Inf/饱和过滤、n3 全掩膜、n1 NULL（:563-614） | ×（I4 仅同输入双跑 bitwise :268-277，非 1vN；生产单线程） | ● p1noise_performance（仅双跑漂移∈[0.25,4]） |
| drizzle | ● p1drz_units / v6_p1_drz_units / drizzle_adapter | ● p1drz_negative（:459-592）/ v6_p1_drz_negative | ● p1drz_oracle（:381-452）/ v6_p1_drz_oracle | ● p1drz_properties α² 缩放律 / p1drz_taskset_invariance / v6_p1_drz_geometry | ● NaN/零尺寸/非法 nside/pixfrac（:463-590） | ● 1/2/4 + 1..16 逐位（:193-325） | ● p1drz_performance |
| coverage | ● v6_p2_samp case_covsupport:55 | ● case_covsupport_negative:104 / Phase2Coverage.FilterMismatchRejected（**恒跳**） | ×（无合成重叠几何解析 Oracle；RealHipsUnion 恒跳） | ● case_weight_gate:135 | ○ case_boundary_determinism:223 | ● SyntheticFixtureBitwiseDeterminism:162 | × |
| sampling | ● case_covsupport:55 / case_spatial:291 | ● case_spatial_negative:354 | ○ case_spatial 内 long-double 独立重排:48；声明 Oracle `run/v6/p2-samp/oracle/check_spec.py` **未跟踪** | ● case_summary:405 / case_scalar_gate:481 | ● case_boundary_determinism:223 | ● :162 位精确 | × |
| upm | ● v6_p2_upm run_structure:367 | ● run_negative:480 / UpmPersistInvalidModelRejected:5567 / UpmUnknownFrameRejected:5639 | ● run_units:235 + 受控独立 NumPy Oracle（tracked） | ● FrameOrderInvariance:2663 / SparseEqualsDense:2699 / UpmPersistInsertionOrderIndependent:5410 | ● G4BoundaryAndPerturbation:1131 | ● Phase2UpmParallel.OneTvsTwoTDetermine:279 | ○ G4DenseMillionSampleSpatial:2565（无计时） |
| rejection | ● p2_rej_v6 mode_units:918 | ● mode_negative:921 / V16InvalidConfigurationCombos:4752 / V17InvalidMethodStatus:4972 | ● mode_oracle:919 + tracked oracle_rej.py / G6EsdNistRosner54:2988 | ● G6PermutationInvariance:3072 / V15ExPermutationInvarianceTyped:4652 | ● V15SatelliteN2Underdetermined:4450 / V16MinMaxFixedCountExact:4525 | ○ tests/api/test_reject_parallel.py:104（不在 442） | × |
| integration | ● v6_p2_integrate run_routing:208 / p2_output_semantics:34 | ● run_negative:410 / V17NonFiniteWeightInvalid:4880 | ● tests/integration/v6_p2/oracle/v6_p2_oracle.py（tracked） | ● V17StatusesExplicit:4940 / p2_output_semantics:85 | ● p2_output_semantics:109 全零权 | ×（无专属 1vN） | × |
| projection | ● p3_projection_units / v6_p3_proj_units | ● v6_p3_proj_fault_{const_omega,legacy_car,legacy_ait,swap_norm,naive_wrap} / test_domain_limits:471 | ● p3_proj_wcs_oracle.py:138（astropy 绝对对拍，缺 astropy fail-closed） | ● test_normalisation:364 / test_crval2_rotation:430 | ● test_domain_limits:471 / test_plan:536 | ● test_determinism:607（1/2/4/8 worker memcmp bitwise） | × |
| resample | ● p3_rsmp_gate_test:61 | ○ p3_rsmp_gate_test:67-352 12 门；**mutation driver OFF 且路径失效**（§4.3） | ● p3_rsmp_oracle_test:33/61/99 | ● p3_rsmp_core_test:35/71/87/102/161/249/326 | ● test_p3_resample.py:149 NaN / test_p3004:174 ra_wrap | ○ test_p3003_parallel_resampler.py:77（不在 442） | × |
| fits_output | ● p3_output_test:26-148 / v6_p3_export_positive（16 注入对拍） | ● v6_p3_export_negative（16 条注入，逐条必红）/ p3_output_test:103 CRPIX 篡改 | ● p3_output_test:136 WCS 回环 / p3_v6_export_oracle.py | ● p3_output_test:93 无 .tmp 残留 / test_p3_output.py:229 fsync→rename 序 | ○ NaN 掩膜；**>2GiB/长路径 ×** | ×（doc §8 要求 block/cache/worker 一致） | × |
| aio | ● v6_aio_units / v6_aio_fits / aio_abi_units | ● v6_aio_oracle_negative / p1_io_hardening / aio_abi_negative | ● v6_aio_oracle | ● v6_aio_atomic / v6_aio_provenance | ● v6_aio_impl_mutations / phase2_async_io 边界 | ● p1hips_tests_properties:187（1/2/4 worker tree_digest 位级） | ● p1hips_tests_perf:141 |
| cli | ● cli_process / orchestrator_cli_* | ○ tests/cli 旧命令 rc=2（不在 442） | ○ test_phase1_inprocess.py:151（不在 442） | ● v6_runtime_mode_routing / cli_wideconv | ○ | × | ○ cli_bench_verdict |
| runtime | ● v6_runtime_contract_units / rt001_abi / rt005_registry / rt007_artifact_store | ● v6_runtime_contract_negative / v6_runtime_determinism_negative / v6_runtime_resource_record_negative / rt005_registry:84 | ○ v6_runtime_determinism_test:123 负向 order-dependent（独立 harness） | ● rt002_budget / rt003_context / v6_runtime_budget_single_source | ● v6_runtime_so05_policy / rt006_scheduler:53 | ● v6_runtime_determinism_positive/negative（1/2/4/8/16 worker 逐字节） | ×（编排连续性指标无测试） |
| observability | ● mon001_recorder / mon002_gate / v6_runtime_metric_fields | ○ test_frozen_gate.py:177（不在 442） | ● test_frozen_gate.py:318（constants==contracts/resource_gate_v1.json） | ● mon004_enforcement / test_log_contract seq 无空洞 | ● test_frozen_gate.py:105 10.0s 严格界 | ● test_log_contract 4 线程 seq / test_frozen_gate:218 bitwise | ● mon002_alloc / resource_monitor_quality |
| benchmark | ● cpu003_profile_v2 / cpu007_profile_store / cpu006_bench_report | ● cpu006_bench_report / cpu007（负例） | ● tests/cpu/*/run_provider_*_checks.py（rel≤2e-4） | ● cpu_bench winner 仅取 Oracle-OK | ● cpu006:100 单核 / :145 空 kernels | ● avx2 provider 1 vs 4 budget 逐位 | ○ test_isa_variants.py:74 |
| gaia_xpsd | ● gaia_cat_unit / gaia_cat_adapter / gaia_module_manifest_bounds / xpsd_spectrum_count_bounds | ● gaia_cat_negative / gaia_magnitude_range_bounds | ● gaia_cat_oracle / tests/oracle/gaia_oracle.py | ● gaia_cat_properties:295/559 | ● gaia_cat_truncate / :470 退化半径 | ● gaia_cat_test.c:790（1 vs 8 worker 位级） | ● gaia_cat_performance:921 |
| hips_browser | ×（无 C ABI 合同测试） | ○ 裸 assert，Release 下 NDEBUG 抹除 | ○ test_hips_browser_backend.cpp:62（不在构建面） | ○ test_geometry_truth.cpp（未接线） | ○ | × | × |

### 1.1 矩阵结论（按类型汇总缺口数）

| 测试类型 | 存在空白的模块数 | 主要空白模块 |
|---|---|---|
| 合同 | 1 | hips_browser |
| 负例 | 0（均有，但覆盖强度差异大） | — |
| Oracle | 1（+4 部分） | coverage（×）；platesolve（○，独立 Oracle 仅未注册文件）、sampling（○，Oracle 未跟踪）、hips_browser（○）、cli（○） |
| 不变量 | 1 | hips_browser |
| 边界 | 1（+2 部分） | fits_output（>2GiB/长路径 ×）；coverage/cli/hips_browser（○） |
| 并行确定性(1vN) | 6（+3 部分） | platesolve、noise_snr、integration、fits_output、cli、hips_browser（×）；rejection/resample（○ 仅在 442 外的 tests/api、tests/backend） |
| 性能 | 11 | coverage、sampling、rejection、integration、projection、resample、fits_output、cli、runtime（编排指标）、platesolve、hips_browser |

> 说明：
> 1. Phase1 八模块的 1vN 并行一致性**均有**（calibration I4 1/2/4、cosmetic 1-4、star F3 1/2/4、psf P3 1/2/4/8、photometry 1/4/max、drizzle 1..16，均 bitwise）；noise_snr 的 I4 实为同输入双跑而非 1vN。
> 2. **ISA(baseline/AVX2/AVX-512) 等价在全部算法模块级空白**（Phase1 八模块 + projection/resample/fits_output/runtime 均无模块级 ISA 等价；仅 `CHK-ISA-EQ` 的 cpu001/cpu004 provider 面与 `tests/backend/test_isa_*.py`）。
> 3. **双平台允许误差合同**测试在全部算法模块级空白（ENGINEERING_SPEC §5.1 必备项）。
> 4. Phase1 八模块均**无 GTEST_SKIP**（无跳过充数）。

---

## 2. 12 个跳过用例判定表

| # | 测试名 | 跳过原因（file:line） | 判定 |
|---|---|---|---|
| 57 | `cpu001_selftest_avx512` | 宿主无 AVX512F 时 provider 未激活，`return 77`；`tests/unit/cpu001_provider_selftest.cpp:55-60` + `tests/unit/CMakeLists.txt:423-425` | **合理平台跳过**。本机 `/proc/cpuinfo` 无 `avx512`（仅有 avx2/fma），门为硬件事实，非充数 |
| 309 | `Phase2Acr.CudaEquivalent` | `!bridge::api().loaded()` → `GTEST_SKIP`；`lib/algorithms/coverage/tests/synthetic_gate.cpp:3328` | **合理平台跳过**（ACR dormant、GPU 生产不可达） |
| 310 | `Phase2Acr.CudaWeightedSupportEquivalent` | 同上；`:3373` | 合理平台跳过 |
| 311 | `Phase2Acr.G9CompactFrameSubset` | 同上；`:3451` | **合理但留缺口**：该用例的 CPU 侧「非连续帧子集 SNR 映射」断言随 CUDA 一起被跳过，Linux 无 CPU-only 等价用例（§4.4） |
| 312 | `Phase2Acr.G9WinsorizedCpuRoute` | 同上；`:3509` | **合理但留缺口**：名称为「CPU_ROUTE」，却以 CUDA bridge 加载为前提，Linux 上「Winsorized 必须走 CPU_ROUTE」契约零执行 |
| 317 | `Phase2Coverage.RealHipsUnion` | 硬编码 `F:/Astro dev/.../phase1_freeze/T2_v3.hips/signal/properties` 不存在；`:3583-3586` | **不合理（硬编码 Windows 绝对路径）**：Linux 恒跳，coverage union 在 Linux 无合成等价 → 零执行 |
| 318 | `Phase2Coverage.FilterMismatchRejected` | 同上路径守卫；`:3619-3622` | **不合理（过度守卫）**：该负例断言「不存在的 HiPS 路径必须被拒」，**根本不需要真实夹具**，却先被真实数据守卫跳过 → 可用合成路径无条件运行 |
| 319 | `Phase2Sampler.RealHipsControlSampling` | 同上；`:3632-3635` | 不合理（硬编码路径，Linux 零执行） |
| 320 | `Phase2Sampler.G6LocalSnrAvailabilityThreeZones` | 同上；`:3679-3688` | 不合理（硬编码路径，三区局部 SNR 在 Linux 零执行） |
| 322 | `Phase2Identity.G3StableFrameIdentity` | 同上；`:3831-3837` | 不合理（硬编码路径；frame_id 稳定性在 Linux 零执行） |
| 323 | `Phase2Identity.G3ManifestOrderCanonical` | 同上；`:3940-3946` | 不合理（硬编码路径；manifest 规范化在 Linux 零执行） |
| 342 | `phase2_sampler_parallel.Phase2SamplerParallel.OneTvsTwoTDeterminism` | 硬编码路径；`lib/algorithms/coverage/tests/sampler_parallel_consistency_test.cpp:33-37` | **合理平台跳过（有合成替代）**：同文件 `:162 SyntheticFixtureBitwiseDeterminism` 用合成 HiPS 做 1T/2T 逐字段位精确，并有「门前提自检 `ASSERT_GT(n1,0)`」防空门 |

**结论**：12 个跳过中，**6 个合理平台/硬件跳过**（#57、309、310、342 + #311/312 主体），**5 个属「硬编码 Windows 绝对路径导致的 Linux 零执行」**（#317、318、319、320、322、323，共 6 项，其中 318 为过度守卫），**不存在以 SKIP 冒充通过的用例**（跳过不计入 430 通过）。但「Skipped ≠ Passed」：上述 6 项在 Linux 生产平台上的科学门实际为零执行，必须视为**覆盖缺口**（§4.4）。

---

## 3. 断言强度抽检表

抽检对象：任务点名的 12 个模块 + 3 个横向面。证据为 file:line + 断言原文。

### 3.1 空断言 / 恒真断言（确认）

| 文件:行 | 断言原文 | 判定 |
|---|---|---|
| `tests/unit/p2_upm_synthetic_test.cpp:125` | `CHECK(true);  // 语义: 合成框架无单覆盖污染` | **空断言**。更重：该 target 只链 `astrocs_contracts`（`tests/unit/CMakeLists.txt:695`）、不 include 任何产品头，全程本地重实现，未调用 `p2_upm_*` → ctest 名 `p2_upm_synthetic` 名不符实 |
| `tests/unit/p2_rejection_test.cpp:89` | `CHECK(true);` | **空断言**（frame identity 语义无实际断言） |
| `tests/unit/p2_output_semantics_test.cpp:136` | `CHECK(true);` | **空断言** |
| `tests/unit/p2_output_semantics_test.cpp:23-26` | `CHECK(name[0] != '\0');` | **恒真**（对字符串字面量断言） |
| `tests/unit/p2_rejection_test.cpp:76-82` | 全零数组 `CHECK(sum == 0);` | **恒真**（空计数一致） |
| `tests/unit/rt001_abi_test.cpp:89` | `CHECK(r.failed() || r.ok())` | **恒真**（枚举穷举） |
| `tests/unit/rt006_scheduler_test.cpp:114` | `CHECK(r.ok() || r.failed())` | **恒真** |
| `tests/unit/rt006_scheduler_test.cpp:46` | `CHECK(parallel_peak.load() >= 1)` | **恒真**（串行也 ≥1） |
| `tests/system/v6_runtime/v6_runtime_resource_record_test.cpp:137-147` | negative 分支擦除本地字符串后再断言其已擦除 | **自证空转**，未调用任何门 |
| `lib/infrastructure/aio/tests/test_writer_integration.cpp:187` | `ASSERT_TRUE(true, "全部 256 个 signal 值匹配")` | **空断言**（同 :274/:360） |
| `lib/infrastructure/aio/tests/test_wph_cli_browser.cpp:193/200` | `CHECK(qret==0 || qret<0)` | 只排除 >0，未校验数值；:181 `expected_signal` 声明后从未比对 |
| `tests/monitoring/test_log_contract.py:347` | `assertTrue(td.exists() or td.parent.exists())` | **恒真** |
| `tests/monitoring/test_run_graph_render.py:134-135` | `assertNotIn("graphviz", src.replace("graphviz",""))` | **恒真**（先 replace 再断言） |
| `tests/cpu/.../test_isa_avx.py:109` | `self.assertTrue(True)` | **空断言**（比值仅 LOG 人工判读） |

> 影响：上述 C++ 空/恒真断言所在 target 多数在 442 内（`p2_upm_synthetic`、`p2_rejection`、`p2_output_semantics`、`rt001_abi`、`rt006_scheduler`、`v6_runtime_resource_record_negative`）。即 BLD-001 的「430 通过」中包含**至少 6 个断言强度不足的用例**。

**Phase1 补充（子代理审查，file:line 证据）**

| 文件:行 | 断言原文/现象 | 判定 |
|---|---|---|
| `lib/algorithms/noise_snr/tests/p1noise/p1noise_tests_core.cpp:552/561/655/715/814` | `P1NOISE_CHECK(cs, true, ...)` ×5 | **恒真锚**（无验证力，仅故障注入占位） |
| `lib/algorithms/noise_snr/tests/p1noise/noise_model_numpy_oracle.py:35-39/345-346` | 缺 numpy/g++ 时 `sys.exit(0)` | **独立 Oracle 假绿**（缺件即判绿，违反 fail-closed） |
| `lib/algorithms/calibration/tests/p1cal/p1cal_tests_core.cpp:312` | `mean_median_agree_narrow m<=2.0` | 绝对容差偏松（已注记 n=5） |
| `lib/algorithms/star_detection/tests/p1star/p1star_tests_core.cpp:474` | `f4_mag_box_integral_weak max_mag_abs<=1.5 mag` | 明显宽松（自承弱哨兵） |
| `lib/algorithms/drizzle/.../p1drz_tests_core.cpp:558-590` | 非法 nside 用 fork 判「非正常完成」 | abort/异常/返回 false 皆算拒绝，判别力弱 |
| 各模块 `*_tests_perf.cpp` | perf 门仅 ratio 区间（cal [0.4,4.0]、cos [0.25,4]、star/phot/drz parity<4.0 且 trend≥0.25） | 仅 p1psf 断言 E.7≥1.60（`p1psf_tests_perf.cpp:128`），其余为防倒退哨兵，非性能验收 |
| `lib/algorithms/noise_snr/tests/p1noise/p1noise_tests_core.cpp:703-715` | 参数静默钳位（DISP-NOISE-007） | 静默退化，测试锁定现状 |

### 3.2 容差/界过松

| 文件:行 | 断言 | 问题 |
|---|---|---|
| `tests/api/test_upm_recovery_oracle.py:131` | `assertTrue(abs(v) < 1e6)` | 界极松（1e6 量级界对物理量近乎无约束） |
| `tests/api/test_upm_recovery_oracle.py:133` | `assertAlmostEqual(o0, o1, delta=0.5)` | 容差 0.5 偏松 |
| `tests/api/test_reject_parallel.py:114` | `9.0 < chk/(1<<18) < 12.0` | 粗区间，非精确基线 |
| `tests/api/test_reject_parallel.py:121` | `assertLess(four_ms, one_ms*1.25)` | 只证「不慢 1.25 倍」，非加速/扩展性 |
| `tests/backend/test_p3_resample.py:147` | `assertLess(abs(v2-v1), 1e6)` | 在 1e8–12e8 量级上约 1%，容差偏松 |
| `tests/unit/v6_p3_rsmp/p3_rsmp_test_util.h:52` | `P3_CHECK_CODE` 只查期望码「存在」不查唯一性 | 其他门误命中即算过 |
| `tests/oracle/gaia_oracle.py:25` | `POS_TOL=5e-10` | 相对 ulp(~1e-14) 偏松 |

### 3.3 只断 rc / 软通过 / 静默退化

| 文件:行 | 问题 |
|---|---|
| `tests/cli/test_phase2_inprocess.py:225` | `assertIn(r.returncode,(0,10))` —— 资源门失败 rc=10 被当成功接受 |
| `tests/backend/test_p2004_reject_integrate.py:154/160/164/168/172` | 5 个用例只断言同一 monolith driver `returncode==0` / `assertIn("P2-004 DRIVER PASS")`，无逐项独立断言 |
| `tests/api/test_upm_parallel.py:42/50` | 已算 `csum`，printf 的 `calibrate_sum` 实参写死 `0.0`；:91-101 test_02 只比 `controls=(\d+)`，docstring 声称的「标定值一致」从未比较 |
| `tests/backend/test_p2001_parallel_sampler.py:150` | 只比 `o1[0]`（n_obs 计数），未比数值/模型 |
| `tests/api/test_reject_integration_oracle.py:154-158` | test_06 docstring 称 small-N 欠定，实际用 8 样本正常栈，未覆盖欠定分支 |
| `tests/unit/v6_p3_rsmp/run_mutations.py:18` | `LIB=ROOT/lib/phase3_rsmp` 目录不存在（实为 `lib/algorithms/resample`）→ mutation driver 失效；且 CMake `option(... OFF)`（`tests/unit/v6_p3_rsmp/CMakeLists.txt:45`）→ 默认不注册 |
| `tests/unit/p3_coverage_test.cpp` | 全文为本地 `struct TileSampler`（:19-37），不 include/链接产品实现（CMakeLists:737 仅 `astrocs_contracts`）→ 非产品测试 |
| `tests/unit/p2_ir_facade_test.cpp:29-69` | 仅读 `p2_session.cpp` 源文本 `find` 子串，无行为断言 |
| `lib/infrastructure/hips_browser/healpix_browser_qt/tests/test_browser_backend.cpp:8/27-41` | 裸 `assert` + Release(NDEBUG) → 断言被抹除，测试恒 exit 0；:15-18 硬编码 Windows 开发路径 |
| `lib/infrastructure/gaia_xpsd_client/CMakeLists.txt:892-903` | 无 Python3 时 `gaia_cat_oracle` 只 `message(WARNING)` 不注册（静默退化） |
| `tests/cpu/isa/test_isa_bit_manip.py:67-74` | 依赖 `artifacts/prerelease_v5/ISA-005/MEASUREMENTS.csv` 缺失（该目录仅 ISA-001/002/003）→ 条件实质恒真，Oracle 不执行 |
| `tests/unit/cpu006_bench_report_test.cpp:237-261` | release suite 环境变量门控，未设即 `return 0`（ctest 记 PASS） |

### 3.4 正向结论（抽检中确认「真在验证」的高强度用例）

- `tests/integration/v6_p3/p3_v6_export_e2e_test.cpp:518-562`：16 条冻结注入逐条断言 `status != Ok` + 期望码前缀 + **无可见 `product.fits` + 无 tmp 残留**（强负例范式）。
- `lib/algorithms/coverage/tests/sampler_parallel_consistency_test.cpp:162-235`：1T/2T 逐字段位精确（`EXPECT_EQ on double`）+ 门前提自检 `ASSERT_GT(n1,0)`（防空门）。
- `tests/unit/v6_p3_proj/v6_p3_proj_test.cpp:364/430/471/607`：归一化不变量、CRPIX↔CRVAL、奇点 fail-closed、1/2/4/8 worker `memcmp` bitwise。
- `tests/monitoring/test_frozen_gate.py`（19 用例全过）：G-RES-01 判据边界/哨兵/fail-closed 注入，doc §9 点名的负例面。
- `tests/quality/test_secret_hygiene.py`（22 用例全过）：CHK-SECRET-HYGIENE 的正/负例面（真仓绿 + 自造假凭据红 + 缺件 fail-closed）。

---

## 4. 负例缺失清单（每核心检查可执行负例核对）

权威：`ENGINEERING_SPEC.md §8` / `docs/ci/01_CHECKS.md §1`「每项检查必须提供机器可执行负例入口（`--self-test` 或 `--fault-inject`）；仅有人工说明不算」。
核对源：`ci/polarity_evidence.json`（45 条记录）+ 逐条实跑。

### 4.1 总体

| 类别 | 数量 | 说明 |
|---|---|---|
| checks.json 注册项 | **46** | `ci/checks.json` |
| polarity 记录 | **45** | **缺 `CHK-E2E-REPRO`**（`ci/polarity_probe.py --check` 会报 `missing_records`，见 :223-226） |
| PROVEN-EXECUTABLE | 22 | 其中仅 **7** 条使用**本检查器自身**的 `--self-test/--fault-inject` |
| RE-JUDGED | 4 | 均使用自身 `--self-test`（已实跑 rc=0） |
| FACE-DEFINED-NOT-RUN | **19** | 负例入口写成 `sh -c "<中文描述>"`，**非机器可执行** |
| 具备自身可执行负例的检查器 | **11** | 7 PROVEN + 4 RE-JUDGED |

### 4.2 缺可执行负例入口的检查器（19 + 1 = 20）

| ID | 门禁 | polarity 状态 | 记录的「负例」原文 | 缺失条件 |
|---|---|---|---|---|
| CHK-BUILD-LINUX | P0 | FACE | `sh -c 副本注入语法错误 ⇒ cmake …` | 需完整构建树 |
| CHK-BUILD-WIN | P0 | FACE | `sh -c 副本注入语法错误 ⇒ ci_windows_driver.py …` | 需 Windows |
| CHK-CONTRACT-TEST | P0 | FACE | `sh -c 临时树删 docs/TRACEABILITY.csv 单位节 ⇒ …` | 需脚本化夹具 |
| CHK-UNIT | P0 | FACE | `sh -c ctest … -R <target> 注入失败断言 ⇒ 必须非 0` | 需 ctest 树 |
| CHK-ORACLE | P0 | FACE | `sh -c 模块数值 target 注入超差 ⇒ 必须非 0` | 需 ctest 树 |
| CHK-INVARIANT | P0 | FACE | `sh -c 不变量 target 注入违反 ⇒ 必须非 0` | 需 ctest 树 |
| CHK-SYNTH-P1 | P0 | FACE | `sh -c 合成链注入坏帧 ⇒ 必须非 0` | 需 ctest + 合成夹具 |
| CHK-SYNTH-P2 | P0 | FACE | `sh -c 同上（含 v6 CLI 模式矩阵）` | 需构建产物 |
| CHK-SYNTH-P3 | P0 | FACE | `sh -c 同上（v6_p3_rsmp_mutation_driver）` | 记录称目标缺失，但 `v6_p3_export_negative`（16 注入）与 `v6_p3_proj_fault_*`（5）**已注册并运行** → 记录**过时**；真正缺失的是 resample mutation driver（见 4.3） |
| CHK-NWORKER | P0 | FACE | `sh -c 1 vs N worker 注入非确定性 ⇒ 必须非 0` | 需运行时驱动 |
| CHK-SANITIZER | P1 | FACE | `sh -c 副本注入越界 ⇒ ASan/UBSan 必须非 0` | 需 clang 重建 |
| CHK-COVERAGE | P2 | FACE | `sh -c 报告项：无阈值/无红判据` | 报告项（已登记无判据） |
| CHK-PACKAGE | P0 | FACE | `sh -c 候选注入哈希不符 ⇒ ci/validate_candidate.py 必须非 0` | 需 Windows |
| CHK-ENV-ADOPTION | P1 | FACE | `sh -c verify_toolchain.py 注入 lock 漂移 ⇒ 必须非 0` | **本机可跑**；本轮已补机器化负例（§5），并暴露 fail-open 缺陷（§6-1） |
| CHK-SECRET-HYGIENE | P0 | FACE | `sh -c 临时 git 仓注入假凭据 ⇒ …` | **本机已有 `tests/quality/test_secret_hygiene.py`（22 用例，实跑全过）** → 记录过时 |
| CHK-KNOWN-FAILURES-BASELINE | P1 | FACE | `sh -c --mode verify 指向不含基线的 JUnit ⇒ …` | 本机已有 `tests/quality/test_known_failures_baseline_ci.py`（28 用例，实跑全过）→ 记录过时 |
| LINUX-MAIN-FIXTURES | P0 | FACE | `sh -c wf_step.py … 缺夹具树 ⇒ 必须非 0` | wf_step 绑定面 |
| LINUX-MAIN-BUILD-TREE | P0 | FACE | `sh -c 同上（9000s 级构建）` | 构建树 |
| WIN-CANDIDATE-VALIDATE | P0 | FACE | `sh -c 同上（WINDOWS-VALIDATE-CANDIDATE）` | Windows |
| **CHK-E2E-REPRO** | — | **无记录** | — | polarity ledger 未登记 |

### 4.3 PROVEN 但负例非本检查器自身（15）

`CHK-WARN、CHK-CONTRACT-REF、CHK-SCI-REF、CHK-STALE-DOC、CHK-ABI、CHK-ISA-EQ、CHK-RESOURCE、GLOSSARY-DOCS、VERSION-NAMESPACES` 共 9 条，其 polarity `negative` 命令均为 `python3 tools/quality/check_prod_reachability.py --selftest`（want_rc=0）。该脚本 `tools/quality/check_prod_reachability.py:207-231` 只自测「CLI 中直接调 session+drizzle 被抓」，与上述 9 个门各自判据无关；且 want_rc=0 与「负例必红」语义相反 → **负例借壳**。

另有 6 条（CHK-STATIC、CHK-MODULE-MANIFEST、API-DOCS、CHK-ROOT-CLEAN、CHK-SCHEMA、CHK-PKG-CONSISTENCY）以「空树/缺件 fail-closed rc≠0」作为负例，属可执行但**只覆盖 fail-closed 面，未覆盖各自判据违规面**。

### 4.4 真缺失的可执行负例（高价值）

1. **resample 模块 20 条 mutation 驱动失效**：`tests/unit/v6_p3_rsmp/run_mutations.py:18` `LIB=ROOT/lib/phase3_rsmp`（不存在）→ `stage()` 抛 `FileNotFoundError`；且 `tests/unit/v6_p3_rsmp/CMakeLists.txt:45` `option(V6_P3RSMP_ENABLE_MUTATION_DRIVER … OFF)` 默认不注册。
   复现：`ls -d lib/phase3_rsmp`（无此目录）；`grep -n 'phase3_rsmp' tests/unit/v6_p3_rsmp/run_mutations.py`。
2. **projection astropy 变异驱动未注册**：`tests/unit/v6_p3_proj/CMakeLists.txt` 只 `add_test` run，`p3_proj_wcs_oracle.py` 的 mutate/mutate-all 未进 ctest。
3. **8 枚 aio 测试零 CMake 注册**（存在但不在 ctest/CI，假绿覆盖）：`test_p0_io_hardening / test_checksum / test_writer_integration / dataflow_fuzz / test_wph_cli_browser / test_tile_model / test_query_pixel / test_transform`；复现：`grep -rl test_p0_io_hardening --include=CMakeLists.txt .`（0 命中）；`ctest -N | grep -icE 'p0_io|checksum|writer_integration|dataflow'`（0）。
4. **hips_browser 测试不在构建面**：`lib/infrastructure/hips_browser/healpix_browser_qt/CMakeLists.txt:126` `option(BUILD_TESTS OFF)`，且根 `CMakeLists.txt` 无 `add_subdirectory`。
5. **gaia 两枚测试零注册**：`test/test_gaia_race.c`、`test/test_spec_collector_ownership.c`。

---

## 5. 补测试清单（新增文件 + 证据）

### 5.1 新增：`tests/quality/test_env_adoption_negative.py`

- 目的：把 CHK-ENV-ADOPTION 的负例面从「人工说明」变为「机器可执行」（§4.2 中标记为本机可跑的 FACE 项）。
- 内容：**1 正例 + 10 负例**，全部在 tempfile 副本上注入，只读工作区：
  - 正例 `test_positive_real_repo_is_green`：真仓 `ci/toolchain.policy.json` + `ci/toolchain.lock.json` → rc=0；
  - 负例（逐条必红）：`schema_version` 漂移、`scope` 不匹配、`hosted_ci_versions_required`/ `host_reprovisioning`/`acr` 翻转、cmake 版本占位、version_line 与 version 不一致、`present` 非布尔、`missing_tools` 非 list。
- 验证方式（已实跑，绿）：
  ```bash
  python3 -m pytest tests/quality/test_env_adoption_negative.py -q
  # => 10 passed in 0.45s   rc=0
  ```
- 附带发现：该测试的 3 条 fail-closed 负例（lock 非法 JSON / lock 缺失 / policy 缺失）**未通过**，暴露产品 fail-open 缺陷（§6-1），已从绿测试中移出并在文件内注释登记，待修复后恢复。

### 5.2 已登记、建议后续补充（未做，避免重型构建/越权）

| 候选测试 | 目标缺口 | 归属 |
|---|---|---|
| `tests/unit/v6_p3_rsmp/run_mutations.py` 路径修正 + `V6_P3RSMP_ENABLE_MUTATION_DRIVER=ON` | resample 20 条 mutation 负例（§4.4-1） | 产品/测试修复任务（文件域含 lib/algorithms/resample 与 tests/unit） |
| `Phase2Coverage.FilterMismatchRejected` 去真实数据守卫，改合成/不存在路径 | 过度守卫导致的 Linux 零执行（§2 #318） | coverage 测试修复任务（lib/algorithms/coverage/tests） |
| Phase1 各模块 1vN 并行一致性用例（cal/cos/star/psf/wcs/phot/drz） | §1 并行确定性空白 | 各算法模块域 |
| fits_output >2GiB / 长路径 / block-cache-worker 一致性 | §1 边界/并行空白 | fits_output 域 |
| observability 编排连续性指标（worker 空转率、上下文切换、块重载、缓存命中率、跨 worker 搬运量） | §1 性能空白（doc §8 要求） | runtime/observability 域 |

---

## 6. 未修项与建议归属（产品缺陷登记，只登记不修）

> 以下均给出 file:line 与复现命令；修复由前台分派。**本任务未改任何产品代码。**

1. **[P0/治理] 注册表↔文档双向不一致，CHK-REGISTRY-DOC-SYNC 当前判红**
   `ci/checks.json:6959`（`CHK-EXIT-CONSISTENCY`）、`ci/checks.json:7039`（`CHK-E2E-REPRO`）已注册，但 `docs/ci/01_CHECKS.md` §2 表（44 项）未登记。
   复现：`python3 ci/check_registry_doc_sync.py; echo $?` → `REGISTRY_DOC_SYNC_FAIL: registered_not_documented: ['CHK-E2E-REPRO','CHK-EXIT-CONSISTENCY']`，rc=1。
   归属：CI 注册表域（`ci/checks.json` + `docs/ci/01_CHECKS.md`）。

2. **[P0/负例面] polarity ledger 不完整且多处过时/借壳**
   - `ci/polarity_evidence.json` 45 条 vs 46 注册项，缺 `CHK-E2E-REPRO`；
   - 19 条 FACE 负例为 `sh -c` 中文描述，非机器可执行（`ci/polarity_probe.py:159-190`）；
   - 9 条 PROVEN 的负例借壳 `check_prod_reachability.py --selftest`（`ci/polarity_probe.py:143-156`）；
   - CHK-SECRET-HYGIENE / CHK-KNOWN-FAILURES-BASELINE / CHK-SYNTH-P3 记录过时（可执行负例已存在）。
   归属：CI-003/polarity 域。

3. **[P1/测试缺陷] `ci/verify_toolchain.py` fail-open（本任务新增测试暴露）**
   `ci/verify_toolchain.py:207-208` 以 `main()`（无 `sys.exit`）作 `__main__` 入口；`:111` 与 `:123` 的 fail-closed `return 1` 只从函数返回、不传播为退出码 → lock 非法 JSON / lock 缺失 / policy 缺失时打印 `[FAIL]` 却 exit 0。违反 `ENGINEERING_SPEC.md §8`。
   复现：`echo '{ not json' > /tmp/l.json && python3 ci/verify_toolchain.py --policy ci/toolchain.policy.json --actual /tmp/l.json --scope agent-host; echo $?` → `[FAIL] …` 且 rc=0。
   建议修复：入口改 `raise SystemExit(main())`（并让早退路径 `return 1` 生效）。
   归属：CI 环境接管域。

4. **[P1/测试缺陷] 6 个真实 HiPS 门硬编码 Windows 绝对路径，Linux 恒跳**
   `lib/algorithms/coverage/tests/synthetic_gate.cpp:3584/3620/3633/3683/3833/3943`、`lib/algorithms/coverage/tests/sampler_parallel_consistency_test.cpp:33` 写死 `F:/Astro dev/Astro CS Normalization Database/run/temp/...`，违反 `AGENTS.md §5`「不得写死服务器绝对路径」。
   复现：`grep -rn "F:/Astro dev" lib/algorithms/coverage/tests/`。
   归属：coverage/sampling/identity 测试域。

5. **[P1/测试缺陷] 空断言 / 恒真断言 / 名不符实 target**
   - `tests/unit/p2_upm_synthetic_test.cpp:125` `CHECK(true)`；该 target 未链产品库（`tests/unit/CMakeLists.txt:695`），非产品测试；
   - `tests/unit/p2_rejection_test.cpp:89`、`tests/unit/p2_output_semantics_test.cpp:136` 空断言；
   - `tests/unit/p3_coverage_test.cpp` 本地 `TileSampler` 非产品测试（`tests/unit/CMakeLists.txt:737`）；
   - `tests/unit/p2_ir_facade_test.cpp:29-69` 源文本 grep 冒充行为测试。
   归属：tests/unit 域。

6. **[P1/测试缺陷] resample mutation driver 失效且默认关闭**
   `tests/unit/v6_p3_rsmp/run_mutations.py:18` 路径失效；`tests/unit/v6_p3_rsmp/CMakeLists.txt:45` OFF。
   归属：resample 测试域。

7. **[P1/装配缺陷] 多枚测试零 CMake 注册（存在但不在 ctest/CI）**
   aio 8 枚（§4.4-3）、gaia 2 枚、hips_browser 全部（BUILD_TESTS OFF）。
   归属：aio/gaia/hips_browser 域。

8. **[P1/覆盖缺口] tests/test_index.csv 陈旧**
   声称 `tests/version`「16 用例 failures=2」，实测 `python3 -m unittest discover -s tests/version -t tests/version` → `Ran 29 tests … OK`（rc=0）；声称 `tests/unit` 仅 56 `add_test`，实测 ctest 442。
   复现：`python3 -B -m unittest discover -s tests/version -t tests/version`。
   归属：tests 索引维护。

9. **[P0/测试红灯] `tests/quality/test_root_cleanliness.py` 当前失败**
   `RootManifestFidelityTest::test_manifest_matches_engineering_spec_section7`：`ci/root_manifest.json` 的 `allowed_files` 含 `ACCEPTANCE_SPEC.md`，而 `ENGINEERING_SPEC.md §7` 根固定条目代码块未列该文件。
   复现：`python3 -m pytest tests/quality/test_root_cleanliness.py -q` → `1 failed, 4 passed`。
   归属：目录规范域（`ci/root_manifest.json` 或 `ENGINEERING_SPEC.md §7`，需负责人确认哪一侧为准）。

10. **[P1/装配] 文档声明的实现/契约不一致（子代理审查证据）**
    - `docs/plugins/infrastructure/21_observability.md:17` 引用 `contracts/schemas/events.schema.json`，该文件不存在（实际 `jsonl_event_v1.schema.json`）；
    - fits_output 文档 §4/§5 的 `band_height/tile_cache_mb` 在 `lib/`、`include/` 零消费（`grep -rn 'band_height\|tile_cache_mb' lib/ include/` 无输出），`p3_output.cpp:234-236` 单次整幅 `fits_write_pix`；
    - `p3_output.cpp:235` `const long nelem = (long)width*height;` 在 Windows LLP64 下 >2GiB 溢出风险，且无 >2GiB 测试（`grep -rn '2GiB\|2147483648' tests/` 0 命中）。
    归属：observability/fits_output 域（需负责人确认文档与实现哪侧为准）。

11. **[P2/治理] `ci/checks.json` 陈旧路径锚**：含 `lib/phase2/**`（:796,971）、`lib/astro_image_io/**`（:3070,3303,3340,3377,3414），两目录均不存在；`build.sh:26`、`ci/steps/linux_build_root_graph.sh:49-55` 引用 `lib/phase2`、`lib/astro_image_io`。
    复现：`ls -d lib/phase2 lib/astro_image_io`。
    归属：CI/构建域。

12. **[P1/测试缺陷] 独立 Oracle 假绿（缺件 exit 0）**
    `lib/algorithms/noise_snr/tests/p1noise/noise_model_numpy_oracle.py:35-39/345-346`：缺 numpy 或 g++ 时直接 `sys.exit(0)`，ctest/脚本记 PASS。
    复现：`python3 lib/algorithms/noise_snr/tests/p1noise/noise_model_numpy_oracle.py; echo $?`（无 numpy 环境 → 0）。
    归属：noise_snr 测试域。

13. **[P1/装配缺陷] Phase1 多枚测试文件存在但零 CMake 注册**
    platesolve `test_synthetic.cpp / test_kvector.cpp / test_last_inlier_reset.cpp`；star_detection `test/sdet_fp64_test.cpp / test/sdet_saturation_cursor_test.cpp`；psf `test/test_dpsf_nan_sort.cpp`；drizzle `healpix_drizzle/tests/*.cpp` 与 `test_drizzle.py`。
    复现：`ctest --test-dir build -N -R "test_kvector|test_synthetic|last_inlier|sdet_fp64|saturation_cursor|dpsf_nan_sort"` → `Total Tests: 0`。
    归属：各算法模块测试域。

14. **[P2/一致性] 性能门标准不统一**
    仅 `p1psf_tests_perf.cpp:128` 断言 E.7 ≥1.60；cal/cos/star/phot/drz 仅 ratio 区间防倒退（`p1cal_tests_perf.cpp:97-98` 等）。
    归属：各算法模块测试域。

15. **[P1/装配] Phase1 测试未纳入覆盖率收集面**
    `tools/quality/ci_coverage_runner.py:104` 默认 `--tests tests`，不收集 `lib/**/tests` → Phase1 模块测试（ctest 内）不进入覆盖率报告。
    归属：CI 覆盖率域。

---

## 7. 验收门对照（TST-001 §验收门）

| 验收门 | 结论 | 证据 |
|---|---|---|
| 测试缺口矩阵填满，无空白或全部登记转修 | **部分**：矩阵已填；空白缺口 21 处已在本报告 §1.1 与 §6 登记转修 | §1、§1.1 |
| 每个核心检查具备可执行负例 | **不满足**：46 注册项中仅 11 项有自身可执行负例；20 项缺（19 FACE + CHK-E2E-REPRO 无记录） | §4.1-4.2 |
| 无 SKIP 充数、无空断言、无静默退化；抽检存档 | **不满足**：无 SKIP 充数；但确认 ≥6 个 C++ 空/恒真断言、多处软通过/静默退化 | §3.1、§3.3 |
| 补充后 ctest 全绿、新增负例无注入绿/注入红 | **新增测试绿**（10 passed）；ctest 未重跑（任务禁跑全量） | §5.1 |
| 测试覆盖（按合同对象）达 L1 标准 | **待补**：L1「每科学模块至少一条 Oracle + 一条不变量」多数满足；「每核心检查可执行负例」未满足 | §1、§4 |

**登记缺陷/缺口共 15 项（P0 3 项）**；其中**需上呈负责人裁决 5 项**：§6-1（注册表双向不一致）、§6-2（负例面 ledger 不完整/借壳）、§6-3（verify_toolchain fail-open）、§6-9（root_cleanliness 红灯，需定文档/清单哪侧为准）、§6-10（文档↔实现不一致需定侧）。

---

## 附录 A：审查方法与可复现命令

```bash
# A. 442 清单 / 跳过
ctest --test-dir build -N | grep -oE 'Test +#[0-9]+: [^ ]+' | sed 's/.*: //' | sort > /tmp/ctestlist.txt
grep -nE 'Skipped|tests passed' run/RELEASE-01/logs/BLD-001/build_ctest.log

# B. 注册表 / 负例面
python3 ci/check_registry_doc_sync.py; echo "rc=$?"
python3 ci/polarity_probe.py --plan
python3 tools/quality/check_impact_map.py --self-test; echo "rc=$?"
python3 ci/check_registry_doc_sync.py --self-test; echo "rc=$?"
python3 tools/check_l0_docs.py --self-test; echo "rc=$?"
python3 ci/check_version.py --self-test; echo "rc=$?"
python3 tools/check_agents_gov.py --self-test; echo "rc=$?"
python3 tools/doccheck/check_engineering_constraints.py --self-test; echo "rc=$?"
python3 tools/doccheck/check_doc_index.py --self-test; echo "rc=$?"
python3 tools/quality/check_exit_conclusion_consistency.py --self-test; echo "rc=$?"
python3 docs/standards/checks/check_standards_registry.py --root . --fault-inject dangling-deviation-id; echo "rc=$?"

# C. 断言强度抽检
grep -rn 'CHECK(true)' tests/unit/
python3 -m pytest tests/quality/test_secret_hygiene.py -q          # 22 passed
python3 -m pytest tests/quality/test_root_cleanliness.py -q        # 1 failed, 4 passed
python3 -m pytest tests/monitoring/test_frozen_gate.py -q          # 19 passed
python3 -B -m unittest discover -s tests/version -t tests/version  # Ran 29 tests OK

# D. 新增补测试
python3 -m pytest tests/quality/test_env_adoption_negative.py -q   # 10 passed

# E. 产品缺陷复现（只读）
ls -d lib/phase2 lib/phase3_rsmp lib/astro_image_io                  # 均不存在
grep -rn "F:/Astro dev" lib/algorithms/coverage/tests/
echo '{ not json' > /tmp/l.json && python3 ci/verify_toolchain.py   --policy ci/toolchain.policy.json --actual /tmp/l.json --scope agent-host; echo "rc=$?"  # rc=0（fail-open）
```

## 附录 B：证据来源标注

- 标注「实查」：由本 agent 直接执行命令/读源确认；
- 标注「子代理审查」：由三个只读审查子代理产出（Phase1 / Phase2 / Phase3+基建），均附 file:line；其中 §3.2、§3.3、§1 的多数 file:line 来自子代理，已抽验代表性条目（空断言、零注册、BUILD_TESTS OFF、band_height 零消费、root_cleanliness 红灯）与之一致。
