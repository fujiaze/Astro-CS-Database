# Round 2 — Semantic / Single-Path Review

## 方法

独立重读源码 + 全仓库 grep，验证 `reports/semantic_path_inventory.csv`
声明（不依赖 Round1 结论）。

## 验证结果

### 1. rejection 生产调用链（单路径）

```text
生产（stage2.cpp:826）: p2_reject_stack_ex（explicit plan）
ACR backend        : acr_kernels.cpp:133 → p2_reject_stack_ex（同一 contract）
COMPAT（测试/工具）: rejection_cli.cpp / synthetic_gate / sanitize_driver
                     → p2_reject_stack（adapter；生产不调用）
```

grep 确认 `stage2.cpp` 无 `p2_reject_stack(` 调用 → 生产单路径 ✓。

### 2. HEALPix 映射（单实现）

- `astrocs::healpix`（lib/common/healpix/healpix_core.cpp）为唯一手写实现；
- 浏览器 healpix_math.cpp 已委托 canonical（本地 ang2pix/pix2ang 已删除，
  c0fa750）；全仓库 grep 无 common 之外的 handwritten ang2pix ✓；
- `ang2pix_ring` 全仓库 0 命中（ring 未迁移，与 DATA_SEMANTICS 一致）✓。

### 3. 配置默认（单源）

- `tools/config_consistency_check.py` PASS（struct/parser/schema/template
  全部一致；checked_keys 30 项，mismatches=0）；
- rejection 旧字段 low/high/max_iterations/min_samples 仅 deprecation
  adapter（parser 打印 warning），不构成第二默认 ✓。

### 4. 遗留 fallback 不改变科学

- `P2_STATUS_UNDERDETERMINED` 全接受但记录（n<=2 / <min N），不静默换算法；
- ACR/CUDA 与 CPU 同一 contract（等价测试 PASS），非"第二实现"。

### 5. 归档/冻结边界

- stf.js / stf.py：仅在 `lib/healpix_db/archive/`（healpix_browser_web 等）
  → ARCHIVED，非生产；
- healpix_stack（gradient/stack/winsorized）：冻结模块，生产 Phase2 入口
  为 astrocs-stage2，未触碰。

## 结论

```text
two production implementations  = 0
two defaults                    = 0
legacy fallback changes science = 0
duplicate production science paths = 0
ROUND2=PASS
```
