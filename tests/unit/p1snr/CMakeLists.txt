# ============================================================================
# P8-SNR-LINUX 回归锁: Linux 生产路径的逐源 SNR
# ----------------------------------------------------------------------------
# 被测面 = **真实生产目标** astrocs_phase1_noise (根 CMakeLists.txt 声明),
#   即 lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp +
#      lib/algorithms/noise_snr/cpp/src/snr_science.cpp
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
#   p1snr_linux_common_ref  — 组内公共 F_ref（WEIGHT-SCI-001）
#   p1snr_linux_mother_function — fwhm_px 母函数口径（CONFORM-FIX-A ①: 禁止跨块混用）
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
# WEIGHT-SCI-001: 组内公共 F_ref（逐帧中位数回退已删除 ⇒ fail-closed 负例）
add_test(NAME p1snr_linux_common_ref COMMAND p1snr_linux_test common_ref)
# CONFORM-FIX-A ①: fwhm_px 母函数口径（检测块高斯 vs PSF 块 Moffat4, 禁止跨块混用）
add_test(NAME p1snr_linux_mother_function COMMAND p1snr_linux_test mother_function)
set_tests_properties(
  p1snr_linux_units p1snr_linux_oracle p1snr_linux_contract
  p1snr_linux_negative p1snr_linux_determinism p1snr_linux_common_ref
  p1snr_linux_mother_function
  PROPERTIES LABELS "p1snr")

# ============================================================================
# P14-N-08/N-09 (RQS V2-N-08 + V2-N-09) — 交付 SNR/极限星等样本真实性 parity 锁
# ----------------------------------------------------------------------------
# 端到端直调生产 op (p1_op_star_psf_json / p1_op_noise_json):
#   A. psf.max_stars=0/5000/2 三次运行, 交付 SNR/深度逐位一致 (性能开关解耦);
#   B. psf_mode/n_fit_input/n_sources/truncated/psf_fit_truncated/snr_sample
#      如实落到产物;
#   C. snr.max_sources 人为截断 -> truncated=true 且数值确实不同 (非恒真)。
# ============================================================================
# ============================================================================
# FREF-BASELINE-001 — 帧级 SNR 参考通量的**固定星等绝对共同基准** 判别力锁
# ----------------------------------------------------------------------------
# OLD 形态上游产物（p1_phot.json 无 photscale_fit）⇒ 参考通量只能是数据派生的
# 块级中位数（帧不独立、无物理含义、丢 a_f²）⇒ N1–N5 全红;
# NEW 形态 ⇒ 固定星等基准生效, 帧间独立且与帧集无关 ⇒ 全绿。
# ============================================================================
add_executable(p1snr_fref_baseline_test p1snr_fref_baseline_test.cpp)
target_include_directories(p1snr_fref_baseline_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/phase1_session/tests/p1sess)
target_link_libraries(p1snr_fref_baseline_test PRIVATE
  astrocs_module_adapters astrocs_core astrocs_phase1_session)
add_test(NAME p1snr_fref_baseline COMMAND p1snr_fref_baseline_test)
set_tests_properties(p1snr_fref_baseline PROPERTIES LABELS "p1snr")

add_executable(p1snr_frame_parity_test p1snr_frame_parity_test.cpp)
target_include_directories(p1snr_frame_parity_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/phase1_session/tests/p1sess)
target_link_libraries(p1snr_frame_parity_test PRIVATE
  astrocs_module_adapters astrocs_core astrocs_phase1_session)
add_test(NAME p1snr_frame_parity COMMAND p1snr_frame_parity_test)
set_tests_properties(p1snr_frame_parity PROPERTIES LABELS "p1snr")
