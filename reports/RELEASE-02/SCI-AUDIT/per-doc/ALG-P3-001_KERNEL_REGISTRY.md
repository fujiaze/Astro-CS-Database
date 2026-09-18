# per-doc: docs/algorithms/v6/phase3/ALG-P3-001_KERNEL_REGISTRY.md

## 发现

| 编号 | 行 | 主张 | 独立证据 | 判定 | 三分类 |
|---|---|---|---|---|---|
| R2-K-01 | §1 注册表 | kernel_id 集 {nearest,bilinear_4quad,bilinear_area_overlap_exact,bicubic,lanczos}；生产默认 bilinear_area_overlap_exact | 与插件/DATA_SEMANTICS/CLI 的 nearest|bilinear 词表及默认 bilinear 不一致 | CONTRADICTS | DESIGN-OK-DOC-WRONG |
| R2-K-02 | §3.2-3.4 | 误差界 (h²/8)(max|Fxx|+max|Fyy|)；Oracle max_interp_err=0.0273955 ≤ 0.0277778 | 本轮未独立复跑（无构建） | INSUFFICIENT-EVIDENCE | DESIGN-OK-EVIDENCE-MISSING |

## 处置建议

- 统一 kernel_id 词表与生产默认；把 bilinear 明确映射到某一注册核。
- 用 Python 独立复算误差界与两条负向 mutation，补入库证据。
