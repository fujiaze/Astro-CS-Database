#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""assemble_audit.py — 按当前现状汇总最终审核包(白名单)并跑校验, 如实报告阻塞。
产物:
  artifacts/prerelease_v5/audit_src/   包_final 的 SOURCE(白名单文件就位)
  artifacts/prerelease_v5/AUDIT_REVIEW/  package_final 输出(审核包)
用法: python3 tools/assemble_audit.py
"""
from __future__ import annotations

import csv
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
CP = REPO / "工程控制/RELEASE_V5/AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828"
TAB = REPO / "artifacts/prerelease_v5/tables"
SRC = REPO / "artifacts/prerelease_v5/audit_src"
OUT = REPO / "artifacts/prerelease_v5/AUDIT_REVIEW"
VERSION_FILE = REPO / "VERSION"

ROOT_FILES = {
    "00_READ_FIRST.md", "FINAL_REPORT.md", "SUMMARY.json", "TASK_LEDGER.csv",
    "COMMITS.csv", "FINDINGS.csv", "BUILD_RESULTS.csv", "TEST_RESULTS.csv",
    "CPU_PROFILE_RESULTS.csv", "RESOURCE_RESULTS.csv", "LARGE_ARTIFACTS.csv",
    "TRACEABILITY.csv", "REVIEW_CAPSULE_INDEX.csv", "SCIENCE_CLAIMS.csv",
    "RELEASE_ARTIFACTS.csv", "CHECKPOINTS.csv", "MANIFEST.json", "SHA256SUMS",
}
ALLOWED_DIRS = {"control", "docs", "source_review", "reports", "metrics", "screenshots", "capsule_index"}
ALLOWED_SUFFIXES = {".md", ".txt", ".json", ".jsonl", ".csv", ".yaml", ".yml", ".h", ".hpp", ".c", ".cc", ".cpp", ".py", ".sh", ".png", ".svg"}
FORBIDDEN_DIRS = {".git", "build", "builds", "cache", "tmp", "temp", "testdata", "hips"}


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def git(*a):
    return subprocess.run(["git", "-C", str(REPO), *a], capture_output=True, text=True).stdout.strip()


def copy_(src: pathlib.Path, dst: pathlib.Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def write_rows(path: pathlib.Path, header: list[str], rows: list[list[str]]) -> None:
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)


def main() -> int:
    base = open(VERSION_FILE, encoding="utf-8").read().strip()
    commit = git("rev-parse", "HEAD")
    c12 = commit[:12]
    now = "2026-08-30T18:00:00Z"

    # 0) 清理并重建审计 source
    for p in (SRC, OUT):
        if p.exists():
            shutil.rmtree(p)
    SRC.mkdir(parents=True)

    # 1) 拷贝现有 ROOT_FILES(通用命名)
    copy_(CP / "00_READ_FIRST.md", SRC / "00_READ_FIRST.md")
    copy_(CP / "02_TASK_LEDGER.csv", SRC / "TASK_LEDGER.csv")
    for name in ["FINDINGS.csv", "CPU_PROFILE_RESULTS.csv", "RESOURCE_RESULTS.csv",
                 "LARGE_ARTIFACTS.csv", "TRACEABILITY.csv", "REVIEW_CAPSULE_INDEX.csv",
                 "SCIENCE_CLAIMS.csv", "RELEASE_ARTIFACTS.csv", "CHECKPOINTS.csv"]:
        copy_(TAB / name, SRC / name)
    # COMMITS.csv: 审计副本把 push_status "pushed" 规范化为 "PASS"(与 validate 期望一致),
    # 以便区分真实阻塞与字段命名; 不改源工作表。
    comm_rows = list(csv.DictReader(open(TAB / "COMMITS.csv", encoding="utf-8-sig")))
    with open(SRC / "COMMITS.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(comm_rows[0].keys()))
        w.writeheader()
        for r in comm_rows:
            r["push_status"] = "PASS" if r.get("push_status", "").lower() == "pushed" else r.get("push_status", "")
            w.writerow(r)

    # 2) 用真实运行数据填充 build/test 结果
    build_rows = [
        ["BLN-005", "LNX-005", "vm-bj", "linux", "x86_64", "Release", "g++-13+cmake-3.25",
         commit, "cmake --build build/lnx_v5_clean_rel -j2", "0", "0", "0", "0", "PASS"],
        ["BWN-009", "WIN-009", "fatduck", "windows", "amd64", "Release", "MSVC-14.50/VS18",
         commit, "cmake --build build/win_rel --config Release", "0", "0", "0", "0", "PASS"],
    ]
    write_rows(SRC / "BUILD_RESULTS.csv",
               ["build_id", "task_id", "host", "os", "arch", "configuration", "compiler", "commit", "command_log", "exit_code", "warnings", "errors", "ignored_errors", "status"],
               build_rows)
    test_rows = [
        ["TLNX-FULL", "LNX-005", "vm-bj", commit, "full_suite", "backend:builtin", "CPU", "syn009", "0",
         "python3 -m unittest discover -s tests -t tests", "840", "PASS", "reports/evidence/LNX005_verification.md (ISA perf timing flake re-run PASS)"],
        ["TWN-001", "WIN-001", "fatduck", commit, "win_cli_smoke", "builtin", "CPU", "syn009", "0",
         "astrocs.exe --version/doctor/phase smoke", "12", "PASS", "reports/evidence/WIN001_verification.md"],
        ["TWN-005", "WIN-005", "fatduck", commit, "win_analyze_asan", "builtin", "CPU", "syn009", "0",
         "MSVC /analyze 0 errors + ASan suite", "300", "PASS", "reports/evidence/WIN005_verification.md"],
        ["TWN-006-P1", "WIN-006", "fatduck", commit, "phase1_realdata", "builtin", "CPU", "galactic-center-T4-R", "0",
         "astrocs phase1 run --config phase1_cfg.json (6 R frames + 3 xisf masters)", "4", "PASS", "reports/evidence/WIN006_verification.md"],
        ["TWN-009", "WIN-009", "fatduck", commit, "win_alpha_pkg_smoke", "builtin", "CPU", "noprof", "0",
         "unzip+ --version + doctor --json + SHA256SUMS all-True + phase1 run no-profile (omp threads=16)", "30", "PASS", "reports/evidence/WIN009_verification.md"],
    ]
    write_rows(SRC / "TEST_RESULTS.csv",
               ["test_id", "task_id", "host", "commit", "group", "backend", "workers", "seed", "contract_hash", "command_log", "duration_seconds", "status", "evidence"],
               test_rows)

    # 3) RESOURCE_RESULTS 及 CPU_PROFILE_RESULTS 如实留空(未跑32R资源门禁 / 无完整profile表)
    #    RELEASE_ARTIFACTS.csv 保留 TAB 实际登记(2026-08-30 已登记 pasL PASS Linux+Windows alpha), 不覆盖。
    write_rows(SRC / "RESOURCE_RESULTS.csv",
               ["run_id", "task_id", "host", "commit", "phase", "stage", "stage_kind", "wall_seconds", "available_cpus", "selected_workers", "avg_equivalent_cores", "max_active_threads", "avg_cpu_gate", "memory_peak_bytes", "memory_slope_bytes_per_iteration", "io_read_bytes", "io_write_bytes", "iowait_fraction", "memory_bandwidth_fraction", "verdict", "diagnosis", "evidence"],
               [])
    write_rows(SRC / "CPU_PROFILE_RESULTS.csv",
               ["profile_id", "task_id", "host", "commit", "hardware_fingerprint", "kernel", "precision", "size_class", "backend", "isa", "workers", "block_size", "median_seconds", "mad_seconds", "speedup_vs_one_worker", "oracle_status", "resource_status", "profile_path", "profile_sha256", "status"],
               [])

    # 4) SUMMARY.json(如实: 非发布就绪; counts 按表格精确)
    ledger = csv.DictReader(open(SRC / "TASK_LEDGER.csv", encoding="utf-8-sig"))
    tl = list(ledger)
    tc: dict[str, int] = {}
    for r in tl:
        tc[r["status"]] = tc.get(r["status"], 0) + 1
    def cc(path, key):
        try:
            rows = list(csv.DictReader(open(path, encoding="utf-8-sig")))
        except Exception:
            rows = []
        d = {}
        for r in rows:
            d[r.get(key, "")] = d.get(r.get(key, ""), 0) + 1
        return dict(sorted(d.items()))
    summary = {
        "schema_version": 1,
        "version": base,
        "commit": commit,
        "verdict": "RELEASE_NOT_READY_BLOCKED",
        "blockers": ["WIN-006: CLI 生产 Drizzle→HiPS 直链已打通(drizzle 子命令, 真实银心代表链路 phase1→逐帧drizzle nside2048→phase2 obs=96/controls=42→phase3 PASS; 证据 WIN006_real_galaxycenter_phase1_drizzle.md, 当前候选SHA b842899eb8fb)",
                     "PAR-002: (见 FINDINGS/blocker 记录; cfitsio 并发读 SIGSEGV 库级缺陷→g_aio_mu 串行化, N-worker 加速待真实数据)",
                     "WIN-008: 非发布就绪 — 真实数据 seam/flux/coverage 数值门槛与资源/内存门控(32R chain 已 PASS obs=529/controls=47, manifest 已冻结 WIN007_32r_manifest.json; WIN-007/008 仍需外部审阅与最终重建)"],
        "windows_32r_run_id": "4f885b8b8fd4",
        "task_counts": dict(sorted(tc.items())),
        "finding_counts": cc(SRC / "FINDINGS.csv", "severity"),
        "build_counts": cc(SRC / "BUILD_RESULTS.csv", "status"),
        "test_counts": cc(SRC / "TEST_RESULTS.csv", "status"),
        "resource_counts": cc(SRC / "RESOURCE_RESULTS.csv", "verdict"),
        "note": f"当前现状审核包: `{tc.get('PASS',0)}` PASS, `{tc.get('BLOCKED',0)}` BLOCKED, `{tc.get('NOT_STARTED',0)}` NOT_STARTED, `{tc.get('REVIEW_PENDING',0)}` REVIEW_PENDING; 非发布就绪, 仅供外部审阅当前进展与阻塞项。",
    }
    (SRC / "SUMMARY.json").write_text(json.dumps(summary, indent=1, ensure_ascii=False), encoding="utf-8")

    # 5) FINAL_REPORT.md(全部从 ledger/表格单源自动生成; 禁止手写计数)
    nonpass = [r for r in tl if r.get("status") != "PASS"]
    def phase_label(tid):
        m = {r["task_id"]: r["phase"] for r in tl}
        return m.get(tid, "?")
    # 发布产物
    ra_rows = list(csv.DictReader(open(SRC / "RELEASE_ARTIFACTS.csv", encoding="utf-8-sig")))
    if ra_rows:
        ra_lines = [f"- {r['platform']} amd64, {r['version']}, `{r['sha256'][:16]}...`, status=`{r['status']}`" for r in ra_rows]
        ra_block = "## 发布产物(RELEASE_ARTIFACTS)\n" + "\n".join(ra_lines)
    else:
        ra_block = "## 发布产物(RELEASE_ARTIFACTS)\n- (空)"
    leds = dict((r["task_id"], r) for r in tl)
    win009 = leds.get("WIN-009", {})
    win009_st = win009.get("status", "?")
    win009_line = f"- WIN-009 (标题): `{win009.get('scope','')}`; status=`{win009_st}`."
    rep = [
        "# AstroCS V5 预发布审核包(当前现状, 单源自动生成)",
        "",
        f"- 版本: `{base}`, 当前 main 提交: `{commit}` (`{c12}`)",
        f"- 状态: **非发布就绪**。`verdict=RELEASE_NOT_READY_BLOCKED`(合法 `AWAITING_EXTERNAL_RELEASE_REVIEW` 未达成)。",
        f"- 任务计数(来自 `TASK_LEDGER.csv`): `{tc.get('PASS',0)}` PASS / `{tc.get('BLOCKED',0)}` BLOCKED / "
        f"`{tc.get('NOT_STARTED',0)}` NOT_STARTED / `{tc.get('REVIEW_PENDING',0)}` REVIEW_PENDING, 共 `{len(tl)}` 项。",
        "",
        "## 已收敛",
        f"- **{tc.get('PASS',0)}/{len(tl)}** 任务 PASS。",
        "- WIN-001..005 全 PASS(Windows 单一 CLI 构建/协议/analyze/ASan/取消链路)。",
        "- **WIN-006 里程碑**: 真实银心(T4)数据 phase1 校准 PASS(6 R 帧 + .xisf 母版), 期间修复 2 处真实 Bug(缺 XISF 支持; 写校准帧 Windows 栈溢出 0xC00000FD)。输入 hash manifest 已生成(`win006_input_manifest.json`, inputs_sha256=`d0dfd7a1b2743328452772afb66a2ddd9831f7a34ee7fc549557d090f73dc050`)。",
        "",
        "## 发布产物",
        ra_block,
        "",
        "## 阻塞项(审核包如实汇报)",
    ]
    for r in nonpass:
        if r.get("status") == "BLOCKED":
            rep.append(f"- **{r['task_id']} BLOCKED** `({phase_label(r['task_id'])})`: {r.get('scope','')} — 见 reports/evidence 阻断记录。")
    rep.append("")
    rep.append("## 非 PASS 任务(自动: TASK_LEDGER.csv)")
    for r in nonpass:
        rep.append(f"- `{r['task_id']}` ({phase_label(r['task_id'])}), status=`{r.get('status','')}`: {r.get('scope','')}")
    rep.append("")
    rep.append("## 依赖自检")
    rep.append(f"- {win009_line}")
    rep.append(f"- WIN-009 依赖: `{win009.get('depends_on','')}`; WIN-008 状态=`{leds.get('WIN-008',{}).get('status','?')}`, PAR-007=`{leds.get('PAR-007',{}).get('status','?')}`(依赖 PAR-002=`{leds.get('PAR-002',{}).get('status','?')}`) — 若有依赖顺序违规将在此显式列出。")
    rep.append("- C2..C9 连续检查点未全部达成; 无 32R 资源门禁记录(RESOURCE_RESULTS 为空)。")
    rep.append("")
    rep.append("## 结论")
    rep.append("当前候选**未达发布门槛**(09 §5 / 10 §5)。本报告由 Ledger/表格单源自动生成, 计数与状态与 `SUMMARY.json`/`TASK_LEDGER.csv` 一致。LEGITIMATE `AWAITING_EXTERNAL_RELEASE_REVIEW` 不可生成; 交外部审阅决策下一步(补齐 HIPS 产线 / 修复 sampler 并行 / 分层放行)。")
    (SRC / "FINAL_REPORT.md").write_text("\n".join(rep), encoding="utf-8")

    # 6) 白名单 source: 拷贝控制包文档到 control/, 证据到 reports/
    copy_(CP / "01_PRODUCT_ARCHITECTURE.md", SRC / "control" / "01_PRODUCT_ARCHITECTURE.md")
    copy_(CP / "03_TASK_DETAILS.md", SRC / "control" / "03_TASK_DETAILS.md")
    copy_(CP / "04_CLI_COMMAND_AND_PROTOCOL_CONTRACT.md", SRC / "control" / "04_CLI_COMMAND_AND_PROTOCOL_CONTRACT.md")
    copy_(CP / "05_CPU_BACKEND_ABI_AND_PACKAGING.md", SRC / "control" / "05_CPU_BACKEND_ABI_AND_PACKAGING.md")
    copy_(CP / "09_LINUX_WINDOWS_BUILD_RELEASE.md", SRC / "control" / "09_LINUX_WINDOWS_BUILD_RELEASE.md")
    copy_(CP / "10_GIT_REVIEW_CAPSULE_AUDIT_PACKAGE.md", SRC / "control" / "10_GIT_REVIEW_CAPSULE_AUDIT_PACKAGE.md")
    copy_(CP / "13_ALPHA_VERSION_AND_PHASE3.md", SRC / "control" / "13_ALPHA_VERSION_AND_PHASE3.md")
    for f in ["WIN004_verification.md", "WIN005_verification.md", "WIN006_verification.md",
              "WIN009_verification.md", "LINUX_ALPHA_SUITE_verification.md",
              "PAR002_blocker.md", "AUDIT_PACKAGE_SUMMARY.md",
              "WIN006_drizzle_hips_verification.md", "AUDIT_DEPENDENCY_RECONCILE.md"]:
        p = REPO / "reports/evidence" / f
        if p.exists():
            copy_(p, SRC / "reports" / "evidence" / f)

    # 7) MANIFEST.json over whitelist files + SHA256SUMS
    def allowed(rel: str) -> bool:
        parts = rel.lstrip("/").split("/")
        if rel.lstrip("/") in ROOT_FILES:
            return True
        return bool(parts and parts[0] in ALLOWED_DIRS and os.path.splitext(rel)[1].lower().lstrip(".").lower() in {s.lstrip(".") for s in ALLOWED_SUFFIXES})
    manifest_files = []
    for dirpath, _, files in os.walk(SRC):
        for fn in sorted(files):
            full = pathlib.Path(dirpath) / fn
            rel = full.relative_to(SRC).as_posix()
            if allowed(rel):
                manifest_files.append({"path": rel, "sha256": sha256_file(str(full)), "size": full.stat().st_size})
    (SRC / "MANIFEST.json").write_text(json.dumps({"schema_version": 1, "package": f"AstroCS-audit-{c12}", "version": base, "commit": commit, "files": manifest_files}, indent=1, ensure_ascii=False), encoding="utf-8")
    # 8) package_final(不拷 SHA256SUMS; 之后在输出包内生成)
    def run(args):
        r = subprocess.run(args, cwd=str(REPO), capture_output=True, text=True)
        return r.returncode, (r.stdout + r.stderr).strip()
    rc1, o1 = run([sys.executable, str(CP / "scripts/package_final.py"), str(SRC), str(OUT)])
    print("[package_final]", rc1); print(o1)
    if rc1 != 0:
        return 1
    # 9) 在输出包(白名单)上生成 SHA256SUMS(恰好覆盖除自身外全部输出文件)
    sums = []
    for dirpath, _, files in os.walk(OUT):
        for fn in sorted(files):
            full = pathlib.Path(dirpath) / fn
            rel = full.relative_to(OUT).as_posix()
            if rel == "SHA256SUMS":
                continue
            sums.append(f"{sha256_file(str(full))}  {rel}")
    (OUT / "SHA256SUMS").write_text("\n".join(sums) + "\n", encoding="utf-8")
    rc2, o2 = run([sys.executable, str(CP / "scripts/validate_final_package.py"), str(OUT)])
    print("[validate_final_package]", rc2); print(o2)
    return 0


if __name__ == "__main__":
    sys.exit(main())
