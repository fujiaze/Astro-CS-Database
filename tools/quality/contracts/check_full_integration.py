#!/usr/bin/env python3
"""check_full_integration.py — T411 full integration checker

Checks: 全生产运行；waivers []；P0/P1=0 (pending T500 for T407)
Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error
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
    cmd = [sys.executable, str(repo / "tools/quality/contracts/generate_contract_report.py"),
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
    # Check waivers
    waivers = repo / "waivers.json"
    if waivers.exists():
        try:
            w=json.loads(waivers.read_text(encoding="utf-8"))
            if w != []:
                findings.append({"id":"INTEG-WAIVERS-NONEMPTY","severity":"P1","observed":f"waivers {w}","expected":"[]"})
        except Exception as exc:  # noqa: BLE001
            # W4-A3：原为裸 `except: pass` —— waivers.json 坏掉时静默放行（"豁免面被移空"
            # 正是要防的失效型）。现显式登记为 P1 finding。
            findings.append({"id":"INTEG-WAIVERS-BAD-JSON","severity":"P1",
                             "observed":f"waivers.json 不可解析: {exc}","expected":"[] 或合法 JSON"})
    # Check P0/P1: 豁免必须条件化, 否则 DELIVERED 永远可达。
    # 豁免白名单口径(来自 T411/T500 上下文): 仅 T407 的 FORBID-HARDCODE-THREADS
    # (hardcoded num_threads(16)) 属挂账债务 pending T500, 额度上限 10 条;
    # T407 的任何其他 finding(如 FORBID-ABS-PATH / FORBID-DETACH)不可豁免。
    HARDCODE_THREAD_DEBT_CAP = 10
    failing = [r for r in data.get("results",[]) if not r.get("passed")]
    debt_count = 0
    if failing:
        for f in failing:
            tool = f["tool"]
            if tool == "check_forbidden_patterns":
                # generate_contract_report 的汇总不含明细; 直接取 T407 输出以判定豁免范围
                fp_findings = []
                try:
                    fp_out = subprocess.run([sys.executable, str(repo / "tools/quality/contracts/check_forbidden_patterns.py"), "--repo", str(repo)], capture_output=True, text=True, timeout=60)
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
                    findings.append({"id":"INTEG-P1-FAIL","severity":"P1","symbol":tool,"observed":f"FORBID-HARDCODE-THREADS count {debt_count} > waiver cap {HARDCODE_THREAD_DEBT_CAP}","expected":f"<={HARDCODE_THREAD_DEBT_CAP} pending T500"})
                    debt_count = HARDCODE_THREAD_DEBT_CAP  # 超额部分不再享受豁免
                if debt_count > 0:
                    findings.append({"id":"INTEG-P1-DEBT","severity":"P1","symbol":tool,"observed":f"{debt_count} hardcoded threads findings pending T500 (waiver scope: FORBID-HARDCODE-THREADS only, cap {HARDCODE_THREAD_DEBT_CAP})","expected":"P1=0 after T500"})
            else:
                findings.append({"id":"INTEG-P1-FAIL","severity":"P1","symbol":f["tool"],"observed":"checker FAIL","expected":"PASS"})
    status = "PASS" if not [f for f in findings if f["severity"]=="P1" and f["id"]!="INTEG-P1-DEBT"] else "FAIL"
    # DELIVERED 仅当全部 P1 finding 都是白名单内的挂账债务 (且数量在额度内);
    # 其他任何失败(其他工具 FAIL / T407 其他 id)都保持 FAIL, 豁免不适用。
    if findings and all(f["id"]=="INTEG-P1-DEBT" for f in findings) and debt_count <= HARDCODE_THREAD_DEBT_CAP:
        status="DELIVERED"
    result = {"tool":"check_full_integration","status":status,"findings":findings,"passed": status in ("PASS","DELIVERED"),"report":data}
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
    # For T411, return 0 even if DELIVERED (checker exists)
    return 0 if status in ("PASS","DELIVERED") else 1

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
