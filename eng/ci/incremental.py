#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/incremental.py — 增量范围计算（CI-INCREMENTAL）。

权威依据
  - docs/ci/CI_SPEC.md §2.1（三种 scope：changed 默认 / full 显式 / explicit 点名）、
    §2.2（改动集 = git diff ∪ git status 未提交）、§2.3（选择规则 + 构建图反查）、
    §2.4（fail-closed 三条：未覆盖路径判红 / 敏感面强制升级全量 / 空选择判红）、
    §2.5（超时预算）、§2.6（prerelease 档 + 输入指纹缓存）；
  - ENGINEERING_SPEC.md §10（机器一致性检查）、§13（与 CI 的关系）；
  - AGENTS.md §6（不用 waiver 盖红灯；拿不准升级为全量而不是跳过）。

本模块只做**范围计算**，不执行任何检查；执行仍由 eng/ci/run_checks.py 负责。
设计红线（与任务书一致）：
  - 任何"跳过"必须有机器可验证依据；任何"拿不准"升级为全量，而不是跳过；
  - 构建图不可用 / 依赖缺失 / 路径无法归属 ⇒ 保持选中（fail-closed），不缩范围。

用法（供 run_checks.py 导入；也可单独复跑取证据）
  python3 eng/ci/incremental.py change-set --base HEAD
  python3 eng/ci/incremental.py affected-targets --build-dir build lib/algorithms/coverage/src/sampler.cpp
  python3 eng/ci/incremental.py fingerprint --archive artifacts/acceptance/l2_performance \\
      --commit "$(git rev-parse HEAD)" --config eng/contracts/resource_gate_v1.json \\
      --data testdata/BASS_DR3 --workers 16
"""
from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path

SCHEMA_VERSION = 1
TOOL = "eng/ci/incremental.py"

# CI_SPEC.md §2.4-2：全局敏感面。命中即升级为整档全量（scope="full"）。
SENSITIVE_PATTERNS = (
    "eng/ci/**",
    "eng/cmake/**",
    "CMakeLists.txt",
    "CMakePresets.json",
    "lib/include/**",
    "eng/contracts/**",
    "eng/packaging/config/**",
    "docs/DOCUMENT_INDEX.yaml",
)

# 构建图反查的输入上限（防止异常环境下把 CI 拖死）；超限即放弃缩范围（fail-closed）。
MAX_DEPS_BYTES = 512 * 1024 * 1024
CTEST_JSON_TIMEOUT = 300


class IncrementalError(Exception):
    """增量范围计算自身不可用（调用方按 runner error rc=2 处理，不得当成空改动集）。"""


# ---------------------------------------------------------------------------
# glob 语义（CI_SPEC.md §2.3）
# ---------------------------------------------------------------------------

def glob_match(path: str, pattern: str) -> bool:
    """路径匹配：'dir/**' 命中 dir 及其任意子孙；含通配符按 shell glob（'*' 不跨 '/'）。"""
    path = path.replace("\\", "/").lstrip("./")
    pattern = pattern.replace("\\", "/").lstrip("./")
    if not pattern:
        return False
    if pattern.endswith("/**"):
        base = pattern[:-3].rstrip("/")
        return path == base or path.startswith(base + "/")
    if not any(ch in pattern for ch in "*?["):
        return path == pattern
    # fnmatch 的 '*' 会跨 '/'，这里改用自定义翻译保证 '*' 不跨目录分隔符。
    rx, i = "", 0
    while i < len(pattern):
        ch = pattern[i]
        if pattern.startswith("**", i):
            rx += ".*"
            i += 2
        elif ch == "*":
            rx += "[^/]*"
            i += 1
        elif ch == "?":
            rx += "[^/]"
            i += 1
        elif ch == "[":
            j = pattern.find("]", i + 1)
            if j == -1:
                rx += re.escape(ch)
                i += 1
            else:
                rx += pattern[i:j + 1]
                i = j + 1
        else:
            rx += re.escape(ch)
            i += 1
    return re.fullmatch(rx, path) is not None


def matches_any(path: str, patterns) -> bool:
    return any(glob_match(path, p) for p in patterns or ())


def first_match(path: str, patterns):
    for p in patterns or ():
        if glob_match(path, p):
            return p
    return None


# ---------------------------------------------------------------------------
# 改动集（CI_SPEC.md §2.2）
# ---------------------------------------------------------------------------

def git(repo: Path, *args: str) -> str:
    proc = subprocess.run(["git", *args], cwd=str(repo), stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, timeout=300)
    if proc.returncode != 0:
        raise IncrementalError(
            f"git {' '.join(args)} 失败 rc={proc.returncode}："
            f"{proc.stderr.decode('utf-8', 'replace').strip()[:400]}")
    return proc.stdout.decode("utf-8", "replace")


def parse_porcelain(text: str) -> list:
    """解析 git status --porcelain=v1 输出（处理重命名 -> 与引号包裹的非 ASCII 路径）。"""
    paths = []
    for line in text.splitlines():
        if len(line) < 4:
            continue
        entry = line[3:]
        if " -> " in entry:
            entry = entry.split(" -> ", 1)[1]
        if entry.startswith('"') and entry.endswith('"'):
            raw = entry[1:-1]
            # git 对非 ASCII 路径输出八进制转义；先尝试 unicode_escape 还原。
            try:
                entry = raw.encode("latin-1", "backslashreplace").decode("unicode_escape")
            except (UnicodeDecodeError, UnicodeEncodeError):
                entry = raw
        if entry.strip():
            paths.append(entry.strip())
    return paths


def change_set(repo: Path, base: str) -> dict:
    """改动集 = git diff --name-only <base> ∪ git status --porcelain（未提交，含未跟踪）。"""
    repo = Path(repo)
    try:
        git(repo, "rev-parse", "--verify", "--quiet", f"{base}^{{commit}}")
    except IncrementalError as exc:
        raise IncrementalError(f"基线 ref 不可用（--base {base}）：{exc}") from exc
    committed = [p.strip() for p in git(repo, "diff", "--name-only", base).splitlines() if p.strip()]
    status = parse_porcelain(git(repo, "status", "--porcelain=v1", "--untracked-files=all"))
    union = sorted({p.replace("\\", "/") for p in (committed + status) if p.strip()})
    return {
        "base_ref": base,
        "committed_changed": sorted({p.replace("\\", "/") for p in committed}),
        "uncommitted_changed": sorted({p.replace("\\", "/") for p in status}),
        "changed_files": union,
        "injected": False,
    }


def change_set_from_file(path: Path) -> dict:
    """注入改动集（仅用于 --self-test 负例与可复现实验，CI_SPEC.md §2.4 的负例面）。

    注入集**替换**git 推导结果，因此必须在输出里显式标注 injected=true，
    且 run_checks.py 会把它写进结果 JSON 的 selection 字段，禁止与真实运行混淆。
    """
    if not path.is_file():
        raise IncrementalError(f"注入改动集文件不存在：{path}")
    lines = [ln.strip() for ln in path.read_text(encoding="utf-8").splitlines()]
    files = sorted({ln for ln in lines if ln and not ln.startswith("#")})
    return {
        "base_ref": None,
        "committed_changed": [],
        "uncommitted_changed": [],
        "changed_files": files,
        "injected": True,
        "injected_from": str(path),
    }


# ---------------------------------------------------------------------------
# fail-closed（CI_SPEC.md §2.4）
# ---------------------------------------------------------------------------

def escalation_reasons(changed) -> list:
    """命中全局敏感面的改动文件 -> 原因列表（空列表 = 不升级）。"""
    reasons = []
    for path in changed:
        pat = first_match(path, SENSITIVE_PATTERNS)
        if pat:
            reasons.append({"path": path, "sensitive_pattern": pat})
    return reasons


def uncovered_paths(changed, all_patterns) -> list:
    """改动集中不匹配任何注册检查 changed_paths 的文件（注册表覆盖缺口）。"""
    return [p for p in changed if not matches_any(p, all_patterns)]


def coverage_patterns(steps) -> list:
    pats = set()
    for s in steps:
        pats.update(s.get("changed_paths") or ())
    return sorted(pats)


# ---------------------------------------------------------------------------
# 构建图反查（CI_SPEC.md §2.3；ninja build.ninja 边 + ninja -t deps）
# ---------------------------------------------------------------------------

def _ninja_unescape(tok: str) -> str:
    out, i = "", 0
    while i < len(tok):
        ch = tok[i]
        if ch == "$" and i + 1 < len(tok):
            nxt = tok[i + 1]
            if nxt in (" ", ":", "$"):
                out += nxt
                i += 2
                continue
            if nxt == "\n":
                i += 2
                continue
        out += ch
        i += 1
    return out


def _ninja_tokens(text: str) -> list:
    toks, cur, i = [], "", 0
    while i < len(text):
        ch = text[i]
        if ch == "$" and i + 1 < len(text):
            nxt = text[i + 1]
            if nxt == " ":
                cur += " "
                i += 2
                continue
            if nxt == ":":
                cur += ":"
                i += 2
                continue
            if nxt == "$":
                cur += "$"
                i += 2
                continue
        if ch in " \t":
            if cur:
                toks.append(cur)
                cur = ""
            i += 1
            continue
        cur += ch
        i += 1
    if cur:
        toks.append(cur)
    return toks


def _logical_lines(path: Path):
    buf = ""
    with path.open("r", encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            line = raw.rstrip("\n")
            if line.endswith("$"):
                buf += line[:-1]
                continue
            buf += line
            yield buf
            buf = ""
    if buf:
        yield buf


def parse_build_ninja(build_dir: Path) -> dict:
    """解析 build.ninja 的 build 边；返回 {input_abs: {output_abs, ...}} 反向索引。"""
    ninja_file = build_dir / "build.ninja"
    if not ninja_file.is_file():
        return {}
    rev: dict = {}
    for line in _logical_lines(ninja_file):
        if not line.startswith("build "):
            continue
        body = line[len("build "):]
        idx, depth = None, 0
        i = 0
        while i < len(body):
            if body[i] == "$":
                i += 2
                continue
            if body[i] == ":":
                idx = i
                break
            i += 1
        if idx is None:
            continue
        outs = _ninja_tokens(body[:idx])
        rest = body[idx + 1:]
        if rest.startswith(" "):
            rest = rest[1:]
        ins_part = rest
        for sep in ("||",):
            if sep in ins_part:
                ins_part = ins_part.split(sep, 1)[0]
        # 去掉 rule 名（第一个 token）
        toks = _ninja_tokens(ins_part)
        if not toks:
            continue
        rule = toks[0]
        if rule.startswith("|"):
            continue
        ins = [t for t in toks[1:] if t != "|"]
        out_abs = [os.path.normpath(os.path.join(str(build_dir), o)) for o in outs]
        in_abs = [os.path.normpath(os.path.join(str(build_dir), p)) for p in ins]
        for o in out_abs:
            for p in in_abs:
                rev.setdefault(p, set()).add(o)
    return rev


def parse_ninja_deps(build_dir: Path) -> dict:
    """ninja -t deps：{output_abs: [dep_abs, ...]}（含 depfile 发现的头文件依赖）。"""
    try:
        proc = subprocess.run(["ninja", "-C", str(build_dir), "-t", "deps"],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=900)
    except (OSError, subprocess.SubprocessError):
        return {}
    if proc.returncode != 0:
        return {}
    if len(proc.stdout) > MAX_DEPS_BYTES:
        return {}
    out: dict = {}
    cur = None
    for raw in proc.stdout.decode("utf-8", "replace").splitlines():
        if not raw.strip():
            cur = None
            continue
        if raw.startswith("    "):
            if cur is not None:
                dep = raw.strip()
                out.setdefault(cur, []).append(
                    os.path.normpath(os.path.join(str(build_dir), dep)))
            continue
        head = raw.split(":", 1)[0].strip()
        cur = os.path.normpath(os.path.join(str(build_dir), head))
    return out


class BuildGraph:
    """从改动文件反查受影响构建产物 + ctest 测试名（真实依赖边，非路径猜测）。"""

    def __init__(self, build_dir: Path, repo: Path):
        # 必须绝对化：build.ninja / ctest 里的路径都要与改动文件同一坐标系，
        # 否则反向索引键与 ctest 命令 token 解析出的路径对不上（会静默返回"无受影响测试"）。
        self.build_dir = Path(build_dir).resolve()
        self.repo = Path(repo).resolve()
        self.available = False
        self.reason = None
        self._rev: dict = {}
        self._tests: dict = {}
        self._test_exe: dict = {}

    def load(self) -> "BuildGraph":
        if not (self.build_dir / "build.ninja").is_file():
            self.reason = f"构建图不可用：{self.build_dir}/build.ninja 不存在"
            return self
        self._rev = parse_build_ninja(self.build_dir)
        for out, deps in parse_ninja_deps(self.build_dir).items():
            for d in deps:
                self._rev.setdefault(d, set()).add(out)
        if not self._rev:
            self.reason = f"构建图不可用：{self.build_dir} 无可用依赖边"
            return self
        self._tests = self._load_ctest_tests()
        self.available = True
        return self

    def _load_ctest_tests(self) -> dict:
        """ctest --show-only=json-v1：测试名 -> 命令 token（用于把构建产物映射到测试名）。"""
        try:
            proc = subprocess.run(
                ["ctest", "--test-dir", str(self.build_dir), "--show-only=json-v1"],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=CTEST_JSON_TIMEOUT)
        except (OSError, subprocess.SubprocessError):
            return {}
        if proc.returncode != 0:
            return {}
        try:
            data = json.loads(proc.stdout.decode("utf-8", "replace"))
        except Exception:  # noqa: BLE001 - 解析失败即放弃缩范围
            return {}
        tests = {}
        for t in data.get("tests", []) or []:
            name = t.get("name")
            cmd = (t.get("command") or [])
            if name and cmd:
                tests[name] = cmd
        return tests

    def _resolve(self, token: str):
        """命令 token -> 绝对路径（相对 build_dir 或 repo 解析；不存在返回 None）。"""
        for base in (self.build_dir, self.repo):
            cand = token if os.path.isabs(token) else os.path.normpath(os.path.join(str(base), token))
            if os.path.exists(cand):
                return cand
        return None

    def affected_outputs(self, changed_rel) -> set:
        """改动文件 -> 受影响的构建产物集合（自底向上闭包）。"""
        if not self.available:
            return set()
        seen, queue = set(), []
        for rel in changed_rel:
            abs_p = os.path.normpath(os.path.join(str(self.repo), rel))
            if abs_p in self._rev:
                queue.append(abs_p)
        while queue:
            cur = queue.pop()
            for out in self._rev.get(cur, ()):  # noqa: B010
                if out not in seen:
                    seen.add(out)
                    queue.append(out)
        return seen

    def affected_tests(self, changed_rel) -> set:
        """改动文件 -> 受影响的 ctest 测试名集合。"""
        affected = self.affected_outputs(changed_rel)
        if not affected:
            return set()
        names = set()
        for name, cmd in self._tests.items():
            for tok in cmd:
                if tok.startswith("-"):
                    continue
                abs_p = self._resolve(tok)
                if abs_p and abs_p in affected:
                    names.add(name)
                    break
        return names


def expand_target_globs(patterns, names) -> set:
    out = set()
    for n in names:
        for p in patterns or ():
            if fnmatch.fnmatchcase(n, p) or n == p:
                out.add(n)
                break
    return out


# ---------------------------------------------------------------------------
# 输入指纹缓存（CI_SPEC.md §2.6）
# ---------------------------------------------------------------------------

def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def _digest_paths(repo: Path, label: str, paths, h) -> list:
    detail = []
    for rel in sorted(paths or ()):
        fp = (repo / rel) if not os.path.isabs(rel) else Path(rel)
        if fp.is_file():
            digest = sha256_file(fp)
            h.update(f"{label} {rel} {digest}\n".encode("utf-8"))
            detail.append({"path": rel, "sha256": digest})
        elif fp.is_dir():
            for sub in sorted(x for x in fp.rglob("*") if x.is_file()):
                digest = sha256_file(sub)
                key = sub.relative_to(repo).as_posix() if not os.path.isabs(rel) else str(sub)
                h.update(f"{label} {key} {digest}\n".encode("utf-8"))
                detail.append({"path": key, "sha256": digest})
        else:
            h.update(f"{label} {rel} MISSING\n".encode("utf-8"))
            detail.append({"path": rel, "sha256": None, "missing": True})
    return detail


def input_fingerprint(repo: Path, *, commit: str, config_paths=(), data_paths=(),
                      workers=None, extra=None) -> dict:
    """输入指纹 = commit + 相关配置 SHA256 + 数据清单 SHA256 + worker 数。"""
    repo = Path(repo)
    h = hashlib.sha256()
    h.update(b"astrocs.prerelease-fingerprint/v1\n")
    h.update(f"commit={commit}\n".encode("utf-8"))
    h.update(f"workers={workers}\n".encode("utf-8"))
    for k, v in sorted((extra or {}).items()):
        h.update(f"extra {k}={v}\n".encode("utf-8"))
    cfg = _digest_paths(repo, "config", config_paths, h)
    dat = _digest_paths(repo, "data", data_paths, h)
    return {
        "schema_version": SCHEMA_VERSION,
        "algorithm": "sha256(commit|workers|extra|config-digests|data-digests)",
        "commit": commit,
        "workers": workers,
        "extra": dict(extra or {}),
        "config": cfg,
        "data": dat,
        "sha256": h.hexdigest(),
    }


def fingerprint_hit(archive: Path, fingerprint: dict) -> bool:
    """归档目录里已有指纹与本次一致 ⇒ 命中（跳过并复用）。"""
    path = Path(archive) / "fingerprint.json"
    if not path.is_file():
        return False
    try:
        prev = json.loads(path.read_text(encoding="utf-8"))
    except Exception:  # noqa: BLE001 - 损坏指纹视为未命中（重跑，fail-closed）
        return False
    return isinstance(prev, dict) and prev.get("sha256") == fingerprint.get("sha256")


def write_fingerprint(archive: Path, fingerprint: dict) -> Path:
    archive = Path(archive)
    archive.mkdir(parents=True, exist_ok=True)
    target = archive / "fingerprint.json"
    tmp = target.with_suffix(".json.tmp")
    tmp.write_text(json.dumps(fingerprint, ensure_ascii=False, indent=2, sort_keys=True),
                   encoding="utf-8")
    os.replace(tmp, target)
    return target


# ---------------------------------------------------------------------------
# CLI（复跑取证据用；run_checks.py 以库方式导入同一实现）
# ---------------------------------------------------------------------------

def _main(argv=None) -> int:
    p = argparse.ArgumentParser(prog=TOOL, description="增量范围计算（CI-INCREMENTAL）")
    sub = p.add_subparsers(dest="cmd", required=True)

    c1 = sub.add_parser("change-set", help="打印改动集（git diff ∪ git status）")
    c1.add_argument("--repo-root", default=None)
    c1.add_argument("--base", default="HEAD")

    c2 = sub.add_parser("affected-targets", help="构建图反查：改动文件 -> 受影响 ctest 测试名")
    c2.add_argument("paths", nargs="+")
    c2.add_argument("--repo-root", default=None)
    c2.add_argument("--build-dir", default="build")

    c3 = sub.add_parser("fingerprint", help="计算/校验输入指纹（prerelease 重步骤）")
    c3.add_argument("--repo-root", default=None)
    c3.add_argument("--archive", required=True)
    c3.add_argument("--commit", default="unknown")
    c3.add_argument("--config", action="append", default=[])
    c3.add_argument("--data", action="append", default=[])
    c3.add_argument("--workers", type=int, default=None)

    args = p.parse_args(argv)
    repo = Path(args.repo_root).resolve() if args.repo_root else \
        Path(__file__).resolve().parent.parent.parent
    if args.cmd == "change-set":
        print(json.dumps(change_set(repo, args.base), ensure_ascii=False, indent=2))
        return 0
    if args.cmd == "affected-targets":
        g = BuildGraph(Path(args.build_dir), repo).load()
        if not g.available:
            print(json.dumps({"available": False, "reason": g.reason}, ensure_ascii=False))
            return 2
        tests = sorted(g.affected_tests(args.paths))
        print(json.dumps({"available": True, "build_dir": args.build_dir,
                          "changed": list(args.paths), "affected_tests": tests,
                          "affected_test_count": len(tests)},
                         ensure_ascii=False, indent=2))
        return 0
    fp = input_fingerprint(repo, commit=args.commit, config_paths=args.config,
                           data_paths=args.data, workers=args.workers)
    hit = fingerprint_hit(Path(args.archive), fp)
    print(json.dumps({"fingerprint": fp, "archive": args.archive, "hit": hit},
                     ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(_main())
