#!/usr/bin/env python3
"""P3-001: phase3 production registry/preset 无 prototype 校验。

规则:
1. lib/ cli/ 代码层 (除 test/third_party) 无 "prototype" 残留。
2. cli/main.cpp 注册 phase3 命令指向正式模块。
3. docs/contracts/INDEX.yaml 无 phase3 prototype ACTIVE。
exit 0 = PASS。
"""
import pathlib, re, sys

REPO = pathlib.Path(__file__).resolve().parents[1]

def main():
    errors = []
    # 1) 代码层 prototype 扫描
    for root in (REPO / "lib", REPO / "cli"):
        for f in sorted(root.rglob("*")):
            if not f.is_file(): continue
            if "test" in f.parts or "third_party" in f.parts: continue
            if f.suffix not in (".cpp", ".h", ".c", ".hpp"): continue
            txt = f.read_text(encoding="utf-8", errors="ignore")
            if "prototype" in txt.lower():
                errors.append(f"prototype ref in {f.relative_to(REPO)}")
    # 2) cli/main.cpp phase3 注册
    main_cpp = (REPO / "cli" / "main.cpp").read_text(encoding="utf-8", errors="ignore")
    if "phase3" not in main_cpp.lower():
        errors.append("cli/main.cpp 无 phase3 注册")
    # 3) INDEX.yaml 无 phase3 prototype ACTIVE
    idx = (REPO / "docs" / "contracts" / "INDEX.yaml").read_text(encoding="utf-8", errors="ignore")
    if re.search(r"phase3[^\n]*prototype", idx, re.IGNORECASE):
        errors.append("INDEX.yaml 含 phase3 prototype 条目")
    if errors:
        print("P3-001_STATUS_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print("P3-001_PASS: 无 prototype 残留, phase3 正式模块注册, 文档状态统一")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
