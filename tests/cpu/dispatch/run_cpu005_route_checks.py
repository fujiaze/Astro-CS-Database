#!/usr/bin/env python3
"""AstroCS CPU-005 provider 数值自测与路由表 — 验收运行器
tests/cpu/dispatch/run_cpu005_route_checks.py

验收 (04_CPU_RESOURCE_TASKS.md CPU-005):
  1. 固定执行序 query→self_test→eligible→benchmark→select (cpu005_route_decision_test.cpp);
  2. 路由以 kernel_id 粒度 (同一 profile 各 kernel 独立选 provider, 无全局 preferred_isa);
  3. 伪造 profile (损坏 JSON / 字段类型篡改) → baseline 不抛异常;
  4. build/CPU/OS/benchmark hash 变化 → baseline@query;
  5. provider self_test hash 不符 (错误 hash) → baseline@self_test;
  6. 缺 benchmark (oracle:fail / 上游剔除) → baseline@benchmark;
  7. NaN/Inf/oracle-fail live rows → baseline (数值 mismatch);
  8. 低收益 (<3% 冻结门限) → baseline@select; 足够收益 → 选择;
  9. 路由表 JSON trace 显示每 kernel 实际 provider; 无全局 preferred_isa=avx512
     (源码静态 grep + 运行时断言)。

编译: 生产同源全量 TUs (cpu_routing 为本任务修改文件; 其余 b99fcd8 原样) +
       -Wall -Wextra -Wpedantic 严格零告警; 链接 -ldl -lpthread。
依赖: g++ (C++17), python3 标准库。
"""
import os
import re
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
TST = os.path.join(REPO, "tests", "cpu", "dispatch")
BH = os.path.join(REPO, "lib", "backend_host")
CRYPTO = os.path.join(REPO, "lib", "common", "crypto")
INC = os.path.join(REPO, "include")
TP = os.path.join(REPO, "third_party")

SRC = os.path.join(BH, "cpu_routing.cpp")       # 本任务改动
TEST_MAIN = os.path.join(TST, "cpu005_route_decision_test.cpp")
# 生产同源其余 TU (base b99fcd8 未改动; 全量链接证集成)
LIB_TUS = [
    "cpu_features.cpp", "hardware_inspect.cpp", "backend_loader.cpp",
    "baseline_backend.cpp", "profile_gen.cpp", "profile_gen_v2.cpp",
    "host_services.cpp", "bench_harness.cpp",
]
SHA_SRC = os.path.join(CRYPTO, "sha256.cpp")

FAILURES = []
COMPILE_STDERR = []


def log(msg):
    print(msg, flush=True)


def fail(msg):
    FAILURES.append(msg)
    log("FAIL: " + msg)


def run(cmd, cwd=REPO, timeout=300):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)
    log("$ " + " ".join(cmd) + f"\n  exit={r.returncode}")
    if r.stdout.strip():
        log("  stdout: " + r.stdout.strip()[-6000:])
    if r.stderr.strip():
        log("  stderr: " + r.stderr.strip()[-3000:])
    return r


def cc_cpp(src, out, extra=None):
    cmd = ["g++", "-std=c++17", "-O2", "-DNDEBUG", "-fPIC", "-Wall", "-Wextra",
           "-Wpedantic", f"-I{INC}", f"-I{BH}", f"-I{CRYPTO}", f"-I{TP}",
           "-c", src, "-o", out]
    if extra:
        cmd.extend(extra)
    r = run(cmd)
    if r.stderr:
        COMPILE_STDERR.append((os.path.basename(src), r.stderr))
    return r


def main():
    tmp = tempfile.mkdtemp(prefix="cpu005_dispatch_")
    log(f"repo={REPO} tmp={tmp}")

    # 1) 全量 TU 严格编译 (含本任务 cpu_routing.cpp 零告警证据)
    objs = []
    for tu in LIB_TUS:
        obj = os.path.join(tmp, os.path.splitext(tu)[0] + ".o")
        r = cc_cpp(os.path.join(BH, tu), obj)
        if r.returncode != 0:
            fail(f"生产 TU 编译失败: {tu}")
            return 1
        objs.append(obj)
    obj_route = os.path.join(tmp, "cpu_routing.o")
    r = cc_cpp(SRC, obj_route)
    if r.returncode != 0:
        fail("cpu_routing.cpp 编译 (本任务修改文件)")
        return 1
    objs.append(obj_route)
    obj_sha = os.path.join(tmp, "sha256.o")
    r = cc_cpp(SHA_SRC, obj_sha)
    if r.returncode != 0:
        fail("sha256.cpp 编译")
        return 1
    objs.append(obj_sha)
    # 严格告警检查: 任一 TU 编译 stderr 含 warning:/error: 即失败 (编译纪律)
    for srcname, errtext in COMPILE_STDERR:
        if "warning:" in errtext or "error:" in errtext:
            fail(f"严格编译存在 warning/error: {srcname}\n{errtext[-2000:]}")
            return 1

    # 2) 编译测试 main + 链接
    obj_main = os.path.join(tmp, "cpu005_route_decision_test.o")
    r = cc_cpp(TEST_MAIN, obj_main)
    if r.returncode != 0:
        fail("测试 main 编译")
        return 1
    exe = os.path.join(tmp, "cpu005_route_decision_test")
    r = run(["g++", "-std=c++17", "-O2", "-DNDEBUG",
             obj_main] + objs + ["-o", exe, "-ldl", "-lpthread"], timeout=300)
    if r.returncode != 0:
        fail("测试链接")
        return 1

    # 3) 运行验收矩阵
    r = run([exe], timeout=120)
    if r.returncode != 0:
        fail("route 决策测试二进制退出非 0")
        return 1
    if "CPU-005 ROUTE TESTS PASS" not in r.stdout:
        fail("route 决策测试未宣告 PASS")
        return 1
    # trace 表须显示实际 provider
    if "TABLE_BEGIN" not in r.stdout or '"hips-bulk-transform"' not in r.stdout:
        fail("路由表 trace 未打印")
    if '"provider": "avx512"' not in r.stdout:
        fail("trace 未显示 hips avx512 实际 provider")

    # 4) 静态断言: 无全局 preferred_isa=avx512 (源码面, 仅代码非注释行)
    #    扫描对象: 生产路由源码 + 测试 C++ (runner 自身代码不含该赋值, 无须自扫)
    scan = [os.path.join(BH, "cpu_routing.h"), os.path.join(BH, "cpu_routing.cpp"),
            TEST_MAIN]
    for f in scan:
        with open(f, "r", encoding="utf-8", errors="replace") as fh:
            for ln, line in enumerate(fh, 1):
                code = line.split("//", 1)[0].strip()
                if not code:
                    continue   # 纯注释/空行
                if re.search(r"preferred_isa\s*=\s*[\"']?avx512", code):
                    fail(f"{os.path.relpath(f, REPO)}:{ln}: 全局 preferred_isa=avx512 赋值")
    # 全 CPU provider/backend_host 面 grep (跨文件全局 ISA 声明禁止; 注释除外)
    r = run(["grep", "-rn", "preferred_isa", os.path.join(BH),
             os.path.join(REPO, "providers", "cpu")], timeout=60)
    if r.returncode == 0 and r.stdout.strip():
        hits = [l for l in r.stdout.splitlines()
                if l.split("//", 1)[0].strip() and
                re.search(r"preferred_isa\s*=\s*[\"']?avx512", l.split("//", 1)[0])]
        if hits:
            fail("backend_host/providers 出现全局 preferred_isa 赋值:\n" + "\n".join(hits))
    # baseline_avx512 对照已由 CPU-002/003/004 oracle 覆盖; 此处查路由层无硬编码首选 ISA

    if FAILURES:
        log(f"\nCPU-005 CHECKS FAIL ({len(FAILURES)})")
        return 1
    log("\nCPU-005 CHECKS ALL PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
