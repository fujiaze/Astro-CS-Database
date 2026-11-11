# D0｜2026-11-11 修复批次的独立复核（数学命题＋反例，非测试通过数）

> 审查者：D0 独立复核（不参与实现、未改 `src/`、未改他人文档、未 commit）。
> 工作目录 `/workspace/Astro CS Database`，分支 `rebuild/acsd-v2`，复核时 HEAD `63c21e6a5`。
> 范围：`git log c351c2fd1..HEAD` 的收尾提交（`853336c8a`、`66f034456`、`2ddea33d1`、`34193cdbb`）＋其所属整批 2026-11-11 修复（`3fa331a90` 04:45 → `63c21e6a5` 12:58，`src/` 侧 17 文件 +856/−98），重点按任务给定的五块。
> 依据：`AGENTS.md`、`docs/workplans/v2-science-engineering/review/13-INDEPENDENT-REVIEW-AND-EXPERIMENTS.md`、`docs/audit/c0-process-audit.md`（前一轮 C0）、`docs/engineering/rebuild/processes/projection.export.md`、`docs/science/foundation/INDEPENDENT_ORACLES.md`，并逐行阅读实际 C++ 路径与实测发布对象。
> 本报告不以任何测试/CI/脚本作为科学裁决。我自己执行过的实验只有三件，均在 `/tmp/d0/`（编译命令见附录 A），结论里逐条标明哪些是「我实跑」、哪些是「代码/数学推导」。

## 0. 结论先行

| 判定 | 内容 |
|---|---|
| 本批真实修好 | `decode_support_map` 的 **states/area/state 数组引用**（重开不再静默丢支持）；export 在 **数组不可读** 时具名失败；coadd/sky 在 **无 support map** 时拒绝；`information` 的 `covariance_has_unknown_term` 纳入 state；`bind` 的内容哈希纳入父内容；`bind` 的参数单位/协方差**长度**校验；astrometry `query_quads` 失败不再解引用空 optional。 |
| 仍开 | ① export 仍把 **缺失 states 数组** 当全支持（probe-2 实测）；② coadd/export 对 **state==2（bounded undecided）** 采取相反策略；③ 全链 `variance_unit` 丢失量纲幂，coadd 又从该标签**反推**信号单位，把 sr⁻¹ 面亮度发布成 sr⁰（probe-1 实测）；④ export 的 `C_out`/`SignalEstimate`/`NoiseModel` 身份只依赖 (像素数, quantity, state)；⑤ `parameter_units` 无量纲幂、无消费者、不序列化；⑥ export 数值界漏 `Ω(E_q)` 区间且归一化量的下界保护会给出**不包含自身点值**的区间。 |
| P0 | 2 项（无 states 数组→全支持；单位链把面亮度改成无 sr 量） |
| P1 | 9 项 |
| P2 | 10 项 |

C0 的 P0-1（bind 内容哈希）已修；C0 的 P1-3（bind `complete=true` 与 unknown 并存）**仍开**；C0 的 P1-9（information 把条件协方差发布成 known）**已修**；C0 的 P1-16（export 数值界漏 overlap 区间）**半修**（overlap 已传播，`native_area` 区间仍漏）。

---

## 1. 命题 S｜support state 的重开与语义

### S.0 数学陈述

设叶集合为 `L=(l_0..l_{n-1})`（由 leaf index 的 `(order,ipix)` 序列给定），发布的支持状态向量 `s∈{0,1,2}^n`：
- `s_i=0` 叶不存在/无支持；`s_i=1` 有正支持；`s_i=2` 支持**有界但未决**。
导出算子 `Q` 的行支持必须满足：`Q_{p,i}=0` 若 `s_i=0`；`s_i=2` 的叶必须在 `Q` 或其 assessment 中**保留未决性**，不得以确定值发布。

重开命题：`decode(encode(x)) = x` 对 `SupportMap` 的所有几何/支持数组成立，且**缺失证据 ≠ 全支持**：若产品不携带任何支持证据，则导出必须具名失败，而不是按 `s≡1` 处理（这是本批 `6889aa307`/`7775f6a14` 自称关闭的 P0）。

### S.1 `decode_support_map` 的修复范围（已修，但有两处缺口）

`src/workflows/product_io.cpp:688-725`：新增 `decode_array` 为 9 个引用建占位 `Array`（`states`、`area_lower_sr`、`area_upper_sr`、`normalization_states`、`observational_states`、`cell_face`、`cell_x`、`cell_y`、`cell_state`），`src/workflows/product.cpp:565-580` 的 `links[5]` 把这 5 个真正解析成可读数组。

**缺口 S1-a（P2，仍开）**：`encode_support_map` 写 `cell_depth`（`product_io.cpp:677`），`decode_support_map` **没有** `decode_array("cell_depth", ...)`（对比 `:720-723`）。同理 publish 侧 `src/workflows/product.cpp:293-295` 只持久化 5 个数组，`cell_face/x/y/state/depth` **从不进入产品闭包**。因此：
- 发布一个携带 cell 数组的 `SupportMap` 会在 `publish_product` 的闭包检查处**具名失败**（`product.cpp:1441-1446`：`$ref` 不在 manifest ⇒ `corrupt_product`）。这条是好事（不是静默），代价是 `SupportMap::Kind::dyadic_cells` 在「有 cell 数组」时根本发布不出去；
- 即使绕过发布，重开时 `support->cell_face` 会是非空占位（`shape` 空 ⇒ `LocalStorage::read` 的 `begin+count>total` 返回 `invalid_request`，`src/storage/local_storage.cpp:257-260`），而 `cell_depth` 为 `nullptr`；
- `src/information/triangulation.cpp:738-743` 的 `if (support->cell_face && support->cell_state)` 会进入，然后 `read_u32_array(context, support->cell_depth=nullptr)` 失败。

今天没有生产者写 cell 数组（`grep '->cell_'` 只有 reader/serializer），所以是**潜伏**故障；但「encode 有、decode 无、persist 无」三处不一致，等于把 `dyadic_cells` 标成可用而实际不可发布/不可重开。C0 已指出 `dyadic_cells` 无 driver（P1-13），本批没有改变这一点。

**缺口 S1-b（P2）**：占位数组只带 `object_id`，`dtype/shape/block_ids` 全空。若某条解析路径没被 `links[5]` 覆盖（cell 四个即是），代码拿到的是一个「看起来存在、读起来必失败」的 `ArrayRef`——`if (support->cell_face)` 这类判空会通过，错误被推迟到 `storage.read`。这不是「unknown」的正确表示。

### S.2 导出侧：缺失 states 仍被当作全支持（P0，仍开，我实跑）

`src/projection/export_projection.cpp:92-107`：

```cpp
std::vector<std::uint8_t> states;
if (!array) return model::ok_result(std::move(states));   // ← 缺失 = 空 = 全支持
...
if (!ok(view)) { return fail(...); }                      // ← 只有「不可读」才失败
```

`build_export` 在 `:289` 用 `if (!states.empty() && states[row->second] == 0) continue;`：`states` 为空时**每个几何重叠的叶都进入 Q**，且 `:676` 的 `export_estimate->assessment` 只在源 estimate 已经 conditional 时才降级，自身不记录「无支持证据」。

对照 `src/coadd/coadd.cpp:219-224` 与 `src/sky/sky.cpp:826-836`：

```cpp
if (!ok(support) || !val(support) || !val(support)->states) {
  return model::fail<FrameView>(..., "the frame publishes no support states", ...);
}
```

**同一个产品，coadd/sky 判为不可用，export 判为全支持。** 这是本批自称关闭的那个 P0 的同一条失败模式，只关了「不可读」分支。

**反例 S-1（已执行，见附录 A probe-2）**：发布一个 `SupportMap{kind=full_leaf_approx, states=null}` 的单叶产品，重开后 `support->states` 为空，`build_export(quantity=supported_flux)` **成功**并给出有限值 `2.350443054e-05 (=1e6·Ω_p)`。即：产品没有携带任何支持证据，导出仍发布了一张「完整」图像。正确行为（与 coadd/sky 一致）是具名失败。

补充边界：如果「无 states」在 `full_leaf_approx` 下被认为是合法的（整叶近似），那么 `coadd/sky` 的拒绝就是错的；两种解释必有一错，当前实现同时持有两种。

### S.3 `state==2` 的两种相反策略（P1，仍开）

- coadd 选择：`src/coadd/coadd.cpp:766` `if (views[f].state[row] != 1) { reasons[flat] = kNoSupport; continue; }` —— state 2 被**排除**，并且理由写成「no support」而不是「undecided」。
- export：`export_projection.cpp:289` 只跳过 `== 0` —— state 2 的叶**以全权重进入 Q**，像素被判 `pixel_supported=1`，发布确定值。

`src/geometry/drizzle.cpp:314-316` 对未决块写 `support_states=2, normalization_states=2`，所以 state 2 是**真实生产物**（X01 的单叶产品恰好是 1，见 §5）。

**反例 S-2（输入描述，最小）**：两帧同一叶，帧 A 的 `states=[2]`、`DQ=0`、信号 10；帧 B 的 `states=[1]`、信号 20。coadd 的 Selection 把 A 判为 `kNoSupport`，只用 B ⇒ 发布 20。把同一叶以 `states=[2]` 单独成产品 export ⇒ 发布 10（有限值、`complete` 继承为 true）。两处对同一 `s=2` 的语义相反，且都未在 assessment 里写出「支持未决」。按 `projection.export.md`「目标支持/面积的未决量也进入输出误差」，export 这一侧是明确违反。

### S.4 `read_frame` 本身（已修）

`coadd.cpp:204-212` 新增 `dq.size()!=count` 检查；`coadd.cpp:219-237`、`sky.cpp:826-850` 都把缺失/长度不符的支持状态变成具名失败。`coadd.cpp:232` 与 `sky.cpp:845` 都校验 `states.size()==count`。这一块我判**已修**且没有新的数学缺陷。

export 侧对应位置**没有** `states.size()==leaf_count` 的校验（`export_projection.cpp:213-223`）：靠 `LocalStorage::read` 的越界检查兜住「states 短于叶数」（返回 `invalid_request`），没有 UB；但 `states` 长于 `leaf_count` 时只读前缀，若 index 的 `ids` 被截断（`read_leaves` 用 `index.size*2` 读、按实际解码对数建叶，`export_projection.cpp:65-77`），支持状态会与叶**错位**而无人发现（P2）。

---

## 2. 命题 A｜assessment / identity 是否覆盖 state

### A.0 数学陈述

一个对象的 `Assessment` 是其估计量的**条件证明**：`complete=true` 只能在该对象的全部估计量项（值、方差、共享根、未知项、几何）都已封闭时发布；`noise=unknown`（完整方差未知）与 `science=bounded`（给出确定性界）互斥；`validity=certified` 只表示数值域内的包含，不表示完整性。identity 必须是内容的函数：`content_hash = H(内容)`，同型不同内容 ⇒ 不同 id。

### A.1 已修的部分

- `information/internal.cpp:151-160`：`covariance_has_unknown_term` 现在对 `nullptr`、`state!=known`、`assessment.noise==unknown` 都返回 true。配合 `information.cpp:200-211`，`flux_variance.state=known`（`:200`）会被立即覆盖为 `unknown` + `no_support` 语义 ⇒ C0 P1-9 的**主判据**已修。
- `uncertainty/api.cpp:331-346`：`CovarianceAction.assessment.complete = (state==known)`，并在 `state!=known || unknown_terms 非空` 时 `science=conditional` + `set_omitted("known_complete_variance")`。
- `sky.cpp:2620-2645`：`SkyFit` 在 `control_covariance_unknown` 时同时把 `fit->assessment` 与返回 `Result` 的 assessment 置 `noise=unknown / science=conditional / complete=false` 并加 `Code::unavailable`。
- `coadd.cpp:983-993`：缺 DQ ⇒ `science=conditional`、`complete=false`、`set_omitted("the_quality_mask_is_absent_for_at_least_one_frame")`。
- `export_projection.cpp:676-690`：export 继承源 estimate 的 `omitted/issues/conditional/complete`（修掉了 C0 的 P1「export 把条件源变成 bounded/complete」）。
- 实测（我实跑 `validation/product/assessment_invariants_probe.py`）：mosaic/export 各 3 个 assessment 对象，0 violation；我逐字段核对 `SignalEstimate{noise=unknown, science=conditional, complete=false}`、`NoiseModel{..., omitted=["known_complete_variance"]}`、`CovarianceAction{issues=[unavailable]}`，三者现在自洽。

### A.2 仍开：`WeightSolution` 的内层 assessment 仍是 forbidden triple（P2，未发布所以降级）

`src/coadd/coadd.cpp:1612-1618`：

```cpp
solution->assessment = model::make_assessment(Code::ok);   // noise=unknown, science=bounded
solution->assessment.complete = true;                      // ← complete=true
if (!covariance_known) { solution->assessment.noise = NoiseState::unknown; }
```

即发布对象自身是 `noise=unknown, science=bounded, complete=true`；真正的降级只写在返回 `Result` 的外层 assessment（`:1620-1638`），以及 `empty_leaves>0` 时只把外层置 conditional（`:1632-1637`）而 `complete` 仍为 true（违反它自己的 invariant 3）。**今天 `WeightSolution` 不进产品闭包**（`src/workflows/product.cpp` 的 `store_object` 只有 Array/Index/ErrorRecord/LinearAction/NoiseRoot/NoiseModel/SupportMap/SpatialResponse/CovarianceAction/SignalEstimate），所以只是内存对象错，可降为 P2；但一旦按 `coadd.weights.md` 把它封存，就是直接的 forbidden triple。

### A.3 仍开：`SupportMap` 的默认 assessment（P2，已发布的证据里就有一条）

`src/model/units.cpp:266-275`：`make_assessment(Code::ok)` 产出 `complete=false, validity=certified, science=bounded, noise=unknown`。所有 support map 构造者只追加 `complete=true`（`drizzle.cpp:450`、`normalize.cpp:287`、`sky.cpp:1145`、`coadd.cpp:1957`），**没有**人写 `.noise`。

实测证据（mosaic 与 export 产品的 `acsd.SupportMap` 对象）：

```
"noise": "unknown", "science": "bounded", "complete": false
```

这正好是 `assessment_invariants_probe.py` 的第 1 条 invariant（`noise=unknown ⇒ science≠bounded`）要抓的形态，而探针**没有报**——原因见 §5：`SupportMap` 不在它的 `MODEL_TYPES` 里。也就是说：探针「0 violation」是在跳过已经违约的对象之后得到的。

### A.4 identity 不是内容的函数（P1，随机型全覆盖，我实跑哈希匹配）

这一批在 identity 上只把 `state` 拼进 payload（`c7989484c`、`71bd5b44d` 等），**仍然没有把内容拼进去**：

| 对象 | 位置 | identity payload | 自由度 |
|---|---|---|---|
| `mosaic` 的 `ProductGraph` | `src/workflows/mosaic.cpp:395` | `sha256("acsd.mosaic")` | **常量**，所有 mosaic 产品同 id |
| export `SignalEstimate` | `export_projection.cpp:672` | `"export:signal:"+像素数+":"+quantity` | 2 |
| export `NoiseModel` | `export_projection.cpp:617` | `"export:noise:"+像素数+":"+quantity+":"+state` | 3 |
| export `CovarianceAction` | `export_projection.cpp:639` | `"export:covariance:"+像素数+":"+quantity` | 2 |
| `CovarianceAction`（通用） | `uncertainty/api.cpp:327-330` | `hash_index(latent, 根数, 0, {})` | 1 |
| sky 联合噪声 | `sky.cpp:999` | `"sky:joint:"+influence数+":"+state` | 2 |
| sky 参数协方差 | `sky.cpp:2549`、`:3125` | `"sky:parameter:"+influence数+":"+state` | 2 |
| sky `SkyFit` | `sky.cpp:2615-2618` | `hash_index(parameter, 参数数, 0, {})` | 1 |
| coadd 联合噪声（死代码） | `coadd.cpp:651-656` | `hash_index(latent, influence数, 0, {})` | 1 |
| coadd `Selection`/`WeightSolution`/`SignalEstimate` | `coadd.cpp:964/982/1508/1520/1612/1970` | `hash_index(leaf, leaf_count, {0|order}, {})` | 1–2 |
| coadd `SupportMap` | `coadd.cpp:1962` | `sha256("acsd.coadd.support")` | **常量** |
| `information::control_covariance` | `information.cpp:663` | `"control:"+根数+":"+state` | 2 |

**实证 1（哈希匹配，我实跑）**：export 产品里 `SignalEstimate.identity.content_hash = sha256("export:signal:16384:1")`、`NoiseModel.identity.content_hash = sha256("export:noise:16384:1:3")` —— 用源码里的字符串直接重算即得，逐字命中。所以「两个不同 mosaic → 不同目标 WCS → 同 16384 像素 → 同 quantity」的两次导出，`SignalEstimate`/`CovarianceAction` 的 `object_id` 与 `content_hash` **完全相同**，`NoiseModel` 只要 state 相同也相同。

这个碰撞今天**不是静默覆盖**，而是「内容寻址不可用」：`publish_product` 有闭包检查（`product.cpp:1416-1447`，所有可达 `$ref` 必须在 manifest 里），单产品内按 hash 取对象也自洽；但 `store_object` 是 `builder.objects[hash] = payload`（`product.cpp:61-66`，**覆盖式**），所以任何把两个图并入同一个 builder 的路径（跨产品去重、合并产品、外部内容寻址存储，即 `SYSTEM.md:43`/OBJECTS 要求的「按 hash 去重与重开校验」）都无法区分两张不同的图像；`same_identity()` 也会把两者判为同一对象。这与 C0 P0-1 是同一失效面，只是那次在 `bind`。

**实证 2（跨类型同 content_hash，我实跑）**：同一 mosaic 产品里

```
acsd.Index:ea3d86e7...            shape kind=leaf size=1 order=0   content_hash=sha256:ea3d86e7...
acsd.SignalEstimate:ea3d86e7...                                   content_hash=sha256:ea3d86e7...
```

`Index`（叶索引）与 `SignalEstimate`（coadd 结果）共享同一个 `content_hash`，因为 coadd 用 `hash_index(Index::Kind::leaf, 1, 0, {})` 直接当身份串（`coadd.cpp:1970`）。`uncertainty/api.cpp:41-45` 的 `same_identity` 只比 `content_hash` ⇒ 任何走该函数的去重/合并会把叶索引与信号估计当同一对象。

**实证 3**：mosaic 的 `SupportMap.index` 是 `kind=leaf size=1 order=6`，而 `CovarianceAction.input_output.domain` 是 `kind=leaf size=1 order=0`。`order=0` 的叶索引不可能只有 1 个叶（order 0 有 12 个），所以协方差定义域的 `Index` 元数据是伪造的（`order` 恒 0）。任何用 `leaf_area_sr(order)`（`export_projection.cpp:88`）解释该索引的消费者会拿到 12 倍面积。

裁决：**仍开，P1**。C0 的 P0-1 只修了 `bind` 一处；这批把 `state` 拼进 payload 反而让「内容寻址」看起来已完成。

### A.5 `bind` 的 `complete=true` 与 unknown 并存（P1，仍开；C0 P1-3 未修）

`src/observation/bind.cpp:155`、`:256` 仍然无条件 `bound->assessment.complete = true;`；紧接其后的 `:157-175` 可能在同一次调用里写 `omitted` 与 `add_issue(Code::unavailable, "the astrometric parameter error is unknown")`。本批对 `bind` 只改了 `revise()` 的 payload 与新增长度校验，没有碰这一段。

**反例 A-1（输入描述）**：`AstrometricSolution{solution->parameter_noise=nullptr, influence.from_measurements=nullptr}` + 一个合法 `WcsModel`（域覆盖检查通过）⇒ `bind_wcs` 返回 `complete=true` 且 `omitted=[astrometric_parameter_error_is_unknown]` + `issues=[unavailable]`。下游据 `complete` 认为误差项齐全。`ALGORITHM_CONTRACTS.md:24` 要求条件性结论不得变成「完整」。这不是「探针漏检」，是探针**类型过滤**直接排除了 `NativeObservation`（§5）。

---

## 3. 命题 C｜`C_out = Q C_leaf Qᵀ` 与 full 协方差

### C.0 数学陈述

设 `C = Σ_k L_k F_k F_kᵀ L_kᵀ`（`L_k` 为第 k 个根的叶空间影响，`F_k` 为因子；共享根同一随机变量共享 `F`），则
`C_out = Q C Qᵀ = Σ_k (Q L_k) F_k F_kᵀ (Q L_k)ᵀ`，
前提：(i) 每个根的 influence 与 factor 的潜变量维一致；(ii) 同一 `SharedId` 的多个 influence **先相加再平方**（否则丢交叉项）；(iii) `unknown_terms` 一并被 `Q` 推过去；(iv) `unit(C_out) = unit(Q)²·unit(C_leaf)`。

### C.1 `dense_control_covariance`（`sky.cpp:610-677`）：公式对，三处前置条件缺一

`out(i,j) += d[col]²·A(i,col)A(j,col)` 逐列累加，等于 `Σ A diag(d²) Aᵀ`——**代数正确**，且对非对角因子显式拒绝（`:640-643` "a non-diagonal root factor" → `unavailable`），而不是给出错的平方根。`roots` 为空时也拒绝（`:619`），不伪造零矩阵。

缺的前置条件：
1. **`unknown_terms` 完全不看**。若 `noise.state==known` 但携带 `unknown_terms`（含 influence），`dense_control_covariance` 仍然返回「已知部分」并 `ok`。`fit_relative_sky` 在 `:1887-1905` **另行**检查 state 与 `unknown_terms` 并把 SkyFit 降为 conditional，所以**当前没有把 unknown 当 0 发布**（P2 级：函数级契约不完整，靠唯一调用者的额外检查兜住）。
2. 潜变量维一致性**是**有保证的（我逐行核对过）：`to_csr(context, influence, rows, latent)` 对 CSR 比对 `payload.cols`（`sky.cpp:437-440`），对稠密路径走 `dense_action(rows, cols)`，后者校验 `apply_action_full` 的输出长度（`sky.cpp:413-416`）且对 >4096 的维度返回 `unavailable`（`:395-401`）而不是截断。所以这一条**不是**缺陷，我最初的怀疑不成立，记录在此以免下一轮重复怀疑。
3. 行空间只比 influence 之间的 codomain 大小，**不比单位**：若两个 influence 的 `spaces.input` 单位不同（一个 ADU²、一个 sr⁻²），`out` 直接把不同量纲的数相加（P2）。

`sky.cpp:2094-2111` 的用法值得单独写清：先用 `dense_control_covariance` 得 `C_c`，再 `jacobi_svd` 取 `B = U diag(√λ)`；若返回 `unavailable`（例如非对角因子）则退回 `dense_covariance_factor`。两条路径给出同一个 `B Bᵀ = C_c`，所以白化是等价的——这一点是**正确**的，也是本批（`14eb2f815`）真正解决的问题（不materialise 潜变量大小的单位矩阵）。

### C.2 sparse joint covariance（`coadd.cpp:1030-1210`）：共享根处理正确

- 每个根的 influence 先 `compose(frame.noise->influence, influence)`（外层 I0），再 `compose(embed_f, ·)` 把帧行嵌入拼接空间（`:1122-1138`）。
- 同一 `SharedId` 的关键证据：`:1160-1198` 用 `sum` 动作把 **同一 key 的多个嵌入影响相加**，然后**只挂一个 root**（`joint->shared_factors.push_back(entry.root)`）。于是 `Σ_f E_f L_f` 一起平方 ⇒ 交叉项 `E_1L_1 F Fᵀ L_2ᵀ E_2ᵀ` 保留。这一处与 `uncertainty/api.cpp:110-200` 的 `merge_shared_roots` 语义一致，**正确**。
- `unknown_terms` 也被嵌入（`:1152-1170`），缺噪声模型的帧写成带 `embed` influence 的未知项（`:1085-1094`）——`unknown ≠ 0`，**正确**。

**缺口 C2-a（P1）**：README 级契约「同 `SharedId` ⇒ 同一随机变量 ⇒ 因子必须相同」在 coadd/sky 的合并里**没有检查**，只在 `make_covariance`（`uncertainty/api.cpp:126-146`）里检查。而 coadd `:1192` 之后立刻调用 `make_covariance`，此时重复项**已经被合并成一个**，所以该检查看不到冲突。**反例 C-1（输入描述）**：帧 A 与帧 B 的两个 shared root 带同一 `SharedId`，但 `factor` 的对角分别为 `(1,2)` 与 `(3,4)`。真实模型矛盾（同一随机变量的尺度不一致），实现静默保留 A 的因子并把两个 influence 相加，等价于断言两者完全相关且同尺度——发布一个既不等于 A 也不等于 B 的协方差。正确行为是 `Code::identity_conflict`（`make_covariance` 对未合并情形正是这样做的）。

**缺口 C2-b（P1，与 P0-2 同源）**：`joint->variance_unit`（`coadd.cpp:1032-1035`、`sky.cpp:872-875`）取**第一帧**的 `variance_unit`，其余帧的 influence 各自带自己的单位（`coadd.cpp:938` 用 `noise.variance_unit`），无人校验所有帧一致。**反例 C-2**：帧 A 的 `variance_unit.sr_power=-2`、帧 B 为 `0` ⇒ `Σ_k E_1L_1F FᵀL_1ᵀE_1ᵀ + E_2L_2F FᵀL_2ᵀE_2ᵀ` 把 sr⁻² 与 sr⁰ 直接相加，模型声明为 A 的单位。这是量纲错误且**不影响任何数值**，所以不会被任何数值探针发现。

### C.3 导出链 `C_out = Q C_leaf Qᵀ`（`export_projection.cpp:548-660`）

结构上**完整**：`outer = compose(Q, estimate.noise->influence)`（`:556-577`），每个 `source.influence` 再 `compose(outer, influence)`（`:585-600`），`unknown_terms` 同样 `compose(outer, term.influence)`（`:602-615`），`shared_factors` 原样复制交由 `make_covariance` 合并。`compose(outer,inner)=outer∘inner`（`src/operators/api.cpp:243-285`）与 `transpose_apply`（`linear_action.cpp:326-338`）我逐行核对**顺序正确**。

**缺口 C3-a（P1）：单位是断言而非导出。** `variance_unit = unit_multiply(signal_unit, signal_unit)`（`:543`），而 `signal_unit` 是两个**硬编码**分支（`:524-531`）：

```cpp
signal_unit = model::unit_signal(estimate.unit.scale);   // 所有幂=0
signal_unit.sr_power = (quantity==supported_mean) ? -1 : 0;
```

`unit_signal` 会**清掉** `pixel_power/nm_power`（`src/model/units.cpp:29-31`），然后只把 `sr_power` 定成 −1/0。代码从不检查 `estimate.noise->model->variance_unit == unit_multiply(estimate.unit, estimate.unit)`。所以：
- 当叶信号是 sr⁻¹ 面亮度（drizzle 的正常输出）时，supported_flux 得 sr⁰、supported_mean 得 sr⁻¹，**碰巧对**；
- 当叶信号是 `unit_sr_per_pixel()`（sr¹·px⁻¹，`units.cpp:36`）或带 `nm_power` 时，`Q` 的 sr 幂被强行改写，`C_out` 的单位与真实的 `Q C_leaf Qᵀ` 差 sr²（P1）。**反例 C-3**：叶单位 `{scale="x", sr=1, pixel=-1}`、quantity=`supported_mean` ⇒ 真单位 sr¹·px⁻¹，发布 sr⁻¹。
- 另外 `recipe.spaces.input/output = unit_dimensionless()`（`:428-429`）把 `Q` 声明成无量纲，而 supported_mean 的 `Q` 系数是 sr⁻¹、supported_flux 的是 sr。`OBJECTS.md:133` 明说「支持均值系数无量纲，积分系数 sr」，所以**算子自身的单位声明是错的**（P1），只是没人用它做量纲检查。

**缺口 C3-b（P1）：数组单位与估计量单位不一致。** `:469` 的像素信号数组用 `estimate.unit`（叶单位）存储，`:676-679` 的 `export_estimate->unit = signal_unit`（改写过 sr 幂）；`:506-511` 的 `numerical_lower/upper` 数组也用 `estimate.unit`。于是同一产品里「数组说的单位」与「SignalEstimate 说的单位」不同。实测 X01 export：两者都是 `sr_power=0`（因为叶单位已被 coadd 改成 sr⁰，见 P0-2），所以这次一致；一旦叶单位是 sr⁻¹（正确的 drizzle 语义），数组 sr⁻¹ / 估计量 sr⁰ 就会同时出现在产品里。

**缺口 C3-c（P1，仍开且新引入）：归一化量的系数区间可能不包含自身点值。** `:315-340`：

```cpp
coefficient = entry.second / total;                                    // 点值
coefficient_lower = total_upper > 0.0 ? entry_lower / total_upper : 0.0;
coefficient_upper = total_lower > 0.0 ? entry_upper / total_lower : 0.0;  // ← 若 total_lower==0 则 0
```

`total_lower = Σ overlap_area.lower_sr`（`:317-320`）。当某个叶与目标像素只有边界相接（几何核给出的下界可以是 0）时 `total_lower==0` ⇒ `coefficient_upper=0 < coefficient=entry.second/total>0`。随后 `:521-541` 用该区间做 `lo/hi`，得到 `hi < 发布的点值`，即**发布的「数值包含」界不包含它自己发布的值**。`projection.export.md` 对这种情况明确要求 `support_unresolved`（「Σb 正下界未证明为 support_unresolved」），实现没有这条路径。

**缺口 C3-d（P1，半修 C0 P1-16）：`Ω(E_q)` 的区间仍未进界。** `:299-301` 只取 `native_area` 的**中点**（`0.5*(lower+upper)`，`max` 只是跨片合并，`native` 对同一源是同一个值，见 `overlap.cpp:454` 一次算出、`:632` 复制到每个 contribution）。`extrapolated_full_flux` 的系数是 `native_area·a/Σa`（`:334-340`），而 `native_area` 本身是区间量。`export_constant_probe.cpp` 对界的检查是 `lo ≤ actual ≤ hi`，两侧都由同一中点驱动，所以**永不可能发现**这个漏项。**反例 C-4**：叶值 `x=[1,1]`，某叶 `overlap_area=[0.5,1.5] sr`，`native_area=[0.9,1.1] sr`，单叶 ⇒ 真值域 `[0.45,1.65]`，发布 `[0.5,1.5]`（`1.1·1.5=1.65 > 1.5`）。

**缺口 C3-e（P2）**：`pixel_area[pixel]=native_area`（`:311`）之后**从未被使用**（`grep pixel_area` 只有两处：声明与赋值）。说明原设计要发布每像素面积/覆盖，现在被丢掉了，而 `Q` 的归一化依赖它。

**缺口 C3-f（P2）**：无任何像素有支持时 `offsets.assign(2,0)`（`:380`）⇒ 稀疏阵列只有 1 行，而 `recipe.spaces.codomain = make_pixel_index(pixel_count)`（`:419-421`）声明 `pixel_count` 行。算子形状自相矛盾；`apply` 之后 `read_f64(..., pixel_count)`（`:450-452`）会失败，所以是「具名失败 + 错误消息误导」而非静默错值。

### C.4 `uncertainty/api.cpp::make_covariance`

- `merge_shared_roots`（`:110-200`）用 `sum` 动作把同 `SharedId` 的 influence 相加后平方，注释给出交叉项展开，**正确**（与 coadd/sky 的 row-stack 等价，因为帧行空间互不相交）。
- `:240-258` 在 `influence` 全空、无 root、无 material unknown 时返回 `unavailable`，不发明零协方差——**正确**。
- `:331-346` 的 assessment 与 state 一致——**已修**。
- **缺口 C4-a（P1）**：`covariance->identity = make_identity(..., hash_index(latent, merged_count, 0, {}))`（`:327-330`）只依赖「合并后的根数」。两个内容完全不同的协方差只要根数相同，`object_id` 与 `content_hash` 相同。这与 §A.4 是同一类，但发生在**通用**入口，影响面最大：`covariance_apply`/`quadratic_form` 的结果数组都用 `covariance->model->variance_unit` 和该身份发布。
- **缺口 C4-b（P2）**：`same_factor`（`:47-83`）在两边都不是 `ActionImpl`、且任一侧无 `content_hash` 时返回 `true`（未决当相等）。合并这类共享根可能把不同因子当同一因子。

---

## 4. 命题 U｜astrometry 参数单位是否足以防止跨块误用

### U.0 数学陈述

参数向量 `θ=(crpix_x,crpix_y,ra0,dec0,cd_{00..11},sip_a…,sip_b…)` 的各块量纲为 px、rad、rad/px、px^{-(i+j)}。要防止跨块误用，必须存在一个**可运算的**分块单位结构：块内单位带正确的幂，块间运算（如 `cᵀCc` 混合了 crpix 与 ra）能被检出。

### U.1 实现：标签而已（P1，仍开）

`include/acsd/model.hpp:305-311` 新增 `std::vector<Unit> parameter_units`；`src/sources/astrometry.cpp:819-836` 填充：

```cpp
solution->parameter_units.push_back(model::unit_dimensionless());  // sr=px=nm=0, lin=0, mult=1
solution->parameter_units.back().scale = "px";                     // 只改字符串
```

`make_unit("1",0,0,0,1,0)` 之后只改 `scale`，所以每个块的 `sr_power/pixel_power/nm_power/linear_power` 全是 0、`multiplier=1`。后果：
- `units_equal`（`units.cpp:41-45`）能区分块（`scale` 不同），但 **`unit_multiply`/`unit_divide` 不能**：`px × rad` 得到 `{scale="px*rad", 全幂 0}`，与 `unit_dimensionless()` 在**幂**上等价，只是 `scale` 字符串不等于 `"1"`。所以没有任何代数能推出 `cᵀCc` 的 px²/rad²；
- `sip_a/b` 全部标成同一个 `"px^-n"`（`:833-836`），`n` 未定，SIP 各项的真实指数 `px^{-(i+j)}` 没有表达；
- `bind` 的校验（`src/observation/bind.cpp:82-93`）**只比长度**：`parameter_units.size() != parameter_count`，以及 `parameter_noise.input_output.domain->size != parameter_count`（`:94-101`）。它不校验单位内容（没有参照物可校验），也不要求 `parameter_units` 非空（`:87` 的 `!empty()` 让空向量直接通过）。
- `parameter_noise.model->variance_unit` 仍是单一的 `unit_variance("astrometric_parameter")`（`astrometry.cpp:329`），`unit_variance` 丢幂（`units.cpp:32-34`）。所以即使块单位齐全，协方差的单位标签也不分块。

### U.2 `parameter_units` 无消费者、不序列化（P1，仍开）

`grep -rn parameter_units` 的全部命中：`bind.cpp:87-88`（长度）、`astrometry.cpp:819-836`（填充）、`validation/process/sources.astrometry/astrometry.cpp:240-249`（**验证程序自己**比对 `scale` 字符串）、以及本批手改的 `schemas/science-input.schema.json`（+`parameter_units` 且列为 required）。

- `src/workflows/product_io.cpp` 的 `encode/decode_astrometric_solution` **没有** `parameter_units`（`grep AstrometricSolution src/workflows/*.cpp` 无命中，说明该类型不在产品 codec 里；`src/runtime/generated/codecs.cpp` 里也没有该字段）⇒ 重开产品后 `parameter_units` 为空，`bind` 的 `!empty()` 守卫直接跳过检查，**校验在重开后归零**。
- `schemas/science-input.schema.json` 现在 **required** `parameter_units`，而真实 codec 不产出它 ⇒ schema 与实际编码不一致（与 C0 P0-2 的生成器脱节同源；本批是**手改** schema 而不是重生成）。

### U.3 裁决

「足以防止跨块误用」= **否**。目前得到的是：一个可区分块名的字符串列表 + 一个只比长度的守卫 + 一个验证程序自证。真正的误用（`c=(1,0,…)` 只作用在 CRPIX 时应有 px²，作用在 RA 时应有 rad²，二者被同一个 `astrometric_parameter` 单位覆盖）**仍然无法检出**，与 C0 P1-6 的实质相同，只是多了一个不起作用的标签。

---

## 5. 证据工具的独立性审查

### 5.1 `validation/product/assessment_invariants_probe.py`

三条 invariant 都只读同一对象的 `noise/science/complete/issues` 四字段。问题：

1. **类型过滤把最关键的对象排除了。** `MODEL_TYPES` 列了 7 类，其中 `Selection`、`WeightSolution`、`SkyFit`、`ProductGraph` **从不进入产品**（`src/workflows/product.cpp` 的 `store_object` 只有 Array/Index/ErrorRecord/LinearAction/NoiseRoot/NoiseModel/SupportMap/SpatialResponse/CovarianceAction/SignalEstimate），而真正被持久化却**不在**列表里的有 `SupportMap`、`ErrorRecord`、`NoiseRoot`、`SpatialResponse`。实测每产品只检查 **3 个对象**；而已经违约的 `SupportMap{noise=unknown, science=bounded}`（§A.3）就在产品里被跳过。「0 violation」的覆盖面是 3/7 类型、6 个对象。
2. **不检查 `omitted`，也不检查 `validity`。** 本批所有修复的主要载体是 `omitted`（`known_complete_variance`、`the_quality_mask_is_absent_...`、`known_control_covariance`）。`complete=true` + 非空 `omitted` 是这套代码里 unknown 的第二条通道，探针完全不看。`bind` 的 `complete=true` + `omitted=[astrometric_parameter_error_is_unknown]` 也就不可能被它抓（且类型被过滤，双重漏检）。
3. **只验证内部一致性，不验证科学。** 三个字段在生产者里往往由同一个 `if` 一起写（例如 `coadd.cpp:1105-1118`、`uncertainty/api.cpp:331-346`、`export_projection.cpp:613-625`），探针检查的是「这行代码同一分支写的三个值是否互相矛盾」，即**重复生产代码的假设**。它的通过不能说明「完整方差已知」，只能说明「没有人在同一处写反」。用它作为「评估一致性已修复」的证据，正是 `13-INDEPENDENT-REVIEW...md` §一禁止的「用测试程序替代数学裁决」。
4. **不检查对象间的引用一致性**：`SignalEstimate.assessment.noise` 与它引用的 `CovarianceAction.model.state` 是否一致（这正是本批 `export` 新增的 `:679-682` 逻辑），探针不做跨对象比较。

### 5.2 `validation/product/projection_covariance_probe.cpp`

检查 `‖C_out x − Q C_leaf (Qᵀx)‖∞`。三重自证：

1. **两侧共享同一个 `covariance_apply`。** `C_out x` 与 `Q C_leaf Qᵀ x` 都经 `covariance_apply` → `uncertainty::apply_covariance`。因子约定（√方差 vs 方差）、对角/CSR 处理、共享根合并全部走同一实现。若因子约定系统性地错（例如把 `d` 当方差而非标准差），两侧同错，探针**仍然通过**。
2. **`Q` 两侧出现，完全抵消。** 任何 `Q` 的几何/归一化错误（包括 C3-c 的区间错、C3-d 的 `Ω` 错、supported_mean 的归一化错）都因为 `Q` 同时出现在左右两侧而不被检测。它能抓的只有「根集合/顺序/合并」这一维度。
3. **`Q` 的识别方式脆弱**：`for (m : eg->closure) if (m->identity.schema_id == "acsd.LinearAction") { q = ...; break; }` 取闭包中**第一个** LinearAction。export 产品的闭包里有 61 个 LinearAction（$Q$ 与 8 个 `Q L_k`、它们的子动作）。取到的是哪一个完全取决于 `closure` 的插入顺序；若取到的是某个 `Q L_k`，探针比较的就是另一对算子（维度都可能不符 → 直接失败或偶然通过）。
4. **单向量、单范数。** 只取一个固定 `x[i]=sin(0.3i+0.2)`，`∞` 范数。差分算子 `D=C_out−QCQᵀ` 若与 `x` 正交则不被发现。`nleaf=1`（见 5.4）时这一维尤其弱。
5. `read()` 在失败时返回空 vector，`read(cx,np)` 之后直接 `l[i]` 下标访问——`read_f64` 已保证 `np` 个元素，这条还算安全，但失败路径会 UB（空 vector 越界）而不是具名失败。

**结论**：它证明的是「`C_out` 的 influence 列表确实是 `Q∘L` 的同一组、同一顺序」，不足以支撑 `INDEPENDENT_ORACLES.md §9.2.3` 里「`C_out=QC LQᵀ` 已验证」的强陈述，更不能支撑「共享根与跨叶项在投影后保留」——X01 里**没有共享根**（见 5.4）。

### 5.3 `validation/projection/export_constant_probe.cpp`

1. **常面亮度恒等式是退化检验。** 对常数场 `x_j=b_0`，任何行和为 `Ω_p` 的 `Q` 都给出 `b_0Ω_p`。`Q^{flux}=a_{jp}/A_j`（配 per-leaf 积分量）与代码的 `Q_{pj}=a_{jp}`（配 sr⁻¹ 面亮度）**都满足**。所以这条「解析真值」无法区分两种定义，也就无法验证 `Q` 的定义本身。`supported_mean=b_0` 同理（单叶时归一化直接抵消）。
2. **界的检查是自证。** `lo ≤ actual ≤ hi`，`lo/hi` 由同一 `overlap_area` 区间、同一 `native_area` 中点生成；它只能发现「区间方向写反」，发现不了区间**漏项**（C3-d）或归一化下界保护错（C3-c）。真正独立的做法是对 TAN 像素给出**独立的**球面面积上下界（`|det CD|` 乘一个可证的界），再要求 `[lo,hi] ⊇ [独立的真值下界, 上界]`。
3. `nosupport` 模式是对的、也是唯一真正覆盖本批 `decode_support_map` 的地方（重开后 `states` 仍在），但它把 `states present=%d` 只 `printf` 不 `check`；`states` 若丢失，它仍会因为 `state==0` 无从判断而走到「全支持」并给出有限值——`check(isnan)` 会失败，这点反而是对的。它**没有**覆盖新增的 C3-c（`total_lower==0`）与 C3-d（`native_area` 区间）。

### 5.4 X01「全链证据」的实际覆盖（P1，证据面）

我实测 X01 发布产品：`SupportMap.states.shape=[1]`、值 `1`；`mosaic` 的 `NoiseModel` 是 `innovations=8, shared_factors=0`。

- **nleaf=1**：跨叶项、叶合并（`E-MERGE` 反例里的两片）、`Σb` 的多叶归一化、state 2、跨 face——全部不可能被 X01 覆盖。
- **shared=0**：`shared root` 在投影后保留这一命题（本批 `7223b34ad`/`2540` 多处代码）在 X01 上**完全没有被触发**。

因此 `INDEPENDENT_ORACLES.md §9` 里「`C_out` 恒等式（2 帧链 3.1e-16）」只能说明「1 叶 + 无共享根 + 8 个独立 innovation 的链自洽」。

---

## 6. 发现清单（P0/P1/P2，含状态）

标记：**FIXED**=本批修好；**OPEN**=仍开；**HALF**=部分修；**DEAD**=当前不可达；**UNVERIFIED**=我未验证。

### P0

| # | 发现 | 位置 | 状态 |
|---|---|---|---|
| P0-1 | export 把**缺失 states 数组**当全支持，发布有限值图像；同一产品被 coadd/sky 拒绝。本批自称关闭的 P0 同类失败模式仍开（我实跑 probe-2） | `export_projection.cpp:92-107`、`:289`；对照 `coadd.cpp:219-224`、`sky.cpp:826-836` | **OPEN** |
| P0-2 | 全链 `variance_unit` 丢量纲幂（`unit_variance` 只带 `scale`），coadd 又从该标签**反推**信号单位：实测 `x01-03` 信号 `sr=-1`、其协方差 `sr=0`（`Var(y)≠unit(y)²`），mosaic/export 因此把面亮度发布成 `sr=0`（我实跑 probe-1） | `model/units.cpp:32-34` 与各生产者的 `unit_variance(<scale>)`（`calibration_kernel.cpp:305`、`photometry/api.cpp:658-661`、`measure.cpp:846`）；反推处 `coadd.cpp:1901-1926` | **OPEN**（本批 `6c3f8d7af` 使其从「单个标签错」变成「信号单位被改写」） |

### P1

| # | 发现 | 位置 | 状态 |
|---|---|---|---|
| P1-1 | `state==2`（bounded undecided）在 coadd 被**排除**、在 export 被**全权重包含**，两处语义相反且都不标注未决 | `coadd.cpp:766` vs `export_projection.cpp:289` | **OPEN** |
| P1-2 | export 的归一化系数区间在 `total_lower==0` 时退化为 `[·,0]`，**不包含自身发布的点值**；契约要求的 `support_unresolved` 未实现 | `export_projection.cpp:315-340`（`:326`） | **OPEN** |
| P1-3 | `extrapolated_full_flux` 的 `Ω(E_q)` 只用**中点**，数值界漏该区间（C0 P1-16 半修） | `export_projection.cpp:299-301`、`:334-340`、`:506-541` | **HALF** |
| P1-4 | `Q` 的单位被声明为无量纲（mean 实为 sr⁻¹、flux 实为 sr）；`C_out` 的 `variance_unit` 由硬编码 `signal_unit` 断言，从不与 `C_leaf` 的单位比较；像素/界数组用 `estimate.unit` 而估计量用 `signal_unit` | `export_projection.cpp:428-429`、`:469`、`:506-511`、`:524-531`、`:543` | **OPEN** |
| P1-5 | 全部协方差/估计量身份只依赖 (计数, state, quantity)，`content_hash` 与内容无关；实测 export `SignalEstimate`/`NoiseModel` 的哈希可由源码字符串重算命中；同一产品里 `acsd.Index` 与 `acsd.SignalEstimate` 共享 `content_hash` | `export_projection.cpp:617/639/672`、`uncertainty/api.cpp:327-330`、`sky.cpp:999/2549/3125`、`coadd.cpp:1962/1970`、`mosaic.cpp:395`、`information.cpp:663` | **OPEN**（`store_object` 覆盖式写入使冲突静默） |
| P1-6 | `bind` 的 `complete=true` 与 `omitted/issue(unavailable)` 并存（C0 P1-3 未修） | `bind.cpp:155`、`:256` | **OPEN** |
| P1-7 | `parameter_units` 无幂、单一 `px^-n` 标签、无消费者、不序列化；`bind` 只比长度且允许空向量；schema 手改后要求该字段 | `model.hpp:305-311`、`astrometry.cpp:819-836`、`bind.cpp:82-101`、`product_io.cpp`（无该字段）、`schemas/science-input.schema.json` | **OPEN** |
| P1-8 | 同一 `SharedId` 的共享根在 coadd/sky 合并时**不校验因子/单位一致**，且合并后 `make_covariance` 的冲突检查失效；跨帧 `variance_unit` 不一致无人校验 | `coadd.cpp:1160-1198`、`:1032-1035`、`:938`；`sky.cpp:942-999` | **OPEN** |
| P1-9 | coadd 发布的 `SupportMap{kind=full_leaf_approx}` 用 `area_lower=area_upper=整叶面积`（把「部分覆盖」写成**零宽度的精确面积**），且未在 assessment 记录该近似 | `coadd.cpp:1941-1962` | **OPEN** |

### P2

| # | 发现 | 位置 | 状态 |
|---|---|---|---|
| P2-1 | `cell_depth` 编码但**不解码**；`cell_*` 五个数组**从不持久化** ⇒ 携带 cell 数组的 `SupportMap` 在发布闭包检查处 `corrupt_product`（不是静默，但 `dyadic_cells` 因此不可发布）；占位 `ArrayRef` 只带 object_id 使 `if (support->cell_face)` 判空失效 | `product_io.cpp:677/720-723`、`product.cpp:293-295/565-580/1441-1446` | **OPEN**（无生产者，潜伏） |
| P2-2 | `decode_support_map` 不含 `index`（由 `product.cpp:552-563` 补），但**任何直接调用 `decode_support_map` 的新代码**都会得到无索引的 support；`states` 长度不校验（>leaf_count 时静默错位） | `product_io.cpp:688-725`、`export_projection.cpp:213-223` | **OPEN** |
| P2-3 | `WeightSolution` 内层 assessment 是 `noise=unknown, science=bounded, complete=true`；`empty_leaves>0` 只降外层 science 不降 complete | `coadd.cpp:1612-1618`、`:1632-1637` | **DEAD**（未封存） |
| P2-4 | `SupportMap` 默认 assessment 即 `noise=unknown, science=bounded`，无人写 `.noise`；已在发布产品中实测 | `model/units.cpp:266-275`、`drizzle.cpp:450`、`coadd.cpp:1957` | **OPEN**（探针漏检） |
| P2-5 | `dense_control_covariance` 忽略 `unknown_terms` 与 influence 单位（靠 `fit_relative_sky` 的额外检查兜住） | `sky.cpp:610-677` | **OPEN**（函数级契约） |
| P2-6 | `build_joint_covariance`（稠密版）是**死代码**，内含 `complete=true` + count-only 身份；`solve_weights` 已改用 `_sparse` | `coadd.cpp:246-660`（`:657`） | **DEAD** |
| P2-7 | `sky.cpp:2504-2512` 的注释断言「operators 的组合 helper 会以错误顺序取转置」，据此绕开 `compose` 自建 `multiply_csr`。核对 `linear_action.cpp:326-338`（`compose` 的 `transpose_apply` = `innerᵀ∘outerᵀ`）**该缺陷在 HEAD 不存在**；`multiply_csr(K,L)=K·L` 与 `compose` 等价，所以无数值危害，但**错误的技术理由留在生产代码里**（与 C0 P2-1 同类） | `sky.cpp:482-488`、`:2504-2532` | **OPEN**（注释错） |
| P2-8 | `unit_is_unspecified` 要求 `scale=="1"`，而默认 `Unit{}` 的 `scale==""`（`model.hpp:38` 无默认初始化器）；默认单位进 else 分支被当作「与协方差矛盾」拒绝。`unit_is_dimensionless`（`units.cpp:47-49`）反而接受 `""` ⇒ 同一「未设置」在两处语义不同，且该差异现在是 coadd 的逻辑负载 | `coadd.cpp:48-51`、`:1894-1913` | **OPEN**（`Unit{}` 路径未实跑） |
| P2-9 | 无支持像素时 `Q` 只有 1 行而声明 `pixel_count` 行；`pixel_area` 只赋值不消费 | `export_projection.cpp:380`、`:419-421`、`:311` | **OPEN** |
| P2-10 | 死三元式：`leaf_index = estimate.signal->index_id.empty() ? nullptr : nullptr`——两个分支相同，等于「signal 自带 index 时也不回退」，只影响缺 support.index 的产品（会误报 `missing_dependency`） | `export_projection.cpp:191-196` | **OPEN** |

（P2-10 之后另有一条与 `projection.export.md`/`INDEPENDENT_ORACLES.md §9.1` 冲突的定义问题，见 §7。）

---

## 7. 科学正本之间的矛盾（新发现，P1 级文档缺陷）

`docs/science/foundation/INDEPENDENT_ORACLES.md` §9.1（本批 `34193cdbb` 新增）给出

```
Q^flux_{pj} = a_{jp}/A_j ,  Q^mean_{pj} = a_{jp}/Σa
```

而 `docs/engineering/rebuild/processes/projection.export.md:79` 与 `docs/audit/b5-export-review.md:13` 给的是

```
supported_mean = b/Σb , supported_flux = b , extrapolated = Ω(E_q)·b/Σb
```

实现（`export_projection.cpp:318-340`）跟随后者（`coefficient=b`）。两个公式在**常数场**下同时给出 `b_0Ω_p`，对非常数场给出**不同的估计量**（前者配「每叶积分通量」，后者配「每叶 sr⁻¹ 面亮度」）。X01 的 drizzle 输出是 sr⁻¹ 面亮度，所以实现是对的、§9.1 是错的；但 §9.1 自称「独立定义」，它是唯一的 oracle 文本，却与实现对不上，而唯一验证它的实验（常数场）**无法区分两者**。这属于 `AGENTS.md`「科学矛盾给反例并修订科学正本」。

**反例 D-1（解析，最小）**：单目标像素完全落在 2 个叶上，`a_1=a_2=Ω_p/2`；叶值 `x_1=2, x_2=4`（sr⁻¹ 面亮度，`Ω_p=2`）。
- 实现（`coefficient=b`）：`s = (Ω_p/2)·2 + (Ω_p/2)·4 = 6`。
- §9.1（`coefficient=a/A_j`，若 `x` 被解释为每叶积分通量为 `x_j·A_j`）：`s = (1/2)·(4) + (1/2)·(8) = 6`。

两者恒等（`a_j/A_j · x_jA_j = a_jx_j`），所以这条只能证明 §9.1 **缺 `x_j` 的定义**（而产品里的 `x_j` 是 sr⁻¹ 面亮度），不能证明数值分叉。可判定的分歧在单位，见 §8 反例 3。

---

## 8. 可执行最小反例（≥3）

### 反例 1（已执行）：缺失 states 数组 → 全支持导出

代码：`/tmp/d0/export_absent_states_probe.cpp`（附录 A 编译）。构造 1 叶产品，`SupportMap{kind=full_leaf_approx, states=null}`，发布后重开、`build_export(supported_flux)`。
实测：

```
reopened support->states present = 0
build_export ACCEPTED a product with no support states; pixel=2.350443054e-05 (finite=1)
```

期望：`Code::unavailable/invalid_model`（与 `coadd.read_frame`、`sky.build_sky_controls` 一致）。判据：`std::isfinite(value)` ⇒ 失败。

### 反例 2（已执行）：`Var(y) ≠ unit(y)²` 与单位被反推改写

代码：`/tmp/d0/unit_chain_probe.cpp`。对每个产品的 `SignalEstimate` 比较 `unit(estimate)²` 与 `estimate.noise->model->variance_unit`。
实测：

```
x01-03: signal sr=-1 | variance sr=0 | Var(y)==unit(y)^2 ? NO
mosaic: signal sr=0  | variance sr=0 | yes
export: signal sr=0  | variance sr=0 | yes
```

mosaic/export 的「yes」本身是缺陷证据：`x01-03` 的面亮度（sr⁻¹）在 coadd 处被改写成 sr⁰，因为 coadd 用 `variance_unit`（已丢幂）反推 `output_unit`（`coadd.cpp:1905-1912`）。判据：三者应同时为「yes」且 mosaic 的 `sr_power` 应为 −1。

### 反例 3（纯输入＋代码级）：`Q` 的单位声明与 `§9.1` 的定义互斥

先说清一件事，避免把这条读成错的数值反例：若把 §9.1 里的 `x_j` 解释为**每叶积分通量** `x_j^{int}=x_jA_j`，则 `Q^{flux}=a/A` 与实现的 `Q=b` 是**同一个算子**（`(a_j/A_j)·(x_jA_j) = a_jx_j` 恒等），数值上不可能分叉。所以 §9.1 的问题不是公式，而是它**没有声明 `x_j` 的定义**，而产品里的 `x_j` 是 sr⁻¹ 面亮度（`x01-03` 实测 `sr_power=-1`）。真正可判定的分歧在单位：

输入：叶信号单位 `{scale="x", sr=1, pixel=-1}`（`unit_sr_per_pixel()` 这类合法单位），单叶，`quantity=supported_mean`。
- §9.1 的 `Q^{mean}=a/Σa` 是**无量纲**系数 ⇒ 输出单位必须**继承**叶单位 `sr¹·px⁻¹`（`projection.export.md:79`「单位分别继承 B」）。
- 实现（`export_projection.cpp:524-531`）：`signal_unit = unit_signal(scale)`（清空所有幂）然后 `sr_power=-1` ⇒ 输出单位 `sr⁻¹`，且 `Q` 自身被声明成 `unit_dimensionless()`（`:428-429`）。

即两处独立证据：实现既没有「继承 B」，`Q` 的单位声明也与 `OBJECTS.md:133`（「支持均值系数无量纲，积分系数 sr」）相反。这也是 P1-4 的可判定形式：不需要造几何，只需一个非-`sr⁻¹` 的叶单位就能判 fail。

### 反例 4（代码级，最小）：`total_lower==0` 时界不含点值

输入：单叶，`overlap_area=[0.0,1.0]`（叶与像素边界相接，几何核给出含 0 的区间），`x=[1,1]`，`quantity=supported_mean`。
代码路径：`total=0.5`、`total_lower=0.0` ⇒ `:325-326` 取 `coefficient_lower=0.0/1.0=0`、`coefficient_upper=0.0`（被 `total_lower>0` 守卫置 0）；点系数 `coefficient=0.5`。
结果：发布 `value=0.5`，`numerical_lower=0`，`numerical_upper=0` ⇒ `hi < value`。期望：`Code::unavailable`（`support_unresolved`）或 `hi ≥ 0.5`。

### 反例 5（代码级，最小）：身份碰撞

构造两个不同 mosaic（不同帧、不同叶值、同一 `leaf_count=1`）并各自导出到 16384 像素的同一 quantity。
由 §A.4：两个 `SignalEstimate` 的 `object_id` 都是 `acsd.SignalEstimate:sha256("export:signal:16384:1")`，`content_hash` 相同。判据：两个不同内容的对象必须有不同的 `content_hash`；今天它们没有，因此外部按 `content_hash` 去重/合并/重开校验（`SYSTEM.md:43`、OBJECTS）无法区分。产品**内部**仍自洽（闭包检查 + 单产品按 hash 查表），所以这是「内容寻址失效」而不是「文件被覆盖」——两者的区别写在 §A.4 实证 1。

---

## 9. 我不能验证的边界（诚实声明）

- **未重跑 X01 全链**，`validation/x01/observed/*`（2026-11-11 12:58 由提交者产生）我只读取、未独立执行；「X01 pass」不是我的结论。我实跑的只有 §8 的两个探针与 `assessment_invariants_probe.py`。
- **未复算资源上界**（`plan_*` 与 allocator 峰值），本报告不涉及 21 个 `IMPLEMENTED_NOT_VERIFIED` process 的资源正确性。
- **未审 numerics 原语**（`src/numerics`：`jacobi_svd`、`dyadic`、区间算术）；我引用的「因子/白化等价」结论依赖这些原语的正确性（属 B1/N 范围）。
- **未审 `src/geometry/*` 的包含性算法**：`overlap_area` 区间的真实性我按 C0/B3 的既有结论引用，只检查了它的**消费者**（export 是否把区间推进入界）。§8 反例 4 的可达性因此取决于几何核是否真的会给出 `lower_sr=0` 的 contribution（我没有构造出这样的几何输入）。
- **未构造真实的 dyadic-cell 产品**：P2-1 是代码/序列化三角关系的审查结论，未跑出运行时失败。
- **未验证 Windows/MSYS2、ASan/UBSan**；`read_states` 的越界安全性我只依据 `LocalStorage::read` 的范围检查（`local_storage.cpp:257-260`）作静态判断。
- 未改 `src/`、未改他人文档、未 commit；新增文件只有本报告与 `/tmp/d0/` 两个探针。

---

## 附录 A｜复现命令（我实际执行的）

```bash
cd "/workspace/Astro CS Database"
# 公共编译/链接行（build/science 的既有静态库）
INC='-DACSD_DEV_MODE=0 -DACSD_HAS_GENERATED_ASSEMBLY=1 -I src -I include -I . -I validation -O2 -std=c++20'
LIBS='-L build/science -lworkflows -lprojection -lcoadd -lsky -luncertainty \
 -loperators -linformation -lsources -lgeometry -lcoordinates -lcatalogue -lresponse \
 -lphotometry -lobservation -lruntime -lstorage -lnumerics -lcodec -lmodel -lresources \
 -lacsd_builtin -lzstd -lpthread'
/usr/bin/c++ $INC -o /tmp/d0/unit_chain_probe /tmp/d0/unit_chain_probe.cpp $LIBS
/usr/bin/c++ $INC -o /tmp/d0/export_absent_states_probe /tmp/d0/export_absent_states_probe.cpp $LIBS

/tmp/d0/unit_chain_probe \
  validation/x01/input/cli/products/x01-03 \
  validation/x01/input/cli/products/mosaic \
  validation/x01/input/cli/products/export          # exit 1 = 反例 2 成立
/tmp/d0/export_absent_states_probe                  # exit 1 = 反例 1 成立

python3 validation/product/assessment_invariants_probe.py \
  validation/x01/input/cli/products/mosaic \
  validation/x01/input/cli/products/export          # 3+3 objects, 0 violation（覆盖面见 §5.1）
```

两个探针只 include 公开 `include/acsd/contracts.hpp` 与 `validation/stats/harness.hpp`，不含任何被测实现的内部 helper；`unit_chain_probe` 的单位比较用的是 `unit_multiply`（公开、`units.cpp:54-63`）与逐字段相等，不调用 `export`/`coadd` 的任何单位逻辑。`export_absent_states_probe` 的真值是「缺失支持证据必须具名失败」，来自 `projection.export.md` 与 coadd/sky 的同构路径，不来自实现自身。

## 附录 B｜最小修复顺序建议（≤8）

1. **P0-1**：`read_states` 把「`support.states` 缺失」也变成具名失败（或与 coadd/sky 统一为可选并同步放宽它们）；加 `states.size()==leaf_count` 校验。
2. **P0-2**：让 `unit_variance` 接受完整 `Unit` 并保留幂（`unit_square` 反向），生产者改为 `unit_square(signal_unit)`；coadd 用 `unit_square` 的逆**校验**而不是反推；给「反推失败」一条具名错误。
3. **P1-1**：定义 `state==2` 的唯一语义（建议：进入 Q 但 assessment 记 `support_undecided` 且该像元 `science=conditional`；或与 coadd 一致地排除），两侧共用同一个 helper。
4. **P1-2/P1-3**：`total_lower<=0` ⇒ `Code::unavailable`（`support_unresolved`）；`native_area` 保存区间并把 `Ω` 的上下界一起推进 `lo/hi`。
5. **P1-4**：`Q` 的 `spaces.input/output` 按 quantity 给 sr；`C_out.variance_unit` 用 `unit_multiply(unit(Q), unit(C_leaf), unit(Q))` 并与 `unit(signal)` 交叉校验。
6. **P1-5**：所有 `make_identity` 的 payload 至少包含模型内容摘要（`NoiseModel`/`CovarianceAction`/`SignalEstimate` 的 influences、unit、state、omitted），并给「同型不同内容 ⇒ 不同 id」的负例。
7. **P1-6/P1-7**：`bind` 的 `complete` 由 `omitted` 是否为空决定；`parameter_units` 改成带幂的 `Unit` 并进 codec/schema/`quadratic_form` 的块检查，或者明确降级为 metadata 并删掉误导性的 required。
8. **P1-9/P2-4**：coadd 的 `full_leaf_approx` 必须写 `omitted=["support_area_approximated_by_full_leaf"]` 且 `area_lower/upper` 用 `[0, leaf_area]`；support map 的 assessment 写 `.noise`（或给 `make_assessment` 一个「不适用」值），同时把 `SupportMap`/`NativeObservation` 加进 `assessment_invariants_probe.py` 的检查集合并增查 `omitted`。
