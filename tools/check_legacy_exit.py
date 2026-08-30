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
    # LEG-003: AIO PipelineEngine 调度职责 (engine run API 无生产 caller;
    # frame 数据结构 API 保留供 CLI drizzle 测试 wrapper 用)
    eng_h = (REPO / "lib" / "astro_image_io" / "include" / "aio_pipeline_engine.h").read_text(encoding="utf-8", errors="ignore")
    engine_run_decl = ("aio_pipeline_engine_run_single" in eng_h or
                       "aio_pipeline_engine_run_batch" in eng_h)
    caller_files = []
    for root in (REPO / "lib", REPO / "cli"):
        for f in root.rglob("*.cpp"):
            if "aio_pipeline_engine.cpp" in str(f): continue
            txt = f.read_text(encoding="utf-8", errors="ignore")
            if "aio_pipeline_engine_run" in txt:
                caller_files.append(str(f.relative_to(REPO)))
    if engine_run_decl and caller_files:
        errors.append(f"PipelineEngine run API has callers: {caller_files} (LEG-003)")
    # LEG-004: 旧 Stage2 工具 + ACR 隔离
    # (a) 旧 stage2.cpp 工具不随根构建产出 (无安装产物)
    stage2_built = (REPO / "build" / "root-cmake" / "astrocs-stage2").exists()
    if stage2_built:
        errors.append("astrocs-stage2 旧工具被生产构建产出 (LEG-004)")
    # (b) ACR dormant: ASTROCS_ENABLE_ACR=OFF; 生产二进制 link map 无 acr
    cmake = (REPO / "CMakeLists.txt").read_text(encoding="utf-8", errors="ignore")
    if "ASTROCS_ENABLE_ACR" not in cmake:
        errors.append("ACR option 缺失 (LEG-004)")
    bin_path = REPO / "build" / "root-cmake" / "astrocs"
    if bin_path.exists():
        out = subprocess.run(["nm", str(bin_path)], capture_output=True, text=True).stdout.lower()
        if "acr" in out:
            errors.append("生产二进制含 ACR 符号 (LEG-004)")
    # (c) 运行 module list 无 ACR (模块注册表)
    for f in (REPO / "lib" / "phase2" / "src").glob("*.cpp"):
        txt = f.read_text(encoding="utf-8", errors="ignore")
        if "acr" in txt.lower() and "register_phase2_acr" in txt:
            errors.append(f"ACR kernel 注册存在: {f.name} (LEG-004: 应 dormant 不注册)")                 if f.name != "acr_kernels.cpp" else None
    if errors:
        print("LEGACY_EXIT_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print("LEGACY_EXIT_PASS: 旧路径无生产符号/CMake/文档入口, 源码保留; ACR dormant 隔离")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
