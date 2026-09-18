# per-doc: docs/science/DRIZZLE.md（SCI-DRZ-001，FROZEN；RELEASE-02 已订正）

## 发现

| 编号 | 行 | 主张 | 独立证据 | 判定 | 三分类 |
|---|---|---|---|---|---|
| R2-CC-01 | §2/§5/§7/§11（订正后） | 面亮度保持 S_p=Σ_j B_j a_jp/Σ_j a_jp；legacy w=a/A_drop 给 B0/pixfrac² | Fruchter & Hook 2002 式(3)(7)（DOI 10.1086/338393 ✅，arXiv:astro-ph/9808087）；独立代数复算 | CORRECT（订正成立） | DESIGN-OK-DOC-WRONG（实现仍 legacy=DISP-DRZ-009） |

## 实现缺口

- lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1531 weight=overlap_area/drop_area；:1553-1554；lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:707 sig=flux/area。
- 默认 pixfrac=1.0（config/defaults.json:541-549）时 legacy 与面亮度保持式数值相同；pixfrac<1 时输出面亮度偏 1/pixfrac²。
- 最小修复：weight *= pixfrac²（等价 a/A_pixel）；需构建后跑 p1drz 常量面亮度门 FZ-GATE-CONST-SB（覆盖全 pixfrac∈(0,1]）。

## 结论

设计 §11.1（常量面亮度）正确，文档已订正，实现落后。**不阻塞设计。**
