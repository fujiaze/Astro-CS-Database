---
id: MOD-astrocs-phase2-upm-apply
version: 1.0.0
status: ACTIVE
owner: SA-P2-U21
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-UPM-001, ALG-P2-UPM-IMPL-001, ALG-UPM-001, API-P2-001]
downstream: [DATA-P2-COR, API-P2-UPM-001, TEST-P2-UPM-002]
---

# 模块 astrocs.phase2.upm-apply（P2-UPM-DOC 重写，2026-09-10）

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
> ALG-P2-UPM-001/002）由 P2-XX-INT 对齐修订。本页为 apply 职能；
> fit 职能见 astrocs.phase2.upm-fit.md，模块总页=
> docs/modules/phase2_upm.md。

## 身份与合同落位

- MOD ID：`MOD-astrocs-phase2-upm-apply`（registry 行 ID 沿用）；
  module_id 合同值=`astrocs.p2.upm`（矩阵 P2-UPM 行；descriptor 占位
  `astrocs.phase2.upm-apply` 仅编排层词汇）；dll_target=
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

- 职责（apply）：按 frame_id 稳定绑定逐块校准——calibrated=raw −
  C_f(p)（p2_upm_calibrate_block :1240，与 p2_upm_evaluate_c :1271
  sparse/dense 同一科学语义，ALG-UPM-001 F4）；dense cache 物化/
  读取（p2_upm_materialize_dense_n :1390 分批并行求值→(f,tile) 单
  调序串行写、bit-identical；p2_upm_dense_read_block :1542 stale 拒
  绝 rc=2）；生产 apply 消费链=lib/phase2/tools/stage2.cpp
  （p2_upm_build_geo :432、save :473、materialize_dense_n :482、
  calibrate_block :927/:1272）。
- 非职责：不做 UPM 拟合/权重归一（fit 职能）；不做采样/几何、
  coverage union、积分、排异、写出；不跨滤镜统一（模型 filter 分组
  调用方保证）；不暴露 per-frame gradient 产品（upm.h:11-12 冻结）。

## 输入输出端口、DATA、单位、坐标、invalid

编排层 descriptor 端口表（module_adapters.cpp:618-632，**descriptor
占位**词汇，按 frontmatter registry 生成词保留）:

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `upm_model` | `DATA-P2-UPM` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `calibrated_frames` | `DATA-P2-CAL` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `corrected` | `DATA-P2-COR` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

内核级真实 I/O 合同=DATA-P2-UPM（§25）模型 + DATA-P2-CAL 帧 →
DATA-P2-COR（§26）校准输出：calibrated=raw−C_f(p) FP64 ADU，
sparse/dense 同一科学语义。invalid: null 参数 → rc=1；未知
frame_id → p2_upm_evaluate_c 返回 NaN（显式不可用，禁 frame 0 参数
伪装，upm.cpp:1277-1280）；dense_read_block source hash 不匹配 →
rc=2 stale 拒绝（upm.h:166-167）。

## 公共 header、核心 symbol 与生命周期

- 唯一权威签名头: lib/phase2/include/astro/phase2/upm.h（184 行；
  P2ModelInfo :60-68、calibrate_block :113-、dense_read_block
  :166-。
- 核心 symbol（upm.cpp 16 导出）: p2_upm_build（:929）、
  p2_upm_build_geo（:934）、p2_upm_save（:940）、p2_upm_open
  （:1008）、p2_upm_info（:1233）、p2_upm_calibrate_block（:1240）、
  p2_upm_evaluate_c（:1271）、p2_upm_raw_weight（:1292）、
  p2_upm_normalized_weights（:1325）、p2_upm_geometry_hash（:1346）、
  p2_upm_component_gauges（:1369）、p2_upm_materialize_dense_n
  （:1390）、p2_upm_materialize_dense（:1518）、p2_upm_dense_info
  （:1523）、p2_upm_dense_read_block（:1542）、p2_upm_close（:1559）。
- 生命周期=open（或 fit 侧 build 产物传递）→ info/dense_info →
  calibrate_block/evaluate_c/dense_read_block → materialize →
  p2_upm_close；模型/缓冲所有权=调用方。C API 面: API-P2-UPM-001
  （PUBLIC_API.md）+ 编排级 API-P2-001（FROZEN）。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase2.upm-apply`（占位）；execution_class=
`cpu_heavy`；parallel_ok=True（module_adapters.cpp:618-632）。占位
ID 清单（旧派生词汇，P2-XX-INT 对齐修订）: SCI-P2-UPM-002/
ALG-P2-UPM-002/TEST-P2-UPM-002；DISP-P2UPM-004=descriptor 端口占位
语义 persist→reload。apply 侧无独立求解配置（消费 fit 侧
P2UpmBuildConfig 产物模型）；dense 物化 worker 数经 stage2.cpp:482
与积分一致传入。

## Execution class、并行轴、ThreadBudget lease、确定性

- `cpu_heavy`；并行轴=dense tile 求值（upm.cpp:1477-1484 std::thread
  池，分批求值→(f,tile) 单调序串行写，无 OpenMP）；worker 数=
  Runtime lease 唯一来源（stage2.cpp:482 传入，无
  hardware_concurrency）。
- 确定性: 稠密缓存 bit-identical（:1387-1390 冻结注释）；
  calibrate_block 逐像素独立无跨样本归并（worker 数无关）；既有验
  证=tests/api/test_upm_parallel.py test_02（1/N 科学等价）+
  tests/backend/test_p2002_parallel_upm.py。

## 内存/cache/I-O/所有权

内存=dense 物化上界 kChunk·kLeafPx·8 字节（:1389）；模型 C[frame]
[control] 调用方经 open/build 持有。I/O=唯一 AIO aio_upm_write_sparse
（:1005）+ aio_upm_open/aio_upm_dense_info/aio_upm_read_dense_block
读面；dense cache=空间求值缓存，同模型 hash/目标 order/frame hash
校验。所有权=调用方分配 model 与缓冲，p2_upm_close（:1559）唯一释
放口；无全局状态。

## 错误、日志、指标、取消和 checkpoint

- rc 语义: 0=ok；1=参数/open/parse/IO/未知 frame（:1009-1028/
  :1542-1550）；2=dense stale cache（source hash 不匹配）。未知
  frame_id evaluate_c=NaN（:1277-1280）。无 ACS_ERR_*（旧 registry
  派生词汇以本页修订）。
- 取消: 会话消费面整模型不写半成品（p2_session.cpp:181/:223-227）；
  内核无取消检查点；无段内 checkpoint（dense 物化整缓存一次写）。
- known_defects（登记不改码）: DISP-P2UPM-001（upm.h:154-156/
  :173-175 materialize_dense 重复声明）、DISP-P2UPM-002（upm.h:89-91
  注释漂移 OpenMP vs std::thread）、DISP-P2UPM-003（p2_session.cpp
  :196-204 覆盖键缺口）、DISP-P2UPM-004（端口占位 persist→reload）。
  整改归 P2-UPM-IMPL。

## 独立 synthetic 验证命令与容差

可执行 `TEST-P2-UPM-002` MISSING（P2-UPM-TEST 建立，不冒认）；登记
面=ALG-P2-UPM-IMPL-001 TEST-DESIGN 设计冻结 VERIFIED 承载锚。现状
相邻证据（引用不冒认）: tests/api/test_upm_recovery_oracle.py
test_04（calibrate_block 信号映射/星 flux 不破坏）、tests/unit/
p2_upm_synthetic_test.cpp（sparse/dense 等价面）、tests/backend/
test_p2002_parallel_upm.py。容差权威=ALG-P2-UPM-IMPL-001
TEST-DESIGN 冻结（dense/sparse 1e-12 等价基线）。

## 已知限制

DISP-P2UPM-001..004（上节，登记不改码）；ALG 边界=dense cache 为
空间求值缓存非科学重算（stale 拒绝语义）、未知 frame_id NaN 非异
常；本页旧派生内容（"错误码=ACS_ERR_*"、"取消=host cancel 回调"等
session 层词汇）以本合同页与 SCI-UPM-001/ALG-UPM-001 为准修订。
