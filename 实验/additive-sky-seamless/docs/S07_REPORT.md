# S07 报告：P5 乘性增益研究与非退化测量

> 分支 `audit/acsd-science-docs-p5`；本单元**开工时 HEAD** `a73f577b0`（基线 `e31b2be66`）。
> 实验运行时刻 HEAD `c93b4a6f9`（期间其它轮次并发提交）；聚合时刻 HEAD 见证据 JSON 的
> `head_at_bundle_time`。`a73f577b0..head_at_bundle_time` 对 `lib/**` **零改动**
> （`git diff --stat a73f577b0..HEAD -- lib/` 为空）⇒ 接线审计与探针构建对当前 HEAD 同样有效。
> **本单元未做任何 git 写操作**（`git_write_performed:false`），改动留工作树由 Lead 复核提交。

## 0 结论先行

| 命题 | 判定 | 证据 |
|---|---|---|
| P5 生产链是否施加帧间残余乘性增益 | **IDENTITY**（行为位）+ **NOT_EVALUATED**（状态位） | `multiplicative_gain_applied` 全仓 **1 处读、0 处写**；读点后 `in.gain = nullptr` |
| MA 标量乘加求解器是否在生产调用路径上 | **NOT_IMPLEMENTED_ON_PRODUCTION_PATH** | 调用者只有工具面与一个**全仓零调用者**的 wiring 函数 |
| P1 的 `k_photo·m(x,y)` 是否被用 | **APPLIED**（Phase1 photometry 节点，逐帧 in-place，缺标度 fail-closed） | `apply_photometry_spatial` 生产调用点 |
| 独立匹配恒星通量比能否给出可默认启用的逐帧标量增益 | **不能**（本轮 `PASS=false`，生产默认**恒等**） | 阴性世界 10/10 seed 出现显著伪增益；估计量与孔径强相关 |

**一句话**：把增益证据从「背景梯度线性回归」换成「独立匹配恒星的扣背景积分通量比」之后，
阳性世界确实可检出、可解释（模型留出残差 `0.2069 → 0.0427` dex，中位比 `0.205`），
但**阴性世界（真值 `g≡1`）同样给出 10/10 seed 的显著伪增益**（`|η̂|` 中位 `0.0208` dex，
`χ²(η=0)` 拒绝率 `1.0`），且该伪增益的**孔径依赖达 `0.0298` dex**（是它自身幅度的 1.4 倍）
⇒ 该「增益」主要在吸收逐帧 PSF/孔径系统项，不是乘性天光差。
按 D-01 §4「阴性世界出现假阳性即不得默认启用 G」，判 **不通过**，生产保持恒等。

## 1 先读文档与关键口径（实际读过）

| 文档 | 取到的关键口径 |
|---|---|
| `实验/engineering-evidence/acsd-science-docs-p5/DEEPSEEK_SCIENCE_EXECUTION.md` S07 节 + 验收格式节 | 每帧残余**标量** `g_i=exp(η_i)`；证据必须是独立匹配恒星的扣背景积分通量比，**不是**背景梯度斜率；固定阴性/阳性世界、四臂（A-only/A+G/feather-only/无校正）；阴性假阳性即不得默认启用；二维 `g_i(x)` 本轮不上线；旧报告 `114/196` 的 82 条分原因、缺失不是零；产物三态 `NOT_EVALUATED/IDENTITY/APPLIED` + `NOT_IMPLEMENTED` |
| `REVIEW_DECISIONS.md` D-01/U-01（逐条）、D-02、M4 | 上线默认恒等、标 `NOT_APPLIED`；不得把文档能力写成已交付；`multiplicative_gain_applied` 有读无写视为**状态缺陷**，不以无记录视为已应用；`(I−a)/g` 与 `V/g²`；逐分量独立规范；历史「阴性 8/10 也改善」必须判不通过 |
| `docs/science/sky/UPM.md` §1/§2/§3/§4/§5/§6 | 加性可辨识、共模不可恢复；「无证据、尚未评估或未具生产施加路径时保持恒等并登记相应状态，**不把恒等规范错读成观测到的增益值**」；跨分量绝对零点不可由重叠数据求得；接缝相对台阶门 `1e-2`、判据适用域「只有两侧都在数据内部的帧边界计入判据」 |
| `实验/additive-sky-seamless/REPORT_paper.md`（只读） | §2 观测模型 `y_i=g_i(x)[P_i*S]+a_i+ε`；§2.2 「先用未饱和非变星匹配恒星的背景已校正积分通量」；§3 「feather 只能减小视觉突变，不是取得一致零点的证据」；§4 P5-B 要求增益乱序负对照；§5 现行结论「生产应保持恒等直到独立星光度通过」 |
| `实验/additive-sky-seamless/REPORT_experiment.md`（只读，只追加） | §6 三类数据分歧；附录 S03 / S03-exec / S03-裁决（`1.2815e-1` 是规范失控读数症状；修后跨分量 6.07% 不可辨识） |
| `run/ACSDSDOC-01/T02/P5_IMPL_DIFF.md` | D-2（P0）：生产纯加性、MA 内核非生产接线；`sky_plane.cpp:2974` 纯加性和；取 `G_i={0}` 等价于硬先验 `η≡0` |
| `run/ACSDSDOC-01/T08/docs/01_MULTIPLICATIVE_WIRING.md`、`04_MULTIPLICATIVE_NECESSITY.md` | 生产无乘性项；T08 A3 参考实现「标量增益」从**背景控制样本**估出，10 seed 下阳性 10/10 改善、阴性 8/10 也改善 ⇒ 改善非乘性专属 |
| `实验/shared/synthetic/{synth_gain.py,gainlib.py,noise_model.py}`、`scenes/*.json` | 合成场景库与噪声模型；本单元世界由 `run/ACSDSDOC-01/T08/code/t08_common.py::build_world` 生成（逐字复用，不改） |
| `run/ACSDSDOC-02/S03/REPORT_S03.md` + `实验/additive-sky-seamless/docs/S03_P5_COMPONENT_evidence.exec.json` | S03 已完成逐分量 gauge；负例开关 `ACSD_SKY_P5_FAULT=global_anchor`；S9b 修后 `5.98e-4`；跨分量 18.84 ADU（6.07%）不可辨识 |
| `AGENTS.md` §3.1/§6/§7/§10 | 读文档硬前置、证据三类互印、硬编码按来源处置、子代理无 git 写权限 |

## 2 交付物与复现

```bash
# 0) 独立构建目录（不写 build/linux-sci、build/linux-sci-p5）
cmake -S . -B build/linux-sci-s07 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux-sci-s07 --target acsd_phase2 acsd -j 14
# 1) 探针（只读复用 S03 的探针源码，按 sha256 钉住）
bash run/ACSDSDOC-02/S07/code/build_probe_s07.sh
# 2) 一键复跑（接线审计 → 边界覆盖审计 → 三条件×两世界四臂 → 证据聚合）
bash run/ACSDSDOC-02/S07/code/run_s07.sh 10
```

| 项 | 值 |
|---|---|
| 探针 | `run/ACSDSDOC-02/S07/code/p2_sky_joint_probe_s07`，sha256 `e3231ea1df36ba325bc69089610e7407cf50d43920c8c2739f61e613a64aeac4` |
| 探针源码（只读） | `run/ACSDSDOC-02/S03/code/p2_sky_joint_probe_s03.cpp`，sha256 `50b1485af4eb6217c6c5403ce94b9fa93f37a728816b68c84f8aaa62322bf524` |
| `libacsd_phase2.a` | sha256 `6ce03792955a78dc1153efb1361671eede93685017d0007b6d5bb8fc5fd3cae9` |
| `lib/algorithms/coverage/src/sky_plane.cpp` | sha256 `1c0024fa3c7a94cbb7fccf923bcf0756ea3fde243928966f8bb385e4f8d830e8`（= S03 修后状态） |
| 驱动脚本 | `s07_gain_experiment.py` sha256 `de0d1e9010a2b9ab715ea432d1ba1763fbe4f44eff37bfd79ad245009a6969ab` |
| 库 | `s07_gain_lib.py` sha256 `9e7421cd3c6b63073d70e69a148503867aa404463bee53f4f1503c84757e91a1` |
| seed | `SEED_BASE=20261014`（SHA-256 派生，无环境随机源） |
| 证据 JSON | `实验/additive-sky-seamless/docs/S07_P5_GAIN_evidence.json`（含 `git_write_performed:false`、base/source SHA） |

**产物索引**

| 产物 | 路径 |
|---|---|
| 主报告 | `run/ACSDSDOC-02/S07/REPORT_S07.md` |
| 三条件读数 | `run/ACSDSDOC-02/S07/results/S07_gain_main.json`、`results/psf_uniform/S07_gain_psf_uniform.json`、`results/color/S07_gain_color.json` |
| 接线审计读数 | `run/ACSDSDOC-02/S07/results/S07_wire_audit.json` |
| 边界覆盖读数 | `run/ACSDSDOC-02/S07/results/S07_boundary_coverage.json` |
| 可复跑脚本 | `run/ACSDSDOC-02/S07/code/{run_s07.sh,build_probe_s07.sh}` + `实验/additive-sky-seamless/code/s07_*.py` |
| 实验报告追加 | `实验/additive-sky-seamless/REPORT_experiment.md` 附录 S07 |
| 图 | **无**：本轮乘性臂只有合成世界，无真实数据腿 ⇒ 按派单「figures 仅在真实数据时」不建 `figures/` |

## 3 接线审计（D-01 §1；三态判定 + `文件:行号`）

审计由 `s07_wire_audit.py` **现场重扫** `lib/**` 得出（读数 `S07_wire_audit.json`，含全部命中行）。

### 3.1 P5 帧间残余乘性增益 = `IDENTITY`（行为位）/ `NOT_EVALUATED`（状态位）

| 事实 | `文件:行号` |
|---|---|
| 唯一读点，默认 `false` | `lib/infrastructure/scheduler/src/module_adapters.cpp:12534` `const bool require_gain = cor_doc.value("multiplicative_gain_applied", false);` |
| 该读点随即把增益指针置空 | `module_adapters.cpp:12579` `in.gain = nullptr;`（仅 `require_gain` 为真才取 `frames[f]["frame_gain"]`，`:12581-12582`） |
| 全仓 **0 处写** `multiplicative_gain_applied` | 命中仅 `:12532`（注释）与 `:12534`（读） |
| 权重链的乘性语义存在但未启用 | `lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp:184-193`（`in.gain==nullptr && !require_frame_gain ⇒ g=1`）、`:918-931`；`module_adapters.cpp:12642` `m06_pixel_wpolicy.require_frame_gain = require_gain;` |
| 生产施加是**纯加性** | `module_adapters.cpp:11186` `p2_sky_plane_eval_delta_block(...)`；`:11192` `out_v[...] = base - dvals[...]`（无乘性因子） |

**判定**：生产取 `G_i={0}`，等价于**硬先验 `η_i≡0`**，不是「规范固定乘性自由度」
（与 `P5_IMPL_DIFF.md` D-2 结论一致）。三态里的**行为位** = `IDENTITY`；
**状态位** = `NOT_EVALUATED`（无生产者，不得读成「已应用」，也不得读成「已证明无乘性差」）。
二维 `g_i(x)` 在生产链**无任何成员** ⇒ `NOT_IMPLEMENTED`。

### 3.2 MA 标量乘加求解器 `p2_upm_ma_build` = `NOT_IMPLEMENTED_ON_PRODUCTION_PATH`

| 事实 | `文件:行号` |
|---|---|
| 定义 | `lib/algorithms/coverage/src/upm.cpp:3221` |
| 调用者 1（工具面） | `lib/algorithms/coverage/tools/stage2.cpp:627` |
| 调用者 2（**全仓零调用者**的 wiring 函数内） | `lib/algorithms/integration/phase2_integrate/src/phase2_integrate.cpp:1362` |
| 该 wiring 函数仅「定义 + 声明」 | `phase2_integrate.cpp:1288`、`include/acsd/phase2_integrate.h:245`；`grep -rn run_upm_rej_samp_wiring lib/ include/` 仅此两行 |
| 同类唯一增益估计器亦非生产 | `lib/algorithms/coverage/src/sky_plane.cpp:4141` `p2_sky_estimate_gain_dc`，非测试调用者仅 `tools/stage2.cpp:656` |

### 3.3 P1 `k_photo·m(x,y)` = `APPLIED`（条件性，fail-closed）

| 事实 | `文件:行号` |
|---|---|
| 默认空间阶数 = 2 | `module_adapters.cpp:5870` `constexpr int kDefaultSpatialGainOrder = 2;` |
| 生产施加调用 | `module_adapters.cpp:6555` `calibration::apply_photometry_spatial(im.px(), im.w(), im.h(), sc->k_photo, poly, im.px())` |
| `m_order==0` 走标量入口 | `module_adapters.cpp:6542` 注释 + `:6557` `apply_photometry(...)` |
| 真正带空间项的帧数统计 | `module_adapters.cpp:6614` `if (scm != nullptr && scm->m_order > 0) ++n_spatial_applied;`；`:6626-6630` 组级摘要 `applied_spatial/partial_spatial/degraded_scalar/off_scalar` |
| 施加函数本体 | `lib/algorithms/calibration/src/photometry_apply.cpp:88`（`photometry_apply.h:56`：`order=0` 时与标量入口逐位一致） |
| 条件 | `module_adapters.cpp:5888` `fit_enabled` 默认 `false`；无标度来源则该帧 **fail**（不静默按 1 冒充） |

⇒ `k_photo·m(x,y)` **已被用**（P1 节点、逐帧 in-place）。这不改变 P5 的判定：
P5 的乘性项与 P1 的 `m(x,y)` 是**不同对象**（前者帧间差模，后者帧内空间场），
D-01 §3 明确「避免重复乘 P1 的增益」。

## 4 研究臂设计（D-01 §2/§3/§4）

### 4.1 世界与条件

由 `t08_common.build_world` 生成（逐字复用，不改）；同一 seed 的**阴性与阳性世界共用**
`name/seed_tag` ⇒ `S`、加性场 `a`、噪声、PSF 组成**逐位一致**，仅 `g` 不同。

| 项 | 值 |
|---|---|
| 几何 | 4 帧沿 x 条带，`W=H=256`，`PITCH=8` px 控制格，重叠 `48` px；`pixscale=1.0″/px` |
| 信号 | `signal="mixed"`：常数 200 + 星云（`neb_amp=25`，相关长 30 px）+ 200 颗星（`star_sigma=1.5`，振幅 `10^[1.5,3.5]`） |
| 加性背景 | `off=100 + 10k`，梯度 `a_grad·(k+1)·0.3·xn`、`a_grady·(k+1)·0.2·yn` |
| 噪声 | 逐像素独立高斯 `σ=6.0` ADU（登记为 limitation：真实 drz 噪声已相关化） |
| 阴性世界 | `g_true=[1,1,1,1]` |
| 阳性世界 | `g_true=[1.00, 1.15, 0.88, 1.06]`（`log g = [0, 0.1398, −0.1278, 0.0583]`） |
| PSF 条件 | 主臂 `psf_sigma=[1.2,1.6,1.4,1.0]`（逐帧不同）；对照臂 `psf_uniform=[1.4]*4` |
| 色差条件 | `color` 条件注入帧间色斜率 `b_k=[0,0.06,−0.04,0.03]`，星通量乘 `10^(−0.4·b_k·c_s)`（`c_s∈[−1,1]` 由星位置确定性派生） |
| seed | 每世界 10 个固定 seed（`pos00..pos09`、`neg00..neg09`） |

### 4.2 增益证据 = 独立匹配恒星的**扣背景积分通量比**（非背景梯度斜率）

1. **检测**（数据驱动）：3×3 局部极大 + `4σ`（MAD）阈值 + 最小间距 9 px ⇒ 每帧 39–62 颗
   （10 seed × 4 帧范围；配对观测量中位 67–69 条/世界）。
2. **匹配**：对每个**有交集**的帧对做最近邻匹配（容差 1.5 px）；本几何给出链式重叠对
   `(0,1),(1,2),(2,3)`（相邻 48 px）。不使用帧 0 作全局锚（帧 0 与帧 3 无重叠）。
3. **孔径测光**：`f = Σ_{r≤5px}(y) − n_ap·median(环带 7–10px)`；噪声
   `σ_f = √n_ap·1.4826·MAD(环带)`。**只对星点孔径取比**，不做任何全图像素比值/对数。
4. **求解**：观测量 `d = log f_i − log f_j`，`var = (σ_i/f_i)² + (σ_j/f_j)²`；
   加权最小二乘解 `η`，规范 `Σ_i w_i η_i = 0`（`w_i` = 该帧参与配对的观测量数）；
   `η=0` 检验用 `Δχ² = χ²(0) − χ²(η̂)`，`dof = n_frame − 1`。
5. **训练/留出**：按 `(i,j,⌊x⌋,⌊y⌋)` 的 SHA-256 确定性三分，`2/3` 训练、`1/3` 留出；
   留出中位观测量数 `21–24`。

### 4.3 四臂（+3 个对照臂）

| 臂 | 定义 | 说明 |
|---|---|---|
| **无校正** `UNCORR` | 等权合并 `raw` | 基线 |
| **feather-only** `FEATHER` | 升余弦羽化权重合并 `raw`（不扣任何天光/增益） | 「缝更平滑」对照 |
| **A-only** `A` | 生产探针 `δ`：`combine(raw − δ)` | 修复后纯加性基线（S03） |
| **A+G** `AG_JOINT` | 先在 `z = raw/g` 上**重采控制样本并重解加性** `δ^G`，再 `combine(z − δ^G)` | 主臂（增益感知次序） |
| `AG` | 先 `δ`（在未校正帧上解）再 `(raw−δ)/g` | D-01 §3 的**字面**次序；诊断 |
| `AG_ORACLE_JOINT` | 同 `AG_JOINT` 但用**真值** `g_true` | 已知注入阳性 |
| `AG_SHUFFLE_JOINT` | 同 `AG_JOINT` 但把 `η̂` 的取值**乱序**重排到帧上 | 增益乱序负对照 |

### 4.4 判据量（预声明，跑之前冻结在 JSON 的 `predeclared` 字段）

- **主判据（留出星光度误差）**：`RMS_model` = 留出星配对上
  `r = d − (η_i − η_j)` 的 RMS，单位 **dex**，分母 = 留出观测量数。
  它直接度量「该臂**声明的**增益模型解释帧间星光度不一致的能力」，
  与任何全局标度无关（不能被整体乘常数做小）。
- **成品帧间不一致** `RMS_product` = 该臂**校正后帧**上的 `RMS(d)`（诊断）。
- **接缝**：边界法向有符号台阶 / 局部背景电平，`max|rel_step|`（单位 1，门 `1e-2`），
  并做 `d ∈ {2,4,8,16}` px 的**法向多尺度**扫描（`d8/d2` 比值：纯台阶 ≈1，纯斜坡 ≈4–8）。
- **弥散源保真**：`σ=8` px 高斯平滑后的成品 vs 真值 `S`：`lowfreq_ratio`（低频功率比）、
  `struct_ratio`（结构标准差比）。
- **预声明判据 C1–C6**（方向先声明后判）：
  - C1 阳性可检出：阳性世界 `median(RMS_A − RMS_ORACLE) > 0` 且 95% CI 排除 0；
  - C2 阳性 G 有效：阳性世界 `median(RMS_A − RMS_AG) > 0`、CI 排除 0、中位比 ≤ 0.8；
  - C3 阴性无假阳：阴性世界同一统计量 CI **含 0**，且 `η=0` 的 χ² 拒绝率 ≤ 5%；
  - C4 保真不退化：阳性世界 `lowfreq(AG) ≥ 0.8·lowfreq(A)` 且
    `seam(AG) ≤ max(1e-2, 1.2·seam(A))`；
  - C5 值起因果作用：`median(RMS_A − RMS_SHUFFLE) ≤ 0`（严格形式）；
  - C6 特异性：换孔径（`r_ap: 5 → 8` px）后 `max|Δη̂|/σ < 3`（真乘性增益与孔径无关）。
  - `PASS ⇔ C1∧C2∧C3∧C4∧C5∧C6`；否则 `production_default = IDENTITY`。

## 5 结果

### 5.1 阴性/阳性世界四臂数字表（每格 = 10 seed 的中位；单位 dex，分母见 §4.4）

**主条件（逐帧 PSF 不同）**

| 世界 | `RMS_model` 无校正 | feather-only | **A-only** | **A+G(JOINT)** | A+G(ORACLE) | A+G(乱序) |
|---|---|---|---|---|---|---|
| 阴性 `g≡1` | 0.0546 | 0.0546 | 0.0546 | **0.0418** | 0.0546 | 0.0788 |
| 阳性 `g≠1` | 0.2069 | 0.2069 | 0.2069 | **0.0427** | 0.0527 | 0.2100 |

| 世界 | `median(RMS_A − RMS_AG)` [95% CI] | 中位比 | A+G 更好的 seed 比例 |
|---|---|---|---|
| 阴性 | **+0.00968 [+0.00681, +0.01740]**（CI **排除 0** ⇒ 假阳性） | 0.832 | 10/10 |
| 阳性 | **+0.16851 [+0.13703, +0.17957]** | 0.205 | 10/10 |
| 阳性（ORACLE） | +0.15545 [+0.12893, +0.16626] | 0.253 | 10/10 |
| 阳性（乱序） | +0.01662 [−0.06829, +0.10657]（CI 含 0） | 0.910 | 5/10 |
| 阴性（乱序） | −0.02083 [−0.02325, −0.00113] | 1.320 | 2/10 |

**接缝 / 弥散（主条件，中位）**

| 世界 | `seam` UNCORR | feather | A-only | **A+G(JOINT)** | A+G(字面次序) | `lowfreq` A | `lowfreq` A+G | `lowfreq` feather | `d8/d2` A |
|---|---|---|---|---|---|---|---|---|---|
| 阴性 | 0.0195 | 0.0218 | 0.00682 | **0.00651** | 0.0206 | 0.974 | 0.960 | **26.86** | 1.74 |
| 阳性 | 0.0738 | 0.1091 | 0.00687 | **0.00658** | **0.1148** | 0.996 | 0.979 | **29.85** | 1.62 |

### 5.2 增益恢复（主条件，10 seed 中位）

| 世界 | `η̂` 逐帧中位 | 真值 `log g` | `|η̂|` 中位 | `σ_η` 中位 | `χ²_red` 中位 | `η=0` 拒绝率 |
|---|---|---|---|---|---|---|
| 阴性 | `[+0.0176, −0.0234, −0.0022, +0.0304]` | `[0,0,0,0]` | **0.0208** | 0.00148 | 1.71 | **10/10**（`p` 最大 `8.9e-93`） |
| 阳性 | `[+0.0043, +0.1071, −0.1401, +0.0743]` | `[0, +0.1398, −0.1278, +0.0583]` | 0.0887 | 0.00146 | 3.44 | 10/10（正确拒绝） |

阳性世界恢复**不是**忠实恢复：帧 1 被压到真值的 77%、帧 3 放大到 127%
⇒ 标量增益同时吸收了别的逐帧标度（见 §5.4）。

### 5.3 PSF / 色差条件（同样 10 seed × 两世界）

| 条件 | 阴性 `|η̂|` 中位 | 阴性 `χ²_red` | 阴性 `η=0` 拒绝率 | 阴性 `median(RMS_A−RMS_AG)` [CI] | 阳性 `median(RMS_A−RMS_AG)` [CI] | 阳性 `χ²_red` |
|---|---|---|---|---|---|---|---|
| 逐帧 PSF 不同（主） | **0.0208** | 1.71 | **10/10** | +0.00968 [+0.00681,+0.01740] | +0.16851 [+0.13703,+0.17957] | 3.44 |
| PSF 一致（`1.4`×4） | **0.0014** | 1.55 | **0/10** | −0.00022 [−0.00050,+0.00011]（含 0） | +0.17720 [+0.15636,+0.19050] | 1.41 |
| 注入帧间色差 | 0.0196 | **20.53** | 10/10 | +0.00805 [−0.00310,+0.01008]（含 0） | +0.13550 [+0.10918,+0.14653] | **24.26** |

**读法**：
1. **阴性伪增益几乎全部由逐帧 PSF 失配产生**：`|η̂|` 从 `0.0208`（PSF 不同）降到 `0.0014`
   （PSF 一致），17×；拒绝率从 `10/10` 降到 `0/10`。⇒ 固定圆孔径 + 未匹配 PSF 会把
   逐帧 PSF 差异读成「乘性增益」。这正是 D-01 §2 要求给 PSF 条件的理由。
2. **色差项不被标量增益吸收**：注入色斜率后 `χ²_red` 从 `1.41/1.71` 升到 `24.3/20.5`
   （模型明显失配），但 `η̂` 的幅度并不随之正确增长 ⇒ 标量臂无法替代色差项。
3. **PSF 一致 + 无色差**时阴性世界**无**假阳性（C3 通过），但 C6 仍为假（见 §5.4）。

### 5.4 特异性：估计量随孔径变化（C6）

| 条件 | 阴性 `max|Δη̂|`（`r_ap 5→8` px，中位） | 阳性 `max|Δη̂|`（中位） | 阴性 `max|Δη̂|/σ` | 阳性 `max|Δη̂|/σ` |
|---|---|---|---|---|
| 逐帧 PSF 不同 | **0.0298** | 0.0297 | 24.9 | 26.0 |
| PSF 一致 | 0.0045 | 0.0042 | 4.4 | 10.9 |
| 注入色差 | 0.0400 | 0.0307 | 32.5 | 29.4 |

**阴性世界里 `|Δη̂| = 0.0298` dex 比它自己检出的「增益」`|η̂| = 0.0208` dex 还大 1.4 倍**
⇒ 该读数不是稳定的物理量，而是孔径/PSF 系统项。**C6 在三种条件下全部为假**
（`max|Δη̂|/σ` = 4.4–32.5，远超预声明的 3）。
（`σ` 来自最小二乘统计协方差，本身不含相关系统项，因此 `Δη̂/σ` 只作**相对**量；
绝对量 `max|Δη̂|` 已单独给出。）

## 6 判红/判绿（逐条，方向先声明后判）

| 判据 | 主条件 | PSF 一致 | 注入色差 | 说明 |
|---|---|---|---|---|
| C1 阳性可检出 | ✅ | ✅ | ✅ | ORACLE 臂在阳性世界显著更优（CI 排除 0，10/10）⇒ 判据量对真乘性差**有**检出力（非恒真门） |
| C2 阳性 A+G 有效 | ✅ | ✅ | ✅ | 中位比 0.205 / 0.194 / 0.355 ≤ 0.8 |
| **C3 阴性无假阳** | ❌ | ✅ | ❌ | 主条件 CI `[+0.00681,+0.01740]` 排除 0，拒绝率 10/10；色差条件 CI 含 0 但拒绝率 10/10 且 `χ²_red=20.5` |
| C4 保真不退化 | ✅ | ✅ | ✅ | `lowfreq` 0.979/0.992/0.986 ≥ 0.8×A；`seam` 0.0066 ≤ max(1e-2, 1.2×0.0069) |
| C5 值起因果作用（严格形式） | ❌ | ❌ | ❌ | 乱序臂中位改善 `+0.0166`（CI 含 0，5/10 为正）⇒ 乱序**不劣于** A，但**也不显著优于** A |
| C5′ 乱序不出现同向显著改善 | ✅ | ✅ | ✅ | 乱序臂 CI 均含 0，改善量比真增益小一个量级（0.0166 vs 0.1685） |
| **C6 孔径无关性** | ❌ | ❌ | ❌ | `max|Δη̂|/σ` 4.4–32.5 ≫ 3 |
| **总判** | **FAIL** | **FAIL** | **FAIL** | `production_default = IDENTITY` |

**判红说明（不得调宽门）**：
- 本轮的判红不是「把门调宽」，而是**新增**了两条更严的检验（C3 的 χ² 拒绝率、C6 孔径无关性）；
  三条预声明判据里 C3/C6 明确为假 ⇒ `PASS=false`。
- C5 严格形式为假是**如实登记的失败**，且**不改变总判**（C3/C6 已足以判红）：
  其机制是「错误赋值下的模型残差」与「不校正」统计不可分（乱序臂 `RMS_model` 0.210 vs
  A-only 0.207），不是「乱序也改善」。两种形式都在 JSON 里（`C5`、`C5_alt_...`）。
- **历史口径的对照**：T08 `04_MULTIPLICATIVE_NECESSITY.md` 的阴性对照「8/10 也改善」用的是
  **背景控制样本线性回归**估出的标量增益；本轮把证据换成**独立匹配恒星通量比**后，
  阴性世界仍是 **10/10 改善**。⇒ 「改善」不能归因于真实乘性差这一结论**在换证据源后依旧成立**，
  并且本轮给出了机制（PSF/孔径系统项）。

## 7 「缝更平滑」不构成增益必要性证据（D-01 §4）

| 量 | 无校正 | feather-only | A-only | A+G(JOINT) |
|---|---|---|---|---|
| `seam` max\|rel_step\|（阴性/阳性） | 0.0195 / 0.0738 | 0.0218 / 0.1091 | 0.00682 / 0.00687 | 0.00651 / 0.00658 |
| `lowfreq_ratio`（阴性/阳性） | — | **26.86 / 29.85** | 0.974 / 0.996 | 0.960 / 0.979 |
| `d8/d2` 法向多尺度比值（阴性/阳性） | 1.30 / 0.49 | 1.38 / 0.68 | 1.74 / 1.62 | — |

- feather-only 的接缝读数**并不比无校正小**，却把低频功率比放大到 **27–30 倍**
  ⇒ 它把台阶摊成斜坡、并严重破坏弥散源电平，**不得**据此判增益有效。
- 真正的接缝改善来自**加性**臂（`0.0738 → 0.00687`，10.7×），与 S03 的纯加性基线一致；
  A+G 相对 A 只再改善 4%（0.00687 → 0.00658），落在噪声内。

## 8 次序发现：`(raw−δ)/g` 的**字面**次序会破坏接缝（新增工程结论）

| 次序 | 定义 | 阳性世界 `seam` | 阴性世界 `seam` | 阳性 `RMS_model` |
|---|---|---|---|---|
| `AG_JOINT`（增益感知） | 在 `z=raw/g` 上**重采控制样本并重解加性** `δ^G`，`combine(z−δ^G)` | **0.00658** | 0.00651 | 0.0427 |
| `AG`（字面，D-01 §3 的读法） | 在未校正帧上解 `δ`，再 `combine((raw−δ)/g)` | **0.1148** | 0.0206 | 0.0427 |

机制：加性求解器在**未校正**帧上会把平滑的乘性分量 `(g_k−1)·S_smooth` 一并吸进 `δ_k`
（这正是 `UPM.md` 意义上的可辨识性退化）；此时再除一次 `g_k` 等于**重复校正**，
把公共面的电平按帧拉开 ⇒ 接缝劣化 17×。
`AG_ORACLE`（真值增益、字面次序）同样劣化（阳性世界 10 seed 中位 `seam = 0.12346`），
而 `AG_ORACLE_JOINT` 给出 `0.00590` ⇒ **是次序问题，不是增益估计问题**。
⇒ 建议（交负责人）：若日后启用乘性臂，生产次序必须是
「先按 `g` 校正观测 → 再在 `z` 上采样并解加性 `δ^G` → `corrected = z − δ^G`」，
并在 `UPM.md`/`ACSD_DESIGN.md` 写明次序合同；**本轮不实现**（生产保持恒等）。

## 9 边界覆盖审计：196 条中 82 条**分原因**统计（缺失不是零）

被测对象 `run/M42-E2E-02/results/seam_e2e02_v3.json`，sha256
`61bc51bd39688c5f785b12fba3795e16925286972095e470c41c49019686693c`
—— 与冻结记录（`实验/additive-sky-seamless/results/production_e2e_seam_record.json`，
已随 `1fa477a7a` 从工作树移除，本脚本用 `git show 1fa477a7a^:<path>` 只读取回）
所钉住的源 sha **逐字一致**。判据定义取自生产门 v3 版本
（`git show 0c353f351:eng/tools/e2e/seam_footprint.py`）。

| 分原因维度 | 统计 |
|---|---|
| 总边界 / 进判据 / 被排除 | **196 / 114 / 82**（`exclude` 全部为 `not_interior`，`few_samples = 0`） |
| 缺失对照侧 | `plus_side_absent` **56**、`minus_side_absent` **26**（每条的缺侧 `n_ctrl = 0`） |
| `margin_px`（对照线可放置域，px） | `4`:1、`8`:3、`16`:41、`32`:37；**`None`: 0** |
| 帧边界名（被排除） | `bottom` 35、`right` 21、`top` 14、`left` 12 |
| 帧边界名（进判据） | `left` 37、`top` 35、`right` 28、`bottom` 14 |
| 涉及帧数 | 49（全部 49 帧都有被排除的边界） |
| 被排除边界自身的接缝采样数 `n` | 46 – 256（**全部可测**，不是 `few_samples`） |
| 被排除边界的**已记录** `\|rel_step\|` | 中位 `4.455e-3`、p90 `9.351e-3`、**max `1.2267e-2`**；**超门 `1e-2` 者 5 条**、超门内最大读数 `7.256e-3` 者 16 条 |
| 进判据的 114 条 `\|rel_step\|` | max `7.2561e-3`，超门 0 条 |
| 多尺度可评估性 `n_evaluable_by_ctrl_shift[N]` | `N=200`:114、`128`:114、`64`:114、**`32`:151**、`16`:192、`8`:195、`4`:196 |

**结论（分原因）**：
1. 82 条**全部不是「无观测」**：`margin_px` 无一条为 `None`，即**两侧在 ≤32 px 处各有
   ≥20 个有效样本**。它们是「固定 200 px 对照偏移超出局部足迹余量」，
   而不是「一侧完全在数据外」。工具注释里的 `足迹外缘` 只对 200 px 偏移成立。
2. 因此 `114/196` 是**对照偏移 N=200 的覆盖选择**，不是「另 82 条不可测」。
   把 N 降到 32 可覆盖全部 196；降到 16 覆盖 192。
3. **缺失不是零**：被排除的 82 条里 **5 条**的已记录 `|rel_step|` 已 **> 1e-2 门**，
   16 条超出门内最大读数 `7.256e-3`。这些读数用的是**单侧可用**的对照线口径，
   不构成适用域内的判据值，但也**不允许**把它们当作「达标」。
4. 在 `N=32/16` 上重算判据值需要成品 FITS（已被回收，冻结记录 `provenance_note` 明示）
   ⇒ 登记 **`NOT_RUN`**，需要重跑 49 帧端到端并保留成品或 union footprint 掩膜。

## 10 二维 `g_i(x)` 不在本轮

本轮只做**逐帧标量**臂。二维乘性场需要**独立空间增益锚**与明确模型自由度；
本轮的 C6 已表明：连 0 维标量都会与孔径/PSF 系统项退化，二维场只会更严重。
⇒ 登记为**下一轮任务**（`followups`），本轮不实现、不宣称、不接入生产。

## 11 未决 / 分歧 / 诚实边界

1. **与 `REPORT_paper.md` 的分歧（先回报，未改其口径）**：`REPORT_paper.md` 摘要与 §5 仍写
   「S9b 断连图第二分量的接缝约为 12.8%」。S03 执行者与 Lead 裁决 2（`REPORT_experiment.md`
   附录 S03-裁决）已明确该数**是规范失控的读数症状，不得写成「12.8% 物理接缝」**。
   本轮不改 `REPORT_paper.md`（不在写面内），登记为**待 Lead 统一订正**。
   其余口径（§2 观测模型、§2.2 独立恒星通量锚、§3 feather 不是证据、§5「保持恒等直到独立星光度通过」）
   与本轮结论**一致**，无冲突。
2. **`S07` 与 `T08` 的关系**：T08 的标量增益从**背景控制样本**回归得出（非生产参考实现）；
   本轮改用**独立匹配恒星通量比**，得到**同一判红结论**，并补上机制（PSF/孔径）。
   两轮不互相替代：T08 覆盖空间增益臂 A4–A6，本轮只覆盖标量臂。
3. **权重链未闭合**：`V/g²` 缩放与增益估计不确定度传播属 P2 单元（`weight_chain.cpp`），
   本轮只登记乘性语义存在且未启用，**未**做权重传播实验。
4. **世界为解析合成、逐像素独立噪声**；真实 drz 噪声已相关化 ⇒ 本结论**不得**迁移为
   真实数据上的乘性可辨识性结论。
5. **真实帧乘性增益腿 `NOT_RUN`**：无该运行的真实帧集与独立星表（`testdata` 只读且未建立
   与本几何一致的帧集）⇒ 不伪造真实数据腿；`figures/` 因此不建。
6. **FP32 / Windows / `normalize→mosaic→export` 端到端**：不在本任务范围，`NOT_RUN`。
7. **并发提交**：本单元运行期间 HEAD 由 `a73f577b0` 移到 `c93b4a6f9`（其它轮次提交）。
   已核 `a73f577b0..HEAD` 对 `lib/**` 零改动，故接线审计与探针构建仍对当前 HEAD 有效。
8. **环状重叠图**仍 `NOT_RUN`（继承 S03 结论：本世界 footprint 是一维条带）。

## 12 三态汇总（D-01 §1 要求）

| 对象 | 三态 | 依据 |
|---|---|---|
| P5 帧间残余乘性增益（生产行为） | **IDENTITY** | 生产施加纯加性；`in.gain = nullptr` |
| P5 帧间残余乘性增益（状态通道） | **NOT_EVALUATED** | `multiplicative_gain_applied` 1 读 0 写 |
| P5 二维 `g_i(x)` | **NOT_IMPLEMENTED** | 生产模型无任何乘性成员 |
| P5 MA 标量求解器 | **NOT_IMPLEMENTED_ON_PRODUCTION_PATH** | 调用者只有工具面与零调用者 wiring |
| P1 `k_photo·m(x,y)` | **APPLIED** | `apply_photometry_spatial` 生产调用点，逐帧 fail-closed |
| P5 乘性臂研究实现（本轮） | **RESEARCH_ARM / FAIL 阴性对照** | C3/C6 判红 ⇒ `production_default=IDENTITY` |
