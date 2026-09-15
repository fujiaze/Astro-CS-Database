# =============================================================================
# IMPL-P2-UPM-001 — Phase2 UPM 乘法/加性分离共址测试
# -----------------------------------------------------------------------------
# 被测面 = 生产目标 astrocs_phase2（根 CMakeLists.txt 声明，含 lib/phase2/src/upm.cpp）。
# 若新 UPM API 未编入 astrocs_phase2，本可执行文件链接失败 => 构建即红灯。
#
# CTest 注册（每个用例覆盖一组冻结节点；负例为 fail-closed 门）：
#   v6_p2_upm_ma_units            — 数值锚(Oracle) + 精确恢复 + C_theta/C_out + 确定性
#                                   FZ-AP2S-RANK-RTOL / FZ-FORMULA-COV-PROP
#   v6_p2_upm_ma_structure        — overlap graph / 分量独立 gauge / 秩 / κ / provenance
#                                   ALG-P2S-UPM.2/.3/.4/.8 / FZ-PROV-MINIMAL-SET
#   v6_p2_upm_ma_additive         — 单帧 min_frames 门 + additive-only 降级
#                                   FZ-AP2S-UPM-MINFRAMES
#   v6_p2_upm_ma_negative         — 冻结违反必红: rc=2/3/4/6/7
#                                   FZ-AP2S-KAPPA-MAX / FZ-PROV-KCORR / ADJ-OBS-01
#   v6_p2_upm_ma_control_variance — ALG-P2S-UPM.7 / FZ-PROV-KCORR-VALUE
# =============================================================================
add_executable(v6_p2_upm_ma_test v6_p2_upm_ma_test.cpp)
target_link_libraries(v6_p2_upm_ma_test PRIVATE astrocs_phase2)
if(UNIX)
  target_link_libraries(v6_p2_upm_ma_test PRIVATE m)
endif()

add_test(NAME v6_p2_upm_ma_units COMMAND v6_p2_upm_ma_test units)
add_test(NAME v6_p2_upm_ma_structure COMMAND v6_p2_upm_ma_test structure)
add_test(NAME v6_p2_upm_ma_additive COMMAND v6_p2_upm_ma_test additive)
add_test(NAME v6_p2_upm_ma_negative COMMAND v6_p2_upm_ma_test negative)
add_test(NAME v6_p2_upm_ma_control_variance COMMAND v6_p2_upm_ma_test control_variance)

set_tests_properties(
  v6_p2_upm_ma_units v6_p2_upm_ma_structure v6_p2_upm_ma_additive
  v6_p2_upm_ma_negative v6_p2_upm_ma_control_variance
  PROPERTIES LABELS "v6_p2_upm")
