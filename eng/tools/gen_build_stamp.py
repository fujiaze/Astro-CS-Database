#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_build_stamp.py — 构建期指纹（RUN-PROVENANCE-01）。

问题（一手证据 run/RUN-PROVENANCE-01/REPORT.md §A）：
  产品 provenance 里的 source_sha 由 **CMake configure 期** 的
  execute_process(git rev-parse HEAD) 烙进 version_generated.h。Ninja 的
  RERUN_CMAKE 规则只依赖 CMake 输入（CMakeLists/*.cmake/*.h.in/CMakeCache），
  **不含任何 .cpp/.h**（实测 build/build.ninja:19586）⇒ 改源码不会重跑 configure
  ⇒ 编译进去的是新代码，记录下来的却是旧 SHA。同一次 configure 下的所有运行
  共享同一个 source_sha，于是"同一 source_sha"被误当作"同一二进制"。

本脚本给出**构建期**采样、且**内容决定**的指纹（真构建指纹）：
  * head_sha        构建时刻的 git HEAD（40hex）
  * dirty           构建时刻工作树是否有未提交改动（VER-001 §2 的 dirty 语义）
  * source_digest   声明源集（见 SOURCE_ROOTS / SOURCE_FILES）的**内容摘要**：
                    逐文件取 git blob 对象 id（内容决定，与提交无关），
                    工作树有改动/未跟踪的文件改取工作树内容的对象 id，
                    工作树已删除的文件记 deleted 标记；
                    再对规范化文本做 sha256 ⇒ 恒为 64hex。

  消费者规则：**判"同一二进制/同一代码"只看 source_digest**。
  head_sha 只作人读补充（HEAD 会因纯文档提交前进，而 source_digest 不变）。

成本：干净工作树上零文件读取（只用 git 索引里的对象 id），实测 < 50ms；
      仅对 git 报告有改动的文件读盘取对象 id。

退出码：0 = 已产出指纹；2 = GIT_UNAVAILABLE（非 git 树 / git 缺失，fail-closed，
        与 eng/tools/gen_version.py 同口径：不给结论、不判绿）；3 = 用法错误。

用法：
  python3 eng/tools/gen_build_stamp.py --json
  python3 eng/tools/gen_build_stamp.py --header <path> [--manifest <path>] [--configure-head <40hex>]
  python3 eng/tools/gen_build_stamp.py --self-test
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

STAMP_SCHEMA = "astrocs.build-stamp/v1"
DIGEST_ALGO = "sha256"
# 声明源集：这些根下的、后缀命中的**跟踪文件**构成"编入产品的源文件面"。
# 判据：lib/** 是全部产品代码（含 vendored 第三方源码，它们确实被编译）；
#       eng/cmake/** 是构建模块；根级 CMakeLists/VERSION/preset 是构建输入。
SOURCE_ROOTS = ("lib", "eng/cmake")
SOURCE_FILES = ("CMakeLists.txt", "VERSION", "CMakePresets.json",
                "eng/contracts/resource_gate_v1.json",
                "eng/packaging/config/runtime_resources.json")
SOURCE_SUFFIXES = (".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx",
                   ".inc", ".inl", ".cmake", ".json", ".in")
# 测试代码不编入产品（eng/tests/** 不在根集内；lib/**/tests/** 在此排除）。
EXCLUDE_SEGMENTS = ("/tests/",)


class GitUnavailable(Exception):
    """git 不可用 / 非 git 工作树 —— fail-closed，rc=2，具名 GIT_UNAVAILABLE。"""


def _git(args, cwd=None, timeout=60):
    """(ok, stdout, reason)。任何不可用形态都返回具名 reason，绝不抛异常。"""
    try:
        p = subprocess.run(["git", *args], cwd=cwd or REPO, capture_output=True,
                           text=True, timeout=timeout)
    except FileNotFoundError:
        return False, "", "git 可执行文件不存在（PATH 上找不到 git）"
    except subprocess.TimeoutExpired:
        return False, "", "git %s 超时（%ds）" % (" ".join(args), timeout)
    except OSError as exc:  # noqa: BLE001
        return False, "", "git %s 调用失败: %s" % (" ".join(args), exc)
    if p.returncode != 0:
        last = (p.stderr.strip().splitlines() or [""])[-1]
        return False, "", "git %s 失败 rc=%d（%s）" % (" ".join(args), p.returncode, last[:160])
    return True, p.stdout, ""


def git(args, cwd=None, timeout=60):
    ok, out, why = _git(args, cwd=cwd, timeout=timeout)
    if not ok:
        raise GitUnavailable(why)
    return out


def _in_scope(path):
    if any(seg in "/" + path for seg in EXCLUDE_SEGMENTS):
        return False
    if path in SOURCE_FILES:
        return True
    if not any(path == r or path.startswith(r + "/") for r in SOURCE_ROOTS):
        return False
    return path.endswith(SOURCE_SUFFIXES)


def _object_ids(paths, cwd):
    """批量取工作树内容的对象 id（git hash-object --stdin-paths）。"""
    if not paths:
        return {}
    p = subprocess.run(["git", "hash-object", "--stdin-paths"], cwd=cwd,
                       input="\n".join(paths) + "\n", capture_output=True, text=True,
                       timeout=120)
    if p.returncode != 0:
        raise GitUnavailable("git hash-object 失败 rc=%d（%s）"
                             % (p.returncode, (p.stderr.strip().splitlines() or [""])[-1][:160]))
    ids = p.stdout.split()
    if len(ids) != len(paths):
        raise GitUnavailable("git hash-object 输出条数 %d != 输入 %d" % (len(ids), len(paths)))
    return dict(zip(paths, ids))


def collect_source_entries(cwd=None):
    """返回 (entries, dirty, untracked, deleted, object_format)。

    entries: {path: <内容对象 id 或 "deleted">}，覆盖声明源集的**全部**成员。
    干净文件取索引对象 id（零读盘）；改动/未跟踪文件取工作树内容对象 id。
    """
    cwd = cwd or REPO
    fmt = git(["rev-parse", "--show-object-format"], cwd=cwd).strip() or "sha1"
    raw = git(["ls-files", "-s", "-z", "--", *SOURCE_ROOTS, *SOURCE_FILES], cwd=cwd)
    entries = {}
    for rec in raw.split("\0"):
        if not rec:
            continue
        meta, _, path = rec.partition("\t")
        parts = meta.split()
        if len(parts) < 3 or not _in_scope(path):
            continue
        entries[path] = parts[1]

    porc = git(["status", "--porcelain=v1", "-z", "--untracked-files=all", "--",
                *SOURCE_ROOTS, *SOURCE_FILES], cwd=cwd)
    # -z 形态：XY<空格><path>NUL；重命名/复制另跟一个 NUL 分隔的源路径。
    fields = porc.split("\0")
    dirty, untracked, deleted, modified = [], [], [], []
    i = 0
    while i < len(fields):
        f = fields[i]
        i += 1
        if not f:
            continue
        xy, path = f[:2], f[3:]
        if xy[0] in ("R", "C"):
            i += 1  # 跳过重命名/复制的源路径字段
        if not _in_scope(path):
            continue
        if xy == "??":
            untracked.append(path)
        elif xy[1] == "D" or xy[0] == "D":
            deleted.append(path)
        else:
            dirty.append(path)
            if xy[1] != " ":
                modified.append(path)

    worktree_ids = _object_ids(sorted(set(modified) | set(untracked)), cwd)
    for path in untracked:
        entries[path] = worktree_ids.get(path, "unknown")
    for path in modified:
        if path in entries or os.path.isfile(os.path.join(cwd, path)):
            entries[path] = worktree_ids.get(path, "unknown")
    for path in deleted:
        entries[path] = "deleted"
    return entries, sorted(set(dirty)), sorted(set(untracked)), sorted(set(deleted)), fmt


def source_digest(entries):
    """规范化文本 → sha256（64hex）。与提交、时间、平台无关，只由内容决定。"""
    lines = ["%s\t%s" % (p, entries[p]) for p in sorted(entries)]
    payload = "%s\nroots=%s\nfiles=%s\n" % (
        STAMP_SCHEMA, ",".join(SOURCE_ROOTS), ",".join(SOURCE_FILES))
    payload += "\n".join(lines) + ("\n" if lines else "")
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def build_stamp(cwd=None, configure_head=None, stamp_utc=None):
    cwd = cwd or REPO
    head = git(["rev-parse", "HEAD"], cwd=cwd).strip()
    if not re.fullmatch(r"[0-9a-f]{40}", head):
        raise GitUnavailable("HEAD SHA 异常：%r" % head)
    entries, dirty, untracked, deleted, fmt = collect_source_entries(cwd)
    porcelain_all = git(["status", "--porcelain=v1", "--untracked-files=all"], cwd=cwd)
    return {
        "schema_version": 1,
        "kind": STAMP_SCHEMA,
        "head_sha": head,
        "dirty": bool(porcelain_all.strip()),
        "source_dirty_count": len(dirty) + len(untracked),
        "source_untracked_count": len(untracked),
        "source_deleted_count": len(deleted),
        "source_digest": source_digest(entries),
        "digest_algorithm": DIGEST_ALGO,
        "git_object_format": fmt,
        "source_file_count": len(entries),
        "source_roots": list(SOURCE_ROOTS),
        "source_files": list(SOURCE_FILES),
        "configure_head_sha": configure_head,
        "stamp_utc": stamp_utc or datetime.datetime.now(datetime.timezone.utc)
                     .strftime("%Y-%m-%dT%H:%M:%SZ"),
    }


def render_header(stamp):
    """C 头：宏名与既有 version_generated.h 同族，禁手改。"""
    def s(v):
        return '""' if v is None else '"%s"' % v
    return (
        "// 由 eng/tools/gen_build_stamp.py 生成 — 禁手改（构建期指纹）\n"
        "// 依据：docs/VERSIONING.md 构建指纹合同 / RUN-PROVENANCE-01\n"
        "#pragma once\n"
        "#define ASTROCS_BUILD_HEAD_SHA %s\n"
        "#define ASTROCS_BUILD_DIRTY %d\n"
        "#define ASTROCS_BUILD_SOURCE_DIGEST %s\n"
        "#define ASTROCS_BUILD_STAMP_UTC %s\n"
        "#define ASTROCS_CONFIGURE_HEAD_SHA %s\n"
        % (s(stamp["head_sha"]), 1 if stamp["dirty"] else 0,
           s(stamp["source_digest"]), s(stamp["stamp_utc"]),
           s(stamp["configure_head_sha"]))
    )


def write_if_different(path, text):
    """内容相同则不落盘（mtime 不变 ⇒ 不触发下游重编译/重链接）。"""
    try:
        with open(path, encoding="utf-8") as fh:
            if fh.read() == text:
                return False
    except OSError:
        pass
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as fh:
        fh.write(text)
    os.replace(tmp, path)
    return True


def manifest_doc(stamp, entries):
    return {
        "schema_version": 1,
        "kind": "astrocs.build-stamp-manifest/v1",
        "head_sha": stamp["head_sha"],
        "source_digest": stamp["source_digest"],
        "git_object_format": stamp["git_object_format"],
        "file_count": len(entries),
        "files": [{"path": p, "object_id": entries[p]} for p in sorted(entries)],
    }


def _self_test():
    """能红能绿：内容变 ⇒ digest 变；内容不变 ⇒ digest 不变；非 git ⇒ GIT_UNAVAILABLE。"""
    import shutil
    import tempfile
    problems, cases = [], 0

    def rec(name, ok, detail=""):
        nonlocal cases
        cases += 1
        print("  SELFTEST_%s %-32s %s" % ("PASS" if ok else "FAIL", name, detail))
        if not ok:
            problems.append("%s: %s" % (name, detail))

    tmp = tempfile.mkdtemp(prefix="astrocs_stamp_")
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

        s1 = build_stamp(cwd=repo, configure_head="a" * 40)
        s1b = build_stamp(cwd=repo, configure_head="a" * 40)
        rec("deterministic", s1["source_digest"] == s1b["source_digest"],
            "digest=%s" % s1["source_digest"][:12])
        rec("clean_not_dirty", s1["dirty"] is False and s1["source_dirty_count"] == 0,
            "dirty=%s count=%d" % (s1["dirty"], s1["source_dirty_count"]))

        # 绿→红：改内容（不提交）⇒ digest 必须变，且 dirty 置位
        with open(src, "w", encoding="utf-8") as fh:
            fh.write("int a(){return 2;}\n")
        s2 = build_stamp(cwd=repo, configure_head="a" * 40)
        rec("content_change_red", s2["source_digest"] != s1["source_digest"] and s2["dirty"] is True,
            "%s -> %s dirty=%s" % (s1["source_digest"][:8], s2["source_digest"][:8], s2["dirty"]))

        # 提交后内容不变 ⇒ digest 必须**不变**（指纹由内容决定，不由提交决定）
        subprocess.run(["git", "commit", "-qam", "c2"], cwd=repo, check=True, capture_output=True)
        s3 = build_stamp(cwd=repo, configure_head="a" * 40)
        rec("commit_only_green", s3["source_digest"] == s2["source_digest"]
            and s3["head_sha"] != s1["head_sha"] and s3["dirty"] is False,
            "head %s->%s digest 稳定" % (s1["head_sha"][:8], s3["head_sha"][:8]))

        # 未跟踪的新源文件也算进源集（新增文件即换内容）
        with open(os.path.join(repo, "lib", "b.cpp"), "w", encoding="utf-8") as fh:
            fh.write("int b(){return 3;}\n")
        s4 = build_stamp(cwd=repo, configure_head="a" * 40)
        rec("untracked_counted", s4["source_digest"] != s3["source_digest"]
            and s4["source_untracked_count"] == 1, "untracked=%d" % s4["source_untracked_count"])

        # 未跟踪文件被移除 ⇒ 必须回到 s3 的 digest（内容决定，不留状态残留）
        os.remove(os.path.join(repo, "lib", "b.cpp"))
        s4b = build_stamp(cwd=repo, configure_head="a" * 40)
        rec("untracked_removed_green", s4b["source_digest"] == s3["source_digest"],
            "digest=%s" % s4b["source_digest"][:12])

        # 删除已跟踪文件也算换内容
        os.remove(src)
        s5 = build_stamp(cwd=repo, configure_head="a" * 40)
        rec("deletion_counted", s5["source_digest"] != s3["source_digest"]
            and s5["source_deleted_count"] == 1, "deleted=%d" % s5["source_deleted_count"])

        # 头文件渲染：宏齐全、dirty 为 0/1
        hdr = render_header(s3)
        rec("header_macros", all(m in hdr for m in (
            "ASTROCS_BUILD_HEAD_SHA", "ASTROCS_BUILD_DIRTY",
            "ASTROCS_BUILD_SOURCE_DIGEST", "ASTROCS_CONFIGURE_HEAD_SHA")), "ok")

        # 负例：非 git 树 ⇒ 具名 GIT_UNAVAILABLE，不 traceback
        nogit = os.path.join(tmp, "nogit")
        os.makedirs(os.path.join(nogit, "eng", "tools"))
        os.makedirs(os.path.join(nogit, "lib"))
        # 必须用树内副本：REPO 由 __file__ 上三级决定，用真仓脚本会指向真仓（假绿）
        script_copy = os.path.join(nogit, "eng", "tools", "gen_build_stamp.py")
        shutil.copy2(os.path.abspath(__file__), script_copy)
        env = dict(os.environ, GIT_CEILING_DIRECTORIES=tmp)
        r = subprocess.run([sys.executable, script_copy, "--json"],
                           cwd=nogit, capture_output=True, text=True, env=env, timeout=120)
        rec("non_git_failclosed", r.returncode == 2 and "GIT_UNAVAILABLE" in r.stderr
            and "Traceback" not in r.stderr, "rc=%d" % r.returncode)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    print("SELF_TEST %s cases=%d problems=%d"
          % ("PASS" if not problems else "FAIL", cases, len(problems)))
    for p in problems:
        print("  - %s" % p)
    return 0 if not problems else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="构建期指纹生成（RUN-PROVENANCE-01）")
    ap.add_argument("--json", action="store_true", help="打印指纹 JSON")
    ap.add_argument("--header", default=None, help="写出 C 头（构建期宏）")
    ap.add_argument("--manifest", default=None, help="写出逐文件对象 id 清单（供判据精确点名）")
    ap.add_argument("--configure-head", default=None, help="CMake configure 期 HEAD（仅供对照）")
    ap.add_argument("--root", default=None, help="仓库根（默认脚本上三级）")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return _self_test()
    global REPO
    if args.root:
        REPO = os.path.abspath(args.root)
    try:
        stamp = build_stamp(configure_head=args.configure_head)
        entries, _, _, _, _ = collect_source_entries()
    except GitUnavailable as exc:
        print("GIT_UNAVAILABLE: %s" % exc, file=sys.stderr)
        return 2
    if args.header:
        write_if_different(args.header, render_header(stamp))
    if args.manifest:
        write_if_different(args.manifest,
                           json.dumps(manifest_doc(stamp, entries), ensure_ascii=False, indent=1) + "\n")
    if args.json or (not args.header and not args.manifest):
        print(json.dumps(stamp, ensure_ascii=False, indent=1))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except GitUnavailable as exc:  # 兜底：绝不 traceback
        print("GIT_UNAVAILABLE: %s" % exc, file=sys.stderr)
        sys.exit(2)
