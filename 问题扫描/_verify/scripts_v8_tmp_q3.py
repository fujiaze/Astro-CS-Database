
import json, re
added = json.load(open('问题扫描/_verify/_v8_added.json', encoding='utf-8'))
hits=[a for a in added if re.search('REPORT', a['t'])]
print('REPORT mentions in added lines:', len(hits), 'files:', len(set(h['f'] for h in hits)))
for h in hits:
    print('  %s:%d | %s' % (h['f'], h['n'], re.sub('\s+',' ',h['t'].strip())[:150]))
