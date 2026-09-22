# CI 检查项目录（CI Checks Registry）

> 上游：ASTROCS_DESIGN.md §12.4（验证层级与四层验收）

## 1. 注册表原则

- `eng/ci/checks.json` 是唯一检查注册表；`eng/ci/` 提供确定性执行器；
- 每项检查必须可"能绿能红"（有正例与负例）；
- 豁免显式登记 `eng/ci/exemptions.json`，只减不增，需负责人批准；
- **可执行负例面**（`ENGINEERING_SPEC.md §8`）：每项检查必须提供**机器可执行**的负例入口
  （`--self-test` 或 `--fault-inject`）；仅有人工说明不算。检查器的注册步骤中必须能看到该入口；
- **fail-closed**：输入缺失 / 路径不存在 / 依赖不可用时必须**判红**，不得崩溃后静默通过，
  也不得把「文件不存在」当「无违规」（`scanned == 0 ⇒ rc != 0`）；
- **锚存活**：判据里硬编码引用的仓库路径必须存在；失效时以 `ANCHOR_STALE: <常量名> <路径>`
  **显式失败并点名**，不得 traceback、不得静默降级；
- **注册表双向一致**：`eng/ci/checks.json` 与本文件 §2 必须双向对齐（既不得「注册未登记」，
  也不得「文档承诺 P0 但无实现」），由 `CHK-REGISTRY-DOC-SYNC` 机器保证；

## 2. 检查项清单

| ID | 类别 | 名称 | 命令/入口 | 门禁 ||---|---|---|---|---|
| CHK-BUILD-LINUX | 构建 | Linux Release 构建 | `cmake -S . -B build && ninja -C build` | P0 |
| CHK-BUILD-WIN | 构建 | Windows Release 构建 | VS 工具链 + CMake preset | P0 |
| CHK-WARN | 静态 | 编译警告 | MSVC /W4 /WX · GCC -Wall -Werror | P0 |
| CHK-STATIC | 静态 | 静态分析 | 选定分析器（clang-tidy 等） | P1 |
| CHK-MODULE-MANIFEST | 文档一致性 | 模块 manifest/注册表/构建 target/产品清单一致 | ci 检查器 | P0 |
| CHK-CONTRACT-REF | 文档一致性 | 端口引用有效 DATA 合同 | ci 检查器 | P0 |
| CHK-SCI-REF | 文档一致性 | 算法引用有效 SCI/ALG；含 ACR/编排层退出面（`ACR-DORMANT` = `eng/tools/check_legacy_exit.py`，LEG-002..004）| ci 检查器 | P0 |
| CHK-CONTRACT-TEST | 文档一致性 | 核心合同有独立测试 | ci 检查器 | P0 |
| CHK-DANGLING | 文档一致性 | 删除/重命名无悬空引用 | ci 检查器 | P1 |
| DOC-INDEX | 文档一致性 | 双向层级索引闭合（索引条目路径存在 / 最高设计与根文档 docs/ 指针可达 / 下级文档登记与「上游」抬头 100% / 文档与代码注释 docs/ 路径；悬空即缺陷、fail-closed；跨域未修项台账 `eng/tools/doccheck/dangling_ledger.json` 只减不增） | `python3 eng/tools/doccheck/check_doc_index.py --strict` | P0 |
| DOC-INDEX-SELFTEST | 文档一致性 | 上项的可执行正/负例面（19 例：悬空条目 / 悬空根文档指针 / 缺抬头 / 漏登记 / 代码注释悬空 / 非 ASCII 旧控制包残留 / 台账缺失各自判红） | `python3 eng/tools/doccheck/check_doc_index.py --self-test` | P0 |
| CHK-DOC-HYGIENE | 文档一致性 | 正式文档过程痕迹门 + 已知限制台账 ID 可解析门：D1 不得出现负责人裁决逐字引述 / 「订正·修订 + 日期」流水 / 工作项编号（豁免面 = 研究包与归档件；历史遗留逐条登记 `PREEXISTING`，只减不增）；D2 `docs/KNOWN_LIMITATIONS.md` 条目号与 `artifacts/evidence/known-limitations-ledger/LEDGER.md` §1 编号表双向一致、仓库内「条目 N / §E M-x / 原发现编号 X」引用全部可解析；fail-closed | `python3 eng/tools/doccheck/check_doc_hygiene.py` | P0 |
| CHK-DOC-HYGIENE-SELFTEST | 文档一致性 | 上项的可执行正/负例面（1 正例 + 10 负例：逐字裁决引述 / 订正流水 / 工作项编号 / SCI-5xx 编号 / 条目引用悬空 / 台账编号不一致 / 台账缺失 / 条目号提取为空 / 发现编号未解析 / 引用扫描面为空各自判红） | `python3 eng/tools/doccheck/check_doc_hygiene.py --self-test` | P0 |
| CHK-STALE-DOC | 文档一致性 | 活动文档无陈旧版本号/历史状态冒充 | ci 检查器 | P1 |
| API-DOCS | 文档一致性 | doc↔code 命令树/签名/退出码/schema 一致（命令树：CLI 产物候选缺失即 fail-closed） | `eng/tools/check_api_docs.py` | P0 |
| CHK-ROOT-CLEAN | 目录规范 | 仓库根目录整洁（§7 白名单 / 运行产物落根 / 必需条目缺失） | `eng/tools/quality/check_root_cleanliness.py` | P0 |
| CHK-UNIT | 单元 | 每模块单测 | `ctest --test-dir build`（模块级） | P0 |
| CHK-ORACLE | 模块数值 | SCI/ALG Oracle 测试 | 模块测试 target | P0 |
| CHK-INVARIANT | 模块数值 | 科学不变量/性质测试 | 模块测试 target | P0 |
| CHK-ABI | 合同/ABI | C ABI 兼容性 | ABI 检查器 | P0 |
| CHK-SCHEMA | 合同/ABI | schema 校验 | schema validator | P0 |
| CHK-SYNTH-P1 | 科学 | normalize 合成全链 | 合成全链测试 | P0 |
| CHK-SYNTH-P2 | 科学 | mosaic 合成全链 | 合成全链测试 | P0 |
| CHK-SYNTH-P3 | 科学 | export 合成全链 | 合成全链测试 | P0 |
| CHK-ISA-EQ | 科学 | baseline/AVX2/AVX-512 等价 | ISA 等价测试 | P1 |
| CHK-NWORKER | 科学 | 1 vs N worker 数值等价（判据 = 事前冻结的浮点容差，非逐位一致；口径见 `docs/contracts/SCHEDULER_CONTRACT.md` §2.1） | `eng/tools/v6/v6_determinism_driver.py`（逐字节相同优先；不同时按 TEST_MATRIX §2 容差逐文件数值比对，超差判红） | P0 |
| CHK-NWORKER-TOLERANCE | 科学 | 1/N worker 数值等价的**判据非退化面**：分层容差比较器自检（float64 用 `rtol=1e-12`；float32 产品用 `rtol=5e-6`；整型/掩码/索引/计数/端口逐位；NaN/Inf **位置**必须精确一致；易变卡 `DATE/CHECKSUM/DATASUM/CHECKVER` 除外）；**负例**：扰动一个浮点载荷必须被检出 | `python3 eng/tools/v6/v6_numeric_equiv.py --self-test --json-out run/ci/nworker-tolerance/selftest.json` | P0 |
| CHK-PHOTOMETRY-APPLY-SELFTEST | 科学 | 测光归一化**施加到像素**的像素级复算判据（裁决 B）：判据必须**尺度相关**（`max(rtol·|ref|, 2·float32 ULP)`），绝对窗口在真实 `k≈1e-17` 量级会把「乘两次」判绿；判别力证明 = `k·I` 绿、漏乘 / `k²·I` / 半帧施加全红 | `python3 eng/tools/quality/check_photometry_apply.py --self-test`（**自给自足**：未给 `--calibrated` 时自行合成夹具——仓库约定 FITS 不入库，门不得依赖仓内 FITS 文件） | P0 |
| CHK-RUN-MANIFEST-SCHEMA | 合同 | 真实产出 run manifest 对**两份冻结合同**的符合性：T1 = `docs/api/MANIFEST_VERIFY_V1.md`（CLI-003）§2/§2.1 严格符合（必填、类型、`provenance.source_sha` 40-hex、未登记顶层键判红）；T2 = `eng/contracts/schemas/run_manifest.schema.json`（CFG-001）**偏差棘轮**（与 `eng/ci/ledgers/run_manifest_schema_deviations.json` 逐项比对，新增或陈旧偏差判红）；**负例**：缺必填 / 未登记键 / 类型错 / 新增 CFG-001 偏差逐一判红 | `python3 eng/ci/check_run_manifest_schema.py --json-out run/ci/run-manifest/schema.json` | P0 |
| CHK-RUN-MANIFEST-SCHEMA-SELFTEST | 合同 | 上项的**可执行负例面**（11 例正/负例：CLI-003 必填/类型/未登记键/kind/provenance 缺失与坏 sha、CFG-001 偏差子集与「能检出」两类） | `python3 eng/ci/check_run_manifest_schema.py --self-test --json-out run/ci/run-manifest/selftest.json` | P0 |
| CHK-E2E-REPRO | 科学 | 天测闭环独立 Oracle 工具自测（closure_metric：同输入两跑必绿 + 负例注入必红；不导入生产代码，WCS 仅用 astropy 重建） | `python3 eng/tools/astrometry/closure_metric.py selftest` | P0 |
| CHK-SANITIZER | 资源 | ASan/UBSan | sanitizer 构建测试 | P1 |
| CHK-COVERAGE | 资源 | 覆盖率报告 | 覆盖率工具 | P2（报告） |
| CHK-RESOURCE | 资源 | 内存/线程/利用率门禁 | 资源监控测试 | P0 |
| CHK-PACKAGE | 打包 | 发布候选打包/白名单/哈希/版本/provenance | 打包脚本 | P0 |
| CHK-PKG-CONSISTENCY | 打包 | 产品清单/安装树合同/依赖锁/安装规则/许可登记面一致 + SBOM 实树 hash 自证（含 -NEG 负例面） | `python3 eng/ci/run_checks.py --check CHK-PKG-CONSISTENCY --quiet` | P0 |
| CHK-SECRET-HYGIENE | 安全 | 凭据/密钥卫生（tracked 全域扫描） | `python3 eng/tools/quality/check_secret_hygiene.py --scope tracked …` | P0 |
| CHK-ENV-ADOPTION | 环境 | CI 环境接管基线（工具链策略/锁一致性） | `python3 eng/ci/run_checks.py --check CHK-ENV-ADOPTION --quiet` | P1 |
| AGENTS-GOV | 治理 | AGENTS.md 硬禁令 / §0 权威链唯一性 / 旧权威回归 | `python3 eng/tools/check_agents_gov.py` | P0 |
| ENG-CONSTRAINTS | 治理 | 工程约束（§7 目录规范 / 根条目白名单 / 旧权威回归） | `python3 eng/tools/doccheck/check_engineering_constraints.py` | P0 |
| CHK-RETIRED-CODE | 治理 | 历史实现处置（ENGINEERING_SPEC §2：R1 注释旧逻辑 / R2 保留件注释块 / R3 注释块字段完整性 / R4 生产可达性报表 / R5 锚存活；台账 `eng/ci/retired_code_allowlist.json`） | `python3 eng/ci/check_retired_code.py --json-out run/ci/retired-code/retired_code.json` | P0 |
| CHK-RETIRED-CODE-SELFTEST | 治理 | 上项的可执行负例面（9 例 fault-inject） | `python3 eng/ci/check_retired_code.py --self-test` | P0 |
| VERSION-CONSISTENCY | 文档一致性 | 版本注入链单一真源 + 现行活动文档集完整性 | `python3 eng/ci/check_version.py --expected 0.11.0-alpha.2` | P1 |
| VERSION-NAMESPACES | 文档一致性 | 版本命名空间一致性（陈旧版本号） | `python3 eng/tools/doccheck/check_version_namespaces.py` | P1 |
| DOC-L0 | 文档一致性 | L0 现行文档集（docs/owner/**）与索引 active 登记完整性 | `python3 eng/tools/check_l0_docs.py` | P1 |
| GLOSSARY-DOCS | 文档一致性 | 词典锚点/别名唯一性（报告项） | `python3 eng/tools/check_glossary.py` | P2 |
| LINUX-MAIN-FIXTURES | 构建 | linux-main 夹具准备（wf_step） | `python3 eng/ci/wf_step.py --step LINUX-PREPARE-FIXTURES` | P0 |
| LINUX-MAIN-BUILD-TREE | 构建 | linux-main 根构建图（wf_step） | `python3 eng/ci/wf_step.py --step LINUX-BUILD-ROOT-GRAPH` | P0 |
| WIN-CANDIDATE-VALIDATE | 打包 | Windows 候选校验（wf_step） | `python3 eng/ci/wf_step.py --step WINDOWS-VALIDATE-CANDIDATE` | P0 |
| STD-REG | 标准 | 标准注册表 C1–C8 判据 + 9 场景 fault-inject 负例面 | `python3 eng/ci/run_checks.py --check STD-REG --quiet` | P1 |
| CHK-REGISTRY-DOC-SYNC | 治理 | 注册表 ↔ 本文件 §2 双向一致（§8） | `python3 eng/ci/run_checks.py --check CHK-REGISTRY-DOC-SYNC --quiet` | P0 |
| CHK-EXIT-CONSISTENCY | 治理 | 检查器结论与退出码一致（静态扫描：打印 FAIL 必须存在非零退出路径，防 fail-open；含 --self-test） | `python3 eng/tools/quality/check_exit_conclusion_consistency.py --json-out run/ci/exit-consistency.json` | P0 |
| CHK-LOG-SYS | 治理 | 日志与错误系统判据（最高设计 §7.3）：R1 生产收敛面吞错点登记 / R2 条件回退点显式降级（`degraded_reason`）/ R3 日志落点派生自 `output_dir` / R4 台账锚存活与只减不增 / R5 台账落点默认值与设计+合同文档一致；台账 `eng/ci/ledgers/log_system_ledger.json`（只减不增）；含 --self-test（6 类故障注入必红） | `python3 eng/tools/quality/check_log_system.py --json-out run/ci/log-system/log_system.json` | P0 |
| RESOURCE-GATE-REAL | 资源 | 真实重计算面利用率门（显式 --gate-required + 判定证据） | `python3 eng/ci/resource_monitor.py --timeout 300 …` | P0 |
| RESOURCE-GATE-REAL-NEG | 资源 | 上项的可执行负例面（串行注入 ⇒ 门必须判红） | `python3 eng/tools/quality/check_resource_gate_real.py --fault-inject serial --seconds 20` | P0 |
| CHK-KNOWN-FAILURES-BASELINE | 测试 | 版本化已知失败基线门（聚合型，linux-main 末位） | `python3 eng/ci/run_checks.py --check CHK-KNOWN-FAILURES-BASELINE --quiet` | P1 |
| CHK-IMPACT-MAP | 治理 | `eng/ci/impact_map.json` 判据一致性（id 两层闭包 / fast 候选 / BASE 核心 / 路径域锚存活与覆盖 / 无退役引用 / 结构完整） | `python3 eng/ci/run_checks.py --check CHK-IMPACT-MAP --quiet` | P0 |
| CHK-ALGO-WIRING | 治理 | 算法/关键 API 生产调用图可达性（DORMANT 台账） | `python3 eng/ci/check_algo_wiring.py --json-out run/ci/fix-gates/algo_wiring.json` | P0 |
| CHK-REGISTRY-IR-PARITY | 治理 | 生产注册表 ↔ Pipeline IR 双向一致 | `python3 eng/ci/check_registry_ir_parity.py` | P0 |
| CHK-CONFIG-CONSUMED | 治理 | 生产配置键消费（死键 no-op） | `python3 eng/ci/check_config_consumed.py` | P0 |
| CHK-CONFIG-DEFAULTS | 治理 | 生产默认值一致性 | `python3 eng/ci/check_config_defaults.py` | P0 |
| CHK-PROD-SCALE | 科学 | 关键算法生产尺度参数 | `python3 eng/ci/check_prod_scale.py` | P0 |
| CHK-PROVENANCE-CONSISTENCY | 科学 | 产品 provenance 自洽 | `python3 eng/ci/check_provenance_consistency.py` | P0 |
| CHK-REALDATA-E2E | 科学 | 真实数据 E2E（slow/heavy，linux-deep） | `python3 eng/ci/check_realdata_e2e.py --execute` | P0 |
| CHK-SPEC-NAMED-IMPL-ON-PROD-PATH | 治理 | 规范点名的权威实现必须在生产可达路径（CMake 根构建图闭包 + 生产节点调用点；登记表 `eng/ci/spec_named_impls.json`，缺口台账 `eng/ci/ledgers/spec_named_impl_gaps.json`；含 --self-test） | `python3 eng/ci/check_spec_named_impl.py --json-out run/ci/fix-gates/spec_named_impl.json` | P0 |
| CHK-NO-WEIGHT-MODE | 文档一致性 | §9.73 A44：不存在「权重模式」（**已按 §9.73 A44 作废**）：`docs/**` + `README.md` 零未留痕残留（R1 键族同行留痕 / R2 中文概念 / R3 fail-closed；含 --self-test 负例面） | `python3 eng/ci/check_no_weight_mode.py --json-out run/ci/no-weight-mode/no_weight_mode.json` | P0 |
| CHK-NO-WEIGHT-MODE-SELFTEST | 文档一致性 | 上项（A44 门；该概念**已按 §9.73 A44 作废**，不存在）的可执行负例面：临时目录正例 + 3 类负例 + 缺 docs 的 fail-closed | `python3 eng/ci/check_no_weight_mode.py --self-test` | P0 |
| AHPX-WEIGHT-RETIRED | 合同/ABI | HiPS 格式内部权重枚举作废（FIX-202；N1/N2/P1/N3 共 36 断言；负例面 `ASTROCS_AHPX_FAULT=accept_legacy\|writer_accept_legacy_meta\|writer_drop_snr` 各期望 rc=1） | `python3 eng/tools/quality/deep_ci_driver.py ctest-target --build-dir build --target ahpx_hips_format` | P0 |
| CHK-DRZ-DISP009 | 模块数值 | DISP-DRZ-009 面亮度保持权重回归门：`w_jp=a_jp/A_pixel,j`；判据 = 常量面亮度 `|S_p/B0−1|<1e-3`（pixfrac∈{1.0,0.8,0.6,0.5}）+ "分母取 A_drop 必判红"负例控制 + `pixfrac=1` 端点退化；`pixfrac=1` 产物逐字节不变 | `python3 eng/tools/quality/deep_ci_driver.py ctest-target --build-dir build --target p1drz_disp009` | P0 |
| CHK-FIX203-PROMOTED-KEYS | 治理 | 提升键落地三方一致（CLI 键表 ↔ 生产消费 ↔ 死键台账；内建 test_04 负例面，纯源码级不需构建） | `python3 -B -m unittest discover -s eng/tests/cli -t eng/tests/cli -p "test_fix203_*.py"` | P1 |
| CHK-P3-PROJ-DECL | 合同/ABI | 投影注册表声明集 == 实际可运行集（FIX-205；未实现投影显式报「不支持」，不得静默回落 TAN） | `ctest --test-dir build -R "^p3_projection_registry$\|^p3_projection_unsupported_cli$" --output-on-failure` | P0 |
| CHK-P3-PROJ-DECL-SELFTEST | 合同/ABI | 上项的注册表自检面（`p3_projection_registry_selftest`，可执行负例面） | `ctest --test-dir build -R "^p3_projection_registry_selftest$" --output-on-failure` | P0 |
| CHK-AIO-IO-BOUNDARY | 静态 | aio 是文件级唯一 I/O 边界（HARD H1/H2 + A44 代码面 + 台账棘轮；除 aio 外越界 I/O 命中 = 0；台账 `eng/ci/ledgers/aio_io_boundary_inventory.json`） | `python3 eng/ci/check_aio_io_boundary.py` | P0 |
| AIO-IO-BOUNDARY-SELFTEST | 静态 | 上项的可执行负例面（8 条负例 + 台账缺失 fail-closed + HARD 层台账不可豁免） | `python3 eng/ci/check_aio_io_boundary.py --self-test` | P0 |
| CHK-PSFSW-RETIRED-STATIC | 合同/ABI | PSFSW 退役对象 canonical 面静态清零（`eng/contracts/schemas/unified/` 除「已退役/retired」留痕外零残留；Python 落地满足注册表校验器 R4，含锚缺失 fail-closed 与 `--self-test` 正负例面） | `python3 eng/ci/check_psfsw_retired.py` | P1 |
| CHK-PSFSW-RETIRED-NEGATIVE | 合同/ABI | 退役对象行为门（无 canonical 正本 / 旧声明显式拒绝 / 迁移提示 / 不得回流） | `python3 -B -m unittest discover -s eng/tests/contracts -t eng/tests/contracts -p test_unified_object_contract.py -k RetiredObjectContract` | P1 |
| CHK-FIX208-EVENT-STREAM-DEFAULT | 治理 | 事件流 = 默认输出（无需 `-y`/`--events` 旗标即输出，唯一 schema） | `python3 -B -m unittest discover -s eng/tests/cli -t eng/tests/cli -p test_fix208_event_stream_default.py` | P0 |
| CHK-FIX208-DISK-GATE | 治理 | 资源门只管磁盘（跑前 warn / 写盘失败 error=rc10；内存/CPU 不设门；缺 `unshare -Ur -m` 时用例显式 skip） | `python3 -B -m unittest discover -s eng/tests/cli -t eng/tests/cli -p test_fix208_disk_gate.py` | P0 |
| CHK-FIX406-SIGTERM | 治理 | CLI 取消矩阵（POSIX SIGTERM/SIGINT × 三阶段 × 取消点 + Windows 控制台事件；`CTRL_CLOSE_EVENT` 平台限制显式登记于 `run/FIX-406/WINDOWS_CANCEL_PLATFORM_LIMITS.md` §3，非退化判据 + 结构化登记门为负例面；Linux 节点上 Windows 用例显式 skip ≠ 通过） | `python3 -B -m unittest discover -s eng/tests/cli -t eng/tests/cli -p test_fix406_sigterm_cancel.py` | P0 |
| CHK-REGISTRY-VALIDATE | 治理 | 注册表结构校验器（R1–R15）成为真正的门：`eng/ci/checks.json` 唯一注册表结构一致（命令可执行体/路径锚/ID 唯一/迁移映射覆盖/无孤儿 unit/不得硬编码线程） | `python3 eng/ci/validate_registry.py --registry eng/ci/checks.json --strict` | P0 |
| L2-FROZEN-GATE-SELFTEST | 资源 | L2 性能门四条冻结判据裁决器（平均利用率 / p50 / 达标样本占比 / 无低利用窗）红绿双向自测：合规证据必须绿；缺失证据 / 坏证据 / 空文件 / 门不适用 / 分母未声明 / 阈值合同缺失必须红 | `python3 eng/ci/check_frozen_gate.py --self-test` | P0 |
| L2-FROZEN-GATE-REPLAY | 资源 | 上项对 RELEASE-04 归档 L2 违规证据的回放（历史 `frozen_gate.verdict=pass` 的证据现在必须判红——恒真门改真判红，D-10） | `python3 eng/ci/check_frozen_gate.py --replay --json-out run/ci/l2-frozen-gate/replay.json` | P0 |
| WORKER-BALANCE-METRIC-SELFTEST | 资源 | worker_balance 利用率指标判别力自测：按「忙碌 worker 数 / 已分配 worker 数」正确算法复算，两组合成负载必须给出不同且非常数的输出；退化派生件 / 算法不符 / 缺失 / 空 / 坏表头 / 分母未声明 / 恒定序列必须红 | `python3 eng/ci/check_worker_balance.py --self-test` | P0 |
| WORKER-BALANCE-METRIC-REPLAY | 资源 | 上项对归档证据的回放：11 份退化 `worker_balance.csv`（同源 16/16 恒 50.00）必须判红；权威 `resource_timeseries.csv` 按正确算法复算必须非常数 | `python3 eng/ci/check_worker_balance.py --replay-archived --json-out run/ci/worker-balance/replay.json` | P0 |
| CHK-GATE-FAILCLOSED-SELFTEST | 治理 | 门禁 fail-closed 契约红绿自测（8 例）：requires_monitor 缺监控证据判红 / 证据违反 L2 冻结判据判红 / 合规证据绿 / 纯采样留证绿 / 请求判定无 frozen_gate 判红 / mutates_workspace 写出登记面判红 / 写登记 outputs 绿 / 登记输出缺失判红 | `python3 -B -m unittest discover -s eng/ci/tests -t eng/ci/tests -p test_gate_failclosed_selftest.py` | P0 |
| CHK-FAILCLOSED-SURVEY | 治理 | 全门禁 fail-closed 普查：对每个执行单元（230 个 / 78 个注册项）注入「缺失证据 / 坏证据 / 无输出」三面，适用面必须全部判红（判绿即假绿风险；表落 artifacts/evidence/release-05/FAILCLOSED_SURVEY.md）；含普查自身的红绿自证（恒绿注入必被抓） | `python3 eng/ci/run_checks.py --check CHK-FAILCLOSED-SURVEY --quiet` | P0 |
| CHK-ARCH501-BLOCK-FRAME | 架构 | 命名块与块生命周期回归锁：创建-消费-销毁状态机、消费者引用计数即时归还、DAG 四条非法图判据（消费不存在/重复生产/生命周期不一致/名字非法）、provenance 流转、显式降级、取消路径无泄漏；负例注入实测能红 | `python3 eng/ci/run_checks.py --check CHK-ARCH501-BLOCK-FRAME --quiet` | P0 |
| CHK-SCI502-SKY-KAPPA | 科学 | 天光面 κ 专项：κ 可观测、门控 κ 取**求解矩阵**（λ=0 判红 / 生产 λ 判绿）、正则化方向有效、κ_data 与 λ 无关、拒绝原因点名 kappa 而非 rank | `python3 eng/ci/run_checks.py --check CHK-SCI502-SKY-KAPPA --quiet` | P0 |
| CHK-TEST-DISCRIMINATIVE | 质量 | 空断言静态门：Python AST 扫 test_* 与 C++ 剥注释扫恒真形态（CHECK/ASSERT/EXPECT/REQUIRE/VERIFY/TEST_CHECK 的 (true)/(1)/static_assert/assert），findings 必须为 0；含 --self-test 2 正例必绿 + 6 负例逐条必红 | `python3 eng/ci/run_checks.py --check CHK-TEST-DISCRIMINATIVE --quiet` | P0 |
| CHK-ARCH502-NORMALIZE-WF | 架构 | normalize 异步工作流调度器回归锁：帧内 DAG 流水、多帧并发、N=1/2/4/8 checksum 逐位一致、归约按 frame_id 升序、专用预取线程、块生命周期由调度器掌管、内存超限必失败、取消无泄漏、磁盘满传播 | `python3 eng/ci/run_checks.py --check CHK-ARCH502-NORMALIZE-WF --quiet` | P0 |
| CHK-ARCH503-MOSAIC-WIN | 架构 | mosaic 天球窗口并行调度器回归锁：窗口划分（大小入 manifest）、按窗口路由消除整帧读放大、窗口内固定顺序、稠密 SNR 现场求值、N=1/2/4/8/16 逐位一致、与单窗口参考实现**逐位相同**、峰值驻留与总图大小解耦、取消 | `python3 eng/ci/run_checks.py --check CHK-ARCH503-MOSAIC-WIN --quiet` | P0 |
| CHK-MEM-WIRE-01 | 架构 | 内存静态预算与回压**生产路径**判据：Runtime 把「实测可用内存 × 可配置比例（默认 95，唯一数值源 eng/packaging/config/runtime_resources.json）」交给 Scheduler、节点 estimated_memory_bytes 来自 plan_estimator 真实估算（非硬写 0）；受控回压实测（8 节点/上限 2500 B ⇒ 并发 ≤2，对照上限 0 ⇒ 并发 8）、越界只排队不拒绝、节流前后每节点输出逐位一致、1/2/4/8/16 worker 逐位一致；**负例**「预算传 0」「估算硬写 0」必须判红 | `python3 eng/ci/run_checks.py --check CHK-MEM-WIRE-01 --quiet` | P0 |
| CHK-ARCH504-EXPORT-STREAM | 架构 | export 子块流式调度器回归锁：三级有界流水线+背压、全图行主序 FITS 逐位一致、与 worker 数无关、原子发布无 .tmp 残留、磁盘满 exit 10 不发布、WCS 头缺失拒绝开写、在途字节与子块大小成正比且与总图大小无关、探针、取消 | `python3 eng/ci/run_checks.py --check CHK-ARCH504-EXPORT-STREAM --quiet` | P0 |
| CHK-ARCH505-BLOCK-FLOW | 架构 | 阶段块流执行器回归锁：三阶段规格解析（8/7/5 节点）、块图校验、SHORT 块最后一次消费即销毁、STAGE 块活到单元结束、产品块收集、单元结束无 SHORT 残留、未声明写（名字级）/缺块/节点失败/非法块图四类 fail-closed、阶段隔离、与直接顺序计算**逐位一致**、重复运行 checksum 相同 | `python3 eng/ci/run_checks.py --check CHK-ARCH505-BLOCK-FLOW --quiet` | P0 |
| CHK-BLOCKFLOW-SPEC | 合同 | 块流规格机器门：节点集/operation/entry/端口与注册表逐字一致、阶段内 DAG 拓扑序、生产者唯一、生命周期自洽、双向一致、阶段隔离、派生防手改漂移（12/12 自测含 10 条负例） | `python3 eng/ci/run_checks.py --check CHK-BLOCKFLOW-SPEC --quiet` | P0 |
| CHK-BLOCKFLOW-CONFORMANCE | 合同 | 块流一致性登记册机器门：登记册结构、每条证据的 文件:行+token 复核（代码改了即判红）、符号级判据抗行号漂移、禁注释行凑证据、blocker 必须上呈 OPEN_QUESTIONS（18/18 自测含 16 条负例） | `python3 eng/ci/run_checks.py --check CHK-BLOCKFLOW-CONFORMANCE --quiet` | P0 |
| CHK-BLOCKFLOW-PORTS-VS-CODE | 合同 | 块流端口↔代码**双向**一致门：C1 结构（v2 载体合同）/ C2 声明⇒实现（每条端口锚点必须在声明符号的函数体内命中 token）/ C3 实现⇒声明（代码侧闭合文法抽出的产物 token 必须全部已声明）/ C3b 载体一致（触碰 HiPS 产品树必须有 carrier 端口）/ C4 方向一致（声明方向必须被变量流角色证据确认，推不出即判红）/ C5 载体合同（节点间只走 output_dir 文件约定 + HiPS 产品树，禁跨阶段同名边）/ C6 非退化（模块/端口/代码 token/边数下界，空注册表判红）（14/14 自测含 13 条负例） | `python3 eng/ci/run_checks.py --check CHK-BLOCKFLOW-PORTS-VS-CODE --quiet` | P0 |
| CHK-PREFLIGHT-MATRIX | CLI | 预检语义矩阵：correct/warn/error 三档 × stdin yes / -y / 不确认 / -force 四路，共 11 例。判据：放行无 not confirmed/blocked by；阻断含 blocked by 且不含 not confirmed 且 output_dir 零产物；-force 跳过整个预检 | `python3 eng/ci/run_checks.py --check CHK-PREFLIGHT-MATRIX --quiet` | P0 |
| CHK-E2E-CHAIN | 集成 | 三命令真实数据全链：normalize→mosaic→export 串行；manifest 链**独立复算**（Python 复现 p3n_input_manifest_hash 公式比对产品自报值）+ 阶段内自洽；产品判据 coverage_ok/reopen_ok/canonical_match/covered_px>0 | `python3 eng/ci/run_checks.py --check CHK-E2E-CHAIN --quiet` | P0 |
| CHK-E2E-CHAIN-SELFTEST | 集成 | E2E 链判据自测：退化产品（covered_px==0、FITS 缺失）必须判红，防「空产品也判绿」 | `python3 eng/ci/run_checks.py --check CHK-E2E-CHAIN-SELFTEST --quiet` | P0 |
| CHK-SCHED-PROBE-SCHEMA | 合同 | 探针事件 schema 机器校验：逐行校验 JSONL 的必填字段/枚举/单位与事件名一致性，空文件与无输出判红（fail-closed）；含 --self-test 1 正 5 负 | `python3 eng/ci/run_checks.py --check CHK-SCHED-PROBE-SCHEMA --quiet` | P0 |
| CHK-HIPS-STORAGE-FORM | 合同 | 落盘形态合同（CONTRACT-STORAGE-001）：两形态命名与互斥、归档必须为「整包 tar + 逐成员独立 zstd 帧」且标准工具 `zstd -dc \| tar -xf` 逐字节还原、归档内 properties 与裸形态逐字节一致且不声明非标准 `hips_tile_format`、产品级索引与数据集级覆盖索引的 schema 与不变式（coverage/tiles 集合一致、可重算）、产品身份哈希取解压后内容；**形态输入配置与输出清单**（`storage_form` 缺省/留空 ⇒ 默认 archive + warn、逐帧 `index_path`/`index_sha256`/`archive_sha256` 齐备、运行完成清单 `storage` 段不变式 M1..M4、mosaic/export 出现形态键必须 REJECT、逐层文档字段口径一致 `--doc-consistency`）；负例面 21 例（properties 撒谎 / 读路径不支持形态 / 归档缺索引 / 索引与内容不一致 / 归档截断 / 流内混装不压缩区 / byte-shuffle 预变换 / 两形态 properties 分叉 / 索引 schema 违规 / 覆盖索引非块粒度 / 两形态共存 / 缺省形态键未报 warn / 缺省未走登记默认 / 逐帧缺 index_path / bare 带归档指纹 / 清单 form_source=default 无 warn / 层间同义名 / 取值口径漂移 等）逐一判红 | `python3 eng/tools/hipsform/check_hips_storage_form.py --self-test` | P0 |

### 2.1 检查器退役与预留

- 检查器退役：从 `eng/ci/checks.json` 移除注册项，检查器文件保留可复跑性；退役检查器无参调用时打印退役标识并 exit 2；
- 工具入口退役：退役工具的 `main` 打印退役标识并 exit 2，原实现保留为 `legacy_main()`，仍被其他检查器消费的函数保持活动语义；
- 已退役检查器：`TASK-RESULT-SCHEMA`、`WORKSPACE-ADOPTION`、`RECONCILE-STATE`、`TRACEABILITY-CODE`；退役明细与复原坐标以仓库 git 历史与 `reports/` 台账为准；
- RESERVED（文档登记但无实现，重新注册前须先有实现与可执行负例）：

| RESERVED 项 | 重新注册前置条件 |
|---|---|
| `CHK-FMT`（格式门） | clang-format 纳入 `eng/ci/toolchain.policy.json` + checker 带 `--self-test` |
| `CHK-DUAL-TOL`（双平台误差） | Windows 侧可比候选产物 + checker 带可执行负例 |
| `CHK-AGENT-HARD-RULES` | 语义由 `AGENTS-GOV` 承接；重新注册须有非重复判据 |

---

## 3. 门禁分级

| 级 | 含义 | 处理 |
|---|---|---|
| P0 | 硬门禁 | 红灯阻塞合并，无 waiver |
| P1 | 硬门禁（可负责人豁免） | 红灯阻塞；豁免须显式登记 |
| P2 | 报告项 | 不阻塞，但需留存结果 |

## 4. 新增检查项流程

1. 在 `eng/ci/checks.json` 登记（ID/命令/门禁级/正例负例）；
2. 提供可"能绿能红"的正例与负例测试；
3. 本地复跑确认；
4. 合入 CI 流水线。

## 5. 运行方式（本地）

```bash
# 全量
python3 eng/ci/run_checks.py --all
# 指定项
python3 eng/ci/run_checks.py --check CHK-ROOT-CLEAN CHK-UNIT
# 输出机器可读 JSON 供 CI 消费
python3 eng/ci/run_checks.py --all --json-out ci_result.json
```
