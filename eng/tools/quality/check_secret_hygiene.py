#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-SECRET-HYGIENE —— 凭据入仓/入包卫生机器门（ROOT-006 产物）。

权威：
  * AGENTS.md §5 —— 不读取/打印密钥与凭据（本检查器只输出 路径/布尔/行号）；
  * ENGINEERING_SPEC.md §7 —— 禁止散落根目录、根条目须登记；
  * ENGINEERING_SPEC.md §8 —— 机器检查能绿能红、fail-closed；
  * docs/ci/01_CHECKS.md §4 —— 新增检查项流程；
  * 工程控制/PROJECT-GOVERNANCE-01/tasks/ROOT-006.md —— 本检查的需求来源。

设计要点
--------
1. **输出纪律（不可放宽）**：报告里只允许出现 路径、布尔、行号、计数。
   命中值本身、片段、前后缀、哈希 一律不落盘、不打印；报告序列化后会再自扫一遍，
   一旦自身命中敏感形态即判 FAIL（reason=output_self_leak），并且只输出最小 JSON。

2. **两级形态（按"证据强度"分层，避免长期恒红）**：
   * CRITICAL（任何文件命中即 FAIL）：私钥头（PEM/OPENSSH/PGP）、sk-/ghp_/AKIA/AIza/
     xox/JWT 等可被直接利用的凭据材料；
   * ESCALATE（只在**敏感类文件**里 FAIL，普通文件记 INFO）：password=/secret=/token=
     赋值字面量（非占位符、非变量引用）、裸 32/40/64 位 hex、长 base64。
     理由：仓库里提交哈希与 SHA256 校验和合法且普遍（INFO），而凭据类文件里出现这些
     形态才是风险信号；赋值语句在代码/夹具里大量存在，只有凭据类文件才升级为 FAIL。

3. **fail-closed（无法判定即 FAIL）**：git 不可用、扫描集为空、文件不可读、文本文件超限
   未扫、打包器不可导入、输出自泄 —— 全部 FAIL；不静默跳过。

4. 另有 INFO 级「基础设施标识」形态（CGNAT/Tailscale 网段地址、ssh 目标、私钥路径、
   authorized_keys）——它们不是凭据，但属侦察信息；只报告不判红，清运由治理批次处理。

5. 扫描范围边界（显式登记，非静默）：二进制文件（扩展名或前 8KiB 含 NUL）不做文本形态
   扫描，计数上报；打包器同样排除二进制扩展名。

用法：
  python3 eng/tools/quality/check_secret_hygiene.py                       # 默认 scope=pack
  python3 eng/tools/quality/check_secret_hygiene.py --scope tracked
  python3 eng/tools/quality/check_secret_hygiene.py --scope walk --root <dir>
  python3 eng/tools/quality/check_secret_hygiene.py --files a.md b.md --report-only
证据：tests/quality/test_secret_hygiene.py（正例 rc=0 / 负例 rc!=0 / fail-closed / 无值泄漏）。
"""
from __future__ import annotations

import argparse
import datetime
import json
import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent.parent.parent
DEFAULT_PACKER = REPO / "eng" / "tools" / "pack_audit_package.py"

CHECK_ID = "CHK-SECRET-HYGIENE"
SCHEMA_VERSION = 1
DEFAULT_MAX_BYTES = 4 * 1024 * 1024

# 二进制文件不做文本形态扫描（显式边界，计数上报；与打包器 EXCLUDE_EXT 同源）
BINARY_EXT = {
    ".fts", ".fit", ".fits", ".xisf", ".zip", ".gz", ".tgz", ".bz2", ".xz", ".7z", ".rar",
    ".dll", ".lib", ".a", ".o", ".so", ".exe", ".pdb", ".obj", ".exp", ".cache", ".pyc",
    ".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff", ".gif", ".webp", ".ico", ".pdf",
    ".npy", ".npz", ".parquet", ".feather", ".db", ".sqlite", ".bin", ".dat", ".p12", ".pfx",
    ".whl", ".jar", ".class", ".wasm", ".ttf", ".otf", ".woff", ".woff2",
}

# 敏感类文件名（命中 → ESCALATE 形态升级为 FAIL）
SENSITIVE_EXT = {".env", ".pem", ".key", ".ppk", ".jks", ".keystore", ".p12", ".pfx", ".netrc"}
SENSITIVE_EXACT = {"id_rsa", "id_dsa", "id_ecdsa", "id_ed25519", "credentials", "htpasswd", "netrc"}
SENSITIVE_TOKEN_RE = re.compile(
    r"(?i)(^|[._\-])(secret|secrets|credential|credentials|passwd|password|pwd|pass|"
    r"api[_-]?key|apikey|access|access[_-]?key|client[_-]?secret|private[_-]?key|auth)"
    r"([._\-]|$)")

# 占位符（赋值右侧若是这些形态，不算命中）
PLACEHOLDER_RE = re.compile(
    r"(?i)^(?:\$\{[^}]*\}|\$[A-Z_][A-Z0-9_]*|%\([^)]*\)s|\{\{[^}]*\}\|<[^>]*>|"
    r"x{4,}|X{4,}|\*{4,}|\.{3,}|-{3,}|_{3,}|0{4,}|"
    r"redacted|placeholder|example|sample|dummy|fake|test|testing|changeme|change[_-]?me|"
    r"your[_-]?[a-z0-9_]*|my[_-]?[a-z0-9_]*|none|null|nil|true|false|todo|tbd|n/?a|"
    r"os\.environ.*|getenv.*|environ\[.*|env\[.*|process\.env.*|\*+)$")

# 形态定义：(ID, 级别, 正则)
PATTERNS: list[tuple[str, str, re.Pattern]] = [
    # CRITICAL —— 任何文件命中即 FAIL
    ("PRIVATE_KEY_HEADER", "CRITICAL",
     re.compile(r"-----BEGIN [A-Z0-9 ]*PRIVATE KEY-----")),
    ("OPENSSH_PRIVATE_KEY", "CRITICAL",
     re.compile(r"-----BEGIN OPENSSH PRIVATE KEY-----")),
    ("PGP_PRIVATE_KEY_BLOCK", "CRITICAL",
     re.compile(r"-----BEGIN PGP PRIVATE KEY BLOCK-----")),
    ("SK_API_TOKEN", "CRITICAL",
     re.compile(r"(?<![A-Za-z0-9])sk-[A-Za-z0-9_\-]{16,}")),
    ("GITHUB_TOKEN", "CRITICAL",
     re.compile(r"(?<![A-Za-z0-9])(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,})")),
    ("AWS_ACCESS_KEY_ID", "CRITICAL",
     re.compile(r"(?<![A-Z0-9])(?:AKIA|ASIA|AGPA|AIDA|AROA|AIPA|ANPA|ANVA|ABIA|ACCA)[A-Z0-9]{16}(?![A-Z0-9])")),
    ("GCP_API_KEY", "CRITICAL",
     re.compile(r"(?<![A-Za-z0-9_\-])AIza[0-9A-Za-z_\-]{35}(?![A-Za-z0-9_\-])")),
    ("SLACK_TOKEN", "CRITICAL",
     re.compile(r"(?<![A-Za-z0-9\-])xox[abpros]-[0-9A-Za-z\-]{10,}")),
    ("JWT_TOKEN", "CRITICAL",
     re.compile(r"(?<![A-Za-z0-9_\-])eyJ[A-Za-z0-9_\-]{8,}\.eyJ[A-Za-z0-9_\-]{8,}\.[A-Za-z0-9_\-]{8,}")),
    ("PASSWORD_ASSIGN", "ESCALATE",
     re.compile(r"(?i)\b(?:password|passwd|pwd)\b\s*[:=]\s*[\"']?([^\s\"',;#]{8,})")),
    ("SECRET_ASSIGN", "ESCALATE",
     re.compile(r"(?i)\b(?:secret|token|api[_-]?key|apikey|access[_-]?key|secret[_-]?key|"
                r"client[_-]?secret|private[_-]?key|auth[_-]?token|bearer)\b\s*[:=]\s*"
                r"[\"']?([^\s\"',;#]{8,})")),
    # INFO —— 基础设施侦察信息（非凭据：内网/CGNAT 地址、ssh 目标、私钥路径、authorized_keys）：
    # 只报告布尔与行号，**永不判红**（仓库历史文档里合法存在；清运属治理批次，不属本门职责）
    ("INFRA_CGNAT_IP", "INFO",
     re.compile(r"(?<![\d.])100\.(?:6[4-9]|[7-9]\d|1[01]\d|12[0-7])\.\d{1,3}\.\d{1,3}(?![\d.])")),
    ("INFRA_SSH_TARGET", "INFO",
     re.compile(r"(?i)\b(?:ssh|scp)\b[^\n]{0,120}?[A-Za-z0-9_.-]+@(?:\d{1,3}\.){3}\d{1,3}")),
    ("INFRA_PRIVATE_KEY_REF", "INFO",
     re.compile(r"(?:~?/[\w./-]*\.ssh/[\w.-]+|\bid_(?:rsa|dsa|ecdsa|ed25519)(?:[_-][\w-]+)?)")),
    ("INFRA_AUTHORIZED_KEYS", "INFO", re.compile(r"(?i)\bauthorized_keys\b")),

    # ESCALATE —— 普通文件记 INFO；敏感类文件（文件名判定为凭据类）命中即 FAIL
    ("HEX32", "ESCALATE", re.compile(r"(?<![0-9A-Za-z])[0-9a-fA-F]{32}(?![0-9A-Za-z])")),
    ("HEX40", "ESCALATE", re.compile(r"(?<![0-9A-Za-z])[0-9a-fA-F]{40}(?![0-9A-Za-z])")),
    ("HEX64", "ESCALATE", re.compile(r"(?<![0-9A-Za-z])[0-9a-fA-F]{64}(?![0-9A-Za-z])")),
    ("B64_LONG", "ESCALATE",
     re.compile(r"(?<![A-Za-z0-9+/])[A-Za-z0-9+/]{40,}={0,2}(?![A-Za-z0-9+/=])")),
]

# 规则定义文件（**精确路径**，由测试断言锁定，防止被偷偷扩大）：
# 「检测规则本体」与「其测试夹具」按定义就要写出原始形态字面量（如 sk- 前缀、password 赋值、
# 私钥头拼接、哨兵串）。对它们只**降级**（命中仍逐条列出 路径/形态/行号，但不判红），
# 不是豁免：文件照扫，报告照列。
PATTERN_DEFINITION_FILES = frozenset({
    "eng/tools/quality/check_secret_hygiene.py",
    "tests/quality/test_secret_hygiene.py",
})

# 需要在捕获组上做占位符过滤的形态（占位符 → 不算命中）
VALUE_FILTERED = {"PASSWORD_ASSIGN", "SECRET_ASSIGN"}
# B64_LONG 需含非 hex 字母，避免与 HEX64 重复计数
B64_MIN_NONHEX = re.compile(r"[g-zG-Z+/]")


def is_sensitive_path(rel: str) -> bool:
    """路径级凭据类判定（只看文件名/扩展名，不看内容）。"""
    name = rel.replace("\\", "/").rsplit("/", 1)[-1]
    lower = name.lower()
    if lower in SENSITIVE_EXACT or lower.startswith(".env"):
        return True
    if pathlib.PurePosixPath(lower).suffix in SENSITIVE_EXT:
        return True
    return bool(SENSITIVE_TOKEN_RE.search(lower))


def _placeholder(value: str) -> bool:
    return bool(PLACEHOLDER_RE.match(value.strip().strip("\"'")))


# 赋值右侧为「变量/字段/索引/调用/CLI 选项」等引用形态 → 只记 INFO（不是字面量凭据）
REF_LIKE_RE = re.compile(r"[.\[\]()\"']")
SNAKE_OR_CONST_RE = re.compile(r"^(?:[a-z_][a-z0-9_]*|[A-Z][A-Z0-9_]*|[a-z]+[A-Z][A-Za-z]*)$")


def _ref_like(value: str) -> bool:
    """判定赋值右侧是否为变量引用/字段访问/调用/CLI 选项（非字面量）。"""
    v = value.strip().strip("\"'").rstrip(".,;:)]}")
    if not v:
        return True
    if v.startswith("-"):          # CLI 选项 / 短横线常量
        return True
    if REF_LIKE_RE.search(v):      # self.x / os.environ["X"] / f(...) / ".."
        return True
    return bool(SNAKE_OR_CONST_RE.match(v))   # snake_case / CONST_CASE / camelCase


def scan_text_ex(text: str) -> tuple[dict[str, list[int]], dict[str, list[int]]]:
    """逐行扫描；返回 (fatal_hits, soft_hits)，两者都只记 形态 ID → 行号（绝不记值）。"""
    hits: dict[str, list[int]] = {}
    soft: dict[str, list[int]] = {}
    for lineno, line in enumerate(text.splitlines(), 1):
        if not line or len(line) > 20000:
            continue
        for pid, _severity, rx in PATTERNS:
            m = rx.search(line)
            if not m:
                continue
            if pid in VALUE_FILTERED and m.groups():
                if _placeholder(m.group(1)):
                    continue
                if _ref_like(m.group(1)):
                    soft.setdefault(pid, []).append(lineno)
                    continue
            if pid == "B64_LONG" and not B64_MIN_NONHEX.search(m.group(0)):
                continue
            hits.setdefault(pid, []).append(lineno)
    return hits, soft


def scan_text(text: str) -> dict[str, list[int]]:
    """形态扫描（fatal ∪ soft）；用于审计与输出自检，只输出布尔/行号。"""
    hits, soft = scan_text_ex(text)
    merged = {k: list(v) for k, v in hits.items()}
    for k, v in soft.items():
        merged.setdefault(k, []).extend(v)
    return merged


def _decode(data: bytes) -> str:
    return data.decode("utf-8", errors="replace")


def _looks_binary(rel: str, data: bytes) -> bool:
    if pathlib.PurePosixPath(rel).suffix.lower() in BINARY_EXT:
        return True
    return b"\x00" in data[:8192]


def git_files(root: pathlib.Path) -> tuple[list[str], str | None]:
    """tracked 文件清单；失败返回 (.., 原因) —— 由调用方 fail-closed。"""
    try:
        proc = subprocess.run(["git", "-C", str(root), "ls-files", "-z"],
                              capture_output=True, timeout=300)
    except (OSError, subprocess.SubprocessError) as exc:  # pragma: no cover - 环境异常
        return [], f"git_unavailable:{type(exc).__name__}"
    if proc.returncode != 0:
        return [], "git_unavailable:rc=%d" % proc.returncode
    out = proc.stdout.decode("utf-8", errors="replace")
    return [p for p in out.split("\0") if p], None


def _still_tracked(root: pathlib.Path, rel: str) -> bool:
    """并发竞态判定：清单枚举后文件消失时，再问一次 git。

    仍 tracked（真丢文件）→ True（调用方按 fail-closed 判红）；
    git 判定不再是 tracked 条目（别的线刚删/刚移出索引）→ False（不再属于扫描集）。
    git 本身失败时返回 True —— 保持 fail-closed。
    """
    try:
        proc = subprocess.run(["git", "-C", str(root), "ls-files", "--error-unmatch", "--", rel],
                              capture_output=True, timeout=120)
    except (OSError, subprocess.SubprocessError):  # pragma: no cover - 环境异常
        return True
    return proc.returncode == 0


def pack_files(root: pathlib.Path, packer_path: pathlib.Path) -> tuple[list[str], str | None]:
    """审计包收录文件清单（直接复用打包器 allowed()，保证检查与打包同源）。"""
    import importlib.util
    if not packer_path.is_file():
        return [], "packer_unavailable:missing"
    spec = importlib.util.spec_from_file_location("_astrocs_pack_audit", packer_path)
    if spec is None or spec.loader is None:  # pragma: no cover
        return [], "packer_unavailable:spec"
    mod = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(mod)
    except Exception as exc:  # pragma: no cover - 打包器不可导入
        return [], f"packer_unavailable:{type(exc).__name__}"
    tracked, err = git_files(root)
    if err:
        return [], err
    kept = []
    for rel in tracked:
        try:
            ok, _dst = mod.allowed(rel)
        except Exception as exc:  # pragma: no cover - 打包器行为异常
            return [], f"packer_unavailable:allowed:{type(exc).__name__}"
        if ok:
            kept.append(rel)
    return kept, None


def walk_files(root: pathlib.Path) -> tuple[list[str], str | None]:
    """目录树扫描（测试与本地巡检用）；跳过 VCS/构建/运行产物目录。"""
    if not root.is_dir():
        return [], "walk_root_missing"
    skip_dirs = {".git", "build", "run", "logs", "third_party", "__pycache__", ".venv"}
    out = []
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        rel = path.relative_to(root).as_posix()
        if any(part in skip_dirs for part in rel.split("/")[:-1]):
            continue
        out.append(rel)
    return out, None


def build_report(scope, root, files, max_bytes, report_only, git_backed=False) -> tuple[dict, int]:
    undecidable: list[dict] = []
    results: list[dict] = []
    counts = {"files_in_set": len(files), "files_scanned": 0, "files_skipped_binary": 0,
              "files_oversize": 0, "files_with_critical": 0, "files_with_suspect": 0,
              "files_info_only": 0, "fatal_findings": 0, "files_vanished": 0}
    for rel in files:
        path = root / rel
        try:
            data = path.read_bytes()
        except FileNotFoundError:
            # 并发竞态：清单来自 git，枚举后文件被别的线删除/移出索引 → 不再属于扫描集；
            # 若 git 仍认为它 tracked（真丢文件）则按 fail-closed 判红。
            if git_backed and not _still_tracked(root, rel):
                counts["files_vanished"] += 1
            else:
                undecidable.append({"path": rel, "reason": "file_unreadable:FileNotFoundError"})
            continue
        except OSError as exc:
            undecidable.append({"path": rel, "reason": "file_unreadable:" + type(exc).__name__})
            continue
        if _looks_binary(rel, data):
            counts["files_skipped_binary"] += 1
            continue
        if len(data) > max_bytes:
            counts["files_oversize"] += 1
            undecidable.append({"path": rel, "reason": "oversize_unscanned"})
            continue
        counts["files_scanned"] += 1
        hits, soft = scan_text_ex(_decode(data))
        if not hits and not soft:
            continue
        sensitive = is_sensitive_path(rel)
        downgraded = rel in PATTERN_DEFINITION_FILES
        fatal: list[str] = []
        info: list[str] = []
        for pid, sev, _rx in PATTERNS:
            if pid in soft and pid not in hits:
                info.append(pid)          # 变量引用形态 → 只记 INFO
                continue
            if pid not in hits:
                continue
            if not downgraded and (sev == "CRITICAL" or (sev == "ESCALATE" and sensitive)):
                fatal.append(pid)
            else:
                info.append(pid)
        if fatal:
            counts["files_with_critical"] += 1
        elif info:
            counts["files_info_only"] += 1
        if any(p in hits for p in ("HEX32", "HEX40", "HEX64", "B64_LONG")):
            counts["files_with_suspect"] += 1
        counts["fatal_findings"] += len(fatal)
        if downgraded:
            counts["files_policy_downgraded"] = counts.get("files_policy_downgraded", 0) + 1
        results.append({
            "path": rel,
            "sensitive_class": sensitive,
            "policy_downgraded": downgraded,
            "patterns": {pid: hits[pid] for pid in sorted(hits)},
            "fatal_patterns": sorted(fatal),
            "info_patterns": sorted(info),
        })
    fatal_files = [r["path"] for r in results if r["fatal_patterns"]]
    verdict_reasons: list[str] = []
    if undecidable:
        verdict_reasons.append("undecidable_input")
    if counts["files_in_set"] == 0:
        verdict_reasons.append("empty_scan_set")
    if counts["files_scanned"] == 0 and counts["files_in_set"] > 0:
        verdict_reasons.append("nothing_scanned")
    if fatal_files and not report_only:
        verdict_reasons.append("secret_shape_detected")
    verdict = "PASS" if not verdict_reasons else "FAIL"
    report = {
        "check": CHECK_ID,
        "schema_version": SCHEMA_VERSION,
        "verdict": verdict,
        "verdict_reasons": verdict_reasons,
        "scope": scope,
        "root": str(root),
        "generated_at": datetime.datetime.now().astimezone().isoformat(timespec="seconds"),
        "report_only": report_only,
        "policy": {
            "critical_patterns": [p for p, s, _ in PATTERNS if s == "CRITICAL"],
            "escalating_patterns": [p for p, s, _ in PATTERNS if s == "ESCALATE"],
            "info_only_patterns": [p for p, s, _ in PATTERNS if s == "INFO"],
            "escalation_scope": "sensitive_path_class_only",
            "sensitive_path_rule": "basename 含 secret/credential/password/access 等标记，或扩展名 .env/.pem/.key/.ppk/.netrc/.p12/.pfx",
            "pattern_definition_files": sorted(PATTERN_DEFINITION_FILES),
            "pattern_definition_semantics": "规则本体与测试夹具：命中降级为 INFO（仍列 路径/形态/行号），不判红",
            "binary_scan_excluded_ext": sorted(BINARY_EXT),
        },
        "counts": counts,
        "files_with_findings": results,
        "fatal_paths": fatal_files,
        "undecidable": undecidable,
        "output_contains_values": False,
    }
    return report, (0 if verdict == "PASS" else 1)


# 路径类字段（结构元数据，允许含长字母数字串）只做「前缀/私钥头」形态自检，
# 不做 hex/base64 扫描——否则仓库路径（含 / 分隔符）会自泄误报。
PATH_PROBE_PATTERNS = {"PRIVATE_KEY_HEADER", "OPENSSH_PRIVATE_KEY", "PGP_PRIVATE_KEY_BLOCK",
                       "SK_API_TOKEN", "GITHUB_TOKEN", "AWS_ACCESS_KEY_ID", "GCP_API_KEY",
                       "SLACK_TOKEN", "JWT_TOKEN"}


def _scrub_paths(node, key: str = ""):
    """把报告中的路径类字段挖空（仅用于自泄自检的探针文本）。"""
    if isinstance(node, dict):
        return {k: _scrub_paths(v, k) for k, v in node.items()}
    if isinstance(node, list):
        return [_scrub_paths(v, key) for v in node]
    if isinstance(node, str) and (key == "root" or key.endswith("path") or key.endswith("paths")):
        return "<path>"
    return node


def _self_leak(report: dict) -> bool:
    """输出自检：报告序列化后不得含任何形态命中（路径字段用前缀形态单独扫）。"""
    probe = json.dumps(_scrub_paths(report), ensure_ascii=False)
    if scan_text(probe):
        return True
    for rel in _iter_paths(report):
        for pid, _sev, rx in PATTERNS:
            if pid in PATH_PROBE_PATTERNS and rx.search(rel):
                return True
    return False


def _iter_paths(report: dict) -> list[str]:
    out: list[str] = []

    def walk(node, key=""):
        if isinstance(node, dict):
            for k, v in node.items():
                walk(v, k)
        elif isinstance(node, list):
            for v in node:
                walk(v, key)
        elif isinstance(node, str) and (key == "root" or key.endswith("path") or key.endswith("paths")):
            out.append(node)
    walk(report)
    return out


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CHK-SECRET-HYGIENE 凭据卫生检查（只输出路径/布尔/行号）")
    ap.add_argument("--scope", choices=("pack", "tracked", "walk"), default="pack")
    ap.add_argument("--root", default=str(REPO))
    ap.add_argument("--files", nargs="*", default=None, help="显式文件清单（相对 --root）")
    ap.add_argument("--packer", default=str(DEFAULT_PACKER), help="打包器路径（scope=pack 时复用其 allowed()）")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--max-bytes", type=int, default=DEFAULT_MAX_BYTES)
    ap.add_argument("--report-only", action="store_true", help="只报告不因命中判红（无法判定仍判红）")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)

    root = pathlib.Path(args.root).resolve()
    fatal_setup: str | None = None
    if args.files is not None:
        files = list(args.files)
    elif args.scope == "pack":
        files, fatal_setup = pack_files(root, pathlib.Path(args.packer).resolve())
    elif args.scope == "tracked":
        files, fatal_setup = git_files(root)
    else:
        files, fatal_setup = walk_files(root)

    if fatal_setup:
        report = {"check": CHECK_ID, "schema_version": SCHEMA_VERSION, "verdict": "FAIL",
                  "verdict_reasons": [fatal_setup], "scope": args.scope, "root": str(root),
                  "generated_at": datetime.datetime.now().astimezone().isoformat(timespec="seconds"),
                  "counts": {"files_in_set": 0}, "files_with_findings": [], "undecidable": [],
                  "output_contains_values": False}
        rc = 1
    else:
        git_backed = args.files is None and args.scope in ("pack", "tracked")
        report, rc = build_report(args.scope, root, files, args.max_bytes, args.report_only,
                                  git_backed=git_backed)

    text = json.dumps(report, ensure_ascii=False, indent=2)
    if _self_leak(text):
        report = {"check": CHECK_ID, "schema_version": SCHEMA_VERSION, "verdict": "FAIL",
                  "verdict_reasons": ["output_self_leak"], "scope": args.scope, "root": str(root),
                  "counts": report.get("counts", {}), "files_with_findings": [], "undecidable": [],
                  "output_contains_values": False}
        text = json.dumps(report, ensure_ascii=False, indent=2)
        rc = 1

    if args.json_out:
        out = pathlib.Path(args.json_out)
        if not out.is_absolute():
            out = root / out
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text + "\n", encoding="utf-8")
    if args.quiet:
        print(json.dumps({"check": report["check"], "verdict": report["verdict"],
                          "verdict_reasons": report.get("verdict_reasons", []),
                          "counts": report.get("counts", {}), "fatal_paths": report.get("fatal_paths", [])},
                         ensure_ascii=False))
    else:
        print(text)
    return rc


if __name__ == "__main__":
    sys.exit(main())
