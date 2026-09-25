# -*- coding: utf-8 -*-
"""Task 1.1 ② refined-2 —— exact-symbol membership test for the concrete
constants named inside the layer-2 review items (copied verbatim from the
review docs; see 复核-AUD202.md:35 / :333, 复核-AUD201.md, 复核-AUD203.md,
复核-AUD204.md).  Membership only.
"""
import io, re, sys, csv

sys.stdout.reconfigure(encoding='utf-8')
ROOT = r'产出/'
MASTER = ROOT + r'\独立审计/02_科学\常数公式算法总台账.csv'
rows = [r for r in csv.reader(io.open(MASTER, encoding='utf-8-sig', newline=''))][1:]
rows = [r for r in rows if len(r) >= 16]

NEEDLES = ['intra', 'weight_chain', 'reference_flux_adu', 'sparse', 'snr_path',
           'sigma_residual', 'control_ivar', 'civar', 'tolerance_relative',
           'stalled', 'hips_profile', 'pixfrac', 'bilinear_upsample',
           'variance', 'snr_noise_model_v1', 'F_ref', 'reference_overlap',
           '1e-15', '1e-6', 'seam', 'additive_mode', 'drizzle',
           'drizzle_science_matrix', 'drizzle_spherical_overlap',
           'drizzle_pf_sb_variance', 'variance_propagation_test',
           'test_spherical_overlap', 'fixture_npix', 'required_selftest_cases',
           'excess', 'mad', 'outlier_rate', 'fit_used', 'anchor', 'lambda']

for n in NEEDLES:
    rx = re.compile(r'(?<![A-Za-z0-9_])' + re.escape(n) + r'(?![A-Za-z0-9_])', re.I)
    hits = [i for i, r in enumerate(rows) if any(rx.search(c) for c in r)]
    keyhit = [i for i, r in enumerate(rows) if rx.search(r[0])]
    print('%-30s row-hits=%-4d as-key=%-3d %s'
          % (n, len(hits), len(keyhit),
             ('keys: ' + '; '.join(rows[i][0][:38] for i in keyhit[:4])) if keyhit
             else ('first-rows: ' + '; '.join(rows[i][0][:38] for i in hits[:4]) if hits else '*** ABSENT ***')))
