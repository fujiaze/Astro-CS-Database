#!/usr/bin/env bash
set -u
cd "/home/lighthouse/Astro CS Database"
P="/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/package/09_execution"
mkdir -p "$P"
{
  echo "# Phase2/ACR execution static evidence (Control §11.1) at HEAD 535e73879662"
  echo "collected_utc=$(date -u +%FT%TZ)"
  echo ""
  echo "## 1. #pragma omp count in lib/ (all modules)"
  echo "count=$(git grep -n "#pragma omp" HEAD -- lib/ | wc -l)"
  git grep -n "#pragma omp" HEAD -- lib/ | head -30
  echo ""
  echo "## 2. OpenMP compile definitions / link options in phase2 CMakeLists"
  grep -n "P2_ENABLE_OPENMP\|OpenMP_CXX\|target_compile_definitions\|target_link_libraries" lib/phase2/CMakeLists.txt
  echo ""
  echo "## 3. std::thread / std::async / thread pool in lib/phase2 and lib/acr"
  echo "count=$(git grep -nE "std::thread|std::async|std::jthread|thread_pool|ThreadPool" HEAD -- lib/phase2 lib/acr | wc -l)"
  git grep -nE "std::thread|std::async|std::jthread|thread_pool|ThreadPool" HEAD -- lib/phase2 lib/acr | head -20
  echo ""
  echo "## 4. ACR kernel registration symbols"
  git grep -nE "register_kernel|kernel_registry|KernelRegistry|acr_register|register" HEAD -- lib/acr/api lib/phase2/src | grep -i kernel | head -15
  echo ""
  echo "## 5. Stage2 -> Dispatcher call sites"
  git grep -niE "dispatcher" HEAD -- lib/phase2 lib/acr/scheduler | head -20
  echo ""
  echo "## 6. Linux CUDA stub + bridge load"
  ls lib/phase2/src/cuda_bridge_stub.cpp 2>&1
  git grep -n "cuda_bridge_stub\|ensure_bridge_loaded\|gpu_exec\|executor_create" HEAD -- lib/phase2 | head -20
  echo ""
  echo "## 7. stage2.cpp hot-loop heap allocation (src_idx)"
  grep -n "std::vector<std::uint32_t> src_idx" lib/phase2/tools/stage2.cpp | head -5
  echo "src_idx_total=$(grep -c "std::vector<std::uint32_t> src_idx" lib/phase2/tools/stage2.cpp)"
  echo ""
  echo "## 8. hardcoded thread count references"
  git grep -nE "omp_set_num_threads|num_threads\(|set_num_threads" HEAD -- lib/ | head -10
  echo ""
} > "$P/execution_static_evidence.txt" 2>&1
echo "written: $(wc -l < "$P/execution_static_evidence.txt") lines"
