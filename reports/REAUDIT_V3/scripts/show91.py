import re
p = "/tmp/control_doc/AstroCS_MAIN_AUDIT_SUPPLEMENT_CONTROL_V2_20260826.md"
txt = open(p, encoding="utf-8").read()
for pat in [r"#+\s*9\.1.*?(?=#+\s*9\.2)", r"#+\s*11\..*?(?=#+\s*12\.)"]:
    m = re.search(pat, txt, re.S)
    print("="*60)
    print(m.group(0)[:2500] if m else "NOT FOUND")
