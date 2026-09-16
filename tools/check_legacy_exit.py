#!/usr/bin/env python3
"""LEG-002..004: 旧生产路径退出校验 (orchestrator/AIO PipelineEngine/old Stage2/ACR)。

规则 (LEG 规格: 逐个确认无 canonical caller、链接符号、文档入口、安装产物后退出):
1. 生产二进制 (build/root-cmake/astrocs) 不含目标目录符号。
2. 根 CMakeLists 不链入目标目录。
3. 文档入口标注退出或已清理。
4. 源码目录保留 (不破坏删除), 但无生产引用。

契约（ENGINEERING_SPEC.md §8，2026-09-16 增补）:
- **锚存活**: 本文件硬编码引用的仓库路径集中在 ANCHORS；任一失效 ⇒ 打印
  `ANCHOR_STALE: <常量名> <路径>` 并 `exit 2`，**不得 traceback、不得静默通过**。
- **fail-closed**: 生产二进制缺失 / nm 不可用 / 生产源码扫描面为空 / ACR 内核真源
  内容漂移 ⇒ 一律判红（exit 1），不得把「找不到」当「无违规」。
- **可执行负例面**: `--self-test` 内含 1 组正例 + 9 组注入负例。

exit 0 = PASS；exit 1 = 判据违规；exit 2 = 锚失效或依赖不可用（fail-closed）。
"""
import argparse
import pathlib
import shutil
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[1]

# ── 锚（硬编码引用的仓库路径；ARCH-001/ROOT-007 迁移后由 --self-test 与 ANCHOR_STALE 守护）──
ANCHORS = {
    "AIO_ENGINE_HEADER": "lib/infrastructure/aio/include/aio_pipeline_engine.h",
    "ACR_KERNEL_SOURCE": "lib/algorithms/coverage/src/acr_kernels.cpp",
    "ROOT_CMAKE": "CMakeLists.txt",
    "PUBLIC_API_DOC": "docs/contracts/PUBLIC_API.md",
}
BINARY_CANDIDATES = ("build/root-cmake/astrocs", "build/astrocs", "build/cli/astrocs")
PROD_SCAN_ROOTS = ("lib", "cli")
# ACR 内核真源文件名：本文件定义 register_phase2_acr_(kernels)，其余生产文件不得调用它
ACR_KERNEL_DEFINER = "acr_kernels.cpp"
NON_PROD_SEGMENTS = ("/tests/", "/tools/", "/testdata/", "/fixtures/", "/benchmarks/")


def anchor_errors(repo):
    """锚存活前置断言（§8）。返回失效清单；非空 ⇒ 调用方必须 exit 2。"""
    return [f"ANCHOR_STALE: {name} {rel}" for name, rel in ANCHORS.items()
            if not (repo / rel).exists()]


def find_binary(repo):
    return next((repo / p for p in BINARY_CANDIDATES if (repo / p).exists()), None)


def binary_symbols(bin_path):
    """生产二进制符号面。nm 不可用 ⇒ 返回 None（调用方 fail-closed 判红）。"""
    if shutil.which("nm") is None:
        return None
    proc = subprocess.run(["nm", str(bin_path)], capture_output=True, text=True)
    return proc.stdout.lower()


def prod_sources(repo):
    """生产源码集合（lib/ 与 lib/infrastructure/cli/ 下 *.cpp，排除 tests/tools/fixtures/benchmarks）。"""
    out = []
    for rel in PROD_SCAN_ROOTS:
        root = repo / rel
        if not root.exists():
            continue
        for f in sorted(root.rglob("*.cpp")):
            posix = "/" + f.relative_to(repo).as_posix()
            if any(seg in posix for seg in NON_PROD_SEGMENTS):
                continue
            out.append(f)
    return out


def check(repo, symbols_fn=binary_symbols):
    """跑全部 LEG-002..004 判据。返回 (errors, stats)。"""
    errors, stats = [], {}

    # ── 锚存活前置断言 ──
    stale = anchor_errors(repo)
    if stale:
        return stale, stats

    # ── 生产二进制 ──
    bin_path = find_binary(repo)
    stats["binary"] = str(bin_path.relative_to(repo)) if bin_path else None
    if bin_path is None:
        # GAP-027 fail-closed（CI-001）：原实现路径漂移后整段 nm 符号扫描静默跳过仍 PASS
        errors.append("未找到生产二进制（候选 build/root-cmake/astrocs、build/astrocs、"
                      "build/cli/astrocs）→ 符号面无法验证，fail-closed 判 FAIL")
        syms = None
    else:
        syms = symbols_fn(bin_path)
        if syms is None:
            errors.append("nm 不可用 → 生产符号面无法验证，fail-closed 判 FAIL")
    stats["symbols_checked"] = syms is not None

    # ── LEG-002: 旧 Orchestrator ──
    if syms is not None and "orchestrat" in syms:
        errors.append("production binary contains orchestrator symbol: orchestrat")
    cmake = (repo / ANCHORS["ROOT_CMAKE"]).read_text(encoding="utf-8", errors="ignore")
    if "orchestrator" in cmake:
        errors.append("root CMake references orchestrator (LEG-002)")
    public_api = (repo / ANCHORS["PUBLIC_API_DOC"]).read_text(encoding="utf-8", errors="ignore")
    if "orchestrator.exe" in public_api and "LEG-002" not in public_api:
        errors.append("PUBLIC_API orchestrator.exe 未标 LEG-002 退出")

    # ── LEG-003: AIO PipelineEngine 调度职责 ──
    # (engine run API 无生产 caller; frame 数据结构 API 保留供 CLI drizzle 测试 wrapper 用)
    eng_h = (repo / ANCHORS["AIO_ENGINE_HEADER"]).read_text(encoding="utf-8", errors="ignore")
    engine_run_decl = ("aio_pipeline_engine_run_single" in eng_h or
                       "aio_pipeline_engine_run_batch" in eng_h)
    sources = prod_sources(repo)
    acr_src = repo / ANCHORS["ACR_KERNEL_SOURCE"]
    scan_face = [f for f in sources if f != acr_src]
    stats["prod_sources_scanned"] = len(sources)
    if not engine_run_decl:
        errors.append("AIO PipelineEngine run API 声明缺失（"
                      f"{ANCHORS['AIO_ENGINE_HEADER']}）→ 判据面漂移，fail-closed 判 FAIL")
    if not scan_face:
        errors.append("生产源码扫描面为空（lib/、lib/infrastructure/cli/ 下除 ACR 内核真源外无 *.cpp）"
                      "→ fail-closed 判 FAIL")
    caller_files = []
    for f in sources:
        if f.name == "aio_pipeline_engine.cpp":
            continue
        if "aio_pipeline_engine_run" in f.read_text(encoding="utf-8", errors="ignore"):
            caller_files.append(f.relative_to(repo).as_posix())
    if engine_run_decl and caller_files:
        errors.append(f"PipelineEngine run API has callers: {caller_files} (LEG-003)")
    stats["leg003_callers"] = caller_files

    # ── LEG-004: 旧 Stage2 工具 + ACR 隔离 ──
    # (a) 旧 stage2.cpp 工具不随根构建产出 (无安装产物)
    if (repo / "build" / "root-cmake" / "astrocs-stage2").exists():
        errors.append("astrocs-stage2 旧工具被生产构建产出 (LEG-004)")

    # (b) ACR dormant: ASTROCS_ENABLE_ACR=OFF; 生产二进制符号面无 acr
    if "ASTROCS_ENABLE_ACR" not in cmake:
        errors.append("ACR option 缺失 (LEG-004)")
    if syms is not None and "acr" in syms:
        errors.append("生产二进制含 ACR 符号 (LEG-004)")

    # (c) ACR 内核注册真源内容漂移 + 生产路径无 ACR 注册调用
    acr_txt = acr_src.read_text(encoding="utf-8", errors="ignore")
    if "register_phase2_acr" not in acr_txt:
        errors.append(f"ACR 内核注册真源内容漂移：{ANCHORS['ACR_KERNEL_SOURCE']} "
                      "不含 register_phase2_acr (LEG-004) → fail-closed 判 FAIL")
    for f in sources:
        if f.name == ACR_KERNEL_DEFINER:
            continue
        if "register_phase2_acr" in f.read_text(encoding="utf-8", errors="ignore"):
            errors.append(f"ACR kernel 注册存在: {f.relative_to(repo).as_posix()} "
                          "(LEG-004: 应 dormant 不注册)")
    return errors, stats


# ─────────────────────────── 可执行负例面（--self-test） ───────────────────────────
_CMAKE_OK = "option(ASTROCS_ENABLE_ACR \"Build dormant ACR tree\" OFF)\n"
_AIO_OK = "AIO_EXPORT int aio_pipeline_engine_run_single(PipelineEngine* e);\n"
_ACR_OK = "void register_phase2_acr_kernels() {}\n"


def _build_mini_repo(root, *, with_caller=False, with_acr_reg=False, drop_cmake_token=False,
                     drop_anchor=None, drop_prod_sources=False, cmake_orchestrator=False,
                     public_api_orchestrator=False, binary_symbol="benign_symbol"):
    (root / "lib" / "infrastructure" / "aio" / "include").mkdir(parents=True, exist_ok=True)
    (root / "lib" / "algorithms" / "coverage" / "src").mkdir(parents=True, exist_ok=True)
    (root / "lib" / "other" / "src").mkdir(parents=True, exist_ok=True)
    (root / "docs" / "contracts").mkdir(parents=True, exist_ok=True)
    (root / "build").mkdir(parents=True, exist_ok=True)
    (root / ANCHORS["ROOT_CMAKE"]).write_text(
        ("" if drop_cmake_token else _CMAKE_OK) +
        ("target_link_libraries(x orchestrator)\n" if cmake_orchestrator else ""),
        encoding="utf-8")
    (root / ANCHORS["PUBLIC_API_DOC"]).write_text(
        "orchestrator.exe\n" + ("" if public_api_orchestrator else "LEG-002 retired\n"),
        encoding="utf-8")
    (root / ANCHORS["AIO_ENGINE_HEADER"]).write_text(_AIO_OK, encoding="utf-8")
    (root / ANCHORS["ACR_KERNEL_SOURCE"]).write_text(_ACR_OK, encoding="utf-8")
    if not drop_prod_sources:
        (root / "lib" / "other" / "src" / "a.cpp").write_text("int f(){return 0;}\n", encoding="utf-8")
    if with_caller:
        (root / "lib" / "other" / "src" / "caller.cpp").write_text(
            "int g(){return aio_pipeline_engine_run_single(0);}\n", encoding="utf-8")
    if with_acr_reg:
        (root / "lib" / "other" / "src" / "acr_user.cpp").write_text(
            "void h(){register_phase2_acr_kernels();}\n", encoding="utf-8")
    if drop_anchor:
        (root / drop_anchor).unlink()
    src = root / "_probe.c"
    src.write_text(f"int {binary_symbol}(void){{return 0;}}\n", encoding="utf-8")
    subprocess.run(["gcc", "-shared", "-fPIC", "-o", str(root / "build" / "astrocs"), str(src)],
                   capture_output=True)
    src.unlink()


def _self_test():
    """正例 1 组 + 负例 9 组。返回 rc（0 = 全部符合预期）。"""
    if shutil.which("gcc") is None or shutil.which("nm") is None:
        print("SELFTEST_FAIL: 需要 gcc 与 nm 才能构造二进制符号面负例（fail-closed）")
        return 2
    failures = []
    with tempfile.TemporaryDirectory(prefix="legacy-exit-selftest-") as tmp:
        base = pathlib.Path(tmp)

        pos = base / "positive"
        _build_mini_repo(pos)
        errs, _ = check(pos)
        if errs:
            failures.append(f"正例应 rc=0，实得 errors={errs}")

        cases = [
            ("leg003-caller-injected", dict(with_caller=True), "PipelineEngine run API has callers"),
            ("leg004-acr-symbol", dict(binary_symbol="acr_probe_symbol"), "生产二进制含 ACR 符号"),
            ("leg004-acr-option-missing", dict(drop_cmake_token=True), "ACR option 缺失"),
            ("leg004-acr-registration", dict(with_acr_reg=True), "ACR kernel 注册存在"),
            ("leg002-cmake-token", dict(cmake_orchestrator=True), "root CMake references orchestrator"),
            ("leg002-public-api", dict(public_api_orchestrator=True), "PUBLIC_API orchestrator.exe"),
            ("failclosed-scan-empty", dict(drop_prod_sources=True), "生产源码扫描面为空"),
        ]
        for name, kwargs, expect in cases:
            d = base / name
            _build_mini_repo(d, **kwargs)
            errs, _ = check(d)
            if not errs:
                failures.append(f"负例 {name} 未变红（预期含『{expect}』）")
            elif not any(expect in e for e in errs):
                failures.append(f"负例 {name} 变红但未命中『{expect}』：{errs}")

        for name, drop in (("anchor-aio-header", ANCHORS["AIO_ENGINE_HEADER"]),
                           ("anchor-acr-source", ANCHORS["ACR_KERNEL_SOURCE"]),
                           ("anchor-root-cmake", ANCHORS["ROOT_CMAKE"]),
                           ("anchor-public-api", ANCHORS["PUBLIC_API_DOC"])):
            d = base / name
            _build_mini_repo(d, drop_anchor=drop)
            errs, _ = check(d)
            if not errs or not all(e.startswith("ANCHOR_STALE:") for e in errs):
                failures.append(f"锚失效负例 {name} 未产出 ANCHOR_STALE：{errs}")

    if failures:
        print("SELFTEST_FAIL:")
        for f in failures:
            print("  " + f)
        return 1
    print("SELFTEST_PASS: 正例 rc=0；9 组负例（caller/ACR 符号/ACR option/ACR 注册/CMake token/"
          "PUBLIC_API/空扫描面 + 4 个锚失效）均按预期变红")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="LEG-002..004 旧生产路径退出校验")
    ap.add_argument("--repo", default=None, help="仓库根（默认：本文件上一级）")
    ap.add_argument("--self-test", action="store_true", dest="self_test",
                    help="跑内置正例 + 注入负例，验证本检查器能红能绿")
    args = ap.parse_args(argv)

    if args.self_test:
        return _self_test()

    repo = pathlib.Path(args.repo).resolve() if args.repo else REPO
    stale = anchor_errors(repo)
    if stale:
        print("ANCHOR_STALE:")
        for e in stale:
            print("  " + e)
        return 2

    errors, stats = check(repo)
    if errors:
        print("LEGACY_EXIT_VIOLATION:")
        for e in errors:
            print("  " + e)
        return 1
    print("LEGACY_EXIT_PASS: 旧路径无生产符号/CMake/文档入口, 源码保留; ACR dormant 隔离"
          f" (binary={stats.get('binary')}, prod_sources={stats.get('prod_sources_scanned')})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
