# Code-vs-Doc Inventory: Noise / Variance / SNR

**Scope**: what the CODE in ACSD actually implements for noise, variance and SNR, so it can be
diffed against `docs/science/noise_snr/NOISE_SNR.md`.

**Method**: read-only. All commands below are re-runnable from the repo root
`/workspace/Astro CS Database`. Every claim is tagged **[VERIFIED]** (read the file) or
**[INFERRED]**. No git write operation was performed.

---

## 1. Code tree map

```bash
find . -maxdepth 2 -type d -not -path './.git*' -not -path './build/*' \
  -not -path './out/*' -not -path './run/*' -not -path './testdata/*' | sort
```

| Path | Verdict | Evidence |
|---|---|---|
| `lib/` | **Real code.** 274 `.h`, 223 `.cpp`, 98 `.c`, 34 `.py`, 100 `.md` | `find lib -type f \| sed 's/.*\.//' \| sort \| uniq -c \| sort -rn` |
| `lib/algorithms/` | **Real code.** 19 sub-modules incl. `noise_snr/`, `drizzle/`, `coverage/`, `photometry/`, `integration/`, `resample/` | `ls lib/algorithms` |
| `lib/algorithms/noise_snr/` | **Real code, 6,624 lines** across 15 code files | `find lib/algorithms/noise_snr -type f` + `wc -l` |
| `eng/` | Mostly **Python tooling/tests/contracts** (118 `.py`, 141 `.json`, 21 `.md`); only 1 `.c`, 3 `.h` | `find eng -type f \| sed 's/.*\.//' \| sort \| uniq -c` |
| `src/` at repo root | **Does not exist.** Source lives under `lib/algorithms/*/` and `lib/infrastructure/*/` | root `ls -la` |
| `实验/` | **Real runnable experiment code** (133 `.py` in `absolute-snr` alone) | `find 实验 -type f` |

**This is emphatically NOT a docs-only repo.** The noise/SNR science is implemented in
production C++.

### 1.1 `lib/algorithms/noise_snr/` inventory [VERIFIED]

```bash
find lib/algorithms/noise_snr -type f | sort
wc -l lib/algorithms/noise_snr/cpp/src/*.cpp lib/algorithms/noise_snr/wrapper_phase1/*.cpp \
      lib/algorithms/noise_snr/cpp/include/*.h lib/algorithms/noise_snr/include/acsd/noise/*.h
```

| File | Lines | Role |
|---|---|---|
| `cpp/src/noise_model.cpp` | 1482 | `NoiseWeightModelV1` builder + fill; robust variance |
| `cpp/src/snr_science.cpp` | 282 | **The** per-source SNR science core (Horne 1986) |
| `cpp/include/snr_estimator.h` | 650 | C ABI surface, all parameter structs |
| `src/module_entry.cpp` | 1635 | scheduler adapter (config → `SnrNoiseModelConfig`) |
| `cpp/src/information_weight.cpp` | 320 | `W_info` / `A_NEA` point information |
| `wrapper_phase1/snr_frame_science.cpp` | 214 | frame-level aggregation (zero formula copies) |
| `cpp/src/snr_estimator.cpp` | 932 | **legacy** multiplicative SNR — diagnostic only |
| `include/acsd/information_weight.h` | 127 | W_info/A_NEA header |
| `include/acsd/noise/{types,variance_plane_policy,saturation_policy}.h` | 204/139/70 | policy types |

### 1.2 Which of these are actually COMPILED [VERIFIED]

```bash
grep -n "noise_snr\|snr_science\|acsd_phase1_noise" CMakeLists.txt
```

`CMakeLists.txt:1105-1108` — the production target:

```cmake
add_library(acsd_phase1_noise STATIC
  lib/algorithms/noise_snr/cpp/src/noise_model.cpp
  lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp
  lib/algorithms/noise_snr/cpp/src/snr_science.cpp)
```

- `lib/algorithms/noise_snr/CMakeLists.txt` (the SHARED `acsd_p1_noise` subgraph) is
  **commented out** of the root graph — see its own header comment lines 26-31.
- `cpp/src/snr_estimator.cpp` (legacy multiplicative SNR) is **not** in the production graph.
  The submodule CMakeLists says so explicitly at line 16-17.
- `cpp/src/information_weight.cpp` is **not** in `acsd_phase1_noise` but **is** compiled by two
  other targets:
  ```bash
  grep -n "information_weight" lib/algorithms/integration/phase1_product/CMakeLists.txt \
                             lib/algorithms/integration/phase2_integrate/CMakeLists.txt
  # phase1_product/CMakeLists.txt:28
  # phase2_integrate/CMakeLists.txt:46
  ```
- `information_weight.cpp` / `snr_estimator.cpp` references inside `run/FINAL-07-e2e/bisect/…`
  are **archived run snapshots**, not the live tree.

---

## 2. Answers to the specific questions

### (a) Is there a function that computes SNR? What is the formula?

**YES — several, in two different algebraic forms.**

**(a1) Per-source science SNR — Horne 1986 optimal extraction.**
`lib/algorithms/noise_snr/cpp/src/snr_science.cpp:151` — `snr_source_snr_f64`

```cpp
// snr_science.cpp:202-215
var_f += (Pi * Pi) / var_i;      // σ_F^-2 = Σ_i P_i²/σ_i²   (Horne 1986)
var_f = (var_f > 0.0) ? (1.0 / var_f) : 0.0;   // Var(F) = 1/σ_F^-2
...
out->sigma_f_optimal_adu = std::sqrt(var_f);
out->snr_optimal = F / out->sigma_f_optimal_adu;   // SNR = F/σ_F
```

- **Numerator is a source flux** `F` in ADU (`flux_adu`, `snr_estimator.h:327`). **Not** a
  surface-brightness level. **Not** a square root.
- **Form is F/σ**, not F²/σ².

**(a2) Aperture path (CCD equation) — same file, also F/σ.**
```cpp
// snr_science.cpp:228-235
double var_ap = n_pix * sig_sky * sig_sky * (1.0 + n_pix / n_sky);
if (p->gain_e_per_adu > 0.0 && s_ap > 0.0) var_ap += s_ap / p->gain_e_per_adu;
...
out->snr_aperture = s_ap / std::sqrt(var_ap);
```
(Moffat4 β=4 analytic enclosed fraction `f_in = 1 − (1+r²/2σ²)⁻³` at line 224.)

**(a3) A third, purely diagnostic peak SNR.**
```cpp
// snr_science.cpp:216
out->snr_peak = F * p_center / sig_sky;
```
Header marks it `仅诊断, 不得作科学输出` (`snr_estimator.h:351`).

**(a4) F²/σ² form — in the Phase2 weight chain, not in `noise_snr/`.**
```bash
grep -n "w = \|snr / reference_flux" lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp
```
```cpp
// weight_chain.cpp:104   (frame-level scalar)
const double w = (snr / reference_flux) * (snr / reference_flux);
// weight_chain.cpp:196-201 (per-pixel)
if (!weight_from_snr(layer_snr, in.ref_flux_k, &w, &werr)) { ... }
w *= g * g;
```
i.e. **`w = (SNR/F_ref)² · g²` ≡ 1/σ_F²**. Documented as an exact identity at
`lib/algorithms/integration/phase2_integrate/include/acsd/weight_chain.h:5-6` and `:27`.
This is the *same* physics as (a1), expressed in the variance-ratio (information) form.

**Summary for (a):** the code computes **SNR = F/σ_F** (Horne optimal extraction, source flux
numerator, ADU), and downstream converts to **SNR²/F_ref²** for inverse-variance weighting. A
doc describing "SNR" as F²/σ² would match `weight_chain.cpp` but not `snr_science.cpp`.

---

### (b) Is there a variance model? What terms? Where does read noise appear?

**YES — four distinct, separately-implemented variance expressions.** [VERIFIED]

**(b1) Empirical blank-sky robust variance — the production baseline.**
`lib/algorithms/noise_snr/cpp/src/noise_model.cpp`

```cpp
// noise_model.cpp:112-120
// 稳健尺度: 1.482602218505602 × median(|x − median(x)|)
double robust_sigma(std::vector<double> v) {
    const double med = robust_median(v);
    ... dev.push_back(std::fabs(x - med));
    return 1.482602218505602 * robust_median(dev);
}
```
Terms: **σ_bg² only** — no Poisson term, no read-noise term. Source-masked, blank-sky patches.
Declared as the production baseline at `snr_estimator.h:116-128`.
Spatial form: least-squares plane `var(x,y) = a + b·x + c·y` with relative-error weighting and
convex-hull non-negativity (`noise_model.cpp:1325-1338`).

**(b2) Poisson + read-noise model — DIAGNOSTIC cross-check only.**
```cpp
// noise_model.cpp:1472-1480  (snr_noise_gain_variance)
if (gain_e_per_adu <= 0.0) return 0.0;
const double s = std::max(0.0, signal);
return s / gain_e_per_adu +
       (read_noise_e * read_noise_e) / (gain_e_per_adu * gain_e_per_adu);
```
- **Read noise appears as `(RN/g)²`** — the `(RN/g)^2` option in the question.
- **Gain is in e⁻/ADU** (`gain_e_per_adu`, `snr_estimator.h:135`, `:308`).
- **NOT** `RN^2`, **NOT** `RN²/g²`, **NOT** `0.5·g²`.
- Header contract at `snr_estimator.h:282-286`.

**(b3) Per-pixel Horne variance (inside the SNR function).**
```cpp
// snr_science.cpp:139-143 (contract), :197-205 (implementation)
double var_i = sig_sky * sig_sky + rn_term;
if (si > 0.0) var_i += si / p->gain_e_per_adu;
```
- **Source Poisson term appears as `F·P_i / g`** (in ADU space), plus `σ_sky²`, plus `(RN/g)²`.
- There is an explicit **read-noise double-count guard**: the caller must declare whether
  `sigma_sky` is shot-only or an empirical *total* rms.
  ```cpp
  // snr_science.cpp:176-181
  const bool rn_in_sky = (p->sigma_sky_source == SNR_SIGMA_SKY_EMPIRICAL_TOTAL_RMS);
  const double rn_term = (!rn_in_sky && p->read_noise_e > 0.0)
                             ? (p->read_noise_e / p->gain_e_per_adu) *
                               (p->read_noise_e / p->gain_e_per_adu)
                             : 0.0;
  ```
  Enum at `snr_estimator.h:321-323`; rationale (double count inflates σ_F by +12.8%…+34.0%)
  at `snr_science.cpp:175-176`.

**(b4) Aperture CCD equation.**
```cpp
// snr_science.cpp:228-229
double var_ap = n_pix * sig_sky * sig_sky * (1.0 + n_pix / n_sky);
if (gain > 0 && s_ap > 0) var_ap += s_ap / gain;    // = S_ap/gain
```

**Noise propagation law** (scale invariance), `noise_model.cpp:1463-1470`:
```cpp
*variance = (*variance) * alpha * alpha;   // x' = αx  ⇒  var' = α² var
*ivar     = (*ivar) / (alpha * alpha);
```

---

### (c) Is there MAD-based robust scaling? What constant?

**YES — `1.482602218505602`, used throughout.** [VERIFIED]

```bash
grep -rn "1\.4826\|0\.6745\|0\.7316728" --include=*.c --include=*.h --include=*.cpp \
  --include=*.hpp lib/ eng/
```

| Constant | Meaning | Primary site |
|---|---|---|
| `1.482602218505602` | **MAD → σ** (the blank-sky robust scale) | `noise_model.cpp:119` |
| `0.6744897501960817` | **σ → MAD**, i.e. used as `σ = MAD / 0.6745…` | `star_matcher.cpp:21`, `spatial_gain.cpp:41` |
| `0.7316727929211932` | trimmed-mean-abs-residual → σ (**not** a MAD) | `noise_model.cpp:95`, `snr_science.cpp:56`, used at `noise_model.cpp:1232` |
| `1.253` | zero-point standard error `= √(π/2)` | `snr_science.cpp:279` |

**Exact arithmetic check:** `1 / 0.6744897501960817 == 1.482602218505602` (verified in Python,
exact in double precision). So the codebase uses **two spellings of the same constant**:
`1.4826…·MAD` (noise / cosmetic / dark / scheduler) and `MAD/0.67448975…` (photometry).
A doc-vs-code diff should accept both.

Other `1.4826…` sites:
```bash
grep -rn "1\.482602218505602" --include=*.cpp --include=*.h lib/
```
- `lib/algorithms/calibration/src/cosmetic_corrector.cpp:126,147,373`
- `lib/algorithms/calibration/src/dark_optimizer.cpp:131,276,329`
- `lib/infrastructure/scheduler/src/module_adapters.cpp:11082`
- `lib/infrastructure/benchmark/cpu/baseline/src/baseline_provider.cpp:320,335`

**Important trap the code itself documents:** the PSF block column index 7 is *named* `mad`
but is actually a **10-90% trimmed mean absolute residual**, not a median absolute deviation.
```cpp
// snr_estimator.h:83-89
// status(0) B(1) flux(2) cx(3) cy(4) fwhm(5) A(6) mad(7) eccentricity(8)
// 第 8 列 (index 7) 历史称 "mad", 实际是 10-90% trimmed mean absolute residual
// (非真 MAD), 权威语义 residual_scale,
// robust_residual_sigma — residual_scale / 0.731673 (Gaussian 假设)
```
The old `(A−B)/mad` control-point formula is **retired** (SNR-008) per `snr_estimator.h:44-45`
and `snr_science.cpp:10`.

---

### (d) Fisher matrix / point information / NEA?

**Partly.** No literal `Fisher` *implementation*; but real point-information / A_NEA code and
real information-matrix (JᵀWJ) machinery exist. [VERIFIED]

```bash
grep -rni "fisher\|A_NEA\|point_information" --include=*.cpp --include=*.h lib/ eng/
```

**(d1) Point information / A_NEA — implemented.**
```cpp
// lib/algorithms/noise_snr/cpp/src/information_weight.cpp:231-232
out.a_nea = 1.0 / sum2;                       // A_NEA = 1 / Σ_p P_p²   [px²]
out.w_info = a * a / (sigma_pix2 * out.a_nea); // W_info = a²/(σ_pix²·A_NEA)
```
Gated to the white-noise condition (diagonal `C` **and** declared `σ_pix`):
```cpp
// information_weight.cpp:190-205  (white_noise_gate / w_info_white_noise)
if (!gate.allowed()) { out.reject = "white_noise_condition_not_met"; return out; }
```
Header contract `lib/algorithms/noise_snr/include/acsd/information_weight.h:8-9`
(`FZ-COND-WHITENOISE`).

Also implemented at `lib/algorithms/psf/src/psf_information.cpp:54` (`s.a_nea = 1.0/sum2;`).

General (non-white-noise) GLS point-information solves — diagonal, dense-SPD and low-rank
`D + L Lᵀ` covariance — dispatch at `information_weight.cpp:177-188`.

**(d2) True information-matrix machinery — in `lib/algorithms/coverage/` (UPM), not `noise_snr/`.**
```bash
grep -rn "J^T W J\|JᵀWJ\|H_eq" --include=*.h --include=*.cpp lib/algorithms/coverage/
```
```cpp
// lib/algorithms/coverage/include/astro/phase2/upm.h:252-254
//   H_eq = D^-1 (J^T W J) D^-1,  D = diag(sqrt(H_ii))
//   r_eff = #{ λ_i(H_eq) > τ·λ_max }，κ = λ_max/λ_min
//   identifiable ⟺ r_eff == n_free ⟺ κ < 1/τ
// upm.h:265-266
//   C_theta = (J^T W J)^-1 ; C_out = C_stat + J_out C_theta J_out^T
```
Real numerical implementation of the covariance propagation:
```cpp
// lib/algorithms/coverage/src/upm.cpp:2935-2944
for (a …) for (b …) {
    double s = C_stat[a*ld_C + b];
    for (i …) { jai = J_out[a*ld_J + i]; … for (j …)
        s += jai * mm->C_theta[i*n+j] * J_out[b*ld_J+j]; }
    out_C_out[a*ld_out + b] = s;
}
```

**(d3) What `fisher` actually is in this repo:** a **string token** used by fail-closed
validators to *reject* weight-object provenance, not a matrix implementation:
- `lib/algorithms/calibration/src/calibration_covariance.cpp:690`
- `lib/algorithms/resample/p3_rsmp_failclosed.cpp:46`
- `lib/algorithms/integration/phase1_product/src/phase1_product.cpp:83`
- `lib/algorithms/drizzle/healpix_drizzle/drizzle_science.cpp:120`

**Verdict for (d):** no `Fisher` matrix function; yes to `A_NEA` / `W_info` / `(JᵀWJ)⁻¹`.

---

### (e) Drizzle variance propagation?

**YES.** [VERIFIED]

**Frozen formula:**
```cpp
// lib/algorithms/drizzle/healpix_drizzle/drizzle_science.h:31-35
//   FZ-FORMULA-DRIZZLE-VAR
//       variance_p = Sum_j c_jp^2 v_j = Sum_j v_j w_jp^2 / N_p^2
//       x -> alpha x  =>  var -> alpha^2 var , ivar -> ivar / alpha^2
//   FZ-FORMULA-COV-PROP
//       Cov(S_p,S_q) = Sum_j c_jp c_jq v_j ; C_out = R C_in R^T
```
with `w_jp = a_jp / A_drop_j`, `c_jp = w_jp / N_p`, `N_p = Σ_j w_jp·A_pixel_j`
(`drizzle_science.h:26-30`).

**Actual accumulation:**
```cpp
// lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1430-1435
// 方差传播 (SCI-DRZ-014 / ALG-DRZ-VAR): sumVarNum += v·w²,
//                                     var = sumVarNum/sumNorm², ivar = 1/var
if (varianceValue > 0.0f) {
    const Scalar w2 = weight * weight;
    acc.sumVarNum += Scalar((double)varianceValue * (double)w2);
}
```
Normalisation (`k = 1/N_p`):
```cpp
// lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp:193, 549
dense_var[local] = Scalar((double)acc.sumVarNum * k * k);
```
Accumulator declaration: `drizzle_engine.h:55-57, 81`.
Input is the per-pixel `"variance"` block produced by the SNR stage:
`lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp:1004-1011`.

**Verdict:** the code implements the **diagonal** part (`Σ_j v_j w_jp² / N_p²`) concretely;
`C_out = R C_in Rᵀ` appears as a *contract string and gate*, and the full dense form is
implemented only in the coverage UPM (d2 above), not in the drizzle kernel.

---

### (f) SNR → magnitude conversion? Magnitude constants?

**YES.** [VERIFIED]

```bash
grep -rn "2\.5 \* std::log10\|1\.0857" --include=*.cpp --include=*.h lib/
```

```cpp
// snr_science.cpp:241   (per-source 5σ depth)
out->m5_mag = p->zero_point_mag - 2.5 * std::log10(out->flux5_adu);
// snr_science.cpp:257-261   (frame-level 5σ depth, snr_frame_depth_f64)
const double f5 = 5.0 * reference->sigma_f_optimal_adu;
*out_m5_mag = zero_point_mag - 2.5 * std::log10(f5);
```
- **Magnitude constant = `2.5`** (i.e. 1/0.4).
- **`1.0857` — NOT FOUND as a literal anywhere in `lib/` or `eng/`** (verified negative).
- `F_5 = 5.0 * σ_F` at `snr_science.cpp:217` and `:257`.
- `kLn10 = 2.302585092994045684017991454684` at `noise_model.cpp:92`.
- Related: `σ_mag = 2.5 × σ_logflux_dex` (`snr_estimator.h:64`), applied at
  `wrapper_phase1/snr_frame_science.cpp:87`.

---

### (g) Hardcoded numeric thresholds

[VERIFIED — every row checked against code AND against `eng/packaging/config/defaults.json`]

| Threshold | Value | file:line | Configurable? |
|---|---|---|---|
| cosmic clip sigma | `5.0` | `noise_model.cpp:1252`; used `:161-162` | yes — `noise.clip_sigma` |
| max clip rounds | `2` | `noise_model.cpp:1254` | yes — `noise.max_clip_rounds` |
| patch grid | `8 × 8` | `noise_model.cpp:1248-1249` | yes — `noise.patch_grid` |
| **λ_lo/λ_hi plane-geometry floor** | **`0.0625` = 1/16** | **`noise_model.cpp:183`**; used `:1114` | **NO — hardcoded** |
| mask k (residual SB at mask edge) | `0.1` | `noise_model.cpp:826`, `:1258` | yes — `noise.mask_k_sigma` |
| **r_min** | **`1.5` px** | `noise_model.cpp:827`, `:1259` | yes — `noise.mask_r_min_px` |
| r_min FWHM term | `0.75·FWHM` | `noise_model.cpp:828`, `:1260` | yes — `noise.mask_fwhm_floor_scale` |
| rmax cap | `10 × 6 = 60` px | `noise_model.cpp:824-825`, `:1250-1251` | yes — two keys |
| mask Moffat β | `2.5` | `noise_model.cpp:642` | **NO — hardcoded** |
| min patch samples | `64` | `noise_model.cpp:1253` | yes — `noise.min_patch_samples` |
| variance floor | `1e-12` | `noise_model.cpp:1256` | yes — `noise.variance_floor` |
| sky budget | `9216` px / `8` patches | `noise_model.cpp:829-830` | yes — two keys |
| spatial-field min control points | `≥ 4` | `noise_model.cpp:1114`, `:1325` | **NO — hardcoded** |
| Gaussian FWHM factor | `2.3548200450309493` | `snr_science.cpp:50` | **NO** |
| Moffat4 FWHM factor | `1.230310` | `snr_science.cpp:54` | **NO** |
| zero-point SE | `1.253·σ/√N` | `snr_science.cpp:279` | **NO** |
| 5σ depth | `F_5 = 5·σ_F` | `snr_science.cpp:217`, `:257` | **NO** |
| default aperture radius | `1.5·FWHM` | `snr_science.cpp:220-221` | yes (`aperture_radius_px`) |
| profile grid half-px | `max(30, ceil(12·FWHM))`, cap 256 | `snr_science.cpp:98-103` | yes (`profile_half_px`) |
| IDW power | `2.0` | **ONLY in archived legacy code** — `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/gradient/snr_evaluator.cpp:197` and `.h:109`. **NOT in the live build.** | n/a |
| **"target 1.2 %"** | **NOT FOUND in any noise/SNR context** | see negative check below | n/a |

**Negative check for "1.2 %":**
```bash
grep -rn "0\.012\|1\.2%\|1\.2 %" --include=*.c --include=*.h --include=*.cpp --include=*.hpp lib/
```
Only three hits, all **unrelated** — `star_mask_radius_deg` (a *coverage* veto radius in
**degrees**): `lib/algorithms/coverage/src/sampler.cpp:318` and `:552`;
declared `lib/algorithms/coverage/include/astro/phase2/sampler.h:58`.
**There is no 1.2 % SNR/noise target in the code.**

**Note on `kappa`:** the `kappa` in this repo is an *identifiability* condition number, not a
MAD constant — `lib/algorithms/coverage/include/astro/phase2/upm.h:253-260`
(`FZ-AP2S-KAPPA-MAX=1e6` is **retired**; live criterion is `rank_rtol = 1e-10`).

---

## 3. Configuration surface

```bash
ls eng/packaging/config/
python3 -c "import json; ..." eng/packaging/config/defaults.json
```

`eng/packaging/config/defaults.json` holds every noise parameter, and — importantly for a
doc-vs-code diff — **each key carries a `source_ref` anchor that quotes
`docs/science/noise_snr/NOISE_SNR.md` verbatim**. Re-runnable:
```bash
grep -n "NOISE_SNR.md" eng/packaging/config/defaults.json | head -20
```

| Key | Value |
|---|---|
| `noise.patch_grid` | `[8, 8]` |
| `noise.source_mask_radius_px` | `10` |
| `noise.mask_radius_scale` | `6` |
| `noise.clip_sigma` | `5.0` |
| `noise.min_patch_samples` | `64` |
| `noise.max_clip_rounds` | `2` |
| `noise.saturation_level` | `0` (= "unset", **not** "no saturation") |
| `noise.spatial_field_enabled` | `1` |
| `noise.variance_floor` | `1e-12` |
| `noise.mask_k_sigma` | `0.1` |
| `noise.mask_r_min_px` | `1.5` |
| `noise.mask_fwhm_floor_scale` | `0.75` |
| `noise.mask_budget_min_patches` | `8` |
| `noise.mask_budget_min_sky` | `9216` |
| `sparse_snr.spacing_px` | `64` |
| `snr.path` | `sparse_reconstruct` |
| `detection.threshold_sigma` | `5.0` |

**Gain and read noise have NO default.** `eng/packaging/config/config_registry.json:208-224`
registers both with `"declared_default": null` and the note
`"由帧头元数据或显式覆盖提供；无默认。"` (supplied by frame-header metadata or explicit
override; no default). `read_noise` unit is recorded as `e⁻`
(`config_registry.json:212`).

The phase-config templates carry **no** gain/read-noise/snr block:
```bash
grep -n -i "gain\|read_noise\|snr\|noise" eng/packaging/config/templates/*.json
# only: mosaic.phase_config.json:8:  "snr_path": "sparse_reconstruct"
```

---

## 4. Test coverage — a genuine gap

```bash
ls eng/tests
ls eng/tests/conformance
grep -rln "gtest\|catch2\|Catch2\|add_test" --include=CMakeLists.txt --include=*.cmake lib/ eng/
```

[VERIFIED]

- `eng/tests/conformance/` contains **only** `noop`. The root CMakeLists registers it at
  line 324 (`add_subdirectory(eng/tests/conformance/noop)`).
- **No** gtest / Catch2 / `add_test` anywhere in `lib/` or `eng/` (zero grep hits).
- **`lib/algorithms/noise_snr/tests/` does not exist** in the live tree, though it once did:
  ```bash
  git log --oneline --diff-filter=D -1 -- lib/algorithms/noise_snr/tests/p1noise/CMakeLists.txt
  # c0bb707d 全域移除机器门禁，并把文档引用改为论文格式角标
  ```
- `eng/tests/unit/` (referenced by the archived `run/FINAL-07-e2e` snapshot) no longer exists.

What *does* exist is Python validation harnesses under `eng/tests/validation/release02/`:
```bash
grep -rln "snr\|SNR\|variance" eng/tests | head -30
```
Notably `q2_snr_smoothness/`, `fix_p2b_variance_oracle/`, `unc_propagation/`,
`q3_additive_truth/`, `q1_photometry_gradient/`, `phot_verify/`.

**Verdict:** there are **no C++ unit tests** covering the noise/SNR module. The only C++ SNR
test file in the tree is `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/gradient/test_snr_evaluator.cpp`,
which targets the **archived legacy** evaluator, not the production core.

---

## 5. Sparse → dense SNR reconstruction (P4)

```bash
grep -rln "dense_snr\|sparse_reconstruct\|reconstruct_snr" --include=*.cpp --include=*.h --include=*.py lib/ eng/
```

[VERIFIED] The **reconstruction algorithm** is in
`lib/algorithms/integration/phase2_integrate/` (`weight_chain.h/.cpp`), not `noise_snr/`.

```cpp
// weight_chain.h:133-138
enum class SparseReconOperator : int {
  kNaturalBicubicSplineClip = 0,
  kNaturalBicubicSplineClipMeshMedian = 1,
  kBilinearRegularGrid = 2,
  kNearestControlPoint = 3,
};
```
- Operator chosen by **data source**, not inferred from the control grid:
  `sparse_recon_operator_for_source(high_contrast_unresolved_sources)` — `weight_chain.h:148-154`.
- Fail-closed domain policy (no extrapolation, no frame-level fallback) — `weight_chain.h:166-172`.
- Scheduler routing: `lib/infrastructure/scheduler/src/module_adapters.cpp:11984-12201`.
- **Explicitly NOT wired for this chain:** `module_adapters.cpp:12207-12212` carries a
  registered gate anchor reading `稀疏 SNR 层尚未接入生产数据面` ("the sparse SNR layer is not
  yet connected to the production data plane").

---

## 5b. Spot cross-check against `docs/science/noise_snr/NOISE_SNR.md` itself

I ran a *targeted formula grep only* (the full doc read is the parent's job). Result: on
every constant I checked, **the doc and the code already agree**, and the doc is unusually
precise about the two points I flagged as risk.

```bash
grep -n "Horne\|SNR_F\|σ_F\|1.4826\|0.6745\|1\.253\|A_NEA\|W_info\|Fisher\|RN/g" \
  docs/science/noise_snr/NOISE_SNR.md | head -30
```

| Doc statement | Code | Verdict |
|---|---|---|
| `:19` `SNR = F_ref/σ_F` | `snr_science.cpp:215` `F/σ_F` | **consistent** (numerator question below) |
| `:65` read noise `(RN/g)²` | `noise_model.cpp:1478-1479` | **exact match** |
| `:117` `sigma_bg = MAD·kappa`, `kappa = 1/Phi^{-1}(3/4) = 1.482602218505602` | `noise_model.cpp:119` | **exact match** |
| `:121` states `1/0.6744897501960817` and `1.482602218505602` are **bit-identical** in IEEE-754 | my Python check confirms; both spellings exist in code | **exact match** — the doc already covers the two-spelling point I raised in §(c) |
| `:331` kappa constant table entry | — | **match** |
| `:211` `sigma_i² = sigma_sky² + (RN/g)² + F·P_i/g` | `snr_science.cpp:200-201` | **exact match** |
| `:216-217` `shot_noise_only` / `empirical_total_rms` double-count guard | `snr_science.cpp:176-181`, enum `snr_estimator.h:321-323` | **exact match** |
| `:264` `W_k = a_k²/(sigma_pix,k²·A_NEA,k)`, `A_NEA,k = 1/sum_p P_k,p²` | `information_weight.cpp:231-232` | **exact match** |
| `[1]` Horne 1986, PASP 98:609, doi:10.1086/131801 | cited in code at `snr_science.cpp:4` | **match** |
| `:81` Fisher information matrix `I = c·ones(3,3)`, rank 1 | **no Fisher code** | **consistent** — the doc uses Fisher only as an *analytic non-identifiability argument* for (S, D, B), never as an algorithm. So "no Fisher implementation" is NOT a doc/code mismatch. |

### 5b.1 The one genuine open question: what is the SNR numerator?

This is the single place where a doc/code mismatch is plausible. [VERIFIED facts; the
judgment is **INFERRED**]

- Doc `:19` and `weight_chain.h:4-6` both state the numerator is **`F_ref`** (a
  reference flux), and the identity `w = SNR²/F_ref²` is stated to hold only when
  "SNR 的分子与 F_ref 是同一参考通量" (`weight_chain.h:6`).
- The **frozen schema contract** says the sparse control-point value is `F_ref/σ_F,c`
  (`weight_chain.h:17-18`, quoting
  `eng/contracts/schemas/unified/sparse_snr_layer.schema.json`).
- But the **Phase1 producer** uses the star's *own* flux as the numerator and does **not**
  rescale to `F_ref` units:
  ```cpp
  // snr_science.cpp:162, 215
  const double F = p->flux_adu;                 // the star's own flux
  out->snr_optimal = F / out->sigma_f_optimal_adu;
  // snr_frame_science.cpp:47, 118 — passed straight through, no rescaling
  p.flux_adu = row.flux_adu;
  rn_snr[i]   = r.snr_optimal;
  ```
  An `F_ref`-based SNR is computed **only** for a synthetic reference profile
  (`snr_frame_science.cpp:175-208`, `reference_flux_adu` is required, fail-closed).
- Therefore `SNR²/F_ref² = 1/σ_F²` holds **only if** the per-star flux is already expressed
  in `F_ref` units. Whether that is guaranteed upstream is **not something I verified** — it
  is the right question to put to the doc owner.

---

## 5c. CORRECTIONS to my own earlier statements (supersede §(g) row on IDW)

### 5c.1 `idw_power` default is **1.0** in live code — my earlier note was incomplete

I originally reported only that `2.0` lives in archived legacy code. There are in fact two
`snr_evaluator` implementations:

| File | Built? | `idw_power` default |
|---|---|---|
| `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/gradient/snr_evaluator.cpp:197` | archived/dead | `2.0` |
| **`lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.cpp`** | **yes** — `lib/algorithms/drizzle/CMakeLists.txt:30` | **`1.0`** |

```cpp
// lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.h:110
double idw_power_ = 1.0;   // 规范默认 1.0
// snr_evaluator.cpp:212, :268
idw_power_  = (idw_power > 0.0) ? idw_power : 1.0;
```
This **agrees** with `docs/science/DISPUTE_RESOLUTION.md` §5 (D-05): `终裁：默认 idw_power = 1.0`.
**Doc and code are consistent — do not flag this.**

### 5c.2 The retired `1/(ln10·sigma_residual)` is a stale COMMENT, not an active formula

`snr_evaluator.h:44` documents `snr_phot` as `1/(ln10×sigma_residual)` — the quantity
`snr_estimator.h:296` declares is wrong by 3 orders of magnitude. I checked whether that
formula is executed. **It is not.**
```cpp
// snr_evaluator.cpp:210, :266 — pure parameter passthrough
snr_phot_ = snr_phot;
```
```cpp
// hp_drizzle_api.cpp:771-774 — values come from the "snr_model" AIO block header
double snr_phot, median_snr, idw_power;
std::memcpy(&snr_phot,  tail,     8);
std::memcpy(&median_snr, tail + 8, 8);
```
Drizzle is purely a **consumer**. Live formula is
`snr_evaluator.cpp:317` / `:379` → `snr_out = snr_phot_ * IDW(snr_psf, idw_power) / median_snr`,
which under the P5-SNR redefinition (`snr_estimator.h:586-590`, where
`snr_phot = median_snr = median(SNR_F)`) collapses to a pure IDW of absolute `SNR_F`.
**Docs nit, not a science mismatch.**

---

## 5d. ⭐ REAL DISCREPANCY: sky-budget target is **1.2 %** in the doc, **1.5 %** in the code

This is the strongest concrete doc↔code numeric conflict found. [VERIFIED on all three sources]

```bash
grep -n "1\.2\s*%\|1\.2%\|0\.012" docs/science/noise_snr/NOISE_SNR.md
grep -n "1\.5%\|1\.44\|9216" lib/algorithms/noise_snr/cpp/src/noise_model.cpp docs/science/DISPUTE_RESOLUTION.md
```

| Source | Statement |
|---|---|
| `docs/science/noise_snr/NOISE_SNR.md:153` | `本链默认阈值对应 target ≈ 1.2%` |
| `docs/science/noise_snr/NOISE_SNR.md:338` | `天空相对标准误目标 \| target ≈ 1.2%` |
| `lib/algorithms/noise_snr/cpp/src/noise_model.cpp:1057` | `依据 SE(σ̂)/σ ≈ 1.144/√N_sky (MAD 路径) ≤ 1.5% ⇒ N_sky ≥ 9216` |
| `lib/algorithms/noise_snr/cpp/src/noise_model.cpp:1262` | `cfg->mask_budget_min_sky = 9216;  // 天空预算: N_sky ≥ 9216 (SE ≤ 1.5%)` |
| `docs/science/DISPUTE_RESOLUTION.md:63-64` (D-04) | `终裁：c(n=64)=1.152；复合口径 1.44 = 1.152×1.2533；N_sky ≥ 9216 = (1.44/0.015)²` |

Both paths produce `N_sky ≥ 9216`, which is why nothing has broken:

- via `c = 1.44`: `1.44 / √9216 = 1.44/96 = 1.50 %` — satisfies **1.5 %** exactly, **not** 1.2 %
- via MAD path `c = 1.144`: `1.144/96 = 1.19 %` — satisfies **1.2 %**

`eng/packaging/config/defaults.json:337` records both:
`取 SE≤1.5% ⇒ 9216。全帧兜底走 MAD 路径（解析 SE≈1.144/√N ⇒ 9216 时 1.19%）同样满足。`

**Second confusion to avoid:** there are **two different "1.2 %"s** in the science docs.
`docs/science/DISPUTE_RESOLUTION.md:67` uses 1.2 % in an unrelated sense —
`直接管线测量 c ≈ 1.449（n = 9216）⇒ N_min ≈ 9321，与 9216 差 1.2%` — the
measured-vs-nominal **difference**, not a target. A naive grep conflates them.

**Open question for the doc owner:** `DISPUTE_RESOLUTION.md:68-69` says the canonical landing
spot is `NOISE_SNR.md §3.1` and that it is `已一致` (already consistent) — but that ruling
derives 9216 from a **1.5 %** target while `NOISE_SNR.md` states **1.2 %**. Either the doc's
target number should read 1.5 %, or the two code comments should read 1.2 %.

---

---

## 7. `实验/` experiment units

All three target units exist and contain **runnable code with real numbers**.

| Unit | Runnable | Real numbers | Links production C++ | Tracked JSON |
|---|---|---|---|---|
| `实验/absolute-snr/` | YES — 133 `.py` (~30.8k LOC) + 4 `.cpp` (~1.06k LOC) | YES | **YES, 4 places** | 10 |
| `实验/dense-snr-reconstruct/` | YES — 28 `.py` (~6.2k LOC) | YES | **NO** (0 ctypes/dlopen/subprocess) | 0 |
| `实验/healpix-polar/` | YES — 12 `.cpp` + 33 `.py` (~8.8k LOC) | YES | **PARTIAL** — geometry only | 0 |

### 7.1 Does the experiment code reuse the production core?

**Partly — and the project admits the risk in its own report.**

`absolute-snr` does BOTH reimplements *and* cross-checks:
```bash
grep -rn "snr_estimator.h" 实验/absolute-snr/code/*.cpp
```
- `prod_snr_driver.cpp:8` `#include "snr_estimator.h"`, built by `build_prod_driver.sh:10-13`
  **directly against `lib/algorithms/noise_snr/cpp/src/snr_science.cpp`** — no formula copy.
- `b7_recon_driver.cpp:51-56` includes `snr_estimator.h`, `star_detector.h`,
  `v6/information_weight.h`, `v6/weight_chain.h`, `sky_plane.h`, `rejection.h`
- `exp11_recon_driver.cpp:29` `astrocs/weight_chain.h`
- `reverse_verify/frame_snr/cpp/p1snr_probe.cpp:28` calls `snr_source_snr_f64`

The Python side is a **NumPy mirror** of `snr_science.cpp`, asserted equal at **1e-12**:
```python
# 实验/absolute-snr/code/sci_b_common.py:210-212
"""逐行镜像 lib/algorithms/noise_snr/cpp/src/snr_science.cpp:149-238。
与 C++ 驱动（code/prod_snr_driver.cpp）对拍，容差 1e-12（同一公式两条实现）。"""
```
Mirror confirmed line-by-line against production:
```python
# sci_b_common.py:36, 60, 224-226
K_MAD_TO_SIGMA = 1.482602218505602   # mirror of noise_model.cpp:119
rn_term = (rn_e / gain) ** 2 if rn_e > 0 else 0.0          # == snr_science.cpp:178-181
var_i = sigma_sky_adu**2 + rn_term + np.maximum(F_adu*P, 0.0)/gain   # == :200-201
var_f = 1.0 / float((P * P / var_i).sum())                # == :202,206
```

**But the project states the limitation itself:**
`实验/absolute-snr/REPORT_experiment.md:4-7` — of 36 audit scripts,
**35 do not import, link or execute production `lib/**`**; they are pure Python+numpy
replicas that *"structurally cannot go red from a real pipeline defect"*.
Corroborated by `code/audit/run_all.sh:2` (`零仓库 import`).
Only **one** exception is production evidence: `exp11_frozen_operator_transfer.py`.

`dense-snr-reconstruct` has **zero** production linkage — its `k=0.1` / `r_min=1.5`
attributions appear in comments only, and those constants do not exist in that unit.

### 7.2 SNR formula in the experiments — Horne 1986, same as production

```python
# 实验/absolute-snr/code/sci_b_common.py:170-175
def horne_extract(data, P, var_pix):
    """F_hat = Σ(P d/σ²)/Σ(P²/σ²)；Var(F_hat) = 1/Σ(P²/σ²)。"""
# :246-247
"""帧级 SNR 定义式：SNR_k(F_ref) = F_ref/σ_F（通量型，Horne 1986）。"""
```
`F²/σ²` appears **only** as the weight identity `w = SNR²/F_ref²` — never as an SNR
definition. Matches production.

**Premise correction:** `实验/absolute-snr/code/exp02/` contains **no SNR formula** — it is
the *sky-σ contamination* study, not an SNR experiment (`exp02_common.py:3-9`).
`dense-snr-reconstruct` has **no per-source extraction at all**.

### 7.3 Read noise in experiments

`(RN/g)²` in ADU², gain e⁻/ADU — same as production.
`dense-snr-reconstruct/sim/exp_sim01_m16_forward_snr_truth.py:265-266` adds a **`1/12`
quantization term** not present in the production model:
`((lam + det.read_noise_e**2)/(g*g) + 1/12)`.

The experiments lack the production `SNR_SIGMA_SKY_*` semantic switch
(`snr_estimator.h:321-323`), i.e. the read-noise **double-count guard**. The project's own
scan measured the resulting overestimate at **+12.32 % (bright) / +36.62 % (faint)**,
against the production-documented bound of +12.8 %…+34.0 % (`snr_science.cpp:176`).

### 7.4 MAD constants in experiments — consistent with production

| Constant | Present in experiments |
|---|---|
| `1.482602218505602` | `sci_b_common.py:36`, `exp02_common.py:23`, `exp05_common.py:58`, `exp06_common.py:65` |
| `0.7316727929211932` | `sci_b_common.py:37`, `exp03/exp03_common.py:74` |
| `0.6744897501960817` | `exp02_robust_scale_mad.py:58` |
| `0.6745` | `b4_integration.py:239` — and as a deliberate **negative control** at `exp01_robust_statistics_constants.py:59` (rel. err 1.52e-5, matching NOISE_SNR.md:121) |
| `0.6744` | **not found** |

### 7.5 Thresholds in experiments

- **5σ clipping present**, but **two values coexist**: 5σ×2 for patches
  (`sci_b_common.py:68-69`) and **3σ×2 for the production sky estimator**
  (`exp02_common.py:24` `CLIP_K = 3.0`) — matching
  `module_adapters.cpp:7361` (`整帧 2 轮 median±3*1.482602218505602*MAD`).
- **`1.2%` / `0.012` as a target: NOT FOUND.** Two different budgets exist and must not be
  conflated:
  - `DELTA_BUDGET = 0.014` (1.4 %) — the *structure/delta* budget, not the sky SE target
    (`exp03/exp03_common.py:42`, `exp02/e1_analytic_scan.py:43`).
  - `CTRL_NOISE_REL = 0.015` (1.5 %) — the *control-point precision*, annotated
    `SE/sigma @ N_sky=9216` (`code/audit/route3/exp11_frozen_operator_transfer.py:58`).
    **This one corroborates the production 1.5 %**, strengthening §5d.
- λ_lo/λ_hi = 1/16 present: `exp09_geometry_plane.py:73` `1.0/16.0`;
  `dense .../exp_P4R2_04:48` `0.0625`.
- `k=0.1`, `r_min=1.5` present only in absolute-snr:
  `code/audit/route2/exp08_mask_radius.py:30` `KAPPA = 0.1`;
  `:131` `np.clip(r_local, max(1.5, 0.75*fwhm), 60.0)`.
- `1.253` present, full value `1.2533141373155001`; the 3-digit truncation is audited at
  −2.5065e-4 (`exp03_closed_form_constants.py:94,105`).

### 7.6 Evidence base is largely NOT committed

```bash
git check-ignore -v 实验/absolute-snr/results 实验/healpix-polar/results
# .gitignore:187:/实验/**/results/   -> both ignored
```
`.gitignore:187` excludes `results/` from **all three** units by policy. On disk:
absolute-snr `results/` = 3 empty subdirs; dense-snr-reconstruct = 7 empty subdirs;
healpix-polar = 2 JSON, **both untracked**.

Reproducibility verified — regenerations were bit-identical
(`python3 exp5_error_budget.py --out /tmp/x.json` → `diff` IDENTICAL). So the numbers are
real, just not archived. Quotable committed evidence:
`code/reverse_verify/snr_design/exp5_error_budget.json` — `eps_total = 1.3281510795086529`
(132.8 % SNR error in current production) and `rel_sigma_error = 0.202` for
drizzle-correlated noise, i.e. *"variance low by 36.3 %, sigma low by 20.2 %"*.

### 7.7 healpix-polar does NOT implement drizzle variance propagation

`sumVarNum`, `C_out`, `var_out` → **zero hits in the entire unit**; they live in production
(`drizzle_engine.cpp:1434`). The Python implements the **diagonal** inverse-variance form
only:
- `exp10_chain_usecase.py:177-178` `leaf_sigma2[key] += (w * 0.05 * sigma_pix[...])**2`
- `e4_flux_conservation.py:92` `Var += v*w*w`
- `mc_kcorr.py:303` `var_p = sigma**2 * float(np.sum(row**2))`

with `w_jp = a_jp/A_drop` real code at `exp10:171`. Flux conservation is a genuine numerical
test (TAU_REL=1e-6, fail-closed applicability guard at
`code/audit/route3/exp03_weight_conservation.py:56-57,74-75`), not an analytic assertion.
No correlation term anywhere — consistent with the booked 20.2 % σ underestimate.

### 7.8 Defects found in `实验/`

1. **`实验/healpix-polar/run_all.sh` is dead as written** — VERIFIED:
   ```bash
   grep -oE "for p in [^;]*" 实验/healpix-polar/run_all.sh
   # for p in p0_selftest p1_rootcause p2_algorithms ... p10_seam
   grep -oE "\./p[0-9a-z_]*" 实验/healpix-polar/run_all.sh | sort -u
   # ./p0 ./p1 ./p2 ... ./p10
   grep -c p4_hst3d 实验/healpix-polar/run_all.sh
   # 0        (never built, yet REPORT_paper.md:240 cites it)
   ```
   It compiles 11 binaries then invokes 11 differently-named, **non-existent** ones.
   Do not cite it as a reproduction command; `code/audit/run_all.sh` (Python-only, 29 legs)
   is the working entry.
2. **`run/SCI-402/prod_snr_driver` does not exist** in this workspace, so the 1e-12
   production cross-check gate is currently **unevaluable** without first running
   `code/build_prod_driver.sh`.

---

## 6. Cross-check risk summary for the doc comparison

Points where a doc and the code could plausibly disagree — flag these first:

1. **SNR form.** Code has BOTH `F/σ_F` (`snr_science.cpp:215`) and `SNR²/F_ref²`
   (`weight_chain.cpp:104`). They are the same physics; a doc quoting only one must be mapped
   onto the right layer.
2. **Variance model has four expressions** (empirical-only b1, diagnostic Poisson+RN b2,
   Horne per-pixel b3, aperture b4). A doc giving "the" variance model must state which.
3. **MAD constant spelled two ways** (`1.482602218505602·MAD` vs `MAD/0.6744897501960817`).
   Identical to double precision — verified.
4. **The PSF `mad` column is NOT a MAD** — it is a 10–90% trimmed mean absolute residual
   (`snr_estimator.h:83-86`). A doc using the historical name for a true MAD would be wrong.
5. **"target 1.2%"** — the doc states `target ≈ 1.2%` (§5d above); the code uses **1.5 %**
   in both production and experiments. **This is the one real numeric conflict.**
6. **`idw_power = 2.0`** is only in archived legacy code; **live default is `1.0`**
   (`lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.h:110`), matching
   `DISPUTE_RESOLUTION.md §5`. **Do not flag this.**
7. **No C++ unit tests** guard any of these numbers; and `实验/` evidence is mostly
   gitignored (§7.6), so a large part of the numeric case for these constants is
   **uncommitted and unreproducible from the repo alone**.
8. **The project itself records a 132.8 % SNR error** in current production
   (`exp5_error_budget.json`), attributable to drizzle-correlated noise inflating the
   variance budget. Any doc claiming a validated end-to-end SNR accuracy should be checked
   against this.

---

## Appendix — every command used

```bash
ls -la "/workspace/Astro CS Database"
find . -maxdepth 2 -type d -not -path './.git*' -not -path './build/*' -not -path './out/*' -not -path './run/*' -not -path './testdata/*' | sort
find lib -type f | sed 's/.*\.//' | sort | uniq -c | sort -rn | head -20
find eng -type f | sed 's/.*\.//' | sort | uniq -c | sort -rn | head -20
find lib/algorithms/noise_snr -type f | sort
find lib/algorithms/noise_snr -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.c' -o -name '*.py' -o -name '*.md' \) -exec wc -l {} + | sort -n
grep -n "noise_snr\|snr_science\|acsd_phase1_noise" CMakeLists.txt
grep -n "information_weight" lib/algorithms/integration/phase1_product/CMakeLists.txt lib/algorithms/integration/phase2_integrate/CMakeLists.txt
grep -rn "1\.4826\|0\.6745\|0\.67449\|0\.7316728" --include=*.c --include=*.h --include=*.cpp --include=*.hpp lib/ eng/
grep -rn "0\.012\|1\.2%\|1\.2 %" --include=*.c --include=*.h --include=*.cpp --include=*.hpp lib/
grep -rni "fisher\|\bNEA\b\|point_information" --include=*.cpp --include=*.h lib/ eng/
grep -rn "R C_in\|C_out\|sumVarNum" --include=*.cpp --include=*.h lib/algorithms/drizzle/
grep -rn "2\.5 \* std::log10\|1\.0857" --include=*.cpp --include=*.h lib/
grep -rn "idw_power" --include=*.cpp --include=*.h lib/
grep -n "NOISE_SNR.md" eng/packaging/config/defaults.json | head -20
grep -n -i "gain\|read_noise\|snr\|noise" eng/packaging/config/templates/*.json
ls eng/tests; ls eng/tests/conformance
grep -rln "gtest\|catch2\|Catch2\|add_test" --include=CMakeLists.txt --include=*.cmake lib/ eng/
grep -rln "snr\|SNR\|variance" eng/tests | head -30
git log --oneline --diff-filter=D -1 -- lib/algorithms/noise_snr/tests/p1noise/CMakeLists.txt
```