#!/usr/bin/env python3
"""baseline opcode scanner (05 §2/05 §7) — ABI-003 验收门 1。
反汇编 baseline_backend.o, 禁止超出最低 amd64(SSE2)合同的 SIMD 指令。
"""
import re, subprocess, sys

AVX_MNEMONICS = re.compile(
    r"\b(v(?:add|sub|mul|div|mov|fm|broadcast|blend|pxor|ptest|gather|extract|insert|"
    r"perm|shuffle|round|sqrt|rcp|rsqrt|hadd|hsub|unpck|punpck|cvtd|cvtp|cvt|and|or|xor)"
    r"[a-z0-9]*)\b")
YMM_ZMM = re.compile(r"%[yz]mm\d*")
SSE_ALLOWED = re.compile(r"\b(mov|add|sub|mul|div|ucomis|comis|cvts|pxor|xorps|andps|"
                         r"orps|movaps|movups|movss|movs|sqrt|rsqrt|max|min|punpck|p[a-z]+)\w*\b")


def main():
    obj = sys.argv[1]
    dis = subprocess.run(["objdump", "-d", obj], capture_output=True, text=True, timeout=120)
    if dis.returncode != 0:
        print(f"OBJDUMP_FAIL {dis.stderr[:200]}")
        return 2
    bad = []
    for line in dis.stdout.splitlines():
        if YMM_ZMM.search(line):
            bad.append(("ymm/zmm register", line.strip()))
            continue
        m = AVX_MNEMONICS.search(line)
        if m:
            # v-前缀助记符均为 VEX/AVX 家族; SSE2 无 v-前缀
            bad.append((f"AVX mnemonic {m.group(1)}", line.strip()))
    if bad:
        print(f"BASELINE_OPCODE_FAIL ({len(bad)})")
        for why, ln in bad[:20]:
            print(f"  {why}: {ln[:100]}")
        return 1
    n_ins = len([l for l in dis.stdout.splitlines() if "\t" in l and " " in l.strip()[:2]])
    print(f"BASELINE_OPCODE_PASS instructions~{n_ins} no-VEX/ymm/zmm")
    return 0


if __name__ == "__main__":
    sys.exit(main())
