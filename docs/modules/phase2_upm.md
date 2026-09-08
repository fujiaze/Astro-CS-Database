---
id: MOD-astrocs-phase2-upm-fit
version: 1.0.0
status: ACTIVE
owner: SA-P2-U21
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-UPM-001, ALG-P2-UPM-IMPL-001, ALG-UPM-001, API-P2-001]
downstream: [DATA-P2-UPM, DATA-P2-COR, API-P2-UPM-001, TEST-P2-UPM-001, TEST-P2-UPM-002]
---

# 模块 astrocs.p2.upm（P2-UPM-DOC 新建，2026-09-10）

> P2-UPM-DOC（SA-P2-U21）新建模块页，冻结合同 `astrocs.p2.upm`
> （fit/apply 两职能同页，registry 行 ID 沿用
> `MOD-astrocs-phase2-upm-fit`/`MOD-astrocs-phase2-upm-apply`）。合同
> 三件套落位 `lib/phase2_upm/`（README/module.yaml/memory.md，按
> `lib/phase2_samp/`/`lib/phase2_int/`/`lib/phase2_rej/` 先例新建；
> `lib/phase2/` 一目录一套已被 P2-COV 占用，不可覆盖）。合同权威=
> 三件套 + docs/science/PHASE2_UPM.md（SCI-UPM-001，共享 FROZEN）+
> docs/algorithms/UPM_SOLVER.md（ALG-UPM-001，权威推导）+
> docs/algorithms/PHASE2_UPM_IMPL.md（ALG-P2-UPM-IMPL-001，实现级
> 合同，SCI/ALG 占位 ID⇒合同 ID 映射声明在其映射节）。descriptor
> 词汇 module_id=`astrocs.phase2.upm-fit`/`astrocs.phase2.upm-apply`
> （module_adapters.cpp:599-616/:618-632）为编排层占位，由 P2-XX-INT
> 对齐 `astrocs.p2.upm`，不得反向作为冻结依据。

## 身份与合同落位

- MOD ID：`MOD-astrocs-phase2-upm-fit`（fit 职能 registry 行 ID 沿用）
  / `MOD-astrocs-phase2-upm-apply`（apply 职能 registry 行 ID 沿用，
  两行同指本页合同 `astrocs.p2.upm`）；module_id 合同值=
  `astrocs.p2.upm`（矩阵 P2-UPM 行；descriptor 占位
  `astrocs.phase2.upm-fit`/`astrocs.phase2.upm-apply`
  module_adapters.cpp:599-632 仅编排层词汇）；dll_target=
  `astrocs_p2_upm.dll`（合同值，尚未存在，MISSING 语义归
  P2-UPM-IMPL）。
- 合同三件套：`lib/phase2_upm/`（README r1 + module.yaml
  CONTRACT_READY entrypoint=MISSING + memory.md），按
  `lib/phase2_samp/`→`lib/phase2_int/`→`lib/phase2_rej/` 先例新建；
  `lib/phase2/` 三件套已被 P2-COV（astrocs.p2.coverage）占用，不可
  覆盖。
- 生产源：`lib/phase2/src/upm.cpp`（1565 行，根 CMakeLists.txt
  :337-346 astrocs_phase2 静态库成员，upm.cpp 列于 :338）+ 唯一权威
  签名头 `lib/phase2/include/astro/phase2/upm.h`（184 行）。模块页=
  本文件。
- owner SA-P2-U21；depends_on_int=P2-SAMP-INT;CPU-005
  （MODULE_MIGRATION_MATRIX P2-UPM 行权威）；
  legacy_paths="lib/phase2 upm sources"。

## 职责与明确非职责

- fit 职责：消费 DATA-P2-SMP 控制观测（P2ControlObservation[]，
  sampler 产物），联合求解唯一 UPM——production 权重
  `w = quality_factor × control_ivar`（control_ivar=1/control_
  variance，ALG-UPM-CONTROL-IVAR-001；control_ivar≤0/非有限 →
  p2_upm_raw_weight rc=2 显式 INVALID，禁静默回退）→ per-control
  归一化 `w_norm = w/Σw × control_reliability`（upm.cpp:1325-1344）
  → Huber IRLS（δ=1.345 无量纲、IRLS 迭代重加权 + 图平滑 + 弱零锚
  + 连通分量逐分量 gauge=分量内最小 frame_id，ALG-UPM-001 F3/F5）。
  模型=C[frame][control] 8×8 control cell 双线性场（upm.cpp:74）+
  frame_index/frame_id_by_index 稳定绑定（绑定仅由稳定 frame_id 决
  定，save 前校验行数一致 :944-945，拒绝写绑定损坏的模型文件）。
  build_geo 变体（:934）消费全几何 P2ControlNode（含单帧区），单帧
  区经全局平滑/Laplacian 延拓（harmonic continuation）。
- apply 职责：按 frame_id 绑定逐块校准——`calibrated = raw −
  C_f(p)`（p2_upm_calibrate_block :1240，与 p2_upm_evaluate_c :1271
  sparse/dense 同一科学语义）；dense cache 物化/读取
  （p2_upm_materialize_dense_n :1390 分批并行求值→(f,tile) 单调序
  串行写、bit-identical，p2_upm_dense_read_block :1542 stale 拒绝
  rc=2）；生产 apply 消费链=lib/phase2/tools/stage2.cpp
  （p2_upm_build_geo :432、save :473、materialize_dense_n :482、
  calibrate_block :927/:1272）。
- 非职责：不做控制点采样/几何（P2-SAMP 上游，DATA-P2-SMP 域）；不
  做 coverage union（P2-COV 域）；不做加权积分与排异（P2-INT/P2-REJ
  下游）；不做马赛克写出（P2-HIPS）；不处理乘性尺度差（SCI-UPM-001
  非目标，已撤销）；不跨滤镜统一（filter 分组调用方保证）；不暴露
  per-frame gradient 产品（upm.h:11-12 冻结）；无 session 依赖
  （模型数据面显式传入；会话消费=lib/phase2_session/p2_session.cpp
  :180-233）。

## 输入输出端口、DATA、单位、坐标、invalid

编排层 descriptor 端口表（module_adapters.cpp:599-616 fit/:618-632
apply 实测；占位词汇，按 registry 生成词保留）:

fit 侧:

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `samples` | `DATA-P2-SMP` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `upm_model` | `DATA-P2-UPM` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

apply 侧:

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `upm_model` | `DATA-P2-UPM` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `calibrated_frames` | `DATA-P2-CAL` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `corrected` | `DATA-P2-COR` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

内核级真实 I/O 合同: 输入 DATA-P2-SMP（DATA_SEMANTICS §23 权威，
P2ControlObservation 13 字段：frame_id/control_id/leaf_ipix u64、
ra_deg/dec_deg/value/uncertainty/snr/ivar/control_variance/
control_ivar/support f64、snr_available int、quality_flags u32；
value=patch median 可负 ADU、ivar 弃用仅诊断、科学权重一律
control_ivar、snr_available=0 时 snr 回退整帧中位）；输出模型对象=
DATA-P2-UPM（DATA_SEMANTICS §25；P2ModelInfo version/precision
0=fp32,1=fp64/target_order/control_count/observation_count/
component_count/model_hash[65] + C[frame][control] FP64）；apply 输
出 DATA-P2-COR（§26，calibrated=raw−C_f(p) FP64 ADU）。
- invalid: null 参数/n_obs=0/frame 绑定不一致/open/parse 失败 →
  rc=1；production 模式 control_ivar≤0/非有限 → p2_upm_raw_weight
  rc=2 → build rc=2（显式 INVALID，禁静默回退 support/SNR，
  upm.h:88-95）；未知 frame_id → p2_upm_evaluate_c 返回 NaN
  （显式不可用，禁 frame 0 参数伪装，upm.cpp:1277-1280）；
  dense_read_block source hash 不匹配 → rc=2 stale 拒绝。

## 公共 header、核心 symbol 与生命周期

- 唯一权威签名头: lib/phase2/include/astro/phase2/upm.h（184 行；
  P2ControlObservation :31-57、P2ModelInfo :60-68、P2UpmBuildConfig
  :71-92、build :95-98、build_geo :103-107、save/open/info
  :108-110、calibrate_block :113-、evaluate_c :122、
  raw_weight 冻结注 :126-133、normalized_weights :139-、
  geometry_hash :146、component_gauges :150、
  materialize_dense 重复声明 :154-156/:173-178（DISP-P2UPM-001）、
  dense_info :159-、dense_read_block :166-、close :180）。
- 核心 symbol（lib/phase2/src/upm.cpp 16 导出，extern "C"）:
  p2_upm_build（:929）、p2_upm_build_geo（:934）、p2_upm_save
  （:940）、p2_upm_open（:1008）、p2_upm_info（:1233）、
  p2_upm_calibrate_block（:1240）、p2_upm_evaluate_c（:1271）、
  p2_upm_raw_weight（:1292）、p2_upm_normalized_weights（:1325）、
  p2_upm_geometry_hash（:1346）、p2_upm_component_gauges（:1369）、
  p2_upm_materialize_dense_n（:1390）、p2_upm_materialize_dense
  （:1518，wrap 到 workers=0 auto）、p2_upm_dense_info（:1523）、
  p2_upm_dense_read_block（:1542）、p2_upm_close（:1559）。
- C API 面: API-P2-UPM-001（PUBLIC_API.md，16 导出符号锚）+ 编排级
  API-P2-001（FROZEN）。
- 生命周期=build/build_geo（调用方持有 void* model）→ info/
  evaluate_c/calibrate_block/dense 物化与读取 → save/open 往返 →
  p2_upm_close（:1559，delete Model；nullptr=rc 0 幂等）。模型/缓冲
  所有权=调用方。

## Registry descriptor 与配置 schema

fit descriptor=module_id `astrocs.phase2.upm-fit`（占位）；apply
descriptor=module_id `astrocs.phase2.upm-apply`（占位）
（module_adapters.cpp:599-616/:618-632）；两 descriptor 均
execution_class=`cpu_heavy`、parallel_ok=true、api_id=API-P2-001、
abi=c++17。占位 ID 清单（旧派生词汇，由 P2-XX-INT 对齐修订）:
fit=SCI-P2-UPM-001/ALG-P2-UPM-001/DATA-P2-UPM/TEST-P2-UPM-001；apply=
SCI-P2-UPM-002/ALG-P2-UPM-002/DATA-P2-COR/TEST-P2-UPM-002；
DISP-P2UPM-004=descriptor 端口占位语义 persist→reload（ports 静态
声明与内核 probe/fill 语义的桥接未验证）。配置=P2UpmBuildConfig 16
字段（upm.h:71-92；production 默认单一来源=p2_session.cpp:183-199:
robust_loss=0 huber、snr_weight_mode=0 snr2_normalized、
huber_delta=1.345、max_iterations=100、tolerance=1e-6、
target_order=coverage 实测、sigma_floor=1e-3、support_power=1.0、
use_ivar_weight=1、control_reliability=1.0；upm/smoothing_lambda/
huber_delta/max_iterations 可被 phase config JSON 覆盖
p2_session.cpp:200-206）。

## Execution class、并行轴、ThreadBudget lease、确定性

- `cpu_heavy`；并行轴=观测间 compute_raw/聚合（upm.cpp:513-530
  worker-local tsums、:613-661 逐 obs 独立 w）与 dense tile 求值
  （:1477-1484 std::thread 池，workers 由调用方传 lease）；模块内
  std::thread 实现，**无 OpenMP**（:1477 注释"无 OpenMP"；upm.h
  :89-91 注释仍写 OpenMP=漂移，DISP-P2UPM-002）。
- worker 数=Runtime lease 唯一来源：cfg.cpu_workers=ThreadBudget.
  max_workers 经 p2_session.cpp:197（§3 blocks=budget）与 stage2.cpp
  :482 传入；模块无 hardware_concurrency 自行开线程；0=auto、1=串行
  reference。
- 确定性: 聚合=worker-local tsums + **tid 升序归并**（:513 注释，
  determinism class D1=worker 数无关、同 worker 数位精确）；
  gauge/连通分量/收敛/归并固定顺序；稠密缓存 bit-identical（
  :1387-1390 冻结注释）；既有验证=tests/api/test_upm_parallel.py
  test_02_one_t_same_as_n_t_scientific（1/N 等价）+
  test_01_deterministic_repeat_each_worker；1/N 等价亦见
  tests/backend/test_p2002_parallel_upm.py。

## 内存/cache/I-O/所有权

- 内存: 构建 O(n_ctrl + n_frame·n_ctrl)（Model.C [frame][control]
  upm.cpp:74 + obs_w 权重缓存）；dense 物化上界=kChunk·kLeafPx·8
  字节（:1389 冻结注释）；无无界缓存。
- I/O: 唯一 AIO 通道=aio_upm_write_sparse（upm.cpp:1005，模型稀疏
  持久化，ENG-IO-001 原子写）+ aio_upm_open/aio_upm_dense_* 读面；
  dense cache=空间求值缓存（frame × tile 的 C_i(p) 值，同模型
  hash/目标 order/frame hash 校验，stale 拒绝）。无其他文件 I/O。
- 所有权: model（void*）与全部输入/输出缓冲=调用方分配与释放
  （p2_upm_close :1559 唯一释放口）；模块无全局可变状态
  （reentrant=yes；并行面经显式 worker 数受控）。

## 错误、日志、指标、取消和 checkpoint

- 错误面=rc 三态 + 调用方语义承载: rc=0 ok；rc=1 参数/绑定/open/
  parse/IO（:941/:945/:1009-1028/:1234 等）；rc=2 production 缺
  control ivar（:609→build 传播，upm.h:132-139 冻结）与 dense stale
  cache（upm.h:166-167 注释 0=ok,1=io/parse,2=stale）。evaluate_c
  未知 frame_id=NaN（:1277-1280）。无 ACS_ERR_* 词汇（本页修订旧
  registry 派生内容）。
- 取消: 会话消费面整模型不写半成品——upm_build 入口检查
  p2_session.cpp:181、persist 段取消点 :223-227（取消即 close 模型
  返回 ACS_ERR_CANCELLED，无半成品文件）；模块内核无取消检查点。
- 无段内 checkpoint（dense 物化整缓存一次写）；stage2 消费链逐阶段
  stage 日志由会话/编排层承载，非模块内输出。
- known_defects（登记不改码）: DISP-P2UPM-001（upm.h:154-156/
  :173-175 materialize_dense 重复声明）、DISP-P2UPM-002（upm.h:89-91
  注释漂移 OpenMP vs std::thread 实现）、DISP-P2UPM-003
  （p2_session.cpp:196-204 覆盖键缺口）、DISP-P2UPM-004（descriptor
  端口占位语义 persist→reload）。整改归 P2-UPM-IMPL。

## 独立 synthetic 验证命令与容差

可执行 `TEST-P2-UPM-001`/`TEST-P2-UPM-002` MISSING（P2-UPM-TEST 建
立，不冒认）；登记面=ALG-P2-UPM-IMPL-001 TEST-DESIGN 节设计冻结
VERIFIED 承载锚。现状相邻证据（引用不冒认）:
tests/api/test_upm_recovery_oracle.py（SYN-005 参数恢复 oracle：
test_01 常数面恢复、test_02 逐帧偏移、test_03 收敛确定性 model_hash
逐位、test_04 星 flux 不破坏）；tests/api/test_upm_parallel.py
（PAR-003：test_01 每 worker 重复确定、test_02 1/N 科学等价、
test_03 内存有界）；tests/unit/p2_upm_synthetic_test.cpp（合成单元
面）；tests/backend/test_p2002_parallel_upm.py（P2-002 生产并行
upm 面）。容差权威=ALG-P2-UPM-IMPL-001 TEST-DESIGN 冻结（ALG-UPM-001
F6 dense/sparse 1e-12 等价基线）。

## 已知限制

- DISP-P2UPM-001..004（上节，登记不改码）。
- ALG 边界: 覆盖并集非凸区外推仅经 tile 内 cell 界锚点（外推锚只引
  用真实存在 cell，upm.cpp tile_gx/gy_bounds）；单帧区=harmonic
  continuation 非数据约束解（SCI-UPM-001 §4）；quality_mode=1
  legacy snr²/(1+snr²)/unc² 权重仅 ablation/诊断（SNR-015）。
- 本页若引用旧派生词汇（"错误码=ACS_ERR_*"、"取消=host cancel
  回调"等 session 层词汇），以本合同页与 SCI-UPM-001/ALG-UPM-001
  为准修订。

## 链接

- 合同三件套：`lib/phase2_upm/`（README/module.yaml/memory.md）
- registry 手写页：docs/modules/registry/astrocs.phase2.upm-fit.md、
  docs/modules/registry/astrocs.phase2.upm-apply.md
- SCI：docs/science/PHASE2_UPM.md（SCI-UPM-001，FROZEN T106，零改
  动；descriptor 占位 SCI-P2-UPM-001/002⇒SCI-UPM-001 映射声明=
  PHASE2_UPM_IMPL.md 映射节）
- ALG：docs/algorithms/UPM_SOLVER.md（ALG-UPM-001，权威推导）+
  docs/algorithms/PHASE2_UPM_IMPL.md（ALG-P2-UPM-IMPL-001，实现级
  合同）；DATA：DATA_SEMANTICS §23（DATA-P2-SMP）/§25（DATA-P2-UPM）
  /§26（DATA-P2-COR）；API：API-P2-UPM-001（PUBLIC_API.md）+ 编排层
  API-P2-001（FROZEN）
