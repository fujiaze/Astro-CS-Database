# Phase2 UPM Fit/Apply Algorithms（P2-UPM / astrocs.p2.upm）

> 上游：ASTROCS_DESIGN.md §5.4（天光平面与统一相对模型）

> 本文件是 Phase2 Unified Photometric Model（UPM）
> fit+apply+persist+reload 的**实现级算法合同**：逐符号源码行号锚定 +
> 冻结容差 + 现状缺陷登记。科学语义权威=SCI-UPM-001（docs/science/
> PHASE2_UPM.md，FROZEN 集合，零改动）；推导级算法权威=ALG-UPM-001
> （docs/science/algorithms/UPM_SOLVER.md，公式与容差零改动）。
> 实现源: lib/algorithms/coverage/src/upm.cpp（2981 行）+ 唯一权威签名头
> lib/algorithms/coverage/include/astro/phase2/upm.h（453 行，实测）；
> API: API-P2-UPM-001（矩阵词汇；PUBLIC_API.md 尚未落页，见 §16）；
> DATA: DATA-P2-UPM / DATA-P2-COR；MOD: astrocs.p2.upm
> （TRACEABILITY_MATRIX.csv :21/:22 两行）；TEST: TEST-P2-UPM-001/002
> （设计冻结面=本文档 §13，可执行 MISSING 归 P2-UPM-TEST）。

## 1 目的与非目标

**目的**（SCI-UPM-001 §1 承接；实现域=astrocs.p2.upm 全链）：

- **fit**：从控制观测流（P2ControlObservation[]，上游
  ALG-P2-SMP-001 产出）联合求解 ONE UnifiedPhotometricModel——
  M_k latent reference + C_i(p) 每帧空间校正场，Huber IRLS 坐标
  下降 + 图平滑 + 弱零锚 + 分量 gauge（upm.cpp:500-889）；
- **apply**：calibrated = raw − C(frame, leaf)，运行时唯一入口
  p2_upm_calibrate_block（upm.h:12-13 冻结"不暴露 per-frame
  gradient 产品；运行时只经 p2_upm_calibrate_block 使用"）；
- **persist**：稀疏 json（astrocs-upm-v2）+ 稠密缓存
  （astrocs-upm-dense-v2）双形态落盘，frame 绑定显式持久化；
- **reload**：p2_upm_open 强校验重开，save→open 绑定不变
  （upm.cpp:1022-1247；ALG-UPM-FRAME-BIND-001 / DATA-UPM-MODEL-001）。

**非目标（本模块不做）**：

- 不做控制点采样（ALG-P2-SMP-001 域，obs 输入来自该域）；
- 不做每像素候选排异（ALG-P2-REJ-001 域）；
- 不做样本合并/积分（ALG-P2-INT-001 域）；
- 不产 per-frame gradient 产品（upm.h:12-13 冻结，对外面只含 C 场消费路径，不含
  per-frame 梯度面；C 场只经 calibrate_block/evaluate_c 消费）；
- 不解释 control_variance 科学定义（sampler 域
  ALG-UPM-CONTROL-IVAR-001 承接，本域只消费 control_ivar）。

## 2 符号与单位（权威=本表 + SCI-UPM-001 §3）

| 符号 | 含义 | 单位/域 | 实现锚 |
|---|---|---|---|
| y_ik（value） | 控制观测（patch 位置估计，可负） | 面亮度 ADU·sr⁻¹ | upm.h:282（字段 `value`） |
| uncertainty | control estimator 标准误 = sqrt(control_variance) | 面亮度 ADU·sr⁻¹ | 由 `control_ivar` 反演（`upm.cpp:1665-1681`） |
| control_variance | k_corr·(π/2)·σ_bg²/N_retained（sampler 域产出） | (ADU·sr⁻¹)² | `upm.cpp:2785`（唯一发布点 `p2_upm_control_variance`） |
| control_ivar | 1/control_variance | (ADU·sr⁻¹)⁻² | upm.h:283（字段 `control_ivar`） |
| ivar（弃用字段） | 单像素 Phase1 ivar，仅诊断，禁入科学权重 | — | upm.h 冻结注释（禁入科学权重） |
| M_k | latent unified reference | 面亮度 ADU·sr⁻¹ | upm.cpp:65（`ControlNode::M`） |
| C_i(p) | 每帧空间校正场（centered 双线性 8×8） | 面亮度 ADU·sr⁻¹ | upm.cpp:131-200（`evaluate_field_row`）/ `:202-205`（`evaluate_c_field`） |
| raw / calibrated | 校准前/后信号 | 面亮度 ADU·sr⁻¹ | upm.cpp:1591-1625 |
| θ（每帧系数） | C 稀疏矩阵行 = frame 系数 | 面亮度 ADU·sr⁻¹ | upm.cpp:82（`Model::C` [frame][control]） |
| grid=G | 每 tile cell 网格边长 | 无量纲（=8） | upm.cpp:91 |
| cell_side | tile 边长/G=512/8=64 | leaf px | upm.cpp:92 |
| tile_shift | leaf order = target_order+9 → tile 移位 | 无量纲（=9） | upm.cpp:231-233（`leaf_to_tile`）/ `:235-237`（`leaf_local`）；`:2060` |
| leaf_ipix | NESTED leaf 像素号（cell 中心） | 无量纲 u64 | upm.cpp:66（`ControlNode::leaf_ipix`） |
| frame_id | 内容稳定帧身份（sampler 域 p2_frame_id 产出） | 无量纲 u64 | upm.h:275（`P2UpmMaObservation::frame_id`）/ upm.cpp:81（`Model::frame_id_by_index`） |
| w_UPM | IRLS 最终权重 = raw_norm × huber_w | (ADU·sr⁻¹)⁻² | upm.cpp:1966-1997（`p2_upm_raw_weight`）；`p2_upm_normalized_weights` **已 RETIRED**（定义删除、全仓零消费者，注销登记 `:1999`） |
| quality/support | 观测质量因子 / 覆盖支持度 | 无量纲 [0,1] | upm.cpp:220-229（`quality_factor`：16→0.0 / 2→0.1 / 1→1.0 / 0→0.5 / 其余 0.5） |
| dtype | 全链 FP64（P2ModelInfo.precision=1 fp64 reference） | — | upm.h:62 |

单位面逐项冻结（**逐项与 SCI-UPM-001 §3 同标度**）：`C/M/raw/calibrated/σ_bg` = **面亮度 ADU·sr⁻¹**、
`control_variance` = **(ADU·sr⁻¹)²**、`control_ivar` = **(ADU·sr⁻¹)⁻²**、`w_UPM` = **(ADU·sr⁻¹)⁻²**、
`quality/support` 无量纲、`frame_id` 无量纲 uint64。
**标度来源（正向约束）**：上游 Phase1 HiPS signal 层写盘 BUNIT 冻结集 {ADU/sr, ADU^2/sr^2, sr^2/ADU^2}，
裸 ADU 判红（`lib/infrastructure/aio/src/hiss_writer.cpp:335-365`）；本层一切绝对量随该标度，
**不是**与仪器无关的常数——跨标度（如与 ADU 域孔径测光量）混用前必须先声明 Ω_px 换算。

## 3 逐符号锚（upm.cpp 2981 行 / lib/algorithms/coverage/include/astro/phase2/upm.h 453 行）

**导出符号（`lib/algorithms/coverage/include/astro/phase2/upm.h` 声明 / upm.cpp 实现）**：

| 符号 | 声明（lib/algorithms/coverage/include/astro/phase2/upm.h） | 实现（upm.cpp） | 语义 |
|---|---|---|---|
| p2_upm_build | :124-126 | :1372-1375 | 构建（obs 驱动拓扑）入口 |
| p2_upm_build_geo | :132-135 | :1377-1381 | 全几何节点构建（含单帧区 continuation） |
| p2_upm_save | :137-137 | :1383-1504 | 稀疏 json 原子写（aio_upm_write_sparse） |
| p2_upm_open | :138-138 | :1506-1801 | 强校验重开（format/frames/controls/C） |
| p2_upm_info | :139-139 | :1803-1808 | P2ModelInfo 快照（hash/control_count） |
| p2_upm_convergence | :154-155 | :1893-1905 | 迭代/目标/收敛**只读**访问器（不改 P2ModelInfo 冻结布局） |
| p2_upm_calibrate_block | :158-164 | :1907-1941 | 逐块校准（唯一运行时 apply 面） |
| p2_upm_evaluate_c | :167-168 | :1943-1963 | 直接求值 C(frame,leaf) |
| p2_upm_raw_weight | :179-181 | :1966-1997 | raw 权重单一实现（production/ablation） |
| p2_upm_geometry_hash | :188 | :2001-2022 | 几何/拓扑 hash（不含观测可信度） |
| p2_upm_component_gauges | :192-194 | :2024-2036 | 每分量 gauge frame id 查询 |
| p2_upm_materialize_dense_n | :219-220 | :2045-2174 | 稠密缓存物化（worker 数显式） |
| p2_upm_materialize_dense | :197-198/:216-217 | :2177-2180 | 稠密物化 wrap（workers=0 auto；声明重复见 DISP-P2UPM-001） |
| p2_upm_dense_info | :201-205 | :2182-2199 | dense 信息（等价门用） |
| p2_upm_dense_read_block | :208-215 | :2201-2216 | dense 块读（stale 拒绝） |
| p2_upm_close | :222 | :2218-2222 | 释放 |

**内部符号（匿名 namespace）**：

| 符号/段 | 锚（upm.cpp） | 语义 |
|---|---|---|
| build_impl | :255-1370 | 构建/求解/哈希本体（两入口共用） |
| evaluate_c_field | :215-237 | centered 双线性求值（行求值提取为 `evaluate_field_row` :144-214，sparse/dense 共用语义） |
| quality_factor | :220-229 | quality_flags 映射（16→0 / 2→0.1 / 1→1.0 / 未知→0.5） |
| huber_rho / huber_w | :239-243 / :245-251 | Huber loss / 权重核（无量纲 z） |
| compute_raw（lambda） | :605-674 | raw 计算 + per-control 归一化 + 并行归并（tsums :616、池 :620-635、按 worker 序合并 :636-639） |
| cg_solve_frame（lambda） | :676-733 | 逐帧 CG（(W+λsL+λ0I)x=rhs；CG 循环 :685、max_cg=200 :680） |
| IRLS 主循环 | :736-1065 | 每轮 raw→w→M→C→objective/收敛（`for (int iter` :736） |
| Model/ControlNode | :63-143 | 状态载体（ControlNode :63-74、Model :75-143；C :82、gauge :86、component_ref_frame :97） |
| kNoData（sentinel） | :260 | 无观测几何节点标记 = SIZE_MAX |
| 头部冻结注释 | :1-27 | 语义冻结面（权重/ivar 状态机/gauge/持久化绑定） |

## 4 伪代码（build 与 apply 主流程）

```text
function build_impl(obs, n_obs, nodes, n_nodes, cfg):            # upm.cpp:255-1370
  校验: out_model/obs/n_obs==0 → rc=1 (:258); target_order<0 → rc=1 (:306-315)
  cfg 缺省修补（:261-305，默认值见 §13）
  装配: nodes 全几何 + obs 按 (tile,gx,gy) cell 去重 (:266-314)
  frame_ids(std::set 升序 :325) → frame_index/frame_id_by_index (:400-406)
  邻接图: 网格 4-邻接 + 跨 tile 几何邻接(角距<1.6×cell_dist) (:411-473)
  连通分量: frame-control 二分图 DFS; 无 obs 几何节点=sentinel (:475-530)
  每分量 gauge: ref_frame = 分量内最小 frame_id (:524-528)
  for iter in 0..max_iterations-1:                               # :736 起
    raw = p2_upm_raw_weight 逐 obs; 归一化 raw/Σraw×reliability  # :602-674
    w[i] = raw_norm[i] × huber_w(z_i); z = r/σeff                # :745-785
    M[k] = Σ w·(y−C)/Σ w（参考帧观测优先，未覆盖延拓全帧）        # :784-900
    C[f] = CG( W+λsL+λ0I , rhs ); 参考帧 C=0                     # :903-1010
    objective += raw·huber_rho(z); max_dM/tol && max_dC/tol → break  # :1013-1065
  model_hash = SHA-256(max_digits10 序列化: cfg+manifest+frames+controls+C[+G])  # :1161-1209

function calibrate_block(model, frame_id, leaves, in, out, n):   # :1907-1941（p2_upm_calibrate_block）
  未知 frame_id → rc=1 显式失败（:1919；回退到 frame 0 参数属于错误科学结果）
  逐 leaf: tile=leaf>>18; local=leaf&mask; nested_local_to_xy    # :1924-1929
  out[i] = in[i] − evaluate_c_field(model, fi, tile, x, y)       # :1932
```

## 5 上游 SCI 与映射声明（本域零 SCI 改动）

- 语义权威已有 FROZEN SCI：**SCI-UPM-001**（docs/science/PHASE2_UPM.md；
  冻结集合 = SCI-UPM-001..010 + SCI-UPM-WEIGHT-001 +
  SCI-UPM-PERSIST-001）。**共享 SCI 引用不改动**
  （P2-SAMP/P2-REJ 同构）。单位面（§2）直接承接
  SCI-UPM-001 §3 实测文本。
- **descriptor 占位映射声明**（仿 PHASE2_SAMPLER.md §11.4/§12 写法；
  占位 ID 是矩阵/descriptor 词汇，不注册 INDEX、不入合同）：
  - `SCI-P2-UPM-001`（fit descriptor sci_id，lib/infrastructure/scheduler/src/
    module_adapters.cpp:676）⇒ **SCI-UPM-001**；
  - `SCI-P2-UPM-002`（apply descriptor sci_id，module_adapters.cpp:696）
    ⇒ **SCI-UPM-001**；
  - `ALG-P2-UPM-001`（fit 行 algorithm_id，TRACEABILITY_MATRIX.csv:22）
    ⇒ **ALG-UPM-001**（docs/science/algorithms/UPM_SOLVER.md，推导权威）+
    **ALG-P2-UPM-IMPL-001**（本文件，实现级合同）；
  - `ALG-P2-UPM-002`（apply 行，TRACEABILITY_MATRIX.csv:21）⇒
    **ALG-UPM-001** + **ALG-P2-UPM-IMPL-001**。
  - SCI/公式语义不在此重复定义，两处冲突时以 docs/science/ 为准并
    回改本文档（方向 = 从 docs/science/ 到本文档）；descriptor 词汇 astrocs.phase2.upm-fit/
    upm-apply 端口语义对齐归 P2-XX-INT（DISP-P2UPM-004），不作冻结依据。

## 6 离散公式 F1-F6（公式语义与 UPM_SOLVER.md §2 一致，逐条实现锚）

**F1 raw weight**（p2_upm_raw_weight 单一实现 :1966-1997；upm.h:138-145）：

```text
production（use_ivar_weight=1，默认）: raw_w = quality_factor × control_ivar
                                                   # :1980-1988；无 snr/support 因子
ablation（use_ivar_weight=0，仅诊断 SNR-015）:
  raw_w = qf · support^support_power · snr²/(1+snr²) / max(unc², sigma_floor²)
                                                   # :1989-1997
  # 天光控制点的被估量是**变化的背景电平**，SNR² 在该处不是有效逆方差代理；
  # 生产一律取 control_ivar，SNR 只作 veto/质量门（SCI-UPM-001 §5）
quality_factor: flags&16→0.0; &2→0.1; &1→1.0; 未知→0.5     # :220-229
control_variance = k_corr·(π/2)·σ_bg²/N_retained; control_ivar = 1/control_variance
  （sampler 域 ALG-UPM-CONTROL-IVAR-001 承接产出；upm.h:43-50 冻结注释）
rc=2 门: control_ivar≤0/非有限 → rc=2 显式拒绝（:1985；build 内 :744
  升级 build rc=2；静默降级恒不接受；DATA-UPM-CONTROL-UNC-001）
```

**F2 per-control 归一化**（out_norm = raw/Σraw × control_reliability）：

```text
build 内: sums[ck] 按 control 聚合 raw；s>0 && isfinite(s) → raw/sums×reliability
  else 0.0                                            # :665-669（尺度无关判据 :667-669；
  # 旧绝对阈值 sums[ck] > 1e-12 已按 SCI-UPM 改成尺度无关形式 :655-661）
API 面: `p2_upm_normalized_weights` **已 RETIRED**（定义删除、全仓零消费者，
  注销登记 upm.cpp:1999）——本公式的唯一生产实现 = build 内上列段
reliability 来源: cfg.control_reliability（默认 1.0，upm.cpp:288-289）
```

**F3 Huber IRLS 坐标下降**（`build_impl` :255-1370 的迭代主体，主循环 :736-1065；观测加权 :745-785、M 更新 :786-900、C 更新 :903-1010、收敛 :1013-1065）：

```text
z = r/σeff;  r = y − M − C;  σeff = max(|uncertainty|, sigma_floor)
                                                     # :1008-1009
loss(z) = 0.5z² (|z|≤δ) else δ(|z|−0.5δ)             # huber_rho :239-243
w(z)    = 1 (|z|≤δ) else δ/|z|                       # huber_w :245-251
w[i]    = raw_w[i] × huber_w(z_i)                    # :763（并行）/:780（串行）
raw_w = p2_upm_raw_weight 输出（production: quality_factor×control_ivar）  # :579/:1966-1997
M 更新（固定 C）: M_k = Σ_i w·(y−C)/Σ_i w；参考帧观测优先，
  未覆盖延拓全部帧                                    # :786-900（sentinel 分支随分量 gauge）
C 更新（固定 M，逐帧 CG）: cg_solve_frame (:676-733, max_cg=200 :680)
  (W + λs·L + λ0·I) x = rhs                          # Ap 组装 :685-733
  图平滑 λs·Σ_{k~l}(C_ik−C_il)² + 弱零锚 λ0·Σ C_ik²
objective += raw_w·huber_rho(z)                      # :1023-1026
收敛: tol_M/tol_C 由 tolerance × (tolerance_relative ? max(scale_obs,1) : 1) 决定
      max_dM < tol_M && max_dC < tol_C → converged=1  # :1034-1050（详见 §13）
```

**F4 calibrated = raw − C(frame, leaf)**（C 场 = centered 双线性 8×8）：

```text
calibrate_block: out[i] = in[i] − evaluate_c_field(...)   # :1907-1941
evaluate_c_field: cell 中心节点（gx*cell_side+cell_side/2）；内区夹取两相邻中心；
  tile 最外缘线性外推（前两/后两 covered nodes）；外推锚点限于本
  tile 真实覆盖 cell 范围（tile_gx/gy_bounds）；缺失 cell 不引用为 0
  而按 0 值参与                                          # :144-214（evaluate_field_row）/:215-237（evaluate_c_field）
grid=8, cell_side=64, tile_shift=9                      # :91-92/:218-224
dense tile sheet 与 sparse 共用同一求值语义              # p2_upm_materialize_dense_n :2045-2174
```

**F5 gauge 与连通分量**（断开分量独立、内容稳定 frame_id 排序）：

```text
连通分量 = frame-control 二分图（仅带 obs 节点参与数据图）  # :475-552
每分量 ref_frame = 分量内最小 frame_id                    # :524-528
gauge: ref 帧 C=0（该分量独立，非全局最小帧）              # :928/:976
M 参考帧语义: 分量内 ref 帧观测定义 M；ref 未覆盖延拓全帧    # :786-900（ref 选择 :579/:825/:878）
无观测几何节点: component=sentinel(SIZE_MAX) 不参与
  数据图/gauge；M 由全部帧加权（无 ref 语义）             # :260/:489
单帧区节点: build_geo 提供 nodes 全几何，obs 只含 ≥2 clean 帧；
  单帧区由平滑/Laplacian 延拓（harmonic continuation）     # :1054-1090；upm.h:99-101
gauge/分量固定顺序: frame_ids 升序 set、DFS 起点 f 升序
```

**F6 持久化**（SHA-256 模型 hash + sparse json + dense cache）：

```text
model_hash = SHA-256(payload)                          # :1161-1209
  payload = version|target_order|cfg(smoothing/anchor/sigma_floor/
  support_power/use_ivar/final_gauge)|input_manifest_hash | frames(升序) |
  controls(tile,gx,gy,M) | cell_index | C 全量（max_digits10 序列化 :1166）
  [|G gauge 段]  ——  final_gauge=1 时追加：`payload += "|G"` 后逐 control 写
    `fmt(m->gauge[k])`（**段头 :1199-1205**；G 的产生段 = 末端残差场 :1109-1160）。
    **该段此前在本清单中缺失**（DISP-P2UPM-005 勘误）：生产恒置 final_gauge=1
    （`module_adapters.cpp:9642`）⇒ |G 在生产 payload 里**必然存在**、
    model_hash 必须是含 G 的 hash；final_gauge=0 时 m->gauge 为空、
    payload 与 legacy 逐位一致（:1199-1201 注释冻结，既有 model_hash 门不受影响）。
sparse json: format="astrocs-upm-v2"（:1390），frames[](:1468)+C[](:1490)
  行序显式持久化，唯一 AIO 出口 aio_upm_write_sparse（原子写）  # :1501-1503
open 强校验: format(:1525-1526)、frames 存在/数组/无重复/类型、
  controls/C 字段类型与行数、
  任何损坏 → rc=1 稳定错误；异常越界恒不接受（p2_upm_open :1506-1801）
dense cache: format="astrocs-upm-dense-v2"（aio_upm.cpp），
  source_hash=model_hash 校验（p2_upm_dense_read_block :2201-2216）
dense/sparse 等价门: 1e-12（UPM_SOLVER.md §8/§9 冻结；§13 T5）
```

## 7 确定性与归约

- **并行归并 = worker-local 分块 + 按 tid 升序合并（D1 类）**：
  compute_raw 每 worker 写私有 tsums[tid]，join 后按 t 升序累加进
  sums（实现 :614-641：tsums :616、池 :620-635、按 worker 序合并 :636-639）——
  **冻结口径是三档、不是"worker 数无关"**（`:607-613` 注释冻结；权威
  `docs/science/PHASE2_UPM.md` §7:184-191 与本处同文）：(a) **同配置重复构建 =
  位精确 + model_hash 逐字相同**（构造保证：连续块划分 + 不相交写 + 无共享
  浮点累加器）；(b) **跨 worker 数（1..N）= 1e-12 绝对容差，不是位精确** ——
  per-control 求和的结合顺序随 worker 切片变化（FP 加法非结合），
  实测 ΔC_max 2.22e-15 ≈ 1 ulp @10 ADU；(c) 跨后端等价 = **无此合同**。
  可执行证据 `eng/tests/api/test_upm_parallel.py` 只在**同 worker 数**下断言
  位精确（test_01 :82-89 reps=4、test_02 :91-101），**不跨 worker 数**断言；
  M/C 更新的 max 归约同构
  （tmax per-worker + join 合并 ：793-795/:848-852、:915-917/:965-969）；
- **IRLS 迭代序固定**：每轮严格 raw→w→M→C→objective（:736-1065）；
  CG 从零初值起步（"每轮目标随 M 更新变化：从 0 开始解"：568-573）；
- **gauge/连通分量固定顺序**：frame_ids 为 std::set 升序（:325），
  frame_index 按 set 序分配（:400-406）；DFS 从 f=0 升序起点
  （:475-523）；ref_frame=min(frame_id)（:524-528）——与输入
  obs 顺序无关（输入顺序无关性由 gauge 注释 :20 冻结）；
- **materialize 固定块结构**：kChunk=16（:2062），frame 外层升序 +
  tile 集合 std::set 升序（:2138-2141），"分批并行求值 → 块内按
  (f,tile) 单调序串行写"，每像素值与并行度无关 → dense 缓存
  bit-identical（:2041/:2136 注释冻结）；
- **hash 精确序列化**：max_digits10（:1161-1169）——同输入三次 build
  model_hash 逐位一致（§13 T2 承载）。

## 8 并行语义与 SIMD 安全

- **无 OpenMP**（实测 grep `#pragma omp` 于 upm.cpp 零命中）；
  并行路径全部为 **std::thread 池**，共 5 段：
  1. compute_raw raw+归一化聚合：cworkers :614，池 ：620-635，tsums 合并
     :636-639（段 ：605-674）；
  2. IRLS w 逐 obs Huber 权重（per-obs 写 w[i] 不相交）：cworkers :749，
     池 ：751-768（段 ：747-785）；
  3. M 更新逐 control 分块 + tmax 归并：cworkers :791，池 ：794-851
     （段 ：786-900）；
  4. C 更新逐 frame CG：cworkers :913，池 ：916-964，tmax 合并
     :965-969（段 ：903-1010）；
  5. dense materialize 逐 tile 求值（kChunk=16 分批）：nw=workers :2137，
     池 ：2142-2150（段 ：2136-2174，kChunk :2062）。
- **worker 数唯一来源 = cfg.cpu_workers**（Runtime lease：
  p2_session.cpp:155 sampler / :195 upm 传 budget.max_workers；
  upm.cpp:278 注释「默认 1(串行 reference); 生产由 p2_session 传
  lease」；:607-608 注释明确 **无 hardware_concurrency**——实测 grep 仅命中该注释行）；
- **SIMD 安全**：残差/Huber 权重逐观测独立（w[i] 写不相交）；加权 M/C
  聚合为观测/控制索引固定序累加（FP64，累加按固定序、结合方式逐位固定；归并顺序 §7 冻结）；
  dense 每 (f,tile) 输出 buf 独立无别名（:2139-2174 分块循环）；
  `upm.h:92-94` 注释残留 OpenMP 旧表述为漂移（§14 DISP-P2UPM-002），
  实现以本节为唯一权威；
- **取消点（勘误 DISP-P2UPM-006）**：`upm.cpp` **不含任何取消检查**——
  实测 `grep -c cancel upm.cpp` = **0 命中**，迭代一律跑满
  `cfg.max_iterations`；取消只在 **session 阶段边界**检查并立即返回
  `ACS_ERR_CANCELLED`：coverage 前 `p2_session.cpp:122`、sample 前 `:160`、
  **upm_build 前 :195**、persist 前 `:240-242`。故"取消不写半成品"由
  **阶段边界 + 整模型单元提交**保证，**不是**"迭代边界取消点"（旧表述勘误）；
  权威口径见 `docs/science/PHASE2_UPM.md:215-217`。

## 9 复杂度

- build 主体 O(iter × (n_obs + Σ_k deg(k) + F×CG))；单帧 CG 每次
  max_cg=200 迭代 × O(K + Σdeg)（`cg_solve_frame` :676-733）；rhs/obs_w
  每 frame 每 control 聚合该帧 obs；
- 连通分量/邻接建图 O(n_obs + K + 边界对粗筛)（:475-552）；
- model_hash O(n_obs 序列化 + F×K)（:1161-1209）；
- persist O(F×K)；materialize
  O(F × n_tiles × 512²) 双线性求值（`p2_upm_materialize_dense_n` :1756-1885）；
- 内存 O(n_ctrl + F×K)（C/obs_w 稠密矩阵，Model :74/:76；
  dense 求值缓冲上界 kChunk×512²×8 字节 = 16×512²×8（:1386/:1482））。

## 10 边界与错误（rc 语义表，实测锚）

| 面 | rc | 条件 | 锚 |
|---|---|---|---|
| p2_upm_build / build_geo | 0 | 成功 | :926/:931/:937 |
| 同上 | 1 | 参数错：null out_model/obs、**n_obs=0**、target_order<0（无有效 leaf 层级）；save/open 参数错与 IO/format/字段损坏同码 | :215、:246-249、:941、:1009-1028、:1064-1134、:1177-1227 |
| 同上 | 2 | production 缺 control_ivar（`p2_upm_raw_weight` rc=2 传播；显式科学错误禁静默降级） | :1650-1681 → :2256 |
| p2_upm_raw_weight | :179-181 | ok / 参数错 / production 缺 control_ivar | :1650-1681；语义注释 upm.h:138-145 |
| p2_upm_calibrate_block | :158-164 | ok / 参数 null 或 **未知 frame_id（显式失败禁回退 frame 0）** | :1591-1625 |
| p2_upm_evaluate_c | :167-168 | **未知 frame_id 与 null model 一律返回 NaN**（显式不可用；0.0 是 gauge 参考帧的合法 C 值，禁作哨兵） | :1627-1647 |
| p2_upm_convergence | :154-155 | ok / null model（不写任何出参；`converged` 为**状态枚举** `0=max_iter / 1=converged / 2=stalled / 3=invalid`，旧模型文件未记录读作 0） | :1577-1589 |
| p2_upm_dense_read_block | :208-215 | ok / 参数 null·未知 frame_id·io/parse / **stale-cache（source hash 不匹配）** | :2201-2216；语义注释 upm.h:176-177 |
| p2_upm_materialize_dense_n | :219-220 | ok / null、tile_set 空、aio 失败 | :1756-1885 |
| p2_upm_component_gauges | :192-194 | ok / null model / ref_frame 容量不足 | :1735-1747 |

- 空输入链：n_obs=0 → rc=1（:215）；无 target_order（cfg.target_order<0
  且未显式给出）→ rc=1（:246-249，注释"空间 UPM 必须知道 control
  leaf 层级"）；
- frame 语义边界：frame_id 重复 → open 拒绝（:1078-1081）；参数行数
  ≠ frame 数 → save/open 拒绝（:945/:1181-1184）；
- 退化几何：无观测几何节点 sentinel 不入数据图（:217/:417-418）；
  断开分量各自 gauge（:22 注释/:415-491）；单帧区 continuation
  （upm.h:99-101）；
- 哨兵条款：**不可用一律 NaN**；哨兵面与合法值域互不相交——0.0 属合法值
  （gauge 参考帧合法 C=0）；`p2_upm_evaluate_c(nullptr,…)` 与未知 frame_id 同码。
  开放项：缺失 cell（tile 不在模型 control 图内）仍返回 0.0=无校正（调用契约要求
  frame+leaf 在覆盖域内；升级为显式错误需另行变更）。
- 收敛可观测：`p2_upm_build` 的 rc=0 **只**表示构建成功；"迭代耗尽"
  由 `p2_upm_convergence` 的 `converged=0` 报告；收敛结论以 `converged` 为准（rc=0 只表构建成功）。
- 错误粒度 = rc 二值/三值 + AIO 层 aio_upm_last_error 文本；编排层
  ACS_ERR 映射归 API-P2-001 编排面，不在本模块域。

## 11 Oracle（恢复真值面与等价 oracle）

- **恒等/线性面恢复 oracle**：合成 k·f 线性真值场 → UPM 恢复后
  evalrel(f)≈0.5f、maxdev<0.05（eng/tests/api/test_upm_recovery_oracle.py
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

## 12 TEST-DESIGN（TEST-P2-UPM-DESIGN 冻结）

**双语声明**：本节为设计冻结面 **VERIFIED**（引用既有测试源容差）；
可执行 TEST-P2-UPM-001/002 登记为 **MISSING**（归 P2-UPM-TEST 落地，
本文件不冒认执行态；先例=PHASE2_SAMPLER.md §11.3 双面登记）。每条
阈值已在既有测试源冻结，**TEST 任务一律照录该冻结值**：

| # | 设计面 | 冻结容差 | 现状测试锚 |
|---|---|---|---|
| T1 | 恒等/线性面恢复：线性真值 k·f → evalrel(f) | \|evalrel(f)−0.5f\|≤0.05（delta=0.05） | eng/tests/api/test_upm_recovery_oracle.py test_02 :101-109 |
| T1' | 参数恢复 maxdev | maxdev < 0.05 | 同文件 test_01 :93-99（驱动输出 :51-55） |
| T2 | 确定性：重复 build model_hash | 三次逐位一致（bit-exact） | 同文件 test_03 :112-119 |
| T3 | 并行等价：workers∈{1,2,4} 重复 build hash；1T/2T control_count | hash bit-exact（consistent=1）；count 精确相等 | eng/tests/api/test_upm_parallel.py test_01 :82-89（reps=4）、test_02 :91-101 |
| T4 | 资源：20 次 build/close 循环 RSS | 增长 < 1024 KB | 同文件 test_03 :103-110（rss_kb :26；driver :32-50） |
| T5 | dense/sparse 等价 | 1e-12（dense source hash 强校验） | UPM_SOLVER.md §8/§9 冻结面；dense_read_block :1542-1557 |
| T6 | 权重公式 UPMW-001..007 + raw_weight rc=2 门 | UPMW 判据 exact/double_eq（测试源冻结）；rc=2 exact | synthetic_gate.cpp:3915 起；upm.h:138-145 / upm.cpp:1966 |
| T7 | 退化链：单帧区 continuation / 断开分量 gauge / 空 obs rc=1 / 未知帧 rc=1 | rc exact；gauge frame id exact | upm.h:99-101；:415-491；:215；:1252 |

负面矩阵：null 参数 rc=1、坏路径 open rc=1、format 不符 rc=1、
frames 重复 rc=1、C 行数≠frame 数 rc=1、dense stale rc=2。fixture
由固定 seed 合成输入生成（测试源内嵌驱动），不提交大二进制。

## 13 容差与冻结清单（默认值实测）

**P2UpmBuildConfig 默认值**（`cfg_in` 缺省面 = upm.cpp:255-292 的整块零值回填；
两处生产组装 `p2_session.cpp:199-211` 与 `module_adapters.cpp:7795-7813` 均显式赋
`zero_anchor_weight=1e-3`、`sigma_floor=1e-3`）：

| 参数 | 默认 | 锚（upm.cpp / upm.h） |
|---|---|---|
| huber_delta | 1.345（无量纲，单位=sigma_eff） | upm.h: 74；upm.cpp:267/:286 |
| max_iterations | 100 | upm.h: 77；upm.cpp:270/:287 |
| tolerance | 1e-6 | upm.h: 81；upm.cpp:271/:291 |
| tolerance_relative | 0（=tolerance 作绝对量）；**生产两入口不一致，见 §14** | upm.h: 78；upm.cpp:284/:305 |
| sigma_floor | 1e-3 | upm.h: 83；upm.cpp:273/:292 |
| zero_anchor_weight（λ0） | 1e-3 | upm.h:76；upm.cpp:269/:297 |
| smoothing_lambda（λs） | 0.0（默认关闭平滑） | upm.h:75；upm.cpp:268/:299 |
| use_ivar_weight | 1（production；仅显式 0 进 ablation） | upm.h: 89；upm.cpp:276 |
| control_reliability | 1.0 | upm.h: 90；upm.cpp:277/:296 |
| cpu_workers | 1（串行 reference；生产=Runtime lease） | upm.h: 94；upm.cpp:278 |
| support_power | 1.0（仅 ablation 路径消费） | upm.h: 84；upm.cpp:274/:293 |
| gs_damping | 1.0（legacy）；生产 0.5 | upm.h: 101；upm.cpp:281/:302 |
| m_full_frame | 0（legacy）；生产 1 | upm.h: 104；upm.cpp:282/:303 |
| final_gauge | 0（legacy）；生产 1 | upm.h: 108；upm.cpp:283/:304 |

**数值常数**：CG max_cg=200、CG 早停 pAp≤1e-30 / rs_new<1e-24、
归一化门 s>0 && isfinite(s)、per-control sums 门 den>1e-12、`kChunk=16`、
`kLeafPx=512²`、跨 tile 邻接 link_rad=1.6×cell_dist、dense/sparse 等价 1e-12、
`kStallPatience=5`（upm.cpp:719）、`kObjImproveFloor=1e-12`（upm.cpp:720）。

**收敛判据的精确形式（唯一权威=代码，逐位核验）**：

```text
scale_obs = median(|value|)  （全部观测；非有限或 ≤0 → 0）      # upm.cpp:707-718
tol_M = tol_C = tolerance                                       # :1021-1022（legacy 绝对）
tolerance_relative=1 时: tol_M = tol_C = tolerance × max(scale_obs, 1.0)   # :1023-1027
converged = 1  ⇔ max_dM < tol_M ∧ max_dC < tol_C                # :1037-1040
converged = 3  ⇔ objective 非有限                                # :1033-1036
rel_improve = |obj − obj_prev| / max(|obj_prev|, 1e-300)         # :1029-1032
converged = 2  ⇔ rel_improve < 1e-12 连续 5 轮（kStallPatience）  # :1041-1050
converged = 0  ⇔ 迭代耗尽（max_iterations）                       # :1052
```

**分母口径（正向约束）**：分母**必须**取观测量尺度 `scale_obs`（|value| 的中位数），
**分母取值 = `scale_obs` 唯一**（`max|M|`/`max|C|` 只作对照）。`max(scale_obs,1.0)` 保证近零尺度输入退回绝对判据
（小尺度合成数据与 legacy 逐位等价）。**头文件注释漂移（登记项）**：`upm.h:80` 仍写
`scale = max|M| 或 max|C|`、`upm.h:147` 仍写 `tol_step`/`tol_obj`、`upm.h:111-112` 仍写
「生产显式 1 + 1e-3」——三处均与实现（`:1019-1027`）及生产装配（`tolerance=1e-6`）
不一致，**以代码与本节为准**。

**k_corr 代码默认 1.4（实现记录；公式面 = k_gauss(N_retained)×k_geo 几何查表；MC 实测 1.3883）**：`sampler.cpp:82` 注释
"k_corr_empirical = 1.3883，N_eff ≈ 181 < N_retained=251. 冻结保守值
1.4"；常量 `kControlCorrDefault=1.4`（`sampler.cpp:83`）。
**k_corr 属
sampler 域合同**（ALG-P2-SMP-001 §5.4 / ALG-UPM-CONTROL-IVAR-001，
PHASE2_SAMPLER.md 承载），本域只引用 control_ivar 消费面，不改不重复
标定。**本表数值与公式为冻结面：任何修改必须走 SCI/合同变更（放宽动作只随变更落地），不
在实现或测试内就地放宽。**

## 14 现状缺陷登记（DISP-P2UPM-001..004，登记不改码）

| ID | 锚 | 内容 | 整改去向 |
|---|---|---|---|
| DISP-P2UPM-001 | upm.h:167-168 与 :173-175 | `p2_upm_materialize_dense` **重复声明**（同头文件重复声明同一函数 C++ 合法、非 ODR 违例，运行无影响；纯合同卫生问题） | 头文件注释清理面 |
| DISP-P2UPM-002 | upm.h:92-94 | `cpu_workers` 注释漂移：前半句「CON-005 … 仅 P2_ENABLE_OPENMP 时并行 compute_raw/聚合」与实现不符（现无 OpenMP、std::thread 五段池，§8）；后半句 Runtime lease 语义正确 | 头文件注释清理面 |
| DISP-P2UPM-003 | p2_session.cpp:199-218 | upm 配置覆盖键仅 {max_iterations, huber_delta, smoothing_lambda}；`zero_anchor_weight` / `tolerance` / `tolerance_relative` 无 config 键（`upm.cpp:273-286` 只拦非法值，session 面不可配） | P2-SESSION-IMPL |
| DISP-P2UPM-004 | `lib/infrastructure/scheduler/src/module_adapters.cpp` descriptor 段 | descriptor 端口语义占位：fit 行 upm_model=可选输出、apply 行 upm_model=必选输入，与真实数据流（fit 进程内 build→persist 落盘 upm_sparse.json；apply/reload 经文件 + `p2_upm_open`）不符 | P2-XX-INT |
| DISP-P2UPM-005 | upm.h:78-80、:110-112、:143-149 | 收敛面**头文件注释三处漂移**：①`:80` 分母写 `max|M| 或 max|C|`，实现与生产均为 `scale_obs=median(|value|)`（`upm.cpp:707-718/:1024`）；②`:111-112` 写「生产显式 1 + 1e-3」，生产装配实为 `tolerance=1e-6`（`module_adapters.cpp:7808`）；③`:147` 写 `tol_step`/`tol_obj`，实现无此二字段（只有 `tolerance` + `tolerance_relative`） | 头文件注释清理面 |
| DISP-P2UPM-006 | `module_adapters.cpp:7808-7813` vs `lib/phase2_session/p2_session.cpp:204` | **两个生产入口的收敛判据口径不一致**：适配器 `tolerance=1e-6` ∧ `tolerance_relative=1`（相对）；session `tolerance=1e-6` 且未设 `tolerance_relative`（零初始化 ⇒ 0 ⇒ 绝对）。同段注释（`:7797-7808` 要求回退冻结绝对容差、相对判据待裁决）与 `:7809-7813`（以「定案」名义启用相对判据）**对同一变更的授权状态表述互斥** | 待裁决（§16.3 同项） |
| DISP-P2UPM-007 | upm.h:282-283 | 观测结构体字段单位注释写 `单位 ADU` / `单位 ADU^-2`，与 SCI §3 冻结面（面亮度 **ADU·sr⁻¹** / **(ADU·sr⁻¹)⁻²**）及上游写盘 BUNIT 冻结集不一致 | 头文件注释清理面 |

登记原则：本域只登记不改码；头文件注释类漂移（001/002/005/007）整改编入头文件注释清理面，
003 归 P2-SESSION-IMPL，004 归 P2-XX-INT 对齐，006 待定案。

## 15 消费链（生产编排与 apply/reload 面）

- **编排（fit+persist）**：lib/phase2_session/p2_session.cpp:180-233
  upm_build+persist 段——生产 cfg 组装 :187-195（含
  uc.cpu_workers = s->host->budget.max_workers，:195，Runtime lease
  唯一来源；sampler 同源 :155）；config 覆盖键 ：196-202（三键，
  DISP-P2UPM-003）；p2_upm_build 调用 ：204；info 上账 ：206-219；
  persist 段 ：221-233（persist_upm+upm_save_path 门控、失败
  ACS_ERR_IO）；
- **dense 物化**：lib/algorithms/coverage/tools/stage2.cpp:470-495——
  p2_upm_save :472 → p2_upm_materialize_dense_n :481-483（workers=
  effective_cpu_workers(cfg.exec)，与积分一致）；
- **apply 面**：stage2.cpp 经 p2_upm_calibrate_block（stage2.cpp:1158 与 :1516
  两处逐块校准；进程内 model 直通，无二次 open）；
- **reload 消费（p2_upm_open）**：lib/algorithms/coverage/tools/
  calibrated_pair_diag.cpp:205（诊断工具读取 upm_sparse.json +
  p2_upm_calibrate_block :337 注释锚）；测试面 synthetic_gate.cpp
  多处 save→open 幂等门（:325/:380/:391/:1824/:2066/:2257/:2335/
  :5007/:5019/:5082）；
- **descriptor 面**：astrocs.phase2.upm-fit / astrocs.phase2.upm-apply
  （module_adapters.cpp:665-698；节点链 coverage → sample → upm_fit
  → upm_apply → reject → integrate → write，:557 注释）——端口语义
  占位见 DISP-P2UPM-004。

## 16 关联 ID 映射（本文件承接）

- `ALG-P2-UPM-IMPL-001` = 本文档整体（逐符号锚 §3/§6；实现级合同）。
- 上游：SCI-UPM-001（FROZEN 集合零改动，§5）；ALG-UPM-001
  （docs/science/algorithms/UPM_SOLVER.md，推导权威，本批原位修订）；ALG-
  UPM-CONTROL-IVAR-001（PHASE2_SAMPLER.md §5.4/§12 承载）；ALG-P2-
  SMP-001（obs/frame_id 上游）。
- 下游 DATA：DATA-P2-UPM（fit 产物）/ DATA-P2-COR（apply 产物）
  （descriptor data_id :612/:632；矩阵行 ：21/:22）。
- API 面：**API-P2-UPM-001**（矩阵/descriptor 词汇；PUBLIC_API.md
  尚未落 upm 节，本文件只登记词汇、不冒认条目存在）。
- TEST：TEST-P2-UPM-001（fit 面）/ TEST-P2-UPM-002（apply 面）
  ——设计冻结=本文档 §12；可执行 MISSING 归 P2-UPM-TEST。
- MOD：astrocs.p2.upm（fit/apply 两模块页
  docs/detail/registry/astrocs.phase2.upm-fit.md /
  astrocs.phase2.upm-apply.md）。

## 17 追溯

- 矩阵行：MOD-astrocs-phase2-upm-fit（TRACEABILITY_MATRIX.csv:22，
  SCI-P2-UPM-001/ALG-P2-UPM-001/DATA-P2-UPM/TEST-P2-UPM-001）与
  MOD-astrocs-phase2-upm-apply（:21，SCI-P2-UPM-002/ALG-P2-UPM-002/
  DATA-P2-COR/TEST-P2-UPM-002）。
- 占位 ID 与本文件关系：矩阵 algorithm_id=ALG-P2-UPM-001/002 为
  descriptor 占位词汇，其语义由 §5 映射声明分解为 ALG-UPM-001
  （推导，UPM_SOLVER.md）+ ALG-P2-UPM-IMPL-001（本文件实现合同）；
  占位 ID 本身不注册 INDEX、不入合同（与 PHASE2_SAMPLER.md §12 同构）。
- ALG-UPM-001（UPM_SOLVER.md）与本文档同步登记：行号/并行表述按实测，
  公式与容差零改动。

## 参考文献与参考代码库（含许可证）


- Huber IRLS：Huber 1964, Ann. Math. Statist. 35, 73；Huber & Ronchetti 2009, Robust Statistics 2nd ed., Wiley。
- 弱零锚/正则化：Tikhonov 1963, Soviet Math. Dokl. 4, 1035（卷页需网络核验）。
- 多帧相对定标：SCAMP（GPL-3.0；Bertin 2006, ASPC 351, 112）；Padmanabhan et al. 2008, ApJ 674, 1217。
- 稀疏天光面样条（目标表示）：Duchon 1977；Wahba 1990。
- 共轭梯度（C 更新）：Hestenes & Stiefel 1952, J. Res. NBS 49, 409。
- var(median)≈πσ²/(2N)：Hoaglin et al. 1983。

参考代码库（含许可证）正本 = docs/engineering/SCIENTIFIC_REFERENCES.md §M。


---

## 收敛容差与报告字段（现行登记）

- **收敛配置面只有两个字段**：`tolerance`（默认 1e-6）与 `tolerance_relative`（默认 0）。**不存在** `tol_step`/`tol_obj`/`tolerance_obj` 字段（`upm.h:77-81/:113`、`upm.cpp:1013-1027`）。
- **判据（逐位）**：`tolerance_relative=0` ⇒ `max_dM < tolerance ∧ max_dC < tolerance`；`tolerance_relative=1` ⇒ `max_dM < tolerance·max(scale_obs,1) ∧ max_dC < tolerance·max(scale_obs,1)`，`scale_obs = median(|value|)`。目标相对改善 `|Δobj|/max(|obj_old|,1e-300) < 1e-12` 连续 5 轮 ⇒ `converged=2`（stalled）；目标非有限 ⇒ `converged=3`。
- **`converged` 状态枚举**：`0=max_iter / 1=converged / 2=stalled / 3=invalid`（`upm.cpp:1028-1052`，只读访问器 `p2_upm_convergence` `:1577-1589`；模型文件未记录读作 0 = 未证明收敛）。
- **求解入口四参数须登记**：`gs_damping`、`m_full_frame`、`final_gauge`、`tolerance_relative`（缺省取 legacy 值，生产装配显式启用；`upm.cpp:266-271/:287-292`）。
- **参考通量报告字段**：`reference_flux_spread_rel` / `reference_flux_spread_gate` / `reference_flux_spread_noncommon`。逐帧 `F_ref,k` 的配对性只在**同一帧内**成立；**组间一致不是门**。

