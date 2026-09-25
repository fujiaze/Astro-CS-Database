# -*- coding: utf-8 -*-
"""Task 1.1  —— deterministic set algebra: judgement keys vs master keys.

Row key of a judgement row  = (符号/键, 位置路径) with the :行号 stripped,
matching how the master table names its rows ("符号或键" column).
Row key of a master row     = 符号或键 column, with the "(无名)·" prefix and
the trailing ":path:line" residue normalised the same way.
Nothing is judged here; only membership is computed.
"""
import csv, io, sys, re, collections, json

sys.stdout.reconfigure(encoding='utf-8')
ROOT = r'产出/'
MASTER = ROOT + r'\独立审计/02_科学\常数公式算法总台账.csv'
RAW = ROOT + r'\raw'
J = ['AUD-402-判读-A1.csv', 'AUD-402-判读-A2.csv', 'AUD-402-判读-A3.csv',
     'AUD-402-判读-BD1.csv']
OUT = ROOT + r'\复算\d4finish'


def rd(p):
    with io.open(p, encoding='utf-8-sig', newline='') as f:
        return [r for r in csv.reader(f)]


LINE_RE = re.compile(r':\d+(?:-\d+)?$')
PATH_RE = re.compile(r'[\w./\\-]+\.(?:h|hpp|cpp|cc|c|json|md|py|sh|cmake|txt|yaml|yml|csv|jsonl|toml|inl)\b')


def norm_symbol(s):
    """canonical symbol for join: drop (无名)·prefix, strip whitespace/case."""
    s = (s or '').strip()
    s = re.sub(r'^\(无名\)·', '', s)
    return s.lower()


def key_of_symbol_col(sym, loc):
    """join key = symbol + first path (line numbers removed)."""
    p = PATH_RE.search(loc or '')
    path = LINE_RE.sub('', p.group(0)) if p else ''
    return norm_symbol(sym) + ' @ ' + path.lower()


mrows = rd(MASTER)
mkeys = {}
for r in mrows[1:]:
    if len(r) < 16:
        continue
    mkeys[key_of_symbol_col(r[0], r[2])] = r[0]
msym = collections.Counter(norm_symbol(r[0]) for r in mrows[1:] if len(r) >= 16)

print('master data rows      =', sum(1 for r in mrows[1:] if len(r) >= 16))
print('master distinct keys  =', len(mkeys))
print('master distinct symbols =', len(msym))

jrows = []
for name in J:
    rows = rd(RAW + '\\' + name)
    for r in rows[1:]:
        jrows.append((name, r))
print('judgement rows total  =', len(jrows))
jkeys = collections.OrderedDict()
for name, r in jrows:
    jkeys.setdefault(key_of_symbol_col(r[0], r[1]), []).append((name, r))
print('judgement distinct keys =', len(jkeys))
jsym = collections.Counter(norm_symbol(r[0]) for _, r in jrows)
print('judgement distinct symbols =', len(jsym))

missing = [(k, v) for k, v in jkeys.items() if k not in mkeys]
print('\n### judgement keys NOT in master (by join key):', len(missing))
for k, v in missing:
    print('  -', k, '| sources:', sorted(set(n for n, _ in v)))

# symbol-only view: judgement symbols absent from master entirely
missing_sym = sorted(s for s in jsym if s not in msym)
print('\n### judgement SYMBOLS absent from master (symbol-only):', len(missing_sym))
for s in missing_sym:
    print('  -', s)

json.dump({'missing_keys': [[k, [[n, r] for n, r in v]] for k, v in missing],
           'missing_symbols': missing_sym,
           'master_keys': sorted(mkeys), 'master_symbols': sorted(msym),
           'j_symbols': sorted(jsym)},
          io.open(OUT + r'\setdiff_j_vs_master.json', 'w', encoding='utf-8'),
          ensure_ascii=False, indent=1)
print('\nwrote setdiff_j_vs_master.json')
