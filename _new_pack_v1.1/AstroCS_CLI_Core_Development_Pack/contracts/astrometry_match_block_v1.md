# `astrometry_matches` Optional Block v1

- Type: FLOAT64 `[M,7]`
- Producer: PLATESOLVE
- Purpose: diagnostics and future photometric candidate narrowing; not required by PHOTOMETRIC v1.

Columns:

```text
0 star_det_index
1 x_px
2 y_px
3 gaia_ra_deg
4 gaia_dec_deg
5 gaia_mag
6 residual_px
```

If Gaia source_id becomes available, do not encode uint64 in float64. Introduce a v2 RAW/fixed-record block instead.
