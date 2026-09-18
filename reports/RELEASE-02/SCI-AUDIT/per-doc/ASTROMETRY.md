# per-doc: docs/science/ASTROMETRY.md（SCI-WCS-001，FROZEN；RELEASE-02 已订正）

## 发现

| 编号 | 行 | 主张 | 独立证据 | 判定 | 三分类 |
|---|---|---|---|---|---|
| R2-CC-02 | §5a（订正后） | 求解器 u=det_x−w/2=p−CRPIX=q；输出 x=u+CRPIX=p 为 1-based；无 1px/0.5px 误差 | Paper I 式(1)（DOI 10.1051/0004-6361:20021326 ✅，arXiv:astro-ph/0207407）；代码 sdet_api.cpp:546、ipv_select.cpp:943,947、ipv_wcs.cpp:158-162,942-946；astropy 7.0.1 origin 复算 | CORRECT | DESIGN-OK-DOC-WRONG（ipv_wcs.h 注释标签错误仍在） |

## 遗留（comment-only / 测试面）

1. lib/algorithms/platesolve/cpp/ipv/include/ipv_wcs.h:43,57-71 仍把 x=u+CRPIX 标为 "0-based FITS 像素"。
2. tests/unit/p1wcs/p1wcs_astropy_cross.py:296-302 对已是 1-based 的 x_f 用 origin=0 + crpix+1（等价 relabel）。
3. 风险 R2：sdet 半整数探测与 cx=w/2 的 0.5 相消无测试锁定；建议跨探测合成测试。

## 结论

设计/实现与 Paper I 逐式一致；旧 §5a 是标签错误，已订正。**不阻塞设计。**
