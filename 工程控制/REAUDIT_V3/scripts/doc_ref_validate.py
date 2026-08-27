#!/usr/bin/env python3
"""Doc reference validation (Control §15): markdown links, backticked file refs, commit SHAs."""
import os, re, subprocess, json
REPO = '/home/lighthouse/Astro CS Database'
ROOT = open('/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt').read().strip()
OUT = os.path.join(ROOT, 'package', '13_static_quality')

broken_links = []
broken_file_refs = []
bad_commits = []
checked_links = 0
checked_refs = 0

all_files = set()
for dp, _, fns in os.walk(REPO):
    if '.git' in dp: continue
    for fn in fns:
        all_files.add(os.path.relpath(os.path.join(dp, fn), REPO))

def exists_rel(p):
    return p in all_files or os.path.exists(os.path.join(REPO, p))

for dp, _, fns in os.walk(os.path.join(REPO, 'docs')):
    for fn in fns:
        if not fn.endswith('.md'): continue
        p = os.path.join(dp, fn)
        rel = os.path.relpath(p, REPO)
        txt = open(p, encoding='utf-8', errors='ignore').read()
        for m in re.finditer(r'\!?\[[^\]]*\]\(([^)]+)\)', txt):
            t = m.group(1).strip()
            if t.startswith(('http', '#', 'mailto:')): continue
            frag = t.split('#')[0]
            if not frag: continue
            base = os.path.normpath(os.path.join(os.path.dirname(rel), frag)).replace(chr(92), '/')
            checked_links += 1
            if not exists_rel(base):
                broken_links.append((rel, t))
        for m in re.finditer(chr(96) + '([^' + chr(96) + ']+)' + chr(96), txt):
            t = m.group(1).strip()
            if not t.startswith(('lib/', 'docs/', 'tools/', 'run/')): continue
            if '*' in t or ' ' in t: continue
            checked_refs += 1
            if not exists_rel(t):
                broken_file_refs.append((rel, t))

commits = subprocess.run(['git', '-C', REPO, 'rev-list', '--all'], capture_output=True, text=True).stdout.split()
commit_set = set(commits)
short = set(c[:12] for c in commits)
commit_pat = re.compile(r'\b[0-9a-f]{12,40}\b')
for dp, _, fns in os.walk(os.path.join(REPO, 'docs')):
    for fn in fns:
        if not fn.endswith('.md'): continue
        p = os.path.join(dp, fn)
        rel = os.path.relpath(p, REPO)
        txt = open(p, encoding='utf-8', errors='ignore').read()
        for m in commit_pat.finditer(txt):
            sha = m.group(0).lower()
            if sha in commit_set or sha[:12] in short: continue
            bad_commits.append((rel, m.group(0)))

result = {
    'markdown_links_checked': checked_links,
    'broken_markdown_links': len(broken_links),
    'backticked_file_refs_checked': checked_refs,
    'broken_file_refs': len(broken_file_refs),
    'bad_commit_refs': len(bad_commits),
    'broken_links_sample': broken_links[:20],
    'broken_file_refs_sample': broken_file_refs[:20],
    'bad_commit_sample': bad_commits[:20],
}
os.makedirs(OUT, exist_ok=True)
open(os.path.join(OUT, 'doc_reference_validation.json'), 'w').write(json.dumps(result, indent=2, ensure_ascii=False))
print(json.dumps({k: v for k, v in result.items() if k in ('markdown_links_checked','broken_markdown_links','backticked_file_refs_checked','broken_file_refs','bad_commit_refs')}, indent=2))
