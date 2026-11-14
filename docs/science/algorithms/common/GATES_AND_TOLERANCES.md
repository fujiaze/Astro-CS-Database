# 检测、PSF 与天测的诊断量及比较方法

本文补充检测、PSF 和天测的测量设计。科学定义分别由对应正本拥有[1–3]，Normalize 因果链与单位遵循阶段科学和数据合同[4,5]。文件名与检查 ID 用于定位已有检验，不表示发布许可，也不建立另一张跨主题阈值权威表。

## 量的定义

| 名称 | 计算及单位 | 限制 |
|---|---|---|
| detection peak ratio | 检测模型振幅 / 背景尺度，无量纲 | 指明模型、输入图和噪声估计器；不是积分通量 SNR |
| `SNR_det` | 兼容源目录中的 `(peak-background)/noise_sigma` | 原始峰值与拟合振幅不同；必须说明源目录的真实 producer |
| `q_psf` | 同一数组单位下的模型振幅 / PSF 残差尺度 | 拟合质量代理；残差的截尾绝对均值不是 MAD |
| `source_snr` | 实际源的通量估计 / 配对标准差 | 通量估计器、PSF、背景和协方差明确时才可解释 |
| reference SNR | `F_ref/sqrt(V_F)` | 固定参考及条件模型，独立于实际检出星样本选择 |

相邻差分满足 `Var(I_i-I_(i-1))=2 sigma²(1-rho_adj)`。差分尺度除 sqrt(2) 只在相应独立噪声假设下估计原像元尺度。平滑阈值若使用未平滑图的 sigma，不能称为平滑图上的同倍显著性；固定核 K 应用 `C_sm=K C_raw Kᵀ`[1]。

Moffat4 的残差尺度、FWHM、全 PSF 总通量及像元积分口径按 PSF 正本[2]；不得把高斯候选幅度当积分通量，或将求解窗口重新归一而保留总通量名称。canonical controls、峰值 proxy 和兼容帧级 SNR 分开记录。

## 测量域与检验对象

每个检验声明输入分母、筛选、真值、参数域、统计量、单位和容差来源。召回率用注入数为分母，另报未检出和错误匹配；虚警用独立纯噪声场的面积为分母。训练阈值的样本与留出样本分开。固定条件零失败不证明概率为一，区间估计和扫描选择效应见[1]。

定位检验应覆盖 PSF 宽度、峰值与积分 SNR、子像元相位、拥挤、饱和、坏点、边缘和模型失配。uint16 转换臂只测量量化影响，不能替代浮点生产路径的端到端坐标检验。

独立 PSF oracle 用不同优化器或参数化生成同一模型响应，并分别比较位置、FWHM、总通量和背景。无噪声解析场检验实现；带噪重复采样检验偏差、散度和覆盖率。直接将生产公式抄入 oracle 仅检验接线自洽。

## 检查 ID 的用途与数值域

下表保留已有检查名字及适用问题。数值是具名测试构型的比较目标或实现参数，不能据本表断言已运行、通过或适用于任意真实仪器。

| 定位标识 | 应检验的量与域 | 解释边界 |
|---|---|---|
| G-P1-CENTROID-SCI、G-P1-CENTROID-U16 | 已知中心合成场及对应量化臂；最大定位误差目标分别 0.3/0.5 pixel | 需固定模型、相位、噪声与选择，不能外推到所有 SNR≥20 的源 |
| G-P1-CENTROID-1、G-P1-CENTROID-BRANCH-ORDER | sdet、dpsf、fallback 与写读 adapter 的坐标比较 | 内部公共坐标为整数零基中心；局部 +0.5/-0.5 只由真实 adapter 定义，不改公共合同 |
| G-P1-PSF-ORACLE-F64、G-P1-PSF-POS-FWHM、G-P1-PSF-SCIPY-ORACLE | 合成模型与第三方优化器的位置、FWHM、幅度及背景 | 0.05 pixel、1% 等比较值要绑定场景；不能把中心采样模型的吻合当像元积分已正确 |
| G-P1-STAR-RECALL、G-P1-STAR-FP | 逐 PSF/相位/SNR 分档的召回及纯噪声虚警 | 不复用缺可取得标定输入的 99% 阈表；报告置信区间与阈下过渡带 |
| G-P1-STAR-DET | 排序、NaN 位置、星数截断和重复执行 | 固定序不自动保证所有浮点阶段跨 worker 逐位恒等 |
| G-P1-WCS-F1 | 拟合内点 RMS、匹配数和几何分布 | 不是独立天球残差；无标定的 0.5 arcsec 不作为通用准确度 |
| G-P1-WCS-F2、G-P1-WCS-RT | 已知 SIP 的外部 WCS 变换与独立密集位置往返 | 1e-4 pixel 是具名比较目标，往返小不证明绝对坐标正确 |
| G-P1-WCS-CRPIX | 参考像元、原点与裁剪几何 | 合法 CRPIX 可偏心；`((w+1)/2,(h+1)/2)` 只描述居中生成策略 |
| G-P1-WCS-BRIDGE、G-P1-WCS-BRIDGE-GLOBAL | 零/一基桥接、手性、尺度与第三方读取 | 1e-8/1e-6 pixel 只在对应数值域比较，不声称全部尺度的严格包络 |
| G-P1-WCS-RT-ITER、G-P1-WCS-RT-APBP | 迭代前向反演与 AP/BP 近似表示的不同误差 | 迭代停止值 1e-9 pixel、表示误差及浮点舍入分别测量，不能互相替代 |
| G-P1-WCS-CLOSURE、G-P1-WCS-CLOSURE-REPRO | 外部星表残差及同输入记录重算 | 匹配半径截断、样本上限与实际匹配率必须随中位数报告 |

坐标负例至少包含漏加/重复加 1、漏 SIP、CD 手性反射错误和错误历元。密集网格、边角与独立随机位置用于揭示拟合域之外的误差；相同前向/逆向内部自洽不能替代外部参考。

## 外部闭环工具与已知边界

仓内工具 [closure_metric.py](../../../../eng/tools/astrometry/closure_metric.py) [6] 不导入 ACSD 科学库，使用 Astropy WCS、外部 NPZ 星表及残差向量；它提供 `compute`、`check`、`fault-inject` 和 `selftest`。示例命令从仓库根执行，路径须由运行清单解析后填入：

```bash
python3 eng/tools/astrometry/closure_metric.py compute --product RUN_PRODUCT --catalog CATALOG.npz --flavor solved --out ANALYSIS_DIR
python3 eng/tools/astrometry/closure_metric.py check A.json B.json --median-tol-px 0
python3 eng/tools/astrometry/closure_metric.py fault-inject A.json --mode count-drift --out INJECTED.json
```

当前工具默认 `snr>20`、Gaia `G<18`、最多 20000 个源、1 arcsec 内匹配统计和 5 arcsec 扫描。它的检查器会核对自己声明的参数，不能据此将这些策略提升为所有数据域的科学常数。

静态阅读还表明：`load_sample` 只取 `frames[0]`；header WCS 来自清单指向的输入光帧；中心/邻近匹配有切平面近似和 RA 回绕风险。`selftest` 的合成工作区与真实产品清单需求也需一起核对。这些限制意味着必须逐帧绑定 run、产品几何和天区，不可拿目录位置或工具名替代输入校验；header 无 WCS 不表示 IPV 求解失败。

`check` 的记录一致性只能证明记录的部分字段和残差能互相解释。它不单独证明两份记录实际具有相同输入；当前不同 catalog hash 还可能仅给提示。必须另核对输入身份、样本、单位、所有残差和源匹配的实际对象。工具存在不表示本页已经运行自检或真实观测。

## 数值记录的归因

登记中的 NGC55、LDN43 和 Galaxy Center 例曾分别列出 solved 中位残差 0.5644、0.4202、0.1077 pixel，以及匹配率约 4.86%、3.52%、31.32%。这些数值没有在本页绑定可重跑的逐帧输入和输出，且登记指出母版单位存在限制；它们不作为当前阈值、当前精度或两跑零漂移的实证。读取证据时须回到对应实验单元保留完整过程，不能用一行 PASS 代替。

`gate2_psf_oracle.py` 的原址及输入当前未取得，不能借其四个阈值背书。已有独立源引导与盲检对照入口保留在[测光实验](../../../../实验/photometric-magnitude/code/step4_guided_vs_blind.py) [7]；真实无原始 WCS 的单帧 IPV 记录见[独立复核](../../../../实验/normalize-precision-p5/INDEPENDENT_REVIEW_950C261.md) [8]。这些证据各按其输入和估计量解释，不合并成全 Normalize 验证。

质心合成、两跑比较、FP80 与几何扫描的来源读数和适用限制保留在[证据记录](../../../../实验/normalize-precision-p5/DOCUMENTATION_EVIDENCE_NOTES.md)，完整来源文字可按记录中的 Git blob 恢复。

接缝评价另属背景科学正本[9]：在实际重叠边界测阶跃，同时检查真实低频、星点光度及未测边界。把背景全部扣平可使差值退化，不能据残差小证明天空或通带响应正确，也不把固定“2 倍参考”当无来源的通用标准。

## 参考文献

[1] [检测科学与统计口径](../../detection/STAR_DETECTION.md)。

[2] [PSF、积分响应与总通量](../../psf/PSF.md)。

[3] [天体测量与独立评价](../../detection/ASTROMETRY.md)。

[4] [Normalize 科学链](../../normalize/NORMALIZE_SCIENCE.md)。

[5] [Normalize 数据合同](../../../engineering/contracts/NORMALIZE_DATA.md)。

[6] [外部闭环测量工具](../../../../eng/tools/astrometry/closure_metric.py)。

[7] [引导与盲检对照脚本](../../../../实验/photometric-magnitude/code/step4_guided_vs_blind.py)。

[8] [阶段一独立复核](../../../../实验/normalize-precision-p5/INDEPENDENT_REVIEW_950C261.md)。

[9] [相对天空与低频保真](../../sky/UPM.md)。
