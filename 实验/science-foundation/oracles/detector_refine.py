"""Refine only initial global-budget failures, keeping all original evidence."""
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
from pathlib import Path
import subprocess
import sys


if __name__ == "__main__":
    root = Path(__file__).resolve().parent.parent
    initial_path = root/"evidence"/"detector_independent.json"
    initial = json.loads(initial_path.read_text(encoding="utf-8"))
    failed = [case["name"] for case in initial["cases"] if case["enclosure"]["status"]!="tolerance_met"]
    destination = root/"evidence"/"detector_interval_refinement_cases"
    destination.mkdir(exist_ok=True)
    solver = root/"oracles"/"detector_independent.py"

    def run(name):
        path = destination/(name+".json")
        command = [sys.executable,str(solver),"--output",str(path),"--fixture",name,
                   "--relative-tolerance",".005","--max-nodes","80001","--samples","200000"]
        creation = getattr(subprocess,"CREATE_NO_WINDOW",0)
        result = subprocess.run(command,capture_output=True,text=True,creationflags=creation)
        if result.returncode:
            return {"name":name,"command":command,"exit_code":result.returncode,"stderr":result.stderr}
        evidence = json.loads(path.read_text(encoding="utf-8"))
        print(result.stdout.strip(),flush=True)
        return {"name":name,"command":command,"exit_code":0,"source_sha256":evidence["source_sha256"],
                "evidence_file":path.name,"case":evidence["cases"][0]}

    results = []
    with ThreadPoolExecutor(max_workers=4) as pool:
        futures = [pool.submit(run,name) for name in failed]
        for future in as_completed(futures):
            results.append(future.result())
            out = {"initial_source_sha256":initial["source_sha256"],
                   "initial_evidence_sha256":hashlib.sha256(initial_path.read_bytes()).hexdigest(),
                   "final_solver_source_sha256":hashlib.sha256(solver.read_bytes()).hexdigest(),
                   "refinement_relative_tolerance":.005,"refinement_max_nodes":80001,
                   "preserves_initial_statuses":True,"requested_names":failed,
                   "completed_names":sorted(r["name"] for r in results),"refinements":sorted(results,key=lambda r:r["name"])}
            (root/"evidence"/"detector_interval_refinements.json").write_text(json.dumps(out,indent=2),encoding="utf-8")
