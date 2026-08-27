#!/usr/bin/env python3
"""Round-2 mutation cases: fixed targets + full-token removal."""
import subprocess, os, sys, shutil, json, csv
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
REPO = "/home/lighthouse/Astro CS Database"
OUT = os.path.join(ROOT, "package", "06_checker_truthfulness")
WORK = os.path.join(ROOT, "builds", "checker_mutations2")
os.makedirs(WORK, exist_ok=True)

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
        last = pr.stdout.strip().split(chr(10))[-1]
        status = json.loads(last).get("status", "UNKNOWN")
    except Exception:
        pass
    return pr.returncode, status

def write_file(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)

results = []
def record(case, checker, expected, rc, st, note=""):
    results.append({"case": case, "checker": checker, "expected": expected,
                    "actual_exit": rc, "actual_status": st, "note": note})
    print(case + ": expected=" + expected + " actual_exit=" + str(rc) + " status=" + str(st) + " | " + note)

# Case A: exec - remove ALL critical(aio_read) tokens (positive)
d = fresh_stub("exec_remove_all_critical")
doc = os.path.join(d, "docs/architecture/EXECUTION_MODEL.md")
txt = open(doc, encoding="utf-8").read()
write_file(doc, txt.replace("critical(aio_read)", "critical(aio_read_X)"))
rc, st = run_checker("check_execution_contracts", d)
record("exec_remove_all_critical_tokens", "check_execution_contracts", "FAIL(expected)", rc, st,
       "positive: after removing every occurrence checker flags EXEC-MISSING-CRITICAL")

# Case B: science units - replace ADU with second in DRIZZLE.md (a real SCI doc)
d = fresh_stub("sci_units_adu_to_sec2")
doc = os.path.join(d, "docs/science/DRIZZLE.md")
txt = open(doc, encoding="utf-8").read()
write_file(doc, txt.replace("ADU", "second", 3))
rc, st = run_checker("check_science_units", d)
record("sci_units_adu_to_second_drizzle", "check_science_units", "FAIL(expected dimensional check)", rc, st,
       "no dimensional analysis; only keyword occurrence counting -> false negative likely")

# Case C: science units - replace ADU with second in PHASE2_UPM.md
d = fresh_stub("sci_units_adu_to_sec3")
doc = os.path.join(d, "docs/science/PHASE2_UPM.md")
txt = open(doc, encoding="utf-8").read()
write_file(doc, txt.replace("ADU", "second", 3))
rc, st = run_checker("check_science_units", d)
record("sci_units_adu_to_second_phase2_upm", "check_science_units", "FAIL(expected dimensional check)", rc, st,
       "no dimensional analysis; keyword counts unchanged -> likely PASS")

# Case D: doc symbols - add nonexistent file + symbol to PHASE2_UPM.md
d = fresh_stub("doc_symbols_bad_ref2")
doc = os.path.join(d, "docs/science/PHASE2_UPM.md")
txt = open(doc, encoding="utf-8").read()
write_file(doc, txt + chr(10) + "Reference to nonexistent_xyz_file_12345.cpp and p2_nonexistent_symbol_zzz" + chr(10))
rc, st = run_checker("check_doc_symbols", d)
record("doc_symbols_nonexistent_file_symbol_upm", "check_doc_symbols", "FAIL(expected)", rc, st,
       "heuristic token checker; verbatim result recorded")

# Case E: config contracts - add unknown config key usage (probe)
d = fresh_stub("config_unknown_key")
cfg = os.path.join(d, "lib/orchestrator/configs/stage1_gc_panel1_Red.json")
if os.path.exists(cfg):
    txt = open(cfg, encoding="utf-8").read()
    txt2 = txt.replace("}", ",\"audit_zzz_unknown_key\": 123 }", 1)
    write_file(cfg, txt2)
    rc, st = run_checker("check_config_contracts", d)
    record("config_unknown_key_added", "check_config_contracts", "FAIL(expected)", rc, st, "verbatim")
else:
    record("config_unknown_key_added", "check_config_contracts", "FAIL", None, "SKIP", "config missing")

with open(os.path.join(OUT, "checker_truthfulness_mutation_results_round2.json"), "w") as f:
    json.dump(results, f, indent=2, ensure_ascii=False)
print("HARNESS2_DONE")
