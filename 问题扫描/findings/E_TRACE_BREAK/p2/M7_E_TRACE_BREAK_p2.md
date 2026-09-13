# M7 · E_TRACE_BREAK（追溯断点）· P2

## M7-E-201 SCI/合同/注释三处「与实现一致」声明所引代码行锚整段漂移（本代理读码亲证）
- 类别: E_TRACE_BREAK · 优先级: P2（属 M6b-E-002「行锚系统性失效」主题的**本域实例清单**；父层指令：不另写系统性判词）
| 声明处（锚） | 引用的代码锚 | 复核时实际位置 | 状态 |
|---|---|---|---|
| `docs/science/ASTROMETRY.md::§5 尾`（:66） | `ipv_wcs.cpp:13-16,153-164,274-420,530-576`、`ipv_select.cpp:695,712` | Y-down 翻转块在 `ipv_wcs.cpp::build_fits_wcs_from_solution` 内 **641-696**；`ipv_select.cpp` 命中 :695/:708 | 前段整段失效、后段漂移约 4 行 |
| `docs/contracts/DATA_SEMANTICS.md::§13.2 cd[4] 行`（:739） | 「Y-down，cd12/cd22 已取反 `ipv_wcs.cpp:542-544`」 | 实际取反 :655-656 | 锚失效（**语义正确**：本代理亲验取反存在） |
| `lib/plate_solve/cpp/ipv/include/ipv_solver.h`（SIP 结构注释） | 「flip to FITS Y-down `ipv_wcs.cpp:542-570`」 | :641-696 | 锚失效 |
| `docs/science/DRIZZLE.md::§12 一致性锚`（:71） | `spherical_overlap.cpp:40` | `::HP_CIRCUMRADIUS_FACTOR` 在 :42 | 漂移 2 行（同锚另列 :573/:773-931 未逐核，标待核） |
| `docs/algorithms/PHASE2_SAMPLER.md::§/常数权威块`（:196） | 自注「sampler.cpp:672 **旧行号锚**」 | — | 文档已自认漂移（正面处置样板，可作规范模板） |
- 问题说明 共性是**结论正确、指针失效**：本域五处均未随代码搬迁更新（RESCUE-V3 期间 `lib/` 仍在漂移）。宪章 §12.3-9（文档内引用存在性）目前无机器门 ⇒ 「与实现一致」类声明不可复核，施工者按行锚读码会读到无关函数。
- 影响 追溯可达性；也是我域两条差异条目（M7-A-101、M7-C-201）复核成本的来源。
- 处置 ①五处一律改 `path::符号` 形（本文件即示范）；②与 `40_OWNER_DECISIONS.md::C-06`（§12.3-9 机器门 + doc_globs 覆盖 docs/owner 与 docs/contracts + 裸 `:NN` 纳入语法）并案。
- 置信度 高（四处为本代理亲自读码所得）
- related **M6b-E-002**（主题主落）、C-06、M7-A-101、M7-C-201、M7-H-101（`s1` 未定义符号属同类可执行性断链）
- 四态判定 **仍成立**（漂移亲证；修复属 owner）

## M7-E-202 文档以绝对行号自引，插入即漂移，使同文「见 :N」指向错误行（L19-029）
- 位置 `docs/science/REJECTION.md::§2 尾`（:32 自引 `REJECTION.md:16`；复核时 :16 实为 §1 内容），同篇 :46/:80/:130 的行内自引同族
- 问题说明 自引用行号随任何插入漂移；本代理复核确认 :16 已非所描述内容 ⇒ 同文互指成环且指向错误。
- 处置 文档内互引一律改节名/句锚（`::§5 n<6 分支` 形）。
- related M7-E-201（同族）、M6b-E-002、C-06；判定 **仍成立**；置信度 高
