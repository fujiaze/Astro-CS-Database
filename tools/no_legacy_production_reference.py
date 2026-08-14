#!/usr/bin/env python3
# tools/no_legacy_production_reference.py — V17 legacy 多路径静态 gate
#
# 检查 active production tree（lib/ 非 archive、非 acr、非 tests）不得：
#   - include/引用 healpix_stack / hp_stack_*（legacy Stage2 科学实现）；
#   - 加载 GRADIENT_SPHERE/STACK 模块；
#   - 引用 legacy Stage2 handler（run_stage_gradient_sphere 等已 stub，
#     允许存在但不允许有 healpix_stack 调用体）。
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BANNED = ("hp_stack_api", "hp_stack_gradient_corrected(", "hp_stack_",
          "healpix_stack", "run_stage_gradient_sphere(",
          "run_stage_stack(", "load_module(ModuleId::GRADIENT_SPHERE",
          "load_module(ModuleId::STACK", "healpix_stack.dll")


def strip_comments(txt):
    """去掉 // 行注释与 /* */ 块注释，避免枚举元数据/说明误报。"""
    import re
    txt = re.sub(r"/\*.*?\*/", "", txt, flags=re.S)
    return "\n".join(l for l in txt.splitlines()
                     if not l.lstrip().startswith("//"))


def scan():
    bad = []
    for dp, dns, fns in os.walk(ROOT / "lib"):
        dns[:] = [d for d in dns
                  if d not in ("archive", "build", "build2", "third_party",
                               "node_modules", ".git", "__pycache__",
                               "_deps", "astrometry.net")]
        for fn in fns:
            if not fn.endswith((".cpp", ".h", ".hpp", ".cu")):
                continue
            p = Path(dp) / fn
            rel = str(p.relative_to(ROOT)).replace("\\", "/")
            if "/tests/" in rel or "/acr/" in rel:
                continue
            try:
                txt = p.read_text(encoding="utf-8", errors="ignore")
            except Exception:
                continue
            txt = strip_comments(txt)
            for pat in BANNED:
                if pat in txt:
                    bad.append((rel, pat))
    return bad


def main():
    bad = scan()
    if bad:
        for rel, pat in bad:
            print(f"LEGACY_REF: {rel} -> {pat}")
        print("NO_LEGACY_PRODUCTION_REFERENCE=FAIL")
        return 1
    print("NO_LEGACY_PRODUCTION_REFERENCE=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
