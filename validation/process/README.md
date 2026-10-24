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
2. **`integrate_response` units — analytical oracle needed**: for a flat
   spectrum of **1 W m^-2 nm^-1**, unit dimensionless photon response over
   450–650 nm, and multiplication by wavelength in metres divided by hc,
   the independent exact-form expression is
   `0.5*(650^2-450^2)*1e-9/(h*c) ≈ 5.5375e20 s^-1 m^-2`.
   Thus a result near 5.54e20 is **not by itself suspicious**. The driver
   must additionally assert response photon-conversion flags, nm vs Hz
   spectral-density identity, coefficients/units and absolute-error bounds;
   passing a finite-value assertion does not establish those checks.
3. **Resource accounting**: the legacy synchronous implementations allocate
   `std::vector` scratch outside the ledger, so the plan R/F are declared
   conservative upper bounds, not enforced per-allocation accounting. The
   `work_upper` floors (1 MiB for most, 8 MiB for fit/astrometry) are declared,
   not measured.
4. **Small-field solver — old contradiction, re-run needed**: an earlier
   five-source 32x32 / `min_matches=4` fixture caused an underdetermined
   `least_squares` path to reach invalid Householder indexing and raise
   `std::length_error`. The current `src/numerics/linalg.cpp` checks
   `m<n` and returns `converged=false` before `householder_qr`. This is
   a source-level correction, **not yet an observed re-run of the original
   five-source fixture**. Preserve the old failure as historical evidence;
   re-run it to verify a stable, named scientific non-success outcome through
   the Host, not an unhandled `internal` exception.
5. **Atomic plans**: these synchronous entries publish one barrier region with
   `cohorts=1`; streaming/cohort plans are an M6 upgrade.

## Evidence / scientific completeness boundary (2026-10-24)

This page is a module-level historical/frozen summary, not a claim that all 22
processes, all 57 operations, or complete Normalize→Mosaic→Export are certified.
The current cross-milestone index is [IMPLEMENTATION-STATUS](../../docs/audit/IMPLEMENTATION-STATUS.md).
Seven reported process positive cases cannot certify per-operation negative cases,
allocation accounting for legacy `std::vector` scratch, true probe sampling,
read-after-publish, shared-noise provenance or CPU cancellation/low-H behavior.
Keep pending items pending until tested against a pinned current code SHA.
