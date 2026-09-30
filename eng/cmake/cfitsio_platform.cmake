# eng/cmake/cfitsio_platform.cmake — vendored 第三方源的平台面共用口径（**唯一实现**）
#
# 为什么需要这一份（根站点事实，实测）：
#   vendored cfitsio 的来源在根图里被**多个目标各自重编译**（根 acsd_cfitsio 之外的
#   现行重编译点 = acsd_phase2_integrate / acsd_p1_drizzle / acsd_p1_hips_writer；
#   全仓清单与判据见 run/FINAL-07/审核包/工程/Windows构建错误批次一报告.md）。
#   重编译点必须**逐个**拿到与根目标同一份平台适配，
#   否则同一份第三方源在不同目标里口径不同 —— Windows 全量构建实测：
#     · acsd_phase2_integrate 编入 60 个 cfitsio TU 并定义 _REENTRANT，
#       却没有 pthread 垫片包含面 ⇒ 55 条 C1083 "pthread.h"（fitsio2.h 的
#       `#ifdef _REENTRANT` / `#include <pthread.h>` 分支）；
#     · 同一目标自编 zcompress.c/zuncompress.c 却没有 zlib 包含面 ⇒ C1083 "zlib.h"。
#   根因不是"漏了哪一行"，而是**同一判断在两处各写一遍**。因此本文件把两类平台适配
#   收成共用函数，重编译点只调用函数、不复制路径字面量。
#
# 上游：
#   docs/ACSD_DESIGN.md §8.4（顶层结构·eng/cmake 放平台构建片段）
#   ENGINEERING_SPEC.md §1（语言、编译器与平台：Windows x64 = MSVC / Linux amd64）
#   eng/packaging/dependency-lock.json（cfitsio: `_REENTRANT`；MSVC 需 zlib → ACS_ZLIB_ROOT）
#   .github/workflows/ci-windows.yml "Provide zlib (vcpkg)" 步（注入布局的合同声明）
#
# ── 包含面核查判据（included-surface invariant）─────────────────────────────
#   核查器：eng/ci/check_cfitsio_platform_surface.py（静态、可判定、带正负例）
#   I1  每个"编入 vendored cfitsio 源清单"的目标，必须经
#       `acsd_cfitsio_apply_platform_shim(<target>)` 拿到 pthread 垫片包含面；
#   I2  每个"编入含 zlib.h 的 vendored cfitsio TU"的目标，必须经
#       `acsd_cfitsio_apply_third_party_deps(<target>)` 拿到 zlib 包含面。
#   两条都是**静态可判定**的：判据只看 CMakeLists 声明面，不看工具链、也不看
#   /usr/include 是否存在（Linux 上系统 provider 会掩盖 Windows 的缺失，
#   故判据显式排除系统默认搜索路径）。
#   I3（旗标面，同模块同口径）所有 GCC 专有旗标必须与平台门控写在一处 ——
#       承载点即本文件的 `acsd_openmp_link_if_unix()`；统计值：
#       `rg -n -- '-fopenmp' --glob '**/CMakeLists.txt' --glob '**/*.cmake' \`
#       `  -g '!build/**' -g '!run/**'` 只应命中本文件（调用点不再各写一份）。

include_guard(GLOBAL)

# ── OpenMP 链接 + 编译旗标的**统一门控** ──────────────────────────────────
# 为什么收在一处 (同族两处两套口径的又一例):
#   全仓 7 个调用点改前是**两套口径**:
#     (a) 只判 `if(UNIX AND OpenMP_CXX_FOUND)` 且**同时**手写 `-fopenmp` 又链
#         imported target ⇒ 命令行出现两次 `-fopenmp`: 根 MCU 的 acsd_aio /
#         acsd_calibration / acsd_drizzle 三处 (实测 ninja 边
#         `FLAGS = ... -fopenmp -fopenmp`) —— 未泄漏到 Windows, 但把旗标名
#         在平台探测之外又硬编了一份, 并且比该 imported target 本该给的链接面多写一遍;
#     (b) lib/algorithms/drizzle/CMakeLists.txt 只判了 OpenMP_CXX_FOUND 就直接
#         `-fopenmp`, **没有平台门控** ⇒ Windows/MSVC 全量构建把 GCC 旗标交给 cl.exe
#         (D9002 unknown option, 且 pragma omp 仍被消除)。同族另两处
#         (calibration/cosmetic 子目录) 各自嵌了 if(UNIX), 形态三份不一致
#         —— 不一致正是漏网的土壤。
# 现口径: 门控与旗标只在本函数内出现一次 (旗标由 OpenMP::OpenMP_CXX 自带,
#   不另写 `$<$<COMPILE_LANGUAGE:CXX>:-fopenmp>`); 非 UNIX 或没有 OpenMP 时是空操作。
#   7 个调用点: 根 CMakeLists.txt 的 acsd_aio / acsd_calibration /
#   acsd_drizzle + lib/algorithms/{calibration,cosmetic,drizzle} + 见 I3 统计命令。
#   (MSVC 若将来要开 OpenMP, 应在此处补 /openmp, 而不是在调用点各写一份)。
# ── 第三方告警隔离 (third-party warning surface) ───────────────────────────
# 为什么收在一处: 根项目用 `add_compile_options(... $<$<NOT:$<STREQUAL:
#   $<TARGET_PROPERTY:ACSD_WARNINGS_OFF>,1>>:/W4>)` 把"第三方源码不进项目
#   告警面"做成**目标属性**; 但重复编译同一份 cfitsio 源的 3 个目标里,
#   acsd_phase2_integrate / acsd_p1_drizzle / acsd_p1_hips_writer 都
#   没有这个属性 ⇒ 同一批第三方 TU 在基准目标里安静、在重复点里把
#   drvrnet.c C4206、zuncompress.c C4267/C4244 之类灌进项目告警面
#   (Windows 基线 1274 warnings 里就有这一份)。同族两套口径 ⇒ 收成一处。
function(acsd_cfitsio_isolate_warnings tgt)
  if(NOT TARGET ${tgt})
    message(FATAL_ERROR
      "acsd_cfitsio_isolate_warnings: 目标 '${tgt}' 不存在")
  endif()
  set_target_properties(${tgt} PROPERTIES ACSD_WARNINGS_OFF 1)
  if(MSVC)
    # /W0 (不是 /w) 才能被 CMake 翻成 <WarningLevel>TurnOffAllWarnings</WarningLevel>,
    # 否则 MSBuild 默认 /W1 与 AdditionalOptions 里的 /w 相撞报 D9025
    # —— 零告警门禁下 D9025 也算告警。见根 CMakeLists 同款说明。
    target_compile_options(${tgt} PRIVATE /W0)
  else()
    target_compile_options(${tgt} PRIVATE -w)
  endif()
endfunction()

function(acsd_openmp_link_if_unix tgt)
  if(NOT TARGET ${tgt})
    message(FATAL_ERROR
      "acsd_openmp_link_if_unix: 目标 '${tgt}' 不存在 (含面/旗标口径见 eng/cmake/cfitsio_platform.cmake 抬头)")
  endif()
  if(UNIX AND OpenMP_CXX_FOUND)
    # 只链 imported target: 它自带 INTERFACE 编译旗标 (GCC/Clang 下即 -fopenmp,
    # 见 FindOpenMP 生成的 OpenMP_CXX_FLAGS), 故**不再**另写
    # `$<$<COMPILE_LANGUAGE:CXX>:-fopenmp>` —— 那会把同一个旗标在命令行上写两遍
    # (实测 ninja 边 FLAGS 出现 "-fopenmp -fopenmp"), 且把旗标名从平台探测里
    # 又硬编一份出来。MSVC 下若启用 OpenMP, 只需在此处加 WIN32 分支。
    target_link_libraries(${tgt} PRIVATE OpenMP::OpenMP_CXX)
  endif()
endfunction()

# ── 唯一变量：pthread 垫片目录（重编译点不得另写路径字面量）──────────────
set(ACSD_WIN32_PTHREAD_SHIM_DIR "${CMAKE_SOURCE_DIR}/eng/cmake/win32_pthread_shim"
    CACHE INTERNAL "vendored 第三方源在 MSVC 下的 pthread 垫片目录（唯一来源）")

# ── I1: pthread 垫片 ───────────────────────────────────────────────────────
# 语义：仅在 MSVC 下注入（非 MSVC 平台有真 pthread.h，注入反而遮蔽）；
#   与根 acsd_cfitsio 段逐条同口径（MSVC: shim 包含面 + kernel32 链接）。
#   非 MSVC 下本函数为空操作 —— 调用点无需自己写平台判断（写了就又是一处两套口径）。
function(acsd_cfitsio_apply_platform_shim tgt)
  if(NOT TARGET ${tgt})
    message(FATAL_ERROR
      "acsd_cfitsio_apply_platform_shim: 目标 '${tgt}' 不存在；"
      "本函数只对已声明的目标施加平台适配面（含面口径见 eng/cmake/cfitsio_platform.cmake 抬头）")
  endif()
  if(MSVC)
    target_include_directories(${tgt} PRIVATE "${ACSD_WIN32_PTHREAD_SHIM_DIR}")
    # 垫片用 MSVC 内建 _InterlockedExchange（无 Windows 头依赖），
    # 但第三方静态库的消费者仍需 kernel32 解析该内建的导入面 —— 与根段同款登记为
    # PUBLIC 依赖，使重编译点的链接闭包与根目标等价。
    target_link_libraries(${tgt} PUBLIC kernel32)
  endif()
endfunction()

# ── I2: zlib 依赖包含面 + 链接面 ───────────────────────────────────────────
# 布局的两条注入路径（**必须都兼容**，不得只按本机布局写）：
#   ① hosted runner（.github/workflows/ci-windows.yml:49）：
#        ACS_ZLIB_ROOT = <vcpkg>/installed/x64-windows-static-md
#        头 = <root>/include/zlib.h ；库 = <root>/lib/<ACS_ZLIB_LIB 或 z 族 *.lib>
#   ② 本机 prefix 安装（<root> 即安装前缀，对照 Linux 的 /usr）：
#        头 = <root>/include/zlib.h ；库 = <root>/lib/libz.*
#   历史缺陷：根段曾写死 `<ACS_ZLIB_ROOT>/lib/include` —— 对**两条**注入路径都错
#   （两条都不是 <root>/lib/include），故本函数按**头文件实际落点**判定，不猜目录名。
#
#   兜底顺序（确定性；与根段 HOSTFIX-23④ 的"env 层确定性优先"同口径）：
#     a) ACS_ZLIB_ROOT 非空 ⇒ **只**按它的两条布局判定（相对候选 include、lib/include，
#        按序取首个真实存在 zlib.h 者）。注入点上位后不得再让 FindZLIB 的系统缓存头
#        顶替 —— 那会造成"注入了一套、编的是另一套"的双布局分叉；
#     b) ACS_ZLIB_ROOT 为空 ⇒ 回退 FindZLIB 回执（ZLIB_INCLUDE_DIR / ZLIB_INCLUDE_DIRS）；
#     c) 都不可用且 ACS_ZLIB_ROOT 已声明 ⇒ FATAL_ERROR，并打印**两条**合同布局的探测结果
#        （fail-fast：不允许把错误布局静默传给编译期变成 C1083）。
#   非 MSVC（Linux/macOS）：ACS_ZLIB_ROOT 通常为空 ⇒ 本函数空操作，行为零变化。
function(acsd_cfitsio_apply_third_party_deps tgt)
  if(NOT TARGET ${tgt})
    message(FATAL_ERROR
      "acsd_cfitsio_apply_third_party_deps: 目标 '${tgt}' 不存在；"
      "本函数只对已声明的目标施加依赖包含面")
  endif()
  if(NOT MSVC)
    return()
  endif()

  set(_acs_zroot "$ENV{ACS_ZLIB_ROOT}")
  if(NOT _acs_zroot AND ACS_ZLIB_ROOT)
    set(_acs_zroot "${ACS_ZLIB_ROOT}")
  endif()

  set(_acs_inc "")
  # ① 合同注入点优先: ACS_ZLIB_ROOT 非空 ⇒ 只按它的两条布局判定。
  #    不可把 FindZLIB 的缓存结果排在前面 —— find_package(ZLIB) 可能先前已从系统
  #    路径缓存 ZLIB_INCLUDE_DIR, 那会在注入点上位后仍把系统头传下去 (双布局分叉)。
  if(_acs_zroot)
    foreach(_rel "include" "lib/include")
      if(EXISTS "${_acs_zroot}/${_rel}/zlib.h")
        set(_acs_inc "${_acs_zroot}/${_rel}")
        break()
      endif()
    endforeach()
  endif()
  # ② 无注入点时的回退: FindZLIB / vcpkg 回执 (Linux/MSVC 未设 ACS_ZLIB_ROOT 的同款行为)
  if(NOT _acs_inc AND ZLIB_INCLUDE_DIR AND EXISTS "${ZLIB_INCLUDE_DIR}/zlib.h")
    set(_acs_inc "${ZLIB_INCLUDE_DIR}")
  endif()
  if(NOT _acs_inc AND ZLIB_INCLUDE_DIRS)
    foreach(_d IN LISTS ZLIB_INCLUDE_DIRS)
      if(EXISTS "${_d}/zlib.h")
        set(_acs_inc "${_d}")
        break()
      endif()
    endforeach()
  endif()

  # 作用域: 接口库没有构建单元, 只能携带 INTERFACE 面; 可编译目标用 PRIVATE
  # (重编译点自身的 TU 要看见 zlib.h, 但不把第三方目录外扩到消费者包含面)。
  get_target_property(_acs_type ${tgt} TYPE)
  if(_acs_type STREQUAL "INTERFACE_LIBRARY")
    set(_acs_inc_scope INTERFACE)
  else()
    set(_acs_inc_scope PRIVATE)
  endif()
  if(_acs_inc)
    target_include_directories(${tgt} ${_acs_inc_scope} "${_acs_inc}")
  elseif(_acs_zroot)
    message(FATAL_ERROR
      "ACS_ZLIB_ROOT='${_acs_zroot}' 下未找到 zlib.h。合同声明的两条注入布局均为 "
      "<root>/include/zlib.h（hosted runner 的 vcpkg installed/<triplet> 与本机 prefix "
      "安装同构）；偏移布局 <root>/lib/include 从来不是合同口径。"
      "已探测: '${_acs_zroot}/include' 与 '${_acs_zroot}/lib/include'。"
      "修法：把 ACS_ZLIB_ROOT 指到安装前缀（含 include/ 与 lib/ 的那一层），"
      "或经 ZLIB_INCLUDE_DIR 显式回执该目录 —— 不得在本文件写第三条布局。")
  endif()

  # 链接面：MSVC 走根段同一口径（env 层确定性优先 ACS_ZLIB_LIB → find_library 回退）。
  # 目标已自带 zlib 链接面时（例如已链 ZLIB::ZLIB）不重复注入。
  get_target_property(_acs_links ${tgt} LINK_LIBRARIES)
  if(_acs_links AND ("ZLIB::ZLIB" IN_LIST _acs_links))
    return()
  endif()
  set(_acs_lib "")
  if(_acs_zroot AND "$ENV{ACS_ZLIB_LIB}" AND EXISTS "${_acs_zroot}/lib/$ENV{ACS_ZLIB_LIB}")
    set(_acs_lib "${_acs_zroot}/lib/$ENV{ACS_ZLIB_LIB}")
  else()
    find_library(_acs_lib_found NAMES z libz zlib zlibstatic zs
      HINTS "${_acs_zroot}/lib" NO_DEFAULT_PATH)
    if(_acs_lib_found)
      set(_acs_lib "${_acs_lib_found}")
    endif()
  endif()
  if(_acs_lib)
    # B-2 (FINAL-07/winfix): 改前写死 PUBLIC。对 INTERFACE_LIBRARY 目标, CMake 只允许
    #   INTERFACE 关键字 —— 写 PUBLIC 必报「INTERFACE library can only be used with the
    #   INTERFACE keyword of target_link_libraries」, configure 期硬失败。
    #   实际命中: lib/infrastructure/gaia_xpsd_client/CMakeLists.txt:106 的
    #   acsd_gaia_zlib_include (INTERFACE) 把它传进本函数。
    #   为何 Linux 看不见: 函数体被上方 `if(NOT MSVC) return()` 门控 ⇒ 非 MSVC 主机
    #   根本走不到这一行 (已用 Linux 负对照实验确认: 设了 ACS_ZLIB_ROOT 仍 configure 成功)。
    #   修法: 与包含面**同口径**复用上面已算好的 ${_acs_inc_scope} (它在函数内无条件赋值,
    #   到达此处必然可见) —— 不另起第二份判定, 避免两处再次分叉。
    target_link_libraries(${tgt} ${_acs_inc_scope} "${_acs_lib}")
  endif()
endfunction()
