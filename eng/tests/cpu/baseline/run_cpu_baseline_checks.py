#!/usr/bin/env python3
"""ACSD CPU-002 baseline provider — capability/handshake/.so 加载三门运行器
eng/tests/cpu/baseline/run_cpu_baseline_checks.py

存在理由（WIRING-W34-01 / CHK-PROD-WIRING W4）: 本目录三份 C 门
  provider_capability_gate_test.c / provider_handshake_test.c /
  provider_so_load_test.c
此前**只有源文件、全仓无任何构建路径**（无 CMake 目标、无脚本引用、build.ninja
无编译边），而 avx2 / avx512 两档各有 run_provider_avx2_checks.py /
run_provider_avx512_checks.py 逐行真编各自三份 ⇒ CPU-002 的「能力不足 ⇒ baseline
拒绝」「query/self_test/export/ABI 完整」「.so 唯一导出可加载」三条验收在 baseline
档**无机器证据**（run_provider_oracle_checks.py:12-13 已写明由本文件覆盖，但本文件
原先不存在）。本文件照 avx2 版逐行实现，不改任何断言与容差。

覆盖（04_CPU_RESOURCE_TASKS.md CPU-002）:
  1. capability 门负测: 非 amd64 / OS 不保证 SSE2 ⇒ ACS_ERR_UNSUPPORTED，
     SSE2 齐备 ⇒ 通过（provider_capability_gate_test.c；以链接期强符号替换
     acs_cap_detect_v1 注入 stub 探测，故**不带** capability_detect.c）；
  2. query 握手: host_abi 失配 / host 缺 allocator / out_api NULL 逐条拒绝；
     kernel_list 12 条 + self_test 往返（provider_handshake_test.c；带真实
     capability_detect.c 走真实 CPUID 通过路径）；
  3. .so 加载冒烟: dlopen 唯一导出 astrocs_provider_query_v1、kernel_list 12 条、
     self_test、run_kernel(calibration) 与独立期望一致（provider_so_load_test.c）。

依赖: 仅标准库 + gcc/g++（本机 Linux 开发节点）。退出码 0=全 PASS。
用法: python3 eng/tests/cpu/baseline/run_cpu_baseline_checks.py
"""
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))))
HERE = os.path.dirname(os.path.abspath(__file__))
BASE_SRC = os.path.join(REPO, "lib", "infrastructure", "benchmark", "cpu", "baseline",
                        "src", "baseline_provider.cpp")
CAPSRC = os.path.join(REPO, "lib", "infrastructure", "benchmark", "cpu", "common", "src",
                      "capability_detect.c")
GATE = os.path.join(HERE, "provider_capability_gate_test.c")
HANDSHAKE = os.path.join(HERE, "provider_handshake_test.c")
SO_LOAD = os.path.join(HERE, "provider_so_load_test.c")
INC_ROOT = os.path.join(REPO, "lib", "include")
INC_BASE = os.path.join(REPO, "lib", "infrastructure", "benchmark", "cpu", "baseline",
                        "include")
INC_CAP = os.path.join(REPO, "lib", "infrastructure", "benchmark", "cpu", "common",
                       "include")

FAILURES = []


def log(msg):
    print(msg, flush=True)


def fail(msg):
    FAILURES.append(msg)
    log("FAIL: " + msg)


def run(cmd, timeout=300):
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    log("$ " + " ".join(cmd) + "\n  exit=%d" % r.returncode)
    if r.stdout.strip():
        log("  stdout: " + r.stdout.strip()[-3000:])
    if r.stderr.strip():
        log("  stderr: " + r.stderr.strip()[-2000:])
    return r


def main():
    tmp = "/tmp/cpu002_baseline"
    os.makedirs(tmp, exist_ok=True)
    so_b = os.path.join(tmp, "astrocs_cpu_baseline.so")
    log("repo=%s" % REPO)

    # 1) baseline provider .so（保守 SSE2，无 -mavx*; 供 dlopen 冒烟）
    r = run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-fPIC", "-shared",
             "-Wall", "-Wextra", "-Wpedantic",
             "-I%s" % INC_ROOT, "-I%s" % INC_BASE, "-I%s" % INC_CAP,
             BASE_SRC, CAPSRC, "-o", so_b, "-lpthread"], timeout=300)
    if r.returncode != 0:
        fail("baseline .so 编译")
        return 1

    # 2) capability 门负测（stub 探测: 本 TU 提供 acs_cap_detect_v1 强符号）
    gate_exe = os.path.join(tmp, "baseline_gate")
    r = run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
             "-I%s" % INC_ROOT, "-I%s" % INC_BASE, "-I%s" % INC_CAP,
             GATE, BASE_SRC, "-o", gate_exe, "-lstdc++", "-lpthread", "-lm"],
            timeout=300)
    if r.returncode != 0:
        fail("capability gate 编译")
        return 1
    r = run([gate_exe], timeout=300)
    if r.returncode != 0 or "ALL PASS" not in r.stdout:
        fail("capability gate 负测")

    # 3) handshake（真实 CPUID 通过路径 + 表外 unsupported + self_test）
    hs_exe = os.path.join(tmp, "baseline_handshake")
    r = run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
             "-I%s" % INC_ROOT, "-I%s" % INC_BASE, "-I%s" % INC_CAP,
             HANDSHAKE, BASE_SRC, CAPSRC, "-o", hs_exe, "-lstdc++", "-lpthread",
             "-lm"],
            timeout=300)
    if r.returncode != 0:
        fail("handshake 编译")
        return 1
    r = run([hs_exe], timeout=300)
    if r.returncode != 0 or "ALL PASS" not in r.stdout:
        fail("handshake 测试")

    # 4) .so 加载冒烟（dlopen 唯一导出 + kernel_list=12 + self_test + run_kernel）
    sl_exe = os.path.join(tmp, "baseline_so_load")
    r = run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
             "-I%s" % INC_ROOT, "-I%s" % INC_BASE, "-I%s" % INC_CAP,
             SO_LOAD, "-o", sl_exe, "-ldl"], timeout=300)
    if r.returncode != 0:
        fail("so_load 编译")
        return 1
    r = run([sl_exe, so_b], timeout=300)
    if r.returncode != 0 or "ALL PASS" not in r.stdout:
        fail("so_load 测试")

    if FAILURES:
        log("\nCPU-002 BASELINE FAIL (%d)" % len(FAILURES))
        return 1
    log("\nCPU-002 BASELINE PASS (capability 门负测 / 握手 ABI / 唯一导出 .so 加载: "
        "三门逐行真编真跑)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
