
import re, json
recs = json.load(open('问题扫描/_verify/_v8_comment_lines.json', encoding='utf-8'))
prose = [r for r in recs if r['kind'] in ('cmt-line','cmt-trailing','doc-line')]
checks = json.load(open('ci/checks.json', encoding='utf-8'))
# gather ids and waivable
def walk(o, acc):
    if isinstance(o, dict):
        if 'id' in o and isinstance(o['id'], str):
            acc[o['id']] = o
        for v in o.values(): walk(v, acc)
    elif isinstance(o, list):
        for v in o: walk(v, acc)
acc = {}
walk(checks, acc)
print('checks.json entries:', len(acc))
ID_RE = re.compile(r'\b(?:CTEST|CON|UT|DOC|ABI|API|QUALITY|STATIC|WIN|LINUX|AGENTS|GOV|TRACE|STD|SEC|BUILD|FMT|CLANG|PERF|RES|MON|RESOURCE|TOOL|SMOKE|PKG)-[A-Z0-9][A-Z0-9\-]{2,}\b')
claimed = {}
for r in prose:
    for m in ID_RE.finditer(r['t']):
        claimed.setdefault(m.group(0), []).append((r['c'], r['f'], r['n']))
print('distinct check-like ids claimed in added prose:', len(claimed))
missing = {k:v for k,v in claimed.items() if k not in acc}
print('NOT present in checks.json:', len(missing))
for k, v in sorted(missing.items()):
    print('  %-40s mentions=%d  first=%s %s:%d' % (k, len(v), v[0][0], v[0][1], v[0][2]))
