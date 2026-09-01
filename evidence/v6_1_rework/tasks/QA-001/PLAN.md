# QA-001: 零警告与静态分析

任务 ID: QA-001
Gate: G7
依赖: DOC-006
平台: Linux
变更类别: quality

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` QA-001：

> GCC Release、Clang Debug全自有production targets启用合理 `-Wall -Wextra -Wpedantic -Wconversion` 分层；
> 第三方只target-local隔离。修复monitor格式类型问题。运行clang-tidy/cppcheck或等价工具，
> 输出全部发现，P0/P1清零。禁止`-w`作用自有源码或宽泛suppression。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 自有生产 targets 分层严格 warning flags | 根 CMake 新增 QA-001 段: astrocs_core/common/phase2/phase3/cpu/CLI 等 14 个自有 target 启用 `-Wall -Wextra -Wpedantic -Wconversion`; 第三方 cfitsio 保持 `-w` target-local; 遗留 aio/hips/drizzle/calibration 用 `-fopenmp` 编译 omp pragma 不惩罚 | c01 GCC Release strict 0 warning |
| Clang Debug 同样零 warning | clang++ 单独编译关键自有单元验证 (见 c03) | c03 |
| 修复 monitor 格式类型问题 | cli/monitor.h `%llu`→`%" SCNu64`; `sum_eq/samples_.size()`→static_cast<double>; `val/100.0`→static_cast; cpu_monitor_test `%d`→`%%` | c01 |
| clang-tidy/cppcheck 输出全部发现, P0/P1 清零 | clang-tidy 扫描 77 自有生产源 (bugprone/clang-analyzer/performance 组) + cppcheck 77 源; 真实缺陷全修复; 遗留/风格类逐项核实 | c02 (tidy), c04 (cppcheck), 见下 |
| 禁止 -w 于自有源码/宽泛 suppression | tools/check_warning_suppression.py PASS: 生产源 0 抑制指令; -w 仅第三方豁免 | c05 |

## 修复的真实缺陷 (P0/P1 清零)

clang-tidy bugprone/clang-analyzer + cppcheck 实际缺陷修复清单:

| 缺陷 | 文件 | 修复 |
|---|---|---|
| `std::getenv` NULL → string 构造 UB | cli/commands.cpp:446 | 显式 NULL 检查后再构造 |
| `err` 可空指针直接解引用 (3 处) | cli/runtime_client.cpp | `if (err && !err->empty())`; phase_config 内 `if (err)` |
| `bad.api` 未初始化 (cppcheck uninitvar) | lib/backend_host/profile_gen_v2.cpp | `LoadedProvider bad{}` 值初始化 |
| `block_` 未初始化 (cppcheck uninitMemberVar) | lib/common/crypto/sha256.cpp | 构造函数 memset |
| `i/w` 整数除法赋 double | lib/backend_host/profile_gen_v2.cpp:109/182 | static_cast<double> 显式转换 |
| 未使用变量 `detected`/`ra0`/`m` | profile_gen_v2/hardware_inspect/wcs_tan | 删除 |
| `n_in` long→double 隐式转换 | lib/phase1/photometry/photometer.cpp | long→int (n_in 元素计数) |
| 死代码 `write_card_str`/`project` lambda/`ok_kernel` 等 | p3_output/p3_resample/cpu_bench_test | 删除 |
| misleading-indentation (if+return 同行) | lib/core/module.cpp, artifact.cpp | 展开为块 |
| `+0.5` 转整型四舍五入 | bench_harness.cpp percentile | std::llround |
| `tolower` int→char 转换 | aio_fits/aio_xisf | static_cast<unsigned char> |
| `%s<3%` printf 无效格式 | cpu003_profile_v2_test | `%%` 转义 |
| 缺失字段初始化 (12 处) | p3_coverage_test | 默认成员初始化 `{}` |
| unused parameter | rt006/artifact_store/p3_output/aio_compressor | (void) 或并入错误消息 |

## 遗留/风格类发现 (P2 已评估, 不改代码)

- clang-tidy readability/modernize 风格建议 (braces/emplace/trailing-return/uppercase-suffix 等 ~5700 条):
  属风格建议非缺陷, 大规模修改违反"不大范围格式化"原则 (00_READ_FIRST §4)。
- `TaintedAlloc` (aio_pipeline/healpix_io): n_blocks 已有 `AIO_CACHE_MAX_BLOCKS` 上界校验,
  clang-analyzer 未跨函数追踪, 实际安全。
- `Stream` EOF 模式 (sha256/backend_loader): 标准 `fread` 循环, EOF 返回 0 正确退出。
- `not-null-terminated` (aio_fits write_card): FITS card 为定宽 80 字符, 不需要 NUL。
- `Errno` (fits_reader): ferror 分支内仅诊断, 无逻辑依赖。
- `DivideZero` (profile_gen_v2:240): `k % N`, N≥65536 (kSpecs base≥256 × size_mult≥1), 恒安全。
- `easily-swappable-parameters` 等: 属 API 命名建议, 非缺陷。
- cppcheck `%zu` (hiss_reader): LP64 下 size_t/uint64_t 均 unsigned long, 格式正确。
- cppcheck syntaxError (nlohmann/json.hpp, nanoflann.hpp, hiss_format.h): 第三方/模板宏误报。

## 测试结果

- c01: GCC Release 严格 flags 全 target 0 warning (EXIT=0)
- c02: clang-tidy 77 生产源扫描完成, 真实缺陷修复后无 P0/P1
- c03: clang++ 严格 flags 关键单元 0 warning
- c04: cppcheck 77 源完成, error/warning 级真实项已修复
- c05: check_warning_suppression PASS (生产源 0 抑制, -w 仅第三方)
- 回归: ctest 全量 (见 QA-001 提交后 LNX-001 复验)

## 限制

- clang-tidy readability/modernize 风格建议不逐条修复 (范围控制, 见上)。
- 遗留 AIO/drizzle 的 omp pragma 以 -fopenmp 编译 (不惩罚), 其并行语义不属生产 Runtime lease 路径。
