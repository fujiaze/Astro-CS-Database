# M5 typed-process fixed cases

Each subdirectory holds one frozen driver for an integrated ACSD v2 process:

- `case.json` - the frozen inputs, expected named results and any recorded
  limitation/contradiction.
- `observed.json` - the harness document written by the driver (real run).
- `observed.stderr.txt` - the check log.
- `comparison.json` - written by `lib/compare.py`; a case is `pass` only when
  `attempts > 0` and `successes == attempts` and no failure is recorded.

Run one case against a configured build:

```sh
g++ -std=c++20 -O1 -Iinclude -Isrc validation/process/<process>/<driver>.cpp \
    -Wl,--start-group build/<dir>/lib*.a -Wl,--end-group -lzstd -lcfitsio -lpthread \
    -o /tmp/driver
/tmp/driver > validation/process/<process>/observed.json 2> observed.stderr.txt
python3 validation/process/lib/compare.py validation/process/<process>
```

`lib/fixtures.hpp` is public-header-only: runtime services, the frozen registry,
the ProcessHost and the typed invocation. No internal runtime header is used by
the drivers.

## Frozen evidence summary (M5 closure)

| process | evidence |
| --- | --- |
| coordinates.wcs | 7 operations positive, inverse round trip 1e-9 pixel |
| observation.regroup | identity LinearAction produced by `operators.action` through the Host; exact [1,2,4] |
| catalogue.access | build/provider/query/lookup positive |
| sources.psf | fit converged on the pixel-integral frame (sigma=1.2 within 1e-12); point-sampled frame retained as named `nonconverged` |
| sources.measure | positive with an explicit EffectivePSF fixture |
| sources.astrometry | blind solve solved on a same-source 100x100 field; CRPIX 2.4e-5 px, CD 3.5e-16 rad/px, sky dot 1 at two positions |
| response.integrate | two finite basis integrals |

## Unverified / open items (carried into M6)

1. **Probe sampling**: the seven process registrations do not emit per-probe
   `PerfRecord`s; only the framework invocation span exists. The process
   documents' probe ids are in the descriptors/manifest but are not sampled.
2. **`integrate_response` units**: a flat unit photon throughput over 450-650 nm
   returns basis integrals of about 5.54e20 (deterministic and finite), which is
   suspicious for the intended nm/Hz convention; the physical unit scaling is
   not verified.
3. **Resource accounting**: the legacy synchronous implementations allocate
   `std::vector` scratch outside the ledger, so the plan R/F are declared
   conservative upper bounds, not enforced per-allocation accounting. The
   `work_upper` floors (1 MiB for most, 8 MiB for fit/astrometry) are declared,
   not measured.
4. **Small-field solver robustness (real contradiction)**: with five detections
   and `match_radius_pixel = 2`, `fit_wcs` is called with training pairs below
   the eight WCS parameters; `numerics::least_squares` -> `householder_qr`
   computes `std::vector<double> reflector(m - k)` with `k > m` and throws
   `std::length_error`, which the Host reports as `internal`. Minimal repro:
   5-source 32x32 field, same-source catalogue, `min_matches=4`,
   `match_radius_pixel=2`, `scale_rad_pixel=[1.5e-5,2e-5]`. Not fixed here (the
   numerics/solver files are outside this work package's write scope); the
   positive case above avoids it with a 26-source field and `min_matches=9`.
5. **Atomic plans**: these synchronous entries publish one barrier region with
   `cohorts=1`; streaming/cohort plans are an M6 upgrade.
