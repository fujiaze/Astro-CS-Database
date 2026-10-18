#!/usr/bin/env python3
"""ARS-01 S0 external measurement harness. Python 3.9+; no ACSD source changes.

Evidence statuses: MEASURED, NOT_RUN, UNINSTRUMENTED, UNVERIFIED, FAILED.
Linux /proc probe samples one acsd PID; it does not claim whole-system disk throughput.
"""
import argparse
import copy
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time

HISTORICAL_ROOT = "/workspace/Astro CS Database"
UNKNOWN = "UNINSTRUMENTED"

def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False, sort_keys=True) + "\n", encoding="utf-8")

def sha256_file(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for data in iter(lambda: f.read(4 * 1024 * 1024), b""):
            h.update(data)
    return h.hexdigest()

def file_info(path, full_hash=True):
    p = Path(path)
    if not p.is_file():
        return {"path": str(p), "status": "NOT_RUN", "reason": "missing file"}
    d = {"path": str(p.resolve()), "size_bytes": p.stat().st_size}
    d["sha256"] = sha256_file(p) if full_hash else UNKNOWN
    return d

def walk_properties(root):
    root = Path(root)
    for path in (root / "signal" / "properties", root / "properties"):
        if path.is_file():
            d = {}
            for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
                if raw.lstrip().startswith("#") or "=" not in raw:
                    continue
                k, v = raw.split("=", 1)
                d[k.strip()] = v.strip()
            return d, path
    raise ValueError("HiPS properties missing: " + str(root))

def hips_identity(path):
    props, pf = walk_properties(path)
    order = props.get("hips_order")
    if order is None or not re.fullmatch(r"[0-9]+", order) or not 0 <= int(order) <= 29:
        raise ValueError("invalid/missing hips_order at " + str(pf))
    if props.get("tile_width", "512") != "512":
        raise ValueError("unsupported tile_width at " + str(pf))
    bunit = props.get("BUNIT") or props.get("bunit")
    if not bunit:
        raise ValueError("missing BUNIT, cannot use as scientific baseline: " + str(pf))
    tiles = list(Path(path).glob("**/Norder*/Dir*/Npix*.fits"))
    if not tiles:
        raise ValueError("no signal FITS tiles found at " + str(path))
    return {"path": str(Path(path).resolve()), "hips_order": int(order),
            "bunit": bunit, "response_id": props.get("ACSD_RESPONSE_ID"),
            "obs_filter": props.get("obs_filter") or props.get("ACSD_OBS_FILTER"),
            "properties": file_info(pf), "tile_count": len(tiles)}

def validate_w2(paths):
    if len(paths) < 2:
        raise ValueError("W2 requires >=2 valid real frame HiPS products")
    ids = [hips_identity(p) for p in paths]
    for x in ids[1:]:
        for key in ("hips_order", "bunit", "response_id", "obs_filter"):
            a, b = ids[0].get(key), x.get(key)
            if a != b:
                raise ValueError("W2 requires comparable inputs: " + key + " differs")
    if len({x["path"] for x in ids}) != len(ids):
        raise ValueError("W2 input duplicates, not independent observations")
    return ids

def remap_paths(config, source_root):
    def visit(x):
        if isinstance(x, str):
            if x == HISTORICAL_ROOT or x.startswith(HISTORICAL_ROOT + "/"):
                return str(source_root) + x[len(HISTORICAL_ROOT):]
            return x
        if isinstance(x, list):
            return [visit(v) for v in x]
        if isinstance(x, dict):
            return {k: visit(v) for k, v in x.items()}
        return x
    return visit(config)

def baseline_config(args, attempt_dir):
    if args.workload == "W1":
        if not args.source_root:
            raise ValueError("W1 requires --source-root (mounted original M42/Gaia files)")
        cfg = json.loads(Path(args.w1_config).read_text(encoding="utf-8"))
        cfg = remap_paths(cfg, Path(args.source_root).resolve())
        if len(cfg.get("blocks", [])) != 1 or len(cfg["blocks"][0].get("input_lights", [])) != 6:
            raise ValueError("W1 reference must be exactly original 6-frame one-block run")
        blk = cfg["blocks"][0]
        blk["output_dir"] = str(attempt_dir / "product")
        if blk.get("work_dir"):
            blk["work_dir"] = str(attempt_dir / "work")
        if args.gaia_root:
            gr = str(Path(args.gaia_root).resolve())
            blk.setdefault("wcs", {})["gaia_data_dir"] = gr
            blk.setdefault("photometry", {}).setdefault("fit", {})["gaia_data_dir"] = gr
        if blk.get("photometry", {}).get("fit", {}).get("enabled") is not True:
            raise ValueError("W1 requires enabled Gaia photometric fit")
        sources = [Path(x) for x in blk["input_lights"]] + [
            Path(blk[x]) for x in ("master_bias", "master_dark", "master_flat")]
        ref = json.loads(Path(args.identity_reference).read_text(encoding="utf-8"))
        expected = ref["input_lights_and_masters"]
        for old_path, identity in expected.items():
            mapped = Path(str(Path(args.source_root).resolve()) + old_path[len(HISTORICAL_ROOT):])
            if not mapped.is_file():
                raise FileNotFoundError("frozen W1 input unavailable: " + str(mapped))
            if mapped.stat().st_size != identity["bytes"] or sha256_file(mapped) != identity["sha256"]:
                raise ValueError("frozen W1 input digest mismatch: " + str(mapped))
        if len(sources) != len(expected):
            raise ValueError("W1 six lights + three masters identity count mismatch")
        gaia = Path(blk["wcs"]["gaia_data_dir"])
        for item in ref["gaia_catalog"]["files"]:
            path = gaia / item["name"]
            if not path.is_file() or path.stat().st_size != item["bytes"]:
                raise FileNotFoundError("Gaia expected file/size absent: " + str(path))
        return cfg, sources, {"source_evidence": str(args.identity_reference),
                              "gaia_validation": "NAMES_AND_BYTES_ONLY_NOT_FULL_SHA256",
                              "w1_source_fits_and_masters": "SHA256_VERIFIED"}
    if args.workload == "W2":
        if not args.w2_hips_json:
            raise ValueError("W2 requires --w2-hips-json, explicitly selected real frame HiPS paths")
        paths = json.loads(Path(args.w2_hips_json).read_text(encoding="utf-8"))
        if not isinstance(paths, list) or not all(isinstance(p, str) for p in paths):
            raise ValueError("W2 HiPS manifest must be JSON string array")
        selected = validate_w2(paths)
        cfg = {"schema_version": "1", "blocks": [{
            "name": "ars01_same_order", "hips_paths": paths,
            "output_dir": str(attempt_dir / "product"),
            "snr_path": args.w2_snr_path}]}
        return cfg, [Path(x["properties"]["path"]) for x in selected], selected
    if not args.w3_hips:
        raise ValueError("W3 requires --w3-hips pointing to complete legal P2 signal HiPS")
    entry = hips_identity(args.w3_hips)
    if args.w3_complete_manifest:
        man = json.loads(Path(args.w3_complete_manifest).read_text(encoding="utf-8"))
        if man.get("status") != "complete":
            raise ValueError("W3 source manifest not complete")
    else:
        raise ValueError("W3 requires --w3-complete-manifest from a verified P2 run")
    cfg = {"schema_version": "1", "blocks": [{
        "name": "ars01_p3", "source": {"hips_dir": args.w3_hips},
        "output_dir": str(attempt_dir / "product"),
        "center": {"ra_deg": args.ra, "dec_deg": args.dec},
        "width_px": args.width, "height_px": args.height,
        "scale_deg_per_px": args.scale,
        "rotation_deg": args.rotation,
        "crpix_px": [(args.width + 1) / 2, (args.height + 1) / 2],
        "output_mode": "surface_brightness"}]}
    return cfg, [Path(entry["properties"]["path"]), Path(args.w3_complete_manifest)], [entry]

def host_info():
    d = {"system": platform.platform(), "uname": platform.uname()._asdict(),
         "logical_cpus": os.cpu_count(), "physical_cpus": UNKNOWN,
         "cpu_model": UNKNOWN, "isa_flags": UNKNOWN, "physical_memory_bytes": UNKNOWN,
         "compiler": UNKNOWN, "cgroup_limit_bytes": UNKNOWN}
    p = Path("/proc/cpuinfo")
    if p.is_file():
        raw = p.read_text(errors="replace")
        models = re.findall(r"^model name\s*:\s*(.*)$", raw, re.M)
        flags = re.findall(r"^flags\s*:\s*(.*)$", raw, re.M)
        if models:
            d["cpu_model"] = models[0]
        if flags:
            d["isa_flags"] = sorted(set(flags[0].split()))
        pairs = set(zip(re.findall(r"^physical id\s*:\s*(.*)$", raw, re.M),
                        re.findall(r"^core id\s*:\s*(.*)$", raw, re.M)))
        if pairs:
            d["physical_cpus"] = len(pairs)
    p = Path("/proc/meminfo")
    if p.is_file():
        m = re.search(r"^MemTotal:\s*(\d+)\s+kB", p.read_text(), re.M)
        if m:
            d["physical_memory_bytes"] = int(m.group(1)) * 1024
    p = Path("/sys/fs/cgroup/memory.max")
    if p.is_file():
        d["cgroup_limit_bytes"] = p.read_text().strip()
    return d

def sample_pid(pid):
    base = Path("/proc") / str(pid)
    try:
        status = (base / "status").read_text()
        stat = (base / "stat").read_text().rsplit(")", 1)[1].split()
        io = {}
        for line in (base / "io").read_text().splitlines():
            k, v = line.split(":", 1)
            io[k] = int(v.strip())
        def kb(key):
            m = re.search(r"^" + re.escape(key) + r":\s*(\d+)\s*kB", status, re.M)
            return int(m.group(1)) * 1024 if m else UNKNOWN
        ticks = os.sysconf("SC_CLK_TCK")
        return {"monotonic_ns": time.monotonic_ns(), "pid": pid,
                "rss_bytes": kb("VmRSS"), "hwm_bytes": kb("VmHWM"),
                "threads": int(stat[17]), "user_cpu_s": int(stat[11]) / ticks,
                "system_cpu_s": int(stat[12]) / ticks,
                "rchar": io.get("rchar", UNKNOWN), "wchar": io.get("wchar", UNKNOWN),
                "read_bytes": io.get("read_bytes", UNKNOWN), "write_bytes": io.get("write_bytes", UNKNOWN),
                "cancelled_write_bytes": io.get("cancelled_write_bytes", UNKNOWN)}
    except (OSError, ValueError, IndexError):
        return None

def summarize_fits(path):
    d = {"path": str(path), "sha256": sha256_file(path)}
    try:
        found = {}
        with path.open("rb") as f:
            for _ in range(400):
                card = f.read(80)
                if len(card) != 80 or card.startswith(b"END "):
                    break
                txt = card.decode("ascii", errors="replace")
                k = txt[:8].strip()
                if k in ("NAXIS1", "NAXIS2", "BITPIX", "BUNIT", "CTYPE1", "CTYPE2",
                         "CRPIX1", "CRPIX2", "CRVAL1", "CRVAL2", "CDELT1", "CDELT2",
                         "CD1_1", "CD1_2", "CD2_1", "CD2_2", "PHOTSCAL"):
                    found[k] = txt[10:].split("/", 1)[0].strip().strip("'").strip()
        d["primary_header"] = found
    except OSError as e:
        d["header_error"] = str(e)
    return d

def output_inventory(out):
    if not out.is_dir():
        return {"status": "NOT_RUN", "reason": "output not created"}
    files = sorted(p for p in out.rglob("*") if p.is_file())
    manifest = hashlib.sha256()
    totals = 0
    chosen = []
    for p in files:
        sz = p.stat().st_size
        totals += sz
        digest = sha256_file(p)
        rel = p.relative_to(out).as_posix()
        manifest.update((rel + "\t" + str(sz) + "\t" + digest + "\n").encode())
        if p.name in ("frame_science.json", "p1_phot.json", "p1_wcs.json", "p1_stack.json",
                      "p2_final.json", "p2_integrated.json", "p3_wcs.json",
                      "p3_verify.json", "properties") or p.name.startswith("acsd_run_"):
            rec = {"path": rel, "size_bytes": sz, "sha256": digest}
            if p.suffix == ".json" and sz <= 1_000_000:
                try:
                    obj = json.loads(p.read_text(encoding="utf-8"))
                    rec["science_fields"] = {k: obj.get(k) for k in (
                        "status", "kind", "n_frames_applied", "n_frames_failed",
                        "photscal", "photometry_applied", "spatial_gain_mode",
                        "signal", "source_sha256", "config_sha256",
                        "n_pixels", "n_tiles", "bunit", "complete") if k in obj}
                except (OSError, ValueError):
                    rec["science_fields"] = "UNVERIFIED"
            chosen.append(rec)
        if p.suffix.lower() in (".fits", ".fit") and p.name.startswith("output_"):
            chosen.append({"export_fits": summarize_fits(p)})
    complete = [x for x in chosen if x.get("path", "").startswith("acsd_run_") and
                isinstance(x.get("science_fields"), dict) and x["science_fields"].get("status") == "complete"]
    return {"status": "RECORDED", "file_count": len(files), "total_output_bytes": totals,
            "merkle_list_sha256": manifest.hexdigest(), "selected_products": chosen,
            "complete_run_manifests": len(complete)}

def hips_tree_identity(root):
    p = Path(root)
    if not p.is_dir():
        raise FileNotFoundError("HiPS directory missing: " + str(p))
    digest = hashlib.sha256()
    n = 0
    total = 0
    for f in sorted(x for x in p.rglob("*") if x.is_file()):
        rel = f.relative_to(p).as_posix()
        sz = f.stat().st_size
        digest.update((rel + "\t" + str(sz) + "\t" + sha256_file(f) + "\n").encode())
        n += 1
        total += sz
    return {"root": str(p.resolve()), "file_count": n, "total_bytes": total,
            "merkle_list_sha256": digest.hexdigest()}

def run_once(args, ordinal, binary, cfg, inputs, selection, output_root):
    target = output_root / ("run-%02d" % ordinal)
    target.mkdir(parents=True, exist_ok=False)
    config_path = target / "config.json"
    write_json(config_path, cfg)
    inlist = []
    for p in inputs:
        fi = file_info(p)
        if fi.get("status") == "NOT_RUN":
            raise FileNotFoundError(str(p))
        inlist.append(fi)
    tree_identity = []
    if args.workload == "W2":
        tree_identity = [hips_tree_identity(p) for p in json.loads(Path(args.w2_hips_json).read_text())]
    elif args.workload == "W3":
        tree_identity = [hips_tree_identity(args.w3_hips)]
    write_json(target / "input_identity.json", {
        "config": file_info(config_path), "scientific_files": inlist,
        "selected_hips": selection or [], "input_hips_tree_hashes": tree_identity,
        "binary": file_info(binary)})
    cmd = [str(binary), {"W1": "normalize", "W2": "mosaic", "W3": "export"}[args.workload],
           "--json", str(config_path), "-y"]
    started_utc = dt.datetime.now(dt.timezone.utc).isoformat()
    start_ns = time.monotonic_ns()
    timeseries = []
    with (target / "stdout.jsonl").open("wb") as stdout, (target / "stderr.log").open("wb") as stderr:
        proc = subprocess.Popen(cmd, stdout=stdout, stderr=stderr,
                                cwd=str(Path(args.repo_root).resolve()))
        while proc.poll() is None:
            row = sample_pid(proc.pid)
            if row:
                timeseries.append(row)
            time.sleep(args.interval)
        exit_code = proc.wait()
        row = sample_pid(proc.pid)
        if row:
            timeseries.append(row)
    end_ns = time.monotonic_ns()
    with (target / "process_samples.jsonl").open("w", encoding="utf-8") as o:
        for item in timeseries:
            o.write(json.dumps(item, sort_keys=True) + "\n")
    def maximum(field):
        vals = [x[field] for x in timeseries if isinstance(x.get(field), (int, float))]
        return max(vals) if vals else UNKNOWN
    def range_diff(field):
        vals = [x[field] for x in timeseries if isinstance(x.get(field), (int, float))]
        return max(0, vals[-1] - vals[0]) if len(vals) >= 2 else UNKNOWN
    result = {"workload": args.workload, "ordinal": ordinal,
              "status": "MEASURED" if exit_code == 0 else "FAILED",
              "return_code": exit_code, "argv": cmd,
              "started_at_utc": started_utc,
              "wall_s": (end_ns - start_ns) / 1e9,
              "peak_rss_bytes_vm_hwm": maximum("hwm_bytes"),
              "peak_rss_bytes_sampled": maximum("rss_bytes"),
              "max_threads_observed": maximum("threads"),
              "process_user_cpu_s_delta": range_diff("user_cpu_s"),
              "process_system_cpu_s_delta": range_diff("system_cpu_s"),
              "process_read_bytes_delta_linux_proc": range_diff("read_bytes"),
              "process_write_bytes_delta_linux_proc": range_diff("write_bytes"),
              "sample_count": len(timeseries), "sample_interval_s": args.interval,
              "worker_busy_idle": UNKNOWN, "operation_timing": UNKNOWN,
              "aio_queue_depth": UNKNOWN, "aio_backpressure_wait_s": UNKNOWN,
              "io_wait_s": UNKNOWN, "credit_vs_rss": UNKNOWN,
              "old_new_block_overlap_bytes": UNKNOWN}
    result["product"] = output_inventory(target / "product")
    if result["product"].get("complete_run_manifests", 0) < 1:
        result["status"] = "UNVERIFIED" if exit_code == 0 else "FAILED"
        result["manifest_note"] = "exit 0 does not substitute complete run manifest"
    write_json(target / "measurements.json", result)
    return result

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--repo-root", type=Path, required=True)
    ap.add_argument("--binary", type=Path, required=True)
    ap.add_argument("--workload", choices=["W1", "W2", "W3"], required=True)
    ap.add_argument("--output-root", type=Path, required=True)
    ap.add_argument("--repeats", type=int, default=3)
    ap.add_argument("--interval", type=float, default=.25)
    ap.add_argument("--source-root", type=Path)
    ap.add_argument("--gaia-root", type=Path)
    ap.add_argument("--w1-config", type=Path, default=Path("实验/issue12-simpson-domain/evidence/50-cfg6.json"))
    ap.add_argument("--identity-reference", type=Path, default=Path("实验/issue92-wcs-gaia-match/evidence/00-identity.json"))
    ap.add_argument("--w2-hips-json", type=Path)
    ap.add_argument("--w2-snr-path", choices=["sparse_reconstruct", "frame_reconstruct"], default="sparse_reconstruct")
    ap.add_argument("--w3-hips", type=Path)
    ap.add_argument("--w3-complete-manifest", type=Path)
    ap.add_argument("--ra", type=float, default=83.8)
    ap.add_argument("--dec", type=float, default=-5.4)
    ap.add_argument("--width", type=int, default=512)
    ap.add_argument("--height", type=int, default=512)
    ap.add_argument("--scale", type=float, default=0.0005)
    ap.add_argument("--rotation", type=float, default=0.0)
    args = ap.parse_args()
    args.repo_root = args.repo_root.resolve()
    args.binary = args.binary.resolve()
    args.output_root = args.output_root.resolve()
    if args.repeats < 1 or args.interval <= 0 or args.width <= 0 or args.height <= 0:
        ap.error("positive repeats/interval/geometry required")
    args.output_root.mkdir(parents=True, exist_ok=True)
    about = {"workload": args.workload, "host": host_info(),
             "repo_root": str(args.repo_root),
             "git_head": "NOT_RUN", "binary": file_info(args.binary),
             "w1_reference": str(args.w1_config),
             "notes": "No GPU/system bandwidth/proc-wide disk wait inferred from process counters"}
    try:
        about["git_head"] = subprocess.check_output(
            ["git", "-C", str(args.repo_root), "rev-parse", "HEAD"],
            stderr=subprocess.DEVNULL, text=True, timeout=8).strip()
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
        pass
    if not args.binary.is_file() or not os.access(args.binary, os.X_OK):
        about.update(status="NOT_RUN", reason="binary unavailable")
        write_json(args.output_root / "session.json", about)
        return 2
    try:
        cfg, inputs, selection = baseline_config(args, args.output_root / "CONFIG_PATH_PLACEHOLDER")
        if args.workload == "W1":
            # Actual attempt-specific output path substituted below.
            pass
        results = []
        for n in range(1, args.repeats + 1):
            cfg_n, inp_n, sel_n = baseline_config(args, args.output_root / ("run-%02d" % n))
            results.append(run_once(args, n, args.binary, cfg_n, inp_n, sel_n, args.output_root))
        about.update(status="MEASURED" if all(x["status"] == "MEASURED" for x in results)
                     else "PARTIAL", results=[{"ordinal": x["ordinal"], "status": x["status"],
                                                "wall_s": x["wall_s"],
                                                "return_code": x["return_code"]} for x in results])
        write_json(args.output_root / "session.json", about)
        return 0 if about["status"] == "MEASURED" else 1
    except (ValueError, FileNotFoundError, PermissionError, OSError, json.JSONDecodeError) as ex:
        about.update(status="NOT_RUN", reason=str(ex))
        write_json(args.output_root / "session.json", about)
        print("ARS-01 NOT_RUN: " + str(ex), file=sys.stderr)
        return 2

if __name__ == "__main__":
    raise SystemExit(main())
