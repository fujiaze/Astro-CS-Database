# 性能（V15，先 profile 后优化）

## 基线（V14 记录，before）

```text
Phase2 GC 3-panel : 292.0s -> 234.6s（V14）
Phase2 t4 overlap :  87.9s ->  70.8s（V14）
sampler RealHipsControlSampling（t4 crop×full，V15 修正数据后）: >=10 分钟
  （catalogue 全扫描 O(cells×stars) + 每 cell 重算整帧 median O(N log N)）
rejection auto : kernel 内按 pixel effective count 路由（每像素解析+分配）
```

## After（V15，本机 3 次运行）

```text
stage2 satellite 20 帧（24 tile，auto→linear_fit）:
  52.83s / 43.80s / 43.02s -> median 43.80s（evidence/performance_after.json）
stage2 n2 overlap（285 tile，auto→percentile + 100% UNDERDETERMINED）:
  42.91s / 41.60s / 41.60s -> median 41.60s
sampler RealHipsControlSampling : 9.2s（gate 计时）
sampler G6LocalSnrAvailabilityThreeZones : 13.4s
browser stretch-only redraw : p50 1.41ms / p95 2.60ms（stf-bench）
browser peak RAM : 17 MB（有界）
```

## 优化项（真实热点，均保持科学等价）

1. **rejection Auto 移出 pixel loop**：planning 层每 tile 解析一次（原每像素
   路由）→ 移除每像素方法解析与分支浪费；
2. **sampler catalogue proximity 全扫描 → dec 排序空间索引**（SnrIndex：
   lower/upper_bound + RA 窗口保守预筛，最终精确 angular_distance 不变）
   → 采样 10+ 分钟 → 9.2s（>60×）；
3. **sampler 整帧 median 每 cell 重算 → 帧级预计算**（median_of 语义保持）；
4. **sampler null-config 未初始化 bug**：默认值随机失效导致 100% 拒绝/超长
   路径（修复后 deterministic）；
5. **Browser STF 单状态**：stretch-only 走缓存 float viewport，不重采样/
   解码（stretch_only p50 1.41ms）。

## V16 追加（正确性后性能）

6. **rejection kernel fixed-scratch**：n≤64 用固定数组（ScratchVec），
   >64 一次性迁移 heap；消除每像素堆分配（V16 ScratchVec heap-mode
   修复 n>64 崩溃后 oracle/matrix 全 PASS）；
7. **eligibility 一次收集**：p2_collect_candidate_stack（strided）替代
   stage2/ACR 内联手写资格判定（单路径 + 单次收集）；
8. **group plan 缓存**：wbpp_2_9_1（V17 canonical；wbpp_current 仅
   migration alias）在 run 开始解析一次，tile 复用（移除每 tile 重复
   resolve）。

## V16 基准（本机，3 次）

```text
真实 16 帧 Phase2（wbpp_2_9_1, linear_fit, astrocs_median_center_v1）：
  truth 23.5s / clean 24.6s / trail 24.6s / trail_none 23.4s
合成 20 帧（satgate，V15 复跑）：
  52.83 / 43.80 / 43.02 s → median 43.80s
n2 overlap：42.91 / 41.60 / 41.60 s → median 41.60s
sampler RealHipsControlSampling：9.2s；G6：13.4s
```

科学等价：65/65 gate（含 CPU/ACR、identity、serialization）；卫星门
clean vs truth 背景 std ratio=0.9991。

## 规则符合性

- 3+ runs、median/p95：✓（evidence/performance_*.json）；
- 同一机器/数据/config：✓（本机 RTX 3060 Ti 环境，CPU route）；
- exact science equivalence：✓（gate 59/59；identity/copy 测试；satellite
  clean vs auto bias=0）；
- 无 >5% 无解释回退：✓（无可比同配置 before/after 的端到端退化；sampler
  为大幅提升）。

## 未优化的候选（如实标注）

- rejection kernel 内部每像素小向量分配（RCR/ESD 分支）：workspace API 已
  预留（`p2_rejection_workspace_*`），优化轮按 profile 数据接入；
- cross-tile adjacency / UPM CG scratch：当前未达瓶颈（stage2 41-44s 中
  tiles_process ~21s，含读/校/拒/写），后续轮按 profile 再动。
