# HiPS Browser

## 渲染架构（V14 冻结）

```text
HiPS tile decode/cache → float viewport buffer → display transform/STF → Qt paint
```

STF 改变**只重做 display transform**，不重新 sky→HEALPix 采样/FITS decode。

## Auto STF

- `Auto Global`（默认）：对 dataset/layer 建一次 robust STF，pan/zoom 稳定；
- `Auto View`（可选）：当前 viewport，debounce + 后台 worker；
- `--stf-mode global|view`（CLI）切换两种模式；
- `Lock STF` / `Reset` / manual black/white/midtones；
- 菜单：Auto Stretch (Ctrl+A) / Reset STF (Ctrl+R) / Lock STF (Ctrl+L)；
  CLI 另有 `--lock-stf`；诊断用 `browser_cli --stf-bench`（STF 延迟）与
  `--stf-lock-probe`（锁定行为验证）；
- support 层固定 [0,1] 线性显示，不走 signal STF。

## Robust signal auto stretch

```text
finite && support>0 → median/MAD → bright-tail iterative clipping →
robust black/white → asinh/MTF
```

亮星不主导背景估计。
