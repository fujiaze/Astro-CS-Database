# 模块 astrocs.phase1.wcs-platesolve

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 合同：SCI-WCS-001（docs/science/ASTROMETRY.md，FROZEN，共享引用不改动）/
> ALG-WCS-001（docs/science/algorithms/PLATESOLVE.md §11 逐符号锚）/
> DATA-P1-WCS（DATA_SEMANTICS §18）/ API-WCS-001（PUBLIC_API WCS 节）。
> 模块级事实以 lib/algorithms/platesolve/README.md（CONTRACT_READY）+
> lib/algorithms/platesolve/module.yaml（astrocs.p1.wcs，迁移目标 astrocs_p1_wcs.dll）
> 为准；现状构建 cpp/ipv/build.ps1 / Makefile → ipv_solver.dll，未编入根
> CMake 主构建。测试设计 TEST-WCS-DESIGN-001（PLATESOLVE.md §11.4）已冻结，
> 可执行测试待建（TEST-P1-WCS-001）。失败-置信度语义冻结：CD 退化必须 success=0
> 并按失败登记（缺陷登记 = PLATESOLVE.md §11.3）。

## 职责与明确非职责

Registry production 模块（唯一源 = module_adapters.cpp 的 p1_wcs_descriptor）。
职责：从校准后像素帧做星点检测与星表匹配并解出 TAN WCS——生产入口
ipv_solve_from_detections_v1（lib/algorithms/platesolve/cpp/ipv/ipv_entry.cpp），
C ABI 12 导出与返回码见 API-WCS-001。不做：重检测（消费调用方测量块）、
重采样、星表缓存管理（API-WCS-001 范围界定）。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `sources` | `DATA-P1-SOURCES`（编排词汇；模块权威输入=star_measurements [N,≥15] 权威块 + star_det fallback，DATA_SEMANTICS §18.1） | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::PIXEL`（统一契约 index-is-center，+0.5 桥接至 IPV 接口契约） |
| `wcs` | `DATA-P1-WCS`（DATA_SEMANTICS §18） | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS`（RADESYS=ICRS/EQUINOX=2000 写回） |

生产节点口径：本节点按帧读**校准后像素**（`calibrated`）自行做星点检测与星表匹配，
**不消费** `star_detection` 节点的星表，因此本节点在节点序上先于 `star-psf`；
上表为模块级（算法）端口合同，节点级端口以 `module_adapters.cpp` 的
p1_wcs_descriptor 为准。

invalid = NaN/coverage=0(按 DATA 合同)；求解失败 → PLATESOLVE_FAILED
不写半成品 WCS 头（orchestrator.cpp 两处）。

## 公共 header、核心 symbol 与生命周期

由 `API-P1-004` 公共 API 定义(phase session extern "C"); 现状 C ABI
12 导出见 API-WCS-001（生产入口 ipv_solve_from_detections_v1，
ipv_entry.cpp）; 生命周期 create→validate→run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase1.wcs-platesolve`; execution_class=`cpu_heavy`;
parallel_ok=True; 配置=phase config JSON（initial_ra/initial_dec/
focal_length/pixel_size 覆盖，orchestrator.cpp）。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是(资源门拒绝 heavy+serial 组合); worker 数=ThreadBudget.max_workers(唯一取值源);
现状 OpenMP 仅三角形投票/选星（整数归并，bitwise 与线程数无关），
ThreadLease 未接线（缺陷登记 = PLATESOLVE.md §11.3）; 确定性=固定顺序输出
（determinism=fixed_reduction_order，ALG-WCS-001 §11.4 F5 冻结断言）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; IpvWcsResult 调用方分配
POD（DLL 仅写不 malloc，无堆所有权转移）；inlier 缓冲调用方预分配
（ipv_get_last_inliers）。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源=lib/infrastructure/cli/exit_codes.h（本页不复制数值表）；
现状 ret 0/1 + error_msg[256]（API-WCS-001 返回码节）；
取消=协作取消（契约：宿主 cancel 通道 → exit 9，最高设计 §6.3；现状缺失，缺陷登记 = PLATESOLVE.md §11.3）；
模块内无 checkpoint（无断点续算）。

## 独立 synthetic 验证命令与容差

测试标识=`TEST-P1-WCS-001`（registry descriptor 单源）；执行证据=NOT_VERIFIED（未取得验收证据）；容差=NOT_VERIFIED（同源）。
测试设计=TEST-WCS-DESIGN-001（PLATESOLVE.md §11.4：合成线性场
rms≤0.5″/astropy SIP oracle ≤1e-4 px/CRPIX 不变量/失败语义负例含
CD 退化注入/bitwise 确定性/WcsTan roundtrip <1e-6 deg + 独立前向交叉
≤1e-9 deg），fixture 生成器注记容差来源。

## 已知限制

见 docs/KNOWN_LIMITATIONS.md 与 `ALG-WCS-001` 合同边界；缺陷登记 =
PLATESOLVE.md §11.3（CD 退化静默坍缩、AP/BP 半静默、取消点缺失、三套 TAN 并存）。
