import io, sys, re, os
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")

EXT = r"(?:md|py|yaml|yml|csv|json|txt)"
T = re.compile(r"^\s*\|\s*`?([^\s|`]+?\." + EXT + r")`?\s*\|")
H = re.compile(r"^#{2,6}[^\n]*?([^\s|`]+?\." + EXT + r")")

line = "| docs/design/LOG_AND_ERROR_SYSTEM.md | 日志与错误系统（详细设计） | 207 | 架构设计 |"
print("TEST1", T.match(line))
print("TEST2", T.match(line).group(1) if T.match(line) else None)

RAW = r"独立审计/证据"
text = open(os.path.join(RAW, "AUD-101-DB-10.md"), encoding="utf-8", errors="replace").read().splitlines()
toks = []
for i, l in enumerate(text):
    m = T.match(l) or H.match(l)
    if m:
        toks.append((i, m.group(1)))
print("tokens found:", len(toks))
for i, t in toks[:6]:
    print("  line", i, "->", repr(t))
    print("   raw:", repr(text[i][:90]))
