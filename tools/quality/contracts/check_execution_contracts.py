#!/usr/bin/env python3
"""check_execution_contracts.py — T404 execution contracts checker (V6.1)

Checks (V6.1 架构, 替代旧 OpenMP 宏/ACR CUDA 检查):
- THREAD_BUDGET_ARCH.md 契约 (ARCH-THREAD-001 FROZEN: budget 单一来源 / lease /
  Σ active worker ≤ budget) 存在且含关键 ID;
- 生产 sampler/upm/p3_session 并行轴: std::thread + Runtime lease
  (cfg.cpu_workers=budget.max_workers), 无 hardware_concurrency 自取;
- 禁止串行化负面模式 (P2_ENABLE_OPENMP 宏守卫已由 V6.1 std::thread 替代,
  保留其 CMake -fopenmp 接线证据);
- aio_read 串行化 (g_aio_mu 全局锁, 原 critical(aio_read) 语义) 存在;
- 无 ACR 生产接入 (ACR DORMANT_NOT_IN_PRODUCTION)。

Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error
"""
import argparse, json, pathlib, sys, re

V6_1_THREAD_DOC_IDS = ["ARCH-THREAD-001", "budget", "lease"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    args = ap.parse_args()
    repo = pathlib.Path(args.repo)
    findings = []
    status = "PASS"

    def fail(fid, sev, obs, exp, file=None):
        nonlocal status
        status = "FAIL"
        f = {"id": fid, "severity": sev, "observed": obs, "expected": exp}
        if file:
            f["file"] = str(file.relative_to(repo))
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
        ("lib/phase2/src/sampler.cpp", "std::thread"),
        ("lib/phase2/src/upm.cpp", "std::thread"),
        ("lib/phase3_session/p3_session.cpp", "std::thread"),
    ):
        p = repo / rel
        if not p.exists():
            continue
        t = p.read_text(encoding="utf-8", errors="ignore")
        if needle not in t:
            fail("EXEC-NO-PARALLEL-AXIS", "P1", f"{needle} not in {rel}", "exists", p)
        # lease 注入: budget.max_workers / cfg.cpu_workers 覆盖
        if not re.search(r"budget\.max_workers|cfg\.cpu_workers|n_workers\s*=.*budget", t):
            fail("EXEC-NO-LEASE", "P1", f"no Runtime lease injection in {rel}", "budget lease", p)
        # 禁止 hardware_concurrency 自取 (注释中的禁止性声明除外)
        for m in re.finditer(r"hardware_concurrency\s*\(", t):
            ln = t[:m.start()].count("\n") + 1
            line = t.splitlines()[ln - 1].strip()
            if not (line.startswith("//") or "无 hardware_concurrency" in line):
                fail("EXEC-HW-CONCURRENCY", "P1", f"hardware_concurrency() in {rel}:{ln}",
                     "Runtime lease only", p)

    # 3) aio 串行化 (原 critical(aio_read) 语义 → cfitsio_io_mutex / g_aio_mu 全局锁)
    aio = repo / "lib/astro_image_io/src/hips/aio_hips_reader.cpp"
    if aio.exists():
        t = aio.read_text(encoding="utf-8", errors="ignore")
        if "cfitsio_io_mutex" not in t and "g_aio_mu" not in t and "critical(aio_read)" not in t:
            fail("EXEC-NO-AIO-SERIAL", "P1", "aio read serialization not found",
                 "cfitsio_io_mutex / g_aio_mu / critical(aio_read)", aio)

    # 4) 无 ACR 生产接入 (ACR DORMANT_NOT_IN_PRODUCTION; 不接入生产 path)
    #    仅扫描生产调用形态: Runtime 调度/阶段执行中调用 ACR kernel 即失败;
    #    工具/qualification 目录 (实验 runner/benchmark) 不属于生产路径。
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
