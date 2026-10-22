#!/usr/bin/env bash
# Build the independent native (in-memory public API) drivers.
#
# The drivers themselves only include <acsd/contracts.hpp>; the production
# translation units are compiled separately with their own include path so
# that no production internal header ever enters the validation sources.
#
# Each driver declares the production modules it needs.  A module that does
# not compile at the moment (another agent may be mid-edit) only disables the
# drivers that need it; the validation records that as pending_implementation
# instead of failing the whole run.
#
# Usage: validation/lib/build_native.sh [driver-name ...]
set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
validation_root="$(cd "${here}/.." && pwd)"
repo_root="$(cd "${validation_root}/.." && pwd)"
build_dir="${validation_root}/build"
obj_dir="${build_dir}/obj"

mkdir -p "${obj_dir}"

CXX="${CXX:-g++}"
CXXFLAGS="-std=c++20 -O2 -fno-fast-math -ffp-contract=off -frounding-math -mfpmath=sse"
CXXFLAGS="${CXXFLAGS} -Wno-unused-parameter"

ALL_MODULES="model numerics resources storage operators uncertainty coordinates geometry \
observation catalogue sources response photometry information sky coadd projection workflows"

modules_for_driver() {
  case "$1" in
    u01_driver) echo "model numerics operators uncertainty" ;;
    c01_driver) echo "model numerics operators uncertainty coordinates" ;;
    g01_driver) echo "model numerics operators uncertainty coordinates geometry" ;;
    f01_driver) echo "model numerics operators uncertainty coordinates observation sources response photometry" ;;
    s01_driver) echo "model numerics operators uncertainty coordinates observation sources sky information" ;;
    w01_driver) echo "model numerics operators uncertainty coordinates observation sources sky coadd information" ;;
    v01_driver) echo "model numerics operators uncertainty coordinates observation sources information geometry" ;;
    *) echo "${ALL_MODULES}" ;;
  esac
}

MODULE_OBJECTS=()

compile_module() {
  local module="$1"
  local failed=0
  local module_source
  while IFS= read -r module_source; do
    local relative="${module_source#${repo_root}/src/}"
    local object="${obj_dir}/$(echo "${relative}" | tr '/' '_' | sed 's/\.cpp$/.o/')"
    if [[ ! -f "${object}" || "${module_source}" -nt "${object}" ]]; then
      if ! "${CXX}" ${CXXFLAGS} -I "${repo_root}/src" -I "${repo_root}/include" \
          -c "${module_source}" -o "${object}" 2>"${object}.log"; then
        echo "  module ${module}: ${relative} does not compile" >&2
        tail -5 "${object}.log" >&2
        failed=1
        break
      fi
    fi
    MODULE_OBJECTS+=("${object}")
  done < <(find "${repo_root}/src/${module}" -name '*.cpp' 2>/dev/null | sort)
  return ${failed}
}

drivers=("$@")
if [[ ${#drivers[@]} -eq 0 ]]; then
  drivers=(u01_driver)
fi

status=0
for driver in "${drivers[@]}"; do
  source="${validation_root}/native/${driver}.cpp"
  if [[ ! -f "${source}" ]]; then
    echo "no such driver: ${source}" >&2
    status=1
    continue
  fi
  MODULE_OBJECTS=()
  ok=1
  for module in $(modules_for_driver "${driver}"); do
    if ! compile_module "${module}"; then
      ok=0
      break
    fi
  done
  if [[ ${ok} -eq 0 ]]; then
    echo "driver ${driver}: a required production module does not compile now" >&2
    status=1
    continue
  fi
  echo "building ${driver}"
  if ! "${CXX}" ${CXXFLAGS} -I "${repo_root}/include" "${source}" "${MODULE_OBJECTS[@]}" \
      -lpthread -lzstd -o "${build_dir}/${driver}"; then
    echo "driver ${driver}: link failed" >&2
    status=1
    continue
  fi
done

echo "native drivers built in ${build_dir}"
exit ${status}
