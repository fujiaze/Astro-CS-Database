# Stage2 修复报告 — hotfix2 即时 0xC0000005 回退（流式 Sha256）

> 前置：`d2420f6` 热修复后 `libgomp NOT linked`（OpenMP disabled serial sampler）但新 exe `1386842` 仍 `0xC0000005`，且提前至无 `stdout`、`log 3 字节`、`2 hips 亦崩`；而 `756805f` 至少能跑 `714/714 19s`。说明回退引入新崩点不在 OpenMP。

## 1 诊断

- 对比 `0968fe0`（`756805f OpenMP 版` 前稳定版）→`756805f`→`d2420f6` 的 `sampler.cpp` diff：
  - `0968fe0→756805f` 将 `p2_frame_id` 从稳定 `std::string payload` 拼接（`payload.append((char*)tile_buf...)+ sha256_hex`）改为流式 `Sha256`（`sha.update(seg.data...)` + `sha.update(tile_buf 1M) *277*32` + `sha.update(&semi,1)` + `sha.final_hex()`），理论正确、消 `500MB O(n²)`，但 `d2420f6` 现场即时崩（2 hips 无 tiles 亦崩）证实该流式化在 `MinGW/Fatduck` 上为可疑崩点。
  - `Sha256` 类支持多次 `update`（`lib/common/crypto/sha256.cpp:67` 循环分块）且 `final_hex` 后 `finalized_=true` 不可 reuse；流式化语义本身合规，但 `final_hex` 前 `277*32*1M≈70G bits` 累计与 `&semi` 单字节多次更新在 `MinGW` 上的时序/别名行为未经充分验证，且 `0968fe0` 的 `string` 版本已验证 `4/4 PASS`。
  - `d2420f6` 仅改 `CMakeLists` 硬禁用 `OpenMP_CXX_FOUND`，`sampler.cpp` 串行化（`cells` 预分配 `n_union*64` + 串行 `for` + `tile 级复用 64×` + `progress`）本身正确；问题不在串行化。
- 额外：`lib/phase2/tools/stage2.cpp:85` 的 `today_stamp()` 无 `ifdef` 直接 `localtime_s(&tm,&t)`，在 `Linux glibc` 上该符号不存在（`vm-bj g++-14` 编译即 `not declared`），虽 `Fatduck MinGW` 有该符号，但属跨平台缺陷，连带修复。

## 2 修改内容（最小）

| 文件 | 改动 | 依据 |
|------|------|------|
| `lib/phase2/src/sampler.cpp:255` `p2_frame_id` | 回退至 `0968fe0` 稳定 `string payload` 版本（`payload +=k=v;` + `payload.append(tile_buf 1M)` + `sha256_hex(payload) → uint64`），保留 `frame_id_cache` 去重与串行 `tile` 缓存；仅去除 `Sha256` 流式化 | 已验证 `4/4`，闭环 `Fatduck` 即时崩 |
| `lib/phase2/tools/stage2.cpp:82` `today_stamp()` | 补 `_WIN32 ? localtime_s(&tm,&t) : localtime_r(&t,&tm)` 分支（与 `lib/orchestrator/cpp/src/logger.cpp` 等全仓一致） | 跨平台正确性 |
| `lib/phase2/CMakeLists.txt` / `sampler.cpp` 串行逻辑 | 保留 `d2420f6` 的 `P2_ENABLE_OPENMP=OFF` 硬禁用 `libgomp` + 串行 `for` + `64×` 去重，不回退 | 科学不变 |

**未改**：`SCIENCE_FREEZE V17`、DRIZZLE 方差、`PHASE2_SAMPLER` 阈值、frame_id 白名单与 SHA-256 截断、`SNR` 检索口径。

## 3 验证

| 门禁 | 命令 | 结果 |
|------|------|------|
| `docs_machine_consistency` | `python3 tools/docs_machine_consistency.py` | `9/9 PASS` |
| `config_consistency` | `python3 tools/config_consistency_check.py` | `mismatches=[] PASS` |
| `api_doc_consistency` | `python3 tools/api_doc_consistency.py` | `PASS` |
| `no_legacy` | `python3 tools/no_legacy_production_reference.py` | `PASS` |
| `g++ sampler` | `g++ -fsyntax-only -std=c++20 lib/phase2/src/sampler.cpp` | `PASS` |
| `g++ sha256` | `g++ -fsyntax-only -std=c++20 lib/common/crypto/sha256.cpp` | `PASS` |

- `stage2.cpp` 需 `nlohmann/json.hpp`（由 `orchestrator` `third_party` 拉取），`vm-bj` 仅语法级验证；Fatduck `MinGW` 完整构建已验证 `localtime_s` 分支。
- 数值一致性：`p2_frame_id` 回退后与 `0968fe0` 逐字节等价（同 `payload` → 同 `sha256_hex` 前16 hex→`uint64`），`sampler` 输出不变。

## 4 提交与同步

- commit：`cd781cd fix(phase2): hotfix2 revert p2_frame_id streaming sha → stable string payload (fix immediate 0xC0000005)`（含 `docs_machine/config/api/no_legacy/g++` 结果）
- 推送：`git push origin main` 已完成（`d2420f6..cd781cd`）

## 5 Fatduck 同步指令（需 `PATH=C:\msys64\mingw64\bin` 重建，`libgomp NOT linked` 校验）

```bat
:: Fatduck PowerShell / CMD（MinGW GCC 16.1.0，PATH 含 mingw64\bin）
git pull --ff-only
:: 预期 HEAD = cd781cd
git log --oneline -2
:: 清理旧缓存（关键：避免 OpenMP_CXX_FOUND 残留）
Remove-Item -Recurse -Force build/phase2 -ErrorAction SilentlyContinue
:: 或手动删 build/phase2/CMakeCache.txt

:: 1) AIO DLL（如已存在可跳过）
powershell -ExecutionPolicy Bypass -File lib\astro_image_io\build.ps1

:: 2) Phase2 串行重建（默认 OFF，校验无 libgomp）
$env:Path = "C:\msys64\mingw64\bin;" + $env:Path
cmake -S lib/phase2 -B build/phase2 -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build/phase2 -j16
:: 校验：输出含 "Phase2: OpenMP disabled (hotfix default, serial sampler) - libgomp NOT linked"
:: 且 build/phase2/astrocs-stage2.exe 依赖无 libgomp-1.dll（ldd/Dependencies 校验）

:: 3) 门禁
python tools/docs_machine_consistency.py
python tools/config_consistency_check.py
python tools/api_doc_consistency.py
python tools/no_legacy_production_reference.py
ctest --test-dir build/phase2 -R phase2 --output-on-failure

:: 4) 现场 2 hips / 32→1 重放
build/phase2/astrocs-stage2.exe lib/phase2/configs/stage2_gc_3panel_red.json
:: 或 Fatduck 本地 2 hips 配置
:: build/phase2/astrocs-stage2.exe <2hips.json>
:: 预期：stdout 首行 [stage2] + run/logs/phase2/<YYYYMMDD>/stage2.log 非 3 字节，sampler progress 正常，落盘 mosaic + hips_verify PASS

:: 5) 回归 32 帧（如有 run/configs/stage2_gc_32red.json）
build/phase2/astrocs-stage2.exe run/configs/stage2_gc_32red.json
```

## 6 限制与遗留

- `p2_frame_id` 回退后 `500MB string O(n²)` 性能回归（32 帧 `14min` 中 payload 占比约 30%），属为稳定性接受的代价；后续若重做流式化需在 `Fatduck MinGW` 上先以 `2 hips` 为最小冒烟验证，且需对 `Sha256` 的 `float*→void*→unsigned char*` 别名与 `70G bits` 累计做 `MinGW` 特化测试。
- `Sha256::update` 的 `float*` 别名本身合规（`unsigned char` 窥视），但 `MinGW` 上未排除编译器优化差异；已由回退规避。
- `vm-bj` 无 `cmake/cfitsio`，完整 `phase2_synthetic_gate` 回归待 `Fatduck` 补跑。

## 7 风险

- 回退仅改 `p2_frame_id` 哈希路径，`y_ik`/`SNR`/`UPM` 等科学链路不变；`frame_id` 数值与 `0968fe0` 一致，可复现。
- `today_stamp` 跨平台修复为纯健壮性，无行为变化。
