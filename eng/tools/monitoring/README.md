# monitoring

运行监控与资源约束工具：内存看门狗、受监控执行器、日志合同检查与并发/资源探针。

## 职责边界

- 放：重计算的进程树内存上限看门狗、子进程资源采样与超时控制、并发扫描与节点瀑布分析。
- 不放：被监控的计算任务本身。

## 内容

- `mem_guard.py` —— 内存看门狗：按进程树 RSS 采样，超限只杀该命令进程组（退出码 137），结束打印峰值留证；超时用 `--timeout`，不套外层 `timeout(1)`。
- `run_monitored.py` —— 受监控执行器：argv 数组启动子进程，采样进程树 CPU/RSS/PSS/IO/线程/进度，支持超时杀进程组并输出 JSON 证据。
- `concurrency_sweep.py` —— 并发档位扫描。
- `resource_probe.py` —— 真实资源探针。
- `node_waterfall.py` —— 节点瀑布分析。
- `__pycache__/` —— Python 字节码缓存，不入库。
