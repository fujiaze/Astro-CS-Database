# testkit

测试元数据合同的机器校验器：校验 eng/tests/testkit 的登记表与 schema 一致性、期望来源与选择执行。

## 职责边界

- 放：测试元数据 JSON Schema 校验、test_id 唯一性与 module 标签选择、期望来源审计（生产符号不得生成期望）、故障注入自检。
- 不放：测试用例本体（在 `eng/tests/`）、检查项注册（在 `eng/ci/checks.json`）、测试元数据规范正文（在 `eng/tests/testkit/`）。

## 内容

- `check_testkit.py` —— 校验入口：K1 元数据存在且被 Git 跟踪、K2 逐项过 schema、K3 ID 唯一与标签合法、K4 期望来源审计、K5 `module:<id>` 选择、K6 故障注入；git 面不可用时显式降级留痕并 fail-closed。
- `__pycache__/` —— Python 字节码缓存，不入库。

## 上游

- 注册于 `eng/ci/checks.json`：`CHK-UNIT` 的步骤 `TESTKIT-LIST`（`python3 eng/tools/testkit/check_testkit.py --list`）。
- 检查项条目见 `docs/engineering/01_CHECKS.md`，门禁分级见 `docs/engineering/03_GATES.md`。
