# validation/stats — notes of the independent validation

The three cases under `validation/stats/` use only `include/acsd/contracts.hpp`
and the local harness `validation/stats/harness.hpp`.  They never include a
header of `src/` and they never reuse a helper of the modules under test.

    sh validation/stats/run_validation.sh build/stats

The script builds the three cases against the science libraries of the build
directory and runs them.  The real output of the last run is kept in
`validation/stats/evidence/validation_run.txt` (56 checks, 0 failures).

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
  the selection.

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

## Defects of the base modules that the cases exposed

These are outside the write scope of this agent and are reported to the Lead
instead of being patched here.

1. `src/operators/api.cpp` — `operators::compose` builds a recipe without
   declaring its children, and `make_action` rejects a recipe whose children
   do not match the supplied ones.  The composition therefore always fails.
   The sky module therefore composes the actions with its own helper.
2. `src/model/exact.cpp` — `Rational::from_double_exact(0.0)` returns a default
   constructed rational whose denominator is zero, and every exact operation on
   it throws.  The triangulation therefore builds the zero rational with an
   explicit unit denominator.
3. `src/numerics/linalg.hpp` — the dense `multiply` of this module does not
   check the shapes of its arguments, so a product of incompatible matrices is
   silently wrong.  The sky module had to use the transposed projector
   explicitly.
