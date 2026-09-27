# checks

本目录存放标准注册表的机器校验脚本。

## 职责边界

- 放：针对 STANDARDS_REGISTRY 的检查脚本及其运行所需登记。
- 不放：注册表正文（在上级目录 STANDARDS_REGISTRY.md）；其他门禁检查器（在 eng/ci/）；科学门表校验（在 eng/tools/）。

## 内容

- `check_standards_registry.py` —— 校验注册表的版本冻结、条款锚与符合状态取值域。

## 上游

上游：docs/ASTROCS_DESIGN.md §8.4（模块与 ABI）、附录 B（外部标准与文献）。
