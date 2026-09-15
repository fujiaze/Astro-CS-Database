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

# ============================================================================
# P14-N-08/N-09 (RQS V2-N-08 + V2-N-09) — 交付 SNR/极限星等样本真实性 parity 锁
# ----------------------------------------------------------------------------
# 端到端直调生产 op (p1_op_star_psf_json / p1_op_noise_json):
#   A. psf.max_stars=0/5000/2 三次运行, 交付 SNR/深度逐位一致 (性能开关解耦);
#   B. psf_mode/n_fit_input/n_sources/truncated/psf_fit_truncated/snr_sample
#      如实落到产物;
#   C. snr.max_sources 人为截断 -> truncated=true 且数值确实不同 (非恒真)。
# ============================================================================
add_executable(p1snr_frame_parity_test p1snr_frame_parity_test.cpp)
target_include_directories(p1snr_frame_parity_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/phase1_session/tests/p1sess)
target_link_libraries(p1snr_frame_parity_test PRIVATE
  astrocs_module_adapters astrocs_core astrocs_phase1_session)
add_test(NAME p1snr_frame_parity COMMAND p1snr_frame_parity_test)
set_tests_properties(p1snr_frame_parity PROPERTIES LABELS "p1snr")

# ============================================================================
# P33-COEF — 帧级单一 SNR 系数 (lib/phase1/noise/snr_frame_coefficient.cpp)
# ----------------------------------------------------------------------------
# 负责人 2026-09-15 设计变更: 一帧一个 SNR 系数, 不再交付稀疏控制点/IDW 层。
# 被测面 = 生产目标 astrocs_phase1_noise 的系数聚合:
#   value/robust(样本选择与非恒真)/determinism(顺序无关)/negative(fail-closed)
#   /serialize(JSON 往返与字段)。
# ============================================================================
add_executable(p1snr_frame_coefficient_test p1snr_frame_coefficient_test.cpp)
target_link_libraries(p1snr_frame_coefficient_test PRIVATE astrocs_phase1_noise)
if(UNIX)
  target_link_libraries(p1snr_frame_coefficient_test PRIVATE m)
endif()
add_test(NAME p1snr_scf_value COMMAND p1snr_frame_coefficient_test value)
add_test(NAME p1snr_scf_robust COMMAND p1snr_frame_coefficient_test robust)
add_test(NAME p1snr_scf_determinism COMMAND p1snr_frame_coefficient_test determinism)
add_test(NAME p1snr_scf_negative COMMAND p1snr_frame_coefficient_test negative)
add_test(NAME p1snr_scf_serialize COMMAND p1snr_frame_coefficient_test serialize)
set_tests_properties(
  p1snr_scf_value p1snr_scf_robust p1snr_scf_determinism
  p1snr_scf_negative p1snr_scf_serialize
  PROPERTIES LABELS "p1snr")

# ============================================================================
# P33-FMATCH (P25-F2) — 上游帧匹配: stem 归一化 + fail-closed (静默数据丢失修复)
# ----------------------------------------------------------------------------
# 端到端经生产模块注册表跑真实 star-psf -> 篡改 p1_sources.json 的 file 字段
# -> 单独跑 noise-snr:
#   p1snr_fmatch_exact     — 精确名匹配 (正例: match=exact, snr_phot 有限)
#   p1snr_fmatch_stem      — 仅扩展名不一致 (.fits/.fts) -> stem 匹配成功,
#                            match=stem 且 snr_phot 与 exact 案逐位一致,
#                            manifest n_snr_frame_match_stem=1 (旧实现静默全 null)
#   p1snr_fmatch_missing   — 无匹配 -> 节点显式失败且不落 p1_snr.json
#   p1snr_fmatch_ambiguous — 同 stem 多命中 (均非精确名) -> 显式歧义失败
# ============================================================================
add_executable(p1snr_frame_match_test p1snr_frame_match_test.cpp)
target_include_directories(p1snr_frame_match_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/phase1_session/tests/p1sess)
target_link_libraries(p1snr_frame_match_test PRIVATE
  astrocs_module_adapters astrocs_core astrocs_phase1_session)
add_test(NAME p1snr_fmatch_exact COMMAND p1snr_frame_match_test exact)
add_test(NAME p1snr_fmatch_stem COMMAND p1snr_frame_match_test stem)
add_test(NAME p1snr_fmatch_prefix COMMAND p1snr_frame_match_test prefix)
add_test(NAME p1snr_fmatch_missing COMMAND p1snr_frame_match_test missing)
add_test(NAME p1snr_fmatch_ambiguous COMMAND p1snr_frame_match_test ambiguous)
set_tests_properties(
  p1snr_fmatch_exact p1snr_fmatch_stem p1snr_fmatch_prefix p1snr_fmatch_missing
  p1snr_fmatch_ambiguous
  PROPERTIES LABELS "p1snr")

