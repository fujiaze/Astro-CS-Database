# -*- coding: utf-8 -*-
"""QA-MATRIX-001 一键复跑：组装 -> Oracle -> 规格校验 -> 渲染 -> 一致性 -> case ledger -> mutation。

产物：reports/v6/qa-design/evidence/ 下的 JSON 结果与命令日志 + rc_summary.json。
退出码：0 全部通过；1 任一步红；2 用法。
"""
from __future__ import annotations
import json, os, subprocess, sys, collections

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
EVID = os.path.join(ROOT, "evidence")
LOGS = os.path.join(EVID, "logs")
DOCS = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(ROOT))), "docs", "validation", "v6")

PENDING = {"G-RD-01": ("REAL-SCIENCE-001 (W10)", 6), "G-RD-02": ("REAL-SCIENCE-001 (W10)", 6),
           "G-RD-06": ("WIN-VERIFY-001 (W11)", 1), "G-BASE-03": ("REAL-SCIENCE-001 (W10)", 4)}
P0_EXEC = {"P0-01": 2, "P0-02": 2, "P0-03": 2, "P0-04": 2, "P0-05": 5, "P0-06": 2}


def run(cmd, log):
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")
    with open(os.path.join(LOGS, log), "w", encoding="utf-8") as f:
        p = subprocess.run(cmd, cwd=HERE, stdout=f, stderr=subprocess.STDOUT, text=True, env=env)
    return p.returncode


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


# ------------------------------------------------------------------ 独立证据
# 「该门有多少验收证据」必须独立于该门自身的判决，否则激励方向是反的：
# 恒真门永远绿，于是永远拿满额证据；真正判出红的门反而被扣证据。
# 唯一认可的独立证据是「注入已知缺陷后，本门是否被实测观察到变红」。
#   gate_level_red    —— 本门登记的 mutation 中，实测观测到**本门本身**变红的条数
#   validator_level_red —— spec/doc 类：校验器判红，但未逐门隔离，只能证明校验器能红
#   dispatched        —— 实际跑了并给出判定的条数（不看红绿）
# 基线或 mutation 记录缺失 => 不可判定，不判满额（绝不默认 true）。
EV_RED = "independently_red_observed"
EV_VALIDATOR_ONLY = "validator_level_red_gate_not_isolated"
EV_NONE = "no_independent_evidence"
EV_PENDING = "pending_wave"
EV_UNKNOWN = "undeterminable_missing_evidence"


def load_mutation_log():
    """实跑的 mutation 记录。缺失返回 None（调用方据此判不可判定）。"""
    mp = os.path.join(EVID, "mutations.json")
    if not os.path.exists(mp):
        return None
    return {r["mut"]: r for r in jload(mp)["results"]}


def independent_evidence(gate, mlog):
    """只读 mutation 实跑记录，不读本门是否绿。"""
    dispatched = gate_red = validator_red = undetermined = 0
    for mid in (gate.get("mutations") or []):
        r = mlog.get(mid)
        if r is None or not r.get("detected"):
            undetermined += 1
            continue
        dispatched += 1
        cov = r.get("targets_covered")
        if cov is None:                       # spec / doc：校验器判红，未逐门隔离
            validator_red += 1
        elif cov.get(gate["gate_id"]) is True:
            gate_red += 1
    return dict(registered=len(gate.get("mutations") or []), dispatched=dispatched,
                gate_level_red=gate_red, validator_level_red=validator_red,
                undetermined=undetermined)


def self_check(entries, ev_ok):
    """反激励自检：让本机制自己暴露自己。

    判据：满额（counts_as_pass）与「实测能否判红」必须一一对应。
    n_full_marks_without_gate_level_red 恒须为 0 —— 这条断言本身可被证伪：
    只要有人把 counts_as_pass 改回由本门自身绿度导出，这个数就会离开 0。
    n_hole_under_retired_rule 给出被退役口径的反向激励洞有多大（恒真门拿满额、
    只有校验器级证据的门也拿满额），改口径前后两数之差即本次整改的实际收益。
    """
    full = [e for e in entries if e["counts_as_pass"]]
    leaked = [e["gate_id"] for e in full
              if e["independent_evidence"]["gate_level_red"] < 1]
    hole = [e["gate_id"] for e in entries
            if e["evidence_state"] != EV_PENDING
            and e["independent_evidence"]["gate_level_red"] < 1]
    by_state = collections.Counter(e["evidence_state"] for e in entries)
    return {
        "rule": "满额要求实测观测到本门自身变红；恒真门拿不到满额证据",
        "evidence_available": ev_ok,
        "n_gates": len(entries),
        "n_counts_as_pass": len(full),
        "n_pending": sum(1 for e in entries if e["evidence_state"] == EV_PENDING),
        "n_full_marks_without_gate_level_red": len(leaked),
        "gates_full_marks_without_gate_level_red": leaked,
        "by_evidence_state": dict(by_state),
        "n_hole_under_retired_rule": len(hole),
        "gates_hole_under_retired_rule": hole,
        "retired_rule_note": ("退役口径把「通过的 check 计数」当验收证据量，而该计数由本门"
                             "是否绿导出：恒真门恒拿满额，判红门反被扣证据。"
                             "hole 即该反向激励洞的门数。"),
    }


def build_case_ledger(qm):
    op = os.path.join(EVID, "oracle_baseline.json")
    baseline_ok = os.path.exists(op)
    # executed_cases 只数「实际调度并给出判定的 check」，**不看红绿**：
    # 判红的 check 同样是跑过的用例，扣它的证据同样是反向激励。
    res = jload(op)["checks"] if baseline_ok else []
    cnt = collections.Counter()
    for r in res:
        for g in r["gates"]:
            cnt[g] += 1

    mlog = load_mutation_log()
    ev_ok = baseline_ok and mlog is not None
    entries = []
    for g in qm["gates"]:
        gid = g["gate_id"]
        req = g["zero_case_red"]["min_cases"]

        # ---- 独立证据（与本门是否绿无关）----
        if ev_ok:
            ev = independent_evidence(g, mlog)
            if ev["gate_level_red"] >= 1:
                state = EV_RED
            elif ev["validator_level_red"] >= 1:
                state = EV_VALIDATOR_ONLY
            else:
                state = EV_NONE
        else:
            ev = dict(registered=len(g.get("mutations") or []), dispatched=0,
                      gate_level_red=0, validator_level_red=0, undetermined=0)
            state = EV_UNKNOWN

        # ---- 用例排期：登记/声明口径，不参与 PASS 判定 ----
        if gid in PENDING:
            owner, sched = PENDING[gid]
            ex, impl = 0, max(req, sched)
            note = "设计期定义，执行待对应 Wave；不计 PASS"
            state = EV_PENDING
        elif gid in P0_EXEC:
            owner, ex, impl = g["owner"], P0_EXEC[gid], P0_EXEC[gid]
            note = "结构校验 + mutation 覆盖计数"
        elif gid == "G-RD-04":
            owner, ex, impl = g["owner"], 1, 1
            note = "结构规则 V-RD-EXPECTED + V-ORACLE-MUSTNOT 计数"
        elif gid == "G-RD-03":
            owner, ex, impl = g["owner"], 8, 8
            note = "testdata/index.json v1.2 八个数据集逐一存在性"
        else:
            owner, ex, impl = g["owner"], cnt.get(gid, 0), cnt.get(gid, 0)
            note = "qa_oracle 已调度 check 计数（判红不扣证据）"

        # 满额 = 独立证据实测观测到本门自身变红。pending 与无证据一律不判满额。
        # 放在排期分支之后：pending 分支会把 state 改写为 EV_PENDING。
        counts_as_pass = state == EV_RED

        entries.append(dict(gate_id=gid, required_cases=req, executed_cases=ex,
                            skipped_cases=0, pends=gid in PENDING,
                            counts_as_pass=counts_as_pass, evidence_state=state,
                            independent_evidence=ev,
                            scheduled_wave=g["wave"], implementation_cases_scheduled=impl,
                            owner=owner, note=note))

    led = {"schema": "astrocs.v6.qa-matrix.case-ledger/v1", "task": "QA-MATRIX-001",
           "zero_case_red_rule": qm["zero_case_red"]["runner_rule"],
           "pass_rule": "counts_as_pass := 独立证据中 gate_level_red >= 1（实测观测到本门自身变红）",
           "entries": entries,
           "self_check": self_check(entries, ev_ok)}
    with open(os.path.join(ROOT, "case_ledger.json"), "w", encoding="utf-8") as f:
        json.dump(led, f, ensure_ascii=False, indent=1); f.write("\n")
    return led


def main(argv):
    os.makedirs(LOGS, exist_ok=True)
    steps = []
    def step(name, cmd, log):
        rc = run(cmd, log); steps.append((name, rc)); return rc

    rc1 = step("assemble", [sys.executable, "assemble.py"], "01_assemble.log")
    rc2 = step("oracle_baseline", [sys.executable, "qa_oracle.py", "run", "--json",
                                   os.path.join(EVID, "oracle_baseline.json")], "02_oracle.log")
    qm = jload(os.path.join(ROOT, "qa_matrix.json"))
    led = build_case_ledger(qm)
    rc3 = step("validate_spec", [sys.executable, "validate_spec.py"], "03_validate_spec.log")
    rc4 = step("render_docs", [sys.executable, "render_docs.py", "--write"], "04_render.log")
    rc5 = step("check_docs", [sys.executable, "check_docs.py"], "05_check_docs.log")
    with open(os.path.join(LOGS, "06_case_ledger.log"), "w", encoding="utf-8") as f:
        f.write(json.dumps({"n_entries": len(led["entries"]),
                            "pending": [e["gate_id"] for e in led["entries"] if e["pends"]],
                            "zero_exec": [e["gate_id"] for e in led["entries"] if e["executed_cases"] == 0],
                            "self_check": led["self_check"]},
                           ensure_ascii=False, indent=1))
    rc6 = step("validate_spec_with_ledger", [sys.executable, "validate_spec.py",
                                             "--ledger", os.path.join(ROOT, "case_ledger.json")],
               "07_validate_ledger.log")
    rc7 = step("mutations", [sys.executable, "run_mutations.py", "--json",
                             os.path.join(EVID, "mutations.json")], "08_mutations.log")
    summary = dict(task="QA-MATRIX-001", steps=[{"name": n, "rc": r} for n, r in steps],
                   rc_total=0 if all(r == 0 for _, r in steps) else 1,
                   n_gates=len(qm["gates"]),
                   n_mutations=len(jload(os.path.join(ROOT, "data", "mutations.json"))["mutations"]),
                   ledger_self_check=led["self_check"])
    with open(os.path.join(EVID, "rc_summary.json"), "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=1)
    print(json.dumps(summary, ensure_ascii=False))
    return summary["rc_total"]


if __name__ == "__main__":
    sys.exit(main(sys.argv))
