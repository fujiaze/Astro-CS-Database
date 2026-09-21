#!/usr/bin/env python3
"""TST-001 示例 negative 测试：坏输入（负数/非整数）必须被拒绝。
期望=拒绝行为（exit 非 0 由脚本断言并报告），无容差概念。
"""
import subprocess
import sys


def parse_positive_int(raw: str) -> int:
    """被测迷你逻辑（示例）：拒绝负数/非整数。"""
    v = int(raw)
    if v <= 0:
        raise ValueError("must be positive")
    return v


def main() -> int:
    # 子进程式负测：坏输入 → 期望非 0 退出（harness 会失败=正确拒绝）
    for bad in ("-5", "abc", "3.14"):
        r = subprocess.run([sys.executable, __file__, "--child", bad],
                           capture_output=True, text=True)
        if r.returncode == 0:
            print(f"NEGATIVE_FAIL 坏输入 {bad!r} 未被拒绝（returncode=0）")
            return 1
    print("NEGATIVE_PASS 坏输入 -5/abc/3.14 全部被拒绝")
    return 0


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[1] == "--child":
        try:
            parse_positive_int(sys.argv[2])
        except Exception:
            raise SystemExit(1)  # 坏输入拒绝
        raise SystemExit(0)
    raise SystemExit(main())
