# 统计修复复核（独立审查者，task-5 复审）

复核角色：独立统计审查者（未参与修复）。只读 `src/`，只新建本文件。
受审对象：工作树 HEAD `376850340` + 未提交修改（`src/sky/sky.cpp` 20:01、`src/coadd/coadd.cpp` 19:33、`src/uncertainty/api.cpp` 19:30、`src/operators/linear_action.cpp`、Lead 的 `src/workflows/mosaic.cpp` 19:10 与 `src/projection/export_projection.cpp` 20:09）。
修复报告：[statistics-fix-report.md](statistics-fix-report.md)。原审计：[audit-statistics.md](audit-statistics.md)。

**复核方式（不采信 111 ok / 23 fail 的报告数字）**：读三份 diff 逐行核对数学与前置条件，然后自己写 5 个只依赖 `include/acsd/contracts.hpp` + `validation/stats/harness.hpp` 的探针（`/tmp/audit2/probe{,2,3,4,5}.cpp`，不使用 `src/` 头、不复用被测 helper、期望值在探针里按定义重算），链接当前 `build/science`（sky/coadd/uncertainty 库的 mtime 晚于对应源码，即确为修复后代码）。原审计的 CE-A/B/C/D/F/G/I 我未重算（与本次修改无交集）；下面所有数字都是我自己算出的。

---

## 0. 先更正我自己原报告的两处错误

1. **CE-H 的算术错误（实质性）**。我在 `audit-statistics.md` CE-H 写“`C=I₂+11ᵀ`，真值 `Var(和)=10`、`Var(差)=0`”。按该矩阵 `[[2,1],[1,2]]`：
   `Var(z₁+z₂) = 1ᵀC1 = 2+1+1+2 = **6**`，`Var(z₁−z₂) = (1,−1)ᵀC(1,−1) = **2**`。
   我写的 10/0 只对应 `C=2I+11ᵀ` 或 `C=2·11ᵀ`。实现者报告 §2 的更正正确。缺陷方向不变（修复前逐叶对角化发布 4/4：`Var(和)` 由 6 低估到 4，`Var(差)` 由 2 高估到 4），但我原报告的“低估 2.5 倍/真实 0”两处数值必须作废；本复核以我自己探针重算的 6/2 为准（`probe2` 已实测修复后为 6/2）。
2. **CE-E 的构造不可达**。我原用 `H=0` + 两帧，而实现的连通分量由 `H` 共享列决定，`H=0` 时每帧独立分量、每分量 gauge 自由度 0、`β≡0`，走不到“PSD 精确约束被丢”的路径。数学结论（丢 `U0` 会丢无噪声信息）仍成立，但该具体反例在本实现里不可达。本复核改用可达构型（2 控制、`C_c=11ᵀ`、`u=(1,−1)/√2`、`uᵀH=0`、`uᵀAG=−1/√2`），见 §2 P0-6。

CE-A/B/C/D/G/I 的数字我复核后无误（CE-B 的 BLUE 为 `(41/66,25/66)`、`V=41/132`；CE-G 的 `5/2`、`1/2`、`1` 已在 `probe2` 实测）。

---

## 1. P0 逐项裁决

| 项 | 裁决 | 我的独立证据 |
|---|---|---|
| **P0-3** 等权不得伪造 `1/K` | **关闭** | `probe2` T1：`Γ=[[1,2],[2,5]]`，equal `V=2.5`（旧 0.25）、BLUE `V=0.5`、QP `V=1`；`proc` 读到的是 `tᵀΓt`。代码 `coadd.cpp:1084-1098` 用同一 `block` 算方差，`Γ` 不完整时 `block_available=false` → 不发值。 |
| **P0-4** 协方差不得逐叶对角化 | **关闭**（`WeightSolution` 层） | `probe2` T2：`C=I₂+11ᵀ`、单帧两叶、`t=1` ⇒ `Var(z₁)=2`、`Var(z₁+z₂)=**6**`、`Var(z₁−z₂)=**2**`。实现以 `influence=W` 挂联合模型（`coadd.cpp:1150-1163`），`apply_covariance` 的 `I C Iᵀ` 语义使 `C=W C_joint Wᵀ`，跨叶项保留。 |
| **P0-2** unknown 传播/state | **关闭** | `probe2` T3：帧 B 带 material unknown ⇒ `automatic` 降级 equal 并具名、`V=null/state=unknown`、显式 `blue` 被拒（`Code::unavailable`，原因“the joint covariance model carries an unknown contribution”）。`sky.cpp:756-901` 按帧行偏移搬 unknown 并把无噪声帧变成显式 unknown term；`coadd.cpp:480-481` 同。 |
| **P0-5** `c`/`T` 同源、`A=TE`、chart 超界 | **关闭** | `probe2` T5：帧 A 支持 {0,1}、帧 B 支持 {1}，`y_A=(10,2)` ⇒ `c_A=**2**`（旧 6），且 `T y == c` 两行逐位一致，`A−TE` 每列最大差 0；T6 在 (0,40) 这一对叶上得到 `Code::unsupported`“span more than one gnomonic chart”，同一输入用常数基可建。 |
| **P0-6** PSD 精确零空间 | **关闭**（有限构型，见 §5 容差） | `probe2` T4：2 帧×1 控制、`C_c=11ᵀ`、`H=(1,1)ᵀ`、reference gauge、`c=(1,0)` ⇒ `β=(0,**−1**)`（旧 `(1/3,−1/3)` 类错解）、`Var(β_B)=4.9e−32`（精确 0，旧实现给已知非零）。 |
| **P1-8** 同 `SharedId` 全量合并 | **关闭** | `probe5`：`[S(+1),T,S(+1)]` ⇒ `Var=**5**`（旧 3）；C-ROOT（同根方差 4、影响 +1/−1）⇒ `Var=**0**`（旧按根相加 8）。 |
| **P1-12** `E ≥ 0` 证书 | **关闭** | `probe5`：`E=−5` ⇒ `bound=null`，原因“carries a negative entry”；`E=5` ⇒ 区间 `[35,35]`（真值 `|36−1|=35`）。 |
| **P1-10** nullspace 行信号/方差同算子 | **关闭** | `probe3`：`E=K=T=I₂`、null 方向 `(1,0)`、`y=(7,5)`、`C_y=diag(1,4)` ⇒ `z=(7,0)`；`Var(z₀)=**1**`（即 `C_y` 行，旧给 `P C Pᵀ` 的 0），`Var(z₁)=0`。`P'=I−M E K T`（`sky.cpp:2446-2560`）与信号 `y−M E K̂T y` 同一算子。 |
| **P1-11/13** `Σt=1` 与选择条件 | **关闭（记录层）** | `coadd.cpp:1072-1083`（solver 自检）、`1505-1518`（sealed 自检）、`837-844`（`conditional_on_frozen_selection_mask` / `omitted unconditional_coverage_after_selection`）。身份/revision 部分仍未修（见 §5）。 |

**结论**：实现者写域内的 6 个 P0（P0-2..P0-6）与 3 个 P1（8/10/12）**经我独立反例确认成立**；`information.*`、`operators.*` 的写域外缺口未被本次修复覆盖（§5）。

---

## 2. Lead 接线：**两处仍为 P0**

### 2.1 `mosaic.cpp` 的 `apply_sky` 是“按帧”调用，而 `E` 是联合叶空间（P0，未关闭）

`fit->E` 的行是**所有帧拼接后的叶空间**（`sky.cpp` 用 `blocks[f].offset` 建 E）。`mosaic.cpp:296-305` 对每个输入产品的 `SignalEstimate`（= 单帧叶向量）各调一次 `apply_sky`。`apply_sky` 校验 `Eβ̂` 与 `signal` 等长（`sky.cpp:2466-2475`）。

我的独立复现（`/tmp/audit2/probe.cpp`，2 帧×4 叶）：
```
E rows (joint leaf space) = 8, one frame leaves = 4
[ ok ] apply_sky on the JOINT 2-frame signal succeeds
[FAIL] apply_sky on ONE frame signal is refused  -> code=2 "the correction does not match the signal"
```
即下一次 `mosaic` 会在 `sky_apply` 阶段失败（`Code::invalid_model`），而不是得到一个校正产品。`mosaic.config.json` 现在是 4 个输入产品 ⇒ 4 帧，必然触发。

修法（三选一，需 Lead 定）：① 在 `mosaic` 内把各帧 `SignalEstimate` 拼成联合向量调用一次 `apply_sky`，再按帧切回并重建 provider；② 给 `E` 增加 `frame` 切片视图（`E|_f` 与其参数列），把 `apply_sky` 定义成逐帧算子；③ 由 `sky.fit` 发布 per-frame `E_f`。无论哪种，`SkyFit` 必须带帧身份，否则无法拒绝错配（§5）。

### 2.2 coadd 的联合协方差**忽略** `CovarianceAction.influence`，所以 Γ ≠ P C Pᵀ（P0，未关闭）

`apply_sky` 把校正发布为 `covariance->influence = P'`（模型仍是 `C_y`），但 `build_joint_covariance` 只读 `frame.noise->model` 的根与根影响（`coadd.cpp:365-430`），从不读 `frame.noise->influence`。于是即使 2.1 修好、provider 携带校正后的噪声，权重与发布协方差仍按**未校正**的 `C_y` 计算。

我的独立复现（`/tmp/audit2/probe4.cpp`）：单叶两帧、每帧 `C_y=4`、外层影响 `P=1/2`（即校正后应为 `P C Pᵀ=1`）：
```
equal V = 2 ;   P C P^T gives 0.5 (per frame 1),  C_y gives 2 (per frame 4)
-> the outer influence P was DROPPED: the joint used C_y
```
因此修复报告 §7.1 后置条件 1（`quadratic_form(noise,t)=tᵀP'C_yP'ᵀt`）**对直接使用该协方差的调用者成立**，但对 `coadd.weights`/`coadd.integrate` **不成立**。Lead 声称的“Selection/Γ 使用校正后的 P C Pᵀ”目前为假。

修法：`build_joint_covariance` 在把每帧的根影响放置到行偏移之前，先与 `frame.noise->influence` 组合（`L_k ← P' L_k`，用已修正的 `compose` 或 `multiply_csr`），并对 `unknown_terms` 同样处理；或等价地在每帧内先构造 `P'C P'ᵀ` 的根。

（`coadd` 自身没有再减天光：`combine_sealed` 直接读 provider 的 signal，`result.sky` 只作携带。这点 Lead 做对了。）

---

## 3. `export` 的 `Q C Qᵀ`：数值对了，单位/索引/错误记录仍错

数值与跨叶项：**成立**。`/tmp/audit2/probe.cpp` 用与 `export_projection.cpp:456` 相同的 `compose(row_action, estimate.noise->influence)` 构造，`C_joint=I₂+11ᵀ`、`W=I₂`、`Q=(1,1)` ⇒ `quadratic_form = **6**`（= `Q C Qᵀ = 1ᵀC1`），即跨叶共享项保留；`compose` 转置的顺序修正确实生效（`linear_action.cpp:331-337`：`apply` 是 `A(Bx)`，转置按 `BᵀAᵀ` 先 `Aᵀ` 后 `Bᵀ`）。

仍失败三处（都在 `src/projection/export_projection.cpp`，未提交的 Lead 改动里）：

1. **单位幂没有随 `Q` 变（P1）**。`signal_unit` 在 `421-427` 被改成 `sr_power=-1`（supported_mean）或 `0`（supported_flux），但协方差的 `variance_unit` 仍是模型的叶面亮度方差；`quadratic_form` 返回的 `Variance.unit = covariance->model->variance_unit`（`uncertainty/api.cpp`），Lead 只改了 `covariance->input_output`（`450-455`），**不影响返回值单位**。我的探针实测：模型 `sr_power=-2` 时返回单位仍是 `−2`，而 `supported_flux` 的信号单位是 `sr⁰` ⇒ 方差单位应为 `sr⁰`。
   最小反例：叶面亮度单位 `L sr⁻¹`、`Q` 行系数 `b=Ω`（sr）、叶方差 1 → `Q C Qᵀ` 数值为 `Ω²`（即 `L²`），发布单位却写 `L² sr⁻²`；对 `supported_mean` 恰好同为 `L² sr⁻²`，所以只有 `supported_flux` 暴露。
   修法：为导出协方差复制一份 `NoiseModel` 并令 `variance_unit = signal_unit²`（或让 `quadratic_form` 用 `input_output.input`）。
2. **`input_output.domain` 未更新（P2）**。只替换了 `codomain`（`452-455`）；`domain` 仍是叶空间索引（`leaf_count`），而该协方差作用于 `pixel_count` 个目标像元。任何按 `input_output.domain->size` 取泛函长度的消费者会得到错长度（当前 `covariance_apply` 不校验，所以静默）。
3. **失败 issue 被覆盖（P2）**。`464` 行 `add_issue(export_estimate->assessment, …)` 之后，`475` 行 `export_estimate->assessment = make_assessment(Code::ok)` 把它清零 ⇒ `Q` 推不过去时用户看不到原因，只看到 `noise=nullptr`。另外 `assessment.noise` 一直保持 `make_assessment` 的默认 `unknown`，即使协方差已知——与对象自身状态不一致（保守，但不可区分）。

---

## 4. 我新构造的独立反例（编号接续审计 R 系）

- **R1（export 单位）**：见 §3.1，`Var` 数值 1（=Ω²·1）而 `unit.sr_power=−2`，信号单位 `sr⁰`。可纸面复算。
- **R2（coadd 丢外层影响）**：单叶两帧、`C_y=4`、`P=1/2` ⇒ 正确 `V_equal=0.5`，实测 **2**。见 `probe4`。
- **R3（per-frame apply_sky）**：2 帧×4 叶，`E` 8 行、单帧信号 4 ⇒ `invalid_model`。见 `probe.cpp`。
- **R4（DTO 未初始化，命中于我的探针）**：`acsd::CoaddRequest request;` 后 `mixed_observation`/`rejection.kind`/`solve.*` 为不确定值；我的第一版 `probe2` 因此把 BLUE 判成“mixed observation 无共同均值”而失败。`include/acsd/model.hpp:388-402` 的 POD 无默认成员初始化，所有 `T x;` 调用者都有 UB 风险（冻结测试显式赋了 `mixed_observation=false`，所以没暴露）。属 `src/model` 写域外，建议加默认成员初始化。
- **R5（exact-constraint 一致性容差尺度）**：`sky.cpp:2103-2108` 用 `‖d−C C⁺d‖∞ ≤ 1e-8·max(1,‖d‖∞)`，是绝对下限。若控制协方差整体尺度 `≪1`（例如方差 1e−18、`σ=1e−9`），沿精确零方向 1e−9 的偏差 = 1σ 矛盾，却小于 1e−8 而被接受，随后以 `V=0` 发布该 β，偏差不受任何方差覆盖。应把容差改成相对 `σ_max(C_c)`/模型尺度（同文件 `exact_threshold` 已经这样做）。

---

## 5. 仍失败 / 有限构型 / 未覆盖

**P0 未关闭**：§2.1、§2.2。

**P1 未关闭**：
- `evaluate_scene_variance` 仍不校验 `description.noise` 的 condition/estimator 是否 actual_scene（`src/information/information.cpp:442-579`，写域外，未改）。
- `SkyFit`/`Selection`/`WeightSolution` 仍无 sky/provider/几何 revision 身份；`coadd` 的 `effective_response` 仍为 `nullptr`。这意味着 §2.1 的“拒绝错配”目前无法实现，也仍无法阻止跨 GeometryRevision 复用（实现者已在报告 §8.4 承认）。
- exact-constraint 容差尺度（R5）。

**有限构型**：
- P0-6 的正确性依赖“零方差方向确实是精确零”（由 `rank_relative` 分类），以及一致性容差；见 R5。
- `same_factor`（`uncertainty/api.cpp`）在“无 CSR/diagonal 载荷且无 content hash”时返回 `true`（视为同一 factor）。带 `compose`/`mul`/`sky` 因子的同 `SharedId` 根会被当成相同因子合并——当前构造路径不产生这种根，但这是一个可能静默错误的身份判据。
- `apply_sky` 的 nullspace 判定仍用 `1e-12` 绝对阈值（`sky.cpp:2511`），对 `|E_i|` 很小的行会把“整行都在 null 方向”判成可估；影响量级被 `1e-12·|β|` 限制，属 P2。

**P2（新发现）**：
- `build_sky_controls` 的控制协方差降级只把 `invalid_model/identity_conflict/invalid_request/invalid_covariance` 当 malformed 传播（`sky.cpp:1399-1412`）；`resource_exhausted`、`cancelled`、`unsupported`、`domain` 会落入 equal 降级，把**执行失败**变成“等权 + unknown”的科学结果。建议只允许 `unavailable/unsupported` 降级，其余原样返回。
- `quadratic_form` 负方差仍用 `1e-12·max(1,|v|)` 启发式（实现者报告 §8.5 承认）。
- export 的 domain/issue/assessment.noise（§3.2、§3.3）。
- DTO 未初始化（R4）。

---

## 6. 建议修复顺序（复核后）

1. **P0 接线**：定 `apply_sky` 的算子定义（联合 vs 逐帧，§2.1），改 `mosaic.cpp`；同时让 `build_joint_covariance` 组合 `frame.noise->influence`（§2.2），再做一个“含共享 flat/floor + 未知 gain”的端到端前向链路（前向 MC，二项区间）——当前没有当前分支的端到端证据（`validation/x01` 的 products 目录在工作树被删除，`e2e_summary` 是 normalize 的 fail 快照）。
2. **P0/P1 export**：单位幂（§3.1）、`input_output.domain`（§3.2）、issue 覆盖（§3.3）。
3. **P1 身份**：`SkyFit/Selection/WeightSolution` 加 provider/sky/revision 身份并让 `validate_inputs` 比较；`effective_response` 由 `src/response` 提供真实 `t,c,核`。
4. **P1/P2**：R5 容差、降级白名单、`same_factor` 的 undecided 返回 `false`/`undecided` 而非 `true`、`evaluate_scene_variance` 条件校验、`quadratic_form` 区间证书、DTO 默认初始化。
5. 修完后按 `audit-statistics.md` §3 与本文 R1..R5 重建独立反例（含我这两处更正），并在当前 SHA 记录 `tested_source_sha`。

---

## 7. 证据边界

- 我的全部结论来自：三份 `git diff` 逐行阅读 + 5 个独立探针（`/tmp/audit2/probe{,2,3,4,5}.cpp`，源码不在仓库内）+ `validation/stats/{sky_test,coadd_test}` 在当前库上 0 失败（仅作回归信号，不作裁决）。
- 我没有跑 `mosaic` 端到端：`validation/x01/input/cli/products/*` 在工作树中已被删除（`git status` 显示 D），且运行会写他人目录。§2 的接线判据来自源码 + 对同一调用形状的探针复现，不是端到端实跑。
- 探针链接的 `build/science` 是修复后代码（sky/coadd/uncertainty 库 mtime 晚于其源码；旧行为 0.25/4 已不再出现），但 `src/workflows/mosaic.cpp`（19:10）与 `src/projection/export_projection.cpp`（20:09）之后的 `libworkflows.a`/`libprojection.a` 分别为 20:11/20:10，故 §2/§3 的接线判断对应当前构建。
- 本复核不主张任何真实数据/覆盖结论；`V_F/V_B`、`K H=0`、`tᵀΓt` 均为线性/Gaussian 条件下结论。
