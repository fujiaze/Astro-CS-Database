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

- docs/architecture/THREADING_MODEL.md；
- ENG-THREAD-* 契约（S2 注册）。
