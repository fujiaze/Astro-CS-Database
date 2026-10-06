# eng/tests/module —— 模块层测试

本目录是 ACSD 测试集的**模块层**（module layer）部分，与 `eng/tests/unit`（单元层）、
`eng/tests/conformance`（安装面）、`eng/tests/validation`（验证数据）并列。

## 1 目录内容

| 文件 | 作用 |
|---|---|
| `_blockflow.py` | 共享解析器与判定逻辑（纯标准库）。锚装载（fail-closed）+ 独立 Oracle + A1–A9 的 `judge_*` |
| `test_block_lifecycle.py` | 命名块生产/消费与生命周期用例（含旧块是否及时释放） |
| `README.md` | 本文件：判据清单、登记项、负例有效性自证表、恒真/恒假判据 |

## 2 运行方式

```bash
python3 -m pytest eng/tests/module/ -v
```

纯静态 / 纯数据测试：只读仓内 JSON 与源码文本，**不链接 libacsd、不调 CLI、不起子进程、
不需要构建整个产品**。实测读数（HEAD `bf944fad`）：

```
$ python3 -m pytest eng/tests/module/test_block_lifecycle.py --collect-only -q
25 tests collected in 0.15s        # 收集返回 0 条错误

$ python3 -m pytest eng/tests/module/ -q
25 passed in 0.22s
```

> **收尾纪律提示**：另一个代理并发编辑本目录时，`--collect-only` 会返回 RC=2 且一条不跑
> （半成品窗口）。**报数前必须先跑 `--collect-only -q` 确认 0 条错误**。

## 3 本层不产出阻塞退出码

依据 `AGENTS.md` §8「测试集可接入持续集成，但以非阻塞为常态」与
`docs/engineering/testing/TEST.md:115`「执行者是人…测试集不产出流水线判决」：

- 本目录**不注册进 CMakeLists、不接 CI**，因此不会进入默认构建；
- 测试代码里**不出现** `sys.exit` / `raise SystemExit` / `os._exit`；判红只由 pytest 的
  `AssertionError` / `Failed` 承载，由人读对抗性审核消费；
- 本目录**不裁决代码**。判红是缺陷信号，处置按 `TEST.md:203`「红项是缺陷信号，
  按修复或回回处理；测试不裁决合入」。

## 4 正本依据与容差档

### 4.1 判据 → 正本（逐条 `文件:行`）

| 判据 | 正本依据 |
|---|---|
| A1 生命周期按文档规则独立重算 | `eng/tools/quality/gen_block_flow_spec.py:7-13`（docstring 5 条规则逐字）；`:24` 阶段映射；`:30-32` `STAGE_TERMINALS`；`:50` 节点排序键；`:52-61` 阶段作用域统计 |
| A2 唯一生产者（DAG 第 1 条） | `docs/engineering/contracts/PIPELINE_BLOCK.md:41` |
| A3 无悬空消费（DAG 第 2 条） | `PIPELINE_BLOCK.md:42` |
| A4 无重复生产（DAG 第 3 条） | `PIPELINE_BLOCK.md:43` |
| A5 生命周期与消费者跨度（DAG 第 4 条） | `PIPELINE_BLOCK.md:44` |
| A6 阶段终产物集合（**7 个**） | `gen_block_flow_spec.py:30-32`；`docs/ACSD_DESIGN.md:364-379`；`PIPELINE_BLOCK.md:66` |
| A6 跨阶段唯一载体 = HiPS 产品树 | `PIPELINE_BLOCK.md:20-21` |
| A7 全部消费者用完即销毁 | `PIPELINE_BLOCK.md:54`；`docs/ACSD_DESIGN.md:384` |
| A7 拓扑不倒置 | `PIPELINE_BLOCK.md:71`（负例第 1、2 条） |
| A8 `consumers` 可空 ⇒ 终态块 | `PIPELINE_BLOCK.md:36` |
| A8 阶段结束块残留数 = 0 | `PIPELINE_BLOCK.md:55`；命名块不跨阶段 `:17` |
| A9 块名小写蛇形正则 | `eng/contracts/schemas/pipeline_block.schema.json:32` |
| A9 `consumers` 为 string 数组 | `pipeline_block.schema.json:57-61` |
| A9 schema 侧 `lifecycle` 枚举 | `pipeline_block.schema.json:68-74`；`PIPELINE_BLOCK.md:37` |
| B8 非退化下界 | `ACSD_DESIGN.md:364-379`（3 个平级命令）；`gen_block_flow_spec.py:24,30-32,64`；`PIPELINE_BLOCK.md:78`（PC-C6） |
| fail-closed / 锚存活 | `docs/engineering/testing/VALIDATION_EVIDENCE.md:412-413` |
| 零对象守卫两条 | `VALIDATION_EVIDENCE.md:170-176` |
| 判别力 S2 / S6 / 反例隔离 | `VALIDATION_EVIDENCE.md:189,193,197` |
| 恒真比较无证据资格 | `TEST.md:26` |
| 负例必须可红 | `TEST.md:90` |

### 4.2 容差档

本层**不比较任何浮点量**。全部比较对象是元数据、端口名、节点选择结果与计数，
落 `docs/engineering/testing/TEST.md:46` 第一档「元数据、掩膜、计数、索引、端口、选择结果
= **精确一致**」。因此本层**不引入 `rtol` / `atol`**，
`TEST.md:73` 的「绝对容差可满足性下限（`atol ≥ 1 ulp(scale)`）」在本层不适用
（无绝对容差、无适用量级域 `scale`）。NaN/Inf 语义（`TEST.md:75-82`）在本层不适用
（无浮点比较面）。**容差在写用例前已冻结**（`TEST.md:36`）。

### 4.3 独立 Oracle 来源

- `oracle.truth = structural`（`VALIDATION_EVIDENCE.md:11` 白名单项）；
- 规则文本：`gen_block_flow_spec.py` 文件头 docstring 的 5 行**逐行断言仍在**
  （`_blockflow.RULE_LINES`），`STAGE_TERMINALS` 用 `ast.literal_eval` 从**源码文本**提取
  （**不执行**该脚本）；
- 重算输入：`module_ports.registry.json` 的端口 `direction` / `carrier`，按阶段作用域；
- `oracle.must_not` = 产品可执行程序与调度器代码面。本层**只**读仓内 JSON 与源码文本；
- **不读块流规格的现值生成任何预期值**。`stage_block_flow.json` 在全部判定中是**被测对象**。

## 5 实算对象数（`VALIDATION_EVIDENCE.md:173` 的 `n_objects`）

| 量 | 值 |
|---|---|
| `n_nodes` | 20（export 5 / mosaic 7 / normalize 8） |
| `n_modules` | 20 |
| `n_blocks` | 36（normalize 18 / mosaic 10 / export 8） |
| `n_stages` | 3 |
| `n_prod_consume_edges` | 57 |
| 生命周期分布 | `STAGE` 16 / `EXTERNAL_IN` 8 / `EXTERNAL_OUT` 7 / `SHORT` 5（合计 36） |

## 6 作用域声明（写用例前冻结）

以下三处**作用域**是本层判定的前提，逐条写明并在 docstring 里重复：

1. **阶段作用域**：`gen_block_flow_spec.py:52-53` 注释逐字「**按阶段作用域**：阶段间只通过
   磁盘产品交换，同名块在不同阶段是不同块」。故一切判定以 `(stage, block)` 二元组为键。
   同名块跨阶段出现（实测 `frame_hips`、`mosaic_hips` 各 2 处）**不是**重复生产，
   由 `test_..._CrossStageSameNameIsNotDuplicateProduction` 作作用域锚。
2. **`EXTERNAL_IN` 是合法类别**：`PIPELINE_BLOCK.md:21`（`config_path`，本阶段无生产者）
   + `gen_block_flow_spec.py:12`。故 `PIPELINE_BLOCK.md:41` 的「有且仅有一个生产者」
   在阶段作用域下**不覆盖** `EXTERNAL_IN`；「被消费但无生产者且未声明 `EXTERNAL_IN`」
   那一支归 A3 判（`:42`），不与 A2 重复记账。
3. **A5 只判「有生产者、非终产物、消费者数 ≥1」的块**。`EXTERNAL_IN`（无生产者，无跨度）、
   阶段终产物（发布义务优先于内存跨度，`PIPELINE_BLOCK.md:66`）、零消费者块（按
   `PIPELINE_BLOCK.md:36` 是终态块，跨度判据不适用）三类**根本不进入 A5 的判定域**，
   而不是写成永远绿的分支。

## 7 负例有效性自证表

**隔离**：全部负例在临时目录（`tmp_path`）的副本上注入，**不写仓内留证**
（`VALIDATION_EVIDENCE.md:197`「反例复跑必须隔离」）。
**红侧归属**：每条负例都调用**被判定的那个正例函数本体**（monkeypatch `repo_spec`
后复跑），或直接调用该判定的 `judge_*`，不是共用逻辑的旁路
（`VALIDATION_EVIDENCE.md:193` S6）。

| 负例 | 注入点（JSON 路径） | 注入内容 | 注入后判红的判定 / 断言 | 实测输出片段 |
|---|---|---|---|---|
| **B1a** | `blocks[normalize/p1_snr].produced_by` | 追加第二生产者 `acsd.phase1.drizzle` | A2 `judge_single_producer` | `PRODUCER_NOT_UNIQUE: ('normalize', 'p1_snr') 生产者数=2 ['acsd.phase1.noise-snr', 'acsd.phase1.drizzle']` |
| **B1a** | 同上 | 同上 | A4（面漂移项） | `PRODUCER_FACE_DRIFT: ('normalize', 'p1_snr') nodes 面重算=['acsd.phase1.noise-snr'] blocks 面声明=['acsd.phase1.noise-snr', 'acsd.phase1.drizzle']` |
| **B1b** | `nodes[acsd.phase1.writer].writes` | 追加 `p1_snr` | A4 `judge_no_duplicate_production`；**A2 判绿**（输入面独立性自证） | `DUPLICATE_PRODUCTION: ('normalize', 'p1_snr') 被 2 个节点生产 ['acsd.phase1.noise-snr', 'acsd.phase1.writer']` / A2 → `0 条违规` |
| **B2a** | `blocks[normalize/p1_snr].produced_by` | 清空 `[]` | A3 `judge_no_dangling_consumption` | `DANGLING_CONSUMPTION: ('normalize', 'p1_snr') 被 1 个节点消费、本阶段无生产者、且未声明 EXTERNAL_IN` |
| **B2b** | `nodes[acsd.phase1.writer].reads` | 追加 `ghost_block` | A3 | `CONSUME_NONEXISTENT_BLOCK: (normalize, ghost_block) 被节点消费但无块声明` |
| **B3** | `blocks[normalize/p1_wcs].lifecycle` | `STAGE` → `SHORT`（消费者数 3） | **A1 与 A5 都红** | A1：`LIFECYCLE_MISMATCH: ('normalize', 'p1_wcs') 规则重算=STAGE 规格声明=SHORT`；A5：`CONSUMER_SPAN_MISMATCH: ('normalize', 'p1_wcs') 消费者数=3 ⇒ 应为 STAGE，规格声明 SHORT` |
| **B4** | `blocks[normalize/p1_snr].lifecycle` | `SHORT` → `STAGE`（消费者数 1） | A1（**与 A5 都红**） | A1：`LIFECYCLE_MISMATCH: ('normalize', 'p1_snr') 规则重算=SHORT 规格声明=STAGE`；A5：`CONSUMER_SPAN_MISMATCH: ('normalize', 'p1_snr') 消费者数=1 ⇒ 应为 SHORT，规格声明 STAGE` |
| **B5** | `nodes[acsd.phase1.writer].reads` | 追加 `p1_snr`（`consumed_by` **不**更新 ⇒ 声明释放点 pos=6，实际读到 pos=7） | A7 `judge_prompt_release`：**逐条断言 3 条红侧子句各自变红**（`READ_AFTER_RELEASE_OR_UNDECLARED_READ` 声明/节点面不一致、`SHORT_RELEASE_POINT` 释放点≠最后读者 pos、`READ_AFTER_RELEASE` **use-after-release 专指读法**）；并逐条断言 `TOPOLOGY_INVERTED`、`STAGE_NOT_CROSS_NODE` **不存在**，A7'（`PromptReleaseLastConsumerAfterProducer`）仍判绿 | `READ_AFTER_RELEASE_OR_UNDECLARED_READ: ('normalize', 'p1_snr') 声明消费者=['acsd.phase1.drizzle'] 但 nodes 面重算=['acsd.phase1.drizzle', 'acsd.phase1.writer']`；`SHORT_RELEASE_POINT: ('normalize', 'p1_snr') 声明释放点 pos=6 ≠ 节点面最后一个读者 pos=7（读者位置 [6, 7]）`；`READ_AFTER_RELEASE: ('normalize', 'p1_snr') 声明在 pos=6 释放，但 pos=[7] 的节点仍在读它（释放后使用）` |
| **B6** | `blocks[export/p3_verify].lifecycle` | `EXTERNAL_OUT` → `STAGE`（等价于从终产物集合删掉 `p3_verify`） | A6 `judge_stage_terminals` + `StageTerminalSetIsExact` | `TERMINAL_NOT_EXTERNAL_OUT: (export, p3_verify) 合同点名的终产物未标 EXTERNAL_OUT` / `阶段终产物缺失: {'export': ['p3_verify']}` |
| **B7** | `blocks` / `nodes` 数组 | 各置为 `[]` 一次 | `ZERO_OBJECT_GUARD`（**10 条判定逐条红**） | `ZERO_OBJECT_GUARD: 实算对象数为 0（VALIDATION_EVIDENCE.md:172 第 1 条）：nodes=20 blocks=0`（另一路 `nodes=0 blocks=36`） |
| **B8-1** | `nodes` + `blocks` | 删掉 `normalize` 阶段全部对象 | B8 `judge_non_degenerate` | `NONDEGENERATE_STAGES: 实算阶段数=2，下界=3（PHASE_TO_STAGE 的键数）` |
| **B8-2** | `blocks` | 截断到 5 条 | B8 | `NONDEGENERATE_BLOCKS: 实算块数=5，下界=7（STAGE_TERMINALS 全集）`（同批还有 `NONDEGENERATE_STAGES: 实算阶段数=1`） |
| **B9-1** | `blocks[normalize/p1_psf].lifecycle` | `STAGE` → `SHORT` | A8 子句「零消费者 ⇒ 终态块」 | `STAGE_RESIDUAL: ('normalize', 'p1_psf') 有生产者、零消费者、却声明 SHORT；按 PIPELINE_BLOCK.md:36 零消费者即为终态块，阶段末将残留不销毁` |
| **B9-2** | `blocks[normalize/p1_psf].produced_by` | 清空 `[]` | A8 子句「零消费者块必须确有生产者」 | `ORPHAN_TERMINAL: ('normalize', 'p1_psf') 既无生产者也无消费者` |
| **B9-3** | 注册表副本 `frame_hips` 输出端口的 `carrier` | `hips_product_tree` → `output_dir_file` | A6 跨阶段载体子句 | `CROSS_STAGE_CARRIER: 产物 'frame_hips' 在阶段 ['mosaic'] 被消费（跨阶段），注册表 carrier=['hips_product_tree', 'output_dir_file']，必须且只能是 hips_product_tree（PIPELINE_BLOCK.md:20-21）` |
| **B9-4** | `blocks[normalize/p1_psf].block` | `p1_psf` → `P1_Psf` | A9 块名正则子句 | `BLOCK_NAME_PATTERN: ('normalize', 'P1_Psf') 块名不匹配 schema 的 '^[a-z][a-z0-9_]*$'` |
| **B9-5** | `blocks[normalize/p1_snr].produced_by` | 改为 `["acsd.phase2.coverage"]`（跨阶段生产者） | A2 生产者作用域子句 | `PRODUCER_NOT_IN_STAGE: ('normalize', 'p1_snr') 生产者 'acsd.phase2.coverage' 不在本阶段节点集合里` |
| **B9-6** | `blocks[normalize/frame_hips].produced_by` | 清空 `[]`（仍标 `EXTERNAL_OUT`） | A3 悬空消费子句 | `DANGLING_CONSUMPTION: ('normalize', 'frame_hips') 被 1 个节点消费、本阶段无生产者、且未声明 EXTERNAL_IN` |
| **B10-1** | `_blockflow.oracle_lifecycle`（**判定自身的求值器**，S6） | 换成 `_literal_order_lifecycle`（按 docstring 书写顺序 first-match） | REG-01 登记守卫判红（歧义面塌成空集）**且 A1 本体判红** | `by_evaluators = []`；`A1 违规数 = 10`，首条 `LIFECYCLE_MISMATCH: ('export', 'mosaic_hips') 规则重算=STAGE 规格声明=EXTERNAL_IN` |
| **B10-2** | `_blockflow._literal_order_lifecycle`（对照侧求值器） | 换成本层裁决的 `oracle_lifecycle` | REG-01 登记守卫判红（歧义面塌成空集）；**A1 仍判绿**（A1 不依赖对照侧） | `by_evaluators = []`；`A1 违规数 = 0` |
| **FC** | 临时文件 | 路径不存在 / JSON 不可解析 / 缺 `blocks` / 空 `blocks` | `AnchorStale` / `SpecUnparsable` / `AssertionError` | `ANCHOR_STALE: INJECTED_SPEC /tmp/.../no_such_anchor.json`；`ANCHOR_UNPARSABLE: INJECTED_SPEC …`；`ANCHOR_MISSING_FIELD: SPEC.blocks`；`ZERO_OBJECT_GUARD …` |

**同时验证绿（S2）**：每条负例在注入前先对未注入的仓内规格复跑一次并断言判绿
（B1、B2、B3、B8、B9、B10 内含显式 `assert not judge_*`）。

## 8 恒成立 / 恒不成立的判据（如实登记，不写成永远绿的测试）

### 8.1 派单原文 A3 的字面写法 = **恒不成立**，已改写

派单原文 A3 写「不存在『被某节点消费但**既无本阶段生产者**、**也不在本阶段块的读写集合内**』的块」。
被消费的块按定义就在本阶段的读集合里 ⇒ 第三个合取项恒为假 ⇒ 整个合取**恒不成立**，
写成用例就是一条永远绿的空判据（违反 `TEST.md:26`）。

**已改写为可红的两条**（`PIPELINE_BLOCK.md:42` 的字面口径）：

- `CONSUME_NONEXISTENT_BLOCK`：被节点消费、但在本阶段没有 `blocks` 条目（B2b 证红）；
- `DANGLING_CONSUMPTION`：被消费、本阶段无生产者、且未声明 `EXTERNAL_IN`（B2a / B9-6 证红）。

### 8.2 派单原文 A8 的字面写法 = **恒不成立**，已改写

派单原文 A8 写「所有非终态块都至少有一个消费者」。但 `PIPELINE_BLOCK.md:36` 把
「`consumers` 可空」定义为**终态块**的充分条件 ⇒「零消费者 ∧ 非终态」在合同的字面读法下
**恒不成立**。

**已改写为三条可红判据**（详见 `judge_stage_residual_zero` 的 docstring 与
`test_..._StageResidualZero` 的 docstring）：零消费者 ⇒ 必须声明为终态块
（`STAGE` / `EXTERNAL_OUT`）；作用域闭合（生产者/消费者必须在本阶段）；零消费者块必须
确有生产者。分别由 B9-1 / B9-5 / B9-2 证红。

⚠️ **口径后果**：`normalize/p1_flux`、`normalize/p1_psf` 这两个块「生产了、本阶段零消费者、
又不是合同点名终产物」，在本层口径下**判绿**（见登记项 REG-02）。若改成
「`consumers == 0 且 不在 `STAGE_TERMINALS` 即判红」，它们会判红——那条判据与
`PIPELINE_BLOCK.md:36` 及 `gen_block_flow_spec.py:10` 的字面规则**直接冲突**，
本层**不采用**，交负责人裁定。

### 8.3 `STAGE_NOT_CROSS_NODE` 子句 = **当前节点模型下不可达**，作防御性断言

A7 的第 4 子句判「多消费者 `STAGE` 块的消费者必须落在 ≥2 个不同节点位置」。
`pos` 对 `(stage, module_id)` 唯一 ⇒ 两个不同 `module_id` 永远不同位置；
要让该子句单独变红，需要 `consumed_by` 里出现同一个 `module_id` 两次，
而那会先被 A7 的面一致性子句判红。**本子句没有独立的注入负例**，按
`VALIDATION_EVIDENCE.md:164` 的口径如实登记，不声称它有独立证据。

### 8.4 A2 与 A4 在合同里是重复条款

`PIPELINE_BLOCK.md:41`（「每个被消费的块有且仅有一个生产者」）与 `:43`
（「同一块被重复生产 ⇒ 非法」）陈述的是同一件事。本层**两条都写**，但输入面不同
（A2 读 `blocks[].produced_by`，A4 读 `nodes[].writes`），B1a / B1b 分别证明各自的红侧
可独立触发且不互相顶包。

### 8.5 A7 的「没有任何位置更晚的节点读它」在生成器下是结构保证

`blocks[].consumed_by` 由生成脚本按节点序 append 而成，故天然按 `pos` 升序。
若只用 `consumed_by` 自比，该子句退化成 `max(x) == max(x)` 的恒真比较。
**已改写**：`_reader_positions()` 从 `nodes[*].reads` 独立实算读者位置，再与声明面对照；
B5 证明它有牙（`SHORT_RELEASE_POINT` + `READ_AFTER_RELEASE` 两条同时红）。

### 8.6 ⚠️ `assert … or True` = **字面恒真**，由对抗复核发现（已修）

**发现**：只读对抗复核。**我复核确认成立。**

**原位置**：`test_block_lifecycle.py` 的 `test_..._LifecycleRulePrecedenceIsRegistered` 内，
原第 220 行：

```python
for stage, name in affected:
    assert stage in bf.stage_terminals() or True   # ← 字面恒真
    assert isinstance(name, str) and name
```

**最小反例**：把 `bf.stage_terminals()` 整个函数删掉，该断言**仍然绿**——
`X or True` 恒为真，左项从不参与判定。同一循环体的 `assert isinstance(name, str) and name`
也偏弱：键由 `precedence_sensitive_keys()` 从注册表端口名构造，`name` 非空字符串是构造保证。

**这是我自己的漏报**：本节 §8.1–§8.5 逐条登记了别人指出的恒真/恒假判据与我自己识别出的
恒真子句，但**没有登记这一条**——它不在我的自审清单里，是对抗复核抓出来的。

**修法（不是换成另一条恒真断言）**：整个循环体删除，改为**三条由不同代码路径算出的
独立面**逐条断言（`_blockflow.py` 新增 `rule_predicates()` / `multi_rule_keys()` /
`terminal_with_consumer_keys()` / `all_block_keys()`）：

1. `precedence_sensitive_keys()`（两个求值器之差）非空；
2. `multi_rule_keys()`（docstring 5 条谓词计数，**不经过任何求值器**）非空且是①的**子集**；
3. ①与②的**差集恰为** `terminal_with_consumer_keys()`（终产物 ∧ nc ≥ 1）；
4. ①是块全集的**真子集**。

**能红的负例**：新增 B10（`test_..._NegOraclePriorityAblation`），注入点是
**判定自身的求值器**（S6）——把 `oracle_lifecycle` 换成按 docstring 书写顺序求值的版本，
歧义面即塌成空集 ⇒ 第 1 条判红，**且 A1 本体同时判红（10 条 `LIFECYCLE_MISMATCH`）**，
后者顺带证明 A1 的 Oracle 是承重的、不是把被测规格读回来。

**顺带发现（REG-01 的第二层歧义）**：三面对照实测 **10 / 8 / 2**（求值器差分面 10 项、
docstring 谓词面 8 项、差集 2 项）。差集恰是 `(normalize, frame_hips)` 与
`(export, p3_fits)` ——「阶段终产物且 nc ≥ 1」。成因：docstring 把终产物规则写成规则 2/3
并**以 `nc == 0` 为前提**；本层裁决把 `is_stage_terminal` 提成**独立分支**排在消费者数之前。
即裁决不仅改了「谁优先」，还**把歧义面从 8 项扩到 10 项**。已登记进 REG-01。

## 9 登记项（实现与正本的冲突 / 缺口）

| ID | 事项 | 实测证据 | 处置 |
|---|---|---|---|
| **REG-01** | `gen_block_flow_spec.py:7-13` 的 5 条派生规则是**并列条目、没有写优先级**。多条同时命中时结论歧义。实测受影响的 `(stage, block)` 共 **10** 个：8 个 `EXTERNAL_IN`（`normalize/lights`、`master_bias`、`master_dark`、`master_flat`、`p1_photscale`、`mosaic/frame_hips`、`export/mosaic_hips`、`export/run_context`）+ `normalize/frame_hips` + `export/p3_fits`（阶段终产物且 nc=1）。若按 docstring **书写顺序** first-match，后两类会被判成 `SHORT`、前 8 个里 6 个会被判成 `SHORT`。**歧义面分两层**：(i) 规则**先后**未定（10 项）；(ii) 终产物规则在 docstring 里以 `nc == 0` 为前提，本层裁决把它提成独立分支 → 歧义面由 docstring 谓词计数的 **8** 项扩到求值器差分的 **10** 项，差集恰为「终产物 ∧ nc ≥ 1」（见 §8.6）。 | 三面实测 **10 / 8 / 2**（`precedence_sensitive_keys` / `multi_rule_keys` / 差集）；由 `test_..._LifecycleRulePrecedenceIsRegistered` 做**三面登记漂移守卫**（任一面塌成空集或差集错位即判红），负例 B10 证明守卫有牙 | 本层 Oracle 采用的优先级：**无生产者 → 阶段终产物 → 消费者数**（理由见 `_blockflow.oracle_lifecycle` docstring：溯源事实 + 发布义务优先于内存跨度；且「按书写顺序」会把跨阶段 HiPS 输入判成 `STAGE`，与 `PIPELINE_BLOCK.md:20` 矛盾）。**建议上游在 docstring 里补一句显式优先级**，本层的裁决随之可以删除。<br>⚠️ **A1 的准确表述是「与本层裁决后的规则逐项一致」，不是「与正本无条件一致」**（对抗复核的降级提醒，已采纳并写入 A1 的 docstring）。 |
| **REG-02** | 「生产了但本阶段零消费者、又不是合同点名终产物」的块有 **2** 个：`normalize/p1_flux`（生产者 `acsd.phase1.photometry`）、`normalize/p1_psf`（生产者 `acsd.phase1.star-psf`）。按 `gen_block_flow_spec.py:10` 的字面规则它们**应当** `STAGE`，按 `PIPELINE_BLOCK.md:36` 它们**就是**终态块，注册表 `carrier_contract.statement` 也明写「**无任何消费者**的产物，用 output_dir 文件约定」，两个端口的 `note` 逐字写「生产链路零消费者：本端口只登记事实」。**但** `PIPELINE_BLOCK.md:55` 逐字写「阶段结束时块残留数 = 0」，`ACSD_DESIGN.md:386` 把峰值内存绑在「在途块集合」上。一个**从未被任何消费者读**的块活到阶段末，是否与这两条的意图相容，需要负责人裁定。 | `bf.observe_zero_consumer_stage_blocks(spec)` 实算 2 项；A1/A5/A7/A8 全部判绿；已列为 A7/A8 的**观察项**输出 | 本层**不判红**（规则支持它们判绿）。登记为口径分歧，等裁定。 |
| **REG-03** | `stage_block_flow.json` 与 `pipeline_block.schema.json` 是**两份不同 `$id` 的产物**（`acsd.stage-block-flow/v1` vs `acsd.pipeline-block/v1`），但 `PIPELINE_BLOCK.md:8` 把后者称作「本合同的机器取值源」。实测差异：(a) **`lifecycle` 词表不相交** —— 规格侧 `{EXTERNAL_IN, EXTERNAL_OUT, SHORT, STAGE}`，schema 侧 `{short, frame, run}`（`pipeline_block.schema.json:68-74`），**交集为空集**；(b) schema 要求的物理元数据字段 `name`/`shape`/`dtype`/`unit`/`optional`/`producer`/`consumers`/`provenance`（`:20-28`）在块流规格里**一个都没有**，规格用的是 `stage`/`block`/`lifecycle`/`produced_by`/`consumed_by`。 | A9 只判两者**共有**的形态；`test_..._SchemaVocabularyDivergenceRegistration` 把差异登记为 `_blockflow.SCHEMA_LIFECYCLE_VOCAB` 并做漂移守卫 | **如实登记，不私自改规格、不降级判据**。「块流规格整体符合 `pipeline_block.schema.json`」这条判据在仓库现状下**判红**，故未写成绿的正例用例；漂移守卫会在两侧被对齐时判红并要求更新登记。**需负责人裁决**：是补一份 `acsd.stage-block-flow/v1` 的 schema，还是把 `PIPELINE_BLOCK.md:8` 的措辞改成「块物理元数据的机器取值源（不是流拓扑规格）」。 |
| **REG-04** | `gen_block_flow_spec.py:13` 逐字引用 `check_block_flow_spec.py` 的 **R7 复核**，但 `eng/tools/quality/check_block_flow_spec.py` **在当前仓内不存在**（只在 `run/**` 的历史快照里能找到）。锚存活失效。 | `ls eng/tools/quality/` 实测无该文件；`VALIDATION_EVIDENCE.md:413`「判据中硬编码引用的仓库路径必须存在」 | **登记，不在本层修**。本层 A1 自己做了独立重算 + 逐项精确比对，实际上替代了 R7 的角色；但 docstring 的引用应补文件或删引用。 |
| **REG-05** | `PIPELINE_BLOCK.md:65` 逐字点名的短生命周期块名是 `raw` 与 `calibrated`，而块流规格里**没有 `raw`**，校准产物叫 `p1_calibrated`（`STAGE`，4 个消费者）；`lights`（原始亮场）是 `EXTERNAL_IN`。块名 = 注册表端口名、不发明新名（`gen_block_flow_spec.py:5`），所以 `raw`/`calibrated` 是**叙述性称呼**而非块名。 | 逐块读数见 `test_..._PromptReleaseLastConsumerAfterProducer` 的「逐块读数」输出 | **登记口径差异，不判红**。若 `PIPELINE_BLOCK.md:65` 的例子要成为可执行判据，需先统一块名口径。 |
| **REG-06** | `ACSD_DESIGN.md:385` 逐字写「长生命周期块（WCS、PSF、星表匹配、帧级信噪比）随帧存活到导出」。实测块流规格里：`p1_wcs` 是 `STAGE`（阶段内跨 3 个节点，不是活到导出）、`p1_psf` 与 `p1_flux` 是零消费者 `STAGE`、`p1_snr` 是 **`SHORT`**（单消费者 `acsd.phase1.drizzle`，释放点 pos=6）。 | A7 逐块读数 | **登记，不判红**。`PIPELINE_BLOCK.md:17` 逐字写「命名块…不落盘、不跨节点、不跨阶段」，与 §8.2 的「随帧存活到导出」说的显然是**不同对象**（产品树内的层，不是内存块）。两处措辞需要统一，否则「帧级信噪比块应是长生命周期」会被读成对块流规格的缺陷指控。 |

## 10 未覆盖 / 否决的判据

| 项 | 处置 | 理由 |
|---|---|---|
| 「块流规格整体符合 `pipeline_block.schema.json`」的**严格版** | **未写成绿的正例用例**，登记为 REG-03 | 仓库现状下它**判红**（`lifecycle` 词表不相交 + 8 个物理元数据字段缺失）。写成绿用例等于把判据降级；写成红用例会让本目录长期带红且无裁决主体。改为「共有形态判绿 + 差异登记 + 漂移守卫」。 |
| 「端口↔代码双向一致」C1/C2/C3/C3b、PC-C4/PC-C5/PC-C6 | **不在本单范围** | 这组判据的事实源是 `lib/infrastructure/scheduler/src/module_adapters.cpp` 的函数体与 token，属 pipeline-carrier 判据域（`PIPELINE_BLOCK.md:80-99`），不是块生命周期判据域。本层只判「块的生命周期与 DAG」，端口锚点解析由别处负责。 |
| IR-C4…IR-C8 | **不在本单范围** | 事实源是管线 IR 与 `PHASE1_ARTIFACT_TO_PORT` 桥表（`PIPELINE_BLOCK.md:112-117`），属插值算子侧判据域。本层的 `pos` 只是块流规格节点数组的声明序，不承担 IR 保真职责。 |
| 块元数据的 `shape` / `dtype` / `unit` / `optional` / `provenance` 逐项核对 | **无法判** | 块流规格**不携带**这些字段（REG-03），仓内没有承载面。要判必须先有承载面。 |
| `PIPELINE_BLOCK.md:56`「异常/取消路径必须与正常路径一样销毁（单测覆盖）」 | **不在本单范围** | 这是运行期状态机（`CREATED → CONSUMED → DESTROYED`）的行为验证，需要可执行对象；本层是**纯静态/纯数据**测试，不起子进程、不链接 libacsd。该条应在单元层/集成层以 `object_class` 非 `fixture-only` 的形式落地。 |
| 「峰值内存由在途块集合与分块大小决定」 | **不在本单范围** | 需要真实运行与 RSS 测量（`TEST.md:183` 测量档）。本层只能给出在途块的**静态下界读数**。 |
| A7 中「没有任何位置更晚的节点读它」用 `consumed_by` 自比的写法 | **否决并改写** | 恒真比较无证据资格（`TEST.md:26`）。改用 `nodes[*].reads` 独立实算读者位置，B5 证有牙。见 §8.5。 |
| A8 中「所有非终态块都至少有一个消费者」的写法 | **否决并改写** | 恒不成立（`PIPELINE_BLOCK.md:36`）。见 §8.2。 |
| A3 中派单原文的合取写法 | **否决并改写** | 恒不成立。见 §8.1。 |
| B8 中「删一个终产物块 ⇒ 块数跌破下界」 | **否决并改写** | 块数下界 7（`STAGE_TERMINALS` 全集）太松：删掉 `p1_products` 后仍有 35 块，**不触发**下界，负例无效。改用「`blocks` 截断到 5 条」并同时保留「删掉 `normalize` 阶段」作阶段数下界注入。实测两次注入分别命中 `NONDEGENERATE_BLOCKS` 与 `NONDEGENERATE_STAGES`。 |

## 11 需要构建产品才能跑的测试

**0 条。** 本目录 25 条用例全部是纯静态 / 纯数据测试：

- 只读仓内 4 个锚：`stage_block_flow.json`、`module_ports.registry.json`、
  `gen_block_flow_spec.py`（**源码文本**，用 `ast` 解析，不执行）、
  `pipeline_block.schema.json`；
- 不链接 libacsd、不调 CLI、不起子进程、不读产品产物、不需要 CMake 配置或编译。

## 12 命名

函数名 `test_<Suite>_<Feature>`（pytest 不接受点号），`Suite.Feature` 标识与函数名的
映射表在 `test_block_lifecycle.py` 的**模块 docstring** 里逐条列出
（`docs/engineering/testing/TEST.md:88`）。

## 13 对抗复核发现与处置

只读对抗复核开出 **2 处不合格**，均**复核确认成立**，已全部修掉。

| # | 位置 | 复核意见 | 最小反例 | 修法 | 复跑读数 |
|---|---|---|---|---|---|
| **①** | `test_block_lifecycle.py` 的 `test_..._LifecycleRulePrecedenceIsRegistered`，原第 **220** 行 | `assert stage in bf.stage_terminals() or True` 字面恒真；同一循环体内另一条断言也偏弱；且**我的恒真/恒假清单漏登了这条** | 把 `bf.stage_terminals()` 整个函数删掉，该断言**仍然绿**（`X or True` 恒真，左项从不参与判定） | 删掉整个循环体，改为**三条由不同代码路径算出的独立面**逐条断言；`_blockflow.py` 新增 `rule_predicates()` / `multi_rule_keys()` / `terminal_with_consumer_keys()` / `all_block_keys()`；**补能红的负例 B10** | 三面实测 **10 / 8 / 2**（求值器差分面 / docstring 谓词面 / 差集 = `{('normalize','frame_hips'), ('export','p3_fits')}`），块全集 36 项，守卫绿。B10-1 消融后 `by_evaluators = []` ⇒ 守卫判红**且** A1 判红 10 条；B10-2 消融后 `by_evaluators = []` ⇒ 守卫判红、A1 仍绿 0 条。登记见 §8.6 与 REG-01。 |
| **②** | `test_..._NegPromptReleaseViolation`（B5） | 只断言了 A7 三条红侧里**最弱**的 `READ_AFTER_RELEASE_OR_UNDECLARED_READ`（声明/节点面不一致），漏了**实测同样变红**的 `READ_AFTER_RELEASE`（use-after-release 的**专指**读法，落 S1） | — | 改为**逐条断言 A7 的每条红侧子句各自变红**，并**保持**断言 `TOPOLOGY_INVERTED` 与 `STAGE_NOT_CROSS_NODE` 不存在、A7' 仍判绿；`_run_against_injection` 的返回文本也逐条点名 | B5 注入后 A7 报 **3 条**：`READ_AFTER_RELEASE_OR_UNDECLARED_READ`（声明消费者 `['acsd.phase1.drizzle']` vs nodes 面 `['…drizzle','…writer']`）、`SHORT_RELEASE_POINT`（声明释放点 pos=6 ≠ 节点面最后读者 pos=7，读者位置 `[6, 7]`）、`READ_AFTER_RELEASE`（声明在 pos=6 释放，但 `pos=[7]` 的节点仍在读它）；`TOPOLOGY_INVERTED` 不存在、`STAGE_NOT_CROSS_NODE` 不存在、A7' 判绿。 |

**另采纳一条降级提醒（复核提出）**：A1 的结论句已改为
「与**本层裁决后**的规则逐项一致」，不再写成「与正本无条件一致」；
REG-01 的登记条目里也加了同一句限定。A1 的 docstring 里同时补上
「Oracle 是承重的」自证指针（B10-1）。

**复核明确认可、本轮未改动的部分**：A1 的 Oracle 真异源（规则文本 + `ast` 字面量 +
注册表重算，不读被测规格任何现值）；B1 的输入面互斥断言；B3/B4/B5 用
`_run_against_injection` 真复跑正例函数本体；B5「不同时改 `consumed_by`」的设计；
派单「6 个块」错误的拒收与订正为 7；A3/A8/A7 三条恒成立/恒不成立的改写；
拒按与正本冲突的 `consumers==0` 口径写。

## 14 纪律自检

- **不编造**：每条断言的预期值来自 (a) `gen_block_flow_spec.py` docstring 的规则文本 /
  (b) `PIPELINE_BLOCK.md` / `ACSD_DESIGN.md` / `module_ports.registry.json` 的条款 /
  (c) `TEST.md:46` 的容差档。每条用例 docstring 末尾有 `**来源依据**：<文件:行>`。
- **不迁就实现**：REG-01…REG-06 如实登记，未改规格、未改 schema、未降级判据。
- **不堆叠豁免**：全文件**无** `pytest.skip` / `pytest.xfail` / `xfail` 标记 / 空断言。
- **不写恒真断言**：每条断言写前自问「字面读法下是否恒为真/恒为假」；恒成立的一律不写成
  永远绿的测试，改写成可红判据并补能红的负例。**对抗复核抓到的漏报已补进 §8.6。**
- **fail-closed**：4 个锚任一缺失 ⇒ `ANCHOR_STALE: <常量名> <路径>`；JSON 不可解析 ⇒
  `ANCHOR_UNPARSABLE`；结构缺字段 ⇒ `ANCHOR_MISSING_FIELD`；零对象 ⇒ `ZERO_OBJECT_GUARD`。
  **无一条 skip、无一条静默降级**。
- **不裁决代码**：不产出阻塞退出码，不接 CI，不注册进构建。