# M42 真实数据自治研发执行规范：先修复 S1，再自动执行完整 196 帧 S2

**适用仓库/分支**：fujiaze/Astro-CS-Database，`rebuild/acsd-v2`。**旧执行批次**：[Issue #123](https://github.com/fujiaze/Astro-CS-Database/issues/123)，真实证据 `docs/audit/m42-phase1-2026-11-12.md`，审查 `validation/m42/review/independent/REPORT.md`，冻结旧失败于 `7205796096569137697564e8e87435a62dcb1702`。本规范是执行指令，不代表任何修复或科学能力已经通过。

## 0. 总命令和权限边界

使命是**连续完成**：（一）S1 修复真实 M42 验收失败并建立可重开、可导出、资源有界的实际科学全链；（二）在独立审查 S1 PASS 后，**无需用户再次催促**，Agent 自行建立新的 S2 Issue，随后对**全部 196 帧作逐帧尝试、质量筛选，并输出 Hα/R/G/B 四种科学通道的全区域正式 FITS**。不同执行轮次使用不同 Issue，禁止把一个 Issue 当永续日志。任何 S1 FAIL 均须自行分析、定向修复、重新审查；**不能因一次失败就停止回复“等下一轮指令”**。不能越过 S1 门槛伪造 S2。

用户只负责最后查看最终科学 FITS 是否出现肉眼可见的明显接缝/残影，**用户不是 SNR、校准、排异、统计、物理正确性、性能或接缝定量的评审者**。Agent 必须预先完成这些工作。Agent 不得承诺不可观测的科学真值或把有条件的信号产品叫已知完整噪声产品。

默认直接修复分支并推送；不改 `main`，不强推、不 PR、不自动上传原始 FITS、不破坏只读 `testdata/`、`gaia/`、校准 XISF 或其它用户数据。文献/数学定义以五份 `docs/science/foundation/` 和 `docs/engineering/rebuild/` 为权威，不产生平行产品/数据模型。不要新增 CI/覆盖率/文档格式门禁；实验程序可用于独立核验，必须有人或独立 Agent 解释结果。

## 1. 数据与现有证据（禁止重新探索或重复哈希整个数据集）

真实输入目录已由本轮 Agent 实测：`testdata/M42_T2T3_mosaic_Flying_dutchman`，本地绝对路径以前次报告为准，**必须在实际运行机核对存在，不假设网络挂载**。冻结文件 `validation/m42/inputs_manifest.json` 已列出逐文件全 SHA-256、4096×4096 原始图像、头部与观测位置；若 mtime/size/哈希或内容确实变化，才重新计算相应项，**禁止每次重算 196 个大文件 SHA**。

- 总计 196 FITS，约 6.13 GiB；T2 73、T3 123；六个面板 M1–M6（每设备各六面板）。
- FILTER 原始分组：H-alpha 50、Red 49、Green 49、Blue 48；按**文件身份逐项**使用，不能让脚本用文件名猜曝光、观测类型、PSF、response 或校准状态。147 帧 300s、49 帧 600s；按 header 而非“全 Hα=600s”处理。
- 196 帧不含 FITS SIP 系数，只有 166 帧具备 CD 矩阵；历史 PinPoint WCS 已在三帧呈 5.2–5.7″ 偏差。**不许静默信任旧头**：优先以 Gaia 对星的外部独立求解/校验为实际 WCS，所有 frame 与权威解归档一次。无 CD 的 30 帧必须独立解，不能当坏数据直接跳过。
- T2/Hα/M5 三帧冻结于 `validation/m42/inputs_manifest.json` 的 `frozen_s1_2`；前次独立 Gaia TAN+SIP 留出 RMS 0.307/0.327/0.394″，匹配 699–784；这些**是有效的旧证据，复用**，不重新跑同一 plate solve 除非坐标/模型代码实质变化。
- T2 主校准 XISF 位于 `testdata/T2 calibration files`，已经有**只读可重复的**FITS 解码产物及其 SHA；先核对已存在可用副本与校准正确性，再重用，不重复转码。T3 校准须按真实对应设备/日期/通道查找，缺失则具名 conditional/拒绝，不将 T2 master 套给 T3。
- `gaia/GaiaDR3`、`gaia/GaiaDR3SP` 已定位；响应现有 `validation/m42/response_ha.json` 为假设 3nm top-hat，非实测 QE/透过率。真实 GAIN/READNOISE/SATURATE 缺失，不得假装知道完整电子噪声和绝对 Hα 通量。
- 旧提交包含能独立逐像素复算的单帧**诊断** FITS `validation/m42/deliverable/M42-S1_Halpha_256_calibrated.fits`，**并非**本任务正式 Normalize/Mosaic/Export；不能把它复制/改名成 S1 PASS。

先核对最新 git、工作区、现有成功的产品/缓存和可用空闲空间，**冻结并复用**，不清理未知目录。

## 2. 必须纠正的上一轮科学/工程问题：先验证命题再改代码

### S1-P0-A：球面信号、支持、索引与持久化失配；不得错把 CSR nnz 当叶数

旧真实 256² Normalize `SignalEstimate.signal.shape=[122]`、`SupportMap.index.size=122`、`primitives=[""]`、CSR 的 `indptr.length=123`、`nnz=73151`，却产生约 6.1GiB 的大面积空 HiPS tiles（旧检查抽到全 NaN），还 exit 0。

**注意：CSR 122 行 + 73151 个 nnz 在数学上并不自相矛盾！** nnz 是输入到输出的非零贡献，不能当叶数。真正要证明的是：（1）输入像元的球面足迹与输出 HEALPix 叶身份一致；（2）每个物理受支持叶的 R 行与 signal 一一对应，索引唯一且不含空 primitive；（3）结果值/支持/数值界/geometry revision 同域；（4）发布的全部产品 tile 在定义的有效域内能从稀疏信号精确重建且无无关填充。独立 Astropy/astropy-healpix 或高精度球面 oracle，在已冻结 WCS 的小 ROI 计算预期受支持叶集合、面积、重心/边缘及常场/脉冲响应；不能只拿 `sum(area)` 或 `exit0` 自证。

**定位顺序（记录每一层的有效像元/叶数与稀疏尺寸）**：真实校准+D Q→原像元/footprint→`compute_overlap` 的候选叶、交叠区间与面积→`build_drizzle` R 的 row/col/index→`SignalEstimate` 值/区间/单位/assessment→产品闭包/reader→HiPS tile writer→独立 FITS reader。写最小可重复的一个源像元、边缘像元、空支持、双 leaf、真实 WCS 小 ROI、旋转版正负例；每层核实际内容/单射，而非只查 shape。**任何不一致/无支持伪有值/有支持却全部丢信号**，发布必须失败且非零退出，不留“成功”完整 manifest。

**稀疏写盘是关键性能缺陷**：旧报告约 `20 Dir × 10000 planes × 64×64 float64` 产生 6.1GiB 大多空 tile。先证明请求的 coverage/tile set，再**只物化有科学支持的真实 tile**，未支持区域由 coverage manifest/无支持语义表示；不做“生成一切空 FITS 再删”的补救，不生成整天/所有 Npix。只在真实目标 footprint 与空间索引上迭代。产品仍符合现有 NESTED/HiPS 和可重开约定，必要时外部 reader 比对合法空间无支持。

### S1-P0-B：FITS 卡片、完整闭包与原子发布

`ACSUNIT` 长字段的 FITS 卡片必须使用合法标准关键字、`HIERARCH`/`CONTINUE`或可用的短标准映射；长字符串放合法 HIERARCH/CONTINUE 或独立 sidecar 并用正式 FITS 头关联，绝不生造被截断的单引号卡。**读取每个发布产物的全部关键 header card**（`astropy.io.fits.verify('exception')` 及针对 ACSUNIT/BUNIT/CRVAL/CD/CTYPE 的解析），确认 FITS padding、大小端、2D shape、单位、NaN/no support。多通道最终产品不得因任意单个非法卡而被算成功。

`LinearAction` 引用缺失是发布前闭包错误：从 `ProductGraph` 所有对象引用做可达闭包，持久化 action 的 children、CSR payload arrays、support arrays、noise/shared roots、recipe、geometry revision 与内容身份；**完整发布前，在新的 Host/Storage 实例（无旧缓存）重开**至少两次，`covariance_apply(C,x)` 与 `Q C Q^T` 仅在模型实际具备条件时核验，不能发明未知根。缺失引用、循环/不一致哈希、不可读 dependency、`assessment` 与 `frame_science` 冲突、bad FITS 头都必须让发布拒绝；已有 good final 不因失败被污染。负例包括故意删除一个 action block、删 support states、坏 hash、坏 header、部分写/中途取消；不以“reader 宽松解码”替代严格完整性。

### S1-P0-C：证明真实资源规模，移除稠密展开

旧 256² 单帧 Normalize 10–12 分钟/峰值 6.7–7.4GiB、每帧持久化约 6.1GiB；1或2帧 Mosaic 峰值 11.6–12.6GiB，在 12GiB process /约17GiB 主机上失败。必须精确列出对象类型的`count × bytes`和生命周期：source pixels、footprints、candidate leaf、overlap ledger、R CSR、signal/support、noise influence、sky controls、global normal matrix、scatter matrix、product/array serialization、FITS tile buffering、diagnostic。查看堆/分配峰值及 CPU hotspots，找出空间乘数而不是凭直觉加大 Limits。

改为真实 **region/tile/chunk-by-chunk、稀疏块**计划：按视场实际占用的叶和出力 window 分割，先 small ROI 执行，sky 的必要全局小规模 control/gauge 屏障冻结，再二次流式积累/写最终块；保留共享根的 cross term action 表达，必要时 block-wise apply，不可把 C 全部 densify；不能把 split frames 的 shared_root 误拆成相互独立。所有第三方 allocator 和输出缓冲受唯一资源账本计费；无上界操作先具名拒绝，不能顶着 12GiB 限额继续到 OOM。**优先在 12GiB 限额下留主机/系统安全余量**（目标诊断RSS≤10GiB，若无法达到必须给资源方程与修改算法方案，绝不无限加额）。

作 32²→128²→256²→512²→完整4096² 递增规模的有限抽样，在同算法容差/真实叶order下记录 `R rows/nnz、tile数、对象字节、RSS、wall、bytes/valid leaf`，只对受影响的最小规模重跑。真实数据不能动辄因改个writer就重复计算整帧 20 次。Work 上界必须从真实操作计费推导，旧 `fit_psf work=4` 与声明千万级不符、`work 4e9→1e12` 没有实测依据；修计费与回滚/取消/饱和算子，不把增加预算当科学修复。

### S1-P1-D：正确 PSF/局部背景/测光和相对标度

旧 crop 只测 2 颗星，其中一颗约 `−2.99e6 ADU`，位置偏差约 4.7 px，联合光度拟合 `nonconverged`、零点 `~2.7e20`、大量负通量。这**不是**简单调高容差或多挑几颗就解决。采用可复用的**像元积分 PSF + 局部背景空间基**，从常数/局部倾斜平面开始，必要时局部二次项或由独立邻域刻画的星云背景，使用稳健遮罩（饱和、坏点、其它亮星、星云强结构）并对结构过高/模型不辨识给 reason。逐点检查训练星/留出星的残差、相对质心、背景阶数稳定性、正负 flux 与统计显著性。孤立源不足时按实际混叠做邻星联合拟合/扩大搜索到整幅图的合法未饱和天区，绝不 `selected_stars.empty()→无告知放回所有重叠窗口`。

高 SNR 已知亮星的巨大负通量、异常 WCS 偏离或非收敛必须使**相应科学估计量无效**，不能拿来做正的相对尺度；低 SNR 噪声测量可有负点估计，不应对全部科学信号做 clamp。先对独立可复算的几颗低背景星和真实 PSF/孔径验证，再用足够的未参与拟合恒星横跨位置、颜色、亮度、两夜验证。冻结同通带标度 gauge 与相对归一规则；旧四颗星 median ratio 1.2766 (~28%) 是**观察到的差异，不是因果解释**。检验大气/airmass、透明度、PSF aperture-correction、FWHM、instrument throughput、响应条件与 flat 时效（相同 flat 的共同误差对两夜比值大部分相消）；按模型可识别的相对系数修正，不能虚构绝对增益/ QE/绝对线通量。

旧 FITS 原始 dtype unsigned16/BZERO32768，`65535` 是该文件的数字上界，不是未知 SATURATE 的测量值；饱和 DQ 按 FITS scaling/设备已知范围推导并记 **unknown true saturation threshold**，校准后的 NaN/掩膜要反映真实无有效数据。master FITS 校验二维具体 nx,ny、方向/裁剪/设备/时间和 BUNIT：bias/dark 是 ADU，flat 归一后无量纲，不允许“像素总数相同的转置平场”通过。CLI sky groups/gauge/response proof 缺必需字段必须拒绝，不允许空 Id、任意字符串和隐式 reference-frame 伪比较。缺失物理噪声保留 `noise=unknown/science=conditional`，不得造场景 IVAR。

### S1-P1-E：Sky/Coadd/排异/SNR/接缝的独立物理裁决

只在**同通带、可比较估计量**上 fit additive sky 差模，保证真实 M42 星云/暗云的共同低频结构保真，不把不同夜的相对比例差错扣成背景。对每个连通分量固定 gauge/rank；按 `P=I−EKT` 传播校正后的 \(P C P^\top\)，保留负交叉项、共享根和 unknown；无全噪声则不得宣称无条件 BLUE/GLS 或经验完整IVAR。非可识别时用科学正本允许的具名保守选项，**不允许任意平滑/羽化把接缝藏起来**。

所有 SNR/rejection/seam 测试由 Agent/独立审核者承担：
1. 同一输出单位/目标 WCS/共同支持内，比较成图前后星位/恒星积分、星云低频坡度、背景 signed offset/gradient、边界两侧差，区分光度乘法差和sky加性差；排除/单独报告 saturated 和强源区域，而非只在暗背景做“漂亮”接缝数字。
2. 有向接缝残差 \(\Delta B_{AB}(s)\) 与左右重叠邻带独立估计，给 robust median、MAD/相关噪声、支持像元/有效曝光数、标准化残差、残余低频谱及误差来源；评估是否符合实际噪声下统计可辨识域，无法分辨就标 conditional。
3. 以固定 seed 在**合成具有已知低频 M42-like structure/sky差模、PSF/WCS/噪声/共享根**中注入宇宙线、坏点、卫星细线与可注释瞬变，比较回收率/FPR及星云误删，不能把“排异后图更平滑”当效果。小 N=2/3 不强制 sigma clip；N≥4 只有可验证可比/分布条件才启用有依据的传统策略。
4. 经验/重复观测 SNR：对同一个估计量给可复算光度 scatter、空域/独立曝光交叉检验、有效曝光数N_eff，比较等权/模型权重在同条件下预测的改善与真实scatter；只在适用域评估约 \(1/\sqrt N\) 规律，未知 gain/read 不造绝对方差。
5. 实验判据先由无偏估计、独立真值或统计不确定度推出，记录样本数、输入条件及 CI；不可通过改阈值、剔除失败样本或更换 ROI 事后宣称通过。

## 3. S1 自主顺序（发现问题自己修，不反复停下提问）

**S1.0 快速核实与冻结**：读新旧 Issue、review 报告，核最新 HEAD；检查 M42 196 清单、已有 master/Gaia/calibration/crop/review 与剩余盘；确定唯一 write scratch，不重复生成已经 verified 的输入/模型。领取 Issue 写入起始SHA与最小阻塞。

**S1.1 先 fix fail-closed 与结构 Oracle**：用最小原语构造并修好 P0-A/P0-B 的 product/HiPS/FITS，加入独立 leaf-world/真球面支持验证。**只有真的修好 producer→consumer，才重新运行一次真实 256² crop**。不能输出全 NaN/非法卡而 exit0。

**S1.2 找出内存乘数并实现真实 streaming**：对原有 failing 1/2-frame mosaic 流水线做 dry-run/resource model + 已缓存产品的短 block probe；优化 P0-C，固定 12GiB process 限额与足够操作系统余量。验证低H、取消、seal失败、读取恢复，至少证明单帧和双帧 ROI 可以完整 reopen→mosaic→export。

**S1.3 修 PSF 与标度**：先在可用的低背景 ROI 做独立源测量，再以覆盖更广的、未饱和、高SNR恒星验证局部背景模型与位置一致性。修好 `photometry.fit/apply`、响应/仪器 conditional，说明 1.2766 的乘性标度差处理，保证没有非收敛结果进入相对校正。

**S1.4 真实全尺寸能力关口**：在**同通带**的 T2/M5 冻结三帧上用 4096² 原始观测，通过真实流程 `acsd normalize` 每帧→新进程 `acsd mosaic`→新进程 `acsd export`。先以确定科学足迹/有效响应/order 选择输出网格和空间预算；保证不复用错的 PinPoint WCS、不无限分配、只存受支持 tile。失败后自动回到故障 producer 定向修复和复用上游有效缓存，直到可得到正式单通带 FITS，不能以256²小图替代4096²通过。

**S1.5 由 Agent 做完整科学 QC**：上述 P1-E 的 sky/SNR/reject/星位/PSF/response/supported-area/extended lowfreq 以及外部 Astropy 严格 FITS头/数据、原始 product closure/method、完整单位状态复核。差模/增益无法识别需具名说明；若仍存在使后续四通道图失真的系统性问题，S1 FAIL并继续修，不是“用户自己看着调”。

**S1.6 独立审查**：由未编写被审代码的 Reviewer 对同一真实源码 SHA、正式缓存 hash、真实产品、单位、抽样、质量统计、低 H 与关键反例做独立复算。S1 PASS 至少意味着：三幅真实4096² Hα经过 normalize，三帧合法可比较、持久化/重开、mosaic与export产出合法二维天球WCS FITS；输出覆盖与预期足迹、有效信号与估计量、接缝/低频/SNR/排异科学诊断一致；实际RSS/磁盘不越预算；没有必需 P0/P1 未决或用 unknown 冒充 full。若Reviewer FAIL，Agent自行定位整改并重审；**不得提前启动全部196帧S2**。

## 4. S1 PASS 后**自动**建立独立 S2 Issue 并立即继续（不要向用户要新的提示词）

只有 S1 Reviewer明确 `PASS` 并冻结 S1 SHA、配置、输入/输出 hash、证据路径后：将本轮 S1 Issue 以 completed 关闭；用 GitHub 可用接口新建唯一 S2 Issue（标题：“ACSD-v2/M42阶段二｜196帧四通道全区域正式科学拼接与FITS交付”），正文直接引用本规范 §4–§7 和 S1 release manifest，包含实际硬约束及下列清单，立刻认领、实施，**不等待用户再次提出**。无 GitHub 写权限是明确工具阻断，才需报告而不能谎称 Issue 已建。S1 FAIL 时，绝对禁止创建/启动这个 S2 Issue。

### 4.1 S2 是**全 196**（逐帧处理与审计），不是三帧示范

将 `validation/m42/inputs_manifest.json` 中 196 个原始 light 全量作为 candidates，按照 `T2/T3 × M1…M6 × filter × night` 为每帧形成唯一状态：`processed` / `excluded_with_reason` / `blocked_input_with_reason`，不能遗漏、静默跳过或改称所有帧已经叠加。各通道预期输入：Hα 50、R 49、G 49、B 48。可以拒绝不可用数据，但要给源 SHA/真实证据、拒绝前后权重及保留贡献。**所有合格帧都要进入各自通道的最终求和**；不能拿每面板1帧/仅T2几帧替代全区域结果。

每帧只进行必要一次 calibrated/WCS/PSF/Normalize并缓存，每面板/每通道分块 sky/coadd，最终跨面板建立同一通道全局解（sky 的真实连通图与gauge需在最终合成前冻结），不要对拼接面板做有物理歧义的后期图片羽化。可用多步稀疏 accumulator 保留全局权重/真实足迹/重复块身份，不能重复计同一曝光。整套阶段二要验证 T2/T3 与不同夜间滤镜响应、PSF、曝光/相对标度的可比性；各通道内 T2/T3 名义同滤镜不等于严格相同有效响应，若无法证明则先输出分设备科学产品和具名条件说明，不能把不等估计量无条件混为真Hα/R/G/B。

### 4.2 S2 四通道正式目标产品（可复算、保留科学身份）

先以所有合格输入的 ICRS 足迹和真实像元尺度冻结一个**共同目标 TAN WCS**，满足覆盖全部 M1–M6 面板与外缘可支持区域，合理分辨率（不得只是为了资源不够强行粗化或裁出M42中心）。四个通道分别独立 `Normalize→Mosaic→Export` 并共享同一 WCS、尺寸和方向，产出最终四套普通 2D FITS：

- `M42_Halpha.fits`，涵盖候选 50 帧；
- `M42_R.fits`，涵盖候选 49 帧；
- `M42_G.fits`，涵盖候选 49 帧；
- `M42_B.fits`，涵盖候选 48 帧。

名字是交付文件约定，实际目录在唯一的安全 final root；若某通道不能满足科学可比和共同WCS条件，应**保持该产品未完成状态而不是生成“伪全量已完成”FITS**。允许输出各T2/T3通道的诊断/中间科学产品，最终四FITS不能因此被“部分结果冒充全量”。同一 FITS 可另以 IMAGE extensions 或关联副文件保存 coverage/n_eff/variance/quality/provenance，清楚区分科学信号与权重、曝光、DQ、response，不以普通 RGB 混色图冒充单通道科学量；可另附非科学预览供最终视觉查看，但**FITS 是唯一正式交付**。

每个通道的产品必须明确它的 estimand：同一个有效参考响应上的相对 sky signal 或有条件的响应混合，不得在四个滤镜间做未声明的加性sky公共拟合或将Hα当作R差模。饱和/无支持 NaN、合法负背景扣除保留，真实星云低频和亮星晕不因“消缝”被盲目滤掉。对四FITS严格读回 WCS/SIP、CD/CRPIX/CRVAL、像元角尺度、BUNIT、头卡、NaN掩膜、光度、不同面板天球拼合与通道重合。

### 4.3 S2 批处理执行算法（限定开销，不能“全196全重算”）

1. **一次预flight**：196个文件已哈希inventory复用；校准/response/Gaia有效性、缺失CD的30帧、各设备master与日期可用性；WCS解与正常帧的共享目录索引复用。按文件/算子版本/科学recipe做缓存键，存已完成/中断/无效状态及科学失效理由。
2. **真实调度**：按影像空间块/面板/通道逐步批量 Normalize，同时仅持有限个 H/R/F/E 配额；不要一次常驻196个完整Cov/CSR、点源表或全局图像；单并发基线达到资源上界后才能提高并发。上游单帧可原子提交后立即释放RAM。
3. **先全面积每通道可完成路径，再优化质量**：四个通道先冻结真实覆盖图和合法WCS/校准/单位，随后每通道按科学必要的全局SkyFit、共同Selection和Weights运行正式 coadd；只有改了某个上游science identity才invalidate相应产品及被它影响的下游块。**不要产生每一轮完整196帧中间副本或多份全区域临时FITS**。
4. **保存连续进度**：每完成frame/tile记录 `input+recipe+code hash`、复用/重算缘由、peak RSS、work、disk peak、output closure hash、故障与resume位置。无意义地重新扫描6.1GiB源或每次重建全部Sky/geometry是不允许的。最终将同一批分块数据以一次atomic publish交付正式四通道。
5. **空闲空间预flight**：前次报告`/workspace`剩约277GiB。必须先复查实际当时可用，预测四通道实际有效像元/叶/CSR与缓存/导出峰值，**保留至少50GiB或本机空间20%（取较大者）用于系统/其它项目**。不保证100%能装下时停止新批次、改压缩/紧凑存储/逐通道完成-验收-安全清理已无引用staging，不可删输入/仍引用product、不可通过科学降采样逃避预算。
6. **失败恢复**：坏帧、缺测量master、非收敛源或某个叶分块报错时先隔离数据/块、记录原因与scientific impact，只重跑失败对象/其下游；适当拒绝坏帧不能成为偷偷缩小总样本。遇底层 P0 要定位修好再让未受影响缓存继续，不让一个坏header破坏先前成功的其它通道。

### 4.4 S2 必须由 Agent 完成的科学检查（四个通道各自+跨通道）

- **WCS**：所有被选帧的留出星位残差、外部参考世界坐标、CD手性与可辨识域；30帧缺CD也必须有合法实际解；不存在一处明显偏移/旋转/缩放接缝。
- **相对测光**：多夜及T2/T3之间恒星/背景相对标度的空间、颜色、亮度残差。候选物理响应不同（特别Hα vs R）不强迫一致；有效混合响应必须可追踪。
- **PSF/flux**：逐面板局部与整场 PSF、残差/光晕/星云背景，使用未用于拟合的源和有物理模型的局部测光；不因孤立星筛选造成只有2星结果。
- **Sky seam**：针对全部 M1–M6 面板的空间重叠、有向差模图（含正负号、gradient/plane）、拟合前后标准化残差/低频保真/连通图/gauge，单帧不支持的外侧条件明确；不能把共同低频星云当待扣的sky。
- **SNR & rejection**：数据实际能力内的 V_F/V_B/V_ctrl、与共享根有关的可计算 C，重复帧/低背景 empirical scatter 和N_eff，按帧数增加验证或说明不成立；独立有注入真值的cosmic/satellite/transient排异 TPR/FPR，检查星云弱信号和真实瞬变未误删；不能只在强亮度中心验收深度。
- **支持和几何**：四通道的footprint、无覆盖NaN、单位、面积/flux/mean、无虚构出力，原始球面leaf→FITS输出Q、同一R/协方差的可追溯性。跨面板不同CD/可能不同PSF不能偷当重叠的同像素。
- **交付判据**：给出每个通道的输入总数、合格数、拒绝原因、有效积累/覆盖、主要SNR统计及其模型域、标度/接缝指标、剩余未知、产物 SHA和外部读回结论；四套科学FITS在共同WCS上可独立打开，最终只有用户视觉验收。

## 5. 自主管理标准：遇到失败不等用户催

**唯一主代理 = 执行负责人**；可委托只读科学审查、源与坐标验证、I/O FITS规范、资源剖析、实验统计等子代理。**实施代理不能自称对自己改过的代码做独立验收**。并行子代理可在不同 worktree 上独立审查/做对照，只有主代理按同一最新 HEAD 串行整合代码提交，避免不同Agent覆盖文件。每个主要子任务都要有：（a）可复现的数学/代码反例；（b）修复；（c）最小独立oracle；（d）一个真实M42有代表性输入；（e）影响范围/缓存失效说明；（f） Reviewer审查结果。

**连续执行循环**：`证据→根因/影响图→小修复→独立数学/源码复核→定向真实输入→保存可复用成功结果→下一个已知阻断`，直至 S1通过后执行S2。不要在第一个FAIL后写“需下一轮”，也不要为每个bug另开新Issue/整轮重做。若发生真正不可跨越的外部阻断（原始文件不可读、缺少唯一需要的真实校准且没有科学可接受的conditional模式、disk/权限/计算资源绝对不足、GitHub不可写），记录精确错误、前置调查和最小需要用户决定的事项，严禁虚构成功。

**CPU/空间效率**：三命令必须是真正正式入口；只读原始资料、已有master/XPSD/plate-solve/cache重用；按依赖有向图只让变化所影响的块失效。先 tiny synthetic/真实 ROI 定向证伪，成功后唯一一轮全尺寸输入；如果被审源码/科学配方变化需再跑，必须附改变的依赖子图。FITS preview、JSON报告和诊断不是第二套求解器、也不是可代替完整科学产品的替代品。无界生成失败目录/全零sky tile、重复文件或后台无限自检，均视作工程问题。

**不要被动停顿**：除确实需要外部授权/物理数据才能继续外，主代理在当前已获授权的开发环境继续到自己可以完成的最深依赖；每个阶段性报告是事实摘要，不是请求“下一步还做吗”。可在阶段边界交付报告，同时立即执行已授权的下一个步骤。代码/实际科学能力不足就修，不因旧规范写了PASS而忽略真实失败。

## 6. 主代理必须交付的证据格式（每个阶段一次，不刷日志）

- 起始/结束 HEAD、每个修复 commit、源码/二进制/配置 SHA，输入冻结manifest与外部依赖版本；
- 真实执行命令、分块选择/运行/复用/缓存失效与次数，失败attempt保留，Science model、单位与估计量、输出闭包与新进程 reopen；
- 数学/数值 oracle 的真实输入/期望/observed和不足条件（不仅“pass count”）、外部WCS/FITS/支持/噪声与sky独立对照；
- 每个通道/面板/帧数、DQ/排异/标度与独立留出数据，signed seam、低频场、星位/通量、实际scene SNR/不确定度/unknown；
- 内存峰值，最坏块/文件字节数、磁盘峰值/余量及哪部分删除或重用；科学容差/产品维度随版本变化记录；
- **正式最终四通道 FITS**的实际磁盘路径、SHA256、大小、WCS共同身份、外部读取证据、可安全交付给用户的方法（不可编造沙盒下载链接）；另提供必要科学QA小图和结构，不以彩色图替代FITS；
- 独立审核 `PASS/FAIL` 与逐项前置影响，FAIL需回到关联producer修复，科学不可辨识需诚实说明，不能靠审核判据绕过。

## 7. 最终管理约定和不可替代的完成条件

- **S1完成**：真实4096² 同通带至少三帧的 full三命令完整产物，独立可重开；结构/HEAD/FITS合法、无全NaN伪成功、负通量估计不可用于定标；12GiB资源边界真实成立、sky/rejection/SNR/低频/接缝独立评估，Reviewer 正式PASS。**只有此时才能创建并立刻执行S2 Issue**。
- **S2完成**：对全部196帧都有已处理/合理排除/输入阻断的逐个归属；能证明合格帧按 Hα/R/G/B 进入四个通道各自最终合成，六个面板和T2/T3都覆盖且没有强制混不兼容响应；共同目标WCS四个正式科学FITS均合法可读且经科学与工程独立审核。仍需用户最后的**视觉接缝验收**，但不能把科学检查推给用户。
- **一Issue一次交付**：#123以旧 S1 FAIL 转交给下一整改Issue；新 S1 Issue用于本轮修复/独立回收；S1 PASS后主代理自行建立/执行唯一S2 Issue。历史问题和实验旧FAIL保持证据，不关闭其它不相关旧Issue。
