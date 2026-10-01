# arch

架构面静态工具：线程预算检查、导出流生产可达检查与构建图文档生成。

## 职责边界

- 放：线程登记与导出流静态检查、构建图文档机器块生成。
- 不放：清单正文与文档正文（在 `docs/engineering/`）、检查项注册表。

## 内容

- `check_thread_budget.py` —— 未登记线程创建 / 硬编码线程数 / 私有线程池扫描：扫描面由权威来源派生且锚存活，登记键悬空即记为缺陷。
- `check_p3_export_stream_prod.py` —— 导出流生产路径静态检查：生产 export 链必须引用流式调度器，附 `--self-test` 正负例面。
- `gen_build_graph_doc.py` —— 从真实构建图导出构建文档的机器块，供逐行比对。
- `__pycache__/` —— Python 字节码缓存，不入库。

## 上游

- 检查项条目与门禁分级见 `docs/engineering/BUILD_GRAPH.md` 与 `docs/engineering/TEST_STANDARD.md`。
