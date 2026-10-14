# DeepSeek 第二轮科学实验执行包（论文 P1–P5）

> 启动约定：先 `git fetch origin && git switch audit/acsd-science-docs-p5 && git pull --ff-only`。不得重建分支、不得把旧文档覆盖回本轮修订稿、不得修改 `main`、不得自动合并。**执行分支最新 HEAD 为唯一基线**：启动时重新记录完整 `git rev-parse HEAD`，不得从本说明硬编码旧 SHA。运行主文件：`实验/README.md`、`实验/shared/FIVE_PAPER_EXPERIMENT_PLAN.md`、五篇 `REPORT_paper.md`、`实验/shared/references/CORE_REFERENCES_VERIFIED.md` 与 `PRIOR_ART_CLAIM_MATRIX.md`。本包只给计算节点仍需完成的任务，文献整理和论文方法重写已经落仓。

## 总目标与纪律

交付五篇 **研究结论可证伪、引用可追溯、真实数据/模拟/生产三重分离** 的独立小论文，切勿再创建五个并行“新论文文件”；只更新各单元既有 `REPORT_paper.md` 的**结果、图表、讨论与证据级别**，保留当前科学方法和本轮已核对 DOI，若真实发现其物理错误则必须给一手文献、数学推导、反例和同批修订意见。

不可上传 `testdata/` 下原始相机 FITS 或 HST 原图；只入库其合规元数据索引、内容指纹、观测摘要与可重建合成配方。其余 `run/` 仅临时使用，科学记录须写回 `实验/<unit>/REPORT_experiment.md`（追加对应本轮复验专节，不删除历史章节）；真正验证/失败的数字连执行命令、构建 SHA、输入列表一同记录。

### 不许自行改动的科学边界

1. P1 的 XP 合成通量计算需记录 photon/energy response 单位，G 不重复乘；不许把未标定的 0.0007 mag 写成一手外部常数；不得以调大阈值令真实帧变 PASS。
2. P2 条件点源 `SNR_ref` 与弥散源像素逆方差是不同估计目标；自由背景、噪声相关、系统零点均须分报；不许把 2.79 倍直接当作真实噪声可用的全局修正倍数。
3. P3 几何分割数学恒等式 != 生产几何边界算法无误；face-center/apex 如在合法域，不能因旧位置集未扫而豁免；独立几何 oracle 批准前不能把错误“修好”为错误分母上的再归一化。
4. P4 固定参考通量 `F_ref` 的条件 `V_ref(x)` 与实际场景 `F_scene(x)` 分开；自然样条不具普适单调/无过冲保证；域外必须可检测。
5. P5 乘性增益默认恒等，直到恒等阴性对照、留出星光度和弥散源保真均通过；对各重叠连通分量分别给 gauge 与残差；无绝对锚时不能宣称恢复真实天光共模。
6. 所有 prior art 检索结果按确切文献论点入 `PRIOR_ART_CLAIM_MATRIX.md`；找到文献先例并不等于五篇都没有可量化的工程/方法贡献，反之也不能把算法组合自动称作“原创”。

## 任务流水（顺序由科学依赖而非代码目录决定）

### S00 基线、环境与数据真实性

- 固定 `HEAD`、`git status --porcelain`、`git ls-files` 与构建 SHA，导出 CMake preset、编译器、Python/astropy/scipy、CPU 与运行时间。
- 读取 `testdata/` 的本地可见实际校准帧，建立轻量索引：frame_id、探测器、滤镜、原始头 WCS/CRVAL、星区、曝光、BZERO/BSCALE、像元大小、SHA256、校准状态。不存在/不可读一律写 `MISSING`，禁止补假数据。
- 对 HST 已叠加成品建立独立索引和合成噪声配方。不得把已经 drizzle 的 HST 图像自身噪声当作未重采样独立随机输入。
- 数据与衍生实验产物按许可分离：FITS 不上传，摘要 CSV/JSON 和重建代码可入库。
- 交付 `实验/shared/data_catalog... `（优先兼容现有 schema，不新增重复权威清单）、`run/<round>/S00/` 执行日志、公开论文中的 Data 表。
- 失败关闭：实帧无数据则 C 臂 `NOT_RUN`，不能用合成输出伪装真实。

### S01 文献主张与公式双向审查（无计算服务器需求的工作已经由本轮提供基线）

- 按共享 Bib R01–R12 核对每个参照的具体 DOI、论文观点与**未支持**的推断；已有基本书目已核验，不必重复无目的大搜。
- 真正要写入数学公式的外部式号必须打开原出版社论文核对、在登记中给式号/页码；做不到仅记为 `BIB_VERIFIED` 并以独立推导/数值验证补。
- P1 重点核对 Gaia XP 光子积分的 SI 单位与 XP 采样有效域；P2 GLS 背景拟合与 resampling covariance；P3 HEALPix 真球面曲边；P4 自然与形状保持三次样条；P5 NSG 官方“恒星 scale + surface spline gradient”与 PMM 本体不可凭入口页面脑补公式。
- 交付五篇 Table 1、共享 `PRIOR_ART_CLAIM_MATRIX.md` 新证据增量；文献观点与实现结论不能混为一条。

### S02 P3 独立几何 Oracle —— 最高数值风险

- **先核物理量**：报告 `−3.209e-3` 究竟是 `epsilon_j=sum a/A_drop−1`、局部 `B_p` 误差、还是别的分母；与历史 `6.6e-12` 同量同域才能比较。数值字段带量纲、分母和源坐标。
- 实现不复用生产球面分割函数的 Oracle：双精度高细分曲边积分/任意精度回归参照、全候选叶穷举、曲边逼近级数收敛。对 RA=45° face 中心、drop 压叶顶角、面缝两侧、极区、大小交叠与 `nside=512..16384` 扫描，`pixfrac={0.5,0.8,1}`。
- 给每 drop 权重和、全通量闭合、常量场偏差、每叶面积与候选漏选计数，按最大值/分位数和位置热图输出。记录例外处理的状态，不能只报 median。
- **通过标准**：同物量、独立 oracle、真实生产调用路径和原误差构型一起闭合；若发现生产缺陷，单独补生产几何代码+明确负例后重复整套，不靠放宽门。若是科学方法内固有曲边近似误差，计算可校准的合法产品域并由负责人复核发布边界。
- 交付 P3-B/E 的图 2/3/4、`REPORT_experiment.md` 补章与 P3 论文结果。

### S03 P5 连通图与纯加性基线

- 在生产 `sky_plane.cpp` 重叠约束集合上枚举分量，分别选参考 frame_id、固定自由度、记录每分量秩/条件数/有效边/支持比例和质量状态。
- S0、S9、S9b、链、环、细桥五种图，读数分别给各分量；断连分量不可被全局 `ref_frame=0` 模型当作有绝对相对零点。S9b 原报告 compB `~0.1281` 作为必须翻红的旧反例。
- 记录修正前后 `χ²_red`、`identifiable` 与实际接缝、但不允许用收敛小或满秩简单替代结构误差。
- 交付 P5-C/F，独立提交实现修复，五篇正文可引用该生产 SHA。
- P5 的其他消融必须用**修复后的纯加性基线**作参照，否则将规范漂移改善错归因于乘性校正。

### S04 P1 真实帧光度与颜色误差归因

- 合规选帧，重跑原 `step8_real_frame.py` 修订前后对照（保留散粒 ADU² 量纲），输出每星 frame_id、参考通量、残差、颜色、位置、星等、饱和域、通带与质量位。
- 对 `sigma_flat=0.0007 mag` 的来源做**独立**平场/重复位置检验，量化这一假设如何改变 `sigma_ceiling`，不能由 `sigma_obs` 反推。
- 星等色差系数须明确自变量，二轮独立仪器数据的训练/留出/置信区间；检查真实探测器线性区与 PSF 模型。原失败 `0.026520>0.020561` 保留为旧预算结果。
- 并行验证 P1-A/B 的 SI 光子/能量响应、谱形颜色注入、空间增益阶数过拟合负例。
- 交付 P1-A～D、表格、残差图与 `REPORT_paper.md` 结果段更新。

### S05 P2 噪声/协方差/真实 coverage

- 白噪与光子噪声、自由背景、线性 R 重采样后高相关/独立底、点源/弥散源四臂由独立 Oracle 生成真值。
- 对 117 颗共同星复核 frame-paired z 分母：同星差应按两个观测方差及其共享项传播，星等零点/颜色/位置趋势应由不重叠训练集预测到留出集，分析前后分层 coverage；报告统计区间和有效独立样本数。
- `k_corr` 检查源码 `scale_arcsec` 参数实际物理意义，现 300–600″/px 标定表不能直接被改成 0.3–0.6；对真实 nside、pixfrac、控制 N、插值相关性做 MC。证明 1.4 的保守范围否则标记 `UNCALIBRATED`。
- 交付 P2-A～F 与详细噪声分解；`sparse_snr_layer` 从生产者到消费位不得只给设计图。
- 对 P3 修复前后分别复算 covariance，防把 P3 异常当成 P2 随机噪声。

### S06 P4 重建统计目标与域外失败

- 冻结两个对象 `SNR_ref(F_ref fixed)` 与 `SNR_scene(F_scene(x))`，对每个算子核明确具体输入/输出量纲、几何与支持域。
- P4-A～F：常数/缓变/结构场、空洞、2×2/3×3、过冲 `[0,2,0,0]`、0 距/NaN/NaN masks、真实控制点、源光子项注入；比较 natural spline+clip、IDW(p=1/p=2)、bilinear、constant、真实 dense。
- 主要结果为留出方差比、重建偏差、域外百分比、下游同一被估量权重效率；必要时修改算子状态，不能只将负值 clamp 后自称无偏。
- 统一生产 `idw_power` 的有效默认和 API，不能只改一个路径的文档值。
- 交付 P4-A～F，跨 P2/P5 一条真实使用链。

### S07 P5 乘性增益研究与非退化测量

- 在 S03 的分量算法和 S05 的量纲/噪声基础上建立每帧残余**标量** `g_i=exp(eta_i)` 的研究臂。来源是独立匹配恒星的扣背景积分通量比、色差和 PSF 条件，不是把背景梯度线性回归斜率当作星光度证据。
- 固定阴性世界 `g=1` 与阳性世界 `g≠1`，加性背景/随机种子/PSF 组成一致，比较 A-only、A+G、feather-only、无校正。**假阳性在阴性世界出现即不能默认启用 G**。
- 扩展场研究需要独立空间增益锚与明确模型自由度；不准这一轮用 2D gain 强行追平接缝。
- 测旧报告真实 49 帧 `114/196` 条边界：将 82 条分原因统计，只有观测有足够支持的边界计入科学门，缺失不是零；补法向多尺度诊断与 PSF/扩展源保真。
- 产物必须区分 `NOT_EVALUATED`、`IDENTITY`、`APPLIED`，真实不存在 gain 代码则明确 `NOT_IMPLEMENTED`；未通过阴性/留出测试保持生产默认恒等。
- 交付 P5-A/B/D/E/F（C 已在 S03）与论文结果；保留失败和不同模型不显著的证据。

### S08 跨篇误差传播与五论文最终结果

- 冻结至少一套完整相机/滤镜/天区的 P1→P2→P3→P4→P5 传递用例，明确是否是点源或弥散源被估量，记录每步单位和相互协方差。
- 单独注入 P1 标度偏差、P2 噪声低估、P3 漏候选、P4 插值域外、P5 差模规范缺失五个故障，检查下游门能否红。
- 更新五个 `REPORT_paper.md` 的结果/图表/结论，Table 1 相关工作、Table 2 数据、Table 3 假说、Table 4 结果，并按不可复算、真实 fail 或代表性达标情况给准确主张；不允许只贴 JSON 而没有统计解释。
- 复测索引/文档路径、Markdown 相对路径、BibTeX DOI/引用键、Git diff；只修改与本轮结果有证据支持的科学正本，不扩大裁决。
- 交付 `实验/shared/FIVE_PAPER_EXPERIMENT_PLAN.md` 验收对照表和每个实验单元明确状态。

### S09 最终回归与回传

- 在独立构建目录运行 CMake/Ninja、编译/单测/科学负对照及三个命令的合规端到端，必要时 Linux/Windows 双平台分开报告；不会跑则 `NOT_RUN`。
- 每一科学补丁和论文结果至少独立提交一次；如果一次性提交大量不可复算试验数据要先拆开。Git 只存必要的脚本/摘要，不存原始 FITS 或过大的中间文件。
- 回传 `SCIENCE_HANDOFF_REPORT.md`（放 `实验/engineering-evidence/acsd-science-docs-p5/` 或对应新过程证据目录，不放仓库根），含完整 HEAD、全部 commits、每篇主张状态、关键数字附 SHA、失败/未决列表、下一轮需人工裁决事项。
- 不开 PR、不改 main、不用 `--force` 推送覆盖其他人的主张；推送分支后给回传 SHA，由 ChatGPT 独立复核。

## 验收格式（机器与人同时可读）

```json
{
  "paper_id": "P5",
  "experiment_id": "P5-C-S9b",
  "base_sha": "git-full-sha-at-run",
  "source_sha": "git-full-sha-after-change",
  "data_manifest_sha256": "real-sha256-or-null",
  "command": "actual-command",
  "status": "PASS|FAIL|NOT_RUN|UNVERIFIED",
  "evidence_level": "ORACLE|PHYSICAL_FORWARD|REAL_DATA|PRODUCTION",
  "metrics": {
    "metric_name": {"value": null, "unit": "dimensionless", "denominator": "explicit-definition"}
  },
  "negative_control": {"injected_fault": "description", "must_fail": true, "observed": "NOT_RUN"},
  "limitations": ["list of unsupported domains"]
}
```

以上 JSON 是接口示例，字段中的示例字符串不对应已运行数据。所有输出必须贴真实数值或 null，严禁由文档阈值生成“通过结果”。

## 负责人需要再次介入的情形

- 需要扩大/收缩 P3/P5 的科学有效域或发表主张；
- P1 真实门失败而预算与数据均合规，出现两种物理模型都能解释的歧义；
- P2 点源/弥散源分面的产品合同拟修改最高设计；
- P5 乘性增益阳性和阴性控制不能区分或共模无外部锚；
- 试验结果与一手文献、现行顶级合同发生实质矛盾。此时先独立复算并登记为 `UNRESOLVED`，不要为项目进度反向删除物理反例。
