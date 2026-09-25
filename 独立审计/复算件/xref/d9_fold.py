import re
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
PKG = Path(r'独立审计/')
un = (PKG / '07_未决' / 'UNRESOLVED清单.md').read_text(encoding='utf-8')
reps = ['02_科学/天光无缝核验报告.md', '02_科学/面积交叠核验报告.md', '02_科学/SNR链路核验报告.md',
        '02_科学/测光链路核验报告.md']
TOKEN = re.compile(r'[A-Za-z][A-Za-z0-9_.\-]{2,}|1e[−\-]\d+|\d+\.\d+|\d{2,}')
rows = []
for f in reps:
    L = (PKG / f).read_text(encoding='utf-8').splitlines()
    st = next((i for i, l in enumerate(L) if '列入 UNRESOLVED' in l), None)
    if st is None:
        continue
    for l in L[st + 1:]:
        if l.startswith('#'):
            break
        s = l.strip()
        if not s.startswith('|') or set(s) <= set('|-: '):
            continue
        cells = [c.strip() for c in s.strip('|').split('|')]
        if len(cells) < 2 or not cells[0] or cells[0] == '项':
            continue
        rows.append((f, cells[0], cells[-1] if len(cells) > 2 else ''))
print('各报告"列入 UNRESOLVED"表条目共 %d 条' % len(rows))
miss = []
for f, item, need in rows:
    toks = set(TOKEN.findall(item + ' ' + need))
    hit = [t for t in toks if t in un]
    cn = re.sub(r'[`|*（）()].', '', item).strip()
    cn_hit = cn[:6] in un.replace(' ', '') if len(cn) >= 6 else False
    if not hit and not cn_hit:
        miss.append((f, item, need, sorted(toks)[:5]))
print('=== 07 未收录（按拉丁/数字 token 与中文前缀双路匹配）：%d 条 ===' % len(miss))
for f, item, need, tk in miss:
    print('   [%s] %s' % (f.split('/')[-1][:12], item[:78]))
    print('        缺什么：%s   token=%s' % (need[:70], tk))
