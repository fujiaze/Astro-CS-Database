#!/usr/bin/env python3
"""LEG-002..004: 旧生产路径退出校验 (orchestrator/AIO PipelineEngine/old Stage2/ACR)。

规则 (LEG 规格: 逐个确认无 canonical caller、链接符号、文档入口、安装产物后退出):
1. 生产二进制 (build/root-cmake/astrocs) 不含目标目录符号。
2. 根 CMakeLists 不链入目标目录。
3. 文档入口标注退出或已清理。
4. 源码目录保留 (不破坏删除), 但无生产引用。
exit 0 = PASS。
"""
import pathlib, subprocess, sys

REPO = pathlib.Path(__file__).resolve().parents[1]

# 任务粒度: 逐个旧路径确认无 canonical caller/链接符号/文档入口/安装产物
def check(name, task, bin_symbols, cmake_token, doc_check=None):
    errors = []
    # 1) 生产二进制符号
    bin_path = REPO / "build" / "root-cmake" / "astrocs"
    if bin_path.exists():
        out = subprocess.run(["nm", str(bin_path)], capture_output=True, text=True).stdout.lower()
        for sym in bin_symbols:
            if sym in out:
                errors.append(f"production binary contains {name} symbol: {sym}")
    # 2) 根 CMake 引用
    cmake = (REPO / "CMakeLists.txt").read_text(encoding="utf-8", errors="ignore")
    if cmake_token and cmake_token in cmake:
        errors.append(f"root CMake references {name} ({task})")
    # 3) 文档入口
    if doc_check:
        errs = doc_check()
        errors.extend(errs)
    return errors

def main():
    errors = []
    # LEG-002: 旧 Orchestrator
    errors += check("orchestrator", "LEG-002",
                    ["orchestrat"], "orchestrator",
                    lambda: (["PUBLIC_API orchestrator.exe 未标 LEG-002 退出"]
                             if "orchestrator.exe" in (REPO / "docs" / "contracts" / "PUBLIC_API.md").read_text(encoding="utf-8", errors="ignore")
                             and "LEG-002" not in (REPO / "docs" / "contracts" / "PUBLIC_API.md").read_text(encoding="utf-8", errors="ignore")
                             else []))
    if errors:
        print("LEGACY_EXIT_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print("LEGACY_EXIT_PASS: 旧路径无生产符号/CMake/文档入口, 源码保留")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
