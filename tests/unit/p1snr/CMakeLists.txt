# ============================================================================
# P8-SNR-LINUX 回归锁: Linux 生产路径的逐源 SNR
# ----------------------------------------------------------------------------
# 被测面 = **真实生产目标** astrocs_phase1_noise (根 CMakeLists.txt 声明),
#   即 lib/phase1/noise/snr_frame_science.cpp +
#      lib/snr_estimator/cpp/src/snr_science.cpp
# 若 snr_science.cpp 未编入该目标, 本可执行文件链接失败 => 构建即红灯
# (这是"P5 正确实现确实接进 Linux 生产路径"的机器锁, 不依赖任何文本断言)。
#
# CTest 注册:
#   p1snr_linux_units       — 独立 long-double 参考实现 (rtol 1e-12)
#   p1snr_linux_oracle      — P5 NumPy oracle 冻结锚值 (5 真实源 + 帧级量)
#   p1snr_linux_contract    — 字段语义 (snr_phot==median_snr==median(SNR_F),
#                             local_snr 相对质量权重场, 5sigma 深度, 否认旧构造)
#   p1snr_linux_negative    — 退化输入 fail-closed (禁 1.0 伪装 unknown)
#   p1snr_linux_determinism — 重复调用逐位相同 + 顺序无关
# ============================================================================
add_executable(p1snr_linux_test p1snr_linux_test.cpp)
target_link_libraries(p1snr_linux_test PRIVATE astrocs_phase1_noise)
if(UNIX)
  target_link_libraries(p1snr_linux_test PRIVATE m)
endif()
add_test(NAME p1snr_linux_units COMMAND p1snr_linux_test units)
add_test(NAME p1snr_linux_oracle COMMAND p1snr_linux_test oracle)
add_test(NAME p1snr_linux_contract COMMAND p1snr_linux_test contract)
add_test(NAME p1snr_linux_negative COMMAND p1snr_linux_test negative)
add_test(NAME p1snr_linux_determinism COMMAND p1snr_linux_test determinism)
set_tests_properties(
  p1snr_linux_units p1snr_linux_oracle p1snr_linux_contract
  p1snr_linux_negative p1snr_linux_determinism
  PROPERTIES LABELS "p1snr")
