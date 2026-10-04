# 模块 acsd.phase2.integrate

> 上游：docs/ACSD_DESIGN.md §8.5（模块与 ABI）、§5.2（固定科学流程）、
> §5.3（SNR 重建与逆方差叠加）
> 科学正本：docs/science/INTEGRATION.md（SCI-INT-001，FROZEN，零改动）、
> docs/science/PSF_SIGNAL_WEIGHT.md（PSF 与信息权重、§8 诊断面不作权重）
> 算法正本：docs/science/algorithms/PHASE2_INTEGRATION.md（ALG-P2-INT-001；§3 锚表、
> §11.3 约束与回归门、§11.4 测试设计、§11.5 映射声明）
> 数据正本：docs/science/DATA_SEMANTICS.md §21（DATA-P2-INT；§21.2/§21.5）、
> §31.8（权重来源受限表）
> API 正本：docs/engineering/PUBLIC_API.md（API-P2-INT-001）、
> docs/engineering/PHASE2_API_V1.md（API-P2-001，FROZEN）
> 数据对象：docs/detail/UNIFIED_MODEL.md（frame_snr、sparse_snr_layer）

LIB 面 = `lib/algorithms/integration/` 三件套（README / module.yaml ，
CONTRACT_READY，entrypoint 未落地）。MOD ID = MOD-acsd-phase2-integrate；
module_id 合同值 = `acsd.p2.integration`（descriptor 词汇
`acsd.phase2.integrate` 为编排层口径，其对齐属迁移目标、未落地）；dll_target =
`acsd_p2_integration.dll`（迁移目标，未落地）。生产源 =
lib/algorithms/coverage/src/integrate.cpp + 签名头正本
lib/algorithms/coverage/include/astro/phase2/integrate.h；构建 = 根 CMakeLists 的
`acsd_phase2` 静态库成员。owner = SA-P2-I23；depends_on_int = P2-REJ / P1-NOISE /
CPU-005；legacy_paths = 「lib/algorithms/coverage integration sources」。

## 职责与明确非职责

职责：按明确科学目标把归一化 + 排异后的帧集成为马赛克，并按产品族输出多个明确
产品。逐像素加权积分 reducer 的冻结口径：

- **signal**：候选样本的**逆方差加权平均**（仅 eligible ∧ 正权重样本，候选索引
  **固定序**）；权重和为零时**不做除法**；零权重 = 合法零贡献；
- **support canonical reducer**：取 accepted 样本 support 的**最大值**（integrate.h
  冻结语义；零权重 accepted 样本**计入** max；全零权时**仍发布**该 max；
  ALL_REJECTED 保持 0）。约束与回归门 = ALG-P2-INT-001 §11.3；
- **五态显式 status**：`OK` / `NO_CANDIDATES` / `ALL_REJECTED` /
  `ZERO_VALID_WEIGHT` / `INVALID_INPUT`；**零权重合法**（`ZERO_VALID_WEIGHT`），
  NaN / Inf / 负权重 → `INVALID_INPUT`；**reducer 不持有权重键**
  （policy / reducer 分离）；
- **输入防御面** `p2_validate_candidate_weights`（调用方 stage2.cpp 内两处预检）。

**四概念分离**：signal（面亮度） / variance-ivar（输入侧权重语义） / support
（几何覆盖，**禁作科学权重**） / mask（**不入权重式**）。

**不做**：不产「一个万能 weight」；权重策略（外部 numeric weights，构造在 Stage2
的 stage2.cpp，**policy / reducer 分离冻结**）；排异 / eligibility gather
（P2-REJ）；马赛克编排 / 逆归一化（Stage2 域）；内部并行（像素级纯函数，像素间
并行在调用方）。

**宣称边界**：普通像素 ivar coadd 在 PSF 不同时**不保证**最大点源 SNR，两者的
等价性宣称以专项证明为前提；Fisher 最优的宣称同以专项证明为前提。

## 输入输出端口、DATA、单位、坐标、invalid

编排层 descriptor 端口表（p2_integrate_descriptor，编排层口径）：

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `accepted_mask` | `DATA-P2-REJ` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::PIXEL` |
| `corrected` | `DATA-P2-COR` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `integrated` | `DATA-P2-INT` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

内核级真实 I/O 合同 = DATA-P2-INT（DATA_SEMANTICS §21）：输入 `P2PixelStack`
（values f64 ADU / weights f64 1/ADU²，可空 = 等权 / support f64 [0,1]，可空 =
1.0 / accepted u8，可空 = 全接受 / count u32）；输出 `P2PixelResult`（signal f64
ADU / support f64 [0,1] / 五计数器 / status 0..4）。

马赛克输入面：归一化产品组、UPM 参数、rejection、PSF / 信息层、帧级 SNR
（文件头）、[稀疏**绝对** SNR 层]、配置。输出面（同一马赛克包可含多个产品族）：

1. `surface_brightness`：signal（面亮度量纲，写端口 `UnitId::SURFACE_BRIGHTNESS`，
   落盘值 = 通量和 / 覆盖面积）、variance、correlation、effective PSF；
2. `point_source`：Q、W、flux、detection statistic、effective / proper PSF；
3. `psfsw_integration`（被选择时）：四分量、相对权重、conventional coadd、
   variance / correlation、effective PSF、基线比较；
4. support、coverage、validity、rejection；UPM 参数 / 协方差 / 残差；manifest。

输出被 export 消费（**不要求来自同一进程**）。

invalid 显式化：非法输入 → `INVALID_INPUT`（0 / ±Inf 属非法值；§4 同源条款）；
无候选 → `NO_CANDIDATES`；全拒 → `ALL_REJECTED`；全零权重 →
`ZERO_VALID_WEIGHT`。

### 数值落地口径

GLS 扩展源解、点源 Q / W 定义式与 effective PSF 的推导正本 = SCI-INT-001 与
docs/science/PSF_SIGNAL_WEIGHT.md；本页只记落地方式：

- **扩展源 / 面亮度**：按设计矩阵与协方差的广义最小二乘求解；**工程近似独立样本
  才退化到像素 ivar 加权平均**。Drizzle 相关、共同 master、UPM 参数、重叠重采样
  协方差纳入协方差矩阵，或以相关核 / 低秩近似；
- **点源最优**：逐帧的 Q 与 W 按**同一 PSF / 协方差口径**定义（Q 为信号侧线性
  泛函，W 为信息矩阵泛函）；独立帧时 Q 与 W 分别求和，通量估计 = Q / W，其方差 =
  W 的倒数；
- **PSF 信号权重复合分量**：`psfsw_integration` 用 Phase1 产出的 PSF 信号权重复
  合分量做 conventional integration，**只作诊断与基线对照，不计入科学叠加权重**；
  必须用共同星集 / selection-function 门，传播实际线性组合 covariance，输出
  effective PSF，与等权 / exposure / pixel-ivar / 信息权重基线比较；
  **Fisher 最优的宣称以专项证明为前提**。

### SNR 重建与逆方差权重（单一权重口径，没有可选择项）

**分工固定**：阶段一**只生产信噪比**（稀疏 SNR 控制点）；阶段二在叠加前**先算真实
信号面** —— 用每帧的稀疏控制点重建出稠密 SNR 面，再按**逆方差（最优功率）**定权
后叠加。**没有可选的权重口径、口径选择键、口径枚举或口径配置项**；越界 token
一律 fail-closed（`FZ-WEIGHT-SINGLE-PATH` / `FZ-MODE-RETIRED` /
`FZ-FIELD-WEIGHTMODE`）。

**三条 SNR 重建路径**（配置 JSON 显式指定，默认稀疏）：`dense`（稠密面，精度
基准）/ `sparse_reconstruct`（**默认**：由稀疏层重建稠密）/ `frame_reconstruct`
（帧级重建稠密）。这三条是**重建方式**的选择，**不是**权重口径的选择 —— 三者都
产出同一物理量（绝对通量型 SNR）的稠密表示，都走同一条逆方差定权式。

- `sparse_reconstruct` 且输入**有**稀疏层 → 由稀疏**绝对** SNR 控制点**直接
  重建**出稠密 SNR 面（控制点值即绝对信噪比本身，**不乘帧级标量**），参与科学
  运算。**重建算子由层显式声明**（`sparse_snr_layer.reconstruction_operator`）；
  实际生效算子标识与重建误差入 manifest（`SparseReconstruction.operator_id` /
  `node_reproduction_max_abs`）；未识别标识或声明与层形态不符 ⇒ fail-closed。冻结
  词表（算子标识与语义）唯一正本 = registry/acsd.phase1.noise-snr.md；
- `frame_reconstruct` → 帧级 SNR 重建 / 直接参与（等权重面）；
- 输入**无**稀疏层而路径为默认 / `sparse_reconstruct` → 按帧级执行并**显式记录
  实际路径**（`snr_path_effective = frame_reconstruct` + 计数），**不静默**；
  稀疏层存在但损坏 / 不可重建 → 明确失败（帧级回退属另一路径）；
- **三条路径没有全局最优、只有适用域**：完整适用域图谱由 `实验/absolute-snr`
  给出（最高设计 §5.3）；其中 HST 类高对比域的结论**与重建算子绑定**。

**逆方差叠加**：每个天球像素接收多个源像素输入，用每个源像素的 SNR 计算对应
权重（SNR → 逆方差权重）得到最优检测 / 测光功率 —— **不是直接用 SNR 加权**。换算
口径 = 帧内 SNR 与该帧参考通量的商再取平方（配对性只要求**同一帧内** SNR 与参考
通量同源，参考通量是逐帧的）；对稀疏重建场逐像素同式。**稠密权重是数学表示，
工程按需计算**：叠加分块进行（最小单元可为单个像素），**不预计算稠密、不全部加载
内存**，用到哪个像素的 SNR 算哪个。

## 公共 header、核心 symbol 与生命周期

签名头正本 = lib/algorithms/coverage/include/astro/phase2/integrate.h
（`P2PixelStack` / `P2IntegrateStatus` / `P2PixelResult` 三处结构体定义与函数
声明段）。

核心 symbol：`p2_integrate_pixel`（integrate.cpp，唯一生产入口 C ABI）、
`p2_validate_candidate_weights`（同上文件，调用方 stage2.cpp 内两处预检）。

本内核为**无状态纯函数**；编排生命周期 create→validate→run→inspect→destroy 由
session 工厂承接。C API 面 = API-P2-INT-001 + 编排级 API-P2-001（FROZEN）。

## Registry descriptor 与配置 schema

module_id=`acsd.phase2.integrate`（占位）；execution_class=`cpu_heavy`;
parallel_ok=True（像素间）。配置 = phase config JSON（权重策略在 Stage2，本内核
**无策略配置** —— weights 数组外置）。

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `snr_path` | `sparse_reconstruct` | —— | SNR 重建路径（`dense` / `sparse_reconstruct` / `frame_reconstruct`）；权重口径**无对应键** |
| `target_product` | 全 | —— | 输出产品族选择 |
| `correlation_approx` | —— | —— | 相关噪声近似方式 |
| `baseline_compare` | true | —— | 是否输出基线比较 |

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`。并行轴 = 像素间（**调用方** OMP：现存调用方 stage2.cpp 用
schedule(static)）；像素内候选归约**固定序**、无跨 worker 浮点重结合 ⇒ **结果与
worker 数无关（1..N bitwise）**；per-thread 统计按 thread id 定序归并
（stage2.cpp）；large_scale 激活时强制串行（stage2.cpp 的分支条件）。原并列调用方
`acr_kernels.cpp` 已随 ACR 子树退场删除（`383088f2`），该像素间并行面不再存在。

worker 数 = ThreadBudget.max_workers（禁 hardware_concurrency）；lease / 取消
检查点接线属迁移整改面（未落地）。determinism = `fixed_reduction_order`
（module.yaml 合同值）。

## 内存/cache/I-O/所有权

无 I/O（纯函数）；无内部 cache 与全局状态（reentrant = yes）。

所有权 = 调用方分配 scratch 与缓冲（`P2PixelStack` / `P2PixelResult` 由调用方
分配，签名定义见 integrate.h）。

## 错误、日志、指标、取消和 checkpoint

错误面 = 五态 status + rc = 1（null 栈）；无日志 / 指标输出（纯函数）；无内部取消
检查点 / checkpoint（迁移 ThreadLease 接线属迁移目标，未落地）。

- PSF 不同的输入用像素 ivar coadd 且宣称点源最优 → 判红；
- 相关噪声无描述 → variance 不完备，标记；
- PSF 信号权重复合分量缺共同星集 / selection function → fail-closed；
- 稀疏 SNR 层存在但损坏 / 不可重建 → 明确失败；
- 错误码与退出码唯一源 = lib/infrastructure/cli/exit_codes.h（本页不复制数值表）。

## 独立 synthetic 验证命令与容差

可执行 `TEST-P2-INT-001` 待建（不冒认）；设计冻结 = `TEST-P2-INT-DESIGN-001`
（ALG-P2-INT-001 §11.4：常量场 bitwise / 零权重惰性 / 五态穷尽 / 支撑 max 门 /
NumPy 参考 rtol 1e-12 / 并行 1..N 线程 bitwise + ACR↔CPU 等价（ACR 侧已随
`383088f2` 退场））。

已取证但载体不在仓内的相邻结论（不冒认）：Phase2Integrate 组与 weight policy 门、
ACR↔CPU 等价组（ACR 侧随 `acr_kernels.cpp` 一并退场，`383088f2`），
以及含 DRIVER_SRC 段的生产 Oracle 读数。这些读数不在本仓可复算路径上，
引用时只作背景。

Oracle 面：

- 独立高精度矩阵 / NumPy oracle 验证 GLS、Q / W、covariance；
- 注入点源在独立帧下满足组合方差等于各帧信息量之和的倒数（与实测 flux dispersion 对拍，相关帧须判简单求和被拒）；
- 不同 seeing / 透明度 / 背景组合下点源检测功率 ≥ 普通 ivar 叠加；
- **SNR 路径**：三条路径输出正确，适用域入 `实验/absolute-snr`；
- **SNR 重建**：稀疏→稠密重建与帧级铺满重建分别验证；**逐像素**权重按
  「层值（绝对 SNR，不乘帧级标量）÷ 帧参考通量」的平方并乘光度响应项的平方 计算
  正确；无稀疏层时实际路径被显式记录（负例：静默降级判红）；
- 扩展源常量场、梯度、总通量、方差无偏；
- PSF 信号权重复合分量与基线比较 + covariance 传播正确；
- 真实数据（银心）检查。

## 已知限制

- 现行语义与判据正本 = docs/science/algorithms/PHASE2_INTEGRATION.md §11.3；
  support 输出现状为**保守方向偏差**（不改覆盖并集保守下界语义）；
- **权重来源受限表的在役判据面**：`coverage.cpp` 的 `kForbiddenWeightSourceTokens`
  （含 `psfsw_robust_weight`、`psfsw` 等 token）与其同文件内的权重来源检查门、
  以及 `rejection.cpp` 的同源词表，是 DATA_SEMANTICS §31.8
  （`G-WEIGHT-SOURCES` / `G-DIAGNOSTIC-NOT-WEIGHT`）与 PSF_SIGNAL_WEIGHT §8 的
  执行面 —— **不是死代码**。**收缩路径**：DATA_SEMANTICS §31.8 受限来源表与代码
  词表**同源**，任一侧收缩一律走变更流程并在**同一次提交**内同步另一侧；两侧不同步
  会**放宽**冻结科学门（使 `psfsw` 重新成为合法权重来源）。生产权重来源仍是单一
  现场派生量（逐样本 ivar），该受限来源表只负责**拒绝**非法来源，不产生权重；
- 目标交付形态 acsd_p2_integration.dll 未落地；descriptor 占位 module_id 与合同
  值 `acsd.p2.integration` 的对齐属迁移目标（未落地）；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
