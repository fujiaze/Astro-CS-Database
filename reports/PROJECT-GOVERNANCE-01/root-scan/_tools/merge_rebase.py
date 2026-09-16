#!/usr/bin/env python3
"""ROOT-004 merge tool: validate & merge 26 shard PSVs -> 问题扫描/REBASE_TABLE.md + _gen/stats.json.
Read-only over the repo; writes only under 问题扫描/ and reports/PROJECT-GOVERNANCE-01/root-scan/_gen/.
Exit codes: 0 = all shards present, valid, coverage proven, table written; 2 = shards missing/incomplete (dry report);
3 = validation failure in present shards (bad columns/state/id, dup, set-diff)."""
import os, re, csv, json, sys, collections

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..','..','..','..'))
GEN  = os.path.join(ROOT,'reports','PROJECT-GOVERNANCE-01','root-scan','_gen')
SHD  = os.path.join(ROOT,'reports','PROJECT-GOVERNANCE-01','root-scan','shards')
TABLE= os.path.join(ROOT,'问题扫描','REBASE_TABLE.md')
STATS= os.path.join(GEN,'stats.json')
STRICT = '--force' not in sys.argv  # default: only write when everything complete

HEADER = "ID|原类别|原优先级|旧判据(文档+节号/路径)|最新权威条款|当前证据(命令+输出)|结论|归属|GAP关系|备注"
STATES = {"OPEN","RESOLVED","VOID","UNVERIFIABLE"}

led = list(csv.DictReader(open(os.path.join(ROOT,'问题扫描','账本','FIX_LEDGER.csv'), encoding='utf-8-sig')))
LID = [r['id'] for r in led]
LSET = set(LID)
LPRI = {r['id']: r['priority'] for r in led}
LCAT = {r['id']: r['category'] for r in led}

idx = json.load(open(os.path.join(GEN,'shard_index.json')))
shards = idx if isinstance(idx, list) else idx.get('shards', [])
names = [s.get('shard') or s.get('name') for s in shards]
exp_n = { (s.get('shard') or s.get('name')): (s.get('n') or s.get('entries')) for s in shards }

errors, missing = [], []
rows, seen = [], collections.Counter()
per_shard = {}
for name in names:
    p = os.path.join(SHD, name + '.psv')
    if not os.path.isfile(p):
        missing.append(name); continue
    with open(p, encoding='utf-8') as f:
        lines = [ln.rstrip('\n') for ln in f if ln.strip()]
    if not lines or lines[0] != HEADER:
        errors.append(f"{name}: bad header"); missing.append(name+'(header)'); continue
    got = []
    for i, ln in enumerate(lines[1:], 2):
        cols = ln.split('|')
        if len(cols) != 10:
            errors.append(f"{name} line {i}: NF={len(cols)}"); continue
        _id = cols[0].strip()
        if _id not in LSET: errors.append(f"{name} line {i}: unknown id {_id}"); continue
        if cols[6].strip() not in STATES: errors.append(f"{name} line {i}: bad state {cols[6]!r}"); continue
        if cols[2].strip() != LPRI[_id]: errors.append(f"{name} line {i}: priority mismatch {cols[2]} vs {LPRI[_id]}"); continue
        if cols[1].strip() != LCAT[_id]: errors.append(f"{name} line {i}: category mismatch {cols[1]} vs {LCAT[_id]}"); continue
        seen[_id] += 1
        got.append((_id, cols))
    if len(got) != exp_n[name]:
        errors.append(f"{name}: got {len(got)} rows, expected {exp_n[name]}")
        missing.append(f"{name}({len(got)}/{exp_n[name]})")
    per_shard[name] = got

dup = [k for k,v in seen.items() if v>1]
if dup: errors.append(f"duplicate ids across shards: {dup[:20]}")
covered = set(seen)
miss_ids = LSET - covered
extra_ids = covered - LSET
if extra_ids: errors.append(f"ids not in ledger: {sorted(extra_ids)[:10]}")

status = {'total_expected': len(LSET), 'shards_expected': len(names), 'shards_complete': len(names)-len(missing),
          'missing_or_incomplete_shards': missing, 'row_count': sum(len(g) for g in per_shard.values()),
          'missing_ids': sorted(miss_ids)[:50], 'missing_id_count': len(miss_ids), 'errors': errors[:80],
          'error_count': len(errors)}
print(json.dumps(status, ensure_ascii=False, indent=1))

if errors or missing or len(covered)!=len(LSET):
    print("MERGE NOT WRITTEN — fix shards first" + ("" if STRICT else " (force-write skipped: still needs covered ids)"))
    if not STRICT:
        print("NOTE: --force does not fabricate missing rows; table requires full coverage.")
    sys.exit(3 if errors else 2)

# --- coverage proof 3: idmap anchor files exist
imap = {r['id']: r for r in csv.DictReader(open(os.path.join(GEN,'idmap.csv'), encoding='utf-8-sig'))}
nofile = [i for i in LID if not imap.get(i,{}).get('file') or not os.path.isfile(os.path.join(ROOT, imap[i]['file']))]
if nofile: print("ANCHOR FILE PROBLEM:", nofile[:10]); sys.exit(3)

# --- write REBASE_TABLE.md
def esc(c): return c.replace('\n',' ').replace('|','｜')
order = []
for name in names:
    for _id, cols in per_shard[name]:
        order.append((name,_id,cols))
with open(TABLE,'w',encoding='utf-8') as f:
    f.write("# REBASE_TABLE · 旧 bug 清单按最新权威重定版逐条总表（ROOT-004）\n\n")
    f.write("- 基线：HEAD = main = origin/main = 2c328348304d033aecfa81faf79d1c6cd802b30a（2026-09-16 接手轮实测）。\n")
    f.write("- 口径：问题扫描/REBASE.md（四态/10 列/覆盖度证明）；分片判定与 P0 主控复核同表。\n")
    f.write("- 行数 = 785 = FIX_LEDGER.csv 行数；ID 双向差集为空（merge_rebase.py 校验后写出）。\n\n")
    f.write("| " + HEADER.replace('|',' | ') + " |\n")
    f.write("|" + "---|"*10 + "\n")
    for name,_id,cols in order:
        f.write("| " + " | ".join(esc(c) for c in cols) + " |\n")

# --- stats
concl = collections.Counter(c[6].strip() for _,c in order)
by = lambda k: collections.Counter(k(c) for _,c in order)
open_pri = collections.Counter(c[2].strip() for _,c in order if c[6].strip()=='OPEN')
attr = collections.Counter(c[7].strip() for _,c in order)
attr_open = collections.Counter(c[7].strip() for _,c in order if c[6].strip()=='OPEN')
gaprel = collections.Counter('重复' if c[8].strip().startswith('与 GAP') or c[8].strip().startswith('与GAP') else c[8].strip() for _,c in order)
# simpler cross tab:
ct = collections.Counter((c[1].strip(), c[2].strip(), c[6].strip()) for _,c in order)
cat_state = collections.Counter()
for (cat,pri,st),n in ct.items():
    cat_state[(cat,st)] += n
void_no_old = [_id for _,_id,c in order if c[6].strip()=='VOID' and not c[3].strip()]
void_no_alt = [_id for _,_id,c in order if c[6].strip()=='VOID' and (not c[9].strip() or c[9].strip()=='过时')]
unver_no_gap = [_id for _,_id,c in order if c[6].strip()=='UNVERIFIABLE' and not c[9].strip()]
res_no_note  = [_id for _,_id,c in order if c[6].strip()=='RESOLVED' and not c[9].strip()]
stats = {
 'baseline_sha':'2c328348304d033aecfa81faf79d1c6cd802b30a',
 'row_count': len(order), 'ledger_count': len(LSET),
 'conclusions': dict(concl),
 'open_by_priority': dict(open_pri),
 'attribution_all': dict(attr), 'attribution_open': dict(attr_open),
 'gap_relation': dict(gaprel),
 'cross_tab': {f"{a}|{b}": dict((s,n) for (c2,p2,s),n in ct.items() if c2==a and p2==b) for a,b in sorted({(c[1].strip(),c[2].strip()) for _,c in order})},
 'cat_state': {f"{a}|{b}": n for (a,b),n in cat_state.items()},
 'quality_gate': {'void_missing_oldbasis': void_no_old, 'void_missing_replacement': void_no_alt,
                  'unverifiable_missing_note': unver_no_gap, 'resolved_missing_note': res_no_note},
 'next_pack_rows': sorted({_id for _,_id,c in order if c[7].strip().startswith('NEXT-PACK')}),
}
json.dump(stats, open(STATS,'w',encoding='utf-8'), ensure_ascii=False, indent=1)
print("WROTE", os.path.relpath(TABLE, ROOT), len(order), "rows;", os.path.relpath(STATS, ROOT))
print(json.dumps({'conclusions':dict(concl),'open_by_priority':dict(open_pri)}, ensure_ascii=False))