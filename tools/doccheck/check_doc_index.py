#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""GOV-002 机器检查器：docs/DOCUMENT_INDEX.yaml 文档索引 + 归档边界校验。

检查项（exit 0 = PASS）：
  1. docs/DOCUMENT_INDEX.yaml 存在于仓库根（docs/ 下）且受 Git 跟踪；
  2. YAML 结构合法：doc_index.active / doc_index.archived；
  3. 每项 status 唯一取值于 {ACTIVE_NORMATIVE, ACTIVE_INFORMATIVE,
     GENERATED, ARCHIVED_NON_NORMATIVE}；path 全索引唯一（不跨区段重复）；
  4. active 区段不含任何 archive 路径（docs/archive/**、engineering/control/archive/**、
     亦不含路径中含 /archive/ 者）；
  5. archived 区段全部 status == ARCHIVED_NON_NORMATIVE；
  6. 所有 Git 跟踪的 docs/** 与 docs/archive/** md/rst/txt/yaml 文件都被索引覆盖
     （doc_index.active ∪ doc_index.archived 的 path 前缀覆盖）；
  7. archive 内容约束：docs/archive/** 下每个 .md 文件头 400 字符内含
     "ARCHIVED"；engineering/control/archive/** 目录（若存在）含目录级归档说明
     （README_ARCHIVED.md 或目录内 ARCHIVED 标记文件）；
  8. 根与 docs/ 下无散落旧控制包（无 工程控制/ 路径、无 git 跟踪的根级
     旧控制 zip/md）；
  9. 锚存活（ENGINEERING_SPEC §8）：本检查器硬编码引用的仓库路径必须先做存活
     检查；缺失时按口径处理 —— 默认记 skipped_anchor_absent（不判红、但输出可见、
     逐条打印 path），--strict 记红并逐条打印 "ANCHOR_STALE: <path>"；
 10. 旧权威回归（CI-003，2026-09-16）：已退役的旧文档集路径若重新出现，
     必须先显式声明归档（正文头 400 字符含 ARCHIVED* 标记，或在
     docs/DOCUMENT_INDEX.yaml archived 区段登记），否则判红。真仓实测
     2026-09-16：docs/review/** 五份旧 L0 副本中 4 份正文自带
     "ARCHIVED_NON_NORMATIVE（DOC-001，2026-09-16）"抬头 ⇒ 已声明归档、不判红；
     docs/review/CHANGE_REVIEW.md 抬头仍为 "状态：ACTIVE_INFORMATIVE" 且无归档
     声明 ⇒ 本项如实判红（登记 CI-003-B12-F2；DOC-001 对 docs/review/** 的收口缺口）。
 11. 输出稳定排序 JSON 的 machine_index。

--strict 语义（CI-003-B12，2026-09-16 定稿，取代此前空转的 store_true）：
    a) 判据硬引用的锚（ANCHOR_PATHS）任一缺失 ⇒ FAIL，逐条打印
       "ANCHOR_STALE: <path>"（点名常量，不 traceback）；
    b) 退役锚在 --strict 下不判红：旧世代归档树（RETIRED_ANCHORS）已被
       ROOT-007/ARCH-001/RETIRE-001 删除，其缺失即出库完成态；仍缺失 ⇒
       output 打印 "ANCHOR_RETIRED_OK: <path>"，重新出现则由检查 10 处理。
    非 strict 口径：锚缺失 ⇒ 记 skipped_anchor_absent（可见、不判红），
    供「文档集迁移中」的仓库本地自查使用。

用法：
  python3 tools/doccheck/check_doc_index.py [--root <repo>] [--json-out <file>] [--strict] [--quiet]
  python3 tools/doccheck/check_doc_index.py --self-test     # 可执行负例面（tempfile mini-repo）

退出码：0 = PASS，1 = FAIL；--self-test 恒 0 = 全部内置正/负例符合预期，
        任一例不符预期则 1（不依赖真仓状态）。
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys

INDEX_PATH = "docs/DOCUMENT_INDEX.yaml"
LEGAL = {"ACTIVE_NORMATIVE", "ACTIVE_INFORMATIVE", "GENERATED", "ARCHIVED_NON_NORMATIVE"}

# --- 锚存活（ENGINEERING_SPEC §8）-------------------------------------------------
# 本检查器判据硬引用的仓库路径：必须在仓库内存活（缺失 ⇒ fail-closed 判红）。
# 注：目录“不存在”与目录“空”不同 —— 后者是合法状态（该项本就允许档案目录为空），
#     因此 docs/archive / engineering/control/archive 不出现在本表，由检查 7 处理。
ANCHOR_PATHS = (INDEX_PATH,)

# 已退役锚（只增不减的退役登记；每条必须写依据与日期）。
# 退役锚在 --strict 下不判红：其缺失即“整体出库完成”态；重新出现由检查 10 处理。
RETIRED_ANCHORS = (
    {
        "path": "engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/README_ARCHIVED.md",
        "retired_by": "ROOT-007 / RETIRE-001（旧世代控制包归档树整树删除）",
        "date": "2026-09-16",
        "evidence": "git ls-files engineering/ = 0 行；ls engineering/control/archive/ rc=2",
        "note": "原 control_archive_dir_readme 判据已随锚退役降级为检查 7 的目录级说明约束",
    },
)

# --- 旧权威回归面（CI-003 检查 10）-----------------------------------------------
# 退役锚目录树：整树重新出现时要求目录内存在 ARCHIVED 标记文件。
RETIRED_ANCHOR_DIRS = ("engineering/control/archive",)

# 已退役的旧权威文档集（依据：docs/DOCUMENT_INDEX.yaml 抬头 §(b)/(c) DOC-001
# 2026-09-16 —— docs/review/** 移出活动区、权威唯一 = docs/owner/**；
# 旧 CHANGELOG/REVIEW/HANDOVER 已随 ROOT-007 删除且不再索引）。
RETIRED_DOC_PATHS = (
    "docs/review",
    "REVIEW.md",
    "CHANGELOG.md",
    "HANDOVER.md",
)

ARCHIVE_DECL_MARKERS = ("ARCHIVED", "已归档", "已退役")


def check(name: str, ok: bool, detail: str) -> dict:
    return {"check": name, "pass": bool(ok), "detail": detail}


def git_tracked(root: str, rel: str) -> bool:
    # fail-closed：root 不存在/git 不可用时返回 False（调用方据此判红），不得 traceback。
    try:
        r = subprocess.run(["git", "ls-files", "--error-unmatch", "--", rel],
                           cwd=root, capture_output=True, text=True)
    except OSError:
        return False
    return r.returncode == 0


def git_ls(root: str) -> list[str]:
    try:
        r = subprocess.run(["git", "ls-files"], cwd=root,
                           capture_output=True, text=True)
    except OSError:
        return []
    return [x for x in r.stdout.splitlines() if x.strip()]


def is_archive_path(p: str) -> bool:
    return p.startswith("docs/archive/") or p.startswith("engineering/control/archive/") or "/archive/" in "/" + p


def _read_head(path: str, limit: int = 400) -> str:
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            return fh.read(limit)
    except OSError:
        return ""


def _archived_index_paths(root: str) -> set[str]:
    """docs/DOCUMENT_INDEX.yaml archived 区段的 path 集合（读不到 ⇒ 空集）。"""
    try:
        import yaml  # type: ignore
    except Exception:
        return set()
    try:
        with open(os.path.join(root, INDEX_PATH), encoding="utf-8") as fh:
            data = yaml.safe_load(fh) or {}
    except Exception:
        return set()
    out = set()
    for e in ((data.get("doc_index") or {}).get("archived") or []):
        p = (e or {}).get("path")
        if p:
            out.add(str(p))
    return out


def _declares_archived(root: str, rel: str, archived_index: set[str]) -> bool:
    """退役路径是否已显式声明归档（索引 archived 区段登记 或 正文头含 ARCHIVED 标记）。"""
    norm = rel.rstrip("/")
    for a in archived_index:
        a = a.rstrip("/")
        if a == norm or a.startswith(norm + "/") or norm.startswith(a + "/"):
            return True
    full = os.path.join(root, rel)
    if os.path.isfile(full):
        return any(m in _read_head(full) for m in ARCHIVE_DECL_MARKERS)
    return False


def check_anchor_survival(root: str, strict: bool) -> list[dict]:
    """锚存活（ENGINEERING_SPEC §8）：硬引用锚缺失时的显式记账。"""
    out = []
    missing = [p for p in ANCHOR_PATHS if not os.path.exists(os.path.join(root, p))]
    for p in missing:
        if strict:
            print(f"ANCHOR_STALE: {p}")
        else:
            print(f"ANCHOR_ABSENT: {p} (skipped_anchor_absent)")
    detail = ("all live anchors present: " + ", ".join(ANCHOR_PATHS)) if not missing else (
        "ANCHOR_STALE=" + repr(missing) if strict else "skipped_anchor_absent=" + repr(missing))
    out.append(check("anchor_files_alive", not (missing and strict), detail))
    retired_present = [p for p in (a["path"] for a in RETIRED_ANCHORS)
                       if os.path.exists(os.path.join(root, p))]
    for p in (a["path"] for a in RETIRED_ANCHORS):
        if p not in retired_present:
            print(f"ANCHOR_RETIRED_OK: {p} (退役锚，缺失=出库完成态)")
    out.append(check("anchor_retired_not_required", True,
                     "退役锚（不判红）: " + ", ".join(a["path"] for a in RETIRED_ANCHORS)
                     + "；现存=" + repr(retired_present)))
    return out


def check_old_authority_regression(root: str) -> list[dict]:
    """旧权威回归（检查 10）：退役旧文档集重新出现且未显式声明归档 ⇒ 判红。"""
    archived_index = _archived_index_paths(root)
    tracked = git_ls(root)
    offenders: list[str] = []
    declared: list[str] = []
    for rel in RETIRED_DOC_PATHS:
        full = os.path.join(root, rel)
        if os.path.isdir(full):
            for t in tracked:
                if t == rel.rstrip("/") or t.startswith(rel.rstrip("/") + "/"):
                    if _declares_archived(root, t, archived_index):
                        declared.append(t)
                    else:
                        offenders.append(t)
        elif os.path.isfile(full):
            if _declares_archived(root, rel, archived_index):
                declared.append(rel)
            else:
                offenders.append(rel)
    # 退役锚目录树整树回归
    for d in RETIRED_ANCHOR_DIRS:
        if not os.path.isdir(os.path.join(root, d)):
            continue
        marks = [t for t in tracked if t.startswith(d.rstrip("/") + "/")
                 and "ARCHIVED" in os.path.basename(t).upper()]
        if not marks:
            offenders.append(d + "/ (目录存在但无 ARCHIVED 标记文件)")
        else:
            declared.extend(marks[:3])
    ok = not offenders
    detail = ("退役旧文档集未重新出现（已声明归档副本 "
              + repr(sorted(declared)[:5]) + f" 共{len(declared)}）") if ok else \
             ("旧权威回归（未声明归档）=" + repr(sorted(offenders)[:10]) + f" 共{len(offenders)}")
    return [check("retired_authority_not_reintroduced", ok, detail)]


def run_checks(root: str, strict: bool) -> tuple[list[dict], list[dict]]:
    results: list[dict] = []
    machine_index: list[dict] = []

    # fail-closed（ENGINEERING_SPEC §8）：--root 指向不存在目录 ⇒ 立即判红并给出
    # ANCHOR_STALE，不进入 git 调用（否则 subprocess 抛 FileNotFoundError traceback）。
    if not os.path.isdir(root):
        print(f"ANCHOR_STALE: {root} (--root 指向的仓库目录不存在)")
        results.append(check("anchor_files_alive", False,
                             f"仓库根不存在（fail-closed）：{root}"))
        for p in ANCHOR_PATHS:
            print(f"ANCHOR_STALE: {p} (in {root})")
        results.append(check("index_file_exists", False, INDEX_PATH + "（仓库根不存在）"))
        results.append(check("index_file_git_tracked", False, "仓库根不存在（fail-closed）"))
        results.append(check("yaml_parse", False, "仓库根不存在（fail-closed）"))
        results.append(check("docs_fully_covered", False, "仓库根不存在（fail-closed）"))
        results.append(check("retired_authority_not_reintroduced", True,
                             "仓库根不存在：无旧权威可判（已由 anchor/index 项判红）"))
        return results, machine_index

    try:
        import yaml  # type: ignore
    except Exception:
        yaml = None

    index_full = os.path.join(root, INDEX_PATH)
    exists = os.path.isfile(index_full)
    results.append(check("index_file_exists", exists, INDEX_PATH))
    tracked = exists and git_tracked(root, INDEX_PATH)
    results.append(check("index_file_git_tracked", tracked, "git ls-files --error-unmatch -- " + INDEX_PATH))

    doc_index: dict = {}
    if exists:
        if yaml is None:
            results.append(check("yaml_parse", False, "pyyaml 不可用，跳过内容解析"))
        else:
            try:
                data = yaml.safe_load(open(index_full, encoding="utf-8"))
                doc_index = (data or {}).get("doc_index", {})
                results.append(check("yaml_parse", True, "ok"))
            except Exception as exc:
                results.append(check("yaml_parse", False, str(exc)))
    else:
        results.append(check("yaml_parse", False, "索引文件缺失（fail-closed）"))

    active = doc_index.get("active", []) or []
    archived = doc_index.get("archived", []) or []
    all_entries = active + archived

    # 3a. status 合法
    bad_status = [e.get("path") for e in all_entries if e.get("status") not in LEGAL]
    results.append(check("status_legal", not bad_status, f"非法={bad_status}" if bad_status else "ok"))
    # 3b. path 唯一
    paths = [e.get("path", "") for e in all_entries]
    dups = sorted({p for p in paths if paths.count(p) > 1})
    results.append(check("path_unique", not dups, f"重复={dups}" if dups else "ok"))
    # 4. active 不含 archive
    bad_active = [e.get("path") for e in active if is_archive_path(e.get("path", ""))]
    results.append(check("active_no_archive", not bad_active,
                         f"active 含 archive={bad_active}" if bad_active else "ok"))
    # 5. archived 全 ARCHIVED_NON_NORMATIVE
    bad_arch_st = [e.get("path") for e in archived if e.get("status") != "ARCHIVED_NON_NORMATIVE"]
    results.append(check("archived_all_archived_status", not bad_arch_st,
                         f"非ARCHIVED={bad_arch_st}" if bad_arch_st else "ok"))

    # 6. 覆盖：git tracked docs/** md/rst/txt/yaml
    if git_tracked(root, INDEX_PATH):
        allf = git_ls(root)
        docfiles = [p for p in allf
                    if p.startswith("docs/") and p.endswith((".md", ".rst", ".txt", ".yaml", ".yml"))
                    and p != INDEX_PATH]  # index 文件自身不作覆盖目标
        covered_paths = [e.get("path", "") for e in all_entries]
        uncovered = []
        for p in docfiles:
            hit = False
            for cp in covered_paths:
                if p == cp or p.startswith(cp.rstrip("/") + "/"):
                    hit = True
                    break
            if not hit:
                uncovered.append(p)
        results.append(check("docs_fully_covered", not uncovered,
                             f"未覆盖={uncovered[:10]}…共{len(uncovered)}" if uncovered else f"覆盖 {len(docfiles)} 文件"))
    else:
        results.append(check("docs_fully_covered", False,
                             "index 未跟踪/不存在，覆盖检查无法执行（fail-closed）"))

    # 7. archive 内容约束
    archive_md = [p for p in git_ls(root) if p.startswith("docs/archive/") and p.endswith(".md")]
    no_header = []
    for p in archive_md:
        fp = os.path.join(root, p)
        head = _read_head(fp)
        if not head:
            no_header.append(p + "(读失败)")
            continue
        if "ARCHIVED" not in head:
            no_header.append(p)
    results.append(check("archive_md_has_archived_marker", not no_header,
                         f"缺标记={no_header}" if no_header else f"docs/archive {len(archive_md)} 个 md 全含 ARCHIVED"))
    ctrl_archive_dir = os.path.join(root, "engineering/control/archive")
    if not os.path.isdir(ctrl_archive_dir):
        # 锚已整体出库（ROOT-007/RETIRE-001）：不作为违规，但输出可见。
        ctrl_ok, ctrl_detail = True, "engineering/control/archive 不存在（已整体出库，判 PASS）"
    else:
        ctrl_marks = [t for t in git_ls(root)
                      if t.startswith("engineering/control/archive/")
                      and "ARCHIVED" in os.path.basename(t).upper()]
        ctrl_ok = bool(ctrl_marks)
        ctrl_detail = ("工程控制归档目录含 ARCHIVED 标记文件: " + repr(sorted(ctrl_marks)[:3])
                       if ctrl_ok else "engineering/control/archive 存在但无 ARCHIVED 标记文件")
    results.append(check("control_archive_dir_readme", ctrl_ok, ctrl_detail))

    # 8. 根无散落旧控制包
    allf = git_ls(root)
    leftover = [p for p in allf if p.startswith("工程控制/")]
    legacy_root = [p for p in allf
                   if "/" not in p and p.endswith(".zip") and ("CONTROL" in p or "AUDIT" in p or "REVIEW" in p or "PACK" in p)]
    results.append(check("no_legacy_工程控制_left", not leftover, f"残留={leftover[:5]}" if leftover else "ok"))
    results.append(check("no_root_legacy_control_zip", not legacy_root, f"残留={legacy_root}" if legacy_root else "ok"))

    # 9. 锚存活 + 10. 旧权威回归
    results.extend(check_anchor_survival(root, strict))
    results.extend(check_old_authority_regression(root))

    for e in all_entries:
        machine_index.append({"path": e.get("path", ""), "status": e.get("status", "")})
    machine_index = sorted(machine_index, key=lambda x: x["path"])
    return results, machine_index


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="GOV-002 文档索引检查器")
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--strict", action="store_true",
                    help="锚缺失判红（ANCHOR_STALE）；非 strict 记 skipped_anchor_absent")
    ap.add_argument("--self-test", dest="self_test", action="store_true",
                    help="tempfile mini-repo 正/负例自证（不依赖真仓）")
    ap.add_argument("--quiet", action="store_true",
                    help="只打结论摘要，不打 machine_index（CI 日志可读性）")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root)

    results, machine_index = run_checks(root, args.strict)
    passed = all(r["pass"] for r in results)
    out = {
        "tool": "tools/doccheck/check_doc_index.py",
        "version": "1.1.0",
        "task": "GOV-002",
        "root": root,
        "strict": bool(args.strict),
        "results": sorted(results, key=lambda r: r["check"]),
        "machine_index_count": len(machine_index),
        "machine_index": [] if args.quiet else machine_index,
        "verdict": "DOC_INDEX_PASS" if passed else "DOC_INDEX_FAIL",
    }
    text = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)), exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as f:
            f.write(text + "\n")
    print(text)
    return 0 if passed else 1


# --------------------------------------------------------------------------- #
# 可执行负例面（ENGINEERING_SPEC §8）：tempfile mini-repo，不依赖真仓状态
# --------------------------------------------------------------------------- #
OWNER_DOCS = ("SCIENCE_OVERVIEW.md", "PIPELINE_OVERVIEW.md", "ARCHITECTURE_OVERVIEW.md",
              "RELEASE_STATUS.md", "CHANGE_REVIEW.md")
BASE_INDEX = (
    "doc_index:\n"
    "  active:\n"
    "    - path: \"docs/owner\"\n"
    "      status: ACTIVE_NORMATIVE\n"
    "    - path: \"docs/ci\"\n"
    "      status: ACTIVE_NORMATIVE\n"
    "    - path: \"docs/DOC-L0.md\"\n"
    "      status: ACTIVE_NORMATIVE\n"
    "  archived:\n"
    "    - path: \"docs/archive\"\n"
    "      status: ARCHIVED_NON_NORMATIVE\n"
    "    - path: \"docs/review\"\n"
    "      status: ARCHIVED_NON_NORMATIVE\n"
)


def _mk(root: str) -> None:
    for d in ("docs/owner", "docs/ci", "docs/archive", "docs/contracts"):
        os.makedirs(os.path.join(root, d), exist_ok=True)
    with open(os.path.join(root, INDEX_PATH), "w", encoding="utf-8") as fh:
        fh.write(BASE_INDEX)
    for d in OWNER_DOCS:
        with open(os.path.join(root, "docs/owner", d), "w", encoding="utf-8") as fh:
            fh.write("# " + d + "\n\n" + ("owner L0 overview content. " * 20) + "\n")
    with open(os.path.join(root, "docs/ci/01_CHECKS.md"), "w", encoding="utf-8") as fh:
        fh.write("# checks\n")
    with open(os.path.join(root, "docs/archive/OLD.md"), "w", encoding="utf-8") as fh:
        fh.write("# ARCHIVED old doc\n")


def _git_init(root: str) -> None:
    for cmd in (["git", "init", "-q"],
                ["git", "add", "-A"],
                ["git", "-c", "user.email=b12@example.invalid",
                 "-c", "user.name=B12", "commit", "-qm", "fixture"]):
        subprocess.run(cmd, cwd=root, capture_output=True, text=True)


def _mk_repo(tmp: str, name: str) -> str:
    root = os.path.join(tmp, name)
    _mk(root)
    _git_init(root)
    return root


def _write(path: str, text: str) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


def _res(root: str, name: str, strict: bool, rc_expected: int, needle: str) -> tuple[bool, str]:
    results, _ = run_checks(root, strict)
    failed = [r for r in results if not r["pass"]]
    rc = 0 if not failed else 1
    hit = any(needle in r["check"] for r in results)
    ok = (rc == rc_expected) and hit
    return ok, f"rc={rc} (expect {rc_expected}) failed={[r['check'] for r in failed]} needle({needle})={hit}"


def self_test() -> int:
    """mini-repo 正/负例：①锚缺失 strict 红 ②旧权威回归红 ③fail-closed 红。"""
    import tempfile

    cases: list[tuple[str, bool, str]] = []
    with tempfile.TemporaryDirectory() as tmp:
        # S0 正例：完整 mini-repo（owner 文档 + 索引 + 归档标记齐备）⇒ rc=0
        r0 = _mk_repo(tmp, "s0")
        cases.append(("S0-positive",) + _res(r0, "s0", True, 0, "docs_fully_covered"))

        # S1 锚缺失（strict）：索引文件不存在 ⇒ rc=1 + anchor_files_alive 红
        r1 = os.path.join(tmp, "s1")
        _mk(r1)
        os.remove(os.path.join(r1, INDEX_PATH))
        cases.append(("S1-anchor-stale-strict",) + _res(r1, "s1", True, 1, "anchor_files_alive"))

        # S2 锚缺失（非 strict）：同一输入 ⇒ 记 skipped_anchor_absent，但 fail-closed
        #    的 index_file_exists 仍判红 ⇒ rc=1；strict/非 strict detail 必须不同。
        ok2, msg2 = _res(r1, "s2", False, 1, "anchor_files_alive")
        results_nonstrict, _ = run_checks(r1, False)
        results_strict, _ = run_checks(r1, True)
        d_non = next(r["detail"] for r in results_nonstrict if r["check"] == "anchor_files_alive")
        d_str = next(r["detail"] for r in results_strict if r["check"] == "anchor_files_alive")
        ok2 = ok2 and (d_non != d_str) and ("skipped_anchor_absent" in d_non) and ("ANCHOR_STALE" in d_str)
        cases.append(("S2-anchor-strict-vs-default", ok2,
                      msg2 + f" | non-strict={d_non!r} strict={d_str!r}"))

        # S3 旧权威回归：docs/review 副本无 ARCHIVED 抬头、索引也未登记归档 ⇒ rc=1
        r3 = _mk_repo(tmp, "s3")
        _write(os.path.join(r3, "docs/review/SCIENCE_OVERVIEW.md"), "# 旧权威副本（无归档声明）\n")
        _write(os.path.join(r3, INDEX_PATH), BASE_INDEX.replace(
            "    - path: \"docs/review\"\n      status: ARCHIVED_NON_NORMATIVE\n", ""))
        _git_init(r3)
        cases.append(("S3-old-authority-return",) + _res(r3, "s3", True, 1,
                                                         "retired_authority_not_reintroduced"))

        # S4 旧权威副本已声明归档：与 S3 同路径 + ARCHIVED 抬头（+ 索引 archived
        #    区段登记 docs/review）⇒ 覆盖与回归两项都绿 ⇒ rc=0
        r4 = _mk_repo(tmp, "s4")
        _write(os.path.join(r4, "docs/review/SCIENCE_OVERVIEW.md"),
               "> **ARCHIVED_NON_NORMATIVE（DOC-001，2026-09-16）**：旧文档体系。\n# 旧副本\n")
        _write(os.path.join(r4, INDEX_PATH),
               BASE_INDEX + "    - path: \"docs/review/SCIENCE_OVERVIEW.md\"\n"
                            "      status: ARCHIVED_NON_NORMATIVE\n")
        _git_init(r4)
        cases.append(("S4-retired-declared-archived",) + _res(r4, "s4", True, 0,
                                                              "retired_authority_not_reintroduced"))

        # S5 根旧权威索引回归：REVIEW.md 重新出现 ⇒ rc=1
        r5 = _mk_repo(tmp, "s5")
        _write(os.path.join(r5, "REVIEW.md"), "# REVIEW\n")
        _git_init(r5)
        cases.append(("S5-root-review-return",) + _res(r5, "s5", True, 1,
                                                       "retired_authority_not_reintroduced"))

        # S6 fail-closed：索引未覆盖的新文档出现 ⇒ rc=1（docs_fully_covered 真实缺口）
        r6 = _mk_repo(tmp, "s6")
        _write(os.path.join(r6, "docs/contracts/CONFIG_CONTRACT.md"), "# config contract\n")
        _git_init(r6)
        cases.append(("S6-coverage-gap",) + _res(r6, "s6", True, 1, "docs_fully_covered"))

        # S7 空树（无索引、无 owner 文档）⇒ fail-closed rc=1
        r7 = os.path.join(tmp, "s7")
        os.makedirs(r7, exist_ok=True)
        cases.append(("S7-empty-tree-fail-closed",) + _res(r7, "s7", True, 1, "index_file_exists"))

        # S8 --root 指向不存在目录 ⇒ fail-closed rc=1，且不得 traceback（run_checks 直接调用）
        r8 = os.path.join(tmp, "s8-does-not-exist")
        cases.append(("S8-nonexistent-root-fail-closed",) + _res(r8, "s8", True, 1,
                                                                "anchor_files_alive"))

    bad = [(n, m) for n, ok, m in cases if not ok]
    for n, ok, m in cases:
        print(f"SELFTEST {'PASS' if ok else 'FAIL'} {n}: {m}")
    if bad:
        print(f"SELFTEST_FAIL: {len(bad)}/{len(cases)} 例不符预期", file=sys.stderr)
        return 1
    print(f"SELFTEST_PASS: {len(cases)}/{len(cases)} 例符合预期（正例 rc=0；锚缺失/旧权威回归/覆盖缺口/空树 rc=1）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
