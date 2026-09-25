import io
import re
import sys

# Replace ASCII double quotes that appear INSIDE a Chinese text cell with 『』,
# so the Python source stays parseable.  Strategy: for each line, keep the first
# and last quote (the string delimiters) and convert any interior pair.
p = sys.argv[1]
s = io.open(p, encoding="utf-8").read()
fixed = []
n = 0
for line in s.split("\n"):
    if line.count('"') > 2 and '="' not in line:
        parts = line.split('"')
        # interior segments are those not at index 0 / last, at even positions
        head = parts[0] + '"'
        tail = parts[-2] + '"' + parts[-1]
        mid = '"'.join(parts[1:-2])
        if mid.count('"') >= 2:
            mid = re.sub(r'"([^"]*)"', lambda m: "\u300c" + m.group(1) + "\u300d", mid)
            n += mid.count("\u300c")
            line = head + mid + tail
    fixed.append(line)
io.open(p, "w", encoding="utf-8").write("\n".join(fixed))
print("converted interior quote pairs:", n)
