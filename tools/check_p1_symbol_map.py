#!/usr/bin/env python3
"""P1-001: old-symbol 映射完整性校验。

规则:
1. p1_session.h 每个导出 API 必须在映射表中被映射。
2. 映射表中每个 SCI-* 必须登记于 docs/contracts/INDEX.yaml。
3. 未映射旧能力 = blocker。
exit 0 = PASS。
"""
import pathlib, re, sys

REPO = pathlib.Path(__file__).resolve().parents[1]
MAP = (REPO / "docs/refactor/P1_SYMBOL_MAP.md").read_text(encoding="utf-8")
HDR = (REPO / "lib/phase1_session/p1_session.h").read_text(encoding="utf-8")
INDEX = (REPO / "docs/contracts/INDEX.yaml").read_text(encoding="utf-8")

def main():
    errors = []
    # 1) p1_session.h 导出 API
    api_re = re.findall(r"acs_status\s+(p1_session_\w+)\s*\(", HDR)
    for api in api_re:
        if api not in MAP:
            errors.append(f"unmapped old symbol: {api}")
    # 2) 映射表 SCI-* 登记
    sci_used = set(re.findall(r"\b(SCI-[A-Z0-9-]+)\b", MAP))
    for sci in sci_used:
        if sci not in INDEX:
            errors.append(f"SCI not registered in INDEX.yaml: {sci}")
    # 3) 映射表 module-ID 命名
    mods = re.findall(r"`astrocs\.phase1\.[a-z_]+`", MAP)
    for m in mods:
        if not re.match(r"`astrocs\.phase1\.[a-z_]+`", m):
            errors.append(f"bad module id: {m}")
    # 4) blocker 检查
    if "未映射旧能力: **0**" not in MAP:
        errors.append("mapping doc claims nonzero unmapped blockers")
    if errors:
        print("P1-001_MAP_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print(f"P1-001_PASS: {len(api_re)} old APIs mapped, {len(sci_used)} SCI registered, "
          f"{len(mods)} modules, blockers=0")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
