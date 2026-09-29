#!/usr/bin/env python3
"""变体 DSO 的测试侧构建助手（R-60 拆 TU 后的唯一测试入口）。

变体 = 门面 TU（零 ISA 旗标: get_api/self_test/注册表）+ 计算面 TU（唯一带 ISA 旗标的 TU，
见 eng/tools/quality/isa_sites.json 的 tu_isolation 与 check_isa_same_source.py 的 S7 判据）。

为什么测试侧必须走本助手:
  · 旧形态"单 TU 直编 + 整命令带 ISA 旗标"已不代表发行产物，且会让自检/握手入口也带上宽
    向量指令（非 GCC 工具链无函数级 ISA 覆盖）—— 那正是 R-60 判红形态，测试若照旧会给出假绿；
  · 共享合同（同一 impl 源）的证据从"一个文件含两个 .inc"变成"计算面 TU 含 impl、门面 TU 含注册表"。

两族各一节（**配方只此一处**，判据与 provider oracle 跑道共用）:
  · build_variant()         —— 第一族 backend_host 变体（astrocs_cpu_<id>.so）;
  · build_cpuprov_variant() —— 第二族 CPU provider 变体（astrocs_cpuprov_<id>.so）：
    门面 TU 零 ISA 旗标 + 计算面 TU 唯一带旗标，两者只经唯一跨 TU 桥
    astrocs_cpuprov_kernel_range_v1 相连。消费者:
      eng/tests/cpu/{avx2,avx512}/run_provider_*_checks.py（CPU-003/004 oracle）
      eng/tests/backend/test_cpuprov_isa_variants.py（产物级 ISA 面判据）
"""
import os
import subprocess

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
HOST = os.path.join(REPO, "lib", "infrastructure", "benchmark", "backend_host")
CPU_SRC = os.path.join(REPO, "lib", "infrastructure", "benchmark", "cpu")
CAPSRC = os.path.join(CPU_SRC, "common", "src", "capability_detect.c")

VARIANTS = {
    "avx2": {"face": "avx2_backend.cpp", "kernels": "avx2_backend_kernels.cpp",
             "isa_flags": ["-mavx2", "-mfma"]},
    "avx512": {"face": "avx512_backend.cpp", "kernels": "avx512_backend_kernels.cpp",
               "isa_flags": ["-mavx512f", "-mavx512bw", "-mavx512vl", "-mavx512dq"]},
}


# 第二族（CPU provider）变体：门面 TU 零旗标 / 计算面 TU 唯一带旗标。
# isa_flags 与根 CMakeLists.txt 的 astrocs_cpuprov_<v>_kernels 逐条同源
# （站点登记见 eng/tools/quality/isa_sites.json 的 product-cpuprov-*）。
CPUPROV_VARIANTS = {
    "avx2": {"face": "avx2/src/avx2_provider.cpp",
             "kernels": "avx2/src/avx2_kernels.cpp",
             "isa_flags": ["-mavx2", "-mfma"]},
    "avx512": {"face": "avx512/src/avx512_provider.cpp",
               "kernels": "avx512/src/avx512_kernels.cpp",
               "isa_flags": ["-mavx512f", "-mavx512cd", "-mavx512bw",
                             "-mavx512dq", "-mavx512vl"]},
}
BRIDGE_SYMBOL = "astrocs_cpuprov_kernel_range_v1"


def cpuprov_isa_flags(variant):
    """第二族变体**计算面 TU** 的 ISA 旗标（与 isa_sites.json 的 product-cpuprov-* 逐条同源）。"""
    return list(CPUPROV_VARIANTS[variant]["isa_flags"])


def cpuprov_face_src(variant):
    return os.path.join(CPU_SRC, CPUPROV_VARIANTS[variant]["face"])


def cpuprov_kernels_src(variant):
    return os.path.join(CPU_SRC, CPUPROV_VARIANTS[variant]["kernels"])


def isa_flags(variant):
    """该变体计算面 TU 的 ISA 旗标（GCC/Clang 形态；MSVC 走 /arch: 档位，见 isa_sites.json）。"""
    return list(VARIANTS[variant]["isa_flags"])


def build_variant(host, inc, variant, out, extra_inc=(), timeout=300):
    """两步编译 + 链接。返回 (returncode, stderr)。"""
    spec = VARIANTS[variant]
    incs = [f"-I{inc}", f"-I{host}"] + [f"-I{p}" for p in extra_inc]
    base = ["g++", "-std=c++17", "-O2", "-DNDEBUG", "-fPIC", "-Wall", "-Wextra", *incs]
    tmpdir = os.path.dirname(out) or "."
    for src, obj, extra in ((spec["face"], os.path.join(tmpdir, variant + "_face.o"), []),
                            (spec["kernels"], os.path.join(tmpdir, variant + "_kernels.o"),
                             spec["isa_flags"])):
        r = subprocess.run(base + extra + ["-c", os.path.join(host, src), "-o", obj],
                           capture_output=True, text=True, timeout=timeout)
        if r.returncode != 0:
            return r.returncode, r.stderr
    r = subprocess.run(["g++", "-shared", os.path.join(tmpdir, variant + "_face.o"),
                        os.path.join(tmpdir, variant + "_kernels.o"), "-o", out],
                       capture_output=True, text=True, timeout=timeout)
    return r.returncode, r.stderr


def _cpuprov_incs(variant):
    """计算面/门面 TU 的 include 面（与根 CMakeLists.txt 的同名 target 逐条同源）。"""
    return ["-I" + os.path.join(REPO, "lib", "include"),
            "-I" + os.path.join(CPU_SRC, "common", "include"),
            "-I" + os.path.join(CPU_SRC, variant, "include"),
            "-I" + os.path.join(CPU_SRC, "baseline", "include")]


def compile_cpuprov_kernels(variant, obj, flags=None, capsrc=None,
                            cxxflags=("-O2", "-DNDEBUG", "-fPIC"),
                            warnings=("-Wall", "-Wextra", "-Wpedantic"),
                            timeout=300, cxx="g++"):
    """只编译第二族变体的**计算面 TU**（带该族登记旗标）到 obj。返回 (returncode, stderr)。

    用于需要把门面 TU 直接链进可执行文件的判据（capability gate stub 负测、handshake、
    so_load）: 这些判据要替换探测符号或 dlopen DSO，门面 TU 必须与发行产物同形
    （零 ISA 旗标），计算面则按发行口径单独编译后一并链接。
    """
    spec = CPUPROV_VARIANTS[variant]
    use = list(spec["isa_flags"]) if flags is None else list(flags)
    cmd = [cxx, "-std=c++17", *cxxflags, *warnings, *_cpuprov_incs(variant), *use]
    src = capsrc if capsrc else os.path.join(CPU_SRC, spec["kernels"])
    r = subprocess.run(cmd + ["-c", src, "-o", obj],
                       capture_output=True, text=True, timeout=timeout)
    return r.returncode, r.stderr


def build_cpuprov_variant(variant, out, extra_link_inputs=(), ld_extra=(),
                          cxxflags=("-O2", "-DNDEBUG", "-fPIC"),
                          warnings=("-Wall", "-Wextra", "-Wpedantic"),
                          face_flags=None, kernels_flags=None, timeout=300, cxx="g++"):
    """两 TU 编译 + 链接第二族 provider DSO。返回 (returncode, stderr)。

    正例: face_flags/kernels_flags 省略 ⇒ 门面**零** ISA 旗标、计算面带登记旗标（R-60 配方）。
    负例注入（判据必须因此判红，正例不得使用）:
      · face_flags=<该族旗标>     —— 回到"整 TU 带旗标"形态（握手入口与高指令集同 TU）;
      · kernels_flags=[]          —— 计算面不编旗标（变体与基线同码 = 假变体）。
    extra_link_inputs: 额外链接输入（如已单独编译的 capability_detect.o；探测路径必须
    零 SIMD 旗标 —— CPU-001 契约"探测自身只用 SSE2 可执行指令"）。
    """
    spec = CPUPROV_VARIANTS[variant]
    incs = _cpuprov_incs(variant)
    base = [cxx, "-std=c++17", *cxxflags, *warnings, *incs]
    tmpdir = os.path.dirname(out) or "."
    os.makedirs(tmpdir, exist_ok=True)
    jobs = []
    # 关键不变量（R-60 硬约束 1）: 门面 TU 的旗标只能是**显式传入的**覆盖值;
    # None = "不覆盖" 对门面意味着**零旗标**，绝不可回落到该族的 ISA 旗标
    # （否则本函数自己就成了它要防的那类假变体 —— 实测踩过：门面被编出 75 条
    #  VEX/EVEX，cap_gate 18 条）。
    for key, tag, override, default in (("face", "face", face_flags, []),
                                        ("kernels", "kernels", kernels_flags,
                                         list(spec["isa_flags"]))):
        flags = list(default) if override is None else list(override)
        obj = os.path.join(tmpdir, "cpuprov_%s_%s.o" % (variant, tag))
        jobs.append((spec[key], obj, flags))
    objs = []
    for src, obj, flags in jobs:
        r = subprocess.run(base + flags + ["-c", os.path.join(CPU_SRC, src), "-o", obj],
                           capture_output=True, text=True, timeout=timeout)
        if r.returncode != 0:
            return r.returncode, r.stderr
        objs.append(obj)
    r = subprocess.run([cxx, "-shared", *objs, *extra_link_inputs, "-o", out, *ld_extra],
                       capture_output=True, text=True, timeout=timeout)
    return r.returncode, r.stderr
