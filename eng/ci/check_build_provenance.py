#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_build_provenance.py — 构建期指纹一致性判据（RUN-PROVENANCE-01）。

判据对象：产品 provenance 自报的**构建指纹**（build_source_digest）与**当前工作树
重算出的指纹**是否一致。语义权威 = docs/VERSIONING.md「构建指纹合同」。

为什么需要这条判据（一手证据 run/RUN-PROVENANCE-01/REPORT.md §A）：
  source_sha 由 CMake configure 期采样一次，改源码不会重跑 configure ⇒ 二进制里是
  新代码、记录里是旧 SHA。于是"两份产物的 source_sha 相同"被当成"同一二进制"做了
  逐位 A/B 比较 —— 结论被污染。本判据把这种不一致变成**具名红灯**。

能红能绿：
  绿 = 记录指纹 == 重算指纹（构建之后工作树未再变动源文件）；
  红 = 记录指纹 != 重算指纹（改源文件未重建 / 用了旧构建树 / 篡改记录），
       并逐条点名差异文件（有 build_stamp_manifest.json 时精确点名）。

具名结论（每条都带证据，不静默降级）：
  BUILD_STAMP_ANCHOR_MISSING   目标里没有构建期指纹 ⇒ rc=2（fail-closed：不可锚定
                               不等于通过；这正是修复前所有历史产物的形态）
  SOURCE_DIGEST_MISMATCH       记录指纹 != 重算指纹 ⇒ rc=1
  STAMP_FORMAT_INVALID         指纹/HEAD 形态非法（64hex / 40hex）⇒ rc=1
  STAMP_INTERNAL_INCONSISTENCY 同一份记录自相矛盾（configure_head_sha != source_sha）⇒ rc=1
  GIT_UNAVAILABLE              git 不可用 ⇒ rc=2（不给结论，不判绿）

退出码：0 = 绿；1 = 判红；2 = 不可锚定 / 依赖不可用（fail-closed）；3 = 用法错误。

用法：
  python3 eng/ci/check_build_provenance.py --build-dir build
  python3 eng/ci/check_build_provenance.py --run-context <out>/run_context.json [--build-dir build]
  python3 eng/ci/check_build_provenance.py --run-manifest <out>/astrocs_run_<id>.json
  python3 eng/ci/check_build_provenance.py --self-test [--json-out PATH]
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "eng", "tools"))

import gen_build_stamp as gbs  # noqa: E402  （唯一指纹实现，判据与生产共用同一口径）

HEX40 = re.compile(r"^[0-9a-f]{40}$")
HEX64 = re.compile(r"^[0-9a-f]{64}$")
STAMP_HEADER = "build_stamp_generated.h"
STAMP_MANIFEST = "build_stamp_manifest.json"
MACROS = ("ASTROCS_BUILD_HEAD_SHA", "ASTROCS_BUILD_DIRTY",
          "ASTROCS_BUILD_SOURCE_DIGEST", "ASTROCS_CONFIGURE_HEAD_SHA")


class NotAnchored(Exception):
    """目标里没有构建期指纹 —— fail-closed，rc=2。"""


def _macro(text, name):
    m = re.search(r'^#define\s+%s\s+"?([^"\n]*)"?\s*$' % name, text, re.M)
    return m.group(1).strip() if m else None


def load_stamp_from_build_dir(build_dir):
    """从构建树的生成头取记录指纹（+ 可选的逐文件清单）。"""
    path = os.path.join(build_dir, STAMP_HEADER)
    if not os.path.isfile(path):
        raise NotAnchored("BUILD_STAMP_ANCHOR_MISSING: %s 不存在（该构建树没有构建期"
                          "指纹生成头；要么是修复前的旧构建树，要么构建没走到该步）" % path)
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    rec = {k: _macro(text, k) for k in MACROS}
    if not rec["ASTROCS_BUILD_SOURCE_DIGEST"]:
        raise NotAnchored("BUILD_STAMP_ANCHOR_MISSING: %s 缺 ASTROCS_BUILD_SOURCE_DIGEST"
                          % path)
    man = os.path.join(build_dir, STAMP_MANIFEST)
    files = None
    if os.path.isfile(man):
        with open(man, encoding="utf-8") as fh:
            files = {e["path"]: e["object_id"] for e in json.load(fh).get("files", [])}
    return {"source": path,
            "head_sha": rec["ASTROCS_BUILD_HEAD_SHA"],
            "dirty": rec["ASTROCS_BUILD_DIRTY"] == "1",
            "source_digest": rec["ASTROCS_BUILD_SOURCE_DIGEST"],
            "configure_head_sha": rec["ASTROCS_CONFIGURE_HEAD_SHA"],
            "files": files}


def _stamp_from_json_doc(doc, source, prov=False):
    obj = doc.get("provenance", {}) if prov else doc
    if not isinstance(obj, dict) or not obj.get("build_source_digest"):
        raise NotAnchored(
            "BUILD_STAMP_ANCHOR_MISSING: %s 里没有 build_source_digest —— 该产物产自"
            "修复前的构建（只有 configure 期 source_sha），不可作为「同一二进制」的锚"
            % source)
    return {"source": source,
            "head_sha": obj.get("build_head_sha"),
            "dirty": obj.get("build_dirty"),
            "source_digest": obj.get("build_source_digest"),
            "configure_head_sha": obj.get("configure_head_sha"),
            "recorded_source_sha": obj.get("source_sha"),
            "files": None}


def load_stamp_from_run_context(path):
    with open(path, encoding="utf-8") as fh:
        return _stamp_from_json_doc(json.load(fh), path)


def load_stamp_from_run_manifest(path):
    with open(path, encoding="utf-8") as fh:
        return _stamp_from_json_doc(json.load(fh), path, prov=True)


def diff_against_manifest(recorded_files, current_entries):
    """逐文件点名差异（只列声明源集内的路径）。"""
    if not recorded_files:
        return None
    changed, added, removed = [], [], []
    for p, obj in sorted(current_entries.items()):
        if p not in recorded_files:
            added.append(p)
        elif recorded_files[p] != obj:
            changed.append(p)
    for p in sorted(recorded_files):
        if p not in current_entries:
            removed.append(p)
    return {"changed": changed, "added": added, "removed": removed}


def evaluate(recorded, root):
    """返回 (problems, evidence)。problems 非空 ⇒ 判红。"""
    problems, ev = [], {}
    if recorded["source_digest"] and not HEX64.match(recorded["source_digest"]):
        problems.append("STAMP_FORMAT_INVALID: build_source_digest 非 64hex: %r"
                        % recorded["source_digest"])
    if recorded["head_sha"] and not HEX40.match(recorded["head_sha"]):
        problems.append("STAMP_FORMAT_INVALID: build_head_sha 非 40hex: %r"
                        % recorded["head_sha"])
    if recorded.get("recorded_source_sha") and recorded.get("configure_head_sha") \
            and recorded["recorded_source_sha"] != recorded["configure_head_sha"]:
        problems.append("STAMP_INTERNAL_INCONSISTENCY: source_sha=%s != configure_head_sha=%s"
                        % (recorded["recorded_source_sha"], recorded["configure_head_sha"]))
    try:
        entries, dirty, untracked, deleted, _ = gbs.collect_source_entries(cwd=root)
        current = gbs.build_stamp(cwd=root)
    except gbs.GitUnavailable as exc:
        raise NotAnchored("GIT_UNAVAILABLE: %s" % exc)
    ev["current_source_digest"] = current["source_digest"]
    ev["current_head_sha"] = current["head_sha"]
    ev["current_dirty"] = current["dirty"]
    if current["source_digest"] != recorded["source_digest"]:
        d = diff_against_manifest(recorded.get("files"), entries)
        detail = ""
        if d:
            ev["file_diff"] = d
            named = (d["changed"] + d["added"] + d["removed"])[:20]
            detail = "；差异文件(%d): %s" % (len(d["changed"]) + len(d["added"]) + len(d["removed"]),
                                           ", ".join(named))
        else:
            cand = sorted(set(dirty) | set(untracked) | set(deleted))[:20]
            ev["candidate_dirty_sources"] = cand
            detail = ("；无逐文件清单，候选(工作树改动/未跟踪/已删): %s"
                      % (", ".join(cand) if cand else "无（记录来自另一构建树或已被篡改）"))
        problems.append(
            "SOURCE_DIGEST_MISMATCH: 记录 build_source_digest=%s != 当前工作树重算=%s"
            " —— 产物不是由当前源集构建出来的（改源码未重建 / 用了旧构建树 / 记录被篡改）%s"
            % (recorded["source_digest"], current["source_digest"], detail))
    return problems, ev


def _self_test():
    """可执行正/负例面：1 绿 + 5 负例（每条都要求具名 token 与退出码）。"""
    import shutil
    import subprocess
    import tempfile

    problems, cases = [], 0

    def run_check(args):
        p = subprocess.run([sys.executable, os.path.abspath(__file__), *args],
                           capture_output=True, text=True, timeout=300)
        return p.returncode, p.stdout + p.stderr

    def rec(name, rc, want_rc, blob, token):
        nonlocal cases
        cases += 1
        ok = rc == want_rc and token in blob
        print("  SELFTEST_%s %-34s rc=%d want=%d token=%r"
              % ("PASS" if ok else "FAIL", name, rc, want_rc, token))
        if not ok:
            problems.append("%s: rc=%d(want %d) token=%r missing\n%s"
                            % (name, rc, want_rc, token, blob[-800:]))

    tmp = tempfile.mkdtemp(prefix="astrocs_bp_")
    try:
        repo = os.path.join(tmp, "r")
        os.makedirs(os.path.join(repo, "lib"))
        for a in (["init", "-q"], ["config", "user.email", "t@t"], ["config", "user.name", "t"]):
            subprocess.run(["git", *a], cwd=repo, check=True, capture_output=True)
        with open(os.path.join(repo, "VERSION"), "w", encoding="utf-8") as fh:
            fh.write("0.11.0-alpha.2\n")
        src = os.path.join(repo, "lib", "a.cpp")
        with open(src, "w", encoding="utf-8") as fh:
            fh.write("int a(){return 1;}\n")
        subprocess.run(["git", "add", "-A"], cwd=repo, check=True, capture_output=True)
        subprocess.run(["git", "commit", "-qm", "c1"], cwd=repo, check=True, capture_output=True)

        stamp = gbs.build_stamp(cwd=repo)
        entries, _, _, _, _ = gbs.collect_source_entries(cwd=repo)
        build_dir = os.path.join(tmp, "build")
        os.makedirs(build_dir)
        with open(os.path.join(build_dir, STAMP_HEADER), "w", encoding="utf-8") as fh:
            fh.write(gbs.render_header(dict(stamp, configure_head_sha=stamp["head_sha"])))
        with open(os.path.join(build_dir, STAMP_MANIFEST), "w", encoding="utf-8") as fh:
            json.dump(gbs.manifest_doc(stamp, entries), fh)
        ctx = os.path.join(tmp, "run_context.json")
        doc = {"schema_version": "1", "kind": "astrocs_run_context", "run_id": "r1",
               "software_version": "0.11.0-alpha.2+g" + stamp["head_sha"],
               "source_sha": stamp["head_sha"],
               "build_head_sha": stamp["head_sha"], "build_dirty": stamp["dirty"],
               "build_source_digest": stamp["source_digest"],
               "configure_head_sha": stamp["head_sha"]}
        with open(ctx, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)

        # P1 绿：记录 == 重算
        rc, blob = run_check(["--build-dir", build_dir, "--root", repo])
        rec("P1_build_dir_green", rc, 0, blob, "BUILD-PROVENANCE_PASS")
        rc, blob = run_check(["--run-context", ctx, "--root", repo])
        rec("P2_run_context_green", rc, 0, blob, "BUILD-PROVENANCE_PASS")

        # N1 改源文件（不重建）⇒ 必须判红并点名 lib/a.cpp
        with open(src, "w", encoding="utf-8") as fh:
            fh.write("int a(){return 2;}\n")
        rc, blob = run_check(["--build-dir", build_dir, "--root", repo])
        rec("N1_source_changed_red", rc, 1, blob, "SOURCE_DIGEST_MISMATCH")
        rec("N1b_names_file", rc, 1, blob, "lib/a.cpp")

        # N2 篡改记录指纹 ⇒ 判红
        bad = dict(doc, build_source_digest="0" * 64)
        with open(ctx, "w", encoding="utf-8") as fh:
            json.dump(bad, fh)
        rc, blob = run_check(["--run-context", ctx, "--root", repo])
        rec("N2_tampered_digest_red", rc, 1, blob, "SOURCE_DIGEST_MISMATCH")

        # N3 记录形态非法 ⇒ 判红
        with open(ctx, "w", encoding="utf-8") as fh:
            json.dump(dict(doc, build_source_digest="zz"), fh)
        rc, blob = run_check(["--run-context", ctx, "--root", repo])
        rec("N3_bad_format_red", rc, 1, blob, "STAMP_FORMAT_INVALID")

        # N4 缺指纹（修复前形态）⇒ 不可锚定，rc=2（不得判绿）
        with open(ctx, "w", encoding="utf-8") as fh:
            json.dump({"schema_version": "1", "kind": "astrocs_run_context", "run_id": "r1",
                       "software_version": "0.11.0-alpha.2+g" + stamp["head_sha"],
                       "source_sha": stamp["head_sha"]}, fh)
        rc, blob = run_check(["--run-context", ctx, "--root", repo])
        rec("N4_anchor_missing_rc2", rc, 2, blob, "BUILD_STAMP_ANCHOR_MISSING")

        # N5 记录自相矛盾 ⇒ 判红
        with open(ctx, "w", encoding="utf-8") as fh:
            json.dump(dict(doc, source_sha="f" * 40), fh)
        rc, blob = run_check(["--run-context", ctx, "--root", repo])
        rec("N5_internal_inconsistency", rc, 1, blob, "STAMP_INTERNAL_INCONSISTENCY")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    print("SELF_TEST %s cases=%d problems=%d"
          % ("PASS" if not problems else "FAIL", cases, len(problems)))
    for p in problems:
        print("  - %s" % p)
    return 0 if not problems else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="构建期指纹一致性判据（RUN-PROVENANCE-01）")
    ap.add_argument("--build-dir", default=None, help="构建树（读 build_stamp_generated.h）")
    ap.add_argument("--run-context", default=None, help="产品 run_context.json")
    ap.add_argument("--run-manifest", default=None, help="产品 astrocs_run_<id>.json")
    ap.add_argument("--root", default=REPO, help="重算指纹的仓库根（默认本仓）")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return _self_test()
    if not (args.build_dir or args.run_context or args.run_manifest):
        print("用法: --build-dir DIR | --run-context FILE | --run-manifest FILE | --self-test",
              file=sys.stderr)
        return 3
    try:
        if args.run_context:
            recorded = load_stamp_from_run_context(args.run_context)
        elif args.run_manifest:
            recorded = load_stamp_from_run_manifest(args.run_manifest)
        else:
            recorded = load_stamp_from_build_dir(args.build_dir)
        if args.build_dir and recorded.get("files") is None:
            try:
                recorded["files"] = load_stamp_from_build_dir(args.build_dir)["files"]
            except NotAnchored:
                pass
        problems, ev = evaluate(recorded, os.path.abspath(args.root))
    except NotAnchored as exc:
        print(str(exc), file=sys.stderr)
        if args.json_out:
            with open(args.json_out, "w", encoding="utf-8") as fh:
                json.dump({"verdict": "not_anchored", "reason": str(exc)}, fh,
                          ensure_ascii=False, indent=1)
        return 2
    out = {"schema_version": 1, "check": "CHK-BUILD-PROVENANCE",
           "anchor_source": recorded["source"], "recorded_source_digest": recorded["source_digest"],
           "verdict": "pass" if not problems else "violation",
           "problems": problems, "evidence": ev}
    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)), exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump(out, fh, ensure_ascii=False, indent=1)
    if problems:
        print("BUILD-PROVENANCE_VIOLATION (%d):" % len(problems))
        for p in problems:
            print("  - %s" % p)
        return 1
    print("BUILD-PROVENANCE_PASS: 构建指纹一致（anchor=%s digest=%s head=%s dirty=%s）"
          % (recorded["source"], recorded["source_digest"][:12],
             (recorded["head_sha"] or "")[:12], recorded["dirty"]))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print("TOOLING_FAILURE: %r" % (exc,), file=sys.stderr)
        sys.exit(3)
