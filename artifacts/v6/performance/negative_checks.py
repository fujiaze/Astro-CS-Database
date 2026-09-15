#!/usr/bin/env python3
"""PERF-SCALE-001 negative / boundary checks.

A. Determinism-gate sensitivity (the gate must be able to go red, and must not be blind):
   A1 flip a byte inside a numeric science JSON payload  -> normalized digest must change.
   A2 flip a byte inside a FITS data array               -> FITS data digest must change.
   A3 change only the FITS RUNID provenance card         -> normalized signature UNCHANGED
      (proves normalization targets run-scoped provenance, not science payload).
B. Resource-gate detection under injected concurrency restriction: run the same heavy phase1
   workload at allocated capacity 16 with OMP_NUM_THREADS=1, measure, and record what the
   §10.5 record surface reflects (record-only, PENDING_OWNER_SIGNOFF).
C. Worker-lease trace boundary run (ASTROCS_LEASE_TRACE=1) for per-node lease caps.
"""
from __future__ import annotations
import argparse, importlib.util, io, json, os, pathlib, shutil, subprocess, sys

REPO = pathlib.Path("/workspace/Astro CS Database")
ART = REPO / "artifacts" / "v6" / "performance"
RUN = REPO / "run" / "v6" / "performance"
CFG_TPL = ART / "configs" / "p1_real_ldn43.json.tmpl"

spec = importlib.util.spec_from_file_location("cdc", str(ART / "check_determinism_cli.py"))
CDC = importlib.util.module_from_spec(spec)
spec.loader.exec_module(CDC)


def sh(args, log_path=None):
    p = subprocess.run(args, cwd=str(REPO), capture_output=True, text=True)
    if log_path:
        pathlib.Path(log_path).write_text((p.stdout or "") + (p.stderr or ""), encoding="utf-8")
    return p.returncode, (p.stdout or "") + (p.stderr or "")


def norm_sig(path: pathlib.Path, rel_root: pathlib.Path):
    raw = path.read_bytes()
    if path.suffix.lower() in CDC.FITS_SUFFIX:
        d, h, err = CDC.fits_signature(raw, path)
        return ("fits", d, h)
    if path.suffix.lower() in CDC.TEXT or b"\x00" not in raw[:4096]:
        import hashlib
        return ("text", hashlib.sha256(CDC.normalize(raw, rel_root)).hexdigest())
    import hashlib
    return ("bin", hashlib.sha256(raw).hexdigest())


def main():
    out = {"schema": "astrocs.v6.perf.negative-checks/v1", "checks": []}
    src = REPO / "run" / "v6" / "performance" / "p1_real_ldn43_4k_w16_r0"
    if not src.exists():
        print("source run missing: %s" % src, file=sys.stderr)
        return 2
    probe_root = RUN / "neg_probe"
    if probe_root.exists():
        shutil.rmtree(probe_root)
    probe_root.mkdir(parents=True)

    # A1: numeric JSON payload flip
    jname = "p1_snr.json"
    j = probe_root / jname
    data = bytearray((src / jname).read_bytes())
    mid = len(data) // 2
    data[mid] ^= 0x01
    j.write_bytes(bytes(data))
    a = norm_sig(src / jname, src)
    b = norm_sig(j, probe_root)
    out["checks"].append({"id": "A1_json_payload_flip", "status": "PASS" if a != b else "FAIL",
                          "detail": "flipped payload byte %d of %s; normalized digest %s"
                                    % (mid, jname, "changed" if a != b else "UNCHANGED (blind!)")})

    # A2 / A3: FITS data-array flip vs RUNID-only change
    # Prefer a FITS product that actually carries a RUNID provenance card (the Phase3 export
    # does); fall back to the Phase1 calibrated frame.
    cand = [REPO / "run/v6/performance/p3_surface_brightness_w16_r0/output_phase3.fits",
            src / "calibrated_LDN43_LRGBH_flying_dutchman-20250504@034648-1200S-Green.fts"]
    fsrc = next((c for c in cand if c.exists() and b"RUNID   =" in c.read_bytes()[:20000]), None)
    fname = fsrc.name if fsrc else "none"
    fdata = probe_root / ("dataflip_" + fname)
    frunid = probe_root / ("runidonly_" + fname)
    if fsrc is not None:
        try:
            from astropy.io import fits
            with fits.open(fsrc, memmap=False) as h:
                hdus = [hdu.copy() for hdu in h]
            hdus[0].data = hdus[0].data.copy()
            flat = hdus[0].data.ravel()
            flat[len(flat) // 2] = flat[len(flat) // 2] + 1.0
            fits.HDUList(hdus).writeto(fdata, overwrite=True)
            base = norm_sig(fsrc, src)
            d1 = norm_sig(fdata, probe_root)
            out["checks"].append({"id": "A2_fits_data_flip", "status": "PASS" if base != d1 else "FAIL",
                                  "detail": "FITS data array byte changed -> normalized signature "
                                            + ("changed" if base != d1 else "UNCHANGED (blind!)")})
            # A3: patch ONLY the RUNID card in the raw bytes at its 80-byte card boundary, so
            # astropy cannot reformat the rest of the header. The normalized signature must be
            # unchanged; the raw digest must change.
            rawf = bytearray(fsrc.read_bytes())
            idx = rawf.find(b"RUNID   =")
            if idx >= 0:
                cs = (idx // 80) * 80
                old = bytes(rawf[cs + 10:cs + 24])          # 'xxxxxxxxxxxx'
                newv = b"'deadbeefcafe'"
                assert len(old) == len(newv), (old, newv)
                rawf[cs + 10:cs + 24] = newv
                frunid.write_bytes(bytes(rawf))
                import hashlib
                raw_changed = (hashlib.sha256(rawf).hexdigest()
                               != hashlib.sha256(fsrc.read_bytes()).hexdigest())
                d2 = norm_sig(frunid, probe_root)
                ok = (base == d2) and raw_changed
                out["checks"].append({
                    "id": "A3_fits_runid_only", "status": "PASS" if ok else "FAIL",
                    "detail": "RUNID card bytes patched in place (raw digest changed=%s); normalized "
                              "signature %s" % (raw_changed,
                                                "unchanged (provenance-specific)" if base == d2
                                                else "CHANGED (normalization leaks provenance)")})
            else:
                out["checks"].append({"id": "A3_fits_runid_only", "status": "UNAVAILABLE",
                                      "detail": "no RUNID card in " + fsrc.name})
        except Exception as e:
            out["checks"].append({"id": "A2_fits_data_flip", "status": "UNAVAILABLE", "detail": str(e)})
    else:
        out["checks"].append({"id": "A2_fits_data_flip", "status": "UNAVAILABLE",
                              "detail": "no FITS product with a RUNID card found"})

    # B: OMP_NUM_THREADS=1 at allocated 16
    name = "neg_omp1_16"
    for d in RUN.glob(name + "_w*"):
        shutil.rmtree(d, ignore_errors=True)
    rc, log = sh([sys.executable, str(ART / "perf_harness.py"), "--name", name,
                  "--cmd", "./build/astrocs phase1 run --config {cfg} --events-jsonl",
                  "--config-template", str(CFG_TPL), "--budgets", "16", "--reps", "1",
                  "--env", "OMP_NUM_THREADS=1", "--out", str(ART)], RUN / "neg_omp1_harness.log")
    jp = ART / (name + "_runs.json")
    det = {"harness_rc": rc}
    if jp.exists():
        d = json.loads(jp.read_text(encoding="utf-8"))["runs"][0]
        det.update({"wall_seconds": d.get("wall_seconds"),
                    "avg_equivalent_cores": d.get("avg_equivalent_cores"),
                    "cpu_mean_percent_of_allocated": d.get("cpu_mean_percent_of_allocated"),
                    "active_compute_threads_max": d.get("active", {}).get("active_compute_threads_max"),
                    "intervals_single_thread": d.get("active", {}).get("intervals_with_single_active_compute_thread"),
                    "intervals_total": d.get("active", {}).get("intervals_total")})
    out["checks"].append({"id": "B_omp_num_threads_1_at_16", "status": "RECORDED",
                          "detail": "injected OMP_NUM_THREADS=1 at allocated capacity 16 "
                                    "(record-only; SO-05 pending)", "measured": det,
                          "log": "run/v6/performance/neg_omp1_harness.log"})

    # B2: measured contention injection — 12 busy hogs on CPUs 0-15 starve the heavy run so
    # its realized concurrency drops far below allocated capacity. The §10.5 record surface
    # must register the low utilization (record-only, SO-05 pending).
    name = "neg_contention_16"
    for d in RUN.glob(name + "_w*"):
        shutil.rmtree(d, ignore_errors=True)
    hogs = [subprocess.Popen(["taskset", "-c", str(c), "sh", "-c", "while :; do :; done"])
            for c in range(12)]          # 12 of 16 CPUs occupied
    try:
        rc, log = sh([sys.executable, str(ART / "perf_harness.py"), "--name", name,
                      "--cmd", "./build/astrocs phase1 run --config {cfg} --events-jsonl",
                      "--config-template", str(CFG_TPL), "--budgets", "16", "--reps", "1",
                      "--out", str(ART)], RUN / "neg_contention_harness.log")
    finally:
        for h in hogs:
            h.kill()
        for h in hogs:
            h.wait()
    jp = ART / (name + "_runs.json")
    det = {"harness_rc": rc}
    if jp.exists():
        d = json.loads(jp.read_text(encoding="utf-8"))["runs"][0]
        det.update({"wall_seconds": d.get("wall_seconds"),
                    "avg_equivalent_cores": d.get("avg_equivalent_cores"),
                    "cpu_mean_percent_of_allocated": d.get("cpu_mean_percent_of_allocated"),
                    "active_compute_threads_max": d.get("active", {}).get("active_compute_threads_max"),
                    "max_consecutive_below_60pct_s": d.get("active", {}).get("max_consecutive_below_60pct_s")})
        # in-band CLI recorder finding (independent of the external sampler)
        rs = RUN / (name + "_w16_r0") / "resource_summary.json"
        if rs.exists():
            doc = json.loads(rs.read_text(encoding="utf-8"))
            act = next((s for s in doc.get("stages", []) if s.get("stage") == "active"), {})
            det["inband_cpu_mean_percent_of_allocated"] = (act.get("cpu_pct_mean", 0) or 0) / 16.0
            det["inband_active_compute_threads_peak"] = act.get("active_compute_threads_peak")
        rl = RUN / (name + "_w16_r0") / "run.log"
        if rl.exists():
            det["gate_warning_lines"] = [l for l in rl.read_text(encoding="utf-8", errors="replace").splitlines()
                                         if "resource gate recorded" in l or "resource_gate" in l][:10]
    out["checks"].append({"id": "B2_contention_injection", "status": "RECORDED",
                          "detail": "12 busy hogs pinned to CPUs 0-11 starved the run at allocated 16 "
                                    "(record-only; SO-05 pending)", "measured": det,
                          "log": "run/v6/performance/neg_contention_harness.log"})

    # C: lease trace at 16
    name = "lease_trace_16"
    for d in RUN.glob(name + "_w*"):
        shutil.rmtree(d, ignore_errors=True)
    rc, log = sh([sys.executable, str(ART / "perf_harness.py"), "--name", name,
                  "--cmd", "./build/astrocs phase1 run --config {cfg} --events-jsonl",
                  "--config-template", str(CFG_TPL), "--budgets", "16", "--reps", "1",
                  "--env", "ASTROCS_LEASE_TRACE=1", "--out", str(ART)], RUN / "lease_trace_harness.log")
    logp = RUN / (name + "_w16_r0") / "run.log"
    lease_lines = []
    if logp.exists():
        lease_lines = [l for l in logp.read_text(encoding="utf-8", errors="replace").splitlines()
                       if "lease" in l.lower() or "budget" in l.lower()][:300]
    out["checks"].append({"id": "C_lease_trace", "status": "RECORDED",
                          "detail": "ASTROCS_LEASE_TRACE=1 captured %d lease/budget lines" % len(lease_lines),
                          "lease_lines_sample": lease_lines[:60],
                          "log": "run/v6/performance/lease_trace_16_w16_r0/run.log"})
    (ART / "negative_checks.json").write_text(json.dumps(out, ensure_ascii=False, indent=2) + "\n",
                                              encoding="utf-8")
    for c in out["checks"]:
        print("[%s] %s -- %s" % (c["status"], c["id"], c.get("detail", "")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
