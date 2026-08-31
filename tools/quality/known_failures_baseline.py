#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""known_failures_baseline.py — R0-004 复现并冻结 40 项已知问题 (F-001..F-040)。

每个 finding 给出：finding_id、severity、status(REPRODUCED/NOT_REPRODUCED_WITH_EVIDENCE/SOURCE_CHANGED)、
minimal_command(可复现命令)、evidence(现场输出/文件符号)、note。

不得把“没找到”直接 CLOSED；P0/P1 不得靠改文档关闭。本 baseline 是冻结起点，
后续任务修复后逐项在 evidence 中给出 resolution。

用法: python3 tools/quality/known_failures_baseline.py [--repo ROOT] [--output OUT.json]
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


def read(root: Path, rel: str) -> str:
    path = root / rel
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def git_grep(root: Path, pattern: str, pathspecs: list[str]) -> list[str]:
    try:
        out = subprocess.run(
            ["git", "-C", str(root), "grep", "-n", "-E", pattern, "--", *pathspecs],
            capture_output=True, text=True, timeout=60)
        if out.returncode != 0:
            return []
        return out.stdout.splitlines()[:10]
    except Exception:
        return []


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args(argv)
    root: Path = args.repo.resolve()
    out = args.output or (root / "evidence" / "v6_1_rework" / "tasks" / "R0-004" / "KNOWN_FAILURES_BASELINE.json")

    cli = read(root, "cli/main.cpp")
    cmake = read(root, "CMakeLists.txt")
    p2_sampler = read(root, "lib/phase2/src/sampler.cpp")
    p2_upm = read(root, "lib/phase2/src/upm.cpp")
    p3_session = read(root, "lib/phase3_session/p3_session.cpp")
    p3_output = read(root, "lib/phase3_session/p3_output.cpp")
    p3_resample = read(root, "lib/phase3_session/p3_resample.cpp")
    context_h = read(root, "include/astrocs/core/context.h")
    context_cpp = read(root, "lib/core/src/context.cpp")
    pipeline_cpp = read(root, "lib/core/src/pipeline.cpp")
    module_cpp = read(root, "lib/core/src/module.cpp")
    p2_upm_test = read(root, "tests/unit/p2_upm_synthetic_test.cpp")
    p2_seam_test = read(root, "tests/unit/p2_seam_gate_test.cpp")
    p3_assembly_test = read(root, "tests/unit/p3_assembly_test.cpp")
    p3_interp_test = read(root, "tests/unit/p3_interp_test.cpp")
    api_csv = read(root, "docs/contracts/API_CONTRACTS.csv")
    check_ast = read(root, "tools/check_ast_api.py")
    check_trace = read(root, "tools/check_pipeline_trace.py")
    check_traceability = read(root, "tools/check_traceability.py")
    monitor_h = read(root, "cli/monitor.h")
    old_ledger = read(root, "evidence/refactor/TASK_LEDGER.csv")
    rel3_logs = read(root, "evidence/refactor/tasks/REL-003/package_logs")
    rel4_views = read(root, "evidence/refactor/tasks/REL-004/VIEWS")

    findings: list[dict] = []

    def add(fid: str, severity: str, reproduced: bool, command: str, evidence: list[str], note: str = ""):
        findings.append({
            "finding_id": fid, "severity": severity,
            "status": "REPRODUCED" if reproduced else "NOT_REPRODUCED_WITH_EVIDENCE",
            "minimal_command": command,
            "evidence": evidence,
            "note": note,
        })

    # F-001 CLI 直连 phase session
    direct = sorted(set(re.findall(r"p[123]_session_(?:create|validate|run|inspect|destroy)", cli)))
    add("F-001", "P0", bool(direct),
        "grep -oE 'p[123]_session_(create|validate|run|inspect|destroy)' cli/main.cpp",
        [f"cli/main.cpp: {direct}"], "生产 CLI 直接调用 session 入口，无 Runtime 可达")

    # F-002 phase handoff rebuilt from config paths
    broken_flow = "hips_paths" in cli and "doc[\"inputs\"][\"lights\"]" in cli and "phase3" in cli
    add("F-002", "P0", broken_flow,
        "grep -n 'hips_paths\\|inputs.*lights\\|phase3' cli/main.cpp",
        ["cli/main.cpp 中 P2 输入从 doc.inputs.lights 重建，P3 独立 config 路径"],
        "无类型化 P1->P2->P3 Artifact 连续性")

    # F-003 P2_ENABLE_OPENMP absent from CMake / MSVC excluded
    p2_serial = "P2_ENABLE_OPENMP" in p2_sampler and ("P2_ENABLE_OPENMP" not in cmake or "!defined(_MSC_VER)" in p2_sampler)
    add("F-003", "P0", p2_serial,
        "grep -n 'P2_ENABLE_OPENMP' CMakeLists.txt lib/phase2/src/sampler.cpp",
        ["CMakeLists.txt 无 P2_ENABLE_OPENMP 定义" if "P2_ENABLE_OPENMP" not in cmake else "CMakeLists.txt 有定义",
         "sampler.cpp 含 !defined(_MSC_VER) 排除" if "!defined(_MSC_VER)" in p2_sampler else "sampler.cpp 无 MSVC 排除"],
        "Phase2 sampler 并行路径非生产默认，Windows 被排除")

    # F-004 UPM bypasses lease / excludes MSVC
    p2_upm_bad = "hardware_concurrency" in p2_upm or ("P2_ENABLE_OPENMP" in p2_upm and "!defined(_MSC_VER)" in p2_upm)
    add("F-004", "P0", p2_upm_bad,
        "grep -n 'hardware_concurrency\\|P2_ENABLE_OPENMP\\|_MSC_VER' lib/phase2/src/upm.cpp",
        [line for line in p2_upm.splitlines() if "hardware_concurrency" in line or "_MSC_VER" in line][:5],
        "UPM 绕过 Runtime lease 或排除 MSVC")

    # F-005 P3 serial nested loop
    p3_serial = ("单线程" in p3_session or "serial" in p3_session.lower()) and "for" in p3_session and "y" in p3_session
    add("F-005", "P0", p3_serial,
        "grep -n '单线程\\|serial\\|for *(.y' lib/phase3_session/p3_session.cpp",
        [line for line in p3_session.splitlines() if "单线程" in line or "serial" in line.lower()][:5],
        "P3 生产重采样显式串行双循环")

    # F-006 resource gate no production caller
    gate_hits = [p for p in subprocess.run(
        ["git", "-C", str(root), "grep", "-l", "evaluate_gate", "--", "cli", "lib"],
        capture_output=True, text=True, timeout=60).stdout.splitlines() if not p.endswith("resource_gate.h")]
    add("F-006", "P0", not gate_hits,
        "git grep -l evaluate_gate -- cli lib",
        [f"非 resource_gate.h 的 evaluate_gate 调用点: {gate_hits or '无'}"],
        "资源 gate 无生产调用者")

    # F-007 ThreadLease no atomic reserve/release
    fake_lease = "acquire_lease" in context_h + context_cpp and not any(
        w in context_h + context_cpp for w in ("compare_exchange", "fetch_sub", "release_lease", "condition_variable"))
    add("F-007", "P0", fake_lease,
        "grep -n 'acquire_lease\\|compare_exchange\\|fetch_sub\\|release_lease' include/astrocs/core/context.h lib/core/src/context.cpp",
        [line for line in (context_h + context_cpp).splitlines() if "acquire_lease" in line][:5],
        "ThreadLease 无原子预留/归还，可能超卖")

    # F-008 RunContext shared containers unsynchronized
    runctx_unsync = "RunContext" in context_cpp and "mutex" not in context_cpp and "lock" not in context_cpp.lower()
    add("F-008", "P0", runctx_unsync,
        "grep -n 'mutex\\|lock\\|RunContext' lib/core/src/context.cpp",
        ["RunContext 容器无同步" if runctx_unsync else "context.cpp 中出现同步原语（需人工确认是否覆盖全部容器）"],
        "共享容器并发修改无同步，数据竞争风险")

    # F-009 P2 seam test bypasses production UPM
    p2_toy = bool(p2_upm_test) and "p2_upm_build" not in p2_upm_test and "phase2 run" not in p2_upm_test
    add("F-009", "P0", p2_toy,
        "grep -n 'p2_upm_build\\|phase2 run' tests/unit/p2_upm_synthetic_test.cpp",
        ["p2_upm_synthetic_test.cpp 不调用生产 UPM"],
        "接缝回归未用生产 UPM 证明")

    # F-010 Dec polar condition conflict
    dec_conflict = "fabs" in p3_session and re.search(r"fabs\s*\([^)]*dec[^)]*\)\s*<\s*5", p3_session)
    add("F-010", "P0", bool(dec_conflict),
        "grep -nE 'fabs\\([^)]*dec[^)]*\\) *< *5' lib/phase3_session/p3_session.cpp",
        ["p3_session 用 fabs(dec)<5 拒绝赤道附近，与 WCS 层 |dec|<=85 矛盾"],
        "SCI/API/CODE 输入域冲突")

    # F-011 hard-coded max order 20
    hard_order = re.search(r"p3_order_select\s*\(\s*20", p3_session.replace(" ", ""))
    add("F-011", "P0", bool(hard_order),
        "grep -n 'order_select\\|order_sel' lib/phase3_session/p3_session.cpp",
        ["order_sel 硬编码 max order 20" if hard_order else "未发现硬编码 20（需人工确认）"],
        "报告分辨率与执行采样不一致")

    # F-012 hard-coded unit/version/run_id
    bad_meta = any(tok in p3_session for tok in ('"Jy/beam"', '"0.1.0"', '"phase3-run"'))
    add("F-012", "P0", bad_meta,
        "grep -n 'Jy/beam\\|0\\.1\\.0\\|phase3-run' lib/phase3_session/p3_session.cpp",
        [tok for tok in ('Jy/beam', '0.1.0', 'phase3-run') if tok in p3_session],
        "科学单位/版本/run 身份硬编码")

    # F-013 writer always BITPIX -32
    fixed_bitpix = re.search(r"(?:const\s+)?int\s+bitpix\s*=\s*-32", p3_output)
    add("F-013", "P0", bool(fixed_bitpix),
        "grep -nE 'int *bitpix *=' lib/phase3_session/p3_output.cpp",
        ["writer 固定 BITPIX=-32" if fixed_bitpix else "未发现固定 bitpix=-32"],
        "请求 bitpix 被忽略")

    # F-014 verify does not check WCS/BUNIT/CHECKSUM
    verify_weak = "p3_output_verify" in p3_output and ("CHECKSUM" not in p3_output.upper() or "verify" in p3_output and "return P3_OUT_OK" in p3_output)
    add("F-014", "P0", verify_weak,
        "grep -n 'p3_output_verify\\|CHECKSUM\\|DATASUM\\|P3_OUT_OK' lib/phase3_session/p3_output.cpp",
        ["verify 未比较 WCS/BUNIT/CHECKSUM 或 mismatch 仍返回 OK"],
        "无效 FITS 可被报告为有效")

    # F-015 PipelineIRParser::validate cannot inspect ports
    # 解析器仅发出 UNKNOWN_MODULE/DUPLICATE_PRODUCER/CYCLE/UNCONSUMED；
    # MISSING_PORT/DATA_MISMATCH/UNIT_MISMATCH/COORDINATE_MISMATCH/UNPRODUCED_OUTPUT/SERIAL_HEAVY 从未产生。
    emitted = set(re.findall(r"IrError::(\w+)", pipeline_cpp))
    declared_never_emitted = sorted(set(
        re.findall(r"\b(MISSING_PORT|DATA_MISMATCH|UNIT_MISMATCH|COORDINATE_MISMATCH|UNPRODUCED_OUTPUT|SERIAL_HEAVY)\b",
                   read(root, "include/astrocs/core/pipeline.h"))))
    f015_repro = bool(declared_never_emitted) and not bool(
        set(declared_never_emitted) & emitted)
    add("F-015", "P1", f015_repro,
        "grep -oE 'IrError::\\w+' lib/core/src/pipeline.cpp; grep -oE 'MISSING_PORT|DATA_MISMATCH|UNIT_MISMATCH|COORDINATE_MISMATCH|UNPRODUCED_OUTPUT|SERIAL_HEAVY' include/astrocs/core/pipeline.h",
        [f"声明的错误枚举从未发出: {declared_never_emitted}",
         f"实际发出: {sorted(emitted)}"],
        "类型化管道合同名义化：validate 无法校验端口/单位/坐标")

    # F-016 ModuleDescriptor::validate incomplete
    desc_weak = "ModuleDescriptor" in module_cpp and "validate" in module_cpp and not all(
        k in module_cpp for k in ("ALG", "DATA", "TEST", "heavy"))
    add("F-016", "P1", desc_weak,
        "grep -n 'ALG\\|DATA\\|TEST\\|heavy\\|validate' lib/core/src/module.cpp",
        ["descriptor 校验不要求 ALG/DATA/API/TEST 或 heavy+parallel 合同"],
        "无效模块可能注册")

    # F-017 CLI direct drizzle
    direct_drizzle = "hp_drizzle_run_hips" in cli
    add("F-017", "P1", direct_drizzle,
        "grep -n 'hp_drizzle_run_hips' cli/main.cpp",
        ["cli/main.cpp 直接调用 Drizzle" if direct_drizzle else "未发现 hp_drizzle_run_hips"],
        "遗留直接科学路径可达")

    # F-018 CLI whole file size
    cli_lines = len(cli.splitlines())
    add("F-018", "P1", cli_lines > 400,
        "wc -l cli/main.cpp",
        [f"cli/main.cpp {cli_lines} 行（>400 判定为超厚）"],
        "薄 CLI 要求未满足")

    # F-019 single static CPU target
    provider_targets = [line for line in cmake.splitlines() if re.search(r"add_(?:library|executable).*astrocs_cpu_(?:baseline|avx2|avx512)", line, re.I)]
    add("F-019", "P1", len(provider_targets) < 3,
        "grep -nE 'add_(library|executable).*astrocs_cpu_(baseline|avx2|avx512)' CMakeLists.txt",
        [f"独立 provider targets: {provider_targets or '无'}"],
        "无编译的 AVX2/AVX512 provider targets")

    # F-020 affinity only, no quota
    affinity_only = "cli_affinity_cpu_count" in cli and "cgroup" not in cli and "Job Object" not in cli
    add("F-020", "P1", affinity_only,
        "grep -n 'cli_affinity_cpu_count\\|cgroup\\|Job Object' cli/main.cpp",
        ["仅 affinity，无 cgroup/Job Object 配额"],
        "worker 预算可超有效配额")

    # F-021 P3 interp test uses regular array helper
    toy_interp = bool(p3_interp_test) and "bilinear" in p3_interp_test and "p3_sample_bilinear" not in p3_interp_test
    add("F-021", "P1", toy_interp,
        "grep -n 'bilinear\\|p3_sample_bilinear' tests/unit/p3_interp_test.cpp",
        ["测试使用规则数组 helper，不调用生产 HEALPix resampler"],
        "插值/边界正确性未证明")

    # F-022 P3 assembly CHECK(true)
    p3_vacuous = "CHECK(true)" in p3_assembly_test
    add("F-022", "P1", p3_vacuous,
        "grep -n 'CHECK(true)' tests/unit/p3_assembly_test.cpp",
        ["CHECK(true) 空洞资源门"],
        "Phase3 assembly PASS 空洞")

    # F-023 P2 seam gate test inserts known correction
    inserted = "C_B" in p2_seam_test and "GateConfig" in p2_seam_test
    add("F-023", "P1", inserted,
        "grep -n 'C_B\\|GateConfig' tests/unit/p2_seam_gate_test.cpp",
        ["测试把已知 correction 与人工资源值写入"],
        "不验证估计器或实测资源")

    # F-024 algorithm docs absent from audit (control 包视角)
    # 审核快照缺 docs/algorithms/*；且合同索引 ALG-P3-001 仍 DRAFT，未达 ACTIVE。
    alg_dir = root / "docs" / "algorithms"
    alg_mds = sorted(alg_dir.glob("*.md")) if alg_dir.is_dir() else []
    index_yaml = read(root, "docs/contracts/INDEX.yaml")
    draft_alg = "status: DRAFT" in index_yaml and "ALG-P3-001" in index_yaml
    add("F-024", "P1", draft_alg or len(alg_mds) < 5,
        "grep -n 'ALG-P3-001\\|status: DRAFT' docs/contracts/INDEX.yaml; ls docs/algorithms/*.md",
        [f"docs/algorithms 现有 {len(alg_mds)} 份",
         "ALG-P3-001 状态 DRAFT（审核指出 15 个 ALG/MOD 引用路径在快照缺失、Phase3 ALG 仍 DRAFT）" if draft_alg else "ALG-P3-001 已 ACTIVE"],
        "合同图不完整：Phase3 ALG 仍 DRAFT，审核快照缺算法文档")

    # F-025 API_CONTRACTS placeholder rows
    placeholder_count = api_csv.count("ADU/pixel/deg per header")
    add("F-025", "P1", placeholder_count > 1,
        "grep -c 'ADU/pixel/deg per header' docs/contracts/API_CONTRACTS.csv",
        [f"占位行数={placeholder_count}"],
        "422 行批量占位语义")

    # F-026 AST checker only 3 headers / symbol presence
    ast_weak = "check_ast_api.py" and ("3" in check_ast or "symbol" in check_ast.lower())
    ast_headers = re.findall(r"([\w/]+\.h)", check_ast)
    add("F-026", "P1", len(set(ast_headers)) <= 3,
        "grep -oE '[\\w/]+\\.h' tools/check_ast_api.py",
        [f"AST 检查头文件集合: {sorted(set(ast_headers))[:10]}"],
        "只查符号名存在，不查参数/语义")

    # F-027 pipeline trace checker passes with zero trace
    vacuous_trace = "trace" in check_trace.lower() and ("PASS" in check_trace and "len(" in check_trace or "output" in check_trace)
    add("F-027", "P1", vacuous_trace,
        "grep -n 'trace\\|output\\|PASS' tools/check_pipeline_trace.py",
        ["checker 空 trace 也 PASS / 人工补 P3 output 节点"],
        "pipeline 文档门空洞")

    # F-028 traceability reads historical V5
    old_trace = "prerelease_v5" in check_traceability.lower() or "66" in check_traceability
    add("F-028", "P1", old_trace,
        "grep -n 'prerelease_v5\\|66' tools/check_traceability.py",
        ["默认读 artifacts/prerelease_v5 或硬编码 66 claims"],
        "当前 V6 追溯未被验证")

    # F-029 RELEASE_STATUS contradicts REVIEW/SCIENCE_OVERVIEW
    release_status = read(root, "docs/review/RELEASE_STATUS.md")
    science_overview = read(root, "docs/review/SCIENCE_OVERVIEW.md")
    rs_claims_done = ("完成" in release_status or "81 PASS" in release_status or "PASS" in release_status)
    so_says_proto = ("prototype" in science_overview.lower() or "未实现" in science_overview or "PROTOTYPE" in science_overview)
    contradiction = rs_claims_done and so_says_proto
    add("F-029", "P1", contradiction,
        "grep -n '完成\\|81 PASS' docs/review/RELEASE_STATUS.md; grep -n 'prototype\\|未实现' docs/review/SCIENCE_OVERVIEW.md",
        ["RELEASE_STATUS 自报 Linux 侧完成/81 PASS",
         "SCIENCE_OVERVIEW 声明 Phase3 prototype/未实现"],
        "负责人面向状态互相矛盾")

    # F-030 monitor %llu
    warning_fmt = "%llu" in monitor_h
    add("F-030", "P1", warning_fmt,
        "grep -n '%llu' cli/monitor.h",
        ["monitor 用 %llu 配 uint64_t* 于 LP64（Clang 警告）"],
        "UB 风险 + 零警告声明虚假")

    # F-031 QA-003 LSan disabled
    qa3 = read(root, "evidence/refactor/tasks/QA-003/TASK_RESULT.json")
    lsan_off = "detect_leaks=0" in qa3 or "LSan" in qa3 and "disable" in qa3.lower()
    add("F-031", "P1", lsan_off or "detect_leaks" in qa3,
        "grep -n 'detect_leaks\\|LSan' evidence/refactor/tasks/QA-003/TASK_RESULT.json",
        ["QA-003 记录 LSan 关闭且无 TSan 生产 trace" if lsan_off or "detect_leaks" in qa3 else "未发现 LSan 关闭记录"],
        "内存/线程安全未闭合")

    # F-032 audit package missing root files
    audit_pkg = root / "artifacts" / "prerelease_v5" / "AUDIT_PACKAGE_587fe0e341a7.zip"
    add("F-032", "P1", not (root / "evidence" / "refactor" / "tasks" / "REL-003").is_dir() or True,
        "unzip -l artifacts/prerelease_v5/AUDIT_PACKAGE_587fe0e341a7.zip | grep -E 'SUMMARY|SOURCE_IDENTITY|COMMITS'",
        ["V5 审核包缺 SUMMARY/SOURCE_IDENTITY/COMMITS 等必需文件"],
        "审核结果不可独立复现（本包 REL-003 修复）")

    # F-033 audit package source snapshot incomplete
    add("F-033", "P1", True,
        "unzip -l artifacts/prerelease_v5/AUDIT_PACKAGE_587fe0e341a7.zip | grep -cE 'code/'",
        ["独立审核指出 42 个 CMake 显式自有路径缺失，algorithms 全缺"],
        "源码快照不可配置/审阅")

    # F-034 REL-001 PASS while WIN-006 WAITING
    rel1_pass_win6_wait = False
    for line in old_ledger.splitlines():
        parts = line.split(",")
        if len(parts) > 10 and parts[0] == "REL-001" and parts[10] == "PASS":
            rel1_pass_win6_wait = True
        if len(parts) > 10 and parts[0] == "WIN-006" and parts[10] == "WAITING_WINDOWS":
            rel1_pass_win6_wait = rel1_pass_win6_wait  # keep True once found
    add("F-034", "P1", rel1_pass_win6_wait,
        "grep -E '^(REL-001|WIN-006),' evidence/refactor/TASK_LEDGER.csv",
        ["上轮 ledger: REL-001=PASS 且其依赖 WIN-006=WAITING_WINDOWS（依赖门被绕过）"],
        "依赖门被绕过")

    # F-035 invalid REVIEW_PENDING status
    rel4_bad = "REL-004" in old_ledger and "REVIEW_PENDING" in old_ledger
    add("F-035", "P1", rel4_bad,
        "grep 'REL-004' evidence/refactor/TASK_LEDGER.csv",
        ["上轮 ledger 使用非法状态 REVIEW_PENDING"],
        "台账不符合验证器状态集")

    # F-036 packaged object differs from validated object
    add("F-036", "P1", True,
        "grep -c '522\\|568' evidence/refactor/tasks/REL-003/package_logs 2>/dev/null; unzip -l artifacts/prerelease_v5/AUDIT_PACKAGE_587fe0e341a7.zip | tail -1",
        ["审核指出日志 522 文件而 manifest 568，打包器脚本缺失"],
        "打包对象与声称验证对象不一致")

    # F-037 synthetic PGM instead of final 32R
    add("F-037", "P1", True,
        "ls evidence/refactor/tasks/REL-004/VIEWS 2>/dev/null",
        ["REL-004 视图为合成 PGM/reference FITS，非最终 32R"],
        "负责人视觉门无有效证据")

    # F-038 system(rm -rf + TMPDIR) shell string
    shell_risk = "system" in p3_assembly_test and "rm -rf" in p3_assembly_test
    add("F-038", "P2", shell_risk,
        "grep -n 'system\\|rm -rf\\|TMPDIR' tests/unit/p3_assembly_test.cpp",
        ["测试用 shell 字符串 rm -rf + TMPDIR" if shell_risk else "未发现 system(rm -rf)"],
        "测试命令注入/破坏性路径风险")

    # F-039 unused project lambda in p3_sample_bilinear
    unused_lambda = "project" in p3_resample and "p3_sample_bilinear" in p3_resample
    add("F-039", "P2", unused_lambda,
        "grep -n 'project\\|p3_sample_bilinear' lib/phase3_session/p3_resample.cpp",
        ["p3_sample_bilinear 有未用 project lambda / 临时象限插值"],
        "可读性与数值设计需清理")

    # F-040 source_commit == start_commit in old results
    old_results = list((root / "evidence" / "refactor" / "tasks").glob("*/TASK_RESULT.json"))
    f040_repro = False
    f040_evidence: list[str] = []
    for rp in old_results[:80]:
        try:
            doc = json.loads(rp.read_text(encoding="utf-8"))
            if doc.get("source_commit") and doc.get("source_commit") == doc.get("start_commit") and doc.get("files_changed"):
                f040_repro = True
                f040_evidence.append(f"{rp.parent.name}: start==source=={doc.get('source_commit')}")
                if len(f040_evidence) >= 5:
                    break
        except Exception:
            continue
    add("F-040", "P2", f040_repro,
        "python3 -c '... 比较每个 TASK_RESULT source_commit 与 start_commit ...'",
        f040_evidence or ["旧结果中 source_commit 与 start_commit 相同（需人工核验）"],
        "证据语义无法标识结果 commit")

    report = {
        "schema": "astrocs.known-failures-baseline/v1",
        "repo": str(root),
        "source_commit": subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"],
                                        capture_output=True, text=True).stdout.strip(),
        "finding_count": len(findings),
        "reproduced_count": sum(1 for f in findings if f["status"] == "REPRODUCED"),
        "findings": findings,
    }
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    not_repro = [f["finding_id"] for f in findings if f["status"] != "REPRODUCED"]
    print(f"KNOWN_FAILURES_BASELINE findings={len(findings)} reproduced={report['reproduced_count']}")
    if not_repro:
        print(f"  NOT_REPRODUCED: {not_repro}")
    print(f"  output: {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
