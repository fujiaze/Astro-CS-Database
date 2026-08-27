import csv, os
ROOT = open('/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt').read().strip()
P = os.path.join(ROOT, 'package', '06_checker_truthfulness', 'checker_truthfulness.csv')
rows = list(csv.reader(open(P, encoding='utf-8')))
seen = set(r[0] for r in rows)
new_row = ['generate_contract_report','aggregates 10 contract checkers into one verdict','REAL_REPO: PASS exit=0 (10/10 tools PASS)','n/a','n/a',
 'AGGREGATES THE SAME FALSE NEGATIVES: overall PASS is the AND of the 10 checkers, each of which has proven false negatives (see rows above: api param swap, science units ADU->second, execution omp-wiring, traceability algorithm_id, doc symbols, config unknown key, build_graph missing CMakeLists, test_contracts nonexistent test_files, forbidden-patterns 16-cap, comments fragile). A checker-level PASS therefore does not certify contract truth.',
 'no', 'YES', 'NO (for certification)','report generator itself is honest (parses status + returncode; byte-stable JSON) but cannot rise above the false-negative checkers it runs; also does NOT run check_full_integration','tools/quality/contracts/generate_contract_report.py L33-52; real run exit=0']
if new_row[0] not in seen:
    rows.append(new_row)
with open(P, 'w', newline='', encoding='utf-8') as f:
    csv.writer(f).writerows(rows)
print('checker_truthfulness rows:', len(rows)-1)
