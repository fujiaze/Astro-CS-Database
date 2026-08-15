# Repeated Work 审计（V18R2）

## 已消除的重复工作

1. **gaia 极区查询全树遍历**（最大项）：0.86° 锥查询每次读 16.3-26GB mmap
   数据、遍历 16-30 万节点（原 bbox 因 RA 环绕退化）；修复为极投影平面
   剪枝后 1224 节点 / 35MB / 0.03s。星集 899/899 逐颗一致（stash 前后 DLL
   对照）。
2. **Drizzle 每像素 sin/cos 重复**：4 角 radec→Vec3 每像素 8 次，相邻像素
   共享顶点 → 行级 Vec3 缓存（每行 2×(W+1) 次转换）。
3. **Drizzle 快速拒绝 4.23 亿次 acos**：仅阈值判定 → 安全余量 dot 预判
   （lim+1e-9 rad，拒绝集为原集严格子集；边界带仍走 acos，位级一致）。
4. **Drizzle 每像素堆分配**：candidates/drop corners/DropGeometry 全部
   线程本地复用（PERF-002），首像素后零分配。
5. **HiPS 每 tile 4 个 dtype 缓冲**：只分配当前 dtype，ProductSet 跨 tile
   复用（PERF-007）。
6. **HiPS hierarchy FITS 反查**：hierarchy 累加按 NESTED 序直通，不再每
   pixel 一次 nested_local_to_fits_index（PERF-009）。
7. **SNR model malloc/free 泄漏**：HiPS-only 路径 free 不执行 → RAII vector
   （PERF-010）。
8. **进程退出 40s**：36GB 工作集释放 + DLL 卸载 —— 根因是 gaia 查询读入
   36GB mmap 页；修复后 RSS 1.2GB，退出 0.7s。

## 保留的重复（有正当理由）

- Drizzle 候选查询每像素独立枚举（相邻包围盒重叠）：零漏 oracle 约束下
  批量滑动窗口风险高，暂缓；每候选成本已降至 ~100ns 位操作。
- CALIBRATE 每帧读 master（bias/dark/flat ~235MB）：输入数据语义，冷读
  一次后系统缓存命中。
- queryDisc 回退（极冠/跨 face）保留保守路径：R13 零漏证明所需。

## 未发现的问题

- 无外部网络等待（Gaia 本地 mmap）；
- 无锁竞争（OpenMP static 调度，线程本地累积）；
- 无 O(P×C×H) 超标结构（C≈25 常界，H 已直通）。
