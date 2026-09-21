#!/usr/bin/env python3
"""check_execution_contracts.py — T404 execution contracts checker (V6.1)

Checks (V6.1 架构, 替代旧 OpenMP 宏/ACR CUDA 检查):
- THREAD_BUDGET_ARCH.md 契约 (ARCH-THREAD-001 FROZEN: budget 单一来源 / lease /
  Σ active worker ≤ budget) 存在且含关键 ID;
- 生产 sampler/upm/p3_session 并行轴: std::thread + Runtime lease
  (cfg.cpu_workers=budget.max_workers), 无 hardware_concurrency 自取;
- 禁止串行化负面模式 (P2_ENABLE_OPENMP 宏守卫已由 V6.1 std::thread 替代,
  保留其 CMake -fopenmp 接线证据);
- **AIO 读路径线程模型 (PERF-401 取代 EXEC-NO-AIO-SERIAL)**: 旧不变式
  「aio 读由 cfitsio_io_mutex / g_aio_mu 进程级锁串行化」已被取代 —— 规范出处
  docs/architecture/EXECUTION_MODEL.md §2/§3 + §8 ARC-EXEC-002，依据
  ASTROCS_DESIGN.md §9（串行 I/O 与控制面单线程 / 编排连续性）与
  ACCEPTANCE_SPEC.md §4（无单线程跑满全程、无连续 ≥10 s 低利用窗）。
  新不变式（更强、可机器判定）:
    (a) EXEC-AIO-READ-GLOBAL-LOCK   四个 tile 读函数体内零锁获取原语;
    (b) EXEC-AIO-READ-SHARED-HANDLE AioHipsDataset 结构体不得缓存 fitsfile
        （句柄不得驻留在跨线程共享的数据集对象里）;
    (c) EXEC-AIO-READ-HANDLE-ESCAPE 每个 fits_open_file 所在函数必须有
        fits_close_file（句柄生命周期不跨线程/调用转移）;
    (d) EXEC-AIO-LOCK-UNOBSERVABLE  剩余串行化点必须走计数式 CfitsioLockGuard
        （锁等待进 resource_timeseries.csv 的 lock_wait_ns，可观测）;
    (e) EXEC-SMP-GLOBAL-READ-LOCK   sampler 读路径不得有进程级锁;
    (f) EXEC-AIO-READ-CHECK-VACUOUS 反向自证: 四个读函数必须真实存在且
        fits_open_file ≥ 4（防符号改名后判据静默退化）。
  负例面: `--self-test` 对 (a)..(f) 逐条注入回归并断言判红。
- 无 ACR 生产接入 (ACR DORMANT_NOT_IN_PRODUCTION)。

Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error
"""
import argparse, json, pathlib, re, shutil, sys, tempfile

V6_1_THREAD_DOC_IDS = ["ARCH-THREAD-001", "budget", "lease"]

# 四个 tile 读函数：PERF-401 后必须零锁（读路径无进程级共享可变状态）。
TILE_READ_FUNCS = ("read_tile_t", "read_tile_pixel_t", "read_tile_i32",
                   "load_tiles_from_moc")
LOCK_TOKENS = ("CfitsioLockGuard", "lock_guard", "unique_lock", "scoped_lock",
               "cfitsio_io_mutex", "std::mutex", "pthread_mutex_lock", "g_aio_mu")


def _strip_comment(line):
    """去掉整行注释与行尾 // 注释（token 判定只看代码）。"""
    s = line.strip()
    if s.startswith("//") or s.startswith("*") or s.startswith("/*"):
        return ""
    i = line.find("//")
    return line[:i] if i >= 0 else line


def _func_span(text, name):
    """取 `name(` **定义**（行首起，排除调用点）的大括号体 [start,end)；找不到返回 None。"""
    m = re.search(r"(?m)^[A-Za-z_][\w:<>,*&\s]*?\b" + re.escape(name) + r"\s*\(",
                  text)
    if not m:
        return None
    i = text.find("{", m.end())
    if i < 0:
        return None
    depth = 0
    for j in range(i, len(text)):
        c = text[j]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return i, j + 1
    return None


def _func_body(text, name):
    span = _func_span(text, name)
    return None if span is None else text[span[0]:span[1]]


def _struct_body(text, name):
    m = re.search(r"\bstruct\s+" + re.escape(name) + r"\b", text)
    if not m:
        return None
    i = text.find("{", m.end())
    if i < 0:
        return None
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i:j + 1]
    return None


def run_checks(repo: pathlib.Path):
    """返回 (status, findings)。repo 可为真实仓库或 --self-test 的临时根。"""
    findings = []
    status = "PASS"

    def fail(fid, sev, obs, exp, file=None):
        nonlocal status
        status = "FAIL"
        f = {"id": fid, "severity": sev, "observed": obs, "expected": exp}
        if file is not None:
            try:
                f["file"] = str(pathlib.Path(file).relative_to(repo))
            except ValueError:
                f["file"] = str(file)
        findings.append(f)

    # 1) V6.1 权威线程文档: THREAD_BUDGET_ARCH.md 必须存在且含契约 ID
    tb = repo / "docs/architecture/THREAD_BUDGET_ARCH.md"
    if not tb.exists():
        fail("EXEC-MISSING-THREADBUDGET", "P1", "THREAD_BUDGET_ARCH.md missing", "exists")
    else:
        text = tb.read_text(encoding="utf-8", errors="ignore")
        for ident in V6_1_THREAD_DOC_IDS:
            if ident not in text:
                fail("EXEC-MISSING-ID", "P1", f"{ident} not in THREAD_BUDGET_ARCH.md", "exists", tb)
        if "worker" not in text:
            fail("EXEC-NO-WORKER-BUDGET", "P1", "no worker/budget constraint in THREAD_BUDGET_ARCH.md",
                 "Σ(活动 worker) ≤ budget", tb)

    # 2) 生产并行轴: sampler/upm/p3_session 经 Runtime lease (std::thread), 无自取
    for rel, needle in (
        ("lib/algorithms/coverage/src/sampler.cpp", "std::thread"),
        ("lib/algorithms/coverage/src/upm.cpp", "std::thread"),
        ("lib/phase3_session/p3_session.cpp", "std::thread"),
    ):
        p = repo / rel
        if not p.exists():
            continue
        t = p.read_text(encoding="utf-8", errors="ignore")
        if needle not in t:
            fail("EXEC-NO-PARALLEL-AXIS", "P1", f"{needle} not in {rel}", "exists", p)
        if not re.search(r"budget\.max_workers|cfg\.cpu_workers|n_workers\s*=.*budget", t):
            fail("EXEC-NO-LEASE", "P1", f"no Runtime lease injection in {rel}", "budget lease", p)
        for m in re.finditer(r"hardware_concurrency\s*\(", t):
            ln = t[:m.start()].count("\n") + 1
            line = t.splitlines()[ln - 1].strip()
            if not (line.startswith("//") or "无 hardware_concurrency" in line):
                fail("EXEC-HW-CONCURRENCY", "P1", f"hardware_concurrency() in {rel}:{ln}",
                     "Runtime lease only", p)

    # 3) AIO 读路径线程模型 (PERF-401)
    aio_rel = "lib/infrastructure/aio/src/hips/aio_hips_reader.cpp"
    aio = repo / aio_rel
    if not aio.exists():
        fail("EXEC-AIO-READ-MISSING", "P1", f"{aio_rel} missing", "exists")
    else:
        t = aio.read_text(encoding="utf-8", errors="ignore")
        # (f) 反向自证：读函数与 open 点必须真实存在，防判据静默退化
        bodies = {}
        for fn in TILE_READ_FUNCS:
            b = _func_body(t, fn)
            if b is None:
                fail("EXEC-AIO-READ-CHECK-VACUOUS", "P1",
                     f"tile read function {fn} not found in {aio_rel}",
                     "4 tile read funcs present", aio)
            else:
                bodies[fn] = b
        n_open = t.count("fits_open_file")
        if n_open < 4:
            fail("EXEC-AIO-READ-CHECK-VACUOUS", "P1",
                 "fits_open_file occurrences = %d < 4" % n_open,
                 ">= 4 (one per read path)", aio)
        # (a) 读函数体内零锁获取原语
        for fn, b in bodies.items():
            for raw in b.splitlines():
                code = _strip_comment(raw)
                for tok in LOCK_TOKENS:
                    if tok in code:
                        fail("EXEC-AIO-READ-GLOBAL-LOCK", "P1",
                             f"{fn} body acquires process-level lock token {tok!r}",
                             "lock-free tile read path (PERF-401)", aio)
                        break
        # (b) 句柄不得驻留在跨线程共享的数据集对象里
        ds = _struct_body(t, "AioHipsDataset")
        if ds is None:
            fail("EXEC-AIO-READ-CHECK-VACUOUS", "P1", "AioHipsDataset struct not found",
                 "dataset struct present", aio)
        elif "fitsfile" in ds:
            fail("EXEC-AIO-READ-SHARED-HANDLE", "P1",
                 "AioHipsDataset caches a fitsfile handle (shared across threads)",
                 "handle must be function-local / thread-private", aio)
        if re.search(r"static\s+fitsfile", t):
            fail("EXEC-AIO-READ-SHARED-HANDLE", "P1", "static fitsfile in reader",
                 "no process-level handle", aio)
        # (c) open/close 配对：句柄生命周期不跨调用转移
        for fn, b in bodies.items():
            if "fits_open_file" in b and "fits_close_file" not in b:
                fail("EXEC-AIO-READ-HANDLE-ESCAPE", "P1",
                     f"{fn} opens a fitsfile without closing it",
                     "open/close in same function scope", aio)
        # (d) 剩余串行化点必须可观测（计数式守卫）
    for rel in ("lib/infrastructure/aio/src/aio_fits.cpp",
                "lib/algorithms/fits_output/p3_output.cpp",
                "lib/infrastructure/aio/src/hips/aio_hips_reader.cpp"):
        p = repo / rel
        if not p.exists():
            continue
        t = p.read_text(encoding="utf-8", errors="ignore")
        if "cfitsio_io_mutex" in t and "CfitsioLockGuard" not in t:
            fail("EXEC-AIO-LOCK-UNOBSERVABLE", "P1",
                 f"{rel} takes cfitsio_io_mutex without the counted CfitsioLockGuard",
                 "counted guard (lock_wait_ns observable)", p)
    # (e) sampler 读路径无进程级锁
    smp = repo / "lib/algorithms/coverage/src/sampler.cpp"
    if smp.exists():
        t = smp.read_text(encoding="utf-8", errors="ignore")
        if "g_aio_mu" in t:
            fail("EXEC-SMP-GLOBAL-READ-LOCK", "P1",
                 "sampler.cpp re-introduces process-level read lock g_aio_mu",
                 "lock-free read path (PERF-401)", smp)
        for i, raw in enumerate(t.splitlines(), 1):
            code = _strip_comment(raw)
            if "aio_hips_" in code and re.search(r"lock_guard|unique_lock|scoped_lock", code):
                fail("EXEC-SMP-GLOBAL-READ-LOCK", "P1",
                     f"sampler.cpp:{i} locks around an aio_hips_ call",
                     "lock-free read path (PERF-401)", smp)

    # 4) 无 ACR 生产接入 (ACR DORMANT_NOT_IN_PRODUCTION; 不接入生产 path)
    prod = ""
    for top in ("cli", "lib"):
        base = repo / top
        if base.is_dir():
            for p in base.rglob("*.cpp"):
                rel = str(p.relative_to(repo))
                if "acr_classic_runner" in rel or "qualification" in rel or "benchmark" in rel or "examples" in rel:
                    continue
                prod += p.read_text(encoding="utf-8", errors="ignore")
    if re.search(r"acr.*(?:run|execute|launch).*(?:pipeline|kernel)", prod, re.I) and \
       "DORMANT_NOT_IN_PRODUCTION" not in prod:
        fail("EXEC-ACR-PROD", "P1", "ACR production call detected", "ACR dormant")

    return status, findings


SELFTEST_FILES = (
    "docs/architecture/THREAD_BUDGET_ARCH.md",
    "lib/algorithms/coverage/src/sampler.cpp",
    "lib/algorithms/coverage/src/upm.cpp",
    "lib/phase3_session/p3_session.cpp",
    "lib/infrastructure/aio/src/hips/aio_hips_reader.cpp",
    "lib/infrastructure/aio/src/aio_fits.cpp",
    "lib/algorithms/fits_output/p3_output.cpp",
)


def _stage(tmp: pathlib.Path, repo: pathlib.Path):
    for rel in SELFTEST_FILES:
        src = repo / rel
        if not src.is_file():
            return False
        dst = tmp / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
    (tmp / "cli").mkdir(exist_ok=True)
    (tmp / "lib").mkdir(exist_ok=True)
    return True


def _mutate(tmp: pathlib.Path, rel: str, old: str, new: str) -> bool:
    p = tmp / rel
    t = p.read_text(encoding="utf-8")
    if t.count(old) < 1:
        return False
    p.write_text(t.replace(old, new, 1), encoding="utf-8")
    return True


def self_test(repo: pathlib.Path) -> int:
    """正例 + 6 条负例注入：判据必须能红能绿（非退化）。"""
    aio_rel = "lib/infrastructure/aio/src/hips/aio_hips_reader.cpp"
    smp_rel = "lib/algorithms/coverage/src/sampler.cpp"
    p3_rel = "lib/algorithms/fits_output/p3_output.cpp"
    cases = []
    with tempfile.TemporaryDirectory(prefix="exec-contract-selftest-") as d:
        tmp = pathlib.Path(d)
        if not _stage(tmp, repo):
            print("SELFTEST_ENV_ERROR: 无法暂存被测文件", file=sys.stderr)
            return 2
        st, fd = run_checks(tmp)
        cases.append(("pos_pristine", "PASS", st, [f["id"] for f in fd]))
        # neg-1: 重新引入进程级读锁（tile 读函数体内取锁）
        _mutate(tmp, aio_rel,
                "    if (!d || !out) return -1;\n    std::string p = tile_path_resolve(d->dir, d->hips_order, ipix, \".fits\");",
                "    if (!d || !out) return -1;\n    aio::CfitsioLockGuard cfitsio_guard;\n    std::string p = tile_path_resolve(d->dir, d->hips_order, ipix, \".fits\");")
        st, fd = run_checks(tmp)
        cases.append(("neg1_reintroduce_read_lock", "EXEC-AIO-READ-GLOBAL-LOCK", st,
                      [f["id"] for f in fd]))
        # 复原
        _stage(tmp, repo)
        # neg-2: 把句柄共享回多线程（数据集缓存 fitsfile）
        _mutate(tmp, aio_rel, "struct AioHipsDataset {",
                "struct AioHipsDataset {\n    fitsfile* cached_fptr = nullptr;")
        st, fd = run_checks(tmp)
        cases.append(("neg2_shared_handle", "EXEC-AIO-READ-SHARED-HANDLE", st,
                      [f["id"] for f in fd]))
        _stage(tmp, repo)
        # neg-3: 句柄生命周期逃逸（该读函数内的 close 全部移除）
        p = tmp / aio_rel
        t = p.read_text(encoding="utf-8")
        span = _func_span(t, "read_tile_i32")
        assert span is not None, "selftest anchor missing (read_tile_i32)"
        body = t[span[0]:span[1]].replace("fits_close_file", "(void)0 /*escape*/")
        p.write_text(t[:span[0]] + body + t[span[1]:], encoding="utf-8")
        st, fd = run_checks(tmp)
        cases.append(("neg3_handle_escape", "EXEC-AIO-READ-HANDLE-ESCAPE", st,
                      [f["id"] for f in fd]))
        _stage(tmp, repo)
        # neg-4: sampler 恢复全局读锁
        _mutate(tmp, smp_rel, "// PERF-401：CON-010 的全局读锁已撤销。",
                "static std::mutex g_aio_mu;  // selftest 注入\n// PERF-401：CON-010 的全局读锁已撤销。")
        st, fd = run_checks(tmp)
        cases.append(("neg4_sampler_global_lock", "EXEC-SMP-GLOBAL-READ-LOCK", st,
                      [f["id"] for f in fd]))
        _stage(tmp, repo)
        # neg-5: 串行化点不可观测（绕过计数式守卫）
        _mutate(tmp, p3_rel, "    aio::CfitsioLockGuard cfitsio_guard;",
                "    std::lock_guard<std::mutex> cfitsio_guard(aio::cfitsio_io_mutex());")
        st, fd = run_checks(tmp)
        cases.append(("neg5_unobservable_lock", "EXEC-AIO-LOCK-UNOBSERVABLE", st,
                      [f["id"] for f in fd]))
        _stage(tmp, repo)
        # neg-6: 判据退化（读函数被改名 ⇒ 必须判 vacuous 而不是静默通过）
        _mutate(tmp, aio_rel, "static int read_tile_i32(AioHipsDataset* d,",
                "static int read_tile_i32_renamed(AioHipsDataset* d,")
        st, fd = run_checks(tmp)
        cases.append(("neg6_check_vacuous", "EXEC-AIO-READ-CHECK-VACUOUS", st,
                      [f["id"] for f in fd]))
    bad = 0
    for name, expect, st, ids in cases:
        if name == "pos_pristine":
            ok = (st == "PASS")
        else:
            ok = (st == "FAIL" and expect in ids)
        print("SELFTEST %-28s expect=%-32s status=%-4s ids=%s -> %s"
              % (name, expect, st, ",".join(ids) or "-", "OK" if ok else "MISMATCH"))
        if not ok:
            bad += 1
    print("SELFTEST_SUMMARY cases=%d mismatches=%d" % (len(cases), bad))
    return 0 if bad == 0 else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    ap.add_argument("--self-test", action="store_true",
                    help="正例 + 6 条负例注入（判据非退化自证）")
    args = ap.parse_args()
    repo = pathlib.Path(args.repo)
    if args.self_test:
        return self_test(repo)
    status, findings = run_checks(repo)
    result = {"tool": "check_execution_contracts", "status": status, "findings": findings,
              "passed": status == "PASS"}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(result, indent=2, ensure_ascii=False),
                                               encoding="utf-8")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0", "P1")])
        junit = (f'<testsuite name="check_execution_contracts" tests="1" '
                 f'failures="{failures}"><testcase classname="exec" name="contracts"/></testsuite>')
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
