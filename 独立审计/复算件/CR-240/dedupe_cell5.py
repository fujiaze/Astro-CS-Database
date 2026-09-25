import io, sys
sys.stdout.reconfigure(encoding='utf-8')

path = r"独立审计/证据/通读-CR-240.md"
s = io.open(path, encoding="utf-8").read()

marker = "# 格 5 · `module_adapters.cpp:1201-1500`"
first = s.find(marker)
second = s.find(marker, first + 1)
assert first != -1 and second != -1, (first, second)

head = s[:first].rstrip("\n")
tail = s[second:]
new = head + "\n\n---\n\n" + tail
io.open(path, "w", encoding="utf-8").write(new)
print("removed stale first copy; occurrences now:", new.count(marker))
