
import os,re
ROOT="/workspace/Astro CS Database"
t=open(ROOT+"/tests/monitoring/test_log_contract.py",encoding="utf-8",errors="replace").read()
lines=t.split(chr(10))
neg=[(i+1,l.strip()[:110]) for i,l in enumerate(lines) if re.search(r"assertFalse|assertEqual\(errs|self\.assert.*errs|bad|invalid|不合法|违规|reject", l, re.I)]
print("tests/monitoring/test_log_contract.py negative-style assertion lines:",len(neg))
for x in neg[:14]: print("   ",x)
print()
print("def test_ names:",len(re.findall(r"def test_",t)))
print("names containing 'reject|invalid|bad|fail':",[m for m in re.findall(r"def (test_\w+)",t) if re.search(r"reject|invalid|bad|fail|missing",m,re.I)][:14])
