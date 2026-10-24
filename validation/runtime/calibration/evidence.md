# observation.calibrate (M4) — evidence (P1-4 / P1-8 revision)

## 1. Kernel extraction equivalence

The old `calibrate_observation` and the refactored `calibrate.cpp` +
`calibration_kernel.cpp` were compared on 12 fixed scenarios (C-CAL-1/2, no
masters, flat zero/negative, NaN missing/valid, Poisson plug-in, raw+master
noise, invalid exposure cases, 1000-sample mixed DQ).  For every **successful**
case the result code, NativeObservation identity, frame/context, value/lower/
upper/dq bit patterns, array metadata, NoiseModel, ErrorRecord and regroup
recipe are identical (normalized diff empty).

Two failure scenarios changed intentionally with P1-4, because the synchronous
entry now uses the same validation as the typed process (`exposure_seconds == 0`
is rejected as `invalid_model`; the dark/flat normalisation message text now
comes from the shared function).  Both have `invalid_model`; the older sync
entry accepted a zero exposure and later produced a different result.  This is
the requested single-verdict change, not a regression of a successful
calibration.

Legacy observation validation (calibration/regroup/PSF/measurement/bind/timing)
rebuilt from the revised sources: **170 checks, 0 failures**, exit 0.

## 2. P1-4 — one metadata verdict (pass)

`cal_equivalence` (**8/8**) compares, for the same `RawObservation`/
`Calibration`:

| case | synchronous `calibrate_observation` | typed `validate_calibration` | `bounds_calibration` |
|---|---|---|---|
| valid C-CAL-1 frame | ok | ok | ok |
| DQ unit ADU | invalid_model | invalid_model | invalid_model |
| `exposure_seconds = 0` | invalid_model | invalid_model | invalid_model |
| absent flat, normalization 0 | invalid_model | invalid_model | invalid_model |
| missing footprints | unavailable | unavailable | unavailable |
| raw shape mismatch | invalid_model | invalid_model | invalid_model |
| flat dtype u16 | invalid_model | invalid_model | invalid_model |
| NaN payload with valid DQ | invalid_model (CPU stage) | ok (metadata only) | ok |

The last row is the documented, allowed difference: `validate` must not read
payload; the CPU stage rejects the NaN/DQ conflict after the block hash.

`cal_host` additionally runs five metadata cases through the real typed Host
invocation (`cal.host.p1_4_*`): Host code == typed validate code in all of
them.  The implementation is the single
`observation::validate_calibration_input(raw, request)` called by both
`calibrate_observation` and `validate_calibration`/`bounds`/`plan`.

## 3. P1-8a — workspace is a function of n (not a 1 MiB constant)

`S(n) = 2 * 1 MiB (canonical read staging + decompression) + 8n (append encode
vector) + 64 KiB (hash/zstd/control state)`.

Supporting allocator measurement in `cal_host` for the N=200000 run (plain
`operator new/delete` metering only; the arena uses aligned new and is
deliberately not counted): unmanaged heap peak **14,849,040 B**, below the
declared `92 * N = 18,400,000 B` materialization term.  This is recorded as
support, **not** as a proof of the peak; the arena-side peak is separately
reported by the ledger.

## 4. P1-8b — work bounds cover real read/write/assembly work

Per cohort the plan declares:

* read: `5 * (64 + work_units(8n))` (five correlated read reservations),
* calibrate: `n` (one unit per native sample),
* write: `4*16 (begin) + 4*work_units(8n) (append) + 4*16 (seal)`.

`cal_host` records the real `usage.work` delta and asserts it is covered:
N=200000 → measured **201,906**, declared **268,580** (4 cohorts).  No test
relaxes an equality to `>=`; the check is "measured ≤ declared upper bound".

## 5. P1-8c — no unused pipes

The current composition is serial.  The plan therefore declares:

* **no region edges** (an empty edge list), and
* `PlanOptions.window == 1` only; `window = 2` is rejected with
  `invalid_request`.

`cal_kernel`/`cal_host` assert `edges.empty()` and the window rejection.  The
stage graph keeps stages 0/1/2 with precedence for diagnostics.  A future pipe
integration adds edges together with real `make_pipe`/permit handling.

## 6. Host chain and C-CAL matrix (pass)

`run_cal_host.sh` → **33/33**, exit 0.

| evidence | observed |
|---|---|
| C-CAL-1 | `x=[4,14,-8]`, enclosure contains the exact value, DQ unchanged |
| cross-entry identity | `acsd.NativeObservation:5ebea39f…54106`, identical to the synchronous entry |
| C-CAL-2 | `x=[0,10,-12]` |
| C-CAL-DQ | `flat=0` → value NaN, bad flag, named `invalid_model` flat Issue |
| C-CAL-H | N=200000, blocks 65536/65536/65536/3392, exact values; plan `R=26,832,768 B`, `F=6,623,232 B` |
| C-CAL-ROOT | two invocations keep the same shared root `acsd.native/master_flat/1/amplitude`, one influence |
| DEV OFF | two invocations return the same identity; no probe changes science |
| plan (N=3) | `R=33,170 B`, `F=2,166,988 B` |
| resources | peak host bytes ≈ 10.0 MB for C-CAL-H; measured work 201,906; written 7,200,708 B |

## 7. Unaccounted materialization (declared, not claimed verified)

The plan comment lists the `std::vector` copies that are not arena-metered and
charges them as `92 bytes/sample` to R:

* synchronous entry: raw 8N, DQ 2N, bias 8N, dark 8N, flat 8N, value/lower/
  upper 24N, DQ 2N, inverse_flat 8N, assembly flat/raw/bias copies 24N;
* runtime `finish_native` reads back value 8N, DQ 2N, flat 8N, raw 8N, bias 8N
  plus the shared assembly copies (66N, below the same 92N term);
* the noise-model weight vectors and LinearAction diagonal copies are charged
  separately in `noise_model_bytes()`.

The allocator measurement above is the only measured support; the peak is not
claimed verified.

## 8. Still not exercised / follow-ups

* **spill placement**: the science bridge reads from the in-memory block store;
  spilled sealed arrays would need the bridge to read storage blocks before
  spill can be claimed.
* **C-CAL-WRITE / C-CAL-CLEAN**: separate writer-layer injection evidence
  exists under `validation/process/calibration-faults` (reported 16/16 in
  the anchored M0–M4 follow-up review). This is **not** a current re-run of
  the complete calibration adapter → begin/seal four-writer chain. That
  adapter-level path remains a separately required injection case; do not
  mark it certified solely from writer unit fixtures.
* **DEV ON**: the calibration evidence in this file used `ACSD_DEV_MODE=0`;
  a distinct `validation/runtime/d01_on` DEV=1 case is recorded as pass
  in the runtime manifest. This does not imply the entire calibration Host
  science path was re-run with every DEV snapshot enabled.
* **pipe credit**: this particular calibration plan is intentionally serial
  (`edges=0`); Runtime now has Pipe/permit facilities in other paths.
  Multi-stage calibration Pipe/credit integration and low-H progress still
  need their own positive/negative fixtures. Do not say the Pipe mechanism
  itself does not exist.
