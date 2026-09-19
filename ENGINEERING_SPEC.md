# AstroCS 工程规范（Engineering Specification）

---

## 1. 语言、编译器与平台

- C++17 双平台；MSVC v143（Windows）/ GCC 或 Clang（Linux）；
- 公共 C ABI 版本化；跨 DLL 不传 STL/异常/RTTI/编译器私有类型；
- Windows 10+ amd64 下限（22H2），Windows 11 主验证；Linux amd64；
- 构建系统：唯一根 CMake（`CMakeLists.txt`）+ presets；不引入第二套构建入口。

---

## 2. 代码风格

- 遵循 `.clang-format` 与 `.editorconfig`；CI 检查格式；
- 文件编码 UTF-8；行尾统一（Windows 仓库 CRLF 治理见 .gitattributes）；
- 命名：模块/函数/变量按现有 `lib/` 惯例，不混用多种风格；
- 注释只写：单位、数学原因、前后置条件、所有权、线程安全、生命周期、边界条件、非显然决定；**禁止**堆积历史版本号/任务编号/审计流水/代码复述。

---

## 3. 科学代码红线（最高优先级）

- 科学公式、权重/variance/ivar/SNR 定义、排异规则、归约顺序、精度与默认容差**不可随意修改**；
- **科学正确性优先**：`docs/science/**` 与 `docs/algorithms/**` 必须科学正确。当独立证据（外部标准、文献、可复现实验）证明文档与标准或事实不符时，订正文档是义务：走**变更 claim** 流程（记录证据、影响面、版本递增）+ 一致性回归；文档已证明正确而实现不符时，改实现；
- 架构重构与科学语义订正分开提交；架构迁移保持 bitwise 相等（顺序变化时先冻结容差并登记）；
- 模块按声明精度与公式执行，计算结果与 CPU 型号无关；`cpu_profile` 只影响并行/ISA；
- 数据对象按 `docs/design/UNIFIED_MODEL.md` 区分，一个字段只承载一个含义。

---

## 4. 模块规范（每模块必备）

每个可调度模块（algorithms/ 或 infrastructure/ 下）必须同时有：

1. `README.md` —— 职责、用法、依赖；
2. `module.yaml` —— ID、版本、ABI、端口、schema 链接、entrypoint；
3. 公开头文件 —— 版本化 C ABI，带 `struct_size`/`abi_version`；
4. 实现 —— 单一 entrypoint，不隐藏整阶段 Session；
5. CMake target —— 独立 DLL/SO；
6. 共址可复用测试 —— 单测 + 合同 + 负例；
7. 输入/输出端口引用有效 DATA 合同（`contracts/schemas/` 唯一事实源）。

新增能力流程：定义合同 → 实现模块 → 注册 → 修改声明式 Pipeline → 加测试。**不应**同时改 CLI/AIO/调度器/多个无关模块。

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

单元测试 → 模块数值测试（SCI/ALG Oracle）→ 合同/ABI 测试 → Phase 内 Pipeline 测试 → 合成全链（三阶段分别）→ Linux 真实数据流 → Windows 复验 → 图像审核。详见最高设计 §11.2。

### 5.3 测试纪律

- 普通重构跑模块测试 + 影响分析选出的链路，不反复跑历史版本/全量真实数据；
- 发布候选/科学/数据/拓扑/编译器/ISA 变化才扩大验证；
- 真实数据只验证当前候选，不作为历史对拍工具；
- 控制包完成前必须过真实数据终验（未过不标完成）。

---

## 6. Git 与提交

- 只 `main` 开发；禁止分支/worktree/额外 clone/force push/amend/历史重写；
- 一个 commit = 一个可独立验证的目的；科学/架构/性能/文档不混提；
- SubAgent 零 git 写权限；前台串行提交，push 后核对三 SHA 一致；
- 所有预存修改先登记，不自动 reset/stash/clean/rebase/覆盖；
- 提交消息：做了什么 + 依据权威条款。

---

## 7. 目录规范（强制）

```text
仓库根固定条目：
README.md / AGENTS.md / ASTROCS_DESIGN.md / ENGINEERING_SPEC.md /
CONTROL_PACK_SPEC.md / ACCEPTANCE_SPEC.md / memory.md / DEPENDENCIES.md /
CMakeLists.txt / CMakePresets.json / build.sh / toolchain.ps1 /
.clang-format / .editorconfig / .gitignore / .gitattributes / .github/

lib/
├── algorithms/         科学算法（并联放置；phase 为内部指代）
│   ├── calibration cosmetic star_detection psf platesolve photometry
│   ├── noise_snr drizzle coverage sampling upm rejection integration
│   ├── projection resample fits_output
│   └── shared/
└── infrastructure/     基建（cli/ 下挂 normalize/mosaic/export 子命令 + scheduler/pipeline/aio/benchmark/observability/gaia/acr/hips_browser）

其他固定目录：include/ contracts/ cmake/ docs/ tests/ scripts/ tools/ ci/ testdata/ third_party/ reverse_verify/
config/（程序根全局配置：filters.json / defaults.json；ASTROCS_DESIGN.md §3.3 点名要求）
工程控制/ 报告 reports/ 证据 artifacts/ 工程 engineering/ 打包 packaging/
run/（gitignore：临时产物/日志）  logs/（gitignore）
```

- **任何新产物必须落位到对应目录，禁止散落根目录**；
- 确需新增根目录条目，先登记并获得负责人确认；
- **`reverse_verify/` 为逆向验收工作区**（RELEASE-02 负责人确认新增）：主线做**正向推导**，该区从第一性原理与公开文献/开源实现**逆向验证**；**独立构建**（不得被根 `CMakeLists.txt` 引用），存放实验代码、合成数据代码、方案设计与参考文献；中间产物落 `run/reverse_verify/`；
- CLI 运行产物只落 `output_dir`；ctest 残留归 `run/Testing_archive/`；
- 修改代码/测试后同步订正 `ci/checks.json`；禁止把运行产物产出到项目根目录；
- **证据与产物落 `artifacts/`**（含 CI 运行产物 `artifacts/ci/<sha>/`）；根下**不得**新建 `evidence/` 等未登记目录（历史 `evidence/` 已删除，其内容为旧世代残留）；
- **Alpha 之前代码与产物中不包含任何版本信息**（见最高设计 §12；`VERSION/CHANGELOG.md` 仅作内部助记，不进入程序与发布产物）；发布 Alpha 时 CLI `--version` 输出 **`0.0.1alpha`**。

---

## 8. 机器一致性检查

- `ci/checks.json` 是唯一检查注册表；`ci/` 提供确定性执行器；
- 每项检查有正例与负例（能红能绿）；豁免必须显式登记且只减不增；
- **可执行负例面**：每项检查提供机器可执行的负例入口（`--self-test` 或 `--fault-inject`），注册时即可验证"能红能绿"；
- **fail-closed**：检查器在输入缺失、路径不存在、依赖不可用时判红；"文件不存在"按"无违规"通过视为假绿；
- **锚存活**：检查器硬编码引用的文件/目录必须存在，失效时显式报 `ANCHOR_STALE`；
- **注册表双向一致**：`ci/checks.json` 与 `docs/ci/01_CHECKS.md §2` 双向对齐（注册项必登记、P0 承诺必有实现）；
- **裁决引用规范**：引用负责人裁决使用 named-ID（如 `PSF-FAST-001`）并落位在 §0 权威链内的文档；
- 修改代码/测试后必须本地复跑对应检查项；
- 检查器覆盖（至少）：模块 manifest/注册表/构建 target/产品清单一致、端口引用有效 DATA 合同、算法引用有效 SCI/ALG、核心合同有独立测试、API 文档与 AST 一致、删除/重命名无悬空引用、活动文档无陈旧版本号/历史状态冒充、Git diff 映射到受影响合同与最小测试集。

---

## 9. 日志、诊断与错误

- 统一状态码（最高设计 §6.3 退出码表），跨平台同失败同码；
- 结构化日志走 JSONL 事件（schema 见 contracts/schemas）；
- 错误通过统一状态码+结构化诊断传播；**不跨 C ABI 抛异常**；
- 输出临时文件+原子提交；失败不得留下可被误认成正式产品的半成品；
- 未捕获异常 → exit 70 + 脱敏 crash report（不泄露凭据）。

---

## 10. 资源与性能

- 一个进程只有一个资源调度器与线程预算源；模块不得硬编码 workers、不得私建长期线程池；
- CPU-heavy 必须多线程；重计算负载资源门（G-RES-01）的判据与判定域见 `docs/plugins/infrastructure/21_observability.md` §8，数值唯一源 `contracts/resource_gate_v1.json`（最高设计 §8 只作定性要求与指针）；
- 异步只用于能隐藏延迟的 I/O/预取/压缩/落盘；队列必须有容量/背压/取消/超时/错误传播，禁止无界队列；
- 科学 kernel 归约顺序与确定性由 ALG/ARCH 明确。

---

## 11. 与 CI 的关系

- 本文定义"检查什么"；`docs/ci/` 定义"CI 怎么组织、哪些是门禁、流水线如何跑"；
- 本地必跑：格式、构建、单测、机器一致性检查（§8）；
- CI 必跑：双平台构建、静态检查、文档/合同/ABI 检查、单测、合成科学测试、sanitizer/coverage、候选打包、结果留存。

---

## 12. 与其它文档关系

| 文档 | 关系 |
|---|---|
| ASTROCS_DESIGN.md | 一切条款的上位来源 |
| AGENTS.md | 干活纪律（本文的可执行补充） |
| CONTROL_PACK_SPEC.md | 任务拆分与验收如何引用本文规则 |
| docs/ci/ | CI 组织与门禁定义 |
| docs/plugins/ | 各模块具体规范（README/module.yaml/测试） |
