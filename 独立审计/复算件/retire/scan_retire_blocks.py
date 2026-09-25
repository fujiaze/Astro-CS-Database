# -*- coding: utf-8 -*-
"""AUD-404 复算 1：退役标注普查（统一注释块 6-token 合规 + R2 横幅候选）。

只读仓库文本，不运行 eng/** 脚本。判据口径取自
eng/ci/check_retired_code.py 文件头（唯一格式定义），本脚本自行独立实现。
"""
import json
import re
import subprocess
import sys
from pathlib import PurePosixPath

sys.stdout.reconfigure(encoding="utf-8")

REPO = r"F:\Astro dev\Astro CS Normalization Database"
MARKER = "RETIRED-CODE-RETAINED"
FIELDS = ("WHAT:", "WHY-KEPT:", "STATUS:", "EXIT:", "AUTHORITY:")
TOKENS = (MARKER,) + FIELDS
MIN_FIELD_CHARS = 2
SRC = (".c", ".cpp", ".cc", ".cxx", ".h", ".hpp", ".py")


def tracked():
    out = subprocess.run(
        ["git", "-C", REPO, "-c", "core.quotepath=false", "ls-files"],
        capture_output=True)
    return [l.decode("utf-8") for l in out.stdout.split(b"\n") if l]


def read(rel):
    with open(REPO + "/" + rel, "rb") as f:
        return f.read().decode("utf-8", "replace")


def comment_lines(text):
    """(lineno, is_comment, body) 逐行。"""
    rows = []
    for i, l in enumerate(text.split("\n"), 1):
        m = re.match(r"^\s*(?://+|#+|/\*+|\*|--|!)\s*(.*)$", l)
        rows.append((i, bool(m), m.group(1) if m else l))
    return rows


def blocks(text):
    """把连续的注释行聚成注释块，返回 (start_line, end_line, joined_text)。"""
    rows = comment_lines(text)
    out = []
    cur = []
    for lineno, is_c, body in rows:
        if is_c:
            cur.append((lineno, body))
        else:
            if cur:
                out.append((cur[0][0], cur[-1][0], "\n".join(b for _, b in cur)))
            cur = []
    if cur:
        out.append((cur[0][0], cur[-1][0], "\n".join(b for _, b in cur)))
    return out


BANNER = ("DEPRECATED", "RETIRED", "LEGACY", "OBSOLETE", "SUPERSEDED",
          "已退役", "已作废", "遗留实现", "未接入生产", "死代码")
BANNER_RE = re.compile("|".join(re.escape(t) for t in BANNER))
DECOR = re.compile(r"^[\s\*⚠!\-–—>#/|]+")
LABEL_RE = re.compile(
    r"^(?:状态\s*[:：]\s*)?[\s\*⚠!\-–—>#/|]*(?:%s)(?![0-9A-Za-z_])"
    % "|".join(re.escape(t) for t in BANNER))


def field_empty(block_text, tok):
    m = re.search(re.escape(tok) + r"[ \t]*(.*)", block_text)
    if not m:
        return True
    v = m.group(1).strip()
    return len(re.sub(r"^[\s\-—:：]+", "", v)) < MIN_FIELD_CHARS


def main():
    files = [f for f in tracked() if PurePosixPath(f).suffix in SRC]
    files = [f for f in files if "third_party/" not in f]
    retained = []
    banners = []
    for f in files:
        text = read(f)
        bs = blocks(text)
        has_retained = any(MARKER in b for _, _, b in bs)
        for start, end, b in bs:
            if MARKER not in b:
                continue
            missing = [t for t in TOKENS if t not in b]
            empty = [t for t in FIELDS if t in b and field_empty(b, t)]
            retained.append({
                "file": f, "block_start": start, "block_end": end,
                "missing_tokens": missing, "empty_fields": empty,
                "compliant": not missing and not empty,
            })
        # R2 近似：前 40 行注释行上的横幅标签，且该文件无 RETAINED 块
        rows = comment_lines(text)
        for lineno, is_c, body in rows[:40]:
            if not is_c:
                continue
            if LABEL_RE.match(body) or (BANNER_RE.search(body)
                                        and DECOR.sub("", body).startswith(BANNER)):
                toks = BANNER_RE.findall(DECOR.sub("", body))
                if toks and not has_retained:
                    banners.append({"file": f, "line": lineno,
                                    "tokens": sorted(set(toks)),
                                    "text": body[:150]})
                break
    print("扫描翻译单元/头文件（跟踪面，排除 third_party）:", len(files))
    print("RETIRED-CODE-RETAINED 注释块:", len(retained))
    print("  合规:", sum(1 for r in retained if r["compliant"]))
    print("  不合规:", sum(1 for r in retained if not r["compliant"]))
    print("前40行横幅且无 RETAINED 块（R2 候选）:", len(banners))
    OUT = r"独立审计/复算件/retire\retire_scan.json"
    json.dump({"retained": retained, "banners": banners},
              open(OUT, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print("\n--- 不合规块 ---")
    for r in retained:
        if not r["compliant"]:
            print(f"{r['file']}:{r['block_start']}-{r['block_end']} "
                  f"missing={r['missing_tokens']} empty={r['empty_fields']}")
    print("\n--- 合规块 ---")
    for r in retained:
        if r["compliant"]:
            print(f"{r['file']}:{r['block_start']}")
    print("\n--- R2 候选（横幅无块） ---")
    for b in banners:
        print(f"{b['file']}:{b['line']} {b['tokens']} :: {b['text']}")


main()
