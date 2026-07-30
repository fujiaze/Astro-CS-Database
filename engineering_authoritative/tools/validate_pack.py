from pathlib import Path
import argparse, csv, sys

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--pack-root',required=True); args=ap.parse_args()
    root=Path(args.pack_root)
    required=['README.md','AUTONOMOUS_ENTRY.md','START_PROMPT.txt','control/MASTER_TASK_REGISTER.csv','control/PROJECT_STATE.yaml']
    missing=[x for x in required if not (root/x).exists()]
    if missing: raise SystemExit('Missing: '+', '.join(missing))
    prompt=(root/'START_PROMPT.txt').read_text(encoding='utf-8').strip()
    if len(prompt)>100: raise SystemExit(f'Start prompt too long: {len(prompt)}')
    with (root/'control/MASTER_TASK_REGISTER.csv').open(encoding='utf-8') as f:
        rows=list(csv.DictReader(f))
    ids={r['task_id'] for r in rows}
    for r in rows:
        for d in filter(None,r['dependencies'].split(',')):
            if d not in ids: raise SystemExit(f"Unknown dependency {d} for {r['task_id']}")
    # cycle check
    graph={r['task_id']:[d for d in r['dependencies'].split(',') if d] for r in rows}
    visiting=set(); done=set()
    def dfs(n):
        if n in visiting: raise SystemExit('Cycle at '+n)
        if n in done:return
        visiting.add(n)
        for d in graph[n]:dfs(d)
        visiting.remove(n);done.add(n)
    for n in graph:dfs(n)
    print(f'PASS: {len(rows)} tasks, prompt length {len(prompt)}, required files present')
if __name__=='__main__': main()
