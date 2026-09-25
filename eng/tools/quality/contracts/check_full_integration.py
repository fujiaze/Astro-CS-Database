#!/usr/bin/env python3
"""check_full_integration.py — T411 full integration checker

Checks: 全生产运行；豁免面为空（正本 = eng/ci/exemptions.json）；P0/P1=0。
Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error

TRUTHFUL-CONCLUSION-01（一页纸 S2-B）订正两处「说谎的结论」：
  1. 原 status 有一个**造词**取值（ASTROCS_DESIGN.md §12.5 唯一状态阶梯里不存在该词），
     其语义是「存在挂账 P1 债务也算通过」。现改为只输出
     PASS/FAIL 判定词（与文件头退出码合同同源），债务另立 debt 字段如实登记。
  2. 原豁免面读一个**全仓不存在的文件名**（悬空引用），于是「豁免必须为空」
     这条断言在真实仓库里从未被执行过。现读真实载体
     eng/ci/exemptions.json；文件缺失或不可解析一律判红（fail-closed）。
"""
import argparse, json, pathlib, sys, subprocess

# W4-A3（B 类收口）：报告生成器的子进程超时此前**未捕获** —— subprocess.run(timeout=30)
# 抛出 TimeoutExpired 时本脚本以未捕获 Traceback 退出（exit 1 + 30 行栈），既不是
# 合同 FAIL 也不是环境错，违反 ENGINEERING_SPEC §8「fail-closed 且不 traceback」。
# 现改为：① 默认预算放宽到 300s（30s 对全仓合同报告不足，实测超时）；
# ② 超时/无法启动一律转成具名 finding（INTEG-REPORT-TIMEOUT / INTEG-REPORT-ERROR）
#    并以 status=FAIL 干净退出；③ __main__ 兜底把任何意外异常转成 exit 2 单行说明。
DEFAULT_REPORT_TIMEOUT_S = 300


def _run_report(repo, tf, timeout_s):
    """跑 generate_contract_report；返回 (findings, note)。绝不抛异常。"""
    cmd = [sys.executable, str(repo / "eng/tools/quality/contracts/generate_contract_report.py"),
           "--repo", str(repo), "--out-json", str(tf)]
    try:
        subprocess.run(cmd, capture_output=True, text=True, timeout=timeout_s)
    except subprocess.TimeoutExpired:
        return [{"id": "INTEG-REPORT-TIMEOUT", "severity": "P1",
                 "symbol": "generate_contract_report",
                 "observed": "子进程 %ds 未结束（已终止）" % timeout_s,
                 "expected": "在 --report-timeout 内产出 report JSON"}], \
               "报告生成超时 %ds" % timeout_s
    except OSError as exc:
        return [{"id": "INTEG-REPORT-ERROR", "severity": "P1",
                 "symbol": "generate_contract_report",
                 "observed": "无法启动: %s" % exc, "expected": "可执行"}], \
               "报告生成无法启动"
    return [], None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    ap.add_argument("--report-timeout", type=int, default=DEFAULT_REPORT_TIMEOUT_S,
                    help="generate_contract_report 子进程预算秒数（默认 %d）"
                         % DEFAULT_REPORT_TIMEOUT_S)
    args = ap.parse_args()
    repo = pathlib.Path(args.repo)
    findings = []
    # Run generate_contract_report to get overall
    import tempfile
    tf = pathlib.Path(tempfile.mktemp(suffix=".json"))
    rep_findings, rep_note = _run_report(repo, tf, args.report_timeout)
    findings.extend(rep_findings)
    try:
        data = json.loads(tf.read_text(encoding="utf-8"))
        tf.unlink(missing_ok=True)
    except Exception as e:
        findings.append({"id":"INTEG-BAD-REPORT","detail":str(e),"severity":"P1","observed":"report parse fail","expected":"valid JSON"})
        data={"status":"FAIL","results":[]}
    # Check 豁免面：唯一正本 = eng/ci/exemptions.json（docs/ci/01_CHECKS.md §1）。
    # TRUTHFUL-CONCLUSION-01：原实现读一个全仓不存在的文件名 ⇒ 该断言从未执行过。
    # 悬空引用必须判红，且缺失/坏 JSON 一律 fail-closed（不静默放行）。
    exemptions_rel = "eng/ci/exemptions.json"
    exemptions = repo / exemptions_rel
    registered_debt_ids = set()
    if not exemptions.exists():
        findings.append({"id":"INTEG-EXEMPTIONS-MISSING","severity":"P1",
                         "observed":f"{exemptions_rel} 不存在","expected":"豁免面显式登记且可解析"})
    else:
        try:
            ex = json.loads(exemptions.read_text(encoding="utf-8"))
            entries = ex.get("exemptions", ex if isinstance(ex, list) else [])
            if not isinstance(entries, list):
                raise ValueError("exemptions 不是列表")
            for ent in entries:
                if isinstance(ent, dict):
                    for key in ("check", "id", "finding", "check_id"):
                        if isinstance(ent.get(key), str):
                            registered_debt_ids.add(ent[key])
            hw = ex.get("high_water", {}) if isinstance(ex, dict) else {}
            cap_hw = hw.get("max_entries")
            if isinstance(cap_hw, int) and len(entries) > cap_hw:
                findings.append({"id":"INTEG-EXEMPTIONS-OVER-HIGH-WATER","severity":"P1",
                                 "observed":f"豁免条目 {len(entries)} > high_water {cap_hw}",
                                 "expected":"只减不增（超水即红）"})
        except Exception as exc:  # noqa: BLE001
            findings.append({"id":"INTEG-EXEMPTIONS-BAD-JSON","severity":"P1",
                             "observed":f"{exemptions_rel} 不可解析: {exc}",
                             "expected":"合法 JSON（fail-closed）"})
    # Check P0/P1: 豁免必须条件化, 否则「有债务也算通过」永远可达。
    # 豁免白名单口径(来自 T411/T500 上下文): 仅 T407 的 FORBID-HARDCODE-THREADS
    # (hardcoded num_threads(16)) 属挂账债务 pending T500, 额度上限 10 条;
    # T407 的任何其他 finding(如 FORBID-ABS-PATH / FORBID-DETACH)不可豁免。
    # 债务额度不再由本文件写死（原为常量 10，且超额时把 debt_count 截回上限 ⇒
    # 「有债务也算通过」恒真）。额度唯一来源 = 豁免面 eng/ci/exemptions.json 的登记条目：
    # 只有显式登记的 finding id 才计入可豁免额度，未登记者额度为 0。
    HARDCODE_THREAD_DEBT_CAP = (1 if "INTEG-P1-DEBT" in registered_debt_ids
                                or "FORBID-HARDCODE-THREADS" in registered_debt_ids else 0)
    failing = [r for r in data.get("results",[]) if not r.get("passed")]
    debt_count = 0
    if failing:
        for f in failing:
            tool = f["tool"]
            if tool == "check_forbidden_patterns":
                # generate_contract_report 的汇总不含明细; 直接取 T407 输出以判定豁免范围
                fp_findings = []
                try:
                    fp_out = subprocess.run([sys.executable, str(repo / "eng/tools/quality/contracts/check_forbidden_patterns.py"), "--repo", str(repo)], capture_output=True, text=True, timeout=60)
                    fp_data = json.loads(fp_out.stdout.strip().split("\n")[-1])
                    fp_findings = fp_data.get("findings", [])
                except Exception as e:
                    findings.append({"id":"INTEG-P1-FAIL","severity":"P1","symbol":tool,"observed":f"cannot obtain T407 findings: {e}","expected":"parsable findings"})
                    continue
                hw = [x for x in fp_findings if x.get("id")=="FORBID-HARDCODE-THREADS"]
                others = [x for x in fp_findings if x.get("id")!="FORBID-HARDCODE-THREADS"]
                debt_count = len(hw)
                for x in others:
                    findings.append({"id":"INTEG-P1-FAIL","severity":"P1","symbol":tool,"observed":f"{x.get('id')} {x.get('file','')}","expected":"PASS"})
                if debt_count > HARDCODE_THREAD_DEBT_CAP:
                    findings.append({"id":"INTEG-P1-DEBT-UNAUTHORIZED","severity":"P1","symbol":tool,
                                     "observed":f"FORBID-HARDCODE-THREADS count {debt_count} > 豁免面登记额度 {HARDCODE_THREAD_DEBT_CAP}",
                                     "expected":f"<={HARDCODE_THREAD_DEBT_CAP}（额度唯一来源 = {exemptions_rel}）"})
                if debt_count > 0:
                    findings.append({"id":"INTEG-P1-DEBT","severity":"P1","symbol":tool,"observed":f"{debt_count} hardcoded threads findings pending T500 (waiver scope: FORBID-HARDCODE-THREADS only, cap {HARDCODE_THREAD_DEBT_CAP})","expected":"P1=0 after T500"})
            else:
                findings.append({"id":"INTEG-P1-FAIL","severity":"P1","symbol":f["tool"],"observed":"checker FAIL","expected":"PASS"})
    # 判定词只取退出码合同里的 PASS/FAIL（§12.5 状态阶梯是另一个面，本文件不产状态词）。
    # 挂账债务如实登记在 debt 字段；只要存在任何 P1 finding 就是 FAIL。
    status = "PASS" if not [f for f in findings if f["severity"]=="P1"] else "FAIL"
    result = {"tool":"check_full_integration","status":status,"findings":findings,
              "passed": status == "PASS",
              "debt":{"p1_debt_findings":debt_count,
                      "exemption_surface":exemptions_rel,
                      "registered_debt_cap":HARDCODE_THREAD_DEBT_CAP,
                      "registered_ids":sorted(registered_debt_ids)},
              "report":data}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(result, indent=2, ensure_ascii=False, sort_keys=True), encoding="utf-8")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False, sort_keys=True))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0","P1") and f["id"]!="INTEG-P1-DEBT"])
        junit = f'<testsuite name="check_full_integration" tests="1" failures="{failures}"><testcase classname="integration" name="full"/></testsuite>'
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    return 0 if status == "PASS" else 1

if __name__ == "__main__":
    # W4-A3：兜底 —— 任何意外异常转成 exit 2 + 单行说明，绝不打印 Traceback。
    try:
        sys.exit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print("INTEGRATION_CHECK_ERROR: 未捕获异常 %r（fail-closed，exit 2）" % (exc,),
              file=sys.stderr)
        sys.exit(2)
