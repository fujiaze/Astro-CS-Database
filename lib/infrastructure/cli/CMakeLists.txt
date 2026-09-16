# lib/infrastructure/cli/CMakeLists.txt — 模块 target 清单（BLD-002: 非产品事实源）
#
# 唯一产品事实源 = 根 CMakeLists.txt。本文件不 add_subdirectory 自己，只把
# 「接入根图所需的最小改动」写清，供 INT-001 在最终切换时登记（本任务不改根文件）：
#
#   1) 根 CMakeLists.txt 在 astrocs target 之前加一行：
#        add_subdirectory(lib/infrastructure/cli)
#   2) astrocs target 的 include 目录加：
#        ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/cli
#        ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/cli/normalize
#        ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/cli/mosaic
#        ${CMAKE_CURRENT_SOURCE_DIR}/lib/infrastructure/cli/export
#   3) 当前实现为 header-only（inline 命令入口），不新增 .cpp，故 add_executable
#      源清单无需变化；若后续拆分 .cpp，把 astrocs_cli_subcommands 加入
#      astrocs 的 target_link_libraries。
#
# 三个子命令模块各声明一个 INTERFACE target，便于模块化引用与共址测试引用；
# 不 install、不产出独立二进制（CLI 只有唯一可执行 astrocs，ASTROCS_DESIGN §6.2）。
cmake_minimum_required(VERSION 3.16)

add_library(astrocs_cli_subcommands INTERFACE)
target_include_directories(astrocs_cli_subcommands INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/normalize
  ${CMAKE_CURRENT_SOURCE_DIR}/mosaic
  ${CMAKE_CURRENT_SOURCE_DIR}/export)

add_library(astrocs_cli_subcommand_normalize INTERFACE)
target_include_directories(astrocs_cli_subcommand_normalize INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/normalize)
target_link_libraries(astrocs_cli_subcommand_normalize INTERFACE astrocs_cli_subcommands)

add_library(astrocs_cli_subcommand_mosaic INTERFACE)
target_include_directories(astrocs_cli_subcommand_mosaic INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/mosaic)
target_link_libraries(astrocs_cli_subcommand_mosaic INTERFACE astrocs_cli_subcommands)

add_library(astrocs_cli_subcommand_export INTERFACE)
target_include_directories(astrocs_cli_subcommand_export INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/export)
target_link_libraries(astrocs_cli_subcommand_export INTERFACE astrocs_cli_subcommands)
