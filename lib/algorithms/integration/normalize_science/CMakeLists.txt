# ACSD Normalize — frame_science.json 唯一 serializer/reader（NORMALIZE_DATA §7/§8）
#
# 本 target 只做阶段一产品的序列化/读回验证与原子发布原语接线，**不做科学计算**
# （数学在 acsd_phase1_noise 的 complete_spherical_reference）。
if(NOT TARGET acsd_normalize_science)
  add_library(acsd_normalize_science STATIC
    src/normalize_science.cpp)
  target_include_directories(acsd_normalize_science PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party
    # 统一 I/O 原子落盘 / 内容哈希
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../infrastructure/aio/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../infrastructure/aio/include)
  target_link_libraries(acsd_normalize_science PUBLIC acsd_contracts)
endif()
