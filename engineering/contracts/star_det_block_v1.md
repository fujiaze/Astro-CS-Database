# `star_det` Block Contract v1

- Block name: `star_det`
- Type: `AIO_BLOCK_FLOAT64`
- Dims: `[N,6]`
- Count: `N*6`
- Producer: exactly one active producer selected by PlateSolve path ADR:
  - `STAR_DETECT` for `UPSTREAM_SHARED_DETECTIONS`; or
  - `PLATESOLVE_INTERNAL_EXPORT` for `PRESERVE_INTERNAL_DETECTION_EXPORT`.
- Consumers: PLATESOLVE only on upstream path; PSF on both paths.

| Column | Unit | Meaning |
|---|---|---|
| 0 | pixel | x, 0-based, pixel-center convention |
| 1 | pixel | y, 0-based, downward positive |
| 2 | detector units | flux returned by star_detector |
| 3 | mag-like | detector box-integral magnitude |
| 4 | bool as 0/1 | saturated |
| 5 | bool as 0/1 | aperture contains saturated pixel |

Required header keys:

```text
ASTROCS.STARDET.SCHEMA=1
ASTROCS.STARDET.COLUMNS=x_px,y_px,flux,mag,saturated,has_saturated
ASTROCS.STARDET.PRODUCER=STAR_DETECT|PLATESOLVE_INTERNAL_EXPORT
ASTROCS.STARDET.INPUT_REVISION=<integer>
ASTROCS.STARDET.PARAM_HASH=<sha256>
ASTROCS.STARDET.COUNT=<N>
ASTROCS.STARDET.HASH=<sha256 of canonical bytes>
ASTROCS.PLATESOLVE.DETECTION_PATH=upstream_shared_v1|internal_export_v1
```

Constraints:

- one production run enables exactly one producer;
- x/y finite and within a documented border tolerance;
- flags exactly 0 or 1;
- stable ordering is the original detector output ordering;
- no downstream consumer may reorder in place;
- selections must store source row indices;
- on internal-export path the callback data are copied before the PlateSolve call returns;
- no consumer owns or frees a pointer borrowed from another DLL.
