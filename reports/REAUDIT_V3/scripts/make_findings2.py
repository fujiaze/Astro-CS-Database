#!/usr/bin/env python3
"""Compile findings.csv (Control §17) from evidence gathered this round."""
import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
OUT = os.path.join(ROOT, "package", "14_findings")
os.makedirs(OUT, exist_ok=True)

fields = ["id","severity","status","category","module","path","line","symbol","claim","observed","expected","reproduction","impact","proposed_fix","verification","evidence"]
rows = []

def add(r):
    rows.append(r)

add(["P0-01","P0","CONFIRMED","execution_architecture","phase2","lib/phase2/CMakeLists.txt","18,56-58","P2_ENABLE_OPENMP / sampler",
      "docs describe Phase2 as OpenMP/Dispatcher/Mixed production architecture",
      "At HEAD P2_ENABLE_OPENMP option default OFF; when ON only target_link_libraries(OpenMP::OpenMP_CXX) runs - no target_compile_definitions(P2_ENABLE_OPENMP). sampler.cpp guard (L30) never activates; zero #pragma omp in lib/phase2. stage2 uses bridge/executor_create + legacy_parallel pointers, not Dispatcher (no dispatcher call in lib/phase2).",
      "OpenMP pragma regions + macro defined for target + production Dispatcher call path",
      "cmake -DP2_ENABLE_OPENMP=ON produces sampler.cpp compile command WITHOUT -DP2_ENABLE_OPENMP (compile_commands.json); git grep #pragma omp in lib/phase2 = 0.",
      "Docs overstate parallelism; users expect multi-thread speedup that cannot happen",
      "Either wire real OpenMP/Dispatcher or correct docs to serial-CPU",
      "compile_commands.json check + git grep; ctest phase2 79 PASS in serial",
      "compile_commands.json from builds/phase2_ompON_probe; execution_static_evidence.txt"])

add(["P0-02","P0","CONFIRMED","checker_truthfulness","quality","tools/quality/contracts/check_api_contracts.py","67-73","signature mismatch no-op",
      "check_api_contracts verifies full API signature consistency",
      "Signature mismatch branch is literal pass (L71-73); mutation swapping parameter order in a real API row -> checker PASS exit=0. 14/22 API_CONTRACTS columns identical across all 422 rows; all status=VERIFIED.",
      "checker flags signature/param order/const/noexcept mismatches",
      "mutation_harness api_param_order_swap exit=0",
      "Contract checkers produce false PASS; 422-row API table is template-filled",
      "Implement real AST comparison + per-interface semantics",
      "mutation harness + per-column distinct count",
      "checker_truthfulness.csv; API_CONTRACTS.csv column analysis"])

add(["P0-03","P0","CONFIRMED","api_contract","all","docs/contracts/API_CONTRACTS.csv","all","API_CONTRACTS rows",
      "422 API contracts are per-interface definitions",
      "14/22 columns identical across all 422 rows; all status=VERIFIED; config_keys empty for all rows; 63/67 TRACEABILITY rows have empty algorithm_id",
      "per-interface units/valid_range/lifetime/error_model semantics",
      "csv column distinct-value count (TOTAL_COLS 22, IDENTICAL_ALL 14)",
      "Contract table not a real semantic contract",
      "Regenerate per-interface; Clang AST extraction",
      "re-run with libclang-based extraction",
      "API_CONTRACTS.csv analysis output"])

add(["P1-01","P1","CONFIRMED","science_contract","phase2","docs/algorithms/UPM_SOLVER.md","17,48,52","Huber/complexity/OpenMP claims",
      "algorithm doc: Huber delta=1.345*median_abs_r; block eval OpenMP; complexity O(iter*(obs+K log K))",
      "source: z=r/sigma_eff dimensionless delta=1.345 (upm.cpp L580-590); p2_upm_calibrate_block serial for loop (L1055+); per-frame full-K CG max_cg=200 for (F-1) frames (upm.cpp L528-564,642-648)",
      "doc matches source",
      "static source reading at HEAD; see SCIENCE_ORACLES_SUMMARY.md",
      "doc-source science contradictions block science closure",
      "fix docs to match frozen formulas",
      "numeric oracle runs pending clean-build pipeline",
      "upm.cpp L580-590, L1055-1090; UPM_SOLVER.md L17,48,52"])

add(["P1-02","P1","CONFIRMED","science_contract","healpix_drizzle/astro_image_io","docs/science/DRIZZLE.md + aio_hips_writer.cpp","27 + 477,816,1035","Drizzle units/constant-field",
      "doc: S,F,x units ADU; constant field x=C -> S=C",
      "writer sig=flux/area (aio_hips_writer.cpp L477,816), product labeled surface brightness (L1035); S=F/D is ADU/area; S=C only on normalized equal-area grid",
      "dimensionally consistent doc",
      "static dimensional analysis",
      "unit/invariant contradiction blocks closure of surface-brightness semantics",
      "freeze definition x=integrated flux ADU, output surface brightness ADU/sr; fix doc",
      "numeric S=C oracle pending",
      "SCIENCE_ORACLES_SUMMARY.md 10.4"])

add(["P1-03","P1","CONFIRMED","traceability","all","docs/TRACEABILITY.csv","all","algorithm_id",
      "full SCI-ALG-API-SRC-TST chain coverage",
      "67 rows; only 4 non-empty algorithm_id; 63 empty",
      "complete chain links",
      "csv count (67 rows, 4 non-empty algorithm_id)",
      "traceability matrix not a complete chain",
      "fill ALG/API/TST links per interface",
      "checker trace_blank_all_algorithm_id mutation PASS (checker cannot detect)",
      "TRACEABILITY.csv analysis"])

add(["P1-04","P1","CONFIRMED","build_reproducibility","astro_image_io/phase2","lib/astro_image_io/Makefile + lib/phase2/CMakeLists.txt","69 + 65,76","clean Linux build",
      "documented command (make all) builds AIO DLL; phase2 CMake references ../astro_image_io/astro_image_io.dll",
      "clean make FAILS: TLS relocation R_X86_64_TPOFF32 against __tls_guard can not be used when making a shared object; recompile with -fPIC (Makefile compiles without -fPIC). Workaround (CXXFLAGS+=-fPIC) succeeds producing ELF shared object. phase2 CMake links the .dll path but never expresses AIO build order; BUILD_GRAPH.md references nonexistent lib/astro_image_io/CMakeLists.txt (tree has Makefile only).",
      "clean build succeeds with documented commands; build order expressed",
      "logs/aio_build_stderr.log; aio_build_fpic artifact ELF shared object",
      "clean-clone reproducibility FAIL; status FAIL_REPRODUCIBILITY for AIO documented command",
      "add -fPIC to Makefile flags and express AIO as phase2 dependency; fix BUILD_GRAPH.md reference",
      "re-run clean build from fresh git archive",
      "aio_build_stderr.log (TLS relocation), BUILD_META.txt, BUILD_GRAPH.md L15"])

add(["P1-05","P1","PARTIAL","test_execution","phase2","lib/phase2/tests/ivar_wiring_test.cpp","238","WireProductionStage2PerFrameIvar",
      "production stage2 per-frame ivar wiring test passes on all platforms",
      "ctest FAIL exit=32512: test invokes astrocs-stage2.exe (Windows exe name) which does not exist on Linux (sh: astrocs-stage2.exe: not found)",
      "test runs on Linux as astrocs-stage2",
      "ctest --test-dir builds/phase2_clean_release_ompOFF -j1 -> Phase2IvarWiring.WireProductionStage2PerFrameIvar Failed",
      "cross-platform test wiring broken; masks real ivar verification on Linux",
      "parameterize executable name by platform",
      "rerun ctest after fix",
      "logs/phase2_ctest_run.log"])

add(["P1-06","P1","PARTIAL","test_coverage","all","test inventory","-","206-item inventory",
      "206 test inventory items have current results",
      "This round only phase2 module 90 tests executed (79 PASS/1 FAIL/10 SKIP) on clean build; other modules not built/run this round",
      "all inventory items run on current clean checkout",
      "ctest phase2 run; other modules BLOCKED-not-built this round",
      "coverage incomplete for release acceptance",
      "build+run remaining modules",
      "test_execution_matrix.csv",
      "ctest run log"])

add(["P2-01","P2","CONFIRMED","performance","phase2","lib/phase2/tools/stage2.cpp","1085,782-783,636","std::vector<uint32_t> src_idx(depth)",
      "hot loops allocate per-pixel/per-chunk vectors",
      "src_idx(depth) constructed per pixel (L1085); per-chunk cal/supv (L782-783) and per-tile frames (L636) allocations inside tile/chunk loops",
      "no allocation in hot loop",
      "static reading; heap-allocation profiling not yet run (recorded, not estimated)",
      "high allocation churn; consistent with low CPU utilization reported historically",
      "hoist buffers; add profiling gate",
      "profiler/allocator run before fixing (pending section 14)",
      "stage2.cpp L1085, L782-783"])

add(["P2-02","P2","CONFIRMED","portability","all","docs/contracts/API_CONTRACTS.csv + configs","all","Windows hardcoded paths",
      "portable cross-platform paths",
      "99 tracked files reference F:\\, C:\\Users, msys64, or .exe (e.g., stage1_gc_panel*_Red.json point to F:\\Astro dev\\... paths; ivar_wiring_test.cpp invokes astrocs-stage2.exe)",
      "path-neutral configs/tests",
      "git grep for Windows path patterns count=99",
      "Linux portability friction; real-data configs unusable as-is on Linux",
      "relativize configs; parameterize test exe names",
      "re-run Linux build/test after fix",
      "static_quality_scan.txt section 3"])

add(["P3-01","P3","CONFIRMED","gitlink","repo","AstroCS.wiki","-","mode 160000 gitlink",
      "AstroCS.wiki is a working submodule with valid target",
      "AstroCS.wiki tracked as mode 160000 gitlink sha 901725847ded7d1185a98b995df8fce9a7e20a1e; .gitmodules absent; gitlink target object NOT present in repo (cat-file fails); git submodule status fatal",
      "either .gitmodules+object present or no gitlink",
      "git ls-tree -r HEAD shows 160000 entry; git cat-file -e fails",
      "dangling gitlink; wiki content unavailable",
      "decide: add .gitmodules+submodule or remove gitlink (decision, not this audit round)",
      "verify with git submodule status",
      "submodules.txt in 00_identity"])

with open(os.path.join(OUT, "findings.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f); w.writerow(fields); w.writerows(rows)
print("findings.csv rows=" + str(len(rows)))
print("by severity: " + str({s: sum(1 for r in rows if r[1]==s) for s in set(r[1] for r in rows)}))
