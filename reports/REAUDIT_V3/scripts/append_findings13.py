import csv, os
ROOT = open('/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt').read().strip()
P = os.path.join(ROOT, 'package', '14_findings', 'findings.csv')
rows = list(csv.reader(open(P, encoding='utf-8')))
seen = set(r[0] for r in rows)
new_rows = [
['P2-09','P2','CONFIRMED','checker_truthfulness','quality','tools/quality/contracts/check_forbidden_patterns.py','23-24','hardcoded-thread checker narrow regex',
 'check_forbidden_patterns catches hardcoded thread counts',
 'checker only matches literal num_threads(16)/set_num_threads(16); it returns PASS while lib/acr/scheduler/dispatcher.cpp:476 hardcodes std::min<std::size_t>(16, std::thread::hardware_concurrency()) - a 16-thread cap the checker misses',
 'actual hardcoded 16 cap flagged',
 'checker run exit=0 status=PASS on HEAD; dispatcher.cpp:476 contains the 16 cap',
 'hardcoded-thread debt invisible to the checker; related to P2-01/P2-02 and memory.md 16-thread claim',
 'widen pattern to std::min(...16...), omp_set_num_threads patterns',
 're-run checker after widening',
 'checker stdout PASS; dispatcher.cpp:476; check_forbidden_patterns.py L23-24']
]
for nr in new_rows:
    if nr[0] not in seen: rows.append(nr)
with open(P, 'w', newline='', encoding='utf-8') as f:
    csv.writer(f).writerows(rows)
from collections import Counter
c = Counter(r[1] for r in rows[1:])
print('findings rows=' + str(len(rows)-1) + ' by_severity=' + str(dict(c)))
