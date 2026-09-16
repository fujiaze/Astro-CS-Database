#!/usr/bin/env python3
"""ROOT-004 helper: split the 785-entry ledger into balanced shards (read-only)."""
import csv, os, json
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..','..','..','..'))
SCAN = os.path.join(ROOT,'问题扫描')
GEN  = os.path.join(ROOT,'reports','PROJECT-GOVERNANCE-01','root-scan','_gen')
ASG  = os.path.join(ROOT,'reports','PROJECT-GOVERNANCE-01','root-scan','shards','_assign')
os.makedirs(ASG, exist_ok=True)
rows = list(csv.DictReader(open(os.path.join(GEN,'idmap.csv'), encoding='utf-8')))
led  = {r['id']: r for r in csv.DictReader(open(os.path.join(SCAN,'账本','FIX_LEDGER.csv'), encoding='utf-8-sig'))}
for r in rows:
    L = led[r['id']]
    for k in ('position','evidence','clause','impact','suggested_disposition','related'):
        r[k] = (L.get(k) or '').replace('\n',' ').replace('\t',' ')[:400]

def cell(cat, pris):
    return [r for r in rows if r['category']==cat and r['priority'] in pris]

plan = []
plan.append(('A_SCI_DEF_P0',  cell('A_SCI_DEF',['P0'])))
p1 = cell('A_SCI_DEF',['P1']); plan.append(('A_SCI_DEF_P1a', p1[:38])); plan.append(('A_SCI_DEF_P1b', p1[38:]))
plan.append(('A_SCI_DEF_P2',  cell('A_SCI_DEF',['P2'])))
plan.append(('B_STD_MISMATCH_ALL', cell('B_STD_MISMATCH',['P0','P1','P2'])))
ca = cell('C_ALG_IMPL',['P0','P1','P2','P?']); plan.append(('C_ALG_IMPL_a', ca[:35])); plan.append(('C_ALG_IMPL_b', ca[35:]))
plan.append(('C_DOC_CODE_GAP_P0', cell('C_DOC_CODE_GAP',['P0'])))
c1 = cell('C_DOC_CODE_GAP',['P1']); plan.append(('C_DOC_CODE_GAP_P1a', c1[:32])); plan.append(('C_DOC_CODE_GAP_P1b', c1[32:]))
plan.append(('C_DOC_CODE_GAP_P2', cell('C_DOC_CODE_GAP',['P2'])))
plan.append(('D_COMMENT_P1', cell('D_COMMENT',['P1'])))
plan.append(('D_COMMENT_P2', cell('D_COMMENT',['P2'])))
plan.append(('E_TRACE_BREAK_P0P1', cell('E_TRACE_BREAK',['P0','P1'])))
plan.append(('E_TRACE_P2_J_FS', cell('E_TRACE_BREAK',['P2'])+cell('J_FS_PUBLISH',['P0','P1','P2'])))
plan.append(('F_TEST_GAP_P0', cell('F_TEST_GAP',['P0'])))
plan.append(('F_TEST_GAP_P1', cell('F_TEST_GAP',['P1'])))
plan.append(('F_TEST_GAP_P2', cell('F_TEST_GAP',['P2'])))
plan.append(('G_GOV_GATE_P0', cell('G_GOV_GATE',['P0'])))
g1 = cell('G_GOV_GATE',['P1'])
for i in range(4):
    part = g1[i*31:(i+1)*31]
    if part: plan.append((f'G_GOV_GATE_P1{chr(97+i)}', part))
plan.append(('G_GOV_GATE_P2', cell('G_GOV_GATE',['P2'])))
plan.append(('H_NUMERIC_ALL', cell('H_NUMERIC',['P0','P1','P2'])))
plan.append(('I_DOC_HYGIENE_ALL', cell('I_DOC_HYGIENE',['P0','P1','P2'])))

seen=set(); index=[]; total=0
for name, rs in plan:
    for r in rs:
        assert r['id'] not in seen, ('dup', r['id'])
        seen.add(r['id'])
    with open(os.path.join(ASG, name+'.tsv'),'w',newline='',encoding='utf-8') as f:
        w=csv.writer(f, delimiter='\t')
        w.writerow(['ID','原类别','原优先级','producer','文件','行','标题','旧位置字段','旧证据字段','旧条款字段','fix_state','verified_state'])
        for r in rs:
            w.writerow([r['id'],r['category'],r['priority'],r['producer'],r['file'],r['line'],r['title'],r['position'],r['evidence'],r['clause'],r['fix_state'],r['verified_state']])
    index.append({'shard':name,'n':len(rs),'first':rs[0]['id'] if rs else '','last':rs[-1]['id'] if rs else '','cats':sorted({r['category'] for r in rs}),'pris':sorted({r['priority'] for r in rs})})
    total+=len(rs)
print('shards:',len(plan),'entries covered:',total,'unique:',len(seen),'of',len(rows))
print('missing:',[r['id'] for r in rows if r['id'] not in seen])
json.dump(index, open(os.path.join(GEN,'shard_index.json'),'w',encoding='utf-8'), ensure_ascii=False, indent=1)
for i in index: print(' %-22s n=%-3d %s .. %s  %s' % (i['shard'], i['n'], i['first'], i['last'], ','.join(i['pris'])))

