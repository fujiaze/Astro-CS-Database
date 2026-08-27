import re
p = "/tmp/control_doc/AstroCS_MAIN_AUDIT_SUPPLEMENT_CONTROL_V2_20260826.md"
txt = open(p, encoding="utf-8").read()
# print the 9.2 section
m = re.search(r"#+\s*9\.2.*?(?=#+\s*9\.3|#+\s*10\.)", txt, re.S)
if not m:
    m = re.search(r"9\.2.*?(?=##|\n# )", txt, re.S)
print(m.group(0) if m else "NOT FOUND")
