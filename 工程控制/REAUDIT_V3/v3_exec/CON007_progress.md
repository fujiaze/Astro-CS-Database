# CON-007 progress note

- Commit: fe83104bf3e9e76157bd52fb3aee3f4c44b6744e
- Pushed origin/main: yes (6b41c06..fe83104)
- Summary:
  - Added `p2_acr_block_eligible()` in stage2_common: ACR block eligibility is now
    based on ACR registered + explicit sigma + non-ivar + non-large-scale + route
    cpu/auto/cuda. `acr_route=cpu` no longer automatically bypasses ACR block.
  - Stage2 production route records `requested_route`, `effective_route`, `workers`,
    `fallback_reason` in tile logs and diagnostics.
  - `auto` on Linux without CUDA records `linux_no_cuda_auto_fallback` and effective
    route `cpu` (observed in ivar wiring test log).
  - routing_test updated with new eligibility tests.
- Validation:
  - phase2_routing (4 tests) PASS in ON build.
  - phase2_ivar_wiring PASS in ON build (includes 1T/2T CON-006 differential).
  - phase2_synthetic_gate selected 75 tests PASS.
- Remaining for CON-007:
  - The ACR CPU block still invokes the registered CPU launcher via `legacy_parallel`
    (serial per-chunk). Actual ACR Dispatcher/CPU executor or parallel ACR CPU kernel
    wiring is not yet implemented. The route/fallback/bookkeeping layer is in place.

## Manual route evidence
- Command:
  `astrocs-stage2 /tmp/stage2_con007_cpu_acr.json --cpu-workers 1`
  with `weight_mode=support_x_snr2`, `rejection.method=sigma`, `acr_route=cpu`.
- Log line:
  `[stage2] tile 0 ACR block: enabled=1 gpu=0 requested_route=cpu effective_route=cpu workers=1 fallback_reason=`
- Exit 0; mosaic written.
