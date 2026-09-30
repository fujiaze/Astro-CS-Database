# ============================================================================
# 编排层（legacy Phase1 orchestrator）模块 CMake —— ORCH-001 批次 1
# ----------------------------------------------------------------------------
# 背景：本目录在此之前**不在任何构建图**（根 CMakeLists.txt grep orchestrator = 0，
# ctest 0 命中），仅有一份指向旧路径的 MSYS2 Makefile ⇒ 结构性孤岛：
# SAT-001（claim SC-008 饱和电平解析）与 MASK-002（claim SC-007 逐星掩膜输入）
# 在 orchestrator.cpp 里的改动从未被编译过。
#
# 归属判定（逐字依据见 docs/ACSD_DESIGN.md §7.1）：
#   docs/ACSD_DESIGN.md:373 「scheduler/  注册、资源预算、执行、取消、checkpoint」
#   docs/ACSD_DESIGN.md:374 「pipeline/   typed DAG、artifact、内存/数据管线」
#   本目录 7 个编译单元的职责面逐条落在 :373 的五个词上（注册=dll_loader、
#   资源预算=admission_controller/resource_monitor、执行=run_stage_*、
#   取消=request_cancel/SIGINT、checkpoint=checkpoint.cpp）⇒ 职责家 = scheduler/**；
#   物理位 = ARCH-001 迁移清单 #15（lib/orchestrator → lib/infrastructure/pipeline/orchestrator，
#   该条依据写的是「7.1 infrastructure/pipeline（typed DAG 编排）」）⇒ 位置与职责冲突，
#   登记 ORCH-HOME-01 另行处置（目录搬迁 = git mv，非本任务权限）。
#   本 target 名取 MODULE_MAP 的 infrastructure 约定（docs/modules/MODULE_MAP.yaml
#   conventions: target_infrastructure = acsd_infra_<id>），id = orchestrator
#   （模块文档 docs/detail/orchestrator.md），**不写死 pipeline 或 scheduler 任意一侧**。
#
# 契约（BLD-002）：本文件只声明本模块 target；唯一 project()/唯一产品可执行
# acsd 仍只在根 CMakeLists.txt。
# ============================================================================

# ── 1) 第三方 json-schema-validator v2.4.0（vendored，与 cpp/Makefile 同一份源）──
# 独立 target + -w 隔离（第三方，与 acsd_cfitsio 同款处置）。
add_library(acsd_orchestrator_jsv STATIC
  third_party/json-schema-validator/json-uri.cpp
  third_party/json-schema-validator/json-validator.cpp
  third_party/json-schema-validator/json-patch.cpp
  third_party/json-schema-validator/smtp-address-validator.cpp
  third_party/json-schema-validator/string-format-check.cpp
  third_party/json-schema-validator/json-schema-draft7.json.cpp)
target_include_directories(acsd_orchestrator_jsv PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json-schema-validator
  ${CMAKE_SOURCE_DIR}/lib/third_party)
if(MSVC)
  # /W0 而非 /w: 等效于第三方目标完全关闭告警，但走的是告警级元素而非附加选项，
  # 避免 MSBuild 缺省 /W1 与显式旗标相撞的 D9025。
  target_compile_options(acsd_orchestrator_jsv PRIVATE /W0)
  # 第三方隔离 (见根 CMakeLists add_compile_options 的 ACSD_WARNINGS_OFF): 本目标不收项目 /W4。
  set_target_properties(acsd_orchestrator_jsv PROPERTIES ACSD_WARNINGS_OFF 1)
else()
  target_compile_options(acsd_orchestrator_jsv PRIVATE -w)
endif()

# ── 2) 编排层静态库（模块本体；唯一消费者 = legacy CLI 入口 + 共址测试）──
# 源 = cpp/src 全部编译单元（main.cpp 除外，它只属入口可执行）。
# 依赖闭包：模块头（AIO/PSF/IPV/photometry/noise_snr/star_detection/drizzle/gaia）
# 全部走**头文件面**（真链接由 DllLoader 运行时 dlopen/LoadLibrary 完成，与
# cpp/Makefile 的「暂不链接任何 DLL」口径一致）；唯一静态依赖 = acsd_common
# （sha256 + healpix_core，即 Makefile 里的 ../../common/{crypto,healpix}）。
add_library(acsd_infra_orchestrator STATIC
  src/orchestrator.cpp
  src/dll_loader.cpp
  src/checkpoint.cpp
  src/logger.cpp
  src/cli_command.cpp
  src/json_config.cpp)
target_include_directories(acsd_infra_orchestrator PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  # orchestrator.h 直接 include "aio_pipeline.h" ⇒ AIO 头面随本 target 传递。
  ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/include
  # 共址测试需要 SnrNoiseModelConfig 等 C ABI 结构定义（可执行级接线门）。
  ${CMAKE_SOURCE_DIR}/lib/algorithms/noise_snr/cpp/include
  ${CMAKE_SOURCE_DIR}/lib/algorithms/noise_snr/include)
target_include_directories(acsd_infra_orchestrator PRIVATE
  ${CMAKE_SOURCE_DIR}/lib/algorithms/shared
  ${CMAKE_SOURCE_DIR}/lib/algorithms/shared/include
  ${CMAKE_SOURCE_DIR}/lib/algorithms/shared/crypto
  ${CMAKE_SOURCE_DIR}/lib/algorithms/shared/healpix
  ${CMAKE_SOURCE_DIR}/lib/algorithms/platesolve/cpp/ipv/include
  # CLEAN-403: ipv_log.h 的日志追加写经 aio 机制原语 (aio_atomic_file.h)。
  ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src
  ${CMAKE_SOURCE_DIR}/lib/algorithms/psf/include
  ${CMAKE_SOURCE_DIR}/lib/algorithms/photometry/cpp/include
  # PHOTOCURVE-ORCH-01: 曲线解析唯一实现 filter_curve_json.h 位于 photometry 的
  # cpp/src（与 frame_photometry_fit.cpp 共用同一份代码 ⇒ 只在此加头文件搜索面，
  # 不新增静态链接依赖: 该头 header-only，唯一外部符号 aio_file::read_all 亦 inline）。
  ${CMAKE_SOURCE_DIR}/lib/algorithms/photometry/cpp/src
  ${CMAKE_SOURCE_DIR}/lib/algorithms/drizzle/healpix_drizzle
  ${CMAKE_SOURCE_DIR}/lib/infrastructure/gaia_xpsd_client/src
  ${CMAKE_SOURCE_DIR}/lib/algorithms/star_detection/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json-schema-validator
  ${CMAKE_SOURCE_DIR}/lib/third_party)
target_link_libraries(acsd_infra_orchestrator
  PUBLIC acsd_common
  PRIVATE acsd_orchestrator_jsv ${CMAKE_DL_LIBS})
if(NOT MSVC)
  target_compile_options(acsd_infra_orchestrator PRIVATE -Wall)
endif()

# ── 3) legacy CLI 入口（非产品；不进 install 白名单）──
# docs/ACSD_DESIGN §6.2 的唯一命令树 = 产品 acsd（normalize|mosaic|export）；
# 本可执行是 docs/detail/orchestrator.md 记载的 legacy「stage1.json 入口」，
# 仅用于共址冒烟测试与诊断，**不 install、不进产品 manifest**。其退场与否
# （§7.1 收编入 scheduler 后的形态）登记为 ORCH-HOME-01 的一部分另行处置。
add_executable(orchestrator_legacy_cli src/main.cpp)
# WIN-PORT 缺陷⑤: main.cpp 直接 include nlohmann/json.hpp (源 :36)。上面
# acsd_infra_orchestrator 的 lib/third_party 是 PRIVATE include 面, 不向本
# 可执行传播 ⇒ MSVC C1083。此处显式登记本 TU 的第三方头搜索面。
target_include_directories(orchestrator_legacy_cli PRIVATE
  ${CMAKE_SOURCE_DIR}/lib/third_party)
target_link_libraries(orchestrator_legacy_cli PRIVATE acsd_infra_orchestrator)

# 共址测试子目录由根 CMakeLists.txt 在 enable_testing() 之后单独注册
# （add_test 必须在顶层 enable_testing() 生效后进入目录树；本文件被提前注册时
# 若自行 add_subdirectory(tests) 会得到「测试不注册」的静默空门）。
