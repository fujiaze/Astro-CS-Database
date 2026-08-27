#!/usr/bin/env python3
"""Compile checker_truthfulness.csv from real-runs + mutation harnesses + source inspection."""
import csv, os, json
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
OUT = os.path.join(ROOT, "package", "06_checker_truthfulness")

fields = ["checker","claimed_scope","positive_result","negative_result","mutation_result","false_positive","false_negative","usable_as_gate","source_issue","evidence"]
rows = []
def add(checker, scope, pos, neg, mut, fp, fn, gate, issue, ev):
    rows.append([checker, scope, pos, neg, mut, fp, fn, gate, issue, ev])

add("check_api_contracts",
    "API full signature vs AST; param order/type/const/noexcept/linkage/export macro",
    "REAL_REPO: PASS exit=0 (422 rows, 418 ast symbols)",
    "REAL_REPO invalid fixture: none exercised in this run",
    "MUTATION api_param_order_swap: swapped first two params in a real API signature -> checker PASS exit=0 (not caught)",
    "no", "YES", "NO",
    "signature mismatch branch is a literal pass (check_api_contracts.py L67-73); only checks symbol exists in AST, non-empty sig, header file exists, count>=300",
    "tools/quality/contracts/check_api_contracts.py L59-82; mutation result exit=0")

add("check_science_units",
    "SCI/ALG/API units, precision, valid range dimensional consistency",
    "REAL_REPO: PASS exit=0",
    "n/a",
    "MUTATION sci_units_adu_to_second: ADU->second in DRIZZLE.md and PHASE2_UPM.md -> PASS exit=0 (not caught)",
    "no", "YES", "NO",
    "no dimensional analysis; only counts keyword occurrences (ADU/deg/pixel/rad/mag/dex >=20) and precision mentions (check_science_units.py L35-46); units column non-empty check only",
    "tools/quality/contracts/check_science_units.py L19-46; mutation exit=0")

add("check_execution_contracts",
    "execution model vs CMake defines, OpenMP pragma, thread pool/async/Dispatcher, log fields, test mapping",
    "REAL_REPO: PASS exit=0",
    "MUTATION exec_remove_all_critical: removed every critical(aio_read) token -> FAIL exit=1 (positive: literal string required)",
    "MUTATION exec_openmp_declared_no_pragma_source: repo source has NO #pragma omp parallel anywhere and macro P2_ENABLE_OPENMP is never defined for target -> checker PASS exit=0 (false negative)",
    "no", "YES", "NO",
    "only greps literal strings: requires critical(aio_read) in EXECUTION_MODEL.md (L32-34), option+link strings in CMake (L39-44), guard string in sampler.cpp (L49-51), dispatcher.cpp existence (L53-55); never verifies #pragma omp parallel exists, macro is compiled in, or production path uses Dispatcher",
    "tools/quality/contracts/check_execution_contracts.py L19-62; lib/phase2/CMakeLists.txt L18,56-58 (no target_compile_definitions); git grep #pragma omp in lib/ = 0 matches; mutations exit codes as recorded")

add("check_traceability",
    "ID unique, refs exist, core chain complete, no orphan SCI/API/SRC/TST; SCI->ALG->API->SRC->TST full chain",
    "REAL_REPO: PASS exit=0",
    "MUTATION trace_remove_upm_core_keyword: removed UPM from requirement IDs -> FAIL exit=1 (positive: core keyword detection)",
    "MUTATION trace_blank_all_algorithm_id: blanked algorithm_id for all rows -> PASS exit=0 (false negative: algorithm_id column never read)",
    "no", "YES", "NO",
    "only checks ID uniqueness, authority_doc file exists, core keyword substrings present in requirement_ids (check_traceability.py L34-61); never reads algorithm_id / test mapping / API mapping",
    "tools/quality/contracts/check_traceability.py L34-67; mutation exits as recorded")

add("check_doc_symbols",
    "backtick symbols, files, config keys in authoritative docs resolve to real files/symbols",
    "REAL_REPO: PASS exit=0",
    "n/a",
    "MUTATION doc_symbols_backticked_bad: backticked nonexistent p2_nonexistent_symbol_zzz + nonexistent_xyz_file_12345.cpp appended to PHASE2_UPM.md -> PASS exit=0 (false negative)",
    "no", "YES", "NO",
    "heuristic regex with many skips: ignores bare .cpp/.h/.json tokens (L74-76, L95-97), only scans backtick tokens, api_inventory.csv may be empty, UPPER_CASE-only symbol flagging (L103-116)",
    "tools/quality/contracts/check_doc_symbols.py L30-116; mutation exit=0")

add("check_config_contracts",
    "schema, examples, parser, defaults, enums, error messages consistent; single default source",
    "REAL_REPO: PASS exit=0",
    "n/a",
    "MUTATION config_unknown_key_added_valid: valid-JSON unknown top-level key added to lib/orchestrator/configs/stage1_gc_panel1_Red.json -> PASS exit=0 (false negative: orchestrator configs never scanned)",
    "no", "YES", "NO",
    "only scans lib/phase2/configs/*.json for inputs/integration keys + literal strings in stage2_common.cpp (weight_mode = 2, acr_route auto default, enum names, error msgs) (check_config_contracts.py L18-56); orchestrator/GC configs and schema validation out of scope",
    "tools/quality/contracts/check_config_contracts.py L18-56; mutation exit=0")

add("check_full_integration",
    "full production run; waivers []; P0/P1 = 0",
    "REAL_REPO: PASS exit=0 (aggregates generate_contract_report)",
    "n/a",
    "STATIC: status=DELIVERED counts as passed and returns 0 when only INTEG-P1-DEBT findings (hardcoded threads) remain (check_full_integration.py L43-47,59)",
    "no", "YES (DELIVERED-with-P1 treated as pass)", "NO",
    "waives known P1 debt as DELIVERED and treats it as passed; does not run production pipeline itself",
    "tools/quality/contracts/check_full_integration.py L28-59")

add("check_build_graph",
    "BUILD_GRAPH.md build nodes/deps match real build files",
    "REAL_REPO: PASS exit=0",
    "n/a",
    "MUTATION build_graph_nonexistent_cmake_reference: BUILD_GRAPH.md references nonexistent lib/astro_image_io/CMakeLists.txt (P1-04) -> PASS exit=0 (false negative: no CMakeLists existence check for referenced files)",
    "no", "YES", "NO",
    "does not verify referenced CMakeLists/objects exist in tree; P1-04 real state (lib/astro_image_io has Makefile, no CMakeLists.txt) still passes",
    "tools/quality/contracts/check_build_graph.py (probed via mutation); real tree lib/astro_image_io/CMakeLists.txt absent, BUILD_GRAPH.md L15 references it")

add("extract_cpp_api",
    "authoritative API extraction (claims Clang AST preference)",
    "REAL_REPO: exit=0, 418 symbols extracted",
    "n/a",
    "STATIC: implementation is regex-only (extract_cpp_api.py L16-38 main loop); NO clang AST code path exists despite docstring claim",
    "no", "n/a", "NO (for API-verification use)",
    "docstring claims Clang AST preference but main() never invokes clang; regex cannot parse multi-line signatures/overloads/const/noexcept reliably",
    "tools/quality/extract_cpp_api.py L1-10 (docstring) vs L47-77 (main)")

with open(os.path.join(OUT, "checker_truthfulness.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f); w.writerow(fields); w.writerows(rows)
print("checker_truthfulness.csv rows=" + str(len(rows)))
