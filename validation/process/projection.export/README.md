# projection.export M5 validation

Driven by `validation/process/geometry.overlap/m5_contracts.cpp`; raw result in
`observed_m5.jsonl`.

`build_export` is registered, negative-checked and runs a real minimal product
fixture: a `ProductProvider` returning a `ProductGraph` with one leaf (order 0,
ipix 2 — the leaf actually touched by the target WCS), one supported leaf with
signal 2.0 sr/pixel and support states/areas, exported to a 2x2 target through
the production `compute_overlap` path (primitives policy, f64).

Observed: `run_code=ok`, output 4 target pixels (unsupported pixels would stay
NaN); the same-row invariant holds. Plan: R=12369 B, F=65667 B, work=20000.
