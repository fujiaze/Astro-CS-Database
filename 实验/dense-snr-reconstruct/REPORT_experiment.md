# P4 重建稠密 SNR · 实验报告（实验单元）

**单元目录**: 实验/dense-snr-reconstruct/
**科学链位置**: 第 4 点——由稀疏控制点重建稠密信噪比场及其定权消费（最高设计 §2）
**证据源**: 独立审计/实验重做/P4重建稠密SNR/ 路线1/2/3（互不通信的三路独立审计）的 code/results，原样收录本单元 code/ 与 results/；本单元不重跑实验，全部读数引用既有存档 JSON。
**复现**: bash code/run_all.sh（约 30 s CPU，产物与存档 JSON 逐字段同构）
**判读口径**: 分歧台账 D-xx/A-P4-xx 终裁。

---

## 1 假说（事前声明）与判定总表

| # | 假说 | 判定 | 关键读数 | 证据（实验:脚本 → results/） |
|---|---|---|---|---|
| H1 | w = SNR²/F_ref² = 1/σ_F² 逐位恒等（幂次 2 非自由参数，γ=2 = GLS 逆方差最优） | **成立（四路验证）** | 恒等偏差 5.55e-16 / 4.44e-16 / 3.7e-16 / 4.4e-16（2 ulp）；γ=2 方差最小，γ=0 差 26.3×，γ=1 效率 3.73，等权 104.58 | route1/exp_p4_01 → exp_p4_01_weight_optimality.json；route2/exp_P4R2_01 → exp01_weight_identity_gamma.json；route3/exp01 → exp01_weight_identity.json；P2 路线1 exp12（台账 A-P2-11 转写） |
| H2 | 生产默认算子 = natural_bicubic_spline_clip_v1，节点精确复现、收敛阶 −4、钳制必要、正齐次性可红 | **成立** | 节点残差 ≤3.6e-15（门 1e-9）；样条内部阶 −3.97/−4.18；去钳制 min=−161.5 / 出现负 σ；公理偏差 ≤2.8e-15、收缩反例 0.54 | route1/exp_p4_02 → exp_p4_02_interpolators.json；route3/exp03 → exp03_sparse_dense_reconstruction.json；route3/exp05 → exp05_operator_axioms.json |
| H3 | IDW 默认 idw_power=2.0 有标定支撑 | **推翻 → 按 D-05 终裁默认 1.0** | 含噪（2%/6%）最优 p≈0.5–1，p=2 差 2–3×；无噪 p=2 最优（1.639e-3）；误差面光滑无尖峰、p→∞ 退化为最近点 | route2/exp_P4R2_02 → exp02_idw_params.json；route3/exp04 → exp04_idw_parameters.json；route1/exp_p4_02 |
| H4 | 控制值必须携带源项亮度，否则下游效率损失 | **成立** | Oracle：含源项 E=2.22e-16（机器零）、丢源项 1.31e-3、等权 1.78e-2；极端对比 A_full 0.037 vs A_bglimit 0.433（12×）；零源负例两臂逐位恒等 | route1/exp_p4_04 → exp_p4_04_brightness_forward.json；route2/exp_P4R2_07 → exp07_luminance_chain_negative.json |
| H5 | 三口径（dense/sparse_reconstruct/frame_reconstruct）是同一物理量的三种还原粒度，不构成三种权 | **成立** | frame 平铺 dex-RMSE 0.0295 vs 样条 0.0058、动态范围保持 0.75 vs 0.99，权重公式不变；w 对 m_ref 不变逐位 | route1/exp_p4_04；route3/exp01（H1d：w 比 = 1.0 逐位） |
| H6 | λlo/λhi ≥ 1/16 是保守良态守卫（κ=√(λhi/λlo)=轴比） | **成立** | 放大 1.007/2.025/3.956/7.980 vs 理论 1/2/4/8（0.5% 内）；共线触发率 1.000；各向同性最小 0.330 永不误红 | route2/exp_P4R2_04 → exp04_plane_conditioning.json；route1/exp_p4_03 → exp_p4_03_plane_geometry.json |
| H7 | 分母方差面缓变：白性 lag-1=−1/2、relspread 散粒 1×/PRNU 2×、指纹斜率 1/2/0；分子纯度（无源⇒臂逐位相等） | **成立** | lag-1 −0.5007/−0.4995/−0.4984 与 −0.49963±0.00078；0.924 与 0.99999、1.911 与 1.9907；斜率 1.000/2.000/0.000；无源 SNR≡0 精确 | route2/exp_P4R2_05 → exp05_whiteness_variance_map.json；route3/exp06 → exp06_white_noise_and_purity.json |
| H8 | 判据 E = Var_w/Var_opt − 1 有证据资格（能红）且乘性免疫 | **成立** | 乘性 σ̂=3.17σ 时 E=−3.3e-16；平坦+平坦 E=0 精确；错误臂 0.314 判红；打乱 0.729 | route3/exp02 → exp02_metric_E_properties.json |
| H9 | 辅助常数链：1.4826 解析恒等、9216 预算自洽、Moffat4=1.230310 闭式 | **成立** | 1/Φ⁻¹(3/4)=1.4826022185056023（1.50e-16）；9216=(1.44/0.015)² 精确，n_min 带 8905–9494；闭式 1.2303077 vs 登记 1.230310（1.79e-6） | route2/exp_P4R2_08 → exp08_mad_sigma_budget.json；route2/exp_P4R2_09 → exp09_moffat4_factor.json |
| H10 | Δ=64 是可实验证伪的科学量 | **豁免（结构性）** | tile_width=512 冻结、512/8 密度口径自洽；科学后果由 Δ/ℓ 判据承载（零噪偏置 128/64=4.06 vs 理论 4，随 Δ² 增长） | route2/exp_P4R2_03 → exp03_delta_grid.json；台账 A-P4-01 |

负例纪律：19 个实验全部含"真值无效应 ⇒ 度量归零/判据失效"负例（等 σ 方案差归零、平坦场全算子归零、零源稠密场 max|SNR|=0、无源两臂逐位恒等、常数数据 MAD=0、高斯轮廓对照 1.6651 等），对应错误臂均判红——无恒真门。

## 2 方法

- **三腿模型**：文献腿（一手核验，见 refs.md）/ 实验腿（固定 seed、含负例）/ 理论腿（docs/derivations.md：定权恒等式与 Cauchy–Schwarz 最优性、条件数放大、收敛阶、IDW p→∞ 极限、Δ² 偏置律、白性 −1/2 恒等式）。
- **算子实现**：样条按标准分段基独立实现（路线3，节点复现 1.8e-15 + 收敛阶双重验证）；IDW 含 K 近邻截断与 γ=1e-10 重合点守卫、大 p 对数归一化防溢出；双线性为规则网格对照档。
- **合成数据**：HST 信号模板性质的 Moffat(β=2.5/4) 源面 + 完整物理前向（散粒/读噪/PRNU 项构成同 NOISE_MODEL §5b）、纯解析代数合成（平面 + 弱曲率、GRF）、负例（平坦场、零源、等 σ、常数数据）。
- **度量**：E = Var_w/Var_opt − 1（效率损失）、dex-RMSE、动态范围保持比、log-log 指纹斜率；E 必须与 dex 水平判据成对使用（H8）。

## 3 数据来源

| 路线 | 脚本（code/ 下） | 存档结果（results/ 下） | seed |
|---|---|---|---|
| route1 | exp_p4_01_weight_optimality.py … exp_p4_04_brightness_forward.py（4） | route1/*.json（4） | 20260926 |
| route2 | exp_P4R2_01…09（9） | route2/*.json（9） | 20260926–20261003（每实验一档） |
| route3 | exp01…06（6） | route3/*.json（6） | 20260926 |

三路互不通信、全仓库只读、纯 Python+numpy、单实验 CPU ≤5 min；seed 写死于脚本（route1 统一 SEED=20260926 + 偏移派生；route2 按实验日递增；route3 统一 20260926）。results/summary.json 为关键读数汇总（注明来源路线与字段）。

## 4 结果（判读按台账终裁）

1. **定权恒等式（H1）**：四路独立验证机器精度成立；γ=2 唯一确定（A-P4-02 三路同判，审查"γ=1 保守"不成立）。γ 扫描负例（同方差场景 gain≈1.0125）证明判据非退化。
2. **默认算子（H2）**：natural_bicubic_spline_clip_v1 为生产默认（**按台账 D-10 订正**旧稿"bilinear 是默认"），双线性对照/回退；节点复现 ≤3.6e-15 对 1e-9 门为浮点性质余量而非松容差；"默认档总是最优"不成立——Δ=256 被双线性/IDW 反超，跨 Δ 外推须重验（A-P4-07）。
3. **IDW 默认值（H3）**：**按台账 D-05 终裁 idw_power=1.0**（含噪最优带 0.5–1 上端），K=16 折中、γ<1e-10 数值守卫，配置化 + 运行日志输出实测 p*（不落盘产品）；p=2 降级为无噪/光滑极限最优读数。
4. **亮度携带（H4）**：丢源项 Oracle E=1.31e-3（机器零对照 2.22e-16），极端对比 12 倍效率损失；端到端链亮度跟随 √10 闭合（3.1970 vs 3.1623，+1.1% 来自 2% 控制噪声）；P5 定权增益 1.0428 vs 理论 1.0444。
5. **三口径（H5）**：同一物理量表示约定（台账 X3/A-P4 口径）；m_ref 降格为记录参考电平（A-P4-06）。
6. **几何与常数链（H6/H9）**：1/16 判据为带余量保守守卫；1.152 取值按 D-04 终裁（路线2 的"来源未定"闭环，登记义务归 P2）。
7. **诚实边界主数**：稀疏控制格不表示 PSF 尺度——偏差中位 3.8 dex（p99 5.7 dex）[实验:route1/exp_p4_04]；有效域为 ≥Δ 尺度平滑场。

## 5 结论

假说判定：H1/H2/H4–H9 成立，H3 推翻并按 D-05 换默认值，H10 豁免为结构常数（科学后果另由 Δ/ℓ 判据承载）。P4 单元的科学内容收敛为一句话：**稀疏控制点到稠密 SNR 场的重建以自然三次样条钳制算子为生产默认（节点复现 3.6e-15、可分辨域内最优），定权恒等式 w = SNR²/F_ref² = 1/σ_F² 机器精度成立且 γ=2 由定义唯一确定；控制值是否携带亮度决定 P5 权重是否最优（最劣 12 倍），IDW 以 idw_power=1.0 保持为配置化备选口径。**

## 6 诚实边界

1. 稀疏控制格不表示 PSF 尺度结构（3.8 dex 中位）：有效域边界，非精度缺陷；PSF 尺度归 P1 与结构感知估计器。
2. 跨 Δ 外推须重验（Δ=256 反超）；场曲率显著增大时 Δ 偏置上升须复评。
3. IDW p* 依赖场形态与噪声档，配置化后随日志积累重标定（台账 §4.2 开放项）。
4. E 判据乘性免疫，必须与 dex 水平判据成对使用。
5. γ=2 最优性以 SNR 估计无偏为前提；恒等式检验未测相关噪声（k_corr 面归 P2/P5，D-08 两因子查表由 P3 单元承载）。
6. 端到端链为缩小场景（8 源/512²、两帧）；Oracle 检验隔离信息内容。
7. 05 规格去钳制 E 1.007→1.44e4 数字未独立复刻，钳制必要性以机制佐证。
8. 标注级文献锚（Aitken/Moffat/de Boor）不承担任何数值判据。
9. 项目约定豁免：quality_factor 0.1/0.5（比值 0.1/0.5 与 share/absolute 口径差不豁免）、γ<1e-10、节点容差 1e-9、Δ=64。

## 7 复现命令

```bash
cd "实验/dense-snr-reconstruct"
bash code/run_all.sh                 # 全部 19 实验按 route1→route2→route3 串行，~30 s
# 单项示例（输出与 results/ 存档同构，固定 seed）：
python3 code/route1/exp_p4_01_weight_optimality.py
python3 code/route2/exp_P4R2_02_idw_params.py
python3 code/route3/exp03_sparse_dense_reconstruction.py
```

依赖：Python3 + numpy（stdlib json），无仓库内 import、无网络、无时间/环境随机源。脚本输出路径指向各 route 的 results/（本单元内同名目录）；若在审计原目录重跑则输出到原 results/，两者逐字段同构。
