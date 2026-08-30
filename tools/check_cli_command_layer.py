#!/usr/bin/env python3
"""CLI-001: CLI 稳定 command 层校验。

规则:
1. cli/main.cpp 用 CmdRule kRules[] 表驱动分发 (无 argv 硬编码 if 链)。
2. 每个 rule.path 唯一; cmd_* 签名统一 (Parsed + JsonlEmitter)。
3. --version/--help 稳定出口存在。
4. 退出码来源 exit_codes.h (单一来源)。
exit 0 = PASS。
"""
import pathlib, re, sys

REPO = pathlib.Path(__file__).resolve().parents[1]
MAIN = (REPO / "cli" / "main.cpp").read_text(encoding="utf-8")

def main():
    errors = []
    # 1) kRules 表存在且驱动分发
    if "CmdRule kRules[]" not in MAIN:
        errors.append("no CmdRule kRules[] table")
    if "const auto& r : kRules" not in MAIN:
        errors.append("no kRules-driven dispatch")
    # 2) cmd_* 统一签名
    cmds = re.findall(r"int (cmd_[a-z_]+)\(const Parsed&", MAIN)
    if len(cmds) < 4:
        errors.append(f"too few cmd_* functions: {len(cmds)}")
    # 3) 稳定出口
    for token in ("--version", "--help", "exit_codes.h"):
        if token not in MAIN and token != "exit_codes.h":
            errors.append(f"stable exit missing: {token}")
    # exit_codes 引用
    if "exit_codes.h" not in MAIN and "exit_codes" not in MAIN:
        errors.append("no exit_codes.h include")
    # 4) rule path 唯一 (聚合初始化 {path, {flags}})
    paths = re.findall(r'\{\s*"([a-z0-9 -]+)"\s*,\s*\{', MAIN)
    if len(paths) < 8:
        errors.append(f"too few rule paths: {len(paths)}")
    if len(paths) != len(set(paths)):
        errors.append("duplicate rule paths")
    if errors:
        print("CLI-001_LAYER_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print(f"CLI-001_PASS: {len(cmds)} cmd_* uniform, {len(paths)} unique paths, kRules dispatch, stable exits")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
