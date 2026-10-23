# coadd.integrate evidence (M5)

`coadd_integrate_evidence.cpp` is a real 2-frame × 2-leaf case with the
selection and weights solved once and sealed.  It compares the historical
`coadd()` with the new unique core on the sealed objects and records the
provider call counters, the negative cases, the process validate/bounds/plan
numbers and the real run hook through the runtime science Context bridge.

## Entry points

```cpp
// src/coadd/coadd.hpp (science, unique combination body)
struct CoaddInputs {
  const FramesProvider* frames;
  Ref<SkyFit> sky;
  Ref<Selection> selection;
  Ref<WeightSolution> weights;
  CoaddRequest request;
};
Result<rt::Done> validate_coadd_inputs(const CoaddInputs&);      // metadata only
Result<CoaddResult> coadd_with_inputs(Context&, const CoaddInputs&); // no select/weights

// compatibility entry (select + weights once, then the same core)
Result<CoaddResult> coadd(Context&, const FramesProvider&, Ref<SkyFit>, const CoaddRequest&);
```

The typed `coadd.integrate` adapter builds a `CoaddInputs` from its
`CoaddInput` and calls `coadd_with_inputs` inside the runtime science Context
bridge; the compatibility entry calls exactly the same function.

## Observed results (`run_validation.sh`, 19/19)

* signals: both paths return exactly `[20, 32.5]`; the `SignalEstimate`
  content identity is identical (FP64 bit comparison).
* provider calls for the same fixture:
  * historical `coadd()` = 3 × `list_frames`, 6 × `describe_frame`,
    6 × `read_leaf_block` (select + weights + combination);
  * `coadd_with_inputs` with sealed inputs = **1 / 2 / 2**;
  * the real `run_coadd_integrate` = **1 / 2 / 2**.
  This is the "no second selection/weights" evidence: the only provider pass
  left is the frozen-frame read of the combination.
* negative codes:
  * wrong Selection identity → `identity_conflict` (science core and typed
    validate);
  * missing `WeightSolution` → `invalid_request`;
  * coefficient matrix dimension ≠ leaves × frames → `invalid_request`;
  * requested weight policy ≠ sealed policy → `identity_conflict`;
  * requested rejection policy ≠ sealed policy → `identity_conflict`.
* plan (whole-frame serial core): `R=73,744 B`, `F=1,056,860 B`,
  `work_upper=1,088`, no pipe edge, one cohort;
  `bounds.min_block_elements = bounds.max_block_elements = 2`, `window=1`,
  no spill.
* plan is conservative but honest: the core reads the whole frozen frame set,
  so a smaller block would be a plan fiction; `F` covers frame views
  (11 B per leaf×frame), the coefficient matrix (8 B per pair), the combined
  vector, one 1 MiB workspace and control.  These vectors are not arena-metered;
  the term is an upper bound, not a measured peak.

## Legacy entry regression

`validation/stats/coadd_test` (both groups) and `validation/stats/coadd_loo_test`
pass with the refactored library: `0 failure(s)`.  A null `SkyFit` remains the
historical "no sky applied" case (the combination never applies a sky); the
frozen fit, when present, is carried unchanged to the result.

## Files

* `case.json` — frozen fixture, counters, codes, plan numbers, required checks.
* `observed.json`, `comparison.json` — real evidence and verdict.
* `run_validation.sh` — build/run against `$ACSD_BUILD` (default
  `/tmp/acsd-build`); missing archives are reported as pending, never as pass.
