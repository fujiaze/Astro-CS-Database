cmake_minimum_required(VERSION 3.25)
project(ACSD VERSION 2.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ACSD_BUILD_SCIENCE "Build the ACSD v2 science implementation" OFF)
option(ACSD_BUILD_CONTRACT_DRIVER "Build the acsd-contract driver" ON)

add_library(acsd_contracts INTERFACE)
target_include_directories(acsd_contracts INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(acsd_contracts INTERFACE cxx_std_20)

if(ACSD_BUILD_SCIENCE)
  find_package(Threads REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)
  pkg_check_modules(CFITSIO REQUIRED IMPORTED_TARGET cfitsio)
  find_package(nlohmann_json REQUIRED)

  # ------------------------------------------------------------------
  # Common compile settings for every science target.
  #   * no fast-math: the interval primitives rely on IEEE semantics
  #   * FP contraction off: expressions must not be fused silently
  #   * -mfpmath=sse keeps x87 extended precision off the x86-64 path
  # ------------------------------------------------------------------
  set(ACSD_NUMERIC_FLAGS -O2 -fno-fast-math -ffp-contract=off)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    list(APPEND ACSD_NUMERIC_FLAGS -frounding-math -mfpmath=sse)
  endif()

  function(acsd_add_library target)
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/${target}/*.cpp)
    if(sources)
      add_library(${target} STATIC ${sources})
      target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
                                                   ${CMAKE_CURRENT_SOURCE_DIR}/include)
      target_link_libraries(${target} PUBLIC acsd_contracts)
      target_compile_options(${target} PRIVATE ${ACSD_NUMERIC_FLAGS})
      set(ACSD_${target}_IS_STATIC ON PARENT_SCOPE)
    else()
      # The module directory exists but no implementation has landed yet; keep
      # the dependency graph explicit without creating an empty archive.
      add_library(${target} INTERFACE)
      target_link_libraries(${target} INTERFACE acsd_contracts)
      set(ACSD_${target}_IS_STATIC OFF PARENT_SCOPE)
    endif()
  endfunction()

  function(acsd_link target)
    if(ACSD_${target}_IS_STATIC)
      target_link_libraries(${target} PUBLIC ${ARGN})
    else()
      target_link_libraries(${target} INTERFACE ${ARGN})
    endif()
  endfunction()

  acsd_add_library(model)
  acsd_add_library(numerics)
  acsd_link(numerics model)

  acsd_add_library(resources)
  acsd_link(resources model Threads::Threads)

  acsd_add_library(storage)
  acsd_link(storage model PkgConfig::ZSTD)

  acsd_add_library(operators)
  acsd_link(operators model numerics)

  acsd_add_library(uncertainty)
  acsd_link(uncertainty operators numerics)

  acsd_add_library(coordinates)
  acsd_link(coordinates model numerics)

  acsd_add_library(observation)
  acsd_link(observation model numerics operators uncertainty)

  acsd_add_library(catalogue)
  acsd_link(catalogue model coordinates numerics uncertainty)

  acsd_add_library(response)
  acsd_link(response model numerics)

  acsd_add_library(sources)
  acsd_link(sources observation coordinates catalogue uncertainty numerics)

  acsd_add_library(photometry)
  acsd_link(photometry sources response uncertainty numerics)

  acsd_add_library(geometry)
  acsd_link(geometry coordinates operators numerics model)

  acsd_add_library(information)
  acsd_link(information uncertainty numerics operators)

  acsd_add_library(sky)
  acsd_link(sky uncertainty information operators numerics)

  acsd_add_library(coadd)
  acsd_link(coadd sky uncertainty operators numerics)

  acsd_add_library(projection)
  acsd_link(projection geometry operators uncertainty numerics)

  acsd_add_library(workflows)
  acsd_link(workflows sources photometry geometry information sky coadd
                                      projection response operators uncertainty resources storage
                                      model numerics coordinates observation catalogue)

  # The observation module binds the world coordinate solution and the
  # observation model to the observation.  The capability is detected from the
  # sources of the module: while an entry point is missing the normalize
  # workflow answers with the declared code (unsupported) instead of the build
  # failing with an undefined symbol.
  set(ACSD_OBSERVATION_BINDING OFF)
  file(GLOB ACSD_OBSERVATION_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/observation/*.cpp)
  foreach(ACSD_OBSERVATION_SOURCE ${ACSD_OBSERVATION_SOURCES})
    file(READ ${ACSD_OBSERVATION_SOURCE} ACSD_OBSERVATION_CONTENT)
    if(ACSD_OBSERVATION_CONTENT MATCHES "bind_wcs[ \\t]*\\(" AND
       ACSD_OBSERVATION_CONTENT MATCHES "bind_observation_model[ \\t]*\\(")
      set(ACSD_OBSERVATION_BINDING ON)
    endif()
  endforeach()
  if(ACSD_OBSERVATION_BINDING)
    target_compile_definitions(workflows PRIVATE ACSD_HAVE_OBSERVATION_BINDING=1)
    message(STATUS "the observation binding of the world coordinate solution is available")
  else()
    message(STATUS "the observation binding of the world coordinate solution is missing: "
                   "the normalize workflow answers unsupported")
  endif()

  add_library(acsd_science INTERFACE)
  target_link_libraries(acsd_science INTERFACE workflows)

  if(ACSD_BUILD_CONTRACT_DRIVER AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/apps/contract/main.cpp)
    add_executable(acsd-contract apps/contract/main.cpp apps/contract/driver.cpp
                           apps/contract/json_io.cpp)
    target_link_libraries(acsd-contract PRIVATE acsd_science nlohmann_json::nlohmann_json)
    target_include_directories(acsd-contract PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  endif()

  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/apps/cli/main.cpp)
  add_executable(acsd apps/cli/main.cpp apps/cli/config.cpp apps/cli/events.cpp apps/cli/commands.cpp)
  target_link_libraries(acsd PRIVATE acsd_science nlohmann_json::nlohmann_json PkgConfig::CFITSIO)
  target_include_directories(acsd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  endif()
endif()

if(ACSD_BUILD_SCIENCE AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/validation/lead/numerics_selftest.cpp)
  add_executable(acsd_numerics_selftest validation/lead/numerics_selftest.cpp)
  target_link_libraries(acsd_numerics_selftest PRIVATE numerics)
  add_test(NAME numerics_selftest COMMAND acsd_numerics_selftest)
endif()

if(ACSD_BUILD_SCIENCE AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/validation/lead/operators_selftest.cpp)
  add_executable(acsd_operators_selftest validation/lead/operators_selftest.cpp)
  target_link_libraries(acsd_operators_selftest PRIVATE operators resources storage model)
  add_test(NAME operators_selftest COMMAND acsd_operators_selftest)
endif()


if(ACSD_BUILD_SCIENCE AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/validation/lead/healpix_dump.cpp)
  add_executable(acsd_healpix_dump validation/lead/healpix_dump.cpp)
  target_link_libraries(acsd_healpix_dump PRIVATE coordinates)
endif()

add_library(acsd_contracts_compile OBJECT validation/contracts_compile.cpp)
target_link_libraries(acsd_contracts_compile PRIVATE acsd_contracts)
find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)
add_custom_target(check_design ALL
 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_design.py
 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} VERBATIM)
enable_testing()
add_test(NAME design_integrity COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_design.py)
