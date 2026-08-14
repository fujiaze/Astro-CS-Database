# Large-Scale Rejection（V17 True Final Freeze）

## semantic ID

```text
astrocs.large_scale_rejection.v1
```

（WBPP Large-Scale Pixel Rejection 的 AstroCS 自有实现；
`PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED`。）

## 语义

在 per-frame pixel-level rejection mask（低/高侧独立）上做
connected-component 后处理：

```text
1. 每帧低/高 mask 分别做 8-连通分量标注；
2. 只有分量大小 >= min_structure_pixels（默认 8）的结构视为大尺度；
3. 合格结构按 Chebyshev 邻域扩张 grow_radius 像素（低/高独立，默认 2）；
4. 扩张只增不减：compact cosmic（2×2=4 < 8）与星点稀疏 reject 不生长；
5. 最终 mask 应用回原始 calibrated 科学值积分（与 pixel rejection 同一
   accepted 语义）。
```

默认关闭（enabled=false）与 WBPP 2.9.1 `largeScaleClipLow/High=false`
默认一致。

## 配置（schema/template/parser 四源一致）

```json
{"enabled": false, "min_structure_pixels": 8,
 "low_grow_radius_pixels": 2, "high_grow_radius_pixels": 2}
```

## 测试

```text
单元（V17LargeScale* 5 项）：
  - 细线 grow ±2（含端点）、±3 不扩张；
  - compact cosmic 不生长（自身保留，无扩张）；
  - sparse 星点噪声（分量 1）不生长；
  - low/high 独立；
  - disabled no-op；非法参数拒绝。
E2E（受控 20 帧）：
  - thin satellite：pre=88055 post=91134 grown=3079（线带扩张）；
  - compact cosmic：grown=0（不扩张）。
```

## 目标结构（本轮验证）

```text
细卫星线：grown（recall 保持 1.0 且 band 加宽）
宽卫星线/飞机线：同一 grow 语义（分量 >> min）
compact cosmic：不生长（不误伤）
星点：不生长（星点不形成 rejected 大分量）
faint extended nebula：不误伤（无 rejected 分量）
```

```text
LARGE_SCALE_REJECTION = IMPLEMENTED + TESTED
```
