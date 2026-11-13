# 07｜独立验证矩阵：每个Process的科学/工程反例、性能与真实M42验收

**本文件是迁移包共用的测试任务表，不是新建CI门禁。** 每个测试由实现Agent产出输入/命令/源SHA/实测/期望/独立解释，未执行为NOT_RUN；独立Reviewer不能只检查实现者提供的测试退出码。总体科学公式沿五篇 Foundation、数值判据沿现有 VERIFICATION；下表明确**旧功能未退化且V2新科学正确**的最小真实证据。非核心 trivial I/O只需完整性和故障路径核对，不重复成千“格式测试”虚增PASS数。

## V-00 判别方法：同输入等价、独立真值、真实质量三分开

1. **主分支生产行为**：固定`main@c5fee09f`，只选择旧正式可调用的代码；历史实验可用于构造反例但不替代同一机器实际运行。旧main科学失效的样本用“旧错、新能按真值修”反例，不强行逐像素一致。
2. **独立数学真值**：解析常值/脉冲/已知PSF/SED、Gaia星表/物理 forward simulation、Astropy/WCSLIB与healpy/astropy-healpix独立读数；期望先冻结并解释误差的统计来源。若大气、仪器gain/QE真实未知，SNR和绝对响应标unknown并用经验数据或条件真值，不可造参数。
3. **用户实际图像质量**：真实高阶像元细节、至少两个不同面板的真正天空重叠、Sky有向差模、星云共同低频、四通道共同WCS与真实排异的注入真值。数学闭包正确/文件哈希全绿/exit0可作为工程证据，但**不能代替这一步**。
4. **真实性与功耗/存储**：每个重要测试记录真实source_sha、二进制sha、被调用module、输入哈希、科学recipe、真实frame count、结果产品SHA、wall/CPU/peakRSS/work/disk/IO；新进程重开和外部FITS严格校验。没有原始数据或计算资源则说明BLOCKED，不虚构跑过。

## V-01 全22 Process独立任务矩阵

| Process | 必须通过的独立方法与负例（内容是任务而非宣称PASS） | 基线性能对照 |
|---|---|---|
| observation.calibrate | 真实T2 master逐像元独立bias/dark/flat，flat≤0、饱和/坏点、BZERO、T3母版缺失与shared roots | bytes decoded、额外全图拷贝、pixel/s、RSS |
| observation.regroup | G与Dα不交换，sum/mean/翻转/转置、含孔洞和GCGᵀ，不得密集G | 稀疏nnz/块、额外复制与流式吞吐 |
| coordinates.wcs | TAN/SIP反向及强畸变，旧#92 0/2309负例，非整数CRPIX、CD手性/RAwrap/域外 | 独立Gaia/RMS、batch transforms/s |
| catalogue.access | Gaia索引/PM/epoch/错hash/缺RV及重复source_id，与真实范围查询对照 | 数据库一次加载、随机IO、cache命中 |
| sources.astrometry | IPV盲解与留出Gaia，30帧无头CD、假匹配、RANSAC耗尽和多frame成功率 | sources/s、每候选work/实际OMP租约 |
| sources.psf | 已知积分PSF/双星/空间变化/saturated中心/模板数值区间和背景污染 | 星/秒、模板/迭代成本、工作区占比 |
| sources.measure | 星点总flux/邻星联合/局部星云梯度，旧负299万亮星误报、低SNR负值合法 | windows/s、模板重用、矩阵rank与scratch |
| response.integrate | XP/SED×filter×QE分段解析积分、外支持/单位/尾界，旧Antlia误配Baader负例 | 缓存前后求积次数、quadrature/work |
| photometry.fit | 共享θ/Gaia latent、夜间1.2766比值、强背景、训练/留出星，秩亏/非收敛 | 点源/区域/s、LM/GLS实际work |
| photometry.apply | α、α²、α⁻²、空间梯度、重复应用拒绝、旧#9真实同帧开关对照 | 每像元遍历次数/带宽与copy量 |
| geometry.overlap | 全WCS曲边、RAwrap/face seam、sliver/边缘/极区、区间包含、pixfrac | candidate完整性、overlap/s、cache-hit |
| geometry.drizzle | 自动分辨率、R常场/脉冲/真PSF、CSR身份、支持与被感知叶；旧order13退化 | nnz、valid leaf、touched_tile、CPU/RSS |
| operators.action | CSR apply/transpose/composition、跨块、闭包children丢失、非法hash | sparse SpMV/s、scratch、读块次数 |
| uncertainty.covariance | shared roots 2帧/多帧、C作用/二次型/PSD、同数据sky负交叉 | action_apply/work、CSR bytes、无dense展开 |
| information.reference | 有真实条件的F_ref信息与PSF模板、多星Gaia共享、模型不可识别 | 稀疏GLS/solve、rank/数值认证成本 |
| information.scene | V_F与V_B不同、真实scene条件未知、插值/支持三态与空间外推 | 只求必要窗口与叶、不展开全图 |
| sky.controls | 真跨面板空间重叠、单位一致、星云/亮星mask/噪声与响应匹配 | TilePair读次数、controls/s、局部缓存 |
| sky.fit | 联合Schur/IRLS/断连gauge/rank、真实乘性flat与加性sky分离，PCPᵀ | 控制规模/秩/求解/装配/峰RSS |
| coadd.select | cosmic/坏点/卫星注入TPR/FPR，小N 2/3不误删，旧N4/5异常 | per-stack算法吞吐、scratch、N分层 |
| coadd.weights | shared vs independent、scene variance有效域、不把Fisher充scene权 | 权重规划/块、依赖action成本 |
| coadd.integrate | 同真实单位、已排异样本加权/负零/NaN与重复帧身份、真多面板 | valid leaf/s、chunk/IO、并发差异 |
| projection.export | 正确Nside→tile order、独立球面→FITS、nearest/bilinear/area适用域、ACSUNIT长卡、闭包 | SharedLRU容量/worker、FITS像元/s、stream内存 |

**每行审查必须回答四件事**：main可达函数/旧fixture是什么、V2本轮改了哪个真实consumer、独立真值和现有unknown如何区分、相同合法数学模型下性能是否退化。缺乏一个可靠独立证据时状态PENDING，而不是“整张表22/22通过”。

## V-02 固定真实数据实验阶梯，避免无限重跑

| 阶梯 | 输入及前置条件 | 目标及不可用替代 |
|---|---|---|
| T0 small analytic 32²/多像元 | 原作者算法数学域，独立算子和边界 | 证明真实元素/单位与必要unsupported，不能只看shape |
| T1 128/256/512真实ROI | 冻结M42 T2/M5与已解Gaia、母版、PSF模型 | 定向诊断/修复、稀疏扩容、可复用缓存，不重复全量hash |
| T2 4096²一帧高阶 | autoResolutionPlan、真正像元R、Storage可重开 | 完整细节/星点PSF/真实FITS/内存，order13不计 |
| T3 同通带不同面板≥2帧 | 外部天球证据确认面板实际重叠，统一order/WCS | 全局Sky/Coadd/空间signed seam，不可用M5三帧顶替 |
| T4 六面板各通道生产预flight | 全局WCS/response/PSF/Gaia/校准已可用 | Coverage完整、零点/背景模型与资源预测 |
| T5 S2 196候选正式处理 | 经独立Reviewer通过T2/T3且没有重大性能阻断 | Hα50/R49/G49/B48逐帧去向；全部合格帧合成四个全域FITS |

真实`testdata/M42_T2T3_mosaic_Flying_dutchman`之前冻结的196个输入SHA清单`validation/m42/inputs_manifest.json`必须复用，确有变化才重新计算单文件哈希；同一配方/源码hash的有效Normalize产品不重跑。main `实验/m42-realdata/`历史49帧真实运行报告是参考，不保证旧`run/RELEASE-05`还存在；不可将不存在的旧产物当本次输出。

## V-03 真实FITS必须科学与几何上可信

针对截图中“菱形星点/大片斜块/中心黑洞”：先对每帧WCS/PSF/真实像元尺度和导出实际leaf/order做数学因果诊断。运行三个独立分组：
- **原生源星真值**：生成与真实M42相似的已知PSF/星云/噪声模型，原生CCD前向曝光，与完整Normalize→Mosaic→Export、直接高精真值目标WCS比较，量FWHM、ellipticity、PSF残差、高频MTF/功率谱、孔径通量和伪块周期。真实M42无法知道绝对真值时使用未参加fit的Gaia星与重复帧统计及外部原始图对照。
- **真实面板接缝**：沿空间球面邻接交叠带量有向背景跳跃`ΔB`、gradient、PSF匹配后的相对gain、扩展源共享低频能量和预测/经验统计不确定度；对不同强度区独立评估，而非单独在暗区域通过。根据真实统计误差建立有预先固定置信区间的判据，**不能为达到PASS事后改变region或阈值**。
- **排异/SNR**：N≥4时在注入宇宙线、卫星、坏点/真实星云结构与独立控制真值上分别测TPR/FPR/误删弱信号；SNR仅按同估计量的V_F、V_B、V_ctrl或有来源的经验重复估计；缺真实gain/read/shared covariance时保持conditional/unknown，不能宣称最佳BLUE。

外部Astropy严格解析每张FITS全Header、WCS逆/正映射、shape、BUNIT、响应与产品来源；合法负值/真实不支持的NaN不可一律涂抹。缺LinearAction / sidecar完整性 /非法卡/产品信号全NaN而退出0属于P0失败。四通道共同目标WCS在像素级一致，覆盖所有合格M1–M6输入的真实footprint（不是随意4096²裁剪）。

## V-04 性能也必须成为独立Reviewer的必看证据

测试硬件与配置、二进制SHA固定，按[06](06-PERFORMANCE.md)提供每模块和每真实阶段wall/CPU/peakRSS/memory owner/IO/work、算法等价与科学差异分解、线程1/2/4/8、冷暖缓存、3次（全量高成本可例外有依据）。若因为高阶和严格区间比旧代码重，必须说出复杂度增量和哪些原性能优化已真正移植；不要求错误旧计算零成本，但不能把慢都归因于“数学更好”。

**独立审核者必须打开真正数据**：抽样失败像素/真实source FITS、head/CSR/支持/ sky-controls/权重、实际M42 FITS、客观空间接缝、性能采样。需要在已经通过的基础上“故意毁掉”NSIDE/LinearAction/ACSUNIT/sky增益/PSD或降低并行，看测试能否判红。若不能判红，则该检查没有判别力。

## V-05 通过状态和回修

状态统一 `NOT_RUN / IMPLEMENTED_NOT_VERIFIED / PASS_SCOPED / FAIL / BLOCKED_EXTERNAL`。Reviewer只对同SHA同科学recipe、实际冻结输入、已执行的窄域作判决。FAIL时主Agent回到具体Producer，修源码→最小oracle→只重算失效缓存与真实帧→独立复核；本轮不因一个失败就询问用户“是否继续”。若实际用户无法提供校准/响应/磁盘/权限且数学上不可推断，则记录真实不可逾越依赖与可复用结果，不造合法性。科学研发完成才进入[08](08-EXECUTION.md)自动 S2。
