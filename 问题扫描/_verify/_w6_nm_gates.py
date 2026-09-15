import os, re, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
for f in ["tools/check_legacy_exit.py","tools/quality/check_isa_leak.py","tools/quality/check_prod_reachability.py","tools/check_duplication.py","tools/check_baseline_opcodes.py"]:
    txt = open(f, encoding="utf-8", errors="replace").read()
    print("="*100)
    print("FILE", f, "lines:", txt.count(chr(10))+1)
    for n, line in enumerate(txt.splitlines(), 1):
        if re.search(r"nm\b|objdump|shutil.which|return 0|sys.exit|PASS|SKIP|skip|FileNotFoundError|not os.path.exists|exists\(\)", line):
            print("   %4d %s" % (n, line[:130]))