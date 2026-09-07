---
id: MOD-astrocs-phase1-cosmetic
version: 1.0.0
status: ACTIVE
owner: astrocs-core
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-CAL-001, ALG-COS-001, API-P1-002]
downstream: [TEST-P1-COS-001]
---

# 模块 astrocs.phase1.cosmetic

> P1-COS-DOC 事实修订（2026-09-07，wave W1）：本页由源码核对后修订——
> 合同 ID 由 W3 骨架占位（SCI-P1-COS-001/ALG-P1-COS-001）更正为真实
> 冻结 ID；模块级事实以 lib/cosmetic/README.md（r1，CONTRACT_READY）
> 与现行生产实现 lib/calibration/src/cosmetic_corrector.cpp 为准；
> descriptor 占位 ID（module_adapters.cpp p1_cosmetic_descriptor）
> 将由 W3 接线任务对齐本页，不得反向作为冻结依据。

## 职责与明确非职责

Registry production 模块(唯一源=module_adapters.cpp descriptor)。职责:
坏点检测/修复（热/冷像素全局阈值检测 + 8 连通结构过滤 + 中值/IDW
插值），ALG-COS-001..005（docs/algorithms/COSMETIC_ALGORITHMS.md，
CONTRACT_READY，P1-COS-DOC 冻结）。不做: master 生成/校准算术
（P1-CAL）、FITS 读写（astro_image_io）、参数接线决策（调用方）——
现状生产调用 p1_session.cpp:294-307 未接线母版（检测全禁用、恒等
pass，DISP-COS-009）。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `calibrated` | `DATA-P1-CAL` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `cleaned` | `DATA-P1-COS` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

invalid = NaN（透传，不判坏——DISP-COS-002）；掩码极性 1=坏点
（SCI-CAL-001 §9a）；数据语义 DATA_SEMANTICS §10（DATA-P1-COS，
P1-COS-DOC 冻结；DATA-P1-COSMETIC 为 descriptor 占位名，合同以
DATA-P1-COS 为准）。

## 公共 header、核心 symbol 与生命周期

模块级: API-COS-001（docs/contracts/PUBLIC_API.md，ac_correct_frame/
ac_correct_frame_f64/ac_set_num_threads，头
lib/calibration/include/astro_calibration.h）；编排级: API-P1-002
（PHASE1_API_V1 §2，生命周期 create→validate→run→inspect→destroy，
多模块共享）。迁移目标 astrocs_p1_cosmetic.dll + C ABI adapter 由
P1-COS-IMPL 建立（entrypoint 现状 MISSING）。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase1.cosmetic`; execution_class=`cpu_heavy`;
parallel_ok=True; 配置=cosmetic JSON（enabled/hot_sigma/cold_sigma/
method/max_structure_size，p1_session.cpp:132-138 校验；正式版本化
schema 由 P1-COS-IMPL 冻结）。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是(heavy+serial 资源门禁止); worker 数=
ThreadBudget.max_workers(禁 hardware_concurrency)。现状并行=OpenMP
像素域 schedule(static)（DISP-COS-008，ThreadLease 迁移整改点）;
确定性=输出 bitwise 与线程数无关（逐像素独立 + 固定遍历顺序）。

## 内存/cache/I-O/所有权

无 cache; 内存 O(n) 额外（检测统计复制 + labels/sizes 向量，
DISP-COS-006）; I-O 零文件/网络; 所有权=调用方分配 buffer
（out/out_hot/out_cold）。

## 错误、日志、指标、取消和 checkpoint

错误码=AC_OK/AC_ERR_PARAM（模块级，ac_correct_frame；AC_ERR_MEMORY/
INTERNAL 死值，DISP-COS-001）；编排级 ACS_ERR_INTERNAL（session 映射
rc!=AC_OK）。无日志（cosmetic 路径零 stderr）。取消=帧粒度（session
层；模块内无检查点，DISP-COS-007）；无 checkpoint。

## 独立 synthetic 验证命令与容差

TEST-COS-DESIGN-001（docs/algorithms/COSMETIC_ALGORITHMS.md §9: 合成
fixture FIX-COS-A..F、NumPy oracle rtol=1e-6/atol=1e-7、解析解
bitwise、I1-I6 不变量）；可执行 TEST-P1-COS-001 由 P1-COS-TEST 建立
（逐任务 TASK_RESULT 证据）。

## 已知限制

DISP-COS-001..011（COSMETIC_ALGORITHMS.md §10）；模块级清单见
lib/cosmetic/README.md §9。
