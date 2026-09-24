# lib/algorithms/projection/tests/p3wcs/CMakeLists.txt — p3_wcs (TAN 投影/WCS 内核) 共址测试
#
# W4-A9 批次 1: 源自 eng/tests/unit/p3_wcs_test.cpp 迁入本模块共址测试目录
# (ENGINEERING_SPEC §4.6「共址可复用测试」; ASTROCS_DESIGN §7.1 projection)。
# target 名 p3_wcs_test 与 ctest 名 p3_wcs 保持不变 —— 外部消费者
# eng/tests/unit/p1wcs/CMakeLists.txt::p1wcs_std_f1_bridge_cross 以 $<TARGET_FILE:p3_wcs_test>
# 取该二进制 (STD-F1 导出边界 +1 桥接第三方交叉), 改名即断链。
add_executable(p3_wcs_test p3_wcs_test.cpp)
target_include_directories(p3_wcs_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../infrastructure/aio/include)
target_link_libraries(p3_wcs_test PRIVATE astrocs_p3_projection_wcs astrocs_common)
add_test(NAME p3_wcs COMMAND p3_wcs_test)
# FIX-205: 投影产品声明注册表 + 显式「不支持」 + 适用域 + 机器判据
# （声明集 == 实现集 == 实际可运行集; 8 冻结码逐一黑盒实跑探针）。
# p3_proj_v6.cpp 一并编入以做跨注册表一致检查（内核行不得冒充产品声明）。
add_executable(p3_projection_registry_test
  p3_projection_registry_test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../p3_proj_v6.cpp)
target_include_directories(p3_projection_registry_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(p3_projection_registry_test PRIVATE
  astrocs_p3_projection_wcs)
add_test(NAME p3_projection_registry COMMAND p3_projection_registry_test)
# 负例入口（ENGINEERING_SPEC §8）: 判据函数在变异输入下必红、真值输入必绿。
add_test(NAME p3_projection_registry_selftest
  COMMAND p3_projection_registry_test --self-test)

# FIX-205 端到端负例: 请求未实现投影 ⇒ rc≠0 + 明确原因（不得静默回落 TAN）。
find_program(PYTHON3_EXECUTABLE python3)
if(PYTHON3_EXECUTABLE)
  add_test(NAME p3_projection_unsupported_cli
    COMMAND ${PYTHON3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/p3_projection_unsupported_cli.py
            $<TARGET_FILE:acsd>)
endif()
