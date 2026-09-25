# -*- coding: utf-8 -*-
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emit import flush

ROWS = [
    ("71", "common/healpix/healpix_core.cpp :: R4-05.md::F-05-02",
     "common/healpix/healpix_core.cpp", "R4-05.md::F-05-02", "S2/S2/确认",
     "`lib/infrastructure/aio/Makefile` 与 `build.ps1` 是与根 CMake 并存的第二套构建，编译闭包按当前树已不可成立",
     "../common/healpix/healpix_core.cpp; ../common/crypto/sha256.cpp; lib/algorithms/shared/healpix/healpix_core.cpp; lib/infrastructure/aio/build.ps1:147; shipping",
     "lib/infrastructure/aio/Makefile:55/:57; lib/infrastructure/aio/build.ps1:147; CMakeLists.txt:216/243/256/429; eng/tools/HANDOVER.md:219",
     "仍在",
     "闭包路径仍按移动前布局：`lib/infrastructure/aio/Makefile:55` `../common/healpix/healpix_core.cpp \\`、`:57` `../common/crypto/sha256.cpp \\`（在 `SRCS` 列表内），`build.ps1:147` `$cmdArgs += 「-I../common/include」`。该 Makefile 位于 `lib/infrastructure/aio/`，`../common/` ⇒ `lib/infrastructure/common/`；`git ls-files lib/infrastructure/common | wc -l` ⇒ **0**（跟踪集中不存在该目录，移动前的 `lib/common/**` 亦为 0）。真实文件在 `lib/algorithms/shared/healpix/healpix_core.cpp`、`lib/algorithms/shared/crypto/sha256.cpp`、`lib/algorithms/shared/include/{astro_scalar.h,precision_context.h}`（`git ls-files lib/algorithms/shared` 命中），差两级 ⇒ 两套脚本的编译闭包当前不可成立。并存构建面为实：根 `CMakeLists.txt` 直接编 aio 源（`:216 acsd_io_adapter` ← `lib/infrastructure/aio/io/src/io_adapter.cpp`、`:243 acsd_runtime`、`:256 acsd_io` ← `io/fits_core.c`、`:429` 把 `lib/infrastructure/aio/src` 加进 include）⇒ 同一模块有根图与模块级 Makefile/ps1 两套并存。本批整改只动了 ISA 面未动闭包面：`aio/Makefile:1-3` 现读「ISA-001（TRUTHFUL-CONCLUSION-01）：本文件**不得**自带宿主 ISA 旗标（原 -march=native 已移除）… 根 CMakeLists.txt 是唯一产品事实源」（`5f8c237b` 09-25 加），即该提交自己承认「模块级独立构建路径」仍在位；同族事实见 `eng/tools/HANDOVER.md:219`「**8 个模块 Makefile 是无任何检查项覆盖的独立构建路径**（`-march=native` 已移除，是否收敛到根构建图待裁）」⇒ 本条主诉（第二套构建 + 闭包不可成立）逐字存续。",
     "高",
     "是（S2，与 ROW 70 同批）：模块级 Makefile/build.ps1 要么按新布局改路径并纳入根构建图反查，要么随 ARCH-001 残留除名（负责人待裁项，须给结论）"),

    ("72", "core/browser_backend.h :: R5-18.md::C-02",
     "core/browser_backend.h", "R5-18.md::C-02", "S1/S3/降级",
     "`BrowserBackend::ud_grade` 的头注释说「求均值」，实现是「求和」，二者科学语义相反",
     "core/browser_backend.h; core/browser_backend.cpp; memory.md; lib/healpix_db/archive/healpix_browser_cpp/include/browser_backend.h",
     "lib/infrastructure/hips_browser/healpix_browser_qt/core/browser_backend.h:124; core/browser_backend.cpp:593/619/622/629/649",
     "仍在",
     "被点名的头注释逐字未改（`git log -- …/core/browser_backend.h` 最新 `9a2b5d11`(09-21)，早于工单）：`browser_backend.h:123-125` 现读「// ---- 降采样 ---- / // NESTED 排序位运算: ipix_coarse = ipix_fine >> (2 * log2(ratio)) / **// 4^k 个相邻像素求均值合并**」，实现上方注释同样自陈均值：`browser_backend.cpp:593`「// ud_grade 降采样 (4^k 个相邻像素合并**求均值**, NESTED 排序)」。函数体却是求和：`:619`「// B20: signal = 累计通量 (HISS 规范: 不除面积), LOD 降采样必须**求和**」、`:629` `g.first += (double)input.pixel[i];  // signal **求和 (不取平均)**`、`:649`「// B20: signal 降采样用求和 (非平均), 符合累计通量语义」、`:622`「// 注: support 是面积比 [0,255] uint8, 应独立按面积求和后归一化处理」。⇒ 同一函数上「头注释=均值 / 实现=求和」相反语义原样并存（且 cpp 内 :593 与 :619-649 自相矛盾）；判据 `git grep -n 「求均值|求和」 -- …/browser_backend.h …/browser_backend.cpp` 命中 6 行，其中 2 行为「求均值」表述、位于声明与函数总述注释。",
     "高",
     "是（S3，显示面）：`browser_backend.h:124` 与 `cpp:593` 的注释须按 B20 口径改写为逐通道语义（signal 求和 / support 面积比另行归一），不得留「求均值」总述"),

    ("73", "core/healpix_math.cpp :: R5-18.md::C-07",
     "core/healpix_math.cpp", "R5-18.md::C-07", "S1/S2/降级",
     "同一模块内并存两套 LOD 降采样语义：`HealpixMath::ud_grade` 取均值，`BrowserBackend::ud_grade` 取求和",
     "core/healpix_math.cpp; core/browser_backend.cpp; core/gl_renderer.cpp",
     "lib/infrastructure/hips_browser/healpix_browser_qt/core/healpix_math.cpp:60-100; core/browser_backend.cpp:596-649; core/gl_renderer.cpp:1356/1365; app/browser_cli.cpp:1211",
     "仍在",
     "两套语义现读均在位且都是活调用点。均值套：`healpix_math.cpp:60`「// ud_grade: NESTED 降采样」，实现 `:84-99` 累加后**除计数** —— `entry.first += src_pixel[i]; entry.second += 1;` … `result.pixel.push_back((float)(kv.second.first / (double)kv.second.second));`。求和套：`browser_backend.cpp:596 LeafData BrowserBackend::ud_grade(...)`，`:629` `g.first += (double)input.pixel[i]; // signal 求和 (不取平均)`，`:649` 同义注释。调用侧分叉：渲染主路径用均值套 `gl_renderer.cpp:1356`「// 转换为 vector 调用 HealpixMath::ud_grade」→ `:1365 auto graded = HealpixMath::ud_grade(all.nside, src_ipix, src_pixel, target_nside);`；加载/CLI 路径用求和套 `browser_backend.cpp:548`/`:582` `LeafData downsampled = ud_grade(result, target_nside, data_min_, data_max_);` 与 `app/browser_cli.cpp:1211 LeafData graded = backend.ud_grade(all, 64, 0.0f, 1.0f);` ⇒ 同一 HISS signal（累计通量，B20 口径）经两条路径落到同一显示面时相差 `4^k` 倍。本批整改（`c4af4136`/`139a2bc4`/`5f8c237b`）未触及该模块（`git log -- …/core/browser_backend.h` 最新 `9a2b5d11`）。",
     "高",
     "是（S2）：LOD 降采样口径须统一到单一实现（按 B20 通道语义分派 signal/support/variance），并删去重复的一份；显示面亦应有正例/负例锁住「两条路径同一 nside 输出一致」"),

    ("74", "cpp/.h/.h :: C2-01gap-科学算法源码.md::A-C2gap-02",
     "cpp/.h/.h", "C2-01gap-科学算法源码.md::A-C2gap-02", "S2/S2/确认",
     "本批 29 个测试 TU 属 tracked-but-unbuilt，而 `docs/science/DRIZZLE.md` 以现在时把其中 3 个当作科学门证据；既有的「三合一证据门」因 watched 表手工维护而看不见它们",
     "CMakeLists.txt; p1drz_oracle.h; docs/science/DRIZZLE.md:111; docs/science/DRIZZLE.md:112; docs/algorithms/DRIZZLE_GEOMETRY.md:293; drizzle_freeze_test.cpp; eng/tests/quality/test_sci_evide…",
     "lib/algorithms/drizzle/healpix_drizzle/tests/CMakeLists.txt:91-102/159/291; tests/p1drz/CMakeLists.txt:85-90; docs/ci/01_CHECKS.md:103; eng/tests/quality/test_sci_evidence_registration.py:7/:47-74; docs/science/DRIZZLE.md:125/166/189/224",
     "仍在（DRIZZLE 点名的 3 个 TU 已入构建图；全量 tracked-but-unbuilt 面与手工 watched 表两半未收口）",
     "点名半**已收口**（`a84844cb` 09-25 01:42「新增 ctest 目标登记、证据登记转 EXECUTABLE」，工单之后）：`DRIZZLE.md:125/:166` 点名的 `candidate_oracle_test` 现有 `tests/CMakeLists.txt:91-92 add_executable(candidate_oracle_test …cpp)` + `:102 COMMAND candidate_oracle_test`；`:189` 点名的 `variance_propagation_test` 现注册为 `:291 add_test(NAME drizzle_pf_sb_variance COMMAND variance_propagation_test)`（并带 `:289-300` 的在册说明「原只编入不注册…本轮把 oracle 期望式改为科学正本口径」）；`:170/:224` 点名的回归门 `p1drz_disp009_gate` 现 `tests/p1drz/CMakeLists.txt:85-86 add_executable` 且被 `docs/ci/01_CHECKS.md:103 CHK-DRZ-DISP009` 与 `docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv:25 exe_target,p1drz_disp009_gate,…:85` 双向登记。全量半**未收口**：自写统计脚本（`独立审计/复算件/d9rest/unbuilt_tus.py`，只读 `git ls-files` × 受跟踪 CMake 非注释行名匹配；GLOB 仅 2 处 ⇒ 误判面小）⇒ 当前树 `lib/` + `eng/tests/` 的测试类 TU 281 个，其中 **39 个** 不出现在任何 CMake 非注释行，含 `lib/algorithms/platesolve/cpp/ipv/test/test_kvector.cpp`、`test_synthetic.cpp`、`lib/algorithms/noise_snr/cpp/test/noise_model_science_test.cpp`、`lib/algorithms/photometry/cpp/test/test_spectrum_integrator.cpp`、`lib/algorithms/shared/healpix/tests/test_healpix_oracle.cpp`、`lib/infrastructure/aio/tests/{test_checksum,test_tile_model,test_transform,test_query_pixel,hiss_correctness_test,pipeline_frame_contract_test,…}.cpp`、`lib/infrastructure/pipeline/orchestrator/cpp/tests/{test_checkpoint,test_dll_loader,test_logger,test_p1_batchB_fixes,test_p1_batchH_star_coord}.cpp`、`eng/tests/cpu/**/provider_*_test.c` 等 —— 与 `eng/tools/HANDOVER.md:219`「8 个模块 Makefile 是无任何检查项覆盖的独立构建路径」相互印证（部分 TU 由那些模块级 Makefile 编，故「不可构建面」与 ROW 70/71 同源）。门自身仍手工：`test_sci_evidence_registration.py:46`「# ── watched 证据表（**唯一登记面**；新增 SCI/ALG 点名的测试必须登记在此）──」，`WATCHED` 现只含 3 条（`synthetic_gate.cpp`、`control_median_mc_test.cpp` 两个阳性对照 + `kcorr_matrix_test.cpp` 一个未注册样本），`:79 CMAKE_GLOBS`/`:259 文档点名邻域判据` 的覆盖面完全取决于该手工表 ⇒ 「SCI/ALG 新点名一个未建 TU」不会被门自动看见，须有人先把它写进表里。",
     "高（点名半为逐条现读；39 这一数自写脚本可复算，口径见脚本）",
     "是（S2）：①39 个未入构建图的测试 TU 逐个收敛（入根图并登记 ctest，或除名）；②证据门的 watched 表改为从构建图/文档扫描自动生成，取消「先手工登记才可见」的结构"),

    ("75", "cpp/ipv/REPORT.md :: R7-04.md::S-R7-04-03",
     "cpp/ipv/REPORT.md", "R7-04.md::S-R7-04-03", "S1/S2/降级",
     "文档把 `-ffast-math` 写成在册编译面，实际两构建脚本均无该 flag；在册的 `-march=native` 反无人登记",
     "SIRIL_COMPARISON.md:544; build.ps1; cpp/ipv/REPORT.md:884; lib/algorithms/platesolve/cpp/ipv/build.ps1:27; REPORT.md/SIRIL_COMPARISON.md; docs/algorithms/CALIBRATION_ALGORITHMS.md:374; docs/architectu…",
     "lib/algorithms/platesolve/cpp/ipv/REPORT.md:884/914/916; cpp/ipv/SIRIL_COMPARISON.md:544/554; cpp/ipv/build.ps1:27/72; cpp/ipv/Makefile:1/:9; eng/tools/quality/isa_sites.json:4; eng/tools/quality/check_isa_same_source.py:19/327-338",
     "仍在",
     "文档侧行本身未改（`git log -- …/ipv/REPORT.md` 最新 `ee99ca26`(09-21)，早于工单）：`REPORT.md:884` 现读「| C | 6.4 | 编译优化 -O3 -ffast-math -funroll-loops | ✅ |」（以现在时记为已完成项），`:914`「**CXXFLAGS**: `-O2` → `-O3 -ffast-math -funroll-loops` (保留 `-fopenmp`)」，`:916`「**精度验证**: 5 帧 0 回归, -ffast-math 对 SVD/IRLS 数值稳定性无影响」；`SIRIL_COMPARISON.md:544` 把 `-ffast-math` 记入 V4.28 版本收益行、`:554`「| 编译优化 -O3 -ffast-math -funroll-loops | C Task 6.4 | build.ps1 | 中位 -7.4%, 0 回归 |」指名 `build.ps1` 承载。两条在册构建面实测无该旗标：`ipv/build.ps1:27 $CXXFLAGS = 「-std=c++17」,「-O3」,「-funroll-loops」,「-march=native」,「-Wall」,「-Wextra」,「-DIPV_EXPORTS」,…`、`:72` 同（**无 -ffast-math**）；`ipv/Makefile:9 CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -fopenmp`（**无 -ffast-math、无 -march**，`:1` 为 `5f8c237b` 新增的 ISA-001 声明「本文件不得自带宿主 ISA 旗标（原 -march=native 已移除）」）⇒ 「文档称已启用而构建面不存在」原样成立，且 `REPORT.md:221`「编译开关: `-O2 -march=native -Wall -Wextra`」与现 Makefile 亦不一致。第二半（`-march=native` 无人登记）现况：登记面 `eng/tools/quality/isa_sites.json:4` policy 要求「本表登记全量构建输入里的每一个 ISA 旗标站点…站点未登记 ⇒ 红…`-march=native`/`-march=` 一律红（AGENTS §6），仅 exemptions 可解，当前豁免面为空」，而 `git grep -n 「platesolve|ipv」 -- eng/tools/quality/isa_sites.json` ⇒ **0 命中**，`ipv/build.ps1:27/:72` 的宿主 ISA 站点仍不在册 ⇒ 半收口：登记义务与判红机制已在位（`check_isa_same_source.py:19`「S4 宿主 ISA 一律红：构建输入里出现 -march=native / 裸 -march= 即红」、`:338` 判词「宿主 ISA 不得硬编码: %s 出现 %s（AGENTS §6；豁免面为空）」），但该站点当前未登记、文档口径未订正，门一跑即红而非「已合规」。",
     "高",
     "是（S2，两条）：①`REPORT.md:884/914/916` 与 `SIRIL_COMPARISON.md:544/554` 的 `-ffast-math` 记述按实际构建面订正（未启用即不得记 ✅）；②`ipv/build.ps1:27/:72` 的 `-march=native` 站点入 ISA 登记表并给出收敛或除旗标结论"),
]

flush(ROWS)
print("batch3 ok:", [r[0] for r in ROWS])
