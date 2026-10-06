# 模块 acsd.phase1.photometry

> 上游：`docs/ACSD_DESIGN.md`「模块与 ABI」一节、「Phase1 节点流程」一节、
> 「产品基数不变量」口径
> 科学正本：docs/science/photometry/PHOTOMETRY.md（SCI-PHOT-001，含「判据与误差」一节判据与观测统计量、
> 「平移精化」判据）、docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md（ALG-PHOT-001..002
> 「逐符号锚」一节）、`docs/science/noise_snr/NOISE_SNR.md`「不确定度传播」一节
> 数据正本：docs/detail/registry/acsd.phase1.photometry.md（DATA-P1-PHOT 端口表，本页输入输出端口表）
> API 正本：docs/engineering/api/PUBLIC_API.md（API-PHOT-001）、
> docs/engineering/api/abi/ABI.md、API-P1-005（docs/engineering/api/PUBLIC_API.md「分阶段 API 面」）
> 数据对象：`docs/detail/common/unified_model.md`「观测模型」一节

模块级事实以 `lib/algorithms/photometry/README.md` + `lib/algorithms/photometry/module.yaml`
（acsd.p1.photometry，迁移目标 acsd_p1_photometry.dll，entrypoint 未落地）
为准。现状构建 = `lib/algorithms/photometry/cpp/Makefile` + `lib/algorithms/photometry/cpp/build.ps1` → `photometric_calib.dll`，
未编入根 CMake 主构建。缺陷与整改登记 = PHOTOMETRIC_FIT［A-1］。
`lib/algorithms/photometry/wrapper_phase1` 的 Photometer aperture 面为迁移目标面
（README相应章节），当前无仓内可复算的用例引用。

## 职责与明确非职责

Registry production 模块（唯一源 = module_adapters.cpp 的 p1_photometry_descriptor）。
职责：对检测源做 **PSF 拟合域**测光（孔径测光仅作显式声明的诊断/交叉验证，
非生产口径——依据最高设计「节点流程」一节的「全链一个通量口径」），把本帧 signal 映射到
统一线性通量尺度，给出 `a_k` 及其不确定度；源星表 ↔ 参考星表双向最近邻配对
（KD-tree，2.0px）+ 星等预过滤 + IRLS/Tukey 稳健零点求解（scale 取
10^(−location)）；质量结构体落位 snr_estimator。

不做：绝对光度定标到物理流量；Phase2 集成（integration）；逐像素 ivar
（边界 = docs/science/photometry/PHOTOMETRY.md）；星点检测；星表缓存管理。相对标度不足时
标记不可跨帧合并，**不用 median stellar flux 静默代替**；**不以物理单位论证
标定因子**——`a_k` / `k_photo` 的绝对值无物理意义，判据 = 测光一致性
（「帧间一致性」是语义目标与报告字段，不是门禁）。

端口 DATA 编目（psf→DATA-P1-PSF / sources→DATA-P1-SOURCES /
fluxes→DATA-P1-FLUX）为编排层词汇，模块合同 DATA 层 = DATA-P1-PHOT
（本页输入输出端口表）。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `calibrated` | `DATA-P1-CAL` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `sources` | `DATA-P1-SOURCES` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `fluxes` | `DATA-P1-FLUX` | 可 | `UnitId::ADU`（与本页输入输出端口表 psf_flux 一致） | `CoordinateFrame::ICRS` |

invalid = NaN/coverage=0（按 DATA 合同）。

输入面：定标信号、variance/ivar、validity、PSF 模型、WCS、检测目录、配置。

输出面：源通量 `F`、通量方差、通量估计不确定度、`a_k`（光度响应）及其不确定度、
颜色项、有效域、测光 flags、通带身份块 `passband_identity`（声明通带名、
`FILTER` 关键字、解析出的库键、曲线自述名与采样点数、波长范围、曲线来源）。
绝对通量锚定由 Gaia XP 合成通量 `F_syn` 与输出 `location` / `scale` 承担（正本
= docs/science/photometry/PHOTOMETRY.md）；配置中无 `zero_point` 字段。仪器/合成流量 →
dex 残差 + `sigma_mag` / `sigma_cal_rel`。`a_k` 随产品输出，供 Phase2 使用。

**精度归属**：测光定标（`k_photo` / 零点 / `F_syn` / `a_k`）与星表匹配属**稀疏与
元数据**面，按最高设计的「数据对象与配置」章内「精度归属」条取**全程双精度**
（生产入口即 `_f64` 变体）；JSON 显式指定位深时以 JSON 为准。像素面属**稠密大面**
（单精度），定标标度施加到该面时按面自身的位深落盘，两类面的精度归属不得互相代入。

### 数值落地口径

通量尺度、误差传播与判据的推导正本 = SCI-PHOT-001 与 ALG-PHOT-001..002；本页
只记落地方式：

- 孔径测光（孔径定义、sky 环、误差传播）仅作显式声明的诊断/交叉验证，不是
  生产口径；PMM 用的正是孔径口径，**不照搬**；
- 生产口径 = PSF 拟合域测光：全链各模块共用同一个 `flux` 口径，通量为 PSF
  拟合域下的最大似然估计，其不确定度为信息权重的倒数（与 noise_snr 联动）；
- 相对标度不足 → 标记"不可跨帧合并"，不静默降级。

**测光一致性判据（单帧、尺度无关、双边界）**：判据只有一条 —— 施加后星点星等
与 Gaia 残差的散度小（「帧间一致性」是语义目标与报告字段，不是门禁）。门判据
= **单帧标定是否可信**，与其它帧无关。阈值只能从本帧误差预算导出，拍脑袋的
固定阈值属另一口径。

落地形态为三个量：**观测量散度**（本帧匹配星 — Gaia inlier 残差的 MAD，按
SCI-PHOT-001 的换算因子还原为标准差）、**上界**（本帧九项误差预算合成的
预算上限，按本帧匹配星数放宽）、**下界**（本帧匹配星数给统计涨落设的物理
下限，按星数收缩）。判定 = 观测量落在下界与上界之间；三个量与逐项分解无论
通过与否都报出。九项预算为：拟合（白 / 含天光结构）、生产拟合器对独立孔径的
系统比值散度（系统主导项）、颜色项（QE 未建模的通带失配，生产取 QE ≡ 1）、
天光残差、Gaia、平场高通、量化；逐项定义、取值与出处正本 =
docs/science/photometry/PHOTOMETRY.md。MAD→标准差估计量的相对标准误 SD 因子取闭式
`c_se = 1/(4φ(d)·d) = 1.1663872874444212`（`d = Φ⁻¹(3/4)`；文献登记值 `1.361`
只按 3 位有效数字使用，其平方与闭式相差约 −0.04%，在 RC93 JASA 未给 MAD 绝对
渐近方差前不作为精确式），3 倍作抽样允差（唯一约定性选择）。

**判不了的项按不加处理**（上限偏严 ⇒ fail-closed）：平场大尺度残差（缺
repeat-flat / sky-flat 对照）、Gaia XP 合成通量 `F_syn` 定标误差、本仓 `*.xpsd`
的 `magBP` / `magRP` 实测恒为 0、望远镜光学/大气/差分消光项（无仓内曲线）。

**落地状态**：该门判据尚未在代码中生效。本模块须先产出 ① 逐星残差表
（`star_id`, `F_instr`, `G_Gaia`, `F_syn`, `r_i`, `inlier`）、② 每帧的
系统比值散度、③ 冻结的误差预算表（九项 + 出处，作表而非散落常数）、④ 能红能绿
负例（见本页「独立 synthetic 验证命令与容差」的 Oracle 面）。在 ①–④ 具备前，
测光一致性门取「未生效」形态，固定 mag 阈值
不以任何形式进入实现、测试与文档。

### 外部参考实现的引用纪律

PhotometricMosaic（PMM）的使用面 = 只读方法研究：可读源码做方法研究；代码只读、
保持原样；引用面 = 方法学描述；分发面 = 无。测光口径**不照搬** PMM。可借鉴的
是其报告范式：跨帧一致性只作 warning / 报告字段，不作拒绝帧的门（实现字段
`photscale_spread_dex` / `photscale_spread_warn` / `photscale_spread_gate =
"none (frame-independent)"`）。

## 公共 header、核心 symbol 与生命周期

模块级 API = API-PHOT-001（docs/engineering/api/PUBLIC_API.md 测光节；头
`lib/algorithms/photometry/cpp/include/photometric_calib.h`，6 导出符号，
生产入口 `pc_calibrate_simple_with_gaia_f64` / `_v2`）；编排级 API =
API-P1-005（phase session extern "C"）；生命周期 create→validate→run→inspect→
destroy。

C ABI 规则：`PC_API` / `extern "C"` 不抛异常；`gaia_client_handle` 为 opaque
borrow，模块不持有；`spec_stars` / `spectra_buf` 在调用内释放；`out_*` 由调用方
分配与释放。

### Production callers

生产调用 = orchestrator.cpp `run_stage_photometric` → `pc_calibrate_simple_with_gaia_f64_v2`
/ `_v2`（dll_loader.cpp 加载）。

### 源文件

`lib/algorithms/photometry/`（生产面 `cpp/`；aperture 诊断面
`wrapper_phase1/photometer.h`）。

## Registry descriptor 与配置 schema

module_id=`acsd.phase1.photometry`（registry descriptor 口径）；模块合同
module.yaml 登记 `acsd.p1.photometry`。execution_class=`cpu_heavy`;
parallel_ok=True。配置 = phase config JSON：

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `mode` | `psf` | —— | 生产口径 = `psf`（PSF 拟合域）；`aperture` 仅诊断/交叉验证，须显式声明 |
| `aperture_radius` | 4.0 | px | 孔径测光半径——仅诊断/交叉验证。取值口径 = 孔径测光实现 `wrapper_phase1/photometer.h` 的 `Photometer` 构造默认值；配套 sky 环 6.0–10.0 px |
| `sky_annulus` | —— | px | sky 环——仅诊断/交叉验证；生产口径的局部背景由 PSF 拟合域承担 |

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是（资源门拒绝 heavy+serial 组合）; worker 数 =
ThreadBudget.max_workers（唯一取值源）。现状帧级串行，无共享状态。

确定性 = NOT_VERIFIED（未取得验收证据；不写成 PASS/FAIL）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同（bounded）; I-O 单 writer。

所有权 = 输出结构由调用方释放；输出 buffer 由调用方分配。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源 = lib/infrastructure/cli/exit_codes.h（本页不复制数值表）。
输入校验失败 → 负返回码（API-PHOT-001）。参考星 / PSF 星 / 光谱星不足或滤光片
失败 → 退化恒等校正（scale = 1.0、rc = 0，diag/records显式登记，README相应章节）。

边界：源太暗/太亮 → 测光 flags，不产出无意义通量；`a_k` 不确定度缺失 → 标记
不可跨帧合并；饱和/拖线源标记，不进默认路径。

**失败作用域（帧级 vs 全局，各自具名；正本见 docs/detail/infrastructure/log_and_error_system.md
相应章节）**：

- **帧级失败**（该帧自身条件不成立）⇒ 该帧记 `status=fail` + `error_domain` /
  `error_status` / `error`、不产出 `photoapplied_<base>`、不进 `photscales`，
  其余帧照常拟合与施加。稳定错误码：`PHOT_SOURCES_FRAME_MISSING`、
  `PHOT_WCS_UNUSABLE`、`PHOT_WCS_SIP_INVALID`、`PHOT_FRAME_UNREADABLE`、
  `PHOT_FIT_NO_SCALE`、`PHOT_SCALE_NON_PHYSICAL`、`PHOT_SCALE_MISSING`、
  `PHOT_SCALE_NO_FIT_EVIDENCE`、`PHOT_FIT_IMPLAUSIBLE_SCATTER`、
  `PHOT_PASSBAND_IDENTITY_INCONSISTENT`（该帧曲线身份与组内首帧不一致；同时置
  `passband_identity_inconsistent` 标志）。帧级失败**不是降级**，不写
  `degraded_reason`；
- **全局失败**（星表 / 响应曲线不可读、`gaia_data_dir` / `filter` /
  `filters_json` 缺项、冻结 C 入口返回非零）⇒ **中止运行**
  （`ErrorDomain::CONFIG` / `IO` 上行到 CLI 收敛为退出码），不把整批帧逐帧判 fail；
- **运行级判红**由产品基数不变量给出（最高设计「输出合同」一节「任何一帧未被处理、跳过
  或失败都显式判红」）：失败帧没有 HiPS 产品 ⇒ `write_hips` 上抛
  `SCIENCE_PRECONDITION`（退出码 4）且**不发布** `p1_products.json`；其他帧已
  写出的产品保留在磁盘上；
- **逐帧真相**在 `p1_phot.json.frames[]`（`DATA-P1-PHOTPROV-001`）；组级
  `photometry_applied` 只表示「至少一帧已施加」（`pixel_scaling` ∈
  `applied` / `partial` / `none`），下游按 `frames[]` 逐帧选择输入面；
- **通道未配置**（无 `photometry.fit` 段且无 `p1_photscale.json`）⇒ 显式降级
  `degraded_reason=photscale_absent`（中性 photscale = 1.0，下游按未归一化 ADU
  消费并如实登记）。

取消 = 协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成
→ exit 9，最高设计协作取消口径；接线以实测为准）；模块内无 checkpoint。
- **负例与归零分支 N25（T05–T07 负向轮，本卡死值与静默 scale）**：负例输入构造 =
  相对标度不足（`a_k` 不确定度缺失）的帧。预期行为 = 标记不可跨帧合并，不静默
  降级、不以物理单位论证标定因子绝对值。落盘标记 = 不可合并标记 + 误差预算三量
  （观测量 / 上界 / 下界）；静默降级或跨帧一致性门 ⇒ 判红；

## 独立 synthetic 验证命令与容差

测试标识 = `TEST-P1-PHOT-001`（registry descriptor 单源）；执行证据 = NOT_VERIFIED
（未取得验收证据）；容差 = NOT_VERIFIED（同源）。测试设计 = `TEST-PHOT-DESIGN-001`
（PHOTOMETRIC_FIT［A-1］，冻结容差：fixture F1–F6、不变量 I1–I6、负面矩阵、
SCI-PHOT-001 容差 —— 注入 rtol 1e-4、20% 离群 Δlocation < 0.1 dex、NumPy
rtol 1e-9）。

已取证的相邻锚：Photometer 4 组读数与对齐回归读数；其载体不在本仓可复算路径上，
引用时只作背景。

Oracle 面：

- 注入已知通量源 → 通量恢复的 bias / variance 符合理论；
- PSF 测光与独立孔径测光交叉；
- 通量不确定度 = 信息权重倒数的理论一致性；
- `a_k` 不确定度传播到 Phase2 covariance 验证；
- **负例与归零分支 N01（T05–T07 负向轮，星数不足记 NaN）**：负例输入构造 = 匹配后
  `|r_consistent| < 3`（或有效参考星 / PSF 星 / 光谱星任一不足）。预期行为 = 不拟合、
  不写标度，`scale = 1.0`、`fit_used = 0`、`sigma_residual = 0`（含义 = 无不确定度
  可用，不是零不确定度），该帧 `status = fail` + 具名 `error_status`
  （如 `PHOT_FIT_NO_SCALE`），不产出 `photoapplied_<base>`、不进 `photscales`。
  落盘标记 = `p1_phot.json.frames[]` 该帧 `status/fit_used/sigma_residual` 三键齐全；
  把该分支改成静默用 `median stellar flux` 代替 ⇒ 判红；
- 测光一致性判据的负例（能红能绿，判据生效的必备条件）：① 伪造常数残差表
  （`r_i` 全等）⇒ 观测量低于下界 ⇒ **必须判红**；② 把某帧 `F_instr` 乘随机
  因子（人为注入 0.2 mag 散度）⇒ 观测量高于上界 ⇒ **必须判红**；③ 健康帧
  （合成与真实各一帧）⇒ **必须判绿**；④ 人为加入跨帧 k/scale 一致性门 ⇒
  **必须判红**（`PHOT-GATE-DROP-001`：组间一致性只作报告字段）。在 ①–④ 具备前
  通过/不通过门取「未生效」形态。

## 已知限制

- 缺陷与整改登记 = `docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md`［A-1］；
- 测光一致性门判据尚未在代码中生效（缺逐星残差表、每帧系统比值散度、冻结的
  误差预算表、能红能绿负例四项前置产物）；
- 平场大尺度残差、Gaia XP 合成通量定标误差、光学/大气/差分消光三项误差无实测
  依据，按不加处理；
- 现状构建产物 `photometric_calib.dll` 未编入根 CMake 主构建；目标交付形态
  acsd_p1_photometry.dll 的 entrypoint 未落地；
- `wrapper_phase1` 的 Photometer aperture 面为迁移目标面，仅作诊断/交叉验证；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
