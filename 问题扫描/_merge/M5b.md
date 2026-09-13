# M5b · 合并复核台账（第二层）

- 代理码: **M5b**（域：CLI 单入口 / C ABI / 模块注册与构建安装面 + 治理与发布门禁）
- 输入切片: `_cache/L12.md`（24 条）+ `_cache/L17.md`（22 条）= **46 条**
- 定稿产出: **43 条 finding**（P0 8 / P1 28 / P2 7），按「类别 × 优先级」合并为 10 个文件：
  - `findings/G_GOV_GATE/p0/M5b_L12_L17.md`（G-01…G-06）
  - `findings/G_GOV_GATE/p1/M5b_L12_L17.md`（G-07…G-20）
  - `findings/C_DOC_CODE_GAP/p0/M5b_L12_L17.md`（C-01…C-02）
  - `findings/C_DOC_CODE_GAP/p1/M5b_L12_L17.md`（C-03…C-07）
  - `findings/E_TRACE_BREAK/p1/M5b_L12_L17.md`（E-01…E-05）
  - `findings/E_TRACE_BREAK/p2/M5b_L12_L17.md`（E-06）
  - `findings/I_DOC_HYGIENE/p1/M5b_L12_L17.md`（I-01…I-02…I-03）
  - `findings/I_DOC_HYGIENE/p2/M5b_L12_L17.md`（I-04…I-05…I-06…I-07…I-08）
  - `findings/F_TEST_GAP/p1/M5b_L12_L17.md`（F-01）
  - `findings/F_TEST_GAP/p2/M5b_L12_L17.md`（F-02）
- 纪律遵守: 全程只读（read/grep/glob），未执行任何 shell / 构建 / 测试 / 脚本 / git；未修改任何被扫描文件；仅写 `问题扫描/findings/**` 与本文件。

---

## 0 主控指定的四组焦点结论

### 焦点 1 · L12-001：这道门验证的对象与用户实际得到的对象**不是同一个**（定稿 P0 = M5b-G-01）
1. `ci/steps/linux_build_root_graph.sh` **确实**在根图之外额外跑 `cmake -S cli -B build/cli` + `cmake --build build/cli --target astrocs`（现 :22-24），把根图产物 cp 成 `build/astrocs`（现 :25），二者同名 `astrocs`；本步骤对两个产物只做 `version --json`（现 :50）。
2. `tests/cli` 的 CLI 协议/golden 族与 `tools/check_api_docs.py::check_command_tree` 的目标都是 **`build/cli/astrocs`（兼容子图产物）**；`cli/CMakeLists.txt:1-8` 自我声明该树是「COMPATIBILITY 声明（BLD-002，**非产品事实源**）」，且 :251-252 无 install 规则。
3. **二进制缺失时确实没有 else 分支** → `check_api_docs.py:132-133` 只有 `if os.path.isfile(exe):`，无 else、无告警 ⇒ 命令树检查退化为纯文档结构检查；`API-DOCS` 同时挂在 `windows-main`，而路径无 `.exe` 后缀 ⇒ **Windows profile 下该子判断恒为零检查**。
4. `cli/CMakeLists.txt:164` **确实**把 `aio_pipeline_engine.cpp` 编入（ARCH-001 §7 :80 把它登记为 LEG-003 / P1-003 违例），并用 `ACS_WARN_SUPPRESS="-w"`（:18、:165）静默；根 `CMakeLists.txt` 内 `aio_pipeline_engine` 命中数 = **0**（只编 `aio_pipeline.cpp`，:328）。
5. 叶子措辞修正：`tests/cli` 并非「全部」用兼容树 —— `test_p1003_drizzle_path.py:13` 用 `build/astrocs`（根图 cp 副本）、`test_cli_single_install.py` 扫安装树；且 `test_cli_protocol.py::built` 缺失时会自行 `cmake -S cli` 现场构建，故本地跑测永不暴露错位。
6. 结论：命令树/help/退出码/JSONL 的 CI 级证据建立在**与交付物源码集不同**的二进制上（一个含宪章判为违例的第二调度器），且该证据在 Windows 面可静默归零 ⇒ §12.3-1/-5、§14.4 的 CLI 面验证与交付面不对应。

### 焦点 2 · 「门能否失败」逐条判定（L12-002 / L17-011 / L17-015）
| 门 | 能否失败 | 判定依据（定稿时点复算） | 定稿 |
|---|---|---|---|
| `CON-BUILD-GRAPH` | **能，但方向相反** | `check_build_graph.py::main` 只判 `"phase2"/"astrocs-stage2"/"calibrated_pair_diag"/"rejection_cli"` 四串是否**出现在 BUILD_GRAPH.md 文本**中 + `lib/phase2/CMakeLists.txt` 四个 `add_*` 前缀；从不读根 `CMakeLists.txt`/`install_layout.cmake`/File API ⇒ 删掉过期目标名（订正文档）即 FAIL；而按产品目标名集合 grep `BUILD_GRAPH.md` 命中 = **0** ⇒ 产品结构永不可失败 | M5b-G-05（P0） |
| `DOC-L0` | **能，但对着错误目录** | `check_l0_docs.py` 判据 = 5 份 `docs/review/**` 存在 + ≥200 字节 + 文件名出现在 `REVIEW.md`；宪章 §12.1（:410-421）点名的是 `docs/owner/**`（零覆盖），其中 `docs/owner/PHASE_OVERVIEW.md` **全仓不存在**（唯一命中是宪章第 416 行）⇒ 「L0 面缺文档/结论无当前 SHA」永不红 | M5b-G-07（P1） |
| `AGENTS-GOV` | **能（仅关键词/启发式）** | `check_agents_gov.py` 判据 = 10 组字面量子串全含 + `Git Bash/pwsh` 行须含"禁止"；`FORBIDDEN = []` ⇒ docstring 的"无冲突条款"未实现；表内宪章条款号从不出现在 `REQUIRED` ⇒ 条款号写错永不红。**反向锁定实证**：`("状态机", [... "REVIEW_PENDING" ...])` 使 AGENTS.md 必须保留 `REVIEW_PENDING` 字面量，而 `tools/quality/validate_task_ledger.py:42-43` 的 STATUSES 不含它、:208/:229 的 selftest 负例名即 `illegal_state`；该验证器在 `ci/checks.json` 命中 = **0** ⇒ 唯一权威状态机（宪章 §14.5）无门，AGENTS.md 只能加"该值已非法"的注解自保 | M5b-G-08（P1） |

### 焦点 3 · 版本单源簇：**并档为一条根因**（M5b-G-04，P0）
- 合并来源：L17-002 + L12-006 + L12-007（同一根因，保留全部 path:line 证据）。
- 复算副本数 = **6**（叶子报 3）：`packaging/astrocs.product.json:3`、`packaging/install-tree.contract.json:5`（+`root_layout` :6 内嵌目录名）、`packaging/dependency-lock.json:5`、`packaging/schemas/install-tree-contract.schema.json:12`（JSON-Schema `const`）、`packaging/schemas/astrocs-product.schema.json:10`（`pattern`）、`ci/checks.json:16`（`--expected` 硬编码当前值 = 第五份手工副本）。
- **新叶未报事实**：两份 schema 用 `const/pattern` 把旧版本钉成第二事实源，且**全仓无消费者**（`verify_install_tree.py::main` 不 import jsonschema、不加载任何 schema）⇒ 现在不报错，一旦接 validator 即锁死旧版本。
- 「门无法发现本条漂移」**成立**（逐面复算）：`ci/check_version.py` 扫描面 = VERSION / 根 `project(VERSION)` / 依赖链在位 / `cli/**`+根 CMake 字面量（[4e]）/`DOC_SET_FILES`+`docs/governance`+`docs/owner`（[5]）⇒ `packaging/**`、`lib/**`、`ci/checks.json` 全不在内；`tools/doccheck/check_version_namespaces.py` 的 `SCAN_FILES`（6 个根文件）+`SCAN_DIRS`（同两目录）+`LOG_FILES` 同样不含 packaging，且 `NAMESPACE_EXEMPT` 整行豁免 `module_version\|module.yaml` ⇒ module 命名空间借用与漂移均不可见。无任何 CI 项把 packaging 对 VERSION 校验（只有 `WIN-PACKAGE`/`WIN-CANDIDATE-VALIDATE` 在 windows-main 列 `packaging/**` 触发路径）。
- 叶子修正：`cmake/astrocs.product.windows.json.in:3` 是 `@ASTROCS_BASE_VERSION@` **生成**（非手抄），故该子主张剔除；漂移实际只在 Linux 交付清单 + 两份合同/锁 + 两份 schema + `--expected`。
- 附带实证：`lib/calibration/{types.h:20, module.yaml:16}` 已随版本升到 alpha.2，`lib/star_detector/module.yaml:45` 仍 alpha.1 ⇒ 同族 module_version 内部即不一致。

### 焦点 4 · fail-open 双实例（M5b-G-02 / M5b-G-03，均 P0）
- **SPEC 指定的正式检查器存在**：`tools/traceability/check_traceability_matrix.py`（463 行）**fail-closed**（`::main` 现 :281/:415 `return 1 if errors else 0`；未捕获异常 → `SystemExit(3)`，:458-463），且**已挂 CI** = `TRACEABILITY-MATRIX`（`ci/checks.json:1103-1126`，三 profile、`waivable=false`）。
- ⇒ 根因判定为「**三套矩阵/三道门并存无权威**」，**不是**"接错检查器"：
  - `TRACEABILITY`（:36-62，waivable=false，触发面覆盖 `lib/** cli/** tests/**`）→ `tools/quality/check_traceability.py`：读非权威的 `docs/TRACEABILITY.csv`，且 `::main` **无条件 `return 0`**（全文件 `return 1` 命中 = 0）⇒ 恒绿；
  - `TRACEABILITY-CODE`（:63-87）→ `tools/check_traceability.py`：无参运行 ⇒ 默认校验 `artifacts/prerelease_v5/tables/TRACEABILITY.csv` **预发布快照**；
  - `TRACEABILITY-MATRIX`（:1103-1126）：fail-closed，但 changed_paths 只有 `schemas/... docs/** evidence/**` ⇒ **代码侧断链没有任何一道门会红**。
- AST-API：`tools/check_ast_api.py::main` 的 declared 集合与 ast_names 集合**同源**（都取自被检头文件自身，:49 与 :59 对同一 `txt`/`dump`）⇒ 恒真；`nparams` 在 :53-54 算出、:60-62 只用 name ⇒ 参数从不比较；全文不读任何 API 文档；`DOC-004_PASS` 文案与 `gen_module_readmes.py:3`、`RT-001.md:55` 的 DOC-004 三个对象同 ID。
- 定稿：G-02（追溯，含 L17-001 + L17-018 并档）、G-03（AST-API）。

---

## 1 逐条处置表（四态分列，46 条全覆盖）

### 1A 仍成立（44 项 → 并档后定稿 41 条）
| 来源 | 叶子级别 | 处置 | 定稿 ID | 一句话理由（定稿时点复算） |
|---|---|---|---|---|
| L12-001 | P0 | 成立（措辞修正） | M5b-G-01 (P0) | 焦点 1 全五项复算成立；`tests/cli` "全部"限定为"绝大多数 golden/协议族" |
| L12-002 | P0 | 成立 | M5b-G-05 (P0) | 门只做子串存在性；产品目标在 BUILD_GRAPH.md 提及 0 次 |
| L12-003 | P0 | 成立（判据重述） | M5b-C-01 (P0) | 422 行全 VERIFIED / 396 行 `TST-GEN-001`（tests/ 与 docs/traceability/ 命中 0）；`SCI-SCOPE-001` 改判为"全局范围 ID 冒充逐函数上游" |
| L12-004 | P0 | 成立 | M5b-G-03 (P0) | 自反恒真 + arity 弃用 + 不读 API 文档 |
| L12-005 | P0 | 成立 | M5b-C-02 (P0) | Windows 模板与 Linux 清单同 `source_commit`、同 unit 状态互斥（IMPLEMENTED vs SKELETON） |
| L12-006 | P1 | **并档** | M5b-G-04 (P0) | 与 L17-002 同根因；剔除 windows.json.in 子主张 |
| L12-007 | P1 | **并档** | M5b-G-04 (P0) | 版本命名空间借用属同一"多副本 + 门不可见"根因 |
| L12-008 | P1 | 成立 | M5b-G-13 (P1) | `BOUNDARY_ONLY` 单头 + PASS 文案报 `len(HEADERS)` + :30 死分支 |
| L12-009 | P1 | 成立 | M5b-E-04 (P1) | `acs_status/acs_head` 两头族各定义一次；`tests/abi/run_abi_checks.sh` 不存在 |
| L12-010 | P1 | 成立 | M5b-G-14 (P1) | 导出门只判非空；5 科学模块 DLL 不在范围；Linux 面零覆盖 |
| L12-011 | P1 | 成立 | M5b-G-15 (P1) | `tools/check_cli_protocol.py` glob = 0；CLI-COMMAND-LAYER 只判两串存在 |
| L12-012 | P1 | 成立 | M5b-C-03 (P1) | `kRules:61` 有 drizzle、`kHelp` 与 §1 无；`cmd_drizzle:1835` → `ARGS` |
| L12-013 | P1 | 成立 | M5b-E-03 (P1) | `include/astrocs/exit_codes.h` 与 `schemas/` 顶层均不存在；检查器三候选回退吸收错误 |
| L12-014 | P1 | 成立（措辞修正） | M5b-G-06 (P0) | §18.4 白名单无机器实现：产品静态链、`loader/registry` 不入根图、sha256 全 null（10 处）、mod001 不被 discover 收集 |
| L12-015 | P1 | 成立（类别改判） | M5b-C-04 (P1) | `int state` 非原子；`destroy` 先 free 后再判 double（读已释放内存）且签名为 void |
| L12-016 | P1 | 成立 | M5b-C-05 (P1) | ERROR_MODEL 规范化 LEG 码表；`RESOURCE→5` 与唯一源 10 并存；`docs_machine_consistency` 在 CI 命中 0 |
| L12-017 | P1 | 成立（再锚定） | M5b-E-01 (P1) | `register_phase_modules/p1_nodes/p2_nodes/p3_nodes` 现 :5447/:5471/:5496/:5523，文档写 :4257/:4282/:4309 |
| L12-019 | P1 | 成立 | M5b-E-02 (P1) | `lib/phase3` glob = 0；文档 FROZEN 且四单元 ≠ 生产五节点 |
| L12-020 | P1 | 成立 | M5b-C-06 (P1) | `{"provider","baseline"}` 常量 4 处 vs 真实读取 1 处；`started/finished` 同取秒级时钟 |
| L12-021 | P2 | **升为 P1** | M5b-G-16 (P1) | `kConfigTemplate:101` `"output_dir": "."` + `UT-CLI.dirty_ignore_prefixes`（:1407-1415）以豁免替代拦截，违反 AGENTS.md 强制目录规范 |
| L12-022 | P2 | **升为 P1** | M5b-G-20 (P1) | 门覆盖 5/21 模块，且与 registry 页（26）无对账 |
| L12-023 | P2 | 成立 | M5b-E-06 (P2) | `P1\.READ\|P2\.SAMPLER` 在 cli/ 命中 0；词表缺 Phase3；`DIAGNOSTICS_STANDARD.md` 不存在 |
| L12-024 | P2 | 成立 | M5b-F-02 (P2) | `class TestManifestVerify` 在 `if __name__`（:230）之后 |
| L17-001 | P0 | 成立 | M5b-G-02 (P0) | 焦点 4：`return 0` 恒定 + 三套矩阵并存（含 L17-018） |
| L17-002 | P0 | 成立（并档） | M5b-G-04 (P0) | 焦点 3：复算 6 副本 |
| L17-003 | P0 | 成立，**降为 P1** | M5b-G-09 (P1) | 权威倒挂确凿（ARCHITECTURE_OVERVIEW:93 把 ARCHIVED 文件列 ACTIVE_NORMATIVE），但宪章 §1.1 已保证裁决结果不受影响 ⇒ 无直接数值后果；数值面由 M5a 承载 |
| L17-005 | P0 | 成立，**定为 P1** | M5b-G-17 (P1) | 以文件自述为准陈述；**基线新鲜度不可判**（禁 git）⇒ 不定性为时间线矛盾 P0 |
| L17-006 | P1 | 成立 | M5b-G-10 (P1) | `build/root-cmake` 从不由 CI 产生 + `if bin_path.exists()` 无 else + `nm` 无 timeout + 源码判据只 glob `lib/phase2/src` |
| L17-007 | P1 | 成立 | M5b-G-11 (P1) | 注册表无反向 heavy 规则；CI 零 `--gate-required`；providers 三处 `workers = 1` 不被两门抓 |
| L17-008 | P1 | 成立 | M5b-E-05 (P1) | 宪章无字母节（`§H\|§F.1\|§K` 命中 0），L0/README/RELEASE_STANDARD 仍以"宪章 §X"引用 |
| L17-009 | P0 | 成立，**降为 P1** | M5b-G-12 (P1) | 覆盖假象 + AND 恒不触发 + `ALLOWLIST` 零引用；后果面（HISS 不可见）属 M5a 的 F00-03 并档 → 无独立 P0 数值后果在本域 |
| L17-010 | P1 | 成立 | M5b-F-01 (P1) | `-p test_ci001b_*.py` 只命中 2/21；heavy/monitor 不变量唯一断言处不在 CI |
| L17-011 | P1 | 成立 | M5b-G-07 (P1) | 焦点 2 第 2 行 |
| L17-012 | P1 | 成立 | M5b-I-02 (P1) | README:11 仍宣称 `run --phases` 已实现；:20 与 RELEASE_STATUS:24 互斥 |
| L17-013 | P1 | 成立 | M5b-I-01 (P1) | `docs/RELEASE_STATUS.md` 以 V19R8 结论 + 废止门字面量现行化（含 F00-02 四份并存事实） |
| L17-014 | P1 | 成立 | M5b-G-19 (P1) | 112 项 CI 无图像/视觉审核项；`VISUAL_CHECK_README:8` 指向不存在的 `launch/` |
| L17-015 | P1 | 成立 | M5b-G-08 (P1) | 焦点 2 第 3 行 |
| L17-016 | P1 | 成立 | M5b-C-07 (P1) | ALG :20/:190 仍 `SIN/ZEA/CAR/AIT`，宪章 §18.1 冻结 `TAN/SIN/CAR/AIT`，同文档 :106-109 自证 |
| L17-017 | P2 | 成立 | M5b-I-04 (P2) | CHANGELOG:3 `[0.11.0-alpha.1] — Current Alpha` |
| L17-018 | P1 | 成立（并入 G-02） | M5b-G-02 (P0) | `DEFAULT_TABLES` 指向 `artifacts/prerelease_v5` 快照，触发面却是 `docs/**` |
| L17-019 | P2 | 成立 | M5b-I-05 (P2) | `INVENTORY_REPORT:6/:18/:31` 写"70 项"，实测 112；`impact_map:649` `launch/**` 已迁走 |
| L17-020 | P2 | 成立 | M5b-I-06 (P2) | `doc_classification:7` 登记 `docs/RELEASE_STATUS.md,CANDIDATE`；`risk_verification_T012:3` 引用 `tools/stage2.cpp`（不存在） |
| L17-021 | P2 | 成立，**升为 P1** | M5b-I-03 (P1) | `SCIENCE_OVERVIEW.md:81/82` 同一模块同 verdict 两行；:74-80 状态列混用裸文本/`IMPLEMENTED`/"部分" |
| L17-022 | P2 | 成立 | M5b-I-08 (P2) | `VERSION_NAMESPACES.md:8` "版本: 2" vs :22 "（本文档 = 1）" |

### 1B 已被修复（0 条）
| 来源 | 叶子原判据 | 当前树事实 | 是否有回归保护 | 处置 |
|---|---|---|---|---|
| — | — | — | — | **本域 46 条中无一条在当前树完全不再成立**（并发提交 `9a3b5a7d` 等修复的是资源门观测/HISS 覆盖面，属 M5a 域） |

### 1C 部分修复（1 条）
| 来源 | 叶子原判据 | 已被改动的部分 | 残留事实（定稿） | 回归保护 |
|---|---|---|---|---|
| L12-018 | 「`astrocs.phase3.resample` descriptor 已定义但**未进入注册表**，registry 页仍称 production 模块」 | 现 `lib/core/src/module_adapters.cpp::register_phase_modules`（:5447）**确实**注册了 `phase3_descriptor()`（:5460-5465）⇒ 叶子的"未注册"事实已不成立（且注册路径用的是 `make_session_module<P3Api>` 整阶段 Session 包装，与宪章 §8.2/§8.4 方向相悖；代码注释 :5521-5522 自认"P2 模板复制残留/占位，由 P3-RSMP-INT 处理"） | 残留 = registry 合同页 :15/:40/:52 仍冒认"Registry production"+"1/N 等价已验"+"TEST-P3-RES-001 对应测试"，而八层矩阵同一行（:25）把 SRC/TEST/EVIDENCE 记 MISSING、`module_ports.registry.json` 只有 `resample2`（:268）、CLI IR 也只有 `resample2`（`runtime_client.cpp:215`） | **无**：不存在 registry 页 ↔ `register_phase_modules` ↔ `module_ports` ↔ CLI IR 节点集的机器对账门。已按"同一事实只定稿一处"并入 M5b-G-18 的「建议处置」（新增 MODULE-REGISTRY 门），不另立 F_TEST_GAP 条目 |

### 1D 无法判定（3 项 → §6 待复核）
| 项 | 无法判定的具体子问题 | 需要的权限/操作 |
|---|---|---|
| L17-005 | `REVIEW.md`（BASE=`da3c4b4a`）与 `ci/known_failures.json`（base=`f8778bbb`，generated 2026-09-12T05:20Z）**谁更新**、红灯是否仍在 | 允许 `git log/show` 或允许执行 `python3 ci/run.py --profile linux-main`；本审计禁 git/执行 ⇒ 定稿时已按"以文件自述为准"表述并定 P1 |
| L12-014 ④ | `mod001_install_load_check.py` 在当前树是否仍 64/64 PASS（含 sha256/负向注入路径） | 需要 cmake configure+build+`--prefix` 安装后跑脚本；只读模式下不可判 ⇒ 只定"无 CI 接线 + L0 引用不可核" |
| L12-001 附加 | 产品二进制与兼容二进制的命令面是否**实际**已有差异 | 需要两棵构建 + `--help` diff；只读模式不可判 ⇒ 只定结构性风险（源码集不同 + 缺失即零检查） |

---

## 2 关键复算（原始 vs 定稿）
| 复算项 | 叶子原文 | 定稿时点实测 | 差异处置 |
|---|---|---|---|
| `API_CONTRACTS.csv` 规模 | "102 行、91 条记录" | **423 行、422 条 `^API-` 记录**，VERIFIED 422、`TST-GEN-001` 396、`SCI-SCOPE-001` 396 | 按实测数字定稿；叶子规模已过期（并发提交追加） |
| `ci/checks.json` 项数 | L17 报 111 | **112** | 采用实测；元文档仍写 70（入 I-05） |
| BUILD_GRAPH.md 产品目标提及 | "极少/以 phase2 为主" | **0 次**（按 `astrocs_runtime\|astrocs_io\|astrocs_noop\|astrocs_cpu_baseline\|astrocs_catalog_gaia\|add_executable(astrocs` 集合 grep） | 强化为"零覆盖" |
| 版本副本数 | 3 处 | **6 处**（含两份 schema + `--expected`） | 并档后扩写 |
| 追溯检查器份数 | 2 份并存 | **3 份**（1 份 fail-open + 1 份读快照 + 1 份权威 fail-closed） | 改判为"三套并存无权威" |
| `module_adapters.cpp` 注册表锚 | 叶子复验时 :5435/:5459/:5484/:5511 | 现 **:5447/:5471/:5496/:5523**（会话内即漂移） | 见 §5 |
| `tests/cli` 目标二进制 | "全部" | 13+ 个协议/golden 族指向 `build/cli/astrocs`；`test_p1003_drizzle_path.py`、`test_cli_single_install.py` 例外 | 措辞限定 |
| `pc_api.cpp` omp | 前台 F00-03 报 19 | 现树复算：`#pragma omp parallel` **19**、`num_threads` **0** | 与 F00-03 一致（转 M5a 引用） |
| module 集合 | 5（门内） | `lib/*/module.yaml` **21**、`lib/*/README.md` **7**、`docs/modules/registry/*.md` **26** | 覆盖率 5/21 入 G-20 |
| provider 常量 | 未给计数 | `{"provider","baseline"}` **4** 处、真实读取 **1** 处（:5213） | 定稿引用 |
| `sha256` 空值 | "全部 null" | `packaging/astrocs.product.json` 内 `"sha256": null` **10** 处 | 定稿引用 |

---

## 3 剔除与降级显式清单
**剔除（4 项子主张，均不静默丢弃）**
1. L12-006 子主张「`cmake/astrocs.product.windows.json.in` 手抄旧版本字面量」→ 该文件 :3 为 `@ASTROCS_BASE_VERSION@`（configure 生成），**不成立**，从版本簇中剔除。
2. L12-003 子主张「`SCI-SCOPE-001` 属未定义即引用」→ 该 ID 在 `docs/science/SCIENCE_SCOPE.md:61` 有定义 ⇒ 剔除"未定义"定性，**改判**为"396 个函数的科学上游一律挂全局范围 ID，等价于逐函数关联缺失"（结论不变，判据换掉）。
3. L12-014 子主张「`secure_loader.c` 仅被 `tests/abi/*` 引用」→ `runtime/registry/module_registry.c:502` 亦引用；**修正**为"`runtime/registry` 与 `runtime/module_loader` 均不入根构建面"（结论不变）。
4. L12-018 子主张「descriptor 未进入注册表」→ 现 :5460-5465 已注册 ⇒ 剔除，残余事实见 §1C。

**降级（4 条）**
| 来源 | 原 | 定 | 理由 |
|---|---|---|---|
| L17-003 | P0 | P1 (M5b-G-09) | 权威倒挂属实，但宪章 §1.1 已保证冲突时以宪章为准 ⇒ 无数值/交付后果；具体阈值后果由 M5a 资源门条目承载 |
| L17-005 | P0 | P1 (M5b-G-17) | 主控令 + 禁 git ⇒ 无法定序，按"以文件自述为准、无法确定基线新鲜度"表述；确凿部分只到"两份活动文档互斥且均无当前 SHA 证据指针" |
| L17-009 | P0 | P1 (M5b-G-12) | 门覆盖假象成立，但其后果面（HISS 裸 omp 对门不可见）与 F00-03 同源，由 M5a 定 P0 根因；本域不重复登记为 P0 |
| L12-016 | P0（L12-014 派生的 §18.4 项） | 保留 P0（G-06），但把 L12-014 原 P1 升为 P0 | §18.4 是负责人裁决条款且 §17-3/§17 门禁直接引用"签名清单"⇒ 属发布阻断级证据缺口 |

**升档（4 条）**：L12-021 P2→P1（G-16）、L12-022 P2→P1（G-20）、L17-021 P2→P1（I-03）、L12-014 P1→P0（G-06）。

---

## 4 移交清单
| 项 | 移交给 | 内容 |
|---|---|---|
| L17-004 | **M5a** | 资源门阈值与 §18.2/§17.6 冲突：`cli/resource_gate.h::compute_cores_threshold`（现 :181-186）用 `0.80*m`、`::gate_window_representative`（:175-179）+`::kMon001QueueUtilMinPercent=50.0`（:107）替代 60% 规则；**新增实证**：同文件现 :153 注释自述「阈值(85%/60%/32MB/s/10s)与判定式不动」而代码无 60，且 :233-234 存在 `if (!gate_window_representative(g)) return Ok;` / `wall_seconds<5 → Ok` 两条绕过分支。按主控令与 L11-002 并成一条，本域不重复定稿。 |
| F00-03（线程预算门对裸 omp 不可见） | **M5a** | 本域只复算了 `pc_api.cpp` 现树事实（19 处 omp / 0 num_threads）与 `SERIAL-HARDCODE\|THREAD-BUDGET\|SERIAL-HEAVY` 三门的扫描面缺口（G-11/G-12），根因条目请由 M5a 并档。 |
| F00-01（两份追溯矩阵并存）+ 旧表 DOCUMENT_INDEX 状态 | **M6 / 前台** | 本域 G-02 只定"门 fail-open + 代码侧无覆盖"；矩阵/旧表谁是真源、`docs/TRACEABILITY.csv` 在 `DOCUMENT_INDEX.yaml:66-69` 的 ACTIVE_NORMATIVE 降级请 M6 定稿。 |
| DOC-004 同 ID 三对象 | **M6** | `AST-API`/`gen_module_readmes`/`RT-001.md:55` 的 ID 冲突已记在 G-03 与 I-07，文档 ID 归口请 M6 统一。 |
| F00-04（测光双实现 / 第二实现永不进交付面） | **M3 + M5a** | 本域复算：`CMakeLists.txt:484` 只编 `lib/phase1/photometry/photometer.cpp`；`lib/photometric_calib/module.yaml:25` 自述"未编入根 CMake 主构建"、:53 `entrypoint: MISSING`、:24 迁移目标 DLL 标"合同值，尚未建立"；`cmake/install_layout.cmake` 与 `packaging/**` 对 `photometry\|photometric` 命中 = **0**；而八层矩阵第 9 行把 SRC 层指向 `photometric_calib.h::pc_calibrate_*` 并标 VERIFIED、TEST 层为 `TEST-PHOT-DESIGN-001`（在 `tests/` 只命中 `tests/unit/p1_phot_test.cpp:505` 的一行注释）⇒ 注册/构建/安装/打包四面不一致，请 M3 定科学面、M5a 定线程面。 |
| F00-05（「本版实测」无 SHA 证据指针） | **M6 + 前台** | 本域只在 G-17/G-06 内引用其结论（L0 绿证据指向 gitignore 的 `run/docconv001/logs/`），独立条目请 M6 定稿。 |
| L12-015（并发实例状态机） | **M5a（并发族）** | 定稿为 C-04，已在 related 中指向 M5a 的并发条目，请并档避免与 L11 并发面重复。 |

---

## 5 行号漂移处置
1. 本域 46 条中，**行号全部在定稿时点重取**：先用符号名/关键语句（`::main`、`check_command_tree`、`BOUNDARY_ONLY`、`SCIENCE_MODULES`、`kRules`、`compute_cores_threshold`、`"const": "0.11.0-alpha.1"` 等）grep 定位，再取命中行；所有定稿条目的「位置」字段一律写成 **`path::符号`（现 :N）** 形式，行号仅作辅助、可随时失效。
2. **会话内实测漂移证据（硬证据，非推测）**：
   - `tests/cli/test_cli_protocol.py::HELP_LINES` 分组行在本会话两次读取之间由 `…|rejection_integration\|pipeline>` 变为 `…|rejection_integration\|p1_ir_facade>`；
   - `lib/core/src/module_adapters.cpp` 的注册表四锚在叶子复验时为 :5435/:5459/:5484/:5511，本域定稿时为 :5447/:5471/:5496/:5523（并发提交追加）；
   - `ci/checks.json` 项数 111 → 112、`docs/contracts/API_CONTRACTS.csv` 记录数 91 → 422 同属并发追加；
   - `cli/commands.cpp` 内 `provider\|"baseline"` 与 `output_dir` 命中集在两次读取间行数不同（:842/:1016/:1042/:1107 等）。
3. **文档侧引用代码行号者一律重锚**：`MODULE_MAP.md:32-33`、`RELEASE_STATUS.md:70-71`、`REVIEW.md:82` 引用的 `module_adapters.cpp:4257/:4282/:4309/:3777-3793` 现落在 `inspect()` 收尾花括号与 config 解析 catch 块上 ⇒ 该事实已升级为 M5b-E-01，并建议全仓文档锚点改用「path::符号」。
4. 免报区（`run/** build/** artifacts/** evidence/** reports/** 工程控制/** \`问题扫描/**\`、GaiaDR3*/、BASS DR3/、AstroCS.wiki/）内的路径只作为**被引用证据的存在性**出现（如 `artifacts/prerelease_v5/tables/TRACEABILITY.csv` 被 CI 当默认输入、`evidence/v8_1_ci_control/…` 记录 integration_required），未在其中登记任何新 finding。
5. 本文件与 findings 中出现的 `ci/checks.json` 行号，若后续被并发提交追加，请以 `id` 字段值为定位键（`API-DOCS\|TRACEABILITY\|TRACEABILITY-CODE\|TRACEABILITY-MATRIX\|AST-API\|CON-BUILD-GRAPH\|DOC-L0\|AGENTS-GOV\|ACR-DORMANT\|SERIAL-HARDCODE\|THREAD-BUDGET\|MODULE-READMES\|UT-ABI\|UT-CLI\|CI-BINDING-TESTS\|VERSION-CONSISTENCY\|VERSION-NAMESPACES`）。

---

## 6 待复核（需要额外权限）
| # | 待复核内容 | 需要的操作/权限 | 若可执行会得出什么 |
|---|---|---|---|
| 1 | L17-005 基线新鲜度（谁更新、红灯是否仍在） | `git log -1 --format=%cI <file>` 或执行 `ci/run.py` | 可把 G-17 从 P1/置信度中 升为时间线矛盾 P0/置信度高，或反向排除 |
| 2 | mod001 在当前树是否仍 64/64 | cmake configure + build + `--prefix` 安装 + 跑脚本（写 `build/**`） | 判定 G-06 的"64/64 是否已过期"；不影响白名单无实现这一主结论 |
| 3 | 两棵 astrocs 命令面是否已实际差异 | 两棵构建 + `--help`/`version --json` diff | 若已有差异，G-01 从"结构性风险"升为"已发生偏差" |
| 4 | packaging schema 是否被任何工具加载 | 允许执行 python 校验脚本 | 确认"两份 schema 无消费者"是否仍成立（当前为静态 grep 结论） |

---

## 7 统计
- 账目（46 条全覆盖，逐项可追）：
  - 移交 1 项（L17-004 → M5a）⇒ 本域处理 **45 项**；
  - 其中 **仍成立 44 项**、**部分修复 1 项**（L12-018 → G-18）、**已被修复 0 项**；
  - 并档：L12-006 + L12-007 + L17-002 → 一条 **G-04**（−2）；L17-018 → 并入 **G-02**（−1）⇒ 44 项 → 41 条定稿；
  - 加 G-18（部分修复的残留事实）= 42 条；再加本域自算新增 1 条（**I-07**：焦点 4 复算中发现的 DOC-004 同 ID 三对象与 `API_STANDARD.md:13` 字段清单 vs `required` 4 项的口径差）= **43 条**；
  - **无法判定 0 条整体**，但 3 个子问题（基线新鲜度 / mod001 实测 / 两棵命令面差异）入 §6 待复核。
- 定稿合计 **43 条**：P0 = 8、P1 = 28、P2 = 7。
