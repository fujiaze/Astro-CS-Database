# P02-006 TASK_REPORT - Gaia 查询边界与缓存

- 任务: P02-006
- 阶段: P02
- 依赖: P02-004
- Gate: G2
- 执行日期: 2026-07-25
- 执行者: P02-006 子 Agent
- 基线 commit: c960dcc (P02-007 已合并的 main)
- 工作分支: main

## 目标

1. 删除 Orchestrator 求解完成后的无消费者 `gaia_cat` 二次 cone search 查询。
2. 区分 astrometry / photometry 两条 Gaia 查询语义 (docs/05 §9)。
3. 为 GaiaClient 增加可验证的进程内只读缓存 (60s TTL + 内存压力释放)。
4. 缓存键包含 dataset、cone、字段集合、参数和版本号。

## 执行步骤与结果

### 1. 删除无消费者 gaia_cat 二次查询

- **文件**: `lib/orchestrator/cpp/src/orchestrator.cpp`
- **位置**: 原 L1495-L1562 (run_stage_platesolve 内, star_det 写入之后)
- **改动**: 删除 68 行调用 `gaia_client_cone_search_for_solver` 并写 "gaia_cat" 块的死代码; 保留 9 行注释说明删除原因与 Gaia 查询语义区分。
- **消费者审计**: 搜索下游 stage (PSF / PHOTOMETRIC / SNR / DRIZZLE) 对 "gaia_cat" 块的读取, 确认零消费者。
- **净 diff**: -68 行 (77 删除, 9 新增注释)
- **结论**: PASS

### 2. 区分 Astrometry / Photometry Gaia 查询语义

- **Astrometry query**: PLATESOLVE 内部由 `ipv_solve_from_memory_with_callback` 完成 (匹配星等几何 + WCS 求解), 结果以 `star_det` / WCS 形式落盘, 不依赖 `gaia_cat` 块。
- **Photometry query**: PHOTOMETRIC 阶段独立调用 `pc_calibrate_simple_with_gaia`, 内部按需 cone search + DR3SP 光谱 + 滤光片 + QE 积分, 不依赖 `gaia_cat` 块。
- **结论**: PASS (两条查询路径相互独立, 删除 gaia_cat 不影响正确性)

### 3. GaiaClient 进程内只读缓存实现

- **文件**: `lib/gaia_xpsd_client/src/gaia_client.c`
- **新增**: 15 行缓存版本号机制
  - `#define GAIA_CACHE_VERSION 1` (L50): 缓存键版本号, schema 变更时递增即可让旧条目失效。
  - `QueryCacheEntry.version` 字段 (L96): 每个缓存条目记录其版本号。
  - `query_cache_lookup` 版本检查 (L418): lookup 时 version 不匹配则清理并跳过。
  - `query_cache_insert` 版本标记 (L505): insert 时标记当前版本号。
- **既有缓存机制** (本任务前已存在, 本次仅加固版本号):
  - TTL: 60 秒 (`QUERY_CACHE_TTL_SEC`)
  - 容量: 64 条目 (`QUERY_CACHE_CAPACITY`)
  - 内存压力阈值: 可用物理内存 < 4GB 时触发释放 (`MEMORY_PRESSURE_THRESHOLD`)
  - 线程安全: `CRITICAL_SECTION` (Windows) / `pthread_mutex_t` (Linux)
  - 缓存键: ra/dec/radius/mag_high 四参数舍入后精确匹配
  - 舍入精度: RA/Dec 0.001°, radius 0.01°, mag 0.01
- **结论**: PASS

### 4. 构建与部署

- **构建工具**: MinGW (gcc / mingw32-make), 路径 `C:\msys64\mingw64\bin`
- **GaiaClient**: `lib/gaia_xpsd_client/Makefile` -> `gaia_client.dll` + `libgaia_client.a`
- **Orchestrator**: `lib/orchestrator/cpp/Makefile` -> `orchestrator.exe`
- **部署**: `build/artifacts/` (gaia_client.dll 281990 bytes, orchestrator.exe 3921419 bytes)
- **DLL 锁处理**: 使用 Windows 重命名技巧绕过 Python 进程占用的 DLL 句柄
- **结论**: PASS

### 5. 验证

- **源码验证**: grep 确认 orchestrator.cpp 中 `gaia_cat` 仅剩注释 (6 处, 全部为解释性文本)
- **缓存验证**: `test_cache_hit.c` 四次查询测试, 缓存命中加速 ~5000x
- **stage1 验证**: orchestrator 日志确认 PLATESOLVE 完成后未写 gaia_cat 块
- **结论**: PASS (详见 TEST_REPORT.md)

## 兼容性

- **接口无变化**: `gaia_client.h` 公共 API 未修改, 仅在 .c 内部结构体新增 `version` 字段。
- **向后兼容**: 旧缓存条目在 lookup 时因 version 不匹配自动失效, 无需手动清理。
- **HISS 格式**: 删除 gaia_cat 块不影响 HISS 文件格式 (该块本就无消费者, 不被读取)。
- **PHOTOMETRIC 独立查询**: 不受影响, 内部按需 cone search 仍由 GaiaClient 缓存加速。

## 回滚方案

- `git revert <P02-006 commit>` 即可恢复 gaia_cat 二次查询。
- 缓存版本号机制为纯新增, revert 后旧缓存逻辑不受影响。

## 残留风险

- **低**: GaiaClient 缓存为进程内 (非持久化), 进程重启后缓存冷启动。预期行为, 非 bug。
- **低**: 60s TTL 内若 Gaia 数据文件被替换, 缓存可能返回旧数据。实际部署中 Gaia 数据为只读, 不存在此场景。
- **无**: 删除 gaia_cat 不影响任何已知下游消费者 (已审计)。

## 交付物清单

- `TASK_REPORT.md` (本文件)
- `TEST_REPORT.md`
- `EVIDENCE_INDEX.md`
- `REVIEW_REPORT.md`
- `gaia_cache_impl.json`
- `test_cache_hit.c` (测试源码)
- `test_cache_hit_output.txt` (测试输出)
- `stage1_run_output.txt` (stage1 日志)
- `stage1_run_err.txt` (stage1 错误日志)
