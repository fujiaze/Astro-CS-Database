#!/usr/bin/env python3
"""TST-001 示例 properties 测试：round 幂等不变量（seed 固定 20260902）。
不变量：round(round(x)) == round(x)，任何 x。属性自证，不依赖生产实现。
"""
import random


def main() -> int:
    rng = random.Random(20260902)  # 固定 seed：可复现
    for _ in range(2000):
        x = rng.uniform(-1e6, 1e6)
        once = round(x)
        if round(once) != once:
            print(f"PROPERTY_FAIL x={x} round(round(x))={round(once)} != round(x)={once}")
            return 1
    print("PROPERTY_PASS round 幂等 2000 样本（seed=20260902）")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
