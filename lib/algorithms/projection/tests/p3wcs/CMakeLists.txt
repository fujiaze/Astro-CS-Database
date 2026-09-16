# lib/algorithms/projection/tests/p3wcs/CMakeLists.txt — p3_wcs (TAN 投影/WCS 内核) 共址测试
#
# W4-A9 批次 1: 源自 tests/unit/p3_wcs_test.cpp 迁入本模块共址测试目录
# (ENGINEERING_SPEC §4.6「共址可复用测试」; ASTROCS_DESIGN §7.1 projection)。
# target 名 p3_wcs_test 与 ctest 名 p3_wcs 保持不变 —— 外部消费者
# tests/unit/p1wcs/CMakeLists.txt::p1wcs_std_f1_bridge_cross 以 $<TARGET_FILE:p3_wcs_test>
# 取该二进制 (STD-F1 导出边界 +1 桥接第三方交叉), 改名即断链。
add_executable(p3_wcs_test p3_wcs_test.cpp)
target_include_directories(p3_wcs_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../infrastructure/aio/include)
target_link_libraries(p3_wcs_test PRIVATE astrocs_p3_projection_wcs astrocs_common)
add_test(NAME p3_wcs COMMAND p3_wcs_test)
