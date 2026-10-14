# DeepSeek Agent 第二轮启动提示词｜独立审查后的科学实施

你现在继续 ACSD（fujiaze/Astro-CS-Database）的科学 V2 开发，分支 audit/acsd-science-docs-p5。用户的第一次交接记录了从 942c978e8 到 f8c011bad 的 7 次提交。**前台已在此基础上独立审查并提交新的数学更正、调试标准与实施裁决**；开始前重新 fetch 并检查实际远端 HEAD，不能直接用旧 checkout、旧任务包和旧假设开展工作。有未提交改动不得覆盖。无 PR、不动 main、不上传 /testdata 原始校准 FITS/HST 大文件。

## 必须先读（按此顺序）

1. docs/ACSD_DESIGN.md
2. docs/science/unified/UNIFIED_SCIENCE_MODEL.md、docs/science/noise_snr/NOISE_SNR.md（特别 §4.2 合法协方差）
3. docs/engineering/architecture/DEV_MODE.md、docs/engineering/standards/DIAGNOSTIC_PROBES.md（特别 §9A）
4. 实验/engineering-evidence/acsd-science-docs-p5/SCIENCE_V2_AGENT_INDEPENDENT_REVIEW_F8C011BAD.md（**本轮裁决正本**）
5. 实验/engineering-evidence/acsd-science-docs-p5/SCIENCE_V2_AGENT_TASK_PACKAGE.md（已新增本轮附录）
6. 必要时查 photometry/PHOTOMETRY.md、drizzle/DRIZZLE.md、sky/UPM.md、projection/HIPS_TO_FITS.md、resample/RESAMPLE.md。

前台继续负责最高设计、science 和工程规范。你负责 C++ 代码和独立实测，不能未经证据擅自更换基础物理公式；如有反例提交数据供前台订正。

## 第一阶段：修复审查发现的明确问题，并产生真实对照

1. 取消 Phase1 内仍存在的 PHOT_PASSBAND_IDENTITY_INCONSISTENT 对合法混滤镜帧的拒绝，但**不能删除检查后沿用第一帧响应**。要根据真实每帧 filter/QE 和 Gaia XP 模型独立定标，若为效率分组则内部自动分组，不限制用户提交混设备/混滤镜任务。增加测试：同组 R/H-alpha、第一帧未知通带、滤镜响应不同但合法 WCS；没有测光模型时要如实报告未知参数，不伪造色彩校正。
2. 修复 mosaic.integration.signal_tile probe **误把 flux_sum=signal×covered_area 当 ADU/sr 的 signal** 的问题。分开原样 flux_sum/covered_area/归一后 signal，保证对应的单位、HEALPix NESTED→FITS row-major、NaN/有效零和负值；从真正最终 HiPS 读回逐像元对拍。Normalize SNR probe 必须按实际 gain/snr_caliber 声明条件上界或真实绝对方差，不能冒充真正稀疏控制点。
3. 修复 R3 Python 相关核：旧 corr_eye(n=4,rho=0.7) 最小特征值约 -0.132624，非协方差矩阵；更换经过 SPD 验证的 K，重跑所有 7 例，保存 λmin/条件数/新 T 值。现有正例 Python 尚未真正测 C++ weight_chain；接入真正 C++ 数值算子的独立阴/阳反例，不以结构恒等式自证。
4. 修复 p2 writer 的 mixed passband 首帧空字符串缺陷：first.empty() 不能作为是否赋首帧身份的哨兵。保存 frame_count 和 distinct_passband_count，未知滤镜不得被写成已知通带。P2 products=[signal] 时不要让内部 uncertainty_available 形成误导性“已发布 variance/ivar 图像”元数据。
5. dev_diagnostics 目前在全局 mutex 内直接进行 FITS/JSON 写盘及 SHA-256，可能阻塞计算；根据探针规范做短锁选择+有界只读拷贝+独立低优先 writer 和有界 flush，sidecar/hash 成功后才报告 WRITTEN；验证跨运行复位、并发、取消、磁盘错误。不要新造 timer/AIO。保留 ACSD_DEV_MODE 唯一公开开关与 OFF 无诊断符号原则。

每项修改先列源函数/数据语义/已存在消费者/会影响哪些产物，再提交代码，保存能真实触发旧错误的反例和修复后的读数。不要因前述问题跳过真正的科学重构。

## 第二阶段：实现用户真正要的稀疏精度链，而非只写额外报告

目前阶段二生产实际走 per_sample_ivar，snr_path_effective=not_used，说明 P1→P2 稀疏 SNR 尚未跑通。前台裁决为：**允许实现稀疏 T 模型并与现有 IVAR 对照，但不能在缺证明时盲切默认**。

实际代码必须包含：

- 从 P1 真正生产 canonical reference SNR 控制点：同一 F_ref,k 标度、局部 PSF/相关噪声、条件 σ_F、SNR_caliber、天球坐标和空间支持；查清旧 HioSnrControlPoint.snr_psf=(A−B)/MAD 与绝对参考点源 SNR 不是自动同义词；
- 使用真实写出的 Catalogue HiPS 或单一 sidecar，由 P2 真正读取，构造局部插值模型/支撑域，现场得到预测 V_F；不要再单独等待不存在的 ACSD_SPARSE_SNR_LAYER JSON；
- 对有效 HEALPix 面亮度被估量，计算具名 T=V_B/V_F，w_B=(SNR_ref/F_ref,k)^2/T。T 需有 PSF/面积/Drizzle 相关核/背景 nuisance 的实际依据；如果 K 不可辨识，明确近似/不可用，而不是自动 T=1；
- 在**真实生产 C++ 流程**提供 selectable sparse_T_model 与 per_sample_ivar 参考臂/影子诊断，实际采用哪条必须如实记录，并可与同一输入的独立重复帧/合成真值比较；
- 为用户不同望远镜、相机和通带联合合成保留物理解释限制：不同 PSF 的加权面亮度不是天然同一个无卷积天空，未知星云 SED 也不会被随机逆方差加权消除。

## 第三阶段：科学 E2E

优先利用已经有 WCS 的真实 M42、LDN43、NGC1727 等数据，实跑跨滤镜/混设备（合法天区必须重合）、以及 2 帧及更大样本。针对 M42 30 帧及 GalaxyCenter 1 帧缺 WCS，可用 OBJCTRA/OBJCTDEC 作 plate-solve 初值，必须有独立配准残差后才生产 sidecar，不猜 WCS、不覆盖原始帧。

完成 P3 真实 HEALPix 曲边几何独立 oracle、混合分辨率 target_order 用户可控及星点/扩展源效果对照；P5 连通分量与真正星云低频接缝验证；HST 成品仅作为干净结构模板做传感器/PSF/噪声前向模拟，不能当未经重采样的噪声真值。把用户可见的全景、亮星/接缝局部与低频星云拉伸及真实普通 FITS 留在计算节点。

GAIN 未知时优先消费已有 snr.gain_e_per_adu 的有来源设备配置、FITS header 或独立 PTC（如果有合格 flat 对）；不虚构 g=1。允许用户继续合并，标注条件背景主导上界、不当作绝对 5sigma。ACSD_*_FAULT 移到测试 harness 中，正常发布和普通 DEV_MODE=ON 都不能因为环境变量改变科学答案。

## 提交、证据与边界

所有改动推送到 audit/acsd-science-docs-p5，不需 PR 和形式化审批。每项提交给出 Git SHA、真实编译/运行命令、输入元信息/哈希、实际命中生产路径、被估计量单位/坐标、正反例和未运行项目。单独保存 Normalize ON/OFF 的 1496 项机器 diff JSON；现存 run/A/onoff_compare.json 是**另一次 D02 改前/后**188 项数据，不可当作同一证据。

不要求所有测试形式上变绿，但不能隐藏失败、不允许更改真值去适配实现，也不能把 scipy/numpy 单独重算的公式叫“C++ 生产已修复”。发布科学产品仍是 Phase2 单 signal HiPS、Phase3 普通 FITS，调试快照和权重只放开发工作区。先完成审查问题与真实稀疏链的科学基础，再按实际算力推进曲边/HST/全链。**本次完整提交后报告事实，前台会再次独立审查；没有跑的事项如实标未运行。**
