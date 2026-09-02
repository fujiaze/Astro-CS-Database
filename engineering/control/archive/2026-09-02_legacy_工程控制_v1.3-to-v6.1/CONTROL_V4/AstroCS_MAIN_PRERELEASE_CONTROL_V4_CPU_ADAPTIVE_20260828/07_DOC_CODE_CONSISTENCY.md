# 科学、算法、架构、接口、代码一致性

## 单一链

```text
SCI definition
  -> ALG derivation
  -> ARCH/API contract
  -> SRC symbol
  -> SYN/UNIT/INTEGRATION test
  -> runtime/profile evidence
```

## 机器真相源

- `docs/contracts/science_contracts.json`：量、单位、公式、invariant；
- `docs/contracts/algorithm_contracts.json`：SCI引用、步骤、复杂度、误差；
- `docs/contracts/api_contracts.json`：AST signature + 语义；
- `docs/contracts/traceability.csv`：全链；
- compile database/build graph：实际 target/source/define；
- test registry：实际可运行 test ID。

Markdown 是面向人的解释，机器合同是核对源；二者由 checker 比较，不允许各自漂移。

## 必查不一致

- 文档声称并行但生产调用链未接；
- 文档写 CPU/GPU/Mixed 但 ACR 已 deferred；
- header签名/错误码/所有权与 API 文档不同；
- SCI单位与变量名/输出 BUNIT 不同；
- ALG复杂度漏 frame/control/iteration；
- config默认值在文档/schema/parser重复且不同；
- TEST ID存在但 binary不可构建或命令不可运行；
- checker只检查关键词存在而不检查值。

最终 `traceability.csv` 的每条核心链必须 PASS，禁止 `VERIFIED` 模板批量填充。

