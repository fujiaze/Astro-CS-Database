# INDEX · 定稿条目机械汇总

- 生成方式：前台按 `问题扫描/findings/<类别>/p<0|1|2>/*.md` 的 finding 标题（`### Mxx-A-001` 形式）机械计数；复跑口径见 `_tools/verify_anchors.js` 同一目录。
- 现状：**161** 个定稿文件、**333** 条 finding 标题（M7/M8/M8a/M9 与 M3b 之后的合并域尚未计入）。
- 优先级与类别的**终判**以各域 `_merge/M*.md` 处置表为准；本表只作导航与去重后的规模核对，不改判。

## 一、类别 × 优先级
| 类别 | P0 | P1 | P2 | 小计 |
|---|---|---|---|---|
| A_SCI_DEF | 13 | 21 | 5 | 39 |
| B_STD_MISMATCH | 9 | 11 | 1 | 21 |
| C_DOC_CODE_GAP | 17 | 48 | 10 | 75 |
| D_COMMENT | 0 | 14 | 20 | 34 |
| E_TRACE_BREAK | 2 | 22 | 7 | 31 |
| F_TEST_GAP | 9 | 22 | 5 | 36 |
| G_GOV_GATE | 15 | 33 | 6 | 54 |
| H_NUMERIC | 3 | 7 | 4 | 14 |
| I_DOC_HYGIENE | 0 | 11 | 18 | 29 |
| **合计** | **68** | **189** | **76** | **333** |

## 二、按域（文件名后缀）
| 域 | 条目 |
|---|---|
| M1a | 44 |
| M2a | 46 |
| M2b | 29 |
| M3 | 43 |
| M3b | 22 |
| M4 | 31 |
| M5a | 22 |
| M5b | 43 |
| M6a | 31 |
| M6b | 22 |
| 其他 | 0 |

## 三、文件清单（条目数降序）
| 文件 | 条目 |
|---|---|
| G_GOV_GATE/p1/M5b_L12_L17.md | 14 |
| C_DOC_CODE_GAP/p1/M2a_L04_L10.md | 11 |
| C_DOC_CODE_GAP/p1/M3_L05_L07.md | 9 |
| D_COMMENT/p2/M6a_L13_L14.md | 8 |
| A_SCI_DEF/p1/M1a_L01_L02.md | 7 |
| D_COMMENT/p1/M6a_L13_L14.md | 7 |
| F_TEST_GAP/p1/M4_L08_L09.md | 6 |
| G_GOV_GATE/p0/M5b_L12_L17.md | 6 |
| G_GOV_GATE/p1/M5a_L11.md | 6 |
| B_STD_MISMATCH/p0/M2b_L03_L15.md | 6 |
| C_DOC_CODE_GAP/p1/M5b_L12_L17.md | 5 |
| E_TRACE_BREAK/p1/M5b_L12_L17.md | 5 |
| I_DOC_HYGIENE/p2/M5b_L12_L17.md | 5 |
| C_DOC_CODE_GAP/p1/M4_L08_L09.md | 4 |
| C_DOC_CODE_GAP/p1/M1a_L01_L02.md | 4 |
| F_TEST_GAP/p1/M1a_L01_L02.md | 4 |
| C_DOC_CODE_GAP/p0/M1a_L01_L02.md | 4 |
| G_GOV_GATE/p0/M5a_L11_L17.md | 4 |
| C_DOC_CODE_GAP/p1/M5a_L11.md | 4 |
| A_SCI_DEF/p1/M3b_L06.md | 4 |
| D_COMMENT/p2/M2a_L04_L10.md | 4 |
| E_TRACE_BREAK/p1/M2a_L04_L10.md | 4 |
| A_SCI_DEF/p1/M3_L05_L07.md | 4 |
| F_TEST_GAP/p1/M3_L05_L07.md | 4 |
| G_GOV_GATE/p1/M2b_L03_L15.md | 4 |
| B_STD_MISMATCH/p1/M2b_L03_L15.md | 4 |
| C_DOC_CODE_GAP/p0/M4_L08_L09.md | 3 |
| A_SCI_DEF/p1/M4_L08_L09.md | 3 |
| A_SCI_DEF/p0/M1a_L01_L02.md | 3 |
| B_STD_MISMATCH/p1/M1a_L01_L02.md | 3 |
| E_TRACE_BREAK/p1/M1a_L01_L02.md | 3 |
| I_DOC_HYGIENE/p2/M1a_L01_L02.md | 3 |
| I_DOC_HYGIENE/p1/M5b_L12_L17.md | 3 |
| B_STD_MISMATCH/p1/M2a_L04_L10.md | 3 |
| A_SCI_DEF/p0/M3b_L06.md | 3 |
| C_DOC_CODE_GAP/p1/M3b_L06.md | 3 |
| F_TEST_GAP/p0/M3b_L06.md | 3 |
| I_DOC_HYGIENE/p2/M2a_L04_L10.md | 3 |
| F_TEST_GAP/p1/M2a_L04_L10.md | 3 |
| C_DOC_CODE_GAP/p0/M3_L05_L07.md | 3 |
| C_DOC_CODE_GAP/p1/M2b_L03_L15.md | 3 |
| I_DOC_HYGIENE/p1/M6a_L13_L14.md | 3 |
| G_GOV_GATE/p0/M6b_L16_L18.md | 3 |
| G_GOV_GATE/p1/M6b_L16_L18.md | 3 |
| D_COMMENT/p1/M3_L05_L07.md | 3 |
| D_COMMENT/p2/M3_L05_L07.md | 3 |
| E_TRACE_BREAK/p2/M6b_L16_L18.md | 3 |
| A_SCI_DEF/p2/M3_L05_L07.md | 3 |
| C_DOC_CODE_GAP/p1/M6b_L16_L18.md | 3 |
| E_TRACE_BREAK/p1/M6b_L16_L18.md | 3 |
| C_DOC_CODE_GAP/p2/M2b_L03_L15.md | 3 |
| A_SCI_DEF/p0/M4_L08_L09.md | 2 |
| F_TEST_GAP/p0/M4_L08_L09.md | 2 |
| C_DOC_CODE_GAP/p2/M4_L08_L09.md | 2 |
| D_COMMENT/p1/M4_L08_L09.md | 2 |
| E_TRACE_BREAK/p1/M4_L08_L09.md | 2 |
| C_DOC_CODE_GAP/p0/M5b_L12_L17.md | 2 |
| B_STD_MISMATCH/p0/M1a_L01_L02.md | 2 |
| H_NUMERIC/p0/M2a_L04_L10.md | 2 |
| G_GOV_GATE/p1/M1a_L01_L02.md | 2 |
| D_COMMENT/p2/M1a_L01_L02.md | 2 |
| A_SCI_DEF/p1/M2a_L04_L10.md | 2 |
| C_DOC_CODE_GAP/p2/M2a_L04_L10.md | 2 |
| H_NUMERIC/p1/M3b_L06.md | 2 |
| C_DOC_CODE_GAP/p0/M3b_L06.md | 2 |
| G_GOV_GATE/p2/M2a_L04_L10.md | 2 |
| H_NUMERIC/p2/M2a_L04_L10.md | 2 |
| F_TEST_GAP/p2/M2a_L04_L10.md | 2 |
| A_SCI_DEF/p0/M2b_L03_L15.md | 2 |
| C_DOC_CODE_GAP/p1/M6a_L13_L14.md | 2 |
| I_DOC_HYGIENE/p1/M6b_L16_L18.md | 2 |
| G_GOV_GATE/p2/M6b_L16_L18.md | 2 |
| C_DOC_CODE_GAP/p2/M3_L05_L07.md | 2 |
| A_SCI_DEF/p0/M3_L05_L07.md | 2 |
| F_TEST_GAP/p1/M2b_L03_L15.md | 2 |
| I_DOC_HYGIENE/p2/M6a_L13_L14.md | 2 |
| H_NUMERIC/p1/M3_L05_L07.md | 2 |
| I_DOC_HYGIENE/p1/M3_L05_L07.md | 2 |
| I_DOC_HYGIENE/p2/M3_L05_L07.md | 2 |
| A_SCI_DEF/p2/M4_L08_L09.md | 1 |
| G_GOV_GATE/p1/M4_L08_L09.md | 1 |
| G_GOV_GATE/p2/M4_L08_L09.md | 1 |
| H_NUMERIC/p2/M4_L08_L09.md | 1 |
| D_COMMENT/p2/M4_L08_L09.md | 1 |
| B_STD_MISMATCH/p2/M1a_L01_L02.md | 1 |
| A_SCI_DEF/p0/M2a_L04_L10.md | 1 |
| B_STD_MISMATCH/p0/M2a_L04_L10.md | 1 |
| C_DOC_CODE_GAP/p0/M2a_L04_L10.md | 1 |
| F_TEST_GAP/p0/M2a_L04_L10.md | 1 |
| D_COMMENT/p1/M1a_L01_L02.md | 1 |
| E_TRACE_BREAK/p2/M1a_L01_L02.md | 1 |
| F_TEST_GAP/p0/M1a_L01_L02.md | 1 |
| G_GOV_GATE/p2/M1a_L01_L02.md | 1 |
| H_NUMERIC/p1/M1a_L01_L02.md | 1 |
| F_TEST_GAP/p1/M5b_L12_L17.md | 1 |
| F_TEST_GAP/p2/M5b_L12_L17.md | 1 |
| E_TRACE_BREAK/p2/M5b_L12_L17.md | 1 |
| D_COMMENT/p1/M5a_L11.md | 1 |
| F_TEST_GAP/p1/M5a_L11.md | 1 |
| A_SCI_DEF/p1/M5a_L11.md | 1 |
| I_DOC_HYGIENE/p1/M5a_L11.md | 1 |
| D_COMMENT/p2/M5a_L11.md | 1 |
| I_DOC_HYGIENE/p2/M5a_L11.md | 1 |
| E_TRACE_BREAK/p1/M5a_L11.md | 1 |
| H_NUMERIC/p1/M5a_L11.md | 1 |
| F_TEST_GAP/p2/M1a_L01_L02.md | 1 |
| H_NUMERIC/p0/M3b_L06.md | 1 |
| E_TRACE_BREAK/p2/M2a_L04_L10.md | 1 |
| G_GOV_GATE/p0/M3b_L06.md | 1 |
| E_TRACE_BREAK/p1/M3b_L06.md | 1 |
| D_COMMENT/p2/M3b_L06.md | 1 |
| I_DOC_HYGIENE/p2/M3b_L06.md | 1 |
| G_GOV_GATE/p1/M2a_L04_L10.md | 1 |
| E_TRACE_BREAK/p0/M6b_L16_L18.md | 1 |
| C_DOC_CODE_GAP/p0/M6a_L13_L14.md | 1 |
| C_DOC_CODE_GAP/p0/M2b_L03_L15.md | 1 |
| C_DOC_CODE_GAP/p2/M6a_L13_L14.md | 1 |
| I_DOC_HYGIENE/p2/M6b_L16_L18.md | 1 |
| E_TRACE_BREAK/p1/M6a_L13_L14.md | 1 |
| E_TRACE_BREAK/p2/M6a_L13_L14.md | 1 |
| B_STD_MISMATCH/p1/M6a_L13_L14.md | 1 |
| A_SCI_DEF/p2/M6a_L13_L14.md | 1 |
| E_TRACE_BREAK/p0/M3_L05_L07.md | 1 |
| E_TRACE_BREAK/p1/M3_L05_L07.md | 1 |
| F_TEST_GAP/p0/M3_L05_L07.md | 1 |
| G_GOV_GATE/p0/M6a_L13_L14.md | 1 |
| G_GOV_GATE/p1/M6a_L13_L14.md | 1 |
| F_TEST_GAP/p1/M6b_L16_L18.md | 1 |
| F_TEST_GAP/p0/M2b_L03_L15.md | 1 |
| G_GOV_GATE/p1/M3_L05_L07.md | 1 |
| F_TEST_GAP/p2/M6a_L13_L14.md | 1 |
| E_TRACE_BREAK/p1/M2b_L03_L15.md | 1 |
| H_NUMERIC/p1/M2b_L03_L15.md | 1 |
| H_NUMERIC/p2/M2b_L03_L15.md | 1 |

> 另有 27 个文件的条目标题不是 `Mxx-<类别>-<编号>` 形式（多为早期命名或纯说明件），未被机械计数；对应域由 R 层核对补录。
