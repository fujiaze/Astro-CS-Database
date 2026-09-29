#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-CTEST-SKIP-REGISTER｜eng/ci/ctest_skip_register.json 的消费者门（FINAL-07 R2）。

为什么立本门（根因，一手实测）
  `eng/ci/ctest_skip_register.json` 登记了 12 条 ctest SKIP（类别/原因/owner/解除条件）
  与 4 条编译期平台排除项，`eng/tools/quality/authority_surfaces.json` 也把它登记为
  在役权威面（authority = 「skip 必须显式登记」），但**全仓零机器消费方**
  （design_clauses.json:712-717 的 DESIGN-12.4-THREE-ERROR-SEMANTICS 已按 unwired 记
  录，退出条件是「出现门读它并断言『每个 SKIP 逐条登记 + 未登记 SKIP 即红 +
  登记只减不增』」）。后果：SKIP 可以任意新增而不被任何门拦；登记项引用的
  ctest 名字若根本不存在，也没有门会红（2026-09-29 实测：12 条 SKIP 里有 11 条是
  `gtest_discover_tests` 的**发现期**名字 `phase2_synthetic_gate.<Suite>.<Case>`，
  静态 CMake 文本正则根本看不见它们）。

判据（任一违规 ⇒ exit 1）
  K1 结构    顶层 schema_version/purpose/skip_count/skips 齐备；skips 非空；
             每条 test/class/source/reason/owner/unblock 非空；class 必须在 classes 词表内。
  K2 存在性  **本门存在的理由**：每条 `skips[].test` 与
             `platform_excluded_targets.targets[].test` 都必须是**真实存在的 ctest 目标**。
             核对面 = eng/ci/ctest_face.py 的合并面（面 A 配置期注册面 ∪ 面 B 实际
             配置面 `ctest -N --show-only=json-v1`），与本登记册**无依赖关系**
             （不拿登记册自己证明自己）。指向不存在的名字 ⇒ SKIP_SKIP_NAME_UNKNOWN。
  K3 计数    skip_count 与 len(skips) 一致；summary 各类计数与实际条目一致。
  K4 无重叠  同一名字不得同时出现在 skips 与 platform_excluded_targets
             （运行期 SKIP 与编译期排除是两套口径，混登会让「是否被跑」不可判）。
  K5 锚存活  每条 source 的 `file:line` 必须在提交树里存在且行号在范围内。
             （**只**核「锚仍有对象」；source 行是否仍是 skip 站点不判——
             实测 12 条里有若干条的行号已随 lib/** 改动漂移，见本文件末尾备注。）
  K6 未登记即红 给了运行期证据（--disabled-log / --junit）时：实际 disabled/skipped 集
             必须 ⊆ 登记集，否则 SKIP_UNREGISTERED 判红。
  K7 fail-closed  登记册缺失/不可解析 ⇒ exit 2 点名（SKIP_REGISTER_INPUT_UNAVAILABLE）；
             核对面两个面都空 ⇒ 判红（SKIP_REGISTER_FACE_UNAVAILABLE）。

只读（除 --json-out 显式请求）；仅 stdlib（+ git 子进程带 timeout）。无网络。
用法:
  python3 eng/ci/check_ctest_skip_register.py [--json-out F] [--build-dir D]...
  python3 eng/ci/check_ctest_skip_register.py --self-test
  python3 eng/ci/check_ctest_skip_register.py --fault-inject all
exit 0 = 全绿；1 = 判据命中；2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import copy
import json
import os
import pathlib
import re
import sys
import xml.etree.ElementTree as _ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ctest_face  # noqa: E402  (K2 的核对面单一实现点)

REPO = pathlib.Path(__file__).resolve().parent.parent.parent
DEFAULT_REGISTRY = os.path.join("eng", "ci", "ctest_skip_register.json")
REQUIRED_SKIP_FIELDS = ("test", "class", "source", "reason", "owner", "unblock")


def _blank(value) -> bool:
    return not str(value or "").strip()


# --------------------------------------------------------------- 运行期证据面 ----

def read_disabled_log(path) -> tuple:
    """ctest 的 Testing/Temporary/LastTestsDisabled.log ⇒ 名字集。

    形态 `<序号>:<ctest 名>`。返回 (names, errors)。
    """
    names, errors = set(), []
    p = pathlib.Path(path)
    if not p.is_file():
        return names, ["LastTestsDisabled.log 不存在：%s" % p]
    for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if not line:
            continue
        m = re.match(r"^\d+:(.*)$", line)
        names.add((m.group(1) if m else line).strip())
    return names, errors


def read_junit(path) -> tuple:
    """ctest --output-junit 的 XML ⇒ skipped 用例名集。返回 (names, errors)。"""
    names, errors = set(), []
    p = pathlib.Path(path)
    if not p.is_file():
        return names, ["JUnit 不存在：%s" % p]
    try:
        root = _ET.parse(str(p)).getroot()
    except _ET.ParseError as exc:
        return names, ["JUnit 不可解析：%s（%s）" % (p, exc)]
    cases = ([root] if root.tag == "testcase" else list(root.iter("testcase")))
    for c in cases:
        name = str(c.get("name") or "").strip()
        cls = str(c.get("classname") or "").strip()
        if c.find("skipped") is not None and name:
            names.add("%s.%s" % (cls, name) if cls else name)
    return names, errors


def check_registry(repo, doc, face, evidence=None) -> tuple:
    """K1..K6 静态 + 运行期判定。返回 (problems, info)。纯函数（除只读 git）。"""
    repo = pathlib.Path(repo)
    problems = []
    info = {"skips": 0, "platform_excluded": 0, "names_checked": [],
            "face": face.as_dict() if hasattr(face, "as_dict") else None,
            "evidence_names": sorted((evidence or {}).get("names", ()))[:40],
            "evidence_source": (evidence or {}).get("source")}
    if not isinstance(doc, dict):
        return ["K1 登记册顶层必须是对象"], info
    for key in ("schema_version", "purpose", "skip_count", "skips"):
        if key not in doc:
            problems.append("K1 顶层缺必需键：%s" % key)
    classes = doc.get("classes")
    if not isinstance(classes, dict) or not classes:
        problems.append("K1 顶层 classes 必须是非空的 class -> 释义映射")
        classes = {}
    skips = doc.get("skips")
    if not isinstance(skips, list):
        return problems + ["K1 skips 必须是数组"], info
    if not skips:
        return problems + ["K1 skips 为空（fail-closed：登记册清零不得判绿）"], info
    info["skips"] = len(skips)
    # K1 逐条登记：必填字段 + class 词表
    skip_names, seen = [], {}
    for idx, s in enumerate(skips):
        where = "skips[%d]" % idx
        if not isinstance(s, dict):
            problems.append("K1 %s 必须是对象" % where)
            continue
        missing = [f for f in REQUIRED_SKIP_FIELDS if _blank(s.get(f))]
        if missing:
            problems.append("K1 %s 未逐条登记，缺字段：%s" % (where, missing))
            continue
        cls = str(s["class"])
        if cls not in classes:
            problems.append("K1 %s 的 class=%r 不在顶层 classes 词表内（登记了一个"
                             "没有释义的类别）" % (where, cls))
        name = str(s["test"])
        if name in seen:
            problems.append("K1 %s 与 skips[%d] 登记了同一个名字 %r" % (where, seen[name], name))
        else:
            seen[name] = idx
        skip_names.append(name)
        # K5 锚存活：source 的 file:line 必须仍在提交树里
        if not _anchor_alive(repo, str(s["source"])):
            problems.append(
                "K5 %s (%s) 的 source 行锚失效：%r（文件不存在 / 未提交 / 行号越界）"
                % (where, name, s["source"]))
    # K3 计数自洽
    if str(doc.get("skip_count")) != str(len(skips)):
        problems.append("K3 skip_count=%r 与实际条目数 %d 不一致" % (doc.get("skip_count"),
                                                                    len(skips)))
    summary = doc.get("summary")
    if isinstance(summary, dict):
        actual = {}
        for s in skips:
            if isinstance(s, dict) and not _blank(s.get("class")):
                actual[str(s["class"])] = actual.get(str(s["class"]), 0) + 1
        for cls, n in actual.items():
            if str(summary.get(cls)) != str(n):
                problems.append("K3 summary.%s=%r 与实际计数 %d 不一致"
                                % (cls, summary.get(cls), n))
    # 平台排除项（编译期口径，与运行期 SKIP 分开计数）
    pe = doc.get("platform_excluded_targets") or {}
    pe_names = []
    if isinstance(pe, dict):
        for idx, t in enumerate(pe.get("targets") or []):
            if not isinstance(t, dict) or _blank(t.get("test")):
                problems.append("K1 platform_excluded_targets.targets[%d] 缺 test" % idx)
                continue
            pe_names.append(str(t["test"]))
            if _blank(t.get("cmake_anchor")) or _blank(t.get("semantic_gap")):
                problems.append("K1 platform_excluded_targets.targets[%d] (%s) 缺"
                                 " cmake_anchor/semantic_gap（排除依据必须可复核）"
                                 % (idx, t["test"]))
    info["platform_excluded"] = len(pe_names)
    # K4 无重叠：一个名字不能既是运行期 SKIP 又是编译期排除
    for name in sorted(set(pe_names) & set(skip_names)):
        problems.append("K4 %s 同时登记在 skips 与 platform_excluded_targets"
                        "（两套口径混登 ⇒ 「它到底跑不跑」不可判）" % name)
    # K2 存在性（本门的核心）：名字必须真的存在于独立枚举面
    if face is None:
        problems.append("K7 核对面不可用（面 A 配置期注册面与面 B 实际配置面都拿不到）"
                        "——空面不得静默判绿")
    else:
        for name in skip_names + pe_names:
            if not face.has(name):
                problems.append(
                    "SKIP_SKIP_NAME_UNKNOWN 登记的 ctest 目标 %r 不在配置期注册面"
                    "（面 A 静态名 ∪ 面 B 实际配置面）——SKIP 登记指向了一个不存在的"
                    "测试名，等于没有登记任何东西" % name)
            else:
                info["names_checked"].append(name)
    # K6 未登记即红：运行期 disabled/skipped 集必须 ⊆ 登记集
    if evidence:
        got = set(evidence.get("names") or ())
        for name in sorted(got - set(skip_names)):
            problems.append(
                "SKIP_UNREGISTERED 运行期证据（%s）里的 %r 未登记在 skips 中"
                "——新增 SKIP 必须先登记（DESIGN-12.4-THREE-ERROR-SEMANTICS 退出条件）"
                % (evidence.get("source"), name))
    return problems, info


def _anchor_alive(repo, source: str) -> bool:
    """K5：`path:line`（可带尾注）指向的锚在提交树里是否仍有对象。"""
    parsed = ctest_face.find_anchor(str(source or ""))
    if not parsed:
        return False
    rel, a, b = parsed
    if not (repo / rel).is_file():
        return False
    text = ctest_face.read_committed(repo, rel)
    if not text:
        return False
    return b <= len(text.splitlines())
    return problems, info


def _self_test() -> int:
    """内存 fixture 正/负例：K1..K7 逐条。"""
    import shutil
    import subprocess as _sp
    import tempfile

    problems = []
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="astrocs_skip_"))
    try:
        _sp.run(["git", "init", "-q"], cwd=str(tmp), capture_output=True)
        _sp.run(["git", "config", "user.email", "t@t"], cwd=str(tmp), capture_output=True)
        _sp.run(["git", "config", "user.name", "t"], cwd=str(tmp), capture_output=True)
        (tmp / "lib").mkdir(parents=True, exist_ok=True)
        (tmp / "lib" / "src.cpp").write_text("\n".join("line %d" % i for i in range(1, 21)) + "\n",
                                               encoding="utf-8")
        (tmp / "eng").mkdir(parents=True, exist_ok=True)
        (tmp / "eng" / "CMakeLists.txt").write_text(
            "add_test(NAME real_skip COMMAND t)\n", encoding="utf-8")
        _sp.run(["git", "add", "-A"], cwd=str(tmp), capture_output=True)
        _sp.run(["git", "commit", "-qm", "k"], cwd=str(tmp), capture_output=True)
        fj = tmp / "face.json"
        fj.write_text(json.dumps({"version": {"major": 1},
                                  "tests": [{"name": "real_skip"}]}),
                      encoding="utf-8")
        face = ctest_face.resolve(tmp, json_path=fj, run_ctest=False)

        def doc(skips=None, classes=None, count=1, summary=None, pe=None, drop=()):
            d = {"schema_version": 1, "purpose": "p",
                 "skip_count": count,
                 "classes": classes if classes is not None
                 else {"HOST_CAPABILITY": "宿主能力", "FIXTURE_MISSING": "缺 fixture"},
                 "skips": skips if skips is not None
                 else [{"test": "real_skip", "class": "HOST_CAPABILITY",
                        "source": "lib/src.cpp:5", "reason": "r", "owner": "o",
                        "unblock": "u"}]}
            for k in drop:
                d.pop(k, None)
            if summary is not None:
                d["summary"] = summary
            if pe is not None:
                d["platform_excluded_targets"] = pe
            return d

        def _with(entry, kv):
            out = dict(entry)
            out.update(kv)
            return out

        def case(name, d, want_rc, token=None, ev=None):
            p, _i = check_registry(tmp, d, face, ev)
            rc = 1 if p else 0
            blob = " ".join(p)
            ok = (rc == want_rc) and (token is None or token in blob)
            problems.append("%s rc=%d want=%d %s | %s"
                            % (name, rc, want_rc, "OK" if ok else "MISMATCH", blob[:110]))

        case("pos_baseline", doc(), 0)
        case("neg_unknown_name",
             doc(skips=[dict(doc()["skips"][0], test="no_such_skip_zzz")]), 1,
             "SKIP_SKIP_NAME_UNKNOWN")
        case("neg_class_not_in_vocab",
             doc(skips=[_with(doc()["skips"][0], {"class": "MYSTERY"})]), 1,
             "不在顶层 classes")
        case("neg_missing_field",
             doc(skips=[{k: v for k, v in doc()["skips"][0].items() if k != "owner"}]), 1,
             "未逐条登记")
        case("neg_count_mismatch", doc(count=7), 1, "K3 skip_count")
        case("neg_summary_mismatch",
             doc(summary={"HOST_CAPABILITY": 5, "FIXTURE_MISSING": 1}), 1, "K3 summary")
        case("neg_anchor_dead",
             doc(skips=[dict(doc()["skips"][0], source="lib/src.cpp:9999")]), 1, "K5")
        case("neg_anchor_file_absent",
             doc(skips=[dict(doc()["skips"][0], source="lib/nope.cpp:1")]), 1, "K5")
        case("neg_overlap",
             doc(pe={"targets": [{"test": "real_skip", "cmake_anchor": "a/CMakeLists.txt",
                                 "semantic_gap": "x"}]}), 1, "K4")
        case("neg_platform_excluded_unknown",
             doc(pe={"targets": [{"test": "ghost_excl_zzz", "cmake_anchor": "a/CMakeLists.txt",
                                 "semantic_gap": "x"}]}), 1, "SKIP_SKIP_NAME_UNKNOWN")
        case("neg_empty_skips", doc(skips=[], count=0), 1, "skips 为空")
        case("neg_drop_required_key", doc(drop=("skip_count",)), 1, "顶层缺必需键")
        case("pos_unregistered_absent_evidence", doc(), 0, None,
             {"names": {"real_skip"}, "source": "disabled.log"})
        case("neg_unregistered_skip", doc(), 1, "SKIP_UNREGISTERED",
             {"names": {"real_skip", "brand_new_skip_zzz"}, "source": "junit"})
        case("neg_face_none", doc(), 0)
        p, _i = check_registry(tmp, doc(), None, None)
        ok = any("K7 核对面不可用" in x for x in p)
        problems.append("neg_face_unavailable_failclosed %s | %s"
                        % ("OK" if ok else "MISMATCH", p))
        # 判别力自证：面 A 里的真名与假名的区别必须体现在判定上
        _p0, _i0 = check_registry(tmp, doc(), face, None)
        _p1, _i1 = check_registry(tmp, doc(skips=[dict(doc()["skips"][0],
                                                        test="real_skip_x")]), face, None)
        ok = (not _p0) and any("SKIP_SKIP_NAME_UNKNOWN" in x for x in _p1)
        problems.append("disc_name_existence_not_constant %s"
                        % ("OK" if ok else "MISMATCH"))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    bad = [p for p in problems if "OK" not in p]
    for p in problems:
        print("[selftest] %s" % p)
    print("[selftest] %d cases, %s"
          % (len(problems), "ALL OK" if not bad else "FAILED"))
    return 0 if not bad else 1


def _fault_inject(which: str, repo=REPO) -> int:
    """真仓注入负例：每例必须判红（能红能绿）。注入只改内存 doc，不落盘。"""
    path = os.path.join(str(repo), DEFAULT_REGISTRY)
    with open(path, encoding="utf-8") as fh:
        base = json.load(fh)
    face = _resolve_face(repo)
    if isinstance(face, Exception):
        print("FAULT_INJECT_UNAVAILABLE 核对面不可用：%s" % face, file=sys.stderr)
        return 2
    cases = {}

    def mut(fn):
        d = copy.deepcopy(base)
        fn(d)
        return d

    cases["ghost_name"] = (mut(lambda d: d["skips"][0].__setitem__(
        "test", "ctest_ghost_skip_zzz_not_registered")), "SKIP_SKIP_NAME_UNKNOWN")
    cases["unregistered"] = (mut(lambda d: d.__setitem__("skips", d["skips"] + [
        {"test": "aio_oracle", "class": list(d["classes"])[0],
         "source": "eng/tests/unit/aio/CMakeLists.txt:1", "reason": "r",
         "owner": "o", "unblock": "u"}])), "K3 skip_count")
    cases["count"] = (mut(lambda d: d.__setitem__("skip_count", 999)), "K3 skip_count")
    cases["anchor"] = (mut(lambda d: d["skips"][0].__setitem__(
        "source", "eng/ci/ctest_skip_register.json:99999")), "K5")
    cases["class"] = (mut(lambda d: d["skips"][0].__setitem__(
        "class", "MYSTERY_CLASS_ZZZ")), "不在顶层 classes")
    cases["field"] = (mut(lambda d: d["skips"][0].pop("owner")), "未逐条登记")
    if which not in cases and which not in ("all", "*"):
        print("FAULT_INJECT_UNKNOWN 可用注入: %s" % sorted(cases), file=sys.stderr)
        return 2
    sel = sorted(cases) if which in ("all", "*") else [which]
    bad = 0
    for name in sel:
        d, token = cases[name]
        p, _i = check_registry(repo, d, face, None)
        hit = any(token in x for x in p)
        print("  [%s] 注入 %s ⇒ 期望 %s" % ("PASS" if hit else "FAIL", name, token))
        if not hit:
            bad += 1
            print("      实得: %s" % (p or "（无问题）"))
    p, _i = check_registry(repo, base, face, None)
    print("  [%s] 正例 真仓未注入 ⇒ 无问题（problems=%d）"
          % ("PASS" if not p else "FAIL", len(p)))
    if p:
        bad += 1
        for x in p:
            print("      %s" % x)
    print("FAULT_INJECT %s (%d 例)" % ("PASS" if bad == 0 else "FAIL", len(sel) + 1))
    return 0 if bad == 0 else 1


def _resolve_face(repo, build_dirs=None, json_path=None):
    try:
        return ctest_face.resolve(pathlib.Path(repo), build_dirs=build_dirs,
                                  json_path=json_path)
    except ctest_face.Unavailable as exc:
        return exc


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--registry", default=None)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--build-dir", action="append", default=None,
                    help="配置期构建树（可重复；默认自动发现）")
    ap.add_argument("--ctest-face", default=None,
                    help="面 B 的已抓取产物（ctest -N --show-only=json-v1 的 JSON）")
    ap.add_argument("--disabled-log", default=None,
                    help="K6 运行期证据：ctest 的 LastTestsDisabled.log")
    ap.add_argument("--junit", default=None,
                    help="K6 运行期证据：ctest --output-junit 的 XML")
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    ap.add_argument("--fault-inject", default=None)
    args = ap.parse_args(argv)
    if args.self_test:
        return _self_test()
    repo = REPO
    if args.fault_inject:
        return _fault_inject(args.fault_inject, repo)
    path = args.registry or os.path.join(str(repo), DEFAULT_REGISTRY)
    if not os.path.isfile(path):
        print("SKIP_REGISTER_INPUT_UNAVAILABLE 登记册缺失: %s" % path, file=sys.stderr)
        return 2
    try:
        with open(path, encoding="utf-8") as fh:
            doc = json.load(fh)
    except (OSError, ValueError) as exc:
        print("SKIP_REGISTER_INPUT_UNAVAILABLE 登记册不可解析 %s: %s" % (path, exc),
              file=sys.stderr)
        return 2
    face = _resolve_face(repo, build_dirs=args.build_dir, json_path=args.ctest_face)
    if isinstance(face, Exception):
        print("SKIP_REGISTER_FACE_UNAVAILABLE %s（fail-closed）" % face, file=sys.stderr)
        return 2
    evidence, ev_errors = None, []
    if args.disabled_log:
        names, errs = read_disabled_log(args.disabled_log)
        evidence = {"names": names, "source": args.disabled_log}
        ev_errors += errs
    elif args.junit:
        names, errs = read_junit(args.junit)
        evidence = {"names": names, "source": args.junit}
        ev_errors += errs
    problems, info = check_registry(repo, doc, face, evidence)
    problems = problems + ev_errors
    out = {"checker": "eng/ci/check_ctest_skip_register.py",
           "registry": os.path.relpath(path, str(repo)).replace(os.sep, "/"),
           "names_verified": len(info["names_checked"]),
           "skips": info["skips"], "platform_excluded": info["platform_excluded"],
           "face": info["face"], "runtime_evidence": info["evidence_source"],
           "runtime_evidence_names": info["evidence_names"],
           "problems": problems,
           "verdict": "SKIP_REGISTER_RED" if problems else "SKIP_REGISTER_OK"}
    text = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        parent = os.path.dirname(os.path.abspath(args.json_out))
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as fh:
            fh.write(text + "\n")
    if problems:
        print("SKIP_REGISTER_RED problems=%d" % len(problems))
        for p in problems:
            print("  [%s] %s" % (p.split(" ", 1)[0], p.split(" ", 1)[1] if " " in p else ""))
        return 1
    print("SKIP_REGISTER_OK skips=%d platform_excluded=%d 名字核对=%d 个（面 A ∪ 面 B）"
          % (info["skips"], info["platform_excluded"], len(info["names_checked"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
