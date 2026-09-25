#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUDIT-06 第②层复算 R1：独立复刻 check_doc_hygiene.namespace_prefixes 的前缀表来源。
只读仓库；不执行仓库脚本。三种情形：本机工作树 / 干净克隆（仅跟踪集） / 删掉最后一个控制包。
"""
import os
import re
import subprocess
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = r"F:\Astro dev\Astro CS Normalization Database"


def prefixes_from_dirs(names_by_base):
    """与 check_doc_hygiene.py:390-403 同一判据：目录名 -> head -> 正则过滤。"""
    out = set()
    for base, names in names_by_base.items():
        for name in names:
            if name.startswith("."):
                continue
            head = re.split(r"[-_]", name)[0]
            if re.match(r"^[A-Z][A-Za-z0-9]*$", head) and len(head) >= 2:
                out.add(head)
    return out


def dirs_on_disk(base):
    d = os.path.join(REPO, base)
    if not os.path.isdir(d):
        return None  # 情形：目录不存在 => 门里 continue
    return [n for n in os.listdir(d) if os.path.isdir(os.path.join(d, n))]


def dirs_tracked_only(base):
    """干净克隆上该 base 下的子目录 = 含至少一个跟踪文件的二级目录名。"""
    r = subprocess.run(["git", "-C", REPO, "ls-files", "--", base],
                       capture_output=True, text=True, encoding="utf-8")
    names = set()
    nfiles = 0
    for ln in r.stdout.splitlines():
        nfiles += 1
        parts = ln.split("/")
        if len(parts) >= 3 and parts[1]:
            names.add(parts[1])
    return sorted(names), nfiles


print("== A. 本机工作树（含 gitignore 残留） ==")
disk = {b: dirs_on_disk(b) for b in ("run", "工程控制")}
for b, v in disk.items():
    print("  %s -> %s" % (b, v))
print("  prefixes = %s" % sorted(prefixes_from_dirs({k: (v or []) for k, v in disk.items()})))

print("== B. 干净克隆（只有跟踪集） ==")
tr = {}
for b in ("run", "工程控制"):
    names, n = dirs_tracked_only(b)
    tr[b] = names
    print("  %s: 跟踪文件 %d 个，含跟踪文件的子目录 = %s" % (b, n, names))
pb = prefixes_from_dirs(tr)
print("  prefixes = %s  (count=%d)" % (sorted(pb), len(pb)))

print("== C. 干净克隆 + 删掉最后一个控制包（工程控制/RELEASE-05 移除） ==")
tr_c = {"run": tr["run"], "工程控制": [n for n in tr["工程控制"] if n != "RELEASE-05"]}
pc = prefixes_from_dirs(tr_c)
print("  run/ 目录仍在？%s（其唯一条目 .gitkeep 以 '.' 开头，line 398 被跳过）"
      % os.path.isdir(os.path.join(REPO, "run")))
print("  prefixes = %s  (count=%d)  => line 648 `if not prefixes_now` 命中 ⇒ 判红"
      % (sorted(pc), len(pc)))

print("== D. 前缀表的判别力（D3B_ID_RE 分组 1 命中面） ==")
D3B = re.compile(r"\b([A-Za-z][A-Za-z0-9]*)((?:-[A-Za-z0-9]+)+)-(\d{1,3})\b")
samples = ["AUDIT-06", "RELEASE-05", "DOC-HYGIENE-01", "GATE-TRIAGE-01", "P5-SNR", "SCI-503",
           "MEM-WIRE-01", "PROJECT-GOVERNANCE-01", "工包-AUDIT-06"]
for s in samples:
    m = D3B.search(s)
    if not m:
        print("  %-24s 形态不匹配 D3B_ID_RE" % s)
        continue
    print("  %-24s head=%-10s 干净克隆命中=%s 本机命中=%s"
          % (s, m.group(1), m.group(1) in pb, m.group(1) in prefixes_from_dirs(disk)))
