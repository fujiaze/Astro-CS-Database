# per-doc: docs/algorithms/STAR_DETECTION_ALGORITHMS.md

| 编号 | 行 | 主张 | 独立证据 | 判定 | 三分类 |
|---|---|---|---|---|---|
| P1C-STD-01 | 52-53,102 | s_factor=√(−2ln0.001)=3.7172 | 复算 √(2ln1000)=3.7169221888（rel 7.47e-5，舍入） | AMBIGUOUS | DESIGN-OK-DOC-WRONG |
| P1C-STD-02 | 56-57,117 | 截断常数、锚 | peaker/record 锚整体漂移约 +145-150（s_factor 声称 :1678 实为 :1823 等） | WRONG | DESIGN-OK-DOC-WRONG |
| P1C-STD-03 | §6/§11.1 | 生产候选阶段截断 maxStars×2（:2028-2034） | sdet_api.cpp 中 maxStars×2 仅在 legacy 路径（:1135,:1470）；生产 sdet_detect_impl 排序 :2181、输出截断 :2394-2395 → 文档资源界不成立 | WRONG | DESIGN-OK-DOC-WRONG |
| P1C-STD-04 | 255 | MOFFAT4_FWHM_FACTOR 1.230310 | 精确 1.2303076526 | AMBIGUOUS | DESIGN-OK-DOC-WRONG |

## 处置
重锚；订正"生产截断"表述；常数标 ≈ 或全精度。
