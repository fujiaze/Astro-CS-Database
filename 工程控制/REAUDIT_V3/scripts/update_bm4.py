import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "04_build", "build_matrix.csv")
rows = list(csv.DictReader(open(P, encoding="utf-8")))
keys = list(rows[0].keys())
seen = set(r["layer"] for r in rows)
new = [
["gaia_xpsd_client-linux","Makefile (lib/gaia_xpsd_client)","PASS(workaround) / FAIL(document cmd)","logs/gaia_build.log + gaia_build_fpic.log","doc=2; workaround=0",
 "Makefile shared build lacks -fPIC (R_X86_64_PC32 against stderr relocation); adding -fPIC builds gaia_client.dll ELF so sha256 7131da01..."],
["photometric_calib-linux","Makefile (lib/photometric_calib/cpp)","PASS(workaround) / FAIL(document cmd)","logs/photometric_build.log + photometric_build3.log","doc=2; workaround=0",
 "needs gaia_client.dll (Windows dll) present; with gaia -fPIC + dropping -static from the link, photometric_calib.dll builds ELF so sha256 df732b8f... (same -static bug class as snr P2-11)"],
]
added = 0
for nr in new:
    if nr[0] not in seen:
        rows.append(dict(zip(keys, nr))); added += 1
for r in rows:
    if r["layer"] == "all-first-party-module-builds-linux":
        r["notes"] = ("calibration OK; healpix_drizzle OK(3 workarounds); phase2 C release/debug/asan PASS; "
                      "snr_estimator OK(workaround -static); gaia_xpsd_client OK(workaround -fPIC); "
                      "photometric_calib OK(2 workarounds: gaia -fPIC + drop -static); star_detector BLOCKED (GSL); "
                      "anchors A/B FAIL (Windows source); plate_solve/dynamic_psf not built this round")
        r["exit_code"] = "calibration=0; drizzle=0(w); phase2=0; snr=0(w); gaia=0(w); photometric=0(w); star=GSL missing"
with open(P, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=keys); w.writeheader(); w.writerows(rows)
print("build_matrix rows:", len(rows))
