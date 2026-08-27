import csv, os
ROOT = open('/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt').read().strip()
P = os.path.join(ROOT, 'package', '06_checker_truthfulness', 'checker_truthfulness.csv')
rows = list(csv.reader(open(P, encoding='utf-8')))
seen = set(r[0] for r in rows)
new_rows = [
['check_comments','stale audit rounds/legacy claims + missing SCI/ALG ID near invariants','REAL_REPO: PASS exit=0','CLEAN_MUTATION stale V19R3 claim + false_negative invariant w/o ID in isolated upm.cpp -> FAIL exit=1 (checker works on clean file)','n/a','REAL_FRAGILITY: greedy `//.*` regex with re.S swallows rest of file incl. 冻结 strings, suppressing stale-claim finding in real files; scan capped at 50 files', 'no', 'PARTIAL', 'NO (as sole gate)','works on clean inputs; fragile comment regex can suppress findings in real multi-comment files','tools/quality/contracts/check_comments.py L29-34 (regex), L40-46 (loop); mutation2 exit=1'],
['check_test_contracts','each TST id registered, runnable by label, assertions map to upstream contract','REAL_REPO: PASS exit=0 (41 unique TST)','MUTATION test_files points to 3 nonexistent files with TST- ids -> PASS exit=0 (false negative: TEST-BAD-FILE waived by TST- fallback, only synthetic_gate.cpp existence required)','no','YES','NO','test-to-file linkage not actually verified; any test_files path is waived when test_ids contain TST-','tools/quality/contracts/check_test_contracts.py L52-62 (fallback); mutation exit=0']
]
for nr in new_rows:
    if nr[0] not in seen: rows.append(nr)
with open(P, 'w', newline='', encoding='utf-8') as f:
    csv.writer(f).writerows(rows)
print('checker_truthfulness rows:', len(rows)-1)
