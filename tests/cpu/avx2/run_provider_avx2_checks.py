#!/usr/bin/env python3
"""AstroCS CPU-003 AVX2/FMA provider — 科学对照 oracle 运行器
tests/cpu/avx2/run_provider_avx2_checks.py

验收 (04_CPU_RESOURCE_TASKS.md CPU-003):
  1. 非支持 CPU 不加载: capability gate stub 负测 (探测失败/无 AVX2 hw/
     OS 不保存 YMM → ACS_ERR_UNSUPPORTED) —— provider_avx2_capability_gate_test.c;
  2. CPUID/XGETBV negative: 合成证据缺位拒绝 (同上; 真实主机 CPUID+XGETBV
     通过路径由 handshake/so_load 覆盖);
  3. 函数入口由 provider 表查询: dlopen baseline.so + avx2.so, 以 kernel_id
     查各自 kernel_list → 索引 run (oracle main; provider 表是唯一路由源);
  4. 其余 kernel 回落 baseline: avx2 表只注册 2 热点; noise-snr-reductions
     在 avx2 表查不到 (NOT_FOUND) + run_kernel 表外索引 → ACS_ERR_UNSUPPORTED
     (handshake/so_load 断言);
  5. baseline 对照容差: 两热点 avx2 vs baseline 同输入输出 相对差 ≤ 2e-4
     (ALG oracle 同规; baseline 已与 f64 参考一致 CPU-002);
  6. FMA 是否改变归约顺序记录: 输出 ULP_MAX (avx2 vs baseline bits 差;
     max-ULP 与 REL_MAX 打印入日志 —— 两热点均无跨项/跨线程归约, 顺序
     不变 (CPU_003_AVX2_PROVIDER.md §6 记录), 数值差仅来自 FMA 单次舍入);
  7. determinism/1-N worker: avx2 budget 1 vs 4 逐位相同 (DET OK);
     budget=4 时租借 ≥2 (本机 ≥2 CPU 时); 性能不是唯一通过条件 ——
     本 runner 不做计时断言, 只验证正确性/确定性/加载门。

依赖: 仅标准库。输出退出码 0=全 PASS。
"""
import hashlib
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
AVX2_SRC = os.path.join(REPO, "providers", "cpu", "avx2", "src",
                        "avx2_provider.cpp")
BASE_SRC = os.path.join(REPO, "providers", "cpu", "baseline", "src",
                        "baseline_provider.cpp")
CAPSRC = os.path.join(REPO, "providers", "cpu", "common", "src",
                      "capability_detect.c")
ORACLE = os.path.join(REPO, "tests", "cpu", "avx2",
                      "provider_avx2_oracle_main.cpp")
GATE = os.path.join(REPO, "tests", "cpu", "avx2",
                    "provider_avx2_capability_gate_test.c")
HANDSHAKE = os.path.join(REPO, "tests", "cpu", "avx2",
                         "provider_avx2_handshake_test.c")
SO_LOAD = os.path.join(REPO, "tests", "cpu", "avx2",
                       "provider_avx2_so_load_test.c")
INC_ROOT = os.path.join(REPO, "include")
INC_BASE = os.path.join(REPO, "providers", "cpu", "baseline", "include")
INC_AVX2 = os.path.join(REPO, "providers", "cpu", "avx2", "include")
INC_CAP = os.path.join(REPO, "providers", "cpu", "common", "include")

FAILURES = []
HW = os.cpu_count() or 1
TOL = 2e-4  # baseline 对照容差 (ALG oracle 同规, 冻结于 CPU-003 文档 §7)


def log(msg):
    print(msg, flush=True)


def fail(msg):
    FAILURES.append(msg)
    log("FAIL: " + msg)


def run(cmd, timeout=300, cwd=None):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                       timeout=timeout)
    log("$ " + " ".join(cmd) + f"\n  exit={r.returncode}")
    if r.stdout.strip():
        log("  stdout: " + r.stdout.strip()[-4000:])
    if r.stderr.strip():
        log("  stderr: " + r.stderr.strip()[-2000:])
    return r


def b2f(u):
    import struct
    return struct.unpack("<f", struct.pack("<I", u))[0]


def main():
    tmp = "/tmp/cpu003_avx2"
    os.makedirs(tmp, exist_ok=True)
    so_b = os.path.join(tmp, "astrocs_cpu_baseline.so")
    so_a = os.path.join(tmp, "astrocs_cpu_avx2.so")
    log(f"repo={REPO} hw_cpus={HW}")

    # 1) 编译 baseline .so (无 -mavx*; 保守 SSE2) + capability_detect.c
    r = run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-fPIC", "-shared",
             "-Wall", "-Wextra", "-Wpedantic",
             f"-I{INC_ROOT}", f"-I{INC_BASE}", f"-I{INC_CAP}",
             BASE_SRC, CAPSRC, "-o", so_b, "-lpthread"], timeout=300)
    if r.returncode != 0:
        fail("baseline .so 编译")
        return 1

    # 2) 编译 avx2 .so (仅此 target 带 -mavx2 -mfma; 编译隔离 15 §6)
    r = run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-mavx2", "-mfma",
             "-fPIC", "-shared", "-Wall", "-Wextra", "-Wpedantic",
             f"-I{INC_ROOT}", f"-I{INC_BASE}", f"-I{INC_AVX2}", f"-I{INC_CAP}",
             AVX2_SRC, CAPSRC, "-o", so_a, "-lpthread"], timeout=300)
    if r.returncode != 0:
        fail("avx2 .so 编译 (-mavx2 -mfma)")
        return 1

    # 3) 编译对照 oracle main (本 TU 无 -mavx*; dlopen 两 .so)
    exe = os.path.join(tmp, "avx2_oracle")
    r = run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-Wall", "-Wextra",
             "-Wpedantic", f"-I{INC_ROOT}", f"-I{INC_BASE}", f"-I{INC_AVX2}",
             f"-I{INC_CAP}", ORACLE, "-o", exe, "-ldl"], timeout=300)
    if r.returncode != 0:
        fail("oracle main 编译")
        return 1
    r = run([exe, so_b, so_a], timeout=300)
    if r.returncode != 0:
        fail("oracle main 运行")
        return 1
    if "AVX2_ORACLE_DONE" not in r.stdout or \
            "alloc_balance=0" not in r.stdout:
        fail("oracle runner 未完成 / allocator 不平衡")

    # 4) 解析 oracle 输出并断言
    ops = {}
    cur = None
    for line in r.stdout.splitlines():
        if line.startswith("OP "):
            cur = {"avx2a": None, "avx2b": None, "base": None,
                   "det": None, "acq": None, "rel": None, "ulp": None}
            ops[line.split()[1]] = cur
        elif cur is None:
            continue
        elif line.startswith("AVX2A "):
            cur["avx2a"] = [b2f(int(x, 16)) for x in line.split()[1:]]
        elif line.startswith("AVX2B "):
            cur["avx2b"] = [b2f(int(x, 16)) for x in line.split()[1:]]
        elif line.startswith("BASE "):
            cur["base"] = [b2f(int(x, 16)) for x in line.split()[1:]]
        elif line.startswith("DET "):
            cur["det"] = line.split()[1]
        elif line.startswith("ACQ "):
            _, a, b = line.split()
            cur["acq"] = (int(a), int(b))
        elif line.startswith("REL_MAX "):
            cur["rel"] = float(line.split()[1])
        elif line.startswith("ULP_MAX "):
            cur["ulp"] = float(line.split()[1])
        elif line.startswith("NONHOT_AVX2"):
            if "NOT_FOUND" not in line:
                fail("noise-snr-reductions 应在 avx2 表查不到 (回落 baseline)")
        elif line.startswith("NONHOT_BASE"):
            if "NOT_FOUND" in line:
                fail("noise-snr-reductions 应在 baseline 表查到")

    if len(ops) != 2:
        fail(f"oracle op 数 {len(ops)} != 2")
    for name, op in ops.items():
        if op["avx2a"] is None or op["base"] is None:
            fail(f"{name}: 输出缺失 (avx2a/base)")
            continue
        # 2a. determinism budget1 vs 4 逐位
        if op["det"] != "OK":
            fail(f"{name}: determinism FAIL (budget 1 vs 4 非逐位相同)")
        # 2b. worker 观测
        a, b = op["acq"]
        if a != 1:
            fail(f"{name}: budget=1 应租借 1 (got {a})")
        if HW >= 2 and b < 2:
            fail(f"{name}: budget=4 应租借 ≥2 (got {b})")
        # 2c. baseline 对照容差 (相对 2e-4; FMA 舍入差记录)
        if op["rel"] is None or op["rel"] > TOL:
            fail(f"{name}: baseline 对照 REL={op['rel']} > {TOL}")
        # 2d. FMA 归约顺序记录 (输出 ULP_MAX; 无断言阈值, 记录入日志)
        log(f"{name}: FMA 对照 max_rel={op['rel']:.3g} max_ulp={op['ulp']}")
        log(f"  → FMA 归约顺序记录: 两热点均无跨项/跨线程归约, 顺序不变; "
            f"差异仅 FMA 单次舍入 (见 CPU_003_AVX2_PROVIDER.md §6)")

    # 5) capability gate stub 负测 (非支持 CPU 不加载; CPUID/XGETBV negative)
    gate_exe = os.path.join(tmp, "avx2_gate")
    r = run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Wpedantic",
             f"-I{INC_AVX2}", f"-I{INC_CAP}", f"-I{INC_BASE}",
             f"-I{INC_ROOT}",
             GATE, AVX2_SRC, "-o", gate_exe, "-lstdc++", "-lpthread"],
            timeout=300)
    if r.returncode != 0:
        fail("capability gate 编译")
        return 1
    r = run([gate_exe], timeout=300)
    if r.returncode != 0 or "ALL PASS" not in r.stdout:
        fail("capability gate 负测")

    # 6) handshake (真实 CPUID/XGETBV 通过路径 + 表外 unsupported)
    hs_exe = os.path.join(tmp, "avx2_handshake")
    r = run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Wpedantic",
             f"-I{INC_AVX2}", f"-I{INC_CAP}", f"-I{INC_BASE}",
             f"-I{INC_ROOT}", HANDSHAKE, AVX2_SRC, CAPSRC,
             "-o", hs_exe, "-lstdc++", "-lpthread"], timeout=300)
    if r.returncode != 0:
        fail("handshake 编译")
        return 1
    r = run([hs_exe], timeout=300)
    if r.returncode != 0 or "ALL PASS" not in r.stdout:
        fail("handshake 测试")

    # 7) so_load (dlopen 唯一导出 + kernel_list=2 + calibration 冒烟)
    sl_exe = os.path.join(tmp, "avx2_so_load")
    r = run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Wpedantic",
             f"-I{INC_AVX2}", f"-I{INC_CAP}", f"-I{INC_BASE}",
             f"-I{INC_ROOT}",
             SO_LOAD, "-o", sl_exe, "-ldl"], timeout=300)
    if r.returncode != 0:
        fail("so_load 编译")
        return 1
    r = run([sl_exe, so_a], timeout=300)
    if r.returncode != 0 or "ALL PASS" not in r.stdout:
        fail("so_load 测试")

    if FAILURES:
        log(f"\nCPU-003 AVX2 FAIL ({len(FAILURES)})")
        return 1
    log("\nCPU-003 AVX2 PASS (2 hotspot kernels oracle/baseline-rel 2e-4, "
        "determinism budget1vs4 bitwise, capability gate negative, "
        "non-hotspot fallback)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
