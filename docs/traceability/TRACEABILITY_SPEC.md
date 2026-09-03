# 机器追溯合同（TRACEABILITY_SPEC v1）

> 建立：DOC-001（SA-QA-29，wave W1）base_main_sha=0d32c07d65c6d7489fa408cbafaa98ddf9ecf4da
> 状态：ACTIVE_NORMATIVE —— 本文件冻结追溯 ID 格式、唯一性、跨层关系、CSV/JSON schema
> 与 source symbol 表达；仓库内所有模块追溯矩阵与机器检查器必须与本文件一致。
> 权威顺序与分层语义按控制包 `16_SCIENCE_DOCUMENT_AND_TRACEABILITY_STANDARD.md`
> 与仓库 `AstroCS_ENGINEERING_CONSTRAINTS.md`（源 doc_id DOC-GOV-CONSTRAINTS-001），
> 本文件不重复公式、不改科学定义、不放宽既有工程约束。

## 1. 目的与范围

为每个模块建立**机器可执行**的八层追溯合同，缺口必须**显式表达**（`MISSING`），
禁止以空字符串、占位符或静默缺列通过。矩阵的“事实”是

- `docs/traceability/TRACEABILITY_MATRIX.json`（权威，机器真相）
- `docs/traceability/TRACEABILITY_MATRIX.csv`（同构视图，供人工/diff）

Schema：

- `schemas/traceability_matrix.schema.json`（JSON 合同）
- `docs/traceability/TRACEABILITY_LAYERS.csv`（CSV 合同：列定义 + 每层必填规则 + 取值域）

检查器：`tools/traceability/check_traceability_matrix.py`（exit 0 = `TRACEABILITY_MATRIX_PASS`；
任何断链输出**具体模块 + 层 + 缺失/悬空引用路径**并以非 0 退出，绝不崩溃吞异常）。

## 2. 追溯链与分层

权威链（控制包 16 号文 §1）：`SCI → ALG → DATA/API/ARCH → MOD/SRC → TEST → EVIDENCE`。

仓库矩阵把它展成 8 个**必填层**，同一行（同一模块）形成从科学定义到现场证据的
parent→child 链：

| 层 | 层代码 | 必填列 | 唯一性域 | 状态取值（必须非空） |
|---|---|---|---|---|
| 科学定义 | SCI | science_id / science_doc | 全矩阵唯一（跨模块） | VERIFIED / MISSING |
| 算法/近似/误差 | ALG | algorithm_id / algorithm_doc | 全矩阵唯一 | VERIFIED / MISSING |
| 数据合同 | DATA | data_id | 全矩阵唯一 | VERIFIED / MISSING / NONE |
| 接口/API | API | api_id | 全矩阵唯一 | VERIFIED / MISSING / NONE |
| 架构 | ARCH | arch_id | 全矩阵唯一 | VERIFIED / MISSING / NONE |
| 模块承载 | MOD | module_id | 模块唯一（= 行键） | VERIFIED（由行存在即满足） |
| 实现 | SRC | src_id / src_path | 全矩阵唯一 | VERIFIED / MISSING |
| 验证 | TEST | test_id / test_path | 全矩阵唯一 | VERIFIED / MISSING / NONE |
| 证据 | EVIDENCE | evidence_id | 全矩阵唯一 | VERIFIED / MISSING |

实现注意：

- DATA/API/ARCH 三个承载层**并列**（同属“软件承载”），不是先后级联；每层各自可
  `VERIFIED / MISSING / NONE`。`NONE` 仅允许模块文档显式声明“该模块无此类合同”
  （如 conformance 模块无科学合同），且必须在 `notes` 给出原因。
- SCI/ALG 层对 conformance/服务性模块可 `MISSING`（显式），但 MOD/SRC/TEST/EVIDENCE
  对**每个已注册模块**都必须是 `VERIFIED`（有真实文件）或至少 `MISSING` 显式占位；
  服务模块（io）允许 DATA/API `VERIFIED` 而 SCI/ALG `MISSING`。
- “每层必填”= 矩阵每行、每层状态单元格**必须出现且非空**（取值于该层合法集合），
  不允许缺列/空串/空白；`MISSING` 是合法显式值，不是空串。

### 2.1 空缺的表达（禁止空字符串通过）

- 状态空缺 → 状态列必须写 `MISSING`（或按层的 `NONE`），不得留空、不得写 `-`/`?`/`TBD`。
- ID 空缺 → ID 列写显式占位：
  - 通用层（SCI/ALG/DATA/API/ARCH/TEST/EVIDENCE/SRC 的 id 列）：`<LAYER>-MISSING`
    （例如 `SCI-MISSING`、`TEST-MISSING`）。
  - SRC 的 source symbol 表达：`<src_path>::MISSING`（占位无符号）。
- 文档/路径空缺 → 列写 `MISSING`（禁止空串）。
- 任何单元格为 `""`、纯空白、`-`、`?`、`TBD`、`TODO` → 机器检查器判
  `EMPTY_CELL_VIOLATION`（FAIL，报告具体行/列）。

## 3. ID 格式（机器正则）

ID 一律 ASCII 大写，允许段分隔符 `-`，不允许空格、点、下划线之外的字符，不允许空段：

```text
SCI      ^SCI-[A-Z0-9]+(-[A-Z0-9]+)*$          例如 SCI-CAL-001、SCI-P1-DRIZ-001
ALG      ^ALG-[A-Z0-9]+(-[A-Z0-9]+)*$          例如 ALG-CAL-001、ALG-005
DATA     ^DATA-[A-Z0-9]+(-[A-Z0-9]+)*$         例如 DATA-P1-FRAME、DATA-HIPS-001
API      ^API-[A-Z0-9]+(-[A-Z0-9]+)*$          例如 API-P1-001、API-ABI-001
ARCH     ^ARCH-[A-Z0-9]+(-[A-Z0-9]+)*$         例如 ARCH-001
MOD      ^MOD-[A-Z0-9]+(-[A-Z0-9]+)*$          例如 MOD-astrocs-phase1-calibration
TEST     ^TEST-[A-Z0-9]+(-[A-Z0-9]+)*$         例如 TEST-P1-CAL-001、TEST-BLD003-NOOP-HANDSHAKE
EVID     ^EVID-[A-Z0-9]+(-[A-Z0-9]+)*$         例如 EVID-DOC-001-MATRIX
```

- 占位符 ID 是合法 ID 的超集特例：`SCI-MISSING`、`TEST-MISSING` 等（见 2.1）。
- **唯一性强制域（机器 ERROR）**：`module_id`（行键）全矩阵唯一；SRC 层 id、
  EVIDENCE 层 id（非占位）全矩阵唯一。SCI/ALG/DATA/API/ARCH/TEST 是**合同层**，
  ID 可被多个模块行共享（如 `API-P2-001` 被 8 个 phase2 模块共同承载、
  `TEST-P3-RES-001` 由 phase3.resample/resample2 共享——registry 文档既定事实），
  其**真实唯一性裁决归合同注册表**（`docs/contracts/INDEX.yaml` +
  `tools/check_contract_graph.py`），本矩阵对共享引用只登记不判重。
- 状态 `MISSING` 的层允许保留 **descriptor/registry 已预留的真实 ID**（ID 占用
  命名空间但独立 authority 文档/实现尚未落地），也允许占位符 ID；空串一律禁止。
- 状态 `VERIFIED` 的层必须满足：id 非占位，且锚可机器解析（见 §4/§5 与 §7）。
- 旧表（docs/TRACEABILITY.csv、docs/contracts/*）沿用各自历史格式，不由本合同重写；
  本矩阵是新模块化事实源（第 5 节给出与旧表的关系）。

## 4. 跨层 parent→child 关系

- 每个模块一行（key=`module_id`），层的 child 关系由**行内同列取值**表达：
  - `science_id` 是 SCI 层的“主对象”（模块级）；
  - `algorithm_id` 是 ALG 层主对象；`data_id`/`api_id`/`arch_id` 是承载层主对象；
  - `src_path::symbol`（SOURCE SYMBOL 表达）给出实现层锚点；
  - `test_id` + `test_path` 给出验证层锚点；
  - `evidence_id` 给出 EVIDENCE 层锚点（证据包/日志/现场 hash）。
- 跨层 parent-child 校验规则（机器可查）：
  1. **前导链非空**：TEST 层引用成立的前提是同一行 MOD（行存在）与 SRC 层非 `MISSING`
     （SRC `MISSING` 时 TEST 必须 `MISSING`，禁止“有测试无实现”）；
  2. **承载层引用**：API `VERIFIED` 的行应能通过 API 注册表/API_CONTRACTS.csv
     找到对应 ID（由扩展检查给出具体缺失，不崩溃）；
  3. **证据锚**：EVIDENCE `VERIFIED` 时 evidence_id 应能在 `evidence/`、`reports/`
     、`returns/` 或 TASK_STATE evidence_refs 中解析（同 2 语义）；
  4. 一行内不允许出现“下层 VERIFIED 而上层同链 MISSING”的科学链断裂
     （SCI MISSING 但 ALG VERIFIED 之类）→ 判 `CHAIN_BREAK`（给出 module_id 与层）。
     例外（显式登记，不判断链）：conformance/service/provider 行 —— SCI/ALG
     MISSING 而 DATA/API/ARCH/VERIFIED 属宿主/服务边界语义，notes 已给出原因。
  5. TEST 层 `VERIFIED` 时该行 SRC 层必须也 `VERIFIED`（有实现才有测试证据），
     SRC `MISSING` 而 TEST `VERIFIED` 判 `CHAIN_BREAK`（给出 module_id）。
- 说明：矩阵是**模块↔锚**机器合同；旧 TRACEABILITY.csv 的逐 claim 细粒度
  （authority/anchor/oracle）仍由 `tools/check_traceability.py` 负责，二者互补不冲突。

## 5. SOURCE SYMBOL 表达

- SRC 层引用格式：`<repo-relative-path>::<symbol>[,<symbol>...]`，多符号用逗号分隔。
- 文件必须存在且受 Git 跟踪；符号必须在文件文本中可见（宽松匹配标识符边界）。
- 无符号可锚时写 `<path>::MISSING`（文件存在但符号待补）——禁止留空、禁止裸路径冒充。
- 表达示例：`modules/conformance/noop/src/noop_module.c::astrocs_module_query_v1`、
  `providers/cpu/common/README.md::MISSING`。

## 6. 初始矩阵（DOC-001 基线）

`docs/traceability/TRACEABILITY_MATRIX.json` 覆盖仓库**全部已注册模块**：

- `modules/services/io`（IO-001/IO-002 落地）：`astrocs.services.io`
- `modules/conformance/noop`（BLD-003 SKELETON）：`astrocs.conformance.noop`
- `docs/modules/registry/astrocs.phase*.md` 声明的 22 个 registry 生产模块
  （module_id 以 `astrocs.phase1./phase2./phase3.` 开头，唯一源 `lib/core/src/module_adapters.cpp`）
- `providers/cpu`（CPU-001 落地，provider 能力清单）

每行 8 层全部显式；尚无科学/算法合同的行用 `SCI-MISSING`/`ALG-MISSING` + 状态
`MISSING`，有实现有测试的行用真实 id/path/符号（`VERIFIED`）；空缺从不为空串。
模块清单的“机器可发现”方法记录于第 7 节。

## 7. 机器闭环（检查器行为契约）

运行：`python3 tools/traceability/check_traceability_matrix.py --root . [--json-out out.json] [--strict]`
（Python 3.10+ 标准库；无网络；不依赖 cwd 之外路径；`timeout 120` 内完成）。

必须实现且失败时给出**具体断链**（模块 + 层 + 路径 + 期望/实际），不允许崩溃：

| 检查 | 失败输出前缀 | 说明 |
|---|---|---|
| 文件存在且被 Git 跟踪 | `MISSING_FILE` | JSON/CSV/LAYERS/schema 缺一即 FAIL |
| JSON schema 合法（列存在/取值/附加字段） | `SCHEMA_VIOLATION` | 用内置轻量校验，缺列/坏值报路径 |
| 空单元格（空串/空白/`-`/`?`/`TBD`/`TODO`） | `EMPTY_CELL_VIOLATION` | 报 模块+列 |
| ID 格式不合法 | `ID_FORMAT_VIOLATION` | 报 模块+列+值 |
| 同层真实 ID 重复 | `DUPLICATE_ID` | 报 层+ID |
| 前导链断裂（见 §4 规则 1/4） | `CHAIN_BREAK` | 报 module_id+层对 |
| 文档/路径/SOURCE SYMBOL 引用悬空（文件不存在/未跟踪/符号不可见） | `DANGLING_REF` | 报 模块+列+路径+期望符号 |
| 引用越界（API/TEST/EVID 允许外部注册表缺失但必须逐条列 WARN） | `REF_OUT_OF_SCOPE` | 报 具体 ID（strict 下为 FAIL） |

- exit 0 且仅当零 ERROR；输出一行 `TRACEABILITY_MATRIX_PASS modules=<n> rows=<n> errors=0`。
- 任何未捕获异常 → 打印 `TOOLING_FAILURE` 并 exit 3（不允许伪 PASS）。
- 负面 fixture 在 `tests/traceability/fixtures/`，试金石测试
  `tests/traceability/test_traceability_matrix.py` 用 mutation 证明：删任意层/填空串/
  造重复 ID/悬空引用 → 检查器必失败且不崩溃。

## 8. 状态与演进

- 本文件 + 矩阵 + 检查器由 DOC-001 建立；后续模块填充由对应任务推进并把行状态
  `MISSING` → `VERIFIED`，同时补 `EVID-*` 证据锚。
- 科学公式/单位/容差以 docs/science、docs/algorithms 与测试 oracle 为权威；
  本合同只做机器身份与断链报告，不裁决科学正确性。
- 冻结约束（项目负责人）优先于本文件；本文件只可在负责人确认后修订。
