# -*- coding: utf-8 -*-
# 交付物 B 自审脚本：字数、围栏、指针、seq/sha 对账
import json, re, sys, os

path = "大报告_历代控制包.md"
t = open(path, encoding="utf-8").read()

def is_han(c):
    o = ord(c)
    return 0x4E00 <= o <= 0x9FFF

def is_han_punct(c):
    o = ord(c)
    return (0x4E00 <= o <= 0x9FFF) or (0x3000 <= o <= 0x303F) or (0xFF00 <= o <= 0xFFEF) or o in (0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026)

han = sum(1 for c in t if is_han(c))
hab = sum(1 for c in t if is_han_punct(c))
print("bytes", len(t.encode()))
print("han", han)
print("han+punct", hab)
print("fences", t.count(chr(96)*3))
print("backticks", t.count(chr(96)))
print("lines", t.count(chr(10)))

# per-section
secs = []
idxs = [m for m in re.finditer(r"^## [0-9]+ ", t, re.M)]
for i, m in enumerate(idxs):
    end = idxs[i+1].start() if i+1 < len(idxs) else len(t)
    seg = t[m.start():end]
    secs.append((m.group(0).strip(), sum(1 for c in seg if is_han(c))))
for name, h in secs:
    print("SEC", h, name[:46])

# pointers
p_pg = re.findall(r"P-G\d\d[^，。）)]{0,14}", t)
p_cseq = re.findall(r"C([0-9]{1,4})｜([0-9a-f]{8})", t)
p_hex8 = re.findall(r"\b([0-9a-f]{8})\b", t)
p_sec = re.findall(r"§[一二三四五六七八九十0-9.]+", t)
p_kou = re.findall(r"缺口 \d+(?:/\d+)?", t)
p_file = re.findall(r"[\w/_.\-]+\.(?:md|csv|json|jsonl|py|jsonc|txt|yaml|yml|js|sh|h|cpp)", t)
print("PTR P-G", len(p_pg), "| C-seq|sha8", len(p_cseq), "| hex8", len(p_hex8), "| §", len(p_sec), "| 缺口n", len(p_kou), "| file paths", len(p_file))

# seq<->sha audit against index.jsonl
seq2sha = {}
sha2seq = {}
for line in open("_evidence/commits/index.jsonl", encoding="utf-8"):
    d = json.loads(line)
    s8 = d["sha"][:8]
    seq2sha[d["seq"]] = s8
    sha2seq.setdefault(s8, []).append(d["seq"])
bad = []
for seq, sha in p_cseq:
    s = int(seq)
    if s not in seq2sha:
        bad.append(("no-seq", seq, sha))
    elif seq2sha[s] != sha:
        bad.append(("mismatch", seq, sha, "expect " + seq2sha[s]))
print("C-anchor checked", len(p_cseq), "anomalies", len(bad))
for b in bad[:20]:
    print("  BAD", b)

# hex tokens not resolvable at all (informational; zip/blob hashes are expected)
unres = sorted(set(h for h in p_hex8 if h not in sha2seq))
print("hex8 tokens not in index.jsonl:", len(unres))
print("  ", " ".join(unres))

# single-line quotes length check
qs = re.findall(r"「([^」]{1,200})」", t)
too = [q for q in qs if len(q) > 40]
print("quoted spans", len(qs), "over40", len(too))
for q in too[:10]:
    print("  LONG", len(q), q[:60])
# tables
print("table rows", len([l for l in t.split(chr(10)) if l.strip().startswith("|")]))
