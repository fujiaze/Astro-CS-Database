# -*- coding: utf-8 -*-
"""D4 finish —— deterministic bucket statistics for the mechanical layer,
the judgement layer, the master table and the multi-side default prescreen.

Single command:
    python t2_stats.py            # writes stats.json + stats.txt
All counts are computed from the CSVs themselves (no judgement).
"""
import io, re, sys, csv, json, collections

sys.stdout.reconfigure(encoding='utf-8')
ROOT = r'产出/'
RAW = ROOT + r'\raw'
OUT = ROOT + r'\复算\d4finish'
S = collections.OrderedDict()


def rd(p):
    with io.open(p, encoding='utf-8-sig', newline='') as f:
        return [r for r in csv.reader(f) if r and len(r) > 1]


def is_test(path):
    return bool(re.search(r'(^|/)(eng/tests|tests|test)/|^实验/|/__tests__/', path))


def path_of(row, col):
    m = re.search(r'([\w.\-]+(?:/[\w.\-]+)+\.[A-Za-z]+)(?::\d+)?', row[col] or '')
    return m.group(1) if m else ''


# ---------- 1. mechanical layer ----------
main = rd(RAW + r'\AUD-402-常数台账.csv')[1:]
side = rd(RAW + r'\AUD-402-常数台账.旁表-无名值与测试钉值.csv')[1:]
S['mech_main_rows'] = len(main)
S['mech_side_rows'] = len(side)

bucket = collections.Counter()
for r in main:
    sym, loc = (r[0] or '').strip(), r[1] or ''
    p = path_of(r, 1)
    if sym in ('', '(无名)') or sym.startswith('(无名)'):
        bucket['无名行内'] += 1
    elif is_test(p):
        bucket['测试钉值'] += 1
    else:
        bucket['具名未判'] += 1
for r in side:
    sym, p = (r[0] or '').strip(), path_of(r, 1)
    if sym in ('', '(无名)') or sym.startswith('(无名)'):
        bucket['旁表-无名行内'] += 1
    elif is_test(p):
        bucket['旁表-测试钉值'] += 1
    else:
        bucket['旁表-具名'] += 1
S['mech_main_buckets'] = dict(bucket)

# ---------- 2. judgement layer ----------
J = ['AUD-402-判读-A1.csv', 'AUD-402-判读-A2.csv', 'AUD-402-判读-A3.csv',
     'AUD-402-判读-BD1.csv']
jr = []
for n in J:
    with io.open(RAW + '\\' + n, encoding='utf-8-sig', newline='') as f:
        allr = [r for r in csv.reader(f) if r and len(r) > 1]
    hdr, body = allr[0], allr[1:]
    S['judgement_rows_' + n.replace('AUD-402-判读-', '').replace('.csv', '')] = len(body)
    jr.extend(body)
S['judgement_rows_total'] = len(jr)
syms = collections.Counter((r[0] or '').strip() for r in jr)
named = {s for s in syms if s and not s.startswith('(无名)')}
S['judgement_distinct_rows'] = len(jr)
S['judgement_distinct_symbols_incl_unnamed'] = len(syms)
S['judgement_distinct_named_symbols'] = len(named)
S['judgement_unnamed_site_rows'] = sum(v for k, v in syms.items() if k.startswith('(无名)') or k == '')
disp = collections.Counter(r[9] for r in jr)
S['judgement_disposition'] = dict(disp)
src = collections.Counter(r[7] for r in jr)
S['judgement_source_status_top'] = dict(src.most_common(12))
# structural-not-applicable share
nas = sum(v for k, v in src.items() if k.startswith('不适用'))
S['judgement_share_不适用'] = round(nas / max(1, len(jr)), 4)

# ---------- 3. master table ----------
m = [r for r in csv.reader(io.open(ROOT + r'\独立审计/02_科学\常数公式算法总台账.csv',
                                   encoding='utf-8-sig', newline=''))][1:]
m = [r for r in m if len(r) >= 16]
S['master_rows'] = len(m)
S['master_类别'] = dict(collections.Counter(r[1] for r in m))
S['master_处置'] = dict(collections.Counter(r[10] for r in m))
S['master_冲突行数'] = sum(1 for r in m if r[13].strip())
S['master_复核层'] = dict(collections.Counter(
    '第②层' if '第②层' in r[12] else ('空' if not r[12].strip() else '判读层') for r in m))
fam = collections.Counter()
for r in m:
    ps = re.split(r'；|;', r[2] or '')
    first = path_of(r, 2)
    top = '/'.join(first.split('/')[:2]) if first else '(无路径)'
    fam[top] += 1
S['master_dir_family'] = dict(fam.most_common(20))
# rows with empty 位置集合
S['master_位置集合空'] = [r[0] for r in m if not (r[2] or '').strip()]
# sides per conflict row
def sides(r):
    txt = (r[2] or '')
    return [p for p in re.split(r'；|;', txt) if p.strip()]
conf = [r for r in m if (r[13] or '').strip()]
S['master_conflict_rows'] = len(conf)
S['master_conflict_side_hist'] = dict(sorted(collections.Counter(len(sides(r)) for r in conf).items()))
S['master_conflict_rows_1side'] = [r[0] for r in conf if len(sides(r)) <= 1]
# 待确认 rows: standard 02 §4.4 requires (a) conservative direction and
# (b) impact range.  The master has no dedicated column for them, so the test
# is run over the full row text (出处 / 适用域 / 冲突标记 / 备注-carrying cells).
pend = [r for r in m if r[10].strip() == '待确认']
S['master_待确认'] = len(pend)
CONS = re.compile(r'保守|建议|推荐|应当|应改|优先|取小|取大|偏保|偏严|偏松|不放松|上呈|待裁|需负责人|需外部|缺输入')
IMPACT = re.compile(r'影响|波及|范围|域|链路|下游|生产|可达|面')
S['master_待确认_有保守方向文字'] = sum(1 for r in pend if CONS.search(' '.join(r)))
S['master_待确认_有影响面文字'] = sum(1 for r in pend if IMPACT.search(' '.join(r[2:])))
S['master_待确认_两缺'] = [r[0] for r in pend
                        if not CONS.search(' '.join(r)) and not IMPACT.search(' '.join(r[2:]))]
S['master_待确认_空出处'] = [r[0] for r in pend if not (r[9] or '').strip()]

# ---------- 4. multi-side default prescreen ----------
ABSENT = ('', '该侧无此项', '—', '-', '缺', '无', 'N/A', 'null')
pre_all = rd(RAW + r'\D4-多侧默认值全表.csv')
pre = [r for r in pre_all[1:] if r[0].strip() != '键']          # file repeats its header
cand_all = rd(RAW + r'\D4-多侧默认值候选.csv')
cand = [r for r in cand_all[1:] if r[1].strip() != '键']
S['prescreen_rows'] = len(pre)
S['prescreen_cols'] = pre_all[0]
filled = collections.Counter()
div = 0
side_hist = collections.Counter()
for r in pre:
    vals = [r[i].strip() for i in range(3, 9)]
    nz = []
    for i, v in enumerate(vals):
        hit = v and not (v in ABSENT or v.startswith('该侧无此项'))
        if hit:
            filled['S%d' % (i + 1)] = filled.get('S%d' % (i + 1), 0) + 1
            nz.append(v.split('；')[0][:40])
    side_hist[len(nz)] += 1
    if len(set(nz)) > 1:
        div += 1
S['prescreen_side_fill'] = dict(filled)
S['prescreen_sides_present_hist'] = dict(sorted(side_hist.items()))
S['prescreen_keys_ge2_sides'] = sum(v for k, v in side_hist.items() if k >= 2)
S['prescreen_divergent_keys'] = div
S['prescreen_families'] = dict(collections.Counter(r[1] for r in pre).most_common(15))
S['prescreen_candidate_rows'] = len(cand)
S['prescreen_candidates'] = [(r[0], r[1], r[2]) for r in cand]

json.dump(S, io.open(OUT + r'\stats.json', 'w', encoding='utf-8'),
          ensure_ascii=False, indent=1)
for k, v in S.items():
    print(k, '=', v if not isinstance(v, dict) else '')
    if isinstance(v, dict):
        for a, b in list(v.items())[:15]:
            print('      %-46s %s' % (str(a)[:46], b))
