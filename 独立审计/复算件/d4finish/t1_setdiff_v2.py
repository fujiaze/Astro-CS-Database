# -*- coding: utf-8 -*-
"""Task 1.1 v2 —— robust join key between judgement CSVs and the master table.

Two key families:
  NAMED:<symbol>          symbol text normalised (parenthetical qualifier and
                          trailing path:line residue removed, lower-cased)
  UNNAMED:<path>:<line>   for judgement/master rows whose symbol is (无名)/empty;
                          identity is the concrete site, not a symbol.
Membership only -- no judgement is made here.
"""
import csv, io, re, sys, json, collections

sys.stdout.reconfigure(encoding='utf-8')
ROOT = r'产出/'
MASTER = ROOT + r'\独立审计/02_科学\常数公式算法总台账.csv'
RAW = ROOT + r'\raw'
OUT = ROOT + r'\复算\d4finish'
J = ['AUD-402-判读-A1.csv', 'AUD-402-判读-A2.csv', 'AUD-402-判读-A3.csv',
     'AUD-402-判读-BD1.csv']

PATHLINE = re.compile(
    r'((?:[\w.\-]+/)+[\w.\-]+\.(?:h|hpp|hh|inl|cpp|cc|c|json|jsonl|md|py|sh|cmake|'
    r'txt|yaml|yml|csv|toml|in|def))(?::(\d+)(?:-(\d+))?)?', re.I)
UNNAMED = re.compile(r'^\(无名\)')


def rd(p):
    with io.open(p, encoding='utf-8-sig', newline='') as f:
        return list(csv.reader(f))


def norm_named(sym):
    s = sym.strip()
    s = UNNAMED.sub('', s)                       # drop (无名)· prefix
    s = re.split(r'[（(]', s, 1)[0]              # drop parenthetical qualifier
    s = re.sub(r'[\s:：/\\]*[\w./\-]+\.(h|hpp|cpp|cc|json|md|py|yaml|yml|csv|txt|jsonl)\b.*$',
               '', s, flags=re.I)               # drop embedded path tail
    return 'NAMED:' + s.strip().strip('·').lower()


def keys(sym, loc):
    """return (primary_key, kind)."""
    s = (sym or '').strip()
    if s == '' or UNNAMED.match(s):
        m = PATHLINE.search(loc or '') or PATHLINE.search(s)
        if m:
            return 'UNNAMED:%s:%s' % (m.group(1).lower(), m.group(2) or '?'), 'unnamed'
        return 'UNNAMED:%s:?' % s.lower(), 'unnamed'
    return norm_named(s), 'named'


# ---------- master ----------
mrows = [r for r in rd(MASTER)[1:] if len(r) >= 16]
mkeys = collections.OrderedDict()
for r in mrows:
    k, kind = keys(r[0], r[2])
    mkeys.setdefault(k, []).append(r)
m_named = sum(1 for k in mkeys if k.startswith('NAMED:'))
m_un = sum(1 for k in mkeys if k.startswith('UNNAMED:'))

# ---------- judgement ----------
jrows = []
for name in J:
    for r in rd(RAW + '\\' + name)[1:]:
        jrows.append((name, r))
jkeys = collections.OrderedDict()
for name, r in jrows:
    k, kind = keys(r[0], r[1])
    jkeys.setdefault(k, []).append((name, r))
j_named = sum(1 for k in jkeys if k.startswith('NAMED:'))
j_un = sum(1 for k in jkeys if k.startswith('UNNAMED:'))

print('master rows=%d distinct keys=%d (named=%d unnamed-site=%d)'
      % (len(mrows), len(mkeys), m_named, m_un))
print('judgement rows=%d distinct keys=%d (named=%d unnamed-site=%d)'
      % (len(jrows), len(jkeys), j_named, j_un))

miss_k = [k for k in jkeys if k not in mkeys]
miss_named_k = [k for k in miss_k if k.startswith('NAMED:')]
miss_un_k = [k for k in miss_k if k.startswith('UNNAMED:')]
print('\n[A] judgement keys absent from master = %d  (named %d / unnamed-site %d)'
      % (len(miss_k), len(miss_named_k), len(miss_un_k)))
for k in miss_named_k:
    src = collections.Counter(n for n, _ in jkeys[k])
    print('   NAMED  %-58s rows=%d %s' % (k[6:], sum(src.values()), dict(src)))
for k in miss_un_k:
    src = collections.Counter(n for n, _ in jkeys[k])
    print('   UNNAME %-58s rows=%d %s' % (k[8:], sum(src.values()), dict(src)))

extra = [k for k in mkeys if k not in jkeys]
print('\n[B] master keys never seen in judgement layer = %d' % len(extra))
cnt = collections.Counter(k.split(':')[1] if k.startswith('NAMED:') else '(无名站点)' for k in extra)
print('   by source-成稿 of master row:')
bysrc = collections.Counter()
for k in extra:
    for r in mkeys[k]:
        bysrc[r[14].strip() or '(空)'] += 1
for s, c in bysrc.most_common():
    print('     %5d  %s' % (c, s[:90]))

json.dump({'judgement_absent_named': miss_named_k,
           'judgement_absent_unnamed': miss_un_k,
           'master_only_keys': extra,
           'master_keys': list(mkeys), 'judgement_keys': list(jkeys)},
          io.open(OUT + r'\t1_setdiff_v2.json', 'w', encoding='utf-8'),
          ensure_ascii=False, indent=1)
print('\nwrote t1_setdiff_v2.json')
