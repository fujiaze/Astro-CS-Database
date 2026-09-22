#!/usr/bin/env python3
"""IMPL-AIO-001 实现级负向 mutation harness。

对 lib/infrastructure/aio/v6 源码副本注入"违反冻结"的实现错误，重新编译共址测试，
断言对应测试子命令 rc != 0（门必红）。正控制：未变异源码同一子命令 rc == 0。
"""
import os
import shutil
import subprocess
import sys

MUTATIONS = [
    dict(id="MUT-01-signal-bunit", path="src/v6_bunit.cpp",
         old='case Quantity::kSignalSb: return "ADU/sr";',
         new='case Quantity::kSignalSb: return "ADU";',
         cmd="units",
         freeze="FZ-UNIT-SIGNAL-SB"),
    dict(id="MUT-02-quadratic-law", path="src/v6_bunit.cpp",
         old="if (v.adu_power != 2 * s.adu_power || v.px_power != 2 * s.px_power) {",
         new="if (v.adu_power != s.adu_power || v.px_power != s.px_power) {",
         cmd="units",
         freeze="FZ-P3-BUNIT-QUADRATIC"),
    dict(id="MUT-03-bunit-decidable", path="src/v6_bunit.cpp",
         old="  if (e.px_power != 0) {\n    out.decidable = true;\n    out.reason = \"explicit solid-angle power\";\n    return out;\n  }",
         new="  if (e.px_power == 0) {\n    out.decidable = true;\n    out.reason = \"explicit solid-angle power\";\n    return out;\n  }",
         cmd="units",
         freeze="FZ-BUNIT-SEMANTICS"),
    dict(id="MUT-04-prov-required-fcf", path="src/v6_provenance.cpp",
         old='      "correlation_summary", "flux_conservation_factor", "k_corr",',
         new='      "correlation_summary", "k_corr",',
         cmd="provenance",
         freeze="FZ-PROV-MINIMAL-SET"),
    dict(id="MUT-05-prov-required-kcorr", path="src/v6_provenance.cpp",
         old='      "correlation_summary", "flux_conservation_factor", "k_corr",\n      "generated_utc",       "output_hash"};',
         new='      "correlation_summary", "flux_conservation_factor",\n      "generated_utc",       "output_hash"};',
         cmd="provenance",
         freeze="FZ-PROV-MINIMAL-SET"),
    dict(id="MUT-06-kcorr-one-allowed", path="src/v6_provenance.cpp",
         old="    } else if (std::fabs(value - 1.0) <= 1e-12) {",
         new="    } else if (false) {",
         cmd="provenance",
         freeze="FZ-PROV-KCORR"),
    dict(id="MUT-07-unavailable-placeholder", path="src/v6_provenance.cpp",
         old="    } else if (flag && is_placeholder_reason(reason)) {",
         new="    } else if (false) {",
         cmd="provenance",
         freeze="ADJ-GEN-03"),
    dict(id="MUT-08-fits-checksum-encode", path="src/v6_fits.cpp",
         old="  const std::array<char, 16> enc =\n      fits_encode_checksum(0xFFFFFFFFu - total);",
         new="  const std::array<char, 16> enc = fits_encode_checksum(total);",
         cmd="fits",
         freeze="ALG-P3-008"),
    dict(id="MUT-09-atomic-verify-not-removed", path="src/v6_atomic_publish.cpp",
         old="      if (opts.remove_on_verify_failure) {\n        unlink_path(target);",
         new="      if (false) {\n        unlink_path(target);",
         cmd="atomic",
         freeze="ALG-P3-008"),
    dict(id="MUT-10-atomic-staging-residue", path="src/v6_atomic_publish.cpp",
         old="  if (!build_ok || cancelled(cancel)) {\n    remove_tree(staging);",
         new="  if (!build_ok || cancelled(cancel)) {\n    /* injected: keep staging */",
         cmd="atomic",
         freeze="ALG-P3-008"),
    dict(id="MUT-11-manifest-role", path="src/v6_hips_manifest.cpp",
         old='      "rejection", "point_information", "psf", "manifest", "properties", "tile"};',
         new='      "rejection", "point_information", "psf", "manifest", "properties", "tile", "weight"};',
         cmd="manifest",
         freeze="ALG-P3-008"),
    dict(id="MUT-12-props-creator-not-required", path="src/v6_hips_manifest.cpp",
         old='  const char* kRequired[] = {"creator_did",       "obs_collection",',
         new='  const char* kRequired[] = {"obs_collection",',
         cmd="manifest",
         freeze="ALG-P3-008"),
]


def run(cmd, timeout=900):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, timeout=timeout)


def main(argv):
    if len(argv) != 3:
        print("usage: mutate_and_check.py <repo_root> <workdir>")
        return 2
    repo_root, workdir = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    src_root = os.path.join(repo_root, "lib/infrastructure/aio/v6")
    tests_dir = os.path.join(repo_root, "eng/tests/unit/v6_aio")
    third = os.path.join(repo_root, "lib", "third_party")
    os.makedirs(workdir, exist_ok=True)

    def build_and_run(lib_dir, tag, subcmd):
        build = os.path.join(workdir, "build_" + tag)
        shutil.rmtree(build, ignore_errors=True)
        cfg = run(["cmake", "-S", tests_dir, "-B", build,
                   "-DCMAKE_BUILD_TYPE=Release",
                   "-DAIO_V6_LIB_DIR=" + lib_dir,
                   "-DAIO_V6_THIRD_PARTY_DIR=" + third])
        if cfg.returncode != 0:
            return None, "cmake configure failed:\n" + cfg.stdout[-2000:]
        bld = run(["cmake", "--build", build, "--target", "v6_aio_test", "-j", "8"])
        if bld.returncode != 0:
            return 1, "build failed (rc=1):\n" + bld.stdout[-2000:]
        exe = os.path.join(build, "v6_aio_test")
        rundir = os.path.join(workdir, "run_" + tag)
        shutil.rmtree(rundir, ignore_errors=True)
        os.makedirs(rundir, exist_ok=True)
        r = run([exe, subcmd, rundir])
        return r.returncode, r.stdout[-2000:]

    # ── 正控制：未变异源码全绿 ──────────────────────────────────────────
    ctrl_rc, ctrl_out = build_and_run(src_root, "control", "all")
    if ctrl_rc != 0:
        print("CONTROL FAIL: unmutated implementation rc=%s\n%s" % (ctrl_rc, ctrl_out))
        return 1
    print("CONTROL OK: unmutated implementation 'all' rc=0")

    missed = []
    for m in MUTATIONS:
        mut_v6 = os.path.join(workdir, "mut_" + m["id"], "v6")
        shutil.rmtree(os.path.dirname(mut_v6), ignore_errors=True)
        shutil.copytree(src_root, mut_v6)
        target = os.path.join(mut_v6, m["path"])
        text = open(target, encoding="utf-8").read()
        if text.count(m["old"]) != 1:
            print("ANCHOR-MISS %-34s (%s: found %d)" %
                  (m["id"], m["path"], text.count(m["old"])))
            missed.append(m["id"])
            continue
        open(target, "w", encoding="utf-8").write(text.replace(m["old"], m["new"]))
        rc, out = build_and_run(mut_v6, m["id"], m["cmd"])
        if rc is None:
            print("BUILD-FAIL %-34s %s" % (m["id"], out))
            missed.append(m["id"])
            continue
        if rc != 0:
            print("CAUGHT %-34s subcmd=%-12s rc=%s freeze=%s" %
                  (m["id"], m["cmd"], rc, m["freeze"]))
        else:
            print("MISSED %-34s subcmd=%-12s rc=0 freeze=%s" %
                  (m["id"], m["cmd"], m["freeze"]))
            missed.append(m["id"])
    if missed:
        print("IMPL MUTATION FAIL: %d/%d not caught: %s" %
              (len(missed), len(MUTATIONS), missed))
        return 1
    print("IMPL MUTATION PASS: %d/%d caught" % (len(MUTATIONS), len(MUTATIONS)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
