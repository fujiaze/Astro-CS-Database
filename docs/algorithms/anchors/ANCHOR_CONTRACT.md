# ALG/SCI 源码行号锚合同（ANCHOR CONTRACT）

> ID: ALG-ANCHOR-001　任务: SCI-ANCHOR-001　状态: ACTIVE_NORMATIVE（文档锚合同）
> 机器检查器: `docs/algorithms/anchors/check_doc_line_anchors.py`（CI 检查项 DOC-LINE-ANCHORS）
> 数据: `docs/algorithms/anchors/anchor_contract.json`（解析规则 / 豁免 / 符号绑定）
> 　　　`docs/algorithms/anchors/unresolved_registry.json`（未解析锚登记台账，C8 只减不增）
> 上游：ASTROCS_DESIGN.md §0.2（详细文档层与双向索引）

> 上游: `ENGINEERING_SPEC.md` §8（文档—代码一致性 / 锚存活 fail-closed；原引「冻结宪章」已废止）；顶层只收稳定内容。

## 1 适用范围

作用域 = `anchor_contract.json.doc_globs` 命中的全部 Markdown，现为 **`docs/**/*.md`**
（DOC-DRIFT-FIX-01 由 `docs/science/*.md` + `docs/algorithms/*.md` 扩到全 docs 面；
旧作用域只覆盖两目录，`docs/contracts/**`、`docs/modules/**` 的行数锚与边界锚长期无门）。
锚形态：

`@text
path/to/file.ext:N          path/to/file.ext:N-M
path/to/file.ext:N,M        path/to/file.ext:N/L          path/to/file.ext#N
path/to/file.ext:N（symbol）    path/to/file.ext:N + backticked symbol
`@

**`#N` 是行号锚，`§N` 是章节引用——两者各自独立、取值互不代用**（DOC-DRIFT-FIX-01 消歧）：
C 头文件/脚本里的 `#158` 指第 158 行；指向 Markdown 章节时必须写 `docs/contracts/DATA_SEMANTICS.md §4a`，
章节引用一律写全路径；`…#4a` 形态会被扫描器把章节号当行号判界内/判空行（GLOSSARY.md 曾有 12 处此类碰撞）。

现行规模（C7 逐字复测）：**101 文档 / 1821 锚** = 1786 目标锚 + 9 登记豁免 + 26 未解析登记。

> C7 逐字复测本行：数字必须等于检查器实测（口径 = 检查器自己的扫描器；改锚后跑
> `check_doc_line_anchors.py --print-scale` 取现行行替换，口径唯一 = 检查器自己的扫描器）。
> 「文档」= 至少含 1 个锚的文档数（与旧读法一致）；锚总数含豁免与未解析登记。

## 2 锚的语义义务

1. **锚是断言**：`file:N` 断言「本文档所描述的该符号/行为位于 file 的第 N 行」。
   代码移动即锚失效，**必须在同一提交内更新行号**（语义不变，只改数字）。
2. **语义不变**：更新行号时锚所指向的符号、公式、默认容差与冻结门保持逐字不变。
   任何顺手改写语义的编辑都属越界，必须走 SCI/ALG 变更流程。
3. **SCI 修改从 SCI 发起**：当 ALG 文档引述 SCI 冻结声明原文时，SCI 原文保持不动，
   漂移在 ALG 侧显式登记（现行实例见 §4.1 的 FROZEN_SCI_CLAIM）。
4. **一个锚一个目标**：裸文件名锚（如 `sampler.cpp:76`）必须能被唯一解析；
   存在同名多候选时用 §3 的 resolver 显式钉死，解析口径 = 显式 resolver。
5. **边界行只取有内容的行**：`file:N` 的 N、`file:N-M` 的 N 与 M 都必须是**有内容的行**。
   边界落在空行 = 锚没指向任何内容（典型成因：内容整体下移一行后只改了尾不改头）。
   订正方式仍是「只改数字」：区间锚裁掉边界空行，单行锚平移到其后首个非空行。
6. **语义已变的不硬凑**：被引内容已不在该处（不是边界空一行而是整体漂走）时，
   行号取值只来自实际内容位置；按 §5 走「更新行号 + 复核语义」或登记为已知漂移。

## 3 解析规则（checker C2）

按顺序尝试，命中即止：

1. **exact**：把锚文本当仓库根相对路径，存在即采用；
2. **doc-relative**：相对锚所在文档目录解析；
3. **contract-rule**：`anchor_contract.json.resolvers` 中按 `doc`/`doc_prefix` +
   `basename` 匹配的显式规则（**唯一**处理同名多候选的合法手段；
   `basename` 取 `os.path.basename(锚文本)`，故 `cpp/build.ps1` 要写 `build.ps1`）；
4. **basename-unique**：全仓（排除归档标记目录，清单 = `ARCHIVE_MARKERS`，
   `docs/algorithms/anchors/check_doc_line_anchors.py` 顶部常量：archive / 历史实现 / `.git` /
   `third_party` / `build` / `out` / `run` / `worktrees`）唯一同名文件。

四步皆不命中 → 该锚必须**逐条登记**在 `unresolved_registry.json`（C8，只减不增）；
未登记即 `C2_anchor_resolved` FAIL。目标必须是 **Git 跟踪**文件。
登记不是放行：登记项只是「本门暂时无法判」的显式台账，新增一条要在同一提交里
写清 `kind`/`reason`/`owner`/`handoff` 并抬高 `max_entries`（棘轮）。

## 4 检查项

| 规则 | 断言 | 失败含义 |
|---|---|---|
| C1 docs_tracked | 作用域文档存在且被 Git 跟踪 | 文档缺失/未入库 |
| C2 anchor_resolved | 每个锚经 §3 唯一解析到真实跟踪文件，或已登记于 unresolved_registry | 目标改名/删除，或同名分歧未登记 resolver，或新出现的未解析锚未登记 |
| C3 range_in_bounds | start/end 落在 1..目标行数 内且 start<=end | 目标变短或锚越界（典型漂移） |
| C4 symbol_binding | `bindings` 声明的 (doc, target, symbol)：该文档内必存在指向 target 且**行范围内逐字包含** symbol 的锚 | 符号整体消失 = STALE_BINDING；符号已移出全部锚范围 = BINDING_VIOLATION（漂移锚） |
| C5 exemptions_live | 每条豁免必须命中至少一个真实锚 | STALE_EXEMPTION（锚已不存在，豁免必须删除） |
| C6 boundary_blank | 已解析且界内的锚，其 start/end 行**都不是空行**（空行口径 = `read_lines`：只去一个尾换行后的空串；**纯空白行不算空行**，用 rstrip 判空会误报） | 锚的边界落在空行 = 锚没指向内容（DOC-DRIFT-FIX-01 新增，全 docs 面实测 158 条） |
| C7 contract_scale | 本文件 §1「现行规模」行与本门实测逐项相等；该行缺失/不可解析亦判红 | 规模声明过期（旧读数冒充现读数），或声明被删 |
| C8 unresolved_registry | 未解析锚必须已登记；登记项必须仍命中；条目数 ≤ max_entries | 新未解析锚未登记 / STALE_REGISTRY / 棘轮被突破 |

### 4.1 豁免（exemptions）纪律

豁免是**登记**而非放行：每条必须给出 `kind` / `reason` / `owner` / `evidence`。现行三类：

- `HISTORICAL_RUN_ARTIFACT`：锚指向 `run/` 下的历史 bughunt 账本（gitignore 运行产物，
  非仓库源码锚），仅作来源追溯，不构成可复测锚；
- `FROZEN_SCI_CLAIM`：ALG 文档引述 SCI 冻结声明**原文**（如 SCI §5:63 中的
  `integrate.cpp:10-79`），原文行号与实际行数不符属 SCI 侧已知漂移，ALG 文档已显式
  登记且**修改方向 = 从 SCI 到 ALG**；该豁免随 SCI 变更流程一并消除；
- `EXTERNAL_REFERENCE`：锚指向未 vendor 进仓库的外部开源实现（photutils / astropy /
  DeepSkyStacker 等）或外部许可证据文件，属文献与开源对照引用，非本仓源码锚。

豁免（`exemptions`，本仓锚但按上表三类**永久不判**）与未解析登记
（`unresolved_registry`，**本门暂时判不了**的台账，只减不增）是两回事，各自独立登记。

## 5 维护义务（同提交规则）

- **改了被锚定的源码**：符号仍在原锚范围内 → 无需动作；符号移出范围或行号整体位移 →
  checker 报 BINDING_VIOLATION / C6，同一提交内更新文档行号（语义不变）。
  只改了行数（文件变长/变短）→ 同提交更新文档自述的**行数锚**（`file（N 行）`）。
- **新写裸文件名锚**且该文件名在仓内非唯一 → 同一提交在 `resolvers` 增加规则。
- **删除/改名被锚定的文件** → 同一提交更新文档锚，或（仅限 §4.1 三类）登记豁免。
- **新增 `bindings`**：只允许绑定**可逐字复测**的符号（函数名、宏、FITS 关键字、
  CMake 目标名），绑定面只含可逐字复测的符号——泛词绑定（如 signal、pixel）不产生检测力。
- **新增解析不到的锚**（外部引用、被引文档已删、历史留痕）→ 同一提交登记
  `unresolved_registry.json` 并抬高 `max_entries`；锚能修好时**必须先删登记项**
  （C8 会判 STALE_REGISTRY，登记项不能留成僵尸）。
- **引用 Markdown 章节**写 `§N`，不要写 `#N`（见 §1 形态消歧）。

## 6 SCI-ANCHOR-001 全量复测结果（历史基线，作用域 = 旧的两目录）

复测基线：执行时最新 main（三 SHA 一致，见任务证据包）。全量 793 锚复测结论：

| 类别 | 数量 | 处置 |
|---|---|---|
| 锚解析 + 范围 + 符号绑定全绿 | 784 | 无需改动 |
| 漂移锚（行号更新，语义不变） | 36 个锚 token / 37 个文档行 / 9 个文档 | 本提交内更新 |
| 登记豁免 | 9 条锚（7 条豁免键） | 见 §4.1 |

> 本节是 SCI-ANCHOR-001 的**当时快照**（旧作用域、793 锚），不回改历史读数；
> 现行规模与判定面以 §1 的 C7 行与 §4 表为准。

**已订正的漂移锚（按文档）**：

- `docs/algorithms/PHASE3_FITS_IMPL.md`：`p3_output.cpp` 关键字块整体下移（CTYPE1/2、
  CUNIT1/2、CRPIX1/2、CRVAL1/2、CD1_1..CD2_2、BSCALE/BZERO、BUNIT、HIPSID..SWVER
  共 8 组）＋ `p3_session.cpp` 的 hardware_concurrency / std::thread ＋
  `cfitsio_io_mutex`（并订正文件名 mutex.h → aio_cfitsio_mutex.h）。
- `docs/algorithms/PLATESOLVE.md`：`ipv_entry.cpp` C API 13 个锚整体 +29 行
  （`ipv_api.h` 侧锚核对无误，未改）。
- `docs/science/REJECTION.md`：`rejection.h` 状态枚举三行 +1/+1/+2。
- `docs/science/CALIBRATION.md`：`calibrator.cpp:84` → `:86`（文档原文即引述该 if）。
- `docs/algorithms/NOISE_ESTIMATION.md`：`snr_estimator.h:107` → `:112`、
  `default_config :312` → `:333`、`CMakeLists.txt:435-438` → `:490-493`、
  `:513` → `:556`、`noise_model.cpp:179-200` → `:179-204`。
- `docs/algorithms/CALIBRATION_ALGORITHMS.md` / `COSMETIC_ALGORITHMS.md`：
  `CMakeLists.txt:321-333` → `:373-380`（`astrocs_calibration` 目标）。
- `docs/algorithms/PHASE3_RSMP_IMPL.md`：`module_adapters.cpp:425-439` → `:498`
  （`p3_resample2_descriptor`）。
- `docs/algorithms/DRIZZLE_GEOMETRY.md`：DISP-DRZ-001 双方锚由 `api.cpp:88-92`
  订正为 `hp_drizzle_api.cpp:98-103`。

## 7 DOC-DRIFT-FIX-01：作用域扩展与新增判据的实测结果

| 判据 | 扩域前红数 | 扩域后红数 | 处置 |
|---|---|---|---|
| C6 boundary_blank（新增） | 不判 | 158 | 逐条按 §2.5「只改数字」订正（区间裁边界空行 / 单行平移至其后首个非空行） |
| C2 未解析锚 | 不判（旧域内 0） | 50 | 加 10 条 evidence-backed resolver 修好 24 条；余 26 条登记 unresolved_registry（23 外部引用 + 3 历史留痕） |
| C7 contract_scale（新增） | 不判 | 1 | §1 规模行改为 C7 规范句式 |
| 行数锚（ALG-LINE-ANCHORS 门 L1） | 51（仅 algorithms） | 126 | 70 条与实测不符，逐条按实测订正（见该门回执） |

`#N` 形态消歧：`docs/GLOSSARY.md` 里指向 DATA_SEMANTICS 的 12 处章节引用
（原写作 #1 / #2 / #4 / #4a / #5，实际是 §1 / §2 / §4 / §4a / §5）改写为 `§N` 形态；
GLOSSARY 里指向 CALIBRATION / DRIZZLE 的 `#27` 两处（§3 标题已下移到第 28 行）
按「只改数字」订正为 `#28`。本段是订正留痕，不写成可解析锚形态。

## 8 负向验收（必败）

`eng/tests/quality/test_doc_line_anchors.py`（由既有 CI 检查项 `UT-QUALITY` 覆盖）含注入用例：
目标删除、目标截断越界、符号移出锚范围、符号整体消失、锚指向不存在文件、锚越界、
同名分歧未登记 resolver、缺失豁免登记、陈旧豁免、**边界落在空行（start/end 各一例）**、
**规模声明漂移**、**规模声明缺失**、**新增未登记未解析锚**、**登记项不再命中（STALE_REGISTRY）**；
每条均要求检查器 **rc≠0**，并在还原后回到 **rc=0**（证明失败由注入引起）。
`eng/tools/doccheck/check_alg_line_anchors.py --self-test` 覆盖行数锚与逐符号锚的
正例/负例/恢复三态。确定性：同 cwd 双跑与跨 cwd 跑的 JSON 输出逐字节相同
（检查器单进程串行，1/N worker parity 不适用）。
