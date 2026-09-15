#!/usr/bin/env bash
# CONTRACT-FREEZE-001 一键复跑：单一 rc（全部子步骤 rc=0 才 0）。
set -u
cd "$(dirname "$0")/../../.." || exit 2
ROOT="$(pwd)"
LOG="$ROOT/reports/v6/contract-review/logs"
EV="$ROOT/reports/v6/contract-review/evidence"
mkdir -p "$LOG" "$EV"
rc_total=0
run() { local name="$1"; shift; echo "== $name"; "$@" > "$LOG/$name.log" 2>&1; local rc=$?; echo "   rc=$rc"; [ "$rc" -eq 0 ] || rc_total=1; return $rc; }
run 10_gen          python3 reports/v6/contract-review/tools/gen_freeze.py
run 20_oracle       python3 reports/v6/contract-review/tools/oracle_contract_freeze.py
run 30_mutations    python3 reports/v6/contract-review/tools/mutate_and_check.py
run 40_scope        python3 reports/v6/contract-review/tools/scope_check.py
run 50_doc_consistency python3 reports/v6/contract-review/tools/check_doc_consistency.py
python3 - "$EV" "$rc_total" <<'PY'
import json, sys, os
ev, rc = sys.argv[1], int(sys.argv[2])
out = {"run_all_rc": rc}
mp = os.path.join(ev, "mutations.json")
if os.path.isfile(mp):
    out.update(json.load(open(mp, encoding="utf-8")))
sp = os.path.join(ev, "scope_check.json")
if os.path.isfile(sp):
    out["scope_check"] = json.load(open(sp, encoding="utf-8"))
json.dump(out, open(os.path.join(ev, "rc_summary.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
print("rc_total=%d" % rc)
PY
echo "RUN_ALL rc=$rc_total"
exit $rc_total
