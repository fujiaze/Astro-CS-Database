import subprocess
b=subprocess.run(['git','--no-optional-locks','show','HEAD:docs/traceability/TRACEABILITY_MATRIX.csv'],capture_output=True).stdout
wt=open('docs/traceability/TRACEABILITY_MATRIX.csv','rb').read()
print('HEAD blob first 12 bytes:', b[:12])
print('worktree first 12 bytes:', wt[:12])
print('blob == worktree:', b==wt, len(b), len(wt))
j=subprocess.run(['git','--no-optional-locks','show','HEAD:docs/traceability/TRACEABILITY_MATRIX.json'],capture_output=True).stdout
print('json blob bytes:', len(j))
import json,io,csv
mods=json.loads(j.decode('utf-8'))['modules']
CSV_COLS=['module_id','kind','requirement_ids','test_ids','test_path','evidence_id','evidence_status','notes']
r2=subprocess.run(['git','--no-optional-locks','show','HEAD:docs/traceability/TRACEABILITY_MATRIX.csv'],capture_output=True,text=True).stdout
rd=list(csv.reader(io.StringIO(r2)))
print('blob header[0] repr:', repr(rd[0][0]))
