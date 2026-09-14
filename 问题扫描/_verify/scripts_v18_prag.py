
import os,re
ROOT="/workspace/Astro CS Database"
txt=open(ROOT+"/tools/quality/check_prod_reachability.py",encoding="utf-8",errors="replace").read()
lines=txt.split(chr(10))
print("=== all def names ===")
for i,l in enumerate(lines):
    if re.match(r"\s*def ", l): print("  :%d %s"%(i+1,l.strip()[:110]))
print()
print("=== where are BANNED_CLI_INCLUDES / BANNED_CLI_SYMBOLS used? ===")
for i,l in enumerate(lines):
    if "BANNED_CLI" in l: print("  :%d %s"%(i+1,l.strip()[:140]))
print()
print("=== main() body 1..? ===")
mi=[i for i,l in enumerate(lines) if re.match(r"\s*def main", l)][0]
for k in range(mi, min(mi+42,len(lines))): print("%4d: %s"%(k+1,lines[k].rstrip()[:140]))
