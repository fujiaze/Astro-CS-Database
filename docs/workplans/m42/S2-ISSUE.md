# S2: process all 196 M42 frames into the formal four-channel FITS (authorized only after an independent S1 PASS)

This issue is created by the S1 recovery run (issue #124) after the three-frame
4096x4096 Normalize -> Mosaic -> Export chain passed independent review.  It is
the authorized stage-two delivery of
`docs/workplans/m42/AUTONOMOUS-RECOVERY-TO-FULL-DATASET.md` (section 4 to 7).

## Frozen S1 baseline

- branch `rebuild/acsd-v2`, commit recorded in `docs/audit/m42-s1-recovery-2026-11-12.md`
- three T2/M5 H-alpha 600 s frames, 4096x4096, SHA256 frozen in
  `validation/m42/inputs_manifest.json`
- three Normalize products, one Mosaic, one Export FITS with the independent
  QC numbers and the reviewer verdict attached to the S1 issue
- no raw FITS is uploaded, no `main` change, no PR

## Required outcome

1. Process all 196 light frames (T2/T3, M1..M6, H-alpha/R/G/B) through the real
   Normalize path with the frozen Gaia solutions (three frames already solved,
   the remaining frames solved or reused from a certified archive solution).
   Every frame gets a per-frame disposition (published / named exclusion) and
   the account is closed: published + excluded = 196, no silent drop.
2. Four formal coadded FITS, one per channel, on a common target WCS covering
   M1..M6, written by the real Mosaic + Export path (SIP, BUNIT, support and
   coverage).  Each contributing frame is attributed in the product sidecar and
   in `evidence/S2/frames.json`.
3. The S1 acceptance gates are re-run on the four final products: support and
   coverage, photometry and PSF holdout, SNR/noise state, rejection, sky/seam,
   FITS strictness, resources and storage.  The night-to-night relative scale
   must be either estimated by the declared photometry path or named as an
   unseparated systematic with its measured value and its effect on the
   mosaic; it may not be silently ignored.
4. Evidence under `validation/m42/S2/<run-id>/` and a report
   `docs/audit/m42-s2-2026-11-12.md`; the four FITS delivered to the user for
   the final visual seam acceptance.

## Constraints

- no duplicate full computation: reuse certified products/caches, never
  recompute a published frame;
- raw data read-only; calibration masters reused, not re-decoded;
- no raised resource limits, no lowered precision, no changed acceptance
  criteria, no faked completeness;
- per-channel grouping states the response/kernel comparability it actually
  has; frames that cannot be combined are grouped or excluded, never averaged
  across an unproven response;
- the S2 run may reuse the S1 three Normalize products only if their identity
  and the common target are compatible; otherwise they are recomputed in the
  same pass with the same configuration.
