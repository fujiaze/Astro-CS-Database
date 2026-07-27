#!/usr/bin/env python3
from pathlib import Path
import csv, hashlib, sys

def main():
    root = Path(sys.argv[1] if len(sys.argv)>1 else '.').resolve()
    required = ['AUTONOMOUS_ENTRY.md','START_PROMPT.txt','control/PROJECT_STATE.yaml','control/CURRENT_TASK.md','control/MASTER_TASK_REGISTER.csv','agent/MASTER_AGENT_INSTRUCTIONS.md']
    missing=[x for x in required if not (root/x).is_file()]
    if missing:
        print('MISSING:', *missing, sep='\n'); return 2
    prompt=(root/'START_PROMPT.txt').read_text(encoding='utf-8').strip()
    if len(prompt)>100:
        print('START_PROMPT_TOO_LONG',len(prompt)); return 3
    with (root/'control/MASTER_TASK_REGISTER.csv').open(encoding='utf-8-sig',newline='') as f:
        rows=list(csv.DictReader(f))
    ids={r['task_id'] for r in rows}
    bad=[]
    for r in rows:
        if not (root/'tasks'/f"{r['task_id']}.md").is_file(): bad.append(r['task_id'])
        for dep in filter(None,r['dependencies'].split(';')):
            if dep not in ids: bad.append(f"{r['task_id']} unknown dep {dep}")
    if bad:
        print('TASK_ERRORS:',*bad,sep='\n'); return 4
    print(f'OK root={root} tasks={len(rows)} prompt_chars={len(prompt)}')
    return 0
if __name__=='__main__': raise SystemExit(main())
