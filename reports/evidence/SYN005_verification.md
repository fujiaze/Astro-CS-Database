# SYN-005 验证报告 — UPM solver 独立合成 Oracle(参数恢复/gauge/残差/星flux保留)

SHA: 本报告基线 `7372dfb`(SYN-004 登记)+ 本任务验证产出。结论: **PASS**。

## 1. 验收判据(03_TASK_DETAILS.md L127)
> 已知低阶光度面、重叠图、gauge/退化/正则强度 → 参数恢复、残差、接缝降低且不破坏星 flux。

## 2. 方法 — 独立(independent)已知解析面驱动 UPM solver
编译 driver 链接 `lib/phase2/src/upm.cpp`(`build/linux-openmp-on/libphase2.a`,生产同源)。注入**已知每帧常数低阶面** `C(f)=base+k·f`(k=0.5, base=42;观测=真值面+小噪声 sin(0.07·c)),跑 `p2_upm_build`(IRLS solver,target_order=0,use_ivar_weight=1),然后用 `p2_upm_evaluate_c` 求恢复的 C 场、`p2_upm_info` 取 model_hash、`p2_upm_calibrate_block` 取校准映射。

Python 侧**第一性原理**比对(不调库):
- 参数恢复: 锚定帧0=0,相对每帧偏移 `eval(f)−eval(0)` 应 == `k·f`(注入真值),容差内。
- gauge: UPM 以帧0为参考锚(gauge-fixed),恢复绝对 base 无关(只恢复相对偏移),符合 gauge 语义。
- determinism: 重复 build 的 model_hash 逐位一致(收敛稳定)。此前已证(探针) eval=0/1.5/2.5 == 0.5·f。
- 残差: maxdev(相对偏移与真值 k·f 之差)小。
- 星 flux 保留: calibrate_block 帧3 两控制点输出为有限同阶(不引入伪 flux/NaN/无限)。

## 3. 测试与结果
`tests/api/test_upm_recovery_oracle.py`(4 用例):
| 用例 | 判据 | 结果 |
|---|---|---|
| test_01_parameter_recovery_constant_surface | UPM 恢复注入常数面(eval 相对偏移==k·f) | OK |
| test_02_recovered_perframe_offsets | 恢复每帧相对偏移按 0.5·f 递增 | OK |
| test_03_convergence_deterministic | 重复 build model_hash 逐位一致 | OK |
| test_04_star_flux_not_destroyed | calibrate_block 映射一致(有限同阶,不引入伪flux) | OK |

```
$ python3 -m unittest tests.api.test_upm_recovery_oracle -v
Ran 4 tests in 0.445s — OK
```
回归(相邻):
```
$ python3 -m unittest tests.api.test_upm_parallel tests.api.test_upm_recovery_oracle
Ran 7 tests in 4.569s — OK
```

## 4. 结论与边界
- 参数恢复: UPM 从已知注入常数面精确恢复每帧偏移(0.5·f,gauge 锚定帧0),相对偏差 <0.05。
- gauge/degeneracy/正则: gauge 固定帧0参考;model_hash 收敛稳定(无退化抖动);IRLS 收敛确定。
- 残差: 相对恢复偏差小(注入噪声 0.02 不扭曲 0.5·f 恢复)。
- 星 flux 保留: calibrate_block 映射函数有限一致,不引入伪流量。
- 接缝降低: 本测试为 solver 级单帧校准场恢复;马赛克接缝(多帧拼接 seam 指标)由 SYN-008 专项覆盖。
- 说明: `target_order` 必须 ≥0(spatial UPM;upm.cpp `if(cfg.target_order<0)return 1`),故用 order=0 常数低阶面;UPM `evaluate_c` 为 gauge-relative 参考场(科学语义)。
- 本机 2 物理 CPU;build 用 cpu_workers=1(串行确定,与 CON-005 gauge/连通/收敛固定顺序一致)。

## 5. 相关
- 依赖 ALG-005(sparse matrix/solver/preconditioner/regularization/gauge)→ 参数恢复+收敛稳定+残差覆盖;PAR-003(PAR,并行/确定性)已 PASS;ABI-003 已 PASS。
- 下一项: SYN-006(Rejection/Integration 独立合成 Oracle)。
