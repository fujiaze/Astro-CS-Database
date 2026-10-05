# arch

构建图文档生成工具：从真实构建图导出构建文档的机器块，供逐行比对。

## 职责边界

- 放：构建图文档机器块生成。
- 不放：清单正文与文档正文（在 `docs/engineering/`）。

## 内容

- `cmake_graph.py` —— 构建图解析。
- `gen_build_graph_doc.py` —— 从真实构建图导出构建文档的机器块，供逐行比对。
- `__pycache__/` —— Python 字节码缓存，不入库。

## 上游

- 构建图口径见 `docs/engineering/build/BUILD_GRAPH.md`。
