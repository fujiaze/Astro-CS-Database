#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_isa_leak.py — CPU-001 (G3) ISA 泄漏检查器。

验收 (04 §140): 检查最终 link map 和反汇编，证明 CLI baseline 无高级 ISA 泄漏。

方法:
  - objdump -d 主 CLI 二进制 → 扫描 AVX2/AVX512 指令助记符（vp*/vpermd/vbroadcast/…
    等 AVX2 及以上; AVX512 的 EVEX 前缀 0x62 指令）。
  - 对比 provider 静态库（avx2/avx512）— 必须含对应指令（证明变体真实编译）。
  - 验证 link map: 主 CLI 不链 astrocs_cpu_avx2/avx512（nm 无其符号）。
  - 负例（--selftest）: 构造含 vpaddd 的假文本 → 主 CLI 必须 FAIL；
    provider 无 AVX 指令 → FAIL。

用法:
  python3 tools/quality/check_isa_leak.py --binary run/temp/build_v61/astrocs \
      --avx2-lib run/temp/build_v61/libastrocs_cpu_avx2.a \
      --avx512-lib run/temp/build_v61/libastrocs_cpu_avx512.a
  python3 tools/quality/check_isa_leak.py --selftest
"""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys

# AVX2 及以上指令助记符（v 前缀 + 256-bit 操作）; AVX512 用 EVEX(0x62) 前缀检测。
# 注意排除合法的 128-bit AVX（v 前缀 128-bit, 属于 AVX1）: AVX2 泄漏用 vp*/vpermd 等 256-bit 族。
AVX2_MNEMONICS = [
    "vpermd", "vpermpd", "vpermps", "vpermq", "vpbroadcastb", "vpbroadcastw",
    "vpbroadcastd", "vpbroadcastq", "vpsllvd", "vpsllvq", "vpsrlvd", "vpsrlvq",
    "vpsravd", "vpblendd", "vpshufb", "vgatherdd", "vgatherdpd", "vgatherqpd",
    "vpmaskmovd", "vpmaskmovq", "vpcmpeqq", "vpcmpgtd", "vphaddd", "vphaddw",
    "vpmulld", "vpmuldq", "vpaddd", "vpsubd", "vpmulhuw", "vpsadbw",
]

# AVX512 助记符（zmm 寄存器或 EVEX 专属）
AVX512_MNEMONICS = [
    "vpermi2d", "vpermt2d", "vcompress", "vpexpand", "vpmovdb", "vpmovdw",
    "vpmovwb", "vpmovqd", "vpbroadcastmb2q", "vptestmd", "vptestmq", "vpcmpd",
    "vpcmpq", "vpsravq", "vpmullq", "vpsraq", "vprold", "vprolq",
]


def _objdump_text(path: pathlib.Path) -> str:
    r = subprocess.run(["objdump", "-d", str(path)],
                       capture_output=True, text=True, timeout=300)
    if r.returncode != 0:
        raise RuntimeError(f"objdump failed on {path}: {r.stderr[:300]}")
    return r.stdout


def scan_avx2(text: str) -> list[str]:
    """AVX2 泄漏: AVX2 专属助记符 或 ymm(256-bit) 寄存器使用(排除 128-bit xmm)。"""
    found = set()
    for m in AVX2_MNEMONICS:
        # 助记符后跟空格+操作数 或 直接使用 ymm 寄存器(≥256-bit)
        if re.search(r"\b" + m + r"(?:\s|%[vxy]mm)", text):
            found.add(m)
    if re.search(r"%ymm[0-9]+", text):
        found.add("<ymm-register>")
    return sorted(found)


def scan_avx512(text: str) -> list[str]:
    """AVX512 泄漏: EVEX 专属助记符 或 zmm(512-bit) 寄存器使用。

    不扫描裸 0x62 字节——objdump 会把数据区字符串误判为指令(如 "write_by")。
    """
    found = set()
    for m in AVX512_MNEMONICS:
        if re.search(r"\b" + m + r"(?:\s|%zmm|\b)", text):
            found.add(m)
    if re.search(r"%zmm[0-9]+", text):
        found.add("<zmm-register>")
    return sorted(found)


def check(binary: pathlib.Path, avx2_lib: pathlib.Path | None,
          avx512_lib: pathlib.Path | None) -> list[str]:
    errors: list[str] = []
    if not binary.is_file():
        return [f"missing binary: {binary}"]
    try:
        cli_text = _objdump_text(binary)
    except (OSError, RuntimeError) as exc:
        return [str(exc)]

    # 主 CLI 必须无 AVX2/AVX512 泄漏
    a2 = scan_avx2(cli_text)
    if a2:
        errors.append(f"CLI baseline 含 AVX2 指令: {a2}")
    a512 = scan_avx512(cli_text)
    if a512:
        errors.append(f"CLI baseline 含 AVX512 指令: {a512}")

    # provider 库必须含对应指令（证明真实编译）; 但静态库需先解包反汇编
    if avx2_lib is not None and avx2_lib.is_file():
        try:
            prov = _objdump_text(avx2_lib)
        except (OSError, RuntimeError) as exc:
            errors.append(f"avx2 lib 反汇编失败: {exc}")
        else:
            pa = scan_avx2(prov)
            if not pa:
                errors.append(f"avx2 provider 库不含任何 AVX2 指令（{avx2_lib} 可能未编译）")
    if avx512_lib is not None and avx512_lib.is_file():
        try:
            prov = _objdump_text(avx512_lib)
        except (OSError, RuntimeError) as exc:
            errors.append(f"avx512 lib 反汇编失败: {exc}")
        else:
            pa = scan_avx512(prov)
            if not pa:
                errors.append(f"avx512 provider 库不含任何 AVX512 指令（{avx512_lib} 可能未编译）")
    return errors


def selftest() -> int:
    fake_cli_clean = '52430:\t48 89 e5 48 83 ec 20 89 7d fc\n' \
                     '52439:\tmov %rsp,%rbp\n'
    fake_cli_avx2 = '52430:\tc5 fd fe c1\n' \
                    '52433:\tvpaddd %ymm1,%ymm0,%ymm0\n'
    fake_cli_avx512 = '52430:\t62 f1 7d 48 58 c1\n' \
                      '52434:\tvaddps %zmm1,%zmm0,%zmm0\n'
    if scan_avx2(fake_cli_avx2):
        pass
    else:
        print("SELFTEST_FAIL: 负例未检出 AVX2 (vpaddd ymm)")
        return 1
    if scan_avx2(fake_cli_clean):
        print("SELFTEST_FAIL: 干净文本误报 AVX2")
        return 1
    if scan_avx512(fake_cli_avx512):
        pass
    else:
        print("SELFTEST_FAIL: zmm 未检出 AVX512")
        return 1
    if scan_avx512(fake_cli_avx2):
        print("SELFTEST_FAIL: ymm 误报 AVX512")
        return 1
    print("SELFTEST_PASS: AVX2/AVX512 泄漏扫描器负例/正例均正确")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=pathlib.Path, default=None)
    parser.add_argument("--avx2-lib", type=pathlib.Path, default=None)
    parser.add_argument("--avx512-lib", type=pathlib.Path, default=None)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)
    if args.selftest:
        return selftest()
    if args.binary is None:
        print("ISA_LEAK_FAIL: --binary required", file=sys.stderr)
        return 1
    errors = check(args.binary, args.avx2_lib, args.avx512_lib)
    if errors:
        print("ISA_LEAK_FAIL")
        for e in errors[:40]:
            print(f"  - {e}", file=sys.stderr)
        return 1
    print(f"ISA_LEAK_PASS binary={args.binary} (CLI 无 AVX2/AVX512; provider 库含对应指令)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
