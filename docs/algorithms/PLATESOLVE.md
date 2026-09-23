# WCS / PlateSolve Algorithms (ALG-WCS)

> 上游：ASTROCS_DESIGN.md §4.2（Phase1 节点流程）

> ID: ALG-WCS-001  范围: ALG-WCS-001..002  上游 SCI: SCI-WCS-001  状态: DERIVED  模块: plate_solve/cpp/ipv

## 1 上游 SCI 与输入输出

- 上游: `SCI-WCS-001` (CRPIX=w/2+0.5, CD deg/pixel, cd_inv pixel/arcsec, SIP A/B 解析 / AP/BP 采样网格 ≥7×7（实现 AP/BP 41×41 阶 5；APx/BPx 81×81 阶 7，DISP-WCS-008）, Y-down)
- 输入: 星点表 `(x,y)` + Gaia 参考星表 (RA/Dec)
- 输出: `WCS` (CD+CRPIX/CRVAL+SIP A/B/AP/BP) 或 `NO_SOLUTION`

## 2 离散公式

```text
F1: CRPIX = w/2+0.5, h/2+0.5 (1-based), 0-based x0=CRPIX−1
F2: CD = trans.linear/3600 (deg/pixel), cd_inv = inv(trans.linear) (pixel/arcsec)
F3: SIP前向 A[i][j]=cd_inv·trans.x_ij, B[i][j]=cd_inv·trans.y_ij (解析)
F4: SIP逆向 AP/BP = argmin ||UV − [ (u,v) + SIP_A/B(u,v) ]||² on 采样网格 ≥7×7, AP[1,0]-=1, BP[0,1]-=1
    # 与 SCI-WCS-001 §5 同式同括号; SIP 自变量为像素偏移(px); 采样网格实现值
    # AP/BP 41×41(阶 5)/APx/BPx 81×81(阶 7) — DISP-WCS-008
F5: Y-down: cd12,cd22 取反; A'=A·(-1)^j, B'=−B·(-1)^j, AP/BP 同规则
F6: 投影 TAN + SIP畸变 + J2000, 极区 Lipschitz C=π/2 / C45=π/(2√2) conservative prune
```

来源: `ipv_wcs.cpp:153-576` `ipv_select.cpp:723` `gaia_client.c:polar_plane_intersects`

## 3 伪代码

```text
function solve_wcs(detections, gaia):
  if n_detections < min_stars → NO_SOLUTION
  triangles = build_triangles(detections) scale_tol=0.002
  matches = kd_match(triangles, gaia_triangles) tol=5.0"
  for each hypothesis:
    trans = iterative_reproject(matches) conv 0.01" max5 (ipv_solver.cpp)
    sip = build_sip(trans) order 2-3, IRLS 15× ε1e-6 Huber 1.345 (ipv_sip.cpp:238-261)
    wcs = compose(CRPIX,CRVAL,CD, sip, Y-down)
    rms = residual(wcs, matches)
  best = min rms, rank by n_matches + rms
  if rms > threshold → NO_SOLUTION else return wcs

function build_sip(trans):
  cd_inv = inv(trans.linear)
  for (i,j) in order: A[i][j]=cd_inv·trans.x_ij, B[i][j]=cd_inv·trans.y_ij
  grid = 采样网格 ≥7×7 (实现 41×41 阶 5 / 81×81 阶 7, DISP-WCS-008)
  UV = cd_inv·IWC, fit AP/BP via least squares
  AP[1,0]-=1; BP[0,1]-=1
  apply Y-down sign flips

Polar prune: if |dec|>45° use C/C45 disk B(q,C·radius), false_negative=0
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| `n < min_stars` | `NO_SOLUTION` |
| `det(trans.linear)==0` | reject SIP, `NO_SOLUTION` |
| 网格拟合奇异 | fallback linear |
| 极区跨界 `θ+radius>90°` | 保守不剪枝遍历 |
| RA环绕 `dra>180°` | `dra=360−dra` + cos(dec) 缩放 |
| 输入含 NaN | skip/fail per-field |

## 4a 图像侧选星有效域与极限星等迭代（选星样本 / 饱和 / 空星表）

> 适用实现：`lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp`（`select_image_stars`、
> `compute_fov_density`、`estimate_mag_lim_iterative` 与把迭代结果交付给求解器的封装）。
> 本节只规定**有效域与失效语义**，不改变 §2 的投影/拟合公式与 §9 的容差。

### 4a.1 图像侧选星样本的定义域

- **样本定义域 = 星等可靠的非饱和检测**。进入 `U` 向量组（三角形匹配几何）的图像侧样本，
  其每个成员**必须**满足 `saturated == false`；饱和检测**必须**被排除，**禁止**回填。
- **理由（两条，各自独立可测）**：
  1. 饱和像元的读出被饱和电平截断，box 积分通量既不等于真实通量、也不随真实亮度单调，
     故"按 box 积分星等升序取前 N 颗"在该定义域上不是任何一致亮度量的最亮 N 颗，
     样本的亮度深度不可复现；
  2. 饱和平顶/溢出让质心估计有偏，而该样本同时是三角形匹配的几何输入，偏质心直接进入匹配。
- **判据（可证伪）**：对任意检测表，选星输出中不存在 `saturated == true` 的下标；
  把饱和检测放回候选池（负向注入）时，同一判据必须判红。
- **排序与截取**：候选按 box 积分星等**升序**（越小越亮）排序；`mag` 非有限（NaN）者
  排在末尾且不被选中（候选充足时）；取前 `img_n_target` 颗。
- **候选不足的兜底**：非饱和候选少于 `img_n_target` 时，样本 = 全部非饱和候选
  （即 `|样本| = min(img_n_target, n_unsat)`），**不得**用饱和检测补足。
- **候选为空/过少**：非饱和候选数 < 2 时求解**必须** fail-closed，错误信息**必须**点名
  `n_detected / n_saturated / n_unsat`，**禁止**以饱和检测冒充样本继续求解。

### 4a.2 由样本导出的密度与目标星数

- `rho_img` 的分子**必须**是**实际进入 `U` 样本的成员数**（4a.1 的样本基数），
  分母**必须**是同一图像几何下的图像立体角；两者同域，**禁止**用"检测总数"或
  "含饱和样本数"作分子。
- **判据**：图像几何不变时，样本基数由 `N1` 变为 `N2`，`rho_img` **必须**按 `N2/N1`
  同比例变化。
- `n_target` 由 `rho_img`、查询锥面积与密度比导出，且**必须**落在闭区间 `[50, 60]`。
- `rho_img` 的分子与"极限星等迭代的查询目标星数"是**两件事**：前者恒为 `|U|`
  （§4a.1 的样本基数），后者按 §4a.5 由样本亮度深度基数导出。二者**不得**互相替代。

### 4a.3 极限星等迭代的单调性与步进方向

- 星表查询的星等窗是闭区间 `[-1.5, m_lim]`（`gaia_client_cone_search_for_solver` 的
  `mag_low` 固定为 `-1.5`），故 **`N(m_lim)` 关于 `m_lim` 单调不减**
  （每文件返回上限造成的顺序截断区间除外，该情形单列为 `capped`）。
- **判据**：同一查询锥内 `m2 > m1 ⇒ N(m2) ≥ N(m1)`（上限截断区间除外）。
- 由单调性：`N == 0` 时**唯一能增加星数的方向是更暗**（`m_lim` 增大）。
  规范**禁止**把"向更亮回退重试"作为空结果的补救——它只能取到子集，恒为 0。
  空结果的处置 = 按 `m_lim_zero_step` 向更暗步进并重试，直至查询次数上界。

### 4a.4 空结果与截断结果的失效语义（fail-closed，禁止静默采用）

- 迭代的每一次查询**必须**计入可观测 provenance：查询次数、扫描区间
  `[m_sweep_first, m_sweep_last]`、空结果次数 `n_zero_queries`、是否空扫描 `empty_sweep`、
  是否触顶 `capped`、是否收敛 `converged`；前四项**必须**随交付面
  （`StarSelection` 及其迭代结果结构）一并可读。
- **空扫描（`empty_sweep`）**：全部成功查询都返回 `N == 0`。该状态**必须**以显式错误终止求解，
  错误信息**必须**点名"整个扫描区间内星表返回 0 颗"并携带
  `(ra, dec, query_r, m_sweep_first, m_sweep_last, query_count)`，
  **不得**表述为"星等未收敛"或"星等调参未达容差"——空星表与星等选取是不同失效面。
- **截断样本（`capped`）**：`N` 触到星表每文件返回上限时，返回的星表按遍历序被截断，
  在空间/星等上不完整，属科学有偏样本。该样本**不得**被交付给求解器作参考星表：
  **必须**以显式错误终止并点名 `m_lim / N / 每文件上限`。
- **允许继续的唯一情形**：至少一次成功查询返回 `N > 0` 且未触顶；此时按 §2 流程继续，
  并把 `converged / capped / n_zero_queries / empty_sweep` 逐项落到交付面。

### 4a.5 图像侧样本 U 与星表侧样本 W 必须同亮度序位域

- **定义**：设检测表按 box 积分星等升序排序（饱和与正常统一排序，即 §4a.1 的排序口径），
  `U` 的第 `i` 个成员在该序中的位次为 `r_i`（1-based）。**样本亮度深度基数**
  `n_depth := max_i r_i`。
- **查询深度**：极限星等迭代的目标星数**必须** ≥ `n_depth`
  （实现取 `max(n_target, n_depth)`）。`U` 排除饱和检测后位次整体下移，
  若查询深度仍按 `n_target` 标定，`U` 的对应体**根本不进入**星表返回集，
  两侧样本在星等上互斥，三角形投票不存在真峰。
- **选取窗口**：`W` **必须**取星表 FOV 亮度序中与 `U` **同位次**的成员（第 `r_i` 个），
  **禁止**取"FOV 内最亮 `n_target` 颗"。位次是两侧样本唯一不依赖星等零点的同域判据；
  "最亮 N 颗"只在 `U` 恰为位次 `1..N` 时与位次对齐等价。
- **退化性（无回归）**：`n_sat = 0` 时 `r_i = i`、`n_depth = |U|`，
  位次对齐窗口与"取最亮 `n_target` 颗"**逐位一致**。
- **失效面（fail-closed）**：FOV 内星表成员数 < `n_depth` ⇒ **必须**显式错误终止并点名
  `(n_fov, n_depth, n_target)`，**禁止**静默退化为"最亮 N 颗"——那正是两侧样本亮度域互斥的成因。
- **判据（可证伪）**：构造合成星表使"第 `k` 亮的星表星"恰为"第 `k` 亮的检测星"的对应体时，
  `|U ∩ W| ≥ |U|/2` 必须成立；把选取窗口退回"最亮 `n_target` 颗"时该判据**必须判红**。

## 4b 对外可见错误串的编码（ALG-WCS-001 错误面）

- `IpvWcsResult.error_msg` 与内部 `WcsFitResult.error` 是**对外可见**的失败信息载体，
  其内容**必须**是**合法 UTF-8**（承接 `ENGINEERING_SPEC`「文件编码 UTF-8」与
  `docs/contracts/LOG_AND_ERROR_CONTRACT.md` §8「超限按 UTF-8 边界截断」的口径）。
- **写入定长缓冲的规则**：
  1. 截断**只**发生在 UTF-8 码点边界，**禁止**切断多字节序列；
  2. 非法字节（孤立续字节 / 非法首字节 / 过长编码 / 代理区 / 越界码点 / 被 NUL 截断的序列）
     **必须**替换为 ASCII `?` 后写入，**禁止**原样透传；
  3. 恒以 `'\0'` 结尾，写入字节数 ≤ 缓冲容量 − 1。
- **失败面**：`success == 0` 时 `error_msg` **必须**非空；错误串的内容**必须**点名失败面
  （参数/星表通道/选星样本/几何/拟合），**禁止**以通用文案掩盖具体失效面。
- **判据（可证伪）**：对任意失败输入，`error_msg` 非空且通过严格 UTF-8 校验（RFC 3629）；
  负例注入（含 GBK 字节的消息、跨容量边界的多字节序列）**必须**被判据判红。
- **编码边界声明**：本条只约束**求解器写出的字节**；上层控制台渲染若自行做 ASCII 化
  （非本模块行为），不改变本条的判定对象——判定对象恒为 `error_msg` 缓冲区内的字节。

## 5 确定性与归约

- 单线程求解，三角形匹配 KD-tree 确定性（排序 tie-break by frame_id）；SIP LS 按 grid 索引固定顺序；无跨假设归约。

## 6 时间/空间复杂度

- 匹配 O(n log n)；SIP O(order²·49) LS；空间 O(n_triangles)

## 7 CPU-only 后端策略（V5）

- 仅 CPU：求解为逐星/逐帧独立算术，可经 worker pool（按 affinity）帧级并行，**禁止硬编码线程数**；Gaia 查询由 gaia_xpsd_client 网络 IO 主导，无计算瓶颈。

## 5c SIMD 安全与取消点

- CD/SIP 矩阵算术逐元素独立；最小二乘(AP/BP，采样网格 ≥7×7，实现 41×41/81×81，DISP-WCS-008)为确定性顺序归约(样本序固定)——**禁止并行重结合**；FP64 全链路禁 fast-math。
- 取消点: 按帧(星表行块)粒度检查; 取消时丢弃半成品 trans 并返回错误码(语义随 API 冻结)。

## 8 参考实现/Oracle

- 合成投影图已知 WCS 恢复残差 `<0.1"`；astrometry.net 语义对照 (05 spec)；Astropy WCS 前向/逆向 `Δ<1e-4 px`。

## 9 容差来源

- 收敛 0.01" (pixel/3600)：`ipv_solver.cpp:199 CONV_THRESH_ARCSEC = 0.01`，判据
  `√(x00²+y00²) < 0.01″`（`ipv_solver.cpp:19`）。**量纲 arcsec**；
  **适用域**：该值只约束迭代重投影的**自洽收敛**，不是天测精度界——
  产品级天测精度门另立（G-P1-WCS-CLOSURE，阈值 UNJUSTIFIED）。
- 三角形匹配容差 5.0″：`ipv_solver.cpp:660` 实参。**量纲 arcsec**；
  **阈值来源 UNJUSTIFIED**（无推导、未随像素尺度归一），登记为待标定项，
  在标定前**不得**被引用为精度声明。
- 尺度容差 0.002（各向异性 0.2%）：**阈值来源 UNJUSTIFIED**，登记为待标定项。
- Huber 1.345：**有文献依据**——Huber (1964) `ψ_k` 族在 `k = 1.345` 处对
  Gaussian 的渐近效率为 **95%**（Huber, P. J. 1964, Ann. Math. Statist. 35, 73,
  DOI 10.1214/aoms/1177703732；效率表见 Holland & Welsch 1977, Comm. Statist.
  Theor. Meth. 6, 813, DOI 10.1080/03610927708827533 §2）。实现锚
  `ipv_sip.cpp:242`：`delta = 1.345 × median_abs_r`。
  **适用域**：`δ = 1.345·MAD(|r|)` 要求残差近似对称且尺度由 MAD 稳健估计；
  非对称/重尾残差下 95% 效率结论不成立。
- IRLS 迭代上限 15、收敛 ε 1e-6：`ipv_sip.cpp:408-409`（`IRLS_MAX_ITER`、
  `IRLS_CONV_EPS`）。**阈值来源 UNJUSTIFIED**；该路径经 DISP-WCS-003 登记为
  **非生产**（生产走 `extract_wcs_sip` 的采样网格解析路径），本条只描述
  非生产实现，**不得**作为生产精度依据。
- 预冻结声明不变：上述数值除 Huber 1.345 外均无推导，标记 UNJUSTIFIED 的部分
  在标定前不得引用。

## 10 关联 ARC/API/TST

- ARC: `THREADING_MODEL.md` 单线程阶段内串行
- API: `ipv_api.h: ipv_solve_from_detections_v1`, `ipv_wcs.h: build_wcs`
- TST: `TST-WCS-001` 合成恢复, `TST-WCS-INV` 极区保守, `TST-WCS-FAIL` 退化

## 11 实现锚定附录（SRC-WCS-001 源码实测）

### 11.1 ALG-WCS-001 逐符号锚

生产通道 = orchestrator PLATESOLVE 阶段直调 `ipv_solve_from_detections_v1`
（DLL=ipv_solver.dll，MinGW/MSYS2 g++ 构建 `build.ps1:27` / `Makefile:6`，
`-fopenmp -O3`）；消费 PSF 产出 star_measurements 权威块。行号均 grep 实测。

| 符号 | 锚 | 角色 |
|---|---|---|
| ipv_solve_create | ipv_entry.cpp:369（声明 ipv_api.h:113） | 句柄生命周期 |
| ipv_solve_destroy | ipv_entry.cpp:381（ipv_api.h:116） | 句柄释放 |
| ipv_set_gaia_handle | ipv_entry.cpp:392（ipv_api.h:119） | Gaia 句柄注入 |
| ipv_set_detector_handle | ipv_entry.cpp:405（ipv_api.h:122） | sdet 句柄注入 |
| ipv_get_default_params | ipv_entry.cpp:418（ipv_api.h:227） | IpvParams 默认值（log_dir 空=无日志） |
| ipv_get_last_inlier_count | ipv_entry.cpp:459（ipv_api.h:253） | inlier 计数查询 |
| ipv_get_last_inliers | ipv_entry.cpp:473（ipv_api.h:261） | inlier 9 列缓冲（ipv_api.h:232-250：det_x/det_y/gaia_ra/gaia_dec/pred_x/pred_y/residual_x/residual_y/residual_dist） |
| ipv_solve | ipv_entry.cpp:490（ipv_api.h:126） | 文件路径入口（非生产） |
| ipv_solve_from_memory | ipv_entry.cpp:525（ipv_api.h:139） | PipelineFrame 内存入口 |
| **ipv_solve_from_detections_v1** | ipv_entry.cpp:675（ipv_api.h:175） | **生产入口**（检测坐标 double 数组直入） |
| ipv_solve_from_memory_with_callback | ipv_entry.cpp:720（ipv_api.h:194） | 回调进度变体 |
| ipv_solve_from_memory_with_callback_d | ipv_entry.cpp:767（ipv_api.h:211） | 回调变体 FP64 |
| do_solve_from_detections_v1_impl | ipv_entry.cpp:581 | 参数装配 → IPVSolver::solve_from_memory；try/catch → set_error_msg（:188，:251-262/:288-299） |
| IPVSolver::solve_from_memory | ipv_solver.cpp:781 | 主求解流程（入口日志 :794） |
| 选星 + U 构建 | ipv_select.cpp:787-837（`select_image_stars`：非饱和候选按 mag(box积分) 升序取前 img_n_target，§4a.1）、:840-852（样本不足报错，点名 n_detected/n_saturated/n_unsat） | U=(det_x−cx, −(det_y−cy)) 像素、Y-up、原点图像中心；s0=206.265·pixel_um/focal_mm（:57,:282） |
| 密度/目标星数 | ipv_select.cpp:260-330（`compute_fov_density`，rho_img 分子 = 实际样本基数，§4a.2） | n_target = min(60, max(50, round(ρ_target·query_area/img_area))) |
| 极限星等迭代与交付 | ipv_select.cpp:374-556（`estimate_mag_lim_iterative`，§4a.3/§4a.4 空扫描 provenance）、:600-664（`gaia_query_mag_iterative`：空扫描/触顶样本 fail-closed） | 空结果向更暗步进；全空扫描 empty_sweep；触顶样本不得作参考星表 |
| 错误串编码归一 | ipv_entry.cpp:320-370（`utf8_safe_copy`，§4b）、:186-193（`set_error_msg`）、:179-185（`to_c_result`） | error_msg 恒为合法 UTF-8：码点边界截断 + 非法字节替换为 '?' |
| 三角形投票 | ipv_triangle.cpp:296-357 | 线程局部投票矩阵（:296-300）+ omp for schedule(dynamic,64)（:309-311）+ 整数归并 collapse(2) schedule(static)（:347-357） |
| iter_trans_solve | ipv_itertrans.cpp:974 | 迭代重投影多项式拟合（order 1→3） |
| robust_refine_wcs | 调用点 ipv_solver.cpp:692-712；irls_fit_one_step ipv_robust_refine.cpp:661 | 稳健扩增精化（CD 阻尼 + Tukey biweight），失败回退不破坏主解 |
| extract_wcs_sip | ipv_wcs.cpp:229 | WCS+SIP 提取（生产路径） |
| CD = trans 线性项/3600 | ipv_wcs.cpp:256-266 | 度/像素（F2） |
| CRVAL/CRPIX 冻结 | ipv_wcs.cpp:264-277 | CRPIX=w/2+0.5, h/2+0.5（1-based，F1） |
| ctype 选择 | ipv_wcs.cpp:283-290 | order≤1 → RA---TAN/DEC--TAN；否则 -SIP 后缀 |
| SIP A/B 解析 | ipv_wcs.cpp:322-365 | cd_inv=inv(trans 线性项)（det<1e-15 warn :322-325）；A[i*6+j]=cd_inv·trans.x_ij（F3） |
| SIP AP/BP 网格反变换 | ipv_wcs.cpp:395-527 | 采样网格 ≥7×7（实现 AP/BP 41×41 阶 5；APx/BPx 81×81 阶 7，DISP-WCS-008）最小二乘；AP[6]−=1、BP[1]−=1（:505-509，F4）；奇异仅 warn（:521-523） |
| RMS 统计 | ipv_wcs.cpp:484-516 | rms_arcsec=√(Σr²/n)；rms_px=rms_arcsec/s0 |
| Y-down 输出转换 | ipv_wcs.cpp:528-576 | cd12/cd22 取反（:542-544）；A/B/AP/BP 符号规则（:546-571，F5） |
| inlier 缓存 | ipv_solver.cpp:756-764 | cache_last_inliers_（WCS Gate v2 双层闭环） |
| orchestrator 过滤+坐标契约 | orchestrator.cpp:1855-1876 | star_measurements [N,≥15] FLOAT64；status∈{0,3}、sat r[13]、fwhm r[7]∈[0.5,20]、边缘 5px；**+0.5 转换 :1867**（统一契约 index-is-center → IPV 接口契约 center=index+0.5）；sdet fallback 坐标已是 +0.5 契约（:1878） |
| orchestrator 调用与写回 | orchestrator.cpp:1967-2049 | 求解调用 :1967；失败 → PLATESOLVE_FAILED :1980；CTYPE/CRVAL/CRPIX/CD + RADESYS=ICRS/EQUINOX=2000 写回 :2003-2010；SIP A/B/AP/BP 写回 :2017-2049 |

### 11.2 返回码/失败语义（含像素中心契约）

- C ABI 层：ipv_solve_from_detections_v1 返回 0=失败/1=成功；result->success
  0/1；error_msg[256] 由 set_error_msg（ipv_entry.cpp:141）在 NULL 参数、
  C++ 异常路径填充（:181-187/:218-224）；success=1 时 IpvWcsResult POD
  （ipv_api.h:39-61：cd[4]/crval[2]/crpix[2](1-based)/sip_a·b·ap·bp[36]/
  rms_px/rms_arcsec/n_pairs/trans_order/ctype[2]）为唯一权威输出。
- 求解器层：三角形匹配 0 匹配或 iter_trans_solve 全阶失败
  （ipv_solver.cpp:901-941）→ fail_result（trans_order=0, success=false）
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

### 11.3 现状缺陷清单（DISP-WCS-001..008，登记不改码，整改归 P1-WCS-IMPL/INT）

- DISP-WCS-001 CD/线性变换退化静默坍缩（R1 登记项，失败-置信度语义核心）：
  lib/algorithms/photometry/cpp/src/wcs_transform.cpp:39-49 构造时
  det<1e-15 → cdInv 全零 + 仅 stderr 警告，pixelToSky/skyToPixel 输出坍缩
  到 CRPIX−1 附近，无错误码无标志位；同族 lib/algorithms/platesolve/wrapper_phase1/wcs_tan.cpp:48-51
  det<1e-30 → 直接返回 CRPIX 且零日志。调用方无法区分"真解≈CRPIX"与
  "退化坍缩=CRPIX"（wcs_transform 输出恒 CRPIX−1，0-based，更不可判）。
  失败-置信度语义契约：**CD det 退化必须视为求解失败（success=0），禁止
  以 CRPIX 坍缩值冒充解**——本语义由 P1-WCS-IMPL 落地、P1-WCS-TEST 按
  TEST-WCS-DESIGN-001 F4 验收。
- DISP-WCS-002 wcs_transform 错误通道缺失族：tanWorldToIntermediate
  :148-170 cosc<1e-12 → xi=eta=1e6 哨兵（:154-155）无标志位；skyToPixel
  缺 AP/BP 时 3 迭代牛顿近似（:221-240）无收敛判据——均静默返回可疑值。
- DISP-WCS-003 双 SIP 拟合路径并存：生产 extract_wcs_sip（TRANS 解析 A/B
  + 采样网格 ≥7×7（实现 AP/BP 41×41 阶 5；APx/BPx 81×81 阶 7，DISP-WCS-008）反变换 AP/BP，ipv_wcs.cpp:322-478）与非生产 fit_sip IRLS+Huber
  （ipv_sip.cpp:268；irls_huber_fit :161-263，δ=1.345·MAD :241），后者仅被非生产
  build_wcs（ipv_wcs.cpp:157-165，AP/BP 显式清零）消费；两实现行为漂移，
  去留归 P1-WCS-IMPL。
- DISP-WCS-004 AP/BP 拟合奇异半静默：ipv_wcs.cpp:477 仅 logger warn（写
  日志文件），IpvWcsResult 无拟合失败标志位——ap_order=0 无法区分"线性
  解"与"网格拟合失败"；cd_inv det<1e-15 跳过 SIP（:322-325）同理。
- DISP-WCS-005 取消检查点缺失 + OpenMP 未接 ThreadBudget：ipv_triangle
  .cpp:302/:347、ipv_select.cpp:839/:1123/:1412/:1756 等 #pragma omp 无
  num_threads 注入；长帧求解不可中断。threading_model=host_executor_lease
  为合同值，接线归 P1-WCS-IMPL。
- DISP-WCS-006 三套 TAN 实现并存：ipv（生产）、wcs_tan（lib/algorithms/platesolve/wrapper_phase1，
  仅 eng/tests/unit/p1_wcs_phot_test.cpp 消费）、wcs_transform（P1-PHOT 域）
  ——像素中心契约不一致（§11.2 双契约），维护歧义，去留归
  P1-WCS-IMPL/P1-PHOT-IMPL。
- DISP-WCS-008 SIP 逆映射网格/阶扩展 + 迭代反演：生产 `ipv_wcs.cpp` AP/BP 用
  41×41 网格（`NB_GRID=41`）、APx/BPx 用 81×81（`NB_GRID_X=81`，阶 7）；SCI
  §7/§11 的现行口径为「采样网格 ≥7×7 + 独立
  密集域不变量 ≤1e-4 px」；本机 `p1wcs_tests apbp` low/mid/high 三档独立域
  roundtrip max 3.299e-10/1.518e-9/1.655e-9 px 全过（R-3 §3.6）⇒ 「1e-4 px
  数学不可达」对当前实现为假。维护歧义已消除；本清单 DISP-WCS-006 为
  「三套 TAN 实现并存」，保持有效。

### 11.4 TEST-WCS-DESIGN-001 冻结测试设计（可执行 TEST-P1-WCS-001 由 P1-WCS-TEST 落地）

- F1 合成线性场（order=1，已知 CD/CRVAL/CRPIX 合成星表）：求解成功且
  n_pairs≥12；rms_arcsec ≤0.5″；CD 元素相对误差 ≤2%（§9 尺度容差 0.002 同源）；
  |ΔCRVAL|≤1″。F1 不以任何单一实场实测值为标定依据；产品级外部闭环口径已冻结在
  SCI-WCS-001 §11a 与 GATES_AND_TOLERANCES §3（G-P1-WCS-CLOSURE / -REPRO）。
  **量测域冻结**：本项 `rms_arcsec` 定义在 `trans` 拟合的**内点集**（`n_pairs≥12`）
  与**合成线性场**（order=1，已知 CD/CRVAL/CRPIX 合成星表）上；它**不是**产品级
  天测精度门。产品级外部闭环量（全帧头域 median/p95）另立证据面
  **G-P1-WCS-CLOSURE**，其阈值需另行标定；该门与 F1 量测域不同，两者**不得
  互为证据**。门表见 `docs/algorithms/GATES_AND_TOLERANCES.md`。
- F2 SIP 场 oracle（注入已知 A/B，order=2）：astropy WCS（隔离 test-only
  oracle，§5 规则）前向/逆向 |Δ|≤1e-4 px 于中心 90% 区域（承接 §8 预冻结
  值，不放宽）；AP/BP 逆向一致性 roundtrip 同容差。
- F3 CRPIX/Y-down 不变量：CRPIX=(w/2+0.5, h/2+0.5) 精确断言（F1）；Y 翻转
  后 CD 第 2 列符号翻转与 F5 公式一致；ASTROMETRY.md §7 CRPIX 不变量不破。
- F4 失败语义负例：0 星/<3 星 → ret=0 或 success=0 且 error_msg 非空，进程
  不崩溃；指向偏差 >FOV → 显式失败；**CD det 退化注入（DISP-WCS-001）→
  success=0 禁止坍缩值冒充解**；DLL 缺失 → orchestrator 非零退出码
  （orchestrator.cpp:1764）。
- F5 确定性：同输入同线程数 3 次运行 IpvWcsResult bitwise 一致；线程
  1/2/4 下 bitwise 一致（投票归并为整数求和 ipv_triangle.cpp:347-357、
  拟合单线程，无跨线程浮点重结合——§5c 禁令；若实测违背，P1-WCS-TEST
  如实登记不得放宽语义）。
- F6 WcsTan 桥回归：WcsTan pix2sky/sky2pix roundtrip <1e-6 deg
  （eng/tests/unit/p1_wcs_phot_test.cpp:50 冻结值）；另有**独立前向交叉绝对门**：pix2sky 输出与
  独立 TAN 逆投影参考解（module_adapters.cpp 内 p1_tan_forward_reference，
  与 WcsTan 的 atan(R)/asin 式不同源）角距 **≤1e-9 deg**；测试锚
  eng/tests/unit/p1wcs negative `n1_wcs_tan_unit_anchor` 与 units
  `u1_f6_abs_cross`，生产门同在 p1_op_wcs。roundtrip 仅作次级不变量
  （对 ξ/η 成对单位错零鉴别力，见 AUD-COORD F-01/F-06）。
- 回归锚：Galaxy_Center 实场 fixture。当前版本实测 T4 Galaxy_Center panel1 Red
  `rms_arcsec=0.2803–0.3588″`（六帧）；该值须在 UNIT-001（XISF 母版单位）落地后
  整体复跑，在此之前 P1-WCS-TEST 不得把它写死为冻结数值。容差冻结：其余数值
  在 TEST 落地时逐项写死，不得放宽；fixture 生成器注记容差来源（本节）。

### 11.5 SCI-WCS-001 状态声明

科学专项（matrix P1-WCS 行）映射：plate solving TAN+SIP=本 ALG F1-F5 公式
与 §11.1 生产通道；ICRS/J2000=RADESYS=ICRS/EQUINOX=2000 写回
（orchestrator.cpp:2007，ASTROMETRY.md §3a）；degenerate conditions=
ASTROMETRY §8 ↔ DISP-WCS-001 退化语义（坍缩禁冒充解）；astropy oracle=
ASTROMETRY §11 ↔ F2。共享 SCI（ASTROMETRY.md SCI-WCS-001，FROZEN）
不因本附录改动；本节禁止被编排层词汇反向改写（descriptor
astrocs.phase1.wcs-platesolve 占位 ID SCI-P1-WCS-001/ALG-002/DATA-P1-WCS/
API-P1-004/TEST-P1-WCS-001，module_adapters.cpp:517-529，由 P1-WCS-INT
对齐本合同，不作冻结依据）。

## 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动本文件任何公式、锚点、阈值与容差；原有条款全部保留。

- WCS 框架/TAN/SIP：Paper I §2.1.1；Paper II §2.1/§2.2/Table 1；Shupe et al. 2005, ASPC 347, 491（SIP）。
- 可执行标准：WCSLIB（LGPL-3.0）、astropy.wcs（BSD-3-Clause）≥7.0.1。
- 三角匹配/星表求解：Groth 1986, AJ 91, 1244（DOI 10.1086/114121）；Valdes et al. 1995, PASP 107, 1119（DOI 10.1086/133670）。
- 多帧联合校准：SCAMP（GPL-3.0；Bertin 2006, ASPC 351, 112）。
- Astrometry.net 语义对照（本文件 §8）：Astrometry.net（https://astrometry.net，许可证需网络核验）。
- Huber IRLS（SIP 拟合）：Huber 1964, Ann. Math. Statist. 35, 73。

参考代码库（含许可证；GPL 代码仅作行为/数值对照，不复制进本仓）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）；astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）；ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）；reproject（BSD-3-Clause，https://github.com/astropy/reproject）。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- SExtractor / PSFEx / SWarp / SCAMP（GPL-3.0，https://github.com/astromatic/）。
- healpy（GPL-2.0，https://github.com/healpy/healpy）；Siril（GPL-3.0，https://gitlab.com/free-astro/siril）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）；GSL（GPL-3.0，https://www.gnu.org/software/gsl/）。
- WCSLIB（LGPL-3.0）；CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- NumPy / SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

