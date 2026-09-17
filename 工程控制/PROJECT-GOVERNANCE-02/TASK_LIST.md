# PROJECT-GOVERNANCE-02 · 任务清单

上位权威链：`ASTROCS_DESIGN §0` → `AGENTS.md` → `ENGINEERING_SPEC` → `CONTROL_PACK_SPEC` → 本包。
前包：`工程控制/PROJECT-GOVERNANCE-01`（其附录 G/H/H.5/H.6/H.7 对本包**继续有效**，不得重述或绕过）。

## 0. 起点与本包目标

- 入口条件（已满足）：HEAD=`9902c788`；整树门全绿 —— cmake rc=0 · ninja（clean→全量 1097 目标）0 FAILED · ctest **442/442** · 锚点门 **rc=0**（852 anchors / 844 OK / 8 EXEMPT）。
- 本包目标：把「整树绿」推进到「**三条命令端到端跑通 + 台账清零 + 验收实测列就位**」。
- 明确不做：版本号变更（首次外发前版本号只是标签）· 文档集替换 · 任何发布决定（负责人事项）。

## 1. 纪律（每条任务适用；违反即回退）

| # | 纪律 |
|---|---|
| D1 | **构建隔离**：每线用 `run/PROJECT-GOVERNANCE-02/<ID>/build/`；禁止共享 `build/`；禁止增量目录下结论（混合版本会造假红） |
| D2 | **四门**：`cmake` rc=0 + `ninja -k 0` 0 FAILED + `ctest` ≥ 基线（442）0 failed + 锚点门 rc=0 且**本域 0 新增** |
| D3 | **零 git 写**：子代理不 commit/push/stash/checkout/reset/rebase；只写工作树与 `run/` |
| D4 | **冻结面**：不改 `docs/science/**`、`docs/algorithms/**` 的冻结定义；`ci/exemptions.json` 只许缩小 |
| D5 | **判据必须双向可假 + 活性自持**（附录 H.5）：能证明「期望红必红、期望绿必绿」，且不随真实仓库内容变化失效 |
| D6 | **生成物只许改生成器**：文件头有 `generated_by` 或全仓 grep 到写入脚本 ⇒ 改脚本，不手改产物 |
| D7 | **判据复算 > 冻结数字**：计数类断言由注册表/清单**重算**，禁止写死魔数 |
| D8 | **中间态纪律**（附录 H）：删除与引用改绑**同批**；迁移文件成对（D/?）入库；include 面同批 |
| D9 | **证据**：每条结论给「命令 + rc + 日志行号」；禁止以「环境/工具问题」掩盖失败 |
| D10 | **台账**：任何未完成项必须显式登记（原因 + 归属 + 复现路径），禁止静默 |

## 2. 工作包（W1–W8）

### W1 逐命令冒烟（阶段2）
三命令 `normalize` / `mosaic` / `export` 各自**干净环境**跑通，含失败路径。
**验收**：每命令一份「最小真实配置 + rc=0 + 产物清单 + manifest/哈希自洽」；失败路径给 rc≠0 + 逐字诊断 + **零产物**。

### W2 真实数据端到端（阶段3）
T2/T3/T4 全链 `normalize → mosaic → export`。
**验收**：一条链端到端 rc=0 + 天测闭合指标（冻结口径：`snr>20` + 1.0″ + median + 必报匹配率 + 两 WCS 口径分列）+ 与既有冻结值对照（T3 0.5644 px / T2 0.4202 px / T4 0.1077 px）。

### W3 台账清零（阶段1，未达）
入口：SCAN-CLEAR **606 OPEN** · LEDGER-P1 **141** · LEDGER-CI **144**（37 转 W4-A3 含 patch / 93 域外 / 15 待办）· LEDGER-DOC **49 + MOD-002 大面**。
**验收**：每条落四态之一 —— `FIXED`（带红→绿证据）/ `CRITERION-WRONG`（带旧判据 vs 现权威逐字依据）/ `NOT-APPLICABLE`（带理由）/ `TRANSFERRED`（带**具名接手方**）；**不得留 STILL-OPEN 无主项**。

### W4 验收收口
claim 集中重编号（已知 `SCIENCE_CORRECTNESS.md` **两行同为 SC-010**，另有 SC-008/SC-009 撞车）+ 新旧对照 → `ACCEPTANCE_FINAL.md` 实测列（当前 grep 实测值 = **0**）→ `TASK_LIST_STATUS.md` 更新。

### W5 负载敏感 7 条协议实施
`p1drz_merge_pipeline_lock` / `rt001_unique_executor` / `p1cos_performance` / `p1hips_performance` / `cpu006_bench_report` / `test_monitor::test_02` / `p1noise_performance`。
**只改测量协议**（交错、预热、N≥5 取中位、声明独占窗口），**不得放宽阈值**；双向证据（负载下红 / 独占绿）。

### W6 剩余科学/契约项
D08（产品侧标记载荷卡 + §18.5 义务）· D11（SIP-aware 参考解，分别写 `forward_cross_ref`）· AIO 元数据提案（裁定后实施，须与 DET-001 哈希面同批验证）· drizzle(`astrocs_p1_drizzle`) 与 gaia_xpsd_client(`astrocs_catalog_gaia`) 两个 SHARED 面的结构体 ABI 版本化。

### W7 门与 CI 收口
E2E 新门 CI 注册（转 W4-A3 域）· 域外 93 条转办确认 · 新增扫描项（结论与退出码一致性）已登记为 `CHK-EXIT-CONSISTENCY`。

### W8 迁移收口（先于 W1/W2）
W4-A9 批次 4：`hips_properties* → lib/algorithms/coverage/` + resample TU 收口（解环顺序：先立 `astrocs_hips_properties`，再把 `p3_resample.cpp` 编入 `astrocs_p3_rsmp`）。

## 3. 顺序与依赖

```
W8（迁移收口） ──► W1（逐命令冒烟） ──► W2（真实数据端到端）
W3（台账清零） ─┐
W4（验收收口） ─┴─ 可并行（W4 的 claim 重编号须在 W3 收敛后定稿）
W5 / W6 / W7   ── 依各自依赖，可与 W1–W3 并行，但不得写同一文件（多写者须先向后台报备并冻结行号）
```

## 4. 交付格式（每线一份自证摘要）

1. 根因与分类（逐条判：真缺陷 / 判据过时 / 能力已退役 / 跨域）→ 2. 改动 + 改前→改后 + 依据（逐字引用权威条款）→ 3. 门与 rc（四门逐条）→ 4. 未完成项与归属（D10）→ 5. 纪律自证（D1–D10 逐条）。
