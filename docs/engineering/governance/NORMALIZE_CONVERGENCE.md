# Normalize 文档权威与证据归属

本页说明阶段一设计的唯一归属及科学结论的证据边界。最高设计固定产品和用户目标[1]；Normalize 科学正本固定估计量与因果链[2]；工程架构和数据合同固定实现责任与接口[3,4]。本页不另立科学模型，也不作为运行任务包或研发审批机制。

## 1. 每个主题只有一份正本

| 主题 | 正式归属 | 下级资料的用途 |
|---|---|---|
| 三命令、产品、用户自由组合 | 最高设计[1] | 解释功能，不能改写阶段边界 |
| 完整 Normalize 科学链 | Normalize 科学正本[2] | 专题分册细化校准、天测、PSF、测光、噪声、Drizzle |
| 生产 DAG 与模块责任 | Normalize 工程架构[3] | detail 页说明实际接口和消费方法 |
| 类型、单位、端口、能力与 sidecar | Normalize 数据合同[4] | 实现规格[5]定位 C++、配置和迁移；schema 只描述机器形状 |
| 目标与代码的差异 | 科学实施差异地图[6] | 集中记录可追踪符号和边界，导航页不复制全量待办 |
| 真实执行、反例与测量 | 对应实验单元及保留证据 | 说明特定输入、源码和条件的事实，不成为第二套科学定义 |

`docs/detail/normalize/pipeline.md` 是实现导航，`docs/detail/common/unified_model.md` 是公共对象的消费说明。模块登记页、算法页和机器注册表都不能凭冻结措辞或测试标签反向定义 F_ref、WCS、SNR、通带准入或最终产品。

## 2. 统一的科学与工程解释

| 问题 | 采用的解释 |
|---|---|
| 原始头部没有 WCS | 只是文件属性；以 IPV 实际尝试和逐帧结果判断 solved/unsolved，不能据 census 要求外部解算 |
| 解算候选与科学星表 | IPV 私有 solver_detections 用于几何求解；science_sources 在完整 WCS 后由 Gaia 引导，两者职责不同 |
| block 与设备/通带 | block 是母版和模型复用范围；不同设备/滤镜由用户决定组合，每帧响应须如实绑定，不能由首帧标签替代 |
| 光度与单位 | 线性标度施加一次，随机协方差按二次幂变化；BUNIT、参考通量、application ID 与真实数值配对 |
| 参考源与摘要 | 共同参考是具名 SED/光度约定或显式单位通量；检出星中位数、逐源 proxy 和兼容帧级 SNR 不自动成为 canonical controls |
| precision 完成时间 | noise 先准备平面条件模型，Drizzle 后由同一 noise 库使用实际 R 完成球面 h/C/Z、V_F 与摘要；不设置调度环 |
| 点源与面亮度精度 | I_F=SNR²/F_ref²；w_B=I_F/T，T=V_B/V_F 有量纲且条件相同；不默认 T=1 |
| 母版与共同误差 | Jacobian 和共享误差属于目标模型；没有测量或实现的项明确 unmodelled，不作为零或完整精度 |
| 信号与精度能力 | 合法 signal 可以伴随 precision unavailable；损坏模型则是数据错误，不能以同一 unavailable 状态掩盖 |
| 各阶段发布对象 | 阶段一逐帧 signal 与必要科学辅助对象；阶段二最终单 signal HiPS；阶段三平面 signal FITS |
| 数据载体 | Runtime typed 对象和正式 AIO spill/cache 属科学路径；开发 snapshot/perf 是旁路，不能供下游取数 |

这些解释由相应正式正本承载。发现可验证反例时提供原始证据，由科学负责人修正统一定义；实现方不能独立另定参考通带、SNR 或误差模型。

## 3. 状态词描述事实

文档明确规定某对象，称为“目标已定义”。源码有函数或类型，称为“代码存在”。调度分支实际连接到它，称为“静态接线存在”。有命令、输入和输出记录，才能称为“在该构型运行并观察到产品”。独立解析、外部实现、前向模拟或重复观测支持误差范围时，才陈述相应的独立科学支持。

这些事实不能互相替代，也不是依次审批的关卡。例如 serializer/reader 都存在，不代表 reader 已核验所有引用；R 与 h/C 可写出，不代表模型的 PSF、截断、单位和母版误差完整；退出码成功不代表覆盖了所有预期输入 occurrence。

## 4. 已有证据支持的范围

Normalize 已有真实八节点调用、球面参考精度生产调用和 sidecar 发布调用。当前具体实现限制由差异地图[6]记录，不能沿用“只有 Catalogue writer、没有 canonical producer”或“所有类型/reader 均不存在”的概括。

独立复核[7]记录一帧原始头无 WCS 的 IPV 尝试与解，也明确该帧未施加测光。它支持这一帧确实进入自带解算器；不能推出全部无 WCS 帧成功率、独立 Gaia 留出残差，或 applied photometry 后参考精度的正确性。头部 census 继续只作为输入属性记录。

局部验证日志[8]保存异方差 GLS、背景 nuisance、秩亏、标度、leaf 0 和 Monte Carlo 对照，以及对应构建命令/结果。该证据支持给定模型下的数学修复，不自动覆盖真实逐帧响应、空间积分 PSF、完整 reader、发布事务和多平台 E2E。本文只查阅记录，没有重跑。

其它研究单元的 Gaia XP 残差、几何分割误差、插值留出、连通背景和 ON/OFF 比较按各自输入/量纲/方法继续保留。它们不因文档更新被删除，也不因局部结果相符而升级为全部阶段一科学链的独立支持。

### QR、像元积分 PSF 与因子 v2 的证据

第二批独立复核[9]读取了 QR、PSF 与因子 v2 的源码、验证程序和生产日志。生产改动分别为：以 Qᵀ 的列主元 Householder QR 求保留行空间，分离背景秩与通量可辨识性；复用 PSF 库的单位总通量像元积分；使用 `local-factor/v2` 分离源方差与派生协方差的形状和单位。实现应保留，不能因前一基线尚未具备这些路径而回退。

报告记录一帧精度段从约1594秒降至77秒，整命令墙钟从约1655秒降至138秒；两种加速分母不同。修正 PSF 归一后，显式 F_ref=1000 ADU 的条件结果约为 V_F=5439.9641、SNR=13.5582，与未归一模板下约141的结果不能混作同一个物理标度。本文查阅证据，没有独立重跑这些时间与数值。

该复核同时指出：近共线通量方向的信息相减有消去误差；QR 主元阈值与奇异值阈值不等价，主元比平方不是严格谱条件数；逐点包络不能共用帧最小值；因子读侧还需校验基础线性单位；表示说明文本应与正式格式一致。求积测试中只创建大网格而未调用/断言的部分不计作完成验证，相邻阶数差也不是严格误差上界。问题的当前实现状态在差异地图[6]更新，过程与失败证据继续留在实验单元。

子空间截断、模板求积及条件协方差误差的集中数学问题与推导保存在[数值精度推导记录](../../../实验/normalize-precision-p5/NUMERICAL_PRECISION_DERIVATION.md)。算法补充页收拢时的历史读数与来源保存在[文档证据说明](../../../实验/normalize-precision-p5/DOCUMENTATION_EVIDENCE_NOTES.md)。数学条件界、历史观测与当前生产验证分别判读，不互相代替。

真实生产链盘点[10]仍区分 typed 排序边与实际工作文件传递。空间 PSF、SED 参考、空间 alpha、母版共同项、独立 signal 扰动、多平台及开发开关对照未由这三个局部修复自动完成；少量合法帧出图也不代表完整帧集或留出天测精度已验证。

## 5. 文档维护与实施范围

工程与 detail 随生产代码更新接口和限制；科学正本保留物理定义的唯一解释。NORMALIZE_DATA 和实现规格已经决定八节点、参考来源、球面完成边界、唯一 frame_science.json 和能力状态，无需再由实现方自由设计。

任务包用于组织工作，实验记录保存过程，均不取得正式正本的权威。阶段一文档完善不构成切换 Mosaic 权重或跨阶段算法重构的授权；最终发布与用户视觉评价由项目负责人决定。任何收尾状态只表达实际完成和仍缺证据的范围。

## 参考文献

[1] [ACSD 最高设计](../../ACSD_DESIGN.md)。

[2] [Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)。

[3] [Normalize 生产链与模块责任](../architecture/NORMALIZE_PIPELINE.md)。

[4] [Normalize 数据合同](../contracts/NORMALIZE_DATA.md)。

[5] [Normalize 接口实现规格](../../detail/normalize/contracts.md)。

[6] [科学设计与生产源码差异地图](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

[7] [球面参考精度独立复核](../../../实验/normalize-precision-p5/INDEPENDENT_REVIEW_950C261.md)。

[8] [局部验证运行记录](../../../实验/normalize-precision-p5/evidence/ROUND1_VERIFY.log)。

[9] [QR、PSF 与因子 v2 独立复核](../../../实验/normalize-precision-p5/ROUND2_INDEPENDENT_REVIEW_633FFC31.md)。

[10] [真实生产调用链盘点](../../../实验/normalize-precision-p5/ROUND2_PRODUCTION_CHAIN.md)。
