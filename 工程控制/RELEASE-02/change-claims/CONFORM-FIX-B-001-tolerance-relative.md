# 变更 claim（草案）：CONFORM-FIX-B-001 — UPM 收敛容差由绝对判据改为尺度无关相对判据，并登记 tolerance_relative 字段

- 控制包：RELEASE-02 / 分片 **CONFORM-FIX-B**（符合性修复分片 B）
- 关联审计条目：CONFORM-SWEEP-3-001（C2 常数不符）、CONFORM-SWEEP-3-002（C7 schema 面漏登记）
- 变更对象（**本草案不改任何文档**，仅提出订正请求）：
  - docs/algorithms/v6/frozen/01_NUMERIC_THRESHOLD_FREEZE.md:45（FZ-UPM-CONVERGENCE）
  - docs/algorithms/PHASE2_UPM_IMPL.md:379（§13 冻结数值表）、:400-401（「冻结面，任何修改必须走 SCI/合同变更」）
  - docs/contracts/DATA_SEMANTICS.md:1874（P2UpmBuildConfig 字段表）、:1762
  - docs/contracts/PUBLIC_API.md:1827（config 覆盖键集）
- 日期：2026-09-19
- 依据条款：ENGINEERING_SPEC §3（科学正确性优先 + 变更 claim + 一致性回归）；AGENTS.md §5（不动冻结容差，除变更流程批准）、§8（科学疑义查证流程）
- 状态：**草案，待负责人裁决**（本分片**未**启用相对判据；实现已回退到冻结值 tol=1e-6 / tolerance_relative=0）

---

## 1 问题描述（合规事实）

1. FZ-UPM-CONVERGENCE（01_NUMERIC_THRESHOLD_FREEZE.md:45）冻结
   「Huber δ=1.345; max_iter=100; **tol=1e-6**; sigma_floor=1e-3; zero_anchor=1e-3」，
   并明文「**改动 tol/σ_floor → rc!=0**」。
2. PHASE2_UPM_IMPL.md:379 与 DATA_SEMANTICS.md:1874 同值 1e-6，且 ALG 表自述
   「**本表数值与公式为冻结面：任何修改必须走 SCI/合同变更，禁止在实现或测试内就地放宽**」（:400-401）。
3. RELEASE-02 P2a-3 在实现内就地改为 uc.tolerance = 1e-3; uc.tolerance_relative = 1;
   （module_adapters.cpp p2_op_upm_fit），未走变更流程；且新字段 tolerance_relative 在
   docs/ + contracts/ **零出现**（grep -rn "tolerance_relative" docs/ contracts/ = 0 命中）。
4. 后果：upm.cpp:993-1001 在 tolerance_relative=1 时把判据变成 tol × max(scale,1)；
   生产 max|M| ~ 3e15 ADU ⇒ 实际收敛门 **≈3e12 ADU**，比冻结门 1e-6 ADU 宽
   **18 个数量级**；p2_upm_convergence.converged 在残差仍达 ~1e12 ADU 时即置 1，
   而该键是下游唯一的收敛可见面 ⇒「不收敛」在数据面上不可见。
5. 同时另一条生产路径 p2_session.cpp:204 仍取 1e-6（无相对判据）⇒ 同一输入经
   session 与经 node chain 得到不同收敛语义。

**本分片已执行的合规处置**：把实现回退到冻结值（tolerance=1e-6、tolerance_relative=0），
立即恢复符合性；相对判据保留为**显式 opt-in** 覆盖键（upm.tolerance_relative，默认 0），
未经裁决不得在生产启用。

---

## 2 请求裁决的科学论证：冻结门是**绝对**判据，而生产 M 量级使其物理上不可达

### 2.1 量级事实（实测，见 reports/RELEASE-02/conform-fix-b.md §001）

| 量 | 实测/推导值 | 来源 |
|---|---|---|
| max\|M\|（UPM 公共场） | ~3e15 ADU | RELEASE-02 P2a-3 注释 + 本分片合成复算 |
| IEEE-754 double 在 3e15 处的 ULP | ULP = 2^-52 × 2^52 ≈ 0.5 ADU | 定义 |
| 冻结门 tol=1e-6 | 1e-6 ADU | FZ-UPM-CONVERGENCE |
| 门 / ULP 比 | 1e-6 / 0.5 = 2e-6 ⇒ 比 ULP **小 5.7 个数量级** | 推导 |
| 相对判据有效门 1e-3 × 3e15 | ≈3e12 ADU | upm.cpp:1000 |

**结论 1**：max_dM < 1e-6 在 max|M|~3e15 时**不是「难达到」，而是「不可能达到」**——
IRLS 的 max_dM 是相邻两轮 M 之差，其最小非零值是该量级的 ULP（≈0.5 ADU），
故 max_dM ∈ {0} ∪ [0.5, ∞)，永远不可能落进 (0, 1e-6)。冻结门在此量级下退化为
「只有逐位不动点才收敛」，即**几乎必然迭代耗尽**（iterations=100, converged=0）。
这不是「严格」，而是**判据与量纲不匹配**：绝对容差只对 O(1) 量级有意义。

### 2.2 冻结面的**本意**是收敛控制，不是量级门

PHASE2_UPM_IMPL.md:379 把 tolerance 描述为「收敛门 max_dM/max_dC」，其目的是
「迭代到不再显著改善即停」。一个恒不可达的门使该控制**完全失效**（永远跑满 100 轮），
与冻结条文的意图相反。⇒ 该冻结数值在**生产尺度**上属「规范本身陈旧/不适定」，
应按 ENGINEERING_SPEC §3 订正，而不是继续让实现违反它。

### 2.3 建议订正（三选一，请负责人裁决）

| 方案 | 内容 | 优点 | 风险 |
|---|---|---|---|
| **A（推荐）** | 把 FZ-UPM-CONVERGENCE 的 tol 语义改为**相对判据**：tol_rel = 1e-3，阈值 tol_rel × max(max\|M\|, max\|C\|, 1)；在 ALG §13 / DATA_SEMANTICS §25 登记 tolerance_relative 字段与默认值；PUBLIC_API 覆盖键集补该键 | 尺度无关、生产/合成同一判据；保留绝对档为 tolerance_relative=0 | 需同时订正 3 份文档 + 递增版本；1e-3 取值需实测依据 |
| **B** | 冻结 tol 改为**与 ULP 挂钩**的绝对门（如 tol = 8 × ULP(max\|M\|)） | 不改判据形状，只把数值改成可达 | 需在冻结表引入运行时量；文档复杂度上升 |
| **C** | 维持绝对 1e-6，但在产物中**如实暴露** converged=0 并把它升级为发布门（禁止 converged=0 的产品出包） | 零文档改动 | 生产将**恒不收敛** ⇒ 等于停产；不可行 |

**推荐 A**，理由：① 唯一同时满足「判据可达」与「尺度无关」的选项；
② 与 docs/science/PHASE2_UPM.md:56 的份额式（无量纲）定义同源；
③ 与已修缺陷 FIX-UPMSCALE（归一化门由 s>1e-12 改为尺度无关 s>0 ∧ finite，
upm.cpp:646）**同一类**根因（绝对阈值 vs 量纲），保持项目内判据口径一致。

**A 方案的 tol_rel 取值证据**（RELEASE-02 P2a 实测，供裁决参考）：每轮 max_dM 仅降
~0.7%，相对残差稳定在 1.15e-4 ~ 1.32e-4；故 tol_rel=1e-3 会在相对残差 ≈1e-4 量级停
（约 25 轮），而 tol_rel=1e-4 需 ~60 轮以上。**具体取值请负责人裁决**（本草案不预设生产值）。

---

## 3 tolerance_relative 字段的 schema 登记（对应 CONFORM-SWEEP-3-002）

| 位置 | 现状 | 请求订正 |
|---|---|---|
| DATA_SEMANTICS.md:1860-1880（P2UpmBuildConfig 字段表） | 无 tolerance_relative 行 | 增行：tolerance_relative \| int \| 0 \| 0=绝对判据（冻结默认）；1=相对判据 tol×max(scale,1) |
| DATA_SEMANTICS.md:1966-1967（字段清单） | 无 | 补 |
| PUBLIC_API.md:1827（覆盖键集：max_iterations/huber_delta/smoothing_lambda） | 无 | 补 tolerance / tolerance_relative（实现已存在这两个覆盖键） |
| PHASE2_UPM_IMPL.md:379（tol 行） | 1e-6 | 按 §2.3 方案 A/B 改写 |

---

## 4 影响面（若批准 A）

- **文档**：01_NUMERIC_THRESHOLD_FREEZE.md（FZ 表行 + 版本递增）、PHASE2_UPM_IMPL.md §13、
  DATA_SEMANTICS.md §25 字段表、PUBLIC_API.md 覆盖键集。
- **实现**：module_adapters.cpp p2_op_upm_fit 恢复
  uc.tolerance = <裁决值>; uc.tolerance_relative = 1;；p2_session.cpp 同步（否则两条路径再次分叉）。
- **一致性回归**：p2_upm_synthetic_test + v6_p2_upm 用例需增加「同输入下绝对/相对判据的
  C 场一致性」锚（本分片已在 run/RELEASE-02/conform-fix-b/tests/cfb_tests.cpp::t001 给出实测）。
- **未决耦合（须一并裁决）**：p2_session.cpp 与 node chain 除 tolerance 外还在 gs_damping /
  m_full_frame / final_gauge / 求解入口（p2_upm_build vs p2_upm_build_geo）上分叉，且这三个
  参数在 docs/ + contracts/ + 工程控制/ **零登记**（同属「就地改科学行为」）。仅修 tolerance
  不能使两条路径「同输入同结果」，需一并裁决其规范地位（见 reports/RELEASE-02/conform-fix-b.md §001-3）。

## 5 请求裁决

1. 是否批准方案 A（tol 改为尺度无关相对判据 + 登记 tolerance_relative）？
2. 若批准，tol_rel 取值（建议 1e-3，待收敛专项实测确认）？
3. p2_session 与 node chain 的其余分叉参数（gs_damping/m_full_frame/final_gauge/求解入口）
   如何定性——补齐 session、或把 session 标为非生产路径？
