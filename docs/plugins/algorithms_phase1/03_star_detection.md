# 插件文档：star_detection（源探测）

> 上游：ASTROCS_DESIGN.md §4.2（Phase1 节点流程）

## 1. 职责与边界

- **职责**：检测图像中的源（星点/延展源），输出位置、质心/矩、源身份与 selection function 参数。
- **不是**：不是图像灵敏度本身；不做 PSF 建模（psf 模块）；不做测光（photometry 模块）；检测目录是下游输入，不是产品权重。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §4.6（硬约束：星表引导检测细则）、§4.2（节点流程：星表引导检测与 WCS 解算）与 §2.1（创新点一：测光校准到测光星等坐标系，星点位置由星表逆映射获得）；硬约束转引 `docs/plugins/algorithms_phase1/**` 与 `docs/science/**`；检测阈值的冻结定义见 `docs/science/STAR_DETECTION.md:18-19`、`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:36`、`docs/algorithms/GATES_AND_TOLERANCES.md:38-39`。
- `docs/design/PHASE1_DETAILED_DESIGN.md` §5（背景、有效性与源检测）
- `docs/science/UNCERTAINTY_AND_COVARIANCE.md`（质心/矩不确定度）

## 3. 输入/输出数据合同

- **输入**：定标+cosmetic 后信号、variance/ivar、validity、背景模型（若已有）、配置。
- **输出**：source catalog（源 ID、像素坐标、天球坐标、质心/矩、局部 SNR、flags）+ selection function/completeness 参数。
- 检测、PSF、WCS、测光、SNR 的 source row 绑定同一 `frame_id/source_id`。
- 参考：`eng/contracts/schemas/source_catalog.schema.json`。

## 4. 算法与公式要点

- **权威检测范式 = 星表引导拟合**（最高设计 §4.2）：检测定义域是**星表位置**（用本帧 WCS 把 Gaia 星表反向投影到像素域），只对星表位置做质心/PSF 拟合；拟合成功即星点，失败**直接丢弃**（不计虚警、不报错）；按亮度取 top 2–5 万颗为上限，极限星等按焦距、画幅、曝光时间派生估计（宁多勿少）；**全图盲检测连通域路径不是权威路径**，它只服务非权威的诊断/初值用途。
- 检测阈值 = `median(img) + 5.0·bgnoise`（**全局背景噪声 RMS 的倍数**，`bgnoise` 由 FnNoise1 行差分族估计；阈值作用于 σ=2 平滑图；实现 `sdet_api.cpp:1782-1792`）——**仅适用于全图盲检测路径**（全图盲检测 → 匹配 → 解算；该路径的星表**不是**权威科学产品，最高设计 §4.2）。**权威 WCS** 来自星表引导检测后的高纯度星表解算，其近似指向由 `wcs.init_source` 给出。检测路径不消费逐像素 variance/ivar。局部噪声自适应为目标态、当前未实现（GAP 登记），文档按现状描述；
- 质心/矩与不确定度：一阶矩质心、二阶矩，误差来自局部噪声传播；
- 输出 selection function（完备性 vs 亮度/位置）和 completeness 参数；
- 检测统计量与下游 PSF/测光解耦：检测目录不直接成为科学权重。

**实现落点（唯一权威生产源 `lib/algorithms/star_detection/src/sdet_api.cpp`）**：

| 路径 | 入口符号 | 说明 |
|---|---|---|
| 权威（星表引导拟合） | `sdet_detect_guided_ex_f64`（声明 `lib/algorithms/star_detection/include/star_detector.h`） | 定义域 = 调用方给的星表预测位置；逐位置做饱和判定、σ 估计、椭圆高斯拟合（与盲检测同一 `sdet_gauss_fit`/GSL TR-LM 7 参）与 `reject_star` 质量门；拟合失败直接丢弃。统计量 `SDetGuidedStats{n_predicted,n_dropped,n_fit_failed,n_rejected,n_fit_ok,n_output}` |
| 诊断/初值（全图盲检测） | `sdet_detect_ex[_f64]` | 平滑 → 局部极大 → 二阶导数零交叉宽度 → 连通域/解混 → 拟合；保留，**不是**权威路径 |

节点侧接线在 `lib/infrastructure/scheduler/src/module_adapters.cpp` 的 `p1_op_star_psf_impl`：
星表位置由 `p1_guided_predict`（`p1_guided_approx_wcs` 给出的近似 WCS 做 sky→pix 逆投影）产生，
极限星等复用 `ipv::estimate_mag_lim_iterative` / `ipv::compute_fov_density`（不另立常数）。

## 5. 配置项

### 5.1 `star_detection` 段（检测模式与权威路径输入）

节点 `star-psf` 从输入的 `star_detection` 段解析检测模式；同一键也可回退到 `wcs` 段
（`gaia_data_dir`）。**模式与输入在节点级一次解析**，逐帧不再重解析。

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `mode` | `auto` | —— | `auto` = 有参考星表且**取向先验可用**时走权威路径，否则**显式降级**为 `blind_diagnostic` 并把原因写进 manifest 的 `detection_degraded_reason`（非静默）；`catalog_guided` = 显式声明权威路径，前置条件不满足即 DATA fail-closed；`blind_diagnostic` = 显式声明的非权威诊断路径 |
| `gaia_data_dir` | 无 | path | 本地 XPSD 星表目录（权威路径必需；亦可用 `wcs.gaia_data_dir`） |
| `max_stars` | 20000 | 颗 | 检测定义域上限（按 G 星等升序取 top-N）。合同域 = **[20000, 50000]**（最高设计 §4.2「top 2–5 万」）；越界即 DATA 拒绝，**禁静默夹取** |
| `approx_wcs` | 无 | —— | 近似 WCS 的显式天测键 `{crval1, crval2, cd11, cd12, cd21, cd22}`（可改用 `wcs` 段同名字段）。**取向先验的给法之一** |
| `rotation_deg` + `parity` | 无 / `pos` | deg / `pos\|neg` | 取向先验的另一种给法：像面相对「北向上/东向左」的旋转（逆时针为正）与镜像标志；板尺度由 `wcs.init_source` 派生的 `s0` 给出 |
| `limiting_mag` | 由焦距/画幅/曝光派生 | mag | 显式指定极限星等；缺省时由 `ipv::estimate_mag_lim_iterative` 按 `focal_length_mm`、画幅、`EXPTIME` 迭代派生（宁多勿少） |

**取向先验是权威路径的必需输入**：星表逆投影必须知道像面取向与镜像；缺先验时
`catalog_guided` 直接 DATA 拒绝，`auto` 显式降级（`detection_authoritative=false`），
**不得**以「北向上/东向左」默认值冒充权威取向。

### 5.2 全图盲检测路径的键（诊断/初值，非权威）

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `detection_threshold` | 5.0 | σ（全局 bgnoise） | **全图盲检测专用 / 显式声明的可选诊断**（**不是**模块主路径配置——主路径 = 星表引导拟合，不消费此键）；语义 = `median(img)+5.0·bgnoise`（σ=2 平滑图上判定），与 `eng/packaging/config/defaults.json#detection.threshold_sigma` 同义（最高设计 §4.2/§4.3） |
| `min_area` | 2 | px | 最小连通像素数。**仅全图盲检测 / 连通域诊断**（星表引导路径不做连通域） |
| `deblend` | true | —— | 是否解混。**仅全图盲检测 / 显式声明的可选诊断**（星表引导路径按星表位置逐源拟合，不做盲解混） |
| `selection_function` | true | —— | 是否输出 selection function |

## 6. 接口/ABI

- entrypoint：图像+ivar+validity → source catalog；
- 单源行结构版本化，绑定 frame_id/source_id。

## 7. 错误与边界

- 输入全 NaN/全饱和 → 拒绝并记录，不产出空目录冒充成功；
- 边界源标记边界 flag；距边界 <2px 的星表预测位置允许丢弃并计入 `n_dropped`
  （与全图盲检测同判据）；
- 亮星饱和/拖线标记，不参与后续 PSF/测光默认路径。

### 7.1 权威路径的 fail-closed 语义（DATA 域拒绝，不降级、不冒充）

| 情形 | 行为 |
|---|---|
| `catalog_guided` 且未配置星表目录 | DATA 拒绝（点名 `gaia_data_dir`），**不**回退全图盲检测 |
| 星表目录 0 个 `.xpsd` | DATA 拒绝（`gaia catalog is empty`） |
| 星表 shard 装载失败或装载数 ≠ 条目数（部分装载） | DATA 拒绝（`gaia catalog is incomplete` / `gaia_client_create failed`）——静默部分装载事故不得重演 |
| 取向先验缺失（无 `approx_wcs` CD 且无 `rotation_deg`） | `catalog_guided` DATA 拒绝；`auto` 显式降级并留痕 |
| 近似 WCS 不可解析（指向/板尺度缺失或退化） | DATA 拒绝（禁 silent default） |
| 星表逆投影后帧内 0 星，或全部拟合被质量门拒绝 | DATA 拒绝（`0/N catalog-guided fits survived`），**不**回退全图盲检测冒充成功 |
| `max_stars` 越出 [20000, 50000] | DATA 拒绝（禁静默夹取） |

manifest 顶层与逐帧记录 `detection_mode`、`detection_authoritative`、
`detection_degraded_reason`、`star_detection_max_stars` 与 `gaia` 溯源块，
使「权威 / 非权威」在产物上可审计。

## 8. 测试与 Oracle

- 合成图像注入已知源（位置/亮度分布已知）→ 检测率、误检率、质心精度符合理论；
- selection function 与注入分布一致；
- 改变星表亮度分布只改变 source-SNR 摘要，不改变信息权重（跨模块验证）；
- 1 worker vs N worker 一致；
- 权威路径判据 `p1star_guided`（`lib/algorithms/star_detection/tests/p1star/p1star_guided_test.cpp`）：
  真值位置召回与质心（|Δc| ≤ 0.3px @ SNR≥20）、纯噪声场不计虚警、定义域丢弃计数守恒、
  1/4 线程逐位一致、**定义域非退化**（预测位置整体偏移后输出不落在真星上）、空定义域非错误；
- 节点级判据 `p1stardet_node_gate`（`lib/algorithms/star_detection/tests/p1star/p1stardet_node_gate_test.cpp`）：
  §7.1 每条 fail-closed 的红例 + `blind_diagnostic`/`auto` 的绿例与留痕断言 +
  真实帧（testdata）权威路径与盲检测的对照。
