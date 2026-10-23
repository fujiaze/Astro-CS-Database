# 完整模块实施例：observation.calibrate

本篇是可直接据以编码的模块交付规格，覆盖接口、计划、组合、注册、调试和故障。它只引用[FOUNDATION_API](FOUNDATION_API.md)与[COMPUTE_MODULES](COMPUTE_MODULES.md)已定稿接口；不是独立的一套算法/类型。代码是目标实现结构，本轮不修改生产源码，真实服务实现后必须执行本篇案例。

## 交付文件及依赖

| 目标文件 | 必须实现 |
|---|---|
| include/acsd/modules/observation.hpp | CalibrateInput、CalibrationBlock、对应模块函数；当前共享声明从contracts.hpp拆入且只定义一次 |
| src/observation/calibration_kernel.cpp | 唯一逐样本校准与向外数值区间，不含I/O/线程池 |
| src/observation/calibration_plan.cpp | validate_calibration、plan_calibration和固定资源模型 |
| src/observation/calibration_process.cpp | 下述run/finalize/登记，不重复像元公式 |
| src/storage/signal_writer.cpp | 可复用四数组writer协调器；不属于校准科学核 |
| validation/runtime/calibration/* | 本篇固定输入、请求、observed、逐项比较、资源事件及manifest |

依赖只有公开model/runtime、observation、operators/uncertainty、numerics及Storage接口；不依赖CLI、GUI、产品目录或其它process会话。process_id=observation.calibrate，operation_id=calibrate_observation，Input TypeId=acsd.input.Calibrate/1，Output TypeId=acsd.NativeObservation/1；输入只含raw与request。这里的1是typed binding的semantic revision，和产品格式major及wire协议acsd.process/2分别定义，不互相代替；当前生成codec必须与此登记完全一致。

## 输入校验和唯一像元核

validate先检查raw及四个master/DQ的shape、index、单位与尺寸乘积；raw.frame/context/footprints齐全。validate只看元数据；实际像素NaN/DQ一致性和flat逐值范围，在read完成hash后由calibrate_block的计费CPU段检查，不能在同步validate读payload。exposure_seconds必须有限正数；实际使用dark时dark_exposure_seconds>0；实际使用flat时flat_normalization>0。缺省bias/dark/flat分别是显式0/0/1，不分配常量图。raw_noise/master_noise的根身份先核对，不在每块重造。

对每个合格原生样本，b为bias或0，β=t/t_dark（无dark则0），dark signal为D−b或D，取决于dark_contains_bias。f为flat/flat_normalization（无flat则1），输出：

`x = (raw − b − β * (dark_contains_bias ? D−b : D)) / f`。

bad/saturated/nonlinear/missing保原DQ并不给有效信号；flat≤0或非有限不能修复成1，新增missing/具名Issue并给NaN。repaired须有合法G/influence，否则不当已修复样本。负raw/负x本身合法，不clip。数值lower/upper用同一向外四则原语传播，不能把random variance加到该区间；原始已解析数值定义模型，未给物理误差仍列unknown。

初期此kernel使用严格FP64向外基本四则，禁fast-math/隐式FMA，无法保持有限包围时precision_exhausted而不伪造宽度0。高精度扩展复用numerics能力表，不能在同一算法身份下偷偷改原语。每处理一个原生样本（包括DQ跳过）consume_work(task_context,1)；TaskSpec.work_upper=count，metadata/物理I/O字节另记，无每块预算重置。

NoiseModel Jacobian必须按上述实际公式一次求导。例如dark含bias时bias系数是β−1，不可把减bias和dark里的同bias当两个独立噪声。flat、母版和raw的shared_id/influence合并，缺raw photon期望不能由raw负值clip出“精确shot variance”。这些由统一构造器finish_native保留，不在process adapter中另算。

## 可执行计划与资源数值

初拟block_elements=65536，min_block_elements=1，按flatten顺序，尾块为min(block_elements,N−begin)。四种最坏输入是raw+bias+dark+flat各8n bytes，加raw DQ 2n，共34n；新输出value/lower/upper各8n与DQ 2n，共26n。元数据、codec/hash状态和固定区间workspace由Storage/numerics的workspace_upper给出，必须随构建冻结实测/分配上界。

progress lane单块的保守payload峰值是`60n + S`（S=该CPU区间/Storage工作集上界），最终四数组metadata/共享NoiseModel/ErrorRecord/控制节点入R。raw方式append直接借输出Block直到真实写完成，不再额外复制26n；若codec需额外缓冲，其上界计S，否则用raw。E每多允许一个完整cohort至少再预留该cohort的完整live峰值，不能只预留8n输入。

plan只有stream region_id=0，Host调用run_region后执行组合体；region内stages固定：0异步读/校验输入，1CPU校准，2按key写四输出。edges为0→1、1→2，window默认1，发布信用为最低必需key保留。R+F不够按SCHEDULER缩块/改spill；能容纳额外E时可让下一块read/kernel与前块write重叠。每个数组规范块仍按OBJECTS固定1MiB，计算块n不决定科学hash。

plan固定字段：elements=N，block_elements=n，cohorts=ceil(N/n)，adaptive=false；persistent=R，progress_floor=F，elastic仅由Host按剩余额度给。全部四个数组都使用同index/unit/range；DQ的unit为dimensionless，和数值数组的物理Unit不同，writer必须逐字段验证，不能把DQ也标ADU。

## 组合实现及失败清理

以下代码只用已经声明的接口，函数本身属于src/observation。它不展开全图future，也不访问硬件线程、输出路径或环境变量。

```cpp
namespace acsd::examples {
using NativeRef=Ref<NativeObservation>;
using modules::CalibrateInput;

rt::Task<NativeRef> run_calibration_region(
    rt::InvocationContext ctx, Ref<CalibrateInput> input,
    Ref<rt::InvocationPlan> plan) {
  return rt::then(ctx, modules::begin_signal(ctx,plan,input),
    [ctx,input,plan](modules::SignalWriter writer) {
      auto body=modules::stream_map_ordered<modules::CalibrationBlock,modules::SignalBlock>(
        ctx,plan,0,
        [input](modules::BlockContext block) {
          return modules::read_calibration_block(block,input);
        },
        [](modules::BlockContext block, modules::CalibrationBlock data) {
          return modules::calibrate_block(block,std::move(data));
        },
        [writer](modules::BlockContext block, modules::SignalBlock data) {
          return modules::append_signal(block,writer,std::move(data));
        });
      return rt::settle(ctx,std::move(body),
        [ctx,input,writer](Result<rt::Done> completed) -> rt::Task<NativeRef> {
          if(completed.code!=Code::ok) {
            return rt::settle(ctx,modules::abort_signal(ctx,writer),
              [ctx,completed](Result<rt::Done> cleanup) mutable {
                auto failure=modules::failure_like<NativeRef>(completed);
                failure.assessment.issues.insert(failure.assessment.issues.end(),
                  cleanup.assessment.issues.begin(),cleanup.assessment.issues.end());
                return rt::ready(ctx,std::move(failure));
              });
          }
          return rt::then(ctx,modules::seal_signal(ctx,writer),
            [ctx,input](modules::SignalArrays arrays) {
              return modules::finish_native(ctx,input,std::move(arrays));
            });
        });
    });
}

rt::Task<NativeRef> run_calibration(
    rt::InvocationContext ctx, Ref<CalibrateInput> input,
    Ref<rt::InvocationPlan> plan) {
  return rt::run_region<NativeRef>(ctx,plan,0,
    [input,plan](rt::InvocationContext region_context) {
      return run_calibration_region(region_context,input,plan);
    });
}

rt::Task<NativeRef> finalize_calibration(
    rt::InvocationContext ctx, Ref<CalibrateInput> input,
    Ref<rt::InvocationPlan>, Result<NativeRef> result) {
  if(result.code!=Code::ok || !result.value)
    return rt::ready(ctx,std::move(result));
  return modules::validate_native(ctx,input,*result.value);
}

Result<rt::Done> register_calibration(rt::RegistryBuilder& builder) {
  auto codec=builder.register_type<CalibrateInput>(
    generated::type_codec<CalibrateInput>());
  if(codec.code!=Code::ok) return codec;
  rt::ProcessDescriptor descriptor{
    "observation.calibrate","observation",
    "docs/engineering/rebuild/processes/observation.calibrate.md",1,
    {"model","numerics","operators","uncertainty","runtime"},
    {{"normalize.calibration.signal","snapshot_array",1},
     {"normalize.calibration.frame","span",1}}};
  auto process=builder.begin(std::move(descriptor));
  if(process.code!=Code::ok || !process.value)
    return modules::failure_like<rt::Done>(
      Result<rt::Done>{process.code,std::nullopt,process.assessment,
                      process.usage,process.dependencies});
  rt::Operation<CalibrateInput,NativeRef> operation{
    "calibrate_observation",{"acsd.input.Calibrate",1},
    {"acsd.NativeObservation",1},
    &modules::validate_calibration,&modules::bounds_calibration,
    &modules::plan_calibration,
    &run_calibration,&finalize_calibration};
  return builder.add<CalibrateInput,NativeRef>(*process.value,std::move(operation));
}
} // namespace acsd::examples
```

seal_signal内部负责四个writer的部分成功清理，所以seal失败不需要adapter再猜哪个Array已经封存。finish_native若分配/模型校验失败，释放全部尚未交付的SignalArrays；已sealed数组在首次产品publish前仍属私有暂存，不能被cache永久钉住。Task完成即销毁已执行callable的捕获和已消费的中间value；最终输出Ref除外。calibration此例要求完整Native形状，预算中途耗尽不给半个Native value，保原nonok及已处理范围诊断并abort；geometry等允许partial的process另按其显式有效域封存。

## 引导、调用与事件顺序

应用顺序固定：make_services(PoolConfig)→RegistryBuilder→register_builtin_types→register_calibration及其它模块→freeze→make_host→open_command→Host.invoke<CalibrateInput,NativeRef>→wait_external→消费/发布结果→close_command→Host.close_async/外部等待。错误Result逐次检查，不能取空optional.value。command输入的Raw/Calibration由共享reader或独立内存driver构造，不需要已有产品目录。

Host自动产生resolve/validate/plan/run/finalize和usage，module只在已有校准块kernel后发normalize.calibration.signal、在已有frame边界发span；DEV_MODE OFF宏零求值，ON snapshot只复制在手的SignalBlock，不能为了快照调用read第二遍。异常转换为Code/Issue，writer/arena由Host排空回收；输出Ref超调用存活的计费仍归原invocation。

calibrate_block在输出值/双端点/DQ均已freeze且尚在手时调用ACSD_DEV_SNAPSHOT，使用FOUNDATION_API中的SnapshotPayload只读view；不在process adapter中读回完整Native。局部原始噪声/参数的数值字段用SnapshotField标单位，unknown为monostate，不填0。生成codec先由register_builtin_types登记共享Native/Array等类型，模块登记仅追加CalibrateInput；不能重复登记同一TypeId。

## 固定验收输入与故障矩阵

| case | 输入/调度 | 期望 |
|---|---|---|
| C-CAL-1 | raw=[10,20,-2]，bias=2，dark=4且含bias，t=2，t_dark=1，flat=2，normalization=2 | x=[4,14,-8]；区间包含精确值，负值不clip，DQ合法，frame/context/index不变 |
| C-CAL-2 | 同上，但dark不含bias | x=[0,10,-12]，不沿用含bias分支；两次bias根不能独立化 |
| C-CAL-DQ | flat=[2,0,2]或输入NaN但DQ称有效 | flat=0处missing/NaN且具名；冷磁盘输入也只在read+CPU校验后拒绝NaN/DQ冲突，validate无payload I/O；四writer排空且无Native |
| C-CAL-H | N跨1MiB规范边界，block=65536/32768/尾块1，H=R+F及再少1 | 可行配置同规范hash/误差与精确值；不可行在启动前拒绝或按规定缩块，不挂起 |
| C-CAL-ORDER | key0延迟read、key1缓存快，slot=1；再window=2+足E | 最低key保信用，append连续，无高key先占slot死锁；E可行时有I/O/计算重叠 |
| C-CAL-CLEAN | 已cancelled的ctx、science work全部耗尽，再触发abort/close | 内建cleanup节点仍使用预留R/cleanup配额排空；不重启科学工作；unlink失败只追加secondary Issue |
| C-CAL-WRITE | 第二个数组短写、第四个seal失败、取消在write提交后 | 无Native/complete产品，四writer排空并清理私有块；实际写结束前不释放lease |
| C-CAL-ROOT | 两帧共用同bias/master，分别运行/联合读取 | 同SharedId保交叉项；不因process调用次数制造新随机根 |
| C-CAL-DEV | OFF/ON未选/ON所选且D=0或诊断写失败 | 同科学code/模型/hash；未采集/丢样真实记录，诊断不改变科学任务结果 |

除内核精确例外，还必须通过driver→registry→pool/Storage→Native→独立新进程重开/下游消费。实际observed包含plan R/F/E、每stage/claim/key/pipe水位、written/read/work及原始Issue。接口原型编译只验证拼接类型，不算上述服务或科学验收通过。
