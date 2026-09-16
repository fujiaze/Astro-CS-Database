#!/usr/bin/env python3
"""extract_cpp_api.py — Authoritative API extractor (T301)

Extracts public C/C++ symbols from headers under lib/*/include/.
Prefers compile_commands.json + Clang AST if available (-Xclang -ast-dump=json),
fallback to header regex for offline listing. Output JSON with complete signatures.

判据输入域（ENGINEERING_SPEC.md §8，2026-09-16 增补「可复现」）:
- 输入**只取仓库根下 `lib/` 开头的路径**；显式排除 `run/`、`build/`、`artifacts/`、
  `third_party/`、`.git/` 等 gitignore 影子面。R-6 实测（2026-09-16）：旧实现的
  `repo.rglob("lib/*/include/**/*.h")` 会命中 `run/**` 内的历史副本，使「公共 API 符号」
  从干净树的 295 膨胀到 34454（98.9% 来自 `run/`）⇒ 判据**不可复现**。
- **fail-closed**: 扫描面为空（0 个头文件）⇒ exit 3，不得把「找不到」当「无符号」。
- 输出新增 `input_mode` / `roots` / `excluded_roots` 字段，标明本次判据的量测域。

Exit: 0 success, 2 env error, 3 input/schema error.
Supports: --repo, --out-json, --out-junit, --self-test
"""
import argparse, csv, json, os, re, sys, pathlib, tempfile, shutil

# 判据量测域：只接受仓库根下这些顶层目录内的头文件
ROOTS = ("lib",)
# 显式排除面（可复现性：这些目录在 .gitignore 内，是历史/构建副本）
EXCLUDED_ROOTS = ("run", "build", "artifacts", "third_party", ".git", "output", "logs")
# 头文件相对仓库根的 glob 模式（与旧实现同型，但随后强制 roots 限制）
HEADER_GLOBS = (
    "lib/*/include/**/*.h",
    "lib/*/include/*.h",
    "lib/*/cpp/include/**/*.h",
    "lib/*/*/include/**/*.h",
    "lib/algorithms/platesolve/cpp/ipv/include/*.h",
)

def dedupe_root(repo):
    return pathlib.Path(repo)

HEADER_RE_F = re.compile(r'^\s*(?:P2_API|AC_API|CC_EXPORT|DPSF_EXPORT|SNR_API)?\s*(?:[\w:]+\s+)+(\w+)\s*\([^;]*\)\s*;', re.M)
# Broader: capture function-like lines in include headers
FUNC_LINE_RE = re.compile(r'^\s*(?:extern\s+"C"\s*\{\s*)?(?:P2_API|AC_API|CC_EXPORT|DPSF_EXPORT|SNR_API|extern)?\s*([^\n;]*\b(\w+)\s*\([^;]*\)\s*;)', re.M)

def extract_from_header(path: pathlib.Path):
    text = path.read_text(encoding="utf-8", errors="ignore")
    # Remove block comments for clean scan
    text_nc = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    text_nc = re.sub(r'//.*', '', text_nc)
    results = []
    for m in re.finditer(r'^\s*(?:(P2_API|AC_API|CC_EXPORT|DPSF_EXPORT|SNR_API)\s+)?([A-Za-z_][\w\s\*\:\<\>\,\&]*?)\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*;\s*$', text_nc, re.M):
        prefix = (m.group(1) or "").strip()
        ret = m.group(2).strip()
        name = m.group(3).strip()
        full = m.group(0).strip().replace("\n"," ").strip()
        # Filter obvious non-API (keywords)
        if name in {"if","for","while","switch","return"}:
            continue
        if len(name) < 4 and not name.startswith(("aio_","ac_","cc_","dpsf_","snr_","p2_","sdet_","pc_","ipv_","gaia_","healpix_","aio","astro")):
            continue
        sig = re.sub(r'\s+', ' ', full)
        results.append({"symbol": name, "signature": sig, "header": str(path), "export": prefix or None})
    return results


def relative_posix(path, repo):
    try:
        return path.resolve().relative_to(repo.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def collect_headers(repo: pathlib.Path):
    """判据输入集合：roots 限制 + excluded 排除 + 去重。返回 (headers, excluded_hits)。"""
    repo = repo.resolve()
    candidates = []
    for pat in HEADER_GLOBS:
        candidates += sorted(repo.glob(pat)) if hasattr(repo, "glob") else []
    seen, uniq, excluded_hits = set(), [], 0
    for h in candidates:
        rel = relative_posix(h, repo)
        if rel in seen:
            continue
        seen.add(rel)
        top = rel.split("/", 1)[0]
        if top in EXCLUDED_ROOTS:
            excluded_hits += 1
            continue
        if not any(rel == r or rel.startswith(r + "/") for r in ROOTS):
            excluded_hits += 1
            continue
        # Exclude third_party / 内部实现头（与旧实现一致）
        if "third_party" in rel or "runtime_internal.h" in rel:
            excluded_hits += 1
            continue
        uniq.append(h)
    return uniq, excluded_hits


def extract(repo: pathlib.Path):
    uniq, excluded_hits = collect_headers(repo)
    rows = []
    for h in uniq:
        try:
            for r in extract_from_header(h):
                r["header"] = relative_posix(pathlib.Path(r["header"]), repo)
                rows.append(r)
        except Exception as e:
            print(f"warn: {h}: {e}", file=sys.stderr)
    # Dedupe by symbol+header
    dedup = {}
    for r in rows:
        k = (r["symbol"], r["header"])
        if k not in dedup:
            dedup[k] = r
    rows = sorted(dedup.values(), key=lambda x: (x["header"], x["symbol"]))
    out = {
        "tool": "extract_cpp_api",
        "input_mode": "repo-tree-lib-only",
        "roots": list(ROOTS),
        "excluded_roots": list(EXCLUDED_ROOTS),
        "excluded_candidates": excluded_hits,
        "headers_scanned": len(uniq),
        "symbols": rows,
        "count": len(rows),
    }
    return out


# ─────────────────────── 可执行负例面（--self-test） ───────────────────────
_HEADER_BODY = 'P2_API int demo_symbol_%s(int a, double b);\n'
_SHADOW_MARK = "shadow_only_symbol_%s"


def _mini_repo(root, *, lib_header=True, run_shadow=True, build_shadow=False):
    (root / "lib" / "alpha" / "include").mkdir(parents=True, exist_ok=True)
    if lib_header:
        (root / "lib" / "alpha" / "include" / "alpha_api.h").write_text(
            _HEADER_BODY % "lib", encoding="utf-8")
    if run_shadow:
        d = root / "run" / "shadow" / "lib" / "alpha" / "include"
        d.mkdir(parents=True, exist_ok=True)
        (d / "alpha_api.h").write_text(
            'P2_API int %s(int a);\n' % (_SHADOW_MARK % "run"), encoding="utf-8")
    if build_shadow:
        d = root / "build" / "shadow" / "lib" / "alpha" / "include"
        d.mkdir(parents=True, exist_ok=True)
        (d / "alpha_api.h").write_text(
            'P2_API int %s(int a);\n' % (_SHADOW_MARK % "build"), encoding="utf-8")


def _self_test():
    """正例 1 组 + 负例 4 组。返回 rc（0 = 全部符合预期）。"""
    failures = []
    with tempfile.TemporaryDirectory(prefix="extract-cpp-api-selftest-") as tmp:
        base = pathlib.Path(tmp)

        # 正例：lib/ 有头文件，run/ 有影子副本 ⇒ 只统计 lib/，且不含影子符号
        pos = base / "positive"
        _mini_repo(pos, build_shadow=True)
        out = extract(pos)
        syms = {s["symbol"] for s in out["symbols"]}
        if out["headers_scanned"] != 1 or out["count"] < 1:
            failures.append(f"正例应只扫到 1 个头文件，实得 headers={out['headers_scanned']} count={out['count']}")
        if "demo_symbol_lib" not in syms:
            failures.append(f"正例缺少 lib/ 符号：{sorted(syms)}")
        if any("shadow_only_symbol" in s for s in syms):
            failures.append(f"正例误收 run//build/ 影子符号：{sorted(syms)}")
        if out["input_mode"] != "repo-tree-lib-only":
            failures.append(f"正例 input_mode 异常：{out['input_mode']}")

        # 负例 1：只有 run/ 影子副本 ⇒ 扫描面为空 ⇒ fail-closed exit 3
        neg1 = base / "neg-only-shadow"
        _mini_repo(neg1, lib_header=False)
        if extract(neg1)["headers_scanned"] != 0:
            failures.append("负例 only-shadow：run/ 影子副本被计入扫描面")

        # 负例 2：只有 build/ 影子副本 ⇒ 扫描面为空
        neg2 = base / "neg-only-build"
        _mini_repo(neg2, lib_header=False, run_shadow=False, build_shadow=True)
        if extract(neg2)["headers_scanned"] != 0:
            failures.append("负例 only-build：build/ 影子副本被计入扫描面")

        # 负例 3：lib/ 下双层嵌套 include（旧 glob 的 lib/*/*/include 分支）仍被计入
        neg3 = base / "neg-nested"
        d = neg3 / "lib" / "beta" / "cpp" / "include" / "deep"
        d.mkdir(parents=True, exist_ok=True)
        (d / "beta_api.h").write_text(_HEADER_BODY % "nested", encoding="utf-8")
        if extract(neg3)["headers_scanned"] != 1:
            failures.append("负例 nested：lib/*/cpp/include/** 分支未被扫描（判据面收窄过度）")

    # 负例 4：--repo 指向不存在的路径 ⇒ 扫描面为空 ⇒ fail-closed
    missing = pathlib.Path(tempfile.gettempdir()) / "extract-cpp-api-selftest-absent-repo"
    if missing.exists():
        shutil.rmtree(missing, ignore_errors=True)
    if extract(missing)["headers_scanned"] != 0:
        failures.append("负例 absent-repo：不存在的仓库仍报出扫描面")

    if failures:
        print("SELFTEST_FAIL:")
        for f in failures:
            print("  " + f)
        return 1
    print("SELFTEST_PASS: 正例只统计 lib/ 且不含 run//build/ 影子符号；"
          "负例（only-shadow / only-build / absent-repo 扫描面为空；nested 分支仍计入）均按预期")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args()

    if args.self_test:
        return _self_test()

    repo = pathlib.Path(args.repo)
    if not repo.is_dir():
        print(f"EXTRACT_CPP_API_FAIL: --repo 不存在或不是目录: {repo}（fail-closed）", file=sys.stderr)
        return 3
    out = extract(repo)
    if out["headers_scanned"] == 0:
        print("EXTRACT_CPP_API_FAIL: 扫描面为空（ROOTS=" + ",".join(ROOTS) +
              " 下 0 个头文件）→ fail-closed，判据输入不可用", file=sys.stderr)
        return 3
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(out, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        print(json.dumps(out, indent=2, ensure_ascii=False))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_junit).write_text(f'<testsuite name="extract_cpp_api" tests="1" failures="0"><testcase classname="extract" name="scan"/></testsuite>', encoding="utf-8")
    return 0

if __name__ == "__main__":
    sys.exit(main())
