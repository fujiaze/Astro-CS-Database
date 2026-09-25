# -*- coding: utf-8 -*-
# CR-40 成稿分节重排：把逐段落盘时因标记歧义而错位的章节按 1..6 -> 10..15 顺序重排。
# 只做纯文本块搬移，零内容改写。
import re
import sys

sys.stdout.reconfigure(encoding="utf-8")

PATH = r"独立审计/证据/通读-CR-40.md"

with open(PATH, "r", encoding="utf-8") as fh:
    text = fh.read()

# (1) 去掉遗留的写入标记与骨架占位（§15 已正式承载收工 status）
text = text.replace("<!-- APPEND-HERE -->\n", "")
text = text.replace("<!-- APPEND-HERE-2 -->\n", "")
text = re.sub(r"## 收工 git status --porcelain 原文\n+```\n（待填）\n```\n?", "", text)

# (2) 摘出骨架里的 "## 本批重点" 节（它被粘在最后一个编号块尾部）
m = re.search(r"(?ms)^## 本批重点.*?(?=^# \d+\. |\Z)", text)
focus = m.group(0).rstrip() + "\n" if m else ""
if m:
    text = text[: m.start()] + text[m.end():]

# (3) 只按 "# <数字>." 一级标题切块，文档头（标题/清单/标记）整体留在 head
parts = re.split(r"(?m)^(# \d+\. .*)$", text)
head, blocks, i = parts[0], {}, 1
while i < len(parts):
    title = parts[i].strip()
    body = parts[i + 1] if i + 1 < len(parts) else ""
    blocks[int(re.match(r"^#\s+(\d+)\.", title).group(1))] = title + "\n" + body
    i += 2

# (4) 从各块体内剔除散落的进度标记，只保留清单下的那一条
for k in list(blocks):
    blocks[k] = re.sub(r"(?m)^<!-- PROGRESS: \d+/6 -->\n\n?", "", blocks[k])
head = re.sub(r"<!-- PROGRESS: \d+/6 -->", "<!-- PROGRESS: 6/6 -->", head)

order = [1, 2, 3, 4, 5, 6, 10, 11, 12, 13, 14, 15]
missing = [k for k in order if k not in blocks]
if missing:
    raise SystemExit("missing blocks: %r ; keys=%r" % (missing, sorted(blocks)))
extra = sorted(set(blocks) - set(order))
if extra:
    raise SystemExit("unexpected blocks: %r" % extra)

out = head.rstrip() + "\n\n" + focus + "\n"
for k in order:
    out += blocks[k].rstrip() + "\n\n"
out = out.rstrip() + "\n"

with open(PATH, "w", encoding="utf-8", newline="\n") as fh:
    fh.write(out)

print("reordered ok:", order)
for line in out.splitlines():
    if line.startswith("# ") or line.startswith("## ") or line.startswith("<!-- PROGRESS"):
        print("   ", line)
