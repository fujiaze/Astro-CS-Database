cmake_minimum_required(VERSION 3.16)
project(astrocs_cli LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# V5: 禁全局 -march=/arch:AVX* (ARCH-003 §2); baseline 可加载性优先
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# 版本单源: VERSION + git commit → X.Y.Z-alpha.N+g<hash12>[.dirty] (tools/gen_version.py 同式)
set(BASE_VERSION "0.9.0-alpha.1")
execute_process(
  COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
  OUTPUT_VARIABLE GIT_COMMIT_FULL OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT GIT_COMMIT_FULL)
  set(GIT_COMMIT_FULL "0000000000000000000000000000000000000000")
endif()
execute_process(
  COMMAND git rev-parse --short=12 HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
  OUTPUT_VARIABLE GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT GIT_COMMIT)
  set(GIT_COMMIT "unknown")
endif()
execute_process(
  COMMAND git status --porcelain
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
  OUTPUT_VARIABLE GIT_DIRTY OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(GIT_DIRTY)
  set(GIT_DIRTY_SUFFIX ".dirty")
else()
  set(GIT_DIRTY_SUFFIX "")
endif()
set(ASTROCS_VERSION_STRING "${BASE_VERSION}+g${GIT_COMMIT}${GIT_DIRTY_SUFFIX}")

set(GENIN "version_generated.h.in")
configure_file(version_generated.h.in version_generated.h @ONLY)
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/version_generated.h
  "#define ASTROCS_COMMIT_SHA \"${GIT_COMMIT_FULL}\"\n")

add_executable(astrocs main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/common/crypto/sha256.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/cpu_features.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/hardware_inspect.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/backend_loader.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/bench_harness.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/profile_gen.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/host_services.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/baseline_backend.cpp ${CMAKE_CURRENT_BINARY_DIR}/version_generated.h)
target_include_directories(astrocs PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../third_party ${CMAKE_CURRENT_SOURCE_DIR}/../lib/common/crypto ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_compile_options(astrocs PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra>
  $<$<CXX_COMPILER_ID:MSVC>:/W4;/EHsc;/utf-8>)

# 发布安装规则: 恰一个 exe (04/CLI-001)
install(TARGETS astrocs RUNTIME DESTINATION bin)
