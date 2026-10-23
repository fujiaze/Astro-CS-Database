cmake_minimum_required(VERSION 3.25)
project(ACSD VERSION 2.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ACSD_BUILD_SCIENCE "Build the ACSD v2 science implementation" OFF)
# The single internal-development diagnostics switch.  OFF must evaluate no
# probe argument at all; ON enables bounded, asynchronous probes only.
option(ACSD_DEV_MODE "Enable ACSD internal development probes" OFF)
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
  # The single owner of the flat value-level JSON codecs; the contract driver
  # and the generated runtime codecs both link it, so a value has one encoder.
  acsd_add_library(codec)
  acsd_link(codec model)
  if(ACSD_codec_IS_STATIC)
    target_link_libraries(codec PUBLIC nlohmann_json::nlohmann_json)
  else()
    target_link_libraries(codec INTERFACE nlohmann_json::nlohmann_json)
  endif()
  acsd_add_library(numerics)
  acsd_link(numerics model)

  acsd_add_library(resources)
  acsd_link(resources model Threads::Threads)

  # Fixed runtime: one ledger, one CPU pool, bounded tasks/pipes, asynchronous
  # Storage and the ProcessHost/registry.  This target is the only owner of
  # acsd::rt::Services; modules never create a second pool.
  acsd_add_library(runtime)
  acsd_link(runtime model numerics Threads::Threads PkgConfig::ZSTD)
  if(ACSD_runtime_IS_STATIC)
    target_compile_definitions(runtime PRIVATE
      ACSD_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    # manifest_main.cpp is an application entry point, never part of the
    # runtime archive.
    get_target_property(acsd_runtime_sources runtime SOURCES)
    if(acsd_runtime_sources)
      list(FILTER acsd_runtime_sources EXCLUDE REGEX "manifest_main\\.cpp$")
      # The generated assembly links the science modules, so it is not part of
      # the runtime archive; acsd_builtin owns it.
      list(FILTER acsd_runtime_sources EXCLUDE REGEX "/generated/")
      set_target_properties(runtime PROPERTIES SOURCES "${acsd_runtime_sources}")
    endif()
  endif()
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/validation/runtime/registry/host_lifecycle.cpp AND ACSD_runtime_IS_STATIC)
    add_executable(acsd_host_lifecycle
      validation/runtime/registry/host_lifecycle.cpp)
    target_link_libraries(acsd_host_lifecycle PRIVATE runtime)
    add_test(NAME host_lifecycle COMMAND acsd_host_lifecycle
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endif()
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/validation/runtime/registry/registry_negative.cpp AND ACSD_runtime_IS_STATIC)
    add_executable(acsd_registry_negative
      validation/runtime/registry/registry_negative.cpp)
    target_link_libraries(acsd_registry_negative PRIVATE runtime)
    add_test(NAME registry_negative COMMAND acsd_registry_negative
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endif()
  if(ACSD_runtime_IS_STATIC)
    target_compile_definitions(runtime PUBLIC ACSD_DEV_MODE=$<BOOL:${ACSD_DEV_MODE}>)
  else()
    target_compile_definitions(runtime INTERFACE ACSD_DEV_MODE=$<BOOL:${ACSD_DEV_MODE}>)
  endif()

  acsd_add_library(storage)
  acsd_link(storage model PkgConfig::ZSTD)

  acsd_add_library(operators)
  acsd_link(operators model numerics)

  acsd_add_library(uncertainty)
  acsd_link(uncertainty operators numerics)

  acsd_add_library(coordinates)
  acsd_link(coordinates model numerics runtime)

  acsd_add_library(observation)
  acsd_link(observation model numerics operators uncertainty runtime)

  acsd_add_library(catalogue)
  acsd_link(catalogue model coordinates numerics uncertainty runtime)

  acsd_add_library(response)
  acsd_link(response model numerics runtime)

  acsd_add_library(sources)
  acsd_link(sources observation coordinates catalogue uncertainty numerics runtime)

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

  # The generated typed assembly: 22 process descriptors + 57 typed bindings,
  # codecs and the builtin registration.  It links the science modules, so it is
  # a separate aggregate target, never part of the runtime archive.
  set(ACSD_GENERATED_ASSEMBLY
      ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/generated/builtin_processes.cpp)
  if(EXISTS ${ACSD_GENERATED_ASSEMBLY})
    file(GLOB ACSD_GENERATED_SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/generated/*.cpp)
    add_library(acsd_builtin STATIC ${ACSD_GENERATED_SOURCES})
    target_include_directories(acsd_builtin PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(acsd_builtin PUBLIC runtime codec acsd_science
                          nlohmann_json::nlohmann_json)
    target_compile_definitions(acsd_builtin PUBLIC
                               ACSD_HAS_GENERATED_ASSEMBLY=1)
  endif()

  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/manifest_main.cpp AND EXISTS ${ACSD_GENERATED_ASSEMBLY})
    add_executable(acsd-runtime-manifest src/runtime/manifest_main.cpp)
    target_link_libraries(acsd-runtime-manifest PRIVATE acsd_builtin)
    # The frozen manifest is a build product; keep it next to the binaries so
    # the coverage check never depends on a hand-run command.
    add_custom_target(process_manifest
      COMMAND acsd-runtime-manifest --output
              ${CMAKE_BINARY_DIR}/process-manifest.json
      DEPENDS acsd-runtime-manifest
      COMMENT "Exporting the frozen process manifest")
  endif()
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/validation/runtime/registry/typed_smoke.cpp AND TARGET acsd_builtin)
    add_executable(acsd_typed_smoke
      validation/runtime/registry/typed_smoke.cpp)
    target_link_libraries(acsd_typed_smoke PRIVATE acsd_builtin)
    add_test(NAME typed_smoke COMMAND acsd_typed_smoke
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  if(ACSD_BUILD_CONTRACT_DRIVER AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/apps/contract/main.cpp)
    add_executable(acsd-contract apps/contract/main.cpp apps/contract/driver.cpp
                           apps/contract/json_io.cpp)
    target_link_libraries(acsd-contract PRIVATE acsd_science codec
                          nlohmann_json::nlohmann_json)
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

if(ACSD_BUILD_SCIENCE AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/validation/lead/covariance_selftest.cpp)
  add_executable(acsd_covariance_selftest validation/lead/covariance_selftest.cpp)
  target_link_libraries(acsd_covariance_selftest PRIVATE uncertainty operators resources storage model)
  add_test(NAME covariance_selftest COMMAND acsd_covariance_selftest)
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
