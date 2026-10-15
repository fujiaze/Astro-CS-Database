# Normalize 接口与迁移实现规格

本页从 [数据合同](../../engineering/contracts/NORMALIZE_DATA.md) 推导 C++ 修改边界；科学定义来自 [Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)。以下签名是待实现的内部接口，不是已导出的 ABI。执行者可调整内部容器拼写，不能改变字段语义、算法所有权和生产—消费边。

## 1. 放进现有模块的类型和接口

| 接口/类型 | 声明与实现落点 | 修改责任 |
|---|---|---|
| 帧对象仓、ObjectHeader、runtime payload binding | `lib/include/acsd/core/context.h`、`block_frame.h` 及其现有实现 | Runtime 持有 typed payload；数值块不变成 metadata 字节猜解器 |
| FrameIdentity、PlaneView、节点 typed bundle | `lib/include/acsd/core/module_adapters.h`；与算法库对接 | host adapter，仅用于运行绑定 |
| WCS 对象/转换 adapter | `lib/algorithms/platesolve/cpp/ipv` 的现有 public headers；`module_adapters.cpp` 的 WCS/SIP adapter | IPV 求解只复用一份；规范化原点并保留全部 SIP |
| PSF position evaluator | `lib/algorithms/psf/include/dynamic_psf.h` 及其实现 | 单星参数→空间模型/积分响应；不复制 Gaussian/Moffat 数学到 scheduler |
| response、reference、photometric solution | `lib/algorithms/photometry` 现有 public headers/实现 | 每帧 R_k 绑定；共享 cache 以真实曲线/QE/积分 convention 指纹为键 |
| noise/ref precision、GLS completion | `lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.h/.cpp` 和 `cpp/src/snr_science.cpp` | 数学只在 noise 库；兼容旧聚合接口并新增 typed 入口 |
| spherical operator/model view | `lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h`、引擎/已有 sphere sink | 同一次几何的信号/PSF/噪声系数；C ABI 只经版本化 POD/handle |
| sidecar 读写、manifest/model refs | `lib/infrastructure/aio` 既有 HiPS/atomic/manifest 接口 | 一个 JSON serializer/reader，不能各阶段手写一套解析 |

优先编辑现有文件；确需新编译单元时同步 source-set/build 清单，不能以源码列表维护困难为由把科学代码一直堆在 scheduler。所有 `p1_op_*` 最终只负责 config→typed inputs、调用模块、注册 outputs、错误/生命周期；算法库不读取块级 output_dir 来找上游。

## 2. 内部接口形状

```cpp
// 位于 host C++ 内部，Result 与错误域复用 acsd::core。
struct ObjectHeader {
  std::string run_id, frame_id, producer;
  uint32_t schema_version;
  uint64_t revision;
  std::map<std::string, uint64_t> input_revisions;
};
template<class T> struct FrameObject {
  ObjectHeader header;
  T value;
};

// Runtime 仓：发布后 const；显式完成 consumer 后释放。
template<class T>
Result<void> publish_frame_object(RunContext&, FrameObject<T>&&);
template<class T>
Result<std::shared_ptr<const FrameObject<T>>> acquire_frame_object(
    RunContext&, const std::string& frame_id, uint64_t revision);
Result<void> finish_frame_consumer(
    RunContext&, const std::string& frame_id,
    const std::string& object_id, const std::string& consumer_id);

// scientific views 引用 Runtime/AIO 拥有的数组/模型，不持久复制大面。
Result<ConditionalNoiseModel> build_conditional_noise(
    const PlaneView&, const CalibrationUncertaintyView&,
    const PsfView&, const PhotometricSolution&, const NoiseConfig&);
Result<PlanarReferenceModel> evaluate_planar_reference(
    const ConditionalNoiseModel&, const PsfView&,
    const ReferenceDefinition&, const SamplingConfig&);
Result<SparseReferencePrecision> complete_spherical_reference(
    const PlanarReferenceModel&, const DrizzleObservationView&);
Result<DrizzleObservation> drizzle_frame(
    const PlaneView&, const AstrometrySolution&,
    const PsfView&, const ConditionalNoiseModel&, const DrizzleConfig&);
Result<FrameScientificProduct> publish_frame_product(
    const FrameProductInputs&, AioPublishContext&);
```

`T` 对应数据合同中的有名对象；实现不得用调用方任意字符串替代类型检查。`Result` 的 fail 表示损坏/执行失败；precision 无法辨识是成功返回内带 unavailable/reason 的结果，不由抛异常或空向量暗示。所有视图寿命覆盖同步调用；异步任务持有 Runtime 显式 lease。模板接口可作为现有类成员实现，不能另建全局静态 map。

## 3. 关键对象数值字段

```cpp
struct ReferenceControl {
  std::string control_id, support_id, psf_model_id, covariance_model_id;
  double ra_deg, dec_deg;                 // ICRS
  double source_x0, source_y0;             // detector 0-based centers
  uint64_t leaf_ipix;                     // NESTED at declared leaf_order
  std::optional<double> reference_flux;   // integral unit of reference
  std::optional<double> flux_variance;    // same integral unit squared
  std::optional<double> reference_snr;    // F/sqrt(V_F), not legacy proxy
  PrecisionStatus status;                 // availability/accuracy/omitted terms
  SurfaceTransfer transfer;              // value+unit+condition or reason
};
```

其它逐字段类型按数据合同和 sidecar schema 一一映射；枚举取 schema 的 tokens，单一 serializer 负责映射。`leaf_ipix` 在 JSON 中为十进制字符串，避免读取者用 IEEE double 丢掉 64-bit 索引；leaf order 单独存整数。控制点计算保留 FP64；导出 Catalogue 浏览可降精度但不得逆写 canonical 文件。

PSF 模型必须区分 `flux_estimator=analytic_total|aperture_with_correction|diagnostic`，窗口截断和模型积分一致。检测块 Gaussian 宽度不能直接作为 Moffat 参数。PSF 残差尺度不能冒充背景噪声，FWHM 不能替代模板 h。空间模型基线采用有效样本的局部模型求值，支持外标 limited；没有证据不能默许全帧常 PSF。

## 4. 配置与一次标度应用

在已有 normalize 配置的 `snr` 下扩展 `reference`：

```json
{"reference":{"kind":"explicit_linear_flux","reference_id":"user:ref-star",
              "flux":1000.0,"flux_unit":"adu","normalization":"photo_applied"}}
```

`sed_response` 改为具名 `sed_id`、SED 文件/hash、归一/单位或参考星等及 `response_id`，响应积分规则由 photometry 显式给出；读取配置后成为 ReferenceDefinition，不全链传 JSON。逐帧响应 override 在 block 的 `frame_overrides` 以输入 occurrence 绑定，字段包含 response/masters/gain；不靠 basename 或滤镜字符串推测曲线。override 未提供时继承 block，头部冲突如实记录并按明确配置优先级解析。真实未知曲线记 unknown，不自动拿同名滤镜库代替。

现有 `snr.reference_flux_adu` 仅映射 `legacy_explicit_flux` 诊断；用户明确给新 reference ID/归一后才能进入 canonical。现有 `reference_magnitude` 若没有参考通带/SED/零点合同，同样不能自动升级。缺 reference 输出 unavailable，不阻断合法 signal。缺响应时可保留 native-calibrated signal 或显式相对锚标度；状态为 native/relative，不制造 scale=1 且已绝对测光的假成功。

photometry 优先消费 `CleanedPlane`，保证检测、PSF、flux 和输入像素有效性一致；原校准面保留给 IPV。光度节点输出 `PhotoAppliedPlane` 与 application ID。已 applied 的对象不能再次应用同一 scale；noise 输入若需要原生 Poisson，只能按原生 gain+原生期望建模后通过 D_alpha 变换，不能把 photo-scaled 数值直接除以原生 gain。

## 5. 逐边迁移与代码清理

| 现有载荷/实现 | 确定迁移方向 | 保留范围 |
|---|---|---|
| `p1_wcs.json` | WCS typed object；work cache 显式注册 | explicit_config 适配、debug 旁路、版本化 resume |
| `p1_sources.json`、`p1_psf.json` | source/PSF typed 对象，photometry 明确接 psf | 有效星表记录与模型实现 |
| `p1_phot.json`、photoapplied FITS | typed solution+plane；必要科学 spill 经 AIO | 老文件读取 adapter、未迁边暂存 |
| `p1_snr.json` | legacy diagnostics 与新 planar model 分名，sphere completion 产 canonical | 旧 frame 数值来源可查 |
| `p1_stack.json`、Drizzle 内写 HiPS | DrizzleObservation/AIO staging handle→writer | 大面流式写，不复制整幅球面 |
| `HioSnrControlPoint` / SNR Catalogue | 保留 proxy 类别；可选浏览投影使用新值+明确元数据 | 不修改旧 ABI 结构意义、不伪造转换 |
| `p1_session.cpp` | calibration/cosmetic 局部会话或兼容 facade | 有效局部能力；CLI 只经 P1NodeModule 全链 |

每一边同时修改 descriptor、typed IR builder、`module_ports.registry.json`、artifact-to-port bridge、生命周期与实际取数；注册表标明 `runtime_payload` 或正式 cache carrier，不能写不存在的科学生产者。删除仅供合成 IR 的虚构 flux 输入边，或让其真的承载相同对象；生产事实和测试图不可各有一套定义。

转换只在一个 adapter 完成；禁止双写两份都当权威、typed 不存在时静默读取旧 run 的文件、DEV_MODE 改科学载体。撤掉阶段一组内 passband 全等拒绝时，同时建立每帧 response binding，不能只删检查。共享母版的误差引用以真实 master hash 合并，未知项记 unmodelled，不默认为独立零误差。

## 6. 实施范围与验证含义

执行 Agent 可据本页完成阶段一结构、接口、科学模型和 writer 的实施，无需再选择参考定义或载体。各项验证针对对应行为：原点/SIP 的独立读写、scale 一次与 covariance 二次幂、参考 GLS 的有效协方差、秩亏/不可辨识、sidecar round-trip 和负例、缺模型但合法信号、OFF/ON 取数等价。它们是代码及模型的检验意图，不规定服务器运行批次、数据目录或新的审批门。

当前生产入口/端口和已有科学证据见 NORMALIZE_PIPELINE 与 NORMALIZE_CONVERGENCE；代码改后提交事实差异，尚无实测支持的项目保留 designed/unverified。Mosaic 本轮只兼容读取新产品/能力状态，不改默认权重、背景拟合或 Export 算法。独立数学反例由前台修正科学正本，不由执行者新立一套科学方案。

## 7. 完成实现时的数据边界

### 7.1 Runtime 对象与缓存

对象键为 `(run_id, input_occurrence, object_kind, revision)`；frame_id 由冻结输入清单中的 occurrence 绑定，不由 basename 或源 hash 单独充当。RunContext 拥有每帧 BlockFrame 或同生命周期的 typed payload 容器；容器以类型检查过的 owned immutable object/模型 lease 保存元数据和大面视图。publish 校验唯一 producer 与上游 revisions，acquire 校验实际类型/帧/run/revision，consumer 完成、异常及取消均归还 lease。实现不需另建全局 map，也不要求把 STL 放入 C ABI。

八个现有调度节点可以保持 execute 调用方式；一次节点处理多帧时，对每个 occurrence 发布/获取独立对象。科学数组由 Runtime 预算管理，必要 spill 用带键与 hash 的 AIO cache 句柄；Drizzle 大 tile 可以直接流式进入私有 staging。普通内存充足执行不能为取得上游元数据而写读 p1_*.json；这些文件仅保留具名诊断或显式恢复兼容用途。缓存重算失效须跟随 upstream revision，不能按 mtime 或文件碰巧存在判断。

### 7.2 每帧响应与参考 SED

frame_overrides 在 ingest 时按 input occurrence 一次解析，形成该帧的 masters、gain、response/QE 绑定；优先级为显式逐帧配置、block 继承、允许的真实元数据推导，来源逐项保留。滤镜头名称只是观测记录，不能替代曲线。响应 identity 包含曲线字节 hash、波长/响应单位、光子/能量积分约定与实际 QE 组合。相同 identity 可共享计算缓存，不能拿第一帧的 filter/QE 给不同 identity 的帧用。

sed_response 的基础可执行输入是带 sed_id/hash/波长单位/f_lambda 单位的已归一参考谱，以及本帧响应与拟合系数。复用 photometry 的同一响应积分公开 API 得到 Q_k,ref；在科学分册的 `f_obs=a_k m_k Q_k` 约定下，F_native(u)=a_k m_k(u) Q_k,ref，再按实际像元标度求 F_applied(u)。代码中若存逆系数，须由一个显式 adapter 转换。参考谱已具有绝对归一时不再额外要求任意星等零点；若输入选择星等归一，则须有具名通带、星等系统及其零点/参考谱的可执行定义。缺这些输入的 unavailable 是数据能力状态；有完整输入仍返回 not_wired 则是实现未完成。

逐位置参考通量和模板的标度变换使用 NORMALIZE_DATA §5 的 a_ref、P' 配对，不用中心 alpha 冒充全窗空间变换。显式积分通量参考仍是合法独立路径，不能被用于绕开已配置的 sed_response。

### 7.3 噪声与共同误差

ConditionalNoiseModel 分别承载背景/读噪/暗流、冻结参考源 Poisson、校准/修复算子和共享参数的模型或具名缺项。输入已含哪些分量必须可查询，不能在 completion 重复加源泊松。校准算法对已有母版不确定度输入提供 Jacobian 传播及共享 master hash；仅有母版均值而无原始样本/误差模型时，不从一个文件杜撰母版方差。光度拟合参数协方差及与测量共享数据的交叉项同理，未知交叉项不能默认为0或被称完整。

缺实测噪声输入允许单帧 approximate，但对应模型接口、已知输入的求值与持久化路径须实现并由合成真值检验。基础 diagonal 模型、低秩共同项和需要局部显式协方差的情形需明确 representation，不把所有噪声强塞进一个 variance 数组。PSF 模型失配、母版未知、标度共同误差、几何近似和截断分别反映在状态/来源中；仅缺已知 PSD 随机分量且符合科学分册的前提时才允许 upper_bound。

### 7.4 局部球面模型与独立读取（2026-10-15 更新）

正式工程数值载体以 NORMALIZE_DATA §7.1 为准。**优先的生产载体不是重复写巨大 nl×nl JSON，而是模型精确因子**：source pixel index（长度 m）、球面叶索引（长度 nl）、CSR(R) [nl,m]、对应 source diagonal variance v_j（长度 m）、实际 h=RP、背景基 Z_s、归一/量纲/响应与 PSF/支持域模型，且声明 C_ref=R diag(v)Rᵀ。只有当平面条件噪声确实是 diagonal 时这才是**模型内部精确**表示；未建模 master/scale 等误差仍记 omitted。

超小控制窗可另选显式 C_ref [nl,nl] 作同一模型的对照，但不能把因子 values=[v_j] 的 [m,1] 错当派生 C 的 shape。**当前 `acsd.normalize.local-factor/v1` 单位说明有错位风险**：v_j 的单位是线性源积分量的平方，而派生 C_sphere 单位再多 sr^-2；新正式载体使用版本化 `local-factor/v2`（或等效明确不混读的新 schema）。旧产品严格按版本兼容，不将已经写错 BUNIT 的 v1 在不核对模型来源时悄悄升级。

每控制点记录真实 leaf_order=log2(nside)，不是 tile order；目标 leaf=0 合法，必须有 `has_target_leaf`。CSR R 的每个非零值 \(R_{lj}=(a_{lj}/A_{\rm drop,j})/N_l\)，N_l 是同次正式 Drizzle 产出的**完整**叶归一分母；debug 或 patch 截断算子不得冒称相同 R。PSF P 和 F_ref 的 flux_estimator/单位配对，h=RP 在 sphere leaf 域的单位为 sr^-1。Z 按 [nl,n_bg] 定义，background nuisance 的估计值与单位与模型不可脱钩。

读者除 schema/hash/shape/索引与版本核对外，还必须重算或可复核：P 的总通量口径、h=RP、C factor 与派生单位、V_F、SNR=F_ref/√V_F、T=V_B/V_F、support/leaf 位置、scale once、含 SIP 的 WCS、frame-summary 去重。这些是科学语义而非仅“JSON 能解码”。损坏 refs/null 未经辨识的字段均不得以 availability=available 吞错。

### 7.4A 秩亏球面信息求解器：源空间行子空间投影，而非默认 Jacobi 巨分解

科学证明见 NOISE_SNR §11.4。用同次实际 R 和 D=diag(v)，Q=R D^1/2、W=D^-1/2 [P,Z]，取 rank-revealing SVD/QR 得 Q 的行空间正交基 V_r 或零空间正交基 N；参数信息矩阵为

\[
 M=W^\mathsf{T}V_rV_r^\mathsf{T}W
 =W^\mathsf{T}W-(N^\mathsf{T}W)^\mathsf{T}(N^\mathsf{T}W).
\]

R 满列秩时直接等价于 A_srcᵀD^-1A_src；本轮真实 m≈645、r≈626 时源空间零空间约19 维，可以用小零空间修正构成精度信息，**但 N 的求得仍要可靠的秩揭示数值实现**。对于 nuisance Z 有冗余但 F 可辨识的情况，应以通量分量正交于背景列空间的有效信息 I_F 解 V_F，而不是因为全部参数 M 不可逆就错误拒绝。

推荐优先**数值库提供的可靠 Householder rank-revealing QR / thin SVD**，尽量使用真实稀疏结构；必要时选已验证的高性能 LAPACK/BLAS 后端（必须先检查 ACSD Linux/Windows/CMake 能力及依赖许可）。当前手写 Jacobi/曾失败的 Householder+QL 不能因为耗时严重就直接替换成未验证实现，也不能为了 m≤256 的伪效率把选定观测叶的真实随机贡献裁掉。若资源强迫选不同观测域，明确 `approximate` 与实际丢失贡献比例/误差，它不再是原全叶同一统计估计量。所有算子截断、量纲、rank tolerance/奇异值谱、数值误差与 solver/backend 版本写入真实产品/运行诊断。

### 7.4B 像元积分 PSF 的固定实现边界

唯一默认通量口径 `flux_estimator=analytic_total`。PSF 由 science/psf/PSF.md §4 的单位总通量连续 Moffat4 定义，对各实际源像元面积、亚像元中心相位进行数值求积；部分 footprint 的通量 \(f_{\rm enc}=\sum P_j\) 不重新归一，f_out 从**实际像元集合**求，不以理想圆半径代替实际边界；超 1 不可用 clamp 掩盖。使用 PSF library 的有效模型/空间三角插值，不在 scheduler 独立实现另一个与库口径不同的无单位中心值模板。

已有 scheduler::moffat4_profile 只是 raw center-sampling legacy，即使其 GLS 代数经证明也不足以给 canonical absolute reference SNR。旧代码未改完时所有受其影响控制点标记 `model_invalid_or_unnormalized_psf`/unavailable 或具名 proxy，不宣称 analytic_total 和 accurate；reference flux、Poisson 与 spatial alpha 需要与真正积分模板配对后才可声明完整。缺空间支持、通量修正误差或曲线失败的点按自身状态报告，不能通过每窗重新归一挽救。

### 7.5 发布事务

writer 在私有 staging 内等待所有 AIO 完成，先验证 signal、sidecar、模型与 manifest 的同帧同版本关系，再统一发布并最后标记完成；中途失败/取消不可留下可被普通 reader 当作完整产品的新 sidecar。运行清单反映全部 occurrence，包括 failed/cancelled/precision unavailable。旧 HiPS 的显式读取兼容保留，旧损坏产品不在本机静默修成新 canonical。
