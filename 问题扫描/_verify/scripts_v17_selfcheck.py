import os, re
ROOT = "/workspace/Astro CS Database"
files = []
for dp, dn, fn in os.walk(ROOT):
    if any(x in dp for x in (".git", "run/", "问题扫描", "astrocs_p1sess_", "third_party", "archive", "__pycache__")):
        continue
    for f in fn:
        if "selfcheck" in f and (f.endswith(".cpp")):
            files.append(os.path.relpath(os.path.join(dp, f), ROOT))
print("selfcheck TUs:", len(files))
for p in sorted(files):
    text = open(os.path.join(ROOT, p), encoding="utf-8", errors="replace").read()
    m_fork = re.search(r"if \(pid < 0\)\s*\{[^}]*return (\d+|1);", text)
    fork_rc = re.findall(r"if \(pid < 0\)[\s\S]{0,80}?return (\w+);", text)
    n_zero_test = len(re.findall(r"child_rc == 0", text)) + len(re.findall(r"rc == 0\)", text))
    has_wif = "WIFEXITED" in text
    print(f"  {p}: fork_fail_return={fork_rc[:3]} child_rc==0-gates={n_zero_test} WIFEXITED={has_wif}")
print()
print("== ctest selfcheck registrations ==")
import subprocess
for dp, dn, fn in os.walk(ROOT):
    pass