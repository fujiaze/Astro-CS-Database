# SCI-C 数据指针（data/README.md）

本实验单元**不自带数据副本**：全部输入为仓库内既有真实数据 + 固定 seed 的解析/物理前向仿真。
下表给出每类数据的来源、用途与真值口径。

## 1. 哈勃真实信号模板（最高设计 §12.2 第 1 类）

| 项 | 值 |
|---|---|
| 文件 | `testdata/testdata/HST_M16/hlsp_heritage_hst_wfc3-uvis_m16_f657n_v1_drz.fits` |
| 用途 | 当**纯信号** s(x,y)：真实星云结构 + 星点，帧间连续 |
| 取用 | `sci_c_common.load_hst_signal()` 中心裁剪 512×512（origin=(700,700)），非有限置 0，缩放到掩膜外中位 = 200 e⁻，再作 σ=1.5 px 高斯 PSF 模糊 |
| 真值口径 | 该场**只作信号**，不做物理闭合反推（不做测光定标）；声明量而非反推量 |
| 只读 | 是（不复制、不改写） |

## 2. 纯解析代数合成（§12.2 第 2 类）

由 `实验/SCI-C/code/sci_c_common.py` 在内存中构造，固定 seed `SEED_BASE=20260923`
（`derive_rng(tag)` = SHA-256 派生，无时间/环境随机源）：

    raw_k(x,y) = Poisson( s(x,y) + b_k(x,y) ) + N(0, RN²)        # 电子域，gain=1 e-/ADU
    b_k(x,y)   = 100 + off_k + pl_k·(u,v) + qu_k·(u²,uv,v²) + 正弦项   # 加性天光真值

- 覆盖图案：4 帧 x 向条带 `A:gx0-2  B:gx2-4  C:gx4-6  D:gx6-7`（8×8 control cell / 512² tile），
  覆盖子集在 cell 边界 x=128/192/256/320/384/448 突变 —— **接缝判据的作用面**；
- 负例（真值无效应 ⇒ 度量归零/判红）：
  · `c1` A5 全减背景臂产品中位 ≈ 0；`c4` 退化判据在注入接缝下不翻红；
  · `c2` `g_k≡1` 的 oracle 臂；`c6` 子集求值与全网格逐位一致。

## 3. testdata 真实数据（§12.2 第 3 类）

| 数据集 | 路径 | 本单元用途 |
|---|---|---|
| M42 M1 T3 Red（6 帧，300 s） | `testdata/M42_T2T3_mosaic_Flying_dutchman/T3/M1/*Red.fts` | C7：真实帧间背景失配量化（乘性斜率/加性偏移）+ 真实样本上跑生产天光面 + 接缝度量分布与裁图互证 |
| Galaxy Center T4 | `testdata/Galaxy_Center_T4/lights` | 备用（本版未纳入，见 README §7 诚实边界） |

C7 取 4 帧，中心裁剪 512×512（origin 3000,3000，背景为主区域），先按 FITS WCS
(`CRVAL/CDELT`) 预估位移，再用 FFT 相位相关（`phase_shift`）精对齐（实测位移
[0,0]/[0,0]/[8,6]/[10,8] px）。

## 4. 生产代码（只读链接，非数据）

`run/SCI-403/{upm_probe,sky_probe}` 由 `code/build_probes.sh` 编译，静态链接
`build/libastrocs_phase2.a`（+ hips/aio/common/probes/cfitsio），**不修改任何生产源码**。
入口：`p2_upm_build` / `p2_upm_calibrate_block` / `p2_upm_evaluate_c` /
`p2_upm_convergence` / `p2_upm_materialize_dense` / `p2_upm_dense_read_block` /
`p2_upm_save` / `p2_upm_ma_build` / `p2_upm_ma_solution` / `p2_sky_plane_build` /
`p2_sky_plane_eval[_delta][_block]` / `p2_sky_plane_residuals` / `p2_sky_plane_frame_delta`。

## 5. 复跑

    bash 实验/SCI-C/code/run_all.sh          # 编译探针 + 跑 c1..c7 + 出图

产物：`run/SCI-403/`（日志/二进制/中间 JSON，gitignore）与
`实验/SCI-C/results/*.json` + `results/figs/*.png`（入库）。
