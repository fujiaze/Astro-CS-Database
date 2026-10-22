# 独立 Agent 真值协议与架构审核

本协议依据[独立证据规范](../../science/foundation/INDEPENDENT_ORACLES.md)，使用[公共 API](API.md)验证[统一对象](OBJECTS.md)和[执行模型](EXECUTION.md)。本阶段定义验证入口与审核条件；驱动器、生产实现及以下新实验尚未执行。有限历史证据保持自己的输入与源码身份，不转记为新核心证据。

## 独立性和输入包

独立 Agent 阅读五份科学规范、本目录和公开协议。生成真值前不阅读被测求解器内部，不 import 生产 coordinates/overlap/noise/sky/coadd helper；允许共享标准定义、公开schema、输入头、单位、source身份和通用第三方库。生产C++ API由进程驱动调用，驱动只转字段/数组，不做科学计算。真值生成与被测结果文件分目录，原始请求冻结并hash后再启动被测程序。

每个 case 包含：`case.json`（目标/假设/估计量/单位/condition/容差/正负例预期）；`input/`（解析后精确数值模型、块/hash、源与共享latent）；`truth/`（独立算法、解析式/高精度区间/随机生成器）；`request.json`；`observed/`（驱动原始response、stdout/stderr与资源）；`comparison.json`（每个泛函与失败作用域）；`manifest.json`（来源、版本、seed、trial分母、代码/库身份、依赖/命令）。失败追加attempt目录，不覆盖原始失败。真值代码不从observed中心系数反推输入或调整容差。

两类调用都必须可行：小型对象直接in-memory API；标准输入文件经 `acsd-contract`。独立 Agent 无需建立旧产品目录、旧module注册或旧Runtime会话。端到端生产验证另经三命令和正式发布产品，不能只测合同驱动器就声称生产路径已命中。

## 从物理观测构造合成真值

1. 构造 I_lambda(ω,t)：解析连续面亮度、带已知flux与SED的点源、真实低频梯度/暗结构、窄线与连续谱。明确天空大气项是否已在I中，避免双算。
2. 指定真实仪器响应、观测时间/站址/历元、波长PSF和PRF；独立积分 P_i 与波长/时间，得到非负电子期望。高斯用erf像元积分，Moffat/Airy用带尾/求积界的独立高精度积分。边界收集损失不重新归一。
3. 按独立Poisson photons、读出/IPC相关根、共享母版、Gaia谱及响应latent生成原始ADU。真实gain用于truth，测量输入可故意隐藏gain以验证unknown分支；扣sky后的负值合法。
4. 独立处理crop/bin/Bayer/G的足迹、核及G C Gᵀ，保留hardware/software读出区别；用已知空间α和共享训练数据检验一次应用与交叉covariance。
5. 将真实头、观测块、公开Response/NoiseModel/Footprint送入标准API。真值路径用解析或独立第三方WCS、detector域积分/等面积目标域积分构造目标量，不能直接把被测R输出当几何真值。
6. 对有噪样本重复完整拟合/sky/选择/coadd/Export，评价bias、方差、68/95%coverage和误拒率；另对固定选择/几何小矩阵测试线性恒等式，两种结论分开。

HST处理后图只可作带已有PSF和噪声的条件结构模板，额外退化需证明 P_HST*K=P_target；不能假装它是无PSF/无噪宇宙真值。真实HST/M42数据检验按科学规范单独开展。

## 必需验证矩阵

| 合同 | 全新真值与调用 | 必需反例/判断 |
|---|---|---|
| WCS与原点 | 外部完整WCS+独立向量；非整数CRPIX、镜像非对角CD、PC+CDELT、双极点、LONPOLE缺省、SIP2…9、RA wrap；forward/inverse/derivative/footprint | +1/+.5原点、只改CD不变SIP、背半球/根盒跨域、非ICRS/额外畸变/单射未决具名失败；roundtrip不能单独通过 |
| 真实面积和候选 | 独立dφdz解析楔、TAN球三角、SIP剪切、12face/面缝/apex/order29、触碰/tiny sliver；compute_overlap | 总面积闭合但漏叶、外部cone漏选、候选爆炸/spill、近奇异CD/无效bound；零命中不判面积零 |
| 科学预算 | 已知Ω/A/a的精确有理顶点穷举，另接实际几何；signed、±1e12、弱支持、亮暗邻接；build_drizzle/refine_geometry | area proxy达标但B失败、未决单片不造B、precision升级、单叶成功不扩大范围、序列化后重验、新R混旧K/权、共同A/Ω细化未失效全源 |
| 同R与噪声 | 新小矩阵显式R C Rᵀ、独立共享latent MC、signed aperture、PSD精确约束；apply/transpose/C/quadratic_form | 拆同源或同leaf假独立、未知项填零、C action假冒|C|、ridge改目标、相同V_F但不同V_B |
| Gaia/response | 解析多项式谱和窄线、共享source跨响应积分、已知空间α联合拟合/留出 | 积分精确却谱分辨率错、gain不可辨识、color-position rank缺失、空间α只星心应用、跨帧Gaia误差被重置 |
| 稀疏scene方差 | 已知log平面/二次u、外部δ/M、chart支持/holes、公开reconstruct | 无M的C∞ bump反例、reference控点冒充scene、域外nearest-fill、插值误差冒充photon covariance |
| sky | 已知共同梯度+差模、断连/孤立/共线控制、shared controls covariance、同数据MC | 公共常量/两斜坡不可估、异PSF/response伪边、只加参数方差、不同fit/eval basis、空间holdout误当独立 |
| coadd/selection | Γ解析BLUE/非负QP、共享floor、N1…7完整MC、变星/瞬变和多污染 | 混响应仍出具名混合且不直接共同均值LOO、同数据Γ不套外部固定Γ定理、negative BLUE不称物理通带、小N不猜坏帧、单步最大LOO与swamping、平移等变性及选择后区间 |
| Export/HiPS | 独立外部reader和真实成员/积分；奇偶尺寸/镜像/CRPIX/RAwrap、partial、−2/0/+3、tile方向/face6中断 | 部分支持用错全像元分母、二次pixfrac、同leaf分片独立、单图造variance、FITS卡精度导致不同模型 |
| 产品/资源 | 新进程重开完整图、shared根冲突/缺块、1/多线程、低内存spill、取消/写盘失败 | 仅运行内handle有效、外部依赖未声明、任务顺序改结果、无界队列、取消发布半成品 |

真值类型必须标 `analytic_exact`、`deterministic_enclosure`、`stochastic_confidence`、`empirical` 或 `conditional_model`。解析/确定真值要求被测区间包含真值；两份确定包含区间不相交即可反证至少一份错误，相交不证明两者都正确。MC按预先给定iid模型、样本数和联合δ比较，同时列二项coverage区间；不将MC相容或Sobol一致称确定包围。性能读数锁输入、容差、算术、源码、机器及成功范围，不用不同输入的时长求speedup。

## 架构独立审核与实施条件

独立审核者先逐项追踪科学量 → producer/type → API → consumer → 持久化 → 内存/并行 → 独立真值。特别检查跨阶段模型闭包、共享误差、同数据影响、geometry/sky/权重反馈、弱支持和取消寿命。审核应给具体文件/条款、失效构型、严重程度与必要修订；不以“模块齐全”或“文档链接通过”代替语义审核。

设计审核记录实际阅读范围、发现、修订和未验证项。设计合同可以在未编写驱动器前完成独立审核，但不能声称标准接口已可运行。架构仍缺定义的核心数据或交互必须先修订；仅性能未知、真实模型待验证等已明确能力边界可作为后续实验要求保留，不能用审批字样掩盖缺接口。

独立重构分支已从c5fee09fe建立；负责人接受本设计后才开始科学代码实现。顺序：公共对象/数值/坐标/驱动→校准/PSF/响应/共享误差→严格几何及同R→sky/coadd/Export→产品/CLI/GUI。每阶段配独立真值和负例；代码存在、编译、模拟、真实运行分别报告。

## 可交给独立实现者的最小验收包

每项先冻结输入和明确tolerance，再调用公开API；下列规模为首期必要案例集合，不继承旧测试或把所有历史构型机械复制为回归。一个case可以同时证伪多份合同。所有数值容差是fixture的目标，生产科学请求仍逐用户定义。

| case | 固定输入与独立真值 | 调用、判断与完整路径 |
|---|---|---|
| C01 | TAN切点(0,0)、非整数CRPIX、镜像CD、SIP三阶、极点/LONPOLE与Bayer变换；Astropy/WCSLIB绝对坐标；目标1e−10rad及1e−7pixel | make_wcs/forward/inverse/J/footprint；外库值必须落数值界或给合理声明近似；+1/+.5/只改CD负例必失败 |
| G01 | 12face等面积矩形及TAN球三角；新detector域高精度区间算法B，不import geometry；touch/tiny±1e12 | compute_overlap→build_drizzle→refine；面积真值在界，正支持只以下界证明，科学B绝对.01；work=1/maxprecision=64负例不伪PASS |
| U01 | R=[[.5,.5,0],[0,.25,.75]] sr−1，C=[[4,1,0],[1,9,2],[0,2,16]] L²；t=[1,−.5]，显式有理矩阵 | C输出[[3.75,2],[2,10.3125]]，孔径方差4.328125；使用Storage.copy/make_action/make_covariance/apply/quadratic_form及进程驱动；拆同源假独立必须被证伪 |
| F01 | Gaussian erf像元积分+常量sky、两个相邻源、均匀/空间α；counts/Gaia真实共享latent；解析宽度/通量/参数秩 | fit_psf/measure/integrate/fit_photometry/apply_scale；总flux有限窗不归一，跨帧shared floor不随K消失；颜色=位置rank负例、unknown gain负例 |
| S01 | 三帧共同平面+已知差平面、controls共享C，reference gauge；独立SVD小矩阵与显式PCPᵀ | controls→sky→apply，KH=0精度1e−10，公共坡不被扣；共线控制保nullspace，异响应不造边，空间holdout仍保交叉项 |
| W01 | N=1..7，固定外部Γ和解析BLUE/PSD/非负QP；nonnegative例Γ=[[1,2],[2,9]] unconstrained权[7/6,−1/6]、QP[1,0] | select/weights/coadd；清洁Gaussian+指定单污染全流程MC，各100000 trials，seed冻结；报告误拒/68/95coverage及精确二项CI，不从本矩阵外推真实宇宙线 |
| V01 | chart logV线性/已知Hessian二次，洞/外域，有限点bump反例 | reconstruct_scene_variance；有M/δ时真值在界，无M只能conditional且不产生跨叶C |
| E01 | 2×2偶数输出，CRPIX=(1.37,1.12)，mirror CD/RAwrap，leaf值−2/0/+3；独立detector积分、partial支持真值及外部FITS reader | build_export→publish→新进程open→export；支持均值/积分和外推量分开，同叶先merge，无二次pixfrac，FITS数值头量化入界 |
| P01 | U01扩展成跨tile共享root及sky的完整产品，另signal_only；故意缺块、hash冲突、重复exposure、unknown recipe | publish/open与Mosaic/Export；full无原FITS仍可全部调用/refine；signal_only误差unknown且equal；坏闭包不发布 |
| R01 | 相同科学输入1/4线程、H=32MiB/256MiB、cache off/on、writefail/cancel，frame和target块乱序完成 | 同科学content hash、误差/支持一致；实际peak/资源账本不越承诺；新旧revision重叠预留不足正确spill/失败，无新R混旧sky |
| X01 | 从物理Iλ独立积分的8张128×128原ADU、已知仪器/PSF/noise、3幅sky，两个响应组 | 全新三命令经真正磁盘产品；无WCS组必须实际blind solve；GUI只作协议冒烟，不替代科学路径 |

U01真值须由独立实现者自行有理数重算，本表数值与算法不得相互自证。truth可以共享数学定义、公开头和通用库，不能读取R0输出后构造“真输入”。C01/G01可包含多个分支小例，避免一条assertion一个重复fixture。

## 性能与资源验收

固定两组输入：P-small=256×256原生像元、order18、TAN f=.63、均匀与1e9亮点对比，P-large=4096×4096×8帧、order18、跨tile/面缝及2阶SIP，实际tolerance分别B=.01及Cov=.001（具名模型单位）；第二组可分任务在双平台执行。首次实现记录机器、版本/编译器、源hash、后端、CPU配额、预算、读取字节、wall/CPU时间、peak RSS与payload、spill、nnz、precision升级和未达标作用域；先给正确性/停止，再给吞吐。不得以失败输入更快为优化通过。

同输入同精度比较baseline/优化：若科学结果/误差/支持不同则比较无效。首期性能门不是未经实跑的固定ms：要求资源有界、增长趋势符合EXECUTION的实际nnz/秩/精度模型；P-large工作集H≤1GiB时无需全图常驻且可完成或明确资源拒绝。发布吞吐目标由首次有效双平台profile后负责人评估；数值失败不能以“性能达标”遮盖。统计MC和真实数据另计，不混几何微benchmark。

## 独立设计审核的交付标准

正式记录见[REVIEW](../../audit/REVIEW.md)。审核者不参与编写，不从旧src获取暗知识，必须尝试只用头合同和文档构造至少一个完整Normalize→持久化→Mosaic→Export追踪，以及U01式矩阵调用。每项问题列文件/符号、反例构型、影响和最小修订。修订后的新审核者再执行同标准；未决P0/P1编码阻碍必须继续补齐。链接/编译/schema检查只能辅助发现结构错误，不能作为语义通过的依据。
