#!/usr/bin/env python3
from pathlib import Path
import csv, sys

def load(root):
    p=root/'control/MASTER_TASK_REGISTER.csv'
    with p.open(encoding='utf-8-sig',newline='') as f: rows=list(csv.DictReader(f)); fields=f.fieldnames
    return p,fields,rows

def save(p,fields,rows):
    with p.open('w',encoding='utf-8-sig',newline='') as f:
        w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(rows)

def next_ready(rows):
    done={r['task_id'] for r in rows if r['status']=='DONE'}
    active=[r for r in rows if r['status']=='IN_PROGRESS']
    if active:return active[0]
    for r in rows:
        if r['status']=='TODO' and all(d in done for d in filter(None,r['dependencies'].split(';'))): return r
    return None

def main():
    root=Path(sys.argv[1]).resolve(); cmd=sys.argv[2]
    p,fields,rows=load(root)
    if cmd=='next':
        r=next_ready(rows); print(r['task_id'] if r else 'NONE'); return 0
    if len(sys.argv)<4: return 2
    tid=sys.argv[3]; r=next((x for x in rows if x['task_id']==tid),None)
    if not r: print('UNKNOWN_TASK'); return 3
    if cmd=='start': r['status']='IN_PROGRESS'
    elif cmd=='done': r['status']='DONE'
    elif cmd=='block': r['status']='BLOCKED'
    elif cmd=='fail': r['status']='FAILED'
    else:return 4
    save(p,fields,rows); print(tid,r['status']); return 0
if __name__=='__main__': raise SystemExit(main())
