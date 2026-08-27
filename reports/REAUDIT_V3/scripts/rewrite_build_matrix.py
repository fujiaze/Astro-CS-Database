import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
p = os.path.join(ROOT, "package", "04_build", "build_matrix.csv")
keys = ["layer","build_system","status","evidence","exit_code","notes"]
rows = [
["acr-subproject-cpu-release","CMake (lib/acr)","FAIL_REPRODUCIBILITY","logs/acr_build.log","fatal at system_metrics.cpp:28 windows.h","unconditional #include <windows.h> in lib/acr/utilization/system_metrics.cpp (not guarded by _WIN32); oneTBB deps fetched/built OK; utilization module fails on Linux"],
["astro_image_io-linux-release","Makefile (lib/astro_image_io)","FAIL_REPRODUCIBILITY(documented cmd) / OK(-fPIC workaround)","logs/aio_build_stderr.log + aio_build_fpic logs","doc cmd=2; workaround=0","Makefile CXXFLAGS lacks -fPIC; TLS relocation R_X86_64_TPOFF32 at link; -fPIC override yields ELF shared object sha256 6e57dc1a..."],
["phase2-subproject-linux-release","CMake (lib/phase2)","PASS","logs/phase2_cfg_ompOFF.log + phase2_build_ompOFF.log","0","Release P2_ENABLE_OPENMP=OFF; 0 warnings/0 errors; 6 executables; compile_commands 18 entries; needs AIO dll prebuilt (build order not expressed by CMake)"],
["phase2-subproject-linux-debug","CMake (lib/phase2)","PASS","logs/phase2_dbg_cfg.log + phase2_dbg_build.log","0","Debug build; 0 warnings/0 errors; astrocs-stage2 8629328 bytes"],
["phase2-subproject-asan-ubsan","CMake (lib/phase2)","PASS(build) / PASS(7 sanitizer tests)","logs/phase2_asan_cfg.log + phase2_asan_build.log + 05_tests/asan_ubsan/","0","-fsanitize=address,undefined; 7 synthetic_gate tests PASS, 0 ASan/UBSan errors; LSAN blocked by ptrace sandbox (documented)"],
["stage1-cli-production-chain-linux","Makefile (lib/orchestrator/cpp)","FAIL_REPRODUCIBILITY","logs/orchestrator_build2.log","fatal at orchestrator.cpp:1432 etc","orchestrator.cpp uses Windows DLL APIs unconditionally (GetProcAddress/LoadLibraryExA/FreeLibrary) with no Linux dlopen path; also -Wl,--stack flag Windows-only"],
["stage2-cli-production-chain-linux","CMake (lib/phase2 tools/stage2.cpp)","PASS","logs/phase2_build_ompOFF.log","0","astrocs-stage2 ELF pie built; sha256 371b0644...; no omp symbols; libgomp loaded transitively via AIO but unused (OPENMP_WIRING_FALSE)"],
["all-first-party-module-builds-linux","PARTIAL","FAIL(not all)","logs (calibration/drizzle)","calibration=0 drizzle=2","calibration cosmetic_corrector.dll OK (sha256 e27cbc1b...); healpix_drizzle link fails on -Wl,--stack (Windows-only flag, hardcoded in Makefile L68/72); plate_solve/dynamic_psf/photometric_calib/snr/gaia not built this round"],
["NO_ROOT_BUILD_SYSTEM","CONFIRMED","-","-","-","no root-level CMakeLists/preset; each module builds independently; phase2 CMake references ../astro_image_io/astro_image_io.dll without expressing AIO build order"],
]
with open(p, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f); w.writerow(keys); w.writerows(rows)
print("build_matrix rows:", len(rows))
