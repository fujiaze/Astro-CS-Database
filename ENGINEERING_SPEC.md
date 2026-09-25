# Astro Celestial Sphere Database（ACSD） 工程规范（Engineering Specification）

---

## 1. 语言、编译器与平台

- C++17 双平台；MSVC v143（Windows）/ GCC 或 Clang（Linux）；
- 公共 C ABI 版本化；跨 DLL 不传 STL/异常/RTTI/编译器私有类型；
- Windows 10+ amd64 下限、Windows 11 主验证（平台下限的取值与理由 = `docs/owner/ARCHITECTURE_OVERVIEW.md`）；Linux amd64；
- 构建系统：唯一根 CMake（`CMakeLists.txt`）+ presets；不引入第二套构建入口。

---

## 2. 代码风格与历史实现处置

- 遵循 `.clang-format` 与 `.editorconfig`；CI 检查格式；
- 文件编码 UTF-8；行尾统一（Windows 仓库 CRLF 治理见 .gitattributes）；
- 命名：模块/函数/变量按现有 `lib/` 惯例，不混用多种风格；
- 注释只写：单位、数学原因、前后置条件、所有权、线程安全、生命周期、边界条件、非显然决定。

**历史实现处置（二选一，没有第三种状态）**：

1. **直接删除**：被新实现取代、不在生产路径、无保留价值的代码，连同引用点一起删干净；
2. **保留则注释**：确有参考价值需暂时保留的（如等待接线的实现、隔离实验），在代码上方用统一注释块写明：它是什么、为什么保留、现状（未接入生产/仅供什么实验）、什么条件下删除或接入、权威依据条款。

- 无注释的死代码、被注释掉的旧逻辑、标注了废弃但没有原因与去向的代码，视为缺陷；
- 注释与正式文档只描述现行设计与当前状态；变更过程沉淀在控制包报告与 git 历史中。

---

## 3. 科学代码红线（最高优先级）

- 科学公式、权重/variance/ivar/SNR 定义、排异规则、归约顺序、精度与默认容差以 `docs/science/**`、`docs/algorithms/**` 为准，改动走变更流程；
- **科学正确性优先**：独立证据（外部标准、文献、可复现实验）证明文档与事实不符时，订正文档是义务，记录证据、影响面并做一致性回归；文档正确而实现不符时改实现；
- 架构重构与科学语义订正分开提交；架构迁移保持数值等价（顺序变化时先冻结容差并登记）；
- 模块按声明精度与公式执行，计算结果与 CPU 型号无关；`cpu_profile` 只影响并行/ISA；
- 数据对象按 `docs/design/UNIFIED_MODEL.md` 区分，一个字段只承载一个含义。

---

## 4. 模块规范（每模块必备）

每个可调度模块（algorithms/ 或 infrastructure/ 下）同时具备：

1. `README.md` —— 职责、用法、依赖；
2. `module.yaml` —— ID、版本、ABI、端口、schema 链接、entrypoint；
3. 公开头文件 —— 版本化 C ABI，带 `struct_size`/`abi_version`；
4. 实现 —— 单一 entrypoint，不隐藏整阶段 Session；
5. CMake target —— 独立 DLL/SO（**自持符号闭包**：模块 .so/.dll 从源文件独立重编译其依赖闭包（依赖面限于自身 DT_NEEDED 闭包）；其未定义符号必须能在自身 DT_NEEDED 闭包内解析，由 `CHK-PLUGIN-SYMBOL-CLOSURE` 机器保证）；
6. 共址可复用测试 —— 单测 + 合同 + 负例；
7. 输入/输出端口引用有效 DATA 合同（`docs/contracts/` 唯一事实源）。

新增能力流程：定义合同 → 实现模块 → 注册 → 修改声明式 Pipeline → 加测试。一个任务只动该动的模块，不同时改 CLI/AIO/调度器等无关模块。

### 4.1 管线纪律（对应最高设计 §8）

- 模块只通过命名块与管线交换数据：读入参块、写新块、声明消费的旧块；不持有跨节点的大块数据副本，不跨阶段共享内存；
- 每个块的生产者、消费者、生命周期在模块的 module.yaml 与管线 DAG 中显式声明，调度器据此销毁旧块、控制峰值工作集；
- 三个阶段各自实例化调度器：normalize 异步工作流编排、mosaic 天球窗口并行、export 子块流式；模块不感知调度策略，只声明块依赖与线程安全性；
- 调度器与模块预埋性能探针，编排参数的调优在功能与数值正确闭环后基于探针数据进行。

---

## 5. 测试规范

### 5.1 每模块必备测试

- 确定性合成数据生成器；
- 不调用生产实现的独立 Oracle 或解析解；
- 科学不变量/性质测试；
- 边界、NaN/Inf、空输入、极端参数、错误输入；
- 1 worker 与 N worker 数值一致性（判据 = **事前冻结的浮点容差**，不是逐位一致；容差来源与可满足性下限按 `docs/contracts/SCHEDULER_CONTRACT.md` §2.1 与 `docs/contracts/TEST_MATRIX.md` §2）；
- baseline/AVX2/AVX-512 等价性；
- 双平台允许误差合同；
- 性能、线程与资源利用验证。

### 5.2 测试层级

单元测试 → 模块数值测试（SCI/ALG Oracle）→ 合同/ABI 测试 → Phase 内 Pipeline 测试 → 合成全链（三阶段分别）→ Linux 真实数据流 → Windows 复验 → 图像审核。详见最高设计 §12.4 与 `ACCEPTANCE_SPEC.md`。

### 5.3 测试纪律

- 普通重构跑模块测试 + 影响分析选出的链路；
- 发布候选/科学/数据/拓扑/编译器/ISA 变化才扩大验证；
- 真实数据只验证当前候选；
- 控制包完成前过真实数据终验（未过不标完成；唯一正本 = `CONTROL_PACK_SPEC.md` §9）；
- 测试不可用就跳过并如实标注，不伪造通过。

---

## 6. Git 与提交

- 只 `main` 开发；不做分支/worktree/额外 clone/force push/amend/历史重写；
- 一个 commit = 一个可独立验证的目的；科学/架构/性能/文档不混提；
- SubAgent 零 git 写权限；前台串行提交，push 后核对三 SHA 一致；
- 所有预存修改先登记，不自动 reset/stash/clean/rebase/覆盖；
- 提交消息：做了什么 + 依据权威条款。

---

## 7. 目录规范（强制）

```text
仓库根固定条目（含无扩展名文件 VERSION——产品版本唯一事实源，见本节末版本条款与最高设计 §13）：
README.md / AGENTS.md / ASTROCS_DESIGN.md / ENGINEERING_SPEC.md /
CONTROL_PACK_SPEC.md / ACCEPTANCE_SPEC.md / DEPENDENCIES.md / memory.md /
CMakeLists.txt / CMakePresets.json / eng/build/build.sh / eng/build/toolchain.ps1 /
.clang-format / .editorconfig / .gitignore / .gitattributes / .github/

lib/
├── algorithms/         科学算法（并联放置；phase 为内部指代）
│   ├── calibration cosmetic star_detection psf platesolve photometry
│   ├── noise_snr drizzle coverage sampling upm rejection integration
│   ├── projection resample fits_output
│   └── shared/
├── include/            公共头
├── third_party/        第三方依赖
├── infrastructure/     基建（cli/{normalize,mosaic,export} + pipeline 命名块与块生命周期 +
│                         调度器（scheduler/，三阶段）+
│                         aio/benchmark/observability/gaia_xpsd_client/acr/hips_browser）
└── phase{1,2,3}_session/  三阶段会话编排（引用算法模块，不重复实现）

其他固定目录：eng/contracts/ docs/ eng/tests/ testdata/ eng/packaging/ gaia/
eng/（工程支撑面）
  eng/ci/           机器门注册表与检查器（eng/ci/checks.json、eng/ci/run_checks.py）
  eng/tools/        工具与质量检查器（eng/tools/quality/**、eng/tools/doccheck/**）
  eng/cmake/        CMake 模块
  eng/build/        构建脚本（build.sh / toolchain.ps1；根 CMakeLists.txt 与 CMakePresets.json 为 CMake 入口留在根）
eng/packaging/config/（程序全局配置：filters.json / defaults.json）
docs/contracts/（合同的文档化说明，与 eng/contracts 的 schema 双向对应）
实验/（科学实验单元：photometric-magnitude / absolute-snr / additive-sky-seamless + shared，随仓库维护）
工程控制/（控制包工作区，收口后按 CONTROL_PACK_SPEC §9 清理）
artifacts/（证据与产物，含 CI 运行产物 artifacts/ci/<sha>/ 与证据锚 artifacts/evidence/**）
run/（gitignore：开发/CI 过程产物与过程日志，与块级 output_dir 的运行日志不互替（最高设计 §10）；自清理机制见 eng/tools/run_gc.py 与 eng/tools/round_start.sh）
```

- 新产物落位到对应目录，不散落根目录；确需新增根目录条目，先登记、经负责人核准后生效；
- **根 `VERSION` 是固定条目**（产品版本唯一事实源，最高设计 §13）：它无扩展名，故以本条文字登记；机器登记见 `eng/ci/root_manifest.json` 的 `registered_local_retention` 段（该段是登记面，不是白名单放宽）；
- `eng/build/toolchain.ps1` 是**现役固定条目**（Windows 侧构建/自检脚本），不是遗留待清理对象；其依赖面只允许仓内 vendored 依赖与系统工具链（见 `eng/packaging/dependency-lock.json` 的 `msys2_mingw: FORBIDDEN` 与 `machine_absolute_path: FORBIDDEN`）；
- **外部只读数据集**（不由本仓生成、不随仓库分发、仅供本地实验引用）在根目录以具名目录放置，登记于本节与 `eng/ci/root_manifest.json` 的 `allowed_dirs`，全部由 `.gitignore` 排除；已登记：`gaia/GaiaDR3/`、`gaia/GaiaDR3SP/`。判据：只读引用、不入库、不被根 CMake 引用、不被检查器当作仓库内容；一旦被代码消费或需入库，移入 `testdata/` 或 `artifacts/`；testdata 下数据集（BASS_DR3、HST_M16 等）的入库范围与下载方式以 `testdata/README.md` 为准；
- CLI 运行产物只落 `output_dir`；ctest 残留归 `run/Testing_archive/`；
- **产品落盘形态**（`docs/design/PRODUCT_STORAGE_FORM.md`、`docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md`）：HiPS 产品落盘名只有 `<name>.hips/`（裸 `bare`）与 `<name>.hips.zst`（归档 `archive`）两种，二者互斥；产品级索引 `<name>.hips.index.json` 与数据集级覆盖索引 `coverage.index.json` **不压缩**；归档必须是「整包 tar + 逐成员独立 zstd 帧」，使标准工具 `zstd -dc | tar -xf` 能逐字节还原；归档内 `properties` 与裸形态逐字节一致，`hips_tile_format` 取标准词表值（词表 = `eng/contracts/schemas/hips_storage_form.schema.json#x-astrocs-field-vocabulary`）；产品身份哈希取**解压后内容**（`tree_hash`），容器指纹另记且不作身份；
- **形态配置与清单**：Phase1 的落盘形态由**输入配置键** `storage_form` 选定（`archive` 默认 / `bare`；键缺失或留空 ⇒ 取默认并**报 warn**（默认值来源 = `eng/packaging/config/defaults.json`））；Phase2 / Phase3 的输入合同**不设**该键，出现即 REJECT。产物必须自报形态与索引，**字段名、取值与清单段结构的唯一词表 = `eng/contracts/schemas/hips_storage_form.schema.json#x-astrocs-field-vocabulary`**（本节不复制字段清单；逐帧与运行级字段见该词表）；逐层文档口径一致性由 `CHK-HIPS-STORAGE-FORM --doc-consistency` 机器断言；
- 修改代码/测试后同步订正 `eng/ci/checks.json`；
- **版本信息按阶段出现**（最高设计 §13）：alpha 阶段之前代码与产物中不含任何版本信息；进入 alpha 阶段后一律由根 `VERSION` 派生或与其一致（单源条款 = `docs/owner/RELEASE_STATUS.md` §2），CLI `--version` 输出该源派生的生成串。

---

## 8. 文档集：自解释与层级索引

仓库长期维护一套自解释文档集，随代码持续更新：

- **根文档**：ASTROCS_DESIGN（最高设计）、AGENTS、ENGINEERING_SPEC、CONTROL_PACK_SPEC、ACCEPTANCE_SPEC；
- **docs/**：science（公式权威）、algorithms（推导权威）、plugins（模块工作细节）、design（数据对象与设计）、architecture、interfaces、standards、modules、contracts、development、validation、ci、research、references、api；
- `docs/DOCUMENT_INDEX.yaml` 是唯一索引地图。

规则：

1. **双向索引**：最高设计每节末尾指向对应下级文档；每份下级文档抬头标注上游最高设计条款；
2. **只写现行设计**：正式文档写"要怎样"，不写已撤销设计、不写历史叙事、不堆任务编号与日期；
3. **细节各归其层**：公式与数值在 science/algorithms，模块细节在 plugins，顶层文档只放结论与索引；
4. **悬空即缺陷**：索引指向的文件/章节必须存在，文档引用的代码路径必须真实，CI 检查悬空引用；
5. 代码改动改变行为时，同一提交内更新对应文档与索引。

---

## 9. 仓库整洁与治理工件清理

- 仓库只保留：最新生产代码、自解释文档集（§8）、合同与测试、当前控制包；
- 控制包收口（任务全 PASS、结论沉淀进文档）后，其目录按 CONTROL_PACK_SPEC §9 清理，过程留 git 历史，不长期堆积历史包；
- 过期报告、归档目录、backlog、一次性审计工件在对应控制包收口时甄别清理：有长期价值的结论并入正式文档，其余删除；拿不准的列出清单上呈负责人，不私自删数据；
- 清理与科学/功能改动分开提交，清理提交给出删除清单与依据。

---

## 10. 机器一致性检查

- `eng/ci/checks.json` 是唯一检查注册表；`eng/ci/` 提供确定性执行器；
- 每项检查有正例与负例（能红能绿）；豁免显式登记且只减不增；
- **可执行负例面**：每项检查提供机器可执行负例入口（`--self-test` 或 `--fault-inject`）；
- **fail-closed**：检查器在输入缺失、路径不存在、依赖不可用时判红；"文件不存在"按"无违规"通过视为假绿；
- **锚存活**：检查器硬编码引用的文件/目录必须存在，失效时报 `ANCHOR_STALE`；
- **注册表双向一致**：`eng/ci/checks.json` 与 `docs/ci/01_CHECKS.md §2` 双向对齐；
- 修改代码/测试后本地复跑对应检查项；
- 检查器覆盖（至少）：模块 manifest/注册表/构建 target/产品清单一致、端口引用有效 DATA 合同、算法引用有效 SCI/ALG、核心合同有独立测试、API 文档与 AST 一致、删除/重命名无悬空引用（含文档索引）、活动文档版本号与状态均为现行、历史代码处置合规（§2）、Git diff 映射到受影响合同与最小测试集、**落盘形态合同**（`CHK-HIPS-STORAGE-FORM`：命名/互斥/归档逐成员帧/索引不变式/哈希口径，含正例与负例注入）、**交付共享对象符号闭包**（`CHK-PLUGIN-SYMBOL-CLOSURE`：产品清单登记的每个 plugin .so 的强未定义符号可在 DT_NEEDED 闭包内解析、DT_NEEDED 可解析、`dlopen(RTLD_NOW)` 成功，含正例与负例注入）。

---

## 11. 日志、诊断与错误

规范依据：最高设计 §7.3（错误传播与运行日志）、`docs/design/LOG_AND_ERROR_SYSTEM.md`、
`docs/contracts/LOG_AND_ERROR_CONTRACT.md`。

- 统一状态码（最高设计 §7.2 退出码表），跨平台同失败同码；退出码唯一源 `lib/infrastructure/cli/exit_codes.h`，
  域→码映射的唯一数值表 = `docs/contracts/LOG_AND_ERROR_CONTRACT.md` §5；
- 结构化日志走 JSONL 事件（唯一 schema，见 eng/contracts/schemas）；运行日志行格式正本 =
  `lib/infrastructure/observability/logging/log_event_v1.schema.json`（LOG-001）是日志事件 schema 的唯一来源；
- **错误必须上行到 CLI**：模块不吞错（空 catch、忽略返回码）、不只写日志不返回错误、
  不把故障降级为"警告后继续"；错误通过统一状态码 + 结构化诊断传播；不跨 C ABI 抛异常；
- **降级必须显式**：写 `degraded_reason` + manifest 记录 + 不改变科学语义，三者齐备才允许继续运行；
  改变科学语义的降级按故障处理（fail-closed）；
- **运行日志落输出目录**：成功/失败/取消三路都产出日志工件，**工件名、目录结构与清单字段的唯一正本 = `docs/contracts/LOG_AND_ERROR_CONTRACT.md`**（本节不复制工件名）；
  收尾 fsync + 算哈希 + 原子发布并在 run manifest 的 `log_artifacts[]` 登记；
  日志落点的唯一来源 = 块级 `<output_dir>`；日志写失败即运行失败（非 0 退出码）；
- 日志经 `aio` 唯一 I/O 边界写出；模块不自建文本 logger、不自持日志文件句柄、不自行决定落点；
- 输出临时文件 + 原子提交；失败时不留可被误认成正式产品的半成品；
- **裸形态的体积削减（打洞）在原子发布之前、`fsync` 之后完成，且文件字节逐字节不变**：只对 4 KiB 对齐的整块全零区域打洞；`st_size` 与整文件 `sha256` 必须不变；卷不支持（`EOPNOTSUPP` 等）⇒ 跳过并在 provenance 记 `trim=skipped(reason)`，**不 fail-closed**。包围盒 TRIM（改 NAXIS）是**可选形态**，读端不认其关键字必须 fail-closed。机制唯一实现 = `lib/infrastructure/aio/src/aio_sparse_punch.h`（`aio_sparse::punch_all_zero_blocks`），写端接线 = `aio_hips_writer.cpp` 的 `write_fits_atomic`（次序：内容写出 → 校验 → `fsync` → 打洞 + 读回复算 → 原子 rename）；读回不一致 ⇒ 不发布。细则与判据见 `docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md` §7；机器门 = `CHK-SPARSE-PUNCH` / `CHK-SPARSE-PUNCH-PROBE`。
- 未捕获异常 → exit 70 + 脱敏 crash report（不泄露凭据）；日志/诊断不含凭据与绝对用户路径；
- 判据 `CHK-LOG-SYS`（`eng/ci/checks.json`）：R1 错误不吞 / R2 降级显式 / R3 日志落点 / R4 台账完整 / R5 合同锚，
  每项带可执行负例（`--self-test`）；登记台账 `eng/ci/ledgers/log_system_ledger.json` 只减不增。

---

## 12. 资源与性能

- 一个进程只有一个资源调度器与线程预算源；模块不硬编码 workers、不私建长期线程池；
- CPU 密集路径多线程；资源门只管磁盘（内存/CPU/线程不设门），磁盘判据数值唯一源 `eng/contracts/resource_gate_v1.json`（最高设计 §9 只作定性要求与指针）；
- 异步只用于能隐藏延迟的 I/O/预取/压缩/落盘；队列有容量/背压/取消/超时/错误传播，不用无界队列；科学计算里不用 async/future，不嵌套并行；
- 科学 kernel 归约顺序与确定性由 SCI/ALG 文档明确；
- 内存极简化、编排连续性、缓存复用的要求见最高设计 §9。

---

## 13. 与 CI 的关系

- 本文定义"检查什么"；`docs/ci/` 定义"CI 怎么组织、哪些是门禁、流水线如何跑"；
- 本地必跑：格式、构建、单测、机器一致性检查（§10）；
- CI 必跑：双平台构建、静态检查、文档/合同/ABI 检查（含悬空索引）、单测、合成科学测试、sanitizer/coverage、候选打包、结果留存。

---

## 14. 与其它文档关系

| 文档 | 关系 |
|---|---|
| ASTROCS_DESIGN.md | 一切条款的上位来源 |
| AGENTS.md | 干活纪律（本文的可执行补充） |
| CONTROL_PACK_SPEC.md | 任务拆分与验收如何引用本文规则 |
| ACCEPTANCE_SPEC.md | 四层验收与发布门 |
| docs/ci/ | CI 组织与门禁定义 |
| docs/plugins/ | 各模块具体规范（README/module.yaml/测试） |


---

## 15. 命名：显示名与机器契约保留面

**显示名（唯一）**：`ACSD`，全称 `Astro Celestial Sphere Database`。正文、文档、UI、注释、报告与提交消息一律使用显示名；历史名 `AstroCS` 只允许出现在本节定义的机器契约保留面内（本节为写清判定规则而逐字给出该历史名）。

**判定规则（一句话，能判任何一处该不该改）**：把该处的历史名 `AstroCS` 换成显示名 `ACSD`，**看是否有任何机器会因此失配或指向不存在的对象**——编译器/链接器（include、符号）、`git`（忽略模式）、CMake（target/变量）、schema 与合同校验（键、ID、字面量）、CI 匹配（workflow 名、artifact 名、路径）、测试断言、台账与文档锚。**会 ⇒ 它是机器契约，原样保留；不会 ⇒ 它是显示名，必须写成 `ACSD`。没有第三种状态。**

**机器契约保留面（按类枚举，一律不改；改名会打断锚、schema、include 与产物兼容）**：

| # | 类 | 保留面 | 改名的机器后果 |
|---|---|---|---|
| 1 | C/C++ include 与符号 | 公共头目录 `astrocs/`、`#include <astrocs/…>`、`namespace astrocs`、`astrocs::`、`astrocs_*.dll/.so/.a` | 编译/链接面直接断链 |
| 2 | 合同 / 注册表 / 模块 ID | schema 注解键 `x-astrocs*`；点分、连字符与斜杠 ID：`astrocs.*`、`MOD-astrocs-*`、`astrocs-*`、`astrocs/<x>/vN`；`astrocs*` 台账 schema id | schema 锚、注册表与产品清单按字面匹配，改名即断链 |
| 3 | 环境变量 / CMake 选项 / 根文档名 | `ASTROCS_*`、`ASTROCS_DESIGN.md`，及历史轮次 ID `ASTROCS-*` | 构建入口按字面读取；根文档名是全仓行号锚的宿主 |
| 4 | 可执行 / target / CLI 名 | `astrocs`、`astrocs.exe`、`astrocs-cli`、CI artifact 前缀 `astrocs-windows-candidate-` | 构建 target、CI 选择器与单测断言 | 
| 5 | CI workflow 名 | `AstroCS Linux CI` / `AstroCS Windows CI` / `AstroCS Fatduck Validation` | `workflow_run.workflows` 与 CI 选择器按名精确匹配 |
| 6 | CI 候选产物成员名 | `AstroCS-candidate.zip` 及其落盘路径 | 常量、`require_outputs`、工作流绑定与单测 |
| 7 | 冻结的宿主路径 | `C:/AstroCS/toolchains/…`、`D:\AstroCSRunner\…` | preset 与依赖锁冻结的安装位；SBOM 白名单正则与工作流逐字断言 |
| 8 | 注册目录名与发布/审核产物名 | 根目录名 `AstroCS.wiki/`；`dist/AstroCS-CLI-v1/…`、`AstroCS-<根 VERSION>-win-x64/`、`AstroCS-audit-*` | `git` 忽略模式、根清单登记、安装树合同与生成器常量 |
| 9 | 对外协议标识 | HTTP `User-Agent` product token（形如 `AstroCS-BASS-Index/1.0`） | 对外声明的客户端身份；改名是对外行为改变，不属命名统一的范围 |
| 10 | 封存证据与只读数据登记面 | `artifacts/evidence/**`（证据锚，含对历史文件名与历史标题的逐字引用）、`testdata/**`（§7 只读登记目录，其 `README.md` 规定目录内含数据按只读处理） | 证据锚指向的对象一经封存即保持原样；只读目录的处置属发布权范畴 |

**唯一源与机器门**：

- 本节是显示名与保留面的**唯一定义源**；其它文档、检查器与台账只引用 `ENGINEERING_SPEC.md §15`，不复述本节定义；
- 类级登记与逐类判据 = `eng/ci/ledgers/naming_surface.json`（只登记类、判据与机器依据，不重复本节定义）；
- 门 = `CHK-NAMING-SURFACE`（`eng/ci/check_naming_surface.py`）：以 `git grep -w` 扫历史名 `AstroCS` 与小写别名 `astrocs`、全大写命名空间 `ASTROCS` 三族，**每一处命中必须落在某一保留类内**；落在类外 ⇒ 判红，那就是显示名漏改。门自带 `--self-test` 负例面（14 例）与定义面棘轮（只减不增）。

