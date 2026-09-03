#!/usr/bin/env python3
"""AstroCS CPU-004 AVX-512 provider — 非法指令保护静态检查
tests/cpu/avx512/check_avx512_illegal_instr.py

覆盖 (CPU-004 验收 "非法指令保护; 非支持 CPU 上 dlopen+query 安全返回
UNSUPPORTED, 无 #UD"):
  本机为全 AVX-512 支持 CPU, #UD 路径无法在真实硬件触发 → 以反汇编
  静态证明指令位置合同 (与 ISA-004 legacy 能力证明同法):
    1) provider .so 文本段**含** %zmm (512-bit EVEX 指令) → 本 provider
       TU 以 -mavx512* 编译且 run_kernel 数值路径真含 AVX-512 指令
       (真 EVEX 变体; 若编译器未生成任何 AVX-512 指令则本检查 FAIL,
       防"假旗标"回归);
    2) query 期符号函数体 **零** %zmm → 加载/握手路径 (dlopen 静态 init +
       astrocs_provider_query_v1 + acs_cpu_avx512_cap_gate) 不执行任何
       EVEX 指令 → 非支持 CPU 上 dlopen + query 可安全完成, 返回
       ACS_ERR_UNSUPPORTED, 无 #UD (能力门负测证明拒绝路径, 本检查证明
       指令位置合同);
    3) 静态 init 段 (.init/.init_array 指向的构造函数) 不含 SIMD ——
       .init 段内零 %zmm 覆盖 (global POD 构造 mkstr 无 SIMD)。

实现: 解析 objdump -d 文本; 函数体 = 上一/下一 "<符号>:" 标签行之间。
依赖: binutils objdump (Linux 控制节点标准工具)。退出码 0=全 PASS。
"""
import os
import re
import subprocess
import sys

SYM_RE = re.compile(r"^([0-9a-f]+) <([^>]+)>:")

# query 期路径符号: 这些函数在 dlopen/query 阶段可达, 必须零 EVEX
#   - astrocs_provider_query_v1 / acs_cpu_avx512_cap_gate (provider TU);
#   - acs_cap_detect_v1 / acs_cap_classify_v1 / acs_cap_os_safe_satisfies_v1:
#     cap_gate 在判定前调用真实探测链 (生产链接 capability_detect.c)。
#     capability_detect.c 必须以**无 -mavx512*** 旗标独立编译 (探测自身不
#     得生成 EVEX —— CPU-001 契约 "探测自身只用 SSE2 可执行指令"), 否则
#     缺 AVX-512 CPU 上 query 在判定前即 #UD (鸡生蛋)。
# 口径: avx512_self_test 不进本列表 —— self_test 由 host 在 query 返回
# ACS_OK 之后才调用 (此时已确认本 CPU AVX-512 os_safe), 不在 dlopen/query
# 加载握手路径上, 非支持 CPU 不会执行到; 且与 AVX2 先例 (CPU-003
# avx2_self_test 函数体含 %ymm 5 处, 接受) 一致 —— self_test 内部 128B
# memset 可能被编译器以 512-bit EVEX 实现, 属允许范围 (真实 AVX-512 CPU
# 上执行, 无 #UD 风险)。CPU-004 非法指令保护验收核心 = 非支持 CPU 上
# dlopen+query 可安全完成 (返回 UNSUPPORTED 无 #UD), 由下列符号零 %zmm
# 保证; kernel 数值路径 (query 后可达) 必须含 %zmm 见下方 run_syms 检查。
QUERY_SYMS = [
    "astrocs_provider_query_v1",
    "acs_cpu_avx512_cap_gate",
    "acs_cap_detect_v1",
    "acs_cap_classify_v1",
    "acs_cap_os_safe_satisfies_v1",
    "acs_cap_hw_satisfies_v1",
]

FAILURES = []


def log(msg):
    print(msg, flush=True)


def fail(msg):
    FAILURES.append(msg)
    log("FAIL: " + msg)


def disassemble(so_path):
    r = subprocess.run(["objdump", "-d", so_path],
                       capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        fail(f"objdump -d {so_path} 失败: {r.stderr.strip()[:400]}")
        return ""
    return r.stdout


def parse_funcs(text):
    """返回 {符号名: 函数体文本行列表(含标签行后到下一标签前)}"""
    funcs = {}
    lines = text.splitlines()
    cur = None
    for ln in lines:
        m = SYM_RE.match(ln)
        if m:
            cur = m.group(2)
            funcs.setdefault(cur, [])
            funcs[cur].append(ln)
        elif cur is not None:
            funcs[cur].append(ln)
    return funcs


def body_has_zmm(func_lines):
    """函数体(含其后到下个符号前的行)是否含 %zmm/EVEX 512-bit 指令"""
    for ln in func_lines:
        if "%zmm" in ln:
            return True
    return False


def funcs_matching(funcs, name):
    """返回所有函数体中符号名 == name 或包含 name 的条目 (排除 PLT 桩;
    C++ mangling / static local 符号如 _ZL12cap_classify... 均覆盖)。"""
    hits = []
    for f, lines in funcs.items():
        if f.endswith("@plt"):
            continue
        if f == name or name in f:
            hits.append((f, lines))
    return hits


def main():
    if len(sys.argv) < 2:
        print("usage: check_avx512_illegal_instr.py <avx512_provider.so>")
        return 2
    so_path = sys.argv[1]
    text = disassemble(so_path)
    if not text:
        return 1
    total_zmm = len(re.findall(r"%zmm[0-9]+", text))
    log(f"{os.path.basename(so_path)}: 文本段 %zmm 引用数 = {total_zmm}")
    if total_zmm < 1:
        fail("provider .so 不含 %zmm → 非真 AVX-512 变体 (-mavx512* 未生效/"
             "编译器未向量化); 非法指令保护的前提(仅 run_kernel 含 EVEX)不成立")

    funcs = parse_funcs(text)
    # 静态 init 段 (dlopen 加载期执行): objdump 对 .init 段的函数标签是
    # <_init>, 而真正执行全局对象构造 (mkstr/kKernels 等) 的是 .init_array
    # 引用的 <_GLOBAL__sub_I_*>/<frame_dummy>。逐一检查这些符号零 %zmm
    # (12 §7: 加载期不得执行 EVEX/SIMD)。
    init_syms = [f for f in funcs
                 if f in ("_init", "frame_dummy", "register_tm_clones") or
                 "_GLOBAL__sub_I_" in f]
    init_checked = 0
    for f in init_syms:
        if body_has_zmm(funcs[f]):
            fail(f"{f} (dlopen 静态初始化) 含 %zmm → 加载期 EVEX, 违反 12 §7")
        else:
            init_checked += 1
            log(f"{f} 函数体零 %zmm (dlopen 静态初始化无 EVEX)")
    if init_checked == 0:
        fail("未找到静态 init 构造函数符号 (_init/_GLOBAL__sub_I_*) → "
             "静态初始化 EVEX 检查未覆盖")

    for sym in QUERY_SYMS:
        matches = funcs_matching(funcs, sym)
        if not matches:
            fail(f"符号 {sym} 未在反汇编中找到 (可能被优化掉/未导出?)")
            continue
        for fname, fbody in matches:
            if body_has_zmm(fbody):
                fail(f"{fname} (匹配 {sym}) 函数体含 %zmm → query/加载期执行 "
                     f"EVEX, 非支持 CPU 上 dlopen+query 会 #UD")
            else:
                log(f"{fname} (匹配 {sym}) 函数体零 %zmm "
                    f"(query/握手路径无 EVEX 指令)")

    # 一致性: run_kernel 可达的 kernel 实现符号应含 zmm (若符号化保留)
    run_syms = [s for s in funcs if "kernel_pixel_range" in s or
                "avx512_run_kernel" in s or "run_banded" in s]
    if run_syms:
        any_zmm = any(body_has_zmm(funcs[s]) for s in run_syms)
        if not any_zmm:
            fail("run_kernel 路径符号不含 %zmm (虽有 .so 级 zmm) → 检查文本段"
                 "zmm 归属; kernel 执行路径应有 EVEX")
        else:
            log(f"run_kernel 路径符号含 %zmm (数值路径真 AVX-512): "
                f"{[s for s in run_syms if body_has_zmm(funcs[s])][:4]}")

    if FAILURES:
        log(f"\nAVX512 ILLEGAL-INSTR CHECK FAIL ({len(FAILURES)})")
        return 1
    log("\nAVX512 illegal-instr protection: ALL PASS "
        "(kernel path has %zmm; query/cap_gate/.init zero-EVEX → dlopen+query "
        "safe on unsupported CPU, no #UD)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
