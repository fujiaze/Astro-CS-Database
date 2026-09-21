#!/usr/bin/env python3
"""TST-001 示例 unit 测试：手写期望 4（PRE_FROZEN_VALUES，非生产函数生成）。
用法: python3 check_constant.py <n>  # 断言 2+2 == n（独立手写期望）
"""
import sys

EXPECT = 4  # 手写期望：与任何生产实现无关


def main() -> int:
    n = int(sys.argv[1]) if len(sys.argv) > 1 else EXPECT
    got = 2 + 2
    if got != n:
        print(f"UNIT_FAIL expect={n} got={got}")
        return 1
    print(f"UNIT_PASS expect={n} got={got}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
