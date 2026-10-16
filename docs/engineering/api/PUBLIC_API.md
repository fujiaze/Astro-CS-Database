# 公共接口设计

本分册依据[系统架构](../architecture/ARCHITECTURE.md)，规定接口分层、字段语义、所有权、并发和错误处理。C ABI 布局由[ABI 合同](abi/ABI.md)拥有，算法与阶段接口由对应[模块文档](../../detail/00_INDEX.md)说明，实际声明以链接的公开头为核对入口。

## 1. 接口分层与生产路径

| 层 | 调用边界 | 责任与入口 |
|---|---|---|
| CLI | 进程与 JSON/JSONL | 解析命令、配置、事件、取消和退出码，见[CLI 协议](../contracts/CLI_PROTOCOL.md) |
| Runtime/Module | 同工具链 C++ | DAG、模块工厂、计划、执行、资源和产物，见[Runtime](../contracts/RUNTIME.md) |
| 科学库 | 明确输入的函数或对象 | 单主题算法，不自行推断文件依赖或阶段身份 |
| 统一 I/O | 图像、模型和产品文件 | 缓冲/句柄、实际读写、缓存与原子发布 |
| 跨动态库 | 版本化 C ABI | POD/span/opaque handle、调用约定、分配器与状态码 |

用户命令从 `commands.cpp::run_phase1_block/run_phase2_block/run_phase3_block` 进入本阶段 Runtime，再到 `module_adapters.cpp` 的 P1/P2/P3 节点及算法函数。局部 `p1_session_*`、`p2_session_*`、`p3_session_*` 是另有能力范围的库接口，不因名字含 session 就代表完整 CLI。

三阶段的实际调用和接口分别见[Normalize](../../detail/normalize/modules.md)、[Mosaic](../../detail/mosaic/modules.md)和[Export](../../detail/export/modules.md)。共用 HiPS 写库见[writer](../../detail/infrastructure/hips_writer.md)。工具 main、头文件导出符号、descriptor 和真实生产调用各有身份。

## 2. 每个接口必须说明什么

稳定接口的文档和头注释共同给出以下信息，不依赖返回类型或变量名让读者猜测：

| 项目 | 内容 |
|---|---|
| 输入 | 精确类型、可空性、尺寸/步长、有效域、前置条件和输入版本 |
| 科学语义 | 被估计量、单位、坐标系、像元原点、积分/平均及协方差条件 |
| 输出 | 返回值与出参分别表示什么，空结果、部分结果与失败后的缓冲状态 |
| 所有权 | 谁分配/释放、借用期、是否复制、别名/in-place、异步租约 |
| 并发 | 同对象与不同对象是否可并发、内部并行及预算来源、取消粒度 |
| 错误 | 调用失败、逐项失败、能力受限和数值不可辨识分别怎样表达 |
| 调用 | 实际生产者、消费者、配置来源、持久化与测试入口 |

函数族的共同约定可以引用一处，但不同返回码或释放函数不能统一成虚假规则。声明与注释冲突时记录准确差异，读实现确认，不凭接口清单推断已修复。

## 3. Runtime 与模块 C++ 接口

公开声明位于[runtime.h](../../../lib/include/acsd/core/runtime.h)和[module.h](../../../lib/include/acsd/core/module.h)，使用 `acsd::core`。下列是已存在的声明节选，不是新的目标接口：

```cpp
Result<std::unique_ptr<Runtime>> create_runtime(
    const RuntimeResourceBudget& rb) noexcept;

class Runtime {
 public:
  virtual ~Runtime() = default;
  virtual Result<void> load_pipeline(
      const std::string& ir_json, ModuleRegistry& registry) = 0;
  virtual Result<void> run(RunContext& ctx) = 0;
  virtual void cancel() noexcept = 0;
  virtual Result<std::string> inspect() = 0;
  // 其余状态和 trace 成员见 runtime.h。
};

class IModule {
 public:
  virtual ~IModule() = default;
  virtual const ModuleDescriptor& descriptor() const noexcept = 0;
  virtual Result<void> validate_config(const std::string& config_json) = 0;
  virtual Result<ModulePlan> plan(
      const std::string& node_id, const std::string& config_json) = 0;
  virtual Result<void> execute(RunContext& ctx) = 0;
  virtual Result<std::string> inspect() = 0;
  virtual Result<std::string> last_manifest() = 0;
};
```

以上省略命名空间、包含文件和部分 Runtime 成员，供对照实际声明。工厂返回调用方独占的 Runtime。load 同步解释 IR，调用期 registry 不并发修改；run 阻塞至完成、失败或取消，单实例不并发 run。cancel 可由其它线程请求，inspect 不重新计算科学量。模块实例由 registry 工厂创建并以 RAII 释放，IModule 没有另外的公开 create/destroy 成员。

ModuleDescriptor 描述端口及身份；ModulePlan 说明 work_units、估计内存、并行轴、kernel 和 worker 需求。它们是静态描述或计划，不证明实际对象已由端口传输，也不证明资源估计准确。RunContext 和数组的实际所有权、线程 lease 及取消见[对象寿命](../contracts/OWNERSHIP_LIFETIME.md)和[调度](../contracts/SCHEDULER.md)。

Normalize 的 typed 模型目标与现行 artifact/work-file 载体仍有迁移差异，按[Normalize 数据合同](../contracts/NORMALIZE_DATA.md)和[实施差异](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)判断，不能从类型声明宣布迁移完成。

## 4. 算法与 I/O 接口定位

| 主题 | 真实头文件 | 模块责任 |
|---|---|---|
| 校准/缺陷 | [astro_calibration.h](../../../lib/algorithms/calibration/include/astro_calibration.h) | 母版、单帧校准、坏点/列与标度应用 |
| 检测 | [star_detector.h](../../../lib/algorithms/star_detection/include/star_detector.h) | 盲检与给定位置引导拟合，输出成组释放 |
| PSF | [dynamic_psf.h](../../../lib/algorithms/psf/include/dynamic_psf.h)、[像元积分](../../../lib/algorithms/psf/include/acsd/pixel_integrated_psf.h) | 批量参数/状态与单位总通量积分模板 |
| 天体测量 | [ipv_api.h](../../../lib/algorithms/platesolve/cpp/ipv/include/ipv_api.h) | [完整函数、参数、布局与调用](../../detail/normalize/modules/platesolve.md)：solver、Gaia/detector句柄、内存/候选和解 |
| 光度 | [photometric_calib.h](../../../lib/algorithms/photometry/cpp/include/photometric_calib.h) | [完整C/C++字段、布局、寿命与返回分支](../../detail/normalize/modules/photometry.md)，积分/匹配/拟合及诊断 |
| 条件噪声/参考 | [snr_estimator.h](../../../lib/algorithms/noise_snr/cpp/include/snr_estimator.h)、[sparse_reference.h](../../../lib/algorithms/noise_snr/include/acsd/sparse_reference.h) | 平面模型、球面 GLS、状态和摘要 |
| Drizzle | [hp_drizzle_api.h](../../../lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h) | 同次真实几何、信号和 precision 捕获 |
| sidecar/模型 | [normalize_science.h](../../../lib/algorithms/integration/normalize_science/include/acsd/normalize_science.h) | 唯一 serializer、reader、模型加载及发布原语 |
| Mosaic 覆盖/采样 | [coverage.h](../../../lib/algorithms/coverage/include/astro/phase2/coverage.h)、[sampler.h](../../../lib/algorithms/coverage/include/astro/phase2/sampler.h) | 产品发现、容量协议和背景控制观测 |
| 背景 | [sky_plane.h](../../../lib/algorithms/coverage/include/astro/phase2/sky_plane.h)、[upm.h](../../../lib/algorithms/coverage/include/astro/phase2/upm.h) | 当前生产 sky-plane 与库 UPM 模型分别说明 |
| 排异/集成 | [rejection.h](../../../lib/algorithms/coverage/include/astro/phase2/rejection.h)、[integrate.h](../../../lib/algorithms/coverage/include/astro/phase2/integrate.h) | 规划/收集/选择与固定候选数值归约 |
| 导出 | [p3_wcs.h](../../../lib/algorithms/projection/p3_wcs.h)、[p3_resample.h](../../../lib/algorithms/resample/p3_resample.h)、[p3_output.h](../../../lib/algorithms/fits_output/p3_output.h) | WCS/crop、sampler/cache、流式 FITS 写读 |
| 统一 HiPS | [aio_hips.h](../../../lib/infrastructure/aio/include/aio_hips.h) | 产品句柄、tile 视图、层级与文件 |
| Gaia | [gaia_client.h](../../../lib/infrastructure/gaia_xpsd_client/src/gaia_client.h) | 本地星表读取、锥搜与光谱查询 |

表中仅说明归属，不冻结完整导出数量。新增或变更接口须同步实际消费者与模块文档，ABI 变化核对布局/版本及释放约定。相同能力通过公开 API 复用，不在 scheduler 复制 PSF、GLS、几何或 serializer。

## 5. 所有权和异步寿命

调用方数组的借用不转移所有权，调用期必须保持有效；只有明确支持 in-place 才允许输入输出重叠。容量查询/填充函数分别说明所需数量、实际写入数量及容量不足的返回语义。

库分配结果使用配套释放函数，例如检测结果成组 free、PSF 批结果用 dpsf_free_results、HiPS 句柄 finalize/abort、sky-plane 与 UPM 模型用各自 close。调用方不能跨族释放，不能因函数前缀相似选释放器。session inspect 输出按 host allocator 释放；来源不同的缓冲并不都由调用方普通 free。

Runtime/AIO 拥有异步使用的大数组或模型租约，释放发生在所有消费者/在途任务结束之后。异常、取消和发布失败同样执行清理，不能在尚有读取者时销毁 frame。具体实现限制集中在[实施差异](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)，不由接口期望推断已安全。

## 6. 数学量、状态与错误

内部图像坐标用零基像元中心，FITS CRPIX 使用一基；RA/Dec 使用具名坐标系和单位。图像积分量、球面面亮度、参考通量方差、背景控制方差和几何 support 各有类型与条件，见[数据语义](../../science/unified/DATA_SEMANTICS.md)。图像允许负值和有限零；参考源通量正有限的参数条件不能变成负图像拒绝条件。

成功约定按接口族解释，不能普遍假定整数 0：

| 接口族 | 调用结果的解释 |
|---|---|
| Runtime/Module | Result 的成功/错误域 |
| acsd_status | ACS_OK 与该 ABI 的 ACS_ERR_* |
| 校准/sdet | 依公开头的整数码；批级完成不代表逐项成功 |
| IPV solve | 成功与失败按实际 ret 及 result.success 配对，不能套普通 0-success |
| sidecar bool 接口 | bool 与配套结果/错误字段 |
| P3 | P3WcsStatus、P3ResampleStatus、P3OutputStatus 各自解释 |
| 排异/集成 | 调用 rc 与逐栈 status 分别检查 |

空结果、合法 precision unavailable、模型不可辨识和执行错误分别表达。损坏载荷、坏引用/哈希、非法几何与算术溢出不能伪装成普通能力受限。P2 集成具有包含 NON_FINITE_OUTPUT 的六态，不能沿用五态表。

C ABI 不让 C++ 异常跨库，内部转为本族状态；C++ 层使用 Result/Error 并保留原因。局部返回码经 adapter 映射，最终 CLI 退出码由[错误合同](../contracts/LOG_AND_ERROR.md)与[退出码](../standards/ERROR_MODEL.md)维护，工具 main 的裸整数不与其混用。

## 7. 并发、取消与验证

逐函数说明“同一对象能否并发”“不同对象能否并发”和“内部是否并行”，无内部锁不等于不线程安全，有内部并行也不等于外部可共享句柄。worker 来源是宿主预算，调用不能另造资源池。单次同步 API 没有取消参数时，明确其实际安全点，不凭上层 token 宣称可随时中断。

并发验证使用同输入、同二进制、实际预算及事先说明的容差；固定索引写回不自动证明全链逐位相等。接口验证覆盖容量、别名、失败输出、配对释放、ABI 失配、逐项状态与取消后的寿命；科学结果另需独立真值，不由 AST 符号数量保证。

接口原文证据见[来源档案](../../../实验/documentation-refactor/docs/PUBLIC_API_SOURCE.txt)。有效现行设计由下面列出的正式模块/合同承载；来源档案只保留原始说法与证据，不能代替参数、布局或异常语义。

## 8. 接口主题的唯一正式归属

下表按原API主题保持可追溯职责，API编号仅为既有引用标识，不代表功能已验收。完整声明和数据语义在各主题正文；机器索引用于找文件，不反向定义科学量。

| 原主题 | 正式设计的归属与必要边界 |
|---|---|
| API-GAIA-001 | [Gaia模块](../../detail/infrastructure/gaia_xpsd_client.md)：create/query/free、记录、容量/缓存/错误；[查询算法](../../science/algorithms/common/GAIA_QUERY.md)拥有几何与量化 |
| API-CAL/COS-001 | [校准/缺陷模块](../../detail/normalize/modules.md#calibration)：全部主帧/校准/列/方差声明、缓冲、计数与f64实际精度；坏点/列与母版生成不是同一能力 |
| API-STAR-001 | [检测详细设计](../../detail/normalize/modules/star_detection.md)：6完整签名、13参数、6基础/9extras、O1–O16、LM/边界/释放与身份；solver候选和science_sources分开 |
| API-PSF-001 | [PSF模块](../../detail/normalize/modules.md#star-psf)：7完整签名、13字段结果、两种9列布局、compact回映射、两套0–3状态；不以成功前缀猜源ID |
| API-WCS-001 | [IPV详细设计](../../detail/normalize/modules/platesolve.md)：14签名、全部参数/结果与N×6/N×9、ABI2、线程绑定、成功1；算法归PLATESOLVE |
| API-PHOT-001 | [光度详细设计](../../detail/normalize/modules/photometry.md)：7个C声明和C++质量位/Frame装配全字段，F_syn/单位、退化、records原行、实际应用一次 |
| API-NOISE-001 | [Noise模块](../../detail/normalize/modules.md#noise-snr)：27个声明、15种结构、配置1/模型2、floor绑定、分配/释放；球面位置参考GLS按[Normalize合同](../contracts/NORMALIZE_DATA.md)及噪声科学 |
| API-DRZ-001 | [Drizzle模块](../../detail/normalize/modules.md#drizzle)：14完整声明、自动nside/正逆结构、异步job与capture寿命；snapshot句柄不代替正式signal算子 |
| API-HIPS-001 | [共用writer](../../detail/infrastructure/hips_writer.md)：begin/write/finalize/abort、视图/flags/层级/错误；最终产品通道由阶段合同决定 |
| API-COV-001 | [覆盖模块](../../detail/mosaic/modules.md#coverage)及[覆盖算法](../../science/algorithms/mosaic/PHASE2_COVERAGE.md)：两遍容量、NESTED union/order/来源与POD清零；覆盖不是权重 |
| API-P2-SMP-001 | [采样模块](../../detail/mosaic/modules.md#sample)及[采样算法](../../science/algorithms/mosaic/PHASE2_SAMPLER.md)：frame_id/cached/halo、geometry与accepted计数、输出容量、背景统计量方差 |
| API-P2-UPM-001 | [生产背景模块](../../detail/mosaic/modules.md#upm-fit)与[UPM库算法](../../science/algorithms/mosaic/PHASE2_UPM_IMPL.md)：sky-plane生产链、UPM库/persist/dense/source-hash分别说明，不拼接为一条链 |
| API-P2-REJ-001 | [排异模块](../../detail/mosaic/modules.md#reject)及[排异算法](../../science/algorithms/mosaic/PHASE2_REJECTION.md)：AUTO只进plan，strided→compact经source_indices，reason与status、typed方法和大尺度mask分开 |
| API-P2-INT-001 | [集成模块](../../detail/mosaic/modules.md#integrate)：纯reducer/权重构造分开、6态含NON_FINITE_OUTPUT，support包含接受零权样本，失败默认0不能当成功信号 |
| API-P2-HIPS-001 | [Mosaic写出](../../detail/mosaic/modules.md#write)及[旧工具详细算法](../../science/algorithms/mosaic/PHASE2_MOSAIC_WRITE.md)：p2_op_write生产仅SIGNAL；工具配置/diagnostics/裸整数不等同CLI |
| API-P3-PROJ-001 | [Export WCS](../../detail/export/modules.md#wcs)及[投影算法](../../science/algorithms/export/PHASE3_PROJ_IMPL.md)：descriptor/0基变换/1基CRPIX/crop/当前TAN能力；库PA参数与CLI实际传入分开 |
| API-P3-RSMP-001 | [Export重采样](../../detail/export/modules.md#resample2)及[内核算法](../../science/algorithms/export/PHASE3_RSMP_IMPL.md)：actual order、nearest/bilinear/ex、缺tile/NaN、共享有界cache与每worker句柄分别说明 |
| API-P3-FITS-001 | [Export writer/verify](../../detail/export/modules.md#writer)及[FITS算法](../../science/algorithms/export/PHASE3_FITS_IMPL.md)：stream/整幅、crop、临时/发布/重开、状态与hash；当前单PRIMARY信号 |
| API-P1/P2/P3-SESSION | 以下15签名与各阶段module的session语义；进程内局部facade不与实际CLI八/七/五节点混称 |

跨主题共同规范：参数计数/维数先作可表示性核对；size/version握手只适用于确有相应头字段的接口，不能替legacy POD凭空加头。字符串容量与NUL、UTF-8截断、不写失败输出、内存别名及异常边界逐族确认。err/last_error是人类文本，不解析其内容代替状态；同一进程的不同错误码命名空间可以数值相同，不构成额外错误，也不自动转换为CLI码。

科学状态不等于“允许继续”：例如排异INVALID_*虽经输出status表达，调用者仍应拒绝该结果；正常无候选、NO_DATA、缺precision与坏输入分别处理。归约之前的参数/样本检查与发布之前的I/O完整性是数据责任，不是研发审批门。

## 9. 三个局部会话的完整生命周期签名

这些函数沿legacy common_abi_v1.h的acsd_status/acsd_span/host服务；不能与模块化ABI头混include。config/request为UTF-8字节span，count不含NUL，调用期借用并解析；host与回调/allocator寿命覆盖整个handle。create→validate→run→inspect→destroy，同句柄串行；inspect输出由host分配，调用方用同一host free。destroy后不可复用，不能凭C++诊断last_error代替状态码。

```cpp
acsd_status p1_session_create(const acsd_host_services_v1* host, acsd_handle* out);
acsd_status p1_session_validate(acsd_handle h, const acsd_span_u8 config_json);
acsd_status p1_session_run(acsd_handle h, const acsd_span_u8 config_json, int async_io_depth);
acsd_status p1_session_inspect(acsd_handle h, acsd_span_u8* out_manifest_json);
acsd_status p1_session_destroy(acsd_handle h);

acsd_status p2_session_create(const acsd_host_services_v1* host, acsd_handle* out);
acsd_status p2_session_validate(acsd_handle h, const acsd_span_u8 config_json);
acsd_status p2_session_run(acsd_handle h, const acsd_span_u8 config_json);
acsd_status p2_session_inspect(acsd_handle h, acsd_span_u8* out_manifest_json);
acsd_status p2_session_destroy(acsd_handle h);

acsd_status p3_session_create(const acsd_host_services_v1* host, acsd_handle* out);
acsd_status p3_session_validate(acsd_handle h, const acsd_span_u8 request_json);
acsd_status p3_session_run(acsd_handle h, const acsd_span_u8 request_json);
acsd_status p3_session_inspect(acsd_handle h, acsd_span_u8* out_result_json);
acsd_status p3_session_destroy(acsd_handle h);

```

p1只做read/calibrate/cosmetic/write，async_io_depth接受0/1/2并按预算预读，取消在帧边界；不执行完整WCS/测光/球面发布。p2是coverage/sample/UPM/可选persist，取消在阶段边界；sampler的旧“固定1 worker”注释不能覆盖实际预算路径，model要用UPM close而非sky-plane close。p3局部facade的请求/provenance和五节点生产命令各有装配路径，不因头注释“CLI直调”视为同一调用。详细数据/失败和当前差异见对应模块正文；返回ACS_ERR_*再经adapter映射，非成功时不把旧manifest或缓冲占位当完成。

## 10. 并发、容量和来源的消费规则

同对象与不同对象分别声明：solver/detector/session/HiPS writer/P3Sampler按实例串行；无可变共享状态的纯reducer可在独立数组上并发，不能由旧“threadsafe=no（无锁）”反推不能并发。Noise模型注册表当前已有mutex，仍不能并发销毁正在使用的同一model；ac_set_num_threads与write_pool_ensure是执行资源设置，不能当普通纯查询。

大部分同步科学函数无段内cancel参数；在飞调用结束前保持全部借用。异步Drizzle job、AIO队列及stream以wait/收尾为释放界，不能因上层cancel置位提前free。worker实际来源/租约与模块内部OpenMP默认值分开报告；不再沿用旧总表虚构所有库已接内部星批/行带/迭代取消。

probe/fill的需求量与已写量分开，容量不足即使rc0也不能称收齐。coverage_free是清零调用方POD，不释放其数组；sampler输出数组由调用方分配；UPM/sky-plane各自close；检测整组free；PSF分配结果free_results；Noise内部数组free；session inspect用host free。aliasing只在某接口明确支持时允许，不能由const输入推导输出可重叠。

保留帧身份算法的实际内容覆盖及64位截断，不称全球唯一；response/application须绑定实际数值、单位与曲线内容。point_information=1/V_F与surface precision=1/V_B是不同估计量，通过同条件T连接；control_ivar又属于背景统计。旧weight_mode/legacy来源的接受/拒绝由当前配置合同拥有，登记词表不是生产路由，也不能恢复额外权重开关。
