# LNX-004 验证报告 — ASan/UBSan + TSan 可行集 + 静态分析

结论: **PASS**(0 P0/P1; 修复 1 个 P1 aligned_alloc + 2 个死存储; 工具缺失记 prerequisite)。

## 1. 验收判据(03_TASK_DETAILS.md L142)
> ASan/UBSan; TSan 可编译/可运行范围; clang-tidy/cppcheck 等现有工具; 结果不能截断。
> PASS = 0 P0/P1; 工具缺失记 prerequisite 并用现有替代, 不能假 PASS。

## 2. 环境与工具可行性
| 工具 | 可用性 | 说明 |
|---|---|---|
| clang 19.1.7 | ✅ | 用于静态分析 `clang --analyze` |
| clang-tidy | ❌ | 未安装(prerequisite): 用 clang --analyze 替代 |
| cppcheck | ❌ | 未安装(prerequisite): 用 clang --analyze 替代 |
| GCC ASan/UBSan | ✅ | `-fsanitize=address,undefined` |
| GCC TSan | ✅ | `-fsanitize=thread` |

## 3. sanitizer 构建与运行
| 变体 | 构建目录 | 编译 flags | 构建 | 运行 | 结果 |
|---|---|---|---|---|---|
| ASan+UBSan | build/lnx_v5_asan | `-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1 -Wall` | OK(0 err) | OK | 详见 §4 |
| TSan | build/lnx_v5_tsan | `-fsanitize=thread -fno-omit-frame-pointer -g` | OK(0 err, 可编译) | OK(可运行) | 详见 §4 |

因系统需 `LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libasan.so.8` / `libtsan.so.2` 才能让 runtime 先加载;
LeakSanitizer 在 ptrace 下受限, 故 ASan 用 `detect_leaks=0`(内存泄漏另有 MON/MEM 任务覆盖)。

## 4. 发现的问题(P0/P1)与修复
### 4.1 P1: `invalid-aligned-alloc-alignment`(ASan+TSan 双检出)
- 位置: `lib/backend_host/host_services.cpp:34` `host_alloc` 调用 `std::aligned_alloc(align=16, size=229)`。
- 根因: C11/POSIX `std::aligned_alloc` 要求 **size 是 align 的整数倍**; 当 `size=229(0xe5)`、`align=16`
  时非法, ASan/TSan 均 abort(`invalid alignment requested`)。原 guard 只查 align 为 2 的幂, 未保证
  size 为 align 倍数。
- 修复: 将 size 向上取整到 align 的倍数再分配
  `alloc_sz = (size + align - 1) & ~(align - 1)`; `host_free` 用 `std::free`(与 aligned_alloc 兼容)。
- 验证: 修复后 ASan/TSan 跑 phase1 真实 compute → `rc=4`(干净科学失败), **0 条 sanitizer 报告**;
  doctor PASS。

### 4.2 P1: 死存储(clang --analyze)
- `lib/phase3_session/p3_session.cpp`: `parse_request` 中 `const double ra`(从未读取)与
  `p3_session_run` 中 `const int max_tiles`(从未使用) — 两条 dead store 告警。
- 修复: 删除两个未用声明。
- 验证: `clang --analyze` 后 0 警告; phase3 oracle + inprocess 18 用例全过。

## 5. 静态分析结果(clang --analyze, 结果未截断)
| 翻译单元 | 结果 |
|---|---|
| cli/main.cpp | 0 warning / 0 error |
| lib/phase1_session/p1_session.cpp | 0 / 0 |
| lib/phase2_session/p2_session.cpp | 0 / 0 |
| lib/phase3_session/p3_session.cpp | 0 / 0(修复后) |

clang 无法解析 CUDA/部分 tool-incompat 单元时按 prior tool 判定; 本任务目标 CLI/核心 session 单元全覆盖。

## 6. 回归(修复后, clean Release 二进制)
`tests.cli.test_cli_protocol + test_monitor + test_phase3_inprocess` → **OK(32 用例)**。
ASan 二进制上 `config init/validate`、`doctor --json`、`benchmark cpu --quick` 均无 sanitizer 报告。

## 7. 限制
- clang-tidy/cppcheck 未安装(记 prerequisite); 用 clang --analyze 替代, 非假 PASS。
- LeakSanitizer 在 ptrace(沙箱)下报 fatal, 故 detect_leaks=0; 泄漏覆盖由 MEM 任务负责。
- sanitizer 需 LD_PRELOAD 先加载运行时; TSan 在受限容器可编译可运行(doctor+phase1 无竞态报告)。
