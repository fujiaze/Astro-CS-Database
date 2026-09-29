#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ctest_face.py — 「这个 ctest 名字真的存在吗」的**单一实现点**（两个互相独立的面）。

为什么需要它（FINAL-07 注册面三面交叉核查 §6 的 R1/R2/R3/R4）
  全仓有四个「按名字引用 ctest 目标」的登记面，它们各自的核对面要么**自指**
  （known_failures 的 V3 用「冻结基线 ∪ 模式在基线集合上展开」证明自己）、要么是
  **正则盲面**（GATES_AND_TOLERANCES 用 add_test 的 NAME 正则从 CMake 文本抽名，把
  p1_psfw_${g} 截成不存在的 p1_psfw_），要么**根本没有核对**（mutation_gates 的
  ctest_name、ctest_skip_register 无人读）。四面各修各的会重新长出四套口径
  ⇒ 本文件是唯一实现点，四个消费方都从这里取面。

两个面（各自独立，都不依赖任何登记册）
  面 A static_face      配置期注册面（静态）：git 已跟踪的 CMake 源里 add_test(NAME …)
                        的真名，foreach 头变量已求值。复用
                        eng/tools/quality/check_ctest_reg_condition.py 的解析器
                        （面 A 的权威实现点，不另发明一套求值）。永远可用，不看构建树。
  面 B configured_face  实际配置面：ctest -N --show-only=json-v1 的真实产出。
                        只有它能回答「这个目标此刻**真的会被 ctest 执行**」——
                        面 A 看不见 option(X … OFF) 包裹的分支（实测：
                        p3_rsmp_mutation_driver 在面 A 里存在，在面 B 里不存在）。
                        没有构建树时不可用（返回 None，**不**伪造空集）。

合并口径 resolve() = 面 A ∪ 面 B；两面都拿不到名字 ⇒ 抛 Unavailable（fail-closed：
空面不得静默判绿，与 CHK-REG-SURFACE-CROSS 的 X6 同一纪律）。

只读；仅 stdlib（+ git / ctest 子进程，均带 timeout）。无网络。
用法：
  import ctest_face; ctest_face.resolve()
  python3 eng/ci/ctest_face.py --inventory
  python3 eng/ci/ctest_face.py --self-test
"""
from __future__ import annotations

import importlib.util
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parent.parent.parent
COND_TOOL_REL = "eng/tools/quality/check_ctest_reg_condition.py"
SURFACE_TOOL_REL = "eng/tools/quality/check_ctest_registration.py"
GIT_TIMEOUT = 120
CTEST_TIMEOUT = 300
ENV_BUILD_DIRS = "ACSD_CTEST_BUILD_DIR"
ENV_FACE_JSON = "ACSD_CTEST_FACE_JSON"
# 自动发现的构建树候选（顺序即优先级）。前两个是 CI 的正式构建落点
# （eng/tools/quality/deep_ci_driver.py 的默认值），第三个是开发者本地的 build/。
BUILD_DIR_CANDIDATES = ("run/ci/build-gcc-release", "run/ci/build-clang", "build")


class Unavailable(RuntimeError):
    """两个面都取不到名字 —— fail-closed，不得把「扫不到」当「不存在」。"""


def _load_tool(rel: str, name: str):
    path = REPO / rel
    if not path.is_file():
        raise Unavailable("依赖工具不存在：%s" % rel)
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    try:
        spec.loader.exec_module(mod)
    except Exception as exc:  # noqa: BLE001
        raise Unavailable("依赖工具加载失败 %s（%s: %s）"
                          % (rel, exc.__class__.__name__, exc)) from None
    return mod


def _git_tracked(repo: pathlib.Path) -> set:
    """git 已跟踪文件集。CTest 面 = 版本库面（与 check_ctest_registration 同口径）。"""
    try:
        out = subprocess.run(["git", "-C", str(repo), "ls-files", "-z"],
                             capture_output=True, text=True, timeout=GIT_TIMEOUT)
    except (OSError, subprocess.SubprocessError) as exc:
        raise Unavailable("git 调用失败（%s: %s）"
                          % (exc.__class__.__name__, exc)) from None
    if out.returncode != 0 or not out.stdout:
        tail = (out.stderr.strip().splitlines() or [""])[-1]
        raise Unavailable("git ls-files 不可用（rc=%s: %s）—— 版本库面无法枚举"
                          % (out.returncode, tail[:140]))
    return {p for p in out.stdout.split("\0") if p}


def _cmake_sources(repo: pathlib.Path) -> dict:
    """git 已跟踪的 CMake 源（面 A 的输入），直接读工作树文件。

    口径与 eng/tools/quality/check_ctest_registration.py:discover_sources
    (tracked_only=True) 一致（同一份 SKIP 名单、同一个 git ls-files 版本库面），
    但绕开它的 repo.rglob —— 实测 rglob 要 49 s（要穿过 gaia/ 只读数据集），
    而 git ls-files 只是一次子进程。
    """
    surface = _load_tool(SURFACE_TOOL_REL, "ctest_face_surface")
    tracked = _git_tracked(repo)
    srcs = {}
    for rel in sorted(tracked):
        if not (rel.endswith("CMakeLists.txt") or rel.endswith(".cmake")):
            continue
        if surface._is_skipped(pathlib.PurePosixPath(rel)):
            continue
        try:
            srcs[rel] = (repo / rel).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
    return srcs


def static_face(repo: pathlib.Path = REPO):
    """面 A：配置期注册面（静态）。返回 (names:set, where:dict, errors:list)。

    where[name] = "file:line"，行号是**剥离整行注释后的解析行号**（与该工具一致）。
    行锚核对用 anchor_hits_add_test（物理行号），两者口径不同是已知口径差。
    """
    repo = pathlib.Path(repo)
    cond = _load_tool(COND_TOOL_REL, "ctest_face_cond")
    srcs = _cmake_sources(repo)
    an = cond.analyze(srcs, root=repo)
    reg, _ref, _vio, errors, _summary, _rt = cond.judge(an, root=repo)
    where = {name: occ[0].where(repo) for name, occ in reg.items()}
    return set(reg), where, [str(e) for e in errors]


def auto_build_dirs(repo: pathlib.Path = REPO, extra=None) -> list:
    """自动发现可用构建树（已配置 = 存在 CTestTestfile.cmake）。"""
    repo = pathlib.Path(repo)
    cands = []
    env = os.environ.get(ENV_BUILD_DIRS, "")
    for tok in re.split(r"[;:\s]+", env):
        if tok:
            cands.append(tok)
    cands.extend(extra or [])
    cands.extend(BUILD_DIR_CANDIDATES)
    seen, out = set(), []
    for rel in cands:
        rel = rel.strip()
        if not rel or rel in seen:
            continue
        seen.add(rel)
        if (repo / rel / "CTestTestfile.cmake").is_file():
            out.append(rel)
    return out


def _parse_face_json(path: pathlib.Path) -> tuple:
    """解析 ctest -N --show-only=json-v1 产物（或 ctest -N 文本）的名字集。"""
    text = path.read_text(encoding="utf-8", errors="replace")
    data = None
    try:
        data = json.loads(text)
    except ValueError:
        data = None
    names, disabled = set(), set()
    if isinstance(data, dict) and isinstance(data.get("tests"), list):
        for t in data["tests"]:
            if not isinstance(t, dict):
                continue
            n = str(t.get("name") or "").strip()
            if not n:
                continue
            names.add(n)
            for p in (t.get("properties") or []):
                if isinstance(p, dict) and p.get("name") == "DISABLED":
                    disabled.add(n)
        return names, disabled
    for m in re.finditer(r"^\s*Test\s+#\d+:\s*(\S.*?)\s*$", text, re.M):
        names.add(m.group(1))
    return names, disabled


def configured_face(repo: pathlib.Path = REPO, build_dirs=None, json_path=None,
                     run_ctest=True):
    """面 B：实际配置面。返回 (names, disabled, source_label) 或 None（不可用）。

    优先级：显式 json_path → 环境变量 ACSD_CTEST_FACE_JSON → 对每个可用构建树跑
    ctest -N --show-only=json-v1。一个都拿不到 ⇒ 返回 None（**不是**空集：
    没有构建树不等于一个 ctest 目标都不存在）。
    """
    repo = pathlib.Path(repo)
    for cand in ([json_path] if json_path else []):
        p = pathlib.Path(cand)
        if not p.is_absolute():
            p = repo / p
        if p.is_file():
            names, disabled = _parse_face_json(p)
            if names:
                return names, disabled, str(p)
    env = os.environ.get(ENV_FACE_JSON, "").strip()
    if env:
        p = pathlib.Path(env)
        if not p.is_absolute():
            p = repo / p
        if p.is_file():
            names, disabled = _parse_face_json(p)
            if names:
                return names, disabled, str(p)
    if not run_ctest:
        return None
    for rel in (build_dirs if build_dirs is not None else auto_build_dirs(repo)):
        try:
            out = subprocess.run(
                ["ctest", "--test-dir", str(repo / rel), "-N", "--show-only=json-v1"],
                capture_output=True, text=True, timeout=CTEST_TIMEOUT, cwd=str(repo))
        except (OSError, subprocess.SubprocessError):
            continue
        if out.returncode != 0 or not out.stdout.strip():
            continue
        try:
            names, disabled = _parse_face_json(_spill(out.stdout))
        except (OSError, ValueError):
            continue
        if names:
            return names, disabled, "ctest -N --show-only=json-v1 @%s" % rel
    return None


_SPILL_DIR = None


def _spill(text: str) -> pathlib.Path:
    """把子进程 stdout 落到临时文件（让文本/JSON 两条解析路径共用 _parse_face_json）。"""
    global _SPILL_DIR
    if _SPILL_DIR is None:
        _SPILL_DIR = tempfile.mkdtemp(prefix="astrocs_ctest_face_")
    p = pathlib.Path(_SPILL_DIR) / "ctest_n.txt"
    p.write_text(text, encoding="utf-8", errors="replace")
    return p


class Face:
    """合并面 = 面 A ∪ 面 B。names 是并集；configured 为 None 表示面 B 不可用。"""

    __slots__ = ("names", "where", "configured", "static", "errors", "notes",
                 "configured_source", "configured_disabled")

    def __init__(self, names, where, configured, static, errors, notes,
                 configured_source=None, configured_disabled=None):
        self.names = names
        self.where = where
        self.configured = configured
        self.static = static
        self.errors = list(errors)
        self.notes = list(notes)
        self.configured_source = configured_source
        self.configured_disabled = set(configured_disabled or ())

    def has(self, name: str) -> bool:
        return str(name) in self.names

    def on_configured(self, name: str):
        """面 B 上的存在性；面 B 不可用时返回 None（**不得**当成 False）。"""
        if self.configured is None:
            return None
        return str(name) in self.configured

    def as_dict(self) -> dict:
        return {
            "static_count": len(self.static),
            "configured_count": (None if self.configured is None
                                 else len(self.configured)),
            "union_count": len(self.names),
            "configured_source": self.configured_source,
            "errors": self.errors,
            "notes": self.notes,
        }


def resolve(repo: pathlib.Path = REPO, build_dirs=None, json_path=None,
            run_ctest=True) -> Face:
    """取合并面。两个面都空 ⇒ Unavailable（fail-closed）。"""
    repo = pathlib.Path(repo)
    notes = []
    s_names, s_where, s_err = static_face(repo)
    errors = list(s_err)
    try:
        cfg = configured_face(repo, build_dirs=build_dirs, json_path=json_path,
                              run_ctest=run_ctest)
    except Unavailable as exc:
        errors.append(str(exc))
        cfg = None
    if cfg is None:
        notes.append("面 B（实际配置面）不可用：无可用构建树或 ctest -N 失败"
                     "——「是否真的在跑」只能由面 A + 行锚/条件推导回答")
        cfg_names, cfg_disabled, cfg_src = set(), set(), None
    else:
        cfg_names, cfg_disabled, cfg_src = cfg
    union = set(s_names) | set(cfg_names)
    if not union:
        raise Unavailable("面 A 与面 B 合起来 0 个 ctest 名字（空面不得静默判绿）")
    return Face(union, s_where, cfg_names if cfg is not None else None, s_names,
                errors, notes, cfg_src, cfg_disabled)


_RANGE_RE = re.compile(r"^(?P<file>[^:\s]+):(?P<a>\d+)(?:-(?P<b>\d+))?$")


_ANCHOR_IN_TEXT_RE = re.compile(
    r"(?P<file>[A-Za-z0-9_./\u4e00-\u9fff-]+):(?P<a>\d+)(?:-(?P<b>\d+))?")


def parse_anchor(anchor: str):
    """`path:line` / `path:a-b` → (path, a, b)；形态不符返回 None。"""
    m = _RANGE_RE.match(str(anchor or "").strip())
    if not m:
        return None
    a = int(m.group("a"))
    b = int(m.group("b")) if m.group("b") else a
    return m.group("file"), a, b


def find_anchor(text: str):
    """从登记散文里取出**首个**行锚（允许尾部带注解，如 `…:43-50（option(… OFF)）`）。

    登记册的 registration 字段历史上就是「行锚 + 解释」的混合串；本函数只把行锚
    抽出来核对，注解部分不参与判定。找不到行锚返回 None。
    """
    m = _ANCHOR_IN_TEXT_RE.search(str(text or ""))
    if not m:
        return None
    a = int(m.group("a"))
    b = int(m.group("b")) if m.group("b") else a
    return m.group("file"), a, b


def read_committed(repo: pathlib.Path, rel: str) -> str:
    """读**提交树**版本（行锚是登记在版本库里的坐标，工作区并发改动会让它漂移）。"""
    try:
        out = subprocess.run(["git", "-C", str(repo), "show", "HEAD:" + rel],
                             capture_output=True, text=True, timeout=GIT_TIMEOUT)
    except (OSError, subprocess.SubprocessError):
        return ""
    return out.stdout if out.returncode == 0 else ""


ANCHOR_SLACK = 3


def anchor_hits_add_test(repo: pathlib.Path, anchor: str, name: str):
    """行锚区间内是否存在真正注册 `name` 的 add_test 语句。返回 (hit, detail)。

    核对的是**物理行号**（未剥离注释），与登记册里写的 file:line 口径一致；
    面 A 报的解析行号是剥离注释后的，两者不同是已知口径差（docstring 已注明）。

    判定分三档（只加严：任何「锚指向别的东西」都判红，只有**纯行漂移**才容忍）：
      ① 区间内含 `add_test(NAME <name>)`            ⇒ 命中。
      ② 区间内含**别的** `add_test(NAME X)`          ⇒ 判红（锚指向了另一个注册）。
      ③ 区间内没有任何 add_test，但同文件里 `add_test(NAME <name>)` 存在且
         与区间距离 ≤ ANCHOR_SLACK 行                    ⇒ 命中，detail 标 `漂移 N 行`
         （无关重构会让行号整体平移；行号是定位提示，不是判据本身）。
      ④ 同文件里根本没有该名字的注册                ⇒ 判红。
    """
    parsed = find_anchor(anchor)
    if not parsed:
        return False, "行锚形态不可解析（期望 path:line 或 path:a-b）：%r" % (anchor,)
    rel, a, b = parsed
    if not (pathlib.Path(repo) / rel).is_file():
        return False, "行锚指向的文件不存在：%s" % rel
    text = read_committed(pathlib.Path(repo), rel)
    if not text:
        return False, "行锚文件在提交树里不可读（git show 失败）：%s" % rel
    lines = text.splitlines()
    if b > len(lines):
        return False, "行锚越界：%s:%d-%d（文件共 %d 行）" % (rel, a, b, len(lines))
    flat = re.sub(r"\s+", " ", "\n".join(lines[a - 1:b]))
    if re.search(r"add_test\(\s*NAME\s+%s(?=[\s)])" % re.escape(name), flat):
        return True, "%s:%d-%d 含 add_test(NAME %s …)" % (rel, a, b, name)
    got = re.search(r"add_test\(\s*NAME\s+([^\s)]+)", flat)
    if got:
        return False, ("行锚 %s:%d-%d 注册的是 %r，不是 %r"
                       % (rel, a, b, got.group(1), name))
    name_rx = re.compile(r"add_test\(\s*NAME\s+%s(?=[\s)]|$)" % re.escape(name))
    real = [i + 1 for i, ln in enumerate(lines) if name_rx.search(ln)]
    if not real:
        return False, "%s 里没有任何 add_test(NAME %s …)" % (rel, name)
    near = min(real, key=lambda ln: min(abs(ln - a), abs(ln - b)))
    drift = min(abs(near - a), abs(near - b))
    if drift <= ANCHOR_SLACK:
        return True, ("%s:%d-%d 无 add_test，但 %s:%d 是同名注册（行漂移 %d 行 ≤ %d，"
                      "按定位提示容忍；建议把行锚刷新到 %d）"
                      % (rel, a, b, rel, near, drift, ANCHOR_SLACK, near))
    return False, ("行锚 %s:%d-%d 与同名注册 %s:%d 相距 %d 行（> %d）—— 锚已失效"
                   % (rel, a, b, rel, near, drift, ANCHOR_SLACK))


def _self_test() -> int:
    """内存 fixture 正/负例：行锚形态、行锚指向真身、合并面口径、fail-closed。"""
    import shutil
    import subprocess as _sp

    problems = []

    def case(name, got, want):
        ok = got == want
        problems.append("%s %s (got=%r want=%r)"
                        % (name, "OK" if ok else "MISMATCH", got, want))

    case("pos_anchor_single", parse_anchor("a/b/CMakeLists.txt:12"),
         ("a/b/CMakeLists.txt", 12, 12))
    case("pos_anchor_range", parse_anchor("a/b/CMakeLists.txt:12-18"),
         ("a/b/CMakeLists.txt", 12, 18))
    case("neg_anchor_junk", parse_anchor("no-line-here"), None)
    case("neg_anchor_empty", parse_anchor(""), None)

    tmp = pathlib.Path(tempfile.mkdtemp(prefix="astrocs_face_"))
    try:
        _sp.run(["git", "init", "-q"], cwd=str(tmp), capture_output=True)
        _sp.run(["git", "config", "user.email", "t@t"], cwd=str(tmp), capture_output=True)
        _sp.run(["git", "config", "user.name", "t"], cwd=str(tmp), capture_output=True)
        rel = "eng/tests/unit/x/CMakeLists.txt"
        (tmp / rel).parent.mkdir(parents=True, exist_ok=True)
        (tmp / rel).write_text(
            "set(G alpha beta)\n"
            "foreach(g ${G})\n"
            "  add_test(NAME fx_${g} COMMAND t ${g})\n"
            "endforeach()\n"
            "option(ENABLE_MUT OFF)\n"
            "if(ENABLE_MUT)\n"
            "  add_test(NAME fx_mut COMMAND t)\n"
            "endif()\n", encoding="utf-8")
        _sp.run(["git", "add", "-A"], cwd=str(tmp), capture_output=True)
        _sp.run(["git", "commit", "-qm", "x"], cwd=str(tmp), capture_output=True)
        names, where, errs = static_face(tmp)
        case("pos_static_foreach_expanded",
             sorted(n for n in names if n.startswith("fx_")),
             ["fx_alpha", "fx_beta", "fx_mut"])
        case("pos_static_no_fake_truncation", "fx_" in names, False)
        case("pos_static_where_has_name", "fx_alpha" in where, True)

        ok, detail = anchor_hits_add_test(tmp, "%s:2-4" % rel, "fx_${g}")
        case("pos_anchor_foreach_true", ok, True)
        ok, detail = anchor_hits_add_test(tmp, "%s:1" % rel, "fx_alpha")
        case("neg_anchor_wrong_name", ok, False)
        case("neg_anchor_wrong_name_names_other", "fx_" in detail, True)
        ok, detail = anchor_hits_add_test(tmp, "%s:900-999" % rel, "fx_alpha")
        case("neg_anchor_oob", ok, False)
        case("neg_anchor_oob_says_range", "越界" in detail, True)
        ok, detail = anchor_hits_add_test(tmp, "nope/CMakeLists.txt:1", "x")
        case("neg_anchor_file_absent", ok, False)
        case("neg_anchor_file_absent_names", "不存在" in detail, True)

        f = resolve(tmp, run_ctest=False)
        case("pos_merge_configured_none", f.configured, None)
        case("pos_merge_has_static_name", f.has("fx_alpha"), True)
        case("pos_merge_on_configured_none", f.on_configured("fx_alpha"), None)
        case("pos_merge_notes_face_b_missing",
             any("面 B" in n for n in f.notes), True)

        fj = tmp / "face.json"
        fj.write_text(json.dumps({
            "version": {"major": 1},
            "tests": [{"name": "fx_alpha"},
                      {"name": "fx_gamma",
                       "properties": [{"name": "DISABLED"}]}],
        }), encoding="utf-8")
        got = configured_face(tmp, json_path=fj, run_ctest=False)
        case("pos_face_b_injected_names", sorted(got[0]), ["fx_alpha", "fx_gamma"])
        case("pos_face_b_injected_disabled", sorted(got[1]), ["fx_gamma"])
        f2 = resolve(tmp, json_path=fj, run_ctest=False)
        case("pos_merge_with_face_b", f2.has("fx_gamma"), True)
        case("pos_merge_face_b_on_configured", f2.on_configured("fx_gamma"), True)
        case("pos_merge_face_b_off_configured", f2.on_configured("fx_beta"), False)
        case("pos_merge_where_still_static", "fx_gamma" in f2.where, False)
        case("neg_face_b_missing_is_none",
             configured_face(tmp, json_path=tmp / "nope.json", run_ctest=False), None)
        case("disc_face_b_false_for_absent", f2.on_configured("fx_nope"), False)
        case("disc_face_a_false_for_absent", f2.has("fx_nope"), False)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    bad = [p for p in problems if "OK" not in p]
    for p in problems:
        print("[selftest] %s" % p)
    print("[selftest] %d cases, %s"
          % (len(problems), "ALL OK" if not bad else "FAILED"))
    return 0 if not bad else 1


def main(argv=None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if "--self-test" in argv:
        return _self_test()
    try:
        face = resolve(REPO)
    except Unavailable as exc:
        print("CTEST_FACE_UNAVAILABLE %s" % exc, file=sys.stderr)
        return 2
    print(json.dumps(face.as_dict(), ensure_ascii=False, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
