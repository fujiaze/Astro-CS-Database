#!/usr/bin/env python3
"""TST-001 示例 fixtures 测试：fixture 文件 SHA-256 与预冻结 hash 比对。
期望来自 fixture 自身内容 hash（FIXTURE_HASH），非生产输出。
用法: python3 fixture_hash.py <repo_root>
"""
import hashlib
import pathlib
import sys

PRE_FROZEN = "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"  # sha256("test")


def main() -> int:
    root = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(".")
    p = root / "tests/testkit/fixtures/demo_fixture.txt"
    if not p.is_file():
        print(f"FIXTURE_FAIL 缺少 {p}")
        return 1
    digest = hashlib.sha256(p.read_bytes()).hexdigest()
    if digest != PRE_FROZEN:
        print(f"FIXTURE_FAIL sha256={digest} 期望 {PRE_FROZEN}（fixture 被篡改？）")
        return 1
    print(f"FIXTURE_PASS {p} sha256={digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
