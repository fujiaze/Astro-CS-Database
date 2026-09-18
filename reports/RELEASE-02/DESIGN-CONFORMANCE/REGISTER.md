# RELEASE-02 设计符合性审计 — 主台账（DESIGN-CONFORMANCE REGISTER）

> 审计员：设计符合性审计 SubAgent（独立验证者）。权威：`ASTROCS_DESIGN.md`（最高设计）。
> 证据标准：每条结论 = 「设计怎么说 / 实际怎么做 / 证据」。代码证据 file:line，声明证据 文档:行，优先运行时证据。
> 判定：CONFORMANT / DIVERGENT / UNVERIFIABLE。严重度：阻断 / 高 / 中 / 低。
> 生成时间：2026-09-18。工作目录：/workspace/Astro CS Database。
> **审计基线**：已提交 HEAD `1292c7e` + RELEASE-01 运行证据（`run/RELEASE-01/**`、`run/v6/**`）。
> **并发注意（2026-09-18 18:21 观测）**：前台正在工作树中实施 **P0-21 修复**
> （`lib/infrastructure/scheduler/src/module_adapters.cpp` 出现 `p1_frame_key`/`p1_frame_dir`/`p1_require_unique_frame_keys`，
> 一组进一组出、重复 frame_key fail-closed）。该修复**不在本审计基线内**；本台账对 DC-305 的判定基于 HEAD 行为与 RELEASE-01 证据，
> 待该修复完成并重跑后须复核是否闭合（含「每帧一 HiPS」基数、JSON 计数一致、多帧正/负例测试）。审计员未改该文件。

## 0. 方法与口径

- **「配置/文档声明」≠「实际执行」**：一律以运行日志、实际产物、实测命令输出为准。
- **绿灯不代表符合设计**：ctest 全绿不作为符合性证据。
- 已复核的运行时证据集：
  - `run/RELEASE-01/e2e/l4/configs/p1_*.json`（15 个 normalize 配置，声明输入帧数）
  - `run/RELEASE-01/e2e/l4/logs/p1_*.log`（15 个真实运行日志）
  - `run/RELEASE-01/e2e/evidence/l4__p1_*/{p1_final.json,p1_stack.json,manifest.json,run_context.json}`
  - `run/v6/performance/p1_real_ldn43_4k_*/{p1_snr.json,signal/properties}`
  - 对当前 `build/astrocs` 的只读实测（--help/--template/--version/doctor/坏配置预检）
- 实测命令一律以 `TMPDIR=/dev/shm/astrocs_dc`，临时配置写在 `/dev/shm/astrocs_dc/dc`，不落仓库。

---

## §3 normalize（Phase1）

| 编号 | 设计条款（文档:行，原文摘录） | 实际行为 | 证据 | 判定 | 严重度 | 违反 fail-closed | 建议处置 |
|---|---|---|---|---|---|---|---|
| DC-301 | §3.3 输入合同（ASTROCS_DESIGN.md:117-135）：`{"config":{"precision","output_dir","sparse_snr_layer"},"inputs":[{"light","bias","dark","flat","filter"}]}` | CLI 实际消费**扁平** `input_lights:[路径]` + **单一顶层** `master_bias/master_dark/master_flat`；`--template` 输出的就是这个扁平形态，无 `config` 对象、无 `filter`、无 `sparse_snr_layer` | 实测 `./build/astrocs normalize --template`（输出见 run/RELEASE-02/DESIGN-CONFORMANCE/evidence-cli.txt）；`lib/infrastructure/cli/session_commands.h:110-125`（ConfigField 表）；`lib/infrastructure/cli/runtime_client.cpp:85-90`；L4 真实配置 `run/RELEASE-01/e2e/l4/configs/p1_m42_t3_m1_red.json` | DIVERGENT | 高 | 否 | 由前台按 ENGINEERING_SPEC §3 走变更 claim：要么订正 §3.3 为扁平形态，要么实现 config/inputs 数据块结构 |
| DC-302 | §3.3:145-146「light 是一组路径，**各自对应一组校准帧**（bias/dark/flat/cosmetic）；一组 light + 校准帧 + 滤镜 = 一个数据块」 | 实现只支持**一套** master 用于该次运行的全部 light；无法为每帧指定各自校准帧/滤镜 | `p1_session.cpp:291-293`（masters 取顶层单值）、`module_adapters.cpp:318-330` 同类；配置证据同 DC-301 | DIVERGENT | 高 | 否 | 同上（文档或实现二选一） |
| DC-303 | §3.3:147「**容差和默认参数从 `config/defaults.json` 读取**，运行 JSON 只写必要参数与路径」 | `config/defaults.json`（32KB）在 `lib/`、`cli/`、`cmake/` **零 C++ 消费者**；仅 `session_commands.h:97` 一条注释声称它是"唯一家" | `grep -rn "defaults\.json\|/defaults" lib/ cli/ cmake/` 仅命中注释与一个 Python 检查脚本；`config/defaults.json` 存在但无读取点 | DIVERGENT | 高 | 是（默认值散落在代码/模板，无统一来源） | 实现 defaults.json 加载器或退役该文件并订正 §3.3 |
| DC-304 | §3.3:147「滤镜型号必须与 `config/filters.json` 滤镜库匹配（如 `bader r`），**未知滤镜 → error**」 | `filter_passband` 是自由字符串，预检**从不检查**；未知值直接写入 HiPS properties，无 error | `subcommand.h` `precheck_config`/`config_structure_errors`（只查 output_dir/输入数组/校准帧，无 filter）；`module_adapters.cpp:3536-3540` 只做换行校验；实测未知滤镜配置预检无 error | DIVERGENT | 高 | 是（未知滤镜静默通过） | 加滤镜库校验，未知 → 预检 error（fail-closed） |
| **DC-305** | **§3.4:153-157「一组输入 light → 一组 HiPS 输出；每一帧输入对应一个 HiPS 产品」；「输入 N 帧 ⇒ 必须产出 N 个 HiPS 产品」；「禁止静默丢弃…必须显式报错/判红（fail-closed）」** | **每个 normalize 配置只 drizzle `input_lights[0]` 一帧、只写 1 个 HiPS。15 个 L4 配置共声明 81 帧输入，实际 drizzle 15 次、写 15 个 HiPS（每配置恒 1），静默丢弃 66 帧，rc=0** | 逐配置实测：`p1_m42_t3_m1_red` in=6/driz=1/hips=1、`p1_m42_t2_m2_red` in=4/driz=1/hips=1、`p1_gc_panel1_red` in=11/driz=1/hips=1 …（脚本与全表见 run/RELEASE-02/DESIGN-CONFORMANCE/evidence-p021.txt）；日志判据 `[drizzle_engine] 完成: 16777216 源像素` = 恰一帧 4096²；根因代码 `module_adapters.cpp:3343`（只取 `input_lights[0]`）、`:3385`（建 1 个 PipelineFrame）、`:3541`（只 drizzle 该帧）；`p1_stack.json` `n_source_pixels=16777216` | **DIVERGENT** | **阻断** | **是** | 实现「每帧一 HiPS」；帧失败/未处理显式报错；补多帧正例+负例测试 |
| DC-306 | §3.4:157「产品数与输入帧数必须可核对（JSON 内计数与路径列表一致）」 | 产物 JSON 无"输入帧数/产品数"计数；`p1_final.json` `products:["signal","support"]` 是图层名而非逐帧产品路径列表；无法核对 N↔M | `run/RELEASE-01/e2e/evidence/l4__p1_m42_t3_m1_red/p1_final.json`（无 count 字段）；`p1_stack.json` 无输入帧计数 | DIVERGENT | 高 | 是 | 结构化 JSON 增加 input_frames/products 计数并断言相等 |
| **DC-307** | **§3.4:163-166「帧级 SNR（信噪比）**写入 HiPS 文件头**…帧级 SNR 随 HiPS 文件头输出（唯一承载）」** | **HiPS properties 无任何帧级 SNR 键**；drizzle 入口日志明确「无 snr_model 块, 使用 nullptr (snr=1.0)」；且 `p1_snr.json` 的 `frame_snr` 是 5σ 深度对象（flux5_adu/m5_mag），不是信噪比 | `run/v6/performance/p1_real_ldn43_4k_w4_r1/signal/properties`（键表无 snr）；`aio_hips_writer.cpp:1113-1194`（properties 键表无 SNR）；`run/RELEASE-01/e2e/l4/logs/p1_m42_t3_m1_red.log`「无 snr_model 块…snr=1.0」；`module_adapters.cpp:3177-3183` | **DIVERGENT** | **阻断** | 是（下游权重链断，且静默用 1.0） | 实现通量型帧级 SNR（F_ref/σ_F）并写入 HiPS 头；下游权重消费 |
| DC-308 | §3.4:164-166「可靠且独立…信号经独立背景估计与扣除、**不被加性天光背景虚高**」 | 现无帧级 SNR 标量；`frame_snr` 实装是 5σ 深度（m5_mag 常为 null），与"不被天光虚高"无从对应 | 同 DC-307；`run/v6/performance/p1_real_ldn43_4k_w4_r1/p1_snr.json` `frame_snr.definition` 自述「5-sigma point-source depth…NOT a whole-frame scalar SNR」 | DIVERGENT | 阻断 | 是 | 同上 |
| DC-309 | §3.4:172-175 可选 `sparse_snr_layer=true`：稀疏控制点层作为**标准层插入 HiPS 文件内**；实际 SNR = 帧级×帧内 | `sparse_snr_layer` 在 `lib/` **零出现**；CLI 模板/help 无该键；无帧内层写入路径 | `grep -rn "sparse_snr_layer" lib/ cli/` = 0 命中；`normalize --template` 无该键 | DIVERGENT（未实现） | 中 | 否 | 实现或显式登记 NOT_IMPLEMENTED 并从 §3.4 标注 |
| DC-310 | §3.5:204「🔴 error：**文件找不到、路径问题**、未知滤镜等。**阻塞运行**，且**强制报告原因**」 | 预检对不存在的输入/校准文件判 **`[correct]`**，不阻塞；只有运行到读文件时才失败（且报错码为 3） | 实测 `normalize --json bad1.json`（全部路径 /nonexistent）预检输出 `[correct] master_bias = /nonexistent/bias.fits`、`[correct] input_lights 条目数 = 1`；`subcommand.h` `precheck_config` 无文件存在性检查 | **DIVERGENT** | 高 | **是**（假绿/fail-open） | 预检加 `std::filesystem::exists` 判定，缺失 → error 且 -y 不可越 |
| DC-311 | §3.5:202「🟠 warn：**不合适的设置、不匹配的帧**、可优化项（如暗场时间与亮场不在容差内 → 提示走暗场优化）」（归入既有 optimize 级） | 预检**从不产生 warn/optimize 行**：`precheck_config`/`calibration_checks` 只产 correct/error；无暗场-亮场曝光容差检查（该容差应在 defaults.json，见 DC-303） | `subcommand.h:150-172`；`session_commands.h:198-207`（render_checks 支持 optimize 但无生产者）；`grep -rn "CheckLine" lib/` 仅 3 处定义/渲染/断言，无 optimize 生产者 | DIVERGENT | 中 | 是（应有告警处静默） | 实现 warn/optimize 级与容差检查 |
| DC-312 | §3.5:206-208「页面须包含：逐项检查明细与结论；**资源与磁盘占用预估**（输出规模、临时空间需求、当前可用空间）」 | 预检页只列 output_dir、输入条目数、3 个校准帧路径；无任何资源/磁盘预估 | 实测预检输出（evidence-cli.txt）；`subcommand.h` 无磁盘/空间计算 | DIVERGENT | 中 | 否 | 补资源与磁盘预估 |
| DC-313 | §3.5:212-213「**`-force` 跳过全部检查**，**直接进入运行过程**（不显示预检页面、不请求确认）…**连 error 也一并跳过**」 | `-force` **仍请求确认**（stdin EOF → abort rc=2）；**仍打印预检页**；且**结构错不可 force**（与"连 error 也跳过"矛盾） | `subcommand.h:216-221`（assume_yes 只看 -y，不看 forced；forced 仍走 confirm_run）；`subcommand.h:203-209`（结构错 → rc=2 注释明写"-force cannot override"）；实测 `normalize ... -force` 输出 "not confirmed — aborting" rc=2 | DIVERGENT | 高 | 否（偏 fail-closed，但违反 §3.5 明文） | 按 §3.5 让 -force 跳过页面/确认/全部检查，或走变更 claim 收紧 §3.5 |
| DC-314 | §3.5:182,206「**即使全部检查都通过，也必须弹出页面**」「无论结果是 correct/warn/error，都必须显示这个页面」 | 传 `-y` 时 `confirm_run` 不被调用 ⇒ **页面不显示**（页面渲染只发生在 confirm_run 内） | `subcommand.h:216`（assume_yes 短路 confirm_run，page 变量随之未输出） | DIVERGENT | 中 | 否 | 页面独立于确认输出（correct/warn/error 均打印） |
| DC-315 | §3.5:211「**存在 error 时强制阻断**，**`-y` 也不能越过**，且必须报出原因」 | 结构错确实阻断且 -y 不可越（正确）；但"文件找不到"不被判为 error（DC-310），故 `-y` 实际越过了缺文件这一 error 类 | 同 DC-310；实测 `normalize bad1.json -y` → 直接运行到读文件才 rc=3 | DIVERGENT | 高 | 是 | 与 DC-310 一并修 |
| DC-316 | §3.5:214「预检本身失败（配置 JSON 无法解析）按 error 处理」+ §6.3:411「2 = CLI 参数或配置错误」 | JSON 解析失败返回 **3**（INPUT），不是 2 | 实测 `normalize --json badjson.json -y` → rc=3，stderr「config malformed JSON」；`subcommand.h` parse_error → `astrocs::INPUT`(3) | DIVERGENT | 低 | 否 | 解析失败归 2（配置错误） |
| DC-317 | §3.3:148「`precision`（FP32/FP64）显式声明；科学模块按声明精度执行」 | CLI 用 `drizzle.precision_mode`（0/1），模板默认 **1（FP64）**；无顶层 `precision` 键 | `normalize --template`：`"drizzle":{"nested":1,"pixfrac":1.0,"precision_mode":1}`；`module_adapters.cpp:3319-3337`（precision_mode 必须显式） | DIVERGENT（键名/位置） | 低 | 否 | 文档或键名对齐 |

---

## §6 CLI 合同

| 编号 | 设计条款（文档:行，原文摘录） | 实际行为 | 证据 | 判定 | 严重度 | 违反 fail-closed | 建议处置 |
|---|---|---|---|---|---|---|---|
| DC-601 | §6.2:377-394 命令树：normalize/mosaic/export ×(--json/--template/--help) + help + --version + doctor + benchmark | 命令树与实现一致（实测 `--help` 全列出；doctor --json 可用；`command_tree.h:74-93`） | 实测 `./build/astrocs --help`；`command_tree.h` commands() | CONFORMANT | — | 否 | — |
| DC-602 | §6.3:391 + §12:610「**Alpha 之前：程序与代码中不包含任何版本信息**…此前不存在任何版本信息」 | `--version` 输出 **`0.11.0-alpha.2+g<sha>`**；`--version --json` 含 `version` 字段；运行产物 `run_context.json` 含 `software_version` | 实测 `./build/astrocs --version`、`--version --json`；`run/RELEASE-01/e2e/evidence/l4__p1_m42_t3_m1_red/run_context.json` `software_version=0.11.0-alpha.2+g41b41e2…` | **DIVERGENT** | 高 | 否 | 移除版本信息，或走变更 claim 修改 §12 |
| DC-603 | §6.3:405「机器输出：`--json` 时 stdout 恰一个 JSON 文档…**stdout 无日志污染**」 | `doctor --json` stdout 恰一个 JSON，日志走 stderr（正确）；但 `--json` 同时是"配置路径"值旗标与"机器输出"布尔旗标，语义重载（同一 token） | 实测 `doctor --json 2>/dev/null` 仅 JSON；`command_tree.h:37,57`（--json 同时在 boolean_flags 与 value_flags） | CONFORMANT（stdout 干净）/ 键义重载 | 低 | 否 | 澄清 `--json` 双义（设计 §6.2/§6.3 本身也重载） |
| DC-604 | §6.3:406「退出码（唯一源 `include/astrocs/exit_codes.h`）」 | 唯一源实际在 `lib/infrastructure/cli/exit_codes.h`；`include/astrocs/exit_codes.h` **不存在** | `find . -name exit_codes.h -not -path './build/*'` → 只有 `lib/infrastructure/cli/exit_codes.h`（另一处是 run/ 影子副本） | DIVERGENT（路径） | 低 | 否 | 文档路径订正或文件迁移 |
| DC-605 | §6.3:411-419 退出码表：5=backend ABI/签名/CPU 特征/加载失败；10=资源利用率/内存增长门禁失败 | 资源门返回 10（正确，`commands.cpp:725,937`）；但 `runtime_client.cpp:410` 把模块 `ErrorDomain::RESOURCE` 映射为 **5**，与表义（5=backend）不符 | `lib/infrastructure/cli/runtime_client.cpp:406-412`；`lib/infrastructure/cli/exit_codes.h:17`（RESOURCE=10） | DIVERGENT | 中 | 否 | 明确 RESOURCE 域语义与退出码映射 |
| DC-606 | §6.3:422 取消：协作取消 → 关 writer → 写 incomplete manifest → 删/隔离临时产物 → exit 9；**不得留下看似完整的产品** | 未实测（需触发 SIGINT）；代码有 cancel_token 与 incomplete manifest 路径，但原子性未独立验证 | `runtime_client.cpp:355-374`（cancel_watch）；`commands.cpp`（run manifest） | UNVERIFIABLE | 中 | — | 需前台跑中断注入实测 |
| DC-607 | §6.3:423「运行产物只落配置 `output_dir`，**不得以进程 CWD 作隐式缺省写出**」 | 模板默认 `output_dir:"."`；`runtime_client.cpp:111` `doc.value("output_dir",".")` 在缺键时用 CWD；虽然结构预检会拒缺 output_dir，但模板本身把 "." 当可直接运行值 | `normalize/mosaic/export --template` 均为 `"output_dir":"."`；`runtime_client.cpp:111` | DIVERGENT | 中 | 是（隐式 CWD 缺省） | 模板留空并由预检强制显式 |
| DC-608 | §6.2:399 / §10.1:523「唯一可执行 `ACSD Cli`/`acsd_cli`，安装后直接以命令名调用」 | 构建产物名 `build/astrocs`（ELF），help 前缀 `astrocs` | `ls build/astrocs`；实测 help 文本 | DIVERGENT（命名） | 低 | 否 | 见 §10（子报告 SUB-C） |
| DC-609 | §6.1:360「薄入口：职责限定为命令解析、配置预检、运行控制、机器输出、取消与退出码」 | 大体符合；但 `runtime_client.cpp` 内嵌多阶段 IR 构造（build_pipeline_ir 支持 1/2/3 同时）——虽无用户入口，仍保留串阶段代码路径 | `runtime_client.cpp:101-288` | CONFORMANT（无用户入口） | 低 | 否 | 清理死路径或登记 |
| DC-610 | §1.2:63「**禁止**把三阶段隐式串接为一次运行的入口」 | 命令树无 `run`/--phases 用户入口；`command_tree.h:74-93` 仅 8 条；旧 phase1/2/3 已删 | `command_tree.h`；实测 `--help` | CONFORMANT | — | 否 | — |

---

## §2 数据对象与配置分离

| 编号 | 设计条款（文档:行，原文摘录） | 实际行为 | 证据 | 判定 | 严重度 | 违反 fail-closed | 建议处置 |
|---|---|---|---|---|---|---|---|
| DC-201 | §2:84「**数据对象禁止互相冒充**：… frame_snr … depth_m5 … 各是各」+ ENGINEERING_SPEC.md:29「一个字段只承载一个含义」 | `p1_snr.json` 的 `frame_snr` 字段承载 **depth_m5** 内容（`definition`="5-sigma point-source depth"、`flux5_adu`、`m5_mag`、`zero_point_mag`），**没有** `frame_snr_value`；与冻结 schema `frame_snr.schema.json` 的 required 字段完全不匹配 | `run/v6/performance/p1_real_ldn43_4k_w4_r1/p1_snr.json`（`frame_snr` 对象，`has frame_snr_value: False`）；`contracts/schemas/unified/frame_snr.schema.json`（required `frame_snr_value`…）；实装 `module_adapters.cpp:3177-3183` | **DIVERGENT** | 阻断 | 是（对象冒充，且无 schema 校验红灯） | 分离 `frame_snr`（通量型 SNR 标量）与 `depth_m5` 对象；按冻结 schema 校验 |
| DC-202 | §2:85「三类配置严格分离：phase_config.json（科学，可跨机）与 cpu_profile（机器绑定，仅 benchmark 生成）分开；run_manifest 冻结本次运行」 | 基本分离；但 `config/templates/*.phase_config.json`（科学配置模板）与 CLI 实际模板是**两套不兼容形态**，且模板键零消费（见 dead-keys.md） | `config/templates/normalize.phase_config.json`（`config.sparse_snr_layer`/`config.algorithm_psf_model`）vs `normalize --template`（扁平 `input_lights`） | DIVERGENT | 高 | 否 | 见 dead-keys.md |
| DC-203 | §2:86「帧级 SNR 是**唯一帧级参考**…Phase2 用逆方差叠加把它转成权重」 | Phase2 输入依赖帧级 SNR，但帧级 SNR 未写入 HiPS（DC-307）；L4/L3 E2E 的 mosaic 配置用 `weight_mode:1`（等权）+ `legacy_allow_weight_fallback:True`，绕开 SNR→权重链 | `run/RELEASE-01/e2e/l4/gen_stage23.py:11-13`（`'weight_mode':1, 'legacy_allow_weight_fallback':True`）；`p2_*.json` | DIVERGENT | 阻断 | 是（静默降级等权） | 修 SNR 链；E2E 默认 ivar 权重 |

---

## §5 export（Phase3）— 子审计 SUB-B 结论（证据全文见 run/RELEASE-02/DESIGN-CONFORMANCE/SUB-B-export-io.md）

| 编号 | 设计条款（文档:行，原文摘录） | 实际行为 | 证据 | 判定 | 严重度 | 违反 fail-closed | 建议处置 |
|---|---|---|---|---|---|---|---|
| DC-501 (B-01) | §5.3:343「内置多种投影算法，首批冻结 **TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA**」 | 在役 registry 仅 4/8（TAN/SIN/CAR/AIT）；STG/MOL/CEA/ZEA 未实现 | `lib/algorithms/projection/p3_proj_v6.cpp:272-281` kRegistry[4]；`:348-349` kFrozenSet[8]；`p3_proj_v6.h:42-43` | DIVERGENT | 高 | 否 | 实施 4 投影或按变更流程退役 |
| DC-502 (B-02) | §5.3:345「用户通过 `projection` 字段选择，缺省 TAN」 | 生产路径**只接受 TAN**；SIN/FOO → rc=2 显式拒；CLI help「当前唯一实现 TAN」与 registry 4/8 自相矛盾 | 实测 `export --json cfg_sin.json -y` → rc=2「projection must be TAN」；`p3_wcs.cpp:61-69`；`runtime_client.cpp:60-72` | DIVERGENT | 高 | 否（显式拒） | 接线 registry 或订正设计/文案 |
| **DC-503 (B-03)** | **§5.3:346「输出模式显式：surface_brightness / point_source_flux / visualization」** | **`output_mode` 零 C++ 消费者**：传入 → rc=3「unknown key」；`--export-mode` 仅 token 路由校验不入科学路径；三模式仅在**未接生产**的 p3_v6_export.cpp | `grep -rn output_mode lib/`=**0**；实测 rc=3；`v6_mode_gate.h:26-86`；`parser.cpp:304` | **DIVERGENT** | **阻断** | **是**（模式请求被静默吞） | 接入生产 resample/writer 或对缺失显式 REJECT |
| **DC-504 (B-04)** | **§5.3:346「缺所选模式所需信息 → 拒绝或明确 unavailable」** | 无任何模式所需信息检查；恒按 surface_brightness 输出，从不 unavailable | `module_adapters.cpp:5973-6292`、`:6296-6405`（无 mode 分支）；`p3_proj_v6.cpp:625-650` 语义门无生产调用点 | **DIVERGENT** | **阻断** | **是** | 同 DC-503 |
| DC-505 (B-05) | §5.3:344 新增投影经 registry 注册并附独立往返 Oracle | registry 机制在位，已实现 4 投影有往返/Oracle | `p3_proj_v6.cpp:356-402`；`tests/p3wcs/` | CONFORMANT | — | 否 | — |
| DC-506 (B-06) | §5.2:336「**流式 FITS 输出**」 | 生产 writer 非流式：整幅读入内存再 `fits_write_pix`；流式实现只在未接生产的 p3_v6_export.cpp | `module_adapters.cpp:6310-6371`；`p3_output.cpp:234-236`；`aio/v6/src/v6_fits.cpp:407-416` 无生产消费者 | DIVERGENT | 中 | 否 | 接 FitsStreamWriter 或订正设计 |
| DC-507 (B-07) | §5.2:337「独立 WCS/FITS 验证」 | 独立重开 verify，逐项对拍并独立重算 canonical hash | `p3_output.cpp:396-540`；`module_adapters.cpp:6468-6521`；`l3__p3_m42/p3_verify.json` | CONFORMANT | — | 否 | — |
| **DC-508 (B-08/B-16)** | **§5.2:338/§9.2:508「所有产品：临时文件/目录 + 校验 + fsync + 原子 rename 提交」** | **仅 FITS 与 run manifest 有 tmp+rename（FITS 无目录 fsync）；p3_props.json/p3_wcs.json/p3_resampled.{json,bin}/p3_verify.json 全部 std::ofstream 原地直写正式路径** | `p3_output.cpp:158-161,344-362`（FITS 有）；`module_adapters.cpp:5910,5960,6235,6280,6509`（直写） | **DIVERGENT** | **阻断** | **是** | 所有落盘产物统一原子提交 |
| **DC-509 (B-17/B-24)** | **§9.2:508「失败/取消时清理临时产物，正式产品目录只出现完整产品」** | 已直写中间产物失败/取消后**残留 output_dir**，无清理/隔离 | `commands.cpp:1294-1310`（仅 incomplete manifest）；直写路径无清理 | **DIVERGENT** | **阻断** | **是** | 统一临时目录+失败清理 |
| DC-510 (B-09) | §5.4:350 + §3.5（三命令通用预检：三级、必弹页面、资源/磁盘预估、-force 跳过全部） | export 预检只有 correct/error 两级、无资源/磁盘预估、-y 不显示页面、-force 不跳过确认且不越结构错 | `subcommand.h:126-146,187-226`；实测 -force → abort rc=2 | DIVERGENT | 高 | 是 | 与 DC-310~314 一并修 |
| DC-511 (B-10) | §3.5:211「存在 error 时强制阻断，-y 不能越过」 | 结构错 -y/-force 均 rc=2 | `subcommand.h:200-216`；实测 | CONFORMANT | — | 否 | — |
| DC-512 (B-11) | §5.1:328「输出不需要带权重」 | FITS 仅 signal+COVERAGE(+可选 VARIANCE/IVAR)，无 weight 平面 | `p3_output.cpp:233-308` | CONFORMANT | — | 否 | — |
| DC-513 (B-12) | §5.1:328 + §1.2:65「export 不得假设输入来自 mosaic」 | 输入为任意合同兼容 HiPS；无 mosaic 专属依赖 | `module_adapters.cpp:5761-5763` | CONFORMANT | — | 否 | — |
| **DC-514 (B-13)** | **§6.3:404「模板命令输出**可直接运行**的完整 JSON」** | 官方 schema 模板 `config/templates/export.phase_config.json` 经 CLI 运行 **rc=2**；CLI `--template` 为 flat 格式且不含 output_mode；两套格式互斥 | 实测 rc=2；`contracts/schemas/phase_config_export.schema.json:108-160` | **DIVERGENT** | **阻断** | **是**（唯一事实源不可运行） | 统一 schema↔CLI + 可达性门 |
| DC-515 (B-14/B-15) | §9.1:507「aio 是**唯一** FITS/HiPS/manifest 读写边界；Phase1/2/3 复用同一套 AIO」 | Phase3 FITS 读写由 `lib/algorithms/fits_output/p3_output.cpp` **直接调 CFITSIO**，不经 aio；p3_*.json 由 std::ofstream 直写 | `p3_output.cpp:49,166,236,264,297,414,475,486,493,519,531` | DIVERGENT | 高 | 否 | FITS 读写收归 aio 或修订 §9.1 |
| DC-516 (B-18) | §9.3:509「每次运行生成…run-graph 渲染目录（graph/）」 | resource_timeseries/resource_summary/worker_balance/astrocs_run/run_context 均在；**phase2 成功路径无 graph/** | `commands.cpp:1075`（phase2 无 graph）vs `:1318,1676`；`find run -name graph` 仅 p3 | DIVERGENT | 中 | 否 | phase2 补 write_run_graphs |
| DC-517 (B-19) | §9.3:509「plan 是预期，trace 是实际观测，如实分别记录」 | static_graph.json（plan）与 observed_trace.json（trace）分别落盘 | `commands.cpp:311,388,407`；`p3_m42_vis/graph/*` | CONFORMANT | — | 否 | — |
| DC-518 (B-20) | §9.4:511「工件名统一下划线」 | 生产产物名全下划线；仅测试 fixture 用连字符（非产品） | 实测 find | UNVERIFIABLE | 低 | 否 | 界定 §9.4 适用范围 |
| DC-519 (B-21) | §9.5:512 manifest 至少记录…**像素/采样语义**… | run manifest **缺「像素/采样语义」**；科学配置仅 config_path+sha256 | 30+ `astrocs_run_*.json` 检查无 sampler/pixel 键；`commands.cpp:216-244` | DIVERGENT | 中 | 否 | manifest 补采样语义与科学配置摘要 |
| DC-520 (B-22) | §9.5:512「…模块 build ID…」 | `p3_verify.json` `module_build_id` 为空串 | `l3__p3_m42/p3_verify.json`；`module_adapters.cpp:6507` | DIVERGENT | 低 | 否 | 填真实 build id |
| DC-521 (B-23) | §9.6:510 五个 JSON 状态 NOT_IMPLEMENTED | 全仓不存在这 5 个文件，以 observed_trace/static_graph/graph_sidecar 替代 | `find run -name run-plan.json …`=0 | CONFORMANT | — | 否 | 插件文档仍列为输出，应订正 |

## §4 mosaic（Phase2）— 子审计 SUB-A 结论（证据全文见 run/RELEASE-02/DESIGN-CONFORMANCE/SUB-A-mosaic.md）

| 编号 | 设计条款（文档:行，原文摘录） | 实际行为 | 证据 | 判定 | 严重度 | 违反 fail-closed | 建议处置 |
|---|---|---|---|---|---|---|---|
| DC-401 (A-01) | §4.1:226「对不同科学目标提供**明确的最优统计量**，而不是一个万能 weight」 | 生产 integrate 只用 p2_integrate_pixel（单一加权均值）；实现 GLS/Q-W/psfsw 的 phase2_integrate.cpp **未被节点链调用** | `module_adapters.cpp:4629-4635,4945`；`coverage/src/integrate.cpp:19-79`；13_integration.md:63-68 | DIVERGENT | 高 | 否 | 接线 v6 集成器或退役并订正 |
| DC-402 (A-02) | §4.2:230-240 流程含 admit/verify/publish | 7 节点链；admit 折入 coverage（fail-closed），verify 折入 write（仅完整性回读） | `module_adapters.cpp:6820-6828,3883-3952,5207-5232`；`coverage.cpp:86-231` | CONFORMANT | 低 | 否 | — |
| **DC-403 (A-03)** | **§4.3:254,262「Phase2 自动检测输入 HiPS 是否有稀疏 SNR 层…检测与重建是标准行为，不是可选开关」** | **生产链完全无 sparse_snr_layer 检测/重建；`lib/` 零命中；integrate 读 Phase1 ivar 子产品，无帧级/帧内 SNR 概念** | `grep -rn "sparse_snr\|snr_layer" lib/`→NO_MATCH；`module_adapters.cpp:4693`（RD_IVAR）；GAP_AUDIT P0-12 OPEN | **DIVERGENT** | **阻断** | 否（缺失功能） | 实现稀疏层落盘+检测/重建 |
| DC-404 (A-04) | §4.3:255-256「利用每个源像素的 SNR 计算权重…`w=1/σ²=SNR²/F_ref²`」 | 生产直接取 Phase1 ivar 当权重（`w=ivar_v`），无 SNR→权重换算、无 F_ref | `module_adapters.cpp:4916-4932`；`grep F_ref lib/` 无 | DIVERGENT | 高 | 否 | 证明 ivar=SNR²/F_ref² 或实现换算 |
| **DC-405 (A-05)** | **§4.3:255「逆方差叠加…为 Phase2 标准行为」** | **L4 实测 weight_mode=1 等权；L3 weight_mode=2 因 Phase1 无 ivar 整链失败；全仓无 wm2 成功产物** | `l4/configs/p2_m42.json`（weight_mode=1, fallback=true）；`l4__p2_m42/p2_integrated.json`（weight_basis=unit_weight_mode1）；`l3/logs/p2_m42_wm2.log:11` | **DIVERGENT** | **阻断** | 否（wm2 fail-closed，但默认链不可用） | 修 Phase1 ivar 后以 wm2 重跑 L3/L4 |
| DC-406 (A-06) | §4.3:261「排异先于加权」 | 链序 reject→integrate；integrate 消费 sample_mask 剔除被拒样本 | `module_adapters.cpp:4644-4649,4899-4915,5024-5025`；`p2_integrated.json` rejected_samples_skipped=21976 | CONFORMANT | — | 否 | — |
| DC-407 (A-07) | §4.3:257-260 稠密权重按需计算，不预计算 | 逐 tile/逐像素现场累加 wsum；无稠密权重驻留 | `module_adapters.cpp:4885-4974,4955-4958,5167-5174` | CONFORMANT | — | 否 | — |
| **DC-408 (A-08)** | **§3.5:204（§4.4 强制）「🔴 error：文件找不到、路径问题…阻塞运行」** | **mosaic 预检不查输入路径存在性：不存在 hips_paths 判 `[correct]`，-y 后进入运行到 coverage 才 exit 3** | 实测 `mosaic --json bad1.json` → 仅 2 行 correct；`subcommand.h:64-104,126-146` | **DIVERGENT** | **阻断** | **是** | 预检加输入存在性 error |
| DC-409 (A-09) | §3.5:206-208 页面含资源与磁盘占用预估 | mosaic 预检页仅 2 行，无资源/磁盘预估 | 实测；`subcommand.h:126-146`；`session_commands.h:188-207` | DIVERGENT | 中 | 否 | 增加预估行 |
| DC-410 (A-10) | §3.5:200-203 三级 correct/warn/error | mosaic 预检仅 correct/error，无 warn/optimize 级 | `subcommand.h:126-146`；`session_commands.h:198-207` | DIVERGENT | 中 | 否 | 补 warn 检查项 |
| DC-411 (A-11) | §3.5:212 `-force` 跳过全部检查、不显示页面、不请求确认 | `-force` 仍打印页面并等待 yes（无输入 abort exit 2）；结构 error 不可越 | 实测；`subcommand.h:192,200-205,211-225` | DIVERGENT | 高 | 否 | 与 DC-313 一并裁决 |
| **DC-412 (A-12)** | **§4.4:270 天光亮度平面采用稀疏表示…按需现场求值，不构建稠密背景栅格** | **sky_plane 模块已实现，但只接到独立工具 astrocs-stage2；生产 mosaic 节点链零引用；生产 UPM component_count=1，无 B_ref/δ_k** | `grep sky_plane\|p2_sample_sky\|sky_samples\|star_mask module_adapters.cpp`→NO_MATCH；`l4__p2_m42/p2_upm_model.json` component_count=1；FIX-A-report:67,140 自称生产默认开启（仅对 stage2 成立） | **DIVERGENT** | **阻断** | 否（缺失功能） | 接入生产链或如实标 NOT_IMPLEMENTED |
| DC-413 (A-13) | §4.5.1:287-288 先排异后加权，两个独立步骤，rejection 独立落盘 | 逐像素候选栈→p2_reject_stack_ex，独立 p2_rejection.json+sample_mask.bin，integrate 独立消费 | `module_adapters.cpp:4496-4573,4534,4588-4621` | CONFORMANT | — | 否 | — |
| **DC-414 (A-14)** | **§4.5.2:289-291 按输入集合大小 n 自适应选择（n<6 PercentileClip / 6≤n≤15 Winsorized / n>15 LinearFit）** | **AUTO 只在整组帧数解析一次：`nominal_contributors=frames.size()`，kernel 禁 AUTO，全部像素共用 plan.method；L4 nominal=12 → 全体 winsorized，不随逐像素 n 重选** | `module_adapters.cpp:4412-4423`；`rejection.cpp:1072-1084`；`l4__p2_m42/p2_rejection.json` plan.method=2 单标量，depth=2 占 71.70% | **DIVERGENT** | **阻断** | 否 | 实现逐输出像素 n 路由 |
| DC-415 (A-15) | §4.5.3:292-294 算法清单须有出处与语义 ID | 7 类算法均实现并有 semantic_id 注册 | `rejection.cpp:1012-1026,1249-1690`；`rejection.h:196` | CONFORMANT | — | 否 | — |
| DC-416 (A-16) | §4.5.4:300 映射表须冻结在 12_rejection.md | 12_rejection.md **无** n→算法映射表；映射只在 REJECTION.md:47-53 与代码，且该文档明写「n 一次解析，禁止 per-pixel effective 路由」（与 §4.5.2 冲突） | `12_rejection.md:1-52`；`REJECTION.md:18,32,47-53,78`；`rejection.cpp:1073-1080` | DIVERGENT | 中 | 否 | 冻结映射表 + 上呈口径冲突 |
| DC-417 (A-17) | §4.5.4:298-299 WBPP 明确拒绝 NoRejection 与 MinMax | p2_reject_plan_resolve 接受 P2_REJECT_NONE 与 MINMAX；WBPP 合法性约束未实现 | `rejection.cpp:922-933,1036-1084`；`stage2_common.cpp:229,244-245` | DIVERGENT | 中 | 否 | 生产 profile 拒绝 none/minmax |
| **DC-418 (A-18)** | **§4.5.5:302 排异算法字段留空/0/auto ⇒ 按该输出像素 n 自动选择** | **无任何 JSON 字段被读为排异方法选择；AUTO 硬编码且为整组级** | `module_adapters.cpp:4414`；`grep algorithm_rejection_method lib/`=0；`parser.cpp:297-298` | **DIVERGENT** | **阻断** | 否 | 实现字段消费 + per-pixel 路由 |
| **DC-419 (A-19)** | **§4.5.5:303 显式指定单一算法 ⇒ 按用户指定执行，不被自动覆盖；不得静默改算法** | **reject.method/algorithm_rejection_method 无消费者；方法硬编码 AUTO；顶层键被 CLI 判 unknown key exit 3；reject:{method:"bogus"} 预检 `[correct]` 被忽略** | `module_adapters.cpp:4414,5556`；实测 exit 3 / `[correct]`；`parser.cpp:297-298` | **DIVERGENT** | **阻断** | **是**（显式指定被静默忽略） | 接通方法键；未知方法判 error |
| DC-420 (A-20) | §4.5.5:304-305 支持按 n 的用户自定义表达式/分段映射，语法冻结并由测试锁定 | method_map/method_expr 在 `lib/` 零命中；无 schema/CLI/文档/测试 | `grep method_map\|method_expr lib/`→无；`mosaic --template` 仅 4 键 | DIVERGENT | 高 | 否 | 实现表达式/分段映射 |
| DC-421 (A-21) | §4.5.5:306-311 不合适只告警不硬阻断；不得静默改算法/降级 | 生产路径无 WARN；未知方法名预检 `[correct]`、运行期静默忽略；无 error 级校验 | `module_adapters.cpp:4398-4627`；实测 | DIVERGENT | 高 | **是** | 增加合法性 error + 档位冲突 WARN |
| DC-422 (A-22) | §4.5.5:312 实际使用方法/参数/n 必须写入 rejection provenance | p2_rejection.json 只记单一 plan.{method,semantic_id,minimum_n,...}；无逐像素 n/方法/参数；ASTROCS_* 键 pending 未写入 properties | `l4__p2_m42/p2_rejection.json`；`p2_final.json` provenance 仅 4 键 + pending_contracts；`module_adapters.cpp:5253-5271` | DIVERGENT | 高 | 否 | provenance 补逐像素 n/方法/参数 |
| DC-423 (A-23) | §4.5.6:313 阈值与映射表由合成 Oracle 正/负例锁定，能红能绿 | 有 kernel 级测试（显式 P2_REJECT_SIGMA），**无任何测试覆盖生产 AUTO 路由/per-pixel/配置键消费** | `tests/api/test_reject_integration_oracle.py:1-30,51`；`grep algorithm_rejection_method\|method_map\|method_expr tests/`→无 | DIVERGENT | 高 | 否 | 补生产路由正/负例 |
| **DC-424 (A-24)** | **§4.5 验收:317-320 注入卫星线/宇宙线输出不得被拉高；L4 不得残留** | **L4 74.14% 像素 n=2 UNDERDETERMINED 全接受，accepted_pixels==n_pixels，排异无效，卫星线残留** | `l4__p2_m42/p2_rejection.json` underdetermined=101649311/137101312；tile depth 2:71.70%；GAP_AUDIT §8.3 | **DIVERGENT** | **阻断** | 否 | 修 P0-21 + DC-414 后复验 |
| DC-425 (A-25) | §6.1:364-366/§4.5.5 CLI 合同：--template 写出可直接改 JSON、--help 字段说明 | mosaic --template 仅 4 键（无排异键）；自带 config/templates/mosaic.phase_config.json 无法被 mosaic --json 使用 | 实测；`session_commands.h:129-139`；`config/templates/mosaic.phase_config.json` | DIVERGENT | 中 | 否 | 统一配置格式；模板暴露排异键 |
| DC-426 (A-26) | §4.1/§4.4 声明式配置「算法选择」键须有真实消费者 | algorithm_rejection_method/weight_mode/upm_gauge 三键在 `lib/` **全部零消费者** | `grep`→无；`config/config_registry.json:855-856,924-929,994-995`；`phase_config_mosaic.schema.json:130` | DIVERGENT | 高 | 否 | 接通或显式退役 |

> SUB-A 附注：`docs/science/REJECTION.md:18,32,78` 明写「n 一次解析，**禁止 per-pixel effective 路由**」，与最高设计 §4.5.2 直接冲突；当前实现与测试站在 SCI 文档一侧。按 AGENTS §8/§9 须走变更 claim 上呈负责人裁决（审计员未改任何文档）。

## §7 架构 · §10 双平台 · §12 版本 · §1 独立性 · §2 数据对象 — 子审计 SUB-C 结论（证据全文见 run/RELEASE-02/DESIGN-CONFORMANCE/SUB-C-arch-version.md）

| 编号 | 设计条款（文档:行，原文摘录） | 实际行为 | 证据 | 判定 | 严重度 | 违反 fail-closed | 建议处置 |
|---|---|---|---|---|---|---|---|
| DC-701 (C-01) | §1.2:63 三命令平级独立，禁止隐式串接入口 | CLI 仅 8 条命令；run/phase1..3 均 rc=2 unknown command | `command_tree.h:76-100`；实测 `run --phases 1,2,3` rc=2 | CONFORMANT | — | 否 | 补负例锁死 |
| DC-702 (C-02) | §1.2:63 同上 | 库层仍保留多 phase 进程内串接：build_pipeline_ir 接受 {1,2,3}，单元测试锁死 | `runtime_client.h:17,26`；`runtime_client.cpp:101,345`；`tests/unit/rt008_runtime_client_test.cpp:19-27` | DIVERGENT | 低 | 否（未接命令面） | 收窄为单 phase |
| DC-703 (C-03) | §1.2:64 阶段间只通过磁盘产品+manifest+哈希交换 | 各阶段写 output_dir + input_manifest_hash/config_hash；三命令各起进程 | `module_adapters.cpp:3860-4039`；`commands.cpp:1940-1954` | CONFORMANT | — | 否 | — |
| **DC-704 (C-04)** | **§2:84 数据对象禁止互相冒充（frame_snr 与 depth_m5 各是各）** | **p1_snr.json 的 frame_snr 承载 5σ 点源深度（flux5_adu/m5_mag），即 depth_m5；注释自述「不再输出任何整帧 SNR 标量」** | `module_adapters.cpp:3177-3183,2971-2973`；`frame_snr.schema.json`（要求 frame_snr_value，且明写二者不得互填） | **DIVERGENT** | **阻断** | **是** | 拆分对象；frame_snr 写真实 SNR |
| DC-705 (C-05) | §3.4:163-167 帧级 SNR 写入 HiPS 文件头（唯一承载） | properties 写入器 kv 列表无任何 frame_snr/SNR 键 | `aio_hips_writer.cpp:1114-1180`；`grep frame_snr aio/src/hips/`=0 | DIVERGENT | 高 | 是 | 在 properties 写 frame_snr |
| DC-706 (C-06) | §2:84 数据对象语义不得混淆 | weight_mode 数值语义三处不一致（CLI 1/2、v6 路由 0/1/2、coverage 内部 0/非0） | `session_commands.h:133-134`；`v6_runtime_contract.h:149-174`；`sky_plane.cpp:433,585,806` | DIVERGENT | 中 | 否 | 统一语义+合同测试 |
| DC-707 (C-07) | §7.1:431-464 lib/ 只含 algorithms/ 与 infrastructure/ | lib/ 根多出 phase1_session/phase2_session/phase3_session，均 STATIC 链入主 exe | `ls lib/`；`CMakeLists.txt:638,648,654,733,754,776` | DIVERGENT | 高 | 否 | 迁移/退役三目录 |
| DC-708 (C-08) | §7.1:466-468 CLI 下挂 normalize/mosaic/export 子目录作为子命令实现 | 三子目录各仅 1 个 inline 头，无实现，无人 include；真正实现在扁平 commands.cpp | `cli/normalize/normalize.h:1-18`；`grep 'normalize/normalize.h'`=0 | DIVERGENT | 中 | 否 | 落地或删除空壳 |
| DC-709 (C-09) | §7.1:441-450 算法模块并联放置 | rejection/sampling/upm/integration 四目录仅 README/module.yaml/memory.md，零代码；实现全在 coverage/ | `find lib/algorithms/{rejection,sampling,upm,integration} -type f`；`coverage/src/{rejection,sampler,upm,integrate}.cpp` | DIVERGENT | 高 | 否 | 拆模块或订正设计 |
| DC-710 (C-10) | §7.3:485 每个可调度模块=独立 DLL/SO | 19 个 module.yaml 声明 dll_name，实际 SHARED target 仅约 9 个；13 个声明 DLL 的模块无 .so，全部 STATIC | `grep add_library(.*SHARED`；`CMakeLists.txt` 31 add_library 中 3 SHARED/28 STATIC | DIVERGENT | 高 | 否 | 拆 SHARED 或订正 §7.3 |
| DC-711 (C-11) | §7.3:485 模块含 README/module.yaml/公开头/实现/测试 | 25 个模块中 15 个不完整（4 缺 README、7 缺 module.yaml、4 无头无实现、9 无共址测试） | SUB-C 附录 A python 统计 | DIVERGENT | 中 | 否 | 补齐 |
| DC-712 (C-12) | §7.3:487 每节点映射唯一入口；多个节点不得调用同一完整 Session | 生产注册表同时注册两个整阶段 Session 适配器（phase2.resample→p2_session_run、phase3.resample→p3_session_run） | `module_adapters.cpp:598-615,617-642,6776-6789,1031-1055`；`tests/unit/rt008_runtime_client_test.cpp:59` | DIVERGENT | 高 | 否 | 移除 session 模块 |
| DC-713 (C-13) | §7.2:472-481 typed DAG·aio 唯一 I/O·算法并联 | 端口注册表在位、aio 为唯一边界；但并联被 C-09/C-10 破坏；P2/P3 IR module_id 双份 | `module_ports.registry.json`；`runtime_client.cpp:130-180`；`module_adapters.cpp:598` | DIVERGENT | 中 | 否 | 收敛 module_id |
| DC-714 (C-14) | §10.1:520-525 唯一可执行 `ACSD Cli.exe`/`acsd_cli` | 实际入口名 `astrocs`（product manifest rel_path="astrocs"），无 acsd_cli | `build/astrocs`；`cmake/install_layout.cmake:16-40`；`build/astrocs.product.json` | DIVERGENT | 高 | 否 | 改名或走设计变更 |
| DC-715 (C-15) | §10.1:522-523 唯一 ELF + 各 .so + schemas + manifest | 根仅 3 个 .so；20+ 科学模块 STATIC 链入 exe | `ls build/*.so`；`build/install_manifest.txt` | DIVERGENT | 高 | 否 | 同 C-10 |
| DC-716 (C-16) | §10.2:539-541 双平台不同 CMake 配置，平台相关源码分开写 | 单一根 CMakeLists（27 处 WIN32/MSVC 条件），无平台专用配置；平台源码以 #if _WIN32 内联（101 文件） | `CMakeLists.txt`；`find cmake/`；`grep -rlE _WIN32 lib`→101 | DIVERGENT | 中 | 否 | 拆平台 CMake 或订正措辞 |
| DC-717 (C-17) | §10.2:541/§10.3 同一 C ABI、同一 manifest；Linux→CI→Windows | 双平台 CI 在位；Windows manifest 模板同步 | `.github/workflows/ci-linux.yml,ci-windows.yml`；`cmake/astrocs.product.windows.json.in` | CONFORMANT | — | 否 | — |
| **DC-718 (C-18/C-19)** | **§12:610「Alpha 之前：程序与代码中不包含任何版本信息…此前不存在任何版本信息」** | **`--version` 输出 0.11.0-alpha.2+g<sha>；生成头/product manifest/VERSION/install-tree.contract 均含版本；run/ 下 759 个文件含版本串** | 实测；`build/version_generated.h`；`build/astrocs.product.json`；`VERSION`；`run_context.json` | **DIVERGENT** | **阻断** | **是** | 移除版本注入（已登记 P0-15，负责人裁决 FIN 阶段处理） |
| DC-719 (C-20) | §12:612 只有通过验收的 Phase 才能标 available；未实现必须明确报告 | product manifest 无 available 字段；status 枚举仅 {SKELETON,IMPLEMENTED}，无 NOT_IMPLEMENTED/NOT_VERIFIED/DEFERRED/FAIL；schema 强制 product_version | `packaging/schemas/astrocs-product.schema.json`；`build/astrocs.product.json` | DIVERGENT | 高 | 是 | 增加 availability/负向状态 |
| DC-720 (C-21) | §12:612 不得用命令占位/空输出冒充完成 | 5 个科学模块标 IMPLEMENTED，但其 module.yaml 声明的多数 DLL 从未构建 | `build/astrocs.product.json`；module.yaml dll_name | DIVERGENT | 高 | 否 | 用实际构建证据回填 status |
| DC-721 (C-22) | §7.2:475 typed DAG；§7.3:487 多个节点不得调用同一完整 Session | typed DAG 校验器 typed_dag.py **仅被 tests/ 引用**；C++ 生产 CLI（build_pipeline_ir+load_pipeline）不读 module_ports.registry.json、不执行这些校验 | `pipeline/typed_dag.py:1-30`；消费者仅 `tests/runtime/test_typed_dag_negative.py:25`、`tests/pipeline/test_typed_dag_plan.py:27`；`grep -rn module_ports lib --include=*.cpp` 无加载 | DIVERGENT | 中 | 否 | 让 C++ Runtime 消费同一 registry |

> **DC-718 补强（CI 反向固化）**：`ci/checks.json:6446-6480` 的 `VERSION-CONSISTENCY`（command=`python3 ci/check_version.py --expected 0.11.0-alpha.2`，`waivable:false`）与 `VERSION-NAMESPACES` **强制版本串存在且一致**；`ci/check_version.py:1-60` 的规则 [2][3][4][5] 要求 `VERSION`/CMake/生成头/活动文档全部携带 `0.11.0-alpha.2`，漂移即 FAIL。即：**CI 绿灯 = 必然违反 §12**，且全仓无「不得含版本信息」的负例检查器。这是"绿灯不可信"的根因样本。

## §8 CPU 后端与资源 — 主审计员直接结论

| 编号 | 设计条款（文档:行，原文摘录） | 实际行为 | 证据 | 判定 | 严重度 | 违反 fail-closed | 建议处置 |
|---|---|---|---|---|---|---|---|
| DC-801 | §8:501「重计算负载资源门（G-RES-01）：…exit 10 条件见 docs/...21_observability.md §8；阈值唯一源 = contracts/resource_gate_v1.json」+ §6.3:419「10=资源利用率或内存增长门禁失败」 | 进程内默认 **record_only**；唯一能切到 enforced（exit 10）的开关 `--strict-resource-gate`/`--on-resource-gate` **被 CLI 解析器判 unknown flag（rc=2）**，用户不可达；L4 真实日志记录 `alloc_growth_unbounded` 但 rc=0 | `commands.cpp:565-567`（消费者）；`parser.cpp:36-42` 与 `command_tree.h:37-46`（白名单无此二旗标）；实测 `unknown flag`；`contracts/resource_gate_v1.json:73-78`（in_process_default=record_only, hard_fail_exit_code=10）；`run/RELEASE-01/e2e/l4/logs/p1_m42_t3_m1_red.log` 末尾 WARNING recorded-not-enforced + rc=0 | DIVERGENT | 高 | 否（门不误报，但**无红灯路径**） | 修复白名单或退役该开关并明确 exit 10 不可达；补 CLI 面负例 |
| DC-802 | §8:501 阈值唯一源；契约 `memory.unit`：「MiB/s…禁止再使用 MB/s」 | 数值按 MiB/s 计算（`1048576`），但**日志文案仍写 "MB/s"** | `resource_gate.h:515,544`（"MB/s" 字符串）vs `:197-199`（MiB/s 注释）；`contracts/resource_gate_v1.json:69` | DIVERGENT | 低 | 否 | 统一文案为 MiB/s |
| DC-803 | §8:496「一个进程只有一个资源调度器与线程预算源；模块不得硬编码 workers、不得私建长期线程池」 | 节点级 OMP 注入经 `ScopedOmpWorkerInjection` 作用域 RAII，线程数来自 host budget，未发现生产算法硬编码核数（ACR/legacy 除外） | `module_adapters.cpp:233-263`；`p1_session.cpp:280`；实测 L4 日志 `budget workers=16 (cpus=16)` | CONFORMANT | — | 否 | 保持（SUB-D 复核细节） |

## §11 验证体系 — 主审计员直接结论（SUB-D 细节并入后见下）

| 编号 | 设计条款（文档:行，原文摘录） | 实际行为 | 证据 | 判定 | 严重度 | 违反 fail-closed | 建议处置 |
|---|---|---|---|---|---|---|---|
| **DC-1101** | **§11.3:583-590 + ACCEPTANCE_SPEC.md:116-158「L4 真实数据端到端视觉验收（M42 + Galaxy Center）…负责人逐项确认」为发布门** | **L4 未运行**：官方验收证据自述 `full_run_status="NOT RUN on this node: M42 ~196 lights ~106GB Phase1, GC ~157 lights ~85GB; available disk ~108GB"`；实际 L4 演示是 L3 产物转 PNG 切块 | `artifacts/acceptance/L4/STATUS.json`；`artifacts/acceptance/L4/` 仅 1 文件 | **DIVERGENT** | **阻断** | 是（发布门未过） | 以 49 帧 R 重建 L4 全量并逐块目检 |
| **DC-1102** | **§11.3:583-590 L1 合成科学性验收；ACCEPTANCE_SPEC.md:29-53** | **L1 证据目录完全不存在**；`artifacts/acceptance/` 只有 L2/L3/L4（且命名不符 ACCEPTANCE_SPEC §6 要求的 `l1_science/`、`l2_performance/`、`l3_small_batch/`、`l4_visual/`），**无 `ACCEPTANCE_REPORT.md`** | `ls -la artifacts/acceptance/`（L2/L3/L4，共 3 文件）；`find . -name ACCEPTANCE_REPORT.md`=0 | **DIVERGENT** | 高 | 是（分层证据缺失） | 按 §6 落位补齐四层证据与验收报告 |
| **DC-1103** | **§4.3:255「逆方差叠加…标准行为」+ §11.3 L3「帧级 SNR 参与叠加的权重链路可追溯」** | **L3 验收自述权重链 BLOCKED**：`weight_chain="BLOCKED by PRE-F-01 (no variance/ivar from Phase1 CLI); chain completed via explicit legacy_allow_weight_fallback=true"`——即 L3「通过」靠关闭设计要求的逆方差叠加 | `artifacts/acceptance/L3/EVIDENCE.json`；与 DC-405/DC-203 互证 | **DIVERGENT** | **阻断** | **是** | 修 Phase1 ivar 后以 weight_mode=2 重跑 |
| DC-1104 | §11.3 L3「原子性：中断/失败后产品目录只出现完整产品」 | L3 证据自述 `atomicity="failure paths leave run metadata only (manifest status=incomplete), no science products"`，但 SUB-B 实测 p3 中间产物（p3_props/p3_wcs/p3_resampled/p3_verify）直写正式路径、失败残留（DC-508/509）；两说矛盾，L3 声明未覆盖 p3 中间产物 | `artifacts/acceptance/L3/EVIDENCE.json`；`module_adapters.cpp:5910,5960,6235,6280,6509` | DIVERGENT | 高 | 是 | 统一原子提交；补失败注入门 |
| DC-1105 | §11.4:596-604 状态阶梯；§12:612 未实现必须如实报告 | RELEASE-02 控制包 ACCEPTANCE 总表多项空白：FIX-B/C/D/E、TST-101、BLD-101、E2E-102、VIS-102、PERF-102、DEL-102 **均未填状态/结论**；FIX-REJ 标「PASS（研究+claim；实现待派）」，P0-20 未实现；P0-15（版本）「FIN 阶段处理」 | `工程控制/RELEASE-02/ACCEPTANCE.md:13-22`；`GAP_AUDIT.md §8.3` | DIVERGENT | 高 | 否 | 如实填写各层状态，不得以研究/claim 冒充实现完成 |
| DC-1106 | §11.1:560「每个模型有独立解析/高精度/Monte Carlo Oracle」；§11.1:565「每个近似有 mutation 证明门能红」 | 见 SUB-D 结论（并入下节） | 待 SUB-D | UNVERIFIABLE（本行） | — | — | 由 SUB-D 补 |

## §11 验证体系 / §8 资源 / 测试可信度 — 子审计 SUB-D 结论（证据全文见 run/RELEASE-02/DESIGN-CONFORMANCE/SUB-D-verification.md）

### §11.1 科学正确性

| 编号 | 设计条款（文档:行） | 实际行为 | 证据 | 判定 | 严重度 | fail-closed | 处置 |
|---|---|---|---|---|---|---|---|
| SUB-D-01 | §11.1:562 每个模型有独立 Oracle | 核心模块有；**sampling/fits_output/integration/platesolve 无独立 Oracle** | `lib/algorithms/sampling/module.yaml` entrypoint=MISSING；`find lib/algorithms -iname '*oracle*'`；ACCEPTANCE_SPEC:40 | DIVERGENT | 高 | 否 | 补 Oracle 或登记 NOT_IMPLEMENTED |
| SUB-D-02 | §11.1:562 注入点源验证 σ_F=1/√W_psf | 有 MC 注入 + 独立 long-double Oracle + 故障注入能红 | `tests/unit/v6_p1_psfw/p1psfw_tests_oracle.cpp:15-67`；`p1psfw_oracle.hpp:1-4` | CONFORMANT | — | 否 | — |
| SUB-D-03 | §11.1:562 改变星表亮度分布只改 source-SNR 摘要、不改 information | **无任何测试** | `grep -rn "亮度分布\|information.*unchanged" tests/ lib/`=0 | DIVERGENT | 中 | 否 | 补正例 |
| SUB-D-04 | §11.1:563 独立帧验证 SNR_combined²=ΣSNR_k²，相关项存在时简单求和被拒 | **无任何测试** | `grep -rn "SNR_combined\|quadrature" tests/ lib/`=0 | DIVERGENT | 高 | 否 | 补正例+负例 |
| SUB-D-05 | §11.1:564 扩展源验证常量面亮度/梯度/总通量/covariance | 无同名断言（drizzle 有 T10-T12 但无"面亮度+总通量同时守恒"） | `grep -rin "面亮度\|surface_brightness\|守恒" lib/algorithms/drizzle tests/`=0；ACCEPTANCE_SPEC:40 | UNVERIFIABLE | 中 | 否 | 补双守恒 Oracle |
| SUB-D-06 | §11.1:564 重采样验证 C_out=RC_inRᵀ | 有：生产显式实现 + 独立稠密转写对拍 1e-13 | `p3_rsmp_covariance.cpp:22-50`；`p3_rsmp_oracle_test.cpp` | CONFORMANT | — | 否 | — |
| SUB-D-07 | §11.1:565 每个近似有 mutation 证明门能红 | 部分有；**v6_p3_rsmp mutation driver 默认 OFF**；多数只在 run/ 一次性脚本，无"每近似一门"登记 | `tests/unit/v6_p3_rsmp/CMakeLists.txt:52-58`；`ci/checks.json:1216-1244` | DIVERGENT（部分） | 中 | 否 | 登记"近似↔门"，纳入 ctest |
| SUB-D-08 | §11.1:566 真实数据（M42/银心）验证接缝/背景/星形/排异/黑洞/噪声 | 输入不具代表性且未通过：L4 仅 12/49 帧、逐像素 n=2 | GAP_AUDIT:63-108；`l4__p1_*/p1_stack.json` n_source_pixels=16777216 | DIVERGENT | 高 | 是 | 49 帧 R 重建 L4 后重跑 |

### §11.2 验证层级

| 编号 | 设计条款（文档:行） | 实际行为 | 证据 | 判定 | 严重度 | fail-closed | 处置 |
|---|---|---|---|---|---|---|---|
| SUB-D-09 | §11.2:571 单元测试 | 真实存在且规模大 | `build/**/CTestTestfile.cmake`；`tests/test_index.csv` | CONFORMANT | — | 否 | — |
| SUB-D-10 | §11.2:572 模块数值 SCI/ALG Oracle | 同 SUB-D-01 | 同 SUB-D-01 | DIVERGENT（部分） | 高 | 否 | 同 SUB-D-01 |
| SUB-D-11 | §11.2:573 合同/ABI 测试 | 存在但不完整/被环境跳过：tests/abi 18 用例 errors=9（缺 gcc）；tests/backend 70 用例 errors=29、skipped=50 | `tests/test_index.csv:5,6`；`LastTestsFailed.log`（p1noise_abi_layout） | DIVERGENT（部分） | 中 | 是 | 补齐编译器或登记 ENV_FAIL 不计绿 |
| SUB-D-12 | §11.2:574 Phase 内 Pipeline 测试 | 真实存在 | `tests/unit/p{1,2,3}*_real_nodes_test.cpp`；`tests/integration/v6_p{1,2,3}` | CONFORMANT | — | 否 | — |
| SUB-D-13 | §11.2:575 合成全链（三阶段分别） | 三阶段分别有集成+oracle | `tests/integration/v6_p1/oracle/*`、`v6_p2/oracle/*`、`v6_p3/p3_v6_export_oracle.py` | CONFORMANT | — | 否 | — |
| SUB-D-14 | §11.2:576 Linux 真实数据流终验 | 输入降采样且受 P0-21 污染；L3 自述权重链 BLOCKED、用 legacy_allow_weight_fallback 走通 | GAP_AUDIT:63-108；`artifacts/acceptance/L3/EVIDENCE.json` | DIVERGENT | 高 | 是 | 49 帧 R 重跑；移除 fallback |
| SUB-D-15 | §11.2:577 Windows/Fatduck 复验 | 无当前候选 Windows 复验；旧世代记录未绑定 SHA；RELEASE_STATUS 自述 NOT_VERIFIED | `reports/evidence/WIN*.md`（8月）；`docs/RELEASE_STATUS.md:14` | DIVERGENT/UNVERIFIABLE | 高 | 否 | 正式平台重跑并归档 SHA |
| SUB-D-16 | §11.2:578 图像审核 Agent 初审 → Owner 终审 | 无 Owner 终审证据；L4 STATUS 明写 owner_step 待办 | `artifacts/acceptance/L4/STATUS.json`；`find . -name ACCEPTANCE_REPORT.md`=0 | UNVERIFIABLE | 高 | 否 | 补 Owner 逐块确认 |

### §11.3 四层验收

| 编号 | 设计条款（文档:行） | 实际行为 | 证据 | 判定 | 严重度 | fail-closed | 处置 |
|---|---|---|---|---|---|---|---|
| **SUB-D-17** | ACCEPTANCE_SPEC:166 分层证据落位 l1_science/… | **L1 层证据完全缺失** | `find artifacts/acceptance -type f`=3 文件（L2/L3/L4）；无 l1_science/ | **DIVERGENT** | **阻断** | 否 | 补 L1 证据 |
| SUB-D-18 | ACCEPTANCE_SPEC:166 目录命名 | 目录为 L2/L3/L4，非规定的 l2_performance/… | `ls artifacts/acceptance/` | DIVERGENT | 低 | 否 | 改名或走变更 |
| SUB-D-19 | ACCEPTANCE_SPEC:167 验收报告 | `ACCEPTANCE_REPORT.md` 不存在 | `find . -name ACCEPTANCE_REPORT.md`=0 | DIVERGENT | 高 | 否 | 补写 |
| **SUB-D-20** | §11.3:590 L4 真实视觉验收 M42+GC 全量 | **全量未运行**（自述 NOT RUN，仅 L3 demo） | `artifacts/acceptance/L4/STATUS.json` | **DIVERGENT** | **阻断** | 否 | 正式节点跑全量 |
| SUB-D-21 | §11.3:587 L2 G-RES-01 零 enforce 违约 | 证据 verdict=pass、violations=[]；两条 recorded 属 record_and_justify | `artifacts/acceptance/L2/G-RES-01.json`；`resource_gate_v1.json` | CONFORMANT | — | 否 | — |
| SUB-D-22 | §11.3:588 L3 抽检合格 + §4.3 权重链 | 权重链被 PRE-F-01 阻塞，靠 legacy_allow_weight_fallback 走通 | `artifacts/acceptance/L3/EVIDENCE.json` | DIVERGENT | 高 | 是 | 打通权重链后重判 |

### §11.4 状态阶梯 / §8 资源

| 编号 | 设计条款（文档:行） | 实际行为 | 证据 | 判定 | 严重度 | fail-closed | 处置 |
|---|---|---|---|---|---|---|---|
| SUB-D-23 | §11.4:596-604 状态阶梯 | 22 个 module.yaml 中 21 CONTRACT_READY、1 IMPLEMENTED_HEADER_ONLY，0 IMPLEMENTED/INSTALLED/VERIFIED；module_version 含 0.11.0-alpha.2 | `grep -rh module_status: lib/`；`docs/RELEASE_STATUS.md` | DIVERGENT | 中 | 否 | 按实际执行改标 |
| SUB-D-24 | §11.4:604 合成/历史节点 ≠ VERIFIED | 发布结论诚实：NOT_READY_FOR_RELEASE、真实/Windows NOT_VERIFIED、ACR DORMANT | `docs/RELEASE_STATUS.md:9-19` | CONFORMANT | — | 否 | — |
| SUB-D-25 | §8:496 一个进程一个线程预算源；不得硬编码 workers | 存在自行决定 worker 数路径：execution_options.h 默认 hardware_concurrency；orchestrator.cpp:642-648 回退 hardware_concurrency；ACR dispatcher 硬编码 16（DORMANT） | `coverage/include/astro/phase2/execution_options.h:16,24,38`；`orchestrator.cpp:642-648`；`acr/scheduler/dispatcher.cpp:476` | DIVERGENT | 中 | 否 | 默认取 scheduler lease |
| SUB-D-26 | §8:496 CPU-heavy 必须多线程 | L2 实测 avg_cpu 1302%、threads_max=64、p50 0.9475 | `artifacts/acceptance/L2/G-RES-01.json` | CONFORMANT | — | 否 | — |
| SUB-D-27 | §8:497 内存极简化 | 天光面有实测（8f vs 64f，n_nodes 不增、RSS<200MB）；**稠密权重流式性无实测** | `tests/unit/v6_p2_sky/v6_p2_sky_test.cpp:300-334` | CONFORMANT/UNVERIFIABLE | 中 | 否 | 补权重链 RSS 实测 |
| SUB-D-28 | §8:495 benchmark 输出安装目录、稳定统计 | 落位符合（exe parent/cpu_profile.json）；统计口径未复核 | `commands.cpp:1879-1930`；`profile_gen_v2.cpp` | CONFORMANT/UNVERIFIABLE | 低 | 否 | 归档统计口径 |
| SUB-D-29 | §8:501 阈值唯一源 resource_gate_v1.json | 符合：C++ 经 configure_file 生成常量；Python 运行期读同一 JSON | `build/resource_gate_thresholds_generated.h`；`resource_gate.h:100,107-126`；`run_monitored.py:492-495` | CONFORMANT | — | 否 | — |

### 测试可信度（"绿灯不可信"清单）

| 编号 | 设计条款（文档:行） | 实际行为 | 证据 | 判定 | 严重度 | fail-closed | 处置 |
|---|---|---|---|---|---|---|---|
| **SUB-D-30** | §11.1:560 不得以当前输出生成唯一 expected | **"ctest 460/460 全绿"与运行记录不符**：2026-09-18 16:47 轮 460 tests / 443 passed / 12 skipped / **3 failed**（p1hips_selfcheck、p1noise_abi_layout、v6_aio_impl_mutations）；2026-09-16 轮 414 tests / 1 failed | `evidence/LastTestsFailed.log`；`evidence/ctest-status.log` | **DIVERGENT** | 高 | 是 | 以真实结果为准；修 3 条失败 |
| SUB-D-31 | ENGINEERING_SPEC §5.1；AGENTS §8 SKIP 充数=未完成 | **12 条 SKIP 充数**，含负例 FilterMismatchRejected 与真实 HiPS 用例；p1001 test_golden_parity 环境未设即静默跳过仍计 PASS | `evidence/LastTestsDisabled.log`；`synthetic_gate.cpp:3342-3959`；`p1001_real_nodes_test.cpp:2219-2225` | DIVERGENT | 高 | 是 | 提供 fixture；禁"环境未设即 PASS" |
| SUB-D-32 | AGENTS §8 空证据文件=未完成 | **6 个 0 字节证据文件**（1/N 一致性目录的 alloc_report/alloc_samples/astrocs_run/resource_summary/resource_timeseries/worker_balance） | `find run/RELEASE-01/e2e/evidence -type f -size 0`=6 | DIVERGENT | 中 | 是 | 重跑产出非空证据 |
| **SUB-D-33** | **§3.4:153-157 N 帧⇒N 产物；§11.1:560** | **测试把"只 drizzle 一帧"固化为期望**：p1001 用 2 lights 却断言每节点 call_count==1；全仓无"N 帧→N 产物"计数断言 | `tests/unit/p1001_real_nodes_test.cpp:288,324,340,522-589,2843`；GAP_AUDIT:139 | **DIVERGENT** | **阻断** | 是 | 增 N→N 正例 + 帧失败负例 |
| SUB-D-34 | §11.1:565 / ENGINEERING_SPEC:120 能红能绿 | 关键 mutation 门默认不进 ctest（v6_p3_rsmp OFF）；mutation 类 ctest 仅 1 条 | `tests/unit/v6_p3_rsmp/CMakeLists.txt:52-58`；`ci/checks.json:1216-1244` | DIVERGENT | 中 | 否 | 纳入 ctest/CI |
| SUB-D-35 | ENGINEERING_SPEC §8:120；AGENTS §8 | **Python 套件大面积 error/fail/skip 却登记为"已实测"**：api 2 error+19 skip、backend 29 error+50 skip、cli 1 fail+9 error+72 skip、io 3 error、version 2 fail、abi 9 error | `tests/test_index.csv:2,3,5,6,7,15,23` | DIVERGENT | 中 | 是 | 逐条修复或登记豁免 |

> SUB-D 阻断级：**SUB-D-17（L1 证据缺失）、SUB-D-20（L4 全量未跑）、SUB-D-33（测试固化单帧语义）**。
> SUB-D 可信面：核心模块独立 Oracle、v6_p1_psfw σ_F MC 门、v6_p3_rsmp C_out=RC_inRᵀ 对拍、G-RES-01 阈值单一来源、天光面内存流式性用例。
