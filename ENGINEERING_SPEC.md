# AstroCS 工程规范（Engineering Specification）

---

## 1. 语言、编译器与平台

- C++17 双平台；MSVC v143（Windows）/ GCC 或 Clang（Linux）；
- 公共 C ABI 版本化；跨 DLL 不传 STL/异常/RTTI/编译器私有类型；
- Windows 10+ amd64 下限（22H2），Windows 11 主验证；Linux amd64；
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
5. CMake target —— 独立 DLL/SO；
6. 共址可复用测试 —— 单测 + 合同 + 负例；
7. 输入/输出端口引用有效 DATA 合同（`docs/contracts/` 唯一事实源）。

新增能力流程：定义合同 → 实现模块 → 注册 → 修改声明式 Pipeline → 加测试。一个任务只动该动的模块，不同时改 CLI/AIO/调度器等无关模块。

---

## 5. 测试规范

### 5.1 每模块必备测试

- 确定性合成数据生成器；
- 不调用生产实现的独立 Oracle 或解析解；
- 科学不变量/性质测试；
- 边界、NaN/Inf、空输入、极端参数、错误输入；
- 1 worker 与 N worker 数值一致性；
- baseline/AVX2/AVX-512 等价性；
- 双平台允许误差合同；
- 性能、线程与资源利用验证。

### 5.2 测试层级

单元测试 → 模块数值测试（SCI/ALG Oracle）→ 合同/ABI 测试 → Phase 内 Pipeline 测试 → 合成全链（三阶段分别）→ Linux 真实数据流 → Windows 复验 → 图像审核。详见最高设计 §12.4 与 `ACCEPTANCE_SPEC.md`。

### 5.3 测试纪律

- 普通重构跑模块测试 + 影响分析选出的链路；
- 发布候选/科学/数据/拓扑/编译器/ISA 变化才扩大验证；
- 真实数据只验证当前候选；
- 控制包完成前过真实数据终验（未过不标完成）；
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
仓库根固定条目：
README.md / AGENTS.md / ASTROCS_DESIGN.md / ENGINEERING_SPEC.md /
CONTROL_PACK_SPEC.md / ACCEPTANCE_SPEC.md / DEPENDENCIES.md /
CMakeLists.txt / CMakePresets.json / build.sh / toolchain.ps1 /
.clang-format / .editorconfig / .gitignore / .gitattributes / .github/

lib/
├── algorithms/         科学算法（并联放置；phase 为内部指代）
│   ├── calibration cosmetic star_detection psf platesolve photometry
│   ├── noise_snr drizzle coverage sampling upm rejection integration
│   ├── projection resample fits_output
│   └── shared/
└── infrastructure/     基建（cli/ 下挂 normalize/mosaic/export 子命令 + scheduler/pipeline/aio/benchmark/observability/gaia/acr/hips_browser）

其他固定目录：include/ contracts/ cmake/ docs/ tests/ scripts/ tools/ ci/ testdata/ third_party/
config/（程序根全局配置：filters.json / defaults.json）
实验/（科学实验单元：SCI-A/B/C 等，随仓库维护）
工程控制/（控制包工作区，收口后按 CONTROL_PACK_SPEC §9 清理）
artifacts/（证据与产物，含 CI 运行产物 artifacts/ci/<sha>/）
run/（gitignore：临时产物/日志）  logs/（gitignore）
```

- 新产物落位到对应目录，不散落根目录；确需新增根目录条目，先登记并经负责人确认；
- **外部只读数据集**（不由本仓生成、不随仓库分发、仅供本地实验引用）在根目录以具名目录放置，登记于本节与 `ci/root_manifest.json` 的 `allowed_dirs`，全部由 `.gitignore` 排除；已登记：`GaiaDR3/`、`GaiaDR3SP/`、`BASS DR3/`、`HST_M16/`。判据：只读引用、不入库、不被根 CMake 引用、不被检查器当作仓库内容；一旦被代码消费或需入库，移入 `testdata/` 或 `artifacts/`；
- CLI 运行产物只落 `output_dir`；ctest 残留归 `run/Testing_archive/`；
- 修改代码/测试后同步订正 `ci/checks.json`；
- **Alpha 之前代码与产物中不含任何版本信息**（最高设计 §13）；发布 Alpha 时 CLI `--version` 输出 `0.0.1alpha`。

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

- `ci/checks.json` 是唯一检查注册表；`ci/` 提供确定性执行器；
- 每项检查有正例与负例（能红能绿）；豁免显式登记且只减不增；
- **可执行负例面**：每项检查提供机器可执行负例入口（`--self-test` 或 `--fault-inject`）；
- **fail-closed**：检查器在输入缺失、路径不存在、依赖不可用时判红；"文件不存在"按"无违规"通过视为假绿；
- **锚存活**：检查器硬编码引用的文件/目录必须存在，失效时报 `ANCHOR_STALE`；
- **注册表双向一致**：`ci/checks.json` 与 `docs/ci/01_CHECKS.md §2` 双向对齐；
- 修改代码/测试后本地复跑对应检查项；
- 检查器覆盖（至少）：模块 manifest/注册表/构建 target/产品清单一致、端口引用有效 DATA 合同、算法引用有效 SCI/ALG、核心合同有独立测试、API 文档与 AST 一致、删除/重命名无悬空引用（含文档索引）、活动文档版本号与状态均为现行、历史代码处置合规（§2）、Git diff 映射到受影响合同与最小测试集。

---

## 11. 日志、诊断与错误

- 统一状态码（最高设计 §7.2 退出码表），跨平台同失败同码；
- 结构化日志走 JSONL 事件（唯一 schema，见 contracts/schemas）；
- 错误通过统一状态码 + 结构化诊断传播；不跨 C ABI 抛异常；
- 输出临时文件 + 原子提交；失败时不留可被误认成正式产品的半成品；
- 未捕获异常 → exit 70 + 脱敏 crash report（不泄露凭据）。

---

## 12. 资源与性能

- 一个进程只有一个资源调度器与线程预算源；模块不硬编码 workers、不私建长期线程池；
- CPU 密集路径多线程；资源门只管磁盘（内存/CPU/线程不设门），磁盘判据数值唯一源 `contracts/resource_gate_v1.json`（最高设计 §9 只作定性要求与指针）；
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
