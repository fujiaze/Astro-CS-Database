# uncertainty.covariance M5 validation

Driven by `validation/process/geometry.overlap/m5_contracts.cpp`; raw result in
`observed_m5.jsonl`.

All five operations are registered and negative-checked.
Positive through the registry: `make_covariance` (identity factor),
`covariance_apply`, `quadratic_form` (C=I, t=[1,2] -> exact 5),
`geometry_covariance_bound` (named unavailable: this covariance exposes neither
an absolute factor bound nor a norm bound; unavailable is not zero) and
`control_covariance`.

`control_covariance` is now real: the unique implementation already exists in
`src/information/information.cpp` (it composes T with every root influence,
keeps the shared root identity and calls `make_covariance`); the typed adapter
calls it and no second formula is added. Analytic check with
C = I and T = [[1,2],[0,3]] (CSR): `(T C T^T) e0 = [5,6]` and
`e0^T (T C T^T) e0 = 5`.
