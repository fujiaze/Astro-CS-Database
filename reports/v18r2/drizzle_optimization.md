# Drizzle 优化（V18）

## 已实施（PERF-001..005、010）

```text
PERF-001 fine per-pixel profiler 默认关闭（ASTROCS_DRIZZLE_FINE_PROFILE=1
         显式启用）；overlap 路径计数器同门控；默认仅粗粒度阶段计时
PERF-002 thread-local scratch 复用（candidates / drop corners / DropGeometry）
         ——首像素 reserve 后零堆分配；修复 clip_normals 复用未清空 bug
PERF-003 nside>=256 生产路径固定 4 角 boundary array（无堆分配，逐位等价）
PERF-004 adaptive 边判定 acos→dot（普通像素 0 次 acos；quick-reject 保持
         原 acos 判定——1e-19 sliver oracle 证明 dot<cos 不等价）
PERF-005 DrizzleRunContext 缓存 hp_res_rad / cos 阈值 / shift/mask
PERF-010 SNR model 控制点 RAII（消除 HiPS-only 路径每帧 malloc 泄漏）
```

## 科学等价

```text
candidate oracle 9003/9003 PASS（-O3）
edge-cross / sliver oracle（ORA-101）与 test_spherical_overlap：
  -O2 编译：全 PASS（edge 漏报 0；overlap 76/76）
  -O3 编译：与 HEAD 源码在当前编译器下逐案例一致（2 个 1e-19 尺度 sliver
  为编译环境数值漂移：8/6 旧编译器 PASS、当前编译器 FAIL、-O2 稳定 PASS）
freeze 测试：-O2 39/40（唯一失败为既有 HISS writer 环境问题，老 exe 同样）
完整单帧 E2E：signal max_abs=5.6e-9、support max_abs=4.6e-6（FP32 舍入级）
```

## 性能（合成 4096² TAN-SIP 6.3"/px，nside=65536，16 线程）

```text
OLD 8/6 编译器构建      27.85s（环境基线，不可复现）
HEAD 源码 + 当前编译器   33.79s
V18 优化 + 当前编译器    30.68s
→ 同编译器下 V18 vs HEAD = -9.2% Drizzle 引擎耗时
```

> 8/6 旧二进制快 21% 是编译环境差异（MSYS2 g++/math 库更新），非算法回归；
> 与 edge oracle 的 FP 漂移同源，已在 Round5 如实记录。

## 遗留（待 profile 决策）

```text
PERF-006 tile map reserve/merge（threadTiles unordered_map 每线程 + 单线程
          merge）——先 profile 后处理
PERF-008 nested→FITS LUT（当前位运算已高效，profile 决定）
```
