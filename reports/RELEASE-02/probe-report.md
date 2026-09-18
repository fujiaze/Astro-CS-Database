# RELEASE-02 性能探针框架报告（PROBE）

- 任务：负责人设计意图「Python 系统利用率统计 + 程序内探针，共用一个开关，对性能只有微量影响，不用时在 cmake 里关掉」
- 范围：新增 C++ 探针框架 + 埋点 + Python 系统监视器；**只观测，不改科学行为**
- 约束遵守：未运行 `ninja`/`cmake`/`ctest`（仅 `g++ -fsyntax-only` / 单文件编译）；未改 `docs/**`、`tests/**`、`lib/algorithms/coverage/{include,src}/rejection.*`；**未改 `module_adapters.cpp`**（该文件探针以插入清单交付，见 §5 与 `run/RELEASE-02/probe/module_adapters_probe_insertions.md`）
- 新增：`lib/infrastructure/observability/probes/{include/astrocs/probe.h,src/probe.cpp,README.md}`、`tools/l4_rebuild/sysmon.py`、`run/RELEASE-02/probe/{module_adapters_probe_insertions.json,module_adapters_probe_insertions.md,apply_insertions.py}`

---

## 1. 架构

### 1.1 单一开关，两层

| 层 | 开关 | 默认 | 语义 |
|----|------|------|------|
| 编译期 | CMake option `ASTROCS_PROBES` | **OFF** | OFF：`astrocs/probe.h` 的宏全部展开为 `((void)0)`；不编译 `probe.cpp`；无任何探针符号 |
| 运行期 | 环境变量 `ASTROCS_PROBE_LOG=<path>` | 未设 | 未设：即使编译期 ON 也不计时、不分配、不写文件（仅一次 `getenv`） |

可选调优：`ASTROCS_PROBE_FLUSH_LINES`（默认 1024）、`ASTROCS_PROBE_FLUSH_MS`（默认 1000）。

### 1.2 构建接线（`CMakeLists.txt`）

- `option(ASTROCS_PROBES ... OFF)` 加在选项区（`CMakeLists.txt:23-26`）；
- 在 `find_package(Threads REQUIRED)` 之后（`CMakeLists.txt:301-321`）：
  - `ASTROCS_PROBES=ON` → `add_library(astrocs_probes STATIC src/probe.cpp)`，PUBLIC include 面 + PUBLIC `ASTROCS_PROBES=1` + 链接 `Threads::Threads`；
  - `ASTROCS_PROBES=OFF` → `add_library(astrocs_probes INTERFACE)`，**只登记 include 面**，不编译实现、不产生符号。
- 被埋点 target 链接 `astrocs_probes`：`astrocs_phase2`、`astrocs_phase1_session`、`astrocs_phase2_session`、`astrocs_module_adapters`（含为插入清单预接线）。
- 兼容（独立 configure）构建：`lib/algorithms/coverage/CMakeLists.txt` 的 `phase2` target 显式登记同一 include 面（未定义 `ASTROCS_PROBES` 时宏为空语句）。

### 1.3 低开销实现（`probe.cpp`）

- 计时统一 `std::chrono::steady_clock`；
- **热路径无 I/O**：线程本地 `std::string` 行缓冲 + 线程本地 `count`/`gauge` 聚合（`unordered_map`），记录路径**无锁**；
- 按**行数（默认 1024）或时间（默认 1000ms）**批量 flush；仅 flush 点取一把 `std::mutex` 写文件并 `fflush`；
- 线程退出自动 flush（`thread_local` 析构）；进程 `atexit` 兜底；sink 与配置为泄漏/平凡单例，避免静态析构顺序问题；
- 未启用时构造 `ScopeTimer` 不调用 `now()`，析构立即返回；
- 标签最多 8 条，字符串标签按值保存（无悬垂）；`int/long/size_t` 等算术类型走单一模板避免二义。

### 1.4 输出格式（JSONL）

每行至少 `{ts_utc, thread_id, scope, name, wall_us}`；计数追加 `count`；取值追加 `last/min/max/n`；`key=value` 标签成为顶层字段：

```json
{"ts_utc":"2026-09-18T14:36:23.374Z","thread_id":1,"scope":"phase1","name":"calibrate.frame","wall_us":266,"frame_key":"m42_0001","n":42}
{"ts_utc":"2026-09-18T14:36:23.375Z","thread_id":1,"scope":"phase1","name":"drizzle.frames","wall_us":0,"count":3}
{"ts_utc":"2026-09-18T14:36:23.375Z","thread_id":1,"scope":"phase1","name":"drizzle.pixels","wall_us":0,"last":2097152,"min":1048576,"max":2097152,"n":2}
```

### 1.5 API

```cpp
#include "astrocs/probe.h"
ASTROCS_PROBE_SCOPE("phase1", "drizzle.frame");       // RAII 作用域计时
ASTROCS_PROBE_SCOPE("phase1.drizzle.frame");          // 单参数：scope=首个 '.' 前
ASTROCS_PROBE_SCOPE_CTX(t, "phase1", "calibrate.frame");
ASTROCS_PROBE_TAG(t, "frame_key", key.c_str());
ASTROCS_PROBE_SCOPE_END(t);                           // 提前结束（阶段边界用）
ASTROCS_PROBE_COUNT("phase1", "drizzle.frames", 1);
ASTROCS_PROBE_GAUGE("phase1", "drizzle.pixels", 1048576.0);
ASTROCS_PROBE_FLUSH();
```

---

## 2. 开关用法

```bash
# 关闭（默认）：既有构建/测试行为完全不变
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# 打开：编译探针实现并把 ASTROCS_PROBES=1 传给被埋点 target
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DASTROCS_PROBES=ON
ninja -C build

# 运行期产出（不设 ASTROCS_PROBE_LOG 则无任何产出）
ASTROCS_PROBE_LOG=run/RELEASE-02/probe/probe.jsonl build/astrocs normalize --json cfg.json -y
```

---

## 3. OFF 零开销证据

命令与结果（`g++` 直接编译被埋点 TU，不经过 cmake/ninja）：

### 3.1 `nm`：OFF 无探针符号，ON 有

```
$ g++ -std=c++17 -O2 -c ... lib/phase1_session/p1_session.cpp -o p1_off.o          # 无 -DASTROCS_PROBES
$ nm -C p1_off.o | grep -c 'astrocs::probe'      -> 0
$ g++ -std=c++17 -O2 -DASTROCS_PROBES=1 -c ... lib/phase1_session/p1_session.cpp -o p1_on.o
$ nm -C p1_on.o  | grep -c 'astrocs::probe'      -> 5

$ g++ -std=c++17 -O2 -c ... lib/algorithms/coverage/src/sky_plane.cpp -o sky_off.o
$ nm -C sky_off.o | grep -c 'astrocs::probe'     -> 0
$ g++ -std=c++17 -O2 -DASTROCS_PROBES=1 -c ... lib/algorithms/coverage/src/sky_plane.cpp -o sky_on.o
$ nm -C sky_on.o  | grep -c 'astrocs::probe'     -> 3
```

ON 对象里出现的是**未定义引用**（由 `astrocs_probes` 静态库解析），OFF 对象里一条都没有：

```
$ nm -C p1_on.o | grep 'astrocs::probe' | head -4
                 U astrocs::probe::ScopeTimer::ScopeTimer(char const*, char const*)
                 U astrocs::probe::ScopeTimer::~ScopeTimer()
                 U astrocs::probe::ScopeTimer::tag(char const*, char const*)
                 U astrocs::probe::count_add(char const*, char const*, long long)
```

### 3.2 现有默认构建产物无探针符号

```
$ nm -C build/astrocs | grep -c 'astrocs::probe'   -> 0
```

### 3.3 预处理：OFF 头文件不引入任何探针符号

```
$ echo '#include "astrocs/probe.h"' | g++ -std=c++17 -E -I<probes>/include -x c++ - | grep -c 'astrocs::probe'
0
```

### 3.4 宏展开为空语句

`ASTROCS_PROBE_SCOPE` / `_CTX` / `_TAG` / `_END` / `_COUNT` / `_GAUGE` / `_FLUSH` 在 OFF 时全部为 `((void)0)`；`ASTROCS_PROBE_ENABLED()` 为 `(false)`。因此 OFF 时**不声明变量、不计时、不分配、无分支**。

### 3.5 语法与告警

被埋点 TU 在 OFF/ON 两种模式下用生产告警集（`-Wall -Wextra -Wpedantic -Wconversion`）`-fsyntax-only` 全部通过：

| TU | OFF | ON |
|----|-----|----|
| `lib/phase1_session/p1_session.cpp` | OK | OK |
| `lib/phase2_session/p2_session.cpp` | OK | OK |
| `lib/algorithms/coverage/src/sky_plane.cpp` | OK | OK |
| `module_adapters.cpp`（把插入清单应用到 /dev/shm 副本后） | OK | OK（仅 1 条既有告警 `3829 for (const std::string prod :`，与探针无关） |

### 3.6 运行时功能自测

独立小程序（多线程 + 标签 + count/gauge）链接 `probe.cpp` 运行：产出 11 行合法 JSONL（`python3 json.loads` 逐行通过），未设 `ASTROCS_PROBE_LOG` 时不产出任何文件。

---

## 4. 已落地埋点清单（file:line）

> 这些是**非 `module_adapters.cpp`** 的埋点，已直接改好。`file:line` 为改动后行号。

| # | 文件:行 | scope / name | 类型 | 说明 |
|---|---------|--------------|------|------|
| 1 | `lib/phase1_session/p1_session.cpp:318` | `phase1 / calibrate.stage` | scope | Phase1 阶段边界 calibrate（覆盖全部 return） |
| 2 | `lib/phase1_session/p1_session.cpp:349` | `phase1 / calibrate.frame` | scope + tag `frame_key` | calibrate 逐帧 |
| 3 | `lib/phase1_session/p1_session.cpp:365` | `phase1 / calibrate.frame_pixels` | gauge | 每帧像素数（规模） |
| 4 | `lib/phase1_session/p1_session.cpp:396` | `phase1 / calibrate.frames` | count | 完成帧数 |
| 5 | `lib/phase1_session/p1_session.cpp:430` | `phase1 / cosmetic.stage` | scope | Phase1 阶段边界 cosmetic |
| 6 | `lib/phase1_session/p1_session.cpp:451` | `phase1 / cosmetic.frame` | scope + tag `frame_key` | cosmetic 逐帧 |
| 7 | `lib/phase1_session/p1_session.cpp:475` | `phase1 / cosmetic.frames` | count | 修复帧数 |
| 8 | `lib/phase2_session/p2_session.cpp:124/156` | `phase2 / coverage.stage` | scope | Phase2 阶段边界 coverage |
| 9 | `lib/phase2_session/p2_session.cpp:154-155` | `phase2 / coverage.union_cells`、`coverage.inputs` | gauge | 规模 |
| 10 | `lib/phase2_session/p2_session.cpp:162/192` | `phase2 / sample.stage` | scope | Phase2 阶段边界 sample |
| 11 | `lib/phase2_session/p2_session.cpp:190-191` | `phase2 / sample.n_obs`、`sample.n_controls` | gauge | 样本数 |
| 12 | `lib/phase2_session/p2_session.cpp:197/236` | `phase2 / upm_build.stage` | scope | Phase2 阶段边界 upm_build |
| 13 | `lib/phase2_session/p2_session.cpp:246/259` | `phase2 / persist.stage` | scope | Phase2 阶段边界 persist |
| 14 | `lib/algorithms/coverage/src/sky_plane.cpp:420-421` | `phase2 / sky_plane.build` + `sky_plane.build_samples` | scope + gauge | 天光面构建（整面一次；RAII 覆盖所有 return） |
| 15 | `lib/algorithms/coverage/src/sky_plane.cpp:1004-1005` | `phase2 / sky_plane.eval_block` + `sky_plane.eval_points` | scope + gauge | 天光面应用（逐 tile 块，非逐像素） |

---

## 5. `module_adapters.cpp` 插入清单（未改文件，交前台统一应用）

⚠️ 按前台纠正，**未修改** `lib/infrastructure/scheduler/src/module_adapters.cpp`（HUB-C 正在改）。
清单见 `run/RELEASE-02/probe/module_adapters_probe_insertions.md`（人读）+ `.json`（机读）。
应用器：`python3 run/RELEASE-02/probe/apply_insertions.py [--apply] [--skip-optional]`
—— 按**精确锚点文本**定位（抗行号漂移），锚点非唯一即报错且不写文件，已应用过则跳过。

覆盖（11 项，1 项可选）：

| id | 锚点行（HEAD） | 内容 |
|----|---------------|------|
| `P1-include` | 79 | `#include "astrocs/probe.h"` |
| `P1-stage-switch` | 6126-6135 | **Phase1 七阶段边界**：`calibrate/cosmetic/star_psf/wcs/photometry/noise/drizzle/writer`（每 case 一个 RAII 作用域） |
| `P2-stage-switch` | 6317-6325 | **Phase2 七阶段边界**：`coverage/sample/upm_fit/upm_apply/reject/integrate/write` |
| `P1-calibrate-frame` | 1699-1700 | **calibrate 逐帧** scope + `frame_key` |
| `P1-wcs-frame` | 2763-2769 | **wcs 逐帧** scope + `frame_key` |
| `P1-drizzle-frame` | 3479-3483 | **drizzle 逐帧** scope + `frame_key` |
| `P2-reject-tile` | 4977-4978 | **reject 逐 tile** scope + `tile_id` + gauge `reject.tile_frames` |
| `P2-reject-union-gauge` | 4953-4956 | gauge `reject.union_tiles`（tile 数） |
| `P2-integrate-tile` | 5477-5479 | **integrate 逐 tile** scope + `tile_id` |
| `P2-integrate-tiles-gauge` | 5474-5477 | gauge `integrate.tiles` |
| `P2-skyplane-apply-tile` | 4676-4678 | （可选）upm_apply 逐 tile 的 sky_plane 应用上下文；库层 `sky_plane.eval_block` 已计时，此项仅补 `tile_id` |

已验证：把清单应用到 `module_adapters.cpp` 的 **/dev/shm 副本**后，OFF/ON 两种模式均 `g++ -fsyntax-only` 通过（见 §3.5），且 11 个锚点在当前文件上均**唯一匹配**（dry-run `would apply=11, failed=0`）。**仓库内文件未被改动**。

sky_plane 构建/应用**已在库层埋点**（§4 #14/#15），插入清单中不重复计时。

---

## 6. Python 系统监视器 `tools/l4_rebuild/sysmon.py`

与程序同步启动、按固定间隔采样、写 CSV；数据源只用 `/proc`，**无第三方依赖**（无 psutil 亦可）。

### 用法

```bash
# 跟踪目标进程（含全部后代）
python3 tools/l4_rebuild/sysmon.py --pid 1234 --interval 1 --out run/x/sysmon.csv
# 只看整机
python3 tools/l4_rebuild/sysmon.py --out sysmon.csv --duration 60
# 后台启动 + 结束收尾（run_timed.sh 即此模式）
python3 tools/l4_rebuild/sysmon.py --pid $$ --interval 1 --out sysmon.csv & SYSMON=$!
... 运行被测程序 ...
kill -TERM "$SYSMON"; wait "$SYSMON"
```

参数：`--out`（必填）、`--pid`（可重复/逗号分隔，跟踪进程树）、`--interval`（默认 1.0s）、
`--duration`（0=直到信号）、`--tag`、`--quiet`。`SIGINT/SIGTERM` 收尾后正常退出（rc=0）。

### 采样列（35 列）

整机 CPU% + 每核 CPU%；load1/5/15 + runnable + n_tasks + n_cpus；MemTotal/Available/Used；
SwapTotal/Free/Used + swap 换入/换出 KB；磁盘读/写 KB、IOPS、IO 占用%（仅整盘 `sd*/vd*/nvme*/hd*/mmcblk*`，避免分区/loop/dm 重复计数）；
系统 iowait%、上下文切换；进程树（`--pid` 及后代）CPU%、RSS/PSS/swap/线程数/上下文切换/读写字节。

### `run_timed.sh` 集成（已改，`tools/l4_rebuild/run_timed.sh:23-44`）

- 在 `LOGS` 建好后后台启动 `sysmon.py --pid $$ --out $LOGS/sysmon.csv`（跟踪 runner 整个进程树 ⇒ 覆盖每一步 astrocs 子进程）；
- `trap sysmon_stop EXIT`（+ INT/TERM → exit 130/143）保证正常/异常/闸门 `exit 2` 都收尾；
- `ASTROCS_SYSMON=0` 可关闭；`ASTROCS_SYSMON_INTERVAL` 覆盖间隔；`sysmon.py` 不存在时静默跳过；
- 结尾打印 `sysmon csv = ...`。**未改动 run_timed.sh 之外的既有工具**。

### 自测

对 1 个 busy-loop 子进程采样：`proc_cpu_pct ≈ 100.00`、对应单核 `100.0`、磁盘写 KB/IOPS、swap、load 均正常；trap 生命周期测试确认退出后 sysmon 进程不残留、CSV 行数正确。

---

## 7. 影响评估

- **科学中立**：探针只读时间/计数/规模，不改变任何输入输出、控制流、线程数、block、ISA；所有埋点均在 `ASTROCS_PROBES=OFF` 时为空语句。
- **OFF 构建与测试行为不变**：默认 OFF；`astrocs_probes` 为 INTERFACE（仅 include 面）；被埋点 TU 的 OFF 对象无探针符号/字符串（§3.1）。既有构建图仅新增一条无害的 include 路径与一个 INTERFACE 依赖。
- **ON 运行期影响**：热路径（逐帧/逐 tile）每次作用域约 `2×steady_clock::now()` + 一次 `snprintf`/字符串追加到线程本地缓冲；无锁、无逐次 I/O；每 1024 行或 1s 才 flush 一次。计数/取值探针为线程本地 `unordered_map` 更新（`std::string` 键，逐帧/逐 tile 粒度，非逐像素）。逐像素路径**未埋点**（`p2_integrate_pixel`、`p2_sky_plane_eval` 单点均未插）。
- **可关闭性**：编译期 OFF 即彻底移除（零符号）；运行期不设 `ASTROCS_PROBE_LOG` 即零产出。

---

## 8. 未闭合项 / 交前台

1. **ON 全量构建与端到端运行验证**：受「不跑 ninja/cmake/ctest」约束，未做 `-DASTROCS_PROBES=ON` 的真实构建；已完成单文件编译 + 独立运行自测。建议前台在一次 ON 构建后跑一小段 normalize/mosaic 并确认 JSONL 产出（`ASTROCS_PROBE_LOG=...`）。
2. **`module_adapters.cpp` 插入清单待应用**：等 HUB-C 落地后由前台运行 `apply_insertions.py --apply`（先 dry-run），再统一构建；锚点若因 HUB-C 改动失配，脚本会明确报出是哪一项。
3. **`sysmon.py` 首次运行列语义**：首样本无前值，swap/磁盘/进程 IO 增量按 0 处理（已实现），CPU% 首样本为 0；聚合时建议丢弃 `seq==1`。
4. **PSS 采集**：`/proc/<pid>/smaps_rollup` 在受限容器/旧内核可能不可读，此时 `proc_pss_kb` 退化为已读进程之和（不报错）；如需严格可加显式告警列。
5. **未做**：探针输出的聚合/可视化脚本（JSONL → 热点排序）。当前由 `hotspots.py`/人工分析消费；如需可与 `stage_profile.py` 对齐另开任务。
