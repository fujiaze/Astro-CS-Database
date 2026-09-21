#!/usr/bin/env python3
"""TST-001 示例 oracle 测试：独立解析解期望 sum(1..N)=N(N+1)/2。
禁止用任何生产求和实现生成期望；这里手写闭式解 + 朴素循环双重独立互证。
"""
import sys


def independent_closed_form(n: int) -> int:
    return n * (n + 1) // 2  # 解析解（独立 oracle）


def naive_loop(n: int) -> int:
    total = 0
    for i in range(1, n + 1):
        total += i
    return total  # 朴素参考（与解析解互证，非生产代码）


def main() -> int:
    n = 10000
    expected = independent_closed_form(n)
    got = naive_loop(n)
    if got != expected:
        print(f"ORACLE_FAIL n={n} closed_form={expected} naive={got}")
        return 1
    print(f"ORACLE_PASS sum(1..{n})={got}（解析解独立期望）")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
