
import re, json
recs = json.load(open('问题扫描/_verify/_v8_comment_lines.json', encoding='utf-8'))
prose = [r for r in recs if r['kind'] in ('cmt-line','cmt-trailing','doc-line')]
pat = re.compile(r'(checks\.json|ci/|基线|登记|非豁免|waiv|ctest|在册|profile|CTEST-)')
hits = [r for r in prose if pat.search(r['t'])]
print('== CI/registration mentions:', len(hits))
for r in hits:
    print('%s %s:%d  %s' % (r['c'], r['f'], r['n'], r['t'].strip()[:220]))
