# uncertainty.covariance M5 validation

Driven by `validation/process/geometry.overlap/m5_contracts.cpp`; raw result in
`observed_m5.jsonl`.

All five operations are registered and negative-checked.
Positive through the registry: `make_covariance` (identity factor),
`covariance_apply`, `quadratic_form` (C=I, t=[1,2] -> exact 5),
`geometry_covariance_bound` (returns a named unavailable bound: the fixture
covariance exposes neither an absolute factor nor a norm bound; that is the
documented "unavailable", not zero).
`control_covariance` is registered + validated but its run reports named
`unsupported`: `src/uncertainty` currently has no implementation of the public
`control_covariance` free function. This is a science-owner gap, not a mock pass.
Plan example: `quadratic_form` R=6672 B, F=65600 B, work=2.
