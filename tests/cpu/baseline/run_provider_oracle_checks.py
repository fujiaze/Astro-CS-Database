#!/usr/bin/env python3
"""AstroCS CPU-002 baseline provider — 科学 oracle 比对运行器
tests/cpu/baseline/run_provider_oracle_checks.py

验收 (04_CPU_RESOURCE_TASKS.md CPU-002):
  1. 科学 oracle: 12 个注册 kernel 输出与 Python 独立参考实现 (f64 运算,
     容差 2e-4 相对, test_abi_kernels 同规) 一致;
  2. 确定性 1/N worker tests: budget=1 与 budget=4 输出逐位相同
     (DET=OK; 输出元素独立无跨线程归约, ARCH-004 §4);
  3. worker 观测: budget=4 时 executor 实际租借 ≥2 (本机 ≥2 CPU 时),
     budget=1 时 =1;
  4. provider query/self_test/export ABI 完整: 由 handshake/capability 测试
     (C) 与 provider_so_load 冒烟覆盖 (见 run_cpu_baseline_checks.py)。

依赖: 仅标准库 + numpy (独立参考做 f64 向量化可选用; 缺 numpy 用纯 python)。
"""
import math
import os
import struct
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
SRC = os.path.join(REPO, "providers", "cpu", "baseline", "src",
                   "baseline_provider.cpp")
CAPSRC = os.path.join(REPO, "providers", "cpu", "common", "src",
                      "capability_detect.c")
ORACLE = os.path.join(REPO, "tests", "cpu", "baseline",
                      "provider_kernel_oracle_main.cpp")
INC_ROOT = os.path.join(REPO, "include")
INC_BASE = os.path.join(REPO, "providers", "cpu", "baseline", "include")
INC_CAP = os.path.join(REPO, "providers", "cpu", "common", "include")

FAILURES = []
HW = os.cpu_count() or 1


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
        log("  stdout: " + r.stdout.strip()[-3000:])
    if r.stderr.strip():
        log("  stderr: " + r.stderr.strip()[-2000:])
    return r


def b2f(u):
    return struct.unpack("<f", struct.pack("<I", u))[0]


# ── 独立参考实现 (f64 运算; 与 legacy test_abi_kernels._ref 同式) ──
def reference(name, ins):
    N = 8
    if name == "calibration":
        k = 2.0
        return [(ins["IN0"][i] - ins["IN1"][i] - k * ins["IN2"][i]) * ins["IN3"][i]
                for i in range(N)]
    if name == "noise":
        fr = 3
        med, mad = [], []
        for i in range(N):
            v = sorted(ins["IN0"][f * N + i] for f in range(fr))
            m = v[1]
            dev = sorted(abs(x - m) for x in v)
            med.append(m)
            mad.append(dev[1] * 1.4826)
        return med, mad
    if name == "psf":
        cx, cy = ins["IN0"][0], ins["IN0"][1]
        out = []
        for i in range(N):
            x, y = float(i % 4), float(i // 4)
            out.append(5.0 * math.exp(-((x - cx) ** 2 + (y - cy) ** 2) / 2))
        return out
    if name == "driz_overlap":
        return [max(0.0, 1 - abs(ins["IN0"][i])) * max(0.0, 1 - abs(ins["IN1"][i]))
                for i in range(N)]
    if name == "driz_accum":
        fr = 3
        return [sum(ins["IN0"][f * N + i] * ins["IN1"][f * N + i] for f in range(fr))
                for i in range(N)]
    if name == "driz_norm":
        return [ins["IN0"][i] / ins["IN1"][i] if ins["IN1"][i] > 1e-6 else 0.0
                for i in range(N)]
    if name == "spmv":
        rp = [int(x) for x in ins["IN2"]]
        return [sum(ins["IN0"][k] * ins["IN3"][int(ins["IN1"][k])]
                    for k in range(rp[r], rp[r + 1])) for r in range(len(rp) - 1)]
    if name == "residual":
        return [ins["IN0"][i] - ins["IN1"][i] for i in range(N)]
    if name == "weight_upd":
        return [max(ins["IN0"][i], 0.25) for i in range(N)]
    if name == "rejection":
        fr = 3
        cnt = []
        for i in range(N):
            v = [ins["IN0"][f * N + i] for f in range(fr)]
            m = sorted(v)[1]
            mad = sorted(abs(x - m) for x in v)[1] * 1.4826
            cnt.append(float(sum(1 for x in v if abs(x - m) > 3.0 * mad)))
        return cnt
    if name == "integration":
        fr = 3
        out = []
        for i in range(N):
            acc = sum(ins["IN1"][f * N + i] * ins["IN0"][f * N + i] for f in range(fr))
            wsum = sum(ins["IN1"][f * N + i] for f in range(fr))
            out.append(acc / wsum if wsum > 1e-6 else 0.0)
        return out
    if name == "hips":
        iw, ih, s = 4, 3, 0.5
        out = []
        for i in range(N):
            x, y = (i % 4) * s, (i // 4) * s
            x0 = min(max(int(math.floor(x)), 0), iw - 2)
            y0 = min(max(int(math.floor(y)), 0), ih - 2)
            fx = min(max(x - math.floor(x), 0.0), 1.0)
            fy = min(max(y - math.floor(y), 0.0), 1.0)
            r0, r1 = y0 * iw, y0 * iw + iw
            v00, v10 = ins["IN0"][r0 + x0], ins["IN0"][r0 + x0 + 1]
            v01, v11 = ins["IN0"][r1 + x0], ins["IN0"][r1 + x0 + 1]
            out.append((1 - fx) * (1 - fy) * v00 + fx * (1 - fy) * v10 +
                       (1 - fx) * fy * v01 + fx * fy * v11)
        return out
    raise AssertionError(name)


def parse(text):
    ops, cur = {}, None
    for line in text.splitlines():
        if line.startswith("OP "):
            cur = {"name": line.split()[1], "in": {}, "out": None,
                   "det": None, "acq": None, "out1": None}
            ops[cur["name"]] = cur
        elif line.startswith(("IN0", "IN1", "IN2", "IN3", "OUTA", "OUTB",
                              "OUT1A", "OUT1B")):
            tag, *vals = line.split()
            cur[tag] = [b2f(int(v, 16)) for v in vals]
        elif line.startswith("DET "):
            cur["det"] = line.split()[1]
        elif line.startswith("ACQ "):
            _, a, b = line.split()
            cur["acq"] = (int(a), int(b))
    return ops


def main():
    tmp = "/tmp/cpu002_oracle"
    os.makedirs(tmp, exist_ok=True)
    exe = os.path.join(tmp, "provider_oracle")
    log(f"repo={REPO} hw_cpus={HW}")

    # 1) 编译 (无 -mavx*; O2)
    r = run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-Wall", "-Wextra",
             "-Wpedantic", f"-I{INC_ROOT}", f"-I{INC_BASE}", f"-I{INC_CAP}",
             ORACLE, SRC, CAPSRC, "-o", exe, "-lpthread"], timeout=300)
    if r.returncode != 0:
        fail("oracle 编译")
        return 1

    # 2) 运行 (budget 1 vs 4 双跑内建)
    r = run([exe], timeout=300)
    if r.returncode != 0:
        fail("oracle 运行")
        return 1
    if "ORACLE_RUNNER_DONE" not in r.stdout:
        fail("oracle runner 未完成")
    if "alloc_balance=" not in r.stdout or "alloc_balance=0" not in r.stdout:
        fail("allocator 不平衡 (泄漏)")
    ops = parse(r.stdout)
    if len(ops) != 12:
        fail(f"oracle op 数 {len(ops)} != 12")

    tol = 2e-4
    for name, op in ops.items():
        # 2a. 确定性 (budget 1 vs 4 逐位)
        if op["det"] != "OK":
            fail(f"{name}: 确定性 FAIL (budget 1 vs 4 逐位不一致)")
        # 2b. worker 观测
        a, b = op["acq"]
        if a != 1:
            fail(f"{name}: budget=1 应租借 1 worker (got {a})")
        if HW >= 2 and b < 2:
            fail(f"{name}: budget=4 应租借 ≥2 worker (got {b})")
        # 2c. oracle 独立参考比对 (OUTA; f64 参考, 相对容差)
        ins = {k: v for k, v in op.items() if k.startswith("IN")}
        ref = reference(name, ins)
        got = op["OUTA"]
        if name == "noise":
            med, mad = ref
            for i in range(8):
                if not math.isclose(got[i], med[i], abs_tol=tol,
                                    rel_tol=tol * max(1.0, abs(med[i]))):
                    fail(f"noise med[{i}] got={got[i]} ref={med[i]}")
            # OUT1 (MADσ) 由独立参考: runner 已打印 OUT1A
            o1 = op.get("OUT1A")
            if o1:
                for i in range(8):
                    if not math.isclose(o1[i], mad[i], abs_tol=tol,
                                        rel_tol=tol * max(1.0, abs(mad[i]))):
                        fail(f"noise mad[{i}] got={o1[i]} ref={mad[i]}")
            continue
        for i in range(len(ref)):
            refv = ref[i]
            denom = max(1.0, abs(refv))
            if not math.isclose(got[i], refv, abs_tol=tol * denom,
                                rel_tol=tol):
                fail(f"{name}[{i}]: got={got[i]} ref={refv}")
    if not any(n.startswith("noise") and "mad" in f for f in FAILURES):
        pass
    if FAILURES:
        log(f"\nCPU-002 ORACLE FAIL ({len(FAILURES)})")
        return 1
    log("\nCPU-002 ORACLE PASS (12 kernels oracle f64-rel 2e-4, determinism "
        "budget1vs4 bitwise, worker 1/N observations)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
