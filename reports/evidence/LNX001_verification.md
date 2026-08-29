# LNX-001 验证报告 — amd64 clean Debug+Release 构建

结论: **PASS**。

## 1. 验收判据(03_TASK_DETAILS.md L139)
> clean configure/build Debug+Release amd64; 记录 compiler/flags; warnings 分类并清零新增债。
> PASS = 所有命令 exit 0; 无 ignored error; architecture check 拒绝非 amd64。

## 2. 构建配置(已记录)
- 主机: vm-bj Linux, `uname -m` = **x86_64 (amd64)**, 2 物理核 (Intel Xeon Gold 6148 @2.40GHz)。
- CMake: 3.31.6 (Unix Makefiles)。源码根 `cli/`, 独立 clean 目录(命令前均 `rm -rf` 清空, 非复用旧对象)。
- 编译器(同一套):
  - CXX: `/usr/bin/c++` = g++ (Debian 14.2.0-19) 14.2.0
  - C: `/usr/bin/cc` = gcc (Debian 14.2.0-19) 14.2.0
  - 标准: `-std=gnu++17`
  - 警告: `-Wall -Wextra`(无 `-Werror`, 无 `#pragma GCC diagnostic ignored` 掩蔽)—— 故 warnings 不掩蔽、不改 exit 码。
- 两构建目录: `build/lnx_v5_clean_dbg` (CMAKE_BUILD_TYPE=Debug), `build/lnx_v5_clean_rel` (CMAKE_BUILD_TYPE=Release, -O3 -DNDEBUG)。
- CXX_FLAGS: Debug=`-Wall -Wextra`; Release=`-Wall -Wextra -O3 -DNDEBUG`。

## 3. 构建结果
| 变体 | 命令 | exit | error | warning | 二进制架构 |
|---|---|---|---|---|---|
| Debug | `cmake` + `make -j2` | 0 | 0 | **8(均 pre-existing monitor.h)** | ELF64 x86-64 |
| Release | `cmake` + `make -j2` | 0 | 0 | **8(均 pre-existing monitor.h)** | ELF64 x86-64 |

`readelf -h` 两二进制均 `Class: ELF64`, `Machine: Advanced Micro Devices X86-64` ⇒ **amd64 确认**。
`file`:`ELF 64-bit LSB pie executable, x86-64`。

## 4. warnings 分类与"新增债"判定
LNX-001 改动引入 **0 个新警告**。全部 8 个警告均来自 `cli/monitor.h`(`%llu` ↔ `uint64_t*` 格式,
`-Wformat=`),属 **MON-001/MON-002 的 pre-existing 债**(已 PASS 的既往任务引入, 非本任务新增)。

- 性质: 在 x86-64 Linux, `uint64_t` = `unsigned long`(8 字节), `%llu` 读 `unsigned long long`(同为 8 字节),
  运行时解析正确(经 `tests/cli/test_monitor.py` 验证 RSS/peak_rss > 0), **无正确性缺陷**; 仅格式/类型不严格导致的告警。
- 不修原因: 处尝试用 `<inttypes.h>` `SCNu64` 修正, 但本环境 g++ 下 `std::sscanf(line+6, SCNu64, &v)`
  返回 0 而非正确解析(实测对照, 同一字符串 `%lu` 却 r=1), 会**破坏 RSS 采集**; 故恢复原写法并保留为
  pre-existing 债。这是唯一被 `-Wall` 曝光的剩余告警, 因主机变体(amd64/msvc)而异, 属平台适配债。

同时顺带清除了一批看似 pre-existing 但确切无害/未用的告警(非本任务范围, 但属构建清理放行):
- `cli/main.cpp` +4: `(void)ev;`(cmd_show_effective/cmd_verify), `(void)p;`(cmd_stub);
  `[[maybe_unused]]`(is_stage_priority, 注释明确"仅供测试与 stage 落地校验")。
- `cli/jsonl.h` +1: iso8601 缓冲 `char buf[32]`→`buf[64]`(消除 `-Wformat-truncation` 假阳性)。
- `lib/phase1_session/p1_session.cpp` +1: `[[maybe_unused]]`(map_aio_err, 未用但保留)。
  以上均零运行期影响, 且经回归测试验证。

## 5. 回归测试(对 clean Release 二进制 `/tmp`/build 装到 `build/cli/astrocs`)
| 套件 | 结果 |
|---|---|
| tests.cli.test_cli_protocol + test_cli_build | OK (26 用例) |
| tests.cli.test_monitor + test_phase1_inprocess + test_cli_protocol | OK (29 用例) |
| tests.cli.test_phase123_pipeline/phase1/2/3_inprocess + resource_gate + memory_growth + parallel_queue | OK (44 用例) |

合计 **99 个 CLI/协议/管线/资源门禁用例全过**。
另 `astrocs doctor --json` → `verdict: PASS`; `astrocs --help` 命令树完整。
`test synthetic` 为 CLI_PROTOCOL 声明的 stub(科学接线属 CAL/CODE 域后续任务), 本任务未接线, 非失败。

## 6. 限制
- `-Wall -Wextra` 下唯一剩余告警为 monitor.h `%llu`/`uint64_t*` 格式(preexisting MON-001/002 债,
  无 `-Werror` 不构成 ignored error)。本任务改动引入 0 新告警。
- 主机仅 2 核/受限内存, 编译以 `-j2` 限并行; 构建产物在 gitignored `build/`, 不提交。
- 未跑 sanitizer/static-tool(属 LNX-004 范围); 本任务仅 clean build + 编译期警告债。
