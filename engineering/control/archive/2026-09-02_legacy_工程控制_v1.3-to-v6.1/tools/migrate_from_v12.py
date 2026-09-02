#!/usr/bin/env python3
from pathlib import Path
import argparse, csv, shutil, json, datetime

PASS_LINE='VERDICT: PASS'

def read_rows(path):
    with path.open(encoding='utf-8-sig',newline='') as f:
        reader=csv.DictReader(f)
        fields=reader.fieldnames
        rows=list(reader)
        return rows, fields

def has_pass(evidence):
    p=evidence/'REVIEW_REPORT.md'
    if not p.is_file(): return False
    lines=[x.strip() for x in p.read_text(encoding='utf-8',errors='replace').splitlines() if x.strip()]
    return bool(lines and lines[-1]==PASS_LINE)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--repo',default='.')
    ap.add_argument('--new-root',default='engineering_v1.3')
    args=ap.parse_args()
    repo=Path(args.repo).resolve(); new=(repo/args.new_root).resolve(); old=repo/'engineering_v1.2'
    if not new.is_dir():
        raise SystemExit(f'new root missing: {new}')
    snap=new/'control'/'migration_snapshots'/datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    snap.mkdir(parents=True,exist_ok=True)
    new_reg=new/'control'/'MASTER_TASK_REGISTER.csv'
    new_rows,fields=read_rows(new_reg)
    if not old.is_dir():
        print('NO_V12_FOUND: keep pack defaults')
        return 0
    old_reg=old/'control'/'MASTER_TASK_REGISTER.csv'
    if old_reg.is_file():
        shutil.copy2(old_reg,snap/'MASTER_TASK_REGISTER_v12.csv')
        old_rows,_=read_rows(old_reg); om={r['task_id']:r for r in old_rows}
        for r in new_rows:
            if r['task_id'] not in om: continue
            status=om[r['task_id']].get('status','TODO').upper()
            ev_old=old/om[r['task_id']].get('evidence_dir',f"evidence/{r['task_id']}")
            if status=='DONE' and not has_pass(ev_old): status='IN_PROGRESS'
            if r['task_id']=='P11-004' and status in {'DEFERRED','BLOCKED','FAILED','TODO'}: status='IN_PROGRESS'
            if status not in {'TODO','IN_PROGRESS','DONE','BLOCKED','FAILED'}: status='TODO'
            r['status']=status
    old_ev=old/'evidence'; new_ev=new/'evidence'
    if old_ev.is_dir():
        for item in old_ev.iterdir():
            dst=new_ev/item.name
            if not dst.exists():
                if item.is_dir(): shutil.copytree(item,dst)
                else: shutil.copy2(item,dst)
    # Always preserve old control files
    for name in ['PROJECT_STATE.yaml','CURRENT_TASK.md','DECISION_REGISTER.md','RISK_REGISTER.csv']:
        p=old/'control'/name
        if p.is_file(): shutil.copy2(p,snap/name)
    with new_reg.open('w',encoding='utf-8-sig',newline='') as f:
        w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(new_rows)
    done={r['task_id'] for r in new_rows if r['status']=='DONE'}
    current=next((r for r in new_rows if r['status']=='IN_PROGRESS'),None)
    if not current:
        current=next((r for r in new_rows if r['status']=='TODO' and all(d in done for d in filter(None,r['dependencies'].split(';')))),None)
    (new/'control'/'CURRENT_TASK.md').write_text('# 当前任务\n\n'+(f"`{current['task_id']}`：{current['title']}。\n" if current else '无可执行任务，请检查阻塞或全部完成。\n'),encoding='utf-8')
    print(json.dumps({'migrated_from':str(old),'snapshot':str(snap),'current_task':current['task_id'] if current else None},ensure_ascii=False))
    return 0
if __name__=='__main__': raise SystemExit(main())
