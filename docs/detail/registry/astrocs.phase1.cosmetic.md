# 模块 astrocs.phase1.cosmetic

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 合同三件套落位 `lib/algorithms/cosmetic/`（README/module.yaml/memory.md；
> 落位规则见 docs/detail/README.md）。合同权威 = 三件套 +
> docs/science/algorithms/COSMETIC_ALGORITHMS.md（ALG-COS-001..005，CONTRACT_READY）。
> descriptor 词汇 module_id=astrocs.phase1.cosmetic（p1_cosmetic_descriptor）为
> 编排层口径；模块级事实以三件套与现行生产实现
> lib/algorithms/calibration/src/cosmetic_corrector.cpp 为准。

## 职责与明确非职责

生产模块登记（唯一源 = descriptor）。职责:
坏点检测/修复（热/冷像素全局阈值检测 + 8 连通结构过滤 + 中值/IDW
插值），ALG-COS-001..005（docs/science/algorithms/COSMETIC_ALGORITHMS.md，
CONTRACT_READY）。不做: master 生成/校准算术
（P1-CAL）、FITS 读写（astro_image_io）、参数接线决策（调用方）——
现状生产调用 p1_session.cpp:294-307 未接线母版（检测全禁用、恒等
pass，登记见 COSMETIC_ALGORITHMS.md §10）。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `calibrated` | `DATA-P1-CAL` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `cleaned` | `DATA-P1-COS` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

invalid = NaN（透传，不判坏）；掩码极性 1=坏点
（SCI-CAL-001 §9a）；数据语义 DATA_SEMANTICS §10（DATA-P1-COS，
DATA-P1-COSMETIC 为 descriptor 占位名，合同以
DATA-P1-COS 为准）。

## 公共 header、核心 symbol 与生命周期

模块级: API-COS-001（docs/engineering/PUBLIC_API.md，ac_correct_frame/
ac_correct_frame_f64/ac_set_num_threads，头
lib/algorithms/calibration/include/astro_calibration.h）；编排级: API-P1-002
（PHASE1_API_V1 §2，生命周期 create→validate→run→inspect→destroy，
多模块共享）。迁移目标 astrocs_p1_cosmetic.dll + C ABI adapter（entrypoint
未落地）。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase1.cosmetic`; execution_class=`cpu_heavy`;
parallel_ok=True; 配置=cosmetic JSON（enabled/hot_sigma/cold_sigma/
method/max_structure_size，p1_session.cpp:132-138 校验；正式版本化
schema 待落地）。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是(资源门拒绝 heavy+serial 组合); worker 数=
ThreadBudget.max_workers(禁 hardware_concurrency)。现状并行=OpenMP
像素域 schedule(static)（ThreadLease 迁移整改点）;
确定性=输出 bitwise 与线程数无关（逐像素独立 + 固定遍历顺序）。

## 内存/cache/I-O/所有权

无 cache; 内存 O(n) 额外（检测统计复制 + labels/sizes 向量）;
I-O 零文件/网络; 所有权=调用方分配 buffer
（out/out_hot/out_cold）。

## 错误、日志、指标、取消和 checkpoint

错误码=AC_OK/AC_ERR_PARAM（模块级，ac_correct_frame；AC_ERR_MEMORY/
INTERNAL 死值）；编排级 ACS_ERR_INTERNAL（session 映射
rc!=AC_OK）。无日志（cosmetic 路径零 stderr）。取消=帧粒度（session
层；模块内无检查点）；无 checkpoint。

## 独立 synthetic 验证命令与容差

TEST-COS-DESIGN-001（docs/science/algorithms/COSMETIC_ALGORITHMS.md §9: 合成
fixture FIX-COS-A..F、NumPy oracle rtol=1e-6/atol=1e-7、解析解
bitwise、I1-I6 不变量）；可执行 TEST-P1-COS-001 待建；执行证据
NOT_VERIFIED（验收证据待补）。

## 已知限制

缺陷与现行语义正本 = COSMETIC_ALGORITHMS.md §10；模块级清单见
lib/algorithms/cosmetic/README.md。
