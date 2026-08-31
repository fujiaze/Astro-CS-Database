# CORE-*/IO-*/CPU-* 单元测试 (G2/G3 任务逐步加入)
add_executable(core_contracts_test core_contracts_test.cpp)
target_include_directories(core_contracts_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_contracts_test PRIVATE astrocs_contracts)
add_test(NAME core_contracts COMMAND core_contracts_test)

add_executable(core_artifact_test core_artifact_test.cpp)
target_include_directories(core_artifact_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_artifact_test PRIVATE astrocs_core)
add_test(NAME core_artifact COMMAND core_artifact_test)

add_executable(core_module_test core_module_test.cpp)
target_include_directories(core_module_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_module_test PRIVATE astrocs_core)
add_test(NAME core_module COMMAND core_module_test)

add_executable(core_pipeline_test core_pipeline_test.cpp)
target_include_directories(core_pipeline_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_pipeline_test PRIVATE astrocs_core)
add_test(NAME core_pipeline COMMAND core_pipeline_test)

add_executable(core_context_test core_context_test.cpp)
target_include_directories(core_context_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_context_test PRIVATE astrocs_core)
add_test(NAME core_context COMMAND core_context_test)

add_executable(core_scheduler_test core_scheduler_test.cpp)
target_include_directories(core_scheduler_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_scheduler_test PRIVATE astrocs_core)
add_test(NAME core_scheduler COMMAND core_scheduler_test)

add_executable(core_checkpoint_test core_checkpoint_test.cpp)
target_include_directories(core_checkpoint_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_checkpoint_test PRIVATE astrocs_core)
add_test(NAME core_checkpoint COMMAND core_checkpoint_test)

add_executable(core_logging_test core_logging_test.cpp)
target_include_directories(core_logging_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_logging_test PRIVATE astrocs_core)
add_test(NAME core_logging COMMAND core_logging_test)

add_executable(rt001_abi_test rt001_abi_test.cpp)
target_include_directories(rt001_abi_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(rt001_abi_test PRIVATE astrocs_core)
add_test(NAME rt001_abi COMMAND rt001_abi_test)

add_executable(rt002_budget_test rt002_budget_test.cpp)
target_include_directories(rt002_budget_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(rt002_budget_test PRIVATE astrocs_core)
add_test(NAME rt002_budget COMMAND rt002_budget_test)

add_executable(rt003_context_test rt003_context_test.cpp)
target_include_directories(rt003_context_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(rt003_context_test PRIVATE astrocs_core)
add_test(NAME rt003_context COMMAND rt003_context_test)

add_executable(rt005_registry_test rt005_registry_test.cpp)
target_include_directories(rt005_registry_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party)
target_link_libraries(rt005_registry_test PRIVATE astrocs_module_adapters)
add_test(NAME rt005_registry COMMAND rt005_registry_test)

add_executable(rt006_scheduler_test rt006_scheduler_test.cpp)
target_include_directories(rt006_scheduler_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party)
target_link_libraries(rt006_scheduler_test PRIVATE astrocs_module_adapters astrocs_core)
add_test(NAME rt006_scheduler COMMAND rt006_scheduler_test)

add_executable(rt007_artifact_store_test rt007_artifact_store_test.cpp)
target_include_directories(rt007_artifact_store_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party)
target_link_libraries(rt007_artifact_store_test PRIVATE astrocs_core)
add_test(NAME rt007_artifact_store COMMAND rt007_artifact_store_test)

add_executable(rt008_runtime_client_test rt008_runtime_client_test.cpp)
target_include_directories(rt008_runtime_client_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party)
target_link_libraries(rt008_runtime_client_test PRIVATE astrocs_cli_runtime)
add_test(NAME rt008_runtime_client COMMAND rt008_runtime_client_test)

# RT-009: 节点 trace 数据源（run 后 status/时间/workers/provider; IR 保留）
add_executable(rt009_node_trace_test rt009_node_trace_test.cpp)
target_include_directories(rt009_node_trace_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party)
target_link_libraries(rt009_node_trace_test PRIVATE astrocs_cli_runtime)
add_test(NAME rt009_node_trace COMMAND rt009_node_trace_test)

add_executable(io_adapter_test io_adapter_test.cpp)
target_include_directories(io_adapter_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(io_adapter_test PRIVATE astrocs_io)
add_test(NAME io_adapter COMMAND io_adapter_test)

add_executable(io_ownership_test io_ownership_test.cpp)
target_include_directories(io_ownership_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include)
target_link_libraries(io_ownership_test PRIVATE astrocs_aio astrocs_core)
add_test(NAME io_ownership COMMAND io_ownership_test)

add_executable(io_reentrant_test io_reentrant_test.cpp)
target_include_directories(io_reentrant_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/third_party/cfitsio)
target_link_libraries(io_reentrant_test PRIVATE astrocs_cfitsio z m)
add_test(NAME io_reentrant COMMAND io_reentrant_test)

add_executable(cpu_abi_test cpu_abi_test.cpp)
target_include_directories(cpu_abi_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(cpu_abi_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu_abi COMMAND cpu_abi_test)

add_executable(cpu_lease_test cpu_lease_test.cpp)
target_include_directories(cpu_lease_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu_lease_test PRIVATE astrocs_cpu astrocs_contracts Threads::Threads)
add_test(NAME cpu_lease COMMAND cpu_lease_test)

add_executable(cpu_features_test cpu_features_test.cpp)
target_include_directories(cpu_features_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu_features_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu_features COMMAND cpu_features_test)

add_executable(cpu_provider_test cpu_provider_test.cpp)
target_include_directories(cpu_provider_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu_provider_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu_provider COMMAND cpu_provider_test)

add_executable(cpu_fallback_test cpu_fallback_test.cpp)
target_include_directories(cpu_fallback_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu_fallback_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu_fallback COMMAND cpu_fallback_test)

add_executable(cpu_bench_test cpu_bench_test.cpp)
target_include_directories(cpu_bench_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu_bench_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu_bench COMMAND cpu_bench_test)

add_executable(cpu_profile_test cpu_profile_test.cpp)
target_include_directories(cpu_profile_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/backend_host)
target_link_libraries(cpu_profile_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu_profile COMMAND cpu_profile_test)

# G9: OpenMP 可选 (clang Debug 无 libomp 时降级串行; P2_ENABLE_OPENMP 默认 OFF)
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
  set(ASTROCS_OMP_TARGET OpenMP::OpenMP_CXX)
else()
  set(ASTROCS_OMP_TARGET "")
endif()

add_executable(cpu_monitor_test cpu_monitor_test.cpp)
target_include_directories(cpu_monitor_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli)
target_link_libraries(cpu_monitor_test PRIVATE astrocs_contracts Threads::Threads)
add_test(NAME cpu_monitor COMMAND cpu_monitor_test)

add_executable(p1_calibration_test p1_calibration_test.cpp)
target_include_directories(p1_calibration_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/include)
target_link_libraries(p1_calibration_test PRIVATE astrocs_calibration ${ASTROCS_OMP_TARGET})
add_test(NAME p1_calibration COMMAND p1_calibration_test)

add_executable(p1_stars_test p1_stars_test.cpp)
target_include_directories(p1_stars_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase1/stars)
target_link_libraries(p1_stars_test PRIVATE astrocs_phase1_stars)
add_test(NAME p1_stars COMMAND p1_stars_test)

add_executable(p1_wcs_phot_test p1_wcs_phot_test.cpp)
target_include_directories(p1_wcs_phot_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase1/wcs
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase1/photometry)
target_link_libraries(p1_wcs_phot_test PRIVATE astrocs_phase1_wcs astrocs_phase1_phot)
add_test(NAME p1_wcs_phot COMMAND p1_wcs_phot_test)

add_executable(p1_noise_test p1_noise_test.cpp)
target_include_directories(p1_noise_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase1/noise)
target_link_libraries(p1_noise_test PRIVATE astrocs_phase1_noise)
add_test(NAME p1_noise COMMAND p1_noise_test)

add_executable(p1_nside_test p1_nside_test.cpp)
target_include_directories(p1_nside_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/common/healpix
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase3_session)
target_link_libraries(p1_nside_test PRIVATE astrocs_common astrocs_phase3_session astrocs_hips astrocs_contracts)
add_test(NAME p1_nside COMMAND p1_nside_test)

add_executable(p1_hips_writer_test p1_hips_writer_test.cpp)
target_include_directories(p1_hips_writer_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include)
target_link_libraries(p1_hips_writer_test PRIVATE astrocs_hips astrocs_aio)
add_test(NAME p1_hips_writer COMMAND p1_hips_writer_test)

add_executable(p1_ir_facade_test p1_ir_facade_test.cpp)
target_include_directories(p1_ir_facade_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/include)
target_link_libraries(p1_ir_facade_test PRIVATE astrocs_calibration ${ASTROCS_OMP_TARGET})
add_test(NAME p1_ir_facade COMMAND p1_ir_facade_test)

add_executable(p1_resource_test p1_resource_test.cpp)
target_include_directories(p1_resource_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/calibration/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli)
target_link_libraries(p1_resource_test PRIVATE astrocs_calibration ${ASTROCS_OMP_TARGET} Threads::Threads)
add_test(NAME p1_resource COMMAND p1_resource_test)

add_executable(p2_workers_test p2_workers_test.cpp)
target_include_directories(p2_workers_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase2/include)
target_link_libraries(p2_workers_test PRIVATE astrocs_phase2 astrocs_contracts ${ASTROCS_OMP_TARGET})
add_test(NAME p2_workers COMMAND p2_workers_test)

add_executable(p2_upm_synthetic_test p2_upm_synthetic_test.cpp)
target_link_libraries(p2_upm_synthetic_test PRIVATE astrocs_contracts)
add_test(NAME p2_upm_synthetic COMMAND p2_upm_synthetic_test)

add_executable(p2_block_plan_test p2_block_plan_test.cpp)
target_include_directories(p2_block_plan_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase2/include)
target_link_libraries(p2_block_plan_test PRIVATE astrocs_phase2)
add_test(NAME p2_block_plan COMMAND p2_block_plan_test)

add_executable(p2_rejection_test p2_rejection_test.cpp)
target_include_directories(p2_rejection_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase2/include)
target_link_libraries(p2_rejection_test PRIVATE astrocs_phase2)
add_test(NAME p2_rejection COMMAND p2_rejection_test)

add_executable(p2_output_semantics_test p2_output_semantics_test.cpp)
target_include_directories(p2_output_semantics_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase2/include)
target_link_libraries(p2_output_semantics_test PRIVATE astrocs_phase2)
add_test(NAME p2_output_semantics COMMAND p2_output_semantics_test)

add_executable(p2_ir_facade_test p2_ir_facade_test.cpp)
target_link_libraries(p2_ir_facade_test PRIVATE astrocs_contracts)
add_test(NAME p2_ir_facade COMMAND p2_ir_facade_test)

add_executable(p2_seam_gate_test p2_seam_gate_test.cpp)
target_include_directories(p2_seam_gate_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cli)
target_link_libraries(p2_seam_gate_test PRIVATE astrocs_contracts)
add_test(NAME p2_seam_gate COMMAND p2_seam_gate_test)

add_executable(p3_wcs_test p3_wcs_test.cpp)
target_include_directories(p3_wcs_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase3_session)
target_link_libraries(p3_wcs_test PRIVATE astrocs_phase3_session astrocs_common)
add_test(NAME p3_wcs COMMAND p3_wcs_test)

add_executable(p3_interp_test p3_interp_test.cpp)
target_link_libraries(p3_interp_test PRIVATE astrocs_contracts)
add_test(NAME p3_interp COMMAND p3_interp_test)

add_executable(p3_coverage_test p3_coverage_test.cpp)
target_link_libraries(p3_coverage_test PRIVATE astrocs_contracts)
add_test(NAME p3_coverage COMMAND p3_coverage_test)

add_executable(p3_output_test p3_output_test.cpp)
target_include_directories(p3_output_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase3_session
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include)
target_link_libraries(p3_output_test PRIVATE astrocs_phase3_session astrocs_hips astrocs_common)
add_test(NAME p3_output COMMAND p3_output_test)

add_executable(p3_assembly_test p3_assembly_test.cpp)
target_include_directories(p3_assembly_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/phase3_session
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/common/healpix)
target_link_libraries(p3_assembly_test PRIVATE astrocs_phase3_session astrocs_hips astrocs_aio astrocs_common)
add_test(NAME p3_assembly COMMAND p3_assembly_test)

