# reverse_verify —— 独立构建（standalone）
#
# 本文件 **不得** 被根 CMakeLists.txt 引用；
# 独立构建：cmake -S reverse_verify -B run/reverse_verify/build -G Ninja
cmake_minimum_required(VERSION 3.20)
project(astrocs_reverse_verify LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# 实验目标按需在此登记；每个实验一个可执行文件，互不依赖。
#
# P1-SPATIAL-GAIN: 低阶空间乘法增益的独立 Oracle (C++17, 无外部依赖)
#   判据与阈值内嵌在源码顶部 (判据先行); 退出码 0 = 全部 PASS。
add_executable(rv_p1sg_oracle experiments/p1_spatial_gain/cpp/p1sg_oracle.cpp)
target_compile_options(rv_p1sg_oracle PRIVATE -O2 -Wall)

# 若需对照主线静态库，显式传 -DASTROCS_BUILD_DIR=<path>，不要写死绝对路径。
set(ASTROCS_BUILD_DIR "" CACHE PATH "主线 build/ 目录（可选，仅用于对照）")

message(STATUS "reverse_verify: standalone project (不并入主线构建)")