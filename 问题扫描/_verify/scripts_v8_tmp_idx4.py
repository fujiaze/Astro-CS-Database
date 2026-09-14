
import json, re
added = json.load(open('问题扫描/_verify/_v8_added.json', encoding='utf-8'))
hits=[a for a in added if a['f']=='lib/snr_estimator/README.md' and 205<=a['n']<=220]
for h in hits: print(h['n'], h['t'].strip()[:150])
print('--- added-line attribution for the 4-site claim ---')
txt=open('问题扫描/_verify/_v8_diff_percommit.txt', encoding='utf-8').read()
cur=None
for line in txt.split('\n'):
    m=re.match(r'COMMITSEP (\S+)', line)
    if m: cur=m.group(1)[:8]; continue
    if line.startswith('+') and '4 处计算' in line:
        print(cur, line[:150])
