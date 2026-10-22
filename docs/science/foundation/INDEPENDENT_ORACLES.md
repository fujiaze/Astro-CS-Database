> 当前科学设计状态：已完成独立科学及工程完整性复核，未发现尚存P0/P1设计阻碍；结论与范围见[最终审核记录](../../audit/final-review.md#科学审核状态)。不再以重复的通用独立科学审核作为启动实现前置；负责人启动授权、实现后的独立数值/产品/性能验证及真实数据适用性验证仍分别成立。公共字段、签名与枚举只由[contracts.hpp](../../../include/acsd/contracts.hpp)拥有。
>
> 历史证据边界：下文所有“已运行/生产探针/本轮/独立脚本”、研究性能数字及研究文件名均指[固定基线档案c5fee09fe](https://github.com/fujiaze/Astro-CS-Database/tree/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation)。这些脚本及旧生产源码不在当前工作树；复算只能在该提交的单独历史checkout及其依赖环境中进行，不能在本分支根目录直接运行，也不能用其结果验收新实现。当前可运行内容仅为[根README](../../../README.md)列出的设计/schema/声明检查，未来科学测试另按VERIFICATION重新编写。


# 独立 Oracle、计算证据与科学适用域

**当前科学设计基线（审核范围见页首）。** 以下保留历史研究与复算身份，不把它们转记为新实现或真实数据验收。第一轮生产基线 `e7d273ed78b7d118cf8344110b97224af7378b2a`；第二轮研究基线 `9f0c8bcebd41ccfd74bcda32831ac8eb7ce0d9d0`，2026-10-21，针对[外部审核6294372640](https://github.com/fujiaze/Astro-CS-Database/issues/111#issuecomment-6294372640)继续补证。以下§2–7保留第一轮证据，§8列本轮最终结论和复算入口。历史研究时未修改旧生产 C++、未创建重构分支或 PR；当时交付SHA记录在 #111 评论。

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

## 2. 历史复算包与独立性

固定历史checkout根目录 [实验/science-foundation](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/) 存独立 Python 与轻量 JSON，不包含用户真实 FITS。实际环境为 Python3.12.2、NumPy1.26.4、SciPy1.15.3、Astropy7.1.0、WCSLIB8.4、Astropy-HEALPix1.0.3、mpmath1.3.0；依赖版本见 [requirements.txt](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/requirements.txt)。这些版本用于复算身份，不是新软件内部版本协议。坐标生产探针另用 GCC16.1，临时编译相关原始翻译单元，未跑整个CLI。

仅从上述固定历史checkout的根目录依次执行：

历史复算命令：仅在固定c5fee09fe的独立历史checkout运行，当前工作树没有这些脚本；不是新实现验证命令。

```text
# HISTORICAL_CHECKOUT_ONLY: c5fee09fedb97ccbe39cb47ea0985e0d8a339db2
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

详细逐行记录见 [coordinates_static_audit.md](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/evidence/coordinates_static_audit.md)，实际数字见 [coordinates_independent.json](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/evidence/coordinates_independent.json)。以下源行均锁定本轮基线。

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

几何A的18组实际结果在 [geometry_experiments.json](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/evidence/geometry_experiments.json)：12个face中心+双apex/转折/RAwrap/细阶共17构型，另有解析TAN矩形；全部按各自输入预算收敛。全12face连续offset四个阶数共240点，最大外库向量角差1.45e−15rad。独立审核另用18240点跨完整face、r=1/3和极点邻域做区间包含抽查，未见超界。

| 数学真值/反例 | 结果 |
|---|---|
| 北极apex真叶面积 | π/(3N²)；四角测地相对误差N256约−9.9681198%，N8192约−9.96836814%，不随高阶趋零 |
| apex中央经度楔的独立 `dφ dz` 积分 | 真叶的2/3，非平凡局部成员校验 |
| SIP曲边半平面 | 无折叠真交叠.49、四角.47，证伪短边可免曲率检验 |
| 2°无SIP TAN方形 | 解析1.2180986439133989e−3sr落入A上下界 |
| 原生P与f=.37的Q双面积 | 独立detector有界积分 A/Ω区间约[.13686678,.13686951]，排除f²=.1369 |
| 完整R的常数场/协方差 | 独立矩阵以非单位Ω、不同drop面积验证同一R；不得用D_p代N_p |

**独立审核修正一：参考A的质量累计与预算。** 原float账本的`.25+100·4^-28`可丢失面积，末端扩1ulp不够；初始递归可在max_nodes=1时处理64节点。现全部质量改精确Fraction dyadic、所有根与子块走统一队列、max_depth≤48、未展开粗候选保留位置和上界。独立复核504节点的受控精确集合夹住70位真值，max_nodes=1只处理1节点，精度耗尽仍保界。原反例及修正证据在 [chart_overlap_independent_review.json](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/evidence/chart_overlap_independent_review.json)。

**独立审核修正二：外部球冠查询不能当候选完备证明。** 第一版B以Astropy-HEALPix1.0.3的inclusive cone唯一候选判整块内含，在N=2¹⁸、近北极、1arcsec SIP/drop构型中，与A的四个叶区间均不相交，虽然总面积相符。第三路2百万iid积分支持A；5118个A内含块各8点，共40944点用外部完整逆WCS抽查，0个域外点。

已找到具体漏选：detector子矩形`(.175,.35,−.175,0)`，cap半径`8.187563679e−7rad`，外部query只返回叶274877380095；矩形角点(.35,0)到cap中心的真实向量角距`6.20327635e−7rad`（半径的.758倍），却属于未返回的274877380437。不能靠调宽面积容差、仅增加节点数或总面积闭合掩盖此反例。失败在 [detector_cone_completeness_counterexample.json](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/evidence/detector_cone_completeness_counterexample.json) 和 [cross_geometry_cone_failure.json](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/evidence/cross_geometry_cone_failure.json) 留存；**旧cone模式结果撤销作为科学包围证据的资格**。

正式B因此改用独立区间TAN向量→正向NESTED floor分支的完整成员包围；外库索引/offset/MC仍用于独立对照，外部cone只保留失败重现用途。新B正向分类与外部12face/三阶数288点无遗漏，另60个天球向量含精确双极点和LONPOLE旋转，对Astropy最大差7.02e−16rad。原漏叶矩形的新候选正确同时包含两叶；极端候选工作预算不足时保留all-sky未决质量，也已实际检查。

**最终新双算法对拍**：[cross_geometry.json](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/evidence/cross_geometry.json) 的8例（两个代表面、RAwrap、双极区、转折、tiny sliver和高阶小drop）A/B均按各自预算收敛，全部叶区间与总面积区间有交集；三份源码hash均与交付代码一致。N=2¹⁸近极点原失败构型的新B在全drop0.5%预算下12265节点收敛，A、B与第三路iid相容。

独立B全20例在40001节点预算下11例收敛、9例保持budget_exhausted；后9例以**相同0.5%阈值**追加到80001预算，均在41733或43761节点收敛，见 [detector_interval_refinements.json](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/evidence/detector_interval_refinements.json)。原20例的源码为修tiny-spill显示条件之前的加载快照，已精确保存 `evidence/source_snapshots/detector_interval_pre_spill_guard.py` 且hash相符；追加9例和最终cross使用最终源码。正常20构型无该极端spill，未混淆运行身份。

不能按两算法中点强制归一再宣布相符。逼近零的sliver保留正upper；未收敛状态保留。性能仅是独立Python小尺寸参考的实际耗时，不保证亿级吞吐。

生产静态几何：`spherical_overlap.cpp:1508–1511,1569,1611`高nside仍用四角；`:683–689`采样路径也退回四角；`drizzle_engine.cpp:1238`仅角跨度触发源曲边适应。上述机制与规范不一致；本轮未替换它们。当前生产已有正确detector先收缩、w/N/F代数和连续writer面积桥接，可参考其数学，不因此复用四角几何。

## 5. Gaia、统计、sky及合成的实际数值

[statistics_independent.json](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/evidence/statistics_independent.json) 每个noise构型100000次，seed=11120261020；Gaia联合拟合1000次，保存Clopper–Pearson精确二项95%区间。详解见 [STATISTICS_RESEARCH](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/STATISTICS_RESEARCH.md)。

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

原并行拒所有极端LOO的N5污染后95%coverage=.91603，已否定该方案作为基线，失败原输出 [statistics_parallel_loo_failed.json](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/evidence/statistics_parallel_loo_failed.json) 保留。N1..3缺少外部身份信息时不自动判坏；污染后可能完全不覆盖真值，这是能力边界。不同污染幅度、多污染、真实暂现源仍需真实/合成针对性验证。

当前生产静态差异另见研究报告：spectrum_integrator中旧λ积分是具名模型惯例，不能按新photon单位直接解释；小N百分比接受带不具平移等变性；sky floor-bin拟合与bilinear求值不共享basis。修正科学定义不代表本轮改动这些C++。

### 5.1 稀疏精度与Export的独立闭环

[sparse_variance_reference.json](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/evidence/sparse_variance_reference.json)从25个actual-scene正方差控点构造局部球面gnomonic三角插值。log平面5000查询一次再现最大误差1.61e−15；外部δ/Hessian界下平面/二次真值各5000点均被界包围，最大误差/界约.949/.960。图外返回unavailable；缺曲率先验的C∞ bump在全部控点为0、查询值可任意大，独立证伪仅凭“光滑”即可认证稀疏场的主张。普通float插值实验不是机器区间证书，不混入真实photon covariance。

[export_independent.json](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/evidence/export_independent.json)将真实叶内−2/0/+3信号导入2×2偶数尺寸WCS，CRPIX=(1.37,1.12)、RA回绕、镜像非对角CD。四百万独立外部WCS/成员detector积分给16量的Hoeffding联合失败概率≤1e−6；全/部分支持的面积、积分与均值区间均与reference相交。部分支持像元面积约30.08%，正确支持均值−2、错用全像元分母−.6015。200000次Gaussian covariance模拟最大偏差2.36个抽样标准误；显式双和与同一合并leaf矩阵差5.55e−17。拆同叶.35/.65后假独立会只报正确白噪声方差的.545。

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

历史研究阶段提出的“先额外独立审核、再讨论重构分支”门槛已由最终设计独立复核承接，当前状态见页首与最终审核记录。上述物理假设仍需真实数据支持；历史研究证据不构成新实现修复或科学产品验收。

## 8. 第二轮：科学预算、严格快速路径与唯一 WCS 合同

### 8.1 数学闭合与有真值的实际计算

`round2_science_budget.py/json` 用精确有理数实现面积→w/F/N/B/R、分式盒极值、科学预算反推、输出互Cov和病态BLUE求解包围。八类场景包括signed、1e9亮星/暗背景、弱扩展、强梯度、partial及极低支持，保留逐次未达标记录；58个已知等面积chart矩形/并集目标叶覆盖RA0、face0/1公共边和北极apex。后者**不是TAN detector fixture**，其面积真值由独立矩形裁剪给出，不能用它们冒称TAN/SIP解算已测。传播总成本约.4秒，非几何耗时。

| 场景 | 目标绝对B误差 | 实际B误差 | 确定预测界 |
|---|---:|---:|---:|
| 常场、Ω精确 | 1e−12 | 0 | 0 |
| 常场、Ω区间 | 1e−5 | 8.41e−7 | 3.37e−6 |
| 正负场 | 1e−5 | 7.19e−8 | 3.94e−6 |
| 1e9亮星/暗背景 | .01 | .00046992 | .00926527 |
| 弱扩展结构 | 1e−10 | 1.22e−12 | 3.38e−11 |
| 强梯度细结构 | .001 | 8.16e−6 | .00034818 |
| partial支持 | 1e−4 | 3.45e−6 | 8.14e−5 |
| 极低正支持 | 1e−4 | 2.76e−6 | 6.51e−5 |

分式盒解与三组各64顶点穷举逐分数相等。signed孔径方差真值 `.8369525738`，错误独立输出方差 `4.9512278532`；九个Cov元素和孔径的实际几何误差均被展开式界包含。BLUE初始几何扰动证书ρ=1.272，正确未决；面积半径缩小100倍后ρ=.01271，再100倍为.0001271。由此确定：几何界不当成额外iid噪声；同一R和组合算子贯穿SNR、sky、coadd、Export，求解/选择随几何变化时不能只冻结中心权。

### 8.2 原 tiny_boundary_sliver 已判明及独立新反例

`round2_original_sliver_refine.py/json` 从独立球面边界积分证明原282/293各为 `[1.7943443981693295e−14,1.7943443981693300e−14] sr`。原A的15001节点、约8秒仍含零并耗尽；B在31941节点、最终计时约39.5秒达到1e−16sr全drop预算，各叶给 `[1.7920708298562244e−14,1.7966119907953488e−14] sr`。另一次先行运行约51.7秒单列保留；不把计时波动解释为算法改进。旧包含零的界没有被删掉，也没有被倒写为已证明正支持。

`round2_sliver.py/json` 独立构造N=1、RA45°顶点附近的新TAN源：严格不交、纯切触、普通正片、tiny正片，以及±1e12信号。用 `dφ dz` 解析根区间和原函数得到真值，另给正内部矩形下界。初始A/B预算失败保留在 `round2_sliver_budget_pilot.json` 及加载源码快照；新的普通正片约4.712388704684e−8sr，两算法均证明正；tiny约1.17809724505e−14sr，初始界仍含0。

正式判据是：有可靠下界才称正支持；已有背景正支持时，小片的最终科学影响在容差内可保留 `bounded_undecided`；超预算必须细化或明确 `science_unconverged`。单独的未决片不发布归一B。±高亮度反例和同一几何界下不同科学停止状态实际执行，而不是列为TODO。完整推导见研究记录 `ROUND2_SLIVER_RESEARCH.md`。

### 8.3 已测 C++ 微原型与独立审核

`round2_affine.cpp` 是标量MPFR有向超越运算、binary64逐运算外包、GMP精确有理裁剪的独立cut-cell原型。主公式和证明在[球面规范 §6](SPHERICAL_DRIZZLE.md)。新算法以区间一阶导数的均值余项夹住真实HPX连续坐标，再解析积分内/外多边形；不是无界四角法。`round2_performance_final_kernel.json` 为最终22组输入的完整实测，包含失败；`--leaf`可只按指定叶请求停止，其他叶的原区间仍返回并单列全叶宽度，不能扩大成功范围。

实测覆盖0.19″至413″原生尺度、N=1…2¹⁸、f=.57…1、TAN/弱强SIP、最多56接触叶、双跨极点、RA/face/转折及256相邻像元。subleaf约.2ms/像元；常规TAN/弱SIP约5–6ms；多叶约75ms；强SIP约.35–.43s且15/16达到所请面积预算；跨极点1001节点耗尽并返回all-sky spill。1e−2→1e−4宽度扫描约7→110ms；1e−6/1e−9在3201节点耗尽。256像元tile导数缓存单线程收益约9–11%，双线程约1.8–1.9倍；进程峰值工作集约5.5–8.1MB（含运行时和整批结果，非纯几何heap）。这是单机微测与特定误差请求，不是千万像元真实吞吐承诺，也不是跨算法同精度speedup。

独立审核实际运行：

- `round2_affine_independent_review_final.json`：新CD=.002°的严格不交、touch、tiny，三个解析真值；touch C++ `[0,4.21e−29] sr` 包含0，tiny正区间约 `[1.17809711e−14,1.17809738e−14] sr`。独立A/B在同一新输入也有正界，原始全源资源失败未抹去。
- `round2_affine_sip_review_final.json`：额外二阶shear `X=c(u+.07v²),Y=cv`，det=1、有显式逆及侧界余量，证明局部sliver解析真值不变；三路均包围真值。C++极严全源宽度请求未达到，不能只看tiny叶的正下界就宣称全源成功。
- `round2_cpp_external_membership_final.json`：审核代码不读C++实现，仅消费标准header/request/result；22构型共约147万外部Astropy/WCSLIB/HEALPix点，未发现候选遗漏。iid面积对所有case/全12N²叶联合失败概率1e−4，均相容；独立70位无SIP总面积全部落报告界。采样是置信对照，不是区间包含性证明。
- `round2_high_order_focus_review.json`：N=2²⁹单叶极小像元用外部成员和独立B、90位解析面积对照；宽像元候选超限明确返回含真总面积的spill；聚焦SIP叶0约385节点/.10秒达到1e−20sr，而其他叶全宽约5.18e−12sr，成功作用域准确保留。

边界修正及失败身份全部保留：初版半宽/面积累计有潜在一ulp漏包围，`round2_performance_initial.json` **撤销bounds资格**，原源码在快照；之后binary64位移半径和spill改逐运算外包。Windows long改int64_t、候选数用除法先限额；大FOV独立外包可产生不可能的赤道格对，按真实 `|a−b|≤N` 排除，消除uint64−1假叶标签。近奇异CD还有确定负例：精确正det却区间符号跨0，旧absdet分支在budget_exhausted下报告上界仍小于真实正面积，证据 `round2_cd_domain_underbound.json` 撤销bounds资格。最终代码显式拒绝；finite输入、SIP单射/60°域与cache工作上限都已有负例。各旧实跑绑定原源码快照；最终两组独立复核重跑C++并明确复用未变的A/B输入与运行，不冒称全部A/B又执行了一遍。

特别记录FITS卡序列化反例：一个已规范化binary64 CD经Header卡再解析后改变约6e−15相对值，极精确区间对比因此不相交。`round2_high_order_focus_header_serialization_failure.json`保留失败；正式同模型复核用标准公开WCS字段保留精确CD值。比较必须锁**解析后的同一数值模型**，不能将FITS卡写入前/后默认当作完全相同输入。

### 8.4 科学精度与模型合同的交接

`round2_cpp_science_bridge_final.json` 将当前C++实际面积界接到完整B/R/Cov，Ω/A由另一实现的60位区间TAN球三角式求出；具名积分信号约对应1e9与−2面亮度。subleaf例达到.001目标，但常规/粗分辨率的高对比输出仍明确未达标。π/12 quarter-face及π/3 full-face给已知逐叶真面积，从而同时核对真实信号和Cov误差。不能把性能扫描的1e−4面积proxy当成最终高动态科学精度。

`round2_focused_budget.json` 把±1e12原生积分、.01分配通量容差反推逐叶面积请求；binary64端点原型6401节点仍未达标的记录保留。由此必须区分空间细化与数值精度升级，不能靠再加同精度节点掩盖精度地板。`round2_mp_affine_fallback.py/json` 已实际完成高精度精细路径：60位区间端点保持到精确Fraction裁剪/矩，再向外量化为2^-256共同dyadic ledger。相同±1e12信号，TAN33节点约.06秒，预测通量半宽.002222，实际误差约.000356；二阶SIP q=.07在14089节点约29–33秒，预测界.002576、实际误差约.000077，均小于.01并包含独立真值。源A另行有界积分并传播，9节点失败与有理分母膨胀的未完成尝试保留。局部reference的准入与inactive约束已证明，不冒称它已实现一般投影/任意SIP的完整高精度内核。

WCS唯一合同已在[观测规范 §3、§7](OBSERVATION_COORDINATES.md)冻结：显式ICRS二维TAN[-SIP]、forward阶数0或2…9、degree头、规范化LONPOLE、可证单射域与60°数值范围、非法/unsupported行为及 `WcsModel/FootprintProvider` 扩展。Bayer真实收光域、PRF及G C Gᵀ、完整逆/导数/g/曲边、所有公共类型的单位/寿命/错误均定义。`round2_wcs_contract.json` 的7个全新构型2121前向点对外库最大1.403e−15rad，逆最大2.201e−9pixel，另2000个crop/bin/flip/transpose/Bayer点及独立面积/反例。新发现精确北极省略LONPOLE应默认0°；一律180°会错约6.735e−4rad，已入规范和反例。

### 8.5 复算、证据身份和裁决边界

主要新增入口（从仓库根运行，输出文件中的源码/输入身份必须一起核对）：

历史复算命令：仅在固定c5fee09fe的独立历史checkout运行，当前工作树没有这些脚本；不是新实现验证命令。

```text
# HISTORICAL_CHECKOUT_ONLY: c5fee09fedb97ccbe39cb47ea0985e0d8a339db2
python 实验/science-foundation/oracles/round2_wcs_contract.py
python 实验/science-foundation/oracles/round2_science_budget.py
python 实验/science-foundation/oracles/round2_sliver.py
python 实验/science-foundation/oracles/round2_original_sliver_refine.py
python 实验/science-foundation/oracles/round2_performance.py --output 实验/science-foundation/evidence/round2_performance_final_kernel.json
python 实验/science-foundation/oracles/round2_affine_final_recheck.py
python 实验/science-foundation/oracles/round2_high_order_focus_review.py
python 实验/science-foundation/oracles/round2_cpp_external_membership.py
python 实验/science-foundation/oracles/round2_cpp_science_bridge.py
python 实验/science-foundation/oracles/round2_focused_budget.py
python 实验/science-foundation/oracles/round2_mp_affine_fallback.py
```

C++构建命令和编译器/可执行文件hash在性能JSON的build中；需GCC/OpenMP、MPFR、GMP开发库，本机为MSYS2/Windows amd64。源码不含生产头或生产对象，临时exe不入库。`evidence/round2_manifest.json`记录新文档/研究文件字节及历史源码快照对应关系；第一轮 `manifest.json` 继续作为第一轮交付身份，不重写历史文档hash。

**DERIVED**：面积到signed分式估计、支持/覆盖区别、Cov和求解扰动、affine-strip包含及WCS规范化/导数。**INDEPENDENTLY_TESTED**：上述有限解析真值、A/B重算、C++独立实现/成员及实际微测。**ASSUMPTION**：所用IEEE/MPFR/GMP/mpmath区间原语的正确性、固定WCS/PRF/PSF/response与噪声物理模型。**REQUIRES_REAL_DATA**：§7的真实物理模型及吞吐/星云/孔径/coverage问题。

当前科学与工程设计已完成最终独立复核，状态及范围以页首和最终审核记录为准；历史#111的待外部复审/未建分支措辞不是当前启动门槛。新实现仍须以公开合同自行生成真值，检验预算反推、高精度fallback、边界支持和真实模型输入身份；这些属于实现与产品验证，不把历史研究吞吐转记为生产通过。
