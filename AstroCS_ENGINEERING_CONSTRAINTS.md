# AstroCS 工程约束（负责人冻结）

> 本文件是项目负责人冻结约束的仓库根权威整理版。仓库内 Agent、任务、代码、
> 测试与文档均不得与本文件冲突；历史控制包、旧文档或 Agent 提议与本文冲突时，
> 以本文为准。本文只由项目负责人确认后修改；Agent 不得自行放宽、删除或重新解释。

## 0. 来源与修订关系（机器可读）

```yaml
# GOV-001 冻结 Alpha 工程约束 — 来源与修订关系
doc_id: DOC-GOV-CONSTRAINTS-001
doc_status: ACTIVE_NORMATIVE
doc_scope: repository_root
source_control_package: ASTROCS-ALPHA3-MODULAR-REFOUNDATION-V7
source_control_relpath: 01_OWNER_FROZEN_CONSTRAINTS.md
source_control_sha256: f8f3440053030469f84c5b5e06666696ce014d02e101cd85d2930b8b7d27a7b2
source_package_readme_sha256: b1dfb88846903f635e841f8cd6b06d587cecd08d45cd15e41cb8619b261a4a33
source_package_readme_relpath: 00_READ_FIRST.md
source_main_sha: c1696156583c68c9a3a65287639c726937e9f6e7
source_main_version: 0.10.0-alpha.2
target_main_version: 0.11.0-alpha.1
authoring_task: GOV-001
authoring_owner: SA-GOV-01
audit_note: 修复审计 P1（02_CURRENT_BASELINE_AUDIT.md §4.10）——
  AstroCS_ENGINEERING_CONSTRAINTS.md 曾只出现在审核包 audit/source/ 而不在仓库根、
  来源不清；本文件使冻结约束成为 Git 跟踪的仓库根权威输入。
content_rule: 科学公式、权重/variance/ivar/SNR 定义、排异规则、归约顺序、
  精度与默认容差以 docs/science、docs/algorithms 与测试 oracle 为权威，本文件不重复公式。
```

## A. 产品与阶段

1. 当前为 Alpha 预发布架构收敛，目标版本 `0.11.0-alpha.1`；版本仍可继续 alpha，不冒充稳定版。
2. 仅 `main` 形成正式开发历史。SubAgent 可使用 detached worktree，但不建立正式分支、不直接提交。
3. Phase1、Phase2、Phase3 是三个隔离产品命令，不是固定顺序流水线：
   - Phase1：单帧 light + masters/catalog/config → 单帧标准化 HiPS + manifest；
   - Phase2：任意一组合同兼容 HiPS → 马赛克 HiPS + UPM/rejection/integration provenance；
   - Phase3：任一合同兼容 HiPS → 平面 FITS + WCS/coverage/validity/provenance。
4. 禁止同进程 `--phases 1,2,3`；外部脚本可以显式依次启动三个独立进程，但这不是产品内部状态机。
5. Phase3 不得假设输入来自 Phase2；Phase2 不得假设输入由同一进程 Phase1 产生。
6. 阶段间只通过原子发布、哈希和 provenance 完整的磁盘产品/manifest 交换。

## B. 平台与发布形态

1. 正式开发/客户端/发布平台：Windows x64。
2. 技术兼容下限：Windows 10 22H2 x64（build 19045）；Windows 11 x64 是主验证环境。
3. Linux amd64 仅为常在线控制、静态分析、轻量编译、小合成实验节点；不得为 Linux 便利反向塑造 Windows 架构。
4. Windows 用户只面对 `astrocs.exe`；运行时、I/O、科学模块和 CPU provider 作为 DLL 随包交付。
5. Linux 可产出同源 `astrocs` + `.so` 技术预览，用于轻验证；Linux 性能不作为 Windows 发布性能结论。
6. HiPS Browser 不注册为 CLI 插件，不进入本轮科学 DLL 列表；它属于未来 Windows GUI。
7. 未来 GUI 通过稳定 CLI 命令、JSON/JSONL、退出码和产品文件调用，不直接链接科学模块私有 C++ 接口。

## C. ACR 与 CPU

1. ACR 是正式发布后的 CPU/GPU 异构更新；本轮保留源码和隔离测试，但生产构建、加载、路由、benchmark、发布包均不得依赖或包含 ACR/CUDA。
2. 当前唯一生产计算后端是纯 CPU。
3. 支持 AMD64 baseline、AVX2/FMA、AVX-512；BMI2 可作为 HEALPix 位操作能力，不要求独立 DLL。
4. 不能为每个算法机械复制三份；只为 profile 证明的热点 kernel 写高级 ISA，未实现 kernel 自动退回 baseline。
5. baseline target 禁止泄漏 `/arch:AVX*`；AVX2/AVX-512 选项只能作用于对应 provider DLL。
6. CPUID + OSXSAVE/XGETBV + provider 正确性自测 + 逐 kernel benchmark 全部通过后才可使用高级 ISA。
7. profile 与 CPU/OS/build/provider hash 绑定；缺失、损坏、硬件变化或 build 变化一律失效并回退 baseline。
8. AVX-512 不因“更高级”自动优先；只有对应 kernel 实测可靠且显著更快才选择。

## D. 并行与资源利用

1. 任何持续重 CPU 的生产计算不得单线程或长期低利用率。
2. 短 I/O、元数据、初始化、原子提交和确实小于并行开销的任务可以串行，但必须在 plan/trace 分类。
3. 全 Phase 只有一个共享 CPU executor 和一个原子 ThreadBudget；模块不得私建永久线程池或硬编码 1/2/16/32 核。
4. 模块只能使用 host 授予的 ThreadLease；不得用 `ThreadLease::make(n)` 伪造授权。
5. heavy run 自动启动同 run ID 的 CPU/RAM/I/O/线程/队列时间序列监控；无资源证据即 FAIL。
6. heavy 区间平均归一化 CPU 利用率至少 80%；至少 70% 采样点达到 75%；队列有工作时不得连续 10 秒低于 50%。
7. 2 核、足够工作量的合成测试，相对 1 worker 加速比不低于 1.60；达不到必须定位和修复，不能解释性放行。
8. 若内存带宽或 I/O 限制，必须用带宽、队列、等待和 active worker 证据证明；一句“可能 I/O”无效。
9. RSS/private bytes 持续不受控增长或结束后不能回落到有解释的高水位，视为内存问题。

## E. 科学与迁移

1. 架构迁移不得同时修改科学公式、权重/variance/ivar/SNR 定义、排异规则、归约顺序、精度或默认容差。
2. 每模块先冻结 SCI/ALG/DATA/API/TEST 与合成 oracle，再以兼容适配器迁移现有实现，直调与 DLL 调用通过后才退旧入口。
3. 无 SIMD/FMA/并行归约顺序改变时，迁移默认要求 bitwise 相等；确有顺序变化时必须在实现前冻结绝对/相对/ULP 容差及依据。
4. 正常重构不反复跑历史版本或完整 32R；以解析解、独立 oracle、性质/不变量和小合成数据为主要判据。
5. 真实 32R 只在当前最终候选提交 Windows 上执行一次成功验收；接缝专项可使用少量冻结真实帧辅助。
6. 连接缝回归必须有合成梯度、重叠、UPM、coverage、边界和恒定场测试，并在最终 HiPS 预览供负责人审核。

## F. 模块、接口与文档

1. 每个生产 DAG 节点必须映射唯一真实 module ID、DLL、导出入口和算法操作；多个节点不得调用同一完整 Session。
2. Session 只装配本 Phase，不能含隐藏算法，也不能调用其他 Phase。
3. DLL 边界为版本化 C ABI；禁止跨 DLL 传 STL、异常、RTTI、编译器对象和不明所有权内存。
4. 所有接口明确用途、参数、单位、shape、坐标、有效域、invalid/NaN、所有权、线程安全、取消和错误码。
5. 每模块必须有 `README.md`、`module.yaml`、公共头、实现、CMake、共址可复用测试。
6. 文档追溯固定为：`SCI → ALG → DATA/API/ARCH → module/source symbol → TEST → evidence → L0`。
7. 项目负责人只需审查 L0：`REVIEW.md`、科学/管线/架构/发布状态/变更摘要；底层文档由 Agent 维护并可机器验证。
8. 活跃文档禁止将 V18/V19/V4/V5/V6、旧 commit、旧路径、旧线程数、旧 Phase 连续语义冒充现状；历史只能进入 `docs/archive` 或 `engineering/control/archive` 并标 `ARCHIVED/NON_NORMATIVE`。

## G. Git、执行与交付

1. 一个 task 对应一个可独立验证的 commit；架构、科学、性能、文档清理不得混在一起。
2. 只有前台 Agent 在 `main` 串行 commit/push；每次 push 后 fetch 并验证三 SHA 一致。
3. 禁止 force push、reset --hard、破坏历史、覆盖用户未提交工作。
4. 所有外部命令有 timeout、cwd、argv、起止时间、退出码、stdout/stderr 和 SHA；长任务同步资源监控。
5. 检查点是自动 Gate，不是人工停止点；Windows 离线时继续全部 Linux/文档/静态/合成工作。
6. 审核包白名单打包，包含当前源码基线与精确证据；禁止 raw testdata、完整 HiPS tile 树、build/cache/.git/历史包/大索引/中间 FITS。
7. 机器检查脚本崩溃、跳过关键目录或只检查小子集均算 FAIL，不得记为“工具问题”。

## H. 不得改变的判定权

SubAgent、前台 Agent 和独立审计 Agent 都不能宣布正式发布。它们只能提交证据和建议；最终裁定由项目负责人在下一轮审核作出。

---

## 机器索引

本文件是 GOV-002 `docs/DOCUMENT_INDEX.yaml`（active 文档机器索引）与
`tools/doccheck` 系列检查器收录的仓库根 ACTIVE_NORMATIVE 文档之一；
本文件由 `tools/doccheck/check_engineering_constraints.py` 校验与上游
`01_OWNER_FROZEN_CONSTRAINTS.md` 的 SHA-256 修订关系（见文件头 YAML）。
