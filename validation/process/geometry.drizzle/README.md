# geometry.drizzle M5 validation

Driven by `validation/process/geometry.overlap/m5_contracts.cpp`; raw result in
`observed_m5.jsonl`.

`build_drizzle` is registered, negative-checked (null input ->
`invalid_request`) and runs a real minimal fixture: one overlap contribution on
leaf (order 0, ipix 0), native area [1.0,1.1], drop area [1.0,1.0], overlap area
[0.5,0.6], one native sample x=2 and no recipe (request order 0, pixfrac 1).

Observed: `run_code=ok`, one leaf, same-leaf merge interval
[1.515152, 2.400000] which contains the nominal x/Omega = 2/1.05; the interval
is wider than the point ratio because Omega and drop are separate enclosures
(the drizzle formula is not replaced by a quadratic pixfrac).
Plan: R=8792 B, F=65859 B, work=2000.
