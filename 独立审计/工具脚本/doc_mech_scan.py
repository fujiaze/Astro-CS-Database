"""AUD-101 机械列扫描：只读，逐份文档抽可确定性判定的写法指标。

判据来源：AUDIT-06 standards/01 §2 §3 §4。
本脚本只出"线索与覆盖率"，不出"已审"结论——语义判定（角色、正本、重复）由代理通读给出。
"""

import argparse
import csv
import os
import re
import sys
from pathlib import Path

ENC = "utf-8"  # 显式：Windows 默认 cp936 会把 UTF-8 中文解坏
for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding=ENC, errors="replace")
    except (AttributeError, ValueError):
        pass

DATE_PAT = re.compile(r"\d{4}-\d{2}-\d{2}|\d{4}\.\d{1,2}\.\d{1,2}|\d{4}年\s*\d{1,2}\s*月\s*\d{1,2}\s*日")
TASKID_PAT = re.compile(r"\b(?:EXP|RELEASE|FIX|AUDIT|AUD|GOV|CTL|REL|P|S)-\d{1,4}\b|\b[ERFA]-\d{2,3}\b")
SHA_PAT = re.compile(r"\b(?:[0-9a-f]{40}|(?<![0-9a-f])[0-9a-f]{7,8}(?![0-9a-fg-z]))\b")
SHA256_PAT = re.compile(r"\b[0-9a-f]{64}\b|sha256", re.I)
HIST_PAT = re.compile(r"旧版|作废|废止|留痕|已撤销|曾经|原先|上一版|前一轮|历史遗留|原口径|曾规定|退回至|降级为旧")
META_PAT = re.compile(r"^\s*(文档\s*ID|文档编号|状态[：:]|版本锚|纪律声明|报告性质|适用范围[：:]|编号[：:]|meta[：:])", re.I | re.M)
UPSTREAM_PAT = re.compile(r"ASTROCS_DESIGN|最高设计|上游")
MDLINK_PAT = re.compile(r"\[[^\]]*\]\(([^)\s]+)")
BAREPATH_PAT = re.compile(r"`((?:docs|lib|eng|实验|工程控制|artifacts|testdata|reports)/[^`\s]*\.(?:md|json|yaml|yml|csv|cpp|h|hpp|py|sh|txt|fits|png|svg|tex))`")
TEMP_PAT = re.compile(r"(^|/)run/")

MECHANICAL = ["dates", "task_ids", "commit_sha", "sha256", "history_narrative", "meta_block",
              "dangling_links", "unregistered_index", "no_upstream_pointer"]


def scan_file(repo: Path, rel: str):
    p = repo / rel
    try:
        text = p.read_text(encoding=ENC, errors="replace")
    except OSError as e:
        return {"path": rel, "unreadable": str(e)}
    lines = text.splitlines()
    head = "\n".join(lines[:40])
    fm = (bool(lines) and lines[0].strip() == "---"
          and any(re.match(r"^[A-Za-z_][\w-]*\s*:", l) for l in lines[1:20]))
    # 引用块式抬头（`> 文档 ID：…` / `> 状态：FROZEN`）也是元信息块，前两版都漏判
    bq = sum(1 for l in lines[:12] if l.startswith(">") and re.search(r"(ID|编号|状态|版本|性质|锚|声明|范围)", l)) >= 2
    dangling = []
    for tgt in MDLINK_PAT.findall(text) + BAREPATH_PAT.findall(text):
        if tgt.startswith(("http://", "https://", "mailto:", "#", "/")):
            continue
        if TEMP_PAT.search(tgt):
            continue
        t = tgt.split("#")[0]
        if not t or any(c in t for c in "*?"):
            continue  # 通配/纯片段引用不是仓库路径，判悬空即误报
        cand = [(repo / t).resolve(), (p.parent / t).resolve()]
        if not any(c.exists() for c in cand):
            dangling.append(tgt)
    return {
        "path": rel,
        "lines": len(lines),
        "h1": next((l[2:].strip() for l in lines[:20] if l.startswith("# ")), ""),
        "dates": [l + 1 for l, x in enumerate(lines) if DATE_PAT.search(x)][:20],
        "task_ids": sorted(set(TASKID_PAT.findall(text)))[:20],
        "commit_sha": sorted(t for t in set(SHA_PAT.findall("\n".join(lines)))
                            if sum(c.isalpha() for c in t) >= 2)[:10],
        "sha256": bool(SHA256_PAT.search(text)),
        "history_narrative": [l + 1 for l, x in enumerate(lines) if HIST_PAT.search(x)][:20],
        "meta_block": bool(META_PAT.search(head)) or fm or bq,
        "frontmatter": fm,
        "blockquote_meta": bq,
        "upstream_pointer": bool(UPSTREAM_PAT.search(head)),
        "dangling_links": sorted(set(dangling))[:20],
    }


def self_test(tmp: Path) -> int:
    """注入正负例：每个检测器必须在负例上判红、在正例上判绿，否则脚本自身无判别力。"""
    bad = tmp / "BAD.md"
    bad.write_text(
        "---\nid: X-1\nstatus: draft\n---\n\n# 坏文档\n\n文档 ID：X-1\n\n"
        "纪律声明：本文只读。\n\n"
        "2026-01-02 负责人裁决 FIX-7 曾规定旧版作废，留痕。\n\n"
        "见 `docs/NOPE_missing.md` 与 [链接](docs/ALSO_missing.md) 与 [上跳](../NOWHERE.md)\n\n"
        "sha abcdef0 sha256 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n\n"
        "DOI 1996164 与 seed 20260921 不是 commit。\n",
        encoding=ENC)
    good = tmp / "GOOD.md"
    good.write_text(
        "# 好文档\n\n上游：ASTROCS_DESIGN.md §2。\n\n本文描述现行设计要怎样。\n\n"
        "真实链接 [在册](docs/REAL.md) 与 `docs/REAL.md` 都不该判悬空。\n\n"
        "通配引用 `NOWHERE/*.md` 不是仓库路径，判悬空即误报。\n",
        encoding=ENC)
    (tmp / "docs").mkdir(exist_ok=True)
    (tmp / "docs" / "REAL.md").write_text("# real\n", encoding=ENC)
    exp = {
        "dates": lambda r: r["dates"], "task_ids": lambda r: r["task_ids"],
        "commit_sha": lambda r: r["commit_sha"], "sha256": lambda r: r["sha256"],
        "history_narrative": lambda r: r["history_narrative"], "meta_block": lambda r: r["meta_block"],
        "dangling_links": lambda r: r["dangling_links"],
    }
    upstream = lambda r: r["upstream_pointer"]
    rb, rg = scan_file(tmp, "BAD.md"), scan_file(tmp, "GOOD.md")
    fails = []
    for name, fn in exp.items():
        if not fn(rb):
            fails.append(f"负例未触发 {name}")
        if fn(rg):
            fails.append(f"正例误报 {name}")
    if upstream(rb):
        fails.append("正例判定被污染：坏文档不该有上游指针")
    if not upstream(rg):
        fails.append("负例未触发 upstream_pointer（好文档应有上游指针）")
    if rg["dangling_links"]:
        fails.append(f"好文档链接被误判悬空: {rg['dangling_links']}")
    if not rb["frontmatter"]:
        fails.append("负例未触发 frontmatter（YAML 头整列漏报即 fail-open）")
    if rg["frontmatter"]:
        fails.append("正例误报 frontmatter")
    if not any(".." in d for d in rb["dangling_links"]):
        fails.append("上跳相对链接未被判定（`..` 开头目标被跳过）")
    if "1996164" in rb["commit_sha"] or "20260921" in rb["commit_sha"]:
        fails.append("纯数字标识（DOI 号/seed）被误判为 commit sha")
    (tmp / "BQ.md").write_text("# 抬头文档\n\n> 文档 ID：DOC-9\n> 状态：FROZEN\n> 版本锚：略\n\n正文。\n", encoding=ENC)
    if not scan_file(tmp, "BQ.md")["blockquote_meta"]:
        fails.append("引用块式元信息块漏判（实验与合同层常见载体）")
    long_lines = [f" filler line {i}" for i in range(1, 101)]
    long_lines[54] = "依据 AUDIT-9 的现行口径"
    (tmp / "LONG.md").write_text("\n".join(long_lines), encoding=ENC)
    rl = scan_file(tmp, "LONG.md")
    if "AUDIT-9" not in rl["task_ids"]:
        fails.append("正文中段的任务编号漏报（旧口径只扫抬头 40 行与尾 30 行）")
    if any("NOWHERE/*.md" in d for d in rg["dangling_links"]):
        fails.append("通配引用被误判为悬空链接")
    if fails:
        for f in fails:
            print(f"SELF-TEST FAIL: {f}")
        return 1
    print(f"SELF-TEST OK ({len(exp) + 1} 检测器正负例全过)")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo")
    ap.add_argument("--list", help="每行一个仓库相对路径的清单文件")
    ap.add_argument("--index", default="docs/DOCUMENT_INDEX.yaml")
    ap.add_argument("--out")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()

    if a.self_test:
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            return self_test(Path(d))
    if not (a.repo and a.list and a.out):
        print("FAIL-CLOSED: --repo --list --out 必需（--self-test 除外）")
        return 2

    repo = Path(a.repo).resolve()
    if not (repo / ".git").exists():
        print(f"FAIL-CLOSED: {repo} 不是仓库根")
        return 2
    lp = Path(a.list)
    if not lp.is_file():
        print(f"FAIL-CLOSED: 输入清单不存在 {lp}")
        return 2
    rels = [x.strip().replace("\\", "/") for x in lp.read_text(encoding=ENC).splitlines() if x.strip()]
    if not rels:
        print("FAIL-CLOSED: 输入清单为空（扫描对象 0 个不判绿）")
        return 2
    missing = [r for r in rels if not (repo / r).is_file()]
    if missing:
        print(f"FAIL-CLOSED: 清单中 {len(missing)} 个文件在树里不存在，例: {missing[:5]}")
        return 2
    idx = ""
    ip = repo / a.index
    if ip.is_file():
        idx = ip.read_text(encoding=ENC, errors="replace")
    else:
        print(f"WARN: 索引文件不存在 {a.index}，unregistered 列不判")

    rows = []
    for r in rels:
        rec = scan_file(repo, r)
        # DOCUMENT_INDEX.yaml 是 docs/ 的唯一索引地图；对 docs 外的文件判"未登记"不成立（口径）
        rec["unregistered_index"] = ("" if not r.startswith("docs/")
                                     else (bool(idx) and (r not in idx)))
        rows.append(rec)

    out = Path(a.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    cols = ["path", "lines", "h1", "meta_block", "frontmatter", "blockquote_meta",
            "upstream_pointer", "dates", "task_ids",
            "commit_sha", "sha256", "history_narrative", "dangling_links", "unregistered_index", "unreadable"]
    with out.open("w", encoding=ENC, newline="") as fh:
        w = csv.writer(fh)
        w.writerow(cols)
        for rec in rows:
            w.writerow([rec.get(c, "") for c in cols])

    def cnt(key, truthy=False):
        if truthy:
            return sum(1 for x in rows if x.get(key))
        return sum(len(x.get(key) or []) for x in rows if isinstance(x.get(key), list))

    print(f"扫描对象 {len(rows)} 份 → {out}")
    for k in MECHANICAL:
        tb = k in ("meta_block", "unregistered_index", "no_upstream_pointer")
        print(f"  {k:22s} {cnt(k, tb)}")
    print(f"  upstream_missing       {sum(1 for x in rows if not x.get('upstream_pointer'))}")
    print(f"  unreadable             {sum(1 for x in rows if x.get('unreadable'))}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
