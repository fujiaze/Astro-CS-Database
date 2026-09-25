# -*- coding: utf-8 -*-
"""D4 finish —— task 1.2 evidence sheet: side coverage of the original 148
conflict-marked master rows (before / after the 补侧 pass, plus the reason any
side could not be added).  Read-only over the two CSVs; writes a TSV.
"""
import io, re, sys, csv, collections

sys.stdout.reconfigure(encoding='utf-8')
ROOT = r'产出/'
M = ROOT + r'\独立审计/02_科学\常数公式算法总台账.csv'
BK = ROOT + r'\复算\d4finish\master_backup_original.csv'
OUT = ROOT + r'\复算\d4finish\side_coverage_148.tsv'
SEP = '；'
SITE = re.compile(r'((?:[\w.\-]+/)*[\w.\-]+\.(?:h|hpp|hh|inl|cpp|cc|c|json|jsonl|md|py|sh|cmake|txt|yaml|yml|csv|toml|in))(?:[:：](\d+)(?:-\d+)?)?')


VALPAT = re.compile(r'[=＝≈:：]\s*[-+]?\d|[-+]?\d\.(?:0|[1-9]\d*)\b|\d+(?:\.\d+)?(?:e-?\d+)?\s*(?:px|s|ADU|mag|dex|%)')
SIDEPAT = re.compile(r'唯一数值源|唯一家|缺键兜底|兜底|默认|声明值|出厂模板|骨架|登记册|schema|契约')


def f(s):
    s = re.sub(r'（[^（）]*）$', '', s.strip())          # drop 补侧 annotation
    return re.sub(r'[:：]\d+(?:-\d+)?$', '', s).strip().lower()


def load(p):
    rows = list(csv.reader(io.open(p, encoding='utf-8-sig', newline='')))
    return {r[0]: r for r in rows[1:] if len(r) > 1}


new, old = load(M), load(BK)
conf = [k for k, r in old.items() if r[13].strip()]
rows = []
tally = collections.Counter()
for k in conf:
    o, n = old[k], new[k]
    os_ = [x for x in o[2].split(SEP) if x.strip()]
    ns_ = [x for x in n[2].split(SEP) if x.strip()]
    of_, nf_ = {f(x) for x in os_}, {f(x) for x in ns_}
    prose_valued, prose_anchor = set(), set()
    for m in SITE.finditer(o[9] + SEP + o[13]):
        p = m.group(1)
        if '/' not in p:
            continue
        tail = (o[9] + SEP + o[13])[m.end():m.end() + 70]
        head = (o[9] + SEP + o[13])[max(0, m.start() - 70):m.start()]
        (prose_valued if (VALPAT.search(tail) or SIDEPAT.search(tail) or VALPAT.search(head))
         else prose_anchor).add(f(p))
    bare = {m.group(1) for m in SITE.finditer(o[9] + SEP + o[13]) if '/' not in m.group(1)}
    missing = sorted(prose_valued - nf_)
    if not os_:
        state = '无站点条目（本行是口径/族条目，非某个数值的侧）'
    elif missing:
        state = '仍缺带值侧：' + '、'.join(missing[:4])
    elif bare and len(nf_) < 2:
        state = '另一侧仅以裸文件名出现（多义/无映射，不猜路径）'
    elif len(nf_) < 2 and prose_anchor:
        state = '另一侧是权威锚（文档条款/注释，非数值载体）'
    elif len(nf_) < 2:
        state = '单侧冲突（另一侧是非文件对象：措辞/常量本身）'
    else:
        state = '两侧以上已在位'
    tally[state.split('：')[0]] += 1
    rows.append([k, str(len(os_)), str(len(set(map(f, os_)))), str(len(ns_)),
                 str(len(nf_)), str(len(nf_) - len(set(map(f, os_)))), state])

with io.open(OUT, 'w', encoding='utf-8', newline='') as fh:
    w = csv.writer(fh)
    w.writerow(['符号或键', '位置集合项数·前', '载体文件数·前', '位置集合项数·后',
                '载体文件数·后', '补入侧数', '补侧后判定'])
    w.writerows(rows)
print('conflict rows =', len(conf))
for s, c in tally.most_common():
    print('  %-46s %d' % (s, c))
print('added sides total =', sum(int(r[5]) for r in rows))
print('rows still missing a resolvable side =', sum(1 for r in rows if r[6].startswith('仍缺侧')))
print('wrote', OUT)
