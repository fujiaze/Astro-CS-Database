# validation/runtime/calibration — observation.calibrate (M4)

This directory owns the frozen C-CAL inputs, the runnable kernel/plan case, the
P1-4 equivalence case and the observed/comparison evidence of
`observation.calibrate`.

| file | role |
|---|---|
| `case.json` | frozen C-CAL-1/2/DQ inputs, the plan constants, required checks and tolerances |
| `cal_kernel.cpp` | in-memory kernel + plan + metadata-only validate case; no Host/pool/Storage |
| `run_cal_kernel.sh` | builds and runs `cal_kernel` (science archives when present, otherwise the minimal source subset) and rewrites `observed.json` / `comparison.json` |
| `case_equivalence.json` | P1-4 matrix: same input, same verdict from the sync entry and the typed hooks |
| `cal_equivalence.cpp` | compares `calibrate_observation` with `validate_calibration`/`bounds_calibration` |
| `run_cal_equivalence.sh` | builds and runs `cal_equivalence`, rewrites its observed/comparison |
| `case_host.json` | frozen Host-chain inputs (C-CAL-1/2/DQ/H/ROOT/DEV/P1-4) and required checks |
| `cal_host.cpp` | driver → generated registry → ProcessHost → Storage → NativeObservation end-to-end case |
| `run_cal_host.sh` | links `cal_host` against `$ACSD_BUILD` and rewrites `observed_host.json` / `comparison_host.json` |
| `observed*.json`, `comparison*.json` | real stdout evidence and per-check verdicts |
| `observed*.stdout.json`, `observed*.stderr.txt` | raw runner output of the last script run |
| `evidence.md` | equivalence, legacy validation, plan/work/allocator, Host chain and remaining follow-ups |

## What is covered now

* C-CAL-1: `raw=[10,20,-2]`, `bias=2`, `dark=4` containing the bias, `t=2`,
  `flat=2`, `normalization=2` → exact `[4,14,-8]`; the outward enclosure
  contains the exact value; negative values are not clipped; DQ is preserved.
* C-CAL-2: the same maps with `dark_contains_bias=false` → exact `[0,10,-12]`.
* C-CAL-DQ: `flat<=0` → NaN + bad flag + `flat_usable=false`; an existing
  disqualifying flag is not rewritten; a NaN raw value with valid DQ is
  `invalid_model`; with a missing flag it is accepted without a value; an
  overflow is not repaired and does not claim width 0.
* P1-4: one metadata validation shared by the synchronous entry and the typed
  hooks (DQ dimensionlessness, positive exposure, explicit absent-flat
  normalisation 1, footprints, shape/index/unit).  The only intentional
  difference is the payload NaN/DQ conflict, rejected after the read by the CPU
  stage because `validate` must not read payload.
* Fixed plan: one *serial* stream region (id 0), stages 0/1/2, **no pipe
  edges**, `window=1` only (window=2 rejected), `cohorts=ceil(N/n)`,
  `adaptive=false`; `F = 60n + S(n) + 4 KiB`,
  `R = 26N + model + 92N materialization + 32 KiB` (memory placement).
  For `N=200000`, `n=65536`: `F=6,623,232 B`, `R=26,832,768 B`; for `N=3`,
  `n=3`: `F=2,166,988 B`, `R=33,170 B`.
* Work bounds: read `5*(64+work_units(8n))`, calibrate `n`, write
  `4*16+4*work_units(8n)+4*16`; the Host case asserts measured work ≤ declared.
* Bounds: `min_block_elements=1`, `max_block_elements=min(N,65536)`,
  `max_window=1`, `allow_spill=true`; out-of-range block/window →
  `invalid_request`.
* C-CAL-H: N=200000 crossing the canonical 1 MiB boundary
  (65536/65536/65536/3392 blocks) exact values.
* C-CAL-ROOT, C-CAL-DEV (OFF): same shared master root across invocations and
  deterministic identity.
* Metadata-only validate: positive input and every documented negative
  (exposure, dark exposure, flat normalisation, absent-flat normalisation,
  shape, dtype, DQ dimensionlessness, missing footprints, missing input).

## Host chain

`cal_host` passes **33/33** after the runtime/CMake integration: typed
invocation through `acsd::generated::register_processes`, the real
`ProcessHost`, Storage writers, the shared kernel and the science Context
bridge.  See `evidence.md` for plan R/F, the measured work/heap numbers and the
remaining follow-ups (spill bridge read, injected write/clean failures, DEV ON,
future pipe credit).
