# per-doc: docs/algorithms/DRIZZLE_GEOMETRY.md

| 编号 | 行 | 主张 | 独立证据 | 判定 | 三分类 |
|---|---|---|---|---|---|
| R2-L-01b(P1C-DRZ-01) | 89,91 | HEALPix 尺度 211034.6″/nside | 公式 √(π/3)(180/π)3600 = **211076.285142**（本审计员复算）；实现按公式算（drizzle_engine.cpp:684-685 注释 211076.3），但 module_adapters.cpp:3498、orchestrator.cpp:173-174,191、hp_drizzle_api.h:98 四处注释仍写 211034.6 | WRONG | DESIGN-OK-DOC-WRONG |
| P1C-DRZ-02 | 37-47 | w_jp=a_jp/A_drop,j（drizzle_engine.cpp:1508） | 公式对，锚漂移 +23（实际 :1531/:1553/:1554/:1557-1560） | WRONG | DESIGN-OK-DOC-WRONG |
| P1C-DRZ-03 | 88-92 | 锚 :624-710 / 常数 :676-677 / 钳位 :681-697 | 实际 compute_auto_nside_ex 625-730 / 常数 :684-685 / 钳位 :703-706 | WRONG | DESIGN-OK-DOC-WRONG |
| P1C-DRZ-04 | 37-40 | pixfrac<1 时 S_p 偏 1/pixfrac² | 独立代数确认；修复 w=a/A_pixel 正确 | CORRECT（登记） | DESIGN-OK-DOC-WRONG（代码缺口） |
| P1C-DRZ-05 | 68 | 极区最坏 1.044×hp_res | astropy-healpix 复算 1.04150@nside64；1.25 缓冲成立 | CORRECT | — |
| P1C-DRZ-06 | 73-76 | Eriksson 扇形三角剖分 | 实现=Van Oosterom & Strackee 1983 atan2 三角（DOI 10.1109/TBME.1983.325207）；Eriksson 1990 未列入参考 | INSUFFICIENT-EVIDENCE | DESIGN-OK-EVIDENCE-MISSING |

## 处置
改 211034.6→211076.3（doc+4 处注释）；重锚 §1/§3；补 Eriksson 1990 或改称 VOS 公式。
