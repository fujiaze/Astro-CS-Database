#!/usr/bin/env python3
"""Append round-4 portability/build findings to findings.csv."""
import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "14_findings", "findings.csv")
rows = list(csv.reader(open(P, encoding="utf-8")))
hdr = rows[0]
new = [
["P1-07","P1","CONFIRMED","build_reproducibility","orchestrator","lib/orchestrator/cpp/src/orchestrator.cpp","1432,1530,1555,1675-1699,2155","Windows DLL loading APIs",
 "stage1 CLI builds on Linux clean source",
 "orchestrator.cpp calls GetProcAddress/LoadLibraryExA/FreeLibrary unconditionally (no _WIN32 guard, no Linux dlopen path); compile fails on Linux (errors at L1432,1530,1555,1675,1686,1695-1699,2155)",
 "Linux dlopen code path",
 "make LDFLAGS=-static at lib/orchestrator/cpp; logs/orchestrator_build2.log",
 "stage1 production CLI cannot be built on Linux without source changes -> 32R Stage1 pipeline NOT reproducible on Linux",
 "add dlopen-based loader for non-Windows or document Windows-only",
 "rebuild on Linux after fix",
 "logs/orchestrator_build2.log"],
["P2-03","P2","CONFIRMED","portability","healpix_drizzle","lib/healpix_db/healpix_drizzle/Makefile","68,72","-Wl,--stack Windows-only linker flag",
 "drizzle module links on Linux",
 "Makefile link rules hardcode -Wl,--stack,8388608 (GNU ld on Linux: unrecognized option '--stack'); not overridable via LDFLAGS; compile succeeds, link fails",
 "portable linker flags",
 "make CXXFLAGS+=... -j1 at lib/healpix_db/healpix_drizzle; logs/drizzle_build2.log",
 "drizzle FAIL_REPRODUCIBILITY on Linux without Makefile edit (forbidden this round)",
 "guard -Wl,--stack under Windows",
 "rebuild after Makefile fix",
 "logs/drizzle_build2.log"],
["P2-04","P2","CONFIRMED","portability","acr","lib/acr/utilization/system_metrics.cpp","28","#include <windows.h> unconditional",
 "ACR subproject builds on Linux",
 "system_metrics.cpp includes <windows.h> without _WIN32 guard; ACR CMake build fails (fatal error windows.h); oneTBB deps build OK first",
 "guard Windows-only code",
 "cmake + cmake --build at lib/acr; logs/acr_build.log",
 "ACR subproject FAIL_REPRODUCIBILITY on Linux",
 "wrap windows.h/NVML usage in _WIN32 guard or provide Linux stubs",
 "rebuild after fix",
 "logs/acr_build.log"],
["P2-05","P2","CONFIRMED","portability","phase2","lib/phase2/tests/ivar_wiring_test.cpp","238","astrocs-stage2.exe Windows exe name",
 "tests run on Linux with Linux executable name",
 "test hardcodes astrocs-stage2.exe; fails on Linux (sh: not found rc=32512) - see P1-05",
 "platform-parameterized exe name",
 "ctest phase2; logs/phase2_ctest_run.log",
 "ivar wiring verification masked on Linux",
 "use configurable exe name",
 "rerun ctest",
 "logs/phase2_ctest_run.log"],
]
seen = set(r[0] for r in rows)
added = 0
for nr in new:
    if nr[0] not in seen:
        rows.append(nr); seen.add(nr[0]); added += 1
with open(P, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f); w.writerows(rows)
from collections import Counter
c = Counter(r[1] for r in rows[1:])
print("findings.csv rows=" + str(len(rows)-1) + " added=" + str(added) + " by_severity=" + str(dict(c)))
