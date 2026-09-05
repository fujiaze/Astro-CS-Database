#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci/reconcile_state.py — V81-ADOPT-003 任务与证据对账验收脚本（最小实现）。

用法：
    python3 ci/reconcile_state.py --current-first --strict

校验（任一 FAIL 即非零退出）：
  1 task_state_schema      TASK_STATE.json 可解析且 schema 字段齐备（每任务五字段）；
  2 v71_coverage           191 个 V7.1 task_id 全部在 state.tasks 且状态合法；
  3 reconciliation_match   STATE_RECONCILIATION.csv 行数与 TASK_STATE 任务数一致，
                           每行 final_status 与 TASK_STATE 一致；
  4 commit_ledger          COMMIT_LEDGER.jsonl 每行 SHA 存在于 git log 且行数一致；
  5 seed_integrity         V81-ADOPT-001/002 seed 记录未被覆盖丢失（CLOSED + commit 匹配）；
  6 current_first (--current-first)  关键状态来源可追溯到当前证据，而非盲信审核快照。

--strict 使全部检查为硬失败（默认亦硬失败，flag 显式化验收形态）。
只读校验：除 stdout 外不写任何文件。
"""
import argparse
import csv
import json
import subprocess
import sys
from pathlib import Path

REPO_CANDIDATES = [
    Path(__file__).resolve().parent.parent,          # <repo>/ci/reconcile_state.py
    Path.cwd(),
]

V71_LEDGER = ("engineering/control/active/"
              "AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905/"
              "baseline/V7_1_STATIC_TASK_LEDGER.csv")
OUT_DIR = Path("evidence/v8_1_ci_control")
VALID_STATUS = {"CLOSED", "NOT_STARTED", "READY", "DISPATCHED", "FATDUCK_PENDING",
                "WAITING_RESOURCE"}
REQUIRED_FIELDS = ("owner", "status", "closed_commit", "evidence_path", "source_of_status")
RECON_COLUMNS = ["task_id", "v71_status", "review_claim", "current_evidence",
                 "final_status", "source_of_status", "commit", "evidence_path", "note"]
SEED_EXPECT = {
    "V81-ADOPT-001": ("a4fdee3f", "FG-000", "evidence/v8_1_ci_control/adoption/"),
    "V81-ADOPT-002": ("af9ecdc5", "SA-ADOPT-31", "evidence/v8_1_ci_control/tasks/V81-ADOPT-002/"),
}
KEY_RULES = {
    "CLI-002": lambda s: s["status"] == "CLOSED" and len(s["closed_commit"]) >= 40,
    "CPU-006": lambda s: not (s["status"] == "DISPATCHED" and not s["closed_commit"]),
    "WIN-002": lambda s: s["status"] == "FATDUCK_PENDING",
}


def find_repo_root() -> Path:
    for c in REPO_CANDIDATES:
        try:
            out = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=str(c),
                                 capture_output=True, text=True, timeout=30)
            if out.returncode == 0:
                return Path(out.stdout.strip())
        except (subprocess.TimeoutExpired, OSError):
            continue
    raise SystemExit("FATAL: cannot locate git repository root")


def run_git(root: Path, argv, timeout=120):
    out = subprocess.run(["git"] + argv, cwd=str(root), capture_output=True,
                         text=True, timeout=timeout)
    if out.returncode != 0:
        raise RuntimeError(f"git {' '.join(argv)} failed rc={out.returncode}: {out.stderr[:300]}")
    return out.stdout


def main() -> int:
    ap = argparse.ArgumentParser(description="V81-ADOPT-003 reconciliation acceptance")
    ap.add_argument("--current-first", action="store_true",
                    help="require statuses traceable to current evidence (not review snapshot)")
    ap.add_argument("--strict", action="store_true",
                    help="treat all violations as hard failures (non-zero exit)")
    args = ap.parse_args()

    root = find_repo_root()
    state_path = root / OUT_DIR / "TASK_STATE.json"
    recon_path = root / OUT_DIR / "STATE_RECONCILIATION.csv"
    ledger_path = root / OUT_DIR / "COMMIT_LEDGER.jsonl"
    v71_path = root / V71_LEDGER

    checks = []  # (name, ok, detail)

    def check(name, ok, detail=""):
        checks.append((name, bool(ok), detail))
        return ok

    # ---- 1 task_state_schema ----
    try:
        state = json.loads(state_path.read_text(encoding="utf-8"))
        check("task_state_parse", True, f"{state_path}")
    except Exception as exc:  # noqa: BLE001
        check("task_state_parse", False, f"unparseable: {exc}")
        return report(checks, args, root)
    tasks = state.get("tasks")
    schema_ok = isinstance(state, dict) and state.get("schema_version") == "v8.1" \
        and isinstance(tasks, dict) and len(tasks) > 0
    bad_fields = []
    if schema_ok:
        for tid, rec in tasks.items():
            if not isinstance(rec, dict) or any(f not in rec for f in REQUIRED_FIELDS):
                bad_fields.append(tid)
            elif rec.get("status") not in VALID_STATUS:
                bad_fields.append(f"{tid}:bad_status:{rec.get('status')}")
    check("task_state_schema", schema_ok and not bad_fields,
          f"schema_version={state.get('schema_version')!r} tasks={len(tasks) if isinstance(tasks, dict) else 'N/A'}"
          + (f" violations={bad_fields[:5]}" if bad_fields else ""))

    # ---- 2 v71_coverage ----
    try:
        with open(v71_path, newline="", encoding="utf-8") as fh:
            v71_ids = [r["task_id"] for r in csv.DictReader(fh)]
    except Exception as exc:  # noqa: BLE001
        v71_ids = []
        check("v71_ledger_readable", False, str(exc))
    missing = [t for t in v71_ids if t not in tasks]
    check("v71_coverage", bool(v71_ids) and not missing,
          f"v71_rows={len(v71_ids)} missing={missing[:10]}")
    extra = sorted(set(tasks) - set(v71_ids))
    if extra:
        check("v71_no_extra_tasks", False, f"unexpected in state.tasks: {extra[:10]}")
    else:
        check("v71_no_extra_tasks", True, "state.tasks == V7.1 id set (191)")

    # ---- 3 reconciliation_match ----
    try:
        with open(recon_path, newline="", encoding="utf-8") as fh:
            reader = csv.DictReader(fh)
            header_ok = list(reader.fieldnames or []) == RECON_COLUMNS
            recon = list(reader)
    except Exception as exc:  # noqa: BLE001
        recon, header_ok = [], False
    check("reconciliation_header", header_ok, f"rows={len(recon)}")
    counts_match = bool(recon) and len(recon) == len(tasks)
    mism = []
    if counts_match:
        by_id = {r["task_id"]: r for r in recon}
        if set(by_id) != set(tasks):
            mism = sorted(set(by_id) ^ set(tasks))[:10]
        else:
            mism = [f"{t}: {by_id[t]['final_status']} != {tasks[t]['status']}"
                    for t in tasks if by_id[t]["final_status"] != tasks[t]["status"]]
    check("reconciliation_match", counts_match and not mism,
          f"csv_rows={len(recon)} state_tasks={len(tasks)}"
          + (f" mismatches={mism[:5]}" if mism else ""))

    # ---- 4 commit_ledger ----
    try:
        git_shas = set(run_git(root, ["log", "--format=%H"]).split())
        n_main = int(run_git(root, ["rev-list", "--count", "main"]).strip())
        entries = [json.loads(line) for line in
                   ledger_path.read_text(encoding="utf-8").splitlines() if line.strip()]
        bad_sha = [e["sha"][:12] for e in entries if e.get("sha") not in git_shas]
        bad_keys = [i for i, e in enumerate(entries)
                    if not {"sha", "subject", "task_id", "date"} <= set(e)]
        check("commit_ledger", not bad_sha and not bad_keys and len(entries) == n_main,
              f"lines={len(entries)} git_main={n_main} bad_sha={bad_sha[:5]} bad_keys={bad_keys[:5]}")
    except Exception as exc:  # noqa: BLE001
        check("commit_ledger", False, str(exc))

    # ---- 5 seed_integrity ----
    seed_ok = True
    seed_detail = []
    seeds = dict(state.get("adopt_seed_tasks") or {})
    for t in SEED_EXPECT:  # 兼容：seed 也可能仍在 tasks 顶层
        if t in tasks and t not in seeds:
            seeds[t] = tasks[t]
    for tid, (sha8, owner, ev) in SEED_EXPECT.items():
        rec = seeds.get(tid)
        if not isinstance(rec, dict):
            seed_ok = False
            seed_detail.append(f"{tid}: MISSING")
            continue
        if rec.get("status") != "CLOSED":
            seed_ok = False
            seed_detail.append(f"{tid}: status={rec.get('status')}")
        cc = rec.get("closed_commit") or rec.get("commit") or ""
        if not str(cc).startswith(sha8):
            seed_ok = False
            seed_detail.append(f"{tid}: commit={cc!r} expect {sha8}x")
        if rec.get("owner") != owner or rec.get("evidence_path") != ev:
            seed_ok = False
            seed_detail.append(f"{tid}: owner/evidence drift")
    check("seed_integrity", seed_ok, "; ".join(seed_detail) or
          "V81-ADOPT-001/002 CLOSED + commit/owner/evidence preserved")

    # ---- 6 current_first ----
    if args.current_first:
        cf_ok = True
        cf_detail = []
        for tid, rule in KEY_RULES.items():
            rec = tasks.get(tid)
            if rec is None or not rule(rec):
                cf_ok = False
                cf_detail.append(f"{tid}: rule violated (status={rec.get('status') if rec else 'N/A'})")
        blind = [t for t, rec in tasks.items()
                 if rec.get("source_of_status") in ("review_claim_only", "snapshot_only")]
        if blind:
            cf_ok = False
            cf_detail.append(f"status from snapshot without current evidence: {blind[:10]}")
        check("current_first_key_rules", cf_ok, "; ".join(cf_detail) or
              "CLI-002 CLOSED w/ commit; CPU-006 not DISPATCHED-w/o-commit; WIN-002 FATDUCK_PENDING; "
              "no snapshot-only statuses")

    return report(checks, args, root)


def report(checks, args, root) -> int:
    failed = [c for c in checks if not c[1]]
    ok = not failed
    summary = {
        "script": "ci/reconcile_state.py",
        "mode": {"current_first": args.current_first, "strict": args.strict},
        "repo_root": str(root),
        "checks_total": len(checks),
        "checks_passed": sum(1 for _, s, _ in checks if s),
        "checks_failed": len(failed),
        "verdict": "PASS" if ok else "FAIL",
        "checks": [{"name": n, "ok": s, "detail": d} for n, s, d in checks],
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
