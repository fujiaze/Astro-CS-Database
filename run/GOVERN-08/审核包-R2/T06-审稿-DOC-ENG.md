# T06 审稿 · DOC-ENG（车道 `docs/engineering/**`）

审稿人：独立子代理（未参与 T05 写作）。只审稿，不改文档；全程无 git 写操作（git 仅只读，中文路径一律 `git -c core.quotepath=false`）。

权威链依据（按顺序通读）：`AGENTS.md` → `docs/ACSD_DESIGN.md` → `standards/03_READING_AND_ADVERSARIAL_REVIEW.md` → `standards/04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md`。

**审稿结论：大修。** 本车道存在 4 条阻断级根因（见 §4），任一条不修则文档集不能自解释、复算链不可用。

---

## 1. 读完了吗

| 项 | 数 |
|---|---|
| 本车道文档总数 | **61 份 md，9 662 行**（`find docs/engineering -type f -name '*.md' \| wc -l` / `find … -exec wc -l {} + \| tail -1`） |
| 我**逐行读完**的 | **26 份**（README 1 · architecture 3 · contracts 12 · UNIFIED_OBJECTS 1 · data 3 · standards 2 · resources 1 · testing 1 · api/abi 1 = 25，另加 `api/PUBLIC_API.md` 由子代理全读、我抽核） |
| 我**未逐行读完**的 | **36 份**，逐份列出（见 §1.1） |
| 子代理分头覆盖 | 6 个子代理（contracts+api ×2、architecture+data ×2、standards+resources+testing、build+governance），我对其每条结论独立复核；**采纳 41 条、自行否决/改写 9 条、未核转登记 14 条**（见 §6） |

### 1.1 未逐行读完的 36 份（逐份列出，诚实登记）

| 文件 | 行数 | 未读原因 | 覆盖方式 |
|---|---|---|---|
| `api/PUBLIC_API.md` | 2321 | 单文件超长，须读完 61 份的时限内无法与人读同深度并读完其余 | 子代理 178e0080 全读并取证；我抽核事件 kind 三套集合、`PIPELINE_BLOCK`/退出码相关段落 |
| `api/abi/SECURE_LOADER.md` | 118 | 同上 | 子代理 178e0080 全读；我核对 `secure_loader.h` 枚举与其错误模型 |
| `api/README.md` `api/abi/README.md` | 2 / 2 | 体量过小，纳入结构轮 | 子代理核对（结论：无缺陷，否决登记） |
| `architecture/README.md` `contracts/README.md` `data/README.md` `governance/README.md` `resources/README.md` `resources/cpu/README.md` `resources/observability/README.md` `standards/README.md` `testing/README.md` `build/README.md` | 各 2–4 | 同上 | 子代理 + 我的目录树比对 |
| `data/ARTIFACT_STORE.md` | 123 | 时限 | 子代理 ×2 独立全读 |
| `data/PROVENANCE.md` | 176 | 时限 | 子代理 ×2 独立全读 |
| `governance/DOCUMENT_GOVERNANCE.md` | 246 | 时限 | 子代理 6c924167 全读；我核对目录拓扑段与 UNRESOLVED 章节号 |
| `governance/DUAL_LINE.md` | 93 | 时限 | 子代理 ×2 全读 |
| `governance/TRACEABILITY.md` | 335 | 时限 | 子代理 6c924167 全读；我 grep 复核其 DATA ID 数量与 UNRESOLVED 争用 |
| `governance/UNRESOLVED.md` | 96 | 时限 | 子代理 6c924167 全读；**我亲自 grep 复核章节编号断裂** |
| `build/BUILD_GRAPH.md` `BUILD_NODES.md` `RELEASE.md` `README.md` | 121/74/130/2 | 时限 | 子代理 6c924167 全读并跑复算链；**我亲自复核 `.gitignore` 根因** |
| `resources/BENCHMARK.md` `cpu/BACKEND.md` `cpu/CAPABILITY_PROBE.md` `cpu/AVX2_PROVIDER.md` `cpu/ISA_VARIANTS.md` | 15/59/107/132/238 | 时限 | 子代理 c1e3efd0 全读；我 grep 复核 ISA/能力探测锚 |
| `resources/observability/RESOURCE_MONITORING.md` `RUN_GRAPH.md` `STRUCTURED_LOGGING.md` | 190/171/176 | 时限 | 子代理 c1e3efd0 全读；我核对 `resource_recorder.h` 20 列与 `log_event_v1.schema.json` 路径 |
| `standards/COMPATIBILITY.md` `CONCURRENCY.md` `DEPENDENCY.md` `DOCUMENTATION.md` `CACHE.md` `OPTIMIZATION.md` | 29/44/42/43/38/25 | 时限 | 子代理 c1e3efd0 全读 |
| `testing/VALIDATION_EVIDENCE.md` | 579 | 时限 | 子代理 c1e3efd0 全读 |

**未读部分的诚实边界**：对这 36 份我不宣称「逐行读完后的独立判断」，只宣称「子代理逐行读完 + 我按可复跑命令逐条复核其结论」。凡我未亲自复核的，一律在下文标 `[转]`（未核转登记），不混入我的判定。

---

## 2. 逐轮问题清单

定位约定：`文件:行`（行号随本文提交稳定）；凡需机器复跑的，给出可直接粘贴的命令。中文路径一律加引号。

---

### 轮 1 · 结构

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| 1-1 | `docs/engineering/build/`（整目录 4 份） | **整个构建正本目录不在版本控制内** | `git check-ignore -v docs/engineering/build/BUILD_GRAPH.md` → `.gitignore:20:build/`；`git -c core.quotepath=false ls-files docs/engineering/build \| wc -l` → `0`；`sed -n '20p' .gitignore` → `build/` | `.gitignore` 的编译产物规则改为锚到根的 `/build/`，并补 `!docs/engineering/build/`；4 份入库 |
| 1-2 | `contracts/LOG_AND_ERROR.md:1–177` 与 `:179–317` | **两篇文档被拼进同一文件**。前半称「本合同」，第 179 行起改称「本标准」，且把自己当第三方引 | 前半 `:172` 写「本合同『阶段 ID』一节–『error-sensitive 模块的必备要件』一节 的各表」，而这两节物理上在 `:232` 与 `:299`（后半）；`:180` 写「域 → 退出码的映射面在 `LOG_AND_ERROR.md`「阶段 ID」一节」——同一文件内自引第三方 | 拆回两篇独立正本；前半的 4 条冻结面各自成节；共享部分改为单向引用 |
| 1-3 | `contracts/CONFIG.md:236–379` | **第二、三篇文档被拼入**：`:236` 起是 orchestrator Stage2 配置文档，`:374` 起是 Stage1 config 文档 | `:237` 自述「本文描述的是 **orchestrator 的 Stage2 配置**……**不是**三命令 `phase_config` 合同」，却在 `:239`/`:245` 把 `CONFIG.md`「三命令 phase_config 与模板」一节当第三方引——文档引自己为第三方是拼接的确证；`:376`「见 `lib/infrastructure/pipeline/orchestrator/configs/stage1_*.json` 模板」是另一主题 | 拆出独立文档；CONFIG.md 只留三类配置与 phase_config 字段合同 |
| 1-4 | `resources/PERFORMANCE_MODEL.md:23`、`:130` | **自指**：「性能基线与回归判据见 `PERFORMANCE_MODEL.md`」出现在 PERFORMANCE_MODEL.md 自身两处 | 直接对读 | 改指真实落点（`testing/VALIDATION_EVIDENCE.md` 或 `eng/contracts/resource_gate_v1.json`） |
| 1-5 | `standards/CODE.md:85–98` 与 `:100–141` | **代码标准与注释标准两篇混写**，无「注释纪律」总标题 | `:100` 起的 `## 原则 / ## 必须注释 / ## 必须删除/迁移 / ## 叙述性注释的清理面 / ## 长度 / ## 审计` 全是注释卫生条款，与 `:85` `## 关联` 的代码风格条款不同主题 | 拆出 `standards/COMMENT.md`；CODE.md 只留代码条款并指向 |
| 1-6 | `contracts/PIPELINE_BLOCK.md:85–93` vs `:102–109` | **两套 C 编号冲突**：前表 C4=方向一致 / C5=载体合同 / C6=非退化；后表 C4=无幻边 / C5=序为拓扑序 / C6=psf 在 wcs 之后 | 直接对读两表；`:74`–`:78` 引用的是前表 C2/C3/C3b/C5/C6 | 改名空间（前表 `PC-C*`、后表 `IR-C*`），或合并 |
| 1-7 | `contracts/PIPELINE_BLOCK.md:100` | 声明范围写「C4–C7」，表内却有 C8，且 C8 排在 C7 之上 | `:100` vs `:108`–`:109` | 同 1-6 |
| 1-8 | `testing/TEST.md:15–17` | 导语目录称「测试侧独立 Oracle 的归属面（第十一节）」，实际该节是 **§13**；全文 13 节，导语只列到 11 | `grep -n "^## " docs/engineering/testing/TEST.md` | 导语改「第十三节」并补 12、13 |
| 1-9 | `api/abi/ABI.md:77–79`、`:97` | 句子破碎：「面向用户的分类、退出码与阶段 ID 分别由 「并发合同模板」一节 / 「头文件独立性验证合同」一节 / 「落点映射」一节 与 「落点映射」一节 定义」——3 项对 4 个节名，且「并发合同模板」定义的是并发注解模板而非用户分类；`:97`「分阶段 API 面见 `../PUBLIC_API.md`、`../PUBLIC_API.md`、`../PUBLIC_API.md`」同一路径重复 3 次 | 直接对读 | 重写落点映射表；删重复路径 |
| 1-10 | `contracts/HIPS_STORAGE_FORM.md:18` | 自述「本合同冻结**四**件事」，紧接列出 **6** 条编号项 | `:18` vs `:20`–`:25` | 改为「六件」 |
| 1-11 | `architecture/MODULE_MAP.md:62` | 节点序列写「校准、修饰、**星点与 PSF、天体定位**、测光……」 | `python3 -c "import json;d=json.load(open('lib/infrastructure/pipeline/module_ports.registry.json'));print([m['module_id'] for m in d['modules']][:8])"` → `['acsd.phase1.calibration','acsd.phase1.cosmetic','acsd.phase1.wcs-platesolve','acsd.phase1.star-psf','acsd.phase1.photometry','acsd.phase1.noise-snr','acsd.phase1.drizzle','acsd.phase1.writer']`；`contracts/PIPELINE_BLOCK.md:107` 判据 C6 明确 `pos(psf) > pos(wcs)` | 按注册表与 C6 判据改为「校准、修饰、天体定位、星点与 PSF、测光……」 |
| 1-12 | `governance/UNRESOLVED.md:58`/`:66`/`:78` | 章节编号断裂：出现**两个 `## 5`**，且 `## 4` 排在 `## 5` 之后 | `grep -n "^## " docs/engineering/governance/UNRESOLVED.md` → `5:## 1 / 14:## 2 / 43:## 3 / 58:## 5 / 66:## 4 / 78:## 5 / 85:## 6 / 90:## 参考文献` | 重排为 1–6 单调 |
| 1-13 | `governance/DOCUMENT_GOVERNANCE.md:21–22` `[转]` | 目录拓扑段写 `engineering/` 子目录含 `abi/ cpu/ data/ io/ observability/`，实际为 `api/ resources/(cpu,observability)/ data/`，且 `docs/` 下无 `io/` | `find docs/engineering -maxdepth 1 -type d` | 改为实际拓扑 |
| 1-14 | `docs/engineering/README.md:8–18` `[转]` | 目录表漏列顶层正本 `UNIFIED_OBJECTS.md`（13 7 行，一级正本） | `wc -l docs/engineering/UNIFIED_OBJECTS.md` → 137 | 表内补一行 |

---

### 轮 2 · 口径

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| 2-1 | `architecture/ARCHITECTURE.md:123` | **平台口径与最高设计相反**：本篇写「正式开发、客户端与发布平台是 **Windows x64**，Linux amd64 承载常在线控制、静态分析、轻量编译与小合成实验」；最高设计第 11 章把 **Linux amd64 列为交付平台行**，其流程图为「Linux 节点：开发·构建·合成·**真实数据终验**」，且 `:125` 本篇自认「Linux……其读数不作为 Windows 发布性能结论」——与最高设计的 Linux 终验角色相反 | `docs/ACSD_DESIGN.md:466–470`（双平台交付表）、`:475–479`（Linux→双平台 CI 流程图）；本篇 `:3` 自认「冲突时以最高设计为准」 | 或删去 Linux 降级表述、或由负责人先改最高设计。按 AGENTS §4，最高设计为准 |
| 2-2 | `api/abi/ABI.md:38–42` | `acsd_status` 枚举止于 `ACS_ERR_SELFTEST=9`，**没有 INTERNAL(70) 的 ABI 层对应**，而进程退出码域有 70 且 `LOG_AND_ERROR.md:190` 要求「任何未分类失败按 INTERNAL（码 70）上行」 | `sed -n '37,42p' docs/engineering/api/abi/ABI.md` 与 `cat lib/infrastructure/cli/exit_codes.h`（`INTERNAL = 70`）对读 | 补 `ACS_ERR_INTERNAL=70`（与既有 C ABI 枚举不冲突，因值域不同），或在 ABI.md 写明 70 由宿主收敛、ABI 层不表达 |
| 2-3 | `api/abi/ABI.md:15` vs `:24`–`:42` | 命名规则写「前缀 `acs_`（函数）/ `ACS_`（类型 / 常量）」，但紧随其后的全部类型是 `acsd_head` / `acsd_span_f32` / `acsd_status` / `acsd_handle`（`acsd_` 前缀，非 `ACS_`），且未给任何函数示例 | 直接对读 | 规则改为「类型 `acsd_*`、枚举常量 `ACS_*`、函数 `acsd_*`」并给出真实示例 |
| 2-4 | `UNIFIED_OBJECTS.md:44`、`:60` | `provenance` 对象精度列写 `integer`，与最高设计 3.3「稀疏与元数据……**全程双精度**」冲突，且 provenance 是字符串元数据，标 `integer` 语义不成立 | `docs/ACSD_DESIGN.md:162`–`:163`；`docs/engineering/UNIFIED_OBJECTS.md:44` | 精度列按对象语义重填；或注明该列只对数值面有效 |
| 2-5 | `UNIFIED_OBJECTS.md:37` | `frame_snr` 精度列写 `float32\|float64`，与最高设计 3.3「帧级信噪比」归入「稀疏与元数据 → **全程双精度**」冲突 | 同上 | 收敛为 float64，或由负责人裁定 3.3 |
| 2-6 | `architecture/MODULE_MAP.md:64` vs `:92` | normalize 科学内核路径表**漏 `lib/algorithms/noise_snr`**，职责列却写了「噪声模型」；`:92` 又称该目录「不在根构建图（未 `add_subdirectory`）」 | `grep -n "acsd_phase1_noise" CMakeLists.txt` → `:1105 add_library(acsd_phase1_noise STATIC …)`，源为 `lib/algorithms/noise_snr/cpp/src/noise_model.cpp` 等，`:1245`/`:1349` 链接；`lib/infrastructure/scheduler/src/module_adapters.cpp` 头注亦称 `snr_science.cpp`「已编入 acsd_phase1_noise」 | 路径表补 `lib/algorithms/noise_snr`；`:92` 改为「子目录未 `add_subdirectory`，但其源集编入 `acsd_phase1_noise` 目标」 |
| 2-7 | `standards/CODE.md:14`–`:15` vs `:90` | MUST 声明「MinGW64 的定位 = **本地开发/兼容性验证**工具链；正式工具链取上条所列工具链」，但 `:90`「关联」节把「MSYS2 MinGW64 g++ **16.1.0**」与 C++17 并列为现行口径，未带定位限定，且在正文写入版本号 | 直接对读 `:14`–`:15` vs `:90`；AGENTS §5「正文无……版本号」 | `:90` 补「（本地开发/兼容性验证）」限定，版本号移入依赖锁或 `DEPENDENCY.md` |
| 2-8 | `standards/CODE.md:70` vs `:118`–`:119` | **自相矛盾**：`:70` 把「轮次标识面 `ACSD-*`」列为**必须按字面保留**的机器契约，`:118`–`:119` 又把「审计轮次」列为**必须删除/迁移**面 | 直接对读 | 二者划清边界（文档/台账的轮次 ID vs 生产代码注释），并显式声明各自管辖文件集 |
| 2-9 | `governance/TRACEABILITY.md:329` `[转]` | 排异算法数写「Rejection **7 种**」，最高设计 5.5 写「生产算法集为 none、percentile、winsorized、linear fit」= 4 种 | `docs/ACSD_DESIGN.md:294` | 以最高设计为准，或把另 3 档明确标为非生产 |
| 2-10 | `contracts/LOG_AND_ERROR.md:185`–`:186` vs `:210` | 可恢复科学状态取值集合含 `INVALID_INPUT`，硬错误类别表 `:210` 含 `INPUT_CORRUPT`，两个近义名指向**相反结果**（`rc=0` 有效 vs `rc≠0` 失败），且 `:250` 的 `^[A-Z][A-Z0-9_]{0,63}$` 同时容纳两者 | 直接对读 | 二者至少改名区分（如 `INPUT_TRUNCATED` / `INPUT_CORRUPT`），或在状态表加互斥判据 |
| 2-11 | **`data/ARTIFACTS.md:107` vs `architecture/DATA_FLOW.md:142` vs `lib/algorithms/coverage/include/astro/phase2/upm.h:175`–`:177`** | **同一物理量三套公式**（本车道证据轮的核心发现之一）：① `ARTIFACTS.md:107` 写 `UPM 控制点权重 = quality × geom × control_ivar`（几何乘进**分子**）；② `DATA_FLOW.md:142` 写 `raw_w = quality_factor × control_ivar`（无 geom）；③ 代码正本 `upm.h:175`–`:177` 明文「production（`cfg.use_ivar_weight != 0`）: raw_w = quality_factor × control_ivar（**几何可靠性在 per-control 归一化中施加**）」⇒ ① 把「归一化阶段施加」误写成「分子乘入」，与代码**相反**；且 ①② 互斥 | `sed -n '173,186p' lib/algorithms/coverage/include/astro/phase2/upm.h`（含 ablation 对照式 `quality_factor * support^support_power * snr^2/(1+snr^2) / max(unc^2, sigma_floor^2)`） | 以 `upm.h` + `DATA_FLOW.md` 为准删 `geom`；把「分子 vs per-control 归一化」两阶段关系写进唯一正本 |
| 2-12 | `data/ARTIFACTS.md:105` | **把已退役的权重口径登记为现行正本**：文档登记「`weights` (integrate) = 候选栈数值权重 = **support×SNR² 或等权(1.0)**」且歧义状态标「**已消除**」；代码头注明写该口径**已删除** | `sed -n '8,16p' lib/algorithms/coverage/include/astro/phase2/integrate.h` → 「**单一权重口径** —— 唯一生产策略 = 调用方构造的逐样本逆方差权重 w = SNR²/F_ref² = 1/σ_F² …… 原 `stack.support_x_snr2.v1`（weight_mode=0，weights = support × SNR²）与 `stack.equal.v1`（weight_mode=1 → 等权）两个**可选口径**及其 weight_mode 选择键**已删除** —— support 是无量纲几何量、equal 是等权，二者都不是信号/噪声之比」。同层 `UNIFIED_OBJECTS.md:5`、`:132` 亦明写「`weight_mode` 家族……**不存在**」 | 改为「候选栈数值权重 = 逐样本逆方差 `w = SNR²/F_ref² = 1/σ_F²`（调用方构造；`nullptr`=等权是 C API 输入合同、非可选口径）」 |
| 2-13 | **最高设计 3.3 精度归属 vs `lib/infrastructure/aio/src/aio_api.cpp:35`–`:39`** | **两类精度在生产面不是独立归属，而是一个全局开关**：最高设计 3.3 规定「稠密大面默认单精度；稀疏与元数据全程双精度」——两个**可同时成立、各自独立**的归属；代码侧只有单值全局位 `static int g_aio_precision_mode_fp64 = 0;` 与 `aio_set_precision_mode(int is_fp64)`，头注自述「PrecisionContext 单例在 DLL 边界不共享 (EXE 和 DLL 各有一份副本)」。单个全局位**无法表达**「dense=f32 且 sparse=f64」这一要求组合 | `sed -n '33,42p' lib/infrastructure/aio/src/aio_api.cpp`；`docs/ACSD_DESIGN.md:162`–`:163` | 拆成两个独立精度量，或由负责人裁定该归属在实现层降为一个全局模式并同步订正 3.3。**登记 UNRESOLVED U-7** |
| 2-14 | `data/PHASE_PRODUCT_EXCHANGE.md:35`–`:37` + `:93` `[转]` | 三产品角色的「最小平面集」都含 `mask`，但无生产者写出 mask 平面；且 `mask` 在 13 个统一对象中**无 canonical schema** | `ls eng/contracts/schemas/unified/` → 13 个对象 schema + `port_contract.schema.json`，无 mask；子代理逐条对 `aio_hips.h` 产品面枚举、`p3_output.cpp` 的 `EXTNAME`、唯一写 `product_role` 的生产者，均未见 mask | 最小平面集按代码实际面重写；`mask` 要么补 canonical 对象、要么从 `plane_id` 枚举与最小平面集同时删除 |
| 2-15 | `data/PHASE_PRODUCT_EXCHANGE.md:69`、`:89` `[转]` | 合同要求 `coordinate.frame` 必须 `icrs`，而 HiPS 生产者写 `equatorial` | `grep -n 'hips_frame' lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` → 写 `equatorial`；validator `:208` 唯一允许 `icrs` | 合同显式写 `equatorial → icrs` 映射，或改 writer |
| 2-16 | `architecture/MODULE_MAP.md:15` `[转]` | noop 模块路径列写 `lib/infrastructure/scheduler`，实际源在 `eng/tests/conformance/noop/` | `grep -n 'conformance/noop' CMakeLists.txt` → `add_subdirectory(eng/tests/conformance/noop)`；`TRACEABILITY.md:187` 亦登记该路径 | 路径列改 `eng/tests/conformance/noop` |
| 2-17 | `architecture/MODULE_MAP.md:79` `[转]` | 「在役 registry = `p3_proj.h`/`p3_proj.cpp`」不成立：该目录 CMakeLists **只编译 `p3_wcs.cpp`** | `grep -rn 'p3_wcs.cpp\|p3_proj.cpp' --include=CMakeLists.txt lib/algorithms/projection/` → 仅 `p3_wcs.cpp` | 在役 registry 改 `p3_projection_registry.h`；`p3_proj.*` 移入非交付面 |
| 2-18 | `governance/DUAL_LINE.md:17`–`:19` `[转]` | 文件域**不互斥**：B 线 glob 与 shared 线 glob 同时含 `docs/engineering/**`，与本篇第 11 行自述的互斥目的直接冲突；且表头「B 域 19 篇 / shared 域 60 篇」与实际点名清单（1 条 + 4 条）不符，`:36` 已自改口「篇数待归属裁决后重算」 | `sed -n '15,22p' docs/engineering/governance/DUAL_LINE.md` | `docs/engineering/**` 整体归 shared；表头篇数按清单实数或删列 |

---

### 轮 3 · 公式（我逐条重推，给出推导）

| # | 位置 | 问题 | 我的重推 | 建议改法 |
|---|---|---|---|---|
| 3-1 | `standards/NUMERIC.md:50`–`:53` | **文档给出的预测式与其自报实测值不闭合**：先写「标度未声明时归一化权重最大相对偏差应为 `max_k (α_max/α_k)² − 1`」，再写「实测 **1.0336**」 | 由 `:49` 自报 `α ∈ [1.1387e-17, 6.0083e-17]`：`α_max/α_min = 6.0083/1.1387 = 5.2765`，平方 `= 27.841`，减 1 = **26.841**。文档式子的预测值是 **26.84**，与「实测 1.0336」相差约 26 倍。二者必有一错，或「实测」量的不是该式定义的量 | 明确「实测 1.0336」的定义量（疑似是 `max_k|w_k/⟨w⟩−1|` 而非 `max_k(α_max/α_k)²−1`），或改正公式；不得两者并列而不调和 |
| 3-2 | `standards/NUMERIC.md:72` | **量名错标**：写「33 个实测 **`α²`** ∈ [1.2966e-46, 3.6099e-45]」，但由同段 α 值算出 `α² ∈ [1.2966e-34, 3.6100e-33]`。文中那对数值恰等于 **`variance_floor · α²`**（`1e-12 × 1.2966e-34 = 1.2966e-46`；`1e-12 × 3.6100e-33 = 3.6100e-45`），不是 α² | 我的计算：`(1.1387e-17)^2 = 1.2966e-34`、`(6.0083e-17)^2 = 3.6100e-33`；乘 `variance_floor=1e-12` 后正是文中数值 | 量名改为 `variance_floor·α²`（换算后地板），并补一步乘法 |
| 3-3 | `standards/NUMERIC.md:73`–`:74` | 同一段内「其余为次正规数（**max 4.204e-45**）」与前句给的区间上界 `3.6099e-45` 不一致；且「32/33 精确下溢为 0」在 float32 下要求 ≤32 个样本落在次正规带 `[1.4013e-45, …]` 之下，文档未给分布 | 我的计算：float32 最小正规数 `1.1755e-38`（文档值正确）、最小次正规数 `2^-149 = 1.4013e-45`。若区间上界真是 `3.6099e-45`，则该区间 `[1.2966e-46, 3.6099e-45]` **跨越** 下溢边界，"32 个为 0、1 个为次正规数" 需要逐帧分布支撑，文档给不出 | 给出 33 帧的逐帧值或分位；或改为不依赖分布的定性结论 |
| 3-4 | `standards/NUMERIC.md:63` vs `governance/UNRESOLVED.md:63` `[转]` | `nside = 2^18` 被当作定值用于 21.63 dex 的量级结论，但同车道另一处记录生产实测为 `nside=65536`（=2^16） | 我的计算：`2^18` ⇒ `A_cell = 4π/(12·2^36) = 1.52387e-11 sr`、`1/A² = 4.3063e21`、`log10 = 21.634`（**与文档所写全部一致，算术无误**）；但 `2^16` ⇒ `A_cell = 2.4397e-10`、`1/A² = 1.68e19`、19.2 dex，差 2.4 dex | nside 是运行参数，不得以单一取值承载量级结论；改为「随 nside 变化的表达式 + 一个示例」 |
| 3-5 | `resources/PERFORMANCE_MODEL.md:71` vs `:80`/`:95` | **同一量两条公式**：代码块写 `I = max(1, L / min(n, F))`，结构结论与分类表写 `I = max(1, L / F)` | `L / min(n,F)` 与 `L / F` 只在 `F ≤ n` 时相等。若 `F > n`，前者给 `L/n`、后者给 `L/F`，**非单调「死区」结论只在 `F ≤ n` 下成立**，而文档未声明该前提 | 统一到 `architecture/DATA_FLOW.md` 的正本式（`in_flight = min(n, frame_workers)`，`inner_omp = max(1, thread_budget/in_flight)`），并在结构结论补 `F ≤ n` 前提 |
| 3-6 | `testing/TEST.md:43` | 归约容差式 `γ_n = n·u/(1−n·u)` 中的 `u`（单位舍入）**全文未定义**，读者无法取数 | 由 `:224` IEEE 754 可推 f64 `u = 2^-53 ≈ 1.11e-16`、f32 `u = 2^-24 ≈ 5.96e-8`，但文档未给 | 补 `u` 的定义与取值 |
| 3-7 | `UNIFIED_OBJECTS.md:98` | 权重效率损失 `E = Var_w/Var_opt − 1` 对整体乘性缩放相消 | **我重推并确认成立**：`w = 1/σ̂²`，若全部权重同乘常数 c，则加权均值 `Σw_i x_i / Σw_i` 不变，`Var_w` 不变；等价地若数据整体乘 c，则 `Var_w`、`Var_opt` 同乘 c²，`E` 不变。故 E 的尺度不变性推导正确 | 保留（此处无缺陷，登记为已验证项） |

---

### 轮 4 · 证据（容差与阈值是否有唯一来源）

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| 4-1 | 全车道 | **同一主题在多处各写一份、彼此不一致**（这是本车道最大的证据面缺陷） | 三条互斥来源，全部可复跑：<br>① **通用浮点容差**：本车道**无一份**含数值。`SCHEDULER.md:33` 声明「容差数值与可满足性下限的唯一正本 = 本文件『三阶段调度形态』一节」，但该节（`:20`–`:35`）**一个容差数值都没有**；`architecture/DATA_FLOW.md:90` 声明「正本在 `../testing/TEST.md` 的通用容差规则」，而 `TEST.md:36`–`:53` 的确有表。<br>② **内存安全系数 0.75**：`architecture/DATA_FLOW.md:121` 写字面量 `0.75`，`resources/PERFORMANCE_MODEL.md:49` 写具名常量 `kP1FrameMemSafetyFrac = 0.75`。<br>③ **模块级容差**：`TEST.md:41`–`:42` 定 `rtol=1e-12/5e-6`，`TEST.md:174`–`:183` 的映射表却用 `1e-6 / 1e-9 / 1e-8 像素 / 1e-6 像素`，未按 `TEST.md:49`–`:51` 自订的「必须同时声明适用量级域」给出域 | 取 `TEST.md` 为通用容差唯一正本，其余一律回引；`0.75` 只留具名常量一处 |
| 4-2 | `standards/NUMERIC.md:49`、`:71` | 两处关键实证锚点之一缺失：`run/RELEASE-05/vis/out/m42_p1_t3/p1_phot.json`（33 帧 α 的唯一来源）**不存在** | `ls run/RELEASE-05` → 无该目录（`run/` 下共 138 项，无 `RELEASE-05`）；另一锚 `run/M42-VARIANCE-RCA-01/plane_fixed.f64` **存在** | 补齐 33 帧 α 的可复算来源，否则 3-1/3-2/3-3 三条永久不可复核 |
| 4-3 | `standards/NUMERIC.md:63` | `nside = 2^18` 无来源标注（见 3-4） | 见 3-4 | 标注为示例取值或改写为参数式 |
| 4-4 | `resources/PERFORMANCE_MODEL.md:48` | `kP1FrameBytesPerPixel = 116.0` 是承载内存闸门的标定常数，**无推导、无拟合读数指针**（`:76` 只说「见 实验/…/l2_performance/」，未指名具体件） | 直接对读 `:48` 与 `:76`；该常数直接决定 `F = min(L, floor(0.75A/(P·B)))` | 补拟合件名与拟合口径（base + 边际字节/像素 × F 的实测系数） |
| 4-5 | `resources/PERFORMANCE_MODEL.md:84`、`:96` | 「帧内 stripe 数超过约 4 后进入收益递减区」是性能阈值，**无来源、无读数指针**，却被列为「物理/算法限制（调度参数不可改）」 | 直接对读 `:84`/`:96` | 给实测件；否则降为「待标定」，不得列入不可改类 |
| 4-6 | `standards/CODE.md:83` | 门 `CHK-NAMING-SURFACE` 自称「以 `git grep -w` 扫三族字面量……每一处命中必须落在某一保留类内」，但该门**全仓只有这一处文档提及，无任何实现**；且所列三族为「混写 `ACSD`、小写别名 `acsd`、全大写命名空间 `ACSD`」——前两族之外第三族与第一族**字面相同** | `grep -rln "CHK-NAMING-SURFACE" docs/ eng/ lib/` → 仅 `docs/engineering/standards/CODE.md` 自身 | 三族应写成「`ACSD` / `acsd` / `AstroCS` 族」；否则该门**在结构上无法检出它要检的违规** |
| 4-7 | `data/ARTIFACTS.md:110` | MAD→σ 一致化系数 `1.482602218505602 = 1/Φ⁻¹(3/4)` | **我逐位重算并确认完全正确**：`Φ⁻¹(0.75) = 0.6744897501960817`，`1/0.6744897501960817 = 1.482602218505602`，与文档**逐位相同**（`python3 -c "from statistics import NormalDist; q=NormalDist().inv_cdf(0.75); print(1/q)"`）。式子本身也正确（正态下 `MAD = σ·Φ⁻¹(0.75)` ⇒ `σ = MAD/Φ⁻¹(0.75)`）。来源 DOI `10.1080/01621459.1993.10476408`（Rousseeuw & Croux 1993, JASA 88:1273）**核对为真实且对应正确** | 保留（登记为已验证项） |
| 4-8 | `resources/PERFORMANCE_MODEL.md:139`–`:142` vs `eng/contracts/resource_gate_v1.json::compute` | L2 四条冻结判据的**唯一数值源声明成立且取值逐项一致** | `python3 -c "import json;print(json.load(open('eng/contracts/resource_gate_v1.json'))['compute'])"` → `mean_utilization_min_percent:85`、`p50_utilization_min_percent:90`、`per_sample_utilization_min_percent:85`、`per_sample_pass_fraction_min:0.7`、`queue_low_utilization_percent:60`、`queue_low_window_seconds_min:10`，与文档 `≥0.85 / ≥0.90 / ≥0.70 / <60% 且 ≥10s` 逐项一致 | 保留（登记为已验证项）。但注意 `resource_gate_v1.json` 内 `mean_utilization_enforcement = "record_and_justify"` 且注「85% 均值门在 16-worker 真负载上实测仅 65.09%，未标定前不得硬失败」，而 `PERFORMANCE_MODEL.md:144` 写「enforcement = fail-closed：任一判据违规 ⇒ `verdict=red`」——**文档与唯一数值源对同一字段的 enforcement 取值相反**，需裁决（见 8-6） |
| 4-9 | `resources/PERFORMANCE_MODEL.md:16` | 「UPM dense cache …… 最大相对偏差 ≤ 1e-12」是容差第三份副本（与 `TEST.md:41` 同值），但此处未声明它归属 `TEST.md` 哪一档 | 见 4-1 | 回引 `TEST.md` 并声明适用域 |

---

### 轮 5 · 引用（文献与交叉引用）

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| 5-1 | `contracts/CLI_PROTOCOL.md:10`、`:13`、`:16`、`:23`、`:30`、`:50`、`:124`（7 处） | 反复引最高设计的「**配置与 output_dir**」一节 / 「命令树」一节.2 / 「JSONL 运行事件流」一节.5 —— **ACSD_DESIGN.md 中这三个节名一个都不存在** | `grep -n "配置与 output_dir" docs/ACSD_DESIGN.md` → 0 命中；`grep -n "^#" docs/ACSD_DESIGN.md` → 命令树为 `### 7.1 命令树`（无子节），机器输出为 `### 7.2 机器输出与退出码`，JSONL 只是 7.2 内一句正文 | 全部改指真实节名（命令行合同 / 命令树 / 机器输出与退出码 / 错误传播与日志），并去掉 `.1/.2/.5` 机械编号 |
| 5-2 | `contracts/CLI_PROTOCOL.md:12`、`:92`；`contracts/MANIFEST_VERIFY.md:23`；`api/PUBLIC_API.md:2162`、`:2176`、`:2211`、`:2218` `[转]` | 引 `ARCH-002`/`ARCH-004`/`ARCH-005` 三个文档 ID，**仓内无任何文档带这三个 ID**（重写后的 ARCHITECTURE.md 无 ID 体系） | `grep -rn "ARCH-00[0-9]" docs/engineering/` → 命中全为上述引用本身与 `TRACEABILITY.md` 的 ID 列 | 删 ID 或改引真实节名 |
| 5-3 | `contracts/LOG_AND_ERROR.md:257` | 引最高设计「**机器判据**」一节 —— 该节名在 `docs/ACSD_DESIGN.md` 与 `docs/science/` 中均不存在 | `grep -rn "机器判据" docs/ACSD_DESIGN.md docs/science/` → 0 命中 | 改指真实落点（最高设计第 12 章） |
| 5-4 | `contracts/HIPS_STORAGE_FORM.md`（12 处） | **空章节锚 `「」`** —— 批量替换把节名吃光，句子留下空洞引用 | `grep -c "「」" docs/engineering/contracts/HIPS_STORAGE_FORM.md` → **12**；逐处：`:27`×2、`:140`、`:182`、`:205`×2、`:206`、`:234`、`:254`、`:259`、`:265`、`:266`、`:282` | 逐处补回真实节名（多半是同文件的「索引 schema」「哈希口径」「形态的输入配置与输出清单字段」三节） |
| 5-5 | `data/PHASE_PRODUCT_EXCHANGE.md`（10 处） | 同类失真：空主语与空槽位 | `grep -c` 逐串：`最高设计 ：`×3、`（最高设计 ：`×2、`最高设计 ，`×1、`（最高设计 ，`×1、`；：`×1、`（/ ）`×1、`该文件 （`×1、`该文件`×1。例：`:9`「最高设计 ：一次 CLI 调用只驱动一个阶段……；：跨阶段只交换磁盘产品」；`:167`「NaN 保留给「无覆盖」（/ ）」；`:208`「并与该文件 （`ivar==0` = …）一致」 | 补回被删的条款名 |
| 5-6 | `contracts/CONFIG.md`（多处） | **交叉引用的节名被替换成 schema 文件名**：`docs/science/calibration/CALIBRATION.md`「**`eng/contracts/schemas/run_manifest.schema.json`**」一节a；`docs/science/noise_snr/NOISE_MODEL`「`eng/packaging/config/filters.json`」一节；`AGENTS.md`「`eng/contracts/schemas/run_manifest.schema.json`」一节 | 直接对读 `:28`、`:36`、`:41`、`:47`、`:50`、`:68`、`AGENTS.md`（无此节）；另 `:41` 引 `../../detail/anchors/ANCHOR_CONTRACT.md`「…config_registry.json」一节，节名与文件名混写、且 `:36` 出现引号不配对的 `「旋钮与默认登记册 `eng/packaging/config/config_registry.json」一节`」 | 按目标文档真实节名逐处恢复；该批是机械替换产物，宜整段重写而非逐字补 |
| 5-7 | 全车道 | **机械跳转锚成为主流引用形态**：`「X」一节` 出现 **521 次** | `python3` 计数（见自证段命令）；分布：PUBLIC_API 73、CONFIG 66、VALIDATION_EVIDENCE 37、HIPS_STORAGE_FORM 33、ARTIFACTS 33、TRACEABILITY 32、PHASE_PRODUCT_EXCHANGE 28、LOG_AND_ERROR 20、CLI_PROTOCOL 17、ATOMIC_PUBLISH 16。与 `docs/engineering/README.md:28` 自订「不使用「见第几节」式的跳转锚」及 AGENTS §5 直接冲突 | 分批降为论文格式编号引用；先修 5-4/5-5/5-6 三类**失真**（纯破坏，优先） |
| 5-8 | 全车道 | **参考文献表与正文几乎完全脱钩** | 有参考文献节的 48 份中，**46 份正文零 `[n]` 引用**，文献表全为孤儿条目。复跑见自证段命令 | 要么按论文格式在正文相应断言处补 `[n]`，要么删除无引条目 |
| 5-9 | `contracts/CONFIG.md:319` | 正文出现 `**[18] [19]**`（PixInsight 官方式），超出本篇 5 条参考文献表 | `sed -n '318,320p'` 与 `:382`–`:391` 的 [1]–[5] 对读 | 补入文献表或改为内容锚引用 |
| 5-10 | `UNIFIED_OBJECTS.md:4`、`:9`、`:19`、`:30`、`:46`、`:50` | 引 `docs/detail/common/UNIFIED_MODEL` —— 实际文件是 `docs/detail/UNIFIED_MODEL.md`，**`docs/detail/common/` 目录不存在** | `ls -d docs/detail/common` → 无；`find docs -name "UNIFIED_MODEL*"` → `docs/detail/UNIFIED_MODEL.md` | 改路径 |
| 5-11 | `contracts/CONFIG.md:53`、`:70` | 引 `docs/science/noise_snr/PSF_SIGNAL_SNR` —— **不存在**；该目录只有 `NOISE_SNR.md` 与 `README.md` | `ls docs/science/noise_snr/` | 改指 `NOISE_SNR.md` |
| 5-12 | `data/ARTIFACTS.md:41`–`:60`（13 行）、`UNIFIED_OBJECTS.md:30` | 两篇把 `docs/detail/common/UNIFIED_MODEL` 的 13-对象节称作「**weight/value/scale/sigma/snr 歧义映射**」，而 `UNIFIED_OBJECTS.md:1`、`:9` 把同一节称作「**13 个对象 → canonical schema → schema ID**」。同一节在两篇有两个名字 | 直接对读 | 统一到一处 |
| 5-13 | `contracts/ATOMIC_PUBLISH.md:63`、`contracts/OWNERSHIP_LIFETIME.md:26` `[转]` | 路径写 `governance/TRACEABILITY.md`，从 `docs/engineering/contracts/` 解析为 `docs/engineering/contracts/governance/…`（不存在），应为 `../governance/…` | `ls docs/engineering/contracts/governance` → 无 | 补 `../` |
| 5-14 | `standards/CODE.md:7`–`:8`、`:12` | 「「双平台发行」一节（**双平台发行**）」节名与括注重复；「与 （每模块必备项）」与「`../../ACSD_DESIGN.md` （官方 Windows 工具链 = MSVC）」**节名为空** | 直接对读 `:7`–`:8`、`:12` | 补真实节名 |
| 5-15 | `api/abi/ABI.md:9`、`:16`、`:66` | 「同最高设计 **的** 模块与 ABI 条款」——节名为空 | 直接对读 | 补 |

---

### 轮 6 · 推理（论证链完整性）

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| 6-1 | `contracts/PIPELINE_BLOCK.md:17`–`:20` vs `:37`、`:60` | **本文件自相矛盾**：`:17` 说命名块「**不跨节点**」、`:19` 说「阶段内节点间交换的数据**不是**命名块，而是磁盘产品」；而 `:37` 的 `lifecycle` 枚举含 `frame`（帧内**跨节点**）、`:60` 说「随块流转，下游逐项透传」 | 直接对读；机器侧 `eng/contracts/schemas/pipeline_block.schema.json` 的 `title` 仍是「阶段内命名块元数据与生命周期」 | 若以 `:17` 为准，删 `frame` 档与跨节点条款；若以设计为准，见 8-1 |
| 6-2 | `architecture/DATA_FLOW.md:14` vs `:22`/`:33`/`:44` | 同类自相矛盾：`:14`「阶段内节点的交换面是 `output_dir` 文件约定，**不是命名块**」，但 `:22`/`:33`/`:44` 把中间量列为「阶段内**命名块**」 | 直接对读 | 同 6-1 |
| 6-3 | `contracts/SCHEDULER.md:33`、`:35` | **循环引用 + 指向空内容**：`:33` 把通用容差规则交给本篇「三阶段调度形态」一节并宣称该节是唯一正本，`:35` 把 NaN/Inf 语义也交给同一节；该节（`:20`–`:35`）**不含任何容差数值、也不含 NaN 语义**。同时 `CONFIG.md:91` 又把「精度不变性」的浮点容差判据交给 `SCHEDULER.md` | 直接对读 `:20`–`:35` | 统一到 `testing/TEST.md`，两处改为回引 |
| 6-4 | `contracts/CONFIG.md:263`、`:286` vs `:85` | 同一文件对 `weight_mode` 给出相反结论：`:85` 说「**不存在** `algorithm_weight_mode` / `weight_mode` 键」；拼入段 `:286` 的配置文法里却列着 `weight_mode(auto)` 为现行键，并说 `acr_route` 才是退役键 | 直接对读 | 明确区分两个配置面（phase_config vs orchestrator stage2），各自声明作用域 |
| 6-5 | `standards/CODE.md:70` vs `:118`–`:119` | 见 2-8：轮次标识面被同时要求「保留」与「删除」 | 同 2-8 | 同 2-8 |
| 6-6 | `resources/PERFORMANCE_MODEL.md:110` | 「`K = num_threads` 是达成满宽的**唯一最小取值**」以「定理」措辞给出，但推导只用了 `K ≥ inner_omp` 一个必要条件，**未证充分性**（`K = inner_omp` 是否真能满宽，取决于 `inner_omp` 是否由 `num_threads` 决定） | 我的推导：全文可证的只有「达满宽需 `K ≥ inner_omp`」，故**最小可行 `K` = `inner_omp`**；从 `K ≥ inner_omp` 推不出「`num_threads` 即 `inner_omp`」。代码侧 `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1663` 确为 `int kScratchPoolCap = num_threads;`，但 `num_threads` 与 `inner_omp` 的等同关系文档未证 | 降为「充分条件 + 代码锚」，或补 `num_threads == inner_omp` 的来源证明 |
| 6-7 | `resources/PERFORMANCE_MODEL.md:112`–`:114` | 「在标定形态（`I = 2`）下 `K = num_threads` 与 `K = 2` 等价」与随后的示例「`n = 2` 帧 ⇒ `F = 2, I = 8`」不在同一形态上，读者无法判断「标定形态」指哪一组 | 直接对读 | 补齐示例的参数（`n, L, F, I`） |
| 6-8 | `testing/TEST.md:131`–`:132` | 单步超时上界 `max(60 s, 3 × 最近一次实测墙钟)` —— 「最近一次实测墙钟」的**存放位置、记录者、更新时机全未定义**，该判据不可执行 | 直接对读 | 指明记录面（如运行结果件的 `各步墙钟` 字段，`TEST.md:157` 已列） |

---

### 轮 7 · 负向（该报警/该归零的场景有无处理与负例）

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| 7-1 | 全车道 | **51 条「用例正本 / 机器判据 / 回归锚 / 负例 / 门」指向不存在的文件**，即负例面整体失联 | 可复跑（见自证段）：`docs/engineering` 中 449 处仓库相对路径引用，51 条在磁盘上不存在；分布：`eng/tests/**` 36 条、`eng/tools/{config_consistency_check,docs_machine_consistency}.py` 3 条、`lib/**/tests/**` 10 条、`lib/infrastructure/aio/tests/**` 1 条、`run/RELEASE-05/vis/...` 1 条。git 侧确认：`git -c core.quotepath=false ls-files eng/tests \| wc -l` → 106，且**全部**在 `conformance/` 与 `validation/` 下，无一个被引用路径 | 逐条改指现存正本，或如实登记「判据无载体」；不得继续写成已生效 |
| 7-2 | `contracts/ATOMIC_PUBLISH.md:249`–`:259` `[转]` | 验收映射表 9 个测试类（`TestUniqueRunDirIsolation`、`TestAtomicPublishPipeline`、`TestNoOverwriteDefault`、`TestConcurrentRunsIsolated`、`TestInterruptNoCompleteMark`、`TestPathTraversalRejected`、`TestPermissionRejected`、`TestTreeHashRecomputable`、`TestFitsVerifyCrossOracle`）全仓无实现 | 子代理 178e0080 逐名 `grep -rl`，`lib/`+`eng/` 全 NOT FOUND | 同 7-1 |
| 7-3 | `contracts/ATOMIC_PUBLISH.md:166` `[转]` | 「能区分两序的判据」依赖 LD_PRELOAD 注入器 `ACSD_TEST_CORRUPT_AFTER_RENAME=1`，仓内无实现 | `grep -rn ACSD_TEST_CORRUPT_AFTER_RENAME lib/ eng/` → 0 | 标「判据未接线」 |
| 7-4 | `contracts/LOG_AND_ERROR.md:203`–`:218` | 硬错误类别表十类中，`INPUT_CORRUPT` 与 `DEPENDENCY` **在 `lib/` 与 `eng/` 中零命中** | `grep` 工具在 `lib/` 与 `eng/` 上分别搜两串 → 均 No matches | 二者要么补实现，要么标为未启用类 |
| 7-5 | `contracts/LOG_AND_ERROR.md:62`–`:77` `[转]` | `log_artifacts[]` 标「每次运行必填」并绑定 `exit 8`，但代码零产出该键 | `grep -rn "log_artifacts" lib/ eng/` → 0 命中（我与子代理各查一次，同结论） | 删该节或补实现 + 负例 |
| 7-6 | `standards/CODE.md:83` | 门 `CHK-NAMING-SURFACE` 无实现（见 4-6）；其自订的「每一处命中必须落在某一保留类内，落在类外判红」**在无执行体时恒绿** | `grep -rln "CHK-NAMING-SURFACE" docs/ eng/ lib/` → 仅自身 | 同 4-6 |
| 7-7 | `governance/TRACEABILITY.md:142` `[转]` | 判据明写「门转全量强制（任何 error 即 rc=1）……负例注入（`--fault-inject`）」，但该机制全仓无执行器 | 子代理 6c924167：`grep -rn -- "--fault-inject" docs/ eng/ lib/` 仅 2 处提及 | 同 7-1 |
| 7-8 | `contracts/HIPS_STORAGE_FORM.md:210`、`:256` `[转]` | F0/M2 要求「键缺省/留空 ⇒ 必须发一条点名该键的 warn 事件」，但 `storage_form` 生产写出侧尚未消费该键，无 warn 发出点 | 子代理 178e0080 核 `lib/infrastructure/cli/parser.cpp`（自述「生产零读取」） | 标「判据待接线」 |

---

### 轮 8 · 一致性（以代码为准，文档声称的合同 vs 生产代码实际行为）

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| 8-1 | `contracts/PIPELINE_BLOCK.md:17`–`:22`、`architecture/DATA_FLOW.md:14`、`architecture/ARCHITECTURE.md:29`（组件图「类型化端口 / 命名块」）、`AGENTS.md:88`、`docs/ACSD_DESIGN.md:381`–`:383` | **本车道 + 代码一致地与总章程和最高设计相反**：工程正本与注册表称「节点间不存在内存块传递，命名块只在单节点执行期」，而 AGENTS §7 与最高设计 8.2 明确规定「模块经内存管线读块、写新块、消耗旧块」「块被其全部消费者用完后由管线显式销毁」 | 机器侧证据支持工程正本：`python3 -c "import json;d=json.load(open('lib/infrastructure/pipeline/module_ports.registry.json'));print(d['carrier_contract']['statement'][:120])"` → 「**节点间不存在内存块传递**……命名块（PipelineFrame 的 data/variance）只存在于单节点执行期的内存中」；`eng/contracts/schemas/pipeline_block.schema.json` 的 `title` 仍为「阶段内命名块元数据与生命周期」。**但权威链上 AGENTS.md 与最高设计两处都说块跨节点**（AGENTS.md:88、ACSD_DESIGN.md:381） | **需负责人裁决**：或改最高设计为文件约定，或改工程正本与注册表为内存块。登记 UNRESOLVED（见 §7） |
| 8-2 | `contracts/MANIFEST_VERIFY.md:25`–`:32` + `:37`–`:59` vs `eng/contracts/schemas/run_manifest.schema.json` vs `lib/infrastructure/cli/commands.cpp:530` | **`run_manifest` 三方不相交**：<br>① schema：`required = ['manifest_schema','run_id','software_sha','config_hash','manifest_input_hashes','manifest_output_hashes','toolchain_version','created_utc']`，且 `additionalProperties: false`；<br>② 生产代码 `write_run_manifest`（`commands.cpp:535` 起）写 `schema_version/kind/run_id/acsd_version/platform/config_path/config_sha256/cpu_profile_path/cpu_profile_sha256/phases/…`；<br>③ `MANIFEST_VERIFY.md` 如实镜像 **代码**，而 `contracts/CONFIG.md:146` 描述的却是 **schema**。<br>⇒ 代码写出的每一个字段（除 `run_id`）都会被自己的冻结 schema 拒；工程正本内部对同一对象有两套互斥词表 | 复跑（见自证段）。按 AGENTS §4「代码与文档冲突时以文档为准订正代码」——但此处两份文档互斥，须先裁决哪份是 `run_manifest` 正本 |
| 8-3 | `architecture/MODULE_MAP.md:62` | 节点序与注册表、C6 判据冲突（详见 1-11） | 见 1-11 | 同 1-11 |
| 8-4 | `architecture/MODULE_MAP.md:64`/`:92` | `noise_snr` 在构建图中的登记与实际不符（详见 2-6） | 见 2-6 | 同 2-6 |
| 8-5 | `lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h:145`–`:156` `[转]` | **仓内存在第二套进程退出码数值表** `AstroCsExitCode`，与 `acsd::ExitCode` 逐码语义冲突（该头注释自述「TIMEOUT=9/CANCELLED=10」，即把 9/10 对调；10=CANCELLED vs RESOURCE）；且 `is_process_exit_code()` 把 **1** 也算合法进程码，而 `acsd::ExitCode` 无 1 | `sed -n '145,170p' …/orchestrator.h` 与 `cat lib/infrastructure/cli/exit_codes.h` 对读。`contracts/LOG_AND_ERROR.md:275`–`:279` 的适用边界节只登记了 `acsd-stage2` 一个冲突面，未登记本表 | 在 `LOG_AND_ERROR.md` 的适用边界节补登，或退役该枚举 |
| 8-6 | `resources/PERFORMANCE_MODEL.md:144` vs `eng/contracts/resource_gate_v1.json::compute.mean_utilization_enforcement` | 文档写「enforcement = **fail-closed**：任一判据违规 ⇒ `verdict=red`」，唯一数值源写 `mean_utilization_enforcement = "record_and_justify"`，且同源 `enforcement_note` 明写「85% 均值门在 16-worker 真负载上实测仅 65.09%，**未标定前不得硬失败**」 | 复跑（见自证段） | 二者对同一字段给出相反 enforcement，须裁决；文档不得单方面升级为 fail-closed |
| 8-7 | `contracts/ATOMIC_PUBLISH.md:209`–`:211` vs `contracts/HIPS_STORAGE_FORM.md:135` vs `lib/infrastructure/aio/io/hips_output_store.py:103`–`:123` | `tree_hash` 定义详略不一致：ATOMIC 只说「sha256(规范 JSON 序列化的 tree 条目数组)」，而 HIPS_STORAGE 与实现都规定**条目归一为三元组数组、按 `(path,size,sha256)` 排序、`ensure_ascii=false`、`separators=(",",":")`**。按 ATOMIC 的字面读法（对 manifest 里的对象数组直接散列）会得到**不同的哈希** | `sed -n '103,123p' lib/infrastructure/aio/io/hips_output_store.py` → `norm.append((rel,int(size),sha))`；`norm.sort(key=lambda t:(t[0],t[1],t[2]))`；`json.dumps(norm, ensure_ascii=False, separators=(",",":"))`。**我据此判定 HIPS_STORAGE_FORM.md 与实现正确，ATOMIC_PUBLISH.md 欠定义** | ATOMIC_PUBLISH 补全归一规则，或直接回引 HIPS_STORAGE_FORM「哈希口径」 |
| 8-8 | `contracts/SCHEDULER.md:61` vs `eng/contracts/schemas/scheduler_probe_event.schema.json` | 探针事件「映射表」称「`stage`/`node`/`block`/`worker` 作为 `tags` 的等价展开」，但同篇 `:47`–`:59` 的字段表把它们列为**顶层字段**，且 schema 亦为顶层属性（`tags` 在该 schema 中不存在） | `cat eng/contracts/schemas/scheduler_probe_event.schema.json` → properties 为 `ts/stage/kind/node/block/bytes/value/unit/frame_id/window_id/worker`，无 `tags` | 删「tags 等价展开」表述，改述为「独立工件、独立 schema」 |
| 8-9 | `api/abi/ABI.md:5` vs `lib/include/acsd/abi/status_codes.h` `[转]` | 声称「**单一头** `lib/include/acsd/common_abi_v1.h`」，实际存在两套并行 C ABI 头，且 `status_codes.h` 明文规定二者不得在同一 TU 混合 include | 子代理 178e0080：`sed -n '24,32p' lib/include/acsd/abi/status_codes.h` | ABI.md 补一节说明两套头与收编路径 |
| 8-10 | `architecture/ARCHITECTURE.md:123` vs `docs/ACSD_DESIGN.md:466`–`:479` | 平台角色冲突（详见 2-1） | 同 2-1 | 同 2-1 |
| 8-11 | `docs/engineering/build/BUILD_GRAPH.md:13`–`:56` `[转]` | 生产构建图表 31 行 vs 实际闭包 34：漏 `acsd_gaia_zlib_include`/`acsd_platform_math`/`acsd_platform_zlib`，且 `acsd_hips`（11 源 vs 实 12）与 `acsd_phase2`（9 源 vs 实 8）两行手数与指纹失配；文内自订的「双向差集全部一致」判据**当前为红** | 子代理 6c924167 跑 `eng/tools/arch/cmake_graph.py` 比对（脚本可复跑，见 §6） | 由生成器重导机器块，勿手改 |
| 8-12 | `docs/engineering/build/RELEASE.md:87`–`:99` `[转]` | 交付产物名与打包器实际输出四点不符（`acsd-linux-<sha>.tar.gz` vs 实际 `ACSD-Linux-amd64-<VERSION>.tar.zst`） | 子代理 6c924167 核 `eng/tools/make_linux_release.py:5,58`、`make_windows_release.py:5,165` | 按打包器实际输出改写 |
| 8-13 | `architecture/MODULE_MAP.md:31`–`:56` vs `ACSD_TEST_CORRUPT_AFTER_RENAME` 等 | 该篇把「已知缺口」与「已登记事实」混写（`:52`–`:55` 「运行期宿主接线尚未闭合」「自述平台运行时与 I/O 平台单元尚未落实现」），属状态声明而非登记 | 直接对读 | 状态归 `governance/UNRESOLVED.md`，本篇只留映射 |

---

### 轮 9 · 语言（正向书写、无历史叙事/版本号/机械锚）

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| 9-1 | `resources/PERFORMANCE_MODEL.md:188`–`:219` | **整段历史叙事**：`:188`「原引 `evidence/performance/*.json`（V14 交付）**在本仓不存在**（死指针，**本行原句已订正**）」；`:190`「V14 首轮结果」；`:207`「## V18R2 资源驱动轮（性能基线）」；`:209`「本节数值为**单次读数**……只作历史参照」。AGENTS §5 明禁「日期、版本号、任务流水编号、commit、『旧版/作废/曾/原』等历史叙事」 | 直接对读 | 整段删除；现行基线须重跑后按结构写入，历史读数移入实验域 |
| 9-2 | 同上 `:188` | 同一行内嵌**源码行号** `(I/O 与原子产品)（:151）`，且节名与括注重复 `「I/O 与原子产品」一节（I/O 与原子产品）` | AGENTS §5「源码行号……放下级文档」 | 删行号与重复括注 |
| 9-3 | `contracts/MANIFEST_VERIFY.md:70` | 正文标题写「加性顶层键 `storage`（运行级形态事实；**R-42/P-181**）」——任务流水编号入正文 | AGENTS §5 | 删流水号 |
| 9-4 | `contracts/HIPS_STORAGE_FORM.md:268` `[转]` | 正文写「**同名异型消歧（P-180）**」——流水编号入正文 | 同上 | 删 |
| 9-5 | `contracts/CONFIG.md:331`、`:284`、`:286`、`:321` | 「**M3**：原 `N≥16` linear fit 档改投」「（low/high/… **不是现行键**……）」「`acr_route` **不是现行键**（**已从** parser 删除）」「**V17**：旧顶层 … **已从** parser 删除」——含版本代次 V17 与「原/已删」历史叙事 | AGENTS §5 | 改写为纯现行口径：只说「现行键集合为 X，出现 Y 判错」 |
| 9-6 | `contracts/MANIFEST_VERIFY.md:86` | 「verify* 为**已删**别名 → rc=2」 | AGENTS §5「『旧版/作废/曾/原』等历史叙事」 | 改写为「命令面不含 `verify*`；调用返回 rc=2」 |
| 9-7 | `contracts/CLI_PROTOCOL.md:10`–`:16` | 文档开头正文前有元信息块 `> ID: API-CLI-001 状态: FROZEN 上游: … 下游: …`，且 `FROZEN` **不在最高设计 12.5 的状态阶梯词表**内（CONTRACT_READY/IMPLEMENTED/INSTALLED/VERIFIED/READY_FOR_OWNER_REVIEW/NOT_*/DEFERRED/DORMANT/FAIL） | 直接对读；`docs/ACSD_DESIGN.md:536`–`:546`；`architecture/MODULE_MAP.md:7` 声明状态阶梯以最高设计为唯一口径 | 移入 `governance/TRACEABILITY.md`；`FROZEN` 换成阶梯内词 |
| 9-8 | `standards/CODE.md:90` | 正文写工具链版本号「MSYS2 MinGW64 g++ **16.1.0**」 | AGENTS §5 | 版本归 `DEPENDENCY.md`/锁文件 |
| 9-9 | `standards/CODE.md:76` | 机器契约第 9 类给出 HTTP token 示例 `ACSD-BASS-Index/1.0`；`BASS` 在 ACSD 语境中另有所指（`lib/infrastructure/aio/memory.md:29` 的 BASS DR3 星表数据），该 token 是产品名还是数据来源名未澄清 | `grep -rn "BASS" docs/ lib/ eng/` | 澄清语义或换成真实 token |
| 9-10 | `governance/UNRESOLVED.md:41` `[转]` | 正文含编码损坏字符 `稀疏控制点权**??**卷`，句子不通 | 子代理 6c924167 | 按上下文补回 |
| 9-11 | `governance/TRACEABILITY.md:266`、`:321`–`:327` `[转]` | 正本表格单元内嵌 HTML 订正注记并带行锚 `<!-- 订正: … authority 正本 PHASE2_UPM.md:23 … -->`；`:321`–`:327` 七行标题带「（**V19R3**）」版本代次 | 子代理 6c924167；AGENTS §5 | 整格按现行口径重写 |
| 9-12 | `api/abi/ABI.md:78`–`:79`、`contracts/PIPELINE_BLOCK.md:78`、`architecture/MODULE_MAP.md:97` | 句子因替换残缺：`三项分别由 A / B / C 与 D 定义`（3 项对 4 名）、「引用的节名不存在」 | 见 1-9 | 重写 |

---

### 轮 10 · 可复现

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| 10-1 | 全车道 | **51 条判据/用例/工具的路径不存在**（同 7-1） | 见自证段命令 | 逐条改指或登记 |
| 10-2 | `contracts/CONFIG.md:151`–`:157` | **文档给出的复跑命令不可执行**：`python3 -m unittest discover -s eng/tests/config -t eng/tests/config` 与两条 `python3 eng/tests/config/run_validation.py …` 的目标目录均不存在 | `ls eng/tests` → 仅 `conformance/`、`validation/` | 换现存门或标「门已撤」 |
| 10-3 | `UNIFIED_OBJECTS.md:21` | 自订机器可复跑断言 `python3 -m unittest discover -s eng/tests/contracts -t eng/tests/contracts` 目标目录不存在；其列出的 4 条测试方法全部无载体 | `ls eng/tests` 同上 | 同 10-2 |
| 10-4 | `resources/PERFORMANCE_MODEL.md:133` | 声明「阈值唯一数值源 = `eng/contracts/resource_gate_v1.json`」——**这一条成立且可核**（见 4-8），但同篇的复算/回放判据（`L2-FROZEN-GATE-REPLAY`、`WORKER-BALANCE-METRIC-REPLAY`）无执行器 `[转]` | 子代理 6c924167 | 补执行器或标人读 |
| 10-5 | `docs/engineering/build/BUILD_GRAPH.md:104`–`:110` `[转]` | 复算链三重失效：生成器 `DOC_REL` 仍写 `docs/engineering/BUILD_GRAPH.md`（不存在）；`python3 eng/tools/arch/gen_build_graph_doc.py --out /tmp/bg.md` **返回 1**（fail-closed 在第一个非生产登记项即非零退出）；输入件因 1-1 不在 git | 子代理 6c924167，三条命令均可复跑 | 按 1-1 → 8-11 顺序修后重导 |
| 10-6 | `testing/TEST.md:131`–`:132` | 「最近一次实测墙钟」无存放面，判据不可执行（见 6-8） | 同 6-8 | 指明记录面 |
| 10-7 | `resources/PERFORMANCE_MODEL.md:116`–`:121` | 回归锁 `p1drz_merge_pipeline_lock`（`.canon`/`.norm.hiss` 逐 leaf sha256 对照）无实现载体 `[转]` | 子代理 6c924167 | 补或标 |
| 10-8 | `data/ARTIFACTS.md:70` | 证据锚 `eng/tests/unit/core_pipeline_test.cpp` 不存在（并被用来支撑「`DATA-HIPS-001`/`DATA-TILE-001` 已 VERIFIED」的结论） | 见 7-1 | 同 10-1；该 VERIFIED 结论须重估 |

---

## 3. 本轮是否零发现

**十轮全部有新增实质问题，本轮不是零发现轮。**

| 轮 | 是否有新增实质问题 | 条数（本车道确认） |
|---|---|---|
| 1 结构 | **是** | 14 |
| 2 口径 | **是** | 18 |
| 3 公式 | **是** | 7（含 1 条已验证为正确的推导） |
| 4 证据 | **是** | 9（含 3 条已验证来源正确的常数） |
| 5 引用 | **是** | 15 |
| 6 推理 | **是** | 8 |
| 7 负向 | **是** | 8 |
| 8 一致性 | **是** | 13 |
| 9 语言 | **是** | 12 |
| 10 可复现 | **是** | 8 |

⇒ **收敛判据（规范 03：「连续两轮无新增实质问题」）在本轮未达成，也未接近达成。** 十轮均须继续。

### 3.1 本车道已被我**否决**的疑点（查过发现文档与代码一致）

| # | 疑点 | 核查方式与结论 |
|---|---|---|
| V-1 | 退出码 11 码值是否与代码一致 | `cat lib/infrastructure/cli/exit_codes.h` → `OK=0/ARGS=2/INPUT=3/SCIENCE=4/BACKEND=5/COMPUTE=6/IO=7/INTEGRITY=8/CANCELLED=9/RESOURCE=10/INTERNAL=70`，与 `CLI_PROTOCOL.md:39`–`:42`、`LOG_AND_ERROR.md:259`–`:271` **逐码逐值一致**。**否决** |
| V-2 | `ErrorDomain` 枚举是否与文档一致 | `lib/include/acsd/core/contracts.h:17`–`:26` 八域 `CONFIG/DATA/SCIENCE_PRECONDITION/IO/RESOURCE/BACKEND/CANCELLED/INTERNAL`，与 `LOG_AND_ERROR.md:222`–`:224` **逐名一致**。**否决** |
| V-3 | `HEALPix` 像素立体角与 dex 换算是否算对 | `python3` 重算：`4π/(12·2^36) = 1.5238730e-11 sr`（文档 1.5239e-11 ✓）、`1/A² = 4.3063e21`（✓）、`log10 = 21.6341`（文档 21.63 ✓）。**算术无误，否决**（但见 3-4 关于 nside 取值） |
| V-4 | MAD→σ 常数是否算错 | `python3` 重算 `1/Φ⁻¹(0.75) = 1.482602218505602`，与 `ARTIFACTS.md:110` **逐位相同**；DOI `10.1080/01621459.1993.10476408` 核对为 Rousseeuw & Croux 1993 JASA 88:1273 的真实 DOI，对应正确。**否决** |
| V-5 | `variance_floor = 1e-12 ADU²` 是否无源 | `eng/packaging/config/defaults.json` 中 `noise.variance_floor` = `1e-12`、unit `ADU^2`、`authority_status: sourced`，`source_ref` 指向 `docs/science/noise_snr/NOISE_SNR.md` §2 并带内容指纹。**有唯一来源，否决** |
| V-6 | 注册表 carrier 取值是否与文档一致 | `python3` 统计注册表 `carrier` 取值 = `{config_path, output_dir_file, hips_product_tree}`，与 `PIPELINE_BLOCK.md:21` 三值声明**一致**；`registry_version = 2` 与 `:9` 一致。**否决** |
| V-7 | 命名块字段是否与 schema 一致 | `eng/contracts/schemas/pipeline_block.schema.json` 的 required 8 项与 dtype/lifecycle 枚举、`name` 正则 `^[a-z][a-z0-9_]*$`，与 `PIPELINE_BLOCK.md:30`–`:37` **逐项一致**（仅 `provenance` 未 required，文档将其并列为「冻结字段」但未标必填，属**表述**问题非口径错误）。**否决** |
| V-8 | `E = Var_w/Var_opt − 1` 的尺度不变性是否成立 | 我的推导：`w = 1/σ̂²` 同乘 c 时加权均值不变 ⇒ `Var_w` 不变；数据同乘 c 时分子分母同乘 c² ⇒ E 不变。**成立，否决** |
| V-9 | `acsd::ExitCode` 是否有第二份数值表 | `lib/include/acsd/core/contracts.h` 的第二个枚举在 `namespace acsd::core`，与 `namespace acsd` 不构成 C++ 重定义，注释亦自述「只做映射表，不重定义数值」。**否决**（但翻出 8-5 的 `AstroCsExitCode`，那是另一处） |
| V-10 | `1 ulp` 可满足性下限是否被自身容差违反 | `TEST.md:41` `atol = 1e-13·scale`，f64 `ulp(scale) ≈ scale·2.22e-16` ⇒ `1e-13 > 2.22e-16` ✓；`:42` `atol = 1e-6·scale`，f32 `ulp ≈ scale·1.19e-7` ⇒ `1e-6 > 1.19e-7` ✓。**均满足，否决** |
| V-11 | zstd 默认档位是否写错 | `HIPS_STORAGE_FORM.md:68`「默认 zstd level 3」与 zstd 库默认压缩级一致。**否决** |
| V-12 | ACR 是否仍是工程正本的活跃机制 | `git -c core.quotepath=false ls-files \| grep -i acr` → `lib/**` 与 `eng/**` **零命中**（仅 `lib/algorithms/photometry/cpp/include/log_macros.h` 因含子串误配）；ACR 源集已退场。**但** `docs/engineering` 仍有 6 处引用 ACR（`RUNTIME.md:13`、`BUILD_GRAPH.md:101`、`RELEASE.md:18/81/119`、`DEPENDENCY.md:12`、`PUBLIC_API.md` 多处）且最高设计从未定义该词 ⇒ **否决「ACR 在役」，但保留为引用轮问题（9-x / 8-x）** |

---

## 4. 阻断级根因（先修这 4 条，其余多数是下游症状）

| # | 根因 | 影响面 | 可复跑判据 |
|---|---|---|---|
| **R1** | **构建正本目录不在版本控制内**（1-1） | `build/` 4 份正本对 git 隐身 ⇒ 索引/文件树双向比对恒绿、BUILD_GRAPH 复算链在干净克隆上不存在 | `git -c core.quotepath=false ls-files docs/engineering/build \| wc -l` → `0`；`git check-ignore -v docs/engineering/build/BUILD_GRAPH.md` → `.gitignore:20:build/` |
| **R2** | **判据载体被批量删除、正本未同步**（7-1、10-1）：`eng/tests/**` 只剩 `conformance/` 与 `validation/`，`eng/tools/` 的 `config_consistency_check.py` / `docs_machine_consistency.py` / `check_cfg002_registry.py` 不存在 | 本车道 51 条判据、用例、负例、门全部失联；`CONFIG.md` 的 CFG002-01..12 十二道门无执行器；`TEST.md`、`UNIFIED_OBJECTS.md`、`ATOMIC_PUBLISH.md`、`RUNTIME.md`、`ASYNC_IO.md` 的复跑命令不可执行 | 见自证段脚本；`git -c core.quotepath=false ls-files eng/tests \| wc -l` → `106`（全在 `conformance/`、`validation/`） |
| **R3** | **多篇文档被拼接 / 节名被批量替换**（1-2、1-3、1-5、5-4、5-5、5-6、1-4） | `LOG_AND_ERROR.md` 两篇合一、`CONFIG.md` 三篇合一、`CODE.md` 代码+注释合一、`PERFORMANCE_MODEL.md` 自指；`HIPS_STORAGE_FORM.md` 12 处空锚、`PHASE_PRODUCT_EXCHANGE.md` 10 处空槽、`CONFIG.md` 交叉引用节名被替换成 schema 文件名 | 见 1-2/1-3 的自引证据；`grep -c "「」" docs/engineering/contracts/HIPS_STORAGE_FORM.md` → `12` |
| **R4** | **内存块载体口径与权威链上层相反**（8-1） | `AGENTS.md:88` 与 `ACSD_DESIGN.md:381` 说块跨节点；`PIPELINE_BLOCK.md:17`、`DATA_FLOW.md:14`、注册表 `carrier_contract` 说块不跨节点。这不是文档笔误，是**顶层设计与工程实现未对齐** | `python3 -c "import json;print(json.load(open('lib/infrastructure/pipeline/module_ports.registry.json'))['carrier_contract']['statement'][:60])"` vs `docs/ACSD_DESIGN.md:381` |

---

## 5. 推翻的既有结论

1. **推翻「本车道有三套退出码数值表」——只推翻到一半。**
   我先据 `CLI_PROTOCOL.md:39`、`LOG_AND_ERROR.md:259`、`exit_codes.h` 判「文档声明三份、代码一份」，复核后**推翻**文档侧重复的判断：两份文档表**逐码逐值一致于代码**，且 CLI_PROTOCOL 明确声明「唯一源是 `exit_codes.h`」，符合单一正本纪律（见 V-1）。
   **保留的部分**：`orchestrator.h:145` 的 `AstroCsExitCode` 是真正的第二套（8-5），其 9/10 语义与 `acsd::ExitCode` 对调，且 `LOG_AND_ERROR.md` 的适用边界节未登记它。

2. **推翻「`ARTIFACTS.md`/`UNIFIED_OBJECTS.md` 的 13-对象节名冲突是我的读错」。**
   初读以为两篇用不同的上位节名是合理的（各自指向自己的章节）。复核后**推翻**：两篇指的是 `docs/detail` 同一份 UNIFIED_MODEL 的同一节，却给了两个不同的节名（5-12），且该路径 `docs/detail/common/UNIFIED_MODEL` 本身不存在（5-10）。

3. **推翻我自己的初判：`NUMERIC.md:72` 的 α² 数值是「12 个数量级的算术错误」。**
   我先按 `α ∈ [1.1387e-17, 6.0083e-17]` 直接平方，得到 `[1.2966e-34, 3.6100e-33]`，与文中 `[1.2966e-46, 3.6099e-45]` 相差 1e-12，判为指数笔误。**重推后推翻**：那两个数恰等于 `variance_floor(1e-12) × α²`，即**换算后的地板**。故真实缺陷是**量名错标**（把 `floor·α²` 写成 `α²`），不是算错（3-2）。这个自我纠正也说明：该段数字与公式的关系必须由作者显式写一步乘法。

4. **推翻「`ATOMIC_PUBLISH.md` 与 `HIPS_STORAGE_FORM.md` 的 `tree_hash` 定义互相矛盾」。**
   我先据两处文字差异判为互相矛盾。**重推后推翻**：读实现 `hips_output_store.py:103`–`:123` 后确认，HIPS_STORAGE_FORM 的三元组数组 + 排序 + 规范序列化规则**与代码逐项一致**；ATOMIC_PUBLISH 只是**欠定义**（8-7），不是矛盾。整改动作从「二选一」收窄为「补归一规则」。

5. **接受子代理 178e0080 的一处自我推翻，提升为我的结论。**
   它先判「`MANIFEST_VERIFY.md` 的 run_manifest 词表抄错了 schema」，复核 `commands.cpp:530`–`:558` 后**推翻**为「文档忠实镜像生产代码，错在代码 + 文档同时背离冻结 schema」。我独立复核三方（schema required 集、代码字面量、CONFIG.md 描述）后**确认该修正成立**，并据此把根因定为「文档内部两套互斥词表 + 代码违反 schema」三方问题（8-2），而非文档笔误。

6. **推翻「本车道存在大量「恒真门」。」**
   子代理多处报告「判据无执行器恒绿」。我复核后**部分保留、部分收窄**：确有无执行器的事实（`CHK-NAMING-SURFACE`、CFG002-01..12、`--fault-inject`、BUILD_GRAPH 生成器），但这属于**门已退役而文档未同步**（R2），不是「门写成恒真」——文档并未声称它们恒绿。因此整改定性从「恒真门缺陷」改为「复算链断裂」，前者是设计缺陷，后者是同步缺陷。

---

## 6. 子代理分头审与逐条复核

派出 6 个子代理（contracts+api ×2、architecture+data ×2、standards+resources+testing、build+governance）。**我不采信任何子代理结论，全部自行复核**：

| 处置 | 条数 | 说明 |
|---|---|---|
| **采纳**（我亲自复跑命令或对读原文确认） | **55** | 含 4 条根因级：`docs/engineering/build/` 被 `.gitignore` 排除、`LOG_AND_ERROR.md` 双文档拼接、`PIPELINE_BLOCK.md` 双 C 编号冲突、`run_manifest` 三方不相交；另含 `AstroCsExitCode` 第二套退出码、`UNRESOLVED.md` 双 `## 5`、「参考文献表与正文脱钩 46/48」、「521 处机械锚」——后两条我**独立复跑脚本得到同样数字**；子代理 395115d5 交付后我又**亲自 `sed` 复核**了 UPM 权重公式、integrate 权重退役、精度全局开关三条 |
| **自行否决/改写** | **9** | ① 「仓内没有打包脚本」——子代理自己已推翻，我确认打包器在 `eng/tools/` 而非 `eng/packaging/`，保留其改写后的 8-11；② 「UNRESOLVED.md 是重复正本」——降级为「无优先级声明」；③ 「MODULE_WITHOUT_PAGE 上限是历史残留」——撤回；④ 「contracts.h 第二枚举是重复定义」——我复核后否决（见 V-9）；⑤ 「HIPS_STORAGE_FORM 像素角尺度公式量纲错」——子代理已推翻，我复核推导（Ω = π/(3nside²) ⇒ √Ω·(180/π) = deg/px）确认公式正确；⑥ 「未编译未运行」的自陈——我接受为诚实边界，不作为缺陷计入 |
| **未核转登记 `[转]`** | **14** | 全部集中在 `governance/**` 与 `build/**`（我未逐行读完的 36 份）：BUILD_GRAPH 生产闭包 34 vs 31（8-11）、RELEASE 产物名四点不符（8-12）、BUILD_NODES `-G Ninja` 与 preset 合同冲突、Traceability 模块数 22/23/25/26/30、Traceability 8 层 vs 9 层自相矛盾、Traceability `SCI-P1-PSF-001`/`SCI-PSF-001` 双 ID、Traceability TEST `VERIFIED` 与 `@ MISSING` 互斥、UNRESOLVED 章节条数 18 vs 22、UNRESOLVED 乱码字符、UNRESOLVED 与审核包 UNRESOLVED 双编号空间无优先级、RELEASE `gen_version` 未接线、RELEASE 版本扫描面 4/6 路径解析不到、DOCUMENT_GOVERNANCE 目录树失真、DOCUMENT_GOVERNANCE 三处自引归属错位；另含子代理 395115d5 报而我只做了存在性核对的：mask 平面无生产者（2-14）、`coordinate.frame` icrs/equatorial（2-15）、noop 路径（2-16）、`p3_proj` 在役性（2-17）、DUAL_LINE 文件域重叠（2-18） |

**子代理 395115d5 的四条自我推翻，我采纳其修正后的版本**：它先判「drizzle 方差式量纲错」「稀疏 SNR 默认 f32 违反精度归属」「`mask` 问题意味着交换合同整体不可用」三条，随后自行重推并撤回（分别为：量纲**成立**、证据面**错**（属 hiss 容器非 HiPS 落盘面）、范围**应收窄**）。我对其中第一条独立重算确认（见 V-3/V-8 同法），对第二、三条接受其撤回结论——**不把撤回的三条计入本审稿的问题数**。

**要求「写明否决了哪些」——上表「自行否决/改写」9 条即为被否决的子代理结论。** 另需声明：我在派单时**误把同两个 prompt 各发了两次**（contracts+api 重复一次、architecture+data 重复一次），故这四份文档由两个**互相独立的上下文**各审一遍——这反而提供了双盲交叉验证：两个独立审稿人在**互不知情**的情况下，对 `LOG_AND_ERROR.md` 的双文档拼接、`PIPELINE_BLOCK.md` 的 C 编号冲突、`CONFIG.md` 的多文档拼接给出了**相同结论**，这三项因此按「双盲一致」采信。

---

## 7. UNRESOLVED（需负责人裁决，我无法在车道内裁）

| # | 事项 | 分歧 | 影响 | 所需输入 |
|---|---|---|---|---|
| U-1 | **命名块是否为阶段内节点间载体**（8-1） | `AGENTS.md:88` + `ACSD_DESIGN.md:381` = 是；`PIPELINE_BLOCK.md:17` + `DATA_FLOW.md:14` + 注册表 `carrier_contract` = 否（节点间只走 `output_dir` 文件约定） | 决定 `PIPELINE_BLOCK.md` 的 `lifecycle=frame` 档是否保留、`DATA_FLOW.md` 峰值内存论证是否成立、模块 ABI 是否需要内存块出参 | 负责人裁决：改最高设计，还是改工程正本与注册表。若改工程侧，须同步 `pipeline_block.schema.json` 的 `title` |
| U-2 | **`run_manifest` 的正本字段集是代码侧还是 schema 侧**（8-2） | 代码 `commands.cpp:535` 与 `MANIFEST_VERIFY.md` 用 `kind/acsd_version/config_sha256/…`；schema 与 `CONFIG.md:146` 用 `manifest_schema/software_sha/config_hash/manifest_(input\|output)_hashes/…`，且 schema `additionalProperties:false` | 现状下代码写出的 manifest **必然被自己的 schema 拒**；工程正本内部对同一对象有两套互斥词表 | 负责人指定正本；若选 schema 侧，代码与 `MANIFEST_VERIFY.md` 需同批迁移 |
| U-3 | **平台角色：Linux 是交付平台还是仅控制面**（2-1 / 8-10） | 最高设计第 11 章把 Linux amd64 列为交付平台行且流程图给 Linux「真实数据终验」；`ARCHITECTURE.md:123` 把 Linux 降为「常在线控制、静态分析、轻量编译与小合成实验」 | 决定 L4 真实数据终验在哪平台做、发布候选门槛是否双平台并重 | 负责人裁决；最高设计修改需负责人批准 |
| U-4 | **L2 冻结判据的 enforcement**（8-6） | `PERFORMANCE_MODEL.md:144` 写 fail-closed；`resource_gate_v1.json::compute.mean_utilization_enforcement` 写 `record_and_justify`，并注「85% 均值门在 16-worker 真负载上实测仅 65.09%，未标定前不得硬失败」 | 若按文档 fail-closed，当前生产负载恒判红；若按 schema，非硬失败 | 负责人裁决，并同步两处 |
| U-5 | **`AstroCsExitCode` 是退役还是并存**（8-5） | 该枚举与 `acsd::ExitCode` 9/10 语义对调；`LOG_AND_ERROR.md:275` 的适用边界节未登记 | 同一失败在不同面上收敛为不同退出码 | 负责人裁决退役或收编 |
| U-6 | **`ACSD-BASS-Index/1.0` 的语义**（9-9） | 像是旧产品名残留，也可能是「基于 BASS DR3 数据的索引服务」的真实对外标识 | 若为残留，`CODE.md` 机器契约第 9 类在传播旧名 | 负责人给出该 token 的真实定义 |
| U-7 | **精度归属是两类独立还是单一全局模式**（2-13） | 最高设计 3.3 要求「稠密 f32 + 稀疏/元数据 f64」**同时成立**；`lib/infrastructure/aio/src/aio_api.cpp:35`–`:39` 只有一个全局位 `g_aio_precision_mode_fp64` + `aio_set_precision_mode(int)` | 若以代码为准，3.3 的精度归属不可实现，且 `UNIFIED_OBJECTS.md` 各对象的 float32/float64 列无机器含义；若以 3.3 为准，AIO 的精度面需拆成两个独立量 | 负责人裁决；这决定 `UNIFIED_OBJECTS.md` 精度列（2-4、2-5）该如何重写 |
| U-8 | **UPM 控制点权重的几何因子归属**（2-11） | 代码 `upm.h:177` = `quality_factor × control_ivar`（几何在 per-control 归一化中施加）；`DATA_FLOW.md:142` 同；`ARTIFACTS.md:107` = `quality × geom × control_ivar`（几何乘进分子）；`governance/TRACEABILITY.md:255` = `quality × control_reliability × control_ivar`（第四套）[转] | 四套公式决定 UPM 拟合的权重与归一化，直接影响 P5 的偏差扣除量 | 由 science 正本裁决「几何可靠性在分子还是在归一化」，三处工程文档同批改 |

---

## 8. 自证段

### 8.1 我实际做了什么

- 逐行读完 26 份文档（含 `PUBLIC_API.md` 由子代理全读、我抽核）；未读完的 36 份**逐份列出**（§1.1），不宣称已读。
- 独立重推的公式与常数（全部可复跑）：
  - `A_cell = 4π/(12·nside²)` 在 `nside=2^18` 处：`1.5238730e-11 sr`、`1/A² = 4.3063e21`、`log10 = 21.6341` ⇒ 与文档一致（V-3）。
  - `1/Φ⁻¹(0.75) = 1.482602218505602` ⇒ 与 `ARTIFACTS.md:110` **逐位相同**（V-4）。
  - `NUMERIC.md:50` 的 `max_k(α_max/α_k)² − 1`：`= 5.27646² − 1 = 26.841`，与文中「实测 1.0336」差 26 倍（3-1）。
  - `α²` 区间与 `variance_floor·α²` 区间相差恰 1e-12 ⇒ 判定为量名错标（3-2）。
  - float32 次正规下界 `2^-149 = 1.4013e-45` ⇒ 复核「32/33 下溢为 0」的可行性（3-3）。
  - `E = Var_w/Var_opt − 1` 的尺度不变性 ⇒ 成立（V-8）。
  - `atol ≥ 1 ulp(scale)` 判据 ⇒ f64 `1e-13 > 2.22e-16`、f32 `1e-6 > 1.19e-7`，两条容差均满足（V-10）。
  - `run_manifest` 三方比对（8-2）、注册表 carrier 取值（V-6）、`AstroCsExitCode`（8-5）、`ErrorDomain`（V-2）、`exit_codes.h`（V-1）。
  - UPM 权重三套公式（2-11）、integrate 权重退役（2-12）、精度全局开关（2-13）——三条均 `sed` 直读生产头确认，非转述。
- 未编译、未运行任何二进制；所有「代码为准」的判定来自源码阅读 + grep/schema 解析。**这一条是我的诚实边界。**
- **未核对原文的文献**：本车道参考文献表几乎全为内部文档（`ARTIFACTS.md` 的 Rousseeuw & Croux DOI 是唯一外部文献之一，我核对了 DOI 与题录对应关系，**未取原文全文逐字核对**）。`CONFIG.md:297` 引的 PixInsight 官方式 `[18]/[19]` 与 `WBPP 2.5.9 engine.js:1421-1429`（`ARTIFACTS`/`CONFIG` 中的对照档来源）**核对不到**——仓库内无该文献台账，且 `[18][19]` 超出本篇文献表（5-9）。按红线记为「核对不到」，不凭印象判定。

### 8.2 关键复跑命令（可直接粘贴）

```bash
cd "/workspace/Astro CS Database"

# R1 构建正本不在 git
git -c core.quotepath=false ls-files docs/engineering/build | wc -l      # 0
git check-ignore -v docs/engineering/build/BUILD_GRAPH.md               # .gitignore:20:build/
sed -n '20p' .gitignore

# R2 判据载体缺失：51 条
python3 - <<'PY'
import re,os
root=os.path.abspath('.'); files=[]
for dp,dn,fn in os.walk('docs/engineering'):
    for f in fn:
        if f.endswith('.md'): files.append(os.path.join(dp,f))
pat=re.compile(r'`((?:eng|lib|docs|run|实验)/[A-Za-z0-9_./\-]*\.(?:md|json|py|yaml|yml|h|hpp|c|cpp|cmake|txt|js))`|`((?:\.\./|\./)[A-Za-z0-9_./\-]+\.(?:md|json|py|yaml|h|hpp|c|cpp|txt))`')
bad={}
for p in sorted(files):
    base=os.path.dirname(os.path.abspath(p))
    for i,line in enumerate(open(p,encoding='utf-8',errors='replace').read().splitlines(),1):
        for a,b in pat.findall(line):
            m=(a or b).split('#')[0]
            if not m: continue
            c1=os.path.normpath(os.path.join(base,m)) if m.startswith('.') else os.path.normpath(os.path.join(root,m))
            if not os.path.exists(c1): bad.setdefault(m,[]).append(f"{p}:{i}")
print("MISSING:",len(bad))
for k,v in sorted(bad.items()): print(" ",k,"<-",v[0])
PY
git -c core.quotepath=false ls-files eng/tests | wc -l                  # 106（全在 conformance/ 与 validation/）

# R3 拼接与空锚
grep -c "「」" docs/engineering/contracts/HIPS_STORAGE_FORM.md          # 12
grep -n "最高设计 ：\|（最高设计 ，\|；：\|（/ ）\|该文件 （" docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md
sed -n '179p;232p;299p' docs/engineering/contracts/LOG_AND_ERROR.md
sed -n '236,247p;374,379p' docs/engineering/contracts/CONFIG.md

# 轮 1 节点序
python3 -c "import json;d=json.load(open('lib/infrastructure/pipeline/module_ports.registry.json'));print([m['module_id'] for m in d['modules']][:8])"
grep -n "pos(psf) > pos(wcs)" docs/engineering/contracts/PIPELINE_BLOCK.md

# 轮 3 数值重推
python3 -c "
from statistics import NormalDist
q=NormalDist().inv_cdf(0.75); print('1/Phi^-1(.75)=',1/q)
a=[1.1387e-17,6.0083e-17]; r=a[1]/a[0]
print('ratio',r,'sq',r*r,'max(r^2-1)=',r*r-1)
print('alpha^2',a[0]**2,a[1]**2,' floor*alpha^2',1e-12*a[0]**2,1e-12*a[1]**2)
import math; A=4*math.pi/(12*(2**18)**2); print('A_cell',A,'1/A^2',1/A**2,'dex',math.log10(1/A**2))
print('f32 min normal',1.1754943508222875e-38,'min subnormal',2.0**-149)
"
python3 -c "import json;d=json.load(open('eng/contracts/schemas/unified/frame_snr.schema.json'));print('frame_snr scalar enum:',d['properties'].get('scalar',{}).get('enum') or d['properties'].get('scalar'))"

# 轮 5 引用与语言
grep -n "配置与 output_dir" docs/ACSD_DESIGN.md            # 0 命中
grep -n "^#" docs/ACSD_DESIGN.md | grep -E "7\.1|7\.2|7\.3"
grep -n "机器判据" docs/ACSD_DESIGN.md docs/science/ -r    # 0 命中
ls docs/detail/common 2>/dev/null || echo "docs/detail/common MISSING"
ls docs/science/noise_snr/                                   # 只有 NOISE_SNR.md README.md
python3 - <<'PY'
import re,pathlib
orphan=total=0; lits=0
for f in sorted(pathlib.Path("docs/engineering").rglob("*.md")):
    t=f.read_text(encoding="utf-8")
    if "参考文献" in t:
        total+=1; body,_,rl=t.rpartition("参考文献")
        if set(re.findall(r"\[(\d+)\]",rl))-set(re.findall(r"\[(\d+)\]",body)): orphan+=1
    lits+=len(re.findall(r"一节",t))
print(f"有参考文献节 {total} 份, 正文零引用的孤儿表 {orphan} 份, 「一节」锚 {lits} 处")
PY

# 轮 7/8 负向与一致性
grep -rn "log_artifacts" lib/ eng/ ; echo "(0 hits = none)"
grep -rn "CHK-NAMING-SURFACE" docs/ eng/ lib/            # 仅 CODE.md 自身
python3 -c "import json;d=json.load(open('eng/contracts/schemas/run_manifest.schema.json'));print(d['required']);print('addProps',d['additionalProperties'])"
sed -n '530,560p' lib/infrastructure/cli/commands.cpp
sed -n '145,160p' lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h
cat lib/infrastructure/cli/exit_codes.h
sed -n '103,123p' lib/infrastructure/aio/io/hips_output_store.py
python3 -c "import json;print(json.load(open('eng/contracts/resource_gate_v1.json'))['compute']['mean_utilization_enforcement'])"
grep -n "pos(wcs)\|pos(psf)" docs/engineering/contracts/PIPELINE_BLOCK.md
grep -n "acsd_phase1_noise" CMakeLists.txt | head -3
grep -n "^## " docs/engineering/governance/UNRESOLVED.md

# 轮 9 语言
sed -n '188,190p;207,209p' docs/engineering/resources/PERFORMANCE_MODEL.md
grep -n "FROZEN" docs/engineering/contracts/CLI_PROTOCOL.md
sed -n '70p;83p;118,119p' docs/engineering/standards/CODE.md
```

### 8.3 我没有做的事（诚实声明）

- 没有编译、没有运行 `acsd` 或任何二进制、没有执行文档给出的任何复跑命令（因为它们的路径大多已不存在，见 R2）。
- 没有取 `CONFIG.md:297` 引的 PixInsight 官方式原文与 `WBPP 2.5.9` 源码原文 —— **核对不到**，按红线不凭印象裁。
- 没有取 Rousseeuw & Croux 1993 全文，只核对了 DOI 与题录对应关系。
- 对 §1.1 列出的 36 份不宣称逐行独立判断；相关结论已标 `[转]`。

### 8.4 收敛声明

按 `standards/03_READING_AND_ADVERSARIAL_REVIEW.md` 第 2 节：「每轮输出问题清单；订正后进入下一轮，**直到连续两轮无新增实质问题**」。

**本车道当前状态：十轮全部有新增实质问题，未达成收敛，且距收敛尚远。** 四条根因（R1 版本控制排除、R2 判据载体被删而正本未同步、R3 多文档拼接与节名批量替换、R4 内存块载体口径与权威链上层相反）必须先由写作者车道处理；其中 **U-1（命名块载体）与 U-2（run_manifest 正本）需负责人裁决**，我不能代裁。

**下一轮建议顺序**：R1 → R3（拼接与空锚，纯机械、可批量）→ R2（51 条引用逐条对账）→ U-1/U-2 裁决 → 再跑十轮。
