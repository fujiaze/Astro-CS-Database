"""ACSD 独立漏洞检测测试集 · 合成全链层。

**定位**（`run/GOVERN-08/工作包-RECTIFY-09原件/standards/05_INDEPENDENT_TEST_SUITE.md`
§2 逐字的分层表）：

```text
eng/tests/
├── unit/           单元：函数与核（含 Oracle 对拍、归零负例）
├── module/         模块：单模块输入输出、块读写、生命周期
├── integration/    集成：多模块协同、合同/ABI、方差传播
├── pipeline/       阶段管线：normalize/mosaic/export 阶段内流程
├── synthetic/      合成全链：哈勃仿真与代数合成端到端   ← 本目录
└── e2e/            真实数据端到端与视觉验收辅助
```

**这一层测什么**：两类实验数据（`04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md` §2.1 哈勃仿真
成像、§2.2 纯解析代数合成）驱动的科学不变量，覆盖 AGENTS.md §10 的五个创新点
（`05` §2 逐字「合成全链覆盖五个创新点的关键科学不变量」）。

**这一层不测什么**：不裁决代码，不产出流水线判决，不进默认构建，不接 CI。

## 数据构造面

`_kit.py` 提供两类实验数据的构造器；它**是数据构造器，不是 Oracle**
（纪律见 `_kit.py` 的模块 docstring）。容差冻结表在 `tolerances.py`，
冻结发生在写任何用例之前（`TEST.md` §3）。

## 骨架来源（**一层唯一的例外，已登记**）

本层的用例注册 / 断言 / 证据累积 / 非阻塞裁决**复用 `eng.tests.unit.harness`**，
`run_synthetic.py` 只调它的 `discover("eng.tests.synthetic")`。三条理由与两条不变量：

1. `eng/tests/unit/harness.py` 是仓内唯一的测试骨架，零第三方依赖、可直接跑；
   再造一份会出现两套语义不同的断言，跨层比较无从做起；
2. 它的非阻塞裁决（`verdict()` 恒返回 `warn`、`exit_code()` 默认恒 `0`）正是
   `05` §4 与 `TEST.md` §9 要求的形状；
3. **写域不重叠**：本目录只写 `__init__.py` / `_kit.py` / `tolerances.py` /
   `run_synthetic.py` / `README.md` / `test_*.py`，**一个字都不写进 unit 层**。

不变量一：`harness.close()` 的可满足性前检（`atol ≥ 1 ulp(scale)`）内部只用
`eng.tests.unit.tolerances.ulp()`；本层 `tolerances.ulp()` 与它**逐字同值**（都取
`TEST.md` §4.1 的 `u = 2⁻⁵³`），所以前检在两层给出同一结论。

不变量二：`discover()` 传的是 `"eng.tests.synthetic"`，只扫描本目录的 `test_*.py`；
`eng.tests.unit` 的用例**不会**被本层的运行器收进来。

⚠ 这条耦合是**已知的写域约束产物**，不是设计选择。若后续允许在本目录加骨架文件，
应把 `harness` 上提到 `eng/tests/harness.py` 由各层共用，并把这条登记从本文件删掉。

见 `README.md`（运行方式、两条硬纪律、构造方式与物理环节对照表、诚实边界）。
"""

__all__ = ["_kit", "tolerances", "run_synthetic"]