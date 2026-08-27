import csv, os
ROOT = open('/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt').read().strip()
P = os.path.join(ROOT, 'package', '14_findings', 'findings.csv')
rows = list(csv.reader(open(P, encoding='utf-8')))
new_rows = [
['P2-06','P2','CONFIRMED','documentation','docs','docs/release/PRE_RELEASE_EVIDENCE_INDEX.md + docs/phase2/PRODUCTION_WIRING.md','11 + 3','Nonexistent commit refs',
 'docs reference real commits',
 'PRE_RELEASE_EVIDENCE_INDEX.md:11 freezes HEAD 46d6f951fc2096d61b3fc6fd8c63563b4ce14c63; PRODUCTION_WIRING.md:3 uses a7e063e as baseline - neither exists in git history (git cat-file fails)',
 'commits exist in history',
 'git cat-file -e both fail',
 'documents reference nonexistent commits; traceability to those SHAs is impossible',
 'update frozen-HEAD and baseline to real SHAs or mark historical',
 'git cat-file',
 '13_static_quality/DOC_REFERENCE_FINDINGS.md'],
['P2-07','P2','CONFIRMED','documentation','docs','docs/API_REFERENCE.md + docs/phase2/PRODUCTION_WIRING.md + docs/history/v19/CONFIG_REFERENCE.md + docs/architecture/BUILD_GRAPH.md','16 + 10 + 39','Nonexistent doc/file paths',
 'docs reference existing files',
 'docs/science/HEALPIX_MAPPING.md, docs/algorithms/HEALPIX_MAPPING.md, docs/algorithms/REJECTION.md, docs/algorithms/INTEGRATION.md, docs/architecture/threading.md, lib/orchestrator/cpp/configs/, tools/stage2.cpp all referenced but missing (correct files exist under other names/paths)',
 'referenced paths exist',
 'path existence check',
 'misleading references break doc navigation and symbol traceability',
 'fix paths/names',
 'path check',
 '13_static_quality/DOC_REFERENCE_FINDINGS.md'],
['P2-08','P2','CONFIRMED','portability','healpix_drizzle','lib/healpix_db/healpix_drizzle/Makefile','68,72','-Wl,--stack Windows-only (documented make fails on Linux)',
 'documented make builds drizzle on Linux',
 'documented make fails (Windows-only -Wl,--stack hardcoded); a 3-workaround build (-fPIC; drop flag; link AIO .dll by explicit path) succeeds producing ELF so sha256 d9514c38...',
 'documented command works on Linux',
 'make + workaround link (logs/drizzle_link_workaround2.log)',
 'clean-clone reproducibility of drizzle FAIL without Makefile edits',
 'guard -Wl,--stack under _WIN32; add libfoo.so naming',
 'rebuild from fresh archive',
 '04_build/build_matrix.csv healpix_drizzle-linux-workaround'],
]
seen = set(r[0] for r in rows)
added = 0
for nr in new_rows:
    if nr[0] not in seen:
        rows.append(nr); added += 1
with open(P, 'w', newline='', encoding='utf-8') as f:
    csv.writer(f).writerows(rows)
from collections import Counter
c = Counter(r[1] for r in rows[1:])
print('findings rows=' + str(len(rows)-1) + ' added=' + str(added) + ' by_severity=' + str(dict(c)))
