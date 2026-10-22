# The performance baseline of the drizzle geometry (apps)

The construction is the one the Lead handed to the geometry agent as the
acceptance baseline: the same construction is replayed after the optimisation
and the numbers are compared.

| item | value |
| --- | --- |
| input | validation/x01/input/frames/frame_00_A.fits (128 x 128, BITPIX -64) |
| frames | 1 (group A, the supplied world coordinate solution) |
| leaf order | 6 |
| pixfrac | 1.0 |
| geometry budget | area_absolute_sr 1e-12, normalizer_min_sr 1e-15, max_work 2 000 000, max_bytes 268 435 456, max_depth 64 |
| source rows | 64 blocks of 16 x 16 pixels (ACSD_DRIZZLE_BLOCK=16) |
| calls | 64 exact cut-cell calls (one per block) |
| measured | **more than 35 minutes of CPU per frame, not finished** (the run was killed three times) |
| reference | one call over the whole detector (no per source row attribution): seconds |

The whole detector as a single collection domain is fast but carries no per
source row attribution, so the weight matrix degenerates to a single column.
The block level is the correct shape (64 columns) and is the baseline above.
