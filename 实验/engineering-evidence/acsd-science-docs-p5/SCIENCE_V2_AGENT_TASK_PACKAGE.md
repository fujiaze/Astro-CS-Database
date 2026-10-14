# ACSD 统一科学模型 V2：DeepSeek Agent 代码实施任务

> 执行规格，2026-10-14。执行分支 audit/acsd-science-docs-p5，原始数据在用户计算节点 /testdata。本包不是已执行结果。不得 PR、不得合并 main、不得上传原始 FITS/HST/母版。项目不设研发审批门禁；验证用于发现实际物理错误，不追求机械全绿。用户自主选择跨设备/滤镜合成，不允许以设备/滤镜不同拒绝合法输入。
>
> 前台负责 science 正本和最终工程文档重写；Agent 负责实际 C++、可复现实验、API 变化报告。不得擅自重写 docs/ACSD_DESIGN.md 或 docs/science 核心公式。若有独立反例证明模型错误，提交反例与证据，由前台调整后再实施。

## 0. 不得更改的用户目标

**阶段一 Normalize**：单帧 CCD/CMOS 标定、Gaia XP 光度、PSF/噪声、参考 SNR 稀疏控制点、球面 Drizzle，输出单帧 HiPS 和必要辅助数据。

**阶段二 Mosaic**：用户可将不同相机、焦距、曝光、R/G/B 品牌、3 nm/7 nm Hα 等数据全部合并；利用阶段一稀疏控制点重建内部稠密参考精度、求对应面亮度权重、背景/接缝、排异及合成；**最终科学图像只允许一个 signal HiPS 通道**，内部方差/有效域/背景参数是中间量。HiPS 仍保留 Norder/Dir/Npix 的标准多分辨率层级与 metadata。

**阶段三 Export**：从阶段二 single signal HiPS 导出普通二维 WCS FITS，供通用后期处理；不要求最终 variance、ivar、SNR 或 support 产品。

阅读必须先按顺序：docs/ACSD_DESIGN.md → docs/engineering/architecture/DEV_MODE.md → docs/engineering/standards/DIAGNOSTIC_PROBES.md → docs/science/unified/UNIFIED_SCIENCE_MODEL.md → DATA_SEMANTICS.md → photometry/PHOTOMETRY.md → noise_snr/NOISE_SNR.md → drizzle/DRIZZLE.md → sky/UPM.md → projection/HIPS_TO_FITS.md → resample/RESAMPLE.md → docs/engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md。旧详细算法/模块登记文件可能是历史实现快照，不得覆盖新的物理目标。

## 0A. 先处理本轮补充：内部开发构建、探针与中间量落盘

用户明确要求：**只有一个公开编译开关**控制内部开发诊断是否编入程序。开发模式可以额外保存中间科学数据并采集性能调试探针；发布模式不编译这两类诊断能力。具体规范已由前台写成两个工程正本：

1. `docs/engineering/architecture/DEV_MODE.md`：`ACSD_DEV_MODE=ON/OFF` 的唯一构建边界、科学数据通路不变、产品/工作区隔离、checkpoint 不属于被动探针，以及旧机制迁移。
2. `docs/engineering/standards/DIAGNOSTIC_PROBES.md`：稳定 probe_id 清单、P1/P2/P3 中间数据只读采集、FITS/JSON/JSONL 格式、坐标与 BUNIT、选帧/tile/ROI 与磁盘配额、AIO 并发落盘、失败语义、ON/OFF 性能对照。

**这是需要落实的正式工程任务，不是旁注。** 目前 CMake 同时存在 `ACSD_PROBES` 与 `ACSD_DEV_MODE` 及三个开发子项，`lib/infrastructure/observability/probes/include/acsd/probe.h` 与 src/probe.cpp 已有真实可复用的性能框架。必须统一入口、复用已有基础设施；不要再造新计时库、debug image writer、日志 sink 或额外公开 CMake option。

**首要科学中立性问题**：旧 DEV_MODE.md 曾指出若干 P1 节点“写中间文件→读回”和按编译宏改变缺失回退；这会使两档改变科学行为。请先定位实际调用，确保数据计算依赖始终走同一管线；额外 snapshot 只能读取现有输出块而不可成为消费者的数据源。内部需要的暂存与“纯调试快照”必须区分；不能靠 `#ifdef` 删除算法必需文件再制造隐蔽缺输入。

建议在已有运行时和 AIO 实现被动 `SnapshotProbe`，现有 `acsd/probe.h` 作为细分计时唯一入口；由 ACSD_DEV_MODE 统一编译条件。构建 OFF 时诊断参数表达式不求值、probe.cpp 不链接、无计时/分配/ENV 读取；ON 时用开发诊断选择器选少量有代表性的 P1 SNR 点、Drizzle tile、P2 稀疏输入/重建 `V_F`、精度桥梁 `T`/`V_B`/`w_B`、天空背景场、接缝、排异与 P3 导出区域。开发快照一律落单独工作区，不变成第二个最终 HiPS 科学图像层。

性能记录沿用 thread-local/steady_clock/batched JSONL，增加工作量、nside/tile、backend/worker、IO 与结果身份以支持真实调优。无意义逐像素 printf、每 tile 上锁或无预算全图 dump 均不可接受。快照队列超预算/开发磁盘失败应记录诊断缺失但不破坏科学任务；正式科学产物/正式日志写失败仍按正常错误语义。故障注入环境变量不得从发布版触发；`ACSD_DEV_CHECKPOINT` 的“跳过已完成帧”不是诊断副作用，不能在此任务偷偷改变运行路径。

复核四类同优化构建：OFF、ON 但不记录、ON 仅性能、ON 定位快照。比较物理输出（尽可能 bitwise）、正式产品树、开发工作区、发布二进制诊断符号、实际采样的性能开销，包含并发、取消、磁盘满、内存复用场景。**科学结果不同必须查明原因，不得以“debug 精度不同”掩盖路径分叉。**

Agent **只负责实现和实验**：上述工程设计正本由前台维护；发现设计矛盾提交可复核证据和建议，不得自行覆盖这两个文档或科学正本。实际接口/ABI 修改以函数签名和证据供前台回填。

## 1. 第一份交付：真实正在调用的科学数据流

先从 module_adapters.cpp、阶段一/二/三入口、AIO、p2 coverage/integrate 追踪生产调用图，不能用“函数存在”代替真正执行。逐边登记真实上游生产者、消费者、frame_id、数据对象、光度单位、WCS/NESTED/FITS-local、统计目标及实际运行分支。特别解释：
- hp_drizzle_api.cpp 从 HioSnrControlPoint 的 snr_psf 到 AioHipsSnrPoint 的数据定义；旧 aio_snr_model.h 注释为 (A-B)/mad，与 F_ref/sigma_F 不能未经验证视为同一个物理量。
- 当前 P2 的 sparse_snr_layer 只通过 ACSD_SPARSE_SNR_LAYER 属性探测和规则 JSON schema 读取；阶段一已有 snr/ Catalogue HiPS，但现有静态审查没确认真正写出该规则 JSON。
- corr_var_ready 优先、其后 ivar 产品优先、再后 use_snr_chain 的实际权重选择；sparse_reconstruct 默认配置不代表生产真正走此分支。
- Phase2 两条 writer 输出 SIGNAL|SUPPORT 以及条件性的 VARIANCE|IVAR；target_order 选取 min(max_leaf_order)；跨滤镜 filter mismatch 直接返回失败。
- 前台文档是目标设计，代码尚未变更；所有“已实现”结论必须从源码/实际执行证明。

## 2. R1：移除非物理的跨滤镜限制，保留数据解释

lib/algorithms/coverage/src/coverage.cpp 的当前循环以第一帧 filter_passband 为基准，后续字符串不同直接 filter mismatch；这是与用户选择直接冲突的行为。移除这项设备/滤镜字符串准入。保留真正损坏的 WCS/坐标输入错误，能进行合理坐标转换时不因名称不同报错。保留每帧真实响应/滤镜身份，绝不把第一帧的 FILTER 写到最终单信号 HiPS 中假装全部响应相同。

Gaia XP 光度模型分开 R_k（每帧实际响应）和 R_0（用户选定的参考或混合约定）。匹配恒星可获得色项，但在没有逐位置 SED 时，不能声称任意星云发射线都能靠一个标量完成精确通带同一化；允许继续合并，用可复核的颜色系统差和响应定义记录限制。未知滤镜曲线时可使用有观测支持的相对测光，不虚构曲线，也不因为型号不同拒绝。

## 3. R2：把阶段一 SNR 控制点与阶段二重建真正接通

**数学对象**：在局部面亮度观测向量 y=Fh+Zβ+n 中，h 为每单位点源通量的 PSF/像素面积模板，C_B 为条件噪声协方差。已知背景时：

\[
 V_F=(h^T C_B^{-1}h)^{-1},\qquad SNR_{\rm ref}=F_{\rm ref}/\sqrt{V_F}.
\]

背景同时拟合时，用 A=[h,Z] 的 (A^T C_B^{-1} A)^-1 的 FF 元素。含源散粒噪声时，这只是固定 F_ref 下的条件统计量，不是任意源亮度的全局常数。记录参考星等/通带、F_ref,k、PSF、σ_F、球面叶面积、noise model、估计背景自由度、控制位置/有效域、来源。

不能把旧的 snr_psf=(A-B)/mad 直接改字段名当参考 SNR。需要查明旧估计的条件/单位，保留真实旧值作诊断，建立明确的 canonical reference SNR 生产者。优先复用已存在的 SNR Catalogue HiPS 作为真实稀疏控制载体；若需要规则网格，P2 在内存中由实际点构建带 validity/空洞的模型，避免凭空新增又一套未连接的 JSON 产品。若选择正式 sidecar，则必须完成真实 write→read→reconstruct 证据。

重建优先比较 log V_F、log SNR、形状保持插值、局部双线性、IDW p=1/2、自然样条及帧级常量。历史 S06 已发现：样条过冲，clip 有偏，控制网格边缘和空洞不应静默最近填充；IDW p=2 在既测世界经常胜 p=1。不要因为旧默认 1.0 就把有效 2.0 盲改。用空间块与跨帧留出确定策略；输出内部 V_F_hat、支持状态和模型误差。不得整幅稠密驻留为前提；按球面查询现场求值，跨 tile/face 连续。

## 4. R3：点源 SNR → 球面面亮度精度的核心新实现

阶段二合成的是球面面亮度 B，而 reference SNR 只定义点源通量 F 的方差：V_F=(F_ref/SNR)^2；w_F=SNR^2/F_ref^2。**w_F 不一定等于合成像元的 w_B=1/V_B**。

可以直接实现和独立检验的参数化例子：设局部球面面亮度噪声 C_B=σ_B²K，K 为已知正定无量纲局部相关/异方差形状矩阵，h_i=P_i/Ω_i：

\[
 V_F=\frac{\sigma_B^2}{h^T K^{-1}h},\quad
 V_{B,p}=\sigma_B^2K_{pp},\quad
 T_p=\frac{V_{B,p}}{V_F}=K_{pp}(h^T K^{-1}h),
\]

\[
 w_{B,p}=\frac{(SNR_{\rm ref}/F_{\rm ref})^2}{T_p}.
\]

T 具有面亮度方差/通量方差的相应单位，不能设为无量纲 1。背景自由参数要用有效投影信息。若缺少局部 K，可采用经过明示的对角/白噪近似求 T，继续合法合并并输出近似状态；**不得直接默默 T=1、也不得称其严格统计最优**。不必持久化全协方差矩阵，允许局部核、稀疏算子摘要与经独立模拟标定的近似。

核心反例：构造两帧具有相同 reference SNR，但 PSF 宽度、像元面积或相关 K 不同，真实像元 V_B 不同。比较原始 w_F 与新 w_B 合成的独立散度、PSF、点源及弥散源真值。再构造 T 对各帧相同的阴性世界，证明两种归一只差共同常数。不接受仅检查公式等式左右相等作为物理验证。

将背景控制精度 w_ctrl=1/V_ctrl、点源 w_F 与像元 w_B 三者在 API/日志中独立命名。P5 的背景中位数 k_corr 是另一个统计估计量，不能充当万能像元相关修正。P2 S05 已发现 k_corr 查表几何域缺 nside/source 尺度比，经验 2.05–2.33 与 1.4 不符；必须按实际 patch 统计量、输出 nside、pixfrac、源尺度等独立研究，不是机械改常数。

## 5. R4：球面 Drizzle、HiPS 与混合分辨率

数学定义：源未收缩球面面积 Ω_j、drop 面积 A_j、与 HEALPix 叶的交叠 a_jp，w_jp=a_jp/A_j，N_p=Σ_j w_jp Ω_j，球面信号 S_p=Σ_j w_jp x_j/N_p。固定几何线性算子 R_{pj}=w_jp/N_p，C_out=R C_in R^T。完整源 drop 分配 Σ_p w_jp≈1 与恒定面亮度保持、最终星点孔径光度、实际 HEALPix 弯曲叶几何精度为不同测试目标。

保留已有 spherical_overlap.cpp 稳定性交点修复；新增真正独立的 HEALPix 曲边/chart 或自适应高精度 oracle，不以四角大圆弦近似自身闭合当成真实几何正确。全 12 个 face、极区、RA 0/360、face/tile 交界、小大 pixfrac、FP32/64、不同尺度比均评估相对曲边误差与源 flux/PSF/噪声谱。

lib/algorithms/coverage/src/coverage.cpp 目前 target_order=min(max_leaf_order) 可能将高分辨率观测无条件降到最粗输入阶。增加用户可选输出 order 与明确自动策略，通过不同原生分辨率、PSF 和计算预算的对照选择默认，不能盲目改成 max 就声称最优。粗图在细层不能靠重复像素创造独立高分辨率信息；评估 alias 和实际有效 PSF。

共享 HEALPix NESTED→tile-local FITS 行列函数；使用独立 HEALPix/HiPS generator 验证 12 face、父子、WCS 手性。层级粗化和阶段三再次插值分别测量噪声纹理/MTF/点源及低频损失。

## 6. R5：接缝与真实星云低频

在真实有效重叠图上逐连通分量固定加性背景 gauge；无跨分量观测和外部锚不能推断绝对背景零点。背景估计量用自己测得的 V_ctrl，不用点源 SNR 冒充。仅用 P1 光度确定的标度作公共基础，P5 只处理额外差模。

彩色星云光谱、PSF/孔径差、残余乘性与纯加性背景区分。真实 g=1 的历史 P5 阴性世界存在伪乘性改善，禁止为了减小 seam RMS 而擅启非恒等增益。增加独立恒星、颜色分层、PSF 分层、合成注入阳性与阴性对照后再评估。

对每条真实边界报告评价数量、未评边界原因、有符号阶跃及随法向距离 d 的变化。并列评价扩展源总通量、星云 low-frequency power、梯度/暗云保真、恒星光度和实际合成随机散度。单纯 feathering/平滑接缝、全减背景造成信号损失不可称科学成功。

## 7. R6：最终一个 signal HiPS、阶段三普通 FITS

当前 module_adapters.cpp 的 Phase2 writer flags 含 SIGNAL|SUPPORT，并在 uncertainty_available 时加 VARIANCE|IVAR；lib/algorithms/coverage/tools/stage2.cpp 也存在 SIGNAL|SUPPORT 路径。设计 AIO API 将内部 covered_area、sumNorm 和 validity 与最终写出 science support 图像**分开**；不能为了删 support 而取消 signal 的正确归一。P1 的单帧辅助产品维持其自己的科学需求；仅 P2 最终产品发布单信号。

HiPS properties、MOC、Norder/Dir/Npix、多级 FITS、BUNIT、NaN/负值规则、原子写与 source provenance 保持真实可读。混合通带时不能仅取第一帧 FILTER 作为总体响应。阶段三只依赖 signal 和其坐标/单位，输出普通 2D 浮点 WCS FITS；原有 P3 不确定度 API 可用于内部实验，不能成为导出的额外必需输入。

禁止在普通发行构建中通过 ACSD_DRZ_P3_FAULT 等环境变量触发已知错误算法；故障注入迁入测试构建/独立测试程序，不影响合法生产图像。

## 8. 完整实验矩阵（实验是测量，不是流程门）

| 世界 | 独立真值与构造 | 观察对象 |
|---|---|---|
| E1 | 解析 CCD 星点、Poisson/read、固定/自由背景，已知 F_ref 与 PSF | SNR_ref 的实测散度与区间覆盖，V_B、w_B 对照 |
| E2 | 相同 SNR_ref、不同 PSF/像元面积/相关核的双设备 | 点源信息与像元精度是否被正确区别，最终合成效率 |
| E3 | sphere 12 faces、HEALPix 曲边、多个 pixfrac/nside、跨面 | 几何真值、drop 分割、常数/源 flux、局部噪声相关 |
| E4 | 细/粗设备混合、不同 PSF、不同输出阶数 | FWHM、星点光度、MTF、内存和噪声谱 |
| E5 | Gaia XP 星光谱、不同 QE/R 滤镜与已知星云 Hα/[N II] 谱，含 3/7 nm | 恒星颜色残差和星云不等价性；用户仍可合并 |
| E6 | 断连/连通观测图、已知天空差模、真实低频星云 | 每分量规范、接缝台阶、结构保存 |
| E7 | 真实 g=1 与 g≠1，PSF/颜色差混杂 | 阴性乘性误判和留出光度 |
| E8 | 已知球面信号通过不同 HiPS order→WCS FITS | tile 手性、FWHM、通量、二次插值噪声纹理 |
| E9 | /testdata 真实已校准 CCD/CMOS、HST 成品只当信号模板 | 实际三阶段完整输出、最终拉伸预览与数字测量 |

不得随意按文件名猜目标或曝光。解析产品/原始 FITS 的 WCS、曝光、filter、scale、gain/RN 是否存在、母版及源数据 SHA256；对缺失字段写 UNKNOWN，不编造常数。真实高精度阶段不能复用仅用于 smoke 的 master_scale=65535 并声称绝对光度可信。

分开记录：未运行、运行但模型只适用特定域、实测差、独立真值误差；测试没全绿不停止研究，但不能删除/改坏独立 oracle 为了数字好看。历史 S09 的失败数需要按 pytest nodeid 严格比对，9 个静态 skip 的 E2E 应尽力转为实际脚本。所有实验保存命令、种子、输入清单/哈希、源码 SHA、环境、结果；原始 FITS/HST 不上传 Git。

## 9. 建议提交顺序与交付

1. R0 源码读图、基线真实调用、数据单位表；同时审计旧 CMake 双探针路径和中间产物读回。
2. R0D 实施唯一 ACSD_DEV_MODE 编译边界、复用性能探针、被动 snapshot 工作区与诊断接口；先验证 OFF/ON 科学行为不分叉，再逐步插入更多 P1/P2/P3 probes。
3. R1 取消跨滤镜拒绝，元数据、混合分辨率可选择；
4. R2 P1 真正 reference SNR 稀疏 producer 和 Catalogue/sidecar；
5. R3 Phase2 真正的稀疏 reader/现场重建与模型支持域；
6. R4 精度传递 T 和正确的面亮度逆方差叠加；
7. R5 geometry/HiPS、P5 接缝与低频真值；
8. R6 单信号写盘与 phase3 普通 FITS；
9. R7 独立解析→HST 结构前向→真实 E2E 与用户可视化预览。

提交允许合理重构 module_adapters.cpp 超长混合编排，优先建立清晰 producer/consumer、复用真实 API，不能为了机械锚字符串和最小 diff 容忍单位错误。CMake/Windows/Linux 变更按实际实现适配。旧工程 API 需要修文档时 Agent 只交真实签名、调用图和差异说明，由前台重写文档。

每批提交报告必须包含 Git SHA、真实构建和运行命令/退出码、源文件路径、输入数据索引、科学估计目标、独立读数与误差、未实施事项、对图像的影响；最终产生真实可浏览的 HiPS、普通 FITS、全景预览、边界放大、PSF 裁切与低表面亮度对照，由用户自行判断效果。**没有在计算节点运行就不许声称已运行，没有真实 E2E 就不许声称科学链完全验收。**

直接代码定位：coverage/src/coverage.cpp；integration/phase2_integrate/src/weight_chain.cpp；drizzle/healpix_drizzle/{hp_drizzle_api.cpp,drizzle_engine.cpp,spherical_overlap.cpp}；shared/healpix/healpix_core.cpp；infrastructure/scheduler/src/module_adapters.cpp；infrastructure/aio/src/hips/aio_hips_writer.cpp；coverage/src/{sampler.cpp,sky_plane.cpp,upm.cpp,integrate.cpp}；resample/p3_resample.cpp；photometry/ 和 noise_snr/。完整差异 D01–D18 在 docs/engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md。

## 10. 开发诊断落地的额外交付

Agent 的最终报告须给出：发布构建 CMakeCache 与真实诊断符号扫描、开发构建的性能 JSONL 和至少 P1/P2/P3 各一个可独立解读的快照样例，debug_manifest 元数据字段完整程度、实际 I/O/s 和 wall 额外开销、诊断写盘失败处理、与三命令正式产品逐位/数值对照、旧 ACSD_PROBES/子开关/环境变量的真实迁移表。所有样例应来自真实正在调用的生产函数，不得手工构造虚拟“成功快照”冒充。
