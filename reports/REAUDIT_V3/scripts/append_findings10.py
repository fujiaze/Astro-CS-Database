import csv, os
ROOT = open('/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt').read().strip()
P = os.path.join(ROOT, 'package', '14_findings', 'findings.csv')
rows = list(csv.reader(open(P, encoding='utf-8')))
seen = set(r[0] for r in rows)
new_row = ['P3-02','P3','CONFIRMED','pipeline_gate','healpix_drizzle','lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp + drizzle_engine.cpp','296-302 + 1024-1029,1932-1934','photscal formal-Stage1 gate',
 'formal Stage1 HISS requires photometric calibration (photscal>0)',
 'hp_drizzle_fits_to_ahpx returns rc=12 on the synced uncalibrated frame: photscal 非法 (0.000000) - formal Stage1 refuses uncalibrated ADU HISS (drizzle probe logs/drizzle_probe_run2.log); FITS itself parses cleanly (4500x3600, WCS TAN-SIP)',
 'same gate permits uncalibrated HISS for probe',
 'drizzle_probe on testdata/Galaxy_Center_T4/lights/panel1/...-Red.fts -> rc=12',
 'confirms the 32R Stage1 pipeline cannot be shortcut without the photometric (Gaia) stage - strengthens GAIA_BLOCKER',
 'none (gate is correct scientific behavior)',
 're-run after photometric calibration with Gaia available',
 '03_testdata/DRIZZLE_PROBE_RESULT.md']
if new_row[0] not in seen:
    rows.append(new_row)
with open(P, 'w', newline='', encoding='utf-8') as f:
    csv.writer(f).writerows(rows)
from collections import Counter
c = Counter(r[1] for r in rows[1:])
print('findings rows=' + str(len(rows)-1) + ' by_severity=' + str(dict(c)))
