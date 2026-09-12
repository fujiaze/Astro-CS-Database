#!/usr/bin/env python3
import argparse,csv,json,re,sys
from pathlib import Path
REQ={'00_READ_FIRST.md','01_BASELINE_AUDIT.md','02_GATES_AND_EXECUTION.md','03_AUDIT_PACKAGE_SPEC.md','TASK_LEDGER.csv','OWNER_BINDINGS.yaml','control-pack.json','schemas/evidence.schema.json'}
CRIT=set(["AIO-001", "AIO-002", "ARCH-AUDIT-P1", "ARCH-TB-001", "AUD-001", "BASE-001", "BASE-UTIL-001", "CI-001", "CI-001B", "CI-002", "CI-BACKEND-001", "CI-BASELINE-001", "CI-DATA-REG-001", "CI-REG-002", "CI-REPAIR-001", "CI-REPAIR-002", "CI-VER-CHK-001", "CI-WIN-001", "CLI-001", "CLI-001B", "CON-COMMENT-001", "CORE-RACE-001", "DATA-001", "DOC-001", "DOC-CONV-001", "GOV-001", "GOV-002", "GOV-AGENTS-001", "MOD-001", "MOD-001A", "MOD-001B", "P1-001", "P1-HIPS-DIGEST-001", "P2-001", "P2-002", "P3-001", "P3-002", "PACK-001", "PSF-001", "REAL-000", "REAL-001", "RT-001", "RT-001A", "RT-001B", "SCI-ANCHOR-001", "SCI-F2-001", "SCI-F3-001", "STD-F1-ADJ", "STD-REG-001", "UT-CLI-MAINT", "VIS-001", "WCS-001", "WCS-002", "WCS-003", "WCS-PATH-001", "WIN-000", "WIN-001"])
def fail(s): print('CONTROL_FAIL: '+s,file=sys.stderr);raise SystemExit(1)
def main():
 p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);r=p.parse_args().root.resolve()
 miss=sorted(x for x in REQ if not (r/x).is_file())
 if miss: fail('missing '+str(miss))
 try: cp=json.loads((r/'control-pack.json').read_text(encoding='utf-8'));json.loads((r/'schemas/evidence.schema.json').read_text(encoding='utf-8'))
 except Exception as e: fail('json '+str(e))
 if cp.get('schema')!='cprun/v4' or cp.get('workspace')!='../..': fail('cprun identity/workspace')
 rows=list(csv.DictReader((r/'TASK_LEDGER.csv').open(encoding='utf-8',newline='')))
 need={'task_id','owner_id','lane','depends_on','spec_ref','write_scope','timeout_sec','status'}
 if not rows or not need.issubset(rows[0]): fail('ledger columns')
 ids=[x['task_id'] for x in rows]
 if len(ids)!=len(set(ids)) or set(ids)!=CRIT: fail('task set/duplicate')
 graph={x['task_id']:[d for d in x['depends_on'].split('|') if d] for x in rows}
 for k,ds in graph.items():
  if set(ds)-set(ids): fail('unknown dep '+k)
 seen=set();active=set()
 def visit(n):
  if n in active: fail('cycle '+n)
  if n in seen:return
  active.add(n)
  for d in graph[n]:visit(d)
  active.remove(n);seen.add(n)
 for n in ids:visit(n)
 owners=set(re.findall(r'^  - id: ([A-Z0-9-]+)$',(r/'OWNER_BINDINGS.yaml').read_text(encoding='utf-8'),re.M))
 for x in rows:
  if x['owner_id'] not in owners: fail('owner '+x['task_id'])
  if x['lane'] not in cp['lanes']: fail('lane '+x['task_id'])
  if int(x['timeout_sec'])<=0: fail('timeout '+x['task_id'])
  if not (r/x['spec_ref']).is_file(): fail('spec '+x['task_id'])
  if x['lane']=='repo-write' and not x['write_scope']: fail('write scope '+x['task_id'])
 cpt={x['id']:x for x in cp.get('tasks',[])}
 if set(cpt)!=set(ids): fail('control/ledger mismatch')
 for x in rows:
  cpdeps=cpt[x['task_id']].get('depends_on',[])
  if cpdeps!=graph[x['task_id']]: fail('control/ledger dependency mismatch '+x['task_id'])
 if cp['lanes'].get('repo-write',{}).get('capacity')!=1: fail('repo-write capacity')
 text='\n'.join((r/x).read_text(encoding='utf-8') for x in ['00_READ_FIRST.md','01_BASELINE_AUDIT.md','02_GATES_AND_EXECUTION.md'])
 for tok in ['DRAFT_FOR_OWNER_REVIEW','FROZEN','P0/P1','Fatduck','call_count=1','NOT_READY_FOR_RELEASE']:
  if tok not in text: fail('hard requirement token '+tok)
 print(f'CONTROL_PASS tasks={len(rows)} files={sum(1 for x in r.rglob("*") if x.is_file())}')
if __name__=='__main__':main()
