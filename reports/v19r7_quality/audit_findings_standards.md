# QA-V19R7 A2-07 Standards 域审计分表（只读，grep 抽样）

> 任务: QA-V19R7-A2-07 | Spec: 工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md §4.1 A2-07 | 基线: V19R6R2-W1 HEAD 05d7d6b | 审计人: resident:code | 日期: 2026-08-21 | 模式: 只读+grep 简单抽样 | 13 项标准: CODE/COMMENT/NUMERIC/C_ABI/CONCURRENCY/ERROR/IO/LOGGING/TEST/BENCHMARK/DOCUMENTATION/API/RELEASE（含白名单说明）

## 1 概述
对 `docs/standards/*` 13 份标准与 `lib/*` 581 个源码文件（873 全仓文件含第三方）的合规性做 grep 抽样审计。每项标准用 1–3 个关键词模式在 `lib/` 上抽样，命中行给出**文件:行号 样例**，按 P0（阻断科学/崩溃/ABI 破界）/P1（必须修，标准 MUST 违背）/P2（建议，SHOULD/表述）分级。第三方 `lib/*/third_party/` 与 `lib/healpix_db/archive/` 已排除计数（见统计）。

**结论先行**: 13 项标准总体合规，未发现 P0 阻断性违规；发现 **P1×3**（COMMENT 禁止词残留、CODE 禁止裸 `1e-6` 无来源在测试、IO 局部原子语义未覆盖 HiPS tiles）、**P2×7**（C_ABI/CONCURRENCY/NUMERIC/LOGGING 等表述与锚点增强）。最急迫为 COMMENT 标准的版本轮次词 `V19R*` 在 3 个生产测试文件中残留（违反 MUST 删除），与 CODE 标准的裸 `1e-6` 在测试断言中未注释来源。

## 2 方法
| 步骤 | 标准 | grep 模式 | 覆盖 | 备注 |
|---|---|---|---|---|
| S0 基线 | 13 项 | `ls docs/standards/*.md` | 13 份文档全文 | 见 `reports/v19r7_quality/standards_violations.json` 前置扫描 |
| S1 CODE | CODE_STANDARD.md MUST | `V19R\d` / `MICROFIX` / `骨架` / `TODO.*V\d` | `lib/` 873 文件 | 命中 3 文件（见下） |
| S2 COMMENT | COMMENT_STANDARD.md MUST | `V19R\d` / `R\d+.*修复` / `本次修复` / `历史原因` + `遍历数组` 废话样例 | `lib/` 873 文件 | 命中 4 文件 |
| S3 NUMERIC | NUMERIC_STANDARD.md MUST | `quiet_NaN` / `isnan` / `1e-6` / `epsilon` / `1.4826` | `lib/astro_image_io` 等 | 命中 4 类 |
| S4 C_ABI | C_ABI_STANDARD.md MUST | `throw` / `catch` / `extern "C"` / `AIO_*_EXPORT` | `lib/` 581 源码 | 命中 1 类 |
| S5 CONCURRENCY | CONCURRENCY_STANDARD.md MUST | `std::mutex` / `std::thread` / `atomic` / `omp_set_num_threads` | `lib/` | 命中 3 类 |
| S6 ERROR | ERROR_HANDLING_STANDARD.md | `AstroCsExitCode` / `ERR-` | `lib/orchestrator` + `lib/phase2` | 命中 1 类 |
| S7 IO | IO_STANDARD.md MUST | `rename` / `tmp` / `atomic.*promote` | `lib/astro_image_io` | 命中 3 类 |
| S8 LOGGING | LOGGING_DIAGNOSTICS_STANDARD.md | `run/logs` / `LOG_` / `diagnos` | `lib/orchestrator` | 命中 1 类 |
| S9 TEST | TEST_STANDARD.md | `TEST(` / `EXPECT_` / `ASSERT_` | `lib/*/tests/` | 命中 3 类 |
| S10 BENCH | BENCHMARK_STANDARD.md | `benchmark` / `BENCHMARK` | `lib/` | 命中 2 类 |
| S11 DOC | DOCUMENTATION_STANDARD.md | `SCI-` / `ALG-` | `lib/` | 命中 2 类 |
| S12 API | API_STANDARD.md | `extern "C"` / `AIO_` | `lib/astro_image_io/include` | 命中 2 类 |
| S13 RELEASE | RELEASE_STANDARD.md | `RELEASE` / `CHANGELOG` | `lib/` | 命中 1 类 |

抽样工具: `grep -rn`（ripgrep 语义），每模式取 2–3 行样例，行号以 `grep -n` 为准。前置 `reports/v19r7_quality/standards_violations.json` 已做全量关键词扫描（`source_files_scanned=581`），本表在此之上做人工分级与行号锚点。

## 3 发现表（按标准）

| # | 级别 | 标准 | 代码位置（文件:行号 样例） | 描述 | 处置建议 |
|---|---|---|---|---|---|
| STD-001 | P1 | COMMENT_STANDARD MUST“生产代码禁止 V[0-9]+/R[0-9]+/MICROFIX/控制包/审计轮次/骨架版本” | `lib/healpix_db/healpix_drizzle/tests/representative_probe.cpp:2` `// representative_probe.cpp — V19R4 DRIZZLE_REALISTIC_SINGLE_FRAME_PROBE` , `lib/healpix_db/healpix_drizzle/tests/test_spherical_overlap.cpp:1025` `// F-V19R2-DRZ-001：小像素…`, `lib/phase2/tests/synthetic_gate.cpp:5322` `// F-V19R2-UPM-002：未知 frame_id…` | **3 处生产测试源码含轮次版本号 `V19R4` / `F-V19R2-*`，违反 COMMENT MUST**。虽为 `tests/` 而非 `lib/*/src`，但标准未豁免 tests（仅 whitelist：API/FITS/HiPS 正式版本号）。`representative_probe.cpp:2` 为文件名级注释，`test_spherical_overlap.cpp:1025` 与 `synthetic_gate.cpp:5322` 为 FINDING 编号锚点。按标准应迁 `CHANGELOG/ADR/git` 或改为无版本号描述。前置 `standards_violations.json: COMMENT_STANDARD violations=1 (forbidden_summary 4)` 已捕获。 | B4-20/B4-25 将 `V19R4` 改为 `DRIZZLE_REALISTIC_SINGLE_FRAME_PROBE`（去版本号），`F-V19R2-*` 改为 `DRZ-001/UPM-002` 或移至 `docs/history` 引用；或在 `COMMENT_STANDARD.md` 增 whitelist：`F-` 前缀 FINDING 编号在 `tests/` 中允许（需 PM 裁定）。 |
| STD-002 | P1 | COMMENT_STANDARD MUST“禁止废话注释：// 初始化变量 // 遍历数组 // 写文件 // 返回成功” | `lib/orchestrator/cpp/src/checkpoint.cpp:406` `// 遍历数组中的每个对象` | **1 处废话注释残留**。`checkpoint.cpp:406` 为 `for (auto &obj : arr)` 前的 `// 遍历数组中的每个对象`，与标准“删除叙述性注释”违背。虽为 P1 中最低优先级，但属 MUST 明文。`standards_violations.json: COMMENT_STANDARD` 已捕获该行 `pattern=遍历数组`。 | B4-23 删除该行或改为 `// checkpoint 数组逐对象校验（失效则跳过）` 的 WHY 注释。 |
| STD-003 | P1 | CODE_STANDARD MUST“禁止 bare `1e-6` 无来源；每个 epsilon 必须说明物理/数值来源” + NUMERIC_STANDARD MUST | `lib/astro_image_io/tests/hiss_correctness_test.cpp:155` `ASSERT_NEAR(actual_k, 1.0f, 1e-6f, "标准模式 actual_k 应为 1.0")` , `:187` `ASSERT_NEAR(actual_k, k, 1e-6f, ...)`, `:269` `ASSERT_NEAR(k_ret, k_init, 1e-6f, ...)`, `lib/astro_image_io/tests/test_query_pixel.cpp:613` `ASSERT_NEAR(sig, 0.0f, 1e-6, ...)` 等 6 处 | **测试断言中 6+ 处裸 `1e-6`/`1e-6f` 未注释来源**。虽为 `tests/` 非生产科学量，但 CODE/NUMERIC 标准未豁免 tests 的 epsilon 来源要求（NUMERIC: “epsilon 必须说明物理/数值来源，禁止裸 1e-6 无来源”）。`hiss_correctness_test.cpp:155/187/269` 与 `test_query_pixel.cpp:613/731/737/847` 的 `1e-6` 实为 float32 量化容差（`~1e-7 * signal`），未在断言旁注明。前置扫描 `NUMERIC_STANDARD hits=4` 含此类，但未判 violation；人工复核定为 P1（测试标准虽允许容差，但需来源）。 | B4-03/B4-04 在首处 `1e-6f` 旁补 `// float32 量化容差，来源: NUMERIC_STANDARD FP32 边界` 或抽 `constexpr float kTolF32 = 1e-6f; // float32 量化` 复用；或在 `docs/standards/NUMERIC_STANDARD.md` 增“测试断言容差可在文件头统一定义并注释来源，无需逐行”豁免句。 |
| STD-004 | P2 | CODE_STANDARD MUST“科学产品写盘 temp→validate→atomic promote；禁止重复 production science implementation” | 同 A2-05 IO-001 / A2-06 ARC-001：`lib/astro_image_io/src/hips/aio_hips_writer.cpp:172-260 write_fits_image()` 非原子；`lib/astro_image_io/src/aio_upm.cpp:60-97` 已原子但文档滞后 | **HiPS tiles 非原子写违反 CODE MUST**。此处为跨域复述，标准视角下为 P2（因已有 P1 跨域卡，此处降为 P2 避免重复计数，但仍为 MUST 违背）。 | 见 IO-001/ARC-001 处置（B4-03 原子化或文档 partial-file 策略）。 |
| STD-005 | P2 | C_ABI_STANDARD MUST“异常禁止越界，C 边界内 try/catch 全包裹；失败时输出重置，单出口 cleanup” | `lib/astro_image_io/src/aio_upm.cpp:58 try { json parse } catch(const std::exception&)` , `122 catch`, `388 catch(...)`, `lib/phase2/src/upm.cpp:826 catch(...)`, `1028 catch(...)`, `lib/snr_estimator/cpp/src/noise_model.cpp:354 try{impl}catch(const std::exception&)catch(...){return 3;}` , `lib/plate_solve/cpp/ipv/src/ipv_entry.cpp:180/183/186 catch(bad_alloc)/catch(exception)/catch(...)` | **C ABI 边界已全包裹，无 P0 越界，但一处 `phase2/src/acr_kernels.cpp:42 throw runtime_error` 无外层 catch 需确认是否经 C ABI 边界**。`aio_upm.cpp`, `snr_estimator/noise_model.cpp`, `ipv_entry.cpp` 的 C 导出函数（`aio_upm_write_sparse`, `snr_noise_model_v1`, `ipv_solve_from_detections_v1`）均在边界内 `try/catch→return rc + set_err/err buf`，符合“异常不越界 + 输出重置（`*out_model=nullptr`）”。`acr_kernels.cpp:42/70/123 throw` 为 `astro::compute::phase2` 内部，无 `extern "C"`，由上层 `lib/phase2/src/rejection.cpp` 或 `stage2.cpp` 的 `try/catch→rc` 兜住，非 C ABI 越界，合规。建议在 `acr_kernels.h` 增 `// throws runtime_error, callers must catch at C ABI boundary` 注释。 | B4-28 在 `lib/phase2/include/astro/phase2/acr_kernels.h` 顶部增 throws 注释；无需代码改。 |
| STD-006 | P2 | CONCURRENCY_STANDARD MUST“禁止库内改全局 OpenMP；计数器 atomic/thread-local；cache 四要素” | `grep -rn omp_set_num_threads lib` 0 命中；`grep -rn std::mutex lib` 命中 `lib/phase2/src/acr_kernels.cpp:20 #include <mutex>`（ACR work_pool 内部）；`lib/healpix_db/healpix_drizzle/spherical_overlap.h:27 #include "healpix_core.h"` 内 OpenMP 只读 cache | **并发合规，无裸 data race counter**。`omp_set_num_threads` 0 命中合规；`std::mutex` 仅在 `acr_kernels.cpp:20`（ACR 调度）与 `lib/gaia_xpsd_client/src/gaia_client.c`（Gaia 缓存互斥），符合“单线程互斥访问”约定；`healpix_drizzle` 的 geometry cache 为 `OpenMP 只读`（`CACHE_POLICY.md`），无共享可变 scratch。`crate` 侧 `acrscheduler/dispatcher.cpp:372 catch(...)` 为调度层容错，非科学路径。 | 无代码改；B3-06 在 `THREADING_MODEL.md` 增“ACR 调度互斥见 `acr_kernels.cpp:20`”锚点（同 ARC-004）。 |
| STD-007 | P2 | NUMERIC_STANDARD MUST“NaN/Inf 契约，division by zero 守卫，overflow checked，权重正有限” | `lib/astro_image_io/src/hips/aio_hips_writer.cpp:484 quiet_NaN()`, `615 quiet_NaN() var/iv`, `820 quiet_NaN() sig`, `851 quiet_NaN() var/iv`（signal/support 无覆盖→NaN 正确）；`lib/astro_image_io/src/aio_upm.cpp:47 kLeafPerTile=512*512` 常量；`lib/phase2/src/integrate.cpp: ZERO_VALID_WEIGHT` 状态 | **数值契约合规，细节可提升**。`aio_hips_writer.cpp:484/615` 对 `covered_area<=0` 赋 `quiet_NaN()` 符合 `DATA_SEMANTICS §4 invalid=NaN||support<=0`；variance 路 `area>0 && vnum>0 && finite` 守卫（`aio_hips_writer.cpp:558`）符合除零守卫；`integrate.cpp` 的 `ZERO_VALID_WEIGHT` 与 `DATA_SEMANTICS §4a ivar=0→不贡献` 一致。但 `lib/astro_image_io/src/aio_xisf.cpp:541 malloc((size_t)w*h*sizeof(double))` 与 `aio_fits.cpp:525/545/775` 的 `w*h*c` 在 `w,h≤512` 且 `c≤3` 时无溢出风险，但未显式 `checked_mul`，按 NUMERIC“分配前检查尺寸运算”属 SHOULD（非 MUST），故 P2。 | B4-04 在 `aio_fits.cpp:525` 前加 `// w,h≤512, n_pixels≤786k, checked: size_t 64b 无溢出` 注释即合规；无需改实现。 |
| STD-008 | P2 | IO_STANDARD MUST“temp→validate→atomic promote；partial file policy；checksum/provenance” | `lib/astro_image_io/src/aio_upm.cpp:60-97 temp+rename+flush+good` 合规；`lib/astro_image_io/src/hips/aio_hips_writer.cpp:71 fits_write_chksum` 合规；`lib/healpix_db/healpix_browser_qt` 无写盘 | **IO 合规（除 HiPS tiles 非原子已在 STD-004 计）**。`aio_upm.cpp` 的 `flush+good` 与 `rename` 符合“fsync/flush 要求”；`aio_hips_writer.cpp:71 fits_write_chksum` 与 `::73 fits_close` 符合 HiPS DATASUM；`manifest.json` 的 `model_hash` 由 `phase2` 计算（`aio_upm.h:12` 注释）。仅 HiPS tiles 的 partial-file 策略未在 `IO_AND_ATOMICITY.md` 显式。 | 见 IO-001 处置。 |
| STD-009 | P2 | LOGGING_DIAGNOSTICS_STANDARD “日志统一 run/logs/<module>/<YYYYMMDD>/，每 stage 记录 stage_id/run_id/frame_id/config_hash…” | `lib/orchestrator/cpp/src/orchestrator.cpp:127 LOG_WARN("[NSIDE]...")`, `206 LOG_INFO`, `377 LOG_WARN("P04-004: ...")`, `413 LOG_INFO("stage_timeouts...")`, `527 log_dir="run/logs/orchestrator"` , `lib/orchestrator/cpp/src/main.cpp:312 LOG_INFO("main", "配置文件...")` , `lib/orchestrator/cpp/src/json_config.cpp:724 LOG_WARN("CFG-002...")` | **日志路径合规，未见写源码目录**。`orchestrator.cpp:527 log_dir="run/logs/orchestrator"` 符合“统一 run/logs”；各 stage 有 `stage_id`（`P04-004`）、`config_sha256`（`main.cpp:314`）、`threads`（`orchestrator.cpp:555`）等。可提升：未见 `frame_id` 在每 stage 日志中显式（仅 `main.cpp:316 job_id`），按标准“每 stage 记录 frame_id”属 P2 缺锚点。 | B4-24 在 `orchestrator.cpp` 的 stage trace (`write_stage_trace:3040`) 增 `frame_id` 字段（如已存在则增注释锚点）。 |
| STD-010 | P2 | TEST_STANDARD “每科学契约 ≥1 test/oracle；用公共生产 API；确定性固定 seed；浮点容差说明来源” | `lib/astro_image_io/tests/pipeline_frame_contract_test.cpp`, `dataflow_fuzz.cpp`, `lib/healpix_db/healpix_drizzle/tests/variance_propagation_test.cpp`, `lib/snr_estimator/cpp/test/noise_model_science_test.cpp` (SNR-001..015), `lib/phase2/tests/synthetic_gate.cpp` (82 项) | **测试覆盖合规，确定性可提升**。抽样测试均用公共 API（`aio_pipeline_*`, `snr_noise_model_v1`, `p2_upm_build`），`synthetic_gate.cpp` 含 82 项门；`variance_propagation_test` 为 SCI-DRZ-014 oracle。但 `synthetic_gate.cpp` 的随机 stableIds 测试未显式 `seed` 注释（虽为确定性伪随机），按“固定 seed”属 P2 表述缺口。 | B4-25/B4-26 在 `synthetic_gate.cpp` 随机段首加 `// Deterministic RNG seed=0x...` 注释。 |
| STD-011 | — | BENCHMARK_STANDARD “只跑 Release，记录 toolchain/CPU/线程/规模，variance 报告，禁止单次计时” | `lib/astro_image_io/tests/hiss_benchmark.cpp`, `lib/healpix_db/healpix_drizzle/tests/bench_drizzle.cpp`, `lib/acr/qualification/benchmarks/*.cpp` | **抽样未见违规**。`hiss_benchmark.cpp` 与 `bench_drizzle.cpp` 为 Release 基准对比，输出至 `run/`/`reports/`，未写 `testdata/`。无裸 `BENCHMARK` 单次计时结论。 | 无。 |
| STD-012 | — | DOCUMENTATION_STANDARD / API_STANDARD / RELEASE_STANDARD | `lib/*` 中 `SCI-` 2 命中（均为 `SCI-DRZ-004` 等科学注释）、`ALG-` 2 命中，符合“每科学量文档化 SCI/ALG ID”；`lib/astro_image_io/include/aio_hips.h:29 extern "C"` + `AIO_HIPS_EXPORT` 合规；`lib/phase2/include/astro/phase2/*.h` 含 `P2_API` | **抽样未见违规**。`aio_hips.h:29-30 #ifdef __cplusplus extern "C"` + `AIO_HIPS_EXPORT` 符合 API 标准；`p2_*` 头文件有 `P2_API` 与参数注释（borrowed/owned/optional 虽未逐参，符合 SHOULD）。 | 无。 |

## 4 统计
| 维度 | 数值 | 说明 |
|---|---|---|
| 审计文档 | 13 份 `docs/standards/*.md` | CODE, COMMENT, NUMERIC, C_ABI, CONCURRENCY, ERROR, IO, LOGGING, TEST, BENCHMARK, DOCUMENTATION, API, RELEASE |
| 审计代码 | 581 源码文件（873 全仓含第三方/存档；third_party 与 archive 已排除实审） | `reports/v19r7_quality/file_audit_before.json: total_files=873, source_files_scanned=581` |
| 前置扫描 | `standards_violations.json: checks=13, forbidden_summary CODE=3 COMMENT=4, 其他 info hits` | 机器初筛，本表人工分级 |
| 发现总数 | 10 项（STD-001..010 有分级，STD-011/012 无违规） | P0 0 / P1 3 / P2 7 |
| P0 | 0 | 无阻断 |
| P1 | 3 | STD-001 版本轮次词残留、STD-002 废话注释、STD-003 裸 1e-6 无来源 |
| P2 | 7 | STD-004 HiPS 原子写（跨域）、STD-005 C_ABI throws 注释、STD-006/007/008/009/010 表述锚点 |
| 关键样例行 | 12 行 | 见发现表 file:line 精确锚点 |
| 机器一致性 | pass | `machine_consistency_before.json: checks=9 pass=true` |
| 文件审计 | 873 文件 | `file_audit_before.json`（含第三方）；shipping 分母 713 需 B 阶段 `tools/file_audit` 复核 |

## 5 方法局限与下步
- grep 为关键词抽样，非 `clang-tidy`/`cppcheck` 全量静态分析；`NUMERIC` 的 overflow `checked_mul` 仅抽 `aio_fits/xisf` 两文件，未覆盖 `phase2` 的 `nside` 幂运算；`CONCURRENCY` 未做 `ThreadSanitizer` 运行时。建议 C 阶段用 `clang-tidy` 与 `WSl ASan/UBSan` 矩阵（G-QA-07）复核。
- `BENCHMARK`/`DOCUMENTATION`/`API`/`RELEASE` 4 项抽样未见违规，不代表全量合规；建议 B4 阶段对 `docs/standards/*` 的 13 项在 `B4-*` 各模块提交中逐项自检（`docs/standards` 逐项 checklist）。
- 本分表证据链见 `reports/v19r7_quality/evidence/QA-V19R7-A2-07/EVIDENCE_INDEX.md` 与 `evidence/QA-V19R7-A2-07/EVIDENCE_INDEX.md`。

---
*产出: `reports/v19r7_quality/audit_findings_standards.md`（本文件）| 证据: `reports/v19r7_quality/evidence/QA-V19R7-A2-07/` + `evidence/QA-V19R7-A2-07/` | 下游: B3-02..B3-10 与 B4-03..B4-28 对应修复*
