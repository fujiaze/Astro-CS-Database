# Phase2 UPM Fit/Apply Algorithms（P2-UPM / astrocs.p2.upm）

> ID: ALG-P2-UPM-IMPL-001  状态: CONTRACT_READY（P2-UPM-DOC 冻结，2026-09-10，
> owner SA-P2-U21）。本文件是 Phase2 Unified Photometric Model（UPM）
> fit+apply+persist+reload 的**实现级算法合同**：逐符号源码行号锚定 +
> 冻结容差 + 现状缺陷登记。科学语义权威=SCI-UPM-001（docs/science/
> PHASE2_UPM.md，FROZEN 集合，零改动）；推导级算法权威=ALG-UPM-001
> （docs/algorithms/UPM_SOLVER.md，本批原位修订，公式与容差零改动）。
> 模块: lib/phase2/src/upm.cpp（1565 行）+ 唯一权威签名头
> lib/phase2/include/astro/phase2/upm.h（184 行，实测 2026-09-10）；
> API: API-P2-UPM-001（矩阵词汇；PUBLIC_API.md 尚未落页，见 §16）；
> DATA: DATA-P2-UPM / DATA-P2-COR；MOD: astrocs.p2.upm
> （TRACEABILITY_MATRIX.csv :21/:22 两行）；TEST: TEST-P2-UPM-001/002
> （设计冻结面=本文档 §13，可执行 MISSING 归 P2-UPM-TEST）。

## 1 目的与非目标

**目的**（SCI-UPM-001 §1 承接；实现域=astrocs.p2.upm 全链）：

- **fit**：从控制观测流（P2ControlObservation[]，上游
  ALG-P2-SMP-001 产出）联合求解 ONE UnifiedPhotometricModel——
  M_k latent reference + C_i(p) 每帧空间校正场，Huber IRLS 坐标
  下降 + 图平滑 + 弱零锚 + 分量 gauge（upm.cpp:493-873）；
- **apply**：calibrated = raw − C(frame, leaf)，运行时唯一入口
  p2_upm_calibrate_block（upm.h:12-13 冻结"不暴露 per-frame
  gradient 产品；运行时只经 p2_upm_calibrate_block 使用"）；
- **persist**：稀疏 json（astrocs-upm-v2）+ 稠密缓存
  （astrocs-upm-dense-v2）双形态落盘，frame 绑定显式持久化；
- **reload**：p2_upm_open 强校验重开，save→open 绑定不变
  （upm.cpp:1008-1231；ALG-UPM-FRAME-BIND-001 / DATA-UPM-MODEL-001）。

**非目标（本模块不做）**：

- 不做控制点采样（ALG-P2-SMP-001 域，obs 输入来自该域）；
- 不做每像素候选排异（ALG-P2-REJ-001 域）；
- 不做样本合并/积分（ALG-P2-INT-001 域）；
- 不产 per-frame gradient 产品（upm.h:12-13 冻结，禁止暴露
  per-frame 梯度面；C 场只经 calibrate_block/evaluate_c 消费）；
- 不解释 control_variance 科学定义（sampler 域
  ALG-UPM-CONTROL-IVAR-001 承接，本域只消费 control_ivar）。

## 2 符号与单位（权威=本表 + SCI-UPM-001 §3）

| 符号 | 含义 | 单位/域 | 实现锚 |
|---|---|---|---|
| y_ik（value） | 控制观测（patch 位置估计，可负） | ADU | upm.h:37 |
| uncertainty | control estimator 标准误 = sqrt(control_variance) | ADU | upm.h:38 |
| control_variance | k_corr·(π/2)·σ_bg²/N_retained（sampler 域产出） | ADU² | upm.h:43-50 |
| control_ivar | 1/control_variance | ADU⁻² | upm.h:50 |
| ivar（弃用字段） | 单像素 Phase1 ivar，仅诊断，禁入科学权重 | — | upm.h:40-42 |
| M_k | latent unified reference | ADU | upm.cpp:57/:875 |
| C_i(p) | 每帧空间校正场（centered 双线性 8×8） | ADU | upm.cpp:112-175 |
| raw / calibrated | 校准前/后信号 | ADU | upm.cpp:1266 |
| θ（每帧系数） | C 稀疏矩阵行 = frame 系数 | ADU | upm.cpp:993-1002 |
| grid=G | 每 tile cell 网格边长 | 无量纲（=8） | upm.cpp:259/:1062 |
| cell_side | tile 边长/G=512/8=64 | leaf px | upm.cpp:260/:1063 |
| tile_shift | leaf order = target_order+9 → tile 移位 | 无量纲（=9） | upm.cpp:261/:1254/:1281/:1405 |
| leaf_ipix | NESTED leaf 像素号（cell 中心） | 无量纲 u64 | upm.h:34 |
| frame_id | 内容稳定帧身份（sampler 域 p2_frame_id 产出） | 无量纲 u64 | upm.h:32 |
| w_UPM | IRLS 最终权重 = raw_norm × huber_w | ADU⁻² | upm.cpp:629/:647 |
| quality/support | 观测质量因子 / 覆盖支持度 | 无量纲 [0,1] | upm.cpp:177-186 |
| dtype | 全链 FP64（P2ModelInfo.precision=1 fp64 reference） | — | upm.h:62、upm.cpp:256/:1402 |

禁止单位漂移：C/M/raw/calibrated/σ_bg=ADU、control_variance=ADU²、
control_ivar=ADU⁻²、quality/support 无量纲、frame_id 无量纲 uint64
（SCI-UPM-001 §3 冻结面，2026-09-10 实测复核）。

## 3 逐符号锚（upm.cpp 1565 行 / upm.h 184 行，2026-09-10 实测）

**导出符号（upm.h 声明 / upm.cpp 实现）**：

| 符号 | 声明（upm.h） | 实现（upm.cpp） | 语义 |
|---|---|---|---|
| p2_upm_build | :95-97 | :929-932 | 构建（obs 驱动拓扑）入口 |
| p2_upm_build_geo | :103-106 | :934-938 | 全几何节点构建（含单帧区 continuation） |
| p2_upm_save | :108 | :940-1006 | 稀疏 json 原子写（aio_upm_write_sparse） |
| p2_upm_open | :109 | :1008-1231 | 强校验重开（format/frames/controls/C） |
| p2_upm_info | :110 | :1233-1238 | P2ModelInfo 快照（hash/control_count） |
| p2_upm_calibrate_block | :113-119 | :1240-1269 | 逐块校准（唯一运行时 apply 面） |
| p2_upm_evaluate_c | :121-123 | :1271-1289 | 直接求值 C(frame,leaf) |
| p2_upm_raw_weight | :134-136 | :1292-1323 | raw 权重单一实现（production/ablation） |
| p2_upm_normalized_weights | :139-142 | :1325-1344 | per-control 归一化（API 面） |
| p2_upm_geometry_hash | :146 | :1346-1367 | 几何/拓扑 hash（不含观测可信度） |
| p2_upm_component_gauges | :150-152 | :1369-1381 | 每分量 gauge frame id 查询 |
| p2_upm_materialize_dense_n | :177-178 | :1390-1515 | 稠密缓存物化（worker 数显式） |
| p2_upm_materialize_dense | :154-156/:173-175 | :1517-1521 | 稠密物化 wrap（workers=0 auto；声明重复见 DISP-P2UPM-001） |
| p2_upm_dense_info | :159-162 | :1523-1540 | dense 信息（等价门用） |
| p2_upm_dense_read_block | :166-172 | :1542-1557 | dense 块读（stale 拒绝） |
| p2_upm_close | :180 | :1559-1563 | 释放 |

**内部符号（匿名 namespace）**：

| 符号/段 | 锚（upm.cpp） | 语义 |
|---|---|---|
| build_impl | :212-927 | 构建/求解/哈希本体（两入口共用） |
| evaluate_c_field | :112-175 | centered 双线性求值（sparse/dense 共用语义） |
| quality_factor | :177-186 | quality_flags 映射（16→0 / 2→0.1 / 1→1.0 / 未知→0.5） |
| huber_rho / huber_w | :196-206 | Huber loss / 权重核（无量纲 z） |
| compute_raw（lambda） | :509-561 | raw 计算 + per-control 归一化 + 并行归并 |
| cg_solve_frame（lambda） | :563-599 | 逐帧 CG（(W+λsL+λ0I)x=rhs） |
| IRLS 主循环 | :601-873 | 每轮 raw→w→M→C→objective/收敛 |
| Model/ControlNode | :55-97 | 状态载体（component_ref_frame :85、grid :88、cell_side :89） |
| kNoData（sentinel） | :217 | 无观测几何节点标记 = SIZE_MAX |
| 头部冻结注释 | :1-27 | 语义冻结面（权重/ivar 状态机/gauge/持久化绑定） |

## 4 伪代码（build 与 apply 主流程）

```text
function build_impl(obs, n_obs, nodes, n_nodes, cfg):            # upm.cpp:212-927
  校验: out_model/obs/n_obs==0 → rc=1 (:215); target_order<0 → rc=1 (:246-249)
  cfg 缺省修补（:218-245，默认值见 §13）
  装配: nodes 全几何 + obs 按 (tile,gx,gy) cell 去重 (:266-314)
  frame_ids(std::set 升序) → frame_index/frame_id_by_index (:265/:340-346)
  邻接图: 网格 4-邻接 + 跨 tile 几何邻接(角距<1.6×cell_dist) (:351-413)
  连通分量: frame-control 二分图 DFS; 无 obs 几何节点=sentinel (:415-491)
  每分量 gauge: ref_frame = 分量内最小 frame_id (:464-469)
  for iter in 0..max_iterations-1:                               # :601-873
    raw = p2_upm_raw_weight 逐 obs; 归一化 raw/Σraw×reliability  # :509-561
    w[i] = raw_norm[i] × huber_w(z_i); z = r/σeff                # :612-650
    M[k] = Σ w·(y−C)/Σ w（参考帧观测优先，未覆盖延拓全帧）        # :651-764
    C[f] = CG( W+λsL+λ0I , rhs ); 参考帧 C=0                     # :765-858
    objective += raw·huber_rho(z); max_dM/tol && max_dC/tol → break  # :859-872
  model_hash = SHA-256(max_digits10 序列化: cfg+manifest+frames+controls+C)  # :879-921

function calibrate_block(model, frame_id, leaves, in, out, n):   # :1240-1269
  未知 frame_id → rc=1 显式失败禁回退 (:1249-1252)
  逐 leaf: tile=leaf>>18; local=leaf&mask; nested_local_to_xy    # :1256-1262
  out[i] = in[i] − evaluate_c_field(model, fi, tile, x, y)       # :1263-1266
```

## 5 上游 SCI 与映射声明（本任务零 SCI 改动）

- 语义权威已有 FROZEN SCI：**SCI-UPM-001**（docs/science/PHASE2_UPM.md；
  冻结集合 = SCI-UPM-001..010 + SCI-UPM-WEIGHT-001 +
  SCI-UPM-PERSIST-001）。**不因本任务改动**（共享 SCI 引用不改动；
  P2-SAMP-DOC/P2-REJ-DOC 先例同构）。单位面（§2）直接承接
  SCI-UPM-001 §3 实测文本。
- **descriptor 占位映射声明**（仿 PHASE2_SAMPLER.md §11.4/§12 写法；
  占位 ID 是矩阵/descriptor 词汇，不注册 INDEX、不入合同）：
  - `SCI-P2-UPM-001`（fit descriptor sci_id，lib/core/src/
    module_adapters.cpp:676）⇒ **SCI-UPM-001**；
  - `SCI-P2-UPM-002`（apply descriptor sci_id，module_adapters.cpp:696）
    ⇒ **SCI-UPM-001**；
  - `ALG-P2-UPM-001`（fit 行 algorithm_id，TRACEABILITY_MATRIX.csv:22）
    ⇒ **ALG-UPM-001**（docs/algorithms/UPM_SOLVER.md，推导权威）+
    **ALG-P2-UPM-IMPL-001**（本文件，实现级合同）；
  - `ALG-P2-UPM-002`（apply 行，TRACEABILITY_MATRIX.csv:21）⇒
    **ALG-UPM-001** + **ALG-P2-UPM-IMPL-001**。
  - SCI/公式语义不在此重复定义，两处冲突时以 docs/science/ 为准并
    回改本文档（禁止反向）；descriptor 词汇 astrocs.phase2.upm-fit/
    upm-apply 端口语义对齐归 P2-XX-INT（DISP-P2UPM-004），不作冻结依据。

## 6 离散公式 F1-F6（公式语义与 UPM_SOLVER.md §2 一致，逐条实现锚）

**F1 raw weight**（p2_upm_raw_weight 单一实现 :1292-1323；upm.h:125-133）：

```text
production（use_ivar_weight=1，默认）: raw_w = quality_factor × control_ivar
                                                   # :1307-1313；无 snr/support 因子
ablation（use_ivar_weight=0，仅诊断 SNR-015）:
  raw_w = qf · support^support_power · snr²/(1+snr²) / max(unc², sigma_floor²)
                                                   # :1315-1322
quality_factor: flags&16→0.0; &2→0.1; &1→1.0; 未知→0.5     # :177-186
control_variance = k_corr·(π/2)·σ_bg²/N_retained; control_ivar = 1/control_variance
  （sampler 域 ALG-UPM-CONTROL-IVAR-001 承接产出；upm.h:43-50 冻结注释）
rc=2 门: control_ivar≤0/非有限 → rc=2 显式拒绝（:1311；build 内 :602-610
  升级 build rc=2，禁止静默回退 legacy；DATA-UPM-CONTROL-UNC-001）
```

**F2 per-control 归一化**（out_norm = raw/Σraw × control_reliability）：

```text
build 内: sums[ck] 按 control 聚合 raw；s>1e-12 → raw/sums×reliability
  else 0.0                                            # :553-559（1e-12 :555）
API 面: p2_upm_normalized_weights 同公式（sums map :1330-1335；
  rel :1336-1337；s>1e-12 :1341）                      # :1325-1344
reliability 来源: cfg.control_reliability（默认 1.0，upm.cpp:281-282）
```

**F3 Huber IRLS 坐标下降**（:493-876；公式注释 :493-504）：

```text
z = r/σeff;  r = y − M − C;  σeff = max(|uncertainty|, sigma_floor)
                                                     # :625-628（并行）/:643-646（串行）
loss(z) = 0.5z² (|z|≤δ) else δ(|z|−0.5δ)             # huber_rho :196-200
w(z)    = 1 (|z|≤δ) else δ/|z|                       # huber_w :202-206
w[i]    = raw_norm[i] × huber_w(z_i)                 # :629/:647
M 更新（固定 C）: M_k = Σ_i w·(y−C)/Σ_i w；参考帧观测优先，
  未覆盖延拓全部帧                                    # :656-764（sentinel 分支 :671-687）
C 更新（固定 M，逐帧 CG）: cg_solve_frame (:563-599, max_cg=200 :567)
  (W + λs·L + λ0·I) x = rhs                          # Ap 组装 :573-579
  图平滑 λs·Σ_{k~l}(C_ik−C_il)² + 弱零锚 λ0·Σ C_ik²   # 公式 :501；系数 :503-504
objective += raw_w·huber_rho(z)                      # :861-871
收敛: max_dM < tolerance && max_dC < tolerance → break  # :872
```

**F4 calibrated = raw − C(frame, leaf)**（C 场 = centered 双线性 8×8）：

```text
calibrate_block: out[i] = in[i] − evaluate_c_field(...)   # :1263-1266
evaluate_c_field: cell 中心节点（gx*64+32）；内区夹取两相邻中心；
  tile 最外缘线性外推（前两/后两 covered nodes）；外推锚点限于本
  tile 真实覆盖 cell 范围（tile_gx/gy_bounds）；缺失 cell 不引用为 0
  而按 0 值参与（at :158-165）                          # :112-175
grid=8, cell_side=64, tile_shift=9                      # :259-261/:1254/:1281/:1405
dense tile sheet 与 sparse 共用同一求值语义              # compute_tile :1410-1476
```

**F5 gauge 与连通分量**（断开分量独立、内容稳定 frame_id 排序）：

```text
连通分量 = frame-control 二分图（仅带 obs 节点参与数据图）  # :415-491
每分量 ref_frame = 分量内最小 frame_id                    # :464-469
gauge: ref 帧 C=0（该分量独立，非全局最小帧）              # :786-793/:827-832
M 参考帧语义: 分量内 ref 帧观测定义 M；ref 未覆盖延拓全帧    # :651-652/:688-705
无观测几何节点: component=sentinel(SIZE_MAX,:217) 不参与
  数据图/gauge；M 由全部帧加权（无 ref 语义）             # :417-418/:671-672
单帧区节点: build_geo 提供 nodes 全几何，obs 只含 ≥2 clean 帧；
  单帧区由平滑/Laplacian 延拓（harmonic continuation）     # upm.h:99-101
gauge/分量固定顺序: frame_ids 升序 set(:265)、DFS 起点 f 升序(:433-456)
```

**F6 持久化**（SHA-256 模型 hash + sparse json + dense cache）：

```text
model_hash = SHA-256(payload)                          # :879-921
  payload = version|target_order|cfg(smoothing/anchor/sigma_floor/
  support_power/use_ivar)|input_manifest_hash | frames(升序) |
  controls(tile,gx,gy,M) | cell_index | C 全量（max_digits10 序列化 :882-887）
sparse json: format="astrocs-upm-v2"，frames[]+C[] 行序显式持久化，
  原子写 aio_upm_write_sparse                          # :940-1006（:947/:1003-1005）
open 强校验: format(:1027-1028)、frames 存在/数组/无重复/类型
  (:1064-1084)、controls/C 字段类型与行数（:1104-1134/:1177-1206）、
  任何损坏 → rc=1 稳定错误禁止异常越界（:1224-1228）
dense cache: format="astrocs-upm-dense-v2"（aio_upm.cpp:223/:407-408），
  source_hash=model_hash 校验（:1401-1403/:1530-1532）
dense/sparse 等价门: 1e-12（UPM_SOLVER.md §8/§9 冻结；§13 T5）
```

## 7 确定性与归约

- **并行归并 = worker-local 分块 + 按 tid 升序合并（D1 类）**：
  compute_raw 每 worker 写私有 tsums[tid]，join 后按 t 升序累加进
  sums（:511-514 注释"worker 数无关"定义 D1；:517-539 实现）——
  浮点和顺序与 worker 数无关；M/C 更新的 max 归约同构
  （tmax per-worker + join 合并 ：660-662/:712-716、:777/:818-822）；
- **IRLS 迭代序固定**：每轮严格 raw→w→M→C→objective（:601-873）；
  CG 从零初值起步（"每轮目标随 M 更新变化：从 0 开始解"：568-569）；
- **gauge/连通分量固定顺序**：frame_ids 为 std::set 升序（:265），
  frame_index 按 set 序分配（:340-346）；DFS 从 f=0 升序起点
  （:433-456）；ref_frame=min(frame_id)（:465-469）——与输入
  obs 顺序无关（输入顺序无关性由 gauge 注释 :20 冻结）；
- **materialize 固定块结构**：kChunk=16（:1407），frame 外层升序 +
  tile 集合 std::set 升序（:1397-1400），"分批并行求值 → 块内按
  (f,tile) 单调序串行写"，每像素值与并行度无关 → dense 缓存
  bit-identical（:1383-1386 注释冻结）；
- **hash 精确序列化**：max_digits10（:884）——同输入三次 build
  model_hash 逐位一致（§13 T2 承载）。

## 8 并行语义与 SIMD 安全

- **无 OpenMP**（2026-09-10 实测 grep `#pragma omp` 于 upm.cpp 零命中）；
  并行路径全部为 **std::thread 池**，共 5 段：
  1. compute_raw raw+归一化聚合：cworkers :515，池 ：521-534，tsums 合并
     :537-539（段 ：509-561）；
  2. IRLS w 逐 obs Huber 权重（per-obs 写 w[i] 不相交）：cworkers :615，
     池 ：617-633（段 ：612-650）；
  3. M 更新逐 control 分块 + tmax 归并：cworkers :658，池 ：661-715
     （段 ：656-764）；
  4. C 更新逐 frame CG：cworkers :775，池 ：778-820，tmax 合并
     :821-822（段 ：765-858）；
  5. dense materialize 逐 tile 求值（kChunk=16 分批）：nw=workers :1478，
     池 ：1484-1496（段 ：1479-1502）。
- **worker 数唯一来源 = cfg.cpu_workers**（Runtime lease：
  p2_session.cpp:155 sampler / :195 upm 传 budget.max_workers；
  upm.cpp:235 注释"默认 1(串行 reference); 生产由 p2_session 传
  lease"；:511-514 注释禁 hardware_concurrency——**无硬件探测**，
  2026-09-10 实测 grep hardware_concurrency 仅命中该注释行 :512）；
- **SIMD 安全**：残差/Huber 权重逐观测独立（w[i] 写不相交 ：613 注释；
  raw_w[i] 同构 ：524-531）；加权 M/C 聚合为观测/控制索引固定序累加
  （FP64，禁止重结合；归并顺序 §7 冻结）；dense 每 (f,tile) 输出
  buf 独立无别名（:1482/:1491-1492）；upm.h:89-91 注释残留
  OpenMP/CON-005 旧表述为历史漂移（DISP-P2UPM-002），实现以本节为
  唯一权威；
- **取消点**：迭代边界检查（:770-772 注释），取消不写半成品
  （persist 段整模型单元，p2_session.cpp:221-233）。

## 9 复杂度

- build 主体 O(iter × (n_obs + Σ_k deg(k) + F×CG))；单帧 CG 每次
  max_cg=200 迭代 × O(K + Σdeg)（:563-599）；rhs/obs_w 每 frame 每
  control 聚合该帧 obs（:795-811）；
- 连通分量/邻接建图 O(n_obs + K + 边界对粗筛)（:351-491）；
- model_hash O(n_obs 序列化 + F×K)（:879-921）；
- persist O(F×K)（C 稀疏行 :993-1002）；materialize
  O(F × n_tiles × 512²) 双线性求值（:1410-1476）；
- 内存 O(n_ctrl + F×K)（C/obs_w 稠密矩阵，Model :74/:76；
  dense 求值缓冲上界 kChunk×512²×8 字节 = 16×512²×8（:1386/:1482））。

## 10 边界与错误（rc 语义表，2026-09-10 实测锚）

| 面 | rc | 条件 | 锚 |
|---|---|---|---|
| p2_upm_build / build_geo | 0 | 成功 | :926/:931/:937 |
| 同上 | 1 | 参数错：null out_model/obs、**n_obs=0**、target_order<0（无有效 leaf 层级）；save/open 参数错与 IO/format/字段损坏同码 | :215、:246-249、:941、:1009-1028、:1064-1134、:1177-1227 |
| 同上 | 2 | production 缺 control_ivar（raw_weight rc=2 传播；显式科学错误禁静默降级） | :1311→:602-610 |
| p2_upm_raw_weight | 0/1/2 | ok / 参数错 / production 缺 control_ivar | :1294/:1311；语义注释 upm.h:125-133 |
| p2_upm_calibrate_block | 0/1 | ok / 参数 null 或 **未知 frame_id（显式失败禁回退 frame 0）** | :1244-1247、:1249-1252 |
| p2_upm_evaluate_c | NaN | 未知 frame_id 返回 NaN（显式不可用；null model 返回 0.0） | :1273-1279 |
| p2_upm_dense_read_block | 0/1/2 | ok / 参数 null·未知 frame_id·io/parse / **stale-cache（source hash 不匹配）** | :1547-1553、:1554-1556；语义注释 upm.h:164-165 |
| p2_upm_materialize_dense_n | 0/1 | ok / null、tile_set 空、aio 失败 | :1392-1404、:1508-1514 |
| p2_upm_component_gauges | 0/1/2 | ok / null model / ref_frame 容量不足 | :1372-1376 |

- 空输入链：n_obs=0 → rc=1（:215）；无 target_order（cfg.target_order<0
  且未显式给出）→ rc=1（:246-249，注释"空间 UPM 必须知道 control
  leaf 层级"）；
- frame 语义边界：frame_id 重复 → open 拒绝（:1078-1081）；参数行数
  ≠ frame 数 → save/open 拒绝（:945/:1181-1184）；
- 退化几何：无观测几何节点 sentinel 不入数据图（:217/:417-418）；
  断开分量各自 gauge（:22 注释/:415-491）；单帧区 continuation
  （upm.h:99-101）；
- 错误粒度 = rc 二值/三值 + AIO 层 aio_upm_last_error 文本；编排层
  ACS_ERR 映射归 API-P2-001 编排面，不在本模块域。

## 11 Oracle（恢复真值面与等价 oracle）

- **恒等/线性面恢复 oracle**：合成 k·f 线性真值场 → UPM 恢复后
  evalrel(f)≈0.5f、maxdev<0.05（tests/api/test_upm_recovery_oracle.py
  驱动 :43-55；判据见 §13 T1/T2）；
- **dense==sparse 等价 oracle**：同一模型 dense 缓存读回与
  calibrate_block 稀疏求值 1e-12 等价（UPM_SOLVER.md §8/§9 冻结；
  dense source_hash=model_hash 强校验 ：1530-1532）；
- **权重门 UPMW-001..007**：snr 扰动不变（UPMW-001
  synthetic_gate.cpp:3915 起）、control_ivar 1:4 → raw 比率精确 1:4
  （UPMW-002 :3916/:3965）、星群不变性（UPMW-003 :3971-3998）等，
  锚定 §13 T6；
- **save→open 幂等 oracle**：重开值 max_abs==0（帧绑定门；
  UPM_SOLVER.md §12 承接，DATA-UPM-MODEL-001）。

## 12 TEST-DESIGN（TEST-P2-UPM-DESIGN 冻结，2026-09-10）

**双语声明**：本节为设计冻结面 **VERIFIED**（引用既有测试源容差）；
可执行 TEST-P2-UPM-001/002 登记为 **MISSING**（归 P2-UPM-TEST 落地，
本文件不冒认执行态；先例=PHASE2_SAMPLER.md §11.3 双面登记）。每条
阈值已在既有测试源冻结，**TEST 任务不得事后修改**：

| # | 设计面 | 冻结容差 | 现状测试锚 |
|---|---|---|---|
| T1 | 恒等/线性面恢复：线性真值 k·f → evalrel(f) | \|evalrel(f)−0.5f\|≤0.05（delta=0.05） | tests/api/test_upm_recovery_oracle.py test_02 :101-109 |
| T1' | 参数恢复 maxdev | maxdev < 0.05 | 同文件 test_01 :93-99（驱动输出 :51-55） |
| T2 | 确定性：重复 build model_hash | 三次逐位一致（bit-exact） | 同文件 test_03 :112-119 |
| T3 | 并行等价：workers∈{1,2,4} 重复 build hash；1T/2T control_count | hash bit-exact（consistent=1）；count 精确相等 | tests/api/test_upm_parallel.py test_01 :82-89（reps=4）、test_02 :91-101 |
| T4 | 资源：20 次 build/close 循环 RSS | 增长 < 1024 KB | 同文件 test_03 :103-110（rss_kb :26；driver :32-50） |
| T5 | dense/sparse 等价 | 1e-12（dense source hash 强校验） | UPM_SOLVER.md §8/§9 冻结面；dense_read_block :1542-1557 |
| T6 | 权重公式 UPMW-001..007 + raw_weight rc=2 门 | UPMW 判据 exact/double_eq（测试源冻结）；rc=2 exact | synthetic_gate.cpp:3915 起；upm.h:125-133/:1311 |
| T7 | 退化链：单帧区 continuation / 断开分量 gauge / 空 obs rc=1 / 未知帧 rc=1 | rc exact；gauge frame id exact | upm.h:99-101；:415-491；:215；:1252 |

负面矩阵：null 参数 rc=1、坏路径 open rc=1、format 不符 rc=1、
frames 重复 rc=1、C 行数≠frame 数 rc=1、dense stale rc=2。fixture
由固定 seed 合成输入生成（测试源内嵌驱动），不提交大二进制。

## 13 容差与冻结清单（默认值 2026-09-10 实测）

**P2UpmBuildConfig 默认值**（cfg_in=null 缺省面 ：218-245；生产组装
p2_session.cpp:187-195 同值）：

| 参数 | 默认 | 锚（upm.cpp） |
|---|---|---|
| huber_delta | 1.345（无量纲，单位=sigma_eff） | :224/:237 |
| max_iterations | 100 | :227/:238 |
| tolerance | 1e-6（收敛门 max_dM/max_dC） | :228、收敛判据 ：872 |
| sigma_floor | 1e-3 | :230/:239 |
| zero_anchor_weight（λ0） | 1e-3 | :226/:244 |
| smoothing_lambda（λs） | 0.0（默认关闭平滑） | :225/:245 |
| use_ivar_weight | 1（production；仅显式 0 进 ablation） | :233 |
| control_reliability | 1.0 | :234/:243 |
| cpu_workers | 1（串行 reference；生产=Runtime lease） | :235 |
| support_power | 1.0（仅 ablation 路径消费） | :231/:240 |

**数值常数**：CG max_cg=200（:567）、CG 早停 pAp≤1e-30（:586）/
rs_new<1e-24（:594）、归一化门 s>1e-12（:555/:1341）、per-control
sums 门 den>1e-12（:681/:697/:706/:731/:748/:757，:697/:748 为
den≤1e-12 反向分支）、kChunk=16（:1407）、
kLeafPx=512²（:1406）、跨 tile 邻接 link_rad=1.6×cell_dist（:384）、
dense/sparse 等价 1e-12。

**k_corr=1.4 冻结（MC 实测 1.3883）**：sampler.cpp:81 注释
"k_corr_empirical = 1.3883，N_eff ≈ 181 < N_retained=251. 冻结保守值
1.4"；常量 kControlCorrDefault=1.4（sampler.cpp:82）。**k_corr 属
sampler 域合同**（ALG-P2-SMP-001 §5.4 / ALG-UPM-CONTROL-IVAR-001，
PHASE2_SAMPLER.md 承载），本域只引用 control_ivar 消费面，不改不重复
标定。**本表数值与公式为冻结面：任何修改必须走 SCI/合同变更，禁止
在实现或测试内就地放宽。**

## 14 现状缺陷登记（DISP-P2UPM-001..004，登记不改码）

| ID | 锚 | 内容 | 整改去向 |
|---|---|---|---|
| DISP-P2UPM-001 | upm.h:154-156 与 :173-175 | p2_upm_materialize_dense **重复声明**（复制粘贴遗留；同头文件重复声明同一函数 C++ 合法、非 ODR 违例，运行无影响；纯合同卫生问题） | P2-UPM-IMPL |
| DISP-P2UPM-002 | upm.h:89-91 | cpu_workers 注释漂移：前半句"CON-005 … 仅 P2_ENABLE_OPENMP 时并行 compute_raw/聚合"与实现不符（现无 OpenMP、std::thread 五段池，§8）；:91 后半句 Runtime lease 语义正确 | P2-UPM-IMPL（随 001 一并清） |
| DISP-P2UPM-003 | p2_session.cpp:196-202 | upm 配置覆盖键仅 {max_iterations,huber_delta,smoothing_lambda}；zero_anchor_weight/tolerance 无 config 键（build 缺省修补面 ：244-245 只拦非法值，session 面不可配） | P2-SESSION-IMPL |
| DISP-P2UPM-004 | lib/core/src/module_adapters.cpp:665-698 | descriptor 端口语义占位：fit 行 upm_model=可选输出（:608 required=false）、apply 行 upm_model=必选输入（:626 required=true），与真实数据流（fit 进程内 build→persist 落盘 upm_sparse.json；apply/reload 经文件+p2_upm_open）不符 | P2-XX-INT |

登记原则：本批只登记不改码（P2-UPM-DOC 冻结范围=文档）；001/002
整改编入 P2-UPM-IMPL 任务面，003 归 P2-SESSION-IMPL，004 归
P2-XX-INT 对齐。

## 15 消费链（生产编排与 apply/reload 面）

- **编排（fit+persist）**：lib/phase2_session/p2_session.cpp:180-233
  upm_build+persist 段——生产 cfg 组装 :187-195（含
  uc.cpu_workers = s->host->budget.max_workers，:195，Runtime lease
  唯一来源；sampler 同源 :155）；config 覆盖键 ：196-202（三键，
  DISP-P2UPM-003）；p2_upm_build 调用 ：204；info 上账 ：206-219；
  persist 段 ：221-233（persist_upm+upm_save_path 门控、失败
  ACS_ERR_IO）；
- **dense 物化**：lib/phase2/tools/stage2.cpp:470-495——
  p2_upm_save :472 → p2_upm_materialize_dense_n :481-483（workers=
  effective_cpu_workers(cfg.exec)，与积分一致）；
- **apply 面**：stage2.cpp 经 p2_upm_calibrate_block（:927 与 :1272
  两处逐块校准；进程内 model 直通，无二次 open）；
- **reload 消费（p2_upm_open）**：lib/phase2/tools/
  calibrated_pair_diag.cpp:202（诊断工具读取 upm_sparse.json +
  p2_upm_calibrate_block :334 注释锚）；测试面 synthetic_gate.cpp
  多处 save→open 幂等门（:325/:380/:391/:1824/:2066/:2257/:2335/
  :5007/:5019/:5082）；
- **descriptor 面**：astrocs.phase2.upm-fit / astrocs.phase2.upm-apply
  （module_adapters.cpp:665-698；节点链 coverage → sample → upm_fit
  → upm_apply → reject → integrate → write，:557 注释）——端口语义
  占位见 DISP-P2UPM-004。

## 16 关联 ID 映射（本文件承接）

- `ALG-P2-UPM-IMPL-001` = 本文档整体（逐符号锚 §3/§6；实现级合同）。
- 上游：SCI-UPM-001（FROZEN 集合零改动，§5）；ALG-UPM-001
  （docs/algorithms/UPM_SOLVER.md，推导权威，本批原位修订）；ALG-
  UPM-CONTROL-IVAR-001（PHASE2_SAMPLER.md §5.4/§12 承载）；ALG-P2-
  SMP-001（obs/frame_id 上游）。
- 下游 DATA：DATA-P2-UPM（fit 产物）/ DATA-P2-COR（apply 产物）
  （descriptor data_id :612/:632；矩阵行 ：21/:22）。
- API 面：**API-P2-UPM-001**（矩阵/descriptor 词汇；PUBLIC_API.md
  2026-09-10 实测尚未落 upm 节——落位归 P2-UPM-DOC 其余产物或后续
  合同任务，本文件只登记词汇不冒认条目存在）。
- TEST：TEST-P2-UPM-001（fit 面）/ TEST-P2-UPM-002（apply 面）
  ——设计冻结=本文档 §12；可执行 MISSING 归 P2-UPM-TEST。
- MOD：astrocs.p2.upm（fit/apply 两模块页
  docs/modules/registry/astrocs.phase2.upm-fit.md /
  astrocs.phase2.upm-apply.md）。

## 17 追溯

- 矩阵行：MOD-astrocs-phase2-upm-fit（TRACEABILITY_MATRIX.csv:22，
  SCI-P2-UPM-001/ALG-P2-UPM-001/DATA-P2-UPM/TEST-P2-UPM-001）与
  MOD-astrocs-phase2-upm-apply（:21，SCI-P2-UPM-002/ALG-P2-UPM-002/
  DATA-P2-COR/TEST-P2-UPM-002）。
- 占位 ID 与本文件关系：矩阵 algorithm_id=ALG-P2-UPM-001/002 为
  descriptor 占位词汇，其语义由 §5 映射声明分解为 ALG-UPM-001
  （推导，UPM_SOLVER.md）+ ALG-P2-UPM-IMPL-001（本文件实现合同）；
  占位 ID 本身不注册 INDEX、不入合同（SAMPLER §12 退役先例同构）。
- 本文件属 P2-UPM-DOC 批次（2026-09-10 冻结，owner SA-P2-U21）；
  同批原位修订 ALG-UPM-001（UPM_SOLVER.md）：行号/并行表述如实更新，
  公式与容差零改动（其头部修订声明为证）。
