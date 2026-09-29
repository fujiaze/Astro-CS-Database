# observability

本目录是可观测性实现处：日志事件、资源监控与性能探针在这里集中。

## 职责边界

- 放：日志事件定义与 schema（logging/）、资源与轨迹监控（monitoring/）、探针实现（probes/），以及本目录的迁移登记文件。
- 不放：门禁与监控字段的注册（eng/ci/）、日志契约正文（docs/engineering/LOG_AND_ERROR_CONTRACT.md）、科学算法（lib/algorithms/）。

## 内容

- logging/ —— 日志事件脚本与 schema（log_event.py、log_event_v1.schema.json）。
- monitoring/ —— 监控采集与运行器（monitor.py、runner.py、linux_procfs.py、windows_pdh_etw.py、trace_feed.py）。
- probes/ —— 探针实现（include/astrocs/probe.h、src/probe.cpp）。
- PENDING.md —— 本目录内容与目标位置的迁移登记文件。

## 上游

上游：docs/ASTROCS_DESIGN.md §8.4（infrastructure/observability：日志、事件、运行图、性能探针、资源监控）；ENGINEERING_SPEC.md §11（日志、诊断与错误）。