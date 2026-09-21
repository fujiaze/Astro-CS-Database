# 性能探针框架 (RELEASE-02)

单一开关、两层控制，关闭时零开销，开启时线程本地批量写 JSONL。

| 层 | 开关 | 默认 | 作用 |
|----|------|------|------|
| 编译期 | CMake `-DASTROCS_PROBES=ON` | **OFF** | OFF：宏展开为 `((void)0)`，不编译 `src/probe.cpp`，产物无 `astrocs::probe::*` 符号 |
| 运行期 | 环境变量 `ASTROCS_PROBE_LOG=<path>` | 未设 | 未设：即使编译期 ON 也不计时、不分配、不写文件 |

可选调优：`ASTROCS_PROBE_FLUSH_LINES`（默认 1024 行）、`ASTROCS_PROBE_FLUSH_MS`（默认 1000ms）。

## 用法

```bash
# 1) 编译期打开探针（默认 OFF 时以下全部为无副作用的空语句）
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DASTROCS_PROBES=ON
ninja -C build

# 2) 运行期打开输出（不设则不产出）
ASTROCS_PROBE_LOG=run/RELEASE-02/probe/probe.jsonl build/astrocs normalize --json cfg.json -y
```

## API

```cpp
#include "astrocs/probe.h"

// 作用域计时（RAII）
ASTROCS_PROBE_SCOPE("phase1", "drizzle.frame");
ASTROCS_PROBE_SCOPE("phase1.drizzle.frame");          // 单参数：scope=首个 '.' 前

// 带上下文标签
ASTROCS_PROBE_SCOPE_CTX(t, "phase1", "calibrate.frame");
ASTROCS_PROBE_TAG(t, "frame_key", key.c_str());
ASTROCS_PROBE_SCOPE_END(t);                            // 可选：提前结束

// 计数 / 取值（线程本地聚合，flush 时输出一行）
ASTROCS_PROBE_COUNT("phase1", "drizzle.frames", 1);
ASTROCS_PROBE_GAUGE("phase1", "drizzle.pixels", 1048576.0);

ASTROCS_PROBE_FLUSH();                                 // 手动 flush（可选）
```

## 输出（JSONL）

每行至少 `{ts_utc, thread_id, scope, name, wall_us}`；计数追加 `count`，取值追加
`last/min/max/n`，标签成为顶层字段。示例：

```json
{"ts_utc":"2026-09-18T14:36:23.374Z","thread_id":1,"scope":"phase1","name":"calibrate.frame","wall_us":266,"frame_key":"m42_0001","n":42}
{"ts_utc":"2026-09-18T14:36:23.375Z","thread_id":1,"scope":"phase1","name":"drizzle.frames","wall_us":0,"count":3}
```

## 开销

- 记录路径：线程本地 `std::string` 行缓冲 + 线程本地 count/gauge 聚合，**无锁**；
- 仅 flush 点取一把互斥锁写文件并 `fflush`；
- 线程退出自动 flush；进程 `atexit` 兜底；
- `ASTROCS_PROBES=OFF`：零计时、零分配、零符号。

## 系统监视器

`eng/tools/l4_rebuild/sysmon.py` 与程序同步启动，从 `/proc` 采样整机/进程树利用率写 CSV，
无第三方依赖。见 `reports/RELEASE-02/probe-report.md`。
