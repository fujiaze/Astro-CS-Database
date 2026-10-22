# validation/stats — notes of the independent validation

The three cases under `validation/stats/` use only `include/acsd/contracts.hpp`
and the local harness `validation/stats/harness.hpp`.  They never include a
header of `src/` and they never reuse a helper of the modules under test.

    sh validation/stats/run_validation.sh build/stats

The script builds the four cases against the science libraries of the build
directory and runs them.  The real output of the last run is kept in
`validation/stats/evidence/validation_run.txt`; its first line records the
commit of the tree that produced it.  The last run has 156 checks and 0
failures.

The leave one out case runs 184000 trials and needs about 40 seconds.  The
trial count must not be reduced: a smaller number of trials weakens the
statistical power of the false rejection rate of W01, and the interval of the
reference is computed for the frozen run.

## What each case certifies

* `information_test.cpp` — the flux information `V_F` of two reference sources
  with the same signal and different covariances is the same (1), while the
  inverse variance weight of the actual scene is `1/V_B` and differs from
  `1/V_F`; the scene variance of a leaf is `V_B` and not `V_F`; a flux that is
  fixed by an exact constraint has `V_F = 0`; a flux that is absorbed by the
  background has no finite variance and is reported as `not_identifiable`; the
  Delaunay triangulation of a five by five control grid reproduces a log plane
  with a maximal deviation of 2.22e-16 and the interval of equation (4a) of the
  science note contains the truth; without a curvature bound the prediction is
  conditional and no certified interval is published.
* `sky_test.cpp` — two frames with a constant offset of two: the reference
  frame is the gauge, so its own component vanishes while the component of the
  other frame carries the offset; the estimator leaves the common sky
  untouched (`K H = 0`); the correction removes the common offset from the
  data and the corrected covariance is available.
* `coadd_test.cpp` — two frames whose shared root gives the joint covariance
  `Gamma = [[1, 2], [2, 9]]`: the unconstrained BLUE weights are `(7/6, -1/6)`,
  the weights of the non negative quadratic programme are `(1, 0)`, the equal
  weights are `(1/2, 1/2)` and the coadd carries the estimate, the weights and
  the selection.  A mixed observation has no BLUE weights and the automatic
  policy falls back to the equal weights.
* `coadd_loo_test.cpp` — the conservative leave one out rejection against the
  frozen truth of W01 (`validation/w01/truth_w01.py`): the same external Gamma
  families, the same alpha of 0.001, the same threshold `Phi^-1(1 - alpha/(2N))`
  and the same statistics.  The clean false rejection rates of this run are
  0, 0, 0, 0.00095, 0.00115, 0.0006 and 0.00095 for N = 1..7 against the
  reference 0, 0, 0, 0.00089, 0.00096, 0.00101 and 0.00095, the 68% and the 95%
  coverage agree with the reference within one percentage point, a frame that
  is shifted by twelve sigma is rejected for every N >= 4 and no frame is
  rejected for N < 4.  The statistic of the module is also compared with an
  independent computation of the same quantity: the two reject the same
  trials.

## Failed attempts that were kept during the work

The first runs failed for reasons that belong to the case or to the harness,
not to the science; they are listed here because the Lead asked that the
failed attempts are not overwritten.

1. The harness stored the bytes of an array under its `index_id`.  The
   intermediate arrays of the modules use an empty identity, so every second
   action was applied to the first stored vector and the covariance factor was
   rank one instead of rank two.  The harness now keys the bytes by the
   address of the array.
2. The harness read every array with an element size of eight bytes, so the
   one byte support states were read out of range.  The harness now uses the
   size of the data type.
3. The validation built the background action in the column major layout.  The
   operators expect the row major layout, so the background was read as the
   zero matrix and the absorbed flux was not identified.  The case now uses the
   row major layout.
4. The gnomonic chart of the reconstruction case had its centre at the north
   pole, where the local east direction of the chart is not defined.  The case
   now uses the centre (0.6, 0, 0.8) and the queries are built with the same
   basis.
5. The first two covariance cases of the information case were written with
   the same columns and rows of the covariance action.  They are kept in the
   case as the two sources that share `V_F`.

## The cases of the second round

* the native space of `evaluate_scene_variance`: a frame whose covariance acts
  on two native coordinates and that has a single leaf reports the variance of
  the functional of the leaf, which is 13 for the functional (2, 3) and the
  identity covariance;
* a query outside the declared chart of `reconstruct_scene_variance` has no
  predicted value and no interval, so the prediction is not a number;
* the weighted sum gauge of the sky module: the sum of the components of a
  connected component vanishes and the components are the differences to the
  weighted mean of the component, so the frame below the mean carries the
  negative component;
* the sky module with two patches: every frame contributes a control to each of
  the two patches and the fit of the three frames reproduces the offsets of the
  frames;
* the coadd module after a leave one out rejection: the weights are recomputed
  on the accepted samples, the rejected frame carries the weight zero and the
  accepted weights still sum to one;
* the external Gamma of one leaf per sample: the domain of the external
  covariance is the leaf count times the frame count, the block of a leaf is
  taken from the rows of that leaf and the contaminated frame of every leaf is
  rejected in every trial;
* the plane basis of the sky module: the difference model of two frames has six
  parameters while the white noise subspace of the controls has one dimension,
  so two slope directions cannot be separated from the common sky.  The fit
  reports them, the data of those directions keep no correction at all and the
  correction reports the unavailable positions.

## The holes of the triangulation

The contract declares `SupportedTriangulation.holes` and the science note
requires that a query inside a hole is unavailable instead of being filled
with the nearest control.  The holes of the implementation are always empty:
they are **not** implemented.  The Delaunay mesh of the controls covers the
convex hull of the controls, so the regions without observed support are not
part of the mesh at all; the support map of the frame states which leaves are
observed, and the holes have to be derived from the leaves whose state is not
one.  That derivation is the missing part and it needs the boundary of a leaf
in the chart, which the module does not compute today.  The query outside the
declared chart is covered (no value, no interval), the query inside a hole is
not.

## The multi connected components of the sky

The standard patch builder derives the common sky from the patches that the
frames of a group share, and it requires the group to have a common support:
the frames of a group must observe the same leaves.  A group in which the
frames share only some of their patches therefore has no control at all
(`no_support`), and the case is rejected before the components are computed.
The multi component case therefore needs an explicit `T` and `H` in the
request, which the contract allows.  That construction is not covered yet.

## Defects of the base modules that the cases exposed

These are outside the write scope of this agent and are reported to the Lead
instead of being patched here.

1. `src/operators/api.cpp` — `operators::compose` builds a recipe without
   declaring its children, and `make_action` rejects a recipe whose children
   do not match the supplied ones.  The composition therefore always failed,
   and the sky module composed the actions with its own helper.
2. `src/model/exact.cpp` — `Rational::from_double_exact(0.0)` returned a default
   constructed rational whose denominator is zero, and every exact operation on
   it threw.  The triangulation therefore built the zero rational with an
   explicit unit denominator.
3. The dense product of the sky module is a local helper of that module, not
   a base function.  It now checks the shapes of its arguments and reports a
   failure, and every product of the fit was re-checked with that guard.
4. `Basis.center` and `Basis.scale` are `Vec2` without an initialiser, so a
   default constructed basis carries indeterminate values.  The sky module now
   uses the scale only when it is finite and positive, and the case sets the
   centre and the scale explicitly.  The contract should give the two vectors
   an initialiser or a default of one.

The three base defects were fixed by the Lead in the commits 76f8d9f32 and
f4a8ea89b.  The local workarounds of the sky module (the own composition
helper) and of the triangulation (the explicit unit denominator of the zero
rational) were removed, and the modules use the base implementations again.
