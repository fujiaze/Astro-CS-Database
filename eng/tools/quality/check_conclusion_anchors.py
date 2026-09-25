#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_conclusion_anchors.py — 结论必须带依据锚（「无锚的结论字面量进门即红」）。

存在理由（独立审查《一页纸》S1-2「红灯被改写成绿灯」）：
本仓出现过两类「把结论写死」——(a) 生成器把结论字面量写进产物；(b) 判定脚本把
结论当常量写在源码里。本门把二者变成机器判据。

判据：
  R1 生成页锚有效：带 @@ANCHOR@@ 的文件，锚声明的生成器/证据源必须存在，
     且 tool-sha256 / source-sha256 / body-sha256 与现行一致（正文被手改即红）；
  R2 无锚结论字面量：声明面（--surface，默认 docs/modules/registry）内出现结论词，
     该行必须 (i) 已写 NOT_VERIFIED，或 (ii) 带证据锚（path:line / docs|eng|lib|run|
     artifacts 路径 / § 条款 / @sha），或 (iii) 位于生成锚块内 —— 否则判红；
  R3 源码写死的绿结论：产物生成/判定脚本里，字典字面量的结论字段
     （verdict/result/status/review_status/...）取常量绿值（PASS/VERIFIED/OK/通过/已验）
     且该表达式无条件、不在自检/fixture 函数内、且未标 \x60# conclusion-anchor:\x60 理由 ⇒ 判红；
  R4 用 sed/sed -i 把 FAIL 改写成 PASS 的 shell 行 ⇒ 判红（机器改写红灯即假绿）。

例外标记（必须写理由，逐行）：
  \x60# conclusion-anchor: <为什么这个字面量不是结论>\x60 放在该行或上一行。

用法：
  python3 eng/tools/quality/check_conclusion_anchors.py [--root .] [--json-out F]
  python3 eng/tools/quality/check_conclusion_anchors.py --self-test
exit 0 = 无违规；1 = 有违规；2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import ast
import hashlib
import json
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gen_module_readmes as G                                            # noqa: E402

ANCHOR_MARK = G.ANCHOR_MARK
ANCHOR_RE = G.ANCHOR_RE
DEFAULT_SURFACE = "docs/modules/registry"

# R2 结论词（冻结表）：只收「断言某动作已被验证/已通过」的词，避免误伤普通叙述。
CONCLUSION_TOKENS = (
    "等价已验",
    "验收冻结",
    "全部通过",
    "全部 PASS",
    "已验证",
    "验证通过",
)
CONCLUSION_RES = (
    re.compile(r"\d+\s*/\s*\d+\s*PASS"),
    re.compile(r"\bresult\s*=\s*[" + '"' + r"]?PASS"),
    re.compile(r"\bverdict\s*[:=]\s*[" + '"' + r"]?PASS"),
)
# 证据锚（同一行内即可）：path:line、仓内路径、§ 条款、@sha
EVIDENCE_RE = re.compile(
    r"(\b[\w./\-]+\.(?:py|sh|cpp|c|h|hpp|json|md|csv|yaml|yml|txt|cmake|inc|in):\d+)"
    r"|(\b(?:docs|eng|lib|run|artifacts|testdata|工程控制|实验)/[\w./\-]+)"
    r"|(§\s*[\w.\-]+)"
    r"|(@[0-9a-f]{7,40})")
NOT_VERIFIED = "NOT_VERIFIED"

# R3 结论字段名与绿值
# 只收「结论字段」名。刻意不含裸 "status"/"state"：那是数据面常见字段
# （帧状态/节点状态），收进来会把夹具与数据模型判成假绿。
CONTROL_KEYS = {"verdict", "result", "review_status", "test_status", "src_status",
                "evidence_status", "gate_status", "ship_status", "conclusion",
                "verified", "determinism"}
GREEN_VALUES = {"PASS", "VERIFIED", "OK", "TRUE", "通过", "已验", "等价已验", "YES"}
SRC_ROOTS = ("eng/tools", "eng/ci", "eng/packaging")
# R3 面：只查「产物生成者」（会把结论写进文件/JSON/CSV 的脚本）+ 非测试面。
# 测试与自检里的 {verdict: PASS} 是夹具数据，不是产物结论 —— 按面排除，不按例外豁免。
_Q = '"' + "'"
PRODUCER_RE = re.compile(r"write_text\(|json\.dump\(|csv\.(?:Dict)?Writer\(|"
                         r"open\([^)]*[" + _Q + r"]w")
ANCHOR_MARK_COMMENT = "# conclusion-anchor:"
SKIP_FUNC_HINTS = ("self_test", "selftest", "fixture", "expect", "golden",
                  "sample", "demo", "oracle", "_valid", "_row")
R4_RE = re.compile(r"\bsed\b[^\n]*FAIL[^\n]*PASS")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def _rel(root: pathlib.Path, p: pathlib.Path) -> str:
    try:
        return p.resolve().relative_to(root).as_posix()
    except ValueError:
        return str(p)


def check_anchors(root: pathlib.Path, files: list):
    """R1：锚有效性。返回 (违规, 陈旧)。

    硬红 = 锚结构非法（生成器/证据源不存在）或正文被手改（body-sha256 不符）——
    这正是「下游手工修改被静默回退」的反面判据。
    陈旧 = 生成器/证据源已变更而页未再生成：默认只报不红（并发编辑期避免噪声红），
    由 --strict-freshness 升级为红。
    """
    out = []
    stale = []
    for p in files:
        try:
            text = p.read_text(encoding="utf-8")
        except OSError as exc:
            out.append({"rule": "R1", "file": _rel(root, p), "detail": "unreadable: %s" % exc})
            continue
        fields, plain = G.strip_anchor(text)
        if not fields:
            continue
        rel = _rel(root, p)
        tool = fields.get("tool", "")
        src = fields.get("source", "")
        if not tool or not (root / tool).is_file():
            out.append({"rule": "R1", "file": rel,
                        "detail": "锚声明的生成器不存在: %r" % tool})
            continue
        if not src or not (root / src).is_file():
            out.append({"rule": "R1", "file": rel, "detail": "锚声明的证据源不存在: %r" % src})
            continue
        if fields.get("tool-sha256") != sha256_file(root / tool):
            stale.append({"rule": "R1-STALE", "file": rel,
                          "detail": "生成器已变更，页陈旧（tool-sha256 不符）⇒ 重跑 %s"
                                    % fields.get("regenerate", tool)})
        if fields.get("source-sha256") != sha256_file(root / src):
            stale.append({"rule": "R1-STALE", "file": rel,
                          "detail": "证据源已变更，页陈旧（source-sha256 不符）⇒ 重跑 %s"
                                    % fields.get("regenerate", tool)})
        if fields.get("body-sha256") != sha256_bytes(plain.encode("utf-8")):
            out.append({"rule": "R1", "file": rel,
                        "detail": "正文被改（body-sha256 不符）⇒ 生成页被手工改写；"
                                  "要人工维护请改为无锚的人工页并在索引撤 GENERATED"})
    return out, stale


def check_surface_literals(root: pathlib.Path, files: list):
    """R2：声明面内的无锚结论字面量。"""
    out = []
    for p in files:
        try:
            text = p.read_text(encoding="utf-8")
        except OSError:
            continue
        rel = _rel(root, p)
        anchor_span = None
        m = ANCHOR_RE.search(text)
        if m:
            anchor_span = (text.rfind("\n", 0, m.start()) + 1,
                           text.find("\n", m.end()))
        offset = 0
        for i, line in enumerate(text.splitlines(), 1):
            line_start = offset
            offset += len(line) + 1
            if anchor_span and anchor_span[0] <= line_start <= anchor_span[1]:
                continue
            toks = [t for t in CONCLUSION_TOKENS if t in line]
            toks += [r.pattern for r in CONCLUSION_RES if r.search(line)]
            if not toks:
                continue
            if NOT_VERIFIED in line or EVIDENCE_RE.search(line) \
                    or ANCHOR_MARK_COMMENT in line:
                continue
            out.append({"rule": "R2", "file": rel, "line": i,
                        "detail": "无锚结论字面量 %s: %s" % (toks, line.strip()[:120])})
    return out


def _iter_dict_literals(tree):
    for node in ast.walk(tree):
        if isinstance(node, ast.Dict):
            yield node


def _func_stack(tree):
    """节点 → 所属函数名（用于跳过自检/fixture 函数）。"""
    owner = {}
    for fn in ast.walk(tree):
        if isinstance(fn, (ast.FunctionDef, ast.AsyncFunctionDef)):
            for sub in ast.walk(fn):
                owner[id(sub)] = fn.name
    return owner


# 夹具容器名（模块级常量赋值目标）：里面的结论值是夹具数据，不是产物结论
FIXTURE_NAME_RE = re.compile(r"(?i)(selftest|self_test|fixture|expect|golden|case|"
                             r"negative|positive|good|bad|baseline)")


def _fixture_containers(tree):
    """返回「夹具容器」内的节点 id 集合（模块级常量名命中 FIXTURE_NAME_RE）。"""
    inside = set()
    for node in ast.walk(tree):
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            names = [t.id for t in targets if isinstance(t, ast.Name)]
            if not any(FIXTURE_NAME_RE.search(n) for n in names):
                continue
            value = node.value
            if value is None:
                continue
            for sub in ast.walk(value):
                inside.add(id(sub))
    return inside


def check_source_literals(root: pathlib.Path, files: list):
    """R3：产物/判定脚本里写死的绿结论常量。

    排除面（按性质，不按豁免）：已退役文件的 legacy 体、夹具容器内的数据、
    自检/fixture 函数体内的期望值。剩下的必须修（改成从证据源读）或逐行标
    `# conclusion-anchor: <理由>`。
    """
    out = []
    for p in files:
        try:
            text = p.read_text(encoding="utf-8")
            tree = ast.parse(text)
        except (OSError, SyntaxError):
            continue
        rel = _rel(root, p)
        if "_RETIRED:" in text or "_RETIRED =" in text:
            continue                       # 退役件：入口 exit 2，legacy 体不可达
        lines = text.splitlines()
        owner = _func_stack(tree)
        fixtures = _fixture_containers(tree)
        for node in _iter_dict_literals(tree):
            if id(node) in fixtures:
                continue
            for k, v in zip(node.keys, node.values):
                if not isinstance(k, ast.Constant) or not isinstance(k.value, str):
                    continue
                if k.value.lower() not in CONTROL_KEYS:
                    continue
                if not (isinstance(v, ast.Constant) and isinstance(v.value, str)):
                    continue
                if v.value.strip().upper() not in GREEN_VALUES:
                    continue
                fn = owner.get(id(node), "")
                if fn and any(h in fn for h in SKIP_FUNC_HINTS):
                    continue
                ln = v.lineno
                ref = min(getattr(node, "lineno", ln), ln)   # 字典起始行与值行取靠前者
                ctx = "\n".join(lines[max(0, ref - 8):ref])
                if ANCHOR_MARK_COMMENT in ctx:
                    continue
                out.append({"rule": "R3", "file": rel, "line": ln,
                            "detail": "结论字段 %r 写死为 %r（无条件、未标 %s）"
                                      % (k.value, v.value, ANCHOR_MARK_COMMENT)})
    return out


def check_sed_rewrite(root: pathlib.Path, files: list):
    """R4：shell 行里用 sed 把 FAIL 改写成 PASS。"""
    out = []
    for p in files:
        try:
            text = p.read_text(encoding="utf-8")
        except OSError:
            continue
        rel = _rel(root, p)
        for i, line in enumerate(text.splitlines(), 1):
            if R4_RE.search(line):
                out.append({"rule": "R4", "file": rel, "line": i,
                            "detail": "sed 改写 FAIL→PASS（红灯不得机器改写成绿灯）: %s"
                                      % line.strip()[:120]})
    return out


def collect(root: pathlib.Path, surface: str):
    surf = root / surface
    if not surf.is_dir():
        return None, "SURFACE_MISSING: %s（无声明面则本门无判据对象）" % surface
    docs = sorted(p for p in surf.glob("*.md") if p.is_file())
    srcs = []
    for r in SRC_ROOTS:
        d = root / r
        if d.is_dir():
            srcs += sorted(list(d.rglob("*.py")) + list(d.rglob("*.sh")))
    keep = []
    for p in srcs:
        if "__pycache__" in str(p) or p.resolve() == pathlib.Path(__file__).resolve():
            continue
        rel = _rel(root, p)
        if "/tests/" in rel or rel.startswith("eng/ci/tests/"):
            continue
        try:
            if not PRODUCER_RE.search(p.read_text(encoding="utf-8", errors="ignore")):
                continue
        except OSError:
            continue
        keep.append(p)
    return (docs, keep), None


def run(root: pathlib.Path, surface: str, strict_freshness: bool = False):
    files, err = collect(root, surface)
    if err:
        return None, err
    docs, srcs = files
    anchors, stale = check_anchors(root, docs)
    literals = check_surface_literals(root, docs)
    sourcelits = check_source_literals(root, srcs)
    seds = check_sed_rewrite(root, docs + srcs)
    if strict_freshness:
        anchors = anchors + stale
        stale = []
    report = {
        "tool": "eng/tools/quality/check_conclusion_anchors.py",
        "schema": "astrocs.conclusion-anchors/v1",
        "root": str(root),
        "surface": surface,
        "scanned": {"surface_docs": len(docs), "sources": len(srcs),
                    "anchored_docs": sum(1 for p in docs
                                         if ANCHOR_RE.search(p.read_text(encoding="utf-8",
                                                                         errors="ignore")))},
        "violations": anchors + literals + sourcelits + seds,
        "stale": stale,
        "producers_scanned": len(srcs),
    }
    report["verdict"] = "FAIL" if report["violations"] else "PASS"
    return report, None


# --------------------------------------------------------------------------- #
def self_test() -> int:
    """正/负例自检：删证据源 ⇒ 生成器非零退出且不写盘；无锚结论 ⇒ 判红。"""
    import shutil
    import subprocess
    import tempfile
    root = pathlib.Path(__file__).resolve().parents[3]
    fails = []
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="concl_anchor_"))
    try:
        # N1：registry 生成器 —— 删掉证据源必须非零退出，且不改写任何页
        sb = tmp / "modreadme"
        (sb / "eng/tools/quality").mkdir(parents=True)
        (sb / "lib/infrastructure/scheduler/src").mkdir(parents=True)
        (sb / "docs/modules/registry").mkdir(parents=True)
        shutil.copy2(root / G.GENERATOR_REL, sb / G.GENERATOR_REL)
        shutil.copy2(root / G.INDEX_REL, sb / G.INDEX_REL)
        for p in (root / G.OUT_DIR_REL).glob("*.md"):
            shutil.copy2(p, sb / G.OUT_DIR_REL / p.name)
        p1 = subprocess.run([sys.executable, G.GENERATOR_REL], cwd=sb,
                            capture_output=True, text=True, timeout=120)
        if p1.returncode != 2:
            fails.append("N1 删证据源后生成器未按 fail-closed 退出 2（rc=%d）" % p1.returncode)
        p2 = subprocess.run([sys.executable, G.GENERATOR_REL, "--check"], cwd=sb,
                            capture_output=True, text=True, timeout=120)
        before = {p.name: sha256_file(p) for p in (sb / G.OUT_DIR_REL).glob("*.md")}
        if before != {p.name: sha256_file(p) for p in (sb / G.OUT_DIR_REL).glob("*.md")}:
            fails.append("N1 生成器改写了在册页")
        # N2：cfitsio 生成器 —— 删掉证据源目录 ⇒ 非零退出且不写盘
        sb2 = tmp / "cfitsio"
        (sb2 / "eng/tools").mkdir(parents=True)
        (sb2 / "eng/cmake").mkdir(parents=True)
        shutil.copy2(root / "eng/tools/gen_cfitsio_list.py", sb2 / "eng/tools/")
        shutil.copy2(root / "eng/cmake/cfitsio_sources.cmake", sb2 / "eng/cmake/")
        h_before = sha256_file(sb2 / "eng/cmake/cfitsio_sources.cmake")
        p3 = subprocess.run([sys.executable, "eng/tools/gen_cfitsio_list.py"], cwd=sb2,
                            capture_output=True, text=True, timeout=120)
        h_after = sha256_file(sb2 / "eng/cmake/cfitsio_sources.cmake")
        if p3.returncode != 2:
            fails.append("N2 删证据源后 cfitsio 生成器 rc=%d（期望 2）" % p3.returncode)
        if h_before != h_after:
            fails.append("N2 cfitsio 生成器覆写了在册清单")
        # N3：无锚结论字面量必须判红；写 NOT_VERIFIED 或补证据锚必须转绿
        sb3 = tmp / "surface"
        (sb3 / DEFAULT_SURFACE).mkdir(parents=True)
        f = sb3 / DEFAULT_SURFACE / "x.md"
        f.write_text("确定性=固定顺序输出(1/N 等价已验)。\n", encoding="utf-8")
        rep, err = run(sb3, DEFAULT_SURFACE)
        if err or not rep["violations"]:
            fails.append("N3 无锚结论字面量未判红")
        f.write_text("确定性=NOT_VERIFIED（证据源 eng/ci/ledgers/x.json 无条目）。\n",
                     encoding="utf-8")
        rep, err = run(sb3, DEFAULT_SURFACE)
        if err or rep["violations"]:
            fails.append("N3' NOT_VERIFIED 未转绿: %s" % (rep or {}).get("violations"))
        f.write_text("确定性=固定顺序输出(1/N 等价已验；见 lib/foo.cpp:12)。\n", encoding="utf-8")
        rep, err = run(sb3, DEFAULT_SURFACE)
        if err or rep["violations"]:
            fails.append("N3'' 带证据锚未转绿: %s" % (rep or {}).get("violations"))
        # N4：正文被手改的生成页必须判红
        sb4 = tmp / "drift"
        (sb4 / DEFAULT_SURFACE).mkdir(parents=True)
        src = next(iter(sorted((root / DEFAULT_SURFACE).glob("*.md"))), None)
        sys.path.insert(0, str(root / "eng/tools/quality"))
        if src is not None:
            shutil.copy2(src, sb4 / DEFAULT_SURFACE / src.name)
            t = (sb4 / DEFAULT_SURFACE / src.name).read_text(encoding="utf-8")
            (sb4 / DEFAULT_SURFACE / src.name).write_text(t + "\n手改一行\n", encoding="utf-8")
            v = check_anchors(sb4, [sb4 / DEFAULT_SURFACE / src.name])
            if not v:
                fails.append("N4 正文手改的生成页未判红（body-sha256 未生效）")
        # N5：源码写死绿结论必须判红；加锚注释后转绿
        sb5 = tmp / "src"
        (sb5 / "eng/ci").mkdir(parents=True)
        sf = sb5 / "eng/ci" / "gen_x.py"
        sf.write_text('def main():\n    return {"verdict": "PASS"}\n', encoding="utf-8")
        v = check_source_literals(sb5, [sf])
        if not v:
            fails.append("N5 源码写死绿结论未判红")
        sf.write_text('def main():\n    # conclusion-anchor: 出厂默认值，非判定结论\n'
                      '    return {"verdict": "PASS"}\n', encoding="utf-8")
        if check_source_literals(sb5, [sf]):
            fails.append("N5' 带 conclusion-anchor 未转绿")
        # N6：sed FAIL→PASS 必须判红
        sh = sb5 / "eng/ci" / "x.sh"
        sh.write_text("sed -i 's|,FAIL,|,PASS,|' out.csv\n", encoding="utf-8")
        if not check_sed_rewrite(sb5, [sh]):
            fails.append("N6 sed FAIL→PASS 未判红")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    for f in fails:
        print("SELFTEST_FAIL " + f)
    ok = not fails
    print("CONCLUSION_ANCHORS_SELFTEST %s cases=9 failures=%d"
          % ("PASS" if ok else "FAIL", len(fails)))
    return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=str(pathlib.Path(__file__).resolve().parents[3]))
    ap.add_argument("--surface", default=DEFAULT_SURFACE)
    ap.add_argument("--json-out")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--strict-freshness", action="store_true",
                    help="把「生成页陈旧」也判红（默认只报不红）")
    args = ap.parse_args()
    if args.self_test:
        return self_test()
    root = pathlib.Path(args.root).resolve()
    report, err = run(root, args.surface, strict_freshness=args.strict_freshness)
    if err:
        print(err, file=sys.stderr)
        return 2
    if args.json_out:
        p = pathlib.Path(args.json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    for v in report.get("stale", []):
        print("  [R1-STALE] %s: %s" % (v["file"], v["detail"]))
    if report["violations"]:
        print("CONCLUSION_ANCHORS_FAIL n=%d（surface=%s）"
              % (len(report["violations"]), report["surface"]))
        for v in report["violations"]:
            print("  [%s] %s:%s %s" % (v["rule"], v["file"], v.get("line", "-"),
                                       v["detail"]))
        return 1
    print("CONCLUSION_ANCHORS_PASS anchored_docs=%d surface_docs=%d producers=%d stale=%d"
          % (report["scanned"]["anchored_docs"], report["scanned"]["surface_docs"],
             report["scanned"]["sources"], len(report.get("stale", []))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
