# 独立 Oracle、计算证据与科学适用域

**候选科学规范，待另一个外部独立审核者重新推导与验收。** 本轮研究 Agent 和三个独立验证 Agent 的数值检查不构成正式科学验收。基线生产源码 `e7d273ed78b7d118cf8344110b97224af7378b2a`，2026-10-20；最终提交 SHA 记录在 #111 交付评论。本轮不修改生产 C++，不创建重构分支或 PR。

## 1. 阅读入口及证据身份

五份规范从头定义数学和接口，旧 scheduler/adapter/历史补丁不是阅读前置：

| 文件 | 拥有的定义 |
|---|---|
| [OBSERVATION_COORDINATES](OBSERVATION_COORDINATES.md) | 物理观测、PSF/PRF、坐标、WCS/SIP、仿射图像操作及Export |
| [GAIA_RESPONSE](GAIA_RESPONSE.md) | photon积分、XP误差、联合标度拟合、跨响应不可辨识性 |
| [SPHERICAL_DRIZZLE](SPHERICAL_DRIZZLE.md) | 真曲边/真叶交叠、两套求解器、候选完备与面积上下界、同一R |
| [STATISTICS_SKY_COADD](STATISTICS_SKY_COADD.md) | actual-scene误差、共享/同数据相关、相对sky、传统合成/选择 |
| 本文 | 实际数值、反例、源码差异、复算入口、适用域及真实数据假设 |

证据词含义固定：

- **DERIVED**：已给出数学推导，列明假设；不等价于已证明某程序没有任何bug。
- **INDEPENDENTLY_TESTED**：使用解析、外部标准库或独立构造的随机观测实际复算，仅支持指定测试域。
- **ASSUMPTION**：理论/观测模型前提；未从现有数据识别的物理量不冒充测量。
- **REQUIRES_REAL_DATA**：需真实 HST/M42/重复曝光检验的物理前提和生产效果，本轮未执行。

读过 #111 正文及其坐标 Oracle、#98–#102 原研究/补充裁决，最高设计、统一模型、相关科学分册和实际源消费者。原研究中的阶段性方案不是本轮实现的证据；例如 R2 的 Green 公式模板在本轮被实际包含型积分替代。#102 的混阶扩展已由负责人撤回，本规范保留同阶输入要求。

## 2. 复算包与独立性

根目录 [实验/science-foundation](../../../实验/science-foundation/) 存独立 Python 与轻量 JSON，不包含用户真实 FITS。实际环境为 Python3.12.2、NumPy1.26.4、SciPy1.15.3、Astropy7.1.0、WCSLIB8.4、Astropy-HEALPix1.0.3、mpmath1.3.0；依赖版本见 [requirements.txt](../../../实验/science-foundation/requirements.txt)。这些版本用于复算身份，不是新软件内部版本协议。坐标生产探针另用 GCC16.1，临时编译相关原始翻译单元，未跑整个CLI。

从仓库根依次执行：

```text
python 实验/science-foundation/oracles/coordinates_independent.py --production-probe
python 实验/science-foundation/oracles/chart_overlap_independent_review.py
python 实验/science-foundation/oracles/geometry_experiments.py --output 实验/science-foundation/evidence/geometry_experiments.json
python 实验/science-foundation/oracles/detector_independent.py --output 实验/science-foundation/evidence/detector_independent.json --relative-tolerance .005 --max-nodes 40001 --samples 200000
python 实验/science-foundation/oracles/detector_refine.py
python 实验/science-foundation/oracles/detector_interval_validation.py
python 实验/science-foundation/oracles/cross_geometry.py --output 实验/science-foundation/evidence/cross_geometry.json
python 实验/science-foundation/oracles/high_order_polar_independent.py
python 实验/science-foundation/oracles/statistics_independent.py
python 实验/science-foundation/oracles/sparse_variance_reference.py
python 实验/science-foundation/oracles/export_independent.py
python 实验/science-foundation/oracles/hips_orientation.py
```

其余专门反例/Export/稀疏精度入口与命令保存在各脚本头和 evidence 文件。数值输出有 seed、trial数、容差、状态和 source hash；总清单见 `evidence/manifest.json`。失败文件保持失败身份，不覆盖为成功，也不以脚本数量代替科学结论。

| 路线 | 与被测方法分离的关键计算 |
|---|---|
| 坐标 | 手写TAN单位向量+SIP；Astropy完整WCS；另调用未改生产TU探针 |
| 几何A | 目标等面积face域、SIP边界Bernstein凸包/弦同伦、精确dyadic质量 |
| 几何B | detector域g积分、独立正向成员分类；不调用A的face逆式/曲线/winding |
| 第三路高阶几何 | 独立手算fixture Jacobian、2百万iid样本+外部真实成员；目标块随机点反演检查 |
| 统计 | 已知矩阵/谱/随机latent从头构造；不调用旧ACSD统计函数 |

共享输入头和数学定义是必要条件，不构成共用关键解算代码。MC 的置信区间与确定性面积区间分列；零命中不能证明没有sliver。区间库是算法依赖，有限样点外库对照不能证明其全部舍入行为。

## 3. S0：确证缺陷、正确桥接及仍未知的影响

详细逐行记录见 [coordinates_static_audit.md](../../../实验/science-foundation/evidence/coordinates_static_audit.md)，实际数字见 [coordinates_independent.json](../../../实验/science-foundation/evidence/coordinates_independent.json)。以下源行均锁定本轮基线。

| 发现 | 精确证据与范围 | 分类 |
|---|---|---|
| IPV迭代逆两轴+1 | `ipv_wcs.cpp:942–945` 输出u+CRPIX，头文件却标0基；1003/1003点收敛但两轴差约+1pixel，误差约1e−9pixel | 已确证接口数值/语义缺陷；全库未发现该接口实际消费者，不声称已污染当前CLI |
| TAN背半球被错误接受 | 同文件814行仅排abs(cosc)近0；CRVAL严格对跖点被迭代逆错误判收敛；测光逆正确返回NaN | 已执行生产函数负例 |
| detector与DPSF差半像元 | `sdet_api.cpp:795–796` 在index+.5拟合，输出到1682–1683；DPSF在整数中心。64×64已知Gaussian中心(31.237,30.671)得到detector(31.737,31.171)，DPSF匹配Moffat恢复整数真中心约1.5e−12pixel | 模块原点差已执行证明 |
| adapter缺转换 | `module_adapters.cpp:4283–4284` 直接赋source gx/gy，4292–4293直接lround取峰，4361–4362传DPSF，源表字段未按其PSF cx/cy回写 | 静态可达链不一致已定位；尚未执行完整adapter/真实数据链，不能编造E2E偏移量 |
| IPV核心居中原点 | detector−W/2=(p0+.5)−W/2=p0+1−(W+1)/2；solver的CRPIX=(W+1)/2自洽 | 正确，不将整个IPV泛判+1 |
| Drizzle、测光、Export桥接 | Drizzle使用x−(CRPIX−1)，测光逆输出−1；Export相应±1正确；WcsTan的FITS1接口在adapter已先+1 | 可保留数学；局部标准接口正确不是全链验收 |

独立TAN+2/3/4阶SIP、三个切点、9027点，含非整数CRPIX、非对角镜像CD及双极区，手写向量对Astropy最大3.626e−10arcsec；同构型Drizzle生产前向相同量级，测光生产逆最大3.732e−10pixel。crop、bin2×3、flip-x/y、transpose与复合变换各500点对拍。错误原点正逆往返仍可达4.74e−10pixel，却有约1arcsec绝对错位：round-trip不能替代绝对Oracle。

上述是数值和接口缺陷，真实饱和/混叠/PSF/大气偏差为另一类问题。没有运行真实FITS或完整CLI，不能由这些结果直接宣布旧全产品应整体平移1或.5像元。

声明的HiPS数组方向另由独立HPX FITS头/WCSLIB与外部HEALPix作3600点验证，涵盖12面和Nside=8/512/2¹⁸；0个无定义值、最大角差9.82e−14rad，见 `hips_orientation.json`。先前对face6未处理原生经度中断的92个无定义点保留在 `hips_orientation_initial_seam_failure.json`；修正投影中心而不改变数组原点。它仍不是第三方生产者互操作测试。

## 4. S2：真曲边计算证据与失败修正

几何A的18组实际结果在 [geometry_experiments.json](../../../实验/science-foundation/evidence/geometry_experiments.json)：12个face中心+双apex/转折/RAwrap/细阶共17构型，另有解析TAN矩形；全部按各自输入预算收敛。全12face连续offset四个阶数共240点，最大外库向量角差1.45e−15rad。独立审核另用18240点跨完整face、r=1/3和极点邻域做区间包含抽查，未见超界。

| 数学真值/反例 | 结果 |
|---|---|
| 北极apex真叶面积 | π/(3N²)；四角测地相对误差N256约−9.9681198%，N8192约−9.96836814%，不随高阶趋零 |
| apex中央经度楔的独立 `dφ dz` 积分 | 真叶的2/3，非平凡局部成员校验 |
| SIP曲边半平面 | 无折叠真交叠.49、四角.47，证伪短边可免曲率检验 |
| 2°无SIP TAN方形 | 解析1.2180986439133989e−3sr落入A上下界 |
| 原生P与f=.37的Q双面积 | 独立detector有界积分 A/Ω区间约[.13686678,.13686951]，排除f²=.1369 |
| 完整R的常数场/协方差 | 独立矩阵以非单位Ω、不同drop面积验证同一R；不得用D_p代N_p |

**独立审核修正一：参考A的质量累计与预算。** 原float账本的`.25+100·4^-28`可丢失面积，末端扩1ulp不够；初始递归可在max_nodes=1时处理64节点。现全部质量改精确Fraction dyadic、所有根与子块走统一队列、max_depth≤48、未展开粗候选保留位置和上界。独立复核504节点的受控精确集合夹住70位真值，max_nodes=1只处理1节点，精度耗尽仍保界。原反例及修正证据在 [chart_overlap_independent_review.json](../../../实验/science-foundation/evidence/chart_overlap_independent_review.json)。

**独立审核修正二：外部球冠查询不能当候选完备证明。** 第一版B以Astropy-HEALPix1.0.3的inclusive cone唯一候选判整块内含，在N=2¹⁸、近北极、1arcsec SIP/drop构型中，与A的四个叶区间均不相交，虽然总面积相符。第三路2百万iid积分支持A；5118个A内含块各8点，共40944点用外部完整逆WCS抽查，0个域外点。

已找到具体漏选：detector子矩形`(.175,.35,−.175,0)`，cap半径`8.187563679e−7rad`，外部query只返回叶274877380095；矩形角点(.35,0)到cap中心的真实向量角距`6.20327635e−7rad`（半径的.758倍），却属于未返回的274877380437。不能靠调宽面积容差、仅增加节点数或总面积闭合掩盖此反例。失败在 [detector_cone_completeness_counterexample.json](../../../实验/science-foundation/evidence/detector_cone_completeness_counterexample.json) 和 [cross_geometry_cone_failure.json](../../../实验/science-foundation/evidence/cross_geometry_cone_failure.json) 留存；**旧cone模式结果撤销作为科学包围证据的资格**。

正式B因此改用独立区间TAN向量→正向NESTED floor分支的完整成员包围；外库索引/offset/MC仍用于独立对照，外部cone只保留失败重现用途。新B正向分类与外部12face/三阶数288点无遗漏，另60个天球向量含精确双极点和LONPOLE旋转，对Astropy最大差7.02e−16rad。原漏叶矩形的新候选正确同时包含两叶；极端候选工作预算不足时保留all-sky未决质量，也已实际检查。

**最终新双算法对拍**：[cross_geometry.json](../../../实验/science-foundation/evidence/cross_geometry.json) 的8例（两个代表面、RAwrap、双极区、转折、tiny sliver和高阶小drop）A/B均按各自预算收敛，全部叶区间与总面积区间有交集；三份源码hash均与交付代码一致。N=2¹⁸近极点原失败构型的新B在全drop0.5%预算下12265节点收敛，A、B与第三路iid相容。

独立B全20例在40001节点预算下11例收敛、9例保持budget_exhausted；后9例以**相同0.5%阈值**追加到80001预算，均在41733或43761节点收敛，见 [detector_interval_refinements.json](../../../实验/science-foundation/evidence/detector_interval_refinements.json)。原20例的源码为修tiny-spill显示条件之前的加载快照，已精确保存 `evidence/source_snapshots/detector_interval_pre_spill_guard.py` 且hash相符；追加9例和最终cross使用最终源码。正常20构型无该极端spill，未混淆运行身份。

不能按两算法中点强制归一再宣布相符。逼近零的sliver保留正upper；未收敛状态保留。性能仅是独立Python小尺寸参考的实际耗时，不保证亿级吞吐。

生产静态几何：`spherical_overlap.cpp:1508–1511,1569,1611`高nside仍用四角；`:683–689`采样路径也退回四角；`drizzle_engine.cpp:1238`仅角跨度触发源曲边适应。上述机制与规范不一致；本轮未替换它们。当前生产已有正确detector先收缩、w/N/F代数和连续writer面积桥接，可参考其数学，不因此复用四角几何。

## 5. Gaia、统计、sky及合成的实际数值

[statistics_independent.json](../../../实验/science-foundation/evidence/statistics_independent.json) 每个noise构型100000次，seed=11120261020；Gaia联合拟合1000次，保存Clopper–Pearson精确二项95%区间。详解见 [STATISTICS_RESEARCH](../../../实验/science-foundation/STATISTICS_RESEARCH.md)。

| 实验 | 实际读数 | 能证明/不能证明 |
|---|---|---|
| 4×3 R+共享latent | MC covariance相对Frobenius差.0089913 | 与指定随机模型一致；不是算法统一0.9%误差界 |
| Fisher相同而scene不同 | V_F两者1；surface权2与1.219512 | 参考SNR不能默认转sceneIVAR |
| 100帧共享floor | 真方差.05，误当独立.0104 | 同一latent不能按帧数消失 |
| 公共真实梯度保护 | 最大改变3.55e−15；已知差模参数恢复9.99e−16 | 所声明H中共模保护，不保护错误H |
| 同数据sky | 正确方差.9869281、错独立加法1.0653595、MC .9880928 | 必须保留交叉项 |
| 10%response差、PSF失配 | 分别出现假sky系数/假斜率 | 不同H不能因同天区就作共同天空 |
| 3/7nm photon积分 | 693.97311255/1621.10833603 s⁻¹m⁻²；解析相对差1.11e−16/2.22e−16 | 求积精度仅限声明谱；不恢复未知谱线 |
| σ=.05nm真线以σ=4nm低分辨表示 | 带内线贡献−71.2144%/−38.9240% | 合成分辨率反例，不声称Gaia实际LSF为4nm |
| Gaia共享星latent联合拟合 | 无噪声参数差6.25e−17；两帧log零点cov3.33377e−5 | 共享谱误差形成跨帧相关 |
| Gaia颜色=位置的rank负例 | 15维联合J秩14；最小σ9.28e−14 | 正确返回GAIA_JOINT_FIT_NOT_IDENTIFIABLE |
| N5单步最大LOO，α=.001 | 清洁误拒.00102，95%CI[.00083176,.00123808] | 与清洁Gaussian外部C的理论界相容 |
| 单12σ污染后68/95%coverage | Gaussian .67990/.95096；Poisson+read .67955/.94869 | 指定全流程MC结果，不是选择后一般定理 |

原并行拒所有极端LOO的N5污染后95%coverage=.91603，已否定该方案作为基线，失败原输出 [statistics_parallel_loo_failed.json](../../../实验/science-foundation/evidence/statistics_parallel_loo_failed.json) 保留。N1..3缺少外部身份信息时不自动判坏；污染后可能完全不覆盖真值，这是能力边界。不同污染幅度、多污染、真实暂现源仍需真实/合成针对性验证。

当前生产静态差异另见研究报告：spectrum_integrator中旧λ积分是具名模型惯例，不能按新photon单位直接解释；小N百分比接受带不具平移等变性；sky floor-bin拟合与bilinear求值不共享basis。修正科学定义不代表本轮改动这些C++。

### 5.1 稀疏精度与Export的独立闭环

[sparse_variance_reference.json](../../../实验/science-foundation/evidence/sparse_variance_reference.json)从25个actual-scene正方差控点构造局部球面gnomonic三角插值。log平面5000查询一次再现最大误差1.61e−15；外部δ/Hessian界下平面/二次真值各5000点均被界包围，最大误差/界约.949/.960。图外返回unavailable；缺曲率先验的C∞ bump在全部控点为0、查询值可任意大，独立证伪仅凭“光滑”即可认证稀疏场的主张。普通float插值实验不是机器区间证书，不混入真实photon covariance。

[export_independent.json](../../../实验/science-foundation/evidence/export_independent.json)将真实叶内−2/0/+3信号导入2×2偶数尺寸WCS，CRPIX=(1.37,1.12)、RA回绕、镜像非对角CD。四百万独立外部WCS/成员detector积分给16量的Hoeffding联合失败概率≤1e−6；全/部分支持的面积、积分与均值区间均与reference相交。部分支持像元面积约30.08%，正确支持均值−2、错用全像元分母−.6015。200000次Gaussian covariance模拟最大偏差2.36个抽样标准误；显式双和与同一合并leaf矩阵差5.55e−17。拆同叶.35/.65后假独立会只报正确白噪声方差的.545。

Export最初每像元60000节点、绝对面积预算2e−5sr仍有未收敛，且当时未保存逐像元明细；失败范围如实在 `export_initial_precision_failed.json` 中说明。后续参考预算明确为3e−4sr，逐像元立即缓存并把实际几何区间传到最终结果。这是给定精度下的参考验证，不声称达到初始精度；初始失败不删除。目标科学预算必须由后续真实PSF/孔径用途决定。

## 6. 可复用决策、正确失败与物理假设（A–G）

| 对象 | 重建决策 | 原因 |
|---|---|---|
| FITS/WCSLIB/Astropy完整WCS | 可按公开API及许可证复用成熟实现 | 坐标标准清楚；仍需typed原点和完整SIP边界 |
| GaiaXPy/XP基及其covariance | 可复用成熟谱表示及有效通带计算 | 不把standardisation等同任意原生T×QE |
| HEALPix外部点成员/offset | 用作独立对照；按标准实现点索引 | 有限测试不替代候选/曲边证明 |
| 外部inclusive cone | 不作为严格包含/候选证明 | 本轮已有高阶漏选反例 |
| Bernstein/de Casteljau、包含型积分 | 可编码正式几何原语；以A/B对拍 | 真实多项式曲边与叶的有界求解 |
| 旧Drizzle累加公式 | 保留其w/N/F数学，重新按新对象实现 | 不继承四角几何/未知variance身份混合 |
| 旧IPV全部实现 | 只参考匹配/求解思想；标准接口重新审计 | 核心居中可自洽，逆和detector边界有确证缺陷 |
| QR/SVD/GLS、Schur、Woodbury | 可复用成熟数值库，保留rank/PSD语义 | 不盲加ridge、不漏精确约束 |
| Montage/Madam | 借用差模/profile数学，不照搬全部工程/先验 | 响应、PSF、控制cov和gauge需本观测系统定义 |
| 旧SNR→权、percentile、sky更新 | 不复用其科学声明 | 估计量错配/平移反例/不同basis已明确 |
| SACoadd | 仅保留邻域接口，延期 | 当前采用传统baseline，不扩展任务范围 |

正确失败/退化：无WCS或逆不收敛返回unsolved；模型非单射/尚未证明单射分别说明；area预算不足返回上下界及未决位置；N=0为无支持；非零源系数噪声unknown时不发full variance；sky秩不足保留不可辨识模态/逐分量gauge；稀疏支持外不nearest-fill认证精度；混order明确拒绝；混响应可按用户规则出合法signal但身份为条件混合。未知gain不阻止信号，未知通带不伪造已知曲线。

不可辨识量包括：口径/灰色效率/gain分解、有限星SED未约束的响应模态、单通带观测中的任意SED变换、sky共同低阶模式及断连分量间零点、原像元未采样子结构、稀疏控点以外无先验的任意variance场。各分册已给其不变变换/nullspace证明，而不是把这些问题列成可由更多内部补丁解决的TODO。

## 7. 真实 HST/M42 的后续证伪实验

真实观测不在本环境，本轮结果不声称其物理假设已通过。后续数据验证须固定输入/观测条件、实际WCS、response、PSF、mask、noise身份及处理参数，报告bias、实际误差和独立留出，而非只报训练RMS、接缝变平或PASS数量。

1. **坐标/PSF**：真实重复星按历元/颜色/位置分层；验证detector→PSF→WCS→Drizzle→Export的同一中心和像元积分，含饱和/混叠及DCR。HST有PSF的图只作声明分辨率的条件场景，额外退化需可实现核。
2. **响应**：同设备同带标度留出、相近Red的色项、3/7nm Hα及M42 Hα/[NII]结构。实测曲线和顶帽假设分别评估；未知gain设备继续出signal但不虚构electron方差。
3. **曲边**：真实WCS/SIP的极区/面缝/强曲率，与独立A/B面积区间核对；测星点孔径、centroid/PSF、弱扩展结构和实际吞吐/RSS。数值曲边精确仍可能暴露上游WCS模型误差。
4. **误差**：重复曝光的空场/亮源shot-noise、共享flat/Gaia/sky的跨像元跨帧cov；分别检验V_F、V_B和孔径统计的68/95%coverage，未知项不能从低RMS中“补出”。
5. **天光**：纯真实共同梯度阴性、已知正负差模阳性、断连/稀疏支持、异PSF/混Hα+Red负对照；测低频结构保真与留出接缝，禁止只为平缝扣天体色差。
6. **合成/Export**：N1..7、单/多宇宙线、跨tile拖线、真实时变源；比较无排异和正式基线，记录误拒/漏拒及选择后coverage。独立FITS reader核对CRPIX/BUNIT/响应、部分覆盖的支持积分及二次重采样相关性。

只有上述物理假设被适当真实数据支持、且本候选规范经另一个审核者独立重推与复算认可后，才讨论新核心重构分支。此次提交是完整研究候选与计算证据，不是生产修复、架构发布或正式科学验收。
