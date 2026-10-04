# 模块 acsd.phase1.wcs-platesolve

> 上游：docs/ACSD_DESIGN.md §8.5（模块与 ABI）、§4.2（WCS 解算：近似指向 + 星表匹配精化）、

> 科学正本：docs/science/detection/ASTROMETRY.md（SCI-WCS-001）、docs/science/algorithms/PLATESOLVE.md
> （ALG-WCS-001，解算算法推导）、docs/science/PHOTOMETRY.md §16（平移精化判据与读数）
> 数据正本：docs/detail/registry/acsd.phase1.wcs-platesolve.md（DATA-P1-WCS 端口表，本页输入输出端口表）
> API 正本：docs/engineering/api/PUBLIC_API.md（API-WCS-001）、API-P1-004
> （docs/engineering/api/PUBLIC_API.md「分阶段 API 面」）
> 依赖：infrastructure/22_gaia_xpsd_client.md（星表查询与坐标语义合同）

模块级事实以 `lib/algorithms/platesolve/README.md` + `module.yaml`（acsd.p1.wcs，
迁移目标 acsd_p1_wcs.dll）为准；合同三件套（README.md / module.yaml ）
entrypoint 未落地。现状构建 `cpp/ipv/build.ps1` / Makefile → `ipv_solver.dll`，
未编入根 CMake 主构建。失败-置信度语义冻结：**CD 退化必须 success = 0** 并按失败
登记。

## 职责与明确非职责

Registry production 模块（唯一源 = module_adapters.cpp 的 p1_wcs_descriptor）。
职责：从校准后像素帧做星点检测与星表匹配并解出 TAN/SIP WCS（含 SIP A/B/AP/BP
与质量指标 `rms_px` / `rms_arcsec` / `n_pairs` / `trans_order`），正反变换一致，
独立星表残差验证。

不做：重检测（消费 star_measurements / star_det 权威块，禁重检测）；重采样图像；
背景/噪声估计；测光与流量定标；星表缓存管理（gaia 句柄注入）。WCS 是坐标合同，
不是权重。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `sources` | `DATA-P1-SOURCES`（编排词汇；模块权威输入 = star_measurements [N,≥15] 权威块 + star_det fallback，本页输入输出端口表） | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::PIXEL`（统一契约 index-is-center，+0.5 桥接至 IPV 接口契约） |
| `wcs` | `DATA-P1-WCS`（本页输入输出端口表） | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS`（RADESYS=ICRS / EQUINOX=2000 写回） |

输入面：检测目录（像素坐标）、参考星表匹配集（Gaia / 离线）、初始猜测（可空）、
配置。数据形态 = detections `[n,6]` + 0.5 契约 → `IpvWcsResult` POD + inlier
`[n,9]`。

输出面：WCS（ICRS，像素中心 / 轴向 / 单位 / SIP / PV 域明确）、匹配表、残差
统计、验证记录。编排写回 CTYPE / CRVAL / CRPIX / CD / RADESYS=ICRS /
EQUINOX=2000 / SIP。

**近似指向只作初值**：由 `wcs.init_source`（`header_pointing` / `config` /
`neighbor_crval`）给出，仅用于星表逆映射的初值；权威 WCS 是求解器在该指向下完成
星表匹配与稳健迭代精化后的唯一输出。解算轮次数是求解器实现细节，不是流程语义。

**生产节点口径**：本节点按帧读校准后像素（`calibrated`）自行做星点检测与星表匹配
（实现 = 注入的 sdet 检测算子句柄：module_adapters.cpp 调
`ipv_solve_from_memory_with_callback_d`，内部单次检测 + callback 同步导出供
PSF 复用），**不消费** `star_detection` 节点的星表，因此本节点在节点序上先于
`star-psf`（最高设计 §4.2）。上表为模块级（算法）端口合同，节点级端口以
`module_adapters.cpp` 的 p1_wcs_descriptor 为准。

invalid = NaN/coverage=0（按 DATA 合同）；求解失败 → `PLATESOLVE_FAILED`，
不写半成品 WCS 头（orchestrator.cpp 两处）。

### 数值落地口径

匹配与拟合的推导正本 = ALG-WCS-001；本页只记落地方式：

- 匹配：像素 → 天球 → 匹配（几何 + 亮度辅助），拒绝离群；
- 拟合：TAN 基（或按需 SIP / PV），最小二乘 + 稳健迭代；
- 输出：CRPIX / CRVAL / CD / PC / CDELT / CTYPE，FITS 1-based 关键字、内部
  0-based 像素中心；
- 系统误差与随机误差分开报告（与测光一致）；
- **二轮精化 = 平移精化**：上游 WCS 与 Gaia DR3 之间存在系统平移，设计须能覆盖
  2″ 量级；精化的必要性取决于上游 WCS 质量，不是流程固定开销（判据与读数
  正本 = docs/science/PHOTOMETRY.md §16）；
- 经度 wrap、极点、轴手性按投影规则处理。

## 公共 header、核心 symbol 与生命周期

模块级 API = API-WCS-001（docs/engineering/api/PUBLIC_API.md；签名头正本
`lib/algorithms/platesolve/cpp/ipv/include/ipv_api.h`，12 导出，生产入口 `ipv_solve_from_detections_v1`）；
编排级 API = API-P1-004（phase session extern "C"）；生命周期
create→validate→run→inspect→destroy。

entrypoint = 检测目录 + 星表 → WCS + 匹配 + 残差；与 gaia_xpsd_client 的查询 /
缓存 / 坐标语义合同联动。求解器状态 RAII（`ipv_solve_create` / `ipv_solve_destroy`）。

### 源文件

`lib/algorithms/platesolve/cpp/ipv/`（`ipv_entry.cpp` 12 导出 + 求解内核）。

## Registry descriptor 与配置 schema

module_id=`acsd.phase1.wcs-platesolve`（registry descriptor 口径）；模块合同
module.yaml 登记 `acsd.p1.wcs`。execution_class=`cpu_heavy`; parallel_ok=True。
配置 = phase config JSON：

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `catalog` | `gaia` | —— | 星表源（gaia / 离线） |
| `catalog_version` | —— | —— | 星表版本（必填） |
| `tweak_order` | `tan` | —— | WCS 模型（tan / sip / pv） |
| `max_matches` | 200 | —— | 匹配上限 |
| `residual_gate` | —— | mas/px | 残差门（拒绝） |

节点侧另接受 `initial_ra` / `initial_dec` / `focal_length` / `pixel_size` 覆盖
（orchestrator.cpp）。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是（资源门拒绝 heavy+serial 组合）; worker 数 =
ThreadBudget.max_workers（唯一取值源）。句柄级互斥使用；现状 OpenMP 仅三角形
投票/选星（整数归并，bitwise 与线程数无关），ThreadLease 未接线。

确定性 = 固定顺序输出（determinism=fixed_reduction_order，ALG-WCS-001 §11.4 F5
冻结断言）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同（bounded）; I-O 单 writer。

所有权 = `IpvWcsResult` 由调用方分配 POD（DLL 仅写不 malloc，无堆所有权转移）；
inlier 缓冲由调用方预分配（`ipv_get_last_inliers`）。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源 = lib/infrastructure/cli/exit_codes.h（本页不复制数值表）。
现状返回 ret 0/1 + `error_msg[256]`（API-WCS-001 返回码节）：几何退化 / 星数
不足 → ret=0 / success=0（error_msg 载因）→ 编排 `PLATESOLVE_FAILED`；
`BLOCK_MISSING`（必需块缺失）；CD 退化坍缩按失败处理（失败-置信度语义见
PLATESOLVE.md §11）。

匹配不足 / 无法收敛 → fail-closed（输出面 = 无 WCS 产物）；残差超门 → 拒绝或
标记两态之一（"尽力拟合"属另一口径）。

取消 = 协作取消（契约：宿主 cancel 通道 → exit 9，最高设计 §7.2；现状缺失）；
模块内无 checkpoint（无断点续算）。

## 独立 synthetic 验证命令与容差

测试标识 = `TEST-P1-WCS-001`（registry descriptor 单源）；执行证据 = NOT_VERIFIED
（未取得验收证据）；容差 = NOT_VERIFIED（同源）。测试设计 = `TEST-WCS-DESIGN-001`
（PLATESOLVE.md §11.4：F1 合成线性场 rms ≤ 0.5″ / F2 astropy SIP oracle
≤ 1e-4 px / F3 CRPIX 不变量 / F4 失败语义负例含 CD 退化注入 / F5 bitwise 确定性 /
F6 WcsTan roundtrip < 1e-6 deg + 独立前向交叉 ≤ 1e-9 deg），fixture 生成器注记
容差来源。

Oracle 面：

- Astropy / WCSLIB 独立正反投影往返一致；
- 注入已知 WCS 图像 → 解算残差符合理论；
- 离群匹配拒绝测试；
- 跨 tile 一致性（多帧同场 WCS 一致性）。

## 已知限制

- 缺陷登记 = PLATESOLVE.md §11.3：CD 退化静默坍缩、AP/BP 半静默、取消点缺失、
  三套 TAN 并存；
- ThreadLease 未接线；协作取消现状缺失；
- 合同三件套 entrypoint 未落地；现状构建产物 `ipv_solver.dll` 未编入根 CMake
  主构建，目标交付形态 acsd_p1_wcs.dll 未落地；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
