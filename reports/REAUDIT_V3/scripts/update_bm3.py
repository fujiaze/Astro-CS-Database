import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "04_build", "build_matrix.csv")
rows = list(csv.DictReader(open(P, encoding="utf-8")))
keys = list(rows[0].keys())
seen = set(r["layer"] for r in rows)
new = [
["snr_estimator-linux","Makefile (lib/snr_estimator/cpp)","PASS(workaround) / FAIL(document cmd)","logs/snr_build.log + snr_build_work.log","doc=2; workaround=0",
 "Makefile link line uses -shared -static together -> crtbeginT.o relocation R_X86_64_32 error on Linux; dropping -static builds snr_estimator.dll ELF so sha256 e2765c6e..."],
["star_detector-linux","Makefile (lib/star_detector)","FAIL_REPRODUCIBILITY (missing GSL dev headers on host)","logs/star_build.log","fatal at sdet_api.cpp:28 gsl/gsl_multifit_nlinear.h",
 "module needs GSL trust-region LM; gsl dev headers not installed on this host (would need apt install libgsl-dev; not performed this round)"],
]
added = 0
for nr in new:
    if nr[0] not in seen:
        rows.append(dict(zip(keys, nr))); added += 1
# update all-first-party row to reflect snr
for r in rows:
    if r["layer"] == "all-first-party-module-builds-linux":
        r["notes"] = ("calibration OK; healpix_drizzle OK(3 workarounds); phase2 C release/debug/asan PASS; "
                      "snr_estimator OK(workaround -static); star_detector BLOCKED (GSL not installed); "
                      "anchors A/B FAIL (Windows source); photometric_calib depends on gaia_client.dll; "
                      "plate_solve/dynamic_psf/gaia not built this round")
        r["exit_code"] = "calibration=0; drizzle=0(w); phase2=0; snr=0(w); star=GSL missing"
with open(P, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=keys); w.writeheader(); w.writerows(rows)
print("build_matrix rows:", len(rows))
