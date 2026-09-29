# arch

架构面静态工具：生产执行清单生成、线程预算检查、导出流生产可达检查与构建图文档生成。

## 职责边界

- 放：生产源码面的清单生成、线程登记与导出流静态检查、构建图文档机器块生成。
- 不放：构建图机器读取器（在 `eng/ci/cmake_graph.py`）、清单与文档正文（在 `docs/architecture/`）、检查项注册表（在 `eng/ci/checks.json`）。

## 内容

- `build_production_execution_inventory.py` —— 生产执行清单 CSV 生成器：只读源树、两次运行逐字节一致、支持 `--out` 使校验在临时目录比对、可执行目标面补读根构建图目标集。
- `check_thread_budget.py` —— 未登记线程创建 / 硬编码线程数 / 私有线程池扫描：扫描面由权威来源派生且锚存活，登记键悬空即判红。
- `check_p3_export_stream_prod.py` —— 导出流生产路径静态检查：生产 export 链必须引用流式调度器，附 `--self-test` 正负例面。
- `gen_build_graph_doc.py` —— 从真实构建图导出构建文档的机器块，供门逐行比对。
- `__pycache__/` —— Python 字节码缓存，不入库。

## 上游

- 注册于 `eng/ci/checks.json`：`CHK-RESOURCE` 步骤 `THREAD-BUDGET`、`CHK-P3-EXPORT-STREAM-PROD` 步骤 `STATIC-P3-EXPORT-STREAM-PROD` 与 `SELFTEST-P3-EXPORT-STREAM-PROD`；两个生成器不以命令面注册。
- 检查项条目见 `docs/engineering/01_CHECKS.md`，门禁分级见 `docs/engineering/03_GATES.md`。
