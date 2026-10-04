# CCD 线性缺陷

> 上游：《ACSD 最高设计》（docs/ACSD_DESIGN.md）的「normalize：单帧标准化」节点流程与「验证体系」两章

## 1 主题与目标

CCD/CMOS 图像上的"线性"像素缺陷与线性外源事件是一组**物理成因不同、形态相似、判据不同**的现象：坏列、CTE 电荷拖尾、宇宙线、卫星线、电荷溢出。混为一谈会导致两类错误——把固定的仪器缺陷当成随机的外源事件做跨帧排异（排不掉，每帧重复加权），或者把随机的外源事件当成固定缺陷写进掩膜（掩盖真实天体）。

本分册给出四件事：

- 这几类现象的**一手定义与区分判据**；
- 坏列的**统计检测口径**及其适用域；
- 缺陷插值修复的**方差处置**——这是本主题最容易出错的一步；
- 每条结论的核验状态，以及**在已核对来源中查不到什么**。

与同目录《校准》分册的分工：偏置/暗流/平场的标定口径在校准分册；本册只管像素域缺陷的分类、检测、修复与不确定度。

## 2 物理模型

四类现象的物理模型彼此独立，这是全部判据的来源：

| 现象 | 成因 | 跨帧复现 | 是否依附于真实电荷源 | 方向性 |
|---|---|---|---|---|
| 坏点 / 热像素 | 晶格损伤导致暗电流增强 | 固定复现 | 否（单像元或小簇） | 无 |
| 坏列 | 读出链的偏置结构 | 固定复现 | 否 | 沿列 |
| CTE 拖尾 | 读出转移时电荷被陷阱延迟 | 固定复现 | **是**（星、热像素、宇宙线） | 沿读出方向的反向 |
| 宇宙线 / 卫星线 | 外源粒子与航天器过境 | **不复现** | 否 | 贯穿视场，方向随事件变 |

外源事件的单帧识别有专门的形态学方法：基于拉普拉斯算子边缘检测与细结构比的判据可把宇宙线与欠采样点源分开 [19]。

**坏列在权威数据质量体系里被归类为偏置结构**，而不是"亮"或"暗"的像元缺陷：ACS 数据手册的数据质量位表中，位值 128 的定义是 `Bias structure (e.g., bad columns)` [1]；同一位表中位值 16 是热像素（暗电流大于 0.14 e⁻/秒）、位值 64 是温像素（0.06–0.14 e⁻/秒）[1]，而热/温像素的绝对暗电流阈值在正文里是"高于 0.14 e⁻/像元/秒判为热像素，低于热像素范围而高于 0.06 e⁻/像元/秒判为温像素"[1]。

**CTE 拖尾依附于真实电荷源**。数据手册的表述是：电荷从下游像元取走、沉积到上游像元，视觉效果是从源出发、背离读出放大器方向的电荷尾 [2]；ACS 数据手册给出拖尾可延伸 50 像素以上、出现在热像素、宇宙线与亮星的上游 [1]。辐射致晶格陷阱的成因、陷阱密度的时间累积率与逐像元改正模型见 [14, 16]；延迟电荷与电荷转移效率的术语定义见 [15]。

巡天级管线据此把插值坏列归为**临时缺陷**、与饱和星与电荷溢出这类持续缺陷分开跟踪 [18]；单帧处理流程同样把方差面与掩膜面与科学帧同构生成 [13]。

**外源事件跨帧不复现**。HST 档案的统计表明，单帧被卫星线穿越的比例为 `2.7 ± 0.2%`（典型曝光时长 11 分钟），且卫星线多贯穿整个视场、绝大多数呈直线 [3]。ACS 数据手册另给出 WFC 视场上可探测卫星线的速率与约 10% 全帧图像受污染的比例 [1]。

这张表就是排异与掩膜设计的判据面：**固定复现 + 不依附源 ⇒ 写进缺陷表；固定复现 + 依附源 ⇒ 写进拖尾模型；不复现 ⇒ 交给跨帧排异**。

## 3 公式与推导

### 3.1 坏列的统计检测

坏列不能用连通域尺寸判：它在尺寸分布的**两端都不成立**。

- 大尺寸端：注入一条整列缺陷后，该列所在的连通域尺寸落在帧高的量级上，与同帧天体源（大尺度星云结构）的连通域尺寸处于同一条重尾里，尺寸不可区分；
- 小尺寸端：真实的坏列在**整帧阈值**下根本不是一个连续大连通域。

**仓内复算**（`testdata/T2 calibration files/masterDark_BIN-1_4096x4096_EXPOSURE-600.00s.xisf`，XISF 解码后按声明因子换算到 ADU；逐像元阈值取 `median + 5·σ`，`σ = 1.482602218505602·MAD`，全帧 MAD 稳健尺度为 1.779 ADU）：

| 判据 | 结果 |
|---|---|
| 列统计量的跨列跳变（`column_sigma = 5.0`，即默认配置 `cosmetic.bad_column_sigma`） | 97 处跳变；反号就近配对成 53 段，段长 2–5 |
| 段长上界 `≤ 2k−1`（`k = cosmetic.bad_column_neighbor_k = 3`）约束后保留 | 111 列 |
| 沿这 111 列的**最长连续超阈段** | **中位数 2 像元**，p90 为 927，最长 4096 |
| 同一母版整帧 5σ 超阈像元的连通域 | 260755 个，最大 76757，p99.9 为 8 像元 |

读法：典型的被判出的列在中位情形下**碎成 2 像元的片段**，同时少数列确实整列超阈（最长段等于帧高 4096）。因此连通域尺寸既抓不到典型的坏列，也不能把抓到的碎片与暗弱源的碎片区分开——碎片尺寸落在点缺陷与弱源的共同分布里。**尺寸面无效，判据必须建立在列统计量上。**

**复现状态**：解码环节有可用的最小 XISF 读取器（`run/NOISE-TAXONOMY-01/code/xisf.py`，只读、未压缩 attachment、Float32/UInt16 单通道二维图像）；列判据自身的七步——5σ 逐像元阈值、列统计量、跨列一阶差分、反号就近配对、段长上界过滤、连通域标注与分位数统计——**目前没有复跑脚本**，因此上表的读数是历史读数，不是可复跑判据。把它们升为判据的前提是按本节公式补一个固定 seed 的复跑脚本与结果文件；在此之前凡引用这些数的地方都必须标明"历史读数"。

本链的列判据是：

```text
cs[x]  = median_y data[y, x]                    列统计量
d[x]   = cs[x] − cs[x−1]                        跨列一阶差分
σ_d    = 1.482602218505602 · MAD(d)
med_d  = median(d)
J      = { x : |d[x] − med_d| ≥ column_sigma · σ_d }
```

反号跳变就近配对成段，段长不超过 `2k−1`（`k` 是每侧参与统计的列数，默认 3）且不满帧宽才判坏。阈值分两路：科学帧路径用 `cosmetic.bad_column_sigma`（默认 5.0 个稳健尺度），母版路径用 `cosmetic.bad_column_sigma_master`（默认 10.5）——两路的列统计量尺度不同（母版经多帧合并，标度与噪声结构都变），因此阈值不同源。母版路径另有一道声明上界 `cosmetic.bad_column_master_wide_frac_max`（默认 0.001 帧宽）：超过即整类丢弃并记账，因为该路径的仅标记列数在阈值微调下会在 0 与一个四位数之间跳变，这个量本身不稳定。

检测源并列三路：母版暗场、母版偏置、科学帧自身。各路在帧内自校准，所以源面标度差不影响判坏集合。

**两条独立依据支撑这个口径**：主流实现都不把"列中值偏离 k·σ"当作公开的自动判据——IRAF 的坏像元掩膜按像元幅度判定并由用户提供掩膜文件，Siril 的化妆品校正按幅度逐条记录点缺陷与坏列，LSST 的 `afw` 用带包围盒的显式缺陷表，astropy 的 ccdproc 提供参数化的掩膜流程 [4]。手工核对 ACS 与 WFC3 仪器手册可读到的条款，坏列给的是**清单与位定义**而不是 k·σ 数值，热像素给的是**绝对暗电流阈值** [1]。因此本链的 `column_sigma` 是**项目定义的判据**，其值由在真实母版上的实测标定曲线确定，而不是抄来的行业常数——这也是它必须在溯源中标明标定来源的原因。

### 3.2 插值修复的方差处置

这是本主题的核心风险点：**把插值像素按邻元方差记账，会系统性地低估该处方差。**

一手证据有三层，方向一致。

**第一层：形式噪声随"到掩膜边缘的距离"衰减。** 掩膜填补方法的作者给出填补像元的形式逐像元噪声为

```text
σ_mask = σ_org · (2d + 1)^(−0.5)
```

其中 `d` 是该填补像元到掩膜边缘的距离、`σ_org` 是掩膜外的逐像元噪声 [5]。距离越大，形式噪声越小——按邻元水平给填补像元记账，恰恰落在该式给出的下限方向上。作者同时自陈"算法虽定义明确，但很难给填补值指定不确定度"。

**第二层：把方差显式提高并注入涨落的做法。** DES 单帧/叠加管线的做法是：把点扩散函数（PSF, Point Spread Function）核置于待填补像元，计算核内未掩膜像元的占比，超过阈值才插值；插值值按 PSF 加权求和，并**用高斯误差传播算出该像元的逆方差权重**；随后**以该方差抽一个高斯随机数作为填补值**，使填补像元的逐像元起伏与周围未污染像元相当 [6]。同一工作还明确指出坏列由上游在观测圆顶平场与偏置的离群像元上标定，并在掩膜管线之前标好，被标像元的权重置零 [6]。插值可行性（未成像伪影可插值、成像伪影必须掩膜）的讨论见该文第 4.1 节，该节举的成像伪影例证是卫星线与散射光。

**第三层：官方文字。** SDSS 的成像处理标志文档对坏像元的分级给出直接措辞：单条坏列的插值"测光应当基本完美"；`INTERP_CENTER` 表示被插值像元落在天体中心 3 像元以内；`PSF_FLUX_INTERP` 表示该波段超过 20% 的 PSF 通量来自插值；最严重的一档 `BAD_COUNTS_ERROR` 表示坏像元插值占比过大到"你不该相信 PSF 通量误差；它很可能被低估"（"it is probably underestimated"）[7]。同样的分级在巡天数据处理中被独立采用：只有中心被插值的天体才被判为测量不可靠 [9, 13]。SDSS 的成像注意事项另有一句更直接的因果表述：某些芯片存在坏 CCD 列，被测光管线插值后**产生明显的相关噪声** [8]。

因此可以给出一条三层因果链，每一层都有原文：坏列 → 被插值 → ① 插值像元与邻列产生相关噪声 [8]；② 插值占比过大时 PSF 通量误差被低估、不可信 [7]；③ 中心被插值的天体测量不可靠 [7, 9]。

**本链的处置**：

```text
var_repaired = ( Σ_j w_j² · var_j ) · κ
```

对单列两邻算术平均，`Σ_j w_j² = 1/2`。这一项是**纯误差传播**的结果：设 `ŷ = Σ_j w_j x_j`、`Σ_j w_j = 1`、各 `x_j` 独立同方差 `var`，则

```text
Var(ŷ) = Σ_j w_j² · var + Σ_{j≠k} w_j w_k · Cov(x_j, x_k) = ( Σ_j w_j² ) · var
```

交叉项因独立性为零，故 `κ = 1` 才是插值估计量本身的正确方差（两邻算术平均的方差确实只有 `var/2`，比一次测量更小——这是教科书结论，不是例外）。

因此本链的 `κ = 2` **不是信息论下界**，而是一条**非重复计数的记账约定**：修复值是同帧两个**已被独立计入**的邻居的确定性函数，若按 `κ = 1` 给它记 `1/(Σw²·var)` 的逆方差，逆方差合成时等于把同一批光子数第三遍。所以本链规定修复像元的记账方差**不得优于一次独立测量**，即

```text
var_repaired ≥ var_j      ⇔      κ ≥ 1 / Σ_j w_j²
```

对单列两邻 `Σw² = 1/2` 得 `κ ≥ 2`，取最小可行值 `cosmetic.bad_column_variance_kappa = 2.0`。贴边单侧复制的形态 `Σw² = 1`，此时 `1/Σw² = 1`，同一公式仍成立但给出的下界是 `κ = 1`；两种形态必须分开记账，`κ` 只施加在被修复像元上。

必须区分两个语义域：`κ` 管的是**该像元在逆方差加权与稠密信噪比重建中能携带多少独立信息**；插值值作为该位置真值的估计有多准是另一件事，实测 `Var(pred − truth) = 0.5502·σ²`，确实小于 `σ²`。`κ` 偏小则修复像元的信噪比被系统性高估，`κ` 偏大只是保守。

**不做该补偿的后果是可量化的**：修复像元的信噪比会被系统性高估约 `√2` 倍（方差小一半）。这在稠密信噪比重建里尤其危险——重建算子会把"插值过的像元"与"干净像元"同等对待，从而让修复区的信噪比高于真实水平。下游可依据列状修复掩膜自行决定是否施加补偿，掩膜本身是落盘产物。

把坏像元在叠加前赋予零权重、而不是给它一个估计方差，是巡天级流程的既有口径 [17]。

**三类外源事件的插值可行性不同**：未成像的伪影（宇宙线、坏列）可以插值，代价只是噪声的小幅增加，前提是其宽度远小于 PSF 的 FWHM [6]；成像的伪影（卫星线、散射光）尺寸与 PSF 同量级或更大，插值等于对底层光分布施加强先验，正确处置是掩膜、权重置零、排除出叠加 [6]。本链的排异环节因此在插值之前执行：先排异、后加权。

### 3.3 插值的无偏性条件

一维五点插值在"坏列噪声无穷大"的极限下，权重取 `{−0.274, 0.774, 0.000, 0.774, −0.274}`，坏列自身权重为零；只要权重和为一，插值估计量就是无偏的 [9]。这条性质与"该像元的噪声信息不可用"是两条独立论断，前者给出无偏性，后者要求把该像元排除出加权。二者必须同时成立。

## 4 参数与常数

| 量 | 取值 | 单位 | 来源 |
|---|---|---|---|
| 列统计量稳健尺度系数 | 1.482602218505602 | 1 | 解析可复算，见《校准》分册 |
| 科学帧列跳变阈值 | 5.0 | 稳健尺度倍数 | 默认配置 `cosmetic.bad_column_sigma` |
| 母版列跳变阈值 | 10.5 | 稳健尺度倍数 | 默认配置 `cosmetic.bad_column_sigma_master`，实测标定 |
| 每侧统计列数 `k` | 3 | 列数 | 默认配置 `cosmetic.bad_column_neighbor_k` |
| 段长上界 | `2k−1 = 5` | 列数 | 由上式导出 |
| 母版仅标记列上界 | 0.001 | 帧宽 | 默认配置 `cosmetic.bad_column_master_wide_frac_max` |
| 修复方差补偿 `κ` | 2.0 | 1 | 默认配置 `cosmetic.bad_column_variance_kappa`，取非重复计数记账约定下的最小可行值 |
| 热像素绝对暗电流阈（ACS） | 0.14 | e⁻/像元/秒 | ACS 数据手册 [1] |
| 温像素绝对暗电流阈（ACS） | 0.06 | e⁻/像元/秒 | ACS 数据手册 [1] |
| 形式噪声衰减式 | `σ_org·(2d+1)^(−0.5)` | — | 掩膜填补方法 [5] |

## 5 判据与误差

### 5.1 正确性判据

- **缺陷分类判据**：给定一组注入的、已知成因与位置的缺陷（整列缺陷、点缺陷、拖尾、注入的直线伪影），判据必须把每一类分到正确的桶，且不产生跨桶误判；
- **坏列检测的可复现性**：同一母版上改变像元遍历顺序（按行、按列、按块），判出的列集合必须逐列相同；
- **修复的守恒性**：单列两邻算术平均修复时，被修复像元的取值必须等于两邻的算术平均；空邻域回退原值而不是虚构好值；
- **方差判据**：被修复像元的方差必须满足 `var ≥ κ·Σw²·var_j`，`κ = 2` 来自非重复计数的记账约定（不是信息论下界）；注入"忘记乘 κ"的变异必须使门判红；
- **SNR 后果判据**：在存在坏列的帧上，比较"施加 κ"与"不施加 κ"两种口径下修复区的信噪比，比值必须等于 `√κ`（本链取 `√2`）；
- **零残差判据**：在没有缺陷的合成场上，缺陷路径必须逐位恒等（该量必须为 0，而非常小的非零）；
- **非退化要求**：每条门都必须有能使其变红的注入方式，且合成场中必须同时存在真缺陷与干净区域。

### 5.2 失效域

| 条件 | 显式后果 |
|---|---|
| 坏列与宇宙线在掩膜上叠加成复杂形状 | 修复算法的行为在该形状上不可预测，须由掩膜形状分类显式声明 |
| 图像被重采样或相关噪声污染 | 逐像元幅度判据的噪声假设不成立，须降级 |
| 源密度高、弱源与点缺陷尺寸重叠 | 点缺陷的幅度判据必须在母版差分上做，不能在科学帧上做 |
| 列统计量在重尾分布上出现就近反号配对事故 | 仅标记列数不稳定，须以声明上界拦截并整类记账 |
| 卫星线方向恰与拖尾方向一致 | 形态判据失效，必须叠加"跨帧是否复现"判据 |

### 5.3 证据强度分级

关于"插值像元的方差不可信"这一命题，三类一手证据的强度不同，引用时必须保留分级：

- **显式解析式**（形式噪声随距离衰减）与**显式方差赋值算法**（误差传播 + 按该方差抽样）是论文级证据 [5, 6]；
- **"误差被低估"这四个字**只出现在 SDSS 的标志文档里 [7]；
- **Rubin 侧只有代码级证据**：其仪器签名移除模块用 `badPixels = (mask & BAD) > 0 & (mask & INTRP) == 0` 把插值像元排除出坏区统计；而方差面在插值之前由整幅的像元值一次算出，插值像元的方差另由邻居方差的多项式插值填充，并不按插值后的像元值重算。这解释了为什么显式解析式在文献里找不到——主流管线把可信度判断交给下游标志位，而不是给插值像元单独赋一个解析方差。**代码行为不等于官方论断**，转述时不得升级。

### 5.4 在已核对来源中查不到什么

下面三条是**有边界的否定结果**，边界即所列来源族；超出该边界的结论不成立。

| 未找到的结论 | 已核对的来源族 |
|---|---|
| "坏列由列中值偏离 k·σ 判定，且 k 有行业公认取值"的定量判据 | ACS 数据手册（坏像元与坏列位定义、暗电流绝对阈、拖尾、卫星线各节）、WFC3 与 STIS 数据手册、SDSS 成像处理标志全套文档、Pan-STARRS1 与 DES 数据处理论文、HSC 管线论文全文检索。手册给出的是暗电流绝对阈（0.06 / 0.14 e⁻/秒）与数据质量位分类，没有列级 k·σ |
| "坏列修复导致 mmag 或百分比量级测光偏差"的数字 | 同上来源族。最强的定量口径是**率与掩膜占比**：DECam 的坏像素率 [10]、Pan-STARRS1 的填充因子与掩膜分类占比 [11, 12]、SDSS 的 20% PSF 通量门槛 [7]、PSF 加权的掩膜比例门槛 [12] |
| "插值后相关系数或有效独立样本数"的定量表达式 | 同上来源族。已有的是形式噪声随距离衰减的解析式 [5]，不是相关系数 |

这三个否定结果是本分册保留负例登记的原因：它们划定了"本链的 `column_sigma`、`κ` 是项目定义的、必须在溯源中标注标定来源"的边界。

## 6 与上下游的关系

上游是最高设计的 normalize 流程与验证体系。本分册向下的约束：修复后的像元携带的方差下界被噪声与信噪比分册的稠密重建消费；缺陷掩膜作为独立对象进入排异与排异溯源；外源事件的判定条件（跨帧不复现）交给排异分册。

与《校准》分册的接口：点状缺陷的幅度判据在母版差分上做，母版的物理由校准分册定义；坏点掩膜的极性（1 表示坏点）与修复算子的所有权在校准分册。

## 7 参考文献与参考代码

[1] Space Telescope Science Institute. ACS Data Handbook, §3.4（数据质量数组初始化与位定义表）、§4.3.2（暗电流与热/温像素）、§4.5.6（卫星线）、§4.6.1–§4.6.3（CTE）、§4.4（平场）。https://hst-docs.stsci.edu/acsdhb/chapter-4-acs-data-processing-considerations/4-3-dark-current-hot-pixels-and-cosmic-rays（逐字核对该页的热/温像素阈值句、数据质量位表中位值 16/64/128 的定义文字，以及 §4.6.1 的 "charge trails … can extend to over 50 pixels in length upstream (i.e., away from the parallel transfer direction) of hot pixels, cosmic rays and bright stars"）

[2] Space Telescope Science Institute. WFC3 Data Handbook, §6.3 The Nature Of CTE Losses. https://hst-docs.stsci.edu/wfc3dhb/chapter-6-wfc3-uvis-charge-transfer-efficiency-cte/6-3-the-nature-of-cte-losses（逐字核对电荷从下游取走、沉积到上游、拖尾背离读出放大器的表述）


[3] Kruk S., et al. The impact of satellite trails on Hubble Space Telescope observations. Nature Astronomy, 2023, 7: 262–268. https://doi.org/10.1038/s41550-023-01903-3（开放获取，已取全文逐字核对：典型曝光时长 11 分钟下单帧被卫星线穿越的比例为 2.7 ± 0.2%，卫星线多贯穿整个视场且大多数呈直线）

[4] astropy ccdproc, Reduction toolbox. https://ccdproc.readthedocs.io/en/latest/reduction_toolbox.html（开放获取，取证面限于掩膜流程的参数化形态，未逐页核对）

[5] van Dokkum P., Pasha I. A robust and simple method for filling in masked data in astronomical images. Publications of the Astronomical Society of the Pacific, 2024, 136: 034503. https://doi.org/10.1088/1538-3873/ad2866（预印本 [arXiv:2312.03064](https://arxiv.org/abs/2312.03064)；逐字核对该文结论节的形式噪声式与"难以指定不确定度"的表述）

[6] Desai S., et al. Detection and removal of artifacts in astronomical images. Astronomy and Computing, 2016, 16: 67–78. https://doi.org/10.1016/j.ascom.2016.04.002（预印本 [arXiv:1601.07182](https://arxiv.org/abs/1601.07182)；已取全文逐字核对第 4.2 节的逆方差权重与高斯抽样句、第 3.2 节坏列由圆顶平场与偏置离群像元标定的句，以及第 4.1 节的插值可行性讨论）

[7] Sloan Digital Sky Survey. Understanding the Image Processing Flags — Details（DR17）. https://www.sdss4.org/dr17/algorithms/flags_detail/（逐字核对 `INTERP`、`INTERP_CENTER`、`PSF_FLUX_INTERP` 与 `BAD_COUNTS_ERROR` 四条的文字）

[8] Sloan Digital Sky Survey. Imaging Caveats（DR17）§Bad CCD columns. https://www.sdss4.org/dr17/imaging/caveats/（逐字核对"坏 CCD 列被插值后产生明显相关噪声"一句）

[9] Bosch J., et al. The Hyper Suprime-Cam software pipeline. Publications of the Astronomical Society of Japan, 2018, 70: S5. https://doi.org/10.1093/pasj/psx080（预印本 [arXiv:1705.06766](https://arxiv.org/abs/1705.06766)；逐字核对坏列噪声置无穷大的句、一维插值权重集合与"只有中心被插值的天体不可靠"的句）

[10] Flaugher G., et al. The Dark Energy Camera. The Astronomical Journal, 2015, 150: 150. https://doi.org/10.1088/0004-6256/150/5/150（书目级核验，未取全文）

[11] Chambers K. C., et al. The Pan-STARRS1 Surveys. [arXiv:1612.05560](https://arxiv.org/abs/1612.05560)（书目级核验，未取全文）

[12] Magnier E., et al. Pan-STARRS Pixel Analysis: Source Detection and Characterization. The Astrophysical Journal Supplement Series, 2020, 251: 5. https://doi.org/10.3847/1538-4365/abb82c（书目级核验，未取全文）

[13] Aihara H., et al. First data release of the Hyper Suprime-Cam Subaru Strategic Program. Publications of the Astronomical Society of Japan, 2018, 70: S8. https://doi.org/10.1093/pasj/psx081（书目与 DOI 经 Crossref 核对，未取全文）

[14] Anderson A. C., Bedin G. An empirical pixel-based correction for imperfect CTE. I. HST's Advanced Camera for Surveys. Publications of the Astronomical Society of the Pacific, 2010, 122: 1035–1064. https://doi.org/10.1086/656399（预印本 [arXiv:1007.3987](https://arxiv.org/abs/1007.3987)；书目与 DOI 经 Crossref 核对，未取全文）

[15] Snyder A., Roodman J. Investigation of deferred charge effects in LSST ITL sensors. Journal of Astronomical Telescopes, Instruments, and Systems, 2019, 5(4): 041509. https://doi.org/10.1117/1.jatis.5.4.041509（电荷转移效率与延迟电荷的术语一手来源）

[16] Massey P., et al. Pixel-based correction for Charge Transfer Inefficiency in the Hubble Space Telescope Advanced Camera for Surveys. Monthly Notices of the Royal Astronomical Society, 2010, 401: 371–384. https://doi.org/10.1111/j.1365-2966.2009.15638.x

[17] Erben J., et al. GaBoDS: The Garching-Bonn Deep Survey; IV. Methods for the image reduction of multi-chip cameras. Astronomische Nachrichten, 2005, 326: 432–464. https://doi.org/10.1002/asna.200510396（预印本 [arXiv:astro-ph/0501144](https://arxiv.org/abs/astro-ph/0501144)；该文把坏像元处理表达为"已知坏像元在叠加前赋予零权重"的实践口径）

[18] Abbott T., et al. The Dark Energy Survey: Data Release 1. The Astrophysical Journal Supplement Series, 2018, 239: 18. https://doi.org/10.3847/1538-4365/aae9f0（书目与 DOI 经 Crossref 核对，未取全文）

[19] van Dokkum P. Cosmic-ray rejection by Laplacian edge detection. Publications of the Astronomical Society of the Pacific, 2001, 113: 1420–1427. https://doi.org/10.1086/323894（预印本 [arXiv:astro-ph/0108003](https://arxiv.org/abs/astro-ph/0108003)；书目与 DOI 经 Crossref 核对，未取全文）

参考代码：IRAF `ccdred` 家族（NOAO/IRAF 许可，非 OSI 开源）提供"零校正—暗流—平场"的经典顺序与坏像元掩膜参数；Siril（GPL-3.0）的化妆品校正按幅度记录点缺陷与坏列；LSST `afw`（BSD-3-Clause）的缺陷表与宽缺陷常数回填；astropy ccdproc（BSD-3-Clause）的参数化掩膜。四者只作行为对照，不复制入仓。
