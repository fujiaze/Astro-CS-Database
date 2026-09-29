#!/usr/bin/env python3
"""变体 DSO 的测试侧构建助手（R-60 拆 TU 后的唯一测试入口）。

变体 = 门面 TU（零 ISA 旗标: get_api/self_test/注册表）+ 计算面 TU（唯一带 ISA 旗标的 TU，
见 eng/tools/quality/isa_sites.json 的 tu_isolation 与 check_isa_same_source.py 的 S7 判据）。

为什么测试侧必须走本助手:
  · 旧形态"单 TU 直编 + 整命令带 ISA 旗标"已不代表发行产物，且会让自检/握手入口也带上宽
    向量指令（非 GCC 工具链无函数级 ISA 覆盖）—— 那正是 R-60 判红形态，测试若照旧会给出假绿；
  · 共享合同（同一 impl 源）的证据从"一个文件含两个 .inc"变成"计算面 TU 含 impl、门面 TU 含注册表"。
"""
import os
import subprocess

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
HOST = os.path.join(REPO, "lib", "infrastructure", "benchmark", "backend_host")

VARIANTS = {
    "avx2": {"face": "avx2_backend.cpp", "kernels": "avx2_backend_kernels.cpp",
             "isa_flags": ["-mavx2", "-mfma"]},
    "avx512": {"face": "avx512_backend.cpp", "kernels": "avx512_backend_kernels.cpp",
               "isa_flags": ["-mavx512f", "-mavx512bw", "-mavx512vl", "-mavx512dq"]},
}


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
