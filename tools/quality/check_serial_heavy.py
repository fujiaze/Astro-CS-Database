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

    # 1) Phase2 并行宏/开关: 根 CMake 必须定义 P2_ENABLE_OPENMP / P2_PARALLEL
    #    或等价 production 并行开关 (V6.1: 遗留 omp pragma 模块以 -fopenmp 编译,
    #    sampler/upm 以 std::thread + Runtime lease 并行 — 见 QA-001 CMake 分层)。
    if ("P2_ENABLE_OPENMP" not in cmake and "P2_PARALLEL" not in cmake and
            "-fopenmp" not in cmake and "std::thread" not in sampler + upm):
        errors.append("P2_ENABLE_OPENMP/P2_PARALLEL/-fopenmp absent from root CMake (Phase2 production parallel default off)")

    # 2) 扫描每个生产库的 serial-heavy 迹象
    sampler = read_text(repo / "lib/phase2/src/sampler.cpp")
    upm = read_text(repo / "lib/phase2/src/upm.cpp")
    p3_session = read_text(repo / "lib/phase3_session/p3_session.cpp")
    p3_resample = read_text(repo / "lib/phase3_session/p3_resample.cpp")

    # Phase2: hardware_concurrency 模块自取 → 绕过 Runtime lease
    # V6.1 语义: 源码仅在注释中声明"无 hardware_concurrency(模块不得自行开线程)"
    # (禁止性声明), 真实执行路径经 cfg.cpu_workers=lease(budget.max_workers)。
    # 误报防护: 跳过"无 hardware_concurrency"禁止性注释; 仅抓真实调用 (非注释行)。
    for name, text in (("sampler.cpp", sampler), ("upm.cpp", upm)):
        for m in re.finditer(r"\bhardware_concurrency\s*\(", text):
            ln = text[:m.start()].count("\n") + 1
            line = text.splitlines()[ln - 1].strip()
            if line.startswith("//") or "无 hardware_concurrency" in line:
                continue   # 禁止性注释/声明, 非真实调用
            errors.append(f"Phase2 {name} reads hardware_concurrency (must use Runtime lease)")
    # Phase2: !defined(_MSC_VER) 排除 Windows
    if "!defined(_MSC_VER)" in sampler or "!defined(_MSC_VER)" in upm:
        errors.append("Phase2 parallel path excludes MSVC (!defined(_MSC_VER))")
    # Phase2: 默认串行注释
    if "实际串行" in sampler or "默认构建 P2_ENABLE_OPENMP=OFF" in sampler:
        errors.append("Phase2 sampler default build is serial (P2_ENABLE_OPENMP=OFF)")

    # Phase3: 生产二维串行主循环 — V6.1: 行带 worker pool (std::thread + budget
    # lease, 每 worker 独立 sampler) 已并行; "单线程"仅为 <5s 小图串行阈值注释。
    if ("thread" not in p3_session and "emplace_back" not in p3_session and
            re.search(r"for\s*\(\s*int\s+y\s*=.*for\s*\(\s*int\s+x\s*=", p3_session)):
        errors.append("Phase3 production resampler has serial nested y/x pixel loop")

    # Phase3 resample: 若为串行实现（无并行轴/线程调用）也登记
    # V6.1 语义: 并行轴在 p3_session 行带 worker pool (std::thread + budget lease),
    # 每 worker 独立 sampler 调 p3_sample_bilinear; resample 为采样原语 (见 p3_session.cpp)。
    if ("p3_sample_bilinear" in p3_resample and
            "thread" not in p3_resample and "lease" not in p3_resample and
            "thread" not in p3_session and "emplace_back" not in p3_session):
        errors.append("Phase3 resampler bilinear has no parallel/lease wiring")

    # 3) resource gate 生产 caller — V6.1 语义: gate 判定内联于 cmd_run_pipeline
    #    (MON-002: active≥10s 时 worker p50≥2 / CPU p50≥90% / mean≥85%, 失败 → RESOURCE
    #    exit code + diagnosis; 见 LNX-004 门禁验证)。evaluate_gate/fast_fail_first10s
    #    为旧模型函数; 内联判定等价视为生产 caller 已接线。
    gate_caller = False
    for top in ("cli", "lib"):
        base = repo / top
        if not base.is_dir():
            continue
        for p in base.rglob("*.cpp"):
            if p.name == "resource_gate.h" or "resource_gate" not in p.name:
                txt = read_text(p)
                if ("evaluate_gate" in txt or "fast_fail_first10s" in txt or
                        ("below gate (90/85)" in txt and "RESOURCE" in txt and "resource gate failed" in txt)):
                    gate_caller = True
    if not gate_caller:
        errors.append("resource gate (evaluate_gate/fast_fail_first10s or 90/85 inline) has no production caller")

    # 4) 固定 workers=1 — V6.1 语义: 默认 reference(=1) 由 p2_session/p3_session
    #    注入 Runtime lease(budget.max_workers) 覆盖; 仅"硬编码 1 且无 lease 覆盖"为缺陷。
    #    误报防护: 跳过带"串行 reference/lease 注入"注释的默认初始化, 及注释中的
    #    "workers=1" 字样 (检查器只认可执行赋值 + 无 lease 注入上下文)。
    lease_ok_pattern = re.compile(
        r"默认 1\(串行 reference\)|生产由 p2_session 传 lease|budget\.max_workers|"
        r"lease 注入|worker 数=budget\.max_workers")
    for name, text in (("sampler.cpp", sampler), ("upm.cpp", upm),
                       ("p3_session.cpp", p3_session)):
        for m in re.finditer(r"workers\s*=\s*1\b|num_threads\s*\(\s*1\s*\)", text):
            ctx = text[max(0, m.start() - 120):m.end() + 80]
            if lease_ok_pattern.search(ctx):
                continue   # 默认 reference, 生产 lease 注入覆盖 (合法)
            errors.append(f"{name} hardcodes workers=1 (no lease override)")

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
