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
  find_package(ZSTD REQUIRED)
  find_package(CFITSIO REQUIRED)
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
    add_library(${target} STATIC ${sources})
    target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(${target} PUBLIC acsd_contracts)
    target_compile_options(${target} PRIVATE ${ACSD_NUMERIC_FLAGS})
  endfunction()

  acsd_add_library(model)
  acsd_add_library(numerics)
  target_link_libraries(numerics PUBLIC model)

  acsd_add_library(resources)
  target_link_libraries(resources PUBLIC model Threads::Threads)

  acsd_add_library(storage)
  target_link_libraries(storage PUBLIC model ZSTD::libzstd)

  acsd_add_library(operators)
  target_link_libraries(operators PUBLIC model numerics)

  acsd_add_library(uncertainty)
  target_link_libraries(uncertainty PUBLIC operators numerics)

  acsd_add_library(coordinates)
  target_link_libraries(coordinates PUBLIC model numerics)

  acsd_add_library(observation)
  target_link_libraries(observation PUBLIC model numerics operators uncertainty)

  acsd_add_library(catalogue)
  target_link_libraries(catalogue PUBLIC model coordinates numerics)

  acsd_add_library(response)
  target_link_libraries(response PUBLIC model numerics)

  acsd_add_library(sources)
  target_link_libraries(sources PUBLIC observation coordinates catalogue uncertainty numerics)

  acsd_add_library(photometry)
  target_link_libraries(photometry PUBLIC sources response uncertainty numerics)

  acsd_add_library(geometry)
  target_link_libraries(geometry PUBLIC coordinates operators numerics model)

  acsd_add_library(information)
  target_link_libraries(information PUBLIC uncertainty numerics operators)

  acsd_add_library(sky)
  target_link_libraries(sky PUBLIC uncertainty information operators numerics)

  acsd_add_library(coadd)
  target_link_libraries(coadd PUBLIC sky uncertainty operators numerics)

  acsd_add_library(projection)
  target_link_libraries(projection PUBLIC geometry operators uncertainty numerics)

  acsd_add_library(workflows)
  target_link_libraries(workflows PUBLIC sources photometry geometry information sky coadd
                                      projection response operators uncertainty resources storage
                                      model numerics coordinates observation catalogue)

  add_library(acsd_science INTERFACE)
  target_link_libraries(acsd_science INTERFACE workflows)

  if(ACSD_BUILD_CONTRACT_DRIVER)
    add_executable(acsd-contract apps/contract/main.cpp apps/contract/driver.cpp
                           apps/contract/json_io.cpp)
    target_link_libraries(acsd-contract PRIVATE acsd_science nlohmann_json::nlohmann_json)
    target_include_directories(acsd-contract PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  endif()

  add_executable(acsd apps/cli/main.cpp apps/cli/config.cpp apps/cli/events.cpp apps/cli/commands.cpp)
  target_link_libraries(acsd PRIVATE acsd_science nlohmann_json::nlohmann_json CFITSIO::CFITSIO)
  target_include_directories(acsd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

add_library(acsd_contracts_compile OBJECT validation/contracts_compile.cpp)
target_link_libraries(acsd_contracts_compile PRIVATE acsd_contracts)
find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)
add_custom_target(check_design ALL
 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_design.py
 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} VERBATIM)
enable_testing()
add_test(NAME design_integrity COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_design.py)
