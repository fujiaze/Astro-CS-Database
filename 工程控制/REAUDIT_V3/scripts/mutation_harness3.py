#!/usr/bin/env python3
"""Round-3 targeted mutation cases."""
import subprocess, os, sys, shutil, json, csv
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
REPO = "/home/lighthouse/Astro CS Database"
OUT = os.path.join(ROOT, "package", "06_checker_truthfulness")
WORK = os.path.join(ROOT, "builds", "checker_mutations3")
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
    return pr.returncode, pr.stdout

def write_file(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f: f.write(content)

results = []
def record(case, checker, expected, rc, note=""):
    results.append({"case": case, "checker": checker, "expected": expected, "actual_exit": rc, "note": note})
    print(case + ": expected=" + expected + " actual_exit=" + str(rc) + " | " + note)

# A: doc_symbols with BACKTICKED nonexistent symbol and file
d = fresh_stub("doc_symbols_backticked_bad")
doc = os.path.join(d, "docs/science/PHASE2_UPM.md")
txt = open(doc, encoding="utf-8").read()
write_file(doc, txt + chr(10) + "Audit probe: `p2_nonexistent_symbol_zzz` and `nonexistent_xyz_file_12345.cpp`" + chr(10))
rc, so = run_checker("check_doc_symbols", d)
record("doc_symbols_backticked_bad", "check_doc_symbols", "FAIL(expected)", rc,
       "backticked nonexistent symbol p2_nonexistent_symbol_zzz + nonexistent .cpp file; result verbatim")

# B: config_unknown_key - validate JSON then run config checker
d = fresh_stub("config_unknown_key2")
cfg = os.path.join(d, "lib/orchestrator/configs/stage1_gc_panel1_Red.json")
txt = open(cfg, encoding="utf-8").read()
# append a top-level unknown key safely
obj = json.loads(txt)
obj["audit_zzz_unknown_key_probe"] = 123.45
write_file(cfg, json.dumps(obj, indent=2))
rc, so = run_checker("check_config_contracts", d)
record("config_unknown_key_added_valid", "check_config_contracts", "FAIL(expected)", rc,
       "valid-JSON unknown top-level config key added to stage1_gc_panel1_Red.json; result verbatim")

# C: check_build_graph - doc references nonexistent CMakeLists (P1-04 related)
d = fresh_stub("build_graph_bad_ref")
bg = os.path.join(d, "docs/architecture/BUILD_GRAPH.md")
txt = open(bg, encoding="utf-8").read()
write_file(bg, txt + chr(10) + "Audit probe: lib/astro_image_io/CMakeLists.txt does not exist (P1-04)." + chr(10))
rc, so = run_checker("check_build_graph", d)
record("build_graph_nonexistent_cmake_reference", "check_build_graph", "FAIL(expected)", rc,
       "BUILD_GRAPH.md references nonexistent lib/astro_image_io/CMakeLists.txt; checker does not flag missing CMakeLists")

# D: check_test_contracts - wildcard test ID probe
d = fresh_stub("test_wildcard_id")
rc, so = run_checker("check_test_contracts", d)
record("test_wildcard_id_probe", "check_test_contracts", "see_note", rc,
       "probe only; checker result on unchanged repo (wildcard IDs present in tree)")

with open(os.path.join(OUT, "checker_truthfulness_mutation_results_round3.json"), "w") as f:
    json.dump(results, f, indent=2, ensure_ascii=False)
print("HARNESS3_DONE")
