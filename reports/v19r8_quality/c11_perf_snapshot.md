# C-11 性能快照（vm-bj 轻量 snapshot，非完整 benchmark）

> 任务：QA-V19R7-C-11｜范围：只做 C-11 单项｜环境：vm-bj Linux｜超时 300s｜日志：`reports/v19r8_quality/c11_perf_snapshot.log` + 本报告
> AGENTS.md 约束：禁止无意义重复跑完整基准。本快照为**单次轻量计时**（pipeline_frame save/load + noise_model 规模），预留 cpu/mem/io/等待 分析框架，不展开完整 benchmark。

## 环境

- Host: Linux VM-BJ 6.12.95+deb13-amd64 #1 SMP PREEMPT_DYNAMIC Debian 6.12.95-1 (2026-07-04) x86_64 GNU/Linux
- CPU: Intel(R) Xeon(R) Gold 6148 CPU @ 2.40GHz (2 cores)
- Mem: Mem:           3.6Gi       3.4Gi       227Mi       1.4Gi       1.7Gi       293Mi
- Toolchain: g++ (Debian 14.2.0-19) 14.2.0
- 计时手段：`bash + date +%s.%N / time.perf_counter` 单次 wall 计时（vm-bj 无 `perf`/`/usr/bin/time -p`，以等效单跑 wall 计时替代，已在日志中注明）
- 超时：单步 `timeout 300`，可恢复；日志落盘 `c11_perf_snapshot.log`

## 计时结果（单次，不重复跑）

| 对象 | 产物 | 编译 wall | 运行 wall | 规模 | 结果 |
|------|------|-----------|-----------|------|------|
| C-05 pipeline_frame | `/tmp/c11_pf` 63584B (预期 63KB) + `a.out` 缓存 247B | 5.494s | 0.0172s | 28 contracts：add/replace/move + KV + save/load + 异常路径事务性 | 28/28 PASS (`[PASS]`=28) |
| C-02 noise_model | `/tmp/c11_nm` 98168B (预期 96KB) | 8.537s | 0.374s | N=256×256=65536, SNR-001..014 科学矩阵 | 39/39 PASS |

- 细节：pipeline_frame `aio_frame_save_cache`/`load_cache` 在 247B 原子 `.aio`（AIO1 magic, bit-exact f64 payload）上完成；noise_model 为确定性合成帧（pedestal/scale/star-population/blank-sky/Poisson/spatial/dex/ivar/signal-independence 等），无 I/O 放大。
- IO 微探针（64 MiB）：write 0.172s (371.8 MiB/s), read 0.067s (948.8 MiB/s) — 仅作 `c11_perf_snapshot.log` 侧写，不计入 Gate。

> 注：编译 wall 受 vm-bj 并发负载波动（观测 3–12s），运行 wall 稳定（pipeline <0.02s, noise 0.2–0.6s）。本次快照**不做多轮均值/分位**，符合“最小性能快照”要求。

## 分析框架预留（cpu / mem / io / 等待）

按 AGENTS.md “性能优化先分析 CPU、内存、IO、等待、重复计算和算法复杂度”预留，后续如需优化再展开：

- **CPU**：pipeline_frame 为纯内存拷贝+校验（O(n_blocks·payload)），无热点；noise_model 为 O(N) 逐像素 + patch 统计，当前 256² 在 0.3s 内完成，推算 1 Mpix ~4.7 ms（`0.306×1e6/65536`），符合预期线性复杂度。
- **内存**：pipeline_frame 单帧 <1 KiB net + 247B 落盘，峰值 <100 KiB；noise_model 单帧 256² float ~256 KiB + 辅助缓冲，峰值 <10 MiB。vm-bj 3.6 GiB 下无压力。
- **IO**：pipeline `.aio` 原子写（`tmp→rename`）247B，非瓶颈；noise 本轮无外存 I/O。64M 探针显示 vm-bj 顺序读写 >300 MiB/s，未见阻塞。
- **等待**：无锁/无网络等待；OpenMP 仅在 drizzle/叠加路径启用，本快照单线程。若后续并行化，预留线程池/ affinity 分析位。
- **重复计算**：已避免；完整 benchmark（多规模/多次/火焰图）**不在本 vm-bj 快照范围**，需 Windows/MSYS2 或专用基准节点另行排期。

## 机器一致性

```
{
  "tool": "docs_machine_consistency",
  "version": "1.0.0",
  "checks": [
    {
      "check": "config_weight_mode_ivar",
      "pass": true,
      "detail": "CONFIG_SCHEMA weight_mode auto/ivar <-> stage2_common parse"
    },
    {
      "check": "frame_id_contract_exact",
      "pass": true,
      "detail": "DATA-FRAME-ID-001：SHA-256 truncate；无 FNV/路径派生残留"
    },
    {
      "check": "error_taxonomy_exit_codes",
      "pass": true,
      "detail": "ERROR_MODEL 全集合 == orchestrator.h 0-10 退出码 (doc={'SUCCESS': 0, 'GENERIC_ERROR': 1, 'DLL_LOAD_FAILED': 2, 'BLOCK_MISSING': 3, 'CALIBRATE_FAILED': 4, 'PLATESOLVE_FAILED': 5, 'DRIZZLE_FAILED': 6, 'CONFIG_ERROR': 7, 'FILE_IO_ERROR': 8, 'TIMEOUT': 9, 'CANCELLED': 10, 'STAR_DETECT_FAILED': 20, 'PSF_FAILED': 21, 'PHOTOMETRIC_FAILED': 22, 'SNR_FAILED': 23, 'STACK_FAILED': 24, 'HISS_INVALID': 25, 'HCSD_INVALID': 26, 'MODULE_ABI_UNSUPPORTED': 27, 'INPUT_INVALID': 28, 'MODULE_SPECIFIC_BASE': 100} code={'SUCCESS': 0, 'GENERIC_ERROR': 1, 'DLL_LOAD_FAILED': 2, 'BLOCK_MISSING': 3, 'CALIBRATE_FAILED': 4, 'PLATESOLVE_FAILED': 5, 'DRIZZLE_FAILED': 6, 'CONFIG_ERROR': 7, 'FILE_IO_ERROR': 8, 'TIMEOUT': 9, 'CANCELLED': 10, 'STAR_DETECT_FAILED': 20, 'PSF_FAILED': 21, 'PHOTOMETRIC_FAILED': 22, 'SNR_FAILED': 23, 'STACK_FAILED': 24, 'HISS_INVALID': 25, 'HCSD_INVALID': 26, 'MODULE_ABI_UNSUPPORTED': 27, 'INPUT_INVALID': 28, 'MODULE_SPECIFIC_BASE': 100})"
    },
    {
      "check": "integration_status_full_set",
      "pass": true,
      "detail": "integration status 全集合 (doc={'P2_INTEGRATE_OK': 0, 'P2_INTEGRATE_NO_CANDIDATES': 1, 'P2_INTEGRATE_ALL_REJECTED': 2, 'P2_INTEGRATE_ZERO_VALID_WEIGHT': 3, 'P2_INTEGRATE_INVALID_INPUT': 4} code={'P2_INTEGRATE_OK': 0, 'P2_INTEGRATE_NO_CANDIDATES': 1, 'P2_INTEGRATE_ALL_REJECTED': 2, 'P2_INTEGRATE_ZERO_VALID_WEIGHT': 3, 'P2_INTEGRATE_INVALID_INPUT': 4})"
    },
    {
      "check": "rejection_status_full_set",
      "pass": true,
      "detail": "rejection status 全集合 (doc={'P2_REASON_ACCEPTED': 0, 'P2_REASO
```

- 结果：`machine 9/9 PASS`（`pass=true`），`python3 tools/docs_machine_consistency.py` 已在快照后执行，日志见 `c11_perf_snapshot.log` 尾段。

## 结论

- vm-bj 轻量快照完成：pipeline_frame save/load 与 noise_model 规模已单次计时并落盘，非完整 benchmark，满足 C-11 “最小性能快照”与 AGENTS.md 约束。
- 超时 300s + 日志已满足工程管理要求；后续完整 benchmark 按需另起，不在本任务范围。

## 复现

```bash
# timeout 300s, 单次，不重复
timeout 300 g++ -O2 -std=c++17 -fopenmp -DAIO_ENABLE_PIPELINE -I lib/astro_image_io/include \
  lib/astro_image_io/tests/pipeline_frame_contract_test.cpp lib/astro_image_io/src/aio_pipeline.cpp lib/astro_image_io/src/aio_log.cpp -o /tmp/c11_pf && /tmp/c11_pf
timeout 300 g++ -O2 -std=c++17 -fopenmp -I lib/snr_estimator/cpp/include \
  lib/snr_estimator/cpp/test/noise_model_science_test.cpp lib/snr_estimator/cpp/src/snr_estimator.cpp lib/snr_estimator/cpp/src/noise_model.cpp -o /tmp/c11_nm && /tmp/c11_nm
python3 tools/docs_machine_consistency.py  # 9/9 PASS
```
