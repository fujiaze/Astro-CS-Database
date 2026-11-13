# 03｜Mosaic：恢复main成熟多面板Coverage/Sampler/Sky/Rejection/Integration

**归属**：sky.controls、sky.fit、coadd.select、coadd.weights、coadd.integrate，及 V2 `src/workflows/mosaic.cpp` 的数据计划。**前提**：[02](02-GEOMETRY.md)已建立共同高阶 NSIDE/footprint、同一估计量/单位/response和可重开Normalize产品；否则不可借“多帧”宣称真正多面板科学马赛克。

## M-00 旧生产已分层，而非简单叠加

main `lib/infrastructure/scheduler/src/module_adapters.cpp`正式调用：
- `p2_op_coverage` → `lib/algorithms/coverage/src/coverage.cpp::p2_coverage_build`
- `p2_op_sample` → `sampler.cpp::p2_sample_controls_cached`
- `p2_op_upm_fit` → `sky_plane.cpp::p2_sky_plane_build_joint`
- `p2_op_upm_apply` → 当前有效背景模型的逐帧应用
- `p2_op_reject` → `rejection.cpp::{p2_reject_plan_resolve_n,p2_reject_stack_ex}`
- `p2_op_integrate` → `integrate.cpp::p2_integrate_pixel` 与权重验证
- `p2_op_write` → 生产AIO HiPS SIGNAL发布

main里名为“upm”的旧节点**不等于**现行联合求解必须使用 `upm.cpp` 的所有老F1–F6实现；实际生产拟合通过 `sky_plane_build_joint`。这点直接决定迁移代码要抽什么，不允许把死代码当生产功能。

V2已有 `src/sky/{sky,sky_controls_process,sky_fit_process}.cpp`、`src/coadd/{coadd,weights,coadd_select_process,coadd_weights_process,coadd_integrate_process}.cpp` 和 `src/workflows/mosaic.cpp`；先比较这些现有实现与main完整P2功能，**缺什么补什么，已有更严格的科学算法保持**。V2若已有 sky GLS/协方差但没有真实多面板controls，不能以解算器存在宣称功能完成。

## M-01 跨面板Coverage与稀疏空间规划

旧 `coverage.cpp` 的 MOC union 使用NESTED子父聚合；会按输入最小 leaf order降级目标阶以避免上采样伪细节。这是它的安全防退化逻辑，不是“科学目标只需最低阶”的推荐。V2应在Normalize之前已统一高阶，Mosaic不得再静默粗化。

施工任务：
1. 先从每幅不同M1–M6真实WCS的有效footprint建立集合 `U=∪U_f`、每个通带独立的sky support、面板重叠图(边含真实相交角面积)，允许不连通、全饱和孔洞、坏pixel和边缘区域。绝对不可用一维NESTED序列相邻代替二维/球面空间邻接。
2. 复用旧MOC有序合并、tile检查、NESTED ancestor位运算/稀疏区域扫描，使用V2唯一`Index/SupportMap/Storage`载体；必要时从旧`coverage/src/stage2_common.cpp`抽取有效tile裁切/地址转换。统一结果只含真实受支持leaf/Tile，不生成空天球占位。
3. 跨T2/T3/不同滤镜响应不能仅依 filter字符串判断**同一物理被估计量**。各通道内名义相同滤镜，透过率/QE/色项/PSF仍需严格兼容证据；科学上不可共同估计时分组产品。
4. 每帧Support/coverage与真实R一一对应，单帧一个输出对象可跨进程重开；重复输入源hash必须拒绝或明确同一物理曝光不重复计权。
5. 最小证据：同一面板三帧、真正有球面重叠的两个**不同**面板（不能仅凭M编号推断）、多面板六块连接/断连、空覆盖/全饱和、边角及极面；给出面板并集/交集面积和叶数、Sky节点图、内外侧mask重合度。
6. 性能：旧`coverage.cpp`是tile/MOC有序稀疏处理；保留O(touched_tiles log N)级集合思想，空间域按Tile/HEALPix region分区，输入元数据扫描一次，不能在每一输出叶轮询196帧所有产品或构造196×全叶矩阵。

## M-02 Sky Controls：复用主分支真实Sampler与空间屏障

旧 `lib/algorithms/coverage/src/sampler.cpp` 的实际`p2_sample_controls_cached`读取signal/support tile、沿空间窗口生成sky controls、带星晕遮罩候选与有界并行。V2 `src/sky/sky_controls_process.cpp` 应承担唯一空间控制构造。

1. 严格保留每个control由哪个帧/面板/时间/位置/response与有效像元构成，并记录失效/饱和/星云梯度/星点掩膜、PSF匹配条件和控制统计量自身的误差。采样不改变已支持叶的真实信号定义。
2. 复用旧每frame×tile一次读入和局部窗口有效数据缓存/读取顺序，真正有用的源/星晕排除策略需通过仿真证伪；旧 sampler.cpp 注释中某些halo分支当前**未消费**，不能把“有参数/算法”当已运行。
3. 在全分辨率输入上选合法controls，空间分层可取稀疏控制点，但不能把control-grid的稀疏分辨率误用为最终球面leaf订单；控制统计在必要的响应/PSF匹配空间比较，不把不同夜天空差调成信号。
4. 所有控点先在全局图组装一个可辨识的背景问题，再允许各Tile分块独立coadd；如果每个块自己拟合一个sky零点，跨块必然可能产生接缝。必要的全局系数是小型屏障对象并缓存。
5. 性能：逐tile窗口、短寿命只读TilePair、共享LRU/固定容量、批量预取+有界Pipe，控制提取避免对同一frame N次哈希/重复打开FITS。记录Ncontrols、重复读取次数、bytes/read、cache_hit、seconds/control和峰值窗口大小。

## M-03 Sky Fit：只迁可证的旧Schur/IRLS/组件gauge，保留新科学校正

旧核心 `lib/algorithms/coverage/src/sky_plane.cpp` 实际实现公共天空面 + 每帧加性δ的联合正规方程、Schur消元、Huber IRLS、连通分量gauge、部分按64列分块Cholesky、按帧固定次序并行装配；该核在生产被`p2_sky_plane_build_joint`调用。V2已有 `src/sky/sky.cpp` 数千行同类模型和协方差概念。

**迁移方式**：
1. 逐式对拍旧Schur块/约束/参考系与V2 `fit_relative_sky`，若V2已经具备正确求解就保留它，只吸收旧的稀疏分块、因子复用、排序与条件数处理；若V2缺少成熟的control graph、stencil/IRLS分支，可从旧纯C++求解核抽取适配到唯一V2 solver。**禁止保留两套互相不一致的Sky拟合器。**
2. 统一条件：一帧加入已乘性光度归一空间后才做真正加性sky差模；不存在一个“对所有夜晚有效的常数平场屏幕”。V2 S1主带8.43%的接缝和报告承认残余乘性flat screen未去，意味着最终四通道不能直接宣称无缝：需定向区分空间乘性、加性Sky、PSF/响应差、几何支持变化。
3. 正确的天空修正算子 `P=I−EKT` 应对同数据拟合的噪声共享项施加 `P C Pᵀ`，含负交叉协方差；随机根与control噪声身份不能被已拟合的β覆盖成独立白噪声。条件不足标unknown，不拿预估数值当严格covariance。
4. 断连图每分量只能定相对gauge；不能强行“拉平所有断连组件”而声称恢复绝对大尺度天空。特别M42星云共同低频结构不得作为回归基的待扣空天光。
5. 数值病态不能简单靠ridge吞；旧#80 2×Hα+1×Red 的non-SPD包含模型不可辨识/尺度问题，需对真实rank、gauge/nullity、KKT与独立SVD/QR对照；同通带依然要排查数值。
6. 性能：全局控制问题只存必要稀疏/低秩块，保留旧Schur与按帧分片固定归约、blocked Cholesky思想；但旧 `std::thread` 和 `OMP_NUM_THREADS` 来源不得照搬为第二线程池，需接V2唯一ThreadBudget/Task、让小系统串行，避免N×N超售。记录控制数、矩阵维数/秩、fill-in、求解/装配时间和临时内存。

## M-04 Rejection：尽量移植旧算法群，但重审小N行为

main `coverage/src/rejection.cpp` >3000行已有 RCR、Generalized ESD、MAD、Winsorized、Averaged Sigma、LinearFit、Percentile、MinMax、Extreme-Value-with-prior 等明确的方法核，正式由`p2_reject_stack_ex`调用；V2 `src/coadd/coadd_select_process.cpp` & `coadd.cpp` 不能只实现粗糙sigma-clip后称“恢复原排异”。

1. 将每种被生产实际消费的方法拆为纯`candidate stack → select mask/diagnostics`，保持方法ID、适用N、归一/稳健尺度的科学语义。要优先复用旧已有分位数/稳健求值/数值边界；没有足够真值或场景模型的方法先具名unsupported，不应悄悄自动选择。
2. **旧N=4/5 percentile + |median| 尺度**在靠近零/大加性背景时存在错误剔除的已知反例；小N≤3应保守不自信sigma剪裁。升级V2默认策略时由实际注入TPR/FPR与真实M42形态保真选，不仅按旧代码的if链复刻。
3. 输入DQ、饱和、不在公共支持、PSF/response不相容先隔离；异常宇宙线/卫星/坏点可通过残差/邻域统计提出候选；对真实天体结构和真实瞬变误拒绝必须统计。
4. 排异决策至少写入每帧每区域的接受/排除原因、`N_candidate/N_valid/N_accepted/N_eff`；面积/源像元/单帧重复计数不混为曝光数。
5. 性能：单像素或小批stack局部scratch复用，固定组长度可优化小向量；tile并行、算法选择/阈值可缓存，同一leaf不要调用动态配置解析N次；独立seed与归约固定，按N分层测µs/stack。

## M-05 Weights/Integration：旧数值内核与新协方差科学分开

旧 `coverage/src/integrate.cpp::p2_integrate_pixel`、`integration/phase2_integrate/src/{weight_chain,variance_propagation}.cpp`；V2 `src/coadd/{coadd_weights_process,coadd_integrate_process,weights,coadd}.cpp`。

- 先固定上一节Selection，然后从实际scene的可用`V_B`、`Γ`、共享根求权，必要时用带条件的GLS/BLUE；**旧以参考点源Fisher权替代scene IVAR的捷径不可复用**。可辨识不足时选择清楚声明的保守权重，不发明方差。
- 正式合成与单帧信号同一面亮度标度/response，不把Hα与R当同天空面亮度强制平均。零值/合法负值保留，未覆盖NaN、坏像元不虚构贡献；每个块只归约实际covering frames。
- 优先移植旧像素归约、确定顺序、权重合法性、双精度累加/对数稳定策略；对V2的CovarianceAction/sky控制传播保持链式可组合，不再在每个tile调用dense covariance。
- 按空间分区/响应组和全局Sky屏障两步执行：final weights plan冻结→每块有界读帧→sample selection→权重→signal支持/uncertainty→原子保存。峰值复杂度依“当前块的有效候选帧数×块元素数”，**不是196×全域叶数**。
- 验证：解析2–N帧常量和梯度天空、已知不同噪声/相关根、断连组件、真实重复曝光贡献顺序、SNR与扩展源功率谱。每通道明确性能模型和对比。

## M-06 科学上真正的接缝验收

不能拿“相邻NESTED编码段差”“单M5帧覆盖度边界”替代面板接缝。对每对确实天球交叠的面板，在共享目标WCS中：
1. 建立有向双侧天空邻接带（球面HEALPix邻接+目标输出平面差模）、重复背景控制与PSF匹配统计，记录帧数、面积、有效权重、局部FWHM、multiplicative slope与additive offset。
2. 分别量化$\Delta B$（有方向的偏置及梯度）、按噪声归一化的差、星点通量/形状、共同星云低频场。对过亮结构mask时也报告未测区域，不因掩掉问题最重处就说整体通过。
3. 在注入真值中分别测试加性Sky与乘性flat屏，另注入真实M42-like星云大尺度梯度；不能靠导出羽化/高通去缝破坏科学低频。
4. 如果经证明T2与T3或夜晚存在不可辨识乘性差，需要先改善校准/相对光度模型或输出有条件分设备产品，不能将残余屏留给用户视觉自己处理。
5. 用M42不同面板实际重叠至少两块做门槛，之后扩大至六块、全部196帧；通过的既有Tile/Normalize缓存必须复用。完整质量判据详见[07](07-VERIFICATION.md)。

## M-07 本组交付/性能约束

交付可追溯的 `SkyFit`、`Selection`、`WeightSolution`、`Coadd` 数值、统计和unknown状态，真实多面板覆盖图，跨组件的全局gauge，外部独立差模；同一固定输入一线程/多线程等价、没有重复计算完整旧P2管线，也没有用旧模块私建线程池。性能必须分开统计Coverage、Sampler、Sky(装配+解)、Rejection、Integration、HiPS writer的wall/CPU/RSS/IO，和main实际相同科学策略比较，不得混入不同NSIDE的巨幅工作量差。
