# graph

运行图渲染工具：从 typed plan 与 trace 事件生成 DOT/SVG/JSON 运行图并机器复验。

## 职责边界

- 放：计划图与真实运行图的渲染、图与 trace 的一致性复验。
- 不放：trace 采集实现（在 `lib/infrastructure/`）、检查项注册表（在 `eng/ci/checks.json`）、产品执行（本工具只生成文档与图）。

## 内容

- `render_run_graph.py` —— 静态声明图取自 typed plan、真实运行图取自 trace 事件；真实入口、调用计数、workers、provider、耗时、动态库与产物哈希一律取自观测事件，计划声明属性单独标注来源；`--verify` 重放 trace 逐项比对图节点集合与调用计数。
- `__pycache__/` —— Python 字节码缓存，不入库。

## 上游

- 本目录工具未注册于 `eng/ci/checks.json`（生成与复验按需执行）。
- 本目录在 `docs/ci/` 无逐项对应篇；证据落位规范见 `docs/ci/04_ARTIFACTS.md`。
