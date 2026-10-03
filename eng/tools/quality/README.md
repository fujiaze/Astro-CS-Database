# quality

本目录是产品比对、图像质控、资源监控与派生文档生成的辅助工具集。

> 现态说明：原有的机器质量检查器族、CI 驱动器与检查项注册面已退场，本目录现存件均为按需执行的
> 诊断与生成工具；下文只描述现存件。

## 职责边界

- 放：产物比对、帧质控网格、资源监控采样，以及块流规范、模块页、运行图与源码索引的派生生成。
- 不放：验收判据与门禁、测试用例本体。

## 内容

- `compare_products.py` —— 产品比对。
- `frame_qc_grid.py` —— 帧质控网格。
- `resource_monitor.py` —— 资源监控采样。
- `plane_chunks.py`、`plane_stretch.py` —— 平面切块与拉伸辅助。
- `gen_block_flow_spec.py`、`gen_module_readmes.py`、`gen_run_graphs.py`、`gen_source_index_v61.py` —— 生成器：块流规范、模块页、运行图、源码索引。
- `strip_version_comments.py`、`update_audit_status.py`、`v19r3_traceability.py` —— 源码注释清理、审计状态更新与追溯表生成。
- `isa_sites.json` —— ISA 站点登记数据。
- `tsan/` —— ThreadSanitizer 抑制清单。
- `__pycache__/` —— Python 字节码缓存，不入库。
