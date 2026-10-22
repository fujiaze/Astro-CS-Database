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

## 已知失败（根因待裁决）

`P1_photometry_cross_frame_correlation`：帧尺度协方差为 0（期望 corr≈0.4999996）。
用本模块的已提交版本（6d6d6a348）重编后同样失败，故**不是后续改动引入的**。

已排除的假设：底座的因子作用没有多算。`validation/phot/covariance_probe.cpp`
（只用公开合同、自写 Storage/Resources/Cancellation）对角根 F=diag(1e-5)、
根的 influence 为恒等、协方差动作自身的 influence 为空，实测
`covariance_apply(v=(1,0)) = 1e-10` = 合同写的 C = F·Fᵀ（此前一次 1e-15 的测量是
探针自身把因子数组误当作输入向量所致，结论作废）。

已测得的事实：两帧用例的初始量都正常（抽出的 XP 协方差 1e-10、白化子 1e5、
初始 log a −36.7414、counts 9442），但首迭代的**剖面（Schur 约化）Jacobian**
量级退化到 ~1e-159，即约化里的相减发生了灾难性抵消。根因待 Lead 裁决。

## 独立探针

`validation/phot/covariance_probe.cpp`：底座协方差语义的最小复现（见上）。

## 未覆盖 / 交接

* 真实 Gaia XP 数据、真实 aperture/PSF 偏差、Huber 的聚类 bootstrap 验证未做
  （需要真实数据或独立 `val` Agent 的真值包）。
* 大规模星数的稀疏 QR/LSQR 路线未实现：稠密规模超预算时返回失败原因。
