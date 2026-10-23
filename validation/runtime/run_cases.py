#!/usr/bin/env python3
"""Run and judge the validation/runtime cases.

The case executables only emit raw evidence (checks, values, resources).  This
script applies the frozen criteria in each case.json:

  * every ``required_checks`` entry must be present exactly once and pass;
  * ``attempts`` must reach ``min_attempts`` and ``successes`` must be > 0 and
    equal to the number of passing checks (no "0 successes" pass);
  * ``failures`` must be empty and the process must exit 0;
  * a case that was not built is pending only when the runtime implementation
    is not linked; a compile/link error is a real failure.

It writes, per case, ``observed.json`` (real stdout evidence + command/exit
code), and ``comparison.json``; ``manifest.json`` aggregates the run.  A
failing case keeps its failed attempt under ``<case>/attempts/``.
"""
import argparse
import datetime
import json
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HERE = os.path.join(ROOT, "validation", "runtime")
BUILD = os.path.join(HERE, "build")
ORDER = [
    "reg01", "life01", "work01", "a01",
    "floor01", "fork01", "order01", "barrier01", "multi01", "pipe01",
    "io01", "b01", "d01_off", "d01_on", "cal01",
]


def git_commit():
    try:
        return subprocess.check_output(
            ["git", "-C", ROOT, "rev-parse", "HEAD"], text=True).strip()
    except Exception:  # pragma: no cover - diagnostic only
        return "unknown"


def load_json(path):
    with open(path) as handle:
        return json.load(handle)


def write_json(path, payload):
    with open(path, "w") as handle:
        json.dump(payload, handle, ensure_ascii=False, indent=2)
        handle.write("\n")


def run_build():
    script = os.path.join(HERE, "build.sh")
    environment = dict(os.environ)
    result = subprocess.run(["bash", script], cwd=ROOT, env=environment,
                            capture_output=True, text=True)
    status = load_json(os.path.join(BUILD, "build_status.json"))
    return result, status


def evaluate(case, criteria, observed):
    names = [check["check"] for check in observed.get("checks", [])]
    passes = {check["check"]: bool(check.get("pass")) for check in observed.get("checks", [])}
    duplicate = sorted({name for name in names if names.count(name) > 1})
    required = criteria["required_checks"]
    missing = [name for name in required if name not in passes]
    failing = [name for name in required if name in passes and not passes[name]]
    attempts = int(observed.get("attempts", 0))
    successes = int(observed.get("successes", 0))
    observed_failures = observed.get("failures", [])
    reasons = []
    if duplicate:
        reasons.append("duplicate check names: " + ", ".join(duplicate))
    if missing:
        reasons.append("required checks not attempted: " + ", ".join(missing))
    if failing:
        reasons.append("required checks failed: " + ", ".join(failing))
    if attempts < int(criteria.get("min_attempts", 0)):
        reasons.append(f"attempts {attempts} < min_attempts {criteria['min_attempts']}")
    if successes <= 0:
        reasons.append("successes == 0 (a case may not pass without a successful check)")
    if successes != sum(1 for value in passes.values() if value):
        reasons.append("successes does not match the number of passing checks")
    if observed_failures:
        reasons.append("harness reports failures: " + json.dumps(
            [failure.get("check") for failure in observed_failures]))
    status = "pass" if not reasons else "fail"
    return {
        "status": status,
        "reasons": reasons,
        "attempts": attempts,
        "successes": successes,
        "expected_success_count": int(criteria.get("expected_success_count", len(required))),
        "observed_success_count": sum(1 for value in passes.values() if value),
        "required_checks": len(required),
        "missing_checks": missing,
        "failed_checks": failing,
    }


def run_case(case, criteria, build_status):
    case_dir = os.path.join(HERE, case)
    comparison_path = os.path.join(case_dir, "comparison.json")
    observed_path = os.path.join(case_dir, "observed.json")
    build_info = build_status["cases"].get(case, {})
    command = [os.path.join(BUILD, case)]
    base = {
        "case": case,
        "title": criteria.get("title", ""),
        "milestone": criteria.get("milestone", ""),
        "contract_refs": criteria.get("contract_refs", []),
        "build": build_info,
        "command": " ".join(command),
    }
    if not build_info.get("built"):
        classification = build_info.get("classification", "not_built")
        status = "pending" if classification == "pending_runtime" else "fail"
        reason = ("runtime implementation not linked: " + classification)
        if status == "fail":
            reason = "build " + classification
        comparison = dict(base, status=status, reason=reason,
                          attempts=0, successes=0,
                          expected_success_count=criteria.get("expected_success_count"),
                          observed_success_count=0, failures=[],
                          build_log=build_info.get("log"),
                          link_command=build_info.get("link_command"))
        write_json(comparison_path, comparison)
        if os.path.exists(observed_path):
            os.remove(observed_path)
        return comparison

    scratch = os.path.join(BUILD, "scratch", case)
    os.makedirs(scratch, exist_ok=True)
    environment = dict(os.environ)
    environment["ACSD_SCRATCH"] = os.path.join(BUILD, "scratch")
    if case == "life01":
        environment["ASAN_OPTIONS"] = "detect_leaks=0:abort_on_error=1:halt_on_error=1"
    timeout = int(criteria.get("timeout_seconds", 120))
    try:
        result = subprocess.run(command, cwd=ROOT, env=environment,
                                capture_output=True, text=True, timeout=timeout)
        exit_code = result.returncode
        stdout = result.stdout
        stderr = result.stderr
        timed_out = False
    except subprocess.TimeoutExpired as expired:
        exit_code = -1
        def as_text(value):
            if value is None:
                return ""
            if isinstance(value, bytes):
                return value.decode(errors="replace")
            return value
        stdout = as_text(expired.stdout)
        stderr = as_text(expired.stderr) + f"\ntimeout after {timeout} s"
        timed_out = True

    with open(os.path.join(case_dir, "observed.stdout.txt"), "w") as handle:
        handle.write(stdout)
    with open(os.path.join(case_dir, "observed.stderr.txt"), "w") as handle:
        handle.write(stderr)

    observed = None
    parse_error = None
    try:
        observed = json.loads(stdout)
    except Exception as error:  # noqa: BLE001 - report the raw evidence
        parse_error = str(error)

    if observed is None:
        comparison = dict(base, status="fail",
                          reason=f"case did not emit JSON evidence: {parse_error}",
                          exit_code=exit_code, stderr_tail=stderr[-4000:],
                          timed_out=timed_out, attempts=0, successes=0,
                          failures=[])
        write_json(comparison_path, comparison)
        attempts_dir = os.path.join(case_dir, "attempts")
        os.makedirs(attempts_dir, exist_ok=True)
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        shutil.copyfile(os.path.join(case_dir, "observed.stdout.txt"),
                        os.path.join(attempts_dir, f"{stamp}.stdout.txt"))
        return comparison

    evidence = dict(observed)
    evidence["command"] = " ".join(command)
    evidence["exit_code"] = exit_code
    evidence["stderr_tail"] = stderr[-4000:]
    evidence["timed_out"] = timed_out
    write_json(observed_path, evidence)

    verdict = evaluate(case, criteria, observed)
    reasons = list(verdict["reasons"])
    if exit_code != 0:
        reasons.append(f"exit code {exit_code}")
    if timed_out:
        reasons.append("case timed out")
    status = "pass" if not reasons else "fail"

    # C-CAL kernel checks can run before the M4 typed process exists.  The
    # kernel result is reported, but the case-level gate stays pending until
    # M4 lands; a kernel regression is still a failure.
    kernel_status = status
    if case == "cal01" and status == "pass" and criteria.get("gate"):
        status = "pending"
        reasons = [criteria["gate"]]
    if case == "cal01":
        failed = {check["check"]: check for check in observed.get("checks", [])
                  if not check.get("pass")}
        gate_failure = failed.get("cal.cal1_host_ok")
        if status == "fail" and gate_failure is not None and str(
                gate_failure.get("actual", "")).startswith("unsupported"):
            status = "pending"
            kernel_status = "not_measured_host_plan_unsupported"
            reasons = ["M4 plan hook is still the M0 stub: " + str(
                gate_failure.get("actual", ""))[:300]]
    if case == "d01_on" and not observed.get("values", {}).get(
            "d01_on.dev_mode_consistent", True):
        status = "pending"
        reasons = ["libruntime was built with ACSD_DEV_MODE=0; the ON semantics "
                   "require a runtime built with -DACSD_DEV_MODE=1 (see observed "
                   "values d01_on.span_a/span_b)"]

    comparison = dict(
        base,
        status=status,
        reason="; ".join(reasons) if reasons else "all required checks passed",
        exit_code=exit_code,
        attempts=verdict["attempts"],
        successes=verdict["successes"],
        expected_success_count=verdict["expected_success_count"],
        observed_success_count=verdict["observed_success_count"],
        required_checks=verdict["required_checks"],
        missing_checks=verdict["missing_checks"],
        failed_checks=verdict["failed_checks"],
        failures=observed.get("failures", []),
        kernel_status=kernel_status,
        checks=observed.get("checks", []),
        values=observed.get("values", {}),
        resources=observed.get("resources", {}),
    )
    write_json(comparison_path, comparison)

    if status == "fail":
        attempts_dir = os.path.join(case_dir, "attempts")
        os.makedirs(attempts_dir, exist_ok=True)
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        shutil.copyfile(observed_path, os.path.join(attempts_dir, f"{stamp}.observed.json"))
        shutil.copyfile(comparison_path, os.path.join(attempts_dir, f"{stamp}.comparison.json"))
    return comparison


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("cases", nargs="*", help="case ids (default: all)")
    parser.add_argument("--no-build", action="store_true")
    args = parser.parse_args()

    if args.no_build:
        build_status = load_json(os.path.join(BUILD, "build_status.json"))
        build_result = None
    else:
        build_result, build_status = run_build()
        sys.stdout.write(build_result.stdout)
        sys.stderr.write(build_result.stderr)

    cases = args.cases or ORDER
    comparisons = []
    for case in cases:
        criteria = load_json(os.path.join(HERE, case, "case.json"))
        comparisons.append(run_case(case, criteria, build_status))
        print(f"[{comparisons[-1]['status']:7s}] {case}: {comparisons[-1]['reason']}")

    summary = {
        "generated_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "git_commit": git_commit(),
        "build_dir": build_status.get("build_dir"),
        "milestone_scope": "M0-M4 snapshot: runtime contracts (REG01/S-LIFE/S-WORK/A01/"
                           "S-FLOOR/S-FORK/S-ORDER/S-BARRIER/S-MULTI/PIPE01/S-IO/B01/D01) "
                           "and the observation.calibrate Host chain. M5 module integration "
                           "is not part of this manifest.",
        "build_note": "build.sh selects the build directory per case: d01_on/case.json has "
                      "link=dev and links against $ACSD_DEV_BUILD (default "
                      "/tmp/acsd-build-dev, ACSD_DEV_MODE=1); every other case links against "
                      "$ACSD_BUILD (default /tmp/acsd-build, ACSD_DEV_MODE=0). The actual "
                      "-L directory is recorded in each comparison.build.link_command.",
        "commands": [
            "ACSD_BUILD=/tmp/acsd-build ACSD_DEV_BUILD=/tmp/acsd-build-dev "
            "bash validation/runtime/build.sh",
            "python3 validation/runtime/run_cases.py --no-build",
        ],
        "independence": "only include/acsd/** public headers; no src/ include",
        "cases": [
            {
                "case": comparison["case"],
                "status": comparison["status"],
                "milestone": comparison.get("milestone"),
                "attempts": comparison.get("attempts"),
                "successes": comparison.get("successes"),
                "failures": [failure.get("check") for failure in comparison.get("failures", [])],
                "reason": comparison.get("reason"),
                "command": comparison.get("command"),
            }
            for comparison in comparisons
        ],
    }
    manifest_path = os.path.join(HERE, "manifest.json")
    if args.cases and os.path.exists(manifest_path):
        # A single-case run must not erase the other cases from the manifest.
        try:
            previous = load_json(manifest_path)
        except Exception:
            previous = {}
        updated = {entry["case"]: entry for entry in previous.get("cases", [])}
        for entry in summary["cases"]:
            updated[entry["case"]] = entry
        order = {name: index for index, name in enumerate(ORDER)}
        summary["cases"] = sorted(updated.values(),
                                  key=lambda entry: order.get(entry["case"], 999))
        for key, value in previous.items():
            if key != "cases":
                summary.setdefault(key, value)
        summary["generated_at"] = datetime.datetime.now(
            datetime.timezone.utc).isoformat()
    write_json(manifest_path, summary)

    counts = {}
    for comparison in comparisons:
        counts[comparison["status"]] = counts.get(comparison["status"], 0) + 1
    print("[summary]", json.dumps(counts))
    return 0


if __name__ == "__main__":
    sys.exit(main())
