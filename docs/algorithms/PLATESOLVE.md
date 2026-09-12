# WCS / PlateSolve Algorithms (ALG-WCS)

> ID: ALG-WCS-001  范围: ALG-WCS-001..002  上游 SCI: SCI-WCS-001  状态: DERIVED (T202 冻结; V5 ALG-002 重验 2026-08-28)  模块: plate_solve/cpp/ipv

## 1 上游 SCI 与输入输出

- 上游: `SCI-WCS-001` (CRPIX=w/2+0.5, CD deg/pixel, cd_inv pixel/arcsec, SIP A/B 解析 / AP/BP 7×7网格, Y-down)
- 输入: 星点表 `(x,y)` + Gaia 参考星表 (RA/Dec)
- 输出: `WCS` (CD+CRPIX/CRVAL+SIP A/B/AP/BP) 或 `NO_SOLUTION`

## 2 离散公式

```text
F1: CRPIX = w/2+0.5, h/2+0.5 (1-based), 0-based x0=CRPIX−1
F2: CD = trans.linear/3600 (deg/pixel), cd_inv = inv(trans.linear) (pixel/arcsec)
F3: SIP前向 A[i][j]=cd_inv·trans.x_ij, B[i][j]=cd_inv·trans.y_ij (解析)
F4: SIP逆向 AP/BP = argmin ||UV−(u,v)−SIP(u,v)||² on 7×7 grid, AP[1,0]-=1, BP[0,1]-=1
F5: Y-down: cd12,cd22 取反; A'=A·(-1)^j, B'=−B·(-1)^j, AP/BP 同规则
F6: 投影 TAN + SIP畸变 + J2000, 极区 Lipschitz C=π/2 / C45=π/(2√2) conservative prune
```

来源: `ipv_wcs.cpp:153-576` `ipv_select.cpp:695` `gaia_client.c:polar_plane_intersects`

## 3 伪代码

```text
function solve_wcs(detections, gaia):
  if n_detections < min_stars → NO_SOLUTION
  triangles = build_triangles(detections) scale_tol=0.002
  matches = kd_match(triangles, gaia_triangles) tol=5.0"
  for each hypothesis:
    trans = iterative_reproject(matches) conv 0.01" max5 (ipv_solver.cpp)
    sip = build_sip(trans) order 2-3, IRLS 15× ε1e-6 Huber 1.345 (ipv_sip.cpp:238-262)
    wcs = compose(CRPIX,CRVAL,CD, sip, Y-down)
    rms = residual(wcs, matches)
  best = min rms, rank by n_matches + rms
  if rms > threshold → NO_SOLUTION else return wcs

function build_sip(trans):
  cd_inv = inv(trans.linear)
  for (i,j) in order: A[i][j]=cd_inv·trans.x_ij, B[i][j]=cd_inv·trans.y_ij
  grid = 7×7 UV = cd_inv·IWC, fit AP/BP via least squares
  AP[1,0]-=1; BP[0,1]-=1
  apply Y-down sign flips

Polar prune: if |dec|>45° use C/C45 disk B(q,C·radius), false_negative=0
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| `n < min_stars` | `NO_SOLUTION` |
| `det(trans.linear)==0` | reject SIP, `NO_SOLUTION` |
| `NB_GRID` 奇异 | fallback linear |
| 极区跨界 `θ+radius>90°` | 保守不剪枝遍历 |
| RA环绕 `dra>180°` | `dra=360−dra` + cos(dec) 缩放 |
| 输入含 NaN | skip/fail per-field |

## 5 确定性与归约

- 单线程求解，三角形匹配 KD-tree 确定性（排序 tie-break by frame_id）；SIP LS 按 grid 索引固定顺序；无跨假设归约。

## 6 时间/空间复杂度

- 匹配 O(n log n)；SIP O(order²·49) LS；空间 O(n_triangles)

## 7 CPU-only 后端策略（V5）

- 仅 CPU：求解为逐星/逐帧独立算术，可经 worker pool（按 affinity）帧级并行，**禁止硬编码线程数**；Gaia 查询由 gaia_xpsd_client 网络 IO 主导，无计算瓶颈。

## 5c SIMD 安全与取消点

- CD/SIP 矩阵算术逐元素独立；最小二乘(7×7 grid AP/BP)为确定性顺序归约(样本序固定)——**禁止并行重结合**；FP64 全链路禁 fast-math。
- 取消点: 按帧(星表行块)粒度检查; 取消时丢弃半成品 trans 并返回错误码(语义随 API 冻结)。

## 8 参考实现/Oracle

- 合成投影图已知 WCS 恢复残差 `<0.1"`；astrometry.net 语义对照 (05 spec)；Astropy WCS 前向/逆向 `Δ<1e-4 px`。

## 9 容差来源

- 收敛 0.01" (pixel/3600), 尺度容差 0.002 (各向异性 0.2%), Huber 1.345 (robust 统计), 预冻结。

## 10 关联 ARC/API/TST

- ARC: `THREADING_MODEL.md` 单线程阶段内串行
- API: `ipv_api.h: ipv_solve_from_detections_v1`, `ipv_wcs.h: build_wcs`
- TST: `TST-WCS-001` 合成恢复, `TST-WCS-INV` 极区保守, `TST-WCS-FAIL` 退化

## 11 P1-WCS-DOC 冻结附录（2026-09-07，SRC-WCS-001 源码实测）

### 11.1 ALG-WCS-001 逐符号锚

生产通道 = orchestrator PLATESOLVE 阶段直调 `ipv_solve_from_detections_v1`
（DLL=ipv_solver.dll，MinGW/MSYS2 g++ 构建 `build.ps1:27` / `Makefile:6`，
`-fopenmp -O3`）；消费 PSF 产出 star_measurements 权威块。行号均 grep 实测。

| 符号 | 锚 | 角色 |
|---|---|---|
| ipv_solve_create | ipv_entry.cpp:266（声明 ipv_api.h:84） | 句柄生命周期 |
| ipv_solve_destroy | ipv_entry.cpp:278（ipv_api.h:87） | 句柄释放 |
| ipv_set_gaia_handle | ipv_entry.cpp:289（ipv_api.h:90） | Gaia 句柄注入 |
| ipv_set_detector_handle | ipv_entry.cpp:302（ipv_api.h:93） | sdet 句柄注入 |
| ipv_get_default_params | ipv_entry.cpp:315（ipv_api.h:198） | IpvParams 默认值（log_dir 空=无日志） |
| ipv_get_last_inlier_count | ipv_entry.cpp:343（ipv_api.h:224） | inlier 计数查询 |
| ipv_get_last_inliers | ipv_entry.cpp:357（ipv_api.h:232） | inlier 9 列缓冲（ipv_api.h:203-221：det_x/det_y/gaia_ra/gaia_dec/pred_x/pred_y/residual_x/residual_y/residual_dist） |
| ipv_solve | ipv_entry.cpp:374（ipv_api.h:97） | 文件路径入口（legacy） |
| ipv_solve_from_memory | ipv_entry.cpp:406（ipv_api.h:110） | PipelineFrame 内存入口 |
| **ipv_solve_from_detections_v1** | ipv_entry.cpp:553（ipv_api.h:146） | **生产入口**（检测坐标 double 数组直入） |
| ipv_solve_from_memory_with_callback | ipv_entry.cpp:595（ipv_api.h:165） | 回调进度变体 |
| ipv_solve_from_memory_with_callback_d | ipv_entry.cpp:639（ipv_api.h:182） | 回调变体 FP64 |
| do_solve_from_detections_v1_impl | ipv_entry.cpp:459 | 参数装配 → IPVSolver::solve_from_memory；try/catch → set_error_msg（:141，:181-187/:218-224） |
| IPVSolver::solve_from_memory | ipv_solver.cpp:769 | 主求解流程（入口日志 :794） |
| 选星 + U 构建 | ipv_select.cpp:463-470（flux 降序取前 img_n_target）、:685-693 | U=(det_x−cx, −(det_y−cy)) 像素、Y-up、原点图像中心；s0=206.265·pixel_um/focal_mm（:49,:253） |
| 三角形投票 | ipv_triangle.cpp:296-357 | 线程局部投票矩阵（:296-300）+ omp for schedule(dynamic,64)（:309-311）+ 整数归并 collapse(2) schedule(static)（:347-357） |
| iter_trans_solve | ipv_itertrans.cpp:974 | 迭代重投影多项式拟合（order 1→3） |
| robust_refine_wcs | 调用点 ipv_solver.cpp:680-700；irls_fit_one_step ipv_robust_refine.cpp:661 | 稳健扩增精化（CD 阻尼 + Tukey biweight），失败回退不破坏主解 |
| extract_wcs_sip | ipv_wcs.cpp:229 | WCS+SIP 提取（生产路径） |
| CD = trans 线性项/3600 | ipv_wcs.cpp:256-266 | 度/像素（F2） |
| CRVAL/CRPIX 冻结 | ipv_wcs.cpp:264-277 | CRPIX=w/2+0.5, h/2+0.5（1-based，F1） |
| ctype 选择 | ipv_wcs.cpp:283-290 | order≤1 → RA---TAN/DEC--TAN；否则 -SIP 后缀 |
| SIP A/B 解析 | ipv_wcs.cpp:322-365 | cd_inv=inv(trans 线性项)（det<1e-15 warn :322-325）；A[i*6+j]=cd_inv·trans.x_ij（F3） |
| SIP AP/BP 网格反变换 | ipv_wcs.cpp:400-478 | 7×7 网格最小二乘；AP[6]−=1、BP[1]−=1（:456-461，F4）；奇异仅 warn（:477） |
| RMS 统计 | ipv_wcs.cpp:483-517 | rms_arcsec=√(Σr²/n)；rms_px=rms_arcsec/s0 |
| Y-down 输出转换 | ipv_wcs.cpp:528-576 | cd12/cd22 取反（:542-544）；A/B/AP/BP 符号规则（:546-571，F5） |
| inlier 缓存 | ipv_solver.cpp:744-752 | cache_last_inliers_（WCS Gate v2 双层闭环） |
| orchestrator 过滤+坐标契约 | orchestrator.cpp:1855-1876 | star_measurements [N,≥15] FLOAT64；status∈{0,3}、sat r[13]、fwhm r[7]∈[0.5,20]、边缘 5px；**+0.5 转换 :1867**（统一契约 index-is-center → IPV 接口契约 center=index+0.5）；sdet fallback 坐标已是 +0.5 契约（:1878） |
| orchestrator 调用与写回 | orchestrator.cpp:1967-2050 | 求解调用 :1967；失败 → PLATESOLVE_FAILED :1980；CTYPE/CRVAL/CRPIX/CD + RADESYS=ICRS/EQUINOX=2000 写回 :2003-2010；SIP A/B/AP/BP 写回 :2017-2049 |

### 11.2 返回码/失败语义（含像素中心契约）

- C ABI 层：ipv_solve_from_detections_v1 返回 0=失败/1=成功；result->success
  0/1；error_msg[256] 由 set_error_msg（ipv_entry.cpp:141）在 NULL 参数、
  C++ 异常路径填充（:181-187/:218-224）；success=1 时 IpvWcsResult POD
  （ipv_api.h:39-61：cd[4]/crval[2]/crpix[2](1-based)/sip_a·b·ap·bp[36]/
  rms_px/rms_arcsec/n_pairs/trans_order/ctype[2]）为唯一权威输出。
- 求解器层：三角形匹配 0 匹配或 iter_trans_solve 全阶失败
  （ipv_solver.cpp:883-921）→ fail_result（trans_order=0, success=false）
  显式返回，不抛异常不崩溃。
- 编排层（orchestrator.cpp）：DLL 未加载 :1763；data 块缺失 →
  BLOCK_MISSING :1814；star_measurements 缺失/格式错 → BLOCK_MISSING
  :1832；过滤后 0 星 → BLOCK_MISSING :1896；求解失败 → PLATESOLVE_FAILED
  :1980。
- **像素中心双契约**：统一契约（star_measurements，index-is-center）与
  IPV 接口契约（center=index+0.5）由 orchestrator :1867 显式 +0.5 转换
  桥接；sdet fallback 坐标已在 +0.5 契约（:1878）。消费方对 IpvWcsResult/
  inlier 缓冲坐标必须按 +0.5 契约解读（CRPIX 1-based 与其自洽）。
- 线程安全：solver 句柄级互斥使用；gaia/detector 句柄由调用方保证生存期；
  无内部锁。

### 11.3 现状缺陷清单（DISP-WCS-001..006，登记不改码，整改归 P1-WCS-IMPL/INT）

- DISP-WCS-001 CD/线性变换退化静默坍缩（R1 登记项，失败-置信度语义核心）：
  lib/photometric_calib/cpp/src/wcs_transform.cpp:39-49 构造时
  det<1e-15 → cdInv 全零 + 仅 stderr 警告，pixelToSky/skyToPixel 输出坍缩
  到 CRPIX−1 附近，无错误码无标志位；同族 lib/phase1/wcs/wcs_tan.cpp:48-51
  det<1e-30 → 直接返回 CRPIX 且零日志。调用方无法区分"真解≈CRPIX"与
  "退化坍缩=CRPIX"（wcs_transform 输出恒 CRPIX−1，0-based，更不可判）。
  失败-置信度语义契约：**CD det 退化必须视为求解失败（success=0），禁止
  以 CRPIX 坍缩值冒充解**——本语义由 P1-WCS-IMPL 落地、P1-WCS-TEST 按
  TEST-WCS-DESIGN-001 F4 验收。
- DISP-WCS-002 wcs_transform 错误通道缺失族：tanWorldToIntermediate
  :148-170 cosc<1e-12 → xi=eta=1e6 哨兵（:154-155）无标志位；skyToPixel
  缺 AP/BP 时 3 迭代牛顿近似（:221-240）无收敛判据——均静默返回可疑值。
- DISP-WCS-003 双 SIP 拟合路径并存：生产 extract_wcs_sip（TRANS 解析 A/B
  + 7×7 网格反变换 AP/BP，ipv_wcs.cpp:322-478）与 legacy fit_sip IRLS+Huber
  （ipv_sip.cpp:268；irls_huber_fit :161-263，δ=1.345·MAD :241）仅被 legacy
  build_wcs（ipv_wcs.cpp:157-165，AP/BP 显式清零）消费；两实现行为漂移，
  去留归 P1-WCS-IMPL。
- DISP-WCS-004 AP/BP 拟合奇异半静默：ipv_wcs.cpp:477 仅 logger warn（写
  日志文件），IpvWcsResult 无拟合失败标志位——ap_order=0 无法区分"线性
  解"与"网格拟合失败"；cd_inv det<1e-15 跳过 SIP（:322-325）同理。
- DISP-WCS-005 取消检查点缺失 + OpenMP 未接 ThreadBudget：ipv_triangle
  .cpp:302/:347、ipv_select.cpp:810/:1123/:1412/:1756 等 #pragma omp 无
  num_threads 注入；长帧求解不可中断。threading_model=host_executor_lease
  为合同值，接线归 P1-WCS-IMPL。
- DISP-WCS-006 三套 TAN 实现并存：ipv（生产）、wcs_tan（lib/phase1/wcs，
  仅 tests/unit/p1_wcs_phot_test.cpp 消费）、wcs_transform（P1-PHOT 域）
  ——像素中心契约不一致（§11.2 双契约），维护歧义，去留归
  P1-WCS-IMPL/P1-PHOT-IMPL。

### 11.4 TEST-WCS-DESIGN-001 冻结测试设计（可执行 TEST-P1-WCS-001 由 P1-WCS-TEST 落地）

- F1 合成线性场（order=1，已知 CD/CRVAL/CRPIX 合成星表）：求解成功且
  n_pairs≥12；rms_arcsec ≤0.5″（实测锚 Galaxy_Center=0.1431″，memory.md
  2026-07-12）；CD 元素相对误差 ≤2%（§9 尺度容差 0.002 同源）；|ΔCRVAL|≤1″。
- F2 SIP 场 oracle（注入已知 A/B，order=2）：astropy WCS（隔离 test-only
  oracle，§5 规则）前向/逆向 |Δ|≤1e-4 px 于中心 90% 区域（承接 §8 预冻结
  值，不放宽）；AP/BP 逆向一致性 roundtrip 同容差。
- F3 CRPIX/Y-down 不变量：CRPIX=(w/2+0.5, h/2+0.5) 精确断言（F1）；Y 翻转
  后 CD 第 2 列符号翻转与 F5 公式一致；ASTROMETRY.md §7 CRPIX 不变量不破。
- F4 失败语义负例：0 星/<3 星 → ret=0 或 success=0 且 error_msg 非空，进程
  不崩溃；指向偏差 >FOV → 显式失败；**CD det 退化注入（DISP-WCS-001）→
  success=0 禁止坍缩值冒充解**；DLL 缺失 → orchestrator 非零退出码
  （orchestrator.cpp:1763）。
- F5 确定性：同输入同线程数 3 次运行 IpvWcsResult bitwise 一致；线程
  1/2/4 下 bitwise 一致（投票归并为整数求和 ipv_triangle.cpp:347-357、
  拟合单线程，无跨线程浮点重结合——§5c 禁令；若实测违背，P1-WCS-TEST
  如实登记不得放宽语义）。
- F6 legacy 桥回归：WcsTan pix2sky/sky2pix roundtrip <1e-6 deg
  （tests/unit/p1_wcs_phot_test.cpp:50 冻结值）；lib/phase1/wcs 源码零改动
  断言（矩阵 C7 语义）。
- 回归锚：Galaxy_Center 实场 fixture（rms_arcsec=0.1431″ 基线）。容差
  冻结：上述数值在 TEST 落地时逐项写死，P1-WCS-TEST 不得放宽；fixture
  生成器注记容差来源（本节）。

### 11.5 SCI-WCS-001 状态声明

科学专项（matrix P1-WCS 行）映射：plate solving TAN+SIP=本 ALG F1-F5 公式
与 §11.1 生产通道；ICRS/J2000=RADESYS=ICRS/EQUINOX=2000 写回
（orchestrator.cpp:2007，ASTROMETRY.md §3a）；degenerate conditions=
ASTROMETRY §8 ↔ DISP-WCS-001 退化语义（坍缩禁冒充解）；astropy oracle=
ASTROMETRY §11 ↔ F2。共享 SCI（ASTROMETRY.md SCI-WCS-001，FROZEN T102
2026-08-23）不因本附录改动；本节禁止被编排层词汇反向改写（descriptor
astrocs.phase1.wcs-platesolve 占位 ID SCI-P1-WCS-001/ALG-002/DATA-P1-WCS/
API-P1-004/TEST-P1-WCS-001，module_adapters.cpp:450-464，由 P1-WCS-INT
对齐本合同，不作冻结依据）。
