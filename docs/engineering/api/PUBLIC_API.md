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

长篇接口清单的[来源原文](../../../实验/documentation-refactor/docs/PUBLIC_API_SOURCE.txt)完整保留其字段、符号、未核实读数及历史调用描述；它是研究资料。当前模块正文和真实头负责定位，仍未逐函数验证的范围在[处置记录](../../../实验/documentation-refactor/REPORT.md)列明。发现的数学或模型问题继续进入[待推导记录](../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)。
