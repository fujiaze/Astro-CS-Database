> **ARCHIVED_NON_NORMATIVE** — GOV-002 归档历史技术文档，不再作为当前权威。
> 替代文档：docs/science/DRIZZLE.md、docs/algorithms/DRIZZLE_GEOMETRY.md

# Drizzle 工程文档 (V19)

## 引擎结构

```text
hp_drizzle_api.cpp        C ABI (hp_drizzle_run_hips 生产末端)
drizzle_engine.cpp        6 步流水线 (pixfrac → WCS/SIP → 细分 → 面积 →
                          候选查询 → 球面重叠累加)
astro_sphere_sink.cpp     TileAccumulator → AIO HiPS 直写
spherical_overlap.cpp     球面几何 / 候选查询 / 重叠面积
wcs_sip.cpp               TAN+SIP 映射
```

## V19 新增

### 方差传播 (DRZ-014, P1-003)

```text
输入: 帧 "variance" 块 (FLOAT32/64 [H,W], NoiseWeightModelV1 产出)
累加: sumVarNum += v_j × w_jp²     (TileLeafAccumulatorT 新增字段)
输出: variance_p = sumVarNum / sumArea² ;  ivar_p = 1/variance_p
      产品: <out>/variance/ 与 <out>/ivar/ (AIO_HIPS_PRODUCT_VARIANCE=8,
      AIO_HIPS_PRODUCT_IVAR=16; hierarchy 归约同叶级公式)
```

### 操作计数 (DRIZZLE_OPTIMIZATION)

每个运行输出 `<out>/operation_counts.json`:

```text
source_pixels  candidates  true_overlaps  quick_rejects  pix2radec_calls
boundary_builds  geometry_builds  spherical_overlap_calls  tile_lookups
hot_loop_heap_allocations  candidate_efficiency  overlaps_per_source_pixel
```

计数线程本地累加, 结束时合并; 热循环堆分配目标 ≈0 (V18 线程本地复用)。

## 性能基线 (V18R2, 不重复刷 batch)

```text
Phase1 ~67.35 s/frame ; Drizzle ~64 s/frame (16-frame batch)
V19 只跑 representative single frame + microbench
```

## 科学门 (DRZ-001..016)

DRZ-001..013 沿用冻结 oracle (候选 9003 / 独立 Oracle 5502 / 科学矩阵
37 / 反向 5 等); V19 新增:

```text
DRZ-014 variance propagation  PASS (variance(α²v)=α²·variance, worst_rel<1e-4)
DRZ-015 output covariance      PASS (SNR-012, 已表征)
DRZ-016 optimized cache 不改科学 PASS (signal 在 variance 输入下逐位一致)
```

## 已知限制

- 相邻输出像素协方差由 pixfrac/resampling 引入 (SNR-012 量化)
- HISS 为 legacy 通道, 不携带 variance (variance 仅 HiPS 产品)
