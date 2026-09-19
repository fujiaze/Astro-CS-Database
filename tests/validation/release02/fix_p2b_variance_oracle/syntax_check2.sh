#!/usr/bin/env bash
set -uo pipefail
cd "/workspace/Astro CS Database"
export TMPDIR=/dev/shm/fix-p2b
check() {
  local tgt="$1"; local src="$2"; shift 2
  local DEFINES INCLUDES
  DEFINES=$(awk -v t="$tgt" 'index($0,t":")>0{f=1} f&&/^  DEFINES =/{sub(/^  DEFINES = /,"");print;exit}' build/build.ninja)
  INCLUDES=$(awk -v t="$tgt" 'index($0,t":")>0{f=1} f&&/^  INCLUDES =/{sub(/^  INCLUDES = /,"");print;exit}' build/build.ninja)
  local extra=""
  for d in "$@"; do extra="$extra -I\"$d\""; done
  local OUT=/tmp/p2b_syn_$(basename "$src").log
  eval g++ $DEFINES $INCLUDES $extra -O1 -DNDEBUG -std=gnu++17 -fopenmp -Wall -Wextra -Wpedantic -Wconversion -fsyntax-only "$src" > "$OUT" 2>&1
  local RC=$?
  echo "== $src rc=$RC"
  if [ $RC -ne 0 ]; then head -60 "$OUT"; fi
}
check "CMakeFiles/astrocs_phase2.dir/lib/algorithms/coverage/src/sampler.cpp.o" lib/algorithms/coverage/src/sampler.cpp
check "CMakeFiles/astrocs_drizzle.dir/lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp.o" lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp
check "lib/algorithms/integration/v6/CMakeFiles/astrocs_v6_phase2_integrate.dir/src/weight_chain.cpp.o" lib/algorithms/integration/v6/src/weight_chain.cpp
check "lib/algorithms/integration/v6/CMakeFiles/astrocs_v6_phase2_integrate.dir/src/phase2_integrate.cpp.o" lib/algorithms/integration/v6/src/phase2_integrate.cpp
