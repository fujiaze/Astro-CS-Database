# SCI-B 仓内测试证据（run/ 不入库，此处为入库副本）

生成时间：2026-09-21T13:37:10+08:00

## 运行 A（12:4x，SCI-B 改动前）：6/6 passed

结论：p1snr_science_units/oracle/negative/production/determinism + p1noise_numpy_oracle 全部 Passed，总 3.63 s，exit 0。
该次原始日志已被后续运行覆盖（run/ 不入库），结论由会话记录与本文件运行 B 的对照支撑。

## 运行 B（复跑，当前工作树，run/SCI-402/ctest_snr.log 原件）：5/6
```
    Start 182: p1snr_science_units
1/6 Test #182: p1snr_science_units ..............   Passed    0.00 sec
    Start 183: p1snr_science_oracle
2/6 Test #183: p1snr_science_oracle .............   Passed    0.00 sec
    Start 184: p1snr_science_negative
3/6 Test #184: p1snr_science_negative ...........   Passed    0.00 sec
    Start 185: p1snr_science_production
4/6 Test #185: p1snr_science_production .........   Passed    0.00 sec
    Start 186: p1snr_science_determinism
5/6 Test #186: p1snr_science_determinism ........   Passed    0.00 sec
    Start 189: p1noise_numpy_oracle
6/6 Test #189: p1noise_numpy_oracle .............***Failed    1.36 sec
FAIL g++ 编译失败: ng ‘void’ [-fpermissive]
  239 | #define SNR_FLOOR_UNBOUND (-10)
      |                           ~^~~~
/workspace/Astro CS Database/lib/algorithms/noise_snr/cpp/src/noise_model.cpp:756:28: note: in expansion of macro ‘SNR_FLOOR_UNBOUND’
  756 |                     return SNR_FLOOR_UNBOUND;
      |                            ^~~~~~~~~~~~~~~~~
/workspace/Astro CS Database/lib/algorithms/noise_snr/cpp/src/noise_model.cpp: In function ‘int snr_noise_model_v1_fill(const NoiseWeightModelV1*, int, int, float*, float*)’:
/workspace/Astro CS Database/lib/algorithms/noise_snr/cpp/src/noise_model.cpp:786:26: error: void value not ignored as it ought to be
  786 |     try { frc = fill_impl(model, h, w, out_variance, out_ivar); }
      |                 ~~~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



83% tests passed, 1 tests failed out of 6

Label Time Summary:
p1noise    =   1.36 sec*proc (1 test)

Total Test time (real) =   1.39 sec

The following tests FAILED:
	189 - p1noise_numpy_oracle (Failed)                     p1noise
```

失败原因：并发 FIX-405 把 lib/algorithms/noise_snr/cpp/src/noise_model.cpp 改到不可编译中间态
（mtime 2026-09-21 13:14:04；fill_impl 仍为 void 却被赋值、void 函数 return SNR_FLOOR_UNBOUND）。
SCI-B 文件域不含 lib/，5 个 p1snr_science_* 始终 Passed。

## 生产 C++ 驱动对拍（40 随机点，只读链接 lib/algorithms/noise_snr/cpp/src/snr_science.cpp）
```
GATES: {'G1_def_all_within_3sigma': True, 'G1_def_max_abs_z': 2.6807576187933275, 'G1_def_n_points': 29, 'G2_total_all_within_3sigma': True, 'G2_total_max_abs_z': 2.7106852732058173, 'G4a_skyonly_rn_all_within_3sigma': True, 'G4a_skyonly_rn_max_abs_z': 2.6986289285348746, 'G4c_pred_vs_measured_mc_max_abs_diff': 0.07003034237886552, 'G4c_pred_vs_arm_ratio_max_abs_diff': 0.012500878899093903, 'FINDING_doublecount_pred_matches_arm_ratio_2pp': True, 'G4c_max_bias_over_all_points': 0.33954155771668604, 'G4c_bias_at_base_point': 0.12791299773460385, 'G3_sky_limited_n_points': 2, 'G3_sky_limited_max_rel_diff': 0.010750110729545126, 'G5_cpp_mirror_pass': True, 'G5_cpp_worst_rel_diff': 2.2392380996639857e-14}
```
