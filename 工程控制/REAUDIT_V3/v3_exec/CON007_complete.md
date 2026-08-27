# CON-007 complete note

- Commit: 253e7ee80c88a1c9870e141449766f36766c1d65
- Pushed origin/main: yes
- Summary:
  - `mosaic_reject_legacy` now has a shared per-pixel lambda.
  - OpenMP parallel CPU path: per-thread scratch (stack/weights/support/fid/reasons/
    accepted/frame_seq), fixed static schedule, atomic fail flag + mutex-guarded error
    message.
  - Worker count passed from `ExecutionOptions` via new invocation scalar.
  - `acr_route=cpu/auto` enters ACR block for eligible sigma/non-ivar paths; fallback
    reason and effective route are logged.
- Validation:
  - `phase2_synthetic_gate` all: 81 PASS, 10 SKIP (CUDA/real HiPS), 0 FAIL.
  - `phase2_ivar_wiring` PASS.
  - `phase2_routing` PASS.
  - Manual ACR CPU stage2: `workers=1` and `workers=2` produce byte-identical FITS
    signal/support tiles; logs show `requested_route=cpu effective_route=cpu`.
- Note: This is ACR registered CPU launcher parallelism, not the full ACR TBB
  Dispatcher wiring. If reviewer requires exact Dispatcher `CpuExecutor` path, that
  remains follow-up.
