
import re, json
recs = json.load(open('问题扫描/_verify/_v8_comment_lines.json', encoding='utf-8'))
prose = [r for r in recs if r['kind'] in ('cmt-line','cmt-trailing','doc-line')]
print('prose candidates:', len(prose))

# Pass A: self-reported counts
pat = re.compile(r'(全部|逐字节|每个|共\s*\d+|\d+\s*(个|处|项|条|行|次|步|座|份|档|组|枚|字节|导出|符号|测试|用例|文件|函数|常量|维度|列))')
hits = [r for r in prose if pat.search(r['t'])]
print('== Pass A hits:', len(hits))
for r in hits:
    print('%s %s:%d [%s] %s' % (r['c'], r['f'], r['n'], r['kind'], r['t'].strip()[:200]))
