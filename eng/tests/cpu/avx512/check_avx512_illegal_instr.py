#!/usr/bin/env python3
"""ACSD CPU-004 AVX-512 provider — 非法指令保护静态检查
eng/tests/cpu/avx512/check_avx512_illegal_instr.py

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

R-60 TU 级隔离后的口径变化（本判据只加严不放宽）:
  · 变体 DSO = 门面 TU（**零 ISA 旗标**: query / cap_gate / self_test / 注册表）
    + 计算面 TU（唯一带 -mavx512*）。非 GCC 工具链（MSVC/clang-cl）无函数级指令集
    覆盖 ⇒ 隔离只能在**源文件**层做（见 eng/tools/quality/isa_sites.json 的
    tu_isolation cpuprov-avx512 与 check_isa_same_source.py 的 S7 判据）。
  · 1)「真 EVEX 变体」证据改按**编码层**收: EVEX 是前缀字节 0x62（既不是助记符也不是
    寄存器宽度）—— 数值循环里的 floor / 无符号转换在 -mavx512* 下常被发射成**标量**
    EVEX（VRNDSCALESS / VCVTUSI2SS，无 %zmm）。只认 %zmm 会把真 EVEX 产物判成假变体
    （实测: 计算面 TU 3 条 0x62 编码指令、0 条 %zmm）。判据改为「%zmm ≥ 1 **或** EVEX
    编码指令 ≥ 1」—— 两者都是**本档才可能发射**的证据（在无 AVX-512F 的机器上同样
    #UD），与 check_variant_isa_disasm.py --require-evex 同一编码层口径。
  · 2) query / cap_gate / .init 的断言由「零 %zmm」**收紧**为「零 EVEX 且零 VEX」——
    门面 TU 零旗标后这才可达（改前门面整 TU 带 -mavx512*，VEX 指令合法存在）。
  · 3) 计算面符号名随 TU 拆分为 astrocs_cpuprov_kernel_range_v1（唯一跨 TU 桥）；
    旧名 kernel_pixel_range 作为历史形态留在匹配表里（不得因改名让断言空转）。

实现: 解析 objdump -d 文本; 函数体 = 上一/下一 "<符号>:" 标签行之间。
依赖: binutils objdump (Linux 控制节点标准工具)。退出码 0=全 PASS。
"""
import os
import re
import subprocess
import sys

SYM_RE = re.compile(r"^([0-9a-f]+) <([^>]+)>:")
# objdump 行: addr: byte byte / mnemonic；首字节 0x62 = EVEX, 0xC4/0xC5 = VEX。
INSN_RE = re.compile(r"^\s*[0-9a-f]+:\s+((?:[0-9a-f]{2}\s+)+)")
EVEX_PREFIX, VEX_PREFIXES = "62", ("c4", "c5")

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


def encoding_counts(func_lines):
    """函数体内按**编码前缀**统计 (evex, vex, legacy, unknown) 条数。

    编码层是 ground truth: 0x62 = EVEX, 0xC4/0xC5 = VEX。助记符表会随汇编器方言漂移
    （同一 vmulss 既有 EVEX 形式也有 VEX 形式），字节前缀不会。无字节列的行计入
    unknown —— 调用方据此判 fail-closed，不得当成"零 EVEX"放行。
    """
    out = {"evex": 0, "vex": 0, "legacy": 0, "unknown": 0}
    for ln in func_lines:
        m = INSN_RE.match(ln)
        if not m:
            if ln.strip():
                out["unknown"] += 1
            continue
        first = m.group(1).split()[0]
        if first == EVEX_PREFIX:
            out["evex"] += 1
        elif first in VEX_PREFIXES:
            out["vex"] += 1
        else:
            out["legacy"] += 1
    return out


def body_has_evex_or_vex(func_lines):
    """函数体是否含任何 VEX/EVEX 编码指令（比"零 %zmm"更强: 门面 TU 必须零宽指令）。"""
    c = encoding_counts(func_lines)
    return c["evex"] > 0 or c["vex"] > 0


def body_has_nothing_but_baseline(func_lines):
    """函数体零 VEX 且零 EVEX，且反汇编确有字节列（无字节列 ⇒ fail-closed 判红）。"""
    c = encoding_counts(func_lines)
    if c["evex"] or c["vex"]:
        return False
    return c["legacy"] > 0


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
    funcs = parse_funcs(text)
    total_zmm = len(re.findall(r"%zmm[0-9]+", text))
    total_enc = encoding_counts(text.splitlines())
    log(f"{os.path.basename(so_path)}: 文本段 %zmm 引用数 = {total_zmm}; "
        f"EVEX(0x62) 编码指令 = {total_enc['evex']}, VEX(0xC4/0xC5) = {total_enc['vex']}")
    # 1) 真 EVEX 变体证明。证据面 = %zmm（512-bit 宽寄存器）**或** EVEX 编码指令
    #    (0x62): 后者在无 AVX-512F 的机器上同样 #UD，属本档才可能发射的证据。
    #    标量 EVEX（VCVTUSI2SS/VRNDSCALESS）计入 —— R-60 拆 TU 后计算面 TU 只发射
    #    标量 EVEX（数值循环不被宽向量化），只认 %zmm 会把真变体判成假变体。
    if total_zmm < 1 and total_enc["evex"] < 1:
        fail("provider .so 既无 %zmm 也无 EVEX(0x62) 编码指令 → 非真 AVX-512 变体 "
             "(-mavx512* 未生效/编译器未生成任何本档指令); 非法指令保护的前提"
             "(仅计算面含 EVEX)不成立")
    # 静态 init 段 (dlopen 加载期执行): objdump 对 .init 段的函数标签是
    # <_init>, 而真正执行全局对象构造 (mkstr/kKernels 等) 的是 .init_array
    # 引用的 <_GLOBAL__sub_I_*>/<frame_dummy>。逐一检查这些符号零 %zmm
    # (12 §7: 加载期不得执行 EVEX/SIMD)。
    init_syms = [f for f in funcs
                 if f in ("_init", "frame_dummy", "register_tm_clones") or
                 "_GLOBAL__sub_I_" in f]
    init_checked = 0
    for f in init_syms:
        if body_has_evex_or_vex(funcs[f]):
            c = encoding_counts(funcs[f])
            fail(f"{f} (dlopen 静态初始化) 含 EVEX={c['evex']}/VEX={c['vex']} → "
                 f"加载期宽向量指令, 违反 12 §7")
        elif not body_has_nothing_but_baseline(funcs[f]):
            fail(f"{f} (dlopen 静态初始化) 反汇编无指令字节列 → 编码层判据不可判 "
                 f"(fail-closed)")
        else:
            init_checked += 1
            log(f"{f} 函数体零 EVEX/零 VEX (dlopen 静态初始化只有基线指令)")
    if init_checked == 0:
        fail("未找到静态 init 构造函数符号 (_init/_GLOBAL__sub_I_*) → "
             "静态初始化 EVEX 检查未覆盖")

    for sym in QUERY_SYMS:
        matches = funcs_matching(funcs, sym)
        if not matches:
            fail(f"符号 {sym} 未在反汇编中找到 (可能被优化掉/未导出?)")
            continue
        for fname, fbody in matches:
            c = encoding_counts(fbody)
            if c["evex"] or c["vex"]:
                fail(f"{fname} (匹配 {sym}) 函数体含 EVEX={c['evex']}/VEX={c['vex']} "
                     f"→ query/加载期执行宽向量指令, 非支持 CPU 上 dlopen+query 会 #UD "
                     f"(R-60 硬约束 1: 门面 TU 零 ISA 旗标)")
            elif c["legacy"] == 0:
                fail(f"{fname} (匹配 {sym}) 反汇编无指令字节列 → 编码层判据不可判 "
                     f"(fail-closed)")
            else:
                log(f"{fname} (匹配 {sym}) 函数体零 EVEX/零 VEX ({c['legacy']} 条基线指令; "
                    f"query/握手路径只有 SSE2 级可执行指令)")

    # 一致性: run_kernel 可达的计算面符号必须有本档证据 (%zmm 或 EVEX 编码指令)。
    # 名字: R-60 拆分后计算面入口 = astrocs_cpuprov_kernel_range_v1（唯一跨 TU 桥）;
    # kernel_pixel_range / avx512_run_kernel / run_banded 保留为历史形态匹配
    # （静态符号可能被内联掉，不得因此让断言空转）。
    run_syms = [s for s in funcs
                if "astrocs_cpuprov_kernel_range_v1" in s or "kernel_pixel_range" in s or
                "avx512_run_kernel" in s or "run_banded" in s]
    if run_syms:
        with_ev = [s for s in run_syms
                   if body_has_zmm(funcs[s]) or encoding_counts(funcs[s])["evex"]]
        if not with_ev:
            fail("计算面/执行路径符号无 %zmm 且无 EVEX 编码指令 (虽有 .so 级 EVEX) → "
                 "检查归属; kernel 执行路径应有本档证据")
        else:
            log(f"计算面/执行路径符号含本档证据: {with_ev[:4]}")
    else:
        fail("未找到计算面/执行路径符号 (astrocs_cpuprov_kernel_range_v1 等) → "
             "kernel 执行路径的 EVEX 归属检查未覆盖")

    if FAILURES:
        log(f"\nAVX512 ILLEGAL-INSTR CHECK FAIL ({len(FAILURES)})")
        return 1
    log("\nAVX512 illegal-instr protection: ALL PASS "
        f"(计算面有本档证据: %zmm={total_zmm} / EVEX(0x62)={total_enc["evex"]}; "
        "query/cap_gate/.init 零 EVEX 零 VEX → 非支持 CPU 上 dlopen+query "
        "安全返回 UNSUPPORTED, 无 #UD)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
