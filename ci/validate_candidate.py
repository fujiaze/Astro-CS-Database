#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci/validate_candidate.py —— V8-CI-007 Windows candidate 包结构校验门禁。

用法：``python3 ci/validate_candidate.py <candidate.zip> [--json]``

被校验对象是 ``tools/quality/ci_windows_driver.py``（V8-CI-006）package
阶段产出的 candidate zip（建议路径 artifacts/candidate/AstroCS-candidate.zip，
由该 driver 的 ``--zip`` 打包）。校验范围（全部结构/哈希级，不运行产物）：

1. zip 存在且可读；
2. 三个清单在 zip 根：BUILD_PROVENANCE.json / SOURCE_MANIFEST.json / SHA256SUMS；
3. SOURCE_MANIFEST.json：``files[]`` 每项含非空 ``path`` 与 64 位 ``sha256``；
   path 命中 build-cache/testdata/Testing 等排除模式即违规（SOURCE_MANIFEST
   描述源文件——.py/.c/.h 等源码后缀合法，仅登记不复制进 zip）；
4. SHA256SUMS：``<64hex>  <relpath>`` 行格式；每行与 zip 内实际文件逐一
   比对 SHA256；zip 内除 SHA256SUMS 自身外的每个成员（含
   BUILD_PROVENANCE.json、SOURCE_MANIFEST.json）都必须有对应行（双向完备）；
5. zip 成员本身不得命中排除模式（源码 .py/.h/.cpp/.c/.hpp/.f90 等、
   测试数据 testdata、构建缓存 CMakeFiles/CMakeCache.txt/Testing 等）；
6. BUILD_PROVENANCE.json 必填字段：``source_sha`` / ``built_utc`` /
   ``preset`` / ``acr_enabled``（driver 字段名，acr 开关）；
7. 路径逃逸（V8-CI-009 GAP-G3 补齐）：zip 成员名与 SHA256SUMS 登记的
   relpath 经 PurePosixPath 判定不得为绝对路径或含 ``..`` 段
   （code=``path_escape_in_zip``，防 zip-slip 解包逃逸）。

任一违规 -> stdout/stderr 结构化错误 + exit 1（无 traceback）；
全部通过 -> PASS 摘要 + exit 0。``--json`` 输出完整机读报告。
排除模式为自持副本，与 driver 的 EXCLUDE_* 常量对齐（注释标明来源），
避免跨任务模块耦合。
"""
from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import re
import sys
import zipfile
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath

SCHEMA_VERSION = 1
TASK_ID = "V8-CI-007"

MANIFEST_NAMES = ("BUILD_PROVENANCE.json", "SOURCE_MANIFEST.json", "SHA256SUMS")

# ---- 排除规则（对齐 tools/quality/ci_windows_driver.py EXCLUDE_*，自持副本）----
# SOURCE_MANIFEST 描述源文件（.py/.c/.h 等合法，仅登记不复制进 zip）：
# 其 path 检查只针对 build-cache/testdata 模式；源码后缀排除仅适用于 zip 成员。
MANIFEST_EXCLUDE_PATTERNS = (
    "CMakeCache.txt", "CMakeFiles/*", "CMakeFiles", "CTestTestfile.cmake",
    "Testing/*", "Testing", "install_manifest.txt",
    "build-cache/*", "build-cache",
)
MANIFEST_EXCLUDE_DIR_PARTS = ("CMakeFiles", "Testing", "testdata",
                              "test_data", ".dSYM", "build-cache")
EXCLUDE_PATTERNS = MANIFEST_EXCLUDE_PATTERNS + (   # zip 成员额外规则
    "*.log", "*.ilk",
    "Makefile", "cmake_install.cmake", "*.obj", "*.lib", "*.exp",
)
EXCLUDE_SUFFIXES = (          # 源码/脚本/测试数据类后缀（candidate 不含源码）
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl",
    ".f90", ".f", ".for",
    ".py", ".pyc", ".sh", ".ps1", ".bat", ".cmd",
    ".fits", ".fit", ".xisf", ".npy", ".npz", ".h5", ".hdf5",
)
EXCLUDE_DIR_PARTS = ("CMakeFiles", "Testing", "testdata", "test_data",
                     ".dSYM", "build-cache")

SUMS_LINE_RE = re.compile(r"^([0-9a-f]{64})  (.+)$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")

PROVENANCE_REQUIRED = ("source_sha", "built_utc", "preset", "acr_enabled")

_EXIT_OK = 0
_EXIT_FAIL = 1


def utc_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


# ------------------------------------------------------------------ 排除规则 ----

def exclusion_hit(relpath: str, *, manifest: bool = False) -> str | None:
    """zip 成员 / SOURCE_MANIFEST path 是否命中排除规则；返回原因。

    manifest=True 时仅按 build-cache/testdata 模式判
    （源码后缀对 SOURCE_MANIFEST path 合法——描述的是源文件清单）。
    """
    pure = PurePosixPath(relpath)
    if not manifest:
        suffix = pure.suffix.lower()
        if suffix in EXCLUDE_SUFFIXES:
            return f"excluded suffix {suffix}"
    dir_parts = MANIFEST_EXCLUDE_DIR_PARTS if manifest else EXCLUDE_DIR_PARTS
    for part in pure.parts[:-1]:
        if part in dir_parts:
            return f"excluded dir part {part}"
    patterns = MANIFEST_EXCLUDE_PATTERNS if manifest else EXCLUDE_PATTERNS
    for pattern in patterns:
        if fnmatch.fnmatch(relpath, pattern):
            return f"excluded pattern {pattern}"
    return None


# -------------------------------------------------------------------- 校验器 ----

def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _zip_path_escape(relpath: str) -> str | None:
    """zip 成员 / SHA256SUMS 登记路径的逃逸判定（V8-CI-009 GAP-G3 补齐）。

    绝对路径或含 ``..`` 段即逃逸（zip-slip），返回原因；否则返回 None。
    """
    pure = PurePosixPath(relpath)
    if pure.is_absolute():
        return "absolute path"
    if ".." in pure.parts:
        return "'..' segment"
    return None


def validate_zip(zip_path: Path) -> dict:
    """完整校验一个 candidate zip，返回机读报告（verdict/errors/counts）。"""
    errors: list[dict] = []

    def err(code: str, detail: str) -> None:
        errors.append({"code": code, "detail": detail})

    if not zip_path.is_file():
        err("zip_unreadable", f"candidate zip 不存在：{zip_path}")
        return _report(zip_path, errors, counts={"zip_entries": 0})
    try:
        with zipfile.ZipFile(zip_path) as zf:
            names = [n for n in zf.namelist() if not n.endswith("/")]
            blobs = {n: zf.read(n) for n in names}
    except (zipfile.BadZipFile, OSError, RuntimeError) as exc:
        err("zip_unreadable", f"candidate zip 无法读取：{exc}")
        return _report(zip_path, errors, counts={"zip_entries": 0})

    counts = {"zip_entries": len(names)}

    # 2. zip 成员路径逃逸（GAP-G3）：绝对路径 / '..' 段在任何内容判定之前拒绝
    for member in sorted(names):
        reason = _zip_path_escape(member)
        if reason:
            err("path_escape_in_zip",
                f"zip 成员路径逃逸（{reason}）：{member}")

    # 3. 三个清单在 zip 根
    name_set = set(names)
    for manifest in MANIFEST_NAMES:
        if manifest not in name_set:
            err("manifest_missing", f"zip 根缺少清单文件 {manifest}")

    # 3. SOURCE_MANIFEST 结构 + 排除模式
    manifest_paths: set[str] = set()
    if "SOURCE_MANIFEST.json" in blobs:
        try:
            src_manifest = json.loads(blobs["SOURCE_MANIFEST.json"])
        except json.JSONDecodeError as exc:
            err("source_manifest_invalid",
                f"SOURCE_MANIFEST.json 非法 JSON：{exc}")
        else:
            files = src_manifest.get("files") if isinstance(src_manifest, dict) else None
            if not isinstance(files, list) or not files:
                err("source_manifest_invalid",
                    "SOURCE_MANIFEST.json.files 必须是非空数组")
            else:
                for i, entry in enumerate(files):
                    if not isinstance(entry, dict):
                        err("source_manifest_invalid",
                            f"files[{i}] 必须是 object")
                        continue
                    path = entry.get("path")
                    sha = entry.get("sha256")
                    if not isinstance(path, str) or not path.strip():
                        err("source_manifest_invalid",
                            f"files[{i}].path 必须是非空字符串")
                        continue
                    if not isinstance(sha, str) or not SHA256_RE.match(sha):
                        err("source_manifest_invalid",
                            f"files[{i}].sha256 必须是 64 位十六进制：{path}")
                        continue
                    manifest_paths.add(path)
                    hit = exclusion_hit(path, manifest=True)
                    if hit:
                        err("source_manifest_excluded",
                            f"SOURCE_MANIFEST path 命中排除规则（{hit}）：{path}")

    # 4. SHA256SUMS 行格式 + 与 zip 实际文件逐一比对 + 双向完备
    sums_entries: dict[str, str] = {}
    if "SHA256SUMS" in blobs:
        lines = blobs["SHA256SUMS"].decode("utf-8", errors="replace").splitlines()
        for lineno, line in enumerate(lines, start=1):
            if not line.strip():
                err("sums_invalid", f"SHA256SUMS 第 {lineno} 行为空行")
                continue
            m = SUMS_LINE_RE.match(line)
            if not m:
                err("sums_invalid",
                    f"SHA256SUMS 第 {lineno} 行格式非法（应为 '<64hex>  <relpath>'）：{line!r}")
                continue
            sums_entries[m.group(2)] = m.group(1)
        if not lines:
            err("sums_invalid", "SHA256SUMS 为空（candidate 无任何登记产物）")
        # 每行 -> zip 实际文件（GAP-G3：登记路径本身先做逃逸判定）
        for relpath, expected in sorted(sums_entries.items()):
            reason = _zip_path_escape(relpath)
            if reason:
                err("path_escape_in_zip",
                    f"SHA256SUMS 登记路径逃逸（{reason}）：{relpath}")
                continue
            if relpath not in blobs:
                err("sums_missing_entry",
                    f"SHA256SUMS 登记的文件不在 zip 内：{relpath}")
                continue
            observed = _sha256_bytes(blobs[relpath])
            if observed != expected:
                err("sums_hash_mismatch",
                    f"SHA256 不一致：{relpath} sums={expected} actual={observed}")
        # zip 实际文件 -> 每行（含 BUILD_PROVENANCE.json 本身）
        for member in sorted(n for n in names if n != "SHA256SUMS"):
            if member not in sums_entries:
                err("sums_missing_entry",
                    f"zip 内文件未在 SHA256SUMS 登记：{member}")

    # 5. zip 成员本身不得命中排除模式
    for member in sorted(names):
        hit = exclusion_hit(member)
        if hit:
            err("excluded_entry_in_zip",
                f"zip 成员命中排除规则（{hit}）：{member}")

    # 6. BUILD_PROVENANCE 必填字段
    if "BUILD_PROVENANCE.json" in blobs:
        try:
            provenance = json.loads(blobs["BUILD_PROVENANCE.json"])
        except json.JSONDecodeError as exc:
            err("provenance_invalid", f"BUILD_PROVENANCE.json 非法 JSON：{exc}")
        else:
            if not isinstance(provenance, dict):
                err("provenance_invalid", "BUILD_PROVENANCE.json 必须是 object")
            else:
                for field in PROVENANCE_REQUIRED:
                    if field not in provenance:
                        err("provenance_missing_field",
                            f"BUILD_PROVENANCE.json 缺少必填字段 {field}")
                preset = provenance.get("preset")
                if "preset" in provenance and not isinstance(preset, dict):
                    err("provenance_invalid", "BUILD_PROVENANCE.json.preset 必须是 object")

    counts["manifest_files"] = len(manifest_paths)
    counts["sums_lines"] = len(sums_entries)
    return _report(zip_path, errors, counts=counts)


def _report(zip_path: Path, errors: list[dict], *, counts: dict) -> dict:
    return {
        "schema_version": SCHEMA_VERSION,
        "task_id": TASK_ID,
        "generated_utc": utc_iso(),
        "zip": str(zip_path),
        "verdict": "PASS" if not errors else "FAIL",
        "errors": errors,
        "counts": counts,
    }


# --------------------------------------------------------------------- CLI ----

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="ci/validate_candidate.py",
        description="V8-CI-007：校验 Windows candidate zip（清单/SHA256SUMS/排除规则/溯源字段）。")
    parser.add_argument("candidate", help="candidate zip 路径")
    parser.add_argument("--json", action="store_true",
                        help="stdout 输出完整 JSON 报告")
    args = parser.parse_args(argv)

    report = validate_zip(Path(args.candidate))
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))

    if report["verdict"] == "PASS":
        if not args.json:
            counts = report["counts"]
            print(f"validate_candidate: PASS ({args.candidate}: "
                  f"{counts.get('zip_entries', 0)} entries, "
                  f"{counts.get('sums_lines', 0)} sums lines)")
        return _EXIT_OK

    if not args.json:
        for item in report["errors"]:
            print(f"validate_candidate: FAIL [{item['code']}] {item['detail']}",
                  file=sys.stderr)
    else:
        print(f"validate_candidate: FAIL ({len(report['errors'])} errors)",
              file=sys.stderr)
    return _EXIT_FAIL


if __name__ == "__main__":
    sys.exit(main())
