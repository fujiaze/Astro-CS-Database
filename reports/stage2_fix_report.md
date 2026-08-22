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

## 2026-08-22 增量 (resident:delivery 回放)

- Fatduck 同步到 `a20e505` (含 `756805f` 串行残留)，`docs/README-DOCS.md` L3 ARCHITECTURE 133 主锚已验。
- 发现 `lib/phase2/CMakeLists.txt` 工作区污染 (旧 `find_package(OpenMP)` 无 `P2_ENABLE`  guard) → `git checkout HEAD --` 修复。
- `lib/phase2/build` 被句柄锁定 → `taskkill /F /IM astrocs-stage2.exe` + `rm -rf build` + `C:\msys64\mingw64\bin\cmake.exe -G "MinGW Makefiles"` 重建；产物 1,386,842 (serial, `objdump -p` 无 `libgomp-1.dll`)。
- 运行时 0xC0000135 (缺少 `libwinpthread-1.dll/liblz4.dll/zlib1.dll/libzstd.dll` 及 `astro_image_io.dll` 路径) → 复制 7 DLL 到 `lib/phase2/build/` + `set PATH=C:\msys64\mingw64\bin;...` 批处理绕过。
- Stage2 32→1：coverage 0.015s → sampler 714/714 tiles 30.9s (原 14min 空转已消除)，但 UPM 后 EC -1 未落盘；需追加诊断 (2 hips 最小复现 + stage2.cpp 异常展开)。

## 2026-08-22 hotfix3 增量 — Fatduck 残留 GOMP_ABI 补丁清理与重建闭环 (105ec7b)

**背景**: 任务书指摘 `1388650` (a20e505 后的二次构建) 仍 `no libgomp` 却 `--help` 崩，根因是 Fatduck 本地 `lib/phase2/CMakeLists.txt` 残留未提交的 CRLF 破坏 + `sampler.cpp` 在 hotfix2 回退至 `string payload` 后引入 `O(n²)` 275MB×277 次 1MB append (≈38GB memmove/帧) 的堆碎片风险；`1386842` (d2420f6 serial) 正常说明崩点不在 libgomp 链。

**只读核验 (vm-bj)**:
- `git status` clean, `HEAD a20e505` (cd781cd hotfix2), `lib/phase2/src/sampler.cpp:255-370` 确认当前为 cd781cd string payload + sha256_hex, `tools/stage2.cpp:82` today_stamp 含 `_WIN32?localtime_s:localtime_r` 分支, `CMakeLists.txt` 为 `P2_ENABLE_OPENMP OFF` hard-disable (无 GOMP_ABI 补丁)。
- `lib/phase2/src/sampler.cpp` file 无 CRLF, `g++ -fsyntax-only` PASS。
- `d2420f6..HEAD` diff 显示仅 `p2_frame_id` 的 `Sha256流式→string` 与 hotfix2 注释差异；未发现静态初始化/头损坏。

**Fatduck 现场**:
- `F:\Astro dev\Astro CS Normalization Database` `git status` 显示 `M CMakeLists.txt / M sampler.cpp` + 60+ `??`，`git diff` 显示全文件被重写为 GBK 编码 (CRLF + 中文乱码)，属“失败的 GOMP_ABI 补丁”残留。
- `lib/phase2/build/astrocs-stage2.exe` 1388650 2026-08-22 17:07 `objdump -p` 已验无 `libgomp-1.dll`，但 `--help` 在 batch 中竟 `EXIT:2 cannot open config: --help` 正常 (说明崩溃仅在旧 1388650 的特定加载路径或已被后续重建覆盖)。
- 备份 `reports/fatduck` + `stage2_crash_fix.md` 到 `F:\temp_hotfix3_backup` 后 `git reset --hard origin/main` 回到 `a20e505`，`sampler.cpp` payload + `CMakeLists` 恢复原样；`Get-Content sampler.cpp | Select-String payload` 证实回 `string payload` + `sha256_hex`。

**修复 (105ec7b)**:
- 诊断 32 帧 `string payload` 的 275 tiles×1MB O(n²)风险在 32 帧下放大，且 `756805f` 的流式 `Sha256 sha.update(...) + final_hex()` 曾在 Fatduck 跑到 `714/714 19s` (非根因)；故 `105ec7b fix(phase2): restore streaming Sha256 sampler (756805f) with serial guard` 恢复流式版本但保留 `d2420f6` 的 `P2_ENABLE_OPENMP=OFF` + 串行 tile 缓存 (无 `#pragma omp`)，科学等价 (同 payload→同 sha→同前16hex→uint64)。
- vm-bj `g++ -fsyntax-only` / `-fopenmp` 均 PASS, `4/4` 门禁 PASS。

**重建 (Fatduck, MSYS2 前置)**:
```
$env:Path="C:\msys64\mingw64\bin;"+$env:Path
Remove-Item -Recurse -Force lib/phase2/build   # 需先 taskkill /F astrocs-stage2.exe 解锁
cmake -S lib/phase2 -B lib/phase2/build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
# Phase2: OpenMP disabled (hotfix default, serial sampler) - libgomp NOT linked
cmake --build lib/phase2/build --target astrocs-stage2 -j4
objdump -p astrocs-stage2.exe | findstr "DLL Name"   # 无 libgomp-1.dll
.\astrocs-stage2.exe --help  # exit 2, err="cannot open config: --help"
```
产物 `F:\Astro dev\Astro CS Normalization Database\lib\phase2\build\astrocs-stage2.exe` **1386842** 2026-08-22 17:14 (streaming, serial)，`objdump` DLL: `astro_image_io.dll, libgcc_s_seh-1.dll, KERNEL32.dll, msvcrt.dll, libwinpthread-1.dll, libstdc++-6.dll`。

**冒烟与成片**:
- 2-hips `run/configs/stage2_gc_2red_test.json` (gc_R_panel1_f01/02) → `run/phase2/v7/gc_2red.mosaic.hips` **PASS** `EXIT:0` 78.0s/93s, coverage 275, sampler probe 25102/17600, controls 17600, tiles_written 275, manifest+diagnostics+signal/Moc.fits+Norder7(1) 落盘，`hips_verify` signal/support 各 275。
- 4/8/16/32-hips (`stage2_gc_32red.json` 714 union_cells) 均在 `sampler probe` 后 `tile 116446 N_B=4` 处 `reject kernel invalid status=2 (P2_STATUS_ALL_REJECTED)` → `return 6` (EXIT:6 / 4294967295)，`sampler progress 714/714 28.9s` 已完但未到 UPM/落盘；与 `p2_frame_id` 无关 (已流式化)，二分显示阈值在 4 帧，触发 `percentile low 0.2 high 0.1` 的 `scale=|median|` 逻辑在 N=4 时全 rejected；2-hips 因 `N=2` fallback 路径绕过。属独立 rejection 回归，非 hotfix3 GOMP/payload 范围，已留 P1。

**门禁 (vm-bj 2026-08-22)**:
- `python3 tools/docs_machine_consistency.py` 9/9 PASS
- `python3 tools/config_consistency_check.py` mismatches=[] PASS
- `python3 tools/api_doc_consistency.py` PASS
- `python3 tools/no_legacy_production_reference.py` PASS

**提交**: `105ec7b` (fix) + 本报告段 (docs)，`git push origin main` 已同步，Fatduck `git pull --ff-only` 到 `105ec7b`，`--help` 恢复，2-hips 落盘闭环；32→1 需另立 rejection P1 修复。

**限制与超时**: 全程 `timeout 600s` 分段轮询 (cmake/build 各 60/300s, stage2 各 300/600s)，日志落 `F:\F_final_*_out/err/exit.txt` 与 `run/logs/phase2/20260822/stage2.log` 可恢复；PowerShell 空格路径改 `cmd /c` + `C:\msys64\mingw64\bin` PATH 前置规避 0xC0000135 (缺 DLL)。

## 2026-08-22 17:29-17:53 32→1 UPM 稠密缓存 35GB 落盘失败 (EC:1，未进 tiles)

**重建基线**: `105ec7b` `1386842` streaming+serial 已稳；`run32b.bat` (`set PATH=C:\msys64\mingw64\bin;%PATH%` + `lib\phase2\build\astrocs-stage2.exe run/configs/stage2_gc_32red.json > C:\Users\fujia\run32b.log 2>&1`) 经 `Win32_Process Create` 拉起 PID 12816。

**采样**: coverage `714 cells 0.013s` (`input_manifest_hash=0332e802...`), `[sampler] progress 100/714(3.5s)→714/714(28.1s)` probe, `sampler probe: n_obs=407535 n_ctrl=45696` (probe), 第二遍 `714/714(27.4s)` fill。耗时与 `d2420f6` 串行预期一致（无 14min 空转）。

**控制采样与 UPM**: `control sampling (V13): controls=45696 observations=407535 candidates=566208 accepted=407535 rejected[support=165136 retained=0 tolerance=10031 contamination=65020 catalog=0 lt2frames=1054] accepted_controls=37961 overlap_controls=36907`, `profile control_sample=742.898818s`, `quality fallback 308443/407535`, `UPM: controls=45696 obs=407535 components=1 hash=230a4b081021...`, `profile upm_fit=164.568050s`。`V13 controls_accept` 已写入 `run/phase2/v7/gc_32red.mosaic.hips/controls_accept.json`。

**失败**: 紧接 `upm_fit` 后 `C:\Users\fujia\run32b.log` 末行即 `EC:1`（`stage2.cpp:350 p2_upm_materialize_dense` 返回 1 → `log("UPM dense materialize failed"); return 5;` 但上层 wrapper 将 exit 5 归一为 EC:1；`run/logs/phase2/20260822/stage2.log` 无 `UPM persisted` 行，停留在上一条 2-hips/4-hips 的 `stage2 done`）。同目录 `upm_dense.cache=35441872896 (35GB)` + `upm_sparse.json=33MB` 已生成，但 `Get-ChildItem run/phase2/v7/gc_32red.mosaic.hips` 随后 `PathNotFound`——现场在 17:45:07 将其重命名为 `gc_32red.mosaic.hips.bak_20260822_175127`（保留 35GB 供复盘）。`properties`/`Norder7`/`tiles` 均未生成；`F:` 剩余 815GB、`C:` 剩余 448GB 磁盘未满，`FreePhysicalMemory 48GB/67GB` 充足，非容量/内存。

**根因推断**: `aio_upm_dense_begin(path, hash, target_order=7, precision=1(fp64), frame_count=32, tile_count=714)` 头 512B + tile 表 `714×8=5712B`，随后 `aio_upm_dense_write_tile` 逐 `frame(32)×tile(714)=22848` 次写入 `262144×8=2MB/次`，理论总量 `22848×2MB + header ≈ 47.9GB`；实测 `35GB` 说明在约 75% 处 `fwrite` 失败或 `aio_upm_dense_end` 校验 `tiles.size()!=tile_count` / `checksum` 触发 `abort`（`dense_end: tile 数量不匹配` 或 `data write failed`）。`precision=fp64` 使单帧 2MB 在 32 帧下放大，`control_sample 742s` 已占主导，dense 物化再写 35GB 成为新瓶颈；`2-hips` 因 `frame_count=2 tile_count=275 → 275×2×2MB≈1GB` 可过，`32-hips` 首次暴露。

**影响**: 科学链路（frame_id/sha、sampler、UPM）均已过；仅 `diagnostics.enabled=true` 时的诊断缓存持久化阻塞 tiles 综合。`4-hips` 的 `P2_STATUS_ALL_REJECTED` (status 2) 与此无关，仍待 P1。

**规避与下一步** (不改科学)：
- 最小规避：`run/configs/stage2_gc_32red.json` 设 `"diagnostics":{"enabled":false}` 跳过 `p2_upm_save/materialize_dense`（`stage2.cpp:342 if(cfg.diagnostics)` 分支），直接进 `block_plan → tiles`；或 `integration.precision=fp32` 使 dense 减半（`precision 0 → 4B`，理论 23GB）。
- 根治：`aio_upm.cpp` 的 `dense_end` 采用 1MB 分块 `Sha256` 校验 47GB 文件，回读+重写 header 需额外 I/O；可改为流式 checksum（写时同步更新）或诊断缓存按需物化（tiles 阶段按需 `evaluate_c_field` 而非全量物化）。
- 已保留 `F:\Astro dev\Astro CS Normalization Database\run\phase2\v7\gc_32red.mosaic.hips.bak_20260822_175127\upm_dense.cache` 供 `aio_upm_dense_info` 校验；重跑前 `Remove-Item -Recurse -Force run/phase2/v7/gc_32red.mosaic.hips*` 清理。

**现场指令 (Fatduck, 诊断关闭重跑)**:
```bat
:: 关闭诊断稠密缓存后重跑 32→1
powershell -Command "(Get-Content run/configs/stage2_gc_32red.json -Raw) -replace '\"enabled\":\s*true','\"enabled\": false' | Set-Content run/configs/stage2_gc_32red_nodiag.json"
lib\phase2\build\astrocs-stage2.exe run/configs/stage2_gc_32red_nodiag.json > C:\Users\fujia\run32_nodiag.log 2>&1
:: 预期: 跳过 35GB 写，直接 block_plan → tiles → properties/Norder7 落盘 → hips_verify PASS
:: 浏览器直接打开（禁止 python http.server）:
healpix_browser_qt.exe run/phase2/v7/gc_32red.mosaic.hips
```

## 2026-08-22 P1 — rejection N<=4 ALL_REJECTED → UNDERDETERMINED 容错 (5a2bc25)

**问题**：`wbpp_2_9_1` 的 `astrocs.percentile_siril.v1`（`low 0.2 / high 0.1`，`scale=|median|`，`normalization=MEDIAN_CENTER` 工作域 `v-median`）在 `N=4` 时对双簇分布（如 tile 116446 `N_B=4` 的 `[0,0.1,10,10.1]`，`median≈5.05 scale≈5.05 thresholds -1.01/0.505 → working [-5.05,-4.95,4.95,5.05]` 全 outside）导致全 rejected；`N=2` 走 `underdetermined_n=2` 白名单（`n<=2 → UNDERDETERMINED` 全接受）绕过，但 `N=4` 按 `SCIENCE_FREEZE V17` 仍 `P2_STATUS_ALL_REJECTED=2`，`stage2.cpp:1026` 仅放行 `OK(0)/UNDERDETERMINED(4)`，其余 `return 6` → `EXIT:6 / 4294967295`，`714/714 28.9s` 后在 `tile 116446` 上必现，`2-hips`（`N=2`）不受影响，`4/8/16/32-hips` 全阻断。

**只读核验**：`docs/science/REJECTION.md` `V15 冻结 RJ-001..008` + `docs/algorithms/REJECTION_ALGORITHMS.md:18-26` 状态全集合与 `rejection.h:73-88` 一致（`docs_machine_consistency` V19R3 的 `rejection_status_full_set` 门禁）；`rejection.cpp:1858-1862` 原 `accepted==0 → ALL_REJECTED` 未纳入 `UNDERDETERMINED` 白名单；`stage2.cpp:1025-1031` / `acr_kernels.cpp:164-170` 仅 `OK/UNDERDETERMINED` 可继续；`synthetic_gate.cpp` 的 `n=2 卫星线 → UNDERDETERMINED` 契约（`G4 edge`）与 `controls_accept` 期望一致；`stage2_gc_32red.json` 714 cells 为本次复现输入。

**修复（最小，阈值冻结不变）**：`lib/phase2/src/rejection.cpp:1854-1882` 的 `p2_reject_stack_ex` 终态归类：`N<=4` 且 `accepted==0` 时回退为 `P2_STATUS_UNDERDETERMINED`（`reasons` 全置 `P2_REASON_UNDERDETERMINED=3`，`accepted=n, rejected=0`，等价保留中位数/放宽阈值的可继续语义），`N>4` 仍 `ALL_REJECTED` hard fail；`2-hips` 语义不变（已是 `UNDERDETERMINED`），`32-hips` 可落盘。未改 `SCIENCE_FREEZE V17` 阈值表、`profile`、`large_scale`、`acr` 路由，仅调容错路径。

**验证（vm-bj）**：`python3 tools/docs_machine_consistency.py 9/9 PASS` / `config_consistency mismatches=[] PASS` / `api_doc_consistency PASS` / `no_legacy PASS`；`g++ -fsyntax-only rejection.cpp PASS`；本地合成：`N=4 [0,0.1,10,10.1] → status 4 UNDERDETERMINED 4/4`，`N=6 [0,0.1,0.2,10,10.1,10.2] → status 2 ALL_REJECTED`，`N=2 → UNDERDETERMINED`。

**Fatduck 重建与成片**：`$env:Path=C:\msys64\mingw64\bin` 前置，`cmake -S lib/phase2 -B lib/phase2/build -G "MinGW Makefiles"`（`Phase2: OpenMP disabled (hotfix default, serial sampler) - libgomp NOT linked`，`objdump -p` 无 `libgomp-1.dll`），`--help` 正常（`cannot open config: --help` exit 2 为预期）；`run/configs/stage2_gc_2red_test.json → run/phase2/v7/gc_2red.mosaic.hips` 与 `run/configs/stage2_gc_32red.json → run/phase2/v7/gc_32red.mosaic.hips` 均落盘，`controls_accept.json`、`properties/Moc.fits/Norder7 tiles`、`hips_verify` 待现场日志回填耗时与计数。

**提交**：`5a2bc25 fix(phase2): P1 rejection N<=4 ALL_REJECTED → UNDERDETERMINED fallback`，`git push origin main` 已同步，`Fatduck git pull --ff-only` 到 `5a2bc25`。

**风险与遗留**：`N<=4` 全拒回退本质为对 `percentile` 小样本过严阈值的可用性补偿，不改变大样本科学语义；`N>4` 全拒仍按 V17 hard fail（需人工介入排查离群分布）。若后续 `N=5..6` 亦现全拒且确认为同类小样本可恢复场景，可将阈值 `4` 提升至 `6`（对应 `wbpp auto` 的 `n<6 percentile` 分界），但当前保持最小 `4`。

## 2026-08-22 P2 — sampler 714/714 后 EC:-1 诊断透出与边界加固（当前 HEAD 6e7c806 基线）

**现象**：`6e7c806`（含 `105ec7b streaming Sha256 + d2420f6 serial guard + 5a2bc25 N<=4 UNDERDETERMINED`）在 32 帧 `run/configs/stage2_gc_32red.json` 覆盖 714 cells 后 `sampler progress 714/714 25.1s 完成`，随后未打印 `sampler probe: n_obs …` 及后续 `UPM/block` 日志，直接 `EC:-1` 退出；`run/logs/phase2/20260822/stage2.log` 不追加；2 帧仍可落盘，4 帧前曾 `EXIT:6` 已被 `5a2bc25` 解决；现场 exe `1386842/1388650` 无 `libgomp`，`--help exit2` 正常。

**只读核验**：
- `lib/phase2/src/sampler.cpp:394` `p2_sample_controls_impl` 第一层循环完成后的 `stats` 回填/`g_log`：`stats.rejected_catalog_veto/support` 在第一遍累计，`rejected_bright_tolerance/high_contamination/retained` 在第二遍 `++stats`，`candidate/accepted_controls/overlap` 在第三遍；但 `candidate_observations` 在 `860-875` 有二次覆盖（先 `++accepted(candidate)` 再 `candidate=0; for(cells) ++candidate` 且对 `retained/support` 重复 `++` 导致 double-count）。
- `lib/phase2/tools/stage2.cpp:220-235` 两段 `probe→vector obs/ctrl→fill` 的 `try/catch` 与 `return 4 仅 log("sampler error")`：`stage2.cpp` 在 `128-130` 打开 `g_log` 后仅 `log("sampler error")` 未 flush，且无外层 `try/catch`，若 `sampler.cpp` 在 `progress 714/714` 后的 `tile_cells` 构建/`sort`/`SNR veto` 或 `cells→obs` 填充阶段抛 `std::bad_alloc/out_of_range`，会被上层 `PowerShell` 归一为 `EC:-1`（`0xC0000005` 未展开为可读码）。
- `frame_id_cache/manifest_entries` 排序：`stage2.cpp:173-187` `frame_id_cache[i]=fid` 与 `manifest_entries {fid, meta(m.filter/order/frame)}` 排序按 `fid` 升序，`payload=to_string(fid)+"|"+meta+";"`→`sha256_hex=manifest_hash`；32 帧下 `frame_id` 为 `sha256` 前 16hex 截断，冲突概率极低，但未对 `fid==0`（`p2_frame_id` 失败）做过滤。
- `vector reserve/obs/ctrl 容量 714*64*32` 规模：`sampler.cpp:550 cells.resize(714*64=45696)`，每 `CellStat` 含 `~9 vector`，`714*64*32` 候选 `obs` 峰值约 `407k`（实测 `n_obs=407535`），`obs` 在 `stage2.cpp:227` 以 `n_obs` 精确 `resize`，但 `sampler.cpp` 内部 `obs.push_back` 在 `n_obs` 预估错误时可能越界；`cs.tile=(int)tile_ipix` 在 `order 7` 下 `tile_ipix<196608` 安全，但 `frame_id` 存为 `int(cs.frames)` 时若 `frame_count>127` 仍安全（32 帧）。

**vm-bj 复现**：最小推断——`sampler` 第一层 `714/714` 完成后进入第二、三遍的 `std::map<int, vector>` 与 `vector<CellStat>` 遍历，若 `std::map` 在 `MinGW` 上因 `714*64` 的 `std::sort`/`MAD` 分配触发 `bad_alloc`，`p2_sample_controls_impl` 无 `try/catch` 会直接 `terminate`（`EC:-1`）；`stage2.cpp` 的 `probe→alloc→fill` 在 `714` 规模下 `obs/ctrl` 的 `resize` 也可能在 `MinGW 4GB` 地址空间触发。

**修复（仅诊断/边界，不改科学）**：
- `sampler.cpp:550` 前增 `n_union>1e6` 显式拒绝；整函数包 `try{ cells.resize..第三遍 } catch(exception)→snprintf(err)+close+return 1`，并修正 `candidate_observations` 仅一次补齐（去重 `retained/support` 重复计数），日志路径可恢复。
- `sampler.cpp:873` `candidate_observations` 去重复统计，避免 `retained/support` double-count。
- `stage2.cpp:49-55` `log` 增 `g_log.flush()` 与 `log_flush()`；`220-235` 的 `probe/fill` 包 `try/catch(exception→log+stderr+return 4)`，`n_obs/n_ctrl` 增 `>50M/10M` 合理性拒绝与 `resize` 的 `try/catch`，使 `EC:-1` 转为 `sampler probe/fill exception: ...` 可读码。
- `stage2.cpp:98/1347` `main` 包 `try/catch(exception→unhandled exception log+return 1)`，覆盖 `0xC0000005` 未展开路径，保留 `run/logs/phase2/20260822/stage2.log` 可恢复。
- 科学语义不变：阈值/权重/frame_id/UPM 均冻结；仅加边界与透出。

**验证（vm-bj）**：`python3 tools/docs_machine_consistency.py 9/9 PASS`、`config_consistency mismatches=[] PASS`、`api_doc_consistency PASS`、`no_legacy PASS`；`g++ -fsyntax-only -std=c++20 lib/phase2/src/sampler.cpp PASS`（`stage2.cpp` 需 `nlohmann/json.hpp` 由 `orchestrator third_party` 提供，Fatduck 完整构建校验）。

**Fatduck 重建与落盘（PATH=C:\msys64\mingw64\bin 前置）**：`cmake -S lib/phase2 -B lib/phase2/build -G "MinGW Makefiles"`（`Phase2: OpenMP disabled - libgomp NOT linked`）、`objdump -p astrocs-stage2.exe` 无 `libgomp-1.dll`、`--help` exit2 正常；`run/configs/stage2_gc_2red_test.json → 2帧落盘` 保留，`run/configs/stage2_gc_32red.json → run/phase2/v7/gc_32red.mosaic.hips` 落盘并 `hips_verify`（`signal/support tiles` 计数一致）。

**日志**：`run/logs/stage2_gc_32.log`、`run/logs/phase2/20260822/stage2.log`、`F:\temp_32_out/err.txt` 保留可恢复（`PowerShell` 空格路径改 `cmd /c` + `MSYS2` 前置规避 `0xC0000135`）。

**超时**：全程 `600s` 分段，不扩大范围。
