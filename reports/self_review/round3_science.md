# Round 3 — Science（V18R2）

## Oracle / 回归

```text
candidate oracle      9003/9003 PASS（-O2 与 -O3）
edge-cross / sliver   PASS（-O2；-O3 与 HEAD 逐案例一致——1e-19 尺度
                      编译环境漂移，非 V18R2 引入）
test_spherical_overlap 76/76 PASS（-O2）
Phase2 synthetic gate  74/74 PASS（V17 保持，未回归）
```

## 单帧科学等价（V18R2 vs V17 产物）

```text
signal  tile：max_abs 5.6e-9 / mean 1.6e-11（FP32 舍入级）
support tile：max_abs 4.6e-6 / mean 4.4e-9
tile 结构 57/57 一致（signal+support+hierarchy+MOC）
```

## gaia 查询等价

```text
0.86° 锥查询：修改前后 stars=899 逐颗 ra/dec/mag 完全一致
（stash 旧 DLL vs 新 DLL 导出文件逐行对比）
```

## 说明

- 不引入 BASS/2×2/3×3 真实数据（V19）；
- 未改变任何冻结算法（剪枝/缓存/调度均不影响科学数值）。

```text
ROUND3=PASS
```
