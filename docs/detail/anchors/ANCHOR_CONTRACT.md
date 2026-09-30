# 文档—代码锚合同（ANCHOR CONTRACT）

> 上游：docs/ACSD_DESIGN.md §0（文档权威与索引）、§8.5（模块与 ABI）
> 状态面：AGENTS.md 第 8 节（SubAgent 无 git 写权限）与
> docs/engineering/DOCUMENT_GOVERNANCE.md（准入判据、登记面判据、上游抬头）。
> 写法：docs/engineering/DOCUMENT_GOVERNANCE.md §7（写法判据的封闭词表与分层施加）；
> 引用格式：docs/engineering/DOCUMENT_GOVERNANCE.md 与各 science 分册的论文式引用。

本文件规定「文档条款锚定到源码符号与内容锚」的合同：锚的形态、解析规则、
审查判据、登记面与维护义务。锚的唯一目的是**可机器核验地断言「文档所述的符号与
取值就在此处」**，因此形态选择服从可判定性。

## 1. 适用范围

- **仓内行锚（`文件:行`）不写**：指向本仓任何文件（代码或文档）的位置表达，
  一律用内容锚（§4）或 `§条` 引用。文档指向本仓代码同样不写 `cpp/foo.cpp:123`。
- **行锚只用于外部目标**：未 vendor 进仓库的外部开源实现与外部许可证据文件。
  这类目标在本仓不可解析，只能逐条登记（§3）。
- 配置登记面（`config_registry.json` / `defaults.json` / `filters.json`）一律零行号：
  位置信息一律「锚 id + 内容指纹」。

## 2. 外部行锚的形态

```text
path/to/file.ext:N          path/to/file.ext:N-M
path/to/file.ext:N,M        path/to/file.ext:N/L          path/to/file.ext#N
path/to/file.ext:N（symbol）    path/to/file.ext:N + backticked symbol
```

- **一行多锚 = 逐 token 各成一条独立锚记录**：`<file>:N-M / :A-B / :C` 是**三个**锚；
  续锚与首锚同文件、同登记规则，各自判界内（C3）、各自判符号绑定（C4）。
- **`#N` 是行号锚，`§N` 是章节引用**，两者取值互不代用。引用章节一律写 `§条` 并写全路径。
- 外部锚的语义义务：锚是断言（目标移动即锚失效，**更新行号时锚所指向的符号、公式、
  默认容差保持逐字不变**，顺手改写语义属越界）；一个锚一个目标（同名多候选须钉死）；
  边界行只取有内容的行（区间锚裁掉边界空行，单行锚平移到其后首个非空行）；
  被引内容已整体漂走时行号取实际内容位置，或在 §3 登记为已知漂移。

## 3. 外部锚的登记面

外部锚**逐条登记**在 `eng/contracts/anchors/unresolved_registry.json`（只减不增）；
未登记即由对抗性审查判红。登记不是放行：登记项是「暂时无法判」的显式台账，
新增一条要在同一提交里写清 `kind` / `reason` / `owner` / `handoff` 并抬高
`max_entries`（棘轮）。锚能修好时**必须先删登记项**，否则判 `STALE_REGISTRY`。

登记项 `kind` 取值以数据文件为准，不跨登记面代用；现行取值 =
`EXTERNAL_REFERENCE`（指向未 vendor 的外部开源实现或外部许可证据文件）。
`kind` / `reason` / `owner` / `handoff` 四个字段缺一即不算合格登记。

## 4. 内容锚（content anchor）

**适用面**：一切「引用某段规范文本 / 某个文档表行」的位置表达（登记面、测试期望、
判据表、配置键）默认用内容锚，不用行号。

### 4.1 形态（唯一机器形态）

```text
{ "id": "<锚 id>", "path": "<文档/文件仓库相对路径>", "quote": "<被引原文（可多行）>",
  "sha256": "<16 位十六进制内容指纹>", "value_text": "<该引文里被锁住的取值/字面量文本>" }
```

- `id`：区分目标；无区分力 token（单字、单词、泛词）不能充当 id。复合键面
  （配置旋钮、滤镜、协议字段）一律用**复合键路径**（如 `03_star_detection.min_area`、
  `filter-negative.bader r`）。
- `path`：目标文件；文件不存在即判红（fail-closed）。
- `quote`：**归一化后**在目标文件全文内的**连续子串**；长度 ≥ 2 字符
  （`MIN_QUOTE_CHARS`）。
- `sha256`：`quote` 的归一化指纹（§4.2）。
- `value_text`：该锚必须锁住的那个取值 / 判据文本；它必须**逐字出现在 `quote` 内**
  ⇒ 锚锁的是「值」，不是「任意能找到的句子」。

### 4.2 归一化与指纹（唯一算法）

```text
N1  CRLF -> LF
N2  逐行去首尾空白（strip）
N3  行内连续空白折叠为单个空格
N4  丢弃归一化后为空的行
N5  以 "\n" 连接
fingerprint(quote) = sha256(N1..N5(quote).encode("utf-8")).hexdigest()[:16]
```

归一化只做「空白层面」的对齐，**不做**大小写折叠、Unicode 归一或标点替换：
文档大小写 / 全半角改写仍判红。

### 4.3 判据（全部可红）

| 编号 | 判据 | 判红条件 |
|---|---|---|
| D1 | 区分力下限 | `quote` 归一化后 < 2 字符，或锚里混入位置字段（`line` / `token_line` / `ref_line`） |
| D2 | 唯一命中 | 归一化 `quote` 在归一化全文内命中 0 次（锚不成立）或 ≥2 次（无区分力，须加长引文） |
| D3 | 指纹自洽 | 重算指纹 ≠ 登记 `sha256` |
| D4 | 值覆盖 | `value_text` 不在 `quote` 内（锚没锁住该值） |
| D5 | 登记面零行号 | 登记面（JSON 全文）出现 `文件:行`；测试期望或登记点写行号 |
| D6 | 同一目标唯一 | 同一 `id` 在同一面出现两次而 `path` / `quote` 不同 |

命中偏移到行号的换算由实现提供（返回 1 基行号），**该行号只用于报告**，
只读不回写：登记面与测试期望保持零行号。

### 4.4 何时仍用行锚 / 标记行

- 机器必须在**表格 / 矩阵**中定位（表头行、矩阵行、`CFG002-ANCHOR: <id> -> <登记面>`
  标记行）：保留标记行形态；标记行必须进入锚合同并进入判据（由对抗性审查断言恰出现
  一次且指向的登记项存在）。标记行只出现在表格 / 矩阵定位处，散落正文充当「注释」
  不算标记行。
- 正文散文不撒标记：无表格 / 矩阵可定位时不引入标记行。

### 4.5 正例 / 负例

```text
正例：{"id": "detection.threshold_sigma", "path": "docs/science/STAR_DETECTION.md",
       "quote": "| `threshold_sigma` | 5.0 | sigma | ... |", "sha256": "<16 位>", "value_text": "5.0"}
负例 A（锚不成立）：quote 在目标文档内 0 次命中        -> D2 判红
负例 B（无区分力）：quote = "默认"（全文出现 12 次）  -> D2 判红
负例 C（太短）：quote = "4"                            -> D1 判红
负例 D（指纹过期）：改了表行说明文字但没更新 sha256    -> D3 判红
负例 E（值没锁住）：value_text = "6.0" 而引文里只有 "5.0" -> D4 判红
负例 F（行号混入）：登记面写 "docs/science/PSF.md:7" 或锚里带 "line" 字段 -> D5 / D1 判红
```

### 4.6 自检入口（三态：正例绿 → 负例红 → 还原回绿）

- `python3 eng/tests/config/check_cfg002_registry.py --self-test`：CFG002-01/04/09/11/12
  的内容锚负例面（引文漂移、引文无区分力、引文过短、取值文本缺失、指纹过期、
  登记面内嵌行号、退役行复活、非配置表未登记等），每条都要求「注入必红 + 还原必绿」。
- `python3 eng/tests/config/test_cfg001_contracts.py`：关键锚按「文件 + 值文本 +
  内容指纹」逐条断言。

## 5. 审查判据（无机器门执行者）

下列各条由对抗性审查逐条核对；判定只看断言与失败含义，不看执行器。

| 规则 | 断言 | 失败含义 |
|---|---|---|
| C2 anchor_resolved | 每个锚唯一解析到真实跟踪文件，或已登记于 `unresolved_registry` | 目标改名 / 删除，或同名分歧未登记，或新出现的未解析锚未登记 |
| C3 range_in_bounds | 区间锚 start/end 落在 1..目标行数内且 start ≤ end | 目标变短或锚越界（典型漂移） |
| C4 symbol_binding | `bindings` 声明的 (doc, target, symbol)：该文档内必存在指向 target 且行范围内逐字包含 symbol 的锚 | 符号整体消失 = `STALE_BINDING`；符号已移出全部锚范围 = `BINDING_VIOLATION` |
| C5 exemptions_live | 每条豁免必须命中至少一个真实锚 | `STALE_EXEMPTION`（锚已不存在，豁免必须删除） |
| C6 boundary_blank | 已解析且界内的锚，其 start/end 行都不是空行（空行口径 = 只去一个尾换行后的空串；**纯空白行不算空行**） | 锚的边界落在空行 = 锚没指向内容 |
| C8 unresolved_registry | 未解析锚必须已登记；登记项必须仍命中；条目数 ≤ `max_entries` | 新未解析锚未登记 / `STALE_REGISTRY` / 棘轮被突破 |

## 6. 维护义务（同提交规则）

以下各条只适用于**外部锚**。指向本仓任何文件（代码或文档）的引用不走本节：
仓内文档之间一律按 §4 内容锚（同提交更新 `quote` / `sha256` / `value_text`），
仓内代码不再被文档以行号定位。

- **新增解析不到的锚**（外部引用）→ 同一提交登记 `unresolved_registry.json` 并抬高
  `max_entries`；锚能修好时**必须先删登记项**。
- **引用 Markdown 章节**写 `§N`，不要写 `#N`。
- 豁免面与未解析登记面是两处各自独立的登记：豁免 = 永久不判，未解析 = 暂时判不了的
  台账（只减不增）。
