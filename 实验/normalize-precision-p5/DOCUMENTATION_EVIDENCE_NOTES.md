# 算法文档中诊断记录的保留与复核

正式算法说明不把缺少载体的读数当成通用阈值。本记录保留整理时涉及的数值、来源和复核边界；未重新运行这些构型，不将文档中的“实测”字样转成当前科学事实。实验脚本、日志和失败过程未因本次文档整理删除或覆盖。

## 来源身份与全文恢复

来源提交为 efe280b45cab4fe50b8a53a3d1b9f9e5c68a312a。以下 blob 保留相应文档全文，包括运行描述、局限和具体判据；这是记录来源，不是当前规范。

| 文件 | Git blob |
|---|---|
| docs/science/algorithms/common/GAIA_QUERY.md | b253f16aba8e2b6e0f752a4290f8b9c5bbba60e7 |
| docs/science/algorithms/common/GATES_AND_TOLERANCES.md | 2502a62e424153282758bbda8c85825276ff76d7 |
| docs/science/algorithms/common/NOISE_ESTIMATION.md | 9c968f161f4a186abc0acad68b852b9fbac543a7 |
| docs/science/algorithms/normalize/CALIBRATION_ALGORITHMS.md | 6ef472a268e12c60653592b9eb852be810f1b4d4 |

在包含仓库历史的 checkout 可恢复全文，例如：

```bash
git show efe280b45:docs/science/algorithms/common/GAIA_QUERY.md
git show efe280b45:docs/science/algorithms/common/GATES_AND_TOLERANCES.md
git show efe280b45:docs/science/algorithms/common/NOISE_ESTIMATION.md
git show efe280b45:docs/science/algorithms/normalize/CALIBRATION_ALGORITHMS.md
```

这些命令恢复来源文字，不复现观测实验。真正复测仍需找到每项输入、程序、环境和输出。

## 查询记录

GAIA_QUERY 的 V18R3 记录声明18次赤道查询、683297颗星的差分一致，及80270例极冠属性比较。有限差分覆盖不能证明赤道1.2经验裕量在全域零漏星；极冠连续几何界也不能代替分段代码与实际输入核验。声明的 `eng/tests/unit/gaia_cat_test.c` 当前未取得，不能据名字补造执行状态。量化步长、独立解包与全枚举方法仍保留在算法说明中。

## 检测与天测记录

- R-3 的同一 Moffat4 构型记录：峰值比20时检出0星，50和300时均为36/40；相关质心比较记录 max=0.0535 pixel。它说明峰值代理、平滑阈值与积分通量 SNR 不可混同，不构成任意源的召回保证。
- Gaussian 构型的峰值比20记录 p95=0.109、max=0.137 pixel；uint16量化臂记录 median=0.0018、p95=0.0036、max=0.0056 pixel。记录只支持其采样、噪声和转换条件。
- G-P1-WCS-CLOSURE-REPRO 声明 T4 的 det2/n1 与 det3/n1 两跑记录一致，median=0.107658 pixel、n_matched=6265。没有重新取得原命令和产品时，不将这一摘要当成当前构建的重复性结果。
- G-P1-WCS-BRIDGE 保留 FP80 对照与880个几何构型的描述；ASTROMETRY 中的误差系数、尺度域和 sec²角度条件需一起复核，不能把采样最大值作为全域严格浮点包络。
- NGC55、LDN43、Galaxy Center 的 solved 中位残差记录为0.5644、0.4202、0.1077 pixel，匹配率约4.86%、3.52%、31.32%；样本在1 arcsec内截断，且母版单位存在限制。内部拟合残差、筛选后外部残差和总体天测准确度是不同对象。

## 噪声方差记录

SCI-VAR-ADAPT-01 声明无权平面拟合出现30.42%非正预测像元，并有13.22%信号损失读数；其来源文字指向 coverage/upm 的实现自陈。尚未重绑输入、掩膜及输出，不能把两数当成无权最小二乘的普遍后果，也不能据此证明任何加权非负模型无偏。

SNR-004 的5%背景尺度比较目标、SNR-006 的10%梯度模型目标和 SNR-005 的 gain 诊断属于不同构型。生产背景模型、诊断 `mu/g+(RN/g)^2` 与零消费的 `use_gain_model` 字段应分别核对；默认值或 oracle 名字不证明已经运行，更不构成任意污染/相关噪声下的误差界。

## 校准单位记录

UNIT-001 与 DISP-CAL-013 的来源文字记录 XISF bias中位0.015288、声明换算后1001.87 ADU；flat中位0.206381，未归一除法因子约4.845。T2 600/1200秒暗场外推截距999.96 ADU；T4 180/300/600秒拟合斜率0.5700 ADU/s、截距955.05 ADU，所列bias为916.16 ADU。两曝光点没有检验直线失配的剩余自由度，截距接近bias也不单独证明完整暗场模型。

来源报告的真实帧标度后果如下，未在此次重新运行：

| 构型 | 未声明标度的记录 | 声明后的记录 | 报告倍率 |
|---|---|---|---|
| T2 NGC1727，600秒Red | median=7048.618 ADU | median=436.155 ADU | 16.161 |
| T4 Galaxy Center panel1，180秒Red | median=3650.390 | median=361.236 ADU | 10.105 |

T2 来源还记录声明后 mean=500.7639、min=−23215.701、max=74318.594 ADU，与其独立 NumPy oracle 的相对差1.26×10⁻⁸；未声明时 median=7048.6179、mean=7372.6422。算术 oracle 使用相同错误单位也能自洽，因此这些误差比较必须连同独立 reader、物理单位声明和母版身份解释。DISP-CAL-012 保留 bias 是否参与、K在两种暗场约定下的实帧及逐位比较描述；不把代数相消推广成不同运算序的逐位保证。

## 后续使用

能取得实验单元载体的记录直接引用实际命令和输出；只剩文档声明的记录标为待复核。正式模型分别由[校准](../../docs/science/calibration/CALIBRATION.md)、[检测](../../docs/science/detection/STAR_DETECTION.md)、[天测](../../docs/science/detection/ASTROMETRY.md)和[噪声](../../docs/science/noise_snr/NOISE_SNR.md)拥有，来源记录不得反向覆盖正本。

## 文档修订的校验范围

本轮先以 efe280b45 为审查起点，随后获取远端新增18个提交，并将文档分支快进至 f41cc1c2b23f3722b789e8f33a74bf87e5f435f1，再按该源码订正实现描述。远端的逐帧响应 override、Householder QR、像元积分 PSF 与因子 v2 均保留；尚存的边界写入[实现差异地图](../../docs/engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。不把起点处的静态判断套用到更新后的代码。

只读结构检查使用 Python、PyYAML 与 Git 文件集：191个索引路径与191个跟踪 docs 文件完全对应，无重复 YAML 键、重复路径或缺失上游路径；92份已修改 docs 加3份新增实验单元 Markdown 的本地 Markdown 链接均可解析，代码/块公式围栏闭合，无冲突标记。`git diff HEAD --check` 通过，PSF 正本中保留的复算代码与基线逐字相同。该检查验证文件结构与改动范围，不验证链接目标内的科学结论或公式渲染。

新增三份文件仅为本记录、[集中推导](NUMERICAL_PRECISION_DERIVATION.md)与[单元入口](README.md)。本轮没有修改产品代码、机器 schema、配置、测试或既有实验过程；没有运行产品构建、端到端任务或新增数值实验。集中推导是数学反例与条件界，由前台独立复核后写回正式正本，不能当作已测出的生产误差。
