# -*- coding: utf-8 -*-
"""Task 1.1 ②  —— harvest "常数／容差／阈值／幂次" entries named in the layer-2
review documents (复核-AUD201 / 202 / 202-补 / 203 / 204) and diff them against
the master table keys.

Harvest rule (deterministic, text-only):
  * candidate token = a back-ticked identifier, or a dotted config key
    ([a-z_][a-z0-9_]*(\.[a-z0-9_]+)+), or an ALL_CAPS macro, appearing on a line
    that also carries one of the category words
    常数|容差|阈值|幂次|阶数|默认值|门限|精度|上界|下界|下限|上限|系数|指数
  * candidate is kept only if it is not a pure path / pure number.
Membership only; no re-judgement.
"""
import io, re, sys, csv, json, collections

sys.stdout.reconfigure(encoding='utf-8')
ROOT = r'产出/'
RAW = ROOT + r'\raw'
OUT = ROOT + r'\复算\d4finish'
DOCS = ['复核-AUD201.md', '复核-AUD202.md', '复核-AUD202-补.md',
        '复核-AUD203.md', '复核-AUD204.md']
CATWORDS = re.compile(r'常数|容差|阈值|幂次|阶数|默认值|门限|精度|上界|下界|下限|上限|系数|指数|兜底')
TOK = re.compile(r'`([^`\n]{1,60})`|([a-z_][a-z0-9_]*(?:\.[a-z0-9_]+){1,4})|([A-Z][A-Z0-9_]{3,})')
PATHY = re.compile(r'(/|\.(h|hpp|cpp|cc|md|json|py|sh|yaml|yml|csv|txt|jsonl|cmake)\b|:\d)', re.I)
SPACED = re.compile(r'\s')


def norm(t):
    return t.strip().strip('.,;:，。；：、').lower()


def harvest(path):
    txt = io.open(path, encoding='utf-8').read()
    out = collections.OrderedDict()
    for ln, line in enumerate(txt.splitlines(), 1):
        if not CATWORDS.search(line):
            continue
        for m in TOK.finditer(line):
            t = norm(m.group(0).strip('`'))
            if len(t) < 3 or PATHY.search(t) or SPACED.search(t):
                continue
            if re.fullmatch(r'[\d.eE+\-]+', t):
                continue
            out.setdefault(t, []).append(ln)
    return out


allc = collections.OrderedDict()
for d in DOCS:
    h = harvest(RAW + '\\' + d)
    print('%-22s candidate entries = %d' % (d, len(h)))
    for t, lns in h.items():
        allc.setdefault(t, {})[d] = lns[:6]
print('union of candidate tokens =', len(allc))

# master key space (same normalisation as t1_setdiff_v2)
MASTER = ROOT + r'\独立审计/02_科学\常数公式算法总台账.csv'
mrows = [r for r in csv.reader(io.open(MASTER, encoding='utf-8-sig', newline=''))][1:]
mrows = [r for r in mrows if len(r) >= 16]
mtoks = set()
for r in mrows:
    s = norm(r[0])
    s = re.sub(r'^\(无名\)·', '', s)
    mtoks.add(s)
    for w in re.findall(r'[a-z_][a-z0-9_]*(?:\.[a-z0-9_]+){0,4}', s):
        mtoks.add(norm(w))
    # every identifier inside the whole row text is "covered" too
    for w in re.findall(r'[a-zA-Z_][a-zA-Z0-9_.]{2,}', ' | '.join(r[2:10])):
        mtoks.add(norm(w))

miss = [(t, v) for t, v in allc.items() if t not in mtoks]
print('\n### layer-2 review tokens NOT found anywhere in master row text: %d' % len(miss))
for t, v in sorted(miss):
    print('  %-40s %s' % (t, {k: vv[:3] for k, vv in v.items()}))

json.dump({t: v for t, v in miss}, io.open(OUT + r'\t1_review_tokens_missing.json', 'w',
          encoding='utf-8'), ensure_ascii=False, indent=1)
print('\nwrote t1_review_tokens_missing.json')
