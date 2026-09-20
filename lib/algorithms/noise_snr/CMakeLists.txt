# CMakeLists.txt - astrocs_p1_noise 模块 DLL (P1-NOISE-IMPL 迁移面)
#
# 对齐先例: lib/algorithms/drizzle/hips/CMakeLists.txt (P1-HIPS-IMPL, 1959dc89)、
#           lib/algorithms/calibration/CMakeLists.txt (P1-CAL-IMPL, adf820ac)、
#           lib/algorithms/cosmetic/CMakeLists.txt (P1-COS-IMPL, 948dfcba)。
#
# 关键事实 (迁移报告 T-1, hips/CAL/COS 同款): 既有 STATIC 库对象非 PIC
# (TLS local-exec 重定位 R_X86_64_TPOFF32, 链接 .so 失败实证), 因此本
# SHARED 目标从源文件独立重编译生产源 (POSITION_INDEPENDENT_CODE ON)。
#
# 依赖闭包 (先实证再定 TU 集; 2026-09-09 只读实证):
#   noise_model.cpp 编译闭包 = {snr_estimator.h}; 项目内符号依赖 = 零
#   (grep 实证: 无 astrocs_*/aio_*/ac_* 引用; 仅 std + libm); 生产 TU 无
#   omp pragma (单线程现状, README §8) → 零 OMP 链接 (58d20223 --as-needed
#   教训不适用, DLL 无 GOMP 并行区)。最小闭包 = {noise_model.cpp}。
#   snr_estimator.cpp (旧乘法 SNR legacy diagnostic 通道, snr_estimator.h
#   :19-20 已降级) 不属噪声合同 (README §2 不负责面), 不进本 DLL。
#
# 导出面净化 (ABI-006): legacy snr_noise_* 七符号 (snr_estimator.h SNR_API
# 携带, Linux 空宏) 经链接 version-script (Linux/macOS) / /DEF 白名单
# (MSVC) 降 local; 唯一导出 astrocs_module_query_v1。
# 科学域零改动 (scientific_change=false): 生产 TU 原样编译, 无本地副本;
# kTrimMeanToSigma (noise_model.cpp:37) 挂账项不触碰。
#
# 注: 本子目录**未**由根 CMakeLists.txt 注册 —— 根 :240 `add_subdirectory(lib/algorithms/noise_snr)`
# 自 F-CI-002-01 起注释停用, 本子图 `astrocs_p1_noise`(SHARED, :35) 不在根图。
# ⚠ 2026-09-20 订正（V5 分片 5 / R-5）: 旧文「本子目录由根 CMakeLists.txt (hips 之后) 注册」
#   **已作废**（依据实况 + GAP_AUDIT §9.67 定案 3）。生产噪声源不靠本子图注册接入:
#   按 NOISE-MODEL-CANON-001, A = `cpp/src/noise_model.cpp` 已**源文件直接编入**根目标
#   `astrocs_phase1_noise`（根 CMakeLists.txt:630-633）, B 已退役。本子图注册去留归 P1-NOISE-INT。

# ── 生产源 (依赖闭包最小集, 全部指向唯一权威源, 无本地副本) ──
set(NOISE_PROD_DIR "${CMAKE_SOURCE_DIR}/lib/algorithms/noise_snr/cpp")
set(NOISE_ADAPTER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/module_entry.cpp
)
set(NOISE_PROD_SOURCES
    ${NOISE_PROD_DIR}/src/noise_model.cpp
)
add_library(astrocs_p1_noise SHARED
    ${NOISE_ADAPTER_SOURCES}
    ${NOISE_PROD_SOURCES}
)

set_target_properties(astrocs_p1_noise PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "astrocs_p1_noise"
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    CXX_STANDARD 17
)

# Windows (MSVC): DLL 无 import lib 前缀需求; 输出到 build 根 (host loader
# 与测试按 build 根定位, 对齐 CAL/COS $<1:...> 惯例)
if(WIN32)
    set_target_properties(astrocs_p1_noise PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}>)
endif()

# ── include 路径 (生产源口径 = 生产头目录; adapter 闭包最小集) ──
target_include_directories(astrocs_p1_noise PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${NOISE_PROD_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ── 编译宏: ABI 导出开关 ──
# 注: 生产 TU 的 SNR_API 在非 _WIN32 为空宏 (导出净化由 version-script
# 兜底); _WIN32 下 SNR_API=__declspec(dllexport) 携带的七符号被 /DEF
# 白名单过滤降 local (CAL AC_API= 同款双保险思路, 生产头零改动)。
target_compile_definitions(astrocs_p1_noise PRIVATE
    ASTROCS_ABI_SHARED=1
    ASTROCS_ABI_EXPORTS=1
)

find_package(Threads REQUIRED)

# ── 链接依赖 (闭包自包含; 只剩系统库; 零 OMP) ──
set(NOISE_LINK_LIBS Threads::Threads)
if(UNIX)
    # libm: 生产 TU 的 sqrt/fabs/nth-element 面数学依赖
    list(APPEND NOISE_LINK_LIBS m)
endif()
target_link_libraries(astrocs_p1_noise PRIVATE ${NOISE_LINK_LIBS})

# ── 导出面净化 (唯一导出 astrocs_module_query_v1; ABI-006) ──
if(WIN32)
    # MSVC: /DEF 白名单 (SNR_API dllexport 七符号被 def 过滤)
    set(NOISE_DEF "${CMAKE_CURRENT_SOURCE_DIR}/src/astrocs_p1_noise.def")
    target_link_options(astrocs_p1_noise PRIVATE "/DEF:${NOISE_DEF}")
else()
    # Linux/macOS: version-script 白名单 + hidden 可见性预设
    set(NOISE_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/module_exports.map")
    target_link_options(astrocs_p1_noise PRIVATE
        "-Wl,--version-script=${NOISE_VERSION_SCRIPT}")
    set_target_properties(astrocs_p1_noise PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
endif()
