# The performance baseline of the drizzle geometry (apps)

**Correction (2026-10-22, after the geometry agent's profiling):** the block level
geometry is *not* the bottleneck.  The profiling of the same construction
(validation/geo/perf_geometry.cpp, public API only) gives **2.06 s for the 64
source rows** (32 ms per row, 2752 work units, one leaf, total area
3.85097e-07 sr); the exact rational cut cell is 92 % of that time (1888 ms).
The construction that needed more than 35 minutes stopped **before** the
geometry: the last timing mark of the attempt
(attempt_single_frame_timing.log) is at 0.02 s and the next stage,
`measure_science_sources` of the sources module, did not return.  The
observation module has since fixed that stage; the A group frame now runs in
**4.4 s** end to end (see the attempt below) and stops in the PSF pixel integral
of the sources module, which is outside the apps scope.

| item | value |
| --- | --- |
| input | validation/x01/input/frames/frame_00_A.fits (128 x 128, BITPIX -64) |
| frames | 1 (group A, the supplied world coordinate solution) |
| leaf order | 6 |
| pixfrac | 1.0 |
| geometry budget | area_absolute_sr 1e-12, normalizer_min_sr 1e-15, max_work 2 000 000, max_bytes 268 435 456, max_depth 64 |
| source rows | 64 blocks of 16 x 16 pixels (ACSD_DRIZZLE_BLOCK=16) |
| measured (geometry alone) | **2.06 s** for the 64 source rows (geo's profiling, public API) |
| measured (whole frame) | 4.4 s (the chain reaches the PSF stage of the sources module) |
| source row numbering | row major: block = row * blocks_per_row + column, so the block b covers the rows [(b/8)*16, +15] and the columns [(b%8)*16, +15] of the row major 128x128 detector |

The earlier record of "more than 35 minutes per frame" is kept as the history
of the attempt; its cause was the sources module, not the geometry.
