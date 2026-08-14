# Round 4 — Performance / Concurrency / Resource Review

## 数据（3 次运行，本机，CPU route）

```text
stage2 satellite 20 帧（auto→linear_fit，24 tile）:
  52.83s / 43.80s / 43.02s  -> median 43.80s, max 52.83s
stage2 n2 overlap（auto→percentile，285 tile，100% UNDERDETERMINED）:
  42.91s / 41.60s / 41.60s  -> median 41.60s, max 42.91s
sampler RealHipsControlSampling : 9.2s（gate 计时）
sampler G6LocalSnrAvailabilityThreeZones : 13.4s
browser stretch-only redraw : p50 1.41ms / p95 2.60ms
browser peak RAM : 17 MB（有界 LRU）
```

完整 before/after 见 reports/performance.md 与 evidence/performance_*.json。

## 检查项

| 项 | 结论 |
| --- | --- |
| O(N²) | sampler catalogue 全扫描（O(cells×stars)）→ dec 排序索引（O(logN+k)），已修 |
| repeated allocation | rejection auto 路由移出 pixel loop；workspace API 预留（P3 backlog） |
| repeated FITS open/decode | stage2 每 tile 打开一次复用（原有）；未新增重复打开 |
| cache bound | browser LRU 有界；soak 峰值 17MB |
| OpenMP race | rasterize 并行区只读 tile 缓存 + 输出独立索引（原有验证）；本轮未改 |
| false sharing / oversubscription | 未触及（无新增并行结构） |
| GPU transfer | ACR 仅 sigma 路由；等价测试 PASS；本轮卫星门走 CPU route |
| cancellation / timeout | oracle 脚本 subprocess 全部显式 timeout（V15 修复） |

## 科学等价

- gate 59/59（含 identity/copy、CPU/ACR、serialization、真实数据回归）；
- 卫星门 clean vs auto mosaic：背景/星点 bias=0（trail 帧被拒后输出与
  clean 一致）；
- stage2 无 RNG，同输入确定（identity 测试覆盖路径级确定性）。

## 回退审查

无可比同配置 before/after 的无解释回退；sampler 为 >60× 提升，stage2
端到端在 40-45s 量级（20 帧/285 tile 两种负载），符合 V14 基线量级。

```text
ROUND4=PASS
```
