# AstroCS 测试元数据与 testkit 合同（TST-001）

> 建立：TST-001（SA-QA-29，wave W1）
> base_main_sha=2a8676bf7546a34af0468d5086e4edd54182a008
> 状态：ACTIVE_NORMATIVE —— 本文件冻结 tests/testkit 结构、测试元数据 schema、
> `module:<module_id>` 选择规则、期望来源禁令与故障注入要求。
> 权威输入：控制包 `11_MODULE_SOURCE_TEST_STANDARD.md`（§5 测试复用、§6 必备类型）、
> 16 号文追溯链（TEST 层）、`docs/traceability/TRACEABILITY_SPEC.md`（DOC-001）。
> 本文件不重复科学公式/容差数值（容差以 docs/science、docs/algorithms 与测试
> oracle 为权威），只规定测试结构的机器合同。

## 1. 目标与范围

- `tests/testkit/` **只含** harness、seed RNG、临时目录管理、artifact fixture、
  report writer 与元数据 schema —— **不含生产算法**（11 号文 §5）。
- 每个测试必须携带机器可读元数据，使：
  1. 测试可按 `module:<module_id>` 选择/执行（验收 A1）；
  2. 期望值来源可审计 —— **禁止生产函数生成期望**（验收 A2）；
  3. harness 对故障注入（断链/坏期望/缺失符号/篡改）必然失败（验收 A3）；
  4. 元数据可被 `check_testkit.py` 全量校验（schema/选择/期望来源/故障注入）。

## 2. 目录结构（tests/testkit/）

```text
tests/testkit/
  testkit.spec.md             # 本文件（规范，冻结）
  schemas/
    test_metadata.schema.json # 测试元数据 JSON Schema（权威，被 Git 跟踪）
  registry.json               # 正式测试注册表（TST-001 建立，registry.example.json 的正式实例；
                              #   每项过 schema、module: 标签匹配 module_id、期望来源非生产函数）
  examples/                   # 六类 label 可运行示例（unit/properties/oracle/fixtures/negative/performance）
    check_constant.py
    property_invariant.py
    oracle_sum.py
    fixture_hash.py
    negative_bad_input.py
    perf_linear.py
    registry.example.json     # registry 模板（复制为 registry.json 的源）
  fixtures/
    demo_fixture.txt          # fixture 示例（断链注入对象）
# 模块测试按 label 分布（11 号文 §6 类型目录）：tests/unit|properties|oracle|
# fixtures|negative|performance/ 由各模块任务按 §6 填充；testkit 只提供合同与 harness。
# harness（module:<id> 选择器 + 故障注入演示）实现在 tools/testkit/check_testkit.py
# （--list/--module/--type/--strict/--fault-injection/--json-out，见 §7）。
```

- 每模块测试的 label = `module:<module_id>`（先例：
  `modules/conformance/noop/CMakeLists.txt` → `add_test(NAME module:astrocs.conformance.noop ...)`）。
- 模块测试放置：单元/负测/性能可放模块共址 `tests/` 或 `tests/<type>/`；
  **关键通过证据**必须有：独立 oracle/期望 + 当前 commit 记录 + 故障注入证明
  （11 号文 §6 末句）。

## 3. 测试元数据字段（JSON，schema 见 schemas/test_metadata.schema.json）

每个测试一个元数据对象（内联于测试源头部注释或独立 `*.testmeta.json`）：

| 字段 | 必填 | 类型/取值 | 说明 |
|---|---|---|---|
| `schema` | 是 | `"astrocs.test-metadata/v1"` | schema 标识 |
| `test_id` | 是 | `^TEST-[A-Z0-9]+(-[A-Z0-9]+)*$` | 追溯链 TEST 层 ID（DOC-001） |
| `label` | 是 | `module:<module_id>` 或 `<type>:` 前缀 | 选择键；`module:<id>` 是模块选择主键 |
| `module_id` | 否* | `^MOD-[A-Za-z0-9]+...$` | *label 含 `module:` 时必填 |
| `type` | 是 | enum：`unit/properties/oracle/fixtures/negative/performance/integration` | 必备类型（11 号文 §6） |
| `language` | 是 | `C/C++/Python/shell` | 语言 |
| `command` | 是 | string | 运行命令（可含 `{root}` 占位） |
| `expected_exit` | 是 | int | 期望退出码（0=PASS） |
| `seed` | 条件 | int | 随机性测试必填；固定 seed 可复现（0/None=无随机） |
| `tolerance_source` | 是 | string | 容差来源引用：`docs/science/<file>` / `docs/algorithms/<file>` / `ORACLE:<id>`；禁止"跑后调阈值" |
| `oracle` | 条件 | string | type∈{oracle,properties,performance} 建议填：独立参考来源（解析解/朴素高精度/隔离库），**禁止生产 symbol** |
| `expectation_source` | 是 | enum：`INDEPENDENT_ORACLE/PROPERTY_INVARIANT/PRE_FROZEN_VALUES/FIXTURE_HASH/MISSING` | 期望来源类别；`MISSING` 时检查器 WARN（关键证据禁止） |
| `current_commit` | 是 | 40 hex | 测试所针对的提交（由 run 时注入/校验） |
| `config_digest` | 否 | 64 hex | 配置摘要（可选） |
| `providers` | 条件 | array[enum] | performance/provider 测试必填：`baseline/avx2/avx512` |
| `workers` | 条件 | int | performance 测试必填：worker 数（禁固定私有池，禁读 hardware_concurrency 硬编码） |
| `fixtures` | 否 | array[string] | 依赖 fixture/生成器 |
| `input_hash`/`output_hash` | 否 | 64 hex | 输入/输出摘要（可选） |
| `notes` | 否 | string | 备注 |

未知字段禁止写成空字符串；未知状态写 `MISSING` 并 FAIL（对齐 module.yaml 同规则）。

## 4. module:&lt;id&gt; 选择规则（验收 A1）

- 选择键：`module:<module_id>`，`module_id` 取模块权威 id（module.yaml `module_id`
  或 DOC-001 追溯矩阵 `module_id` 的 `MOD-` 行键对应点分名，如
  `module:astrocs.conformance.noop`）。
- `harness/run_selector.py` 行为（由 `tools/testkit/check_testkit.py` 实现，见 §7）：
  - `--module <module_id>` → 只执行 label==`module:<module_id>` 的测试；
  - `--type <type>` → 按 type 过滤；
  - `--list` → 输出全部已知测试（稳定排序）；
  - 未知 module → 非 0 + `UNKNOWN_MODULE <module_id>`（不崩溃）；
  - 匹配 0 个 → 非 0 + `NO_TESTS_MATCH`（不伪 PASS）。
- 测试注册源：`tests/testkit/registry.json`（手工维护 + check_testkit 校验一致性）或
  由共址 `module.yaml test_ids` 推导；选择器读 registry 的 `command` 执行并校验
  `expected_exit`/元数据。

## 5. 期望来源禁令（验收 A2）

- **禁止**测试调用与被测生产函数同一 symbol/同一公式实现来生成期望（“用被测代码
  证明被测代码”）；允许：解析解、朴素高精度参考、隔离 test-only 库、固定 fixture
  的预冻结值/独立 hash。
- 机器检查（check_testkit.py）：
  - 若 `oracle`/`expectation_source` 引用生产符号或与 `src_path::symbol`
    （DOC-001 矩阵 SRC 层）相同符号 → `FORBIDDEN_EXPECTATION_SOURCE`（FAIL）；
  - `expectation_source==MISSING` 且 type≠negative → `MISSING_EXPECTATION`（WARN，
    --strict 为 FAIL）；
  - oracle 字段引用 `lib/` 生产路径或生产 symbol 文本 → FAIL。

## 6. 故障注入（验收 A3）

- harness 必须证明“故障注入会失败”：对任意测试注入以下任一故障 →
  检查器/执行器报告失败而非 PASS：
  1. 篡改期望（期望值 ±1 ulp / 常量翻转）→ 断言失败；
  2. 断链（删除 oracle 引用的 fixture/文件）→ 非 0 退出；
  3. 破坏独立 oracle 独立性（把 oracle 换成生产 symbol）→ FORBIDDEN_EXPECTATION_SOURCE；
  4. 篡改 seed（不同 seed）→ 统计测试边界外失败或元数据不一致。
- `tools/testkit/check_testkit.py` 对 `tests/testkit/fixtures/` 下的示例测试做
  三类注入（F1 篡改期望 / F2 断链 fixture / F3 oracle 换生产符号），每次注入后
  harness 必须 FAIL（exit 非 0）—— 提供可复现证明。

## 7. 机器闭环

`tools/testkit/check_testkit.py`（TST-001 落地）：
- 校验 `tests/testkit/schemas/test_metadata.schema.json` 存在且为合法 JSON Schema；
- 校验 registry 每项过 schema、id 唯一、label 合法、`module:` 标签 module_id 匹配；
- 校验 `expectation_source` 非空且不指向生产符号；
- `--fault-injection` 演示：篡改期望/断链 → harness 必须 FAIL；
- exit 0 = `TESTKIT_PASS`；任何未捕获异常 → `TOOLING_FAILURE` exit 3。

## 8. 与其他文件关系

- 追溯：TEST 层 test_id 进入 `docs/traceability/TRACEABILITY_MATRIX.json`
  （DOC-001 建立，后续模块任务补 VERIFIED 行）；
- 类型目录 tests/properties|oracle|negative|performance 由各任务按 11 号文 §6 填充，
  testkit 只提供合同与 harness。

## 9. 状态与演进

- 本文件 + schema + harness + check_testkit 由 TST-001 建立；
- 只可在负责人确认后修订；科学公式/容差不在此定义。
