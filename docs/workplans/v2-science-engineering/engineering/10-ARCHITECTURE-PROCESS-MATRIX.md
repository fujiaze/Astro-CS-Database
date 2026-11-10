# C0｜22 个 Process／57 个 typed operation：重构与证明责任矩阵

## 本任务定位

**重构的重点是统一源码真正计算的数学算法，而不是为了代码量把已有实现重写一遍。** 当前分支中 `src/observation`、`src/coordinates`、`src/catalogue`、`src/sources`、`src/response`、`src/photometry`、`src/geometry`、`src/operators`、`src/uncertainty`、`src/information`、`src/sky`、`src/coadd`、`src/projection`、`src/runtime`、`src/workflows` 均有源码；需逐一做“可原样保留／需纠正／需要替换／科学规格有缺口”的实证裁决，不预先判全部失效或全部完成。公共字段只在 `include/acsd/model.hpp`、`runtime/*.hpp`、`modules/*.hpp` 声明；`contracts.hpp` 聚合，不另建立一套当前接口。

本任务属于工程实现，**数学命题以对应 Foundation／NUMERICS／Process 为唯一正本**。本文件只规定责任、审查路径、工作包和交付。历史 `M0–M6` 和 [22 process 索引](../../../engineering/rebuild/processes/README.md) 是输入参考；本任务不自动重新标记其中完成情况。

## 一、逐个 Process 的实现责任、核心正确性命题和必须观察的失败

| # | Process → 实现源码族 | 工程责任、核心证明义务 | 关键定向反例／跨链风险 |
|---|---|---|---|
| 01 | `observation.calibrate` → `src/observation` | ADU→校准原生量；母版/flat/Jacobian/噪声身份；同一校准版封存 | 共享 master 被独立计数、负校准值被抹零、未扣/重复扣偏置、除零flat与饱和 |
| 02 | `observation.regroup` → `src/observation` | crop/bin/Bayer/线性 `G` 的物理采集域、手性、噪声 `GCGᵀ` 及数据版本 | 合并后忘原始像素支持、Bayer半像元物理面积、把非线性修复假作线性 |
| 03 | `coordinates.wcs` → `src/coordinates/tan_sip.*`、`wcs*` | PixelCenter0/FITS1/ICRS/TAN-SIP一套源真值；正逆/导数/界/可证单射 | +1、±0.5、极点/LONPOLE、RA跨界、镜像CD、AP/BP不足 |
| 04 | `catalogue.access` → `src/catalogue` | Gaia历元与source身份、索引查询不漏有效候选、跨块稳定去重 | 高自行运动/视差/站址不足、索引边界漏星、跨进程索引漂移 |
| 05 | `sources.astrometry` → `src/sources/astrometry_*` | 真实检测→独立候选匹配→充分源数/残差/秩→合法WCS | 数据弱/镜像假匹配、星团/畸变、超过预算误写solved |
| 06 | `sources.psf` → `src/sources/psf*` | 连续PSF/PRF像元积分、参数导数/收敛、尾界、空间依赖和模型有效域 | 中心采样假作积分、flux因子丢失、有限窗重归一、强混叠/退化参数 |
| 07 | `sources.measure` → `src/sources/measure*` | 实际测量矩阵 `d=HF+Zβ+ε`、共享噪声和数值通量包围；位置/PSF依赖 | 背景列无PRF积分、弱源负通量、用nominal代替严格通量界、邻源遗漏 |
| 08 | `response.integrate` → `src/response` | Gaia XP basis×响应 photon 积分，分段端点/余项/单位；谱域能力 | 3nm窄带粗格、XP无法恢复未知线谱、QE/λ/hc重复或漏乘 |
| 09 | `photometry.fit` → `src/photometry/joint_fit*` | 共同Gaia潜变量联合拟合、秩/likelihood、真实参数/数值协方差 | 色项与空间共线、重复使用Gaia星误当独立、未知gain强行拆解 |
| 10 | `photometry.apply` → `src/photometry/apply_scale.cpp` | 原生像元空间尺度一次应用、单位/响应/版本和噪声因子同变换 | 空间α只给星中心、重复乘法、先R再α被当等价 |
| 11 | `geometry.overlap` → `src/geometry/containment*`、`overlap*`、`refine.cpp` | 完整HPX叶候选、曲边包含与未决面积、每源Ω/A/a及revision | cone漏候选、tiny sliver、极区、界不包含真值、work耗尽乱返回0 |
| 12 | `operators.action` → `src/operators` | typed线性/非线性能力边界、apply、真实 transpose、recipe与依赖块 | 同源分片在transpose重复/遗漏，动作在重开后变形 |
| 13 | `uncertainty.covariance` → `src/uncertainty` | 确定性界与随机噪声分账、共享root、PSD/约束、C action/查询 | 拆同根算独立、unknown当0、错误绝对界和不合法PSD |
| 14 | `geometry.drizzle` → `src/geometry/drizzle*` | `R` 与支持归一、signed误差、同一R作用signal/PSF/noise/背景、外存CSR | Ω与A混淆、混 revision、4角路径漏叶、不同通道R不一致 |
| 15 | `information.reference` → `src/information` | 针对点源通量 `V_F` / Fisher / 可估参数，明确场景与条件 | Fisher直接冒充球面面亮度inverse variance、欠定模型虚高SNR |
| 16 | `information.scene` → `src/information` | 实际场景 `V_B`、有支持稀疏重建与外部误差先验、holes | 控点插值无曲率界却称认证、zero-fill、域外nearest称测量 |
| 17 | `sky.controls` → `src/sky` | 共同响应可比较性/真实低频共同模式`H`/控制点与相关误差 | Hα对Red直接差、异PSF假Sky、共享控制独立化 |
| 18 | `sky.fit` → `src/sky/sky_fit_process.cpp`、`sky.cpp` | 图连通与gauge、数值rank、同一fit/eval basis、`P=I-EKT` 及交叉协方差 | 断连假绝对背景、常量/坡度过拟合、同数据方差错加 |
| 19 | `coadd.select` → `src/coadd/coadd_select_process.cpp` | 正式DQ/支持/可比较性，N小样本条件式LOO及选择状态 | 近零/负值百分比带、一次拒多个、真实变星误删 |
| 20 | `coadd.weights` → `src/coadd/weights.cpp` | Γ SPD/PSD/BLUE/非负QP、归一与KKT、unknown时具名保守降级 | 共享噪声漏项、负BLUE当非负响应、分母正界/秩未证 |
| 21 | `coadd.integrate` → `src/coadd/coadd_integrate_process.cpp`、`coadd.cpp` | 仅消费同版本Selection/WeightSolution/已校Sky，输出有效kernel | 内部隐藏二次求权、同一raw frame重复贡献、异谱变固定滤镜 |
| 22 | `projection.export` → `src/projection`、`src/workflows/export_workflow.cpp` | 目标真实WCS交叠`Q`/支持平均或积分、同leaf相关、可读FITS | 全像元分母错用、二次pixfrac、负值丢失、写头与模型不一致 |

**特别规则**：57 个具体 operation 的归属白名单仍在 [现有目录](../../../engineering/rebuild/processes/README.md)；逐项审查时必须将该 Process 下所有操作展开为单独记录，不得因为表格只有22行而遗漏例如 WCS 的正向/逆向/导数、geometry 的 refine 和 action 的 transpose。文档/registry 映射是核对手段，不能代替每个 operation 的数学意义检查。

## 二、工程侧五种可交付的证明义务

1. **语义实现**：输入单位/index/shape/response/source身份被校验，合法域外不能伪装正常成功。给出能反驳错实现的一个最小输入和推导真值。
2. **数值实现**：真实表达式、局部算法及停止规则满足所属算法文档；有向算术只能在已审查实现上声称包含性，未定精度应降状态。
3. **资源实现**：plan资源与实际native/string/vector/MPFR/GMP/第三方workspace相符；低额度/取消/失败能终止并清理。资源不足不能改变物理模型或科学阈值。
4. **跨进程实现**：公开typed API与正式CLI调用同一算法；产品封存、退出、再次打开后科学结果/关联对象仍有身份；不能只凭进程内指针可用通过。
5. **组合实现**：producer后置条件→consumer前置条件明确，特别 WCS→PSF→Photo→R、R→C/Information、Sky→Selection→Weights→Coadd、Coadd→Q；同一 shared root、GeometryRevision、SkyFit 及单位在不同目录中不会遗失。

每一条都应由懂该算法的人**实际读代码**并说明重要调用行和逻辑依据；测试只给反证与样例证据。允许源码优化但必须证明替代算法保持估计量、数值有效域及已认证性质。

## 三、执行包与安全修改顺序

### 第一包：公共类型/codec与numerics
1. 对 `include/acsd`、`src/runtime/wire.cpp`、`src/numerics` 建立规格与实际二进制字段映射，不允许将相同单位编码成无语义double。只保留一套当前声明和一套codec。
2. 优先修 interval/PSF/rank 等阻断严格证书的真实问题，随后逐个解开对其的依赖；不得“全局certified=true”掩盖缺陷。
3. 评估公共数值计算积木在哪些 Process 被重复实现，并收敛到同一真实函数；临时 adapters 只做类型与资源连接。

### 第二包：Normalize
`observation→coordinates/catalogue/astrometry→PSF/measure→response/photometry→geometry/information`。先在合法supplied WCS +已知PSF条件下证明物理链，再逐步释放 blind WCS、PSF未知、噪声未知、复杂真实通带等条件，避免一次把全部未知量混起来。

### 第三包：Mosaic/Export
`sky.controls→sky.fit→apply_sky→coadd.select→coadd.weights→coadd.integrate→projection.export`。对同均值的基础场证明一致，再做共享统计、异PSF/滤镜、低支持和部分条件能力。

### 第四包：实际运行
复查全部公开入口和产品生命周期，只有实际服务调用与资源账本符合规范才宣告生产接入；需实际可执行 `Normalize→Mosaic→Export` 全链和产品重新打开，不用临时测试驱动替代正式路径。

## 四、模块独立证明报告具体格式

**每个模块只交一次实质性审查报告**，包括：使用的 Foundation 定理及论文页/式、其目标算法各分支、有别于原论文的推导、当前 C++路径和函数/关键行、输入前置条件、输出后置条件、错误/资源状态、间接consumer、实际证据、反例、仍然conditional的风险、实施者和未参与编码的审查者意见。可以放在对应 Process 正本“工程实现审查”章节或单个交付审计，不创建22个重复模板工具、计分器或自动验收程序。

审查结果分为：可保留／需局部修复／应替换实现／科学规格仍有空缺。若“应替换”，必须说明现有实现违反的具体数学性质、替代法的原论文/推导、迁移数据边与不损失哪些成熟正确能力；禁止主观“旧代码不好看所以重写”。

## 五、完成条件

所有22个 Process及57个operation都有有效一手算法来源或自研归属、独立可读数学合同、实际源码路径和对照审查证据；每一核心跨模块数据边完成正向和逆向追溯；全部本期必需操作在正式CPU应用内实际可用，不能把占位`unsupported`伪装完成。适用域外的正确拒绝可以是成熟功能，但必须不阻断已承诺支持的观测条件。
