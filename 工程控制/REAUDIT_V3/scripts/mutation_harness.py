#!/usr/bin/env python3
"""Checker truthfulness mutation harness (out-of-repo only)."""
import subprocess, os, sys, shutil, json, csv

ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
REPO = "/home/lighthouse/Astro CS Database"
OUT = os.path.join(ROOT, "package", "06_checker_truthfulness")
WORK = os.path.join(ROOT, "builds", "checker_mutations")
os.makedirs(WORK, exist_ok=True)
os.makedirs(OUT, exist_ok=True)

def fresh_stub(name):
    d = os.path.join(WORK, name)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    p = subprocess.Popen(["git", "-C", REPO, "archive", "HEAD"], stdout=subprocess.PIPE)
    subprocess.run(["tar", "-x", "-C", d], stdin=p.stdout, check=True)
    p.stdout.close(); p.wait()
    return d

def run_checker(checker, repo):
    cmd = [sys.executable, os.path.join(REPO, "tools/quality/contracts", checker + ".py"), "--repo", repo]
    pr = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    status = "UNKNOWN"
    try:
        if pr.stdout.strip():
            status = json.loads(pr.stdout.strip().split(chr(10))[-1]).get("status", "UNKNOWN")
    except Exception:
        pass
    return pr.returncode, status, pr.stdout, pr.stderr

def write_file(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)

results = []
def record(case, checker, expected, actual_rc, actual_status, note=""):
    results.append({"case": case, "checker": checker, "expected": expected,
                    "actual_exit": actual_rc, "actual_status": actual_status, "note": note})
    print(case + ": expected=" + expected + " actual_exit=" + str(actual_rc) + " status=" + str(actual_status) + " | " + note)

# Case 1: check_api_contracts - param order swap inside a signature
d = fresh_stub("api_order_swap")
csv_path = os.path.join(d, "docs/contracts/API_CONTRACTS.csv")
rows = list(csv.DictReader(open(csv_path, encoding="utf-8")))
target = None
for r in rows:
    s = r.get("full_signature", "")
    if "(" in s and "," in s and "void" not in s and r.get("symbol", "").strip():
        target = r; break
mutated = list(rows)
if target:
    i = rows.index(target)
    sig = target["full_signature"]
    lp = sig.index("("); rp = sig.rindex(")")
    inner = sig[lp+1:rp]
    parts = [p.strip() for p in inner.split(",")]
    if len(parts) >= 2:
        parts[0], parts[1] = parts[1], parts[0]
        new_sig = sig[:lp+1] + ", ".join(parts) + sig[rp:]
        nr = dict(target); nr["full_signature"] = new_sig
        mutated[i] = nr
with open(csv_path, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(mutated)
rc, st, so, se = run_checker("check_api_contracts", d)
record("api_param_order_swap", "check_api_contracts", "FAIL(expected to catch signature mismatch)", rc, st,
       "signature mismatch branch is a no-op (pass); cannot catch param order swap")

# Case 2: check_science_units - replace ADU with second once in a SCI doc
d = fresh_stub("sci_units_adu_to_sec")
doc = os.path.join(d, "docs/science", "UPM.md")
if os.path.exists(doc):
    txt = open(doc, encoding="utf-8").read()
    write_file(doc, txt.replace("ADU", "second", 1))
    rc, st, so, se = run_checker("check_science_units", d)
    record("sci_units_adu_to_second", "check_science_units", "FAIL(expected dimensional check)", rc, st,
           "no dimensional analysis; only keyword occurrence counting")
else:
    record("sci_units_adu_to_second", "check_science_units", "FAIL", None, "SKIP", "UPM.md missing")

# Case 3: check_execution_contracts - remove critical(aio_read) token from doc
d = fresh_stub("exec_remove_critical")
doc = os.path.join(d, "docs/architecture/EXECUTION_MODEL.md")
if os.path.exists(doc):
    txt = open(doc, encoding="utf-8").read()
    write_file(doc, txt.replace("critical(aio_read)", "critical(aio_read_X)", 1))
    rc, st, so, se = run_checker("check_execution_contracts", d)
    record("exec_remove_critical_token", "check_execution_contracts", "FAIL(expected)", rc, st,
           "positive: literal token IS required; but it is only a string search")
else:
    record("exec_remove_critical_token", "check_execution_contracts", "FAIL", None, "SKIP", "doc missing")

# Case 4: check_execution_contracts - source has no #pragma omp parallel anywhere, checker must FAIL if it truly verified parallelism
d = fresh_stub("exec_openmp_declared_no_pragma")
rc, st, so, se = run_checker("check_execution_contracts", d)
record("exec_openmp_declared_no_pragma_source", "check_execution_contracts", "FAIL(expected to verify real parallel region)", rc, st,
       "FALSE NEGATIVE: only greps for the #if guard + option + link; never verifies #pragma omp parallel nor that macro is defined for the target")

# Case 5: check_traceability - blank all algorithm_id
d = fresh_stub("trace_blank_alg_id")
tpath = os.path.join(d, "docs/TRACEABILITY.csv")
rows = list(csv.DictReader(open(tpath, encoding="utf-8")))
if rows and "algorithm_id" in rows[0]:
    for r in rows: r["algorithm_id"] = ""
    with open(tpath, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
    rc, st, so, se = run_checker("check_traceability", d)
    record("trace_blank_all_algorithm_id", "check_traceability", "FAIL(expected SCI->ALG chain)", rc, st,
           "never reads algorithm_id column; only ID uniqueness, authority doc exists, core keyword presence")
else:
    record("trace_blank_all_algorithm_id", "check_traceability", "FAIL", None, "SKIP", "no algorithm_id column")

# Case 6: check_traceability - remove UPM core keyword (positive)
d = fresh_stub("trace_remove_upm_keyword")
tpath = os.path.join(d, "docs/TRACEABILITY.csv")
rows = list(csv.DictReader(open(tpath, encoding="utf-8")))
for r in rows:
    r["requirement_id"] = r["requirement_id"].replace("UPM", "XXX")
with open(tpath, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
rc, st, so, se = run_checker("check_traceability", d)
record("trace_remove_upm_core_keyword", "check_traceability", "FAIL(expected)", rc, st,
       "positive: removal of core keyword from requirement IDs IS detected")

# Case 7: check_doc_symbols - add a nonexistent file + symbol reference
d = fresh_stub("doc_symbols_bad_ref")
doc = os.path.join(d, "docs/science", "UPM.md")
if os.path.exists(doc):
    txt = open(doc, encoding="utf-8").read()
    write_file(doc, txt + chr(10) + "Ref: nonexistent_xyz_file_12345.cpp and p2_nonexistent_symbol_zzz" + chr(10))
    rc, st, so, se = run_checker("check_doc_symbols", d)
    record("doc_symbols_nonexistent_file_symbol", "check_doc_symbols", "FAIL(expected)", rc, st,
           "heuristic token checker; verbatim result recorded")
else:
    record("doc_symbols_nonexistent_file_symbol", "check_doc_symbols", "FAIL", None, "SKIP", "UPM.md missing")

# Case 8: check_full_integration static inspection
record("full_integration_delivered_with_p1", "check_full_integration", "FAIL(if P1 debt present)", None, "STATIC",
       "SOURCE: DELIVERED counts as passed; passed=status in (PASS, DELIVERED); returns 0 for DELIVERED -> P1 debt allowed through")

with open(os.path.join(OUT, "checker_truthfulness_mutation_results.json"), "w") as f:
    json.dump(results, f, indent=2, ensure_ascii=False)
print("HARNESS_DONE")
