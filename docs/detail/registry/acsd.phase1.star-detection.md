# 模块 acsd.phase1.star-detection

> 上游：`docs/ACSD_DESIGN.md`「模块与 ABI」一节、「节点流程」一节（星表引导检测与WCS解算）、
「P1 通量积分拟合」一节
> 科学正本：docs/science/detection/STAR_DETECTION.md（SCI-P1-STAR-001）、
> docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md（ALG-STARDET-001）、
> `docs/science/algorithms/common/GATES_AND_TOLERANCES.md`「冻结门表」一节、
> `docs/science/detection/STAR_DETECTION.md`「亚像素质心」一节（质心/矩不确定度）
> 数据正本：docs/detail/registry/acsd.phase1.star-detection.md（DATA-P1-STAR 端口表，本页输入输出端口表）
> API 正本：docs/engineering/api/PUBLIC_API.md（API-STAR-001）、
> docs/engineering/api/PUBLIC_API.md「分阶段 API 面」（API-P1-003）

模块词汇 `acsd.phase1.star-detection` 是本页文档路径与模块卡片名，不是生产端口注册表的
`module_id`；生产端口图上承担检测 + PSF 建模的节点模块是 `acsd.phase1.star-psf`（本页
与 `acsd.phase1.star-psf.md` 同属一张登记面，两卡各写一面职责）。模块合同
owner = SA-P1-S15。冻结合同 = `lib/algorithms/star_detection/README.md` +
`module.yaml`（MOD-acsd-phase1-star，dll_target=acsd_p1_star_detection.dll，
entrypoint 未落地）；权威签名头
`lib/algorithms/star_detection/include/star_detector.h`，生产实现
`lib/algorithms/star_detection/src/sdet_api.cpp`（检测内核 `sdet_detect_impl`）。

## 职责与明确非职责

职责：检测图像中的源（星点/延展源），输出位置、质心/矩、源身份与 selection
function / completeness 参数。权威检测范式 = **星表引导拟合**（检测定义域是星表
位置，用本帧已解出的权威 WCS 把 Gaia 星表反向投影到像素域，只对星表位置做
质心/PSF 拟合；拟合成功即星点，失败直接丢弃，不计虚警、不报错）。

生产实现 `sdet_detect_ex` 走**全图盲检测**路径：peaker 七步候选（11×11 局部
极大 / 3×3 meanhigh / 二阶导零交叉 Sr,Sc / 振幅 Ar,Ac / 盒半径按 3.7172·S 向上取整
/ 对称门 / 候选去重）+ Moffat4（GSL trust-region LM，7 参数）逐候选拟合 + 饱和星
（edge-walking 中心、A > dynrange 标记）+ mag 排序去重截断（`SDetParams.maxStars`）；
FP32/FP64 双通道。**全图盲检测连通域路径不是权威路径**，只服务非权威的诊断/初值
用途。

不做：图像灵敏度本身；PSF 批拟合/θ 消歧（star-psf 模块）；匹配 / WCS 求解
（wcs-platesolve / gaia 客户端）；最终测光（photometry 模块）；背景估计与
cosmetic 校正（消费 cleaned 帧）；排异（Phase2 推断）。检测目录是下游输入，
不直接成为科学权重。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `image` | `DATA-P1-COSMETIC` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `star_det` | `DATA-P1-STAR` | 可 | 位置 px / 通量 ADU / 星等 mag | `CoordinateFrame::PIXEL` |

输入 image = FP32 通道 uint16 量化（登记见STAR_DETECTION_ALGORITHMS［A-1］）/
FP64 通道 double 不降级，加 `SDetParams`；另有逆投影先验，取值优先级见本页
「Registry descriptor 与配置 schema」的先验链（显式天测键 → 本帧解算产物 →
初始指向 + 旋转 / 视向）。

输出 `star_det` = FLOAT64 `[N,6]`，列 x, y, flux, mag, saturated, has_saturated；
x/y 为 0-based double 像素坐标（像素中心 = 索引 + 0.5）、flux 为 ADU（正常星 =
振幅 A）、mag 为 float（NaN = 无效）、saturated/has_saturated 为 int 0/1。编排
序列化另有 `star_det_psf_compat` FLOAT32 `[N,4]`。

检测、PSF、WCS、测光、SNR 的 source row 绑定同一 `frame_id` / `source_id`。
检测路径不消费逐像素 variance/ivar。输入全 NaN / 全饱和 → 拒绝并记录，不产出
空目录冒充成功；边界源标记边界 flag。

### 数值落地口径与实现落点

检测阈值的冻结定义正本 = SCI-P1-STAR-001（语义要求）、
STAR_DETECTION_ALGORITHMS［A-1］（离散公式）、docs/science/algorithms/common/GATES_AND_TOLERANCES［A-1］（冻结门表）；
本页只记落地方式：阈值 = 全图中位数 + 阈值倍数 × 全局背景噪声 RMS，倍数由
`detection_threshold` 给出（默认 5.0），作用在 σ=2 平滑图上，`bgnoise` 由 FnNoise1
行差分族估计，实现落在 `sdet_detect_ex`（`pr.thr` 由 `pr.bg` 与 `pr.bgnoise`
推出）。该阈值**仅适用于全图盲检测路径**。质心取一阶矩，二阶矩给出形状，误差
来自局部噪声传播；局部噪声自适应为目标态、当前未实现（GAP 登记）。

两条实现路径：

| 路径 | 入口符号 | 说明 |
|---|---|---|
| 权威（星表引导拟合） | `sdet_detect_guided_ex_f64` | 定义域 = 调用方给的星表预测位置；逐位置做饱和判定、σ 估计、椭圆高斯拟合（与盲检测同一 `sdet_gauss_fit` / 仓内自研 trust-region LM 7 参）与 `reject_star` 质量门；拟合失败直接丢弃。统计量 `SDetGuidedStats{n_predicted,n_dropped,n_fit_failed,n_rejected,n_fit_ok,n_output}` |
| 诊断/初值（全图盲检测） | `sdet_detect_ex` / `sdet_detect_ex_f64` | 平滑（含背景统计与饱和岛预标记）→ 阈值化与 8 连通组分 → deblending 解混 → 局部极大峰 → 边缘行走与零交叉宽度 → 拟合盒/对称门 → 并行 trust-region LM 拟合 → 质量排异；保留，非权威路径 |

节点侧接线在 `lib/infrastructure/scheduler/src/module_adapters.cpp` 的
`p1_op_star_psf_impl`：星表位置由 `p1_guided_predict` 产生（`p1_guided_approx_wcs`
给出的逆投影先验做 sky→pix 逆投影）。先验按固定优先级取：① `star_detection.approx_wcs`
/ `wcs` 段的显式天测键 → ② 本帧解算产物 `<frame_dir>/p1_wcs.json`
（`p1_guided_wcs_product_prior` 解析，天测可用性用与 photometry 同一判据
`p1_wcs_astrometry_usable` 确认）→ ③ `wcs.init_source` 初始指向 +
`rotation_deg` / `parity`。极限星等复用 `ipv::estimate_mag_lim_iterative` /
`ipv::compute_fov_density`，不另立常数。

**节点序上解算先于引导检测**：`platesolve` 节点按帧自读校准后像素自行检测与
匹配，不消费检测产物，其近似指向由 `wcs.init_source` 给出；引导检测再以该权威
WCS 作逆投影先验。

## 公共 header、核心 symbol 与生命周期

模块级 API = API-STAR-001（docs/engineering/api/PUBLIC_API.md），现行 6 个导出符号：
`sdet_create` / `sdet_destroy` / `sdet_detect_ex` / `sdet_detect_ex_f64` /
`sdet_detect_guided_ex_f64` / `sdet_free_detect_ex`。编排级 API = API-P1-003
（docs/engineering/api/PUBLIC_API.md「分阶段 API 面」 底层模块函数登记，一帧一次权威检测）。全量签名清单见
docs/engineering/api/PUBLIC_API.md。

`n_pred = 0` 返回 rc = 0 + count = 0（空定义域非错误）；指针参数非法返回 −1。

### Production callers

生产调用点 = `run_stage_psf`（PSF/STAR_MEASURE 阶段），经 orchestrator.cpp：
`sdet_create` 参数构造、FP64/FP32 通道、权威块 `star_det` FLOAT64[N,6] 写入。
PLATESOLVE fallback 读块，禁止重检测。

### 源文件

`lib/algorithms/star_detection/`（生产源 `src/sdet_api.cpp` 的 `sdet_detect_impl`、
`src/nls_lm.cpp`）。`lib/algorithms/star_detection/wrapper_phase1/` 为 P1-003
桥接层的独立 sigma-clip 实现，与生产 `sdet_api.cpp` 非同一算法路径（matrix
legacy_paths 第二路径）。

现状构建有两条路径：`lib/algorithms/star_detection/Makefile` → `star_detector.dll`
（orchestrator.cpp 显式加载，失败即错）；根 CMakeLists 目标 `acsd_phase1_stars`
（STATIC，wrapper_phase1）。

## Registry descriptor 与配置 schema

module_id=`acsd.phase1.star-detection`; execution_class=`cpu_heavy`;
parallel_ok=True。

节点 `star-psf` 从输入的 `star_detection` 段解析检测模式（同一键也可回退到 `wcs`
段的 `gaia_data_dir`），**模式与输入在节点级一次解析**，逐帧不再重解析。该段是
合法配置段：合同声明 = `eng/contracts/schemas/phase_config_normalize.schema.json`
`#/$defs/star_detection_config`（块内与平铺两形态同面）；CLI 认键面 =
`lib/infrastructure/cli/session_commands.h` 的 `config_fields(SessionId)`（逐会话
字段声明，`SESSION_NORMALIZE` 为 normalize 会话）与 `lib/infrastructure/cli/parser.cpp`
的 `session_keys()`（平铺会话键集）。**缺段不是错误**，全取编译期
默认；段内未知键被 schema 拒（`additionalProperties: false`）。

本段一切「规模/上限」类数字都由配置键与星表查询口径导出：检测定义域 =
`star_detection.max_stars`（设计自定算力上界，非科学常数，语义正本 =
ACSD_DESIGN.md 相应章节）；拟合样本上限 = `photometry.fit.max_stars`（默认 5000）；
交付 SNR 样本上限 = `snr.max_sources`（默认 0 = 不限，只截断交付样本行、**不**
截断检测定义域）。三者各自只承担自己的口径，合同域正本 =
`phase_config_normalize.schema.json`。

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `mode` | `auto` | —— | `auto` = 有参考星表且取向先验可得时走权威路径，否则**显式降级**为 `blind_diagnostic` 并把原因写进 manifest 的 `detection_degraded_reason`；`catalog_guided` = 显式声明权威路径，前置条件不满足即 DATA fail-closed；`blind_diagnostic` = 显式声明的非权威诊断路径 |
| `gaia_data_dir` | 无 | path | 本地 XPSD 星表目录（权威路径必需；亦可用 `wcs.gaia_data_dir`） |
| `max_stars` | 20000 | 颗 | 检测定义域上限（按 G 星等升序取 top-N）。合同域 = [20000, 50000]，越界即 DATA 拒绝，禁静默夹取 |
| `approx_wcs` | 无 | —— | 逆投影先验的显式覆盖：天测键 `{crval1, crval2, cd11, cd12, cd21, cd22}`。给出即优先于本帧解算产物。**六键必须齐备**，缺键即 DATA 拒绝，禁静默回退 |
| `rotation_deg` + `parity` | 无 / `pos` | deg / `pos\|neg` | 逆投影先验的兜底给法（无解算产物且无显式 CD 时生效）：像面相对「北向上/东向左」的旋转（逆时针为正）与镜像标志；板尺度由 `wcs.init_source` 派生的 `s0` 给出 |
| `limiting_mag` | 由焦距/画幅/曝光派生 | mag | 显式指定极限星等；缺省时由 `ipv::estimate_mag_lim_iterative` 按 `focal_length_mm`、画幅、`EXPTIME` 迭代派生（宁多勿少） |
| `limiting_mag_safety` | 3.0 | —— | 极限星等迭代的目标星数倍率（目标星数 = 目标星数基值 × safety）。缺省 = `ipv::IPVSolverParams::m_lim_safety` |
| `query_radius_factor` | 0.55 | —— | Gaia 查询半径因子（查询半径 = FOV 对角线 × 因子）。缺省 = `ipv::IPVSolverParams::gaia_query_radius_factor` |
| `limiting_mag_max_iter` | 4 | 次 | 极限星等迭代的最大 Gaia 查询次数。缺省 = `ipv::IPVSolverParams::m_lim_max_iter` |

**逆投影先验是权威路径的必需输入**：星表逆投影必须知道像面取向与镜像。生产默认
来源 = 本帧解算产物 `<frame_dir>/p1_wcs.json`（`platesolve` 节点先落盘，由 IR 的
typed 边 `artifact:p1_wcs` 保证序）；配置的 `approx_wcs` / `rotation_deg` + `parity`
是覆盖与兜底。两种来源都不可得时：`catalog_guided` 直接 DATA 拒绝，`auto` 显式
降级（`detection_authoritative=false`），**权威取向只出自已解 WCS 或显式声明的
先验**。先验来源逐帧记入 manifest 溯源（`approx_wcs_source` /
`approx_wcs_orientation_assumed` / `orientation_from_solved_wcs`）。

全图盲检测路径的键（诊断/初值，非权威）：

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `detection_threshold` | 5.0 | σ（全局 bgnoise） | 全图盲检测专用；与 `eng/packaging/config/defaults.json#detection.threshold_sigma` 同义 |
| `min_area` | 2 | px | 最小连通像素数。仅全图盲检测 / 连通域诊断（星表引导路径不做连通域） |
| `deblend` | true | —— | 是否解混。仅全图盲检测 / 显式声明的可选诊断 |
| `selection_function` | true | —— | 是否输出 selection function |

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是（资源门拒绝 heavy+serial 组合）。现状无 ThreadBudget
接线，worker 数取进程默认 team（迁移整改点）；取消接线同样未落地。

handle 级互斥使用（单 handle 单线程，无内部锁，docs/engineering/api/PUBLIC_API.md「分阶段 API 面」 底层模块函数登记表行 no/no）。
OpenMP 四处：行差分背景噪声估计、入口像素类型转换、盲检测候选拟合（dynamic +
reduction）、星表引导候选拟合（同款 dynamic + reduction，每线程私有 LM 工作区）。
dedup / sort / maxStars 截断串行。

确定性 = `fixed_reduction_order`，输出 bitwise 与线程数无关。

## 内存/cache/I-O/所有权

I-O 零文件/网络；模块不经统一 I/O 入口直接读写产物。

所有权 = 输出十数组由模块 malloc，调用方唯一经 `sdet_free_detect_ex` 整组释放
（sdet_api.cpp）；释放一律经该接口整组进行，不逐元素释放。无模块内 cache。

## 错误、日志、指标、取消和 checkpoint

入口 rc：0 = 成功（含 0 星空场：输出全 NULL + count = 0，非错误）；−1 = 参数无效
/ 句柄 NULL / 分配失败（`sdet_detect_guided_ex_f64` 在 `pred_x` / `pred_y` 为 NULL
且 `n_pred > 0` 时同样返回 −1）。拟合级 `SDET_FIT_*`（非 OK 候选丢弃）；质量门
`reject_star` 的 `SfError` 六码。编排级 det_ret ≠ 0 或 count ≤ 0 → 退出码
`STAR_DETECT_FAILED`（orchestrator.cpp）。错误码与退出码唯一源 =
lib/infrastructure/cli/exit_codes.h（本页不复制数值表）。

边界与丢弃：边界源标记边界 flag；距边界 < 2px 的星表预测位置允许丢弃并计入
`n_dropped`（与全图盲检测同判据）；亮星饱和/拖线标记，不参与后续 PSF/测光默认
路径。

### 权威路径的 fail-closed 语义（DATA 域拒绝，不降级、不冒充）

| 情形 | 行为 |
|---|---|
| `catalog_guided` 且未配置星表目录 | DATA 拒绝（点名 `gaia_data_dir`），不回退全图盲检测 |
| 星表目录 0 个 `.xpsd` | DATA 拒绝（`gaia catalog is empty`） |
| 星表 shard 装载失败或装载数 ≠ 条目数 | DATA 拒绝（`gaia catalog is incomplete` / `gaia_client_create failed`） |
| 逆投影先验缺失（无本帧 `p1_wcs.json` 产物，且无 `approx_wcs` CD、无 `rotation_deg`） | `catalog_guided` DATA 拒绝；`auto` 显式降级并把原因写入 manifest 的 `detection_degraded_reason` |
| 本帧 `p1_wcs.json` 存在但天测不可用（缺 crval/CD、非有限、CD 退化） | DATA 拒绝（判据 `p1_wcs_astrometry_usable`，禁 silent default） |
| 近似 WCS 不可解析（指向/板尺度缺失或退化） | DATA 拒绝（禁 silent default） |
| 星表逆投影后帧内 0 星，或全部拟合被质量门拒绝 | DATA 拒绝（`0/N catalog-guided fits survived`），不回退全图盲检测冒充成功 |
| `max_stars` 越出 [20000, 50000] | DATA 拒绝（禁静默夹取） |
- **负例与归零分支 N22（T05–T07 负向轮，本卡死值与静默 scale）**：负例输入构造甲 =
  `max_stars` 取 19999 / 50001；负例输入构造乙 = 空场（0 星）输入。预期行为甲 =
  DATA 拒绝，不静默夹取到边界。落盘标记甲 = 拒绝原因 + 越界值。预期行为乙 =
  `rc = 0` + 输出全 NULL + `count = 0`（非错误，归零分支）。落盘标记乙 =
  `count = 0` 空场标记；把空场当失败、或把越界静默夹取 ⇒ 判红；

manifest 顶层与逐帧记录 `detection_mode`、`detection_authoritative`、
`detection_degraded_reason`、`star_detection_max_stars` 与 `gaia` 溯源块。

取消 = 无模块内取消检查点（迁移整改点）；模块内无 checkpoint。

## 独立 synthetic 验证命令与容差

`TEST-STAR-DESIGN-001`（ALG-STARDET-001，冻结测试设计与容差：合成场质心
/ FWHM / 召回 / 虚警、饱和/混合/边缘专项、确定性 bitwise、FP64 独立 oracle、负例、
回归锚）；可执行 `TEST-P1-STAR-001` 待建。现状无仓内可复算的共址测试套件；
FP64 手工验证程序已退役。

Oracle 面：

- 合成图像注入已知源（位置/亮度分布已知）→ 检测率、误检率、质心精度符合理论；
- selection function 与注入分布一致；
- 改变星表亮度分布只改变 source-SNR 摘要，不改变信息权重（跨模块验证）；
- 1 worker vs N worker 输出一致；
- **负例与归零分支 N03（T05–T07 负向轮，检测浮点帧两态）**：负例输入构造甲 = 浮点帧上
  `snr.max_sources = 0`（不限）；负例输入构造乙 = 同一帧上 `snr.max_sources = K > 0`。
  预期行为甲 = 只截断交付样本行、不截断检测定义域（`maxStars` 只管交付上限）；
  预期行为乙 = 同甲，且交付行数 ≤ K 并落盘 `truncated` 标志与被计入样本数。落盘标记 =
  交付样本 `truncated` 布尔 + 计入数；把样本上限解释成定义域截断 ⇒ 判红；
- 权威路径判据 `p1star_guided`：真值位置召回与质心（|Δc| ≤ 0.3px @ SNR ≥ 20）、
  纯噪声场虚警 ≤ 0.1/千像素（发布门 `G-P1-STAR-FP`）、定义域丢弃计数守恒、
  1/4 线程逐位一致、定义域非退化（预测位置整体偏移后输出不落在真星上）、
  空定义域非错误；
- 节点级判据 `p1stardet_node_gate`：本节 fail-closed 表每条的红例 +
  `blind_diagnostic` / `auto` 的绿例与降级落档断言 + 真实帧（testdata）权威路径与
  盲检测的对照；上述两组判据的读数载体不在本仓可复算路径上；
- 节点序与边保真判据（C4–C7：无幻边 / 序
  为注册表 DAG 拓扑序 / IR 序 == 注册表声明序 / `psf` 在 `wcs` 之后且声明
  `artifact:p1_wcs` 输入 / 非退化），含 4 条负例注入（交换 `psf` / `wcs` 序、恢复
  幻边、移除 `psf` 的 WCS 输入），逐条必判红；其执行器不在本仓可复算路径上。

## 已知限制

- ThreadBudget 接线与取消检查点缺失（缺陷登记 = ALG-STARDET-001），迁移
  目标未落地；
- dll_target = acsd_p1_star_detection.dll，entrypoint 未落地；现状生产构建走
  `star_detector.dll` 显式加载与 `acsd_phase1_stars` 静态库两条路径；
- `wrapper_phase1` 为非生产算法路径的桥接实现；
- 局部噪声自适应为目标态、当前未实现；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
