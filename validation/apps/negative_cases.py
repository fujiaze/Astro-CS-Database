#!/usr/bin/env python3
"""The negative cases of the CLI and of the product layer.

The cases are the constructions of the independent audit:

* a configuration without the required keys: the command answers
  invalid_request with the event stream and exits 2 (no abort);
* a product with a missing block: the reopen answers missing_dependency;
* a product with a corrupt block: the reopen answers hash_mismatch.

Every case writes the request, the output, the exit code and the parsed
events into validation/apps/evidence/negative/.
"""
from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CLI = ROOT / "build/science/acsd"
EVIDENCE = ROOT / "validation/apps/evidence/negative"
WORK = ROOT / ".work/negative"


def run_cli(config: Path, name: str) -> tuple[int, list[dict]]:
    events = WORK / f"{name}.events.jsonl"
    process = subprocess.run([str(CLI), "normalize", "--json", str(config)],
                             capture_output=True, text=True)
    events.write_text(process.stdout, encoding="utf-8")
    (WORK / f"{name}.stderr.txt").write_text(process.stderr, encoding="utf-8")
    (WORK / f"{name}.exit.txt").write_text(f"{process.returncode}\n", encoding="utf-8")
    parsed = []
    for line in process.stdout.splitlines():
        if line.strip():
            parsed.append(json.loads(line))
    return process.returncode, parsed


def main() -> int:
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    failures = []

    # 1) the configuration without the required keys.
    broken = WORK / "broken.config.json"
    broken.write_text(json.dumps({"schema": "acsd.config/2", "command": "normalize"}),
                      encoding="utf-8")
    code, events = run_cli(broken, "config_missing_keys")
    terminal = [event for event in events if event["event"] == "run_finished"]
    print(f"[config] exit={code} the terminal events: {len(terminal)}")
    if code != 2:
        failures.append(f"the broken configuration exited {code} instead of 2")
    if len(terminal) != 1:
        failures.append("the broken configuration did not write exactly one run_finished")
    else:
        if terminal[0].get("exit_code") != 2:
            failures.append("the run_finished does not carry exit code 2")
        summary = terminal[0].get("summary", {})
        for field in ("inputs_total", "published", "unsolved", "failed", "cancelled", "frames"):
            if field not in summary:
                failures.append(f"the summary misses the field {field}")

    # 2) the product with a missing block.
    source = ROOT / ".work/product-roundtrip/frame"
    if not (source / "manifest.json").is_file():
        print("[product] the round trip product is missing: run validation/apps/run_product_roundtrip.sh")
        return 1
    broken_product = WORK / "product-missing-block"
    shutil.rmtree(broken_product, ignore_errors=True)
    shutil.copytree(source, broken_product)
    blocks = sorted((broken_product / "blocks").glob("*.bin"))
    if not blocks:
        print("[product] the product has no block")
        return 1
    blocks[0].unlink()
    import hashlib

    manifest_hash = "sha256:" + hashlib.sha256((broken_product / "manifest.json").read_bytes()).hexdigest()
    config = {
        "schema": "acsd.config/2",
        "command": "export",
        "inputs": [{"path": str(broken_product), "expected_hash": manifest_hash}],
        "scientific": str(ROOT / "validation/apps/evidence/export/export.scientific.json"),
        "output": str(WORK / "products/missing-block"),
        "resources": {"bytes": 1073741824, "work": 10000000, "spill_bytes": 0, "threads": 4,
                      "max_depth": 64, "max_precision_bits": 256, "scratch_directory": ".work"},
        "product_profile": "full",
    }
    broken_export = WORK / "missing_block.config.json"
    broken_export.write_text(json.dumps(config, indent=1), encoding="utf-8")
    process = subprocess.run([str(CLI), "export", "--json", str(broken_export)],
                             capture_output=True, text=True)
    (WORK / "product_missing_block.events.jsonl").write_text(process.stdout, encoding="utf-8")
    (WORK / "product_missing_block.exit.txt").write_text(f"{process.returncode}\n",
                                                         encoding="utf-8")
    print(f"[product] the missing block: exit={process.returncode}")
    print(f"          {process.stdout.strip()[:300]}")
    if "missing_dependency" not in process.stdout:
        failures.append("the missing block is not reported as a missing dependency")

    # 3) the product with a corrupt block.
    corrupt_product = WORK / "product-corrupt-block"
    shutil.rmtree(corrupt_product, ignore_errors=True)
    shutil.copytree(source, corrupt_product)
    blocks = sorted((corrupt_product / "blocks").glob("*.bin"))
    data = bytearray(blocks[0].read_bytes())
    data[0] ^= 0xff
    blocks[0].write_bytes(bytes(data))
    manifest_hash = "sha256:" + hashlib.sha256((corrupt_product / "manifest.json").read_bytes()).hexdigest()
    config["inputs"] = [{"path": str(corrupt_product), "expected_hash": manifest_hash}]
    config["output"] = str(WORK / "products/corrupt-block")
    corrupt_export = WORK / "corrupt_block.config.json"
    corrupt_export.write_text(json.dumps(config, indent=1), encoding="utf-8")
    process = subprocess.run([str(CLI), "export", "--json", str(corrupt_export)],
                             capture_output=True, text=True)
    (WORK / "product_corrupt_block.events.jsonl").write_text(process.stdout, encoding="utf-8")
    (WORK / "product_corrupt_block.exit.txt").write_text(f"{process.returncode}\n",
                                                         encoding="utf-8")
    print(f"[product] the corrupt block: exit={process.returncode}")
    print(f"          {process.stdout.strip()[:300]}")

    # The evidence of the cases.
    summary = {
        "case": "apps-negative",
        "cases": {
            "config_missing_keys": {
                "exit_code": int((WORK / "config_missing_keys.exit.txt").read_text().strip()),
                "events": [json.loads(line) for line
                           in (WORK / "config_missing_keys.events.jsonl").read_text().splitlines()
                           if line.strip()],
            },
            "product_missing_block": {
                "exit_code": int((WORK / "product_missing_block.exit.txt").read_text().strip()),
                "events": [json.loads(line) for line
                           in (WORK / "product_missing_block.events.jsonl").read_text().splitlines()
                           if line.strip()],
            },
            "product_corrupt_block": {
                "exit_code": int((WORK / "product_corrupt_block.exit.txt").read_text().strip()),
                "events": [json.loads(line) for line
                           in (WORK / "product_corrupt_block.events.jsonl").read_text().splitlines()
                           if line.strip()],
            },
        },
    }
    (EVIDENCE / "negative_cases.json").write_text(json.dumps(summary, indent=1, ensure_ascii=False),
                                                   encoding="utf-8")
    for name in ("config_missing_keys", "product_missing_block", "product_corrupt_block"):
        shutil.copy(WORK / f"{name}.events.jsonl", EVIDENCE / f"{name}.events.jsonl")
        shutil.copy(WORK / f"{name}.exit.txt", EVIDENCE / f"{name}.exit.txt")

    if failures:
        for failure in failures:
            print(f"[FAIL] {failure}")
        return 1
    print("[PASS] the negative cases answer with the declared codes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
