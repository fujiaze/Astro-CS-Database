# ALG/SCI 源码行号锚合同（ANCHOR CONTRACT）

> ID: ALG-ANCHOR-001　任务: SCI-ANCHOR-001　状态: ACTIVE_NORMATIVE（文档锚合同）
> 机器检查器: `docs/algorithms/anchors/check_doc_line_anchors.py`（CI 检查项 DOC-LINE-ANCHORS）
> 数据: `docs/algorithms/anchors/anchor_contract.json`（解析规则 / 豁免 / 符号绑定）
> 上游: 冻结宪章文档—代码一致性要求；禁止把易过期内容写入顶层。

## 1 适用范围

作用域 = `docs/science/*.md` 与 `docs/algorithms/*.md` 中的**源码行号锚**，形如：

```text
path/to/file.ext:N          path/to/file.ext:N-M
path/to/file.ext:N,M        path/to/file.ext:N/L          path/to/file.ext#N
path/to/file.ext:N（symbol）    path/to/file.ext:N + backticked symbol
```

现行规模：**36 文档 / 793 锚**（其中 784 为源码/文档目标锚，9 条为登记豁免）。
锚的目标可落在 docs/ 之外（lib/、cli/、CMakeLists.txt 等）。

## 2 锚的语义义务

1. **锚是断言**：`file:N` 断言「本文档所描述的该符号/行为位于 file 的第 N 行」。
   代码移动即锚失效，**必须在同一提交内更新行号**（语义不变，只改数字）。
2. **语义不变**：更新行号不得改变锚所指向的符号、公式、默认容差或冻结门。
   任何顺手改写语义的编辑都属越界，必须走 SCI/ALG 变更流程。
3. **禁止反向修改 SCI**：当 ALG 文档引述 SCI 冻结声明原文时，SCI 原文保持不动，
   漂移在 ALG 侧显式登记（现行实例见 §4.1 的 FROZEN_SCI_CLAIM）。
4. **一个锚一个目标**：裸文件名锚（如 `sampler.cpp:75`）必须能被唯一解析；
   存在同名多候选时用 §3 的 resolver 显式钉死，不得依赖多数投票猜测。

## 3 解析规则（checker C2）

按顺序尝试，命中即止：

1. **exact**：把锚文本当仓库根相对路径，存在即采用；
2. **doc-relative**：相对锚所在文档目录解析；
3. **contract-rule**：`anchor_contract.json.resolvers` 中按 `doc`/`doc_prefix` +
   `basename` 匹配的显式规则（**唯一**处理同名多候选的合法手段）；
4. **basename-unique**：全仓（排除 `/archive/`、`/legacy/`、`third_party/`、`build/`、
   `out/`、`run/`、`worktrees/`、`.git/`）唯一同名文件。

四步皆不命中 → `C2_anchor_resolved` FAIL。目标必须是 **Git 跟踪**文件。

## 4 检查项

| 规则 | 断言 | 失败含义 |
|---|---|---|
| C1 docs_tracked | 作用域文档存在且被 Git 跟踪 | 文档缺失/未入库 |
| C2 anchor_resolved | 每个锚经 §3 唯一解析到真实跟踪文件 | 目标改名/删除，或同名分歧未登记 resolver |
| C3 range_in_bounds | start/end 落在 1..目标行数 内且 start<=end | 目标变短或锚越界（典型漂移） |
| C4 symbol_binding | `bindings` 声明的 (doc, target, symbol)：该文档内必存在指向 target 且**行范围内逐字包含** symbol 的锚 | 符号整体消失 = STALE_BINDING；符号已移出全部锚范围 = BINDING_VIOLATION（漂移锚） |
| C5 exemptions_live | 每条豁免必须命中至少一个真实锚 | STALE_EXEMPTION（锚已不存在，豁免必须删除） |

### 4.1 豁免（exemptions）纪律

豁免是**登记**而非放行：每条必须给出 `kind` / `reason` / `owner` / `evidence`。现行两类：

- `HISTORICAL_RUN_ARTIFACT`：锚指向 `run/` 下的历史 bughunt 账本（gitignore 运行产物，
  非仓库源码锚），仅作来源追溯，不构成可复测锚；
- `FROZEN_SCI_CLAIM`：ALG 文档引述 SCI 冻结声明**原文**（如 SCI §5:63 中的
  `integrate.cpp:10-79`），原文行号与实际行数不符属 SCI 侧已知漂移，ALG 文档已显式
  登记且**禁止反向修改 SCI**；该豁免随 SCI 变更流程一并消除。

## 5 维护义务（同提交规则）

- **改了被锚定的源码**：符号仍在原锚范围内 → 无需动作；符号移出范围 →
  checker 报 BINDING_VIOLATION，同一提交内更新文档行号（语义不变）。
- **新写裸文件名锚**且该文件名在仓内非唯一 → 同一提交在 `resolvers` 增加规则。
- **删除/改名被锚定的文件** → 同一提交更新文档锚，或（仅限 §4.1 两类）登记豁免。
- **新增 `bindings`**：只允许绑定**可逐字复测**的符号（函数名、宏、FITS 关键字、
  CMake 目标名），禁止绑定泛词（如 signal、pixel）——泛词绑定不产生检测力。

## 6 SCI-ANCHOR-001 全量复测结果（2026-09-12）

复测基线：执行时最新 main（三 SHA 一致，见任务证据包）。全量 793 锚复测结论：

| 类别 | 数量 | 处置 |
|---|---|---|
| 锚解析 + 范围 + 符号绑定全绿 | 784 | 无需改动 |
| 漂移锚（行号更新，语义不变） | 36 个锚 token / 37 个文档行 / 9 个文档 | 本提交内更新 |
| 登记豁免 | 9 条锚（7 条豁免键） | 见 §4.1 |

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

## 7 负向验收（必败）

`tests/quality/test_doc_line_anchors.py`（由既有 CI 检查项 `UT-QUALITY` 覆盖）含注入用例：
目标删除、目标截断越界、符号移出锚范围、符号整体消失、锚指向不存在文件、锚越界、
同名分歧未登记 resolver、缺失豁免登记、陈旧豁免；每条均要求检查器 **rc≠0**，
并在还原后回到 **rc=0**（证明失败由注入引起）。确定性：同 cwd 双跑与跨 cwd 跑的
JSON 输出逐字节相同（检查器单进程串行，1/N worker parity 不适用）。
