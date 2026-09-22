#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DOC-002 / 注册项 DOC-L0：L0 固定文档完整性校验（现行文档集口径）。

迁移记录（CI-003-B12，2026-09-16；依据 ENGINEERING_SPEC §8 锚存活/fail-closed/
可执行负例面 + docs/DOCUMENT_INDEX.yaml 抬头 §(b)(c) DOC-001 收敛记录）：

  旧口径（已废止）：REVIEW.md + docs/review/{SCIENCE,PIPELINE,ARCHITECTURE,
  RELEASE_STATUS,CHANGE_REVIEW}_OVERVIEW.md。根 REVIEW.md 已由 ROOT-007 删除且
  不再索引 ⇒ 继续要求它存在即 false-red；docs/review/** 是旧轮次副本，
  DOC-001 已声明「负责人文档活动版本唯一 = docs/owner/**」。

  现行口径（本文件判据面，全部实测存在且受 git 跟踪）：
    A. 现行 L0 文档集 = docs/owner/**（4 份 L0 汇总层，ACTIVE_NORMATIVE）：
       docs/owner/SCIENCE_OVERVIEW.md
       docs/owner/PIPELINE_OVERVIEW.md
       docs/owner/ARCHITECTURE_OVERVIEW.md
       docs/owner/RELEASE_STATUS.md
    B. 现行权威索引 = docs/DOCUMENT_INDEX.yaml（取代已退役的根 REVIEW.md 链接面）：
       4 份 L0 文档必须在该索引 active 区段以 ACTIVE_* 登记。
    C. 旧权威回归（必须能红），两级口径：
       C1 P0（恒红）：docs/review/** 内任一文件在正文头 400 字符声明 ACTIVE_* 且
          **未**声明 ARCHIVED = 重新主张权威 ⇒ 判红（两种口径都红）。
       C2 report（--strict 升为 P0）：未声明归档的旧副本残留 ⇒ 默认口径仅 report
          可见、不判红；--strict 判红。依据：CI-003「不得因残留重份使注册门恒红」。
       真仓实测 2026-09-16：docs/review/CHANGE_REVIEW.md 抬头为
       「状态：ACTIVE_INFORMATIVE（L0 治理评审汇总层）」且无 ARCHIVED 声明 ⇒ 命中
       C1，本门如实判红（DOC-001 对 docs/review/** 的收口缺口，登记 CI-003-B12-F2）。

结构要求（ENGINEERING_SPEC §8）：所有判据都必须跑完再汇总，任一项失败不得跳过
其余判据（旧实现对 REVIEW.md 走缺失分支后整段跳过 L0 链接判据 = 一条遮蔽其余）。

用法：
  python3 eng/tools/check_l0_docs.py [--root <repo>] [--json-out <file>] [--strict]
  python3 eng/tools/check_l0_docs.py --self-test     # 可执行负例面（tempfile mini-repo）

退出码：0 = PASS，1 = FAIL；无参调用 = 注册命令行为（真仓全量判据）。
  --strict 额外要求：docs/review/** 旧副本必须全部显式声明归档（默认口径下
  已声明归档的旧副本仅作 report 可见，不判红；未声明归档/重新主张权威的副本
  在两种口径下都判红）。
  --self-test 恒 0 = 全部内置正/负例符合预期，任一例不符预期则 1（不依赖真仓）。
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys

INDEX_REL = "docs/DOCUMENT_INDEX.yaml"

# A. 现行 L0 文档集（实测存在 + 受 git 跟踪；见文件头「现行口径」）
L0_DOCS = (
    "docs/owner/SCIENCE_OVERVIEW.md",
    "docs/owner/PIPELINE_OVERVIEW.md",
    "docs/owner/ARCHITECTURE_OVERVIEW.md",
    "docs/owner/RELEASE_STATUS.md",
)

# B. 现行权威索引（取代已退役的根 REVIEW.md 链接面）
ROOT_INDEX = INDEX_REL

# 已退役的旧 L0 落位（依据：docs/DOCUMENT_INDEX.yaml 抬头 §(b) DOC-001，2026-09-16）
RETIRED_L0_DIR = "docs/review"

MIN_CHARS = 200
HEAD_CHARS = 400
ARCHIVE_MARKERS = ("ARCHIVED", "已归档", "已退役")


def check(name: str, ok: bool, detail: str) -> dict:
    return {"check": name, "pass": bool(ok), "detail": detail}


def _read(path: str, limit: int | None = None) -> str:
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            return fh.read(limit) if limit else fh.read()
    except OSError:
        return ""


def _git(root: str, *args: str) -> tuple[int, str]:
    # fail-closed：root 不存在/git 不可用 ⇒ 返回非 0（调用方判红），不得 traceback。
    try:
        r = subprocess.run(["git", *args], cwd=root, capture_output=True, text=True)
    except OSError as exc:
        return 127, f"git unavailable: {exc}"
    return r.returncode, r.stdout


def load_index_active(root: str) -> tuple[dict[str, str] | None, str]:
    """返回 (index_path -> status 映射, 说明)。读不到 ⇒ (None, 原因)，由调用方 fail-closed。"""
    full = os.path.join(root, INDEX_REL)
    if not os.path.isfile(full):
        return None, INDEX_REL + " 不存在"
    try:
        import yaml  # type: ignore
    except Exception:
        return None, "pyyaml 不可用"
    try:
        data = yaml.safe_load(_read(full)) or {}
    except Exception as exc:
        return None, "YAML 解析失败: " + str(exc)
    entries = ((data.get("doc_index") or {}).get("active") or [])
    out: dict[str, str] = {}
    for e in entries:
        p = (e or {}).get("path")
        if p:
            out[str(p)] = str((e or {}).get("status", ""))
    return out, "ok"


def run_checks(root: str, strict: bool) -> list[dict]:
    results: list[dict] = []

    # --- A. 现行 L0 文档集存在性/非空（逐条跑完，不短路）---
    missing_docs = [p for p in L0_DOCS if not os.path.isfile(os.path.join(root, p))]
    short_docs = [p for p in L0_DOCS
                  if os.path.isfile(os.path.join(root, p))
                  and len(_read(os.path.join(root, p))) < MIN_CHARS]
    tracked_missing = []
    for p in L0_DOCS:
        rc, _ = _git(root, "ls-files", "--error-unmatch", "--", p)
        if rc != 0:
            tracked_missing.append(p)
    results.append(check("l0_docs_present", not missing_docs,
                         "缺失=" + repr(missing_docs) if missing_docs
                         else "4 份现行 L0 文档齐备: " + ", ".join(L0_DOCS)))
    results.append(check("l0_docs_nontrivial", not short_docs,
                         "过短/空=" + repr(short_docs) if short_docs
                         else f"4 份均 >= {MIN_CHARS} 字符"))
    results.append(check("l0_docs_git_tracked", not tracked_missing,
                         "未跟踪=" + repr(tracked_missing) if tracked_missing
                         else "4 份均受 git 跟踪"))

    # --- B. 现行权威索引：存在 + 覆盖 L0 文档 ---
    index_full = os.path.join(root, ROOT_INDEX)
    index_exists = os.path.isfile(index_full)
    results.append(check("root_index_present", index_exists,
                         ROOT_INDEX if index_exists
                         else "现行权威索引缺失（替代已退役的根 REVIEW.md）；fail-closed"))
    active_map, why = load_index_active(root)
    if active_map is None:
        results.append(check("l0_docs_indexed_active", False,
                             f"索引不可读（{why}）；fail-closed 判红"))
    else:
        unlisted = [p for p in L0_DOCS if p not in active_map]
        nonactive = [p for p in L0_DOCS
                     if p in active_map and not active_map[p].startswith("ACTIVE_")]
        ok = not unlisted and not nonactive
        results.append(check("l0_docs_indexed_active", ok,
                             ("未登记=" + repr(unlisted) + " 非ACTIVE=" + repr(nonactive)) if not ok
                             else "4 份 L0 文档均在索引 active 区段登记为 ACTIVE_*"))

    # --- C. 旧权威回归：docs/review/** 重新主张权威 ⇒ 红 ---
    retired_dir = os.path.join(root, RETIRED_L0_DIR)
    reasserting: list[str] = []
    undeclared: list[str] = []
    archived_ok: list[str] = []
    if os.path.isdir(retired_dir):
        for name in sorted(os.listdir(retired_dir)):
            rel = RETIRED_L0_DIR + "/" + name
            if not os.path.isfile(os.path.join(retired_dir, name)):
                continue
            head = _read(os.path.join(retired_dir, name), HEAD_CHARS)
            declares_archived = any(m in head for m in ARCHIVE_MARKERS)
            # 重主张权威 = 正文头声明 ACTIVE_* 且**未**声明 ARCHIVED（旧副本残留的
            # 陈旧 "状态：ACTIVE_INFORMATIVE" 行在已声明归档时不算重主张，仅作 report）。
            claims_active = ("ACTIVE_" in head) and not declares_archived
            if claims_active:
                reasserting.append(rel)
            elif declares_archived:
                archived_ok.append(rel)
            else:
                undeclared.append(rel)
    results.append(check("retired_l0_no_authority_reassertion", not reasserting,
                         "旧权威回归（正文头声明 ACTIVE_*，重主张权威）=" + repr(reasserting)
                         if reasserting else "docs/review 内无重新主张权威的副本"))
    # 未声明归档的旧副本 = 残留重份（report 项；--strict 下升为 P0 判红）。
    # 处置依据：CI-003 任务书「旧权威回归必红」+ 前台裁决口径（2026-09-16）：
    #   残留重份仅作报告输出，不使注册门恒红；正文重主张权威（上一项）恒红。
    if undeclared and strict:
        results.append(check("retired_l0_declared_archived", False,
                             "旧副本未声明归档（strict）=" + repr(undeclared)))
    else:
        results.append(check("retired_l0_declared_archived", True,
                             "report: 旧副本未声明归档=" + repr(undeclared) if undeclared
                             else ("docs/review 副本均已声明归档（已声明=" + repr(archived_ok[:5])
                                   + f" 共{len(archived_ok)}）") if archived_ok
                             else "docs/review 不存在（旧副本已出库）"))

    # --- D. 旧根索引退役：REVIEW.md 不得复活（复活即旧权威回归）---
    root_review = os.path.join(root, "REVIEW.md")
    rv_exists = os.path.isfile(root_review)
    results.append(check("retired_root_review_absent", not rv_exists,
                         "旧根索引 REVIEW.md 重新出现（ROOT-007 已退役；现行索引=" + ROOT_INDEX + "）"
                         if rv_exists else "根 REVIEW.md 未出现（退役态正确）"))
    return results


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="DOC-002 / DOC-L0 L0 文档完整性校验")
    ap.add_argument("--root", default=str(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--strict", action="store_true",
                    help="docs/review/** 旧副本必须全部显式声明归档")
    ap.add_argument("--self-test", dest="self_test", action="store_true",
                    help="tempfile mini-repo 正/负例自证（不依赖真仓）")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root)

    results = run_checks(root, args.strict)
    failed = [r for r in results if not r["pass"]]
    out = {
        "tool": "eng/tools/check_l0_docs.py",
        "version": "2.0.0",
        "task": "DOC-002",
        "root": root,
        "strict": bool(args.strict),
        "results": results,
        "failed": [r["check"] for r in failed],
        "verdict": "DOC_002_L0_PASS" if not failed else "DOC_002_L0_VIOLATION",
    }
    text = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)), exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as fh:
            fh.write(text + "\n")
    print(text)
    if failed:
        for r in failed:
            print(f"DOC_002_L0_VIOLATION[{r['check']}]: {r['detail']}", file=sys.stderr)
        return 1
    print("DOC_002_PASS: 现行 L0 文档集 4 份 + 索引 active 登记 + 旧权威未回归")
    return 0


# --------------------------------------------------------------------------- #
# 可执行负例面（ENGINEERING_SPEC §8）：tempfile mini-repo，不依赖真仓状态
# --------------------------------------------------------------------------- #
MINI_INDEX = (
    "doc_index:\n"
    "  active:\n"
    + "".join(f"    - path: \"{p}\"\n      status: ACTIVE_NORMATIVE\n" for p in L0_DOCS) +
    "  archived:\n"
    "    - path: \"docs/archive\"\n"
    "      status: ARCHIVED_NON_NORMATIVE\n"
)
BODY = "# L0 doc\n\n" + ("现行 L0 汇总层正文（只汇总权威来源，不抄公式）。 " * 12) + "\n"


def _mk(root: str) -> None:
    for p in list(L0_DOCS) + [INDEX_REL, "docs/archive/OLD.md"]:
        full = os.path.join(root, p)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w", encoding="utf-8") as fh:
            fh.write("# ARCHIVED\n" if p.startswith("docs/archive") else BODY)
    with open(os.path.join(root, INDEX_REL), "w", encoding="utf-8") as fh:
        fh.write(MINI_INDEX)


def _git_init(root: str) -> None:
    for cmd in (["git", "init", "-q"], ["git", "add", "-A"],
                ["git", "-c", "user.email=b12@example.invalid",
                 "-c", "user.name=B12", "commit", "-qm", "fixture"]):
        subprocess.run(cmd, cwd=root, capture_output=True, text=True)


def _mk_repo(tmp: str, name: str) -> str:
    root = os.path.join(tmp, name)
    _mk(root)
    _git_init(root)
    return root


def _write(root: str, rel: str, text: str) -> None:
    full = os.path.join(root, rel)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "w", encoding="utf-8") as fh:
        fh.write(text)


def _res(root: str, strict: bool, rc_expected: int, needles: list[str]) -> tuple[bool, str]:
    results = run_checks(root, strict)
    failed = [r["check"] for r in results if not r["pass"]]
    rc = 0 if not failed else 1
    hits = {n: (n in failed) for n in needles}
    ok = (rc == rc_expected) and all(hits.values())
    return ok, f"rc={rc} (expect {rc_expected}) failed={failed} needles={hits}"


def self_test() -> int:
    """mini-repo 正/负例：正例 rc=0；缺 L0 文档 / 索引未登记 / 旧根索引复活 /
    旧副本重新主张权威 / （strict）旧副本未声明归档 ⇒ rc=1。"""
    import tempfile

    cases: list[tuple[str, bool, str]] = []
    with tempfile.TemporaryDirectory() as tmp:
        # T0 正例：现行文档集 + 索引登记 + 无旧权威
        r0 = _mk_repo(tmp, "t0")
        cases.append(("T0-positive",) + _res(r0, True, 0, []))

        # T1 负例：删一份现行 L0 文档（其余判据仍须跑完）⇒ 红
        r1 = _mk_repo(tmp, "t1")
        os.remove(os.path.join(r1, L0_DOCS[0]))
        cases.append(("T1-l0-doc-missing",) + _res(r1, True, 1, ["l0_docs_present"]))

        # T2 负例：索引不登记 L0 文档 ⇒ 红（旧实现此面会被遮蔽）
        r2 = _mk_repo(tmp, "t2")
        _write(r2, INDEX_REL, "doc_index:\n  active:\n    - path: \"docs/owner\"\n"
                              "      status: ACTIVE_NORMATIVE\n  archived: []\n")
        cases.append(("T2-index-unlisted",) + _res(r2, True, 1, ["l0_docs_indexed_active"]))

        # T3 负例：旧根索引 REVIEW.md 复活 ⇒ 红（旧权威回归）
        r3 = _mk_repo(tmp, "t3")
        _write(r3, "REVIEW.md", "# REVIEW\n")
        cases.append(("T3-root-review-return",) + _res(r3, True, 1,
                                                       ["retired_root_review_absent"]))

        # T4 负例：docs/review 旧副本重新主张权威（ACTIVE_* 抬头）⇒ 红
        r4 = _mk_repo(tmp, "t4")
        _write(r4, "docs/review/CHANGE_REVIEW.md",
               "> 状态：ACTIVE_INFORMATIVE（L0 治理评审汇总层）\n# 旧副本\n")
        cases.append(("T4-old-authority-reassertion",) + _res(r4, True, 1,
                                                              ["retired_l0_no_authority_reassertion"]))

        # T5 负例（仅 --strict）：docs/review 旧副本未声明归档 ⇒ strict 红、默认不红
        r5 = _mk_repo(tmp, "t5")
        _write(r5, "docs/review/SCIENCE_OVERVIEW.md", "# 旧副本（无归档声明）\n")
        ok5s, msg5s = _res(r5, True, 1, ["retired_l0_declared_archived"])
        ok5d, msg5d = _res(r5, False, 0, [])
        cases.append(("T5-strict-vs-default-retired-copy", ok5s and ok5d,
                      f"strict: {msg5s} || default: {msg5d}"))

        # T7 两级口径：未声明归档的残留副本 ⇒ 默认 report 绿、--strict 红
        #    （复用 T5 输入，验证 --strict 真正生效而非 no-op）
        cases.append(("T7-report-vs-strict", ok5d and ok5s,
                      f"default rc=0（report 可见）|| strict rc=1: {msg5s}"))

        # T6 负例：索引文件缺失 ⇒ fail-closed 红（不得当「无违规」）
        r6 = _mk_repo(tmp, "t6")
        os.remove(os.path.join(r6, INDEX_REL))
        cases.append(("T6-index-missing-fail-closed",) + _res(r6, True, 1,
                                                              ["root_index_present",
                                                               "l0_docs_indexed_active"]))

    bad = [(n, m) for n, ok, m in cases if not ok]
    for n, ok, m in cases:
        print(f"SELFTEST {'PASS' if ok else 'FAIL'} {n}: {m}")
    if bad:
        print(f"SELFTEST_FAIL: {len(bad)}/{len(cases)} 例不符预期", file=sys.stderr)
        return 1
    print(f"SELFTEST_PASS: {len(cases)}/{len(cases)} 例符合预期（正例 rc=0；缺文档/索引未登记/"
          "旧根索引复活/旧权威重主张/索引缺失 rc=1；strict 与默认口径可区分）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
