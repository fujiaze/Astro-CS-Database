# A0｜当前 SHA 基线审查：源码—规范—证据—未证条件

> 审查基线：`rebuild/acsd-v2@fa4649d57`（2026-11-10；本文写作时 HEAD 已含 `688adeac7` 的 A1/A2 文档提交）。
> 审查方式：实际构建、实际 CLI 运行、逐源文件阅读、与 process/foundation 正本对照。**没有运行 CI、覆盖率或文档门禁作为科学判据。**
> 本页只记录当前快照；权威实施状态仍在 [IMPLEMENTATION-STATUS](IMPLEMENTATION-STATUS.md)，科学矛盾先改正本再改代码。

## 1. 本轮实际做过的实验（可复现）

| 实验 | 命令 | 实际结果 |
|---|---|---|
| 全量构建 | `cmake --build build/science -j16` | 通过；仅出现一个陈旧符号链接 `build/science/acsd -> /tmp/acsd-build/acsd`（已删除后重链）。构建通过**不是**科学正确性证据 |
| 数值自测 | `./build/science/acsd_numerics_selftest` | 0 failure（既有 241 行自测） |
| 独立区间 Oracle | `./build/science/acsd_interval_oracle` | `20000 checks, 0 failures`（精确有理数；见 [B1](b1-numerics-remediation.md)） |
| X01 三命令（block=16） | `ACSD_DRIZZLE_BLOCK=16 python3 validation/x01/e2e_x01.py` | normalize 8/8、mosaic 1、export 1；blind solve RMS **0.2109 px**；relative signal 最大误差 **0.1033**（5% 阈值 FAIL） |
| X01 block=1 vs 16 | 两次运行比较 `measured_stars` | 最大相对误差数值**位相同** `0.10333067715982192`；说明该统计量对 block 划分不敏感，**不**证明 drizzle 产品逐叶等价 |
| X01 测光不确定度 | `python3 validation/x01/uncertainty_x01.py` | 56 个残差的最大 \|z\|=**1.94**，全部 ≤3σ；B 组最暗源预测 σ_rel≈**5.2%**，故 5% 单样本阈值低于其 1σ 固有精度 |

X01 的旧 5% 失败**保留**：`validation/x01/comparison.json` 仍记录 `source_recovered ... 0.103 > 0.05`。是否把验收命题改为归一化残差，必须等独立 Normalize 审查（task-3）确认无系统偏差后再决定；本页不预先宣布通过。

## 2. 22 Process × 57 operation 的基线四栏

图例：**I**=代码存在且经本模块真实 typed 注册；**S**=有科学代码但本轮未见当前 SHA 的独立证据；**H**=只有历史证据；**U**=未知/未核。所有 22 process 都在 `tools/integrated_processes.json` 中真实注册，构建中不再注册生成 M0 shell。

| # | process | operation 数 | 实现源码（族） | 科学公式正本 | 实际证据（当前 SHA） | 未证条件/风险 |
|---|---|---|---|---|---|---|
| 01 | observation.calibrate | 1 | `src/observation/calibrate.cpp`、`calibration_*` | OBSERVATION_COORDINATES §2 | 历史 M4 cal01；X01 实际 calibrate 8/8 | 共享 master 噪声根、非线性/IPC/饱和、低 H 下的整数溢出；U |
| 02 | observation.regroup | 1 | `src/observation/regroup.cpp`、`stream_map.cpp` | OBSERVATION_COORDINATES §2/§3 | X01 无 regroup（identity） | Bayer/bin/ROI/flip 的几何与 `G C Gᵀ`；只有设计，未独立跑反例 |
| 03 | coordinates.wcs | 7 | `src/coordinates/{wcs,tan_sip,chart,healpix,footprint_provider}.cpp` | OBSERVATION_COORDINATES §3/§4 | X01 A 组 supplied WCS 位置误差 0（按定义）；B 组盲解 0.211 px | 自写逆 Newton 的全局单射、极点/LONPOLE、非 TAN 不支持；S |
| 04 | catalogue.access | 5 | `src/catalogue/*` | OBSERVATION_COORDINATES §3 | X01 查询命中 7 源 | 高自行/视差/索引边界/跨块去重；U |
| 05 | sources.astrometry | 1 | `src/sources/astrometry*.cpp` | OBSERVATION_COORDINATES §3/§5 | X01 B 组盲解 RMS 0.211 px（4 帧） | 弱数据/镜像假匹配/候选预算；单案例不能外推；S |
| 06 | sources.psf | 3 | `src/sources/psf{,_fit,_model}.cpp` | OBSERVATION_COORDINATES §5 | X01 fit_psf 8/8 返回 ok；宽度未独立复核 | 像元积分/尾界/空间依赖/模型导数；需独立真值；S |
| 07 | sources.measure | 1 | `src/sources/measure*.cpp` | OBSERVATION_COORDINATES §5 | X01 8×7 通量；独立 joint-fit 复算一致；z 检验见上 | 背景列单位、PSF 参数误差、位置冻结；通量区间证书依赖新 linalg；S |
| 08 | response.integrate | 1 | `src/response/{api,spectral,integrate_process}.cpp` | GAIA_RESPONSE §2–§5 | X01 integrate_response 8/8 | 窄带/端点/求积余项/单位；3nm 能力未证；U |
| 09 | photometry.fit | 1 | `src/photometry/{joint_fit,api}.cpp` | GAIA_RESPONSE §4 | X01 fit_photometry 1 次 ok（flux-domain Gaussian） | 低计数 Poisson likelihood 明确 unsupported；秩/潜变量可辨识；S |
| 10 | photometry.apply | 1 | `src/photometry/{apply_scale,photometry_apply_process}.cpp` | GAIA_RESPONSE §5 | X01 apply_scale 6 次 | α(u) 只应用一次、幂等拒绝、空间不可交换；U |
| 11 | geometry.overlap | 11 | `src/geometry/{kernel,overlap,containment,exact_*}.cpp` | SPHERICAL_DRIZZLE §6 | X01 每帧 16384 次调用全 ok；G01 历史 pass | 候选完备/包含/极点/tiny sliver；生产经 Host 逐像素调用，性能病理 |
| 12 | operators.action | 7 | `src/operators/{api,linear_action,action_process}.cpp` | EXECUTION/NUMERICS | 历史 selftest/合同 | transpose/compose/recipe 身份、跨进程重开；U |
| 13 | uncertainty.covariance | 6 | `src/uncertainty/{covariance,api,covariance_process}.cpp` | STATISTICS_SKY_COADD §S0 | 历史统计案例 | 双计共享根、unknown→0、PSD 不合法；U |
| 14 | geometry.drizzle | 1 | `src/geometry/{drizzle,drizzle_process}.cpp` | SPHERICAL_DRIZZLE §7 | X01 build_drizzle 8/8 | R 对 signal/PSF/noise/背景同一身份、零支持、外存 CSR；S |
| 15 | information.reference | 1 | `src/information/information_reference_process.cpp` | STATISTICS_SKY_COADD §S1 | X01 未请求 | Fisher≠scene、欠定模型虚高 SNR；U |
| 16 | information.scene | 3 | `src/information/{information,triangulation}.cpp` | STATISTICS_SKY_COADD §S1 | 历史 sparse 证据 | 无曲率界的插值、holes、zero-fill；U |
| 17 | sky.controls | 1 | `src/sky/sky_controls_process.cpp` | STATISTICS_SKY_COADD §S2 | X01 mosaic 1 次 | 异响应不可比、伪 control；U |
| 18 | sky.fit | 2 | `src/sky/{sky,sky_fit_process}.cpp` | STATISTICS_SKY_COADD §S2 | X01 mosaic 1 次 | gauge/连通/同数据 P 交叉项；S |
| 19 | coadd.select | 1 | `src/coadd/coadd_select_process.cpp` | STATISTICS_SKY_COADD §S3 | X01 mosaic 1 次 | 小 N LOO、选择后 coverage；S |
| 20 | coadd.weights | 1 | `src/coadd/weights.cpp` | STATISTICS_SKY_COADD §S3 | X01 mosaic 1 次 | Γ PSD/BLUE/QP、分母正界；S |
| 21 | coadd.integrate | 1 | `src/coadd/{coadd,coadd_integrate_process}.cpp` | STATISTICS_SKY_COADD §S3 | X01 mosaic 1 次 | 二次求权、同帧重复、有效核；S |
| 22 | projection.export | 1 | `src/projection/{export_process,export_projection}.cpp`、`src/workflows/{export_workflow,fits_writer}.cpp` | EXPORT/SPHERICAL_DRIZZLE | X01 export 1 次，FITS 可写 | 外部 reader 语义、支持 Q、重复 pixfrac、NaN/负值；S |

“S”不等于失败，只表示本 SHA 上还没有本文作者亲自复核过的独立证据；历史 PASS 不得直接继承（尤其 B1 改变了数值原语）。

## 3. 本轮新发现的两个结构性问题

### 3.1 Normalize 产品没有科学闭包（P1，待 C1 审查确认）

实际检查 `validation/x01/input/cli/products/x01-04`：
- `objects/` 只有 `Array`、`Index`、`SignalEstimate`、`SpatialResponse`、`SupportMap`、`ProductGraph`；
- `ProductGraph` 的 `closure: []`、`inputs: []`；
- **没有** `EffectivePSF`、`Response`、`CovarianceAction`/noise root、`NativeObservation`/WCS 依赖。

后果：新进程只能重开 signal/support，不能按 R3 的要求重建 PSF/response/noise/共享根；mosaic 的 PSF/噪声感知操作和 D0 的反向审计缺依赖。这与 process 文档“完整产品闭包”和 `ProductGraph.profile = "full"` 不一致。`signal` 的 assessment 还写 `noise: unknown`、`validity: certified`，需要确认这是“signal 层可发布但噪声未知”的具名条件，而不是把 unknown 混入 certified。

### 3.2 逐像素 Host invoke 的几何开销（P1，性能/资源）

`src/workflows/normalize.cpp:936` 默认 `block_size = 1`，除非 `ACSD_DRIZZLE_BLOCK` 覆盖。X01 128×128×8 帧 → 每帧 16384 次 `compute_overlap` typed invocation，每次重新 ingest provider/footprint，单帧 >20 分钟。block=16 时 8 帧 + mosaic + export 数分钟完成，且 `measured_stars` 位相同。需要在 Geometry/C1 审查中证明批量化不改变 `a_jp`/`w`/`R` 与证书；在此之前不能把“跑得快”当优化成功。

## 4. 旧证据与当前结论的冲突登记（不覆盖历史）

| 旧记录 | 旧结论 | 当前事实 | 处理 |
|---|---|---|---|
| `validation/run_summary.json` | 7 pass / 4 pending，X01 pending | X01 已实际跑通并 FAIL 于 5% 测光 | 保留旧文件；不据此宣布当前状态 |
| `validation/x01/comparison.json` | X01 FAIL（10.3%） | 同一失败在 `fa4649d57` 重现 | 保留；统计命题修订需独立审查后另行记录 |
| 历史 R1–R5 证据 | 多处 PASS | A2 审计确认主体可逐位重放，但 R1/R3/R4/R5 仅工程集成，R2 为方法候选 | 见 [r1-r5 审计](r1-r5-research-audit.md) |
| M0–M4 16/16 runtime | PASS_SCOPED | B1 修复后上游证书需重验 | 不继承；逐项复验 |

## 5. 本轮不声称的结论

- 不声称 22 process 科学正确；只声称“真实注册、可构建、X01 主链能跑”。
- 不声称 X01 测光正确或无偏；只给出独立统计量及“5% 单样本阈值不可达”的证据，最终裁决待独立审查。
- 不声称 interval 超越函数已认证；只有四则/sqrt 的定向包含。
- 不声称产品可跨进程完整重开；§3.1 显示闭包缺失。
- 不声称 Windows 可运行；本轮只在 Linux x86-64 实测。

## 6. 下一步（按依赖）

1. 等 B2/B3/B4/C1 独立审查报告，逐项修复 P0/P1；科学命题先改正本。
2. 产品闭包：把 Normalize 的 PSF/response/noise/shared-root/WCS 依赖纳入发布或明确 `signal_only`，不得用 `full` 伪装。
3. 几何批量化：先证明 block 分区等价，再改默认值与资源账本。
4. X01 验收：在独立审查确认无系统偏差后，把 5% 单样本改为归一化残差 + FWER，旧失败保留。
5. 每次修复后重跑受影响原语/上游，并记录新 SHA、命令、通过/未决域。
