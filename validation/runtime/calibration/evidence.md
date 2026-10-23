# observation.calibrate (M4) — evidence

## 1. Kernel extraction equivalence (bit-exact)

The old `calibrate_observation` implementation and the refactored
`calibrate.cpp` + `calibration_kernel.cpp` were compiled against the same
independent in-memory fixture and compared on 12 fixed scenarios: C-CAL-1,
C-CAL-2, no masters, flat zero/negative, NaN with missing DQ, NaN with valid DQ
(invalid model), explicit Poisson plug-in, supplied raw + master noise, zero
dark exposure, zero exposure, zero flat normalisation and a 1000-sample
deterministic frame with mixed DQ.

Compared fields: result code and issues, NativeObservation identity,
frame/context, value/lower/upper/dq **bit patterns** (`%016llx`), array
shape/dtype/unit/index, NoiseModel identity/state/condition/roots/influences,
ErrorRecord and the regrouping recipe.

Result: the normalized dumps differ nowhere. The only textual difference is the
fixture's counter-based `Array.object_id`; real Storage derives array ids from
`hash_payload` (content), so allocation order does not change them.
Evidence: `/tmp/kernel-build/{baseline.txt,refactored2.txt,diff2.txt}` (empty
diff).

Legacy observation validation (calibration/regroup/PSF/measurement/bind/timing,
rebuilt from the final refactored sources): **170 checks, 0 failures**, exit 0
(`/tmp/kernel-build/obs_validation_final`).

## 2. `cal_kernel` — kernel + fixed plan + metadata validate (pass)

`run_cal_kernel.sh` → **36/36**, exit 0. Plan numbers recorded:
`N=200000, n=65536, cohorts=4 → F=4,984,832 B, R=8,432,768 B`;
`N=3, n=3 → F=1,052,852 B, R=32,894 B`.

## 3. `cal_host` — driver → generated registry → ProcessHost → Storage → Native (pass)

Full `ninja -C /tmp/acsd-build` is green (exit 0) after the CMake codec/runtime
link fixes. `run_cal_host.sh` → **25/25**, exit 0. The generated manifest from
the same build reports 22 processes / 57 operations and
`tools/check_process_coverage.py` passes; `observation.calibrate` carries
`calibrate_observation` with `acsd.input.Calibrate/1` →
`acsd.NativeObservation/1` and the two declared probes.

| evidence | observed |
|---|---|
| C-CAL-1 through the typed Host chain | `x=[4,14,-8]`, enclosure contains the exact value, DQ unchanged |
| cross-entry identity | `acsd.NativeObservation:5ebea39f…54106` — identical to the synchronous `calibrate_observation` result for the same values/DQ |
| C-CAL-2 (dark without bias) | `x=[0,10,-12]` |
| C-CAL-DQ (`flat=[2,0,2]`) | `value[1]=NaN`, bad flag, named `invalid_model` flat Issue, no fabricated value |
| C-CAL-ROOT | two invocations keep the same shared root `acsd.native/master_flat/1/amplitude` and one influence; no new random root |
| C-CAL-DEV (OFF) | two invocations return the same NativeObservation identity; no probe changes the science result |
| C-CAL-ORDER (window=2, direct module composition) | cohorts 0/1 expanded together, sink order 0→1→2, `x=[4,14,-8]` |
| C-CAL-H (N=200000, blocks 65536/65536/65536/3392) | all sampled values exact, `R=8,432,768 B`, `F=4,984,832 B`, 4 cohorts |
| plan (N=3) | `R=32,894 B`, `F=1,052,852 B` |
| resources | peak host bytes ≈ 10.0 MB for C-CAL-H; work ≈ 204k units (≈ one unit per sample plus storage/codec work) |

## 4. Failure history while wiring the Host chain (kept, not hidden)

1. First Host run returned `invalid_request` from the executor:
   `runtime.task: submit_cpu requires TaskSpec.running.cpu_tokens >= 1` — the
   kernel `submit_cpu` TaskSpec lacked its CPU token. Fixed in
   `calibrate_block` (`spec.running.cpu_tokens = 1`); re-run passed.
2. First C-CAL-H attempt failed with `invalid_model: the source index does not
   cover the frame` because the driver reused the 3-element index for the
   200000-element frame; the test now builds a frame-sized index. Re-run
   passed (this was a test-fixture bug, not a module bug).
3. Before rt-typed made the generated registration a strong reference, the
   weak reference did not pull `calibration_process.o`; the Host silently used
   the M0 shell (`plan: not integrated before M4`). The case now forces a real
   reference (volatile pointer) and rt-typed's strong generated reference keeps
   the real hooks in the production assembly (`nm` shows `T
   acsd::modules::register_calibration`).
4. `nan` in the first host JSON evidence was invalid JSON; the test now records
   the NaN sample as a string classification.

## 5. Still not exercised here (reported, not faked)

* (C-CAL-ORDER is covered: the Host fixes `window=1` by contract, so the
  window=2 ordered composition is exercised directly through the module
  functions; two cohorts are expanded, key 0 is sunk before key 1, and the
  result is exactly `[4,14,-8]`.)
* **C-CAL-WRITE / C-CAL-CLEAN** (short write, seal failure, cancellation in
  write): require injected failures; the runtime verification cases own the
  failure-injection machinery.
* **C-CAL-DEV ON**: this build is `ACSD_DEV_MODE=0`; the ON snapshot/span
  semantics need a runtime built with `-DACSD_DEV_MODE=1` (verify-rt case
  `d01_on`).
* **spill placement**: all Host runs are memory placement; the plan handles
  `Placement::spill`, and the bridge reads find blocks in the block store, so a
  spilled sealed frame would need the bridge to read actual storage blocks
  (currently the memory block store) — a follow-up before claiming spill.
