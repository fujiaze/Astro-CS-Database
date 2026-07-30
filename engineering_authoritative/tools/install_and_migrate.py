from pathlib import Path
import argparse, shutil, datetime, json

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--repo',required=True); ap.add_argument('--pack',required=True); args=ap.parse_args()
    repo=Path(args.repo).resolve(); pack=Path(args.pack).resolve()
    if not repo.exists() or not pack.exists(): raise SystemExit('repo/pack not found')
    ts=datetime.datetime.utcnow().strftime('%Y%m%dT%H%M%SZ')
    archive=repo/'docs'/'archive'; archive.mkdir(parents=True,exist_ok=True)
    if (repo/'README.md').exists(): shutil.copy2(repo/'README.md',archive/f'README_pre_authoritative_{ts}.md')
    shutil.copy2(pack/'README.md',repo/'README.md')
    dst=repo/'engineering_authoritative'
    dst.mkdir(exist_ok=True)
    for name in ['agent','docs','control','tasks','checklists','contracts','templates','tools','migration','evidence']:
        src=pack/name; target=dst/name
        if target.exists(): shutil.copytree(target,archive/f'engineering_authoritative_{name}_{ts}')
        shutil.copytree(src,target,dirs_exist_ok=True)
    report={'installed_at_utc':ts,'readme_backup':str(archive/f'README_pre_authoritative_{ts}.md'),'legacy_evidence_preserved':True,'old_281_hiss':'debug_only','full_regression_allowed':False}
    (dst/'migration'/f'INSTALL_REPORT_{ts}.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report,indent=2))
if __name__=='__main__': main()
