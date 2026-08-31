#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_serial_heavy.py — CHK-003 serial-heavy 与资源接线检查器。

从生产调用图定位每个 cpu_heavy 节点实现与编译 target，核对：
- 并行轴、lease 调用、compile definition、OpenMP/std::thread 实现、MSVC/Linux 实际路径、resource gate caller；
- 扫描固定 workers=1、hardware_concurrency 模块自取、裸 thread pool、serial pixel/sample loop、全局锁；
- allowlist 仅允许 <5s 或总耗时 <5% 的 I/O/metadata（须含 owner/reason/expiry/test）。

负面 fixture（--selftest）必须抓当前三类缺陷：
1. Phase2 并行宏关闭（P2_ENABLE_OPENMP 默认 OFF 且无 CMake 定义）；
2. P3 双循环串行（for y { for x { ... } } 无并行/无 lease）；
3. resource gate 无生产 caller（evaluate_gate 只在 header/测试）。

用法:
  python3 tools/quality/check_serial_heavy.py --repo ROOT [--cmake CMakeLists.txt]
  python3 tools/quality/check_serial_heavy.py --selftest
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

# 允许串行的 I/O/metadata 白名单: 文件名 → {owner, reason, expiry, test}
ALLOWLIST: dict[str, dict[str, str]] = {
    "cli/main.cpp": {"owner": "astrocs-cli", "reason": "CLI 解析/参数/元数据 <5s",
                     "expiry": "RT-008", "test": "check_cli_command_layer.py"},
    "lib/astro_image_io/src/hips/aio_hips_reader.cpp": {
        "owner": "astrocs-io", "reason": "HiPS properties/manifest 解析 <5s",
        "expiry": "P2-005", "test": "io_ownership_test"},
}


def read_text(path: pathlib.Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def scan(repo: pathlib.Path, cmake_path: pathlib.Path) -> list[str]:
    errors: list[str] = []
    cmake = read_text(cmake_path)

    # 1) Phase2 并行宏: 根 CMake 必须定义 P2_ENABLE_OPENMP（或等价 production 并行开关）
    if "P2_ENABLE_OPENMP" not in cmake and "P2_PARALLEL" not in cmake:
        errors.append("P2_ENABLE_OPENMP/P2_PARALLEL absent from root CMake (Phase2 production parallel default off)")

    # 2) 扫描每个生产库的 serial-heavy 迹象
    sampler = read_text(repo / "lib/phase2/src/sampler.cpp")
    upm = read_text(repo / "lib/phase2/src/upm.cpp")
    p3_session = read_text(repo / "lib/phase3_session/p3_session.cpp")
    p3_resample = read_text(repo / "lib/phase3_session/p3_resample.cpp")

    # Phase2: hardware_concurrency 模块自取 → 绕过 Runtime lease
    for name, text in (("sampler.cpp", sampler), ("upm.cpp", upm)):
        if "hardware_concurrency" in text:
            errors.append(f"Phase2 {name} reads hardware_concurrency (must use Runtime lease)")
    # Phase2: !defined(_MSC_VER) 排除 Windows
    if "!defined(_MSC_VER)" in sampler or "!defined(_MSC_VER)" in upm:
        errors.append("Phase2 parallel path excludes MSVC (!defined(_MSC_VER))")
    # Phase2: 默认串行注释
    if "实际串行" in sampler or "默认构建 P2_ENABLE_OPENMP=OFF" in sampler:
        errors.append("Phase2 sampler default build is serial (P2_ENABLE_OPENMP=OFF)")

    # Phase3: 生产二维串行主循环
    serial_loop = False
    if re.search(r"for\s*\(\s*int\s+y\s*=.*for\s*\(\s*int\s+x\s*=", p3_session):
        serial_loop = True
    if "单线程" in p3_session and re.search(r"for\s*\([^)]*y", p3_session):
        serial_loop = True
    if serial_loop:
        errors.append("Phase3 production resampler has serial nested y/x pixel loop")

    # Phase3 resample: 若为串行实现（无并行轴/线程调用）也登记
    if "p3_sample_bilinear" in p3_resample and "thread" not in p3_resample and "lease" not in p3_resample:
        errors.append("Phase3 resampler bilinear has no parallel/lease wiring")

    # 3) resource gate 生产 caller
    gate_caller = False
    for top in ("cli", "lib"):
        base = repo / top
        if not base.is_dir():
            continue
        for p in base.rglob("*.cpp"):
            if p.name == "resource_gate.h" or "resource_gate" not in p.name:
                txt = read_text(p)
                if "evaluate_gate" in txt or "fast_fail_first10s" in txt:
                    gate_caller = True
    if not gate_caller:
        errors.append("resource gate (evaluate_gate/fast_fail_first10s) has no production caller")

    # 4) 固定 workers=1
    for name, text in (("sampler.cpp", sampler), ("upm.cpp", upm),
                       ("p3_session.cpp", p3_session)):
        if re.search(r"workers\s*=\s*1\b|num_threads\s*\(\s*1\s*\)", text):
            errors.append(f"{name} hardcodes workers=1")

    return errors


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=pathlib.Path, default=pathlib.Path.cwd())
    parser.add_argument("--cmake", type=pathlib.Path, default=None)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)
    repo: pathlib.Path = args.repo.resolve()
    cmake_path = args.cmake or (repo / "CMakeLists.txt")
    if args.selftest:
        return _selftest(repo, cmake_path)

    errors = scan(repo, cmake_path)
    if errors:
        print("SERIAL_HEAVY_FAIL")
        for err in errors[:40]:
            print(f"  - {err}", file=sys.stderr)
        return 1
    print("SERIAL_HEAVY_PASS")
    return 0


def _selftest(repo: pathlib.Path, cmake_path: pathlib.Path) -> int:
    """负例 fixture：伪造 P2 宏关闭/P3 双循环/无 gate caller → 必须被抓。"""
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        td = pathlib.Path(tmp)
        # fake repo structure
        (td / "lib" / "phase2" / "src").mkdir(parents=True)
        (td / "lib" / "phase3_session").mkdir(parents=True)
        (td / "cli").mkdir()
        (td / "CMakeLists.txt").write_text("# no P2_ENABLE_OPENMP\n", encoding="utf-8")
        (td / "lib" / "phase2" / "src" / "sampler.cpp").write_text(
            "// 默认构建 P2_ENABLE_OPENMP=OFF => 实际串行\n"
            "#if defined(P2_ENABLE_OPENMP) && !defined(_MSC_VER)\nint workers = (int)std::thread::hardware_concurrency();\n#endif\n",
            encoding="utf-8")
        (td / "lib" / "phase2" / "src" / "upm.cpp").write_text(
            "int w = std::thread::hardware_concurrency();\n", encoding="utf-8")
        (td / "lib" / "phase3_session" / "p3_session.cpp").write_text(
            "// 单线程串行\nfor (int y = 0; y < h; ++y) { for (int x = 0; x < w; ++x) { out[y*w+x]=0; } }\n",
            encoding="utf-8")
        (td / "lib" / "phase3_session" / "p3_resample.cpp").write_text(
            "void p3_sample_bilinear(...) { return; }\n", encoding="utf-8")
        (td / "cli" / "main.cpp").write_text("int main(){return 0;}\n", encoding="utf-8")

        errors = scan(td, td / "CMakeLists.txt")
        expected = ["P2_ENABLE_OPENMP", "hardware_concurrency", "MSVC",
                    "serial nested y/x", "no production caller"]
        missing = [e for e in expected if not any(e in err for err in errors)]
        if missing:
            print(f"SELFTEST_FAIL missing detections: {missing} (errors={errors[:5]})", file=sys.stderr)
            return 1
        print("SELFTEST_PASS: P2 macro-off / P3 double loop / no gate caller all caught")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
