# ACSD 科学设计—生产源码差异地图

> 科学/工程衔接文档。上游：docs/ACSD_DESIGN.md §1–§6、§13，以及 docs/science/unified/UNIFIED_SCIENCE_MODEL.md、science/noise_snr/NOISE_SNR.md、science/drizzle/DRIZZLE.md、science/sky/UPM.md、science/photometry/PHOTOMETRY.md。制作：2026-10-14。
>
> 本文件是**源代码静态追踪 + 已有实验结果的映射**，不是新环境运行测试。读取目标：audit/acsd-science-docs-p5；科学文档变更前的生产库基线是 5629d9767f337547895e1c7606f4e82de05a27f4；从该基线到本次文档提交未授权修改 lib/**。精确行号仅供定位，以函数/关键符号为长期锚。不能把“看见函数”“跑了测试”当成“真实物理链闭合”。
>
> 用户决定：不同设备/滤镜任意选集（无准入限制）；阶段一逐帧 HiPS + 稀疏 SNR 控制；阶段二稀疏→稠密统计模型、背景和接缝、合成，**最终单 signal HiPS**；阶段三仅导出普通平面 FITS；不建立人为研发流程门禁，以正向科学设计、可复现实验和用户看图为准。

## 1. 源文件—科学责任总览

| 源位置 | 当前生产功能 | 应承载的科学定义 |
|---|---|---|
| lib/algorithms/photometry/ | Gaia XP 参考光通量、逐星拟合/帧级标度 | Gaia XP + 实际 R_k；参考 R_0、颜色失配分离 |
| lib/algorithms/noise_snr/ | 逐源/帧级 SNR、背景噪声估计 | 指定 F_ref,k、PSF、条件 C 的参考 SNR；系统项分开 |
| lib/algorithms/drizzle/healpix_drizzle/ | 平面到 HEALPix 交叠、通量累加、单帧 HiPS | 精确/近似几何区分；相同算子信号与相关噪声 |
| lib/algorithms/shared/healpix/healpix_core.cpp | NESTED 角坐标与 tile 索引 | 唯一索引、12 面与独立 tile 手性对照 |
| lib/infrastructure/aio/src/hips/aio_hips_writer.cpp | signal/support/variance/ivar 与 Catalogue/HiPS 层级 writer | 阶段一多辅助对象与阶段二单 signal 的明确产品分离 |
| lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp | 稀疏 SNR、帧级 SNR→像素权重 | SNR→V_F→由 T 转成 V_B→w_B；不能只做恒等式 |
| lib/algorithms/coverage/src/coverage.cpp | 叠加帧兼容检查、MOC 覆盖 union、target_order | 任意设备/滤镜可合并；统一坐标/科学输出网格独立设计 |
| lib/algorithms/coverage/src/sky_plane.cpp、upm.cpp、sampler.cpp | 背景控制、天光面与连通规范 | 真实重叠图、差模、背景估计方差/失配系统项 |
| lib/algorithms/coverage/src/integrate.cpp | 单像元已合格候选的数值聚合 | 精度策略由上游确定；signal 与临时 support 分开 |
| lib/infrastructure/scheduler/src/module_adapters.cpp | P1/P2 各节点实际编排、产品连接、写盘 | 细模块职责和精度口径贯穿，不靠重复合同注释保证 |
| lib/algorithms/resample/p3_resample.cpp | HiPS→平面采样与可选不确定度分支 | 阶段三从单 signal 重采样并准确写 FITS |

## 2. 已由源码静态确认的行为冲突

### D01 — 不同滤镜被阶段二直接拒绝（明确与用户目标冲突）

lib/algorithms/coverage/src/coverage.cpp 的 p2 coverage 输入循环取第一个 frame 的 filter_passband 为 filter_ref，随后执行 f != filter_ref → "filter mismatch" 并返回错误；同处还强制逐帧 hips_frame 字符串相等。后者是**坐标一致性问题**，不应与光谱准入混同。

**应改**：允许不同滤镜、厂商、带宽及空/未知响应身份进入覆盖 union。保留每帧真实 filter/QE/response identity；若已有 WCS 座标可以统一转换，按可解释坐标处理，无法解释的坐标损坏仍报错。不要拿第一帧滤镜名来冒充最终混合响应。跨通带系统偏差单独诊断，不因模型未知排除有效图像。

**独立实验**：相同 WCS、两种不同字符串滤镜、相同物理信号，应可进入 union 并合成；不同响应的前向 Hα 星云保留可测系统误差而非被偷偷加性抹平；错误 WCS/坐标系依旧可定位诊断。禁止为通过测试简单统一修改所有输入滤镜头。

### D02 — 阶段二当前实际发布多层科学图像（明确冲突）

lib/infrastructure/scheduler/src/module_adapters.cpp 的 p2_op_write_mosaic 当前把 flags 初始化为 AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT，uncertainty_available 时再加 VARIANCE | IVAR，并分别调用 aio_hips_write_signal_support_tile 和 aio_hips_write_variance_tile。另一路 lib/algorithms/coverage/tools/stage2.cpp 也使用 SIGNAL | SUPPORT。

**应改**：mosaic 最终只有一个科学图像产品 signal，保留标准 HiPS 目录/元数据和必要处理摘要。**内部**仍要保留 covered_area、sumNorm、variance/precision、sample validity 等以生成正确 signal 和 NaN，不能只删支持对象后让 writer 除以 0。AIO 应清楚区分“signal 写盘需要的内部覆盖分母”与“是否写一个 support 科学子产品”；阶段一仍可按其自身需要保留辅助层。保证 HiPS 高低 order/父层生成一致，无数据 NaN、有限零/负合法。

**独立实验**：真实阶段二产品目录下科学图像子产品只有 signal，读回与旧版本信号相同（若暂未改科学权重）或与新模型期望一致；父/子 tile、FITS BUNIT、MOC、原子写出仍正常；不以删除产物同时损坏源数据归一化为代价。

### D03 — 阶段一写出 SNR Catalogue，阶段二消费的却是另一种规整层（生产断链）

lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp 将旧 HioSnrControlPoint/FP64 的 snr_psf 转为 AioHipsSnrPoint 列表；lib/infrastructure/aio/src/hips/aio_hips_writer.cpp 已实现 snr/ Catalogue HiPS 路径。旧 lib/infrastructure/aio/include/aio_snr_model.h 对 snr_psf 的字面注释为 (A−B)/mad，**不充分证明这些值是条件 F_ref/σ_F**。

但 lib/infrastructure/scheduler/src/module_adapters.cpp 的 p2_sparse_layer_probe 只看 signal 属性 ACSD_SPARSE_SNR_LAYER，读取满足 schema 的规则网格 JSON 和 SparseSnrReconstructor；用户已有 P2 实验报告表示生产没有写出该规整 sparse_snr_layer。两种载体没有已验证完整桥接。

**应改**：先查清阶段一当前 snr_psf 的实际数值构建与参考含义，明确绝对参考源条件 SNR vs 局部 (A−B)/MAD vs 相对指标；依据唯一科学定义生成可追溯的稀疏控制观测。设计 Catalogue→控制点格/稀疏估计对象的单一 producer/consumer API，携带坐标、F_ref,k、PSF、噪声状态、支持域、有效性、版本。阶段二真正消费，不以“配置默认 sparse_reconstruct”或“Catalogue 存在”代替产物闭环。不要改写全部控制值让它们符号相同而掩盖物理估计量不同。

### D04 — 阶段二的 SNR 逆权重缺少像素精度传递模型 T

lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp：weight_from_snr、weight_from_sparse_layer_pixel_prepared 等直接返回 (SNR_layer/F_ref,k)²×g²；未体现 PSF、源像元面积、Drizzle 相关噪声与面亮度统计目标在 T=V_B/V_F 中的转换。该式对点源**通量估计量**是恒等式，不能普遍当作 surface-brightness 的 w_B。

**应改**：设计局部参数化噪声形状 C_B=σ_B²K 时的可计算 T，至少在已知背景、固定 PSF/面亮度坐标和 K 的成立域计算 T_p=K_pp (h^T K^-1 h)；未知 K/背景模型/SED 时明确较弱的近似精度而非伪严格最优。更新阶段二权重来源与单位，不能再用 g² 修饰一个本来量纲不对的对象来冒充校正。实验优先证明点源信息和球面像元精度确实不同的**可证伪反例**。

### D05 — “稀疏 SNR 默认”在实际可用逐像素方差时并非主路径

module_adapters.cpp 的 p2_op_integrate 优先 corr_var_ready → weight_from_corrected_variance；其次输入 ivar 产品；两者缺失才 use_snr_chain，snr_path_effective 常可能为 not_used/frame_reconstruct。现有生产源码还在注释里用 “sparse SNR 尚未接入生产数据面” 等旧锚文本。

**应改**：对齐用户确定的**稀疏控制点建模是阶段二核心**，但不应为强制消费稀疏场而丢弃可信的真实像元噪声信息。可将逐像元方差用于稀疏模型校准/独立对照，明确最终合成权重来自哪个物理估计目标，记录实际方法；如部分阶段一产品缺控制点，按用户选择执行可辨识的基线并诊断其代价。不允许“预设默认 sparse”与“实际从未跑 sparse”并存而不说明。

### D06 — 不同分辨率数据被自动降至最低最高阶

lib/algorithms/coverage/src/coverage.cpp 对 target_order 使用 min(max_leaf_order across input)。混合不同焦距/像素尺度时，强制按最粗层合成可能丢失高分辨率数据中可恢复的星点信息，且在无界多倍采样差时引入错误 PSF 比较。

**应改**：目标球面输出分辨率作为独立、清晰的科学/配置决策，不能等同“所有输入里最粗层”。用户可明确选目标阶或采用有根据的默认；较粗帧如何映射到高阶目标需按其实际足迹、PSF、权重和相关噪声解释。拒绝通过盲目复制 coarse 像素创造“高分辨率真观测”。比较不同 target_order 下的星点 FWHM、低频保真、信息量和资源。

### D07 — 阶段二滤镜元数据来自第一帧

module_adapters.cpp 的 p2_op_write_mosaic 取 cov_doc.frames[0].filter_passband 写 obs_filter，代码注释还说覆盖模块验证了全部帧滤镜全等。这一假定随着 D01 放开必定失效。

**应改**：最终标明用户选的参考响应 R_0 或具名混合响应规则，记录参与帧滤镜分布；绝不以第一帧实际滤镜号冒充最终科学数据响应。即使用户给了不同滤镜但没有 SED 模型，产品仍可生成，但不能声称统一的精确物理 bandpass。

### D08 — Gaia XP 归一/颜色模型仍需核对生产数据与数学口径

lib/algorithms/photometry 与 module_adapters.cpp 的 Phase1 拟合对 Gaia XP + filter/QE 的依赖存在，已有真实 M42 颜色残差和未解释的系统地板。当前是否对整个图像施加用户目标 R_0 的物理颜色变换并未得到生产证据；不能把按仪器自身响应标定的 flux 直接当作通带完全统一。

**应改**：明确第 k 帧原生 R_k 光度标定、目标参考 R_0、参考恒星 SED/颜色项与未知星云 SED 下不可辨识系统差的接口。支持缺少某种设备响应的相对测光/未知模型报告，不因字符串型号不一致拒绝输入。不凭单幅宽带图计算逐像元颜色变换。所有局部参数按实际校准单位与误差传播标定。

### D09 — reference_flux、frame_gain、产品 BUNIT 的单位和重复修正风险

module_adapters.cpp 的 Phase1 SNR reference_flux_adu 及 Phase2 gain 的组合、p2_final 的 bunit=kP3BunitSurfaceBrightness、AIO writer 的单位约定需形成具体逐阶段量纲图。若 P1 已将信号从 ADU 转成 Gaia XP 模型单位，P2 仍以 F_ref [ADU] 还原后应用 g²，会出现二次标度或单位错误。现在**这是待验证风险，不是已实测错误**。

**应改**：提供每步 I/O 的单位表与 scale provenance，单元断言应以可独立求值的光度比/方差为依据，而不是只检查两种按同一个错误公式产生的字段相等。

### D10 — k_corr 的几何定义域欠缺，无法证明绝对统计精度

实验/absolute-snr 的 S05 真实/合成探针：当前控制点 k_corr 的适用表未充分包含 HEALPix leaf nside/source pixel 比 ρ；在一组实际几何下 1.4 与经验约 2.05–2.33 不符。该结果是已有 Agent 执行证据，**本轮未重跑其计算节点**。

**应改**：为背景控制统计量建立实际相关核/影响函数模型或新的几何标定函数，纳入输入角尺度、output nside、pixfrac、局部采样、统计估计量、有限 N 修正，避免常数回退掩盖差异。P2 真星点重复观测经验误差地板与背景控制方差是两个不同问题，不共用一个“通用校正因子”。

### D11 — Drizzle 近共线数值修复已存在，但曲边与高阶尺度未作独立完整证明

lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp 与 drizzle_engine.cpp 已有数值稳定修复：选定小 drop/极区构型 εpartition 改善，历史 1500-case 探针未见超过 1e-6。仍需验证真实 HEALPix 弯曲叶边界、chart 等面积 Jacobian、候选漏选、跨面、FP32、较大 drop 和高阶数值稳定性。

**应改**：将“求当前球面多边形近似的正确交叠”和“对真实 HEALPix 等面积目标的几何准确性”作为两种独立证据；面积分割残差接近零不充分证明后者。保留现有几何代码已有价值，不因尚未曲边对拍就推翻整个 Drizzle。

### D12 — HiPS 文件布局与父层下采样可能再次平滑

共享 healpix_core.cpp 提供 nested_local_to_fits_index 和反向转换，writer/read/export 还要检查所有消费者确实复用。aio_hips_writer.cpp 产生 hierarchy/低阶 tiles，p3_resample.cpp 选层和插值；真正图像经历 drizzle + HiPS 粗化 + export 重采样。旧文档可证明子父面积关系，但不能由此宣称有效 PSF/噪声谱无额外改变。

**应改**：独立 12 面/极区 tile 参考 generator；每条算子的光度、FWHM、MTF、噪声空间频谱，特别是从粗层选阶导致的分辨率变化。最终单信号不能凭空推出准确新的 ivar/点源信息量。

### D13 — P5 连通分量规范修复不是全域无缝

lib/algorithms/coverage/src/sky_plane.cpp 的有效控制图连通分量独立锚修复已发生；历史 S09b 两分量诊断中第二分量的归一指标下降，但物理上断连分量仍不能凭内数据推断背景相对零点。历史诊断分母变化不可写作“真实边界接缝改善 214 倍”。多处未测的真实边界和过拟合星云低频风险仍然存在。

**应改**：以真实重叠/有效控制作边，各分量零空间/规范单独解释；比较留出背景控制、真实天体 low-frequency power 和所有输入边界实际覆盖，不把“原有拟合更平滑”作为最终目标。

### D14 — 乘性残余 g 的生产有效性仍未闭合

P5 阴性模拟中，真实 g=1 时也可能给出显著“优化”。生产目前更多使用 identity、不存在经证实的有效乘性校正全链，不能让 Agent 自行开启乘性以追求更无缝。

**应改**：独立星光度锚、颜色分层、PSF 差与孔径匹配、阴阳前向世界、样本分离；有客观外部证据后再考虑 g(p) 非恒等，否则保留 g=1 并如实说明只是保守模型选择。

### D15 — export 仍保留多层误差消费设计

lib/algorithms/resample/p3_resample.cpp 有 p3_uncertainty_open / p3_uncertainty_propagate 和 nearest/bilinear；旧投影分册曾把 variance/ivar 与点源信息量作为导出必需对象。现用户明确 phase3 普通 signal FITS，阶段二不发布不确定度层，故这些路径不能充当正式产品依赖。

**应改**：从阶段二 signal 直接得到普通二维 FITS，WCS/BUNIT/NaN 正确；原有不确定度内核可继续用于独立科学实验或明示的辅助诊断，但不可强迫生产单信号阶段二额外输出 variance/ivar。

### D16 — 文档与源码角色重复，部分工程分册仍固定旧目标

docs/science/algorithms/mosaic/PHASE2_MOSAIC_WRITE.md 曾以 signal/support 输出为目标；docs/science/algorithms/export 与 docs/detail/registry 部分位置按旧静态“权重模式”、旧滤镜同一性或旧多层支持编写。大量行号/机械锚与 module_adapters.cpp 长达万行的编排代码造成文档易漂移。

**应改**：本次首先重写 ACSD_DESIGN 和 science 物理正本，详细端口/API/工程副本由 Agent 在修码阶段**依据新的物理对象和真实代码再写**；凡旧模块文件冲突，应明确标记为“旧生产实现说明、不得覆盖现行目标”，不将每个冲突再增加一套通过/拒绝台账。不要删历史实验与脚本，历史可保留并注明修订依据。

### D17 — 测试数量与科学正确性是两件事

既有 S09 报告某测试子集 29 failed/152 passed 前后同数，但未保存各失败 nodeid 的严格差分；多个正式 E2E 用 pytest.skip；三命令真实数据 smoke 在一个未科学标定 master_scale 上运行成功，不能说明输出真实面亮度可用于绝对科学解释。已有实验图/JSON 证据部分未跟踪到 Git。

**应改**：工程执行时直接记录每个失败/跳过测试及输入真值；搭建用正确标度的真实 Normalize→Mosaic→Export 端到端，可通过的内容用观察到的物理读数说明。无需增加“新 gate 脚本”；只需足够独立且会因错误而改变结果的实验。

### D18 — 生产可用的故障注入环境变量应仅属于实验构建

现有 ACSD_DRZ_P3_FAULT、ACSD_SKY_P5_FAULT 及旧 normalization fault 等环境变量可能使科学代码有意识复现已知错误，适合负例研究但不宜让正式发行路径在普通环境变量下变成错误数值。

**应改**：将故障注入限制在专用测试/实验构建、测试编排的显式注入接口或独立执行程序；生产始终使用正常数学路径。这是代码可信性与维护问题，不是审批流程门禁。

## 3. 不改哪些有效设计

- 三条命令互相独立，阶段间通过 HiPS/manifest 交换。
- HEALPix NESTED 与共享单点索引；512 tile 和 HiPS 标准目录的已有读写框架。
- 阶段一依靠 Gaia XP 星光谱标定并保留稀疏控制点的研究方向。
- 球面 Drizzle 中基于实际 drop overlap 的面积归一和信号/方差平方律。
- 像元无效用 NaN、信号可为负、统一原子写出、可重复真实数据来源索引。
- 阶段二可利用经过验证的稀疏 SNR 重建，减少全图重复噪声估计。
- 现有 P1–P5 各研究单元、失败实验及对照保留，论文归因重估而不是重演对自己有利的结论。

## 4. 代码重构的科学依赖次序（不是发布门禁）

**组 A：统计对象/光度语义先理清**。统一 scale、F_ref,k、V_F、V_B、背景控制估计量、参考响应；定位 P1 SNR 控制值真正生产口径。

**组 B：稀疏数据真实闭环**。让 P1 控制点经确定载体进入 P2，并重建支持域/孔洞/全局坐标一致的精度场；在不同 PSF/相关噪声下测试 T。

**组 C：球面真实几何与 mixed-resolution**。独立 HEALPix 曲边面积 oracle、HiPS tile 手性、nside/输出阶/候选遗漏；不因最粗设备强制降低目标层级。

**组 D：P5 与统计合并**。有效重叠图逐分量、分离加性/乘性/通带残差，保留低频，消除过滤器字符串拒绝，最终生成合成 signal。

**组 E：单信号产品与导出**。AIO 最终只发布 signal，多尺度 HiPS 与第三阶段 FITS 在不同分辨率/滤镜组合下可用；内部精度和诊断不冒充最终层。

**组 F：一套独立真实 E2E**。恢复真实帧 manifest，建立解析+HST 前向+真实数据对照，保存信号、star photometry、PSF/FWHM、相关噪声、seam 和星云低频。用这些结果驱动后续迭代。

每组有可运行的物理对照和代码范围，但不存在“必须把某个状态卡设为绿才能提交”的规则。所有改变行为的实现必须能说明它改变了哪一个被估计量，并可以解释为何优于旧方法。

## 5. 可复核来源

项目：fujiaze/Astro-CS-Database，工作分支 audit/acsd-science-docs-p5；审阅使用 GitHub fetch_file，**未在当前会话的本地服务器执行 C++、真实 FITS 或 HST 测试**。历史数值来自：

- 实验/absolute-snr/REPORT_paper.md 与 docs/S05_P2_evidence.json；
- 实验/dense-snr-reconstruct/REPORT_paper.md 与 docs/S06_P4_evidence.json；
- 实验/healpix-polar/REPORT_paper.md；
- 实验/additive-sky-seamless/REPORT_paper.md；
- 实验/engineering-evidence/acsd-science-docs-p5/INDEPENDENT_REVIEW_8ecaac427.md。

上述历史读数并非本轮复测；本文记录为既有来源/待重验。对 lib/** 的修复尚未启动，也没有生成新的 E2E 结论。