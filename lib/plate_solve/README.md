# lib/plate_solve — 模块合同（P1-WCS-DOC）

> 状态: **CONTRACT_READY**（P1-WCS-DOC 冻结，2026-09-07，wave W1；实现已存在，
> 模块化迁移落码归 P1-WCS-IMPL；禁止声明 IMPLEMENTED）
> 文档版本: r1（本 README 重写取代 V4.30 营销式旧 README——旧文成功率/性能
> 对比等叙述保留于 GitHub 上游仓库与 memory.md 存档，合同冻结以本版为准）
> 权威来源: SCI=docs/science/ASTROMETRY.md（SCI-WCS-001，FROZEN T102
> 2026-08-23，共享引用不改动）；ALG=docs/algorithms/PLATESOLVE.md
> （ALG-WCS-001 + §11 逐符号源码锚定）；DATA=docs/contracts/
> DATA_SEMANTICS.md §18（DATA-P1-WCS）；API=docs/contracts/PUBLIC_API.md
> （API-WCS-001）；矩阵行=docs/traceability/TRACEABILITY_MATRIX.json
> MOD-astrocs-phase1-wcs-platesolve。
> 唯一权威签名头: lib/plate_solve/cpp/ipv/include/ipv_api.h（238 行；
> 禁止手抄他版）。

## 1. 身份

| 项 | 值 |
|---|---|
| 矩阵行 | P1-WCS（MODULE_MIGRATION_MATRIX.csv） |
| module_id | `astrocs.p1.wcs` |
| owner | SA-P1-W16 |
| 迁移目标 DLL | `astrocs_p1_wcs.dll`（合同值，尚未建立；现状构建产物 `ipv_solver.dll`，见 §8） |
| depends_on_int | CAT-GAIA-INT;P1-STAR-INT |
| module_status | CONTRACT_READY |
| entrypoint | **MISSING**（registry 入口未接；lib/core/src/module_adapters.cpp:450-464 `p1_wcs_descriptor` 持占位 ID，由 P1-WCS-INT 对齐本合同，不得反向作为冻结依据） |
| legacy_paths | `lib/plate_solve`（生产实现 ipv_solver.dll 所在，本合同主落位）+ `lib/phase1/wcs`（旧符号 WcsTan，计划迁移，见 §9） |

落位依据：MODULE_MIGRATION_MATRIX.csv P1-WCS 行 legacy_paths 第一路径即
lib/plate_solve/（生产实现所在），本目录三件套 README.md/module.yaml/
memory.md 为该模块合同冻结唯一落位；lib/phase1/wcs/ 为第二 legacy 路径
（旧符号 astrocs::phase1::WcsTan，CMakeLists.txt:426-428 静态库
astrocs_phase1_wcs，未接 orchestrator 管线，仅单测
tests/unit/p1_wcs_phot_test），其合同并入本 README §9，不另立目录。

## 2. 负责范围 / 不负责

**负责**：帧级天测定标——(a) 消费 PSF/STAR_MEASURE 产出的星点（禁重检测，
PHASE1_API_V1 §2 编排级合同 API-P1-004）；(b) Gaia DR3SP 参考星锥查 +
三角形不变量匹配（k-vector + 投票矩阵 + PROSAC）；(c) 迭代重投影多项式
拟合（order 1→3）+ 稳健扩增精化（CD 阻尼 IRLS + Tukey）；(d) WCS 提取
（CD=trans 线性项/3600、CRVAL=收敛中心、CRPIX=w/2+0.5、SIP A/B 解析 +
AP/BP 7×7 网格反变换、Y-down 输出转换）；(e) IpvWcsResult 质量指标
（rms_px/rms_arcsec/n_pairs/trans_order）与 inlier 9 列缓冲；(f) 编排层
WCS 头写回（CTYPE/CRVAL/CRPIX/CD/RADESYS=ICRS/EQUINOX=2000 + SIP）。

**不负责**（边界，禁止越界）：不做星点检测（消费 star_measurements /
star_det 块）；不做图像重采样（drizzle 域）；不做 Gaia 星表缓存/网络
管理（gaia_xpsd_client 域，句柄注入）；不做星表-图像流量定标（P1-PHOT
域）；不输出逐像素坐标映射网格（仅 FITS WCS 参数化）。

## 3. 输入输出（DATA-P1-WCS，DATA_SEMANTICS §18 唯一权威）

生产通道（ipv_solve_from_detections_v1，orchestrator.cpp:1967 实调）：

| 方向 | 数据 | dtype/shape | 单位/域 |
|---|---|---|---|
| 入 | detections | double `[n,6]` | det_x/det_y pixel（**IPV 接口契约 center=index+0.5**，orchestrator 由统一契约 +0.5 转换 :1867）、flux ADU、mag、sat/has_sat |
| 入 | image_width/height | int | pixel（边缘 5px 过滤基准） |
| 入 | ra0/dec0 | double | deg（OBJCTRA/OBJCTDEC 必需，config 可覆盖） |
| 入 | focal_length_mm/pixel_size_um | double | mm / μm（驱动 s0=206.265·pixel_um/focal_mm） |
| 出 | IpvWcsResult | POD | success/trans_order∈{1,2,3}/cd[4] deg·px⁻¹/crval[2] deg/crpix[2] 1-based/sip_a·b·ap·bp[36]/rms_px/rms_arcsec/n_pairs/ctype[2]/error_msg[256] |
| 出 | inlier 缓冲 | double `[n,9]` | det_x,det_y,gaia_ra,gaia_dec,pred_x,pred_y,residual_x,residual_y,residual_dist |

坐标双契约：统一契约（star_measurements，index-is-center）↔ IPV 接口契约
（center=index+0.5）由 orchestrator 显式桥接；CRPIX 1-based 与 +0.5 契约
自洽。失败语义：**CD det 退化坍缩禁止冒充解**（DISP-WCS-001，§7）。

## 4. 与 SCI-WCS-001 的映射

共享 SCI docs/science/ASTROMETRY.md（不改动）：TAN 投影+SIP 参数化=本模块
输出形式；CRPIX=w/2+0.5 不变量=ipv_wcs.cpp:274-277 冻结实现；ICRS/J2000=
RADESYS/EQUINOX 写回（orchestrator.cpp:2007-2008）；退化条件 §8=DISP-WCS-001
登记域；astropy oracle §11=TEST-WCS-DESIGN-001 F2。科学公式（F1-F6，
PLATESOLVE.md §2）零改动。

## 5. 算法锚定（ALG-WCS-001，PLATESOLVE.md §11 逐符号锚）

生产符号链：ipv_solve_from_detections_v1（ipv_entry.cpp:524）→
IPVSolver::solve_from_memory（ipv_solver.cpp:769）→ 选星/U 构建
（ipv_select.cpp:463-470,:685-693）→ 三角形投票（ipv_triangle.cpp:296-357）
→ iter_trans_solve（ipv_itertrans.cpp:974）→ robust_refine_wcs
（ipv_solver.cpp:680-700，irls_fit_one_step ipv_robust_refine.cpp:661）→
extract_wcs_sip（ipv_wcs.cpp:229：CD :256-266、CRVAL/CRPIX :264-277、
SIP A/B :322-365、AP/BP :400-478、RMS :483-517、Y-down :528-576）。
DISP-WCS-001..006 缺陷清单见 PLATESOLVE.md §11.3（登记不改码）。

## 6. 接口（API-WCS-001，PUBLIC_API.md 唯一权威）

12 个 C ABI 导出（ipv_api.h:84-232 / ipv_entry.cpp:237-649）：句柄
（ipv_solve_create/destroy）、句柄注入（ipv_set_gaia_handle/
ipv_set_detector_handle）、参数（ipv_get_default_params）、生产入口
（**ipv_solve_from_detections_v1**）、legacy/变体（ipv_solve、
ipv_solve_from_memory、ipv_solve_from_memory_with_callback{,_d}）、
inlier 查询（ipv_get_last_inlier_count/ipv_get_last_inliers）。
返回码/失败语义/内存所有权见 PUBLIC_API.md API-WCS-001。

## 7. 现状缺陷与失败-置信度语义（DISP-WCS-001..006，登记不改码）

- **DISP-WCS-001（R1，核心）**：CD 矩阵退化静默坍缩——
  lib/photometric_calib/cpp/src/wcs_transform.cpp:39-49 det<1e-15 →
  cdInv 全零仅 stderr，输出坍缩 CRPIX−1 无错误通道；同族
  lib/phase1/wcs/wcs_tan.cpp:48-51 det<1e-30 → 返回 CRPIX 零日志。
  失败-置信度语义冻结：**退化必须以 success=0 呈现，禁止坍缩值冒充解**
  （PLATESOLVE.md §11.3；验收 TEST-WCS-DESIGN-001 F4）。整改归
  P1-WCS-IMPL/P1-PHOT-IMPL。
- DISP-WCS-002 wcs_transform 错误通道缺失族（cosc<1e-12 哨兵 1e6
  :148-170、牛顿迭代无收敛判据 :221-240）。
- DISP-WCS-003 双 SIP 拟合路径并存（extract_wcs_sip 生产 vs fit_sip
  IRLS+Huber legacy，ipv_sip.cpp:268/:161-263，仅 build_wcs 消费）。
- DISP-WCS-004 AP/BP 拟合奇异半静默（ipv_wcs.cpp:477 仅日志文件 warn，
  无标志位；:322-325 跳过 SIP 同理）。
- DISP-WCS-005 取消检查点缺失 + OpenMP 未接 ThreadBudget（ipv_triangle
  .cpp:302/:347、ipv_select.cpp:810/:1123/:1412/:1756 等）。
- DISP-WCS-006 三套 TAN 实现并存（ipv 生产 / wcs_tan 单测桥 /
  wcs_transform P1-PHOT 域），去留归 P1-WCS-IMPL/P1-PHOT-IMPL。

## 8. 构建/编排现状（如实登记）

- 现状构建：lib/plate_solve/cpp/ipv/build.ps1:27 / Makefile:6（g++
  -std=c++17 -O3 -fopenmp → ipv_solver.dll，MSYS2/MinGW，链接 kernel32；
  依赖 astro_image_io/gaia_client/star_detector DLL）。
- 未编入根 CMake 主构建（根 CMakeLists.txt 无 ipv 目标）；CMake 集成与
  dll 重命名（astrocs_p1_wcs.dll）归 P1-WCS-IMPL。
- 生产调用：orchestrator.cpp:1758 run_stage_platesolve（必需 stage，DLL
  未加载 :1763 退出码 2）；init :1621-1643（create + 句柄注入）；消费
  star_measurements [N,≥15] 权威块（过滤 :1852-1862、+0.5 :1867）+
  star_det fallback（:1878-1897）；调用 :1967；失败 → PLATESOLVE_FAILED
  :1980；WCS 写回 :2003-2050。
- dll_loader ModuleId::PLATESOLVE 加载名与路径吻合（lib/plate_solve/ 下
  ipv_solver.dll）；gaia_client.dll 预加载。

## 9. lib/phase1/wcs legacy 合同（并入，不另立目录）

- 符号：astrocs::phase1::WcsTan（wcs_tan.h，54 行实现 wcs_tan.cpp）；
  crpix 1-based、crval deg、CD deg/px；pix2sky :8-32（RA wrap :28-29）、
  sky2pix :34-52（det<1e-30 → CRPIX :48-51，DISP-WCS-001 同族实例）。
- 构建：根 CMakeLists.txt:426-428 `add_library(astrocs_phase1_wcs STATIC
  lib/phase1/wcs/wcs_tan.cpp)`，:513 链接入 tests。
- 消费方：仅 tests/unit/p1_wcs_phot_test.cpp（roundtrip <1e-6 deg :50，
  TEST-WCS-DESIGN-001 F6 冻结回归锚）。
- 去留：P1-WCS-IMPL 登记迁移决策（并入 astrocs_p1_wcs.dll 或显式退役）；
  本合同冻结其行为语义，不冻结其存续。

## 10. 测试设计（TEST-WCS-DESIGN-001，PLATESOLVE.md §11.4 唯一权威）

F1 合成线性场（rms ≤0.5″/CD ≤2%/|ΔCRVAL|≤1″）；F2 astropy SIP oracle
（|Δ|≤1e-4 px）；F3 CRPIX/Y-down 不变量；F4 失败语义负例（含 CD 退化
注入 → success=0）；F5 确定性（bitwise，线程 1/2/4）；F6 WcsTan
roundtrip <1e-6 deg。可执行 TEST-P1-WCS-001 + EVIDENCE 由 P1-WCS-TEST
落地，容差不得放宽。

## 11. 线程/确定性/资源

threading_model=host_executor_lease（合同值）；现状 OpenMP 并行仅三角形
投票/选星（整数归并+静态调度，输出 bitwise 与线程数无关，
determinism=fixed_reduction_order）；求解主体单线程；FP64 全链路无量化
降级（PREC-105 同族）；Gaia 查询网络 IO 主导。ThreadLease/取消检查点
接线归 P1-WCS-IMPL（DISP-WCS-005）。

## 12. 迁移语义

plan/execute/cancel/inspect 生命周期、C ABI module adapter、
astrocs_p1_wcs.dll 重命名与 CMake 集成由 P1-WCS-IMPL 建立；registry
descriptor（module_adapters.cpp:450-464，ports sources/wcs）由 P1-WCS-INT
对齐本合同；本 README 描述现状 API，不声明 DLL 化完成。禁止声明
IMPLEMENTED。
