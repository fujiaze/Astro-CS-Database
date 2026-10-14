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
