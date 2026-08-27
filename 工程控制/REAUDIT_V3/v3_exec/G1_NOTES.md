# CON-001 地面核验 (执行者独立, 供交叉检查)

## Phase2 (lib/phase2) 生产 CLI = astrocs-stage2 (lib/phase2/tools/stage2.cpp, 1497 行)
main() 调用链 (L105-1396):
  p2_stage2_parse_config -> p2_coverage_build(L175) -> p2_frame_id(L204) -> frame_snr_medians(L232)
  -> p2_sample_controls_cached(L259/L286) -> p2_upm_build_geo(L412) -> upm_persist/p2_upm_write_sparse(L478)
  -> block_plan -> register_phase2_acr_kernels(L603) -> 逐 tile: p2_reject_plan_resolve -> p2_upm_calibrate_block(L888/L1059)
  -> p2_reject_stack_ex(L1189) -> p2_integrate_pixel(L1262/L1324) -> aio_hips_write_signal_support_tile(L1009/L1368) -> mark(tiles_process)

## 并行性现状 (关键)
- lib/phase2 生产代码【无任何 #pragma omp】; 唯一 omp 相关为 CMake 选项 P2_ENABLE_OPENMP (默认 OFF, hotfix)。
  grep '#pragma omp' lib/phase2/src lib/phase2/tools = 无。nm -D astrocs-stage2 | grep omp = 无 (OPENMP_WIRING_FALSE)。
- P2_ENABLE_OPENMP 默认 OFF => stage2 全部串行。阶段2 UPM 单线程卡死 (G2Persistence wall 30.69s %CPU99%)。
- 共享状态: stage2.cpp L1085 曾在逐像素循环构造 vector<uint32_t> src_idx(depth); V2 运行验证 (LD_PRELOAD)
  显示 -O2 下 GCC 已将它 SRA 合并到栈 (operator new=0, malloc=31), P2-01 影响降级为可维护性提示。
- CON-007 关键: stage2.cpp L718-720 use_acr_block = acr_reg!=null && method==SIGMA && cfg.acr_route!='cpu' && !large_scale。
  生产配置 acr_route='cpu' => use_acr_block=false => ACR TBB 并行块被绕过, 落到串行 legacy path。
- ACR 库 (lib/acr) 用 TBB: submit_range (core/runtime.cpp L235) 调 arena_parallel_for(tbb::blocked_range)。
  submit_serial(L389) 为串行。 => ACR parallel_for 是真实 TBB 并行 (非 omp), 但仅 acr_route!=cpu 时接线。

## 阶段1 (orchestrator, main=lib/orchestrator/cpp/src/main.cpp)
- L604 threads=hardware_concurrency(); L765-767 只对 CALIBRATE 调 set_num_threads; 其它模块无 set_num_threads。
- 无全局 ExecutionOptions(workers/gpu_route/memory_budget/deterministic) 对象 => CON-002 需新建。
- 真实 #pragma omp 生产者 (grep lib):
  * lib/calibration: cosmetic_corrector.cpp(L105,126,147,162) + master_generator.cpp(L91,96,189,228) => 校准并行(OpenMP)。
  * lib/healpix_db/healpix_drizzle/drizzle_engine.cpp(L1664 num_threads(config.threads>0?..:omp_get_max_threads), L1836-1845 atomic/reduction) => drizzle 并行。
  * lib/healpix_db/healpix_drizzle/snr_evaluator.cpp(L321) => 并行。  (注意: 这是 healpix_drizzle 下, 非 lib/snr_estimator)
  * lib/dynamic_psf => dpsf_psf.cpp 多处 parallel for; lib/star_detector => sdet_image.cpp 大量 parallel for。
- lib/photometric_calib (src) 无 omp/thread => 串行。 lib/snr_estimator (src) 无 omp => 串行。
  => 现有 docs/architecture/production_call_paths_stage1.csv 称 SNR 为 parallel_cpu 不可靠; 需子代理核实实际接线。

## 历史 wall 锚点 (V2 12_performance)
- UPM G2PersistenceAndHashSensitivity 13.7MiB 模型: wall 30.69s user 30.22s %CPU99% RSS366.6MB (单线程)。
- phase2 ctest 90 tests -j1: wall 35.8s CPU/wall~0.96。
- scaled drizzle 256x256: wall 0.456s。
- 32R 真实运行被 Gaia(~107GB)阻塞 => 无 fake perf。

## 输出位置约定
- exec inventory 放 docs/architecture/execution_inventory.csv (与现有 production_call_paths_stage*.csv 相邻)。
- CON-001 required_commit=yes => 单任务原子 commit 该 CSV; 不改代码 (CON-001 仅为盘点)。
