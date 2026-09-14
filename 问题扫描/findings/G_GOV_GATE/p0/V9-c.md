# V9 收尾 · P0 两条（结构不可红 + **新锁自带静默跳过**）

### V9-N-16（P0·机制①最纯 + 机制③合成）`TRACEABILITY` 门**末行无条件 `return 0`**，且登记 `mutates_workspace=true` **改写受跟踪文件** `reports/v19r2/evidence/quality/traceability_check.json`
- ⇒ **两道同名门并存**：真正判红的是 `CON-TRACEABILITY`（见 `V9-N-01`，A1），而**名为 `TRACEABILITY` 的这道门无论查出什么都绿**，还顺手把结果写进**受跟踪**产物（⇒ 与 `V6-N-04` 的"跑改写型脚本"同族，但这里它是**在册 CI 门**，每天都写）。
- 后果：①**ID 集对账（`E1`）没有载体**，靠这道门保证"ALG 编号 ⊆ 台账"落空；②受跟踪证据文件被 CI 反复改写，掩盖人工订正（与 `C-15`/`§4.3` 相冲）。
- 建议：末行改 `return 1 if failures else 0`；`mutates_workspace` 置 false（或输出改 `run/`，证据走只读比对）。**related** `V9-N-01`、`V6-N-04`、`E1`、`C-15`、`M8-G-001`

### V9-N-17（P0·**本时段最需要隔壁立刻看到的一条**）新建门 `CTEST-P1DRZ-TASKSET-INVARIANCE` 的被测脚本**在 `taskset` 缺失时 `exit 0` 自宣跳过** ⇒ 该锁在缺 `taskset` 的机器上**永不红**
- 语境：这条锁正是隔壁 `8c977118`（DRIZZLE-DET-001，drizzle 累加分组随线程数漂移）的**双向回归锁**，V7 还据此把它记为正例（"已修且带锁"）。
- V9 实测：**被测脚本在 `taskset` 缺失路径上 `exit 0`** ⇒ ctest 目标存在但什么都不测；叠加 `V9-N-09`（39 道 ctest 门**无 `--fail-if-no-tests`**）与 `V9-N-10`（**`exit 77` 被无条件判 `SKIPPED(waivable)`**）⇒ **三层静默同向**：脚本 0、ctest 匹配失败 0、77 吸收 ⇒ **"带锁已修"这件事在任何没有 taskset 的节点上不可证伪**。
- 建议（三步，都很小）：①脚本无 `taskset` 时 `exit 77`→改为**明确 `FAIL` 或在 `waivable=true` 门上才允许 skip**；②`_ctest_argv` 加 `--fail-if-no-tests`；③`run.py:898` 的 77 处理尊重 `waivable`。⇒ 做完这条，我才能把 DRIZZLE-DET-001 的验证从 PARTIAL 升 VERIFIED。
- **related** **`V9-N-09`/`V9-N-10`**、`V7-N-09 §8`（V7 记的正例，**需据此加限定**）、`M9-H-1`、簇 1 机制①⑤、`A-38`
