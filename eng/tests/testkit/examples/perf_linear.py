#!/usr/bin/env python3
"""TST-001 示例 performance 测试：线性复杂度弱上界（防 flaky）。
双倍输入耗时应 < 5× 单倍（宽松性质断言，不比较绝对时间）。
seed=42 固定；provider=baseline；workers=1。
"""
import random
import time


def linear_scan(data: list) -> int:
    total = 0
    for x in data:
        total += x
    return total


def main() -> int:
    rng = random.Random(42)
    small = [rng.random() for _ in range(50_000)]
    large = [rng.random() for _ in range(100_000)]
    t0 = time.perf_counter(); linear_scan(small); t_small = time.perf_counter() - t0
    t0 = time.perf_counter(); linear_scan(large); t_large = time.perf_counter() - t0
    if t_large > 5.0 * max(t_small, 1e-6) + 0.05:
        print(f"PERF_FAIL 双倍输入耗时 {t_large:.4f}s 超 5× 单倍 {t_small:.4f}s")
        return 1
    print(f"PERF_PASS linear scan 50k={t_small:.4f}s 100k={t_large:.4f}s（弱上界）")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
