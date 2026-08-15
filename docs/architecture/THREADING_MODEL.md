# Threading Model

## 分层

- 编排层：orchestrator/stage2 顺序 stage，内部 OpenMP parallel-for。
- 科学模块：内部 OpenMP parallel region；每模块文档化 parallel/shared/
  thread-local/reduction/determinism/float accumulation order。
- ACR：work_pool + device_executor 调度；CPU reference 与 GPU 等价契约。
- 浏览器：Qt 主线程 + 后台 I/O 线程；renderer 只读共享数据。

## 约定

- 禁止库内修改全局 OpenMP 设置；线程数由 run context 配置。
- 计数器：atomic 或 thread-local 聚合（禁止裸 data race counter）。
- 浮点累积顺序固定（确定性输出）；reduction 顺序文档化。
- cache（dense UPM、Gaia 查询缓存）必须线程安全或单线程互斥访问。

## 契约

ENG-THREAD-001..003（S2 注册）。
