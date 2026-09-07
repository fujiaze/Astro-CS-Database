---
id: MOD-astrocs-phase1-drizzle
version: 1.1.0
status: ACTIVE
owner: astrocs-core
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-DRZ-001, ALG-DRZ-001, API-P1-007, API-DRZ-001, DATA-P1-DRZ]
downstream: [TEST-DRZ-DESIGN-001]
---

# 模块 astrocs.phase1.drizzle

> P1-DRZ-DOC（2026-09-07）源码核对修订：本页旧占位 ID
> SCI-P1-DRIZ-001/ALG-005/TEST-P1-DRIZ-001 更正为 SCI-DRZ-001/
> ALG-DRZ-001/TEST-DRZ-DESIGN-001（与 docs/traceability/
> TRACEABILITY_MATRIX.json P1-DRZ 行、lib/drizzle/module.yaml 一致；
> 旧 ID 不存在于任何权威文档）。事实源：lib/drizzle/README.md、
> docs/algorithms/DRIZZLE_GEOMETRY.md、
> run/local/agent_p1_drz_doc/source_facts.md（行号实测底稿）。

## 职责与明确非职责

Registry production 模块（唯一源=module_adapters.cpp descriptor）。
职责：tiled 球面 drizzle（drop 收缩 footprint × HEALPix NESTED leaf
交叠，S-H+Eriksson 面积加权累加 sumFlux/sumArea/sumVarNum/nContrib）、
auto nside、FP32/FP64 通道、HiPS 直写/legacy HISS 输出、反向 drizzle。
不做：S_p=F_p/D_p 归一与 variance finalize（astro_image_io 层）；
master/校准/坏点（P1-CAL/P1-COS）；Phase2 统计合并；线程授予
（现状 omp 遗留 + Runtime lease，ThreadLease 迁移整改点）。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `calibrated` | `DATA-P1-CAL` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `stacked` | `DATA-P1-STACK` | 可 | `UnitId::ADU` | `CoordinateFrame::ICRS` |

> 现状 descriptor（module_adapters.cpp:508-525）data_id=DATA-P1-STACK
> （编排汇总语义）；模块级数据合同 DATA-P1-DRZ
> （DATA_SEMANTICS §11，tile 累加量原始和 + finalize 归一在下游），
> 引用对齐由 P1-DRZ-INT 处理。

invalid：值像素 NaN/Inf 静默跳过（DISP-DRZ-004）；pixfrac∈(0,1]
引擎层严格拒绝；仅 NESTED；covered_area≤0 → variance 记 NaN
（finalize 层合法输出）。

## 公共 header、核心 symbol 与生命周期

现状 C ABI（hp_drizzle_api.h:42,62,70,130,139,140）：
hp_drizzle_fits_to_ahpx / hp_drizzle_run / hp_drizzle_run_hips /
hp_drizzle_reverse_run / hp_drizzle_reverse_capability /
hp_drizzle_reverse_version（API-DRZ-001，PUBLIC_API.md）。编排级
API-P1-007（区间声明）。迁移生命周期 create→validate→run→inspect→
destroy 由 P1-DRZ-IMPL 接线（现状 entrypoint=MISSING：descriptor
未接节点，p1_session 无 drizzle stage）。

## Registry descriptor 与配置 schema

module_id=`astrocs.p1.drizzle`；execution_class=`cpu_heavy`；
parallel_ok=True；配置=pixfrac（缺省 0.8，CFG-001）、ordering=
nested（默认）、nside_mode/nside_value、precision（FP32/FP64 经
header KV "PRECISION"）；版本化 schema 由 P1-DRZ-IMPL 冻结。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`；并行轴=源图像行（schedule(static) 条带 + per-thread
tile 累加器，drizzle_engine.cpp:1670-1671）；**1/N 确定性**=同输入
同线程数 bitwise 可复现（按线程序合并 touched leaf，
:1762-1785）；跨线程数浮点和序不同不保证 bitwise。ThreadLease
现状零命中（omp 遗留通道，CMakeLists.txt:379-382）——迁移整改。

## 内存/cache/I-O/所有权

tile 累加器 leaf 连续数组（禁 per-leaf 全局 map）；TargetGeomCache
per-thread LRU 8192 + run generation 清空；I/O=HiPS 直写（AIO API）
+ legacy .hiss + operation_counts.json；stdout 无日志（全部 stderr）；
buffer 所有权=调用方。

## 错误、日志、指标、取消和 checkpoint

错误码：文件通道正值 1..11、帧通道正负混用（-1..-8/-9/-12/-13，
无集中枚举——登记缺陷）；error_msg[512]。日志=stderr 前缀
[hp_drizzle_api]/[drizzle_engine]/[sink]。指标=DrizzleOpCounters
（operation_counts.json）。取消=模块内无检查点（登记限制）；
checkpoint 无（HiPS 由编排层 overwrite 清理）。

## 独立 synthetic 验证命令与容差

`TEST-DRZ-DESIGN-001`（DRIZZLE_GEOMETRY.md §9）：FP64 通量闭合
<1e-6（主域）/逐 leaf <1e-5、方差 α² worst_rel<1e-4、候选零漏选
9003 例、reverse false_hole/false_fill=0。可执行 TEST-P1-DRZ-001
由 P1-DRZ-TEST 建立。

## 已知限制

ALG-DRZ-001 §10（DISP-DRZ-001..008）+ README §9（NaN 无计数、
pixfrac 双轨、错误码混用、无取消、static 条带负载不均、poly_clip
legacy 零调用）；docs/KNOWN_LIMITATIONS.md。
