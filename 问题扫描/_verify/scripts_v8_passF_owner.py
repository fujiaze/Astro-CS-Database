
import json, re
added = json.load(open('问题扫描/_verify/_v8_added.json', encoding='utf-8'))
pat=re.compile('负责人裁决|负责人裁定|负责人授权|负责人指令|负责人明确要求')
hits=[a for a in added if pat.search(a['t'])]
print('负责人裁决类新增行:', len(hits))
for h in hits:
    print('  %s:%d | %s' % (h['f'], h['n'], re.sub('\s+',' ',h['t'].strip())[:120]))
print()
pat2=re.compile('2026-09-1[0-9]')
h2=[a for a in added if pat2.search(a['t']) and pat.search(a['t'])]
print('同含日期+裁决措辞的新增行:', len(h2))
for h in h2: print('  %s:%d | %s' % (h['f'], h['n'], re.sub('\s+',' ',h['t'].strip())[:140]))
