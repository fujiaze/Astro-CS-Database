# 模块 astrocs.phase2.upm-fit

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 合同：SCI-UPM-001（docs/science/PHASE2_UPM.md，FROZEN）/ ALG-P2-UPM-IMPL-001
> （docs/science/algorithms/PHASE2_UPM_IMPL.md）/ ALG-UPM-001
> （docs/science/algorithms/UPM_SOLVER.md）/ DATA-P2-UPM（DATA_SEMANTICS §25）/
> DATA-P2-COR（§26）/ API-P2-UPM-001（PUBLIC_API）+ 编排级 API-P2-001
> （docs/api/PHASE2_API_V1.md，FROZEN）。合同落位 = lib/algorithms/upm/ 三件套
> （CONTRACT_READY，规则见 docs/modules/README.md）；生产源 =
> lib/algorithms/coverage/src/upm.cpp + 头
> lib/algorithms/coverage/include/astro/phase2/upm.h。本页为 fit 职能；
> apply 职能见 astrocs.phase2.upm-apply.md，模块总页 = docs/modules/phase2_upm.md。

## 身份与合同落位

- MOD ID：`MOD-astrocs-phase2-upm-fit`；module_id 合同值 = `astrocs.p2.upm`；
  descriptor 词汇 `astrocs.phase2.upm-fit` 仅编排层口径；dll_target =
  `astrocs_p2_upm.dll`（迁移目标，未落地）。
- 合同三件套：lib/algorithms/upm/（CONTRACT_READY；落位规则见
  docs/modules/README.md）。
- 生产源：lib/algorithms/coverage/src/upm.cpp + 权威签名头
  lib/algorithms/coverage/include/astro/phase2/upm.h；构建 = 根 CMakeLists.txt 的
  astrocs_phase2 静态库成员。
- owner SA-P2-U21；depends_on_int=P2-SAMP;CPU-005；
  legacy_paths="lib/algorithms/coverage upm sources"。

## 职责与明确非职责

- 职责（fit）：消费 DATA-P2-SMP 控制观测，联合求解唯一 UPM——
  production 权重 w=quality_factor×control_ivar（ALG-UPM-CONTROL-
  IVAR-001）→ per-control 归一化 w/Σw×control_reliability → Huber
  IRLS（δ=1.345）+ 图平滑 + 弱零锚 + 连通分量 gauge（分量内最小
  frame_id，ALG-UPM-001 F3/F5）；模型 C[frame][control] 8×8 cell
  双线性场；save 前校验 frame_id_by_index 与 C 行数一致（upm.cpp
  :944-945）。build_geo 变体消费全几何 P2ControlNode，单帧区
  harmonic continuation。
- 非职责：不做采样/几何（P2-SAMP）、coverage union（P2-COV）、积分、排异（P2-REJ）、写出（P2-HIPS）；不处理乘性尺度差；
  不跨滤镜统一；不暴露 per-frame gradient 产品（upm.h:11-12 冻结）。

## 输入输出端口、DATA、单位、坐标、invalid

编排层 descriptor 端口表（p2_upm_fit_descriptor，编排层口径）:

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `samples` | `DATA-P2-SMP` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `upm_model` | `DATA-P2-UPM` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

内核级真实 I/O 合同=DATA-P2-SMP（DATA_SEMANTICS §23 权威）输入 +
DATA-P2-UPM（§25）模型对象输出：P2ControlObservation 13 字段（科学
权重一律 control_ivar，ivar 弃用仅诊断）；P2ModelInfo + C[frame]
[control] FP64。invalid: null/n_obs=0 → rc=1；production
control_ivar≤0/非有限 → p2_upm_raw_weight rc=2 → build rc=2（显式
INVALID 禁静默回退，upm.h:88-95）；未知 frame_id → evaluate_c NaN。

## 公共 header、核心 symbol 与生命周期

- 签名头正本: lib/algorithms/coverage/include/astro/phase2/upm.h
  （P2ControlObservation :31-57、P2UpmBuildConfig :71-92）。
- 核心 symbol（upm.cpp 16 导出）: p2_upm_build（:929）、
  p2_upm_build_geo（:934）、p2_upm_save（:940）、p2_upm_open
  （:1008）、p2_upm_info（:1233）、p2_upm_calibrate_block（:1240）、
  p2_upm_evaluate_c（:1271）、p2_upm_raw_weight（:1292）、
  p2_upm_normalized_weights（:1325）、p2_upm_geometry_hash（:1346）、
  p2_upm_component_gauges（:1369）、p2_upm_materialize_dense_n
  （:1390）、p2_upm_materialize_dense（:1518）、p2_upm_dense_info
  （:1523）、p2_upm_dense_read_block（:1542）、p2_upm_close（:1559）。
- 生命周期=build/build_geo → info/evaluate/calibrate → save →
  p2_upm_close；模型/缓冲所有权=调用方。C API 面: API-P2-UPM-001
  （PUBLIC_API.md）+ 编排级 API-P2-001（FROZEN）。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase2.upm-fit`（编排层口径）；execution_class=
`cpu_heavy`；parallel_ok=True。descriptor 派生的占位 ID（SCI-P2-UPM-001/
ALG-P2-UPM-001/TEST-P2-UPM-001）与端口占位语义（persist→reload）的对齐属
迁移目标（未落地）。配置=P2UpmBuildConfig 15 字段（upm.h:71-92），
production 默认单一来源=p2_session.cpp:183-199。

## Execution class、并行轴、ThreadBudget lease、确定性

- `cpu_heavy`；并行轴=观测间 compute_raw/聚合（upm.cpp:513-530/
  :613-661，模块内 std::thread，无 OpenMP）；worker 数=Runtime
  lease 唯一来源（cfg.cpu_workers=ThreadBudget.max_workers，经
  p2_session.cpp:197；无 hardware_concurrency）。
- 确定性: worker-local tsums + tid 升序归并（determinism class D1，
  worker 数无关）；gauge/连通分量/收敛固定顺序；稠密缓存
  bit-identical（:1387-1390）。既有验证=eng/tests/api/
  test_upm_parallel.py test_02_one_t_same_as_n_t_scientific（1/N
  等价）。

## 内存/cache/I-O/所有权

内存 O(n_ctrl + n_frame·n_ctrl)（Model.C [frame][control]
upm.cpp:75）；dense 物化上界=kChunk·kLeafPx·8 字节（:1389）。I/O=
唯一 AIO aio_upm_write_sparse（:1005，ENG-IO-001 原子写）+
aio_upm 读面；dense cache 同模型 hash/目标 order 校验（stale 拒
绝）。所有权=调用方分配 model 与缓冲，p2_upm_close（:1559）唯一释
放口；无内部 cache 与全局状态。

## 错误、日志、指标、取消和 checkpoint

- rc 语义: 0=ok；1=参数/绑定/open/parse/IO（:941/:945/:1009-1028/
  :1234）；2=production 缺 control ivar（:609）与 dense stale cache。
  无 ACS_ERR_*（模块级返回码独立于会话层 ACS 语义）。
- 取消: 会话消费面整模型不写半成品（p2_session.cpp:181 upm_build
  入口检查、:223-227 persist 取消即 close 返回 ACS_ERR_CANCELLED）；
  内核无取消检查点；无段内 checkpoint。
- 已知缺陷（登记不改码，正本 = ALG-P2-UPM-IMPL-001 缺陷清单）：
  materialize_dense 重复声明（upm.h:154-156/:173-175）、注释漂移
  OpenMP vs std::thread（upm.h:89-91）、p2_session.cpp:196-204 覆盖键缺口、
  端口占位 persist→reload；整改面未落地。

## 独立 synthetic 验证命令与容差

可执行 `TEST-P2-UPM-001` 待建；设计冻结 = ALG-P2-UPM-IMPL-001 TEST-DESIGN。
现状相邻证据：eng/tests/api/test_upm_recovery_oracle.py
（参数恢复/逐帧偏移/收敛确定性/星 flux 保留）、eng/tests/api/
test_upm_parallel.py（1/N 等价与内存有界）、eng/tests/unit/
p2_upm_synthetic_test.cpp、eng/tests/backend/test_p2002_parallel_upm.py。
容差 = ALG-P2-UPM-IMPL-001 TEST-DESIGN 冻结。

## 已知限制

缺陷登记 = 上节（登记不改码）；ALG 边界 = 外推锚只引用真实存在 cell、
单帧区 harmonic continuation、snr 权重仅 ablation（SNR-015）。
