cmake_minimum_required(VERSION 3.16)
project(astrocs_cli LANGUAGES CXX C)

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

set(AIO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io)
file(GLOB CFITSIO_SRCS ${AIO_DIR}/third_party/cfitsio/*.c)
list(FILTER CFITSIO_SRCS EXCLUDE REGEX "f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|imcopy|imarith|tabcompile|sortcol|tabselect")
list(LENGTH CFITSIO_SRCS _cfn)
message(STATUS "ASTROCS_CFITSIO_n=${_cfn}")
set_property(SOURCE ${CFITSIO_SRCS} PROPERTY COMPILE_OPTIONS "-w")
set(AIO_SRCS ${AIO_DIR}/src/aio_fits.cpp ${AIO_DIR}/src/aio_api.cpp ${AIO_DIR}/src/aio_log.cpp ${AIO_DIR}/src/aio_compressor.cpp)
# aio_api 需要完整 AIOImageData(src/aio_fits.h)
set_property(SOURCE ${AIO_DIR}/src/aio_api.cpp APPEND PROPERTY INCLUDE_DIRECTORIES "${AIO_DIR}/src")
set_property(SOURCE ${AIO_SRCS} PROPERTY COMPILE_OPTIONS "-w;-DAIO_ENABLE_FITS")
set(CAL_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../lib/calibration/src/calibrator.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/calibration/src/master_generator.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/calibration/src/cosmetic_corrector.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/calibration/src/dark_optimizer.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/calibration/src/ac_api.cpp)
set_source_files_properties(${CAL_SRCS} PROPERTIES COMPILE_OPTIONS "-fopenmp")
add_executable(astrocs main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/common/crypto/sha256.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/cpu_features.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/hardware_inspect.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/backend_loader.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/bench_harness.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/profile_gen.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/baseline_backend.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/host_services.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase1_session/p1_session.cpp ${AIO_SRCS} ${CFITSIO_SRCS} ${CAL_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version_generated.h)
target_link_libraries(astrocs PRIVATE gomp z zstd lz4 m)
# vendored/per-lib includes(aio/calibration/cfitsio) — 只影响对应源
set_property(SOURCE ${AIO_SRCS} ${CFITSIO_SRCS} ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase1_session/p1_session.cpp PROPERTY INCLUDE_DIRECTORIES "${AIO_DIR}/include;${AIO_DIR}/third_party/cfitsio;${CMAKE_CURRENT_SOURCE_DIR}/../lib/calibration/include;${CMAKE_CURRENT_SOURCE_DIR}/../include;${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
set_property(SOURCE ${CAL_SRCS} PROPERTY INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../lib/calibration/include;${AIO_DIR}/include;${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(astrocs PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../third_party ${CMAKE_CURRENT_SOURCE_DIR}/../lib/common/crypto ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase1_session ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_compile_options(astrocs PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra>
  $<$<CXX_COMPILER_ID:MSVC>:/W4;/EHsc;/utf-8>)

# 发布安装规则: 恰一个 exe (04/CLI-001)
install(TARGETS astrocs RUNTIME DESTINATION bin)
