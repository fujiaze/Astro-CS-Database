
import re, json
recs = json.load(open('问题扫描/_verify/_v8_comment_lines.json', encoding='utf-8'))
prose = [r for r in recs if r['kind'] in ('cmt-line','cmt-trailing','doc-line')]
pat = re.compile(r'(全部|零副本|零公式|唯一|无任何|永不|一律|绝不|逐字|逐位|不变|无硬编码|不依赖|无魔法|完整)')
hits=[r for r in prose if pat.search(r['t'])]
print('绝对化宣称命中:', len(hits))
for r in hits:
    t=r['t'].strip()
    if len(t)>240: t=t[:240]+'…'
    print('%s %s:%d  %s' % (r['c'], r['f'], r['n'], t))
