#!/usr/bin/env python3
from pathlib import Path
import csv, sys, json

def main():
    root=Path(sys.argv[1] if len(sys.argv)>1 else '.').resolve()
    required=['AUTONOMOUS_ENTRY.md','START_PROMPT.txt','control/PROJECT_STATE.yaml','control/CURRENT_TASK.md','control/MASTER_TASK_REGISTER.csv','control/P11_004_REVIEW_DECISION.json','agent/MASTER_AGENT_INSTRUCTIONS.md','docs/23_P11_004_REVIEW_DECISION.md','docs/24_WCS_VALIDATION_V2_SPEC.md','tools/migrate_from_v12.py','review_inputs/P11-004_review_bundle.zip']
    missing=[x for x in required if not (root/x).is_file()]
    if missing: print('MISSING:',*missing,sep='\n'); return 2
    prompt=(root/'START_PROMPT.txt').read_text(encoding='utf-8').strip()
    if len(prompt)>100: print('START_PROMPT_TOO_LONG',len(prompt)); return 3
    with (root/'control/MASTER_TASK_REGISTER.csv').open(encoding='utf-8-sig',newline='') as f:
        rows=list(csv.DictReader(f))
    ids={r['task_id'] for r in rows}; bad=[]
    for r in rows:
        if not (root/'tasks'/f"{r['task_id']}.md").is_file(): bad.append(r['task_id'])
        for dep in filter(None,r['dependencies'].split(';')):
            if dep not in ids: bad.append(f"{r['task_id']} unknown dep {dep}")
    if bad: print('TASK_ERRORS:',*bad,sep='\n'); return 4
    decision=json.loads((root/'control/P11_004_REVIEW_DECISION.json').read_text(encoding='utf-8'))
    if decision.get('decision_id')!='ADR-P11-004-GATE-V2': print('BAD_DECISION'); return 5
    print(f'OK root={root} tasks={len(rows)} prompt_chars={len(prompt)} review_bundle=yes')
    return 0
if __name__=='__main__': raise SystemExit(main())
