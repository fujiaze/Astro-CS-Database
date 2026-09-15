import os, re, json, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
print("=== build/root-cmake 生产者/消费者 ===")
o = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-n","root-cmake"], capture_output=True, text=True).stdout.splitlines()
for l in o:
    if l.startswith(("问题扫描/","evidence/","engineering/","设计大纲/","reports/","run/","docs/archive/")): continue
    print("   ", l[:150])
print()
print("=== 各 CI 构建树路径的字面量统计（谁产谁消） ===")
paths = ["build/linux-control", "build/root-cmake", "run/ci/build-gcc-release", "build/astrocs", "build/cli", "build/linux-openmp-on", "build/lnx_v5_clean_rel", "build/linux-gcc-release", "run/ci/win-candidate", "artifacts/candidate"]
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]
for p in paths:
    prod = []
    cons = []
    for c in checks:
        j = json.dumps(c, ensure_ascii=False)
        if p in j:
            (cons if c["id"].startswith(("CTEST","UT-","WIN-T","WIN-P","DEEP")) else prod).append(c["id"])
    wfin = subprocess.run(["git","--no-optional-locks","grep","-c",p,"--",".github","ci/steps","CMakePresets.json"], capture_output=True, text=True).stdout.strip().replace(chr(10)," ")
    print("   %-28s 门(生产侧命名)=%d 门(消费)=%d | steps/presets: %s" % (p, len(prod), len(cons), wfin or "-"))