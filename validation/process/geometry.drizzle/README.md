# geometry.drizzle M5 validation

Driven by `validation/process/geometry.overlap/m5_contracts.cpp`; raw result in
`observed_m5.jsonl`.

`build_drizzle` is registered, negative-checked (null input ->
`invalid_request`) and its plan is exercised on a minimal overlaps/native
fixture (R=8200 B). A positive run through the full overlap/observation fixture
is **not claimed yet** in this session.
