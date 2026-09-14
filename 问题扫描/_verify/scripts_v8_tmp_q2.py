
import json, re
added = json.load(open('问题扫描/_verify/_v8_added.json', encoding='utf-8'))
for kw in ['p1_tan_forward', 'tan_forward', '同源换算', '交叉对拍', 'REPORT']:
    hits=[a for a in added if kw in a['t']]
    print('=== %s : %d' % (kw, len(hits)))
    for h in hits[:8]:
        print('  %s:%d  %s' % (h['f'], h['n'], h['t'].strip()[:170]))
