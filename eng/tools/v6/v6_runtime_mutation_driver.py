#!/usr/bin/env python3
"""RUNTIME-CI-001 负向 mutation 驱动：违反「资源门/预算/确定性/CLI 模式路由」必红。

做法（不修改工作树）：
  1. 把运行面关键文件复制到临时树（保持仓库相对路径）；
  2. 先在未注入的临时树上跑 eng/tools/v6/v6_runtime_oracle.py —— 必须 PASS（防 Oracle 假红）；
  3. 逐条注入违规，重跑 Oracle —— 每条必须 FAIL（rc != 0），否则本驱动 FAIL。

用法:
  python3 eng/tools/v6/v6_runtime_mutation_driver.py [--repo <root>] [--json-out <path>]
exit 0 = 全部注入被检出且干净树为绿；1 = 有漏检/Oracle 假红；2 = 用法/IO 错误。
"""
import argparse
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile

DEFAULT_REPO = pathlib.Path(__file__).resolve().parents[3]

ORACLE_REL = "eng/tools/v6/v6_runtime_oracle.py"

# 需要复制进临时树的运行面文件（Oracle 的读取面）
# **W4-A3 订正**：原 COPY_SET 缺 `lib/infrastructure/cli/command_tree.h` —— R2/R3
# 改绑到命令树唯一事实源后，临时树里缺该文件 ⇒ Oracle 报
# `FILE-MISSING command_tree.h` ⇒ mutation_driver 的"干净树必须先 PASS"自检失败
# （clean tree oracle not PASS）。COPY_SET 必须与 Oracle 的 ctx.read() 面逐项一致。
COPY_SET = [
    "lib/infrastructure/cli/v6_runtime_contract.h",
    "lib/infrastructure/cli/v6_mode_gate.h",
    "lib/infrastructure/cli/command_tree.h",
    "lib/infrastructure/cli/parser.cpp",
    "lib/infrastructure/cli/commands.cpp",
    "lib/infrastructure/cli/monitor.h",
    "lib/infrastructure/cli/resource_recorder.h",
    "lib/infrastructure/scheduler/v6_budget.py",
    "eng/ci/checks.json",
    "eng/tests/system/v6_runtime/v6_runtime_determinism_test.cpp",
]

# (id, rel, old, new) —— old 必须精确命中一次
MUTATIONS = [
    ("deferred-mode-enters-production", "lib/infrastructure/cli/v6_runtime_contract.h",
     "FZ-MODE-DEFERRED: psf_snr_power is DEFERRED",
     "FZ-MODE-PRODUCTION: psf_snr_power is DEFERRED"),
    ("second-private-budget-source", "lib/infrastructure/cli/v6_runtime_contract.h",
     "if (has_source_) return false;              // 第二来源：拒绝",
     "if (false) return false;                    // 第二来源被偷偷放行"),
    ("legacy-weight-mode-0-allowed", "lib/infrastructure/cli/v6_runtime_contract.h",
     '    if (v == 0) {\n        r.reason = "FZ-FIELD-WEIGHTMODE: legacy weight_mode 0 (support x SNR^2) is not a "\n                   "science variance surface and is rejected (fail-closed)";\n    } else {\n        r.reason = "FZ-FIELD-WEIGHTMODE: unsupported legacy weight_mode integer";\n    }',
     '    r.kind = RouteKind::kBaseline;\n    r.token = "equal";\n    r.reason = "legacy 0 allowed (violation)";'),
    ("so05-auto-hard-fail", "lib/infrastructure/cli/v6_runtime_contract.h",
     "    v.status = \"record_only_pending_owner_signoff\";\n    v.hard_fail = false;",
     "    v.status = \"record_only_pending_owner_signoff\";\n    v.hard_fail = true;"),
    # W4-A3 改绑：R3 现读 command_tree.h（parser.cpp 的字面量表已随 CLI-001 删除）。
    ("aggregate-pipeline-entry", "lib/infrastructure/cli/command_tree.h",
     '        {"benchmark", true, {}},',
     '        {"benchmark", true, {}},\n        {"pipeline", true, {"--config"}},'),
    # W4-A3 改绑：phase2 阶段的用户命令名 = mosaic（ASTROCS_DESIGN §2:317）；
    # R2 现读 command_tree.h，故注入点随之改到 mosaic 的旗标表。
    #
    # RELEASE-02 改绑（2026-09-19，GATE-RED-FIX）：**语义不变**，仍注入
    # 「phase2(mosaic) 丢失 --mode 显式模式旗标」。原锚点尾部 '--mode"}},' 已因
    # §8/21_observability §8.4 资源门 enforce 旗标落地（command_tree.h:85-87 mosaic
    # 行新增 --strict-resource-gate / --on-resource-gate）而失配：anchor matched
    # 0 times ⇒ mutation_driver 判「漏检」，V6-RUNTIME-CLOSURE/V6-NEGATIVE-MUTATION
    # 双红。按注入锚维护惯例（同 W4-A3 两处改绑）把锚点同步到新形态：注入后
    # mosaic 的 allowed 仍缺 --mode，Oracle 规则 R2-phase2-mode-flag
    # （v6_runtime_oracle.py::rule_r2_cli_flags，"--mode" in flags）必须判红；
    # 可检出性由本驱动的 CLEAN_TREE(必绿) + 逐条注入(必红) 自证。
    ("phase2-mode-flag-dropped", "lib/infrastructure/cli/command_tree.h",
     '                             "-y", "--yes", "-force", "--events-jsonl", "--cpu-profile",\n                             "--mode", "--strict-resource-gate", "--on-resource-gate"}},',
     '                             "-y", "--yes", "-force", "--events-jsonl", "--cpu-profile",\n                             "--strict-resource-gate", "--on-resource-gate"}},'),
    ("per-thread-metric-dropped", "lib/infrastructure/cli/resource_recorder.h",
     "    double per_thread_cpu_max_pct = 0.0; // 单线程区间 CPU / interval × 100",
     "    double deleted_metric_placeholder = 0.0;"),
    ("iowait-metric-dropped", "lib/infrastructure/cli/monitor.h",
     "    double sys_io_wait_seconds = 0.0;",
     "    double sys_deleted_placeholder = 0.0;"),
    ("hardcoded-thread-count", "lib/infrastructure/cli/commands.cpp",
     "static uint32_t cli_affinity_cpu_count() {",
     "static uint32_t cli_affinity_cpu_count() {\n    omp_set_num_threads(8);  // 硬编码线程数（违规）"),
    ("private-long-lived-pool", "lib/infrastructure/cli/v6_mode_gate.h",
     "namespace astrocs {\nnamespace v6cli {",
     "namespace astrocs {\nnamespace v6cli {\n// 违规：不受 Runtime 管理的私有长期线程池\nstatic std::thread g_private_pool_thread;"),
    ("ci-check-deregistered", "eng/ci/checks.json",
     '"id": "V6-RUNTIME-CLOSURE"',
     '"id": "V6-RUNTIME-CLOSURE-DISABLED"'),
]


def run_oracle(repo: pathlib.Path, oracle_src: pathlib.Path) -> tuple:
    """返回 (rc, stdout, parsed_or_none)。oracle 从被测 repo 内执行。"""
    oracle = repo / ORACLE_REL
    oracle.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(oracle_src, oracle)
    p = subprocess.run([sys.executable, str(oracle), "--repo", str(repo)],
                       capture_output=True, text=True, timeout=180)
    parsed = None
    try:
        # oracle 不打印 JSON 主体；用 --json-out 落盘再读
        pass
    except Exception:
        pass
    return p.returncode, p.stdout + p.stderr, parsed


def run_oracle_json(repo: pathlib.Path, oracle_src: pathlib.Path, out: pathlib.Path) -> tuple:
    oracle = repo / ORACLE_REL
    oracle.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(oracle_src, oracle)
    out.parent.mkdir(parents=True, exist_ok=True)
    p = subprocess.run([sys.executable, str(oracle), "--repo", str(repo), "--json-out", str(out)],
                       capture_output=True, text=True, timeout=180)
    parsed = None
    if out.exists():
        try:
            parsed = json.loads(out.read_text(encoding="utf-8"))
        except Exception:
            parsed = None
    return p.returncode, p.stdout + p.stderr, parsed


def build_tree(repo: pathlib.Path, dest: pathlib.Path):
    for rel in COPY_SET:
        src = repo / rel
        if not src.exists():
            raise SystemExit("mutation_driver: missing source file %s" % rel)
        dst = dest / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=str(DEFAULT_REPO))
    ap.add_argument("--json-out", default="")
    args = ap.parse_args(argv)
    repo = pathlib.Path(args.repo).resolve()
    oracle_src = repo / ORACLE_REL
    if not oracle_src.exists():
        print("mutation_driver: oracle missing: %s" % oracle_src, file=sys.stderr)
        return 2

    results = []
    with tempfile.TemporaryDirectory(prefix="v6rt_mut_") as td:
        clean = pathlib.Path(td) / "clean"
        clean.mkdir(parents=True)
        build_tree(repo, clean)
        rc, out, parsed = run_oracle_json(clean, oracle_src, pathlib.Path(td) / "clean.json")
        clean_ok = (rc == 0)
        results.append({"id": "CLEAN_TREE", "detected": clean_ok, "expected": "PASS",
                        "rc": rc, "note": out.strip().splitlines()[0] if out.strip() else ""})
        if not clean_ok:
            print("MUTATION_DRIVER_FAIL: clean tree oracle not PASS (false positive)")
            print(out[:2000])
            _emit(args.json_out, results, "FAIL")
            return 1

        all_detected = True
        for mid, rel, old, new in MUTATIONS:
            work = pathlib.Path(td) / ("mut_" + mid)
            work.mkdir(parents=True)
            build_tree(repo, work)
            target = work / rel
            text = target.read_text(encoding="utf-8")
            n = text.count(old)
            if n != 1:
                results.append({"id": mid, "detected": False, "expected": "RED",
                                "rc": None, "note": "anchor matched %d times (expected 1)" % n})
                all_detected = False
                continue
            target.write_text(text.replace(old, new, 1), encoding="utf-8")
            rc2, out2, _ = run_oracle_json(work, oracle_src, pathlib.Path(td) / (mid + ".json"))
            detected = (rc2 != 0)
            if not detected:
                all_detected = False
            results.append({"id": mid, "rel": rel, "detected": detected, "expected": "RED",
                            "rc": rc2,
                            "note": (out2.strip().splitlines()[0] if out2.strip() else "")})

    verdict = "PASS" if all_detected else "FAIL"
    _emit(args.json_out, results, verdict)
    if all_detected:
        print("V6_RUNTIME_MUTATION_PASS detected=%d/%d clean=PASS" %
              (len(MUTATIONS), len(MUTATIONS)))
        return 0
    print("V6_RUNTIME_MUTATION_FAIL")
    for r in results:
        if not (r["detected"] or r["id"] == "CLEAN_TREE"):
            print("  [MISSED] %s (%s) rc=%s %s" % (r["id"], r.get("rel", "-"), r["rc"], r.get("note", "")))
    return 1


def _emit(path, results, verdict):
    if not path:
        return
    p = pathlib.Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps({"schema": "astrocs.v6.runtime-mutations/v1",
                             "verdict": verdict, "results": results},
                            ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())
