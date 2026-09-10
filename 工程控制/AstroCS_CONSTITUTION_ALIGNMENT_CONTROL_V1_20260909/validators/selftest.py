#!/usr/bin/env python3
import argparse,subprocess,sys,tempfile,shutil,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);a=p.parse_args();r=a.root.resolve()
def run(x): return subprocess.run([sys.executable,str(x/'validators/validate_control.py'),'--root',str(x)],capture_output=True,text=True,timeout=60)
ok=run(r)
if ok.returncode: print(ok.stdout+ok.stderr);raise SystemExit(1)
with tempfile.TemporaryDirectory() as d:
 q=Path(d)/'pack';shutil.copytree(r,q);cp=json.loads((q/'control-pack.json').read_text());cp['tasks'][1]['depends_on']=['PACK-001'];(q/'control-pack.json').write_text(json.dumps(cp),encoding='utf-8')
 # ledger remains unchanged; mismatch/cycle mutation must fail
 bad=run(q)
 if bad.returncode==0: print('SELFTEST_FAIL: mutation accepted');raise SystemExit(1)
print('SELFTEST_PASS')
