# HiPS Browser

## 渲染架构（V14 冻结）

```text
HiPS tile decode/cache → float viewport buffer → display transform/STF → Qt paint
```

STF 改变**只重做 display transform**，不重新 sky→HEALPix 采样/FITS decode。

## V15：唯一 DisplayTransformState

- `core/stf_engine.h` 定义唯一 `DisplayTransformState`（mode =
  AutoGlobal/AutoView/Manual；locked；black；white；midtones；curve；
  compression；generation）。UI（MainWindow/STFPanel/STFBar）只编辑 state；
  renderer（HipsSkyView::rasterize）只消费 state；任何变更 `generation+1`
  （丢弃过期异步结果用）。
- 已删除分散状态字段（preset_/auto_range_/auto_view_/stf_locked_/
  manual_*）；MainWindow 不再持有重复的 stf_locked_/hips_auto_range_。
- support 层固定 **linear [0,1]**（V15 移除 sqrt(support)）。
- HEALPix 位置/层级映射全部委托共享 `astrocs::healpix` core（浏览器第二套
  手写实现已删除；query_disc/ud_grade 保留为构建在 canonical 之上的工具）。

## Auto STF

- `Auto Global`（默认）：对 dataset/layer 建一次 robust STF，pan/zoom 稳定；
- V14 v2：Auto Global 首次扫描**全部 leaf tiles**（每 tile 64×64 均匀采样，
  进程内缓存，一次会话只扫一次）后取 p1/p99 分位，与 V13 fixed stretch
  （1%/99%）视觉一致；p99 被极端亮星/卫星线污染（> median+30×MAD）时
  退回 12×MAD 剔除后重算；
- `Auto View`（可选）：当前 viewport，debounce + 后台 worker；
- `--stf-mode global|view`（CLI）切换两种模式；
- `Lock STF` / `Reset` / manual black/white/midtones；
- 菜单：Auto Stretch (Ctrl+A) / Reset STF (Ctrl+R) / Lock STF (Ctrl+L)；
  CLI 另有 `--lock-stf`；诊断用 `browser_cli --stf-bench`（STF 延迟）与
  `--stf-lock-probe`（锁定行为验证）；
- support 层固定 [0,1] 线性显示，不走 signal STF。

## Robust signal auto stretch

```text
finite && support>0 → (全 dataset 或 viewport) 采样 →
median/MAD 污染守卫 → p1/p99 robust black/white → asinh/MTF
```

亮星不主导背景估计；真实结构（暗星/星系）不被 MAD 过紧的标尺过曝。
