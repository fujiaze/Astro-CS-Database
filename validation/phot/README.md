# phot 模块独立验证（response / photometry）

本目录是 `phot` Agent 的独立验证入口。它**只**包含公开声明合同
`include/acsd/contracts.hpp`，自己实现 Storage / Resources / Cancellation，
因此不依赖 `src/` 下的任何内部头，也不读取被测实现的中间结果。

## 运行

```sh
cmake -S . -B build/dev -G Ninja -DACSD_BUILD_SCIENCE=ON
cmake --build build/dev --target response photometry
validation/phot/run_validation.sh            # 默认使用 build/dev
```

真实运行输出保存在 `validation/phot/evidence/phot_validation.txt`（随提交入库），
程序在任一用例失败时返回非零退出码。

## 用例与判据

| 用例 | 内容 | 判据 | 实测 |
|---|---|---|---|
| `encoder_self_check` | 本地 little-endian 编码/解码逐位往返自检（防 2026-10-22 底座 P0 同类错误） | 字节数 = 元素数×itemsize 且逐位相同 | PASS |
| `R1_response_analytic_photon` | top-hat 响应 + 常数谱基，photon 约定 `∫R·λ/(hc)dλ` 对照闭式真值 | 相对差 ≤ 1e-15 | 8.88e-16 |
| `R1_response_analytic_energy` | 同上，`includes_photon_conversion` 约定（不再乘 λ/(hc)） | 相对差 ≤ 1e-15 | 8.88e-16 |
| `R2_response_segmented` | 5 节点分段线性响应（4 个折点）+ 常数谱基，对照逐段解析积分 | 相对差 ≤ 1e-14 | 3.36e-15 |
| `R2_response_top_hat_edge_on_knot` | top-hat 端点正落在谱基折点上（端点不跨阶跃） | 相对差 ≤ 1e-15 | 8.68e-16 |
| `P1_photometry_shared_latent` | 2 帧 × 6 星、共享谱潜变量、平面空间基，精确数据 | 恢复的 log a 与真值一致（≤1e-9） | 误差 0（−36.741361487904733） |
| `P1_photometry_cross_frame_correlation` | 同一 source 跨帧共用 latent 后的帧尺度协方差 | 相关系数非零且 ≤1（真相关，非重置误差） | **当前失败：cov=0**。原因在底座：src/uncertainty 的对角协方差作用对单位向量返回 1e-30 而不是 1e-10（实测），使光度模块抽出的 XP 方差错误、共享 latent 的白化退化；本模块的提交 6d6d6a348 与本次改动都复现该结果 |
| `P2_photometry_rank_deficient` | 训练位置共线（颜色/位置混淆的等价构型），平面基 | 必须返回 `not_identifiable`（7），不得 ridge、不得发布尺度 | code 7 |
| `P3_photometry_unknown_variance` | 测量方差未知（未知 gain 分支） | 未知项必须被记录，结果必须为 conditional；尺度不得无条件发布 | 未知项已记录，conditional，尺度误差 0 |
| `F01_scale_interval` | counts=100、通量包围 [100,100] 与 [99,101] | 紧包围给出点区间；宽包围给出 log 1.01−log 0.99 的区间并含 [log 0.99, log 1.01] | 紧 1.4e-14；宽 0.02000067，区间 [−36.851411823758234, −36.831411157051555] |
| `F01_scale_interval_width` | 同上的区间宽度差 | 紧包围宽度 <1e-9 且宽包围宽度 >1e-3 | 1.42e-14 / 0.0200 |
| `S1_apply_scale_per_pixel` | 4×4 原生像元上应用空间标度 | 逐像元因子必须不同（不得用星心 α 代替全 PSF） | 因子在 8.646e15…8.711e15 间变化 |
| `S1_apply_scale_identity_conflict` | 同一 application_id 重复应用 | 必须返回 `identity_conflict`（10） | code 10 |

## 口径说明

* 合成观测的「计数」为 `count_scale · exp(log a + h·θ) · (v·c)`；拟合参数
  `log a` 的真值因此为 `log(count_scale) + log a_true`（`a` 是 native ADU / 光子率）。
* 潜变量按每源分块消元（计数协方差不耦合不同源时），全局用 Schur/QR 解 θ 并回代；
  协方差固定，故 profile 与 marginal 只差常数 logdet。
* 数值参数盒为「剖面区间」：在潜变量固定于剖面最优时，使模型预测仍落在
  测量通量数值包围内的最大参数偏移，上下界分别求解（非对称）。
* 未知测量方差不做等权以外的任何假设：等权回退 + 记录未知项 + 结果 conditional。

## 已修复的失败（根因已定位）

`P1_photometry_cross_frame_correlation` 曾经是：帧尺度协方差为 0（期望 corr≈0.4999996）。
用本模块的已提交版本（6d6d6a348）重编后同样失败，故不是后续改动引入的。

**根因（两处，都在数值构造上）**：
1. **稳健权把设计矩阵删掉了**：验证夹具没有给 `SolveRequest` 的 `huber`/`huber_delta`
   赋值 ⇒ 读到的阈值是随机值（实测 denormal 3.5e-322）⇒ Huber 行权 = √(δ/|r|)
   ≈ 1e-161 ⇒ **白化后的 Jacobian 整体变成 denormal** ⇒ 剖面约化后的 Jacobian
   ~1e-159 ⇒ 参数协方差塌成 0 ⇒ 帧尺度相关系数 = 0。同时残差 ~0 使 χ² ~1e-321
   （denormal），梯度判据立刻判「收敛」⇒ 拟合停在初始值（所以尺度恢复误差 0
   其实是没动）。
2. **剖面约化用了显式相减**：`J_θ − J_c(J_cᵀJ_c)⁻¹J_cᵀJ_θ` 把潜变量块的条件数平方，
   相减发生灾难性抵消。

**修法**：
- 剖面约化改为**用底座最小二乘（Householder QR）的残差**构造，即 J_θ 在 J_c 零空间上的
  正交补分量（口径不变，构造稳定）；
- 收敛判据显式排除 denormal/非有限：χ²、score 或剖面设计的最大奇异值是 denormal 或
  非有限 ⇒ 报 `precision_exhausted`，**不得**判 converged；
- 稳健权必须是正规数（不是 denormal），否则拒绝该权并记录；
- API 侧：请求里的 `huber_delta` 不是正规数 ⇒ 用默认 1.345；
- 验证夹具：`SolveRequest` 的每个字段都显式赋值。

**修复前后的数字**：
| | 修复前 | 修复后 |
|---|---|---|
| 帧尺度相关系数 | 0 | **0.49999959655265813**（解析值 0.4999996） |
| 剖面（Schur 约化）Jacobian | ~1e-159（denormal） | 正常量级（条件数记录在 assessment） |
| 白化 Jacobian | ~1e-154（denormal） | 1e5 |
| apply_scale 逐像元因子末位 | …1460594（1e-16 偏差） | …1460347（与 6d6d6a348 一致） |

**回归断言**：`P1_photometry_cross_frame_correlation` 现在断言相关系数等于设计的解析值
0.4999996（|Δ|<1e-6），且断言 assessment 记录了 `non_degenerate_profile_reduction`
（收敛必须伴随一个非退化的约化 Jacobian）。

**已排除的假设**：底座的因子作用没有多算。`validation/phot/covariance_probe.cpp`
（只用公开合同、自写 Storage/Resources/Cancellation）对角根 F=diag(1e-5)、
根的 influence 为恒等、协方差动作自身的 influence 为空，实测
`covariance_apply(v=(1,0)) = 1e-10` = 合同写的 C = F·Fᵀ（此前一次 1e-15 的测量是
探针自身把因子数组误当作输入向量所致，结论作废）。

## 独立探针

`validation/phot/covariance_probe.cpp`：底座协方差语义的最小复现（见上）。

## 未覆盖 / 交接

* 真实 Gaia XP 数据、真实 aperture/PSF 偏差、Huber 的聚类 bootstrap 验证未做
  （需要真实数据或独立 `val` Agent 的真值包）。
* 大规模星数的稀疏 QR/LSQR 路线未实现：稠密规模超预算时返回失败原因。
