# x01 photometry.fit non-convergence: numerical diagnosis

Status: **root cause is upstream of the optimizer (sources.psf / sources.measure /
normalization noise).  photometry.fit only mis-classified the resulting
arithmetic state.  No pass is fabricated; x01 normalize still fails, now with
the true condition named.**

Scope of the code changes in this delivery (allowed write scope only):

- `src/photometry/joint_fit.cpp`: scale-free stationarity test and a named
  `precision_exhausted` classification when no representable Gauss-Newton step
  exists. No science algorithm, no ridge, no tolerance cheat.
- `validation/process/photometry.fit/x01_minimal_repro.cpp`: minimal numerical
  counterexample (below).
- Everything else in this document is diagnosis and a proposed fix for the
  owning modules (`src/sources/**`, `src/observation/**`), **not changed here**.

## 1. What x01 actually produces

Temporary trace of `acsd normalize --json` on the x01-00 frame (the trace was
removed after the diagnosis; the numbers are reproduced by the minimal case
below):

| quantity | value |
|---|---|
| native signal range (ADU-like) | `[1960.73, 2769.35]` |
| measured `counts` (7 stars) | `-2.43e16, 7.58e15, 8.05e15, 9.21e15, 9.30e15, -5.43e15, -4.30e15` |
| `variance[m]` | `1` for every star (`variance_known=false`, `flux_variance=null`, `noise_state=unknown`) |
| `flux_low/high` | equal to `counts` (degenerate enclosure, no interval) |
| response integrals `v` | `[2.31879e20, 1.17463e20, 7.91382e19]`, `dv≈[8.0e6,4.1e6,2.7e6]` |
| Gaia latent `c` | `[1,0,0]` per star, no `xp_noise` (equal weights) |
| PSF parameters x01-00 | `[6.37472, 6.90778, -0.0437595]` |
| PSF parameters x01-02 | `[6.90776, 5.09495, -0.173433]` |
| `joint_noise` | `domain=16384` (native), `codomain=8` (7 stars + background), `unknown_terms=0`, `noise_state=unknown` |
| fit state at iter 0 | `n_theta=1, n_meas=7, n_latent=21, chi2=63.3955, whitener=1` |
| score / reduced Jacobian | `1.92018e17` / `1.53191e16` |
| Gauss-Newton step | `-8.18233e-16` |
| `ulp(theta0)` with `theta0≈-10.2` | `2^-49 = 1.7764e-15` |
| Armijo `directional` / RHS at factor 1 | `-314.231` / `-0.0314`; `change=0` for every backtrack |

`theta0 = median(log(counts_i / (v·c_Gaia)))` over the four positive fluxes:
`log(3.72e-5) = -10.199`.  The step `-score/J² = -1.92e17/(1.53e16)² = -8.18e-16`
is **smaller than `ulp(theta0)`**: every candidate `theta + factor*step` rounds
back to `theta`, so `change = 0` while the Armijo RHS is strictly negative.  The
"Armijo backtracking did not find a decreasing step" message is therefore not a
property of the objective; it is the arithmetic hitting the resolution of
`theta`.

`chi2 = 63.4` with unit weights is not a physical misfit either: at the 1e16
flux scale `ulp(1e16)=2`, so the counts rows alone carry `O(10..60)` of rounding
noise, and the 21 latent rows carry the `|1 - c0| ≈ 1` Gaia prior residual for
each star.  The relative residual is `~1e-16`.

## 2. Root cause chain (owning modules and lines)

### 2.1 `sources.psf`: the PSF fit converges to a flat, degenerate profile

The width is fitted in log space and has no physical bound:

- `src/sources/psf_fit.cpp:218` — initial value `theta = log(initial_width_pixel)`
  (`log(1.35) = 0.30`).
- `src/sources/psf_model.cpp:289` — the template evaluates `sx = exp(local[0])`,
  so the returned parameters `[6.37472, 6.90778, ...]` mean
  `sigma_x = 583.5 px`, `sigma_y = 992.8 px` on a 128×128 frame, not widths of
  1.35 px.  x01-02 gives `sigma_x = 997 px`, `sigma_y = 163 px`.
- `src/sources/psf_fit.cpp:456-470` — the rank test uses column-scaled
  singular values with `rank_relative = 1e-12`.  For a flat Gaussian the
  derivative with respect to `log sigma` is proportional to the amplitude column
  over the 12 px window (correlation `1 - O((r/sigma)^2) ≈ 1 - 1e-4`), so after
  column scaling the smallest singular value is `~1e-4`, far above `1e-12`: the
  design is declared full rank even though the width is practically
  unidentifiable.
- `src/sources/psf_fit.cpp:683-689` and `:694-700` — when neither the pure nor
  the damped step improves the objective, the fit sets `converged = true`
  (`local_minimum = true`).  The flat profile + large amplitude + background is
  a genuine local minimum, so the fit is reported as a success.

The result is that x01-00/02/03 receive an almost flat PSF template.  x01-01
reports a rank deficiency from the same fit (different star geometry); that is
the `sources.psf` case, not a photometry rank failure.

### 2.2 `sources.measure`: ill-conditioned flux+background solve

- `src/sources/measure.cpp:340-368` — the design is the pixel-integrated PSF
  template for each source plus a constant background column:
  `design(k,s) = template_values[k*sources+s]`, `design(k,sources) = 1`.
  With `sigma=(583,993) px` the template mass in the 12 px window is
  `erf(6/(√2·583.5))·erf(6/(√2·992.8)) ≈ 4.0e-5`, i.e. `~2.7e-7` per pixel, and
  the seven source columns differ from each other and from the background only
  at the `1e-3..1e-4` level.
- `src/sources/psf_model.cpp:599-618` (`solve_scaled`) — the pseudo-inverse of
  this nearly collinear 8-parameter design returns alternating
  `±1e15..1e16` fluxes for a `2e3` native signal (amplification up to `~5e12`).
  There is no window-mass or condition guard before the solution is published.

### 2.3 `sources.measure` / calibration: the flux variance is never certified and the covariance is not in the flux domain

- `src/sources/measure.cpp:492-499` — `joint_noise` is published with
  `input_output.domain = observation->native_index` (`16384`), while its
  codomain is the flux index of size `8` (7 stars + background), and
  `input`/`output` are set to `observation->noise->variance_unit`.
- `src/observation/calibration_kernel.cpp:234,269` — that variance unit is
  `model::unit_variance(detector_identity(...))`, i.e. a signal-unit squared
  (`linear_power = 2`) whose scale string is the source identity.  The printed
  `var_unit=acsd.native:x01-00` is therefore the unit naming convention, not by
  itself the bug; the bug is the **shape and domain** of the covariance and the
  missing certification.
- `src/sources/measure.cpp:504-521` — the per-star variance is read by calling
  `covariance_apply(joint_noise, e_s)` and taking the `s`-th component.  A
  probe of that call on x01-00 (temporary, removed) returned
  `code=2` (`Code::invalid_model`) with `size=8`, i.e. the applied covariance
  failed for every star, so `variance_known` stayed false and
  `StarFlux.variance.value` is null (`measure.cpp:535-548`), with
  `unknown_terms = {"the_flux_variance_is_not_certified"}`.
- **Mechanism of that failure:** `src/uncertainty/covariance.cpp:103` sizes the
  accumulator in the model space after `I^T` (native, 16384).  The loop at
  `:119-125` then compares the forwarded contribution `I(F F^T I^T v)`
  (flux, 8) with that native accumulator and rejects it as
  `"the influence changes the output size"`.  The check is in the wrong place:
  for a rectangular `I` (flux `8 x` native `16384`) the contributions have to be
  accumulated in the native space and the final `I(.)` forward has to happen
  after the loop.  As written, `covariance_apply` only works for a square
  influence and every flux-domain variance built by `sources.measure` fails —
  this is the direct reason the photometry weights are `1`.
- The published `joint_noise` is not a flux-domain square covariance: its
  domain is the native index, and it includes the background parameter without
  any parameter identity.

### 2.4 `photometry.fit`: consuming the covariance, and the mis-classification

- `src/photometry/joint_fit.cpp:761-770` — the joint counts covariance is used
  only when `noise->input_output.domain->size == n_meas`.  For x01 that is
  `16384 != 7`, so it is skipped (and the 8×8 flux covariance could not be used
  even if the domain metadata were corrected, because the background row/column
  is not one of the 7 measurements).
- `src/photometry/api.cpp:364-370` — the missing per-star variance falls back
  to `1.0` and `variance_known = false`, i.e. equal weights.  This is recorded
  in `FitResult.unknown_terms` (`joint_fit.cpp:853-861`) but the information was
  not part of the old non-convergence message.
- The old stationarity test compared `|score*column_scale|` with `1e-10` and
  `|step*column_scale|` with `1e-12`.  `column_scale` is `|J| ≈ 1.5e16`, so both
  products were `O(1e33)` and could never meet a relative tolerance; the
  iteration could only leave through a line-search failure.

## 3. Photometry fixes applied (this delivery)

`src/photometry/joint_fit.cpp`:

1. Scale-free stationarity (`relative_gradient`, `relative_step`):
   `relative_gradient = max_j |score_j · step_j| / max(1, chi2)` and
   `relative_step = max_j |step_j| / max(1, |theta_j|)`.  These are
   dimensionless and replace the flux-magnitude products.
2. A Gauss-Newton step that cannot change any representable bit of `theta`
   (`theta[j] + step[j] == theta[j]` for all j) with
   `relative_gradient > gradient_tolerance` is classified as
   `precision_exhausted`, with the unknown-variance reason appended.  A zero
   step at an optimum (case B below) is still a convergence.
3. Rank handling is unchanged: the existing path returns
   `Code::not_identifiable` plus a non-empty null space and no ridge.  Case D
   below exercises it (2 collinear scale columns → null space dimension 1).

`src/photometry/api.cpp:487-492` already maps `precision_exhausted` to
`Code::precision_exhausted`; no change was needed there.

Effect on x01 (real CLI):

- Single-frame config (only x01-00, exactly the Armijo case):

```
before: "the Armijo backtracking did not find a decreasing step"
after:  "the Gauss-Newton step is below the floating-point resolution of the
         scale parameters; the precision is exhausted; the measurement variance
         of frame x01-00 is unknown, so the fit used equal weights and cannot
         resolve the scale"
exit code 4 in both cases (no solution is published; nothing is faked)
```

- Full frame set: x01-01 has no measurement (the `sources.psf` rank failure),
so the joint fit has three frames and `n_theta=3`, `rank=3` — full rank — and
the scale-free criteria now converge.  The first publication attempt then
failed with `"the rank report cannot be stored"` because a full-rank design has
`null_dim=0` and `copy_f64` was called with the shape `{0,0}`.  That is a second
photometry publication bug, fixed in `src/photometry/api.cpp:575-593`: the
null-space array is only stored when `null_dim > 0`; it stays absent for a full
rank solution.  After the fix the three-frame fit converges and its scale is applied, so the
A frames reach the publication path.  In the final full run all of them stop in
`src/workflows/fits_writer.cpp:248-250` with
`"the leaf order is below the tile width"`: the X01 fixture asks for
`publication.tile_log2_width = 6` while `drizzle.order = 5`
(`validation/x01/input/cli/normalize.config.json` and
`normalize.scientific.json`), and the writer requires `order >= tile_log2_width`.
That is a fixture/config contradiction downstream of photometry, not a fit
failure; with it fixed the conditional products would be written.  x01-01 keeps
the `sources.psf` rank failure and the B group still fails the blind astrometric
solve.  The full run needs a longer timeout because the successful frames reach
the expensive drizzle/publication path (880 s was enough for the event stream).
None of this makes x01 pass, and the conditional products are not presented as
a certified scale.

## 4. Minimal numerical counterexample

`validation/process/photometry.fit/x01_minimal_repro.cpp` calls the same
`photometry::run_joint_fit` implementation as `fit_photometry` with
hand-computable inputs (7 star latents, one frame, constant basis,
`v = 2.31879e20`, Gaia latent `[1,0,0]`).

Build/run:

```
g++ -std=c++20 -O1 -I include -I src \
    validation/process/photometry.fit/x01_minimal_repro.cpp \
    src/photometry/joint_fit.cpp \
    -Wl,--start-group /tmp/acsd-build/libphotometry.a /tmp/acsd-build/libuncertainty.a \
        /tmp/acsd-build/liboperators.a /tmp/acsd-build/libmodel.a \
        /tmp/acsd-build/libnumerics.a -Wl,--end-group -lpthread -o /tmp/x01_minimal
/tmp/x01_minimal
```

Observed output (exit 0 = the four expectations hold):

```
A_x01_scale_unknown_variance:
  converged=false precision=true chi2=44.395453
  failure="the Gauss-Newton step is below the floating-point resolution ...;
           the measurement variance of frame x01-00 is unknown ..."
B_same_counts_known_variance (same counts, variance=1e16, sigma=1e8):
  converged=true precision=false iterations=3 chi2=7.0
C_normal_magnitude (counts=100, variance=100):
  converged=true chi2=0
D_collinear_scales (2 frames, 1 star, no Gaia information):
  converged=false identifiable=false nullspace=1
  failure="the joint design matrix is rank deficient in the scale parameters"
```

Case A is the x01-00 arithmetic: `theta0 ≈ -10.2`, `J ≈ 1.53e16`, `step
≈ -8.2e-16 < ulp(10.2) = 1.78e-15`, `change = 0` for all Armijo factors, so the
old code reported an Armijo failure while the real state is exhausted precision
with unknown weights.  Case B proves that the *scale* alone is not the problem:
with a variance of the same order as the squared flux the step is representable
(`r/m ≈ 1e-8`) and the fit converges.  Case C is the normal-magnitude control.
Case D proves that a genuinely rank deficient scale block is reported with its
null space and never ridge-regularized.

Hand arithmetic for case A (7 stars, one frame, unit weights):

- `c_Gaia,s = [1,0,0]`, `v = [2.31879e20, ...]`, so the initial model is
  `v·c = 2.31879e20` per star and the ratio for the positive counts is
  `9.0e15/2.31879e20 ≈ 3.9e-5`, median log `≈ -10.2`.
- The reduced (profiled) Jacobian is `J ≈ -sum m^2 / ... ≈ 1.53e16`;
  `score ≈ J·residual ≈ 1.92e17`; `normal_inverse = 1/J² = 4.3e-33`;
  `step = -score/J² ≈ -8.2e-16`.
- `ulp(10.2) = 1.78e-15`, hence `theta + step == theta`; evaluating any
  candidate returns the same `chi2`, `change = 0`, Armijo RHS `-3.1e-2 < 0`.
- With a known variance `sigma² = 1e16` (case B) the residual after the first
  step is `O(1e8)` instead of `O(1)`, `step = O(1e-8)`, and the same code
  converges in 3 iterations.

## 5. Recommended fixes in the owning modules (NOT implemented here)

1. **`src/sources/psf_fit.cpp`** — bound/penalize the log-width to a physical
   range (e.g. `[log(0.5·initial), log(2·initial)]`, or `sigma < window/2`),
   and treat a `local_minimum` whose width sits on that bound, or whose window
   template mass is below a documented threshold, as `not_identifiable` instead
   of a success.  The current `rank_relative=1e-12` on column-scaled columns
   cannot see the flat-profile degeneracy (smallest singular `~1e-4`); a
   condition/curvature check on the width direction is needed.
2. **`src/sources/measure.cpp`** — after the flux+background solve, check the
   template window mass and the design condition; a window mass of `4e-5` and a
   solution of `±1e16` for a `2e3` signal must be `not_identifiable`, not a
   measurement.  If the design is acceptable, publish the covariance in the
   flux domain: `domain = codomain = flux_index` (or a separate star-only
   `n_meas × n_meas` index), so photometry can use it; expose the background
   parameter explicitly if it must stay in the system.  Do not leave
   `input_output.domain = native_index` for a `8×8` flux covariance.
3. **`src/sources/measure.cpp` / `src/observation/calibration_kernel.cpp`** —
   either certify the flux variance (`StarFlux.variance.value` from the
   diagonal of `I C Iᵀ`) or leave it null **and** report the covariance as
   unknown in a way the consumer can act on.  The variance unit itself is
   `unit_variance(signal identity)` (`linear_power=2`) by convention; the
   covariance's output/domain metadata must describe the flux space, not the
   native index, and the printed unit scale is the signal identity string.
4. **`src/uncertainty/covariance.cpp`** — fix the rectangular-influence
   composition: accumulate the per-root contributions in the native space and
   apply the final `covariance->influence` forward once after the loop; the
   `"the influence changes the output size"` check belongs after that forward
   (`:119-125`), not inside the root loop.  With the fix, the probe should
   return `code=0` and the per-star variance becomes available (case B shows the
   fit then converges).
5. **`photometry.fit` (this module)** — after the upstream fixes, re-run x01.
   If the flux-domain covariance is provided, `dense_covariance` will use it
   (domain `== n_meas`); this module must not add a ridge or a tolerance
   fallback to hide an ill-conditioned upstream design.

## 6. Verification record

- `x01_minimal_repro`: 4/4 checks pass, exit 0 (output above).
- Independent `validation/phot/run_validation.sh /tmp/acsd-build`: **ALL CASES
  PASSED** (R1/R2 response, P1 shared latent and cross-frame correlation, P2
  rank deficiency, P3 unknown variance conditional, F01 interval, S1 apply
  scale).  The new stationarity criteria do not change any of those verdicts.
- x01 single-frame `acsd normalize`: exit 4, first failure now
  `precision_exhausted` with the unknown-variance reason instead of the
  misleading Armijo message.
- `photometry/api.cpp` and the rest of `src/photometry` are unchanged except
  `joint_fit.cpp` (diff: 45 insertions, 5 deletions).
- No `git commit`/`git push`.
