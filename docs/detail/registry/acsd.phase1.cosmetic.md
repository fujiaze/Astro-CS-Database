# Normalize 缺陷修复

本节点消费校准面并产生 cleaned 面及修复来源，供科学源、测光和噪声模型使用。判据由 [CCD 缺陷分册](../../science/calibration/CCD_DEFECT.md)和 [Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)规定；修复产生的相关性不能凭一张对角方差面宣称完整。

## 入口与生产链

[scheduler adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_cosmetic` 对应 `acsd.phase1.cosmetic`，端口为 `calibrated → cleaned`。节点仅在 `cosmetic.enabled=false` 时整体关闭；缺该配置段不表示关闭。

实际路径读取校准工作面，解析显式 dark/bias 或校准节点的 `master_refs.json`，调用 `ac_correct_frame`，并按列配置调用 `ac_correct_columns_ex2`。母版不可得、不可读或尺寸不符的来源被记录。局部 `p1_session.cpp` 的有限修复接线不能代表此生产节点。

## 数组与公开 API

[astro_calibration.h](../../../lib/algorithms/calibration/include/astro_calibration.h) 声明接口，[cosmetic_corrector.cpp](../../../lib/algorithms/calibration/src/cosmetic_corrector.cpp) 实现检测与修复。

| 接口/字段 | 含义 |
|---|---|
| `ac_correct_frame` / `_f64` | 输入图像、dark/bias、hot/cold sigma、插值方法与结构大小；输出修复面和热/冷计数 |
| `ac_correct_columns`、`ac_detect_bad_columns_from_master` | 科学面或单母版的列检测/修复基础能力 |
| `ac_correct_columns_ex`、`ac_correct_columns_ex2` | 科学面/dark/bias 并集及来源/置信度；ex2 分别承载母版阈值与宽段登记范围 |
| col_mask | 每列 0=未检出、1=已修、2=仅标记；不能按布尔值解释已修数量 |
| source/conf mask | 科学面、dark、bias 来源位及其置信度，和修复像素分账 |
| `ac_column_variance_inflate` | 对外部给定方差按修复权重平方和及调用方 kappa 更新；不是独立噪声估计器 |

图像为行主序，内部中心是零基索引；信号保留校准单位，方差用其平方。非坏像元保持原值，修复取输入锚点而不级联读取已修值。`neighbor_k` 在当前列接口中没有行为读点；段长上限由 `max_seg_len` 承担。`_f64` 修复入口的内部转换须依实现解释，不能按名字声明全程 FP64。

## 所有权、并行与错误

调用方提供输入/输出与列掩膜缓冲；库不写盘。adapter 保存 cleaned 工作面、列掩膜与逐帧报告，按帧分片后定序汇总；线程来自 Runtime 授权预算。底层像素循环与帧级并行需要共同服从预算，取消由上层协作传播。

`AC_COLSTAT_*` 区分零尺度、单侧锚、无锚、宽段和边界排除；无锚保留输入值并置状态，不能写成成功补全。`AC_ERR_PARAM` 等返回交给 adapter 上行。单帧 cosmic-ray、trail 的完整分类不属于这些坏点/坏列函数的既有能力。

## 配置与验证入口

有效配置包含 hot/cold sigma、method、max_structure_size、`bad_column_*`，实际默认值由配置与 adapter 解析给出。[Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)要求修复状态/算子与噪声模型配对；具体生产差异只见[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

验证从本页公开函数及[阶段一管线测试](../../../eng/tests/pipeline/test_stage_normalize.py)进入：注入点/列缺陷，检查三路来源、0/1/2 掩膜、宽段不修、边缘单侧、无锚、负信号和修复相关性。本页未运行这些检验。
