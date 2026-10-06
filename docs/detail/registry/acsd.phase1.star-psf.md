# 模块 acsd.phase1.star-psf

> 上游：`docs/ACSD_DESIGN.md`对应章节（模块与 ABI）、对应章节（星表引导检测：候选星来自
> 星表位置拟合）、对应章节（科学叠加权重的边界）
> 科学正本：docs/science/algorithms/STAR_PSF_ALGORITHMS.md（SCI-P1-PSF-001、
> ALG-STARPSF-001）、`docs/science/noise_snr/NOISE_SNR.md`对应章节（PSF 与信息权重）、
> `docs/detail/common/unified_model.md`对应章节（观测模型）
> 数据正本：docs/detail/registry/acsd.phase1.star-psf.md（DATA-P1-PSF 端口表，本页输入输出端口表）
> API 正本：docs/engineering/api/PUBLIC_API.md（API-PSF-001）、API-P1-003
> （docs/engineering/api/PUBLIC_API.md「分阶段 API 面」）

模块级事实以 `lib/algorithms/psf/README.md` + `lib/algorithms/psf/module.yaml`
（acsd.p1.psf，迁移目标 acsd_p1_psf.dll）为准。现状构建 Makefile →
`dynamic_psf.dll`，未编入根 CMake 主构建。缺陷登记 = STAR_PSF_ALGORITHMS.md 对应章节。

## 职责与明确非职责

Registry production 模块（唯一源 = module_adapters.cpp 的 p1_star_psf_descriptor）。
职责：动态 PSF 建模与拟合质量代理（Moffat4 参数拟合、θ 消歧、q_psf 质量指标）——
估计空间变化的 PSF 模型及其参数、残差与适用域；生产源 lib/algorithms/psf/
（dynamic_psf.dll）。

不做：星点检测（P1-STAR）、测光定标（P1-PHOT）、盘面 I/O。PSF 拟合质量代理
（FWHM、残差尺度等）只作诊断，不计入科学叠加权重（最高设计对应章节）；本模块
不产生像素噪声权重、不进 Phase2 science weight。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `cleaned` | `DATA-P1-COSMETIC` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `wcs` | `DATA-P1-WCS` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `sources` | `DATA-P1-SOURCES` | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `psf` | `DATA-P1-PSF` | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::PIXEL` |

invalid = NaN/coverage=0（按 DATA 合同）。

输入面：定标信号（拟合窗口像素值）、检测目录（候选星位置与拟合窗口；来自星表
引导检测，检测定义域 = 星表位置）、初始参数、配置。逐像素 variance/ivar 加权
拟合与 validity 掩膜为待实现项——现行 `moffat4_fit` 的输入面 = 图像 + 窗口几何
+ 初值。星点裁剪图像为 uint16 或 float 像素域、行主序。

输出面：PSF 家族、参数、FWHM / 椭率、有效域、拟合残差（`DPSFFitResult` 的
Moffat4 形状参数 + 拟合质量代理 q_psf / residual_scale，编排序列化为 PSF 块
`[N,9]`）。产品至少提供 PSF 模型/地图 + 摘要。空间变化模型及其协方差为目标态、
未落码：现实现仅帧级 Moffat4。有效域与拟合残差必须随产品输出；残差超阈标记
validity。

## 公共 header、核心 symbol 与生命周期

模块级 API = API-PSF-001（docs/engineering/api/PUBLIC_API.md PSF 节）；编排级 =
API-P1-003（phase session extern "C"）；生命周期 create→validate→run→inspect→
destroy。

头 `lib/algorithms/psf/include/dynamic_psf.h`：参数结构
`DPSFFitParams{fitRadius, maxIter, tolerance}`、结果结构
`DPSFFitResult{status, B, A, cx, cy, sx, sy, theta, fwhm_x, fwhm_y, mad, flux,
eccentricity}`；入口 `dpsf_fit`（uint16）/ `dpsf_fit_batch` /
`dpsf_fit_batch_f32` / `dpsf_fit_batch_f64` / `dpsf_fit_batch_d`；结果整组释放
`dpsf_free_results`。

entrypoint = 信号 + 候选星窗口 + 初值 → PSF 模型 / 参数（帧级 Moffat4）。
输出模型的**生产消费者为零**：节点注册表不声明 `artifact:p1_psf` 输入端口
（module_adapters.cpp 记该边在注册表里不存在，`acsd.phase1.star-psf` 的
`p1_psf` 端口为「生产链路零消费者」）；`docs/engineering/contracts/PIPELINE_BLOCK.md`
把 `photometry ← p1_psf` 列作自测负例（幻边）。`p1_psf.json` 只经 `star_id` 关联
作 PSF 域复核读数；noise_snr 的交付样本与深度按测光有效源独立构造，与 PSF 参数面
解耦。

### 源文件

`lib/algorithms/psf/`。

## Registry descriptor 与配置 schema

module_id=`acsd.phase1.star-psf`（registry descriptor 口径）；模块合同
module.yaml 登记的 module_id 为 `acsd.p1.psf`，两者指同一生产模块。目标交付
形态 acsd_p1_psf.dll。execution_class=`cpu_heavy`; parallel_ok=True。

配置 = phase config JSON（签名见 docs/engineering/api/PUBLIC_API.md「分阶段 API 面」，默认值见 docs/engineering/contracts/CONFIG.md）：

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `psf_model` | `moffat4` | —— | PSF 母函数（当前实现：椭圆 Moffat4 7 参数；检测侧椭圆高斯归属 star_detection） |
| `psf_spatial_order` | 0 | —— | 空间变化阶数（0 = 帧级；非零分支为无行为承载的目标态键） |
| `psf_uniformity_gate` | —— | —— | 均匀性门阈值 |
| `fit_residual_gate` | —— | —— | 拟合残差门 |

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是（资源门拒绝 heavy+serial 组合）; worker 数 =
ThreadBudget.max_workers（唯一取值源）。现状按星点并行，各星点拟合互相独立。

确定性 = NOT_VERIFIED（未取得验收证据；不写成 PASS/FAIL）。

复杂度：O(候选星数 × 拟合窗口像素数)，单次 Moffat4 拟合为定迭代上界的最小二乘。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同（bounded）; I-O 单 writer。逐星工作区 RAII；输出块由
调用方分配。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源 = lib/infrastructure/cli/exit_codes.h（本页不复制数值表）。
拟合状态码 = `DPSF_FIT_OK` / `DPSF_FIT_NO_CONVERGENCE` /
`DPSF_FIT_INVALID_PARAMS` / `DPSF_FIT_ITERATION_LIMIT`；拟合不收敛一律走显式
状态，不静默降级。

边界：候选星不足 → 明确失败或降级（并记录），不静默把帧级 PSF 当空间模型交付；
残差超门 → validity 标记，按拟合质量不足登记；PSF 拟合质量代理（FWHM、残差
尺度等）只作诊断，不计入科学叠加权重（最高设计对应章节），本模块不产生像素噪声
权重、不进 Phase2 science weight。

取消 = 协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成
→ exit 9，最高设计对应章节；接线以实测为准）；模块内无 checkpoint。

## 独立 synthetic 验证命令与容差

测试标识 = `TEST-P1-PSF-001`（registry descriptor 单源）；执行证据 = NOT_VERIFIED
（未取得验收证据）；容差 = NOT_VERIFIED（同源）。测试设计 = `TEST-PSF-DESIGN-001`
（STAR_PSF_ALGORITHMS 对应章节：四状态码负例 / 解析 Moffat4 oracle / θ 消歧确定性 /
NaN 占位一致性，逐码逐锚），fixture 生成器注记容差来源。

Oracle 面：

- 注入已知 PSF 图像 → 参数恢复（FWHM / 椭率 / 质心）符合精度；合成 Moffat
  恢复（PSF-001..008）与残差 Gaussian 假设检查；
- 空间变化模型在位置变化时的残差验证；
- 有效面积与信息权重一致性 Oracle（PSF 归一化与信息核正本 =
  `docs/science/noise_snr/NOISE_SNR.md`对应章节）；
- PSF 不变量（逐位置归一为和为一、含像素响应对称性按模型）。

## 已知限制

- 缺陷登记 = STAR_PSF_ALGORITHMS.md 对应章节：covariance 缺口、`maxIter` /
  `tolerance` 死参数；
- 空间变化 PSF 模型与其协方差未落码，唯一实现路径 = 帧级 Moffat4；
- 逐像素 variance/ivar 加权拟合与 validity 掩膜未落码；
- `p1_psf` 端口生产链路零消费者，`psf_spatial_order` 非零分支无行为承载；
- 现状构建产物 `dynamic_psf.dll` 未编入根 CMake 主构建，目标交付形态
  acsd_p1_psf.dll 未落地；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
