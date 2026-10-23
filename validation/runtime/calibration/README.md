# validation/runtime/calibration — observation.calibrate (M4)

This directory owns the frozen C-CAL inputs, the runnable kernel/plan case and
the observed/comparison evidence of `observation.calibrate`.

| file | role |
|---|---|
| `case.json` | frozen C-CAL-1/2/DQ inputs, the plan constants, the required check names and the exact tolerances |
| `cal_kernel.cpp` | in-memory kernel + plan + metadata-only validate case; no Host/pool/Storage |
| `run_cal_kernel.sh` | builds and runs `cal_kernel` (science archives when present, otherwise the minimal source subset) and rewrites `observed.json` / `comparison.json` |
| `case_host.json` | frozen Host-chain inputs (C-CAL-1/2/DQ/H/ROOT/DEV) and required checks |
| `cal_host.cpp` | driver → generated registry → ProcessHost → Storage → NativeObservation end-to-end case |
| `run_cal_host.sh` | links `cal_host` against `$ACSD_BUILD` and rewrites `observed_host.json` / `comparison_host.json` |
| `observed.json`, `comparison.json` | real `cal_kernel` stdout evidence and per-check verdict |
| `observed_host.json`, `comparison_host.json` | real `cal_host` stdout evidence and per-check verdict |
| `observed*.stdout.json`, `observed*.stderr.txt` | raw runner output of the last script run |
| `evidence.md` | kernel-equivalence, legacy-validation, Host-chain and remaining-pending evidence |

## What is covered now

* C-CAL-1: `raw=[10,20,-2]`, `bias=2`, `dark=4` containing the bias, `t=2`,
  `flat=2`, `normalization=2` → exact `[4,14,-8]`; the outward enclosure
  contains the exact value; negative values are not clipped; DQ is preserved.
* C-CAL-2: the same maps with `dark_contains_bias=false` → exact `[0,10,-12]`
  (the `D-b` branch is not reused).
* C-CAL-DQ: `flat<=0` → NaN + bad flag + `flat_usable=false`; an existing
  disqualifying flag is not rewritten; a NaN raw value with valid DQ is
  `invalid_model`; with a missing flag it is accepted without a value; an
  overflow is not repaired and does not claim width 0.
* Fixed plan: one stream region (id 0), stages 0/1/2, edges 0→1 and 1→2,
  `cohorts=ceil(N/n)`, `adaptive=false`; `F = 60n + S + 4 KiB`,
  `R = 26N + model + 32 KiB` (memory placement). For `N=200000`,
  `n=65536`: `F=4,984,832 B`, `R=8,432,768 B`; for `N=3`, `n=3`:
  `F=1,052,852 B`, `R=32,894 B`.
* Bounds: `min_block_elements=1`, `max_block_elements=min(N,65536)`,
  `max_window=4`, `allow_spill=true`; out-of-range block/window →
  `invalid_request`.
* C-CAL-H: N=200000 crossing the canonical 1 MiB boundary
  (65536/65536/65536/3392 blocks) exact values, `R=8,432,768 B`,
  `F=4,984,832 B`.
* C-CAL-ROOT, C-CAL-DEV (OFF) and the window=2 ordered composition: same shared
  master root across invocations, deterministic identity, key-ordered sinks.
* Metadata-only validate: positive input and every documented negative
  (exposure, dark exposure, flat normalisation, absent-flat normalisation,
  shape, dtype, DQ dimensionlessness, missing footprints, missing input).
  Validate never opens an array: payload NaN/DQ consistency is the kernel rule.

## Host chain

`cal_host` passes **24/24** after the runtime/CMake integration: typed
invocation through `acsd::generated::register_processes`, the real
`ProcessHost`, Storage writers, the shared kernel and the science Context
bridge. See `evidence.md` for the recorded values, plan R/F and the remaining
pending items (window>1 overlap, injected write/clean failures, DEV ON, spill).
