# geometry.overlap M5 validation

Source: `m5_contracts.cpp` (this directory). The same binary exercises all five
M5 processes on one frozen registry; each process directory has its own filtered
`observed_m5.jsonl`.

Run from the repository root:

```bash
g++ -std=c++20 -Iinclude -Isrc validation/process/geometry.overlap/m5_contracts.cpp \
  -Wl,--start-group build/science/libgeometry.a build/science/libcoordinates.a \
  build/science/liboperators.a build/science/libuncertainty.a \
  build/science/libprojection.a build/science/libobservation.a \
  build/science/libacsd_builtin.a build/science/libruntime.a \
  build/science/libcodec.a build/science/libmodel.a build/science/libnumerics.a \
  -Wl,--end-group -lpthread -lzstd -o /tmp/m5 && /tmp/m5 validation/process
```

Result: `M5 five-process registry/lifecycle PASS: 109 checks`.

All eleven operations are registered from
`acsd::generated::descriptor_for("geometry.overlap")`, receive a null-input
negative (`invalid_request`) and a plan call. Positive through the registry:
`make_footprint_provider`, `detector_footprint`, `measure` (interval ordered),
`footprint` (4 curved edges), `footprint.boundary_interval`,
`footprint.forward_interval`, `footprint.derivative_interval`,
`compute_overlap` (1 contribution), `refine_geometry`, `refinable.open`,
`refinable.refine`.

The earlier `footprint` hang was not geometric: `model::BigInt::to_double()`
dropped the sign, so `Rational::to_double_floor()` walked ~2^52 ulps downwards
for negative values. `dyadic_to_double(exact_point(-0.5))` was the minimal
counterexample; the one-line sign fix in `src/model/exact.cpp` makes
`footprint` return in <1 ms.
