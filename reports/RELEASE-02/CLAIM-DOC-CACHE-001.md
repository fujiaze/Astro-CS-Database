# CLAIM-DOC-CACHE-001 — CACHE_POLICY.md 实现描述同步（事实性，非设计变更）

## 变更内容
`docs/architecture/CACHE_POLICY.md` Drizzle geometry cache 行的实现描述：
`deque lru + unordered_map 实现` → `侵入式双向链表 LRU + unordered_map 索引, 命中路径 O(1)`。

## 依据
- 实现改动：`spherical_overlap.h:321-343`、`spherical_overlap.cpp:1391-1457`（PERF-DRZ-IMPL 的 C1 项）；
- 原实现命中路径为 **O(capacity=8192) 线性扫描**（PERF-DRZ 实测 1.487 亿次命中），改为 O(1) 触摸。

## 影响面
- **缓存语义（容量 / 身份 / 失效 / 线程模型）完全不变**：容量仍 8192；身份仍 target ipix；
  仍每次 drizzleTiled run 起始 clear()；仍线程私有；`hits()/misses()` 仍可观测；
- **不影响任何科学数值**：geometry 是 (nside,ipix) 的确定性函数，无跨调用状态；
  新链表 MRU/LRU 语义与旧 deque **逐项一致** ⇒ 命中/未命中序列相同；
- 位级回归（重建后实测）：`p1drz_taskset_invariance.sh` 预算 1/2/4/8/16 的 `norm.hiss`
  sha256 全部 = `ab85199b0acf6aa3e2a912f442fa5233679ba88ff599b06c1db43c7eda59cb62`
  （**与改动前基线一致**）；`p1drz_merge_pipeline_lock.sh` fp64 全 PASS；
- ctest 461/461。

## 授权
`CACHE_POLICY.md` **不在** RELEASE-02 文档包 `authorized_edits` 清单内；
本次为**事实性实现描述同步**（非科学/设计变更），按 `ENGINEERING_SPEC.md §3` 记 claim。
若负责人认为该文件不可改，可回退本行并以代码注释承载。
