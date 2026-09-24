# B7 要点：逐像素绝对 SNR 重建（SCI-RECON-SNR-01）

> 完整报告：`run/SCI-RECON-SNR-01/REPORT.md`（run/ 不入库）
> 结果：`results/b7_absolute_snr_recon.json`（seed 20260921）、
> `results/b7_absolute_snr_recon_seed20260922.json`（换 seed 复跑）
> 代码：`code/b7_recon_driver.cpp`（生产链路驱动）、`code/b7_build_driver.sh`、
> `code/b7_absolute_snr_recon.py`（夹具/四臂/判据）、`code/b7_run.sh`（一键复现）

## 模型（负责人假设的显式形式）

    I(x,y) = S_src(x,y) + S_sky(x,y) + N_local(x,y)                        [ADU]
    sigma_slow^2(x,y) = Var[N_local] + S_sky(x,y)/g                        [ADU^2]  缓变
    sigma_w^2(x,y)    = sigma_slow^2(x,y) + S_src(x,y)/g                   [ADU^2]  = NOISE_MODEL §5c:208
    SNR(x,y)          = S_src(x,y) / sigma_w(x,y)                          [1]      分子只有源

`g` [e-/ADU]，`ADU = N_e/g`（§5c:198 冻结约定；文献倒数形式禁混用）。
缓变面承载的是**方差**不是噪声实现（§5b:140 三谓词纪律）；实测噪声实现一阶差分 lag-1 自相关
−0.4985（白噪声解析值 −1/2），方差图相对散布 2.0%。

## 分离方案（单帧不可分解，只能靠支撑先验）

- **P1 缓变面可表示** → 生产 `p2_sky_plane_build/eval_block`（8×8=81 节点）；
- **P2 源稀疏** → 生产 GLS `w_info_solve`（FZ-FORMULA-WINFO/Q）+ 生产 Moffat4 轮廓；
- **P3 排异层次** → `p2_star_mask_caps/contains` + `p2_sky_patch_estimate` + `p2_reject_plan_resolve/reject_stack`。

`S_src_hat = Σ F_hat_i·P_i`（**不是**残差相减，§5c:214 正向约束）。

## 四臂结果（解析臂 `D_core`，n=1463；两 seed 同向）

- **T_full**（推导出的模型）：中位 |SNR/SNR_true − 1| = **0.0054**、p95 = **0.0296** ⇒ 复原真值；
  方差面比值中位 1.0031。
- **T_slow**（漏源项）：中位比值 **1.657**，解析预言 1.645（对拍差 0.70%）⇒ 偏高。
- **T_naive_sky**（天光双计）：中位比值 **0.874**，解析预言 0.871（对拍差 0.31%）⇒ 偏低。
- **T_traditional**（字面「天光进分子」）：**1.314** ⇒ 偏高（全域 1.4e5 倍，与传统口径失真同源）。
- **T_null**（真值无源）：源项 `median(|S_src_hat|/σ_slow) = 0`、`T_full ≡ T_slow`（相对差 0.0）；
  但 **T_naive_sky 不收敛**（比值 1.36802，与预言相对差 1.1e-16）。
  有源帧上同一判据 37.31 ≫ 0.05 ⇒ **归零判据非退化**。

## 边界与诚实声明

- 盲检测消融臂：检测完备性 0.43，端到端 `D_core` 中位相对误差 **0.376**（判红）——
  差距全在**分子侧**（检测 + PSF 尺度），不在方差模型。
- B 臂（真实 HST M16 星云结构）：源项只捕获 **14.2%** ⇒ 「源是稀疏的」先验在延展发射上失效。
- M42 臂：稳健二阶差分 σ 与生产方差面比值 **0.986**（一致），均值版被星云结构污染 8.64 倍；
  逐源 SNR 与通量 Spearman ρ = 0.9969；源无关区源项中位 0；**增益不可自估**（lever_var = 0.024 < 0.15）。
- 接口事实：生产噪声模型控制点在 `(i+0.5)Δ`（noise_model.cpp:524），与
  `cell_center_v1`（schema:329）差 **0.5 px**，生产重建算子正确 fail-closed
  （weight_chain.cpp:481-484）；本实验显式声明 `grid_origin = 0.5` 使两者重合，不改生产代码。

## 我证伪的任务书前提（详见报告 §12）

1. **「T_null 三臂必须收敛」错**：T_full 与 T_slow 恒等，T_naive_sky 按预言发散 1.368 倍。
   照原话写归零判据，它在两臂上恒真、一臂上恒假，**不携带信息**；必须逐臂写。
2. **「本地噪声与天光散粒在空间上缓变」按字面是假的**：只有**方差图**缓变（散布 2.0%），
   噪声实现是白的（lag-1 自相关 −0.4985 ≈ −1/2）。
3. **「T_naive_sky = 把天光加进分子」自相矛盾**：按其公式（分子纯源）实测偏低 0.874，
   按字面（分子含天光）实测偏高 1.314 —— 两个方向相反的变体被当成一个。
