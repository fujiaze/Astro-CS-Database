# geometry.overlap M5 validation

Source: `m5_contracts.cpp` (this directory). The same binary also exercises the
other four M5 processes so one frozen registry is shared; per-process raw output
is `observed_m5.jsonl` in each process directory.

Command (from the repository root):

```bash
g++ -std=c++20 -Iinclude -Isrc validation/process/geometry.overlap/m5_contracts.cpp \
  -Wl,--start-group build/science/libgeometry.a build/science/libcoordinates.a \
  build/science/liboperators.a build/science/libuncertainty.a \
  build/science/libprojection.a build/science/libobservation.a \
  build/science/libacsd_builtin.a build/science/libruntime.a \
  build/science/libcodec.a build/science/libmodel.a build/science/libnumerics.a \
  -Wl,--end-group -lpthread -lzstd -o /tmp/m5 && /tmp/m5 validation/process
```

Result: `M5 five-process registry/lifecycle PASS: 95 checks`.

Every one of the eleven operations is registered from
`acsd::generated::descriptor_for("geometry.overlap")`, gets a null-input negative
request (`invalid_request`, rejected by validate) and a plan call.

Positive through the registry: `make_footprint_provider`, `detector_footprint`,
`measure` (interval ordered), `footprint.forward_interval`,
`footprint.derivative_interval`, `compute_overlap` (1 contribution),
`refine_geometry`, `refinable.open`, `refinable.refine`.
`footprint` + `footprint.boundary_interval` positive is **not claimed**: the
curved-boundary request did not return within the fixed test budget in this
session (it was left running; no pass is recorded). Negative + plan are covered.
