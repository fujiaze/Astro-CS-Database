---
id: MOD-astrocs-phase2-upm-fit
version: 1.0.0
status: ACTIVE
owner: SA-P2-U21
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-UPM-001, ALG-P2-UPM-IMPL-001, ALG-UPM-001, API-P2-001]
downstream: [DATA-P2-UPM, API-P2-UPM-001, TEST-P2-UPM-001]
---

# 模块 astrocs.phase2.upm-fit（P2-UPM-DOC 重写，2026-09-10）

> P2-UPM-DOC（2026-09-10）重写为手写合同页（registry 页保留先例
> astrocs.phase1.session.md/astrocs.phase2.coverage.md/
> astrocs.phase2.reject.md）：`astrocs.p2.upm` 合同（registry 行 ID
> 沿用 MOD-astrocs-phase2-upm-fit/upm-apply）——SCI-UPM-001（共享
> FROZEN）/ALG-P2-UPM-IMPL-001/ALG-UPM-001/DATA-P2-UPM（§25）/
> DATA-P2-COR（§26）/API-P2-UPM-001/API-P2-001；合同落位
> lib/phase2_upm/ 三件套（README r1 + module.yaml CONTRACT_READY
> entrypoint=MISSING）；生产源 lib/phase2/src/upm.cpp 1565 行 +
> upm.h 184 行；旧 descriptor 派生词汇（module_id=
> astrocs.phase2.upm-fit/upm-apply、SCI-P2-UPM-001/002、
> ALG-P2-UPM-001/002）由 P2-XX-INT 对齐修订。本页为 fit 职能；
> apply 职能见 astrocs.phase2.upm-apply.md，模块总页=
> docs/modules/phase2_upm.md。

## 身份与合同落位

- MOD ID：`MOD-astrocs-phase2-upm-fit`（registry 行 ID 沿用）；
  module_id 合同值=`astrocs.p2.upm`（矩阵 P2-UPM 行；descriptor 占位
  `astrocs.phase2.upm-fit` 仅编排层词汇）；dll_target=
  `astrocs_p2_upm.dll`（合同值，尚未存在，MISSING 语义归
  P2-UPM-IMPL）。
- 合同三件套：lib/phase2_upm/（README r1 + module.yaml
  CONTRACT_READY entrypoint=MISSING + memory.md），按
  lib/phase2_samp/→lib/phase2_int/→lib/phase2_rej/ 先例新建；
  lib/phase2/ 一套已被 P2-COV 占用。
- 生产源：lib/phase2/src/upm.cpp（1565 行，根 CMakeLists.txt
  :337-346 astrocs_phase2 静态库成员，upm.cpp 列于 :338）+ 唯一权威
  签名头 lib/phase2/include/astro/phase2/upm.h（184 行）。
- owner SA-P2-U21；depends_on_int=P2-SAMP-INT;CPU-005；
  legacy_paths="lib/phase2 upm sources"。

## 职责与明确非职责

- 职责（fit）：消费 DATA-P2-SMP 控制观测，联合求解唯一 UPM——
  production 权重 w=quality_factor×control_ivar（ALG-UPM-CONTROL-
  IVAR-001）→ per-control 归一化 w/Σw×control_reliability → Huber
  IRLS（δ=1.345）+ 图平滑 + 弱零锚 + 连通分量 gauge（分量内最小
  frame_id，ALG-UPM-001 F3/F5）；模型 C[frame][control] 8×8 cell
  双线性场；save 前校验 frame_id_by_index 与 C 行数一致（upm.cpp
  :944-945）。build_geo 变体消费全几何 P2ControlNode，单帧区
  harmonic continuation。
- 非职责：不做采样/几何（P2-SAMP）、coverage union（P2-COV）、积分
  （P2-INT）、排异（P2-REJ）、写出（P2-HIPS）；不处理乘性尺度差；
  不跨滤镜统一；不暴露 per-frame gradient 产品（upm.h:11-12 冻结）。

## 输入输出端口、DATA、单位、坐标、invalid

编排层 descriptor 端口表（module_adapters.cpp:661-678，**descriptor
占位**词汇，按 frontmatter registry 生成词保留）:

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

- 唯一权威签名头: lib/phase2/include/astro/phase2/upm.h（184 行；
  P2ControlObservation :31-57、P2UpmBuildConfig :71-92）。
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

module_id=`astrocs.phase2.upm-fit`（占位）；execution_class=
`cpu_heavy`；parallel_ok=True（module_adapters.cpp:661-678）。占位
ID 清单（旧派生词汇，P2-XX-INT 对齐修订）: SCI-P2-UPM-001/
ALG-P2-UPM-001/TEST-P2-UPM-001；DISP-P2UPM-004=descriptor 端口占位
语义 persist→reload。配置=P2UpmBuildConfig 15 字段（upm.h:71-92），
production 默认单一来源=p2_session.cpp:183-199。

## Execution class、并行轴、ThreadBudget lease、确定性

- `cpu_heavy`；并行轴=观测间 compute_raw/聚合（upm.cpp:513-530/
  :613-661，模块内 std::thread，无 OpenMP）；worker 数=Runtime
  lease 唯一来源（cfg.cpu_workers=ThreadBudget.max_workers，经
  p2_session.cpp:197；无 hardware_concurrency）。
- 确定性: worker-local tsums + tid 升序归并（determinism class D1，
  worker 数无关）；gauge/连通分量/收敛固定顺序；稠密缓存
  bit-identical（:1387-1390）。既有验证=tests/api/
  test_upm_parallel.py test_02_one_t_same_as_n_t_scientific（1/N
  等价）。

## 内存/cache/I-O/所有权

内存 O(n_ctrl + n_frame·n_ctrl)（Model.C [frame][control]
upm.cpp:74）；dense 物化上界=kChunk·kLeafPx·8 字节（:1389）。I/O=
唯一 AIO aio_upm_write_sparse（:1005，ENG-IO-001 原子写）+
aio_upm 读面；dense cache 同模型 hash/目标 order 校验（stale 拒
绝）。所有权=调用方分配 model 与缓冲，p2_upm_close（:1559）唯一释
放口；无内部 cache 与全局状态。

## 错误、日志、指标、取消和 checkpoint

- rc 语义: 0=ok；1=参数/绑定/open/parse/IO（:941/:945/:1009-1028/
  :1234）；2=production 缺 control ivar（:609）与 dense stale cache。
  无 ACS_ERR_*（旧 registry 派生词汇以本页修订）。
- 取消: 会话消费面整模型不写半成品（p2_session.cpp:181 upm_build
  入口检查、:223-227 persist 取消即 close 返回 ACS_ERR_CANCELLED）；
  内核无取消检查点；无段内 checkpoint。
- known_defects（登记不改码）: DISP-P2UPM-001（upm.h:154-156/
  :173-175 materialize_dense 重复声明）、DISP-P2UPM-002（upm.h:89-91
  注释漂移 OpenMP vs std::thread）、DISP-P2UPM-003（p2_session.cpp
  :196-204 覆盖键缺口）、DISP-P2UPM-004（端口占位 persist→reload）。
  整改归 P2-UPM-IMPL。

## 独立 synthetic 验证命令与容差

可执行 `TEST-P2-UPM-001` MISSING（P2-UPM-TEST 建立，不冒认）；登记
面=ALG-P2-UPM-IMPL-001 TEST-DESIGN 设计冻结 VERIFIED 承载锚。现状
相邻证据（引用不冒认）: tests/api/test_upm_recovery_oracle.py
（SYN-005 参数恢复/逐帧偏移/收敛确定性/星 flux 保留）、tests/api/
test_upm_parallel.py（1/N 等价与内存有界）、tests/unit/
p2_upm_synthetic_test.cpp、tests/backend/test_p2002_parallel_upm.py。
容差权威=ALG-P2-UPM-IMPL-001 TEST-DESIGN 冻结。

## 已知限制

DISP-P2UPM-001..004（上节，登记不改码）；ALG 边界=外推锚只引用真
实存在 cell、单帧区 harmonic continuation、legacy snr 权重仅
ablation（SNR-015）；本页旧派生内容（"错误码=ACS_ERR_*"、"取消=
host cancel 回调"等 session 层词汇）以本合同页与 SCI-UPM-001/
ALG-UPM-001 为准修订。
