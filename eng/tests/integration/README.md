# 集成层：合同与接口 · 方差传播

本目录是 ACSD 测试集的**集成层**。这一层判的是**多模块协同的合同面**：统一对象的
schema 符合性、对象判别式与端口的配对关系、单位与缺失值语义、以及跨模块方差传播
公式的解析一致性。

## 1 本层范围

| 维度 | 内容 |
|---|---|
| 读什么 | 仓内 JSON 合同与 schema、正例/负例夹具、两份登记表、两个 C 头文件的公式注释 |
| 不读什么 | 产品二进制、运行产物、真实数据 |
| 不做什么 | **不链接 `libacsd`、不调 CLI、不起子进程、不编译、不跑构建、不跑端到端** |
| 输入面 | `eng/contracts/schemas/unified/**`、`eng/contracts/data/*.json`、`lib/algorithms/integration/phase2_integrate/include/acsd/{variance_propagation,weight_chain}.h` |
| 实算对象数 | 14 个 schema 文件（13 个 canonical 数据对象 + 1 个端口合同）、14 个正例夹具、6 个负例夹具、5 环方差链、2 组数值注入向量 |
| 用例数 | 31 条：29 条绿 + 2 条红（两条红同源，见第 7 节） |

文件清单：

- `_object_contracts.py` —— **不含测试函数**，只提供共用工具面：自研 JSON-Schema
  子集校验器、统一对象合同解析、`TEST.md` 第 4 节冻结容差的取值函数。
- `test_variance_propagation.py` —— 本层的判据面。文件顶部 docstring 给出
  函数名 ↔ `Suite.Feature` 映射表（`Suite = ContractVariance`）。
- `README.md` —— 本文件。

## 2 正本依据（本层每条断言的出处）

| 判据面 | 正本 | 引用到的具体位置 |
|---|---|---|
| 通用浮点容差与 NaN/Inf 语义 | `docs/engineering/testing/TEST.md` 第 4 节 | `:44-50`（冻结表）、`:52-65`（`u` 与 `γ_n`）、`:71-73`（可满足性下限）、`:75-82`（NaN/Inf） |
| 覆盖判据（非退化、恒真无资格） | 同上 第 2 节 | `:26` |
| 负例必须可红 | 同上 第 5 节 | `:90` |
| 统一对象语义 | `docs/science/unified/DATA_SEMANTICS.md` | `:73-93`（三态编码）、`:96-130`（BUNIT 推导与二次律）、`:132-143`（标度类别与线性标度律） |
| 方差传播 | `docs/science/noise_snr/NOISE_SNR.md` | `:343-349`（信息量与 `Var(F̂)`）、`:372-410`（协方差传播） |
| 逆方差叠加 | `docs/science/algorithms/PHASE2_INTEGRATION.md` | `:113-121`（`signal = vs/wsum` 逐公式定义） |
| Oracle 独立性 / 判别力 / fail-closed | `docs/engineering/testing/VALIDATION_EVIDENCE.md` | `:9-16`（Oracle 独立性）、`:114-121`（第 4.2 节模式表）、`:168-178`（零对象守卫）、`:180-198`（S1–S7，`:193` 是 S6）、`:410-414`（fail-closed 与锚存活） |
| 退化判据的明文登记 | `docs/science/algorithms/GATES_AND_TOLERANCES.md` | `:122`（接缝退化判据） |
| 权重链公式与 fail-closed | `lib/algorithms/integration/phase2_integrate/include/acsd/weight_chain.h` | `:27-31`（逐像素权重式）、`:34-43`（fail-closed 纪律）、`:45-50`（单位）、`:390-395`（禁止再乘帧级 SNR）、`:396-404`（签名） |
| 残差制造者方差 | `lib/algorithms/integration/phase2_integrate/include/acsd/variance_propagation.h` | `:12-15`（正确式）、`:19-22`（错误式与其 `9/7` 读数）、`:24-25`（`÷g²` 硬要求）、`:57-60`（`naive_variance` 仅红例）、`:70-77`（生产总入口）、`:82-84`（比值诊断） |
| 冻结 schema | `eng/contracts/schemas/unified/*.schema.json` | 各文件 `x-acsd-object.authority` 与 `x-acsd-gate` 标注的条款号（如 `variance.schema.json:490-495` 的 `FZ-BUNIT-SEMANTICS`） |
| 负例期望清单 | `eng/contracts/schemas/unified/negative/EXPECTED.json` | 仓库自带，非本层生成 |

## 3 Oracle 独立性与 `oracle.must_not`

`VALIDATION_EVIDENCE.md:9-16` 要求每条判据声明 `oracle.truth` 与 `oracle.must_not`。
本层的取值如下：

| 判据组 | `oracle.truth` | `oracle.must_not` | 说明 |
|---|---|---|---|
| A1 / A2 / A3 / A5 / A5b / A6 / A10 | `structural` + 仓库自带期望清单 | 产品可执行程序、`libacsd`、任何产品产物 | 被测对象是**仓内合同工件**（schema、夹具、登记表），真值来源是仓库自带的 `EXPECTED.json` 与冻结 schema 文本 |
| A4 / B7 | `structural`（校验器自检） | 同上 + 本校验器自身的实现输出 | 证明自研校验器非恒真 |
| A7 / A8 / A9 / B10–B14 | `independent_stdlib`（`fractions.Fraction` 精确有理数）+ `independent_numpy`（矩阵参考实现） | 同上 | 见第 4 节 |

**本层不消费任何产品行为面**，因此 `must_not` 天然成立。这一点必须说清楚：本层判的是
**合同面**，不是**产品行为面**。A7/A8/A9 的数值判据证明的是「冻结公式与注入值一致、
且判据抓得住交叉项与平方律」，**不能**读作「产品实现符合该公式」。要判后者必须先构建
产品并链接 `libacsd`，见第 7 节。

## 4 跨帧量的纪律（本层最硬的约束）

仓内三处明文登记了**恒等式型退化判据**，本层全部避开：

1. `docs/science/algorithms/GATES_AND_TOLERANCES.md:122`
   「把整张背景减掉后再比帧间差是**退化判据**（背景归零时差值天然为零）」；
2. `docs/science/noise_snr/NOISE_SNR.md:353-359`
   「信噪比平方之和」是**定义式恒等式**，「对任何可达输入恒成立，结构上不可能给出判决」；
3. `lib/.../weight_chain.h:424-438`
   `dimensional_identity = weight·F_ref,k² − layer_snr²·gain²` 是同源相减型退化，
   已被上游删除并注明「任何科学错误都不会让它动」。

因此本层所有跨帧数值判据遵守三条：

1. **注入值是本文件写死的十进制字面量**（`INJECTED_*`），不来自任何产品输出、
   现有 JSON 产物或运行读数；
2. **期望值由 `Fraction` 精确有理数从正本公式逐项展开**，与参考实现的浮点路线
   （`numpy` 显式矩阵）**不同源**；两者是两条独立代码路径；
3. **禁止任何 `A − A` 形式的恒等式当预期值**。每条数值判据都配一条
   「注入错误式必须转红」的负例（B10–B14），由 A4/B7 之外的独立机制证明有牙。

两条注入向量：

- `INJECTED_SIGMA2 = (4, 9, 16, 25, 36, 49, 64, 81)` —— 8 帧等权均值模型
  （`include_self=true`，`H_ij = 1/N`），逐帧方差**互不相同**，判据不能靠均匀化简侥幸通过；
- `INJECTED_SIGMA2_NONEXACT = (0.1, 1/3, 7/11, e, 13/7, 0.3333333333333333, 101/17, 1e-3)`
  —— **不可被 binary64 精确表示**，用来把残差逼到 ulp 量级，使归约档门限真正被走到
  （第一组是小整数，binary64 可精确表示，残差恒为 0，门限不触发 ⇒ 容差判据会退化成装饰）。

> `injected()` 必须走 `Fraction(str(x))` 而不是 `Fraction(int(round(x)))`：
> 后者是银行家舍入，会把 `0.5` 变成 `0`、把 `118.4` 变成 `118` —— 那不是「我构造的
> 注入值」，而是被悄悄改写过的另一个数，正是本层要防的那类错误。

## 5 容差冻结表（逐字抄录，正本 = `TEST.md:44-50`）

| 量类 | 取值 |
|---|---|
| 元数据、掩膜、计数、索引、端口、选择结果 | 精确一致 |
| 双精度非归约 | `rtol = 1e-12`，`atol = 1e-13 × scale` |
| 单精度产品非归约 | `rtol = 5e-6`，`atol = 1e-6 × scale` |
| 归约 | `γ_n = n·u/(1−n·u)`，门限 `C·γ_n·Σ|terms| + atol`，`C ≤ 4` 事前冻结 |
| 并行等价（1/N worker、基线与变体） | 上述三档容差等价，不是逐位一致 |

单位舍入（`TEST.md:52-65`）：

| dtype | `u` | 数值 |
|---|---|---|
| IEEE 754 binary64 | `2⁻⁵³` | `1.1102230246251565e-16` |
| IEEE 754 binary32 | `2⁻²⁴` | `5.9604644775390625e-08` |

本层实际落哪一档：

| 判据 | 档 | `n` 取值 | `scale` 适用量级域（`TEST.md:71-73` 必填） |
|---|---|---|---|
| A7 守恒映射方差、逆方差 | 双精度非归约 | —（非归约） | `variance_p`：1e0–1e2 ADU²/sr²（注入落在 0.67–2.0）；`ivar`：1e-2–1e2 ADU⁻²（注入落在 0.44–1.5） |
| A7 线性标度律、面亮度口径 | 双精度非归约 | —（非归约） | 同上族；`Var(S)` 域 1e-2–1e2 ADU²/sr²（注入落在 0.75） |
| A7 逆方差叠加 | 双精度非归约 | —（非归约） | 合成通量 1e0–1e8 ADU（注入落在 17.05）；合成方差 1e0–1e2 ADU²（注入落在 2.36） |
| A8 `PΣPᵀ`、B10 | **归约** | **`n = N = 8`（实际项数，不是数据总像素数）** | 单帧逐像素方差 1e0–1e2 ADU²（注入落在 7.4–21.0） |
| A9 权重与跨帧比值、B12 | 双精度非归约 | —（非归约） | `w`：1e-6–1e2 ADU⁻²（注入落在 4.0e-4–4.0e-2） |
| A1–A6、A10、A11、B1–B9、B13、B14 | **精确一致** | — | 元数据/掩膜/计数/索引/端口/选择结果一律精确，不适用浮点容差 |

每条数值判据在断言前都显式检查 `atol ≥ 1 ulp(scale)`（`TEST.md:73` 的可满足性下限），
不满足即判红并点名。

## 6 不产出阻塞退出码的声明

本目录是**工具，不是门禁**。具体地：

- 本层测试代码**不含** `sys.exit` / `SystemExit` / `os._exit` / `pytest.skip` /
  `pytest.xfail` / `pytest.importorskip`；
  判据是 `test_ContractVariance_NoBlockingExitCodeInTestCode`（A11），**用 AST 的
  `Call` / `Raise` 节点**扫描整个 `eng/tests/integration/` 与 `eng/tests/module/`
  两棵树（实算 8 个 `.py`），不砍前缀、不做文本匹配；
- A11 的有牙自证 = `test_ContractVariance_NegBlockingExitAfterA11`（B15）：
  在临时副本的 A11 **之后**注入 `return sys.exit(0)` ⇒ A11 必须判红
  （实测读数 `injected.py:2522 调用 sys.exit`），且 docstring/注释里的同名字样 0 误报；
- 本层**不产出流水线判决**、不裁决合入。`TEST.md:203`：
  「红项是缺陷信号，按修复或回退处理；测试不裁决合入，合入由提交纪律决定」；
- 本层**不做豁免**：没有 `xfail`、没有容差放宽、没有「已知问题」白名单。

## 7 如实登记的红项（缺陷信号，非本层缺陷）

### 7.1 `variance.example.json` 缺 `correlation_kernel.oracle_ref` —— A1 与 A5b 同源一条

**读数**：`test_ContractVariance_SchemaPositiveExamplesValidate`（A1）与
`test_ContractVariance_EachChainRingValidatesOwnSchema`（A5b）**判红**。
其余 13 个正例夹具全部零错误通过；方差链五环里只有 `variance` 一环坏。

**证据**（复核者可独立复算，不依赖本校验器）：

- `eng/contracts/schemas/unified/variance.schema.json:341-345`
  `correlation_kernel.required = ["family", "support_radius_px", "oracle_ref"]`，
  `oracle_ref` 在同文件 `:355-358` 的 `properties` 中已声明；
- `eng/contracts/schemas/unified/examples/variance.example.json` 的
  `correlation_kernel` 键集只有 `["boundary_definition", "family", "support_radius_px"]`。

**两种读法并列登记，本层不替负责人裁定**：

1. schema 新增必填键 `oracle_ref` 后正例夹具未同步 ⇒ 修复面 = 补夹具字段；
2. 夹具有意不声明 `oracle_ref`（视为可选项）⇒ 修复面 = 放宽 schema 的 `required`。

两条读法的修复面都在正本变更面，不在本单授权内。本层**既不改夹具也不改 schema
也不放宽判据**。

**对负例 A2 的影响（已显式钉住）**：`n2_source_snr_as_variance` 的目标 schema 是
`variance`，该夹具同样不带 `oracle_ref`，因此除它本来的违规外还会**额外**报一条
`/correlation_kernel[required] missing='oracle_ref'`。A2 用的是 **`must_match` 子集判定**
（要求每条 `must_match` 都被命中，**不**要求错误集精确相等、**不**禁止额外错误），
所以 n2 仍**逐条命中** `unified_object:const` / `object_schema_id:const` /
`variance_value`，与 `oracle_ref` 那条毫无关系 —— 负例有效性不受影响。
A2 内有一条守护断言 `len(hit) == len(must_match)`，0 命中蒙混不过去。

### 7.1b 第三态判据的**未覆盖**面（fail-closed 登记，不写成永远绿的测试）

`TEST.md:79` 的「非有限值与缺失的**位置集合**必须与产品逐项精确一致」这半条
**本层未覆盖**。它需要产品产出的像素面（两者各自的实际位置集合）；本层读的是
标量 JSON 合同文档，仓内没有该对象面（需构建产品并跑真实数据面，见第 9 节）。

A10 原版曾用两个**自造同值**的集合 `positions_nan = {3,7,11}` 与
`positions_missing = {3,7,11}` 取 `==` 冒充这条判据，并用 `math.isnan(float("nan"))`
冒充「非有限值不被折叠」—— 两者都是 `A−A=0` 型恒真（`TEST.md:26`
「恒真的比较没有证据资格」）。**已删除**，不迁就、不留恒真。

A10 现只保留能在 **14 个真实夹具 × 14 个真实 schema** 上判红的那一半：
①每个夹具的 `missing_repr` / `invalid_repr` 必须落在**其自身 schema 的枚举**内；
②`missing_repr != "nan"`（`TEST.md:80` 的可判定规则）；
③折叠守卫：缺必填键的 `ivar` 实例必须被 `ivar` schema 拒绝（不是裸 float 断言）；
④缺失值族的**实算分布**读出来（当前 4 个夹具落在 `('zero','zero',False)` 族，
其余 10 个落在 `('null','nan',True)` 族）——任一夹具漂移即判红。

### 7.1c §4(f) 登记的校验器退化已固化为正式负例 B16

交付报告 §4(f) 记录的两个退化 —— **`if`/`not`/`anyOf` 子模式错误混进主错误表**
（M1：恒取 `then`）与 **子模式布尔值不上传**（M2：丢弃 `walk()` 返回值）——
现固化为 `test_ContractVariance_NegValidatorIfThenErrorLeak`（B16）。
实测读数：M1 使 **13/14** 个干净正例转红（与 §4(f) 的记录一致）；
**M1+M2 呈现「5 条错误但 `ok=True`」的假绿形态** —— 这是任何只读错误表的比较器
都发现不了的形状。

## 7.1d 对抗复核发现与处置

只读对抗复核指出本层 6 处不合格，逐条已修，基线红项未变（见交付报告同名节）：

| 复核指出的问题 | 最小反例 | 修法 | 复跑读数 |
|---|---|---|---|
| A11 用 `text.split(...)[0]` 砍前缀 ⇒ 只扫自身之前，`sys.exit` 放进 A11 之后扫不到 | 该判据对最该抓的位置 fail-open | 改 AST `Call`/`Raise` 扫描，不砍前缀；扫描面扩到 `integration/` + `module/` 两棵树 | A11：AST 扫描 8 个文件 0 违规；B15：注入后 `injected.py:2522 调用 sys.exit` 判红；docstring/注释 0 误报 |
| `assert "signal" not in mode_rows`（`mode_rows` 是硬编码 5 字面量）⇒ 恒真 | `TEST.md:26` 教科书案例 | 新增 `parse_mode_table()`，从 `VALIDATION_EVIDENCE.md` 第 4.2 节**解析**表格，逐行断言权重对象/单位/权威式非空且不含 `signal` | A6：模式表解析 **6** 行（原硬编码只有 5 行，漏 `psf_snr_power`） |
| A10 后半段自造同值集合互比 ⇒ 恒真 | `{3,7,11} == {3,7,11}` | 删除；改为 14 真实夹具的枚举一致性 + 折叠守卫；位置集合那半条登记为**未覆盖**（见 7.1b） | A10：族实算 `('zero','zero',False)`×4；折叠守卫 11 条 required |
| `assert len(files) == 14` 无来源魔数 | `VALIDATION_EVIDENCE.md:192` S5 | 改 `> 0` + 与磁盘实算 schema 文件数、登记表实算数对账 | A0：正例夹具 14 / schema 文件 14 / canonical 对象 13，实算对账一致 |
| B7 的 S6 资格不成立（注入点在共用参数、路径(ii) 构造性恒真、没复跑判据本体） | `skip_required` 被 A1/A2/A3/B1–B6 共用 | 抽出 `judge_positive_examples`（A1 本体）与 `judge_negative_expectations`（A2 本体），对**本体**注入并断言 verdict 逐 fixture 翻转 | B7-A1：`['variance.example.json'] → []`；B7-A2：n2 `required×4→0` 且 hit `3→2`、n3 `required×1→0` 且 hit `3→2`，两条均转为「A2 有问题」 |
| `_object_contracts` 两条 latent fail-open | ①`validate("x", {"type":"string","x-must-be-positive":True})` ⇒ `ok=True`；②`validate(["a",5], {"items":[…]})` ⇒ `ok=True` | ①`x-` 改白名单放行（`ANNOTATION_PREFIXES = ("x-acsd",)`），其余 `x-*` 报 `UnsupportedKeyword`；②数组形 `items` 具名报 `UnsupportedKeyword` | B17：`x-must-be-positive` 判红、`x-acsd*` 仍放行；B18：数组形判红、对象形正常工作；14 个 canonical schema 的 `UnsupportedKeyword` 命中数 **0**（未丢覆盖面） |

复核**认可并要求保持**的三条未改：A1/A5b 的红项（`variance.example.json` 缺
`oracle_ref`，真实产品缺陷）、A2 的 `must_match` 子集语义、B2 的反向自证与
B10/B11 的解析注入与 B12 的签名面排他性。

**复核指出的 `PC-C5` 第 4 子句（`EXTERNAL_INPUT_CARRIER`）缺负例 —— 不在本单授权面。**
`grep -rn "PC-C5\|EXTERNAL_INPUT_CARRIER"` 实测全部命中都在
`eng/tests/integration/_registry_parity.py` 与 `eng/tests/integration/test_registry_code_parity.py`
（另一并行代理的两个文件，本单明令不得触碰）。本层三个文件里该标识**零命中**，
本层不产生 PC-C5 基线红项，故该缺口不属于本单，已回报前台转派给该代理。

### 7.2 `ivar` 的缺失表示与 `DATA_SEMANTICS` §3.3 冲突（待裁决，未编码为断言）

`examples/ivar.example.json` 声明 `missing_repr="zero"`、`invalid_repr="zero"`、
`nan_in_binary=false`（与 `coverage` / `rejection` / `support` 同属 `zero/zero/false` 族），
而 `docs/science/unified/DATA_SEMANTICS.md:88` 的三态编码表给出
「无覆盖 ⇒ `ivar = NaN`」。二者不能同时为真。

本层**不替负责人裁决**：A10 只断言 `TEST.md:80` 的可判定规则那一半
（**缺失不得由 NaN 承载**，即 `missing_repr != "nan"`），并对族二
（`ivar` / `coverage` / `rejection` / `support`）断言其冻结 `missing_repr == "zero"`。
严格四元组 `null`/`nan`/`true`/`true` 只对声明 `missing_repr="null"` 的族一
（`signal` / `variance` / `frame_snr` / `sparse_snr_layer`）断言。

### 7.3 schema 方言差异（不影响判定，已核对）

派单书写「draft-07」，而 14 个 schema 文件的头部写的是
`"$schema": "https://json-schema.org/draft/2020-12/schema"`（例如 `signal.schema.json:2`）。
对本层实际用到的关键字子集，两个方言**逐项语义一致**（`exclusiveMinimum` 在两版都是
数值型，`items` 只用数组形式，且全部 schema 未用 `$ref` / `definitions` / `format`），
故本校验器同时对两版成立。

## 8 自研校验器为什么必须被约束

`jsonschema` 包在本机不可用（实测 `ModuleNotFoundError: No module named 'jsonschema'`），
因此 `_object_contracts.py` 用纯标准库实现了一个 JSON-Schema 子集校验器。
**自研校验器若不校验就没有证据资格**，所以本层给了三层约束：

1. **A4 `ValidatorSelfCheckNonVacuous`** —— 四条独立注入（删 `required` 键、改 `const`、
   加裸 `weight` 键触发 `propertyNames`、改 `enum`），每条都必须让读数从绿翻红；
2. **B7 `NegVacuousValidator`** —— 把校验器对 `required` 的检查弄失效（两条注入路径：
   开关式 `skip_required=True`，文件式「临时副本里把 `required` 列表清空」），
   **读数必须发生可观测的红→绿翻转**，否则 `required` 分支是空转的；
3. **未知关键字 fail-closed** —— `SUPPORTED_KEYWORDS` 之外的键被报成
   `UnsupportedKeyword` 错误，而不是静默忽略。静默忽略会把「schema 写了本层没实现的
   约束」读成「通过」，属 `VALIDATION_EVIDENCE.md:412` 禁止的
   「读不到就按全部合规处理」。

错误定位编码：`location`（相对路径，`/` 分隔，顶层为 `""`）、`pointer`（RFC 6901）、
`keyword`。`EXPECTED.json` 的 `must_match` 条目读法（由
`_object_contracts.hit_expectations` 实现）：

- 带 `:关键字` 的条目 ⇒ 位置与关键字**都精确相等**；
- 不带冒号的条目 ⇒ 命中「该位置上的错误」「该位置以下的错误」或「该位置缺这个
  `required` 键」三者之一。

## 9 需要先构建产品才能跑的测试（本层**不含**，仅登记）

以下判据**不属于本层**，因为它们要链接 `libacsd` 或起子进程。本层只做合同面，
不代做、不假装做：

| 判据 | 需要什么 | 为什么本层不做 |
|---|---|---|
| `residual_maker_variance` / `naive_variance` / `corrected_pixel_variance` 的**实现**符合 `variance_propagation.h:12-15` | 编译 `phase2_integrate` 并链接 | A8 只判**公式**与注入值一致；实现的 fail-closed 返回值、H 行合法性、参数校验分支需要真实符号 |
| `weight_from_sparse_layer_pixel` 的**实现**在层缺失/越界/非正值时确实返回 fail-closed | 编译 `phase2_integrate` | A9/B12–B14 判的是**规则**的有牙性（注入错误式必红），不是 C++ 实现的返回码 |
| `SparseSnrReconstructor` 的四种重建算子（自然三次样条 + 钳制 / 加 3×3 中值 / 双线性 / 最近控制点）的数值复现 | 编译 + 控制网格夹具 | 属模块层判据 |
| `p2_integrate_pixel` 的并行等价（1 worker vs N worker） | 编译 + OpenMP 运行时 | `TEST.md:50` 的并行档需要真实调度 |
| 三命令端到端（`normalize` → `mosaic` → `export`）产物的 schema 符合性 | 构建全部二进制 + 真实数据 | 属端到端层 |

以上清单里的任何一条，本层都**不**给出「已通过」的读数；本层的绿灯只支撑
「合同面成立」这一个结论（`VALIDATION_EVIDENCE.md:164`
「给不出注入负例的判据不进清单」的同类纪律：给不出证据资格的不许被读作已执行）。