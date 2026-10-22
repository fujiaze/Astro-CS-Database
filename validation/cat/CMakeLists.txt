# Independent validation of the catalogue module and of the blind astrometric
# solver.  The project is self contained: it compiles the production libraries
# it exercises and links the validation program against the public API only.
# It never includes an internal header of the production tree.
cmake_minimum_required(VERSION 3.25)
project(ACSD_CATALOGUE_VALIDATION LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(ACSD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

set(ACSD_NUMERIC_FLAGS -O2 -fno-fast-math -ffp-contract=off)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  list(APPEND ACSD_NUMERIC_FLAGS -frounding-math -mfpmath=sse)
endif()

file(GLOB_RECURSE ACSD_MODEL_SOURCES CONFIGURE_DEPENDS "${ACSD_ROOT}/src/model/*.cpp")
file(GLOB_RECURSE ACSD_NUMERICS_SOURCES CONFIGURE_DEPENDS "${ACSD_ROOT}/src/numerics/*.cpp")
file(GLOB_RECURSE ACSD_COORDINATES_SOURCES CONFIGURE_DEPENDS "${ACSD_ROOT}/src/coordinates/*.cpp")
# The catalogue module only needs the HEALPix geometry; keeping it in a separate
# target isolates the catalogue validation from the in flight work on the WCS
# and footprint implementation.
# The HEALPix lookup is built on the equal area chart, so both are needed.
set(ACSD_HEALPIX_SOURCES "${ACSD_ROOT}/src/coordinates/healpix.cpp"
                         "${ACSD_ROOT}/src/coordinates/chart.cpp")
file(GLOB_RECURSE ACSD_CATALOGUE_SOURCES CONFIGURE_DEPENDS "${ACSD_ROOT}/src/catalogue/*.cpp")
file(GLOB ACSD_SOURCES_SOURCES CONFIGURE_DEPENDS "${ACSD_ROOT}/src/sources/astrometry*.cpp")
file(GLOB_RECURSE ACSD_OPERATORS_SOURCES CONFIGURE_DEPENDS "${ACSD_ROOT}/src/operators/*.cpp")
file(GLOB_RECURSE ACSD_UNCERTAINTY_SOURCES CONFIGURE_DEPENDS "${ACSD_ROOT}/src/uncertainty/*.cpp")

add_library(acsd_model STATIC ${ACSD_MODEL_SOURCES})
target_include_directories(acsd_model PUBLIC "${ACSD_ROOT}/src" "${ACSD_ROOT}/include")
target_compile_options(acsd_model PRIVATE ${ACSD_NUMERIC_FLAGS})

add_library(acsd_numerics STATIC ${ACSD_NUMERICS_SOURCES})
target_link_libraries(acsd_numerics PUBLIC acsd_model)
target_compile_options(acsd_numerics PRIVATE ${ACSD_NUMERIC_FLAGS})

add_library(acsd_coordinates STATIC ${ACSD_COORDINATES_SOURCES})
target_link_libraries(acsd_coordinates PUBLIC acsd_model acsd_numerics)
target_compile_options(acsd_coordinates PRIVATE ${ACSD_NUMERIC_FLAGS})

add_library(acsd_operators STATIC ${ACSD_OPERATORS_SOURCES})
target_link_libraries(acsd_operators PUBLIC acsd_model acsd_numerics)

add_library(acsd_uncertainty STATIC ${ACSD_UNCERTAINTY_SOURCES})
target_link_libraries(acsd_uncertainty PUBLIC acsd_operators acsd_numerics)

add_library(acsd_healpix STATIC ${ACSD_HEALPIX_SOURCES})
target_link_libraries(acsd_healpix PUBLIC acsd_model acsd_numerics)

add_library(acsd_catalogue STATIC ${ACSD_CATALOGUE_SOURCES})
target_link_libraries(acsd_catalogue PUBLIC acsd_model acsd_numerics acsd_healpix acsd_uncertainty)
target_compile_options(acsd_catalogue PRIVATE ${ACSD_NUMERIC_FLAGS})

if(ACSD_SOURCES_SOURCES)
  add_library(acsd_sources STATIC ${ACSD_SOURCES_SOURCES})
  target_link_libraries(acsd_sources PUBLIC acsd_model acsd_numerics acsd_coordinates acsd_catalogue
                                         acsd_uncertainty acsd_operators)
  target_compile_options(acsd_sources PRIVATE ${ACSD_NUMERIC_FLAGS})
  set(ACSD_HAVE_SOURCES TRUE)
else()
  set(ACSD_HAVE_SOURCES FALSE)
endif()

add_executable(catalogue_check catalogue_check.cpp)
target_link_libraries(catalogue_check PRIVATE acsd_catalogue acsd_healpix acsd_numerics acsd_model
                                            acsd_uncertainty acsd_operators)
target_include_directories(catalogue_check PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

if(ACSD_HAVE_SOURCES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/astrometry_check.cpp")
  add_executable(astrometry_check astrometry_check.cpp)
  target_link_libraries(astrometry_check PRIVATE acsd_sources acsd_catalogue acsd_coordinates
                                             acsd_numerics acsd_model acsd_uncertainty acsd_operators)
  target_include_directories(astrometry_check PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
