#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-NAMING-SURFACE —— 显示名 / 机器契约保留面 判据门。

权威依据
  - ENGINEERING_SPEC.md §15「显示名与机器契约保留面」：显示名 = ACSD /
    Astro Celestial Sphere Database；机器契约保留面按类枚举；判定规则一句话。
    本节是**唯一源**，本门与台账不得复述该定义之外的判据。
  - ENGINEERING_SPEC.md §10：唯一检查注册表 / 能红能绿 / 可执行负例面
    （--self-test）/ fail-closed / 锚存活 / 注册表双向一致。
  - docs/ci/01_CHECKS.md §1 注册表原则。

判据（任一 R 违规 ⇒ exit 1；输入不可用 ⇒ exit 2，fail-closed）
  R1 保留类闭包   扫描族（AstroCS / astrocs / ASTROCS，与 git grep -w 同
                  语义）的**每一处命中**必须至少落在台账某个保留类内。落在
                  类外 ⇒ 该处是「显示名未改」= 漏改，逐处点名判红。
  R2 保留类非空   每个保留类必须至少贡献一个真实命中（否则是僵尸类 / 判据
                  退化），并打印该类覆盖的文件数与正则形态数。
  R3 定义面棘轮   本门自身定义面（台账 + 检查器）的命中数不得超过台账登记的
                  max_hits（只减不增），防止借「定义面」夹带未登记的形态。
  R4 台账结构     台账 schema / 类字段 / 正则必须可编译、path_globs 必须能
                  命中至少一个真实命中（失效的类判红，防静默退化）。
  R5 变更集自洽   台账必须记录本次「按显示名改写」的处置面（改了什么、按什么
                  判据说它属于类外）；变更集不得覆盖任何 immutable 保留类
                  （声明改过不可改的面 = 自相矛盾）。

为什么另立本门（S1 第 17 条）
  git grep -w AstroCS 全仓 295 处，而「哪些是机器契约必须保留、哪些是漏改」
  全仓没有一处定义。改名会打断 include/schema/CI 匹配/锚，硬改会破坏机器面；
  不改则显示名不统一。本门把判据显式登记为**类级闭包**：命中要么在保留类内，
  要么就是漏改——没有第三种状态。

可执行负例面（--self-test，全部内存 fixture，零副作用，14 例）
  N0  正例（全部命中在类内）⇒ PASS
  N1  文档正文写类外 `AstroCS` ⇒ R1 红
  N2  保留类正则改窄（workflow 名只认 Linux）⇒ 原本绿的 Windows 名变红
  N3  workflow 名字面量（类内真命中）⇒ 绿
  N4  CI 候选包成员名（类内真命中）⇒ 绿
  N5  台账缺失 ⇒ fail-closed exit 2
  N6  台账 line_regex 非法 ⇒ fail-closed exit 2
  N7  定义面棘轮超限 ⇒ R3 红
  N8  类 families 不含该扫描族 ⇒ 不覆盖（跨族不误判）⇒ R1 红
  N9  类 path_globs 失效 ⇒ 原本绿的命中变红（R4 亦红）
  N10 实验/**（科学实验单元）正文里的类外命中 ⇒ R1 红
  N11 保留类零命中（僵尸类）⇒ R2 红
  N12 变更集覆盖 immutable 保留类 ⇒ R5 红
  N13 类 path_globs 静默失效 ⇒ R4 红

用法
  python3 eng/ci/check_naming_surface.py                        # 判据（CI 检查面）
  python3 eng/ci/check_naming_surface.py --json-out F           # 落证据 JSON
  python3 eng/ci/check_naming_surface.py --list-outside N       # 打印类外命中
  python3 eng/ci/check_naming_surface.py --self-test            # 负例自检 N0..N13
exit 0 = PASS；1 = 判据违规；2 = 输入不可用（fail-closed）。

只读；仅 stdlib。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
LEDGER_REL = "eng/ci/ledgers/naming_surface.json"
CHECK_ID = "CHK-NAMING-SURFACE"


# ─────────────────────────── 扫描面（与 git grep -w 同语义） ───────────────────────────

WORD_RE_CACHE: dict = {}


def word_re(word: str) -> "re.Pattern":
    """git grep -w 的语义：命中词两侧不得是 word 字符（[A-Za-z0-9_]）。"""
    r = WORD_RE_CACHE.get(word)
    if r is None:
        r = re.compile(r"(?<![A-Za-z0-9_])" + re.escape(word) + r"(?![A-Za-z0-9_])")
        WORD_RE_CACHE[word] = r
    return r


def scan_git(root: pathlib.Path, words) -> list:
    """扫描**已入库文本**：git grep -z -l -w <word> 取文件集，再逐行按同一 -w 语义匹配。

    为什么先用 -l 拿文件集而不是直接解析 git grep -n 的输出：仓库路径含空格，
    且 git grep -z 的 <path>\0<lineno>\0<text> 记录之间只有 text 末尾的换行分隔
    （后一记录的 path 粘在 text 之后），直接 split("\0") 分组会错位。用 -z -l 拿
    NUL 分隔的路径表是无歧义的，且「哪些文件算命中」仍由 git grep -w 本身决定。

    run/ 等 gitignore 面天然不在扫描面内（git grep 只扫已入库文本）。
    返回 [{"family","word","path","lineno","line"}]。
    """
    hits: list = []
    for fam in words:
        word = fam["word"]
        cmd = ["git", "-c", "core.quotepath=false", "grep", "-z", "-l", "-I", "-w", word, "--"]
        proc = subprocess.run(cmd, cwd=str(root), capture_output=True)
        if proc.returncode == 128:
            raise RuntimeError(f"git grep 不可用（rc=128）：{proc.stderr.decode(errors='replace')[:200]}")
        if proc.returncode not in (0, 1):
            raise RuntimeError(f"git grep rc={proc.returncode}：{proc.stderr.decode(errors='replace')[:200]}")
        blob = proc.stdout.decode("utf-8", errors="replace")
        paths = [p for p in blob.split("\0") if p]
        rx = word_re(word)
        for rel in paths:
            fp = root / rel
            try:
                text = fp.read_text(encoding="utf-8", errors="replace")
            except OSError as exc:
                raise RuntimeError(f"命中文件不可读（fail-closed）：{rel}：{exc}")
            for idx, line in enumerate(text.splitlines(), start=1):
                if rx.search(line):
                    hits.append({"family": fam["id"], "word": word, "path": rel,
                                 "lineno": idx, "line": line})
    return hits


def scan_fixture(files: dict, words) -> list:
    """自检用：对内存 fixture（{path: text}）做与 scan_git 同语义的扫描。"""
    hits: list = []
    for path in sorted(files):
        text = files[path]
        for idx, line in enumerate(text.splitlines(), start=1):
            for fam in words:
                if word_re(fam["word"]).search(line):
                    hits.append({"family": fam["id"], "word": fam["word"],
                                 "path": path, "lineno": idx, "line": line})
    return hits


# ─────────────────────────── 类匹配 ───────────────────────────

def glob_re(pat: str) -> "re.Pattern":
    out, i = [], 0
    while i < len(pat):
        c = pat[i]
        if c == "*":
            if pat[i:i + 2] == "**":
                out.append(".*")
                i += 2
                continue
            out.append("[^/]*")
            i += 1
            continue
        if c == "?":
            out.append("[^/]")
            i += 1
            continue
        out.append(re.escape(c))
        i += 1
    return re.compile("^" + "".join(out) + "$")


def compile_classes(classes: list) -> list:
    """编译台账的保留类；正则/glob 非法 ⇒ RuntimeError（fail-closed）。"""
    out = []
    for cl in classes:
        cid = cl.get("id")
        if not cid:
            raise RuntimeError("保留类缺 id")
        fams = cl.get("families")
        if not isinstance(fams, list) or not fams:
            raise RuntimeError(f"{cid}: families 必须是非空数组")
        try:
            lres = [re.compile(p) for p in cl.get("line_regexes", [])]
        except re.error as exc:
            raise RuntimeError(f"{cid}: line_regex 非法：{exc}")
        globs = [glob_re(g) for g in cl.get("path_globs", ["**"])]
        out.append({"id": cid, "title": cl.get("title", ""), "criterion": cl.get("criterion", ""),
                    "families": set(fams), "line_res": lres, "path_res": globs, "raw": cl})
    return out


def covering_classes(hit: dict, classes: list) -> list:
    got = []
    for cl in classes:
        if hit["family"] not in cl["families"]:
            continue
        if not any(g.match(hit["path"]) for g in cl["path_res"]):
            continue
        if cl["line_res"] and not any(r.search(hit["line"]) for r in cl["line_res"]):
            continue
        # 类若既无 line_regex 也无 path_glob，等于万能类 —— 由 R4 拦
        if not cl["line_res"] and cl["raw"].get("path_globs", ["**"]) == ["**"]:
            continue
        got.append(cl["id"])
    return got


def _def_path_exists(rel: str) -> bool:
    return (REPO / rel).is_file()


# ─────────────────────────── 判据 ───────────────────────────

def evaluate(hits: list, classes: list, ledger: dict, live: bool = True) -> dict:
    """返回 {violations:[...], classes_cover:{}, outside:[...], stats:{}}。

    live=False（自检 fixture 面）时关闭「类必须非空 / glob 必须命中」两项存量自检
    ——fixture 只验证判据逻辑，不代表全仓存量。
    """
    violations: list = []
    cover: dict = {cid: {"hits": 0, "files": set(), "forms": set()} for cid in [c["id"] for c in classes]}
    outside: list = []

    # 定义面（台账 + 检查器自身）不参与 R1：那里的命中是「被定义的形态本身」，
    # 由 R3 棘轮单独看住（只减不增），防止借定义面夹带未登记的形态。
    ratchet0 = ledger.get("definition_surface_ratchet") or {}
    def_paths = set(ratchet0.get("paths") or [])
    def_hits: list = []

    for h in hits:
        if h["path"] in def_paths:
            def_hits.append(h)
            continue
        got = covering_classes(h, classes)
        if not got:
            outside.append(h)
            continue
        for cid in got:
            cover[cid]["hits"] += 1
            cover[cid]["files"].add(h["path"])
            cl = next(c for c in classes if c["id"] == cid)
            for r in cl["line_res"]:
                if r.search(h["line"]):
                    cover[cid]["forms"].add(r.pattern)
            if not cl["line_res"]:
                cover[cid]["forms"].add("<path-only>")

    if outside:
        violations.append({"rule": "R1", "code": "OUTSIDE_PRESERVED_CLASSES",
                           "detail": f"{len(outside)} 处命中不落在任何保留类内（= 显示名漏改）",
                           "hits": outside})

    # R2 保留类非空（存量自检，仅真实扫描面）
    for cid, agg in cover.items():
        if live and agg["hits"] == 0:
            violations.append({"rule": "R2", "code": "DEAD_CLASS", "detail": f"保留类 {cid} 零命中（僵尸类）"})

    # R3 定义面棘轮（只减不增）
    maxhits = ratchet0.get("max_hits")
    if def_paths:
        if maxhits is None:
            violations.append({"rule": "R3", "code": "RATCHET_UNSET", "detail": "定义面棘轮缺 max_hits"})
        elif len(def_hits) > int(maxhits):
            violations.append({"rule": "R3", "code": "DEFINITION_SURFACE_GROWN",
                               "detail": f"定义面命中 {len(def_hits)} > 登记上限 {maxhits}（只减不增）",
                               "hits": def_hits})
        for p in sorted(def_paths):
            if not (pathlib.Path(ledger.get("_root", ".")) / p).is_file() and not _def_path_exists(p):
                violations.append({"rule": "R3", "code": "DEFINITION_SURFACE_ANCHOR_STALE",
                                   "detail": f"定义面路径不存在（锚存活）：{p}"})

    # R4 每个声明了 path_globs 的类必须真的命中同族命中（防 glob 失效静默退化）
    for cl in classes:
        if not (live and cl["raw"].get("path_globs")):
            continue
        same_family = [h for h in hits if h["family"] in cl["families"]]
        if same_family and not any(any(g.match(h["path"]) for g in cl["path_res"]) for h in same_family):
            violations.append({"rule": "R4", "code": "CLASS_GLOB_MATCHES_NOTHING",
                               "detail": f"保留类 {cl['id']} 的 path_globs 不命中任何同族命中"})

    # R5 变更集自洽：不得覆盖 immutable 保留类；每条必须有判据
    change = ledger.get("display_name_change_set") or {}
    immutable = [c for c in classes if c["raw"].get("immutable")]
    for entry in change.get("entries", []):
        if not entry.get("criterion"):
            violations.append({"rule": "R5", "code": "CHANGE_SET_NO_CRITERION",
                               "detail": f"变更集条目缺 criterion：{entry.get('paths')}"})
        for p in entry.get("paths", []):
            gr = glob_re(p)
            for h in hits:
                if not gr.match(h["path"]):
                    continue
                got = [c["id"] for c in immutable if c["id"] in covering_classes(h, classes)]
                if got:
                    violations.append({"rule": "R5", "code": "CHANGE_SET_OVERLAPS_IMMUTABLE",
                                       "detail": f"变更集路径 {p} 命中 {h['path']}:{h['lineno']} "
                                                 f"落在不可改类 {got}"})
                    break

    stats = {
        "hits_total": len(hits),
        "hits_definition_surface": len(def_hits),
        "hits_outside": len(outside),
        "files_total": len({h["path"] for h in hits}),
        "files_outside": len({h["path"] for h in outside}),
        "classes": {cid: {"hits": a["hits"], "files": len(a["files"]),
                          "forms": len(a["forms"])} for cid, a in cover.items()},
    }
    return {"violations": violations, "cover": cover, "outside": outside, "stats": stats}


def load_ledger(root: pathlib.Path, rel: str = LEDGER_REL):
    p = root / rel
    if not p.is_file():
        raise RuntimeError(f"台账不存在（fail-closed）：{rel}")
    try:
        doc = json.loads(p.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"台账非法 JSON：{exc}")
    if doc.get("ledger_schema") != "astrocs.ci-ledger/v1":
        raise RuntimeError("台账 ledger_schema 必须是 astrocs.ci-ledger/v1")
    scan = doc.get("scan") or {}
    fams = scan.get("families")
    if not isinstance(fams, list) or not fams:
        raise RuntimeError("台账 scan.families 必须是非空数组")
    for f in fams:
        if not f.get("id") or not f.get("word"):
            raise RuntimeError("scan.families 条目必须有 id 与 word")
    cls = doc.get("classes")
    if not isinstance(cls, list) or not cls:
        raise RuntimeError("台账 classes 必须是非空数组")
    return doc


def run(root: pathlib.Path, ledger_rel: str, json_out=None, list_outside=0,
        hits_override=None) -> int:
    try:
        ledger = load_ledger(root, ledger_rel)
        classes = compile_classes(ledger["classes"])
    except RuntimeError as exc:
        print(f"{CHECK_ID}_FAILCLOSED: {exc}")
        return 2

    fams = ledger["scan"]["families"]
    if hits_override is None:
        try:
            hits = scan_git(root, fams)
        except RuntimeError as exc:
            print(f"{CHECK_ID}_FAILCLOSED: {exc}")
            return 2
    else:
        hits = scan_fixture(hits_override, fams)

    rep = evaluate(hits, classes, ledger)
    out = {"check": CHECK_ID, "ledger": ledger_rel, "stats": rep["stats"],
           "violations": [{k: v for k, v in vv.items() if k != "hits"} for vv in rep["violations"]]}
    if json_out:
        p = pathlib.Path(json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        full = dict(out)
        full["outside_hits"] = rep["outside"]
        full["class_cover"] = {cid: {"hits": a["hits"], "files": sorted(a["files"]),
                                     "forms": sorted(a["forms"])} for cid, a in rep["cover"].items()}
        p.write_text(json.dumps(full, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    for cid, agg in sorted(rep["stats"]["classes"].items()):
        print(f"  [class] {cid:<38} hits={agg['hits']:<5} files={agg['files']:<4} forms={agg['forms']}")
    print(f"  命中 {rep['stats']['hits_total']} 处 / {rep['stats']['files_total']} 文件；"
          f"定义面 {rep['stats']['hits_definition_surface']} 处；"
          f"类外 {rep['stats']['hits_outside']} 处 / {rep['stats']['files_outside']} 文件")

    if rep["violations"]:
        for v in rep["violations"]:
            print(f"  [{v['rule']}] {v['code']}: {v['detail']}")
        shown = list_outside or 20
        for h in rep["outside"][:shown]:
            print(f"    OUTSIDE {h['path']}:{h['lineno']} [{h['family']}] {h['line'][:120]}")
        print(f"{CHECK_ID}_VIOLATION ({len(rep['violations'])})")
        return 1
    print(f"{CHECK_ID}_PASS: 全部命中落在保留类内")
    return 0


# ─────────────────────────── 自检 ───────────────────────────

BASE_LEDGER = {
    "ledger_schema": "astrocs.ci-ledger/v1",
    "scan": {"families": [{"id": "U", "word": "AstroCS"},
                          {"id": "L", "word": "astrocs"}]},
    "classes": [
        {"id": "U-CI-WORKFLOW-NAME", "families": ["U"],
         "line_regexes": ["AstroCS (?:Linux|Windows) CI", "AstroCS Fatduck Validation"]},
        {"id": "U-CI-CANDIDATE-MEMBER", "families": ["U"],
         "line_regexes": [r"AstroCS-candidate\.zip"]},
        {"id": "U-EVIDENCE", "families": ["U"], "path_globs": ["artifacts/evidence/**"],
         "immutable": True},
        {"id": "L-SYMBOL", "families": ["L"],
         "line_regexes": [r"astrocs::", r"#\s*include\s*[<\"]astrocs/"]},
    ],
    "definition_surface_ratchet": {"paths": [], "max_hits": 0},
    "display_name_change_set": {"entries": [
        {"paths": ["docs/**", "实验/**"], "criterion": "正文显示名 ⇒ ACSD"}]},
}


def _ledger(**kw):
    d = json.loads(json.dumps(BASE_LEDGER))
    d.update(kw)
    return d


def _run_fixture(files, ledger, live=False):
    classes = compile_classes(ledger["classes"])
    hits = scan_fixture(files, ledger["scan"]["families"])
    return evaluate(hits, classes, ledger, live=live)


def self_test() -> int:
    cases: list = []

    def case(name, ok, expect_rule=None, got=None):
        cases.append((name, bool(ok), expect_rule, got))

    green = {"docs/a.md": "workflow name: AstroCS Linux CI\n",
             "eng/x.py": 'Z = "AstroCS-candidate.zip"\n',
             "artifacts/evidence/e.md": "旧的 AstroCS 发布状态\n"}

    # N0 正例
    r = _run_fixture(green, BASE_LEDGER)
    case("N0_green_all_covered", not r["violations"], got=[v["code"] for v in r["violations"]])

    # N1 正文类外命中 ⇒ R1
    r = _run_fixture({"docs/a.md": "本项目 AstroCS 现行做法\n"}, BASE_LEDGER)
    case("N1_doc_prose_outside", any(v["rule"] == "R1" for v in r["violations"]), "R1")

    # N2 保留类正则改窄 ⇒ 原本绿变红
    narrow = _ledger(classes=[
        {"id": "U-CI-WORKFLOW-NAME", "families": ["U"],
         "line_regexes": ["AstroCS Linux CI"]},
        BASE_LEDGER["classes"][1], BASE_LEDGER["classes"][2], BASE_LEDGER["classes"][3]])
    r = _run_fixture({"wf.yml": "name: AstroCS Windows CI\n"}, narrow)
    case("N2_narrowed_regex_turns_red", any(v["rule"] == "R1" for v in r["violations"]), "R1")

    # N3 workflow 名 = 类内真命中
    r = _run_fixture({"wf.yml": "name: AstroCS Windows CI\n"}, BASE_LEDGER)
    case("N3_workflow_name_covered", not r["violations"], got=[v["code"] for v in r["violations"]])

    # N4 候选包成员名 = 类内真命中
    r = _run_fixture({"ci.py": 'CANDIDATE_ZIP_MEMBER = "AstroCS-candidate.zip"\n'}, BASE_LEDGER)
    case("N4_candidate_member_covered", not r["violations"], got=[v["code"] for v in r["violations"]])

    # N5 台账缺失 ⇒ fail-closed
    rc = run(pathlib.Path("/nonexistent-repo-root"), "eng/ci/ledgers/naming_surface.json")
    case("N5_missing_ledger_failclosed", rc == 2, got=rc)

    # N6 非法正则 ⇒ fail-closed
    bad = _ledger(classes=[{"id": "BAD", "families": ["U"], "line_regexes": ["AstroCS ("]}])
    try:
        compile_classes(bad["classes"])
        case("N6_bad_regex_failclosed", False, got="no-raise")
    except RuntimeError:
        case("N6_bad_regex_failclosed", True)

    # N7 定义面棘轮超限 ⇒ R3
    rat = _ledger(definition_surface_ratchet={"paths": ["docs/a.md"], "max_hits": 0},
                  classes=BASE_LEDGER["classes"] + [
                      {"id": "U-SELF", "families": ["U"], "path_globs": ["docs/a.md"]}])
    r = _run_fixture({"docs/a.md": "AstroCS 定义面\n"}, rat)
    case("N7_ratchet_grown", any(v["rule"] == "R3" for v in r["violations"]), "R3")

    # N8 跨族不误判：类只声明 L，U 命中不覆盖
    cross = _ledger(classes=[{"id": "L-ONLY", "families": ["L"], "line_regexes": ["."]}])
    r = _run_fixture({"x.py": 'A = "AstroCS-candidate.zip"\n'}, cross)
    case("N8_family_scoped", any(v["rule"] == "R1" for v in r["violations"]), "R1")

    # N9 类 path_globs 失效 ⇒ 覆盖消失
    dead = _ledger(classes=[
        {"id": "U-CI-WORKFLOW-NAME", "families": ["U"],
         "line_regexes": ["AstroCS (?:Linux|Windows) CI"]},
        {"id": "U-CI-CANDIDATE-MEMBER", "families": ["U"],
         "line_regexes": [r"AstroCS-candidate\.zip"]},
        {"id": "U-EVIDENCE", "families": ["U"], "path_globs": ["artifacts/nowhere/**"]},
        BASE_LEDGER["classes"][3]])
    r = _run_fixture({"artifacts/evidence/e.md": "旧的 AstroCS 发布状态\n"}, dead)
    case("N9_dead_glob_turns_red", any(v["rule"] == "R1" for v in r["violations"]), "R1")

    # N10 实验/** 科学单元正文类外命中 ⇒ R1
    r = _run_fixture({"实验/absolute-snr/README.md": "不运行任何 AstroCS 可执行文件\n"}, BASE_LEDGER)
    case("N10_experiment_prose_outside", any(v["rule"] == "R1" for v in r["violations"]), "R1")

    # N11 僵尸类 ⇒ R2
    zombie = _ledger(classes=BASE_LEDGER["classes"] + [
        {"id": "U-ZOMBIE", "families": ["U"], "line_regexes": [r"AstroCS-never-appears-xyz"]}])
    r = _run_fixture(green, zombie, live=True)
    case("N11_dead_class", any(v["rule"] == "R2" for v in r["violations"]), "R2")

    # N12 变更集与保留类重叠 ⇒ R5
    overlap = _ledger(display_name_change_set={"entries": [
        {"paths": ["artifacts/evidence/**"], "criterion": "不该在这里"}]})
    r = _run_fixture(green, overlap, live=True)
    case("N12_change_set_overlap", any(v["rule"] == "R5" for v in r["violations"]), "R5")

    # N13 类 path_globs 静默失效（命中被别类覆盖，只有 R4 能发现）⇒ R4
    stale = _ledger(classes=BASE_LEDGER["classes"] + [
        {"id": "U-STALE-GLOB", "families": ["U"], "path_globs": ["docs/nowhere/**"]}])
    r = _run_fixture(green, stale, live=True)
    case("N13_stale_glob_detected", any(v["rule"] == "R4" for v in r["violations"]), "R4")

    bad = [c for c in cases if not c[1]]
    for name, ok, rule, got in cases:
        print(f"  {'PASS' if ok else 'FAIL'} {name}" + ("" if ok else f"   expect={rule} got={got}"))
    print(f"{CHECK_ID}_SELFTEST {'PASS' if not bad else 'FAIL'}: {len(cases) - len(bad)}/{len(cases)}")
    return 0 if not bad else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=f"{CHECK_ID} 显示名/机器契约保留面门")
    ap.add_argument("--root", default=None, help="仓库根（默认按脚本位置推断）")
    ap.add_argument("--ledger", default=LEDGER_REL)
    ap.add_argument("--json-out", default=None, dest="json_out")
    ap.add_argument("--list-outside", type=int, default=0, dest="list_outside",
                    help="打印类外命中的条数（默认 20）")
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    root = pathlib.Path(args.root).resolve() if args.root else REPO
    return run(root, args.ledger, args.json_out, args.list_outside)


if __name__ == "__main__":
    sys.exit(main())
