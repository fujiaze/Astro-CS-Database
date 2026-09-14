
import os,re,json
ROOT="/workspace/Astro CS Database"
t=open(ROOT+"/tools/quality/contracts/check_full_integration.py",encoding="utf-8",errors="replace").read().split(chr(10))
print("--- tail 83-88 ---")
for k in range(82,min(88,len(t))): print("%4d: %s"%(k+1,t[k].rstrip()[:150]))
print()
cj=json.load(open(ROOT+"/ci/checks.json",encoding="utf-8"))["checks"]
us=[c["id"] for c in cj if "check_full_integration" in json.dumps(c.get("command",[]))]
print("check_full_integration CI entries:",us)
for c in cj:
    if "check_full_integration" in json.dumps(c.get("command",[])): print("   ",json.dumps(c,ensure_ascii=False)[:330])
print()
print("--- p2hips selfcheck CMake comment (22-24) ---")
cm=open(ROOT+"/lib/astro_image_io/tests/p2hips/CMakeLists.txt",encoding="utf-8",errors="replace").read().split(chr(10))
for k in range(18,28): print("%4d: %s"%(k+1,cm[k].rstrip()[:150]))
