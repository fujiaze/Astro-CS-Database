#!/usr/bin/env python3
"""AstroCS CPU-004 AVX-512 provider — 验收运行器 (全部测试)
tests/cpu/avx512/run_provider_avx512_checks.py

验收 (04_CPU_RESOURCE_TASKS.md CPU-004):
  1. 只迁移实测可能获益 kernel: avx512 表恰 1 热点 hips-bulk-transform
     (ISA-004 实测 avx512 +29.5% vs baseline ≈avx2 +28.3%; calibration
     +3.8% 与 drizzle-accumulate −22.5% NOT_SHIPPED) —— handshake/
     so_load 断言 kernel_list=1;
  2. target 单独 -mavx512*: baseline/avx2/oracle 各 TU 零 avx512 旗标,
     avx512 provider 源单独 -mavx512f -mavx512cd -mavx512bw -mavx512dq
     -mavx512vl (编译隔离 15 §6) —— runner 编译命令即证据;
  3. 所需 AVX-512 子集 + XCR0 ZMM: capability gate stub 负测 (探测失败/
     缺子集 F-only/OS 不保存 ZMM xcr0=0x6/无 AVX-512 hw → 拒绝;
     全 AVX-512 xcr0=0xE6 → 通过) —— provider_avx512_capability_gate_test.c;
  4. 缺任何子集/OS ZMM state 拒绝: 同上 (CPUID/XGETBV negative; 真实
     通过路径由 handshake/so_load/oracle 覆盖: 本机 xcr0 实测含 0xE0);
  5. 非法指令保护: 反汇编静态检查 (kernel 路径含 %zmm; query/cap_gate/
     .init 零 EVEX → 非支持 CPU 上 dlopen+query 安全, 无 #UD) ——
     check_avx512_illegal_instr.py;
  6. baseline/AVX2/AVX512 oracle: 三路 dlopen 对照 (hex 输出; avx512/
     avx2 vs baseline 相对差 ≤ 2e-4; determinism budget1 vs 4 逐位;
     worker 观测) —— provider_avx512_oracle_main.cpp + 本 runner 判;
  7. 每 kernel 可退回: avx512 表外 kernel (calibration-pixel-transform
     在 avx512 表查不到 → NOT_FOUND, avx2/baseline 表查到; run_kernel
     表外索引 → ACS_ERR_UNSUPPORTED) —— oracle/handshake/so_load 断言;
  8. FMA/AVX-512 归约顺序记录: ULP_MAX 打印入日志 (hips 无跨项/跨线程
     归约, 顺序不变; 差异仅 FMA/EVEX 单次舍入)。

依赖: 仅标准库 + objdump (binutils)。输出退出码 0=全 PASS。
"""
import os
import struct
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
AVX512_SRC = os.path.join(REPO, "providers", "cpu", "avx512", "src",
                          "avx512_provider.cpp")
AVX2_SRC = os.path.join(REPO, "providers", "cpu", "avx2", "src",
                        "avx2_provider.cpp")
BASE_SRC = os.path.join(REPO, "providers", "cpu", "baseline", "src",
                        "baseline_provider.cpp")
CAPSRC = os.path.join(REPO, "providers", "cpu", "common", "src",
                      "capability_detect.c")
ORACLE = os.path.join(REPO, "tests", "cpu", "avx512",
                      "provider_avx512_oracle_main.cpp")
GATE = os.path.join(REPO, "tests", "cpu", "avx512",
                    "provider_avx512_capability_gate_test.c")
HANDSHAKE = os.path.join(REPO, "tests", "cpu", "avx512",
                         "provider_avx512_handshake_test.c")
SO_LOAD = os.path.join(REPO, "tests", "cpu", "avx512",
                       "provider_avx512_so_load_test.c")
ILLEGAL = os.path.join(REPO, "tests", "cpu", "avx512",
                       "check_avx512_illegal_instr.py")
INC_ROOT = os.path.join(REPO, "include")
INC_BASE = os.path.join(REPO, "providers", "cpu", "baseline", "include")
INC_AVX2 = os.path.join(REPO, "providers", "cpu", "avx2", "include")
INC_AVX512 = os.path.join(REPO, "providers", "cpu", "avx512", "include")
INC_CAP = os.path.join(REPO, "providers", "cpu", "common", "include")

FAILURES = []
HW = os.cpu_count() or 1
TOL = 2e-4  # baseline 对照容差 (ALG oracle 同规, 冻结于 avx512 头 §6)

AVX512_FLAGS = ["-mavx512f", "-mavx512cd", "-mavx512bw",
                "-mavx512dq", "-mavx512vl"]


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
    return struct.unpack("<f", struct.pack("<I", u))[0]


def main():
    tmp = "/tmp/cpu004_avx512"
    os.makedirs(tmp, exist_ok=True)
    so_b = os.path.join(tmp, "astrocs_cpu_baseline.so")
    so_a = os.path.join(tmp, "astrocs_cpu_avx2.so")
    so_x = os.path.join(tmp, "astrocs_cpu_avx512.so")
    # capability_detect.c 独立编译为无 SIMD 旗标对象 (探测路径零 EVEX;
    # 见 check_avx512_illegal_instr.py 说明 —— 非支持 CPU 上 query 必须先
    # 安全完成探测再拒绝, 探测自身不得含 AVX-512 指令)。C 编译避免
    # -mavx512* 经 g++ 连同 detect 一并向量化。
    cap_obj = os.path.join(tmp, "capability_detect.o")
    log(f"repo={REPO} hw_cpus={HW}")

    # 0) capability_detect.c → 无任何 -mavx* 旗标的独立对象 (纯 C; 探测
    #    路径永不生成 EVEX —— CPU-001 契约 "探测自身只用 SSE2 可执行指令")
    r = run(["gcc", "-std=c11", "-O2", "-DNDEBUG", "-fPIC", "-c",
             "-Wall", "-Wextra", "-Wpedantic",
             f"-I{INC_ROOT}", f"-I{INC_CAP}",
             CAPSRC, "-o", cap_obj], timeout=300)
    if r.returncode != 0:
        fail("capability_detect.c 独立对象编译")
        return 1

    # 1) 编译 baseline .so (无 -mavx*; 保守 SSE2) + capability_detect.o
    r = run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-fPIC", "-shared",
             "-Wall", "-Wextra", "-Wpedantic",
             f"-I{INC_ROOT}", f"-I{INC_BASE}", f"-I{INC_CAP}",
             BASE_SRC, cap_obj, "-o", so_b, "-lpthread"], timeout=300)
    if r.returncode != 0:
        fail("baseline .so 编译")
        return 1

    # 2) 编译 avx2 .so (仅此 target 带 -mavx2 -mfma; 编译隔离 15 §6)
    r = run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-mavx2", "-mfma",
             "-fPIC", "-shared", "-Wall", "-Wextra", "-Wpedantic",
             f"-I{INC_ROOT}", f"-I{INC_BASE}", f"-I{INC_AVX2}", f"-I{INC_CAP}",
             AVX2_SRC, cap_obj, "-o", so_a, "-lpthread"], timeout=300)
    if r.returncode != 0:
        fail("avx2 .so 编译 (-mavx2 -mfma)")
        return 1

    # 3) 编译 avx512 .so (唯一带 -mavx512* 的 target; 编译隔离 15 §6)。
    #    capability_detect.o 独立链接 (无 -mavx512* 旗标) —— 探测路径
    #    零 EVEX; 见 check_avx512_illegal_instr.py
    r = run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-fPIC", "-shared",
             "-Wall", "-Wextra", "-Wpedantic"] + AVX512_FLAGS +
            [f"-I{INC_ROOT}", f"-I{INC_BASE}", f"-I{INC_AVX512}",
             f"-I{INC_CAP}", AVX512_SRC, cap_obj, "-o", so_x, "-lpthread"],
            timeout=300)
    if r.returncode != 0:
        fail("avx512 .so 编译 (-mavx512*)")
        return 1

    # 4) 非法指令保护静态检查 (反汇编; 本机全 AVX-512 无 #UD, 指令位置合同)
    r = run(["python3", ILLEGAL, so_x], timeout=300)
    if r.returncode != 0 or "ALL PASS" not in r.stdout:
        fail("非法指令保护静态检查")

    # 5) 编译对照 oracle main (本 TU 无 -mavx*; dlopen 三 .so)
    exe = os.path.join(tmp, "avx512_oracle")
    r = run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-Wall", "-Wextra",
             "-Wpedantic", f"-I{INC_ROOT}", f"-I{INC_BASE}", f"-I{INC_AVX2}",
             f"-I{INC_AVX512}", f"-I{INC_CAP}", ORACLE, "-o", exe, "-ldl"],
            timeout=300)
    if r.returncode != 0:
        fail("oracle main 编译")
        return 1
    r = run([exe, so_b, so_a, so_x], timeout=300)
    if r.returncode != 0:
        fail("oracle main 运行")
        return 1
    if "AVX512_ORACLE_DONE" not in r.stdout or \
            "alloc_balance=0" not in r.stdout:
        fail("oracle runner 未完成 / allocator 不平衡")

    # 6) 解析 oracle 输出并断言
    #    OP 行键 = "<kernel_id>#<用例序号>" (同一热点 kernel 多次用例同名,
    #    必须带序号区分; 见 provider_avx512_oracle_main.cpp)。op_name =
    #    去序号的 kernel_id (仅日志展示用)。
    ops = {}
    cur = None
    for line in r.stdout.splitlines():
        if line.startswith("OP "):
            cur = {"op": line.split()[1], "name": line.split()[1].rsplit("#", 1)[0],
                   "axA": None, "axB": None, "a2": None, "base": None,
                   "det": None, "acq": None, "relx": None, "rela": None}
            ops[cur["op"]] = cur
        elif cur is None:
            continue
        elif line.startswith("AVX512A "):
            cur["axA"] = [b2f(int(x, 16)) for x in line.split()[1:]]
        elif line.startswith("AVX512B "):
            cur["axB"] = [b2f(int(x, 16)) for x in line.split()[1:]]
        elif line.startswith("AVX2 "):
            cur["a2"] = [b2f(int(x, 16)) for x in line.split()[1:]]
        elif line.startswith("BASE "):
            cur["base"] = [b2f(int(x, 16)) for x in line.split()[1:]]
        elif line.startswith("DET "):
            cur["det"] = line.split()[1]
        elif line.startswith("ACQ "):
            _, a, b = line.split()
            cur["acq"] = (int(a), int(b))
        elif line.startswith("REL512_MAX "):
            cur["relx"] = float(line.split()[1])
        elif line.startswith("REL2_MAX "):
            cur["rela"] = float(line.split()[1])
        elif line.startswith("ULP512_MAX "):
            cur["ulpx"] = float(line.split()[1])
        elif line.startswith("ULP2_MAX "):
            cur["ulpa"] = float(line.split()[1])
        elif line.startswith("NONHOT_AVX512"):
            if "NOT_FOUND" not in line:
                fail("calibration-pixel-transform 应在 avx512 表查不到 "
                     "(NOT_SHIPPED, 退回 avx2/baseline)")
        elif line.startswith("NONHOT_AVX2"):
            if "NOT_FOUND" in line:
                fail("calibration-pixel-transform 应在 avx2 表查到 (SHIP)")
        elif line.startswith("NONHOT_BASE"):
            if "NOT_FOUND" in line:
                fail("calibration-pixel-transform 应在 baseline 表查到")

    if len(ops) != 2:
        fail(f"oracle op 数 {len(ops)} != 2")
    for key, op in ops.items():
        if op["axA"] is None or op["a2"] is None or op["base"] is None:
            fail(f"{key}: 输出缺失 (avx512/avx2/base)")
            continue
        # 6a. determinism budget1 vs 4 逐位
        if op["det"] != "OK":
            fail(f"{key}: determinism FAIL (budget 1 vs 4 非逐位相同)")
        # 6b. worker 观测
        a, b = op["acq"]
        if a != 1:
            fail(f"{key}: budget=1 应租借 1 (got {a})")
        if HW >= 2 and b < 2:
            fail(f"{key}: budget=4 应租借 ≥2 (got {b})")
        # 6c. baseline 对照容差 (三路: avx512 与 avx2 均 ≤ 2e-4)
        if op["relx"] is None or op["relx"] > TOL:
            fail(f"{key}: avx512 vs baseline REL={op['relx']} > {TOL}")
        if op["rela"] is None or op["rela"] > TOL:
            fail(f"{key}: avx2 vs baseline REL={op['rela']} > {TOL}")
        # 6d. FMA/AVX-512 归约顺序记录 (无断言阈值, 记录入日志)
        log(f"{op['name']} ({key}): avx512-vs-baseline "
            f"max_rel={op['relx']:.3g} "
            f"max_ulp={op['ulpx']}; avx2-vs-baseline max_rel={op['rela']:.3g} "
            f"max_ulp={op['ulpa']}")
        log(f"  → AVX-512/FMA 归约顺序记录: hips 无跨项/跨线程归约, 顺序不变;"
            f" 差异仅 FMA/EVEX 单次舍入 (见 avx512_provider_v1.h §6)")

    # 7) capability gate stub 负测 (缺子集/OS 无 ZMM/无 hw 拒绝; 全通过)
    gate_exe = os.path.join(tmp, "avx512_gate")
    r = run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Wpedantic",
             f"-I{INC_AVX512}", f"-I{INC_CAP}", f"-I{INC_BASE}",
             f"-I{INC_ROOT}", GATE, AVX512_SRC,
             "-o", gate_exe, "-lstdc++", "-lpthread"], timeout=300)
    if r.returncode != 0:
        fail("capability gate 编译")
        return 1
    r = run([gate_exe], timeout=300)
    if r.returncode != 0 or "ALL PASS" not in r.stdout:
        fail("capability gate 负测")

    # 8) handshake (真实 CPUID/XGETBV 通过路径 + kernel_list=1 + 回落)
    hs_exe = os.path.join(tmp, "avx512_handshake")
    r = run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Wpedantic",
             f"-I{INC_AVX512}", f"-I{INC_CAP}", f"-I{INC_BASE}",
             f"-I{INC_ROOT}", HANDSHAKE, AVX512_SRC, cap_obj,
             "-o", hs_exe, "-lstdc++", "-lpthread"], timeout=300)
    if r.returncode != 0:
        fail("handshake 编译")
        return 1
    r = run([hs_exe], timeout=300)
    if r.returncode != 0 or "ALL PASS" not in r.stdout:
        fail("handshake 测试")

    # 9) so_load (dlopen 唯一导出 + kernel_list=1 + hips 冒烟 + 退回)
    sl_exe = os.path.join(tmp, "avx512_so_load")
    r = run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Wpedantic",
             f"-I{INC_AVX512}", f"-I{INC_CAP}", f"-I{INC_BASE}",
             f"-I{INC_ROOT}", SO_LOAD, "-o", sl_exe, "-ldl"], timeout=300)
    if r.returncode != 0:
        fail("so_load 编译")
        return 1
    r = run([sl_exe, so_x], timeout=300)
    if r.returncode != 0 or "ALL PASS" not in r.stdout:
        fail("so_load 测试")

    if FAILURES:
        log(f"\nCPU-004 AVX512 FAIL ({len(FAILURES)})")
        return 1
    log("\nCPU-004 AVX512 PASS (1 hotspot kernel hips-bulk; "
        "baseline/avx2/avx512 oracle rel≤2e-4; determinism budget1vs4 "
        "bitwise; capability gate negative; illegal-instr static check; "
        "non-hotspot fallback)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
