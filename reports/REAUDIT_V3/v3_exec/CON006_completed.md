# CON-006 completion note

- Date: 2026-08-27 (session continuation)
- Commit: 6b41c06fd493c6c4d31734bbc191c975407ea04b
- Pushed origin/main: yes (da03792..6b41c06)
- Change summary:
  - `lib/phase2/tools/stage2.cpp`: CPU reference path per-pixel rejection/integration
    loop wrapped with OpenMP (`P2_ENABLE_OPENMP` + `cpu_workers > 1`, non-MSVC).
  - Each worker has independent scratch vectors (stack/weights/support_v/acc/fid_stack/
    reasons/src_idx). Shared counters use atomics or per-thread map merged in fixed order.
  - Fatal paths in parallel region use an atomic fail flag and are handled after
    the parallel region (no `return`/`p2_upm_close` from inside the omp for).
  - `lib/phase2/CMakeLists.txt`: propagate `P2_ENABLE_OPENMP=1` to `astrocs-stage2`.
  - `lib/phase2/tests/ivar_wiring_test.cpp`: Linux stage2 executable lookup (fixes V2
    P1-05 hardcoded `.exe`), and adds a production CLI 1T/2T differential check on the
    same synthetic 3-frame ivar-wiring input; compares signal and support layers.
- Validation:
  - `phase2_ivar_wiring --gtest_filter=Phase2IvarWiring.WireProductionStage2PerFrameIvar`
    PASS in `build/linux-openmp-on` (OpenMP ON, uses 2T path).
  - Same test PASS in `build/linux-release` (OpenMP OFF, serial fallback unchanged).
  - `phase2_synthetic_gate` selected suites PASS in OpenMP ON build.
- Remaining scope for CON-006 per spec:
  - Inner per-pixel/chunk parallel implemented; serial large_scale two-pass branch retained
    as conservative boundary; outer tile-level concurrent I/O/writer not yet implemented.
    This is a partial CON-006 implementation and should be reviewed before marking the
    gate fully closed.
