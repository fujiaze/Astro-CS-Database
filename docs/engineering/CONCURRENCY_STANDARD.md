# Astro Celestial Sphere Database（ACSD） Concurrency Standard

> 上游：ASTROCS_DESIGN.md §8.4（模块与 ABI）

## 全局设置与共享状态的所有权

- 库内随意修改全局 OpenMP setting（omp_set_num_threads 等）— 由 run context
  统一控制（orchestrator stage 级）。
- undocumented static mutable cache — 必须容量/身份/失效/线程模型。
- data race diagnostic counter — 计数器必须 atomic 或 thread-local 聚合。
- shared mutable scratch 无 ownership。

## 每个并行模块文档必须写

parallel region、shared、thread-local、reduction、determinism、
float accumulation order（顺序/成对/多路，结果确定性要求）。

## 默认

- 生产并行默认确定性：相同输入 → 相同输出；浮点累积顺序固定。
- 线程数：外部可配置，默认 min(可用核, 配置上限)；取值来源 = 配置（16 不作硬编码值）。

## 关联

- docs/engineering/execution_options_contract.md；
- ENG-THREAD-* 契约（S2 注册）。

## 异步与并行的适用范围

| 场景 | 允许的并发形态 |
|---|---|
| 科学计算路径（kernel、归约、迭代求解） | 只用显式并行区（OpenMP / 线程池）；**不用 `async` / `future` / promise 链，不嵌套并行**——科学路径的并发度由线程预算唯一给定，嵌套并行绕过预算并使归约顺序不可冻结 |
| 能隐藏延迟的 I/O 与落盘 | 允许异步：I/O 预取、压缩、落盘写。队列必须同时具备容量上限、背压、取消、超时与错误传播五要素，**不使用无界队列** |
| 资源调度 | 一个进程一个资源调度器与线程预算源（最高设计 §9）；模块不硬编码 worker 数、不私建长期线程池 |

浮点归约顺序由科学正本冻结；并行开关不改变科学数值，等价判据是事前冻结的浮点容差（最高设计 §9）。
