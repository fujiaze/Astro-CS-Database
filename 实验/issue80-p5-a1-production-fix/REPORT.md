# Issue #80 · P0-A1 生产修复报告：Q-only 锚逆修正 → 精确 deflation + 命名失败语义

**状态**：`PRODUCTION_FIX_LANDED`（源码 + 独立 oracle + 真实生产 A/B 全部执行）；
真实 2×Hα+1×Red 混通带输入的**数值裁定不变（仍必须失败）**，但失败语义由未命名
`rc=6 not SPD` 变为具名 `rc=7 UNIDENTIFIABLE/NOT_RESOLVABLE` + 可读矩阵读数；
**未**宣称 P5 科学闭合，**未**触碰根因（跨代光度身份 / 跨通带响应模型）。

**范围**：`sky_plane.cpp` 生产可达分支（罚法方程求解）+ 开发态只读取证 + 单元测试 + oracle。
未加任意 ridge、未放宽容差、未丢像元、未改判决阈值、未换锚帧规则、未改科学产品定义。

**交付身份**：
- 基线 = `main@9682b598050a24dbc4ab90d67601984d93829e6f`（修复前反例构建于干净 HEAD 克隆）。
- 构建隔离：本工作树存在**并发就地修改**（`lib/algorithms/resample/p3_*`、`p3_output.*`、
  `module_adapters.cpp`、`p3_session.cpp`、`aio_hips_reader.*`，非本单写面且在编辑中不可编译）。
  为不干扰并发写者、也不在脏树上取数，本单全部构建/运行在 `git clone --shared` 的隔离副本中
  （源码内容 = HEAD + 本单写面），修复前反例另用干净 HEAD 副本。未 stash/未回退任何他人改动。

---

## 0. TL;DR

| 项 | 修复前（`9682b5980`） | 修复后 |
|---|---|---|
| 真实 `abc_real`（2Hα+Red） | exit=2，`rc=6`，末因 `reduced normal matrix not SPD` | exit=2，`rc=7`，末因 `reduced normal matrix does not determine the solution: … not resolvable in double (status=3 branch=1 n_free=25 mq=4 λ=1.695e2 α=2.034e3 chol_fail_col=0 pivot=-1.35e-6 backward_error=nan gate=3.83e-12)` |
| 同通带可解对照（`ab_2halpha`） | exit=0，`p2_sky_plane.bin` sha `1864fae7…` | exit=0，**同一 sha 逐位相同** |
| 同尺度三帧（`def_samefilter`）、全红标签标签臂、单位归一臂 | exit=0 | exit=0，`p2_sky_plane.bin` **逐位相同** |
| 仅改响应标签臂（`abc_label_swap_all_halpha`） | exit=2，`rc=6` | exit=2，`rc=7`（同一具名原因） |
| `B = B_a + Q·G⁻¹·(Qᵀb − QᵀH_red B_a)`（旧 Q-only） | 一般代数错误；同矩阵反例残差 1e-3 量级 | 被替换为精确式 `(I−αR)w = αc, B = B_a + Xw`（`X=A_α⁻¹Q`）；同矩阵残差 ≤4.7e-15（对拍独立 QR/SVD/Woodbury） |
| 生产失败路径可读性 | 全丢（无矩阵/主元/分量读数） | 开发态确定性 dump（`mosaic.sky.normal_matrix`）+ 失败文本带 status/branch/主元/后向误差/门 |

---

## 1. 修复前反例（干净 HEAD 的**真实生产** C++ 反例）

命令（生产 CLI，单线程，冻结真本只读）：

```bash
OMP_NUM_THREADS=1 <acsd> mosaic --json <cfg> -y     # 输入 = /workspace/issue80-scratch/frozen/*
```

| 臂 | 输入 | exit | `[sky_plane]` 末因 |
| --- | --- | --- | --- |
| `abc_real` | A/B=Hα 600 s（`photappl=0,photscal=1`）+ C=Red 300 s（`photappl=1,photscal=5.702e-17`） | **2** | `rc=6 … reduced normal matrix not SPD` |
| `abc_label_swap_all_halpha` | 同上，仅把 C 的 `obs_filter` 改成 Hα | **2** | `rc=6 …`（同一冻结数值载荷） |
| `ab_2halpha` | A+B 两帧 Hα | 0 | ok n_used=11456 n_nodes=25 |
| `abc_label_swap_all_red` | 三帧标签全 Red（数值不变） | 0 | ok n_used=17237 n_nodes=30 |
| `def_samefilter_red_3frame` | M42_M2 真实三帧同滤镜 Red | 0 | ok n_used=17266 n_nodes=30 |
| `abc_photscal_harmonized` | `abc_real` 仅做 signal×c、variance×c²、ivar÷c²（单位换算） | 0 | ok n_used=17237 n_nodes=30 |

读法与 E0 一致：**同一数值载荷只改名义标签即可翻转 PASS/FAIL** ⇒ 失败由尺度/数值决定，
不是"任意三帧必可辨识"。证据：`evidence/prefix_clean_head/{arms.json,*.stderr}`
（构建于干净 HEAD 的隔离副本，与本单改动完全无关；`abc_real` 与
`abc_label_swap_all_halpha` 均 `rc=6 not SPD`，其余四臂 exit=0）。

---

## 2. 修复内容

### 2.1 唯一求解实现 `sky_penalized_solve`（`sky_plane.cpp`）

```
直接 Cholesky(A) 成功                     → 解（与历史逐位相同；只记后向误差，不设门）
否则锚 A_α = A + α Q Qᵀ（α = max>0 diag(λP)）
   chol(A_α) 失败                          → NOT_IDENTIFIABLE：候选惩罚零空间基之外仍有非正曲率
   否则 B_a = A_α⁻¹b, X = A_α⁻¹Q, R = QᵀX, c = QᵀB_a, M = I − αR
        chol(M) 失败                       → NOT_IDENTIFIABLE：(I−αR) 奇异 ⇒ A 在 span(Q) 上不可分辨
        否则 w = M⁻¹αc，B = B_a + Xw
             后向误差 η = ‖b−AB‖∞/(‖A‖∞‖B‖∞+‖b‖∞) > τ_floor → NOT_IDENTIFIABLE
             否则                               → 解（OK_WOODBURY）
```

- **数学更正**：旧式假定 `H_red Q ⊆ span(Q)`；一般 `H_red` 与 `Q` 耦合时留 Q⊥ 残差
  `b − AB = αQQᵀB_a − H_red Q t ≠ 0`。新式由 `A_α B = b + αQQᵀB` 的不动点精确导出：
  `B = B_a + X w`，`(I − αR) w = α c`，**恒有 `AB = b`（在 (I−αR) 可解时）**，与 A 是否奇异无关。
  与 S3/S3R 的独立 Woodbury 参考同式（`Aq = A_α⁻¹Q; tw = (I−αQᵀAq)⁻¹αQᵀB_a; B = B_a + Aq·tw`）。
- **回退判据有定理支撑**：A ≻ 0 ⇒ `M = (I + αQᵀA⁻¹Q)⁻¹`，特征值 ∈ (0,1]；A 在 span(Q) 上奇异
  ⇒ M 有零特征值；A 不定 ⇒ M 不定。故 `chol(M)` 正是"A 在算术可分辨意义下正定"的判据，
  **它把 `ker D`（惩罚零空间）与 `ker A`/物理 gauge 分开**。
- **后向误差门**：`τ_floor = max(n_obs, n_free)·ε`（复用 `p2_identifiability_rank_rtol_floor`，
  本仓唯一由算术推出的阈值），不引入新的标定常数。
- **failing-pivot 读数**：`chol_spd` 增加可空 `fail_col/fail_pivot` 出参（不改数值路径）。
- **候选惩罚零空间基**：单 stencil 连通块 = 全局 `{1,ix,iy,ix·iy}`（4 列，与历史**逐位一致**）；
  支撑被 mask 切成 C 块时按块补多项式列（4 列/块，MGS 去相关，成本上限 64 列），
  因为此时真实 `ker(D)` 维数 >4，全局 4 列只是**候选子集**（S3-R：缺整行/双孤岛 nullity=8）。
  另加 union-find stencil 连通块诊断（dumped）。
- **失败语义**：求解失败返回 `P2_SKY_PLANE_NOT_IDENTIFIABLE(7)`（原为 `RANK_DEFICIENT(6)`），
  错误文本具名原因 + status/branch/n_free/mq/λ/α/失败列/主元/后向误差/门。自适应回路对
  6/7 都按"可放粗"处理，**行为不变**，只把名字改对。

### 2.2 开发态只读取证（DEV_MODE.md §2/§5/§6）

- 新探针 `mosaic.sky.normal_matrix`（注册于 `dev_diagnostics.cpp` 白名单），
  由 `ACSD_DEV_SELECTOR` 的 `snapshots.probe_ids` 选择，落
  `<work_root>/_dev_diagnostics/<run_id>/snapshots/mosaic.sky.normal_matrix/iter_<n>/`。
- `sky_plane.cpp` 内 `#if ACSD_DEV_DIAGNOSTICS` 段的 payload：identity/网格、
  active-node 掩膜与 `free_of_full` 索引、stencil 行（可重算 D）、逐帧权重汇总（min/median/max/
  负/零/非有限）、λ/α、H_red 对角/对称残差、判据谱（H_red/H_solve）、四个主元与失败列、
  原方程后向误差、各矩阵 sha256；`n_free ≤ 128` 时另落稠密矩阵本体（H_data/H_red/H_solve/rhs/Q），
  `n_free ≤ 256` 时落判据谱。**成功与失败路径都落，不改变任何数值/控制流/返回值。**
- CMake：`acsd_phase2` 增加链接 `acsd_dev_diagnostics`（OFF = INTERFACE-only，
  仅 include 面；ON = `PUBLIC ACSD_DEV_MODE=1` 沿链接闭包传入本 target）。
  这是 DEV_MODE.md §2「同一开关控制编译边界」的最小接线；`ACSD_DEV_MODE=OFF` 时
  dump 段不编译（无符号、无文件、无 env 读取）。

---

## 3. 生产真实矩阵读数（`abc_real`，`iter=0`，冻结输入）

来源：`evidence/matrix_dump_abc_real_iter0.json`（开发二进制一次运行产出）。

| 量 | 值 |
| --- | --- |
| 网格 / 自由节点 | nx=5, ny=6, n_full=30, **n_free=25**（5 个角节点被数据支撑门剔除） |
| stencil 连通块 / mq | **1** 块 / **mq=4**（D 的 4 个奇异值 ~5e-16…7e-17 ⇒ nullity=4，Q 恰为 ker D） |
| λ_eff / α | 169.48966496122742 / 2033.87890625 |
| H_red 对角 | min 1.4818e9 … max 1.1021e13；对称残差 rel 7.09e-16 |
| 求解权重 `w = base_w·huber` | median 1.4653e-21 … max 3.4629e11（跨度 ~2.4e32，与 E0 的 1.405e32 同量级） |
| 直接 Cholesky | 失败列 0，主元 −1.3495e-06 |
| 判据（H_red，τ=1e-10） | `rank_eff=19/25`，`n_unidentified=6`，`identifiable=0` |
| 生产分支 | branch=1（锚定+deflation），status=3 `NOT_RESOLVABLE` |

**独立 oracle 同一矩阵**（`scipy` pivoted QR + `gelsd` SVD + 谱）：

| 量 | 独立读数 |
| --- | --- |
| `A = H_red + λDᵀD` 重建 | 与生产 `H_solve` 最大绝对差 1.95e-3（相对 ~2e-16）⇒ 装配语义核对通过 |
| `D Q` / `QᵀQ−I` | 5.6e-17 / 9.5e-16 ⇒ Q 是合法惩罚零空间候选基 |
| `λ_min(A)` / `λ_max(A)` | **−0.029783** / 1.311852e13；τ=1e-10 下 rank **22/25** |
| pivoted QR | `RANK_DEFICIENT_NO_UNIQUE_QR`，rank=22（**拒绝**给唯一解） |
| gelsd SVD | rank=22，最小奇异值 0.0343 / 0.0300 / 4.90e-4（相对 1.3e13 ⇒ 3 个方向低于 τ） |
| `M = I−αQᵀA_α⁻¹Q` | λ_min **−1.4759e-05**、λ_max 0.9999943、`|λ|_min` 3.01e-07 ⇒ **M 不定** |
| 旧 Q-only 公式（同一 A,b） | `‖AB−b‖∞ = 1.4561e+01`（相对 η 1.46e-12）⇒ 旧式确实不满足原方程 |
| 新 deflation（Python 独立式） | `‖AB−b‖∞ = 3.7575e-04`，η 3.74e-17；但 M 非正定 ⇒ 生产**拒绝**采用 |

结论：`(A 不定) ⟺ (M 不定)` 在该真实矩阵上由独立谱直接见证（M 的最小特征值 −1.5e-5，
不是舍入级），生产 `rc=7 / status=3` 与独立 oracle 一致；**没有任何"换锚成功后当作成立解"
的路径**（原 E0 的 Red-锚"成功臂"在本修复下不会被当作跨通带共同天空面）。

---

## 4. 独立 oracle 与单元测试（四个规定矩阵 + 反例）

单元测试 `eng/tests/unit/sky_plane_deflation_test.cpp`（经
`acsd_sky_plane_penalized_solve_for_test` 驱动**生产同一实现**）+ 独立 Python
`a1_penalized_oracle.py`（scipy 真实 pivoted QR / gelsd SVD / 增广设计 / Woodbury / 旧式）。

| case | n_free | rank(D) | nullity(D) | mq | 独立 pivoted QR/gelsd/增广 残差 | 生产 deflation 残差 | 旧 Q-only 残差 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `full_7x6` | 42 | 38 | 4 | 4 | ≤6.3e-16 | 1.13e-16 | **1.15e-03** |
| `missing_full_row_7x6` | 35 | 27 | 8 | **8** | ≤(H≻0) ~1e-16 | 7.46e-17 | **1.50e-03** |
| `two_3x3_islands_7x3` | 18 | 10 | 8 | **8** | ~1e-16 | 1.46e-16 | **2.30e-03** |
| `single_hole_7x6` | 41 | 37 | 4 | 4 | ≤6.5e-16 | 9.89e-17 | **5.43e-04** |
| `full_7x6_singular_on_kerD`（A 精确奇异于 span Q） | 42 | 38 | 4 | 4 | pivoted QR **拒绝** | 生产 status=3（拒绝） | 5.63e-17 |
| `full_7x6_spd_forced_anchor`（A ≻ 0，强制锚定分支） | 42 | 38 | 4 | 4 | ≤6.3e-16 | 2.21e-16 | **9.79e-04** |
| `production_abc_real_iter0`（真实 dump） | 25 | 21 | 4 | 4 | QR/gelsd **rank 22/25** | 生产 status=3（拒绝） | 1.46e+01 (KKT) |

- `rank(D)`/`nullity(D)` 由测试内**独立全主元 Gauss 消元**重算，与 S3-R 独立结论
  （42/38/4、18/10/8、35/27/8、41/37/4）逐例相同。
- `full_7x6_spd_forced_anchor`（A≻0、强制走锚定分支）：deflation 参数与直接解相对差 0（<1e-10 门），
  原方程后向误差 ≤1e-12，**旧式在同一 A,b 上 9.79e-04** ⇒ 修复的直接证据。
- `singular_on_kerD`：直接分支与强制锚定分支都报 `NOT_RESOLVABLE(3)`，**不产生可信解**；
  同时反证"只看后向误差不够"——A 奇异时 `b ∈ range(A)` 也能给出小残差解，必须由 `chol(M)` 判别。
- P0-A1 要求的跨语言对拍：C++ 强制锚定解 vs Python 独立 Woodbury 相对差 ≤4.7e-15（逐例）。

**独立 oracle 判读**：`verdict=PASS`，`checks=94`，`failed=0`（`evidence/oracle_readings.json`；
含 `A == H+λDᵀD`、`DQ=0`、`QᵀQ=I`、`(A≻0)⟺(M≻0)`、秩亏时 pivoted QR 必须拒绝、
C++ 与 oracle 解一致、生产报 OK 时自报后向误差必 ≤ 门）。

**C++ 单元测试**：release 与 dev 两种构建各 `6 cases / 0 failing assertions`（带真实 dump 时 7 cases）。

---

## 5. 真实生产 A/B（六臂，修复前 vs 修复后）

| 臂 | 修复前 exit / 末因 | 修复后 exit / 末因 | `p2_sky_plane.bin` | `p2_final.json` 唯一差异 |
| --- | --- | --- | --- | --- |
| `abc_real` | 2 / `rc=6 not SPD` | 2 / `rc=7` 具名 | 两臂都不产出（失败路径不写） | — |
| `abc_label_swap_all_halpha` | 2 / `rc=6 not SPD` | 2 / `rc=7` 具名 | 两臂都不产出 | — |
| `ab_2halpha` | 0 / ok | 0 / ok | **逐位相同** | 仅绝对输出路径 4 键 |
| `abc_label_swap_all_red` | 0 / ok | 0 / ok | **逐位相同** | 仅绝对输出路径 4 键 |
| `def_samefilter_red_3frame` | 0 / ok | 0 / ok | **逐位相同** | 仅绝对输出路径 4 键 |
| `abc_photscal_harmonized` | 0 / ok | 0 / ok | **逐位相同** | 仅绝对输出路径 4 键 |

⇒ **常规可解档科学解逐位不变**（`p2_sky_plane.bin` sha 相同；`p2_final.json` 差异只有
`hips_root`/`properties`/两个 `phase2_audit` 绝对路径键）；解码路径全部走直接 Cholesky 分支。

**DEV_MODE ON/OFF 等价**（DEV_MODE.md §7）：dev 二进制对六臂的 exit 与 `p2_sky_plane.bin`
与 release 完全一致（唯一额外差异 = `_dev_diagnostics/` 目录的文件计数）；
`cxx_readings_sci.json` 与 `cxx_readings_dev.json` **sha256 完全相同**
（`2bc0c556…`）⇒ 打开诊断不改变数值路径。

**冻结真本只读校验**：E0 `input_index.json` 记录的 6 帧共 **4476 个文件**逐个 sha256 复核，
**0 处改动**（`evidence/input_identity_recheck.json`）。

---

## 6. 裁定（对应 #80 要求的三种判读）

| 判读 | 结论 | 依据 |
| --- | --- | --- |
| `no longer reproduced` | ✗ | 干净 HEAD 上六臂复现（§1） |
| `rank-deficient valid`（真实秩亏/gauge） | **✓（生产判据口径）** | H_red 列均衡判据 **19/25**；A 线性代数秩 **22/25**；独立 pivoted QR 拒绝唯一解；D nullity=4 且 Q 恰为 ker D ⇒ 缺的不是 Q 这一维 |
| `numeric defect` | **✓（机制，已定位到可判读的失败语义）** | A 出现绝对量级 −0.0298 的负特征值（相对 −2.3e-16）；`M` 不定（−1.48e-5）；旧式在同一 A,b 上 KKT=14.6 |
| `model invalid across filters/units` | **✓（主因，未变）** | 权重跨度 2.4e32；`photappl/photscal` 跨代不一致；Hα 与 Baader R 无共同响应/SED 变换 |

**本轮不再声称"求解器存在一般性错误"**：同尺度臂全部逐位不变；**但确实存在一个一般性代数错误
（旧 Q-only 锚逆修正）并已修复**，且失败路径现在可判读。

---

## 7. 明确未证 / 未决（不得误读）

- **未**让 2×Hα+1×Red 变成可解，也**不应**如此：真实输入的跨代光度身份与跨通带响应模型
  是根因（E0 C1/C3）。本修复只保证"要么给出满足原方程的解，要么具名失败"，不制造可辨识性。
- **未**改判决阈值、未改锚帧选择规则（C4 仍开放）、未改 `h==upper` 无放粗退路（C5 仍开放）。
- **未**证明 `Q` 覆盖任意 mask 下 `ker(D)` 的全部维数：只证并实现了"按 stencil 连通块补多项式列"
  且成本上限内生效；超出上限时退回全局 4 列并由 `chol(A_α)` 如实报覆盖不足。
- **未**做 Stage2→Stage3 端到端"恢复"验证（无一个可恢复的混通带输入；同尺度臂逐位不变已覆盖回归面）。
- **未**重跑 #76 的乘加联合研究，也**未**把它当生产已装。
- 生产真矩阵 dump 只在 `ACSD_DEV_MODE=ON` 二进制中存在；release 二进制**无**该路径
  （`ACSD_DEV_MODE=OFF` 不编译、无符号、无 env 读取）。

---

## 8. 产物与复跑

**代码**
| 路径 | 改动 |
| --- | --- |
| `lib/algorithms/coverage/src/sky_plane.cpp` | 求解器重写（精确 deflation + 后向误差门 + 命名失败 + 候选基扩展 + failing-pivot 读数）、开发态 dump、测试接缝 |
| `lib/algorithms/coverage/src/sky_plane_test_hooks.h` | 新增：内部测试接缝（非公开 ABI） |
| `lib/infrastructure/observability/probes/src/dev_diagnostics.cpp` | 注册 `mosaic.sky.normal_matrix` |
| `CMakeLists.txt` | `acsd_phase2` 链接 `acsd_dev_diagnostics`（同一主开关） |
| `eng/tests/unit/sky_plane_deflation_test.cpp`、`run_sky_plane_deflation_test.py` | 新增单元测试与运行器 |
| `docs/science/algorithms/mosaic/PHASE2_UPM_IMPL.md`、`docs/science/sky/UPM.md` | 数值方法与失败语义正本更新 |

**本单产物**（`实验/issue80-p5-a1-production-fix/`）
```
code/{run_arms.py, a1_penalized_oracle.py, summarize_evidence.py, sync_to_clone.sh}
evidence/prefix_clean_head/          # 修复前真实生产 A/B（干净 HEAD 隔离副本构建）
evidence/postfix_release/           # 修复后 release 六臂
evidence/postfix_dev/               # 修复后 dev 六臂（DEV_MODE 等价性）
evidence/devdiag_abcreal/           # 开发二进制 + ACSD_DEV_SELECTOR 运行
evidence/matrix_dump_abc_real_iter0.json
evidence/cxx_readings_all.json      # C++ 单测导出的同一批算子 + 生产读数
evidence/oracle_readings.json       # 独立 oracle 逐项读数 + verdict
evidence/{input_identity_recheck,ab_no_regression,dev_mode_equivalence}.json
evidence/dev_selector.json
```

**复跑**

```bash
# 0) 隔离副本（本工作树有并发 WIP，不在脏树上取数）
git clone --shared --no-checkout "/workspace/Astro CS Database" /workspace/acsd-a1-clone
cd /workspace/acsd-a1-clone && git checkout HEAD
bash 实验/issue80-p5-a1-production-fix/code/sync_to_clone.sh /workspace/acsd-a1-clone
cmake -S . -B build/sci -G Ninja -DCMAKE_BUILD_TYPE=Release -DACSD_DEV_MODE=OFF && ninja -C build/sci
cmake -S . -B build/dev -G Ninja -DCMAKE_BUILD_TYPE=Release -DACSD_DEV_MODE=ON  && ninja -C build/dev

# 1) 单元/正式 C++ 测试（生产同一实现）
python3 eng/tests/unit/run_sky_plane_deflation_test.py --build-dir build/sci --out /tmp/readings.json
python3 eng/tests/unit/run_sky_plane_deflation_test.py --build-dir build/sci \
    --dump 实验/issue80-p5-a1-production-fix/evidence/matrix_dump_abc_real_iter0.json \
    --out 实验/issue80-p5-a1-production-fix/evidence/cxx_readings_all.json

# 2) 独立 oracle（pivoted QR / gelsd SVD / 增广设计 / Woodbury / 旧式反例）
python3 实验/issue80-p5-a1-production-fix/code/a1_penalized_oracle.py \
    --cases 实验/issue80-p5-a1-production-fix/evidence/cxx_readings_all.json \
    --out   实验/issue80-p5-a1-production-fix/evidence/oracle_readings.json

# 3) 开发态取证（dump）
ACSD_DEV_SELECTOR=实验/issue80-p5-a1-production-fix/evidence/dev_selector.json \
python3 实验/issue80-p5-a1-production-fix/code/run_arms.py \
    --acsd /workspace/acsd-a1-clone/build/dev/acsd --label devdiag --arms abc_real --run-root runs_a1 \
    --env ACSD_DEV_SELECTOR=实验/issue80-p5-a1-production-fix/evidence/dev_selector.json

# 4) 真实六臂 A/B + 汇总
python3 实验/issue80-p5-a1-production-fix/code/run_arms.py --acsd /workspace/acsd-a1-clone/build/sci/acsd \
    --label postfix_release --arms abc_real,ab_2halpha,abc_label_swap_all_halpha,abc_label_swap_all_red,def_samefilter_red_3frame,abc_photscal_harmonized
python3 实验/issue80-p5-a1-production-fix/code/summarize_evidence.py
```

---

## 9. 交付边界

- 生产科学产品定义、判决阈值（τ=1e-10）、gauge 规则、权重/方差链、输出集合**均未改变**。
- 未加 ridge、未放宽容差、未丢像元、未换锚帧、未禁用跨响应输入。
- 真实观测 FITS 只在 `/workspace/issue80-scratch` 以硬链接/受控拷贝存在，未入 Git；
  本单只入库 sha256 索引与轻量 JSON。
- 状态：`PRODUCTION_FIX_LANDED / REAL_AB_VERIFIED / CROSS_PASSBAND_MODEL_STILL_OPEN`。
  `#80` 科学仍未闭合（C1/C3 属 P1 光度身份与 #14 响应模型）。
